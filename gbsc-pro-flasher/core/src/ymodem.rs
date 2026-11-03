//! ADV Controller flashing through its bootloader HCMGBoot (115200 8N1, as the Python flasher): 0x55 answers 48 bytes
//! of chip information, '1' enters download mode, then the image goes as YMODEM with 128-byte packets and
//! CRC-16/XMODEM. HCMGBoot returns no checksum of what it wrote: the version is checked over WiFi afterwards.
use crate::error::FlashError;
use crate::image::FirmwareFile;
use crate::link::SerialLink;
use crate::progress::{Progress, ProgressFn};
use std::time::{Duration, Instant};

const SOH: u8 = 0x01;
const EOT: u8 = 0x04;
const ACK: u8 = 0x06;
const NAK: u8 = 0x15;
const CAN: u8 = 0x18;
const CPMEOF: u8 = 0x1A;
const PACKET: usize = 128;
const RETRIES: u32 = 10;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AdvChipInfo {
    pub chip: String,
    pub unique_id: String,
}

/// Waits of the protocol (the defaults are the Python flasher's; tests shorten them).
#[derive(Debug, Clone)]
pub struct AdvTimings {
    pub c_wait: Duration,
    pub ack_wait: Duration,
    pub drain: Duration,
}

impl Default for AdvTimings {
    fn default() -> Self {
        AdvTimings { c_wait: Duration::from_secs(10), ack_wait: Duration::from_secs(5), drain: Duration::from_millis(300) }
    }
}

fn read_byte(link: &mut dyn SerialLink, deadline: Instant) -> Result<Option<u8>, FlashError> {
    let mut b = [0u8; 1];
    while Instant::now() < deadline {
        if link.read(&mut b, deadline.saturating_duration_since(Instant::now()).min(Duration::from_millis(50)))? == 1 {
            return Ok(Some(b[0]));
        }
    }
    Ok(None)
}

fn wait_for_c(link: &mut dyn SerialLink, wait: Duration) -> Result<bool, FlashError> {
    let deadline = Instant::now() + wait;
    while let Some(b) = read_byte(link, deadline)? {
        if b == b'C' {
            return Ok(true);
        }
    }
    Ok(false)
}

enum Answer {
    Ack,
    Nak,
    Cancel,
    Timeout,
}

fn wait_for_ack(link: &mut dyn SerialLink, wait: Duration) -> Result<Answer, FlashError> {
    let deadline = Instant::now() + wait;
    while let Some(b) = read_byte(link, deadline)? {
        match b {
            ACK => return Ok(Answer::Ack),
            NAK => return Ok(Answer::Nak),
            CAN => return Ok(Answer::Cancel),
            _ => {}
        }
    }
    Ok(Answer::Timeout)
}

fn crc16(data: &[u8]) -> u16 {
    let mut crc: u16 = 0;
    for &b in data {
        crc ^= (b as u16) << 8;
        for _ in 0..8 {
            crc = if crc & 0x8000 != 0 { (crc << 1) ^ 0x1021 } else { crc << 1 };
        }
    }
    crc
}

fn packet(seq: u8, data: &[u8], pad: u8) -> Vec<u8> {
    let mut body = data.to_vec();
    body.resize(PACKET, pad);
    let mut p = vec![SOH, seq, !seq];
    p.extend_from_slice(&body);
    p.extend_from_slice(&crc16(&body).to_be_bytes());
    p
}

/// Sends one packet until it is acknowledged (NAK: resend, up to 10 times). No answer stops the transfer, as the
/// Python flasher: the packet may have been written with its ACK lost, and HCMGBoot (no checksum of what it wrote)
/// is not known to drop a repeated sequence number, so a resend could write it twice.
fn send(link: &mut dyn SerialLink, p: &[u8], t: &AdvTimings) -> Result<(), FlashError> {
    for _ in 0..RETRIES {
        link.write_all(p)?;
        match wait_for_ack(link, t.ack_wait)? {
            Answer::Ack => return Ok(()),
            Answer::Cancel => return Err(FlashError::Rejected("cancelled by the bootloader".into())),
            Answer::Nak => continue,
            Answer::Timeout => return Err(FlashError::Rejected(format!("no answer to packet {}", p[1]))),
        }
    }
    Err(FlashError::Rejected(format!("packet {} refused {RETRIES} times", p[1])))
}

