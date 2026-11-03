//! Port classification: which serial port is the ESP8266 (CH340/CP210x), the ADV Controller bootloader (HCMGBoot)
//! or the ADV Controller's application console (refused: it is not a flash target). Same table and order as the
//! former Python flasher's classify_port, so the console is checked before the generic usbmodem fallback;
//! unlike it, a usbmodem without the bootloader's IDs or names is only a candidate the user has to confirm.

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PortKind {
    Adv,
    /// a generic USB modem without the bootloader's IDs or names: maybe HCMGBoot, maybe any CDC board (Arduino...)
    AdvUnconfirmed,
    AdvConsole,
    Esp,
    Unknown,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct PortInfo {
    pub name: String,
    pub vid: u16,
    pub pid: u16,
    pub product: String,
    pub manufacturer: String,
    pub description: String,
    pub serial_number: Option<String>,
    pub location: Option<String>,
}

const CONSOLE_VID: u16 = 0x1209;
const CONSOLE_PID: u16 = 0x0001;
const CONSOLE_PRODUCT: &str = "adv controller console";

/// A Windows device friendly name ("... (COM5)", lowercased): it says nothing about the USB product.
fn windows_friendly_name(product: &str) -> bool {
    product
        .strip_suffix(')')
        .and_then(|s| s.rsplit_once("(com"))
        .is_some_and(|(_, n)| !n.is_empty() && n.bytes().all(|b| b.is_ascii_digit()))
}

pub fn classify_port(p: &PortInfo) -> PortKind {
    let desc = p.description.to_lowercase();
    let mfr = p.manufacturer.to_lowercase();
    let dev = p.name.to_lowercase();
    let product = p.product.to_lowercase();
    let console_ids = p.vid == CONSOLE_VID && p.pid == CONSOLE_PID;
    if console_ids && (product.contains(CONSOLE_PRODUCT) || desc.contains(CONSOLE_PRODUCT)) {
        return PortKind::AdvConsole;
    }
    if console_ids && (product.is_empty() || windows_friendly_name(&product)) {
        // Windows never gives the USB product string: serialport reports the localized friendly name of usbser.sys
        // ("USB Serial Device (COMn)", "Dispositivo seriale USB (COMn)"). The console, so it is refused instead of
        // being sent the bootloader handshake
        return PortKind::AdvConsole;
    }
    if (p.vid == 0x2E88 && p.pid == 0x4603) || mfr.contains("xhsc") {
        return PortKind::Adv;
    }
    if (p.vid == 0x1A86 && p.pid == 0x7523) || p.vid == 0x10C4 {
        return PortKind::Esp;
    }
    if ["ch340", "ch341", "cp210", "silicon labs"].iter().any(|x| desc.contains(x) || mfr.contains(x)) {
        return PortKind::Esp;
    }
    if dev.contains("wchusbserial") || dev.contains("wch") {
        return PortKind::Esp;
    }
    if p.vid == CONSOLE_VID {
        return PortKind::Unknown; // another 1209 device: not ours
    }
    if desc.contains("hcmgboot") || desc.contains("hc32") {
        return PortKind::Adv; // the bootloader's own names
    }
    if dev.contains("usbmodem") {
        // the Python flasher took any macOS usbmodem for the bootloader; an Arduino is one too, so it is listed but
        // only flashed when the user names it (CLI --port, app confirmation)
        return PortKind::AdvUnconfirmed;
    }
    PortKind::Unknown
}

/// Why a port cannot be flashed, or None.
pub fn refuse_reason(k: PortKind) -> Option<&'static str> {
    match k {
        PortKind::AdvConsole => Some(
            "This is the ADV Controller's application console, not its bootloader: hold the button while plugging \
             the ADV Controller USB-C to enter HCMGBoot, or update the ADV Controller from the GBSC (System > \
             Update > ADV Controller).",
        ),
        _ => None,
    }
}

