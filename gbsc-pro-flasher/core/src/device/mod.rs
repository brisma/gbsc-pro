//! A unit on firmware 3.x over WiFi: info, firmware update (GBS-Control and the ADV Controller through it),
//! backup/restore and profiles. Results the device reports later come from the status WebSocket, or from polling
//! /api/info where there is none (safe mode) or no percentage (the ADV Controller copy).
mod http;
mod mdns;
mod ws;

pub use mdns::find_units;

use crate::image::FirmwareFile;
use crate::progress::{Progress, ProgressFn};
use crate::version::compare_versions;
use http::{refusal, Http};
use std::fmt;
use std::time::{Duration, Instant};
use ws::StatusSocket;

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DeviceError {
    Unreachable(String),
    Busy(String),
    Refused { status: u16, reason: String },
    Failed(String),
    NotApplied { version: String },
    Timeout,
}

impl fmt::Display for DeviceError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DeviceError::Unreachable(e) => write!(f, "The GBSC did not answer ({e}). {}", unreachable_hint(e, cfg!(target_os = "macos"))),
            DeviceError::Busy(why) => write!(f, "The GBSC is busy ({why}). Try again in a moment."),
            DeviceError::Refused { reason, .. } => write!(f, "The GBSC refused the request: {reason}."),
            DeviceError::Failed(why) => write!(f, "The GBSC reported a failure: {why}."),
            DeviceError::NotApplied { version } => {
                write!(f, "The GBSC restarted but still runs {version}: it did not accept the new image. Try again, or flash it over USB.")
            }
            DeviceError::Timeout => write!(f, "The GBSC did not finish in time. Check it, then try again."),
        }
    }
}

impl std::error::Error for DeviceError {}

/// What to check when the unit did not answer. macOS answers EHOSTUNREACH (os error 65) while the app has no Local
/// Network decision yet (first start, a new build: the system asks at the same moment), whatever the unit does.
pub fn unreachable_hint(error: &str, macos: bool) -> &'static str {
    if macos && error.contains("os error 65") {
        "If macOS asks, allow GBSC PRO Flasher to find devices on the local network (System Settings > Privacy & Security > Local Network), then try again."
    } else {
        "Check that it is on and on this network."
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UnitInfo {
    pub fw: String,
    pub board: String,
    pub adv_version: Option<String>,
    pub adv_build: Option<u32>,
    pub adv_update_available: bool,
    /// after 3 crash resets: only the update, the backup download and the log answer
    pub safe_mode: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ProfileEntry {
    pub id: String,
    pub name: String,
    pub region: String,
}

pub struct Unit {
    pub host: String,
    pub http_port: u16,
    pub ws_port: u16,
    /// how often /api/info is polled while waiting for a restart or the ADV Controller copy
    pub poll: Duration,
}

const UPDATE_WAIT: Duration = Duration::from_secs(120);
const RESTART_WAIT: Duration = Duration::from_secs(120);
/// a short answer: a GBSC searching for a signal took up to 4.4 s for /api/info
const ANSWER_WAIT: Duration = Duration::from_secs(20);
const ADV_WAIT: Duration = Duration::from_secs(300);

/// The ADV Controller update of this run is over: "done", the link back ("ok") with a version, and when "done" was
/// already there before the run (an earlier update in the same boot), only after the state changed in between.
pub fn adv_update_finished(before: &str, moved: bool, state: &str, status: &str, version: &str) -> bool {
    state == "done" && status == "ok" && !version.is_empty() && (before != "done" || moved)
}

/// The failure step of this run's ADV Controller update. The device starts it on its next loop, so the first polls
/// can still show the last run's outcome: a failure counts once the state differs from before the run, or did since.
pub fn adv_update_failure<'a>(before: &str, left: bool, state: &'a str) -> Option<&'a str> {
    state.strip_prefix("failed:").filter(|_| state != before || left)
}

/// gbsc-backup-<host>-<YYYY-MM-DD>.txt (a unit has no name of its own)
pub fn backup_file_name(host: &str, date: &str) -> String {
    format!("gbsc-backup-{host}-{date}.txt")
}

impl Unit {
    pub fn new(host: &str) -> Unit {
        Unit::with_ports(host, 80, 81)
    }

    pub fn with_ports(host: &str, http_port: u16, ws_port: u16) -> Unit {
        Unit { host: host.to_string(), http_port, ws_port, poll: Duration::from_secs(2) }
    }

    fn http(&self, timeout: Duration) -> Http {
        Http::new(&self.host, self.http_port, timeout)
    }

    fn origin(&self) -> String {
        if self.http_port == 80 { format!("http://{}", self.host) } else { format!("http://{}:{}", self.host, self.http_port) }
    }

