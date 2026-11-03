// A simulated HCMGBoot (ADV Controller bootloader): 0x55 chip info, '1' download mode, YMODEM-128 receiver
use gbsc_core::link::SerialLink;
use std::collections::VecDeque;
use std::io;
use std::time::Duration;

const SOH: u8 = 0x01;
const EOT: u8 = 0x04;
const ACK: u8 = 0x06;
const NAK: u8 = 0x15;
const CAN: u8 = 0x18;

#[derive(PartialEq, Eq, Clone, Copy, Debug)]
enum State {
    Idle,
    WaitHeader,
    Data,
    WaitEnd,
    Done,
}

pub struct FakeHcmgboot {
    pub chip_info: bool,
    pub never_c: bool,
    pub nak_packet: Option<u8>,
    pub cancel_at: Option<u8>,
    /// the packet is written but its ACK is lost on the wire (once); a duplicate is then written again, as a
    /// receiver that does not drop repeated sequence numbers would
    pub lose_ack_of: Option<u8>,
    pub die_after_bytes: Option<usize>,
    pub no_eot_ack: bool,
    pub no_final_ack: bool,
    pub file: Vec<u8>,
    pub file_name: String,
    pub file_size: usize,
    pub seen: Vec<u8>, // data packet sequence numbers in arrival order
    pub finished: bool,
    /// DTR and RTS were both set high before the flasher's first byte (Windows opens a port with DTR low)
    pub lines_up_at_first_write: Option<bool>,
    dtr: bool,
    rts: bool,
    state: State,
    rx: Vec<u8>,
    out: VecDeque<u8>,
    written: usize,
    naked: bool,
}

impl FakeHcmgboot {
    pub fn new() -> Self {
        FakeHcmgboot {
            chip_info: true,
            never_c: false,
            nak_packet: None,
            cancel_at: None,
            lose_ack_of: None,
            die_after_bytes: None,
            no_eot_ack: false,
            no_final_ack: false,
            file: Vec::new(),
            file_name: String::new(),
            file_size: 0,
            seen: Vec::new(),
            finished: false,
            lines_up_at_first_write: None,
            dtr: false,
            rts: false,
            state: State::Idle,
            rx: Vec::new(),
            out: VecDeque::new(),
            written: 0,
            naked: false,
        }
    }

    fn dead(&self) -> bool {
        self.die_after_bytes.is_some_and(|n| self.written > n)
    }

    fn packet(&mut self) {
        while self.rx.len() >= 133 || (!self.rx.is_empty() && self.rx[0] != SOH) {
            if self.rx[0] == EOT {
                self.rx.remove(0);
                if self.no_eot_ack {
                    continue;
                }
                self.out.push_back(ACK);
                self.out.push_back(b'C');
                self.state = State::WaitEnd;
                continue;
            }
            if self.rx[0] != SOH {
                self.rx.remove(0);
                continue;
            }
            let p: Vec<u8> = self.rx.drain(..133).collect();
            let (seq, data) = (p[1], &p[3..131]);
            let crc = u16::from_be_bytes([p[131], p[132]]);
            if p[2] != !seq || crc != crc16(data) {
                self.out.push_back(NAK);
                continue;
            }
            match self.state {
                State::WaitHeader => {
                    let name_end = data.iter().position(|&b| b == 0).unwrap();
                    self.file_name = String::from_utf8_lossy(&data[..name_end]).into();
                    let size_end = name_end + 1 + data[name_end + 1..].iter().position(|&b| b == 0).unwrap();
                    self.file_size = std::str::from_utf8(&data[name_end + 1..size_end]).unwrap().parse().unwrap();
                    self.state = State::Data;
                    self.out.push_back(ACK);
                    self.out.push_back(b'C');
                }
                State::Data => {
                    self.seen.push(seq);
                    if self.cancel_at == Some(seq) {
                        self.out.push_back(CAN);
                        self.out.push_back(CAN);
                        continue;
                    }
                    if self.nak_packet == Some(seq) && !self.naked {
                        self.naked = true;
                        self.out.push_back(NAK);
                        continue;
                    }
                    self.file.extend_from_slice(data);
                    if self.lose_ack_of == Some(seq) {
                        self.lose_ack_of = None;
                        continue;
                    }
                    self.out.push_back(ACK);
                }
                State::WaitEnd => {
                    self.file.truncate(self.file_size);
                    self.finished = true;
                    self.state = State::Done;
                    if !self.no_final_ack {
                        self.out.push_back(ACK);
                    }
                }
                _ => {}
            }
        }
    }
}

impl SerialLink for FakeHcmgboot {
    fn write_all(&mut self, b: &[u8]) -> io::Result<()> {
        self.lines_up_at_first_write.get_or_insert(self.dtr && self.rts);
        self.written += b.len();
        if self.dead() {
            return Err(io::Error::new(io::ErrorKind::BrokenPipe, "device gone"));
        }
        match self.state {
            State::Idle => {
                for &c in b {
                    if c == 0x55 && self.chip_info {
                        let mut info = b"HC32F460".to_vec();
                        info.resize(16, 0);
                        info.extend((0..16).map(|i| 0xA0 + i as u8));
                        info.resize(48, 0);
                        self.out.extend(info);
                    } else if c == b'1' {
                        self.out.extend(b"Download mode\r\n");
                        self.state = State::WaitHeader;
                    }
                }
            }
            _ => {
                self.rx.extend_from_slice(b);
                self.packet();
            }
        }
        Ok(())
    }

    fn read(&mut self, buf: &mut [u8], timeout: Duration) -> io::Result<usize> {
        if self.dead() {
            return Err(io::Error::new(io::ErrorKind::BrokenPipe, "device gone"));
        }
        if self.out.is_empty() && self.state == State::WaitHeader && !self.never_c {
            self.out.push_back(b'C'); // a YMODEM receiver repeats 'C' until the header comes
        }
        if self.out.is_empty() {
            std::thread::sleep(timeout.min(Duration::from_millis(2)));
            return Ok(0);
        }
        let n = buf.len().min(self.out.len());
        for b in buf.iter_mut().take(n) {
            *b = self.out.pop_front().unwrap();
        }
        Ok(n)
    }

    fn set_baud(&mut self, _baud: u32) -> io::Result<()> {
        Ok(())
    }
    fn set_dtr(&mut self, on: bool) -> io::Result<()> {
        self.dtr = on;
        Ok(())
    }
    fn set_rts(&mut self, on: bool) -> io::Result<()> {
        self.rts = on;
        Ok(())
    }
    fn clear_input(&mut self) -> io::Result<()> {
        self.out.clear();
        Ok(())
    }
}

pub fn crc16(data: &[u8]) -> u16 {
    let mut crc: u16 = 0;
    for &b in data {
        crc ^= (b as u16) << 8;
        for _ in 0..8 {
            crc = if crc & 0x8000 != 0 { (crc << 1) ^ 0x1021 } else { crc << 1 };
        }
    }
    crc
}