/// A serial port name rather than a file (device paths exist on disk, so a file check cannot tell them apart).
pub fn looks_like_port(arg: &str) -> bool {
    if arg.starts_with("/dev/") {
        return true;
    }
    let up = arg.to_ascii_uppercase();
    up.len() > 3 && up.starts_with("COM") && up[3..].bytes().all(|b| b.is_ascii_digit())
}

fn alias_key(p: &PortInfo) -> (u8, u16, u16, String) {
    if let Some(sn) = p.serial_number.as_deref().filter(|s| !s.is_empty()) {
        return (0, p.vid, p.pid, sn.to_string());
    }
    let base = p.name.rsplit('/').next().unwrap_or(&p.name);
    let base = base.strip_prefix("cu.").or_else(|| base.strip_prefix("tty.")).unwrap_or(base);
    for prefix in ["wchusbserial", "usbserial"] {
        if let Some(rest) = base.strip_prefix(prefix) {
            let sfx = rest.trim_start_matches(['-', '_']);
            if !sfx.is_empty() && sfx.bytes().all(|b| b.is_ascii_alphanumeric()) {
                return (1, p.vid, p.pid, sfx.to_string());
            }
        }
    }
    if let Some(loc) = p.location.as_deref().filter(|s| !s.is_empty()) {
        return (2, p.vid, p.pid, loc.to_string());
    }
    (3, 0, 0, base.to_string()) // same node under cu. and tty.
}

fn alias_rank(name: &str) -> u8 {
    // lower is better: the WCH driver node, then the macOS call-out node
    let n = name.to_lowercase();
    (!n.contains("wchusbserial")) as u8 * 2 + (n.contains("/tty.")) as u8
}

/// One entry per physical device. On macOS a CH340 shows up as /dev/cu.usbserial-X (Apple driver) and
/// /dev/cu.wchusbserialX (WCH driver), and every port as both cu.* and tty.*: the WCH node and cu.* are kept.
pub fn merge_aliases(ports: Vec<PortInfo>) -> Vec<PortInfo> {
    let mut out: Vec<(_, PortInfo)> = Vec::new();
    for p in ports {
        let key = alias_key(&p);
        match out.iter_mut().find(|(k, _)| *k == key) {
            Some((_, kept)) if alias_rank(&p.name) < alias_rank(&kept.name) => *kept = p,
            Some(_) => {}
            None => out.push((key, p)),
        }
    }
    out.into_iter().map(|(_, p)| p).collect()
}

/// The USB serial ports that are ours (ESP8266, ADV Controller bootloader or console), one per device.
pub fn list_ports() -> Vec<(PortInfo, PortKind)> {
    let ports = serialport::available_ports().unwrap_or_default();
    let infos = ports
        .into_iter()
        .filter_map(|p| match p.port_type {
            serialport::SerialPortType::UsbPort(u) => Some(PortInfo {
                name: p.port_name,
                vid: u.vid,
                pid: u.pid,
                product: u.product.clone().unwrap_or_default(),
                manufacturer: u.manufacturer.unwrap_or_default(),
                description: u.product.unwrap_or_default(),
                serial_number: u.serial_number,
                location: None,
            }),
            _ => None,
        })
        .collect();
    merge_aliases(infos)
        .into_iter()
        .map(|p| {
            let k = classify_port(&p);
            (p, k)
        })
        .filter(|(_, k)| *k != PortKind::Unknown)
        .collect()
}

/// The text shown before flashing an unconfirmed port.
pub const UNCONFIRMED_TEXT: &str = "This USB device does not carry the ADV Controller bootloader's USB IDs: it may be \
    another board (an Arduino, for example), which would be sent the bootloader commands. Flash it only if you are sure \
    it is the ADV Controller in bootloader mode.";

