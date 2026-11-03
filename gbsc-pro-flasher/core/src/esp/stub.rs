//! esptool's ESP8266 flasher stub: code and data loaded into the chip's RAM, then started. It ships as a separate
//! file (GPL-2.0-or-later, resources/STUB-SOURCE.md), never compiled into the app.

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Stub {
    pub entry: u32,
    pub text: Vec<u8>,
    pub text_start: u32,
    pub data: Vec<u8>,
    pub data_start: u32,
}

use base64::Engine;
use std::path::{Path, PathBuf};

pub const STUB_FILE: &str = "esp8266-stub.json";

/// The stub in esptool's JSON format (entry, text and data in base64, their load addresses), or None when the file
/// is missing or damaged: the flasher then uses the slow ROM path.
pub fn load_stub(path: &Path) -> Option<Stub> {
    let v: serde_json::Value = serde_json::from_slice(&std::fs::read(path).ok()?).ok()?;
    let num = |k: &str| v.get(k)?.as_u64().and_then(|n| u32::try_from(n).ok());
    let b64 = |k: &str| base64::engine::general_purpose::STANDARD.decode(v.get(k)?.as_str()?).ok();
    let s = Stub { entry: num("entry")?, text: b64("text")?, text_start: num("text_start")?, data: b64("data")?, data_start: num("data_start")? };
    (!s.text.is_empty() && s.entry != 0).then_some(s)
}

/// Where the stub is: the app's bundled resources, next to the executable, or next to the app (release zip).
pub fn find_stub(resource_dir: Option<&Path>) -> Option<PathBuf> {
    let exe_dir = std::env::current_exe().ok().and_then(|p| p.parent().map(Path::to_path_buf));
    [resource_dir.map(Path::to_path_buf), exe_dir, Some(crate::release::app_dir())]
        .into_iter()
        .flatten()
        .map(|d| d.join(STUB_FILE))
        .find(|p| p.is_file())
}
