// A simulated ESP8266 loader: ROM commands, the flasher stub after MEM_END (OHAI), deflate writes, MD5. The ROM's
// FLASH_BEGIN erase follows the ROM's own behaviour (esptool get_erase_size docstring), so a size the flasher does
// not adjust erases past the requested region, as on the chip.
use flate2::{Decompress, FlushDecompress};
use gbsc_core::esp::cmd::data_checksum;
use gbsc_core::esp::slip::{slip_encode, SlipDecoder};
use gbsc_core::link::SerialLink;
use md5::{Digest, Md5};
use std::collections::VecDeque;
use std::io;
use std::time::{Duration, Instant};

pub const FLASH_SIZE: usize = 4 * 1024 * 1024;
const SECTOR: usize = 4096;

pub struct FakeEsp {
    pub flash: Vec<u8>,
    pub stub_running: bool,
    pub baud: u32,
    pub ignore_syncs: u8,
    pub die_after_bytes: Option<usize>,
    pub corrupt_byte_at: Option<u32>,
    /// sectors the ROM erased beyond 0x400000 (wrapping to 0x0 as the address bus does)
    pub erased_out_of_bounds: bool,
    pub stub_changed_baud: bool,
    /// false: the stub starts after MEM_END but its OHAI greeting is lost
    pub stub_greets: bool,
    /// the stub's flash write speed for deflate blocks (0: instant). It acknowledges a block when it receives it
    /// and writes it while the next one comes, so an answer waits for the previous block's write
    pub write_secs_per_mb: f32,
    busy_until: Instant,
    hold_until: Instant,
    dec: SlipDecoder,
    rts: bool,
    flash_limit: Option<usize>, // the stub's FLASH_BEGIN size, taken literally
    out: VecDeque<u8>,
    written: usize,
    write_base: usize,
    block_size: usize,
    inflater: Option<(Decompress, usize)>, // decompressor, bytes written so far
    stub_bytes: usize,
}

impl FakeEsp {
    pub fn new() -> Self {
        FakeEsp {
            flash: vec![0xFF; FLASH_SIZE],
            stub_running: false,
            baud: 115_200,
            ignore_syncs: 0,
            die_after_bytes: None,
            corrupt_byte_at: None,
            erased_out_of_bounds: false,
            stub_changed_baud: false,
            stub_greets: true,
            write_secs_per_mb: 0.0,
            busy_until: Instant::now(),
            hold_until: Instant::now(),
            dec: SlipDecoder::default(),
            rts: false,
            flash_limit: None,
            out: VecDeque::new(),
            written: 0,
            write_base: 0,
            block_size: 0,
            inflater: None,
            stub_bytes: 0,
        }
    }

    /// The answer being queued leaves once the write in progress is over.
    fn wait_for_write(&mut self) {
        self.hold_until = self.hold_until.max(self.busy_until);
    }

    fn dead(&self) -> bool {
        self.die_after_bytes.is_some_and(|n| self.written > n)
    }

    fn reply(&mut self, op: u8, value: u32, body: &[u8], status: u8, error: u8) {
        let mut data = body.to_vec();
        data.extend([status, error]);
        let mut f = vec![0x01, op];
        f.extend((data.len() as u16).to_le_bytes());
        f.extend(value.to_le_bytes());
        f.extend(data);
        self.out.extend(slip_encode(&f));
    }

    fn ok(&mut self, op: u8) {
        self.reply(op, 0, &[], 0, 0);
    }

    fn put(&mut self, addr: usize, bytes: &[u8]) {
        for (i, &b) in bytes.iter().enumerate() {
            let a = addr + i;
            if a < FLASH_SIZE {
                let bad = self.corrupt_byte_at == Some(a as u32);
                self.flash[a] = if bad { b ^ 0xFF } else { b };
            }
        }
    }

    fn erase_sectors(&mut self, start_sector: usize, count: usize) {
        for s in start_sector..start_sector + count {
            let a = s * SECTOR;
            if a >= FLASH_SIZE {
                self.erased_out_of_bounds = true;
            }
            let a = a % FLASH_SIZE;
            self.flash[a..a + SECTOR].fill(0xFF);
        }
    }

    /// ROM FLASH_BEGIN erase: the inverse of esptool's get_erase_size
    fn rom_erase(&mut self, offset: usize, size: usize) {
        let p = size.div_ceil(SECTOR);
        if p == 0 {
            return;
        }
        let start = offset / SECTOR;
        let head = 16 - start % 16;
        let n = if p <= head { 2 * p } else { p + head };
        self.erase_sectors(start, n);
    }

