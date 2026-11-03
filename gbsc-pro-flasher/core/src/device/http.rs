//! HTTP to the unit: every POST carries X-GBSC: 1; a 503 busy answer is retried after its Retry-After (at most 5
//! times), any other refusal is reported with the device's reason.
use super::DeviceError;
use std::cell::RefCell;
use std::io::Read;
use std::time::Duration;

pub(crate) struct Answer {
    pub status: u16,
    pub body: Vec<u8>,
}

impl Answer {
    pub fn text(&self) -> String {
        String::from_utf8_lossy(&self.body).into_owned()
    }
    pub fn json(&self) -> Result<serde_json::Value, DeviceError> {
        serde_json::from_slice(&self.body).map_err(|e| DeviceError::Failed(format!("unexpected answer: {e}")))
    }
}

pub(crate) struct Http {
    agent: ureq::Agent,
    base: String,
}

const RETRIES: u32 = 5;

impl Http {
    pub fn new(host: &str, port: u16, timeout: Duration) -> Http {
        let config = ureq::Agent::config_builder().http_status_as_error(false).timeout_global(Some(timeout)).build();
        let base = if port == 80 { format!("http://{host}") } else { format!("http://{host}:{port}") };
        Http { agent: ureq::Agent::new_with_config(config), base }
    }

    fn run(&self, f: impl Fn() -> Result<ureq::http::Response<ureq::Body>, ureq::Error>) -> Result<Answer, DeviceError> {
        for attempt in 0..=RETRIES {
            let mut r = f().map_err(|e| DeviceError::Unreachable(e.to_string()))?;
            let status = r.status().as_u16();
            if status == 503 && attempt < RETRIES {
                let wait = r.headers().get("Retry-After").and_then(|v| v.to_str().ok()?.parse::<u64>().ok()).unwrap_or(1);
                std::thread::sleep(Duration::from_secs(wait.min(10)));
                continue;
            }
            let body = r.body_mut().with_config().limit(16 * 1024 * 1024).read_to_vec().map_err(|e| DeviceError::Unreachable(e.to_string()))?;
            if status == 503 {
                let why = r.headers().get("X-GBSC-Busy").and_then(|v| v.to_str().ok()).unwrap_or("busy").to_string();
                return Err(DeviceError::Busy(why));
            }
            return Ok(Answer { status, body });
        }
        Err(DeviceError::Busy("busy".into()))
    }

    pub fn get(&self, path: &str) -> Result<Answer, DeviceError> {
        let url = format!("{}{path}", self.base);
        self.run(|| self.agent.get(&url).call())
    }

    pub fn post(&self, path: &str, content_type: &str, body: &[u8]) -> Result<Answer, DeviceError> {
        let url = format!("{}{path}", self.base);
        self.run(|| self.agent.post(&url).header("X-GBSC", "1").header("Content-Type", content_type).send(body))
    }

    /// A multipart upload with one file part (the device takes the first part with a file name).
    pub fn upload(&self, path: &str, field: &str, file_name: &str, data: &[u8]) -> Result<Answer, DeviceError> {
        self.upload_with_progress(path, field, file_name, data, &mut |_, _| {})
    }

    /// The same upload, telling `sent(done, total)` as the body leaves: the unit's own percentages only reach the
    /// status socket's reader after the upload, so they cannot drive a bar during it.
    pub fn upload_with_progress(
        &self,
        path: &str,
        field: &str,
        file_name: &str,
        data: &[u8],
        sent: &mut dyn FnMut(u64, u64),
    ) -> Result<Answer, DeviceError> {
        let boundary = "----gbscflasher7MA4YWxkTrZu0gW";
        let safe_name: String = file_name.chars().filter(|c| *c != '"' && *c != '\r' && *c != '\n').collect();
        let mut body = format!(
            "--{boundary}\r\nContent-Disposition: form-data; name=\"{field}\"; filename=\"{safe_name}\"\r\nContent-Type: application/octet-stream\r\n\r\n"
        )
        .into_bytes();
        body.extend_from_slice(data);
        body.extend_from_slice(format!("\r\n--{boundary}--\r\n").as_bytes());
        let url = format!("{}{path}", self.base);
        let content_type = format!("multipart/form-data; boundary={boundary}");
        let sent = RefCell::new(sent);
        self.run(|| {
            // the explicit length keeps ureq off chunked transfer, which the unit does not take
            let mut reader = Counting { data: &body, pos: 0, sent: &sent };
            self.agent
                .post(&url)
                .header("X-GBSC", "1")
                .header("Content-Type", &content_type)
                .header("Content-Length", body.len().to_string())
                .send(ureq::SendBody::from_reader(&mut reader))
        })
    }
}

