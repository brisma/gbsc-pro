// Firmware file parsing: ADVC header, GBSCVER marker, gzip, ESP header patch, kind by size
use gbsc_core::image::{esp_patch_header, parse_firmware, adv_header, FirmwareKind, ImageError};
use std::io::Write;
use std::path::PathBuf;

fn repo() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../..")
}

fn adv_image() -> Vec<u8> {
    std::fs::read(repo().join("adv-controller/release/adv-controller.bin")).unwrap()
}

fn esp_image() -> Vec<u8> {
    let mut v = vec![0xE9, 0x01, 0x00, 0x20];
    v.resize(300 * 1024, 0x11);
    let mark = b"GBSCVER=3.0.0;";
    v[4096..4096 + mark.len()].copy_from_slice(mark);
    v
}

fn gzip(data: &[u8]) -> Vec<u8> {
    let mut e = flate2::write::GzEncoder::new(Vec::new(), flate2::Compression::default());
    e.write_all(data).unwrap();
    e.finish().unwrap()
}

#[test]
fn adv_release_image_parses() {
    let raw = adv_image();
    let want = std::fs::read_to_string(repo().join("adv-controller/VERSION")).unwrap();
    let h = adv_header(&raw).unwrap();
    assert!(h.crc_ok);
    let f = parse_firmware("adv-controller.bin", &raw).unwrap();
    assert_eq!(f.kind, FirmwareKind::Adv);
    assert_eq!(f.version.as_deref(), Some(want.trim()));
    assert_eq!(f.adv_crc, Some(h.image_crc32));
    assert!(!f.by_size_only);
}

#[test]
fn adv_length_mismatch_is_error() {
    let mut raw = adv_image();
    raw.extend_from_slice(&[0, 0, 0, 0]);
    assert!(matches!(parse_firmware("a.bin", &raw), Err(ImageError::AdvLength { .. })));
}

#[test]
fn adv_bad_crc_is_flagged() {
    let mut raw = adv_image();
    raw[0x800] ^= 0xFF;
    assert!(!adv_header(&raw).unwrap().crc_ok);
    assert!(matches!(parse_firmware("a.bin", &raw), Err(ImageError::AdvCrc)));
}

#[test]
fn esp_marker_gives_version() {
    let f = parse_firmware("gbsc.bin", &esp_image()).unwrap();
    assert_eq!(f.kind, FirmwareKind::Esp);
    assert_eq!(f.version.as_deref(), Some("3.0.0"));
    assert!(!f.gzipped);
}

#[test]
fn esp_image_from_gzip_is_raw() {
    let raw = esp_image();
    let gz = gzip(&raw);
    let f = parse_firmware("gbsc.bin.gz", &gz).unwrap();
    assert_eq!(f.raw, raw);
    assert_eq!(f.original, gz);
    assert!(f.gzipped);
    assert_eq!(f.version.as_deref(), Some("3.0.0"));
}

#[test]
fn esp_patch_sets_dio_4mb() {
    let mut raw = esp_image();
    esp_patch_header(&mut raw);
    assert_eq!((raw[2], raw[3]), (0x02, 0x40));
    let mut other = vec![0x12u8, 0x00, 0x00, 0x00];
    esp_patch_header(&mut other);
    assert_eq!(other, vec![0x12, 0x00, 0x00, 0x00]);
}

#[test]
fn unknown_small_file_is_adv_by_size() {
    let f = parse_firmware("x.bin", &vec![0u8; 30 * 1024]).unwrap();
    assert_eq!(f.kind, FirmwareKind::Adv);
    assert!(f.by_size_only);
    assert_eq!(f.version, None);
}

#[test]
fn empty_gzip_content_is_an_error_not_a_panic() {
    let r = parse_firmware("empty.bin.gz", &gzip(&[]));
    assert!(matches!(r, Err(ImageError::Empty)), "{r:?}");
}

#[test]
fn gzip_over_the_limit_is_refused_not_cut() {
    let mut big = esp_image();
    big.resize(8 * 1024 * 1024 + 1, 0xFF);
    let r = parse_firmware("big.bin.gz", &gzip(&big));
    assert!(matches!(r, Err(ImageError::TooLarge { .. })), "{:?}", r.map(|f| f.raw.len()));
}
