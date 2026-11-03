//! ESP8266 flashing (esptool v4.8.1 behaviour): classic DTR/RTS reset into the ROM loader and SYNC at 115200;
//! with the stub: load it into RAM, CHANGE_BAUD, deflate writes in 16 KB blocks and an MD5 check on the chip;
//! without it: ROM writes of 1 KB blocks at 115200 (the ESP8266 ROM can neither inflate, hash nor read the flash,
//! so that path is unverified beyond the per-block checksum). ROM FLASH_BEGIN sizes go through rom_erase_size, the
//! workaround esptool applies for the ROM's erase-size bug.
use super::cmd::{data_checksum, parse_response, request, status_ok, Op, Response};
use super::slip::{slip_encode, SlipDecoder};
use super::stub::Stub;
use crate::error::FlashError;
use crate::image::{esp_patch_header, FirmwareFile};
use crate::link::SerialLink;
use crate::progress::{Progress, ProgressFn};
use md5::{Digest, Md5};
use std::io::Write;
use std::time::{Duration, Instant};

const ROM_BAUD: u32 = 115_200;
const FLASH_SIZE: u32 = 4 * 1024 * 1024;
const SECTOR: u32 = 4096;
const RAM_BLOCK: usize = 0x1800;
const ROM_WRITE_BLOCK: usize = 0x400;
const STUB_WRITE_BLOCK: usize = 0x4000;
const USER_AREA: (u32, u32) = (0x00_0000, 0x3F_C000); // sketch, LittleFS, EEPROM (the former Python flasher's ESP_USER_AREA_*)
const WIFI_AREA: (u32, u32) = (0x3F_C000, 0x00_4000); // RF_CAL + SDK system params (WiFi credentials)
const CHIP_DETECT_MAGIC_REG: u32 = 0x4000_1000;
const DEFAULT_TIMEOUT: Duration = Duration::from_secs(3);

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Erase {
    None,
    User,
    Wifi,
    All,
}

#[derive(Debug, Clone)]
pub struct EspOptions {
    pub baud: u32,
    pub erase: Erase,
}

impl Default for EspOptions {
    fn default() -> Self {
        EspOptions { baud: 460_800, erase: Erase::None }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct EspReport {
    pub used_stub: bool,
    pub md5_ok: Option<bool>,
    pub readback_ok: Option<bool>,
    pub seconds: f32,
}

/// The size to ask the ROM's FLASH_BEGIN for, so that it erases `size` bytes from `offset` (esptool
/// ESP8266ROM.get_erase_size: the ROM erases the head sectors of the first 64 KB block twice over).
pub fn rom_erase_size(offset: u32, size: u32) -> u32 {
    let n = size.div_ceil(SECTOR);
    let start = offset / SECTOR;
    let head = (16 - start % 16).min(n);
    if n < 2 * head {
        n.div_ceil(2) * SECTOR
    } else {
        (n - head) * SECTOR
    }
}

fn per_mb(secs_per_mb: f32, bytes: usize) -> Duration {
    DEFAULT_TIMEOUT.max(Duration::from_secs_f32(secs_per_mb * bytes as f32 / 1e6))
}

/// The bytes one block of the zlib stream unpacks to (`z` carries the stream state from block to block).
fn unpacked_len(z: &mut flate2::Decompress, block: &[u8], sink: &mut [u8]) -> usize {
    let start = z.total_out();
    let mut input = block;
    loop {
        let (before_in, before_out) = (z.total_in(), z.total_out());
        if z.decompress(input, sink, flate2::FlushDecompress::None).is_err() {
            break; // our own stream: cannot happen; the default wait then applies
        }
        let used = (z.total_in() - before_in) as usize;
        input = &input[used..];
        let made = (z.total_out() - before_out) as usize;
        if (input.is_empty() && made < sink.len()) || (used == 0 && made == 0) {
            break;
        }
    }
    (z.total_out() - start) as usize
}

fn words(v: &[u32]) -> Vec<u8> {
    v.iter().flat_map(|w| w.to_le_bytes()).collect()
}

struct Loader<'a> {
    link: &'a mut dyn SerialLink,
    dec: SlipDecoder,
    pending: std::collections::VecDeque<u8>, // bytes read past the end of the last frame
    stub: bool,
}

impl Loader<'_> {
    fn frame(&mut self, deadline: Instant) -> Result<Option<Vec<u8>>, FlashError> {
        let mut buf = [0u8; 256];
        loop {
            while let Some(b) = self.pending.pop_front() {
                if let Some(f) = self.dec.push(b) {
                    return Ok(Some(f));
                }
            }
            if Instant::now() >= deadline {
                return Ok(None);
            }
            let wait = deadline.saturating_duration_since(Instant::now()).min(Duration::from_millis(20));
            let n = self.link.read(&mut buf, wait)?;
            self.pending.extend(&buf[..n]);
        }
    }

