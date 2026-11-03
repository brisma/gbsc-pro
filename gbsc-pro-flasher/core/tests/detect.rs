// Port classification, the same table as the former Python flasher (its test_detect.py). The ADV application
// console (USB CDC 1209:0001 "ADV Controller console") must never be taken for the HCMGBoot bootloader (2E88:4603).
use gbsc_core::detect::{classify_port, looks_like_port, merge_aliases, pick_port, refuse_reason, PortInfo, PortKind};

fn port(vid: u16, pid: u16, desc: &str, mfr: &str, name: &str, product: &str) -> PortInfo {
    PortInfo {
        name: name.into(),
        vid,
        pid,
        product: product.into(),
        manufacturer: mfr.into(),
        description: desc.into(),
        serial_number: None,
        location: None,
    }
}

#[test]
fn bootloader_by_vid_pid() {
    assert_eq!(classify_port(&port(0x2E88, 0x4603, "", "", "/dev/cu.usbmodem1", "")), PortKind::Adv);
}

#[test]
fn console_by_vid_pid_and_product() {
    let p = port(0x1209, 0x0001, "ADV Controller console", "GBSC PRO", "/dev/cu.usbmodem2", "ADV Controller console");
    assert_eq!(classify_port(&p), PortKind::AdvConsole);
}

#[test]
fn other_1209_0001_device_is_not_the_console() {
    let p = port(0x1209, 0x0001, "Some hobby board", "", "/dev/cu.usbmodem3", "Some hobby board");
    assert_ne!(classify_port(&p), PortKind::AdvConsole);
}

#[test]
fn windows_console_empty_product_generic_description() {
    let p = port(0x1209, 0x0001, "USB Serial Device (COM5)", "Microsoft", "COM5", "");
    assert_eq!(classify_port(&p), PortKind::AdvConsole);
}

#[test]
fn windows_console_friendly_name_as_product() {
    // serialport on Windows fills product (and so description) with the localized friendly name, never the USB
    // product string: Windows 11 ARM64, Italian, usbser.sys
    for name in ["Dispositivo seriale USB (COM5)", "USB Serial Device (COM12)"] {
        let p = port(0x1209, 0x0001, name, "Microsoft", "COM5", name);
        assert_eq!(classify_port(&p), PortKind::AdvConsole, "{name}");
    }
}

#[test]
fn console_is_not_adv_even_on_usbmodem() {
    let p = port(0x1209, 0x0001, "ADV Controller console", "", "/dev/cu.usbmodem4", "ADV Controller console");
    assert_ne!(classify_port(&p), PortKind::Adv);
}

#[test]
fn ch340_is_esp() {
    assert_eq!(classify_port(&port(0x1A86, 0x7523, "USB Serial", "", "/dev/cu.usbserial-1", "")), PortKind::Esp);
}

#[test]
fn generic_usbmodem_without_ids_needs_confirmation() {
    // an Arduino or any CDC board is a usbmodem too: it must not be sent '1' and YMODEM without the user saying so
    let k = classify_port(&port(0x2341, 0x0043, "Arduino Uno", "Arduino", "/dev/cu.usbmodemX", "Arduino Uno"));
    assert_eq!(k, PortKind::AdvUnconfirmed);
    assert_eq!(classify_port(&port(0, 0, "", "", "/dev/cu.usbmodemY", "")), PortKind::AdvUnconfirmed);
    assert_eq!(refuse_reason(k), None);
    // the bootloader's own names still count as the bootloader
    assert_eq!(classify_port(&port(0, 0, "HCMGBoot", "", "COM7", "")), PortKind::Adv);
}

fn listed(ports: &[(&str, PortKind)]) -> Vec<(PortInfo, PortKind)> {
    ports.iter().map(|(n, k)| (port(0, 0, "", "", n, ""), *k)).collect()
}

#[test]
fn an_unconfirmed_port_is_never_picked_alone() {
    let ports = listed(&[("/dev/cu.usbmodemX", PortKind::AdvUnconfirmed)]);
    let e = pick_port(&ports, PortKind::Adv, None).unwrap_err();
    assert!(e.contains("--port /dev/cu.usbmodemX"), "{e}");
    // naming it is the confirmation
    assert_eq!(pick_port(&ports, PortKind::Adv, Some("/dev/cu.usbmodemX")).unwrap(), "/dev/cu.usbmodemX");
    assert!(pick_port(&ports, PortKind::Esp, Some("/dev/cu.usbmodemX")).is_err());
}