/// Chip information from HCMGBoot (0x55 → 48 bytes), or None when it does not answer within 500 ms.
pub fn adv_chip_info(link: &mut dyn SerialLink) -> Option<AdvChipInfo> {
    link.clear_input().ok()?;
    link.write_all(&[0x55]).ok()?;
    let mut info = Vec::with_capacity(48);
    let deadline = Instant::now() + Duration::from_millis(500);
    let mut buf = [0u8; 48];
    while info.len() < 48 && Instant::now() < deadline {
        let n = link.read(&mut buf[..48 - info.len()], Duration::from_millis(50)).ok()?;
        info.extend_from_slice(&buf[..n]);
    }
    if info.len() < 48 {
        return None;
    }
    let chip: String = info[..6].iter().map(|&b| b as char).filter(|c| c.is_ascii_graphic()).collect();
    let unique_id = info[16..32].iter().map(|b| format!("{b:02X}")).collect::<Vec<_>>().join(" ");
    Some(AdvChipInfo { chip, unique_id })
}

pub fn adv_flash(link: &mut dyn SerialLink, fw: &FirmwareFile, progress: ProgressFn) -> Result<Option<AdvChipInfo>, FlashError> {
    adv_flash_with(link, fw, &AdvTimings::default(), progress)
}

pub fn adv_flash_with(
    link: &mut dyn SerialLink,
    fw: &FirmwareFile,
    t: &AdvTimings,
    progress: ProgressFn,
) -> Result<Option<AdvChipInfo>, FlashError> {
    let image = &fw.raw;
    progress(Progress::Status(format!("Firmware {} ({} bytes)", fw.name, image.len())));
    // as macOS, Linux and pyserial open a port: serialport on Windows leaves DTR low, and a CDC device may stay quiet
    let _ = link.set_dtr(true);
    let _ = link.set_rts(true);
    let info = adv_chip_info(link);
    if info.is_some() {
        // its first bytes are not a stable chip name (seen "388H4P" and "HCMGBo"): not shown
        progress(Progress::Status("Bootloader answered".into()));
    }
    progress(Progress::Status("Entering download mode".into()));
    link.clear_input()?;
    link.write_all(b"1")?;
    let until = Instant::now() + t.drain;
    let mut sink = [0u8; 64];
    while Instant::now() < until {
        link.read(&mut sink, Duration::from_millis(20))?;
    }
    if !wait_for_c(link, t.c_wait)? {
        return Err(FlashError::NoBootloader);
    }
    let size = image.len().to_string();
    let room = PACKET - size.len() - 2; // name NUL size NUL in one 128-byte packet
    let mut header: Vec<u8> = fw.name.bytes().take(room).collect();
    header.push(0);
    header.extend_from_slice(size.as_bytes());
    header.push(0);
    send(link, &packet(0, &header, 0), t)?;
    if !wait_for_c(link, t.c_wait)? {
        return Err(FlashError::Rejected("no data request after the header".into()));
    }
    progress(Progress::Status("Writing".into()));
    let total = image.len() as u64;
    for (i, chunk) in image.chunks(PACKET).enumerate() {
        send(link, &packet((i + 1) as u8, chunk, CPMEOF), t)?;
        progress(Progress::Bytes { done: ((i + 1) * PACKET).min(image.len()) as u64, total });
    }
    link.write_all(&[EOT])?;
    if !matches!(wait_for_ack(link, t.ack_wait)?, Answer::Ack) {
        link.write_all(&[EOT])?;
        if !matches!(wait_for_ack(link, t.ack_wait)?, Answer::Ack) {
            return Err(FlashError::Rejected("no acknowledgement of the end of the transfer".into()));
        }
    }
    // the closing empty packet: sent once, its answer does not matter (the image is complete), as the Python flasher
    if wait_for_c(link, Duration::from_secs(2).min(t.c_wait))? {
        link.write_all(&packet(0, &[], 0))?;
        let _ = wait_for_ack(link, t.ack_wait.min(Duration::from_secs(1)))?;
    }
    progress(Progress::Done("ADV Controller written".into()));
    Ok(info)
}
