// ESP8266 flashing against a simulated loader: stub and ROM paths, erase regions, verify, lost port
mod fakes;
use fakes::esp_rom::{FakeEsp, FLASH_SIZE};
use gbsc_core::esp::flasher::{esp_flash, rom_erase_size, Erase, EspOptions};
use gbsc_core::esp::stub::Stub;
use gbsc_core::image::{esp_patch_header, parse_firmware, FirmwareFile};
use gbsc_core::FlashError;

fn image() -> FirmwareFile {
    let mut v = vec![0xE9, 0x01, 0x00, 0x20];
    let mut x = 0x2545_F491u32; // xorshift: a firmware-like image that zlib cannot shrink much
    v.extend((0..948_172).map(|_| {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        (x >> 24) as u8
    }));
    let mark = b"GBSCVER=3.0.0;";
    v[4096..4096 + mark.len()].copy_from_slice(mark);
    parse_firmware("gbsc.bin", &v).unwrap()
}

fn patched(fw: &FirmwareFile) -> Vec<u8> {
    let mut raw = fw.raw.clone();
    esp_patch_header(&mut raw);
    raw
}

fn stub() -> Stub {
    Stub { entry: 0x4010_E004, text: vec![0x11; 3000], text_start: 0x4010_E000, data: vec![0x22; 500], data_start: 0x3FFE_8000 }
}

fn opts(erase: Erase) -> EspOptions {
    EspOptions { baud: 460_800, erase }
}

fn run(dev: &mut FakeEsp, fw: Option<&FirmwareFile>, st: Option<&Stub>, erase: Erase) -> Result<gbsc_core::esp::flasher::EspReport, FlashError> {
    esp_flash(dev, fw, st, &opts(erase), &mut |_| {})
}

#[test]
fn flash_with_stub_writes_and_verifies() {
    let fw = image();
    let mut dev = FakeEsp::new();
    let r = run(&mut dev, Some(&fw), Some(&stub()), Erase::None).unwrap();
    let want = patched(&fw);
    assert_eq!(&dev.flash[..want.len()], &want[..]);
    assert!(r.used_stub);
    assert_eq!(r.md5_ok, Some(true));
    assert!(dev.stub_changed_baud); // 460800 while writing (the hard reset at the end returns to 115200)
}

#[test]
fn flash_without_stub_uses_rom_unverified() {
    let fw = image();
    let mut dev = FakeEsp::new();
    let r = run(&mut dev, Some(&fw), None, Erase::None).unwrap();
    let want = patched(&fw);
    assert_eq!(&dev.flash[..want.len()], &want[..]);
    assert!(!r.used_stub);
    assert_eq!(dev.baud, 115_200);
    assert_eq!(r.md5_ok, None);
    assert_eq!(r.readback_ok, None);
    assert!(!dev.erased_out_of_bounds);
}

#[test]
fn rom_erase_size_matches_esptool() {
    assert_eq!(rom_erase_size(0x0, 948_176), 884_736);
    assert_eq!(rom_erase_size(0x3F_C000, 0x4000), 8192);
    assert_eq!(rom_erase_size(0x1000, 0x1000), 4096);
    assert_eq!(rom_erase_size(0x0, 0x40_0000), 4_128_768);
}

#[test]
fn rom_wifi_erase_stays_in_bounds() {
    let mut dev = FakeEsp::new();
    dev.flash.fill(0x5A);
    run(&mut dev, None, None, Erase::Wifi).unwrap();
    assert!(!dev.erased_out_of_bounds);
    assert!(dev.flash[0x3F_C000..].iter().all(|&b| b == 0xFF));
    assert!(dev.flash[..0x3F_C000].iter().all(|&b| b == 0x5A));
}

#[test]
fn stub_without_greeting_falls_back_to_rom_after_a_reset() {
    let fw = image();
    let mut dev = FakeEsp::new();
    dev.stub_greets = false;
    let r = run(&mut dev, Some(&fw), Some(&stub()), Erase::None).unwrap();
    assert!(!r.used_stub);
    let want = patched(&fw);
    assert_eq!(&dev.flash[..want.len()], &want[..]);
}

