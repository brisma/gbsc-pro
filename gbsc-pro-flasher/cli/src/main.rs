//! gbsc-flasher: the GBSC PRO flasher from a terminal (same core as the app).
use clap::{Parser, Subcommand, ValueEnum};
use gbsc_core::detect::{list_ports, pick_port, PortKind};
use gbsc_core::device::{backup_file_name, Unit};
use gbsc_core::esp::flasher::{esp_flash, Erase, EspOptions};
use gbsc_core::esp::stub::{find_stub, load_stub};
use gbsc_core::image::{parse_firmware, FirmwareFile, FirmwareKind};
use gbsc_core::link::SerialportLink;
use gbsc_core::progress::Progress;
use gbsc_core::version::compare_versions;
use gbsc_core::ymodem::adv_flash;
use std::io::Write;
use std::path::{Path, PathBuf};
use std::process::ExitCode;

// The stub in the source tree, for a cargo build (target/release has none next to it); a release binary has it next
// to the program, and this build-time path does not exist on other machines
const SOURCE_RESOURCES: &str = concat!(env!("CARGO_MANIFEST_DIR"), "/../resources");

#[derive(Parser)]
#[command(name = "gbsc-flasher", version, about = "Flash and update GBSC PRO units (GBS-Control ESP8266 and ADV Controller)")]
struct Cli {
    #[command(subcommand)]
    cmd: Cmd,
}

#[derive(Clone, Copy, ValueEnum)]
enum EraseArg {
    User,
    Wifi,
    All,
}

#[derive(Subcommand)]
enum Cmd {
    /// List the connected units (USB)
    List,
    /// Flash a firmware file over USB (ESP8266 or ADV Controller bootloader); with --erase and no file: erase only
    Flash {
        file: Option<PathBuf>,
        #[arg(long)]
        port: Option<String>,
        /// ESP8266 only: erase the user data, the WiFi settings or everything first
        #[arg(long, value_enum)]
        erase: Option<EraseArg>,
        /// ESP8266 only: do not use the fast flasher stub (slow ROM path, no check)
        #[arg(long)]
        no_stub: bool,
        /// flash a file without a GBSC header or version marker (factory or upstream images)
        #[arg(long)]
        force: bool,
    },
    /// Update GBS-Control over WiFi (host or host:port)
    Update {
        file: PathBuf,
        #[arg(long)]
        host: String,
        #[arg(long)]
        allow_downgrade: bool,
        /// install a file without the GBSC marker (the unit installs any ESP8266 image)
        #[arg(long)]
        force: bool,
    },
    /// Update the ADV Controller over WiFi from the image inside GBS-Control
    UpdateAdv {
        #[arg(long)]
        host: String,
    },
    /// Download a backup of the unit's settings and profiles
    Backup {
        #[arg(long)]
        host: String,
        #[arg(long)]
        out: Option<PathBuf>,
    },
}

fn fail(msg: impl std::fmt::Display) -> ExitCode {
    eprintln!("error: {msg}");
    ExitCode::from(1)
}

fn printer() -> impl FnMut(Progress) {
    move |p| match p {
        Progress::Status(s) => println!("{s}"),
        Progress::Bytes { done, total } => {
            let pct = if total == 0 { 0 } else { done * 100 / total };
            print!("\r{pct:3}% ");
            let _ = std::io::stdout().flush();
            if done >= total {
                println!();
            }
        }
        Progress::Done(s) => println!("{s}"),
    }
}

fn unit(host: &str) -> Unit {
    match host.rsplit_once(':') {
        Some((h, p)) if p.parse::<u16>().is_ok() => Unit::with_ports(h, p.parse().unwrap(), 81),
        _ => Unit::new(host),
    }
}

fn read_file(path: &Path) -> Result<FirmwareFile, String> {
    let bytes = std::fs::read(path).map_err(|e| format!("cannot read {}: {e}", path.display()))?;
    let name = path.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
    parse_firmware(&name, &bytes).map_err(|e| format!("{}: {e}", path.display()))
}

/// YYYY-MM-DD (UTC) without a date crate: days since 1970 to the civil date (Howard Hinnant's algorithm).
fn today() -> String {
    let days = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs() / 86400).unwrap_or(0) as i64;
    let z = days + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z - era * 146_097;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146_096) / 365;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    let y = yoe + era * 400 + i64::from(m <= 2);
    format!("{y:04}-{m:02}-{d:02}")
}

