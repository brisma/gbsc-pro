// The web app's device simulator (gbs-control/public/sim) as a fake unit on free local ports
use gbsc_core::device::Unit;
use std::io::{BufRead, BufReader, Read, Write};
use std::net::{TcpListener, TcpStream};
use std::path::PathBuf;
use std::process::{Child, Command, Stdio};
use std::time::Duration;

pub struct Sim {
    child: Child,
    pub port: u16,
    pub ws: u16,
}

fn free_port() -> u16 {
    TcpListener::bind("127.0.0.1:0").unwrap().local_addr().unwrap().port()
}

impl Sim {
    pub fn start() -> Sim {
        let public = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../gbs-control/public");
        assert!(public.join("node_modules").is_dir(), "run `npm ci` in gbs-control/public first");
        assert!(public.join("gen/manifest.json").is_file(), "run `npm run gen` in gbs-control/public first");
        // a free port is released before node binds it: tests running side by side can pick the same one, and that
        // simulator exits without its ready line. Start again on other ports.
        for _ in 0..5 {
            let (port, ws) = (free_port(), free_port());
            let mut child = Command::new("node")
                .args(["sim/main.ts", "--port", &port.to_string(), "--ws", &ws.to_string()])
                .current_dir(&public)
                .stdout(Stdio::piped())
                .stderr(Stdio::inherit())
                .spawn()
                .expect("node must be installed (Node 26)");
            let mut line = String::new();
            let mut out = BufReader::new(child.stdout.take().unwrap());
            while out.read_line(&mut line).unwrap_or(0) > 0 && !line.contains("simulator:") {
                line.clear();
            }
            if !line.contains("simulator:") {
                let _ = child.kill();
                let _ = child.wait();
                continue;
            }
            std::thread::spawn(move || std::io::copy(&mut out, &mut std::io::sink()));
            return Sim { child, port, ws };
        }
        panic!("the simulator did not start (5 tries)");
    }

    pub fn unit(&self) -> Unit {
        let mut u = Unit::with_ports("127.0.0.1", self.port, self.ws);
        u.poll = Duration::from_millis(100);
        u
    }

    /// A raw HTTP request (test endpoints and the guard test): status code and body.
    pub fn raw(&self, method: &str, path: &str, headers: &[(&str, &str)], body: &str) -> (u16, String) {
        let mut s = TcpStream::connect(("127.0.0.1", self.port)).unwrap();
        let mut req = format!("{method} {path} HTTP/1.1\r\nHost: 127.0.0.1:{}\r\nConnection: close\r\nContent-Length: {}\r\n", self.port, body.len());
        for (k, v) in headers {
            req += &format!("{k}: {v}\r\n");
        }
        req += "\r\n";
        req += body;
        s.write_all(req.as_bytes()).unwrap();
        let mut resp = String::new();
        s.read_to_string(&mut resp).unwrap();
        let status = resp.split_whitespace().nth(1).unwrap_or("0").parse().unwrap_or(0);
        let body = resp.split("\r\n\r\n").nth(1).unwrap_or("").to_string();
        (status, body)
    }

    pub fn scenario(&self, name: &str) {
        let (st, _) = self.raw("POST", "/__sim/scenario", &[("Content-Type", "application/json")], &format!(r#"{{"name":"{name}"}}"#));
        assert_eq!(st, 200, "scenario {name}");
    }

    pub fn net(&self, profile: &str) {
        let (st, _) = self.raw("POST", "/__sim/net", &[("Content-Type", "application/json")], &format!(r#"{{"profile":"{profile}"}}"#));
        assert_eq!(st, 200, "net {profile}");
    }

    pub fn fault(&self, name: &str) {
        let (st, _) = self.raw("POST", "/__sim/fault", &[("Content-Type", "application/json")], &format!(r#"{{"name":"{name}","on":true}}"#));
        assert_eq!(st, 200, "fault {name}");
    }
}

impl Drop for Sim {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}