    /// Sends a command and waits for its response (other frames, e.g. extra SYNC replies, are skipped).
    fn command(&mut self, op: Op, data: &[u8], checksum: u32, timeout: Duration) -> Result<Response, FlashError> {
        self.link.write_all(&slip_encode(&request(op, data, checksum)))?;
        self.response(op, timeout)
    }

    fn response(&mut self, op: Op, timeout: Duration) -> Result<Response, FlashError> {
        let deadline = Instant::now() + timeout;
        while let Some(f) = self.frame(deadline)? {
            if let Some(r) = parse_response(&f).filter(|r| r.op == op as u8) {
                return match status_ok(&r) {
                    Ok(()) => Ok(r),
                    Err(e) => Err(FlashError::Rejected(format!("{op:?} failed with error 0x{e:02X}"))),
                };
            }
        }
        Err(FlashError::Rejected(format!("no answer to {op:?}")))
    }

    fn drain(&mut self, wait: Duration) -> Result<(), FlashError> {
        let deadline = Instant::now() + wait;
        while self.frame(deadline)?.is_some() {}
        Ok(())
    }

    /// esptool ClassicReset: EN low, then IO0 low while EN goes high, then IO0 high.
    fn reset_into_bootloader(&mut self) -> Result<(), FlashError> {
        self.link.set_dtr(false)?;
        self.link.set_rts(true)?;
        std::thread::sleep(Duration::from_millis(100));
        self.link.set_dtr(true)?;
        self.link.set_rts(false)?;
        std::thread::sleep(Duration::from_millis(50));
        self.link.set_dtr(false)?;
        Ok(())
    }

    fn sync(&mut self) -> Result<(), FlashError> {
        let mut data = vec![0x07, 0x07, 0x12, 0x20];
        data.extend([0x55; 32]);
        for _attempt in 0..7 {
            self.reset_into_bootloader()?;
            self.link.clear_input()?;
            self.dec = SlipDecoder::default();
            self.pending.clear();
            for _ in 0..3 {
                match self.command(Op::Sync, &data, 0, Duration::from_millis(150)) {
                    Ok(_) => return self.drain(Duration::from_millis(100)),
                    Err(FlashError::PortGone) => return Err(FlashError::PortGone),
                    Err(_) => continue,
                }
            }
        }
        Err(FlashError::NoBootloader)
    }

    fn load_stub(&mut self, stub: &Stub) -> Result<(), FlashError> {
        for (bytes, start) in [(&stub.text, stub.text_start), (&stub.data, stub.data_start)] {
            let blocks = bytes.len().div_ceil(RAM_BLOCK) as u32;
            self.command(Op::MemBegin, &words(&[bytes.len() as u32, blocks, RAM_BLOCK as u32, start]), 0, DEFAULT_TIMEOUT)?;
            for (seq, chunk) in bytes.chunks(RAM_BLOCK).enumerate() {
                let mut d = words(&[chunk.len() as u32, seq as u32, 0, 0]);
                d.extend_from_slice(chunk);
                self.command(Op::MemData, &d, data_checksum(chunk), DEFAULT_TIMEOUT)?;
            }
        }
        self.command(Op::MemEnd, &words(&[(stub.entry == 0) as u32, stub.entry]), 0, DEFAULT_TIMEOUT)?;
        let deadline = Instant::now() + Duration::from_secs(1);
        while let Some(f) = self.frame(deadline)? {
            if f == b"OHAI" {
                self.stub = true;
                return Ok(());
            }
        }
        Err(FlashError::Stub("no greeting after loading it".into()))
    }