    fn handle(&mut self, frame: &[u8]) {
        if frame.len() < 8 || frame[0] != 0 {
            return;
        }
        let op = frame[1];
        let chk = u32::from_le_bytes([frame[4], frame[5], frame[6], frame[7]]);
        let d = &frame[8..];
        let w = |i: usize| u32::from_le_bytes([d[i], d[i + 1], d[i + 2], d[i + 3]]) as usize;
        match op {
            0x08 => {
                if self.ignore_syncs > 0 {
                    self.ignore_syncs -= 1;
                } else {
                    for _ in 0..8 {
                        self.reply(0x08, 0x2012_0707, &[], 0, 0); // the ROM answers a SYNC 8 times
                    }
                }
            }
            0x05 => {
                self.stub_bytes = 0;
                self.ok(op)
            }
            0x07 => {
                self.stub_bytes += w(0);
                self.ok(op)
            }
            0x06 => {
                self.ok(op);
                if self.stub_bytes > 0 {
                    self.stub_running = true;
                    if self.stub_greets {
                        self.out.extend(slip_encode(b"OHAI"));
                    }
                }
            }
            0x0F if self.stub_running => {
                self.ok(op);
                self.baud = w(0) as u32;
                self.stub_changed_baud = true;
            }
            0x0B => self.ok(op),
            0x0A => {
                self.wait_for_write();
                self.ok(op)
            }
            0xD0 if self.stub_running => {
                self.flash.fill(0xFF);
                self.ok(op)
            }
            0xD1 if self.stub_running => {
                let (off, size) = (w(0), w(4));
                self.erase_sectors(off / SECTOR, size.div_ceil(SECTOR));
                self.ok(op)
            }
            0x02 => {
                let (size, _blocks, bs, off) = (w(0), w(4), w(8), w(12));
                if self.stub_running {
                    self.erase_sectors(off / SECTOR, size.div_ceil(SECTOR));
                    self.flash_limit = Some(size);
                } else {
                    self.flash_limit = None;
                    self.rom_erase(off, size);
                }
                self.write_base = off;
                self.block_size = bs;
                self.ok(op)
            }
            0x03 => {
                let (len, seq) = (w(0), w(4));
                let data = d[16..16 + len].to_vec();
                if data_checksum(&data) != chk {
                    return self.reply(op, 0, &[], 1, 0x07);
                }
                if self.flash_limit.is_some_and(|lim| (seq + 1) * self.block_size > lim.div_ceil(self.block_size) * self.block_size) {
                    return self.reply(op, 0, &[], 1, 0x08); // the stub: more data than FLASH_BEGIN announced
                }
                self.put(self.write_base + seq * self.block_size, &data);
                self.ok(op)
            }
            0x04 | 0x12 => self.ok(op),
            0x10 if self.stub_running => {
                let (size, off) = (w(0), w(12));
                self.erase_sectors(off / SECTOR, size.div_ceil(SECTOR));
                self.write_base = off;
                self.inflater = Some((Decompress::new(true), 0));
                self.ok(op)
            }
            0x11 if self.stub_running => {
                let len = w(0);
                let data = d[16..16 + len].to_vec();
                if data_checksum(&data) != chk {
                    return self.reply(op, 0, &[], 1, 0x07);
                }
                let (mut z, mut at) = self.inflater.take().unwrap();
                let start = at;
                let mut input = &data[..];
                let mut buf = vec![0u8; 64 * 1024];
                loop {
                    let before_in = z.total_in();
                    let before_out = z.total_out();
                    z.decompress(input, &mut buf, FlushDecompress::None).unwrap();
                    let used = (z.total_in() - before_in) as usize;
                    let made = (z.total_out() - before_out) as usize;
                    let chunk = buf[..made].to_vec();
                    self.put(self.write_base + at, &chunk);
                    at += made;
                    input = &input[used..];
                    if input.is_empty() && made < buf.len() {
                        break;
                    }
                }
                let made = at - start;
                self.inflater = Some((z, at));
                self.wait_for_write();
                self.busy_until = Instant::now().max(self.busy_until) + Duration::from_secs_f32(self.write_secs_per_mb * made as f32 / 1e6);
                self.ok(op)
            }
            0x13 if self.stub_running => {
                self.wait_for_write();
                let (addr, size) = (w(0), w(4));
                let digest = Md5::digest(&self.flash[addr..addr + size]);
                self.reply(op, 0, &digest, 0, 0)
            }
            _ => self.reply(op, 0, &[], 1, 0x05), // the ROM does not know it
        }
    }
}

impl SerialLink for FakeEsp {
    fn write_all(&mut self, b: &[u8]) -> io::Result<()> {
        self.written += b.len();
        if self.dead() {
            return Err(io::Error::new(io::ErrorKind::BrokenPipe, "device gone"));
        }
        for &x in b {
            if let Some(f) = self.dec.push(x) {
                self.handle(&f);
            }
        }
        Ok(())
    }

    fn read(&mut self, buf: &mut [u8], timeout: Duration) -> io::Result<usize> {
        if self.dead() {
            return Err(io::Error::new(io::ErrorKind::BrokenPipe, "device gone"));
        }
        if self.out.is_empty() || Instant::now() < self.hold_until {
            std::thread::sleep(timeout.min(Duration::from_millis(1)));
            return Ok(0);
        }
        let n = buf.len().min(self.out.len());
        for b in buf.iter_mut().take(n) {
            *b = self.out.pop_front().unwrap();
        }
        Ok(n)
    }

    fn set_baud(&mut self, baud: u32) -> io::Result<()> {
        if baud != self.baud {
            return Err(io::Error::other(format!("host {baud} baud, device {} baud", self.baud)));
        }
        Ok(())
    }
    fn set_dtr(&mut self, _on: bool) -> io::Result<()> {
        Ok(())
    }
    fn set_rts(&mut self, on: bool) -> io::Result<()> {
        if self.rts && !on {
            // EN released: the chip restarts into the ROM loader at 115200
            self.stub_running = false;
            self.baud = 115_200;
            self.flash_limit = None;
        }
        self.rts = on;
        Ok(())
    }
    fn clear_input(&mut self) -> io::Result<()> {
        self.out.clear();
        Ok(())
    }
}
