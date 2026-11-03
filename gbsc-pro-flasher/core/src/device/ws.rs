//! The unit's status WebSocket (port 81, subprotocol "arduino", Origin required): lines "s:status.<key>=<value>".
use std::net::{TcpStream, ToSocketAddrs};
use std::time::{Duration, Instant};
use tungstenite::client::IntoClientRequest;
use tungstenite::{Message, WebSocket};

pub(crate) struct StatusSocket {
    ws: WebSocket<TcpStream>,
}

impl StatusSocket {
    /// None when the unit refuses the handshake (safe mode runs no WebSocket server) or is unreachable.
    pub fn open(host: &str, ws_port: u16, origin: &str) -> Option<StatusSocket> {
        // a name (gbscontrol.local) resolves as for the HTTP client; each address is tried in turn
        let stream = (host, ws_port).to_socket_addrs().ok()?.find_map(|a| TcpStream::connect_timeout(&a, Duration::from_secs(3)).ok())?;
        stream.set_read_timeout(Some(Duration::from_millis(200))).ok()?;
        let mut req = format!("ws://{host}:{ws_port}/").into_client_request().ok()?;
        req.headers_mut().insert("Origin", origin.parse().ok()?);
        req.headers_mut().insert("Sec-WebSocket-Protocol", "arduino".parse().ok()?);
        let (ws, _) = tungstenite::client(req, stream).ok()?;
        Some(StatusSocket { ws })
    }

    /// The next value of `s:status.<key>=` within `timeout` (other messages are skipped); None on timeout or close.
    pub fn next_status(&mut self, key: &str, timeout: Duration) -> Option<String> {
        let prefix = format!("s:status.{key}=");
        let deadline = Instant::now() + timeout;
        while Instant::now() < deadline {
            match self.ws.read() {
                Ok(Message::Text(t)) => {
                    if let Some(v) = t.as_str().strip_prefix(&prefix) {
                        return Some(v.to_string());
                    }
                }
                Ok(Message::Close(_)) => return None,
                Ok(_) => {}
                Err(tungstenite::Error::Io(e)) if matches!(e.kind(), std::io::ErrorKind::WouldBlock | std::io::ErrorKind::TimedOut) => {}
                Err(_) => return None,
            }
        }
        None
    }
}
