//! The serial link the protocols talk through: a real port (serialport) or a simulated device in the tests.
use std::io::{self, Read, Write};
use std::time::Duration;

pub trait SerialLink: Send {
    fn write_all(&mut self, b: &[u8]) -> io::Result<()>;
    /// Bytes read within `timeout`; Ok(0) when none came.
    fn read(&mut self, buf: &mut [u8], timeout: Duration) -> io::Result<usize>;
    fn set_baud(&mut self, baud: u32) -> io::Result<()>;
    fn set_dtr(&mut self, on: bool) -> io::Result<()>;
    fn set_rts(&mut self, on: bool) -> io::Result<()>;
    fn clear_input(&mut self) -> io::Result<()>;
}

pub struct SerialportLink {
    port: Box<dyn serialport::SerialPort>,
    baud: u32,
}

impl SerialportLink {
    pub fn open(name: &str, baud: u32) -> io::Result<Self> {
        let port = serialport::new(name, baud).timeout(Duration::from_millis(100)).open()?;
        Ok(SerialportLink { port, baud })
    }
}

/// Room for `len` bytes on the wire at `baud` (10 bits each), twice over, never under 500 ms. Windows serialport
/// shares one COMMTIMEOUTS constant between reads and writes, and a read can leave it at 1 ms: the write then stops
/// halfway (a 6 KB SLIP packet at 115200 baud takes 533 ms).
pub fn write_timeout(len: usize, baud: u32) -> Duration {
    let wire = Duration::from_secs_f64(len as f64 * 10.0 / baud.max(1200) as f64);
    (wire * 2).max(Duration::from_millis(500))
}

fn io_err(e: serialport::Error) -> io::Error {
    io::Error::other(e)
}

impl SerialLink for SerialportLink {
    fn write_all(&mut self, b: &[u8]) -> io::Result<()> {
        self.port.set_timeout(write_timeout(b.len(), self.baud)).map_err(io_err)?;
        self.port.write_all(b)?;
        self.port.flush()
    }

    fn read(&mut self, buf: &mut [u8], timeout: Duration) -> io::Result<usize> {
        self.port.set_timeout(timeout.max(Duration::from_millis(1))).map_err(io_err)?;
        match self.port.read(buf) {
            Ok(n) => Ok(n),
            Err(e) if e.kind() == io::ErrorKind::TimedOut => Ok(0),
            Err(e) => Err(e),
        }
    }

    fn set_baud(&mut self, baud: u32) -> io::Result<()> {
        self.port.set_baud_rate(baud).map_err(io_err)?;
        self.baud = baud;
        Ok(())
    }
    fn set_dtr(&mut self, on: bool) -> io::Result<()> {
        self.port.write_data_terminal_ready(on).map_err(io_err)
    }
    fn set_rts(&mut self, on: bool) -> io::Result<()> {
        self.port.write_request_to_send(on).map_err(io_err)
    }
    fn clear_input(&mut self) -> io::Result<()> {
        self.port.clear(serialport::ClearBuffer::Input).map_err(io_err)
    }
}