    fn socket(&self) -> Option<StatusSocket> {
        StatusSocket::open(&self.host, self.ws_port, &self.origin())
    }

    fn info_json(&self) -> Result<serde_json::Value, DeviceError> {
        let a = self.http(ANSWER_WAIT).get("/api/info")?;
        if a.status != 200 {
            return Err(refusal(&a));
        }
        a.json()
    }

    pub fn info(&self) -> Result<UnitInfo, DeviceError> {
        let v = self.info_json()?;
        let s = |p: &str| v.pointer(p).and_then(|x| x.as_str()).map(str::to_string);
        Ok(UnitInfo {
            fw: s("/fw").unwrap_or_default(),
            board: s("/board").unwrap_or_default(),
            adv_version: s("/adv_controller/version"),
            adv_build: s("/adv_controller/build").and_then(|b| u32::from_str_radix(&b, 16).ok()),
            adv_update_available: v.pointer("/adv_controller/update_available").and_then(|x| x.as_bool()).unwrap_or(false),
            safe_mode: v.pointer("/safe_mode").and_then(|x| x.as_bool()).unwrap_or(false),
        })
    }

    fn boot_id(&self) -> Option<u64> {
        let a = self.http(ANSWER_WAIT).get("/api/log?since=65535").ok()?;   // the device reads larger values as -1 (whole ring)
        a.json().ok()?.get("boot")?.as_u64()
    }

    /// Waits for the restart (unreachable for a while, or a new boot id) and returns the version it came back with.
    /// A unit that stays up with a failed upload (safe mode has no WebSocket to say so) reports its reason.
    fn wait_restart(&self, boot_before: Option<u64>) -> Result<String, DeviceError> {
        let deadline = Instant::now() + RESTART_WAIT;
        let mut was_down = false;
        while Instant::now() < deadline {
            std::thread::sleep(self.poll);
            match self.info_json() {
                Err(_) => was_down = true,
                Ok(v) => {
                    let rebooted = was_down || (boot_before.is_some() && self.boot_id().is_some_and(|b| Some(b) != boot_before));
                    if rebooted {
                        return Ok(v.pointer("/fw").and_then(|x| x.as_str()).unwrap_or_default().to_string());
                    }
                    if v.pointer("/upload/state").and_then(|x| x.as_str()) == Some("failed") {
                        let why = v.pointer("/upload/error").and_then(|x| x.as_str()).unwrap_or("upload");
                        return Err(DeviceError::Failed(why.to_string()));
                    }
                }
            }
        }
        Err(DeviceError::Timeout)
    }

    /// Uploads a GBS-Control image (as the file is: a .gz travels compressed) and returns the version the unit runs
    /// after its restart.
    pub fn update(&self, fw: &FirmwareFile, progress: ProgressFn) -> Result<String, DeviceError> {
        let before = self.info()?.fw;
        let boot = self.boot_id();
        let mut sock = self.socket();
        progress(Progress::Status(format!("Uploading {} to the GBSC ({before})", fw.name)));
        let mut shown = 0u64;   // percent, never backwards (a 503 retry sends the body again)
        let a = self.http(UPDATE_WAIT).upload_with_progress("/api/system/update", "firmware", &fw.name, &fw.original, &mut |done, total| {
            // 99 at most: 100 comes with the unit's own "done"
            let pct = (done * 99 / total.max(1)).max(shown);
            if pct != shown {
                shown = pct;
                progress(Progress::Bytes { done: pct, total: 100 });
            }
        })?;
        if a.status != 202 && a.status != 200 {
            return Err(refusal(&a));
        }
        if let Some(s) = sock.as_mut() {
            let deadline = Instant::now() + UPDATE_WAIT;
            loop {
                let left = deadline.saturating_duration_since(Instant::now());
                match s.next_status("update", left) {
                    Some(v) if v == "done" => break,
                    Some(v) if v.starts_with("error:") => return Err(DeviceError::Failed(v[6..].to_string())),
                    Some(v) => {
                        // queued during the upload: only what is ahead of the bytes already shown
                        if let Ok(pct) = v.parse::<u64>() {
                            if pct > shown {
                                shown = pct;
                                progress(Progress::Bytes { done: pct, total: 100 });
                            }
                        }
                    }
                    None => break, // the socket closed: the unit is restarting (or never had one)
                }
            }
        }
        progress(Progress::Status("Waiting for the GBSC to restart".into()));
        let after = self.wait_restart(boot)?;
        // the unit must come back with the file's version when the file has one; without it, only a change
        // from the version before is known
        let applied = match fw.version.as_deref() {
            Some(v) => compare_versions(v, &after) == std::cmp::Ordering::Equal,
            None => true,
        };
        if !applied {
            return Err(DeviceError::NotApplied { version: after });
        }
        progress(Progress::Done(format!("GBS-Control {after}")));
        Ok(after)
    }

