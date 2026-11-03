// Firmware files next to the app: the newest of each kind is preselected
use gbsc_core::image::FirmwareKind;
use gbsc_core::release::{find_candidates, run_from_zip, translocated};
use std::io::Write;
use std::path::{Path, PathBuf};

fn esp(version: &str) -> Vec<u8> {
    let mut v = vec![0xE9, 0x01, 0x00, 0x20];
    v.resize(300 * 1024, 0x44);
    let mark = format!("GBSCVER={version};");
    v[4096..4096 + mark.len()].copy_from_slice(mark.as_bytes());
    v
}

fn adv() -> Vec<u8> {
    std::fs::read(PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../adv-controller/release/adv-controller.bin")).unwrap()
}

fn gz(data: &[u8]) -> Vec<u8> {
    let mut e = flate2::write::GzEncoder::new(Vec::new(), flate2::Compression::fast());
    e.write_all(data).unwrap();
    e.finish().unwrap()
}

fn dir(name: &str) -> PathBuf {
    let d = std::env::temp_dir().join(format!("gbsc-release-{name}-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&d);
    std::fs::create_dir_all(&d).unwrap();
    d
}

fn put(d: &Path, name: &str, data: &[u8]) {
    std::fs::write(d.join(name), data).unwrap();
}

#[test]
fn finds_esp_and_adv() {
    let d = dir("both");
    put(&d, "gbsc-pro-3.0.0.bin", &esp("3.0.0"));
    put(&d, "adv-controller-3.0.0.bin", &adv());
    let c = find_candidates(&d);
    assert_eq!(c.esp.unwrap().version.as_deref(), Some("3.0.0"));
    assert_eq!(c.adv.unwrap().kind, FirmwareKind::Adv);
}

#[test]
fn newest_by_frozen_order() {
    let d = dir("newest");
    put(&d, "gbsc-pro-3.0.0.bin", &esp("3.0.0"));
    put(&d, "gbsc-pro-3.0.1-beta1.bin", &esp("3.0.1-beta1"));
    put(&d, "gbsc-pro-3.0.1-dev4.bin", &esp("3.0.1-dev4"));
    assert_eq!(find_candidates(&d).esp.unwrap().version.as_deref(), Some("3.0.1-beta1"));
}

#[test]
fn ignores_unmarked_files() {
    let d = dir("unmarked");
    put(&d, "upstream.bin", &vec![0xE9; 400 * 1024]);
    put(&d, "notes.txt", b"hello");
    let c = find_candidates(&d);
    assert!(c.esp.is_none() && c.adv.is_none());
}

#[test]
fn usb_gets_the_bin_and_wifi_the_gz_of_the_same_version() {
    let d = dir("gz");
    put(&d, "gbsc-pro-3.0.0.bin", &esp("3.0.0"));
    put(&d, "gbsc-pro-3.0.0.bin.gz", &gz(&esp("3.0.0")));
    let c = find_candidates(&d);
    assert_eq!(c.esp.unwrap().name, "gbsc-pro-3.0.0.bin");          // the README's USB file
    assert_eq!(c.esp_wifi.unwrap().name, "gbsc-pro-3.0.0.bin.gz");  // smaller over WiFi
}

#[test]
fn one_esp_file_serves_both_paths() {
    let d = dir("onegz");
    put(&d, "gbsc-pro-3.0.0.bin.gz", &gz(&esp("3.0.0")));
    let c = find_candidates(&d);
    assert_eq!(c.esp.unwrap().name, "gbsc-pro-3.0.0.bin.gz");       // USB gunzips it
    assert_eq!(c.esp_wifi.unwrap().name, "gbsc-pro-3.0.0.bin.gz");
}

#[test]
fn empty_dir_has_none() {
    let c = find_candidates(&dir("empty"));
    assert!(c.esp.is_none() && c.adv.is_none());
}

#[test]
fn translocated_app_detected() {
    assert!(translocated(Path::new("/private/var/folders/x/T/AppTranslocation/1234/d/GBSC PRO Flasher.app")));
    assert!(!translocated(Path::new("/Users/me/Downloads/gbsc-pro-3.0.0")));
}

#[test]
fn appimage_folder_is_where_the_file_is() {
    use gbsc_core::release::app_dir_for;
    let exe = Path::new("/tmp/.mount_gbscXYZ/usr/bin/gbsc-pro-flasher");
    assert_eq!(app_dir_for(exe, Some("/home/me/gbsc-pro-3.0.0/gbsc-pro-flasher.AppImage")), PathBuf::from("/home/me/gbsc-pro-3.0.0"));
    assert_eq!(app_dir_for(exe, None), PathBuf::from("/tmp/.mount_gbscXYZ/usr/bin"));
    let mac = Path::new("/Users/me/gbsc-pro-3.0.0/GBSC PRO Flasher.app/Contents/MacOS/gbsc-pro-flasher");
    assert_eq!(app_dir_for(mac, None), PathBuf::from("/Users/me/gbsc-pro-3.0.0"));
}

#[test]
fn skips_huge_files() {
    let d = dir("huge");
    let mut big = esp("3.0.5");
    big.resize(9 * 1024 * 1024, 0);
    put(&d, "disk-image.bin", &big);
    put(&d, "gbsc-pro-3.0.0.bin", &esp("3.0.0"));
    assert_eq!(find_candidates(&d).esp.unwrap().version.as_deref(), Some("3.0.0"));
}

#[test]
fn empty_gzip_next_to_the_app_is_skipped() {
    let d = dir("emptygz");
    put(&d, "broken.bin.gz", &gz(&[]));
    put(&d, "gbsc-pro-3.0.0.bin", &esp("3.0.0"));
    assert_eq!(find_candidates(&d).esp.unwrap().version.as_deref(), Some("3.0.0"));
}

#[test]
fn run_from_inside_a_zip_detected() {
    // Windows 11 "Run" from Explorer's zip view copies only the exe (Windows 11 ARM64); older Windows
    // used Temp1_<zip name>
    let w11 = r"C:\Users\me\AppData\Local\Temp\b746ea7a-a2a8-4209-85fd-3f82c6ef7ce1_gbsc-pro-3.0.0-dev0.zip.ce1\gbsc-pro-3.0.0-dev0";
    assert!(run_from_zip(Path::new(w11)));
    assert!(run_from_zip(Path::new(r"C:\Users\me\AppData\Local\Temp\Temp1_gbsc-pro-3.0.0.zip\gbsc-pro-3.0.0")));
    assert!(!run_from_zip(Path::new(r"C:\Users\me\Desktop\gbsc-pro-3.0.0")));
    assert!(!run_from_zip(Path::new(r"C:\Users\me\Downloads\gbsc-pro-3.0.0")));
    assert!(!run_from_zip(Path::new("/Users/me/Downloads/gbsc-pro-3.0.0")));
}
