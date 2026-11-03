// The stub file next to the app: parsed when valid, None when missing or damaged
use gbsc_core::esp::stub::load_stub;
use std::path::PathBuf;

fn resource() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../resources/esp8266-stub.json")
}

#[test]
fn stub_resource_parses() {
    let s = load_stub(&resource()).unwrap();
    assert_eq!(s.entry, 0x4010_D004); // esptool v4.8.1 legacy stub 1.3.0
    assert_eq!(s.text_start, 0x4010_D000);
    assert_ne!(s.data_start, 0);
    assert_eq!(s.text.len(), 8248);
    assert_eq!(s.data.len(), 768);
}

#[test]
fn stub_missing_is_none() {
    assert!(load_stub(&PathBuf::from("/nonexistent/esp8266-stub.json")).is_none());
}

#[test]
fn stub_corrupt_is_none() {
    let dir = std::env::temp_dir().join(format!("gbsc-stub-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let text = std::fs::read_to_string(resource()).unwrap();
    let cut = dir.join("cut.json");
    std::fs::write(&cut, &text[..text.len() / 2]).unwrap();
    assert!(load_stub(&cut).is_none());
    let bad = dir.join("bad.json");
    std::fs::write(&bad, r#"{"entry": 1, "text": "@@@", "text_start": 2, "data": "", "data_start": 3}"#).unwrap();
    assert!(load_stub(&bad).is_none());
}
