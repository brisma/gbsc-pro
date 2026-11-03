//! Firmware files: what a file is (ADV Controller or ESP8266 image), its version, and the bytes to flash.
//! ADV Controller images carry the ADVC header at app offset 0x400 (adv-controller/tools/fw-header.py); ESP images
//! carry the marker "GBSCVER=<version>;" (GBS_FW_VERSION_MARKER). Files with neither are classified by size, as the
//! Python flasher did, so factory and upstream images stay flashable after a warning.
use crc::{Crc, CRC_32_ISO_HDLC};
use std::fmt;
use std::io::Read;

const ADV_HEADER_OFFSET: usize = 0x400;
const ADV_HEADER_SIZE: usize = 4 + 2 + 2 + 12 + 4 + 4;
const ADV_MAX_SIZE: usize = 256 * 1024;
const GZIP_MAX_OUT: u64 = 8 * 1024 * 1024; // larger than any 4 MB flash image: a bomb guard
const CRC32: Crc<u32> = Crc::<u32>::new(&CRC_32_ISO_HDLC);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FirmwareKind {
    Adv,
    Esp,
    Unknown,
}

#[derive(Debug, Clone)]
pub struct FirmwareFile {
    pub kind: FirmwareKind,
    pub version: Option<String>,
    /// image_crc32 of the ADVC header (= the build id the ADV Controller reports)
    pub adv_crc: Option<u32>,
    /// the flashable image (gunzipped)
    pub raw: Vec<u8>,
    /// the file's own bytes (a .gz stays compressed for WiFi uploads)
    pub original: Vec<u8>,
    pub gzipped: bool,
    pub name: String,
    /// neither header nor marker: the kind comes from the size only
    pub by_size_only: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AdvHeader {
    pub header_version: u16,
    pub version: String,
    pub image_len: u32,
    pub image_crc32: u32,
    pub crc_ok: bool,
}

#[derive(Debug)]
pub enum ImageError {
    Empty,
    Gzip(String),
    AdvLength { header: u32, file: usize },
    AdvCrc,
    /// the gunzipped image is larger than any flash image (`max` bytes)
    TooLarge { max: u64 },
}

impl fmt::Display for ImageError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ImageError::Empty => write!(f, "the file is empty"),
            ImageError::Gzip(e) => write!(f, "the file is not a valid gzip image ({e})"),
            ImageError::AdvLength { header, file } => write!(
                f,
                "the ADV Controller image is damaged: its header says {header} bytes, the file has {file}"
            ),
            ImageError::AdvCrc => write!(f, "the ADV Controller image is damaged: its checksum does not match"),
            ImageError::TooLarge { max } => write!(f, "the unpacked image is larger than {} MB: it is not a GBSC firmware", max / (1024 * 1024)),
        }
    }
}

impl std::error::Error for ImageError {}

fn u32_le(b: &[u8], at: usize) -> u32 {
    u32::from_le_bytes([b[at], b[at + 1], b[at + 2], b[at + 3]])
}

/// The ADVC header of an ADV Controller image, or None when the file has none.
pub fn adv_header(raw: &[u8]) -> Option<AdvHeader> {
    let h = raw.get(ADV_HEADER_OFFSET..ADV_HEADER_OFFSET + ADV_HEADER_SIZE)?;
    if &h[0..4] != b"ADVC" {
        return None;
    }
    let version = String::from_utf8_lossy(&h[8..20]).trim_end_matches('\0').to_string();
    let image_len = u32_le(h, 20);
    let image_crc32 = u32_le(h, 24);
    let mut body = raw.to_vec();
    body[ADV_HEADER_OFFSET + ADV_HEADER_SIZE - 4..ADV_HEADER_OFFSET + ADV_HEADER_SIZE].fill(0);
    Some(AdvHeader {
        header_version: u16::from_le_bytes([h[4], h[5]]),
        version,
        image_len,
        image_crc32,
        crc_ok: CRC32.checksum(&body) == image_crc32,
    })
}

/// The version in the marker "GBSCVER=<version>;" of an ESP image.
pub fn gbscver(raw: &[u8]) -> Option<String> {
    const MARK: &[u8] = b"GBSCVER=";
    let ok = |b: &u8| b.is_ascii_alphanumeric() || *b == b'.' || *b == b'-';
    let mut from = 0;
    while let Some(p) = raw[from..].windows(MARK.len()).position(|w| w == MARK) {
        let start = from + p + MARK.len();
        let len = raw[start..].iter().take_while(|b| ok(b)).count();
        if len > 0 && raw.get(start + len) == Some(&b';') {
            return Some(String::from_utf8_lossy(&raw[start..start + len]).into_owned());
        }
        from = start;
    }
    None
}

/// Flash mode DIO and 4 MB / 40 MHz in the image header, as esptool writes it with --flash_mode dio
/// --flash_size 4MB --flash_freq 40m (the firmware is built qio; the Python flasher forced DIO).
pub fn esp_patch_header(raw: &mut [u8]) {
    if raw.len() >= 4 && raw[0] == 0xE9 {
        raw[2] = 0x02;
        raw[3] = 0x40;
    }
}

fn gunzip(bytes: &[u8]) -> Result<Vec<u8>, ImageError> {
    let mut out = Vec::new();
    flate2::read::GzDecoder::new(bytes)
        .take(GZIP_MAX_OUT + 1) // one byte more tells a file at the limit from a larger one
        .read_to_end(&mut out)
        .map_err(|e| ImageError::Gzip(e.to_string()))?;
    if out.len() as u64 > GZIP_MAX_OUT {
        return Err(ImageError::TooLarge { max: GZIP_MAX_OUT });
    }
    Ok(out)
}

/// What a firmware file is. `name` is the file name (shown, and sent as the upload's file name).
pub fn parse_firmware(name: &str, bytes: &[u8]) -> Result<FirmwareFile, ImageError> {
    if bytes.is_empty() {
        return Err(ImageError::Empty);
    }
    let gzipped = bytes.starts_with(&[0x1F, 0x8B]);
    let raw = if gzipped { gunzip(bytes)? } else { bytes.to_vec() };
    if raw.is_empty() {
        return Err(ImageError::Empty); // a .gz with nothing inside
    }
    let mut f = FirmwareFile {
        kind: FirmwareKind::Unknown,
        version: None,
        adv_crc: None,
        raw,
        original: bytes.to_vec(),
        gzipped,
        name: name.to_string(),
        by_size_only: false,
    };
    if let Some(h) = adv_header(&f.raw) {
        if h.image_len as usize != f.raw.len() {
            return Err(ImageError::AdvLength { header: h.image_len, file: f.raw.len() });
        }
        if !h.crc_ok {
            return Err(ImageError::AdvCrc);
        }
        f.kind = FirmwareKind::Adv;
        f.version = Some(h.version);
        f.adv_crc = Some(h.image_crc32);
    } else if let Some(v) = gbscver(&f.raw) {
        f.kind = FirmwareKind::Esp;
        f.version = Some(v);
    } else {
        f.by_size_only = true;
        f.kind = if f.raw.len() < ADV_MAX_SIZE && f.raw[0] != 0xE9 { FirmwareKind::Adv } else { FirmwareKind::Esp };
    }
    Ok(f)
}
