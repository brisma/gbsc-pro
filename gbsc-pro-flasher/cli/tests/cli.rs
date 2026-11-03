// The command line: usage, refusals before any write, backup against the web app's simulator
use std::io::{BufRead, BufReader};
use std::net::TcpListener;
use std::path::PathBuf;
use std::process::{Child, Command, Stdio};

fn bin() -> Command {
    Command::new(env!("CARGO_BIN_EXE_gbsc-flasher"))
}

fn temp(name: &str) -> PathBuf {
    let d = std::env::temp_dir().join(format!("gbsc-cli-{name}-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&d);
    std::fs::create_dir_all(&d).unwrap();
    d
}

#[test]
fn usage_exit_2() {
    let out = bin().arg("frobnicate").output().unwrap();
    assert_eq!(out.status.code(), Some(2));
}

#[test]
fn flash_unknown_file_refused() {
    let out = bin().args(["flash", "/nonexistent/fw.bin"]).output().unwrap();
    assert_eq!(out.status.code(), Some(1));
    assert!(String::from_utf8_lossy(&out.stderr).contains("/nonexistent/fw.bin"));
}

#[test]
fn flash_by_size_only_needs_force() {
    let d = temp("force");
    let f = d.join("mystery.bin");
    std::fs::write(&f, vec![0u8; 30 * 1024]).unwrap();
    let out = bin().args(["flash", f.to_str().unwrap()]).output().unwrap();
    assert_eq!(out.status.code(), Some(1));
    assert!(String::from_utf8_lossy(&out.stderr).contains("--force"));
}

// Over WiFi too: the unit installs any ESP8266 image, so a file without the GBSC marker (an upstream gbs-control
// build) is refused before any connection unless --force says so
#[test]
fn update_by_size_only_needs_force() {
    let d = temp("update-force");
    let f = d.join("upstream.bin");
    let mut v = vec![0xE9, 0x01, 0x00, 0x20];
    v.resize(300 * 1024, 0x33);
    std::fs::write(&f, v).unwrap();
    let out = bin().args(["update", f.to_str().unwrap(), "--host", "127.0.0.1:9"]).output().unwrap();
    assert_eq!(out.status.code(), Some(1));
    assert!(String::from_utf8_lossy(&out.stderr).contains("--force"), "{}", String::from_utf8_lossy(&out.stderr));
}

// A small copy of core/tests/fakes/sim.rs (test modules cannot be shared between crates)
struct Sim(Child, u16);
impl Sim {
    fn start() -> Sim {
        let public = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../gbs-control/public");
        assert!(public.join("gen/manifest.json").is_file(), "run `npm ci && npm run gen` in gbs-control/public first");
        let free = || TcpListener::bind("127.0.0.1:0").unwrap().local_addr().unwrap().port();
        for _ in 0..5 {
            // the same retry as core/tests/fakes/sim.rs: another test can take the port before node binds it
            let (port, ws) = (free(), free());
            let mut child = Command::new("node")
                .args(["sim/main.ts", "--port", &port.to_string(), "--ws", &ws.to_string()])
                .current_dir(&public)
                .stdout(Stdio::piped())
                .spawn()
                .unwrap();
            let mut out = BufReader::new(child.stdout.take().unwrap());
            let mut line = String::new();
            while out.read_line(&mut line).unwrap_or(0) > 0 && !line.contains("simulator:") {
                line.clear();
            }
            if !line.contains("simulator:") {
                let _ = child.kill();
                let _ = child.wait();
                continue;
            }
            std::thread::spawn(move || std::io::copy(&mut out, &mut std::io::sink()));
            return Sim(child, port);
        }
        panic!("the simulator did not start (5 tries)");
    }
}
impl Drop for Sim {
    fn drop(&mut self) {
        let _ = self.0.kill();
        let _ = self.0.wait();
    }
}

#[test]
fn backup_writes_named_file() {
    let sim = Sim::start();
    let d = temp("backup");
    let out = bin().args(["backup", "--host", &format!("127.0.0.1:{}", sim.1)]).current_dir(&d).output().unwrap();
    assert_eq!(out.status.code(), Some(0), "{}", String::from_utf8_lossy(&out.stderr));
    let files: Vec<String> = std::fs::read_dir(&d).unwrap().map(|e| e.unwrap().file_name().to_string_lossy().into_owned()).collect();
    assert_eq!(files.len(), 1);
    assert!(files[0].starts_with("gbsc-backup-127.0.0.1-") && files[0].ends_with(".txt"), "{files:?}");
    assert!(std::fs::read_to_string(d.join(&files[0])).unwrap().starts_with("# gbsc-backup 1"));
}