    /// Starts the ADV Controller update from the image inside GBS-Control and waits for its end.
    pub fn update_adv_controller(&self, progress: ProgressFn) -> Result<String, DeviceError> {
        let before = self.info_json().ok().and_then(|v| v.pointer("/adv_controller/update").and_then(|x| x.as_str()).map(str::to_string)).unwrap_or_default();
        let mut moved = false;
        let mut left = false;
        let a = self.http(ANSWER_WAIT).post("/api/actions/system.update_adv_controller", "application/x-www-form-urlencoded", b"")?;
        if a.status != 200 && a.status != 202 {
            return Err(refusal(&a));
        }
        progress(Progress::Status("Updating the ADV Controller".into()));
        let deadline = Instant::now() + ADV_WAIT;
        while Instant::now() < deadline {
            std::thread::sleep(self.poll);
            let Ok(v) = self.info_json() else { continue };
            let state = v.pointer("/adv_controller/update").and_then(|x| x.as_str()).unwrap_or("");
            // "done" comes first; the link then restarts and the version is back once the ADV Controller answers
            let status = v.pointer("/adv_controller/status").and_then(|x| x.as_str()).unwrap_or("");
            let ver = v.pointer("/adv_controller/version").and_then(|x| x.as_str()).unwrap_or("");
            moved |= state != before || status != "ok";
            if adv_update_finished(&before, moved, state, status, ver) {
                progress(Progress::Done(format!("ADV Controller {ver}")));
                return Ok(ver.to_string());
            }
            if let Some(why) = adv_update_failure(&before, left, state) {
                return Err(DeviceError::Failed(why.to_string()));
            }
            left |= state != before;
        }
        Err(DeviceError::Timeout)
    }

    /// True when the ADV Controller runs the image of `fw` (its build id is the image's header CRC).
    pub fn adv_verify(&self, fw: &FirmwareFile) -> Result<bool, DeviceError> {
        Ok(fw.adv_crc.is_some() && self.info()?.adv_build == fw.adv_crc)
    }

    pub fn backup(&self) -> Result<String, DeviceError> {
        let a = self.http(Duration::from_secs(60)).get("/api/system/backup")?;
        if a.status != 200 {
            return Err(refusal(&a));
        }
        Ok(a.text())
    }

    fn upload_and_wait(&self, path: &str, field: &str, name: &str, data: &[u8], key: &str) -> Result<(), DeviceError> {
        let mut sock = self.socket();
        let a = self.http(Duration::from_secs(60)).upload(path, field, name, data)?;
        if a.status != 202 && a.status != 200 {
            return Err(refusal(&a));
        }
        let Some(s) = sock.as_mut() else { return Ok(()) }; // no WebSocket (safe mode): accepted is all we know
        let deadline = Instant::now() + Duration::from_secs(60);
        loop {
            match s.next_status(key, deadline.saturating_duration_since(Instant::now())) {
                Some(v) if v == "ok" => return Ok(()),
                Some(v) if v.starts_with("error:") => return Err(DeviceError::Failed(v[6..].to_string())),
                Some(_) => {}
                None => return Err(DeviceError::Timeout),
            }
        }
    }

    /// Restores a backup (the unit checks it, applies it and restarts).
    pub fn restore(&self, text: &str) -> Result<(), DeviceError> {
        self.upload_and_wait("/api/system/restore", "backup", "gbsc-backup.txt", text.as_bytes(), "restore")
    }

    pub fn profiles(&self) -> Result<Vec<ProfileEntry>, DeviceError> {
        let a = self.http(ANSWER_WAIT).get("/api/profiles")?;
        if a.status != 200 {
            return Err(refusal(&a));
        }
        let v = a.json()?;
        let list = v.get("profiles").and_then(|p| p.as_array()).cloned().unwrap_or_default();
        Ok(list
            .iter()
            .map(|p| {
                let s = |k: &str| p.get(k).and_then(|x| x.as_str()).unwrap_or("").to_string();
                ProfileEntry { id: s("id"), name: s("name"), region: s("region") }
            })
            .collect())
    }

    pub fn profile_export(&self, id: &str) -> Result<Vec<u8>, DeviceError> {
        let enc: String = id.bytes().map(|b| if b.is_ascii_alphanumeric() { (b as char).to_string() } else { format!("%{b:02X}") }).collect();
        let a = self.http(ANSWER_WAIT).get(&format!("/api/profiles/export?id={enc}"))?;
        if a.status != 200 {
            return Err(refusal(&a));
        }
        Ok(a.body)
    }

    pub fn profile_import(&self, name: &str, bytes: &[u8]) -> Result<(), DeviceError> {
        self.upload_and_wait("/api/profiles/import", "file", name, bytes, "profile_op")
    }
}