fn kind_text(k: PortKind) -> &'static str {
    match k {
        PortKind::Esp => "the GBS-Control (ESP8266)",
        PortKind::Adv => "the ADV Controller bootloader",
        PortKind::AdvUnconfirmed => "a USB device that may be the ADV Controller bootloader",
        PortKind::AdvConsole => "the ADV Controller application",
        PortKind::Unknown => "an unknown device",
    }
}

/// A named port of the other chip: what it is and what the file is for.
fn wrong_device(name: &str, k: PortKind, want: PortKind) -> String {
    let file = if want == PortKind::Adv { "an ADV Controller file" } else { "a GBS-Control file" };
    format!("{name} is {}, but this is {file}: choose the other port or the other file", kind_text(k))
}

/// The port to flash for `want` (Adv or Esp): the one named (an unconfirmed USB modem passes for an ADV Controller
/// file, since naming it is the confirmation), or the only match. An unconfirmed port is never picked by itself.
/// The same port under another spelling: a Windows name in any case or as \\.\COMn, a macOS node under tty. instead
/// of cu., or the Apple driver's usbserial- node of a CH340 listed under the WCH driver's name.
fn same_port(listed: &str, given: &str) -> bool {
    let com = |n: &str| {
        let n = n.strip_prefix(r"\\.\").unwrap_or(n);
        (n.len() > 3 && n[..3].eq_ignore_ascii_case("com") && n[3..].bytes().all(|b| b.is_ascii_digit())).then(|| n[3..].to_string())
    };
    if let (Some(a), Some(b)) = (com(listed), com(given)) {
        return a == b;
    }
    let unix = |n: &str| -> Option<String> {
        let base = n.strip_prefix("/dev/")?;
        let base = base.strip_prefix("cu.").or_else(|| base.strip_prefix("tty."))?;
        for prefix in ["wchusbserial", "usbserial"] {
            if let Some(rest) = base.strip_prefix(prefix) {
                return Some(format!("serial:{}", rest.trim_start_matches(['-', '_'])));
            }
        }
        Some(base.to_string())
    };
    matches!((unix(listed), unix(given)), (Some(a), Some(b)) if a == b)
}

pub fn pick_port(ports: &[(PortInfo, PortKind)], want: PortKind, given: Option<&str>) -> Result<String, String> {
    let fits = |k: PortKind| k == want || (want == PortKind::Adv && k == PortKind::AdvUnconfirmed);
    if let Some(name) = given {
        let found = ports.iter().find(|(p, _)| p.name == name).or_else(|| ports.iter().find(|(p, _)| same_port(&p.name, name)));
        return match found {
            Some((p, k)) if fits(*k) => Ok(p.name.clone()),
            Some((p, k)) => Err(refuse_reason(*k).map(str::to_string).unwrap_or_else(|| wrong_device(&p.name, *k, want))),
            None => Err(format!("{name} is not a connected GBSC device")),
        };
    }
    let names = |kind: PortKind| ports.iter().filter(|(_, k)| *k == kind).map(|(p, _)| p.name.as_str()).collect::<Vec<_>>();
    match names(want).as_slice() {
        [one] => Ok(one.to_string()),
        [] => {
            let console = ports.iter().find(|(_, k)| *k == PortKind::AdvConsole);
            let unsure = names(PortKind::AdvUnconfirmed);
            Err(match (want, console, unsure.first()) {
                (PortKind::Adv, Some((_, k)), _) => refuse_reason(*k).unwrap().to_string(),
                (PortKind::Adv, None, Some(first)) => format!(
                    "no ADV Controller bootloader with its USB IDs found; {} may be one, but could be another board: if \
                     you are sure, name it with --port {first}",
                    unsure.join(", ")
                ),
                (PortKind::Adv, None, None) => "no ADV Controller bootloader found: hold its button while plugging the USB-C cable".into(),
                _ => "no ESP8266 found: connect the GBSC's USB port".into(),
            })
        }
        many => Err(format!("several devices match, choose one with --port: {}", many.join(", "))),
    }
}