#[test]
fn the_bootloader_is_picked_next_to_an_unconfirmed_port() {
    let ports = listed(&[("/dev/cu.usbmodemX", PortKind::AdvUnconfirmed), ("/dev/cu.usbmodem1101", PortKind::Adv)]);
    assert_eq!(pick_port(&ports, PortKind::Adv, None).unwrap(), "/dev/cu.usbmodem1101");
    let two = listed(&[("/dev/cu.usbmodem1", PortKind::Adv), ("/dev/cu.usbmodem2", PortKind::Adv)]);
    assert!(pick_port(&two, PortKind::Adv, None).unwrap_err().contains("several"));
    let console = listed(&[("/dev/cu.usbmodem3", PortKind::AdvConsole)]);
    assert!(pick_port(&console, PortKind::Adv, None).unwrap_err().contains("console"));
    assert!(pick_port(&console, PortKind::Adv, Some("/dev/cu.usbmodem3")).unwrap_err().contains("console"));
}

#[test]
fn a_named_port_of_the_other_chip_says_which_one_it_is() {
    let ports = listed(&[("/dev/cu.wchusbserial10", PortKind::Esp), ("/dev/cu.usbmodem1101", PortKind::Adv)]);
    let e = pick_port(&ports, PortKind::Adv, Some("/dev/cu.wchusbserial10")).unwrap_err();
    assert!(e.contains("GBS-Control (ESP8266)") && e.contains("ADV Controller file"), "{e}");
    let e = pick_port(&ports, PortKind::Esp, Some("/dev/cu.usbmodem1101")).unwrap_err();
    assert!(e.contains("ADV Controller bootloader") && e.contains("GBS-Control file"), "{e}");
}

#[test]
fn a_dev_path_is_a_port_not_a_firmware_file() {
    assert!(looks_like_port("/dev/cu.usbmodem00000000050C1"));
}

#[test]
fn com3_is_a_port() {
    assert!(looks_like_port("COM3"));
}

#[test]
fn a_bin_path_is_not_a_port() {
    assert!(!looks_like_port("adv-controller.bin"));
}

#[test]
fn console_refused_as_a_flash_target() {
    let why = refuse_reason(PortKind::AdvConsole).unwrap();
    assert!(why.contains("ADV Controller") && why.contains("HCMGBoot"));
}

#[test]
fn bootloader_accepted() {
    assert_eq!(refuse_reason(PortKind::Adv), None);
}

#[test]
fn ch340_aliases_merge_to_wch() {
    let a = port(0x1A86, 0x7523, "USB Serial", "", "/dev/cu.usbserial-1410", "");
    let b = port(0x1A86, 0x7523, "USB Serial", "", "/dev/cu.wchusbserial1410", "");
    let m = merge_aliases(vec![a, b]);
    assert_eq!(m.len(), 1);
    assert_eq!(m[0].name, "/dev/cu.wchusbserial1410");
}

#[test]
fn tty_twin_dropped() {
    let a = port(0x2E88, 0x4603, "", "", "/dev/tty.usbmodem1101", "");
    let b = port(0x2E88, 0x4603, "", "", "/dev/cu.usbmodem1101", "");
    let m = merge_aliases(vec![a, b]);
    assert_eq!(m.len(), 1);
    assert_eq!(m[0].name, "/dev/cu.usbmodem1101");
}

#[test]
fn a_named_port_matches_its_other_spellings() {
    // Windows names have no case (users type com3); \\.\COM10 is the long form
    let win = listed(&[("COM3", PortKind::Esp)]);
    assert_eq!(pick_port(&win, PortKind::Esp, Some("com3")).unwrap(), "COM3");
    assert_eq!(pick_port(&win, PortKind::Esp, Some(r"\\.\COM3")).unwrap(), "COM3");
    assert!(pick_port(&win, PortKind::Esp, Some("COM30")).unwrap_err().contains("not a connected"));
    // macOS lists one node per CH340, the others are the same chip: tty./cu. and the Apple driver's usbserial-
    let mac = listed(&[("/dev/cu.wchusbserial111420", PortKind::Esp)]);
    for alias in ["/dev/tty.wchusbserial111420", "/dev/cu.usbserial-111420", "/dev/tty.usbserial-111420"] {
        assert_eq!(pick_port(&mac, PortKind::Esp, Some(alias)).unwrap(), "/dev/cu.wchusbserial111420", "{alias}");
    }
    assert!(pick_port(&mac, PortKind::Esp, Some("/dev/cu.usbserial-999")).is_err());
    assert!(pick_port(&mac, PortKind::Esp, Some("/dev/CU.WCHUSBSERIAL111420")).is_err(), "macOS names keep their case");
}
