//! SLIP framing of the ESP8266 loader: frames between 0xC0, with 0xC0 / 0xDB escaped as DB DC / DB DD.
const END: u8 = 0xC0;
const ESC: u8 = 0xDB;
const ESC_END: u8 = 0xDC;
const ESC_ESC: u8 = 0xDD;

pub fn slip_encode(payload: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(payload.len() + 8);
    out.push(END);
    for &b in payload {
        match b {
            END => out.extend([ESC, ESC_END]),
            ESC => out.extend([ESC, ESC_ESC]),
            _ => out.push(b),
        }
    }
    out.push(END);
    out
}

/// Byte-by-byte decoder; bytes outside a frame (boot messages, noise) are dropped.
#[derive(Default)]
pub struct SlipDecoder {
    in_frame: bool,
    escaped: bool,
    buf: Vec<u8>,
}

impl SlipDecoder {
    pub fn push(&mut self, b: u8) -> Option<Vec<u8>> {
        if b == END {
            if self.in_frame && !self.buf.is_empty() {
                self.in_frame = false;
                self.escaped = false;
                return Some(std::mem::take(&mut self.buf));
            }
            self.in_frame = true; // a start, or back-to-back ENDs
            self.buf.clear();
            return None;
        }
        if !self.in_frame {
            return None;
        }
        if self.escaped {
            self.escaped = false;
            self.buf.push(match b {
                ESC_END => END,
                ESC_ESC => ESC,
                other => other,
            });
        } else if b == ESC {
            self.escaped = true;
        } else {
            self.buf.push(b);
        }
        None
    }
}