/// The request body in small reads, each one reported: the socket takes them about as fast as the unit reads.
struct Counting<'a, 'b> {
    data: &'a [u8],
    pos: usize,
    sent: &'a RefCell<&'b mut dyn FnMut(u64, u64)>,
}

impl Read for Counting<'_, '_> {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        let n = buf.len().min(4096).min(self.data.len() - self.pos);
        buf[..n].copy_from_slice(&self.data[self.pos..self.pos + n]);
        self.pos += n;
        if n > 0 {
            (self.sent.borrow_mut())(self.pos as u64, self.data.len() as u64);
        }
        Ok(n)
    }
}

/// The reason in a refusal body ({"error": ...} or {"errors": {...}}), else the status line.
pub(crate) fn refusal(a: &Answer) -> DeviceError {
    let reason = a
        .json()
        .ok()
        .and_then(|v| v.get("error").and_then(|e| e.as_str()).map(str::to_string).or_else(|| v.get("errors").map(|e| e.to_string())))
        .unwrap_or_else(|| format!("HTTP {}", a.status));
    DeviceError::Refused { status: a.status, reason }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use std::net::TcpListener;
    use std::time::Instant;

    #[test]
    fn upload_progress_follows_the_bytes_the_unit_reads() {
        // a unit that reads slowly (the ESP8266 has a TCP window of a few KB): the progress must move while it
        // reads, not jump to the end before it starts. Its small receive buffer is set like the unit's (left to the
        // system, Linux grows the loopback buffers until the whole 4 MB fits and the test no longer models a unit).
        let sock = socket2::Socket::new(socket2::Domain::IPV4, socket2::Type::STREAM, None).unwrap();
        sock.set_recv_buffer_size(8 * 1024).unwrap();
        sock.bind(&"127.0.0.1:0".parse::<std::net::SocketAddr>().unwrap().into()).unwrap();
        sock.listen(1).unwrap();
        let listener: TcpListener = sock.into();
        let port = listener.local_addr().unwrap().port();
        let server = std::thread::spawn(move || {
            let (mut s, _) = listener.accept().unwrap();
            let mut buf = [0u8; 32 * 1024];
            let mut got = 0usize;
            let start = Instant::now();
            loop {
                let n = s.read(&mut buf).unwrap();
                got += n;
                if n == 0 || got >= 4 * 1024 * 1024 {
                    break;
                }
                std::thread::sleep(Duration::from_millis(2));
            }
            let done = start.elapsed();
            let mut rest = [0u8; 1024];
            s.set_read_timeout(Some(Duration::from_millis(50))).unwrap();
            while s.read(&mut rest).map(|n| n > 0).unwrap_or(false) {}
            s.write_all(b"HTTP/1.1 202 Accepted\r\nContent-Length: 8\r\nConnection: close\r\n\r\n{\"ok\":1}").unwrap();
            done
        });
        let data = vec![0x33u8; 4 * 1024 * 1024];
        let http = Http::new("127.0.0.1", port, Duration::from_secs(30));
        let start = Instant::now();
        let mut seen: Vec<(Duration, u64, u64)> = Vec::new();
        let a = http.upload_with_progress("/api/system/update", "firmware", "x.bin", &data, &mut |d, t| seen.push((start.elapsed(), d, t))).unwrap();
        let read_for = server.join().unwrap();
        assert_eq!(a.status, 202);
        let total = seen.last().unwrap().2;
        assert_eq!(seen.last().unwrap().1, total);
        // the reports reach the end only as the unit finishes reading, spread over most of its reading time
        let half = seen.iter().find(|(_, d, _)| *d >= total / 2).unwrap().0;
        let end = seen.last().unwrap().0;
        assert!(end - half >= read_for / 4, "half at {half:?}, end at {end:?}, unit read for {read_for:?}");
    }
}