fn flash(file: Option<PathBuf>, port: Option<String>, erase: Option<EraseArg>, no_stub: bool, force: bool) -> ExitCode {
    let fw = match &file {
        Some(p) => match read_file(p) {
            Ok(f) => Some(f),
            Err(e) => return fail(e),
        },
        None if erase.is_some() => None,
        None => return fail("give a firmware file (or --erase alone to erase the ESP8266)"),
    };
    if let Some(f) = &fw {
        if f.by_size_only && !force {
            return fail(format!("{} has no GBSC header or version marker; if you are sure it is the right image, add --force", f.name));
        }
    }
    let kind = fw.as_ref().map_or(FirmwareKind::Esp, |f| f.kind);
    if kind == FirmwareKind::Adv && erase.is_some() {
        return fail("--erase is for the ESP8266 only");
    }
    let want = if kind == FirmwareKind::Adv { PortKind::Adv } else { PortKind::Esp };
    let name = match pick_port(&list_ports(), want, port.as_deref()) {
        Ok(n) => n,
        Err(e) => return fail(e),
    };
    println!("Port {name}");
    let mut link = match SerialportLink::open(&name, 115_200) {
        Ok(l) => l,
        Err(e) => return fail(format!("cannot open {name}: {e}")),
    };
    let mut progress = printer();
    if kind == FirmwareKind::Adv {
        return match adv_flash(&mut link, fw.as_ref().unwrap(), &mut progress) {
            Ok(_) => {
                println!("Done. Unplug the ADV Controller and power the GBSC; check the version on the GBSC (System > Update).");
                ExitCode::SUCCESS
            }
            Err(e) => fail(e),
        };
    }
    let stub = if no_stub { None } else { find_stub(Some(SOURCE_RESOURCES.as_ref())).and_then(|p| load_stub(&p)) };
    if stub.is_none() {
        println!("Slow mode: the fast flasher (esp8266-stub.json) is not next to the program; the write will not be checked.");
    }
    let opt = EspOptions {
        erase: match erase {
            None => Erase::None,
            Some(EraseArg::User) => Erase::User,
            Some(EraseArg::Wifi) => Erase::Wifi,
            Some(EraseArg::All) => Erase::All,
        },
        ..EspOptions::default()
    };
    match esp_flash(&mut link, fw.as_ref(), stub.as_ref(), &opt, &mut progress) {
        Ok(r) => {
            let check = match r.md5_ok {
                Some(true) => "checked",
                _ => "not checked",
            };
            println!("Done in {:.0} s ({check}). The GBSC restarts.", r.seconds);
            ExitCode::SUCCESS
        }
        Err(e) => fail(e),
    }
}

/// The `list` output: one line per GBSC port, or a line saying there is none.
fn list_lines(ports: &[(String, PortKind)]) -> Vec<String> {
    let lines: Vec<String> = ports
        .iter()
        .filter_map(|(name, k)| {
            let what = match k {
                PortKind::Esp => "GBS-Control (ESP8266)",
                PortKind::Adv => "ADV Controller (bootloader)",
                PortKind::AdvUnconfirmed => "USB modem, maybe the ADV Controller bootloader (no HC32 USB IDs: flashed only with --port)",
                PortKind::AdvConsole => "ADV Controller (application: not flashable)",
                PortKind::Unknown => return None,
            };
            Some(format!("{name}\t{what}"))
        })
        .collect();
    if lines.is_empty() { vec!["No GBSC device on USB.".to_string()] } else { lines }
}

fn main() -> ExitCode {
    let cli = Cli::parse();
    match cli.cmd {
        Cmd::List => {
            let ports: Vec<(String, PortKind)> = list_ports().into_iter().map(|(p, k)| (p.name, k)).collect();
            for line in list_lines(&ports) {
                println!("{line}");
            }
            ExitCode::SUCCESS
        }
        Cmd::Flash { file, port, erase, no_stub, force } => flash(file, port, erase, no_stub, force),
        Cmd::Update { file, host, allow_downgrade, force } => {
            let fw = match read_file(&file) {
                Ok(f) if f.kind == FirmwareKind::Esp => f,
                Ok(_) => return fail("this is an ADV Controller image: over WiFi the ADV Controller updates from GBS-Control (update-adv)"),
                Err(e) => return fail(e),
            };
            if fw.by_size_only && !force {
                return fail("the file has no GBSC marker (an upstream gbs-control build?): the unit would install it and leave the PRO firmware; add --force if it is the right image");
            }
            let u = unit(&host);
            let info = match u.info() {
                Ok(i) => i,
                Err(e) => return fail(e),
            };
            if let Some(v) = &fw.version {
                if compare_versions(v, &info.fw).is_lt() && !allow_downgrade {
                    return fail(format!("the unit runs {}, the file is the older {v}: add --allow-downgrade to install it", info.fw));
                }
            }
            match u.update(&fw, &mut printer()) {
                Ok(v) => {
                    println!("The GBSC runs {v}.");
                    ExitCode::SUCCESS
                }
                Err(e) => fail(e),
            }
        }
        Cmd::UpdateAdv { host } => match unit(&host).update_adv_controller(&mut printer()) {
            Ok(v) => {
                println!("The ADV Controller runs {v}.");
                ExitCode::SUCCESS
            }
            Err(e) => fail(e),
        },
        Cmd::Backup { host, out } => {
            let u = unit(&host);
            match u.backup() {
                Ok(text) => {
                    let path = out.unwrap_or_else(|| PathBuf::from(backup_file_name(&u.host, &today())));
                    match std::fs::write(&path, text) {
                        Ok(()) => {
                            println!("Saved {}", path.display());
                            ExitCode::SUCCESS
                        }
                        Err(e) => fail(format!("cannot write {}: {e}", path.display())),
                    }
                }
                Err(e) => fail(e),
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn list_says_when_nothing_is_connected() {
        // an empty output looked like a broken command
        assert_eq!(list_lines(&[]), vec!["No GBSC device on USB.".to_string()]);
        assert_eq!(list_lines(&[("COM9".into(), PortKind::Unknown)]), vec!["No GBSC device on USB.".to_string()]);
        assert_eq!(list_lines(&[("COM3".into(), PortKind::Esp)]), vec!["COM3\tGBS-Control (ESP8266)".to_string()]);
    }

    #[test]
    fn a_cargo_build_finds_the_stub_in_the_source_tree() {
        // target/release/gbsc-flasher has no stub next to it: without this the write ran slow and unchecked
        let p = find_stub(Some(SOURCE_RESOURCES.as_ref())).expect("stub");
        assert!(load_stub(&p).is_some());
    }
}
