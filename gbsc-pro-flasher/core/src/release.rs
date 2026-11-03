//! The firmware files next to the app (the release zip holds the flasher and that release's images).
use std::path::{Path, PathBuf};

/// The folder the user sees the app in: the executable's folder; on macOS the folder that holds the .app bundle;
/// in a Linux AppImage the folder of the .AppImage file ($APPIMAGE), not the temporary mount it runs from.
pub fn app_dir() -> PathBuf {
    let exe = std::env::current_exe().unwrap_or_default();
    let appimage = std::env::var("APPIMAGE").ok();
    app_dir_for(&exe, appimage.as_deref())
}

pub fn app_dir_for(exe: &Path, appimage: Option<&str>) -> PathBuf {
    if let Some(dir) = appimage.filter(|a| !a.is_empty()).and_then(|a| Path::new(a).parent()) {
        return dir.to_path_buf();
    }
    let dir = exe.parent().map(Path::to_path_buf).unwrap_or_default();
    // <folder>/GBSC PRO Flasher.app/Contents/MacOS/<exe>
    let bundle = dir.parent().and_then(Path::parent);
    match bundle {
        Some(b) if b.extension().is_some_and(|e| e == "app") => b.parent().map(Path::to_path_buf).unwrap_or(dir),
        _ => dir,
    }
}

use crate::image::{parse_firmware, FirmwareFile, FirmwareKind};
use crate::version::compare_versions;
use std::cmp::Ordering;

/// The preselected file of each kind; GBS-Control twice, since the release carries it as .bin (USB) and .bin.gz (WiFi).
#[derive(Debug, Default)]
pub struct Candidates {
    pub esp: Option<FirmwareFile>,
    pub esp_wifi: Option<FirmwareFile>,
    pub adv: Option<FirmwareFile>,
}

/// larger than any GBSC image (the ESP8266 flash is 4 MB; a .bin.gz is smaller)
const MAX_CANDIDATE_BYTES: u64 = 8 * 1024 * 1024;

/// The newer file; for the same version the .gz over WiFi (it travels faster), the .bin over USB (the file the
/// README names for USB; a lone .gz still works there, USB gunzips it).
fn better(new: &FirmwareFile, old: &FirmwareFile, want_gz: bool) -> bool {
    match compare_versions(new.version.as_deref().unwrap_or(""), old.version.as_deref().unwrap_or("")) {
        Ordering::Greater => true,
        Ordering::Less => false,
        Ordering::Equal => new.gzipped == want_gz && old.gzipped != want_gz,
    }
}

/// The newest ESP and ADV Controller images directly in `dir` (*.bin, *.bin.gz with a GBSCVER marker or an ADVC
/// header; other files are left out: the user can still pick them by hand).
pub fn find_candidates(dir: &Path) -> Candidates {
    let mut c = Candidates::default();
    let Ok(entries) = std::fs::read_dir(dir) else { return c };
    for e in entries.flatten() {
        let name = e.file_name().to_string_lossy().into_owned();
        let lower = name.to_lowercase();
        if !(lower.ends_with(".bin") || lower.ends_with(".bin.gz")) || !e.path().is_file() {
            continue;
        }
        if e.metadata().map(|m| m.len()).unwrap_or(u64::MAX) > MAX_CANDIDATE_BYTES {
            continue; // a disk image or similar: not read
        }
        let Ok(bytes) = std::fs::read(e.path()) else { continue };
        let Ok(f) = parse_firmware(&name, &bytes) else { continue };
        if f.by_size_only || f.version.is_none() {
            continue;
        }
        match f.kind {
            FirmwareKind::Esp => {
                if c.esp_wifi.as_ref().is_none_or(|old| better(&f, old, true)) {
                    c.esp_wifi = Some(f.clone());
                }
                if c.esp.as_ref().is_none_or(|old| better(&f, old, false)) {
                    c.esp = Some(f);
                }
            }
            FirmwareKind::Adv => {
                if c.adv.as_ref().is_none_or(|old| better(&f, old, false)) {
                    c.adv = Some(f);
                }
            }
            FirmwareKind::Unknown => {}
        }
    }
    c
}

/// macOS runs a quarantined app from a random read-only path (App Translocation): the files next to it are not
/// visible there.
pub fn translocated(path: &Path) -> bool {
    path.to_string_lossy().contains("/AppTranslocation/")
}

/// Windows Explorer runs an exe opened inside a zip from a copy in %TEMP% ("<guid>_<name>.zip.<ext>" on Windows 11,
/// "Temp1_<name>.zip" before), and "Run" copies the exe alone: the firmware files are not next to it.
pub fn run_from_zip(path: &Path) -> bool {
    let p = path.to_string_lossy().to_lowercase();
    let parts: Vec<&str> = p.split(['\\', '/']).collect();
    parts.iter().position(|c| *c == "temp").is_some_and(|t| parts[t + 1..].iter().any(|c| c.ends_with(".zip") || c.contains(".zip.")))
}