    fn erase(&mut self, offset: u32, size: u32) -> Result<(), FlashError> {
        if self.stub {
            if offset == 0 && size == FLASH_SIZE {
                self.command(Op::EraseFlash, &[], 0, Duration::from_secs(120))?;
            } else {
                self.command(Op::EraseRegion, &words(&[offset, size]), 0, per_mb(30.0, size as usize))?;
            }
        } else {
            let rom = rom_erase_size(offset, size);
            self.command(Op::FlashBegin, &words(&[rom, 0, ROM_WRITE_BLOCK as u32, offset]), 0, per_mb(30.0, size as usize))?;
        }
        Ok(())
    }

    fn write_stub(&mut self, image: &[u8], progress: &mut dyn FnMut(Progress)) -> Result<(), FlashError> {
        let mut z = flate2::write::ZlibEncoder::new(Vec::new(), flate2::Compression::best());
        z.write_all(image).map_err(|e| FlashError::Rejected(e.to_string()))?;
        let packed = z.finish().map_err(|e| FlashError::Rejected(e.to_string()))?;
        let blocks = packed.len().div_ceil(STUB_WRITE_BLOCK) as u32;
        self.command(Op::DeflBegin, &words(&[image.len() as u32, blocks, STUB_WRITE_BLOCK as u32, 0]), 0, DEFAULT_TIMEOUT)?;
        // esptool write_flash: a block's wait follows its unpacked size (ERASE_WRITE_TIMEOUT_PER_MB, 40 s), and as
        // the stub writes a block while the next one comes, each answer gets the previous block's wait
        let mut unpack = flate2::Decompress::new(true);
        let mut sink = vec![0u8; 64 * 1024];
        let mut timeout = DEFAULT_TIMEOUT;
        for (seq, chunk) in packed.chunks(STUB_WRITE_BLOCK).enumerate() {
            let mut d = words(&[chunk.len() as u32, seq as u32, 0, 0]);
            d.extend_from_slice(chunk);
            self.command(Op::DeflData, &d, data_checksum(chunk), timeout)?;
            timeout = per_mb(40.0, unpacked_len(&mut unpack, chunk, &mut sink));
            let done = ((seq + 1) * STUB_WRITE_BLOCK) as u64 * image.len() as u64 / packed.len() as u64;
            progress(Progress::Bytes { done: done.min(image.len() as u64), total: image.len() as u64 });
        }
        // the stub acknowledges a block before writing it: this read answers once the last one is in the flash
        self.command(Op::ReadReg, &words(&[CHIP_DETECT_MAGIC_REG]), 0, timeout)?;
        Ok(())
    }

    fn write_rom(&mut self, image: &[u8], progress: &mut dyn FnMut(Progress)) -> Result<(), FlashError> {
        let blocks = image.len().div_ceil(ROM_WRITE_BLOCK) as u32;
        let erase = rom_erase_size(0, image.len() as u32);
        self.command(Op::FlashBegin, &words(&[erase, blocks, ROM_WRITE_BLOCK as u32, 0]), 0, per_mb(30.0, image.len()))?;
        for (seq, chunk) in image.chunks(ROM_WRITE_BLOCK).enumerate() {
            let mut block = chunk.to_vec();
            block.resize(ROM_WRITE_BLOCK, 0xFF);
            let mut d = words(&[ROM_WRITE_BLOCK as u32, seq as u32, 0, 0]);
            d.extend_from_slice(&block);
            self.command(Op::FlashData, &d, data_checksum(&block), per_mb(40.0, ROM_WRITE_BLOCK))?;
            let done = ((seq + 1) * ROM_WRITE_BLOCK).min(image.len()) as u64;
            progress(Progress::Bytes { done, total: image.len() as u64 });
        }
        Ok(())
    }

    fn md5_matches(&mut self, image: &[u8]) -> Result<bool, FlashError> {
        let mut padded = image.to_vec();
        padded.resize(image.len().div_ceil(4) * 4, 0xFF);
        let r = self.command(Op::FlashMd5, &words(&[0, padded.len() as u32, 0, 0]), 0, per_mb(8.0, padded.len()))?;
        let got = &r.data[..r.data.len() - 2];
        let want = Md5::digest(&padded);
        Ok(match got.len() {
            16 => got == want.as_slice(),
            32 => std::str::from_utf8(got).is_ok_and(|h| h.eq_ignore_ascii_case(&hex(&want))),
            _ => false,
        })
    }

