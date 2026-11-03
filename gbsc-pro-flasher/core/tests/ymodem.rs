// ADV Controller flashing (HCMGBoot + YMODEM) against a simulated bootloader
mod fakes;
use fakes::hcmgboot::FakeHcmgboot;
use gbsc_core::image::{parse_firmware, FirmwareFile};
use gbsc_core::progress::Progress;
use gbsc_core::ymodem::{adv_flash_with, AdvTimings};
use gbsc_core::FlashError;
use std::path::PathBuf;
use std::time::{Duration, Instant};

fn image() -> FirmwareFile {
    let p = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../adv-controller/release/adv-controller.bin");
    parse_firmware("adv-controller.bin", &std::fs::read(p).unwrap()).unwrap()
}

fn fast() -> AdvTimings {
    AdvTimings { c_wait: Duration::from_millis(500), ack_wait: Duration::from_millis(300), drain: Duration::from_millis(20) }
}

fn flash(dev: &mut FakeHcmgboot) -> Result<Option<gbsc_core::ymodem::AdvChipInfo>, FlashError> {
    let fw = image();
    let mut last = 0u64;
    let mut progress = |p: Progress| {
        if let Progress::Bytes { done, .. } = p {
            last = done;
        }
    };
    adv_flash_with(dev, &fw, &fast(), &mut progress)
}

#[test]
fn ymodem_writes_the_file() {
    let mut dev = FakeHcmgboot::new();
    let info = flash(&mut dev).unwrap();
    assert!(dev.finished);
    assert_eq!(dev.file, image().raw);
    assert_eq!(dev.file_name, "adv-controller.bin");
    assert!(info.unwrap().chip.starts_with("HC32F4"));
}

// macOS, Linux and the Python flasher open the port with DTR and RTS high; serialport on Windows leaves DTR low: the
// flash raises both itself before the first byte
#[test]
fn ymodem_raises_dtr_and_rts_first() {
    let mut dev = FakeHcmgboot::new();
    flash(&mut dev).unwrap();
    assert_eq!(dev.lines_up_at_first_write, Some(true));
}

#[test]
fn ymodem_without_chip_info_still_flashes() {
    let mut dev = FakeHcmgboot::new();
    dev.chip_info = false;
    assert!(flash(&mut dev).unwrap().is_none());
    assert_eq!(dev.file, image().raw);
}

#[test]
fn ymodem_resends_after_nak() {
    let mut dev = FakeHcmgboot::new();
    dev.nak_packet = Some(3);
    flash(&mut dev).unwrap();
    assert_eq!(dev.seen[..6], [1, 2, 3, 3, 4, 5]); // sequence numbers wrap at 256: look at the start only
    assert_eq!(dev.file, image().raw);
}

#[test]
fn ymodem_cancel_is_rejected() {
    let mut dev = FakeHcmgboot::new();
    dev.cancel_at = Some(2);
    assert!(matches!(flash(&mut dev), Err(FlashError::Rejected(_))));
}

#[test]
fn ymodem_port_gone_is_error() {
    let mut dev = FakeHcmgboot::new();
    dev.die_after_bytes = Some(1000);
    let t = Instant::now();
    assert!(matches!(flash(&mut dev), Err(FlashError::PortGone)));
    assert!(t.elapsed() < Duration::from_secs(15));
}

#[test]
fn ymodem_no_c_is_no_bootloader() {
    let mut dev = FakeHcmgboot::new();
    dev.never_c = true;
    assert!(matches!(flash(&mut dev), Err(FlashError::NoBootloader)));
}

#[test]
fn unacknowledged_end_of_transfer_is_an_error() {
    let mut dev = FakeHcmgboot::new();
    dev.no_eot_ack = true;
    assert!(matches!(flash(&mut dev), Err(FlashError::Rejected(_))));
}

#[test]
fn missing_ack_of_the_closing_packet_is_ignored() {
    let mut dev = FakeHcmgboot::new();
    dev.no_final_ack = true;
    let t = Instant::now();
    flash(&mut dev).unwrap();
    assert!(dev.finished);
    assert!(t.elapsed() < Duration::from_secs(2), "one try, no retries");
}

#[test]
fn long_file_name_keeps_the_size() {
    let mut fw = image();
    fw.name = format!("{}.bin", "x".repeat(200));
    let mut dev = FakeHcmgboot::new();
    adv_flash_with(&mut dev, &fw, &fast(), &mut |_| {}).unwrap();
    assert_eq!(dev.file_size, fw.raw.len());
    assert_eq!(dev.file, fw.raw);
}

#[test]
fn ymodem_lost_ack_stops_without_resending() {
    // HCMGBoot gives no checksum and may not drop a repeated sequence number: a resend after a timeout could
    // write the packet twice, so the transfer stops, as the Python flasher did
    let mut dev = FakeHcmgboot::new();
    dev.lose_ack_of = Some(3);
    let r = flash(&mut dev);
    assert!(matches!(r, Err(FlashError::Rejected(_))), "{r:?}");
    assert_eq!(dev.seen, [1, 2, 3]);
}
