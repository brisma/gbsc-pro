//! Loader command packets: request `00 op len_le16 checksum_le32 data`, response `01 op len_le16 value_le32 data`
//! where the last two data bytes are status and error (ROM and stub alike on the ESP8266).

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum Op {
    FlashBegin = 0x02,
    FlashData = 0x03,
    FlashEnd = 0x04,
    MemBegin = 0x05,
    MemEnd = 0x06,
    MemData = 0x07,
    Sync = 0x08,
    ReadReg = 0x0A,
    SpiSetParams = 0x0B,
    ChangeBaud = 0x0F,
    DeflBegin = 0x10,
    DeflData = 0x11,
    DeflEnd = 0x12,
    FlashMd5 = 0x13,
    EraseFlash = 0xD0,
    EraseRegion = 0xD1,
}

pub fn request(op: Op, data: &[u8], checksum: u32) -> Vec<u8> {
    let mut p = Vec::with_capacity(8 + data.len());
    p.push(0x00);
    p.push(op as u8);
    p.extend_from_slice(&(data.len() as u16).to_le_bytes());
    p.extend_from_slice(&checksum.to_le_bytes());
    p.extend_from_slice(data);
    p
}

/// Checksum of a data block (FLASH_DATA, MEM_DATA, DEFL_DATA): XOR of the bytes, seeded 0xEF.
pub fn data_checksum(data: &[u8]) -> u32 {
    data.iter().fold(0xEFu8, |c, &b| c ^ b) as u32
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Response {
    pub op: u8,
    pub value: u32,
    pub data: Vec<u8>,
}

pub fn parse_response(frame: &[u8]) -> Option<Response> {
    if frame.len() < 8 || frame[0] != 0x01 {
        return None;
    }
    let len = u16::from_le_bytes([frame[2], frame[3]]) as usize;
    let value = u32::from_le_bytes([frame[4], frame[5], frame[6], frame[7]]);
    let data = frame.get(8..8 + len).unwrap_or(&frame[8..]).to_vec();
    Some(Response { op: frame[1], value, data })
}

/// Ok when the status byte is 0; Err(error code) otherwise.
pub fn status_ok(r: &Response) -> Result<(), u8> {
    match r.data.len() {
        n if n >= 2 && r.data[n - 2] != 0 => Err(r.data[n - 1]),
        n if n >= 2 => Ok(()),
        _ => Err(0xFF),
    }
}