#[test]
fn md5_mismatch_is_verify_error() {
    let fw = image();
    let mut dev = FakeEsp::new();
    dev.corrupt_byte_at = Some(0x1000);
    assert!(matches!(run(&mut dev, Some(&fw), Some(&stub()), Erase::None), Err(FlashError::Verify(_))));
}

#[test]
fn sync_retries() {
    let fw = image();
    let mut dev = FakeEsp::new();
    dev.ignore_syncs = 3;
    run(&mut dev, Some(&fw), Some(&stub()), Erase::None).unwrap();
}

#[test]
fn esp_port_gone_is_error() {
    let fw = image();
    let mut dev = FakeEsp::new();
    dev.die_after_bytes = Some(20_000);
    let r = run(&mut dev, Some(&fw), Some(&stub()), Erase::None);
    assert!(matches!(r, Err(FlashError::PortGone)), "{r:?}");
    // the same text serves both chips: the button step is the ADV Controller's only (the ESP8266 resets by itself)
    let text = r.unwrap_err().user_text();
    assert!(text.contains("the ADV Controller: hold its button"), "{text}");
    assert!(!text.contains("put the device back in bootloader mode"), "{text}");
}

#[test]
fn erase_wifi_only_touches_the_last_16k() {
    let mut dev = FakeEsp::new();
    dev.flash.fill(0x5A);
    run(&mut dev, None, Some(&stub()), Erase::Wifi).unwrap();
    assert!(dev.flash[0x3F_C000..FLASH_SIZE].iter().all(|&b| b == 0xFF));
    assert!(dev.flash[..0x3F_C000].iter().all(|&b| b == 0x5A));
}

#[test]
fn header_patched_to_dio() {
    let fw = image();
    let mut dev = FakeEsp::new();
    run(&mut dev, Some(&fw), Some(&stub()), Erase::None).unwrap();
    assert_eq!((dev.flash[2], dev.flash[3]), (0x02, 0x40));
}

#[test]
fn image_larger_than_the_user_area_is_refused_before_erasing() {
    // above 0x3FC000 the write would reach the RF calibration and WiFi sectors, whatever the erase choice
    let mut v = patched(&image());
    v.resize(0x3F_C000 + 0x1000, 0xA5);
    let fw = parse_firmware("big.bin", &v).unwrap();
    for erase in [Erase::None, Erase::User, Erase::All] {
        let mut dev = FakeEsp::new();
        dev.flash.fill(0x5A);
        let r = run(&mut dev, Some(&fw), Some(&stub()), erase);
        assert!(matches!(r, Err(FlashError::TooLarge { size, max: 0x3F_C000 }) if size == v.len()), "{r:?}");
        assert!(dev.flash.iter().all(|&b| b == 0x5A), "nothing may be erased or written ({erase:?})");
    }
    // the largest allowed image still goes through
    v.truncate(0x3F_C000);
    let fw = parse_firmware("max.bin", &v).unwrap();
    let mut dev = FakeEsp::new();
    run(&mut dev, Some(&fw), Some(&stub()), Erase::None).unwrap();
    assert_eq!(&dev.flash[..v.len()], &v[..]);
}

#[test]
fn a_block_that_unpacks_to_megabytes_gets_time_to_be_written() {
    // a long 0xFF tail compresses ~1000:1: one 16 KB block holds about 1 MB, which a slow flash writes for longer
    // than the 3 s default; esptool scales the wait with the block's unpacked size (40 s per MB)
    let mut v = patched(&image());
    v.truncate(64 * 1024);
    v.resize(64 * 1024 + 1024 * 1024, 0xFF);
    let fw = parse_firmware("tail.bin", &v).unwrap();
    let mut dev = FakeEsp::new();
    dev.write_secs_per_mb = 4.0;
    let r = run(&mut dev, Some(&fw), Some(&stub()), Erase::None);
    assert!(r.is_ok(), "{r:?}");
    assert_eq!(&dev.flash[..v.len()], &v[..]);
}