    fn hard_reset(&mut self) -> Result<(), FlashError> {
        self.link.set_rts(true)?;
        std::thread::sleep(Duration::from_millis(100));
        self.link.set_rts(false)?;
        Ok(())
    }
}

fn hex(b: &[u8]) -> String {
    b.iter().map(|x| format!("{x:02x}")).collect()
}

/// Erases (as asked) and writes `fw` at 0x0. `stub` None or a stub that does not start: ROM path at 115200.
pub fn esp_flash(
    link: &mut dyn SerialLink,
    fw: Option<&FirmwareFile>,
    stub: Option<&Stub>,
    opt: &EspOptions,
    progress: ProgressFn,
) -> Result<EspReport, FlashError> {
    let t0 = Instant::now();
    // checked before the port is touched: past the user area the write reaches the RF calibration and WiFi sectors
    if let Some(f) = fw.filter(|f| f.raw.len() > USER_AREA.1 as usize) {
        return Err(FlashError::TooLarge { size: f.raw.len(), max: USER_AREA.1 as usize });
    }
    link.set_baud(ROM_BAUD)?;
    let mut l = Loader { link, dec: SlipDecoder::default(), pending: Default::default(), stub: false };
    progress(Progress::Status("Connecting to the ESP8266".into()));
    l.sync()?;
    if let Some(s) = stub {
        progress(Progress::Status("Starting the fast flasher".into()));
        match l.load_stub(s) {
            Ok(()) => {
                if opt.baud != ROM_BAUD {
                    l.command(Op::ChangeBaud, &words(&[opt.baud, ROM_BAUD]), 0, DEFAULT_TIMEOUT)?;
                    l.link.set_baud(opt.baud)?;
                    std::thread::sleep(Duration::from_millis(50));
                    l.drain(Duration::from_millis(20))?;
                }
                l.command(Op::SpiSetParams, &words(&[0, FLASH_SIZE, 64 * 1024, SECTOR, 256, 0xFFFF]), 0, DEFAULT_TIMEOUT)?;
            }
            Err(FlashError::PortGone) => return Err(FlashError::PortGone),
            Err(e) => {
                // the stub may be running without having said so: reset into the ROM before the ROM path, whose
                // FLASH_BEGIN sizes the stub would take literally
                progress(Progress::Status(format!("{e}; going on with the slow ROM path")));
                l.stub = false;
                l.sync()?;
            }
        }
    }
    match opt.erase {
        Erase::None => {}
        Erase::User => l.erase(USER_AREA.0, USER_AREA.1)?,
        Erase::Wifi => l.erase(WIFI_AREA.0, WIFI_AREA.1)?,
        Erase::All => l.erase(0, FLASH_SIZE)?,
    }
    if opt.erase != Erase::None {
        progress(Progress::Status("Erased".into()));
    }
    let mut report = EspReport { used_stub: l.stub, md5_ok: None, readback_ok: None, seconds: 0.0 };
    if let Some(fw) = fw {
        let mut image = fw.raw.clone();
        esp_patch_header(&mut image);
        progress(Progress::Status(format!("Writing {} ({} bytes)", fw.name, image.len())));
        if l.stub {
            l.write_stub(&image, progress)?;
            progress(Progress::Status("Checking".into()));
            let ok = l.md5_matches(&image)?;
            report.md5_ok = Some(ok);
            if !ok {
                return Err(FlashError::Verify("the MD5 of the flash differs from the file".into()));
            }
            // leave the stub's flash mode without running the firmware yet (esptool write_flash end)
            l.command(Op::FlashBegin, &words(&[0, 0, STUB_WRITE_BLOCK as u32, 0]), 0, DEFAULT_TIMEOUT)?;
            l.command(Op::DeflEnd, &words(&[1]), 0, DEFAULT_TIMEOUT)?;
        } else {
            l.write_rom(&image, progress)?;
        }
    }
    l.hard_reset()?;
    report.seconds = t0.elapsed().as_secs_f32();
    progress(Progress::Done(if fw.is_some() { "ESP8266 written".into() } else { "ESP8266 erased".into() }));
    Ok(report)
}
