//! Tauri commands: thin wrappers over gbsc-core. Long jobs (flash, update, restore) run one at a time and report
//! through the "progress" event.
use gbsc_core::detect::{list_ports, refuse_reason, PortInfo, PortKind, UNCONFIRMED_TEXT};
use gbsc_core::device::{find_units as core_find_units, Unit};
use gbsc_core::esp::flasher::{esp_flash, Erase, EspOptions};
use gbsc_core::esp::stub::{find_stub, load_stub};
use gbsc_core::image::{parse_firmware, FirmwareFile, FirmwareKind};
use gbsc_core::link::SerialportLink;
use gbsc_core::progress::Progress;
use gbsc_core::release::{app_dir, find_candidates, run_from_zip, translocated};
use gbsc_core::ymodem::adv_flash;
use serde::Serialize;
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use std::time::Duration;
use tauri::{AppHandle, Emitter, Manager, State};

#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
#[serde(rename_all = "camelCase")]
pub struct DeviceRow {
    pub port: String,
    pub kind: &'static str,
    pub label: &'static str,
    pub refuse: Option<String>,
    /// asked before flashing (a USB modem without the bootloader's IDs)
    pub confirm: Option<String>,
}

pub fn device_row(p: &PortInfo, k: PortKind) -> DeviceRow {
    let (kind, label) = match k {
        PortKind::Esp => ("esp", "GBS-Control (ESP8266)"),
        PortKind::Adv => ("adv", "ADV Controller (bootloader)"),
        PortKind::AdvUnconfirmed => ("adv", "USB device, maybe the ADV Controller (bootloader)"),
        PortKind::AdvConsole => ("advConsole", "ADV Controller (application)"),
        PortKind::Unknown => ("unknown", "Unknown device"),
    };
    let confirm = (k == PortKind::AdvUnconfirmed).then(|| UNCONFIRMED_TEXT.to_string());
    DeviceRow { port: p.name.clone(), kind, label, refuse: refuse_reason(k).map(str::to_string), confirm }
}

/// One long job at a time (flash, update, restore...): a second one is refused with "job_busy".
#[derive(Default)]
pub struct Jobs(Mutex<Option<String>>);

pub struct JobGuard<'a>(&'a Jobs);

impl Jobs {
    pub fn start_job(&self, name: &str) -> Result<JobGuard<'_>, String> {
        let mut slot = self.0.lock().unwrap();
        if slot.is_some() {
            return Err("job_busy".into());
        }
        *slot = Some(name.to_string());
        Ok(JobGuard(self))
    }
}

impl Drop for JobGuard<'_> {
    fn drop(&mut self) {
        *self.0 .0.lock().unwrap() = None;
    }
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ProgressEvent {
    pub job: String,
    pub status: Option<String>,
    pub done: Option<u64>,
    pub total: Option<u64>,
    pub finished: Option<String>,
}

fn emitter<'a>(app: &'a AppHandle, job: &'a str) -> impl FnMut(Progress) + 'a {
    move |p| {
        let mut e = ProgressEvent { job: job.to_string(), status: None, done: None, total: None, finished: None };
        match p {
            Progress::Status(s) => e.status = Some(s),
            Progress::Bytes { done, total } => (e.done, e.total) = (Some(done), Some(total)),
            Progress::Done(s) => e.finished = Some(s),
        }
        let _ = app.emit("progress", e);
    }
}

pub fn rows() -> Vec<DeviceRow> {
    list_ports().into_iter().map(|(p, k)| device_row(&p, k)).collect()
}

#[tauri::command(async)]
pub fn list_devices() -> Vec<DeviceRow> {
    rows()
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct FileRow {
    pub path: String,
    pub name: String,
    pub kind: &'static str,
    pub version: Option<String>,
    pub by_size_only: bool,
    pub adv_crc: Option<u32>,
    pub gzipped: bool,
}

fn file_row(path: &Path, f: &FirmwareFile) -> FileRow {
    FileRow {
        path: path.to_string_lossy().into_owned(),
        name: f.name.clone(),
        kind: match f.kind {
            FirmwareKind::Adv => "adv",
            FirmwareKind::Esp => "esp",
            FirmwareKind::Unknown => "unknown",
        },
        version: f.version.clone(),
        by_size_only: f.by_size_only,
        adv_crc: f.adv_crc,
        gzipped: f.gzipped,
    }
}

fn load(path: &str) -> Result<FirmwareFile, String> {
    let p = Path::new(path);
    let bytes = std::fs::read(p).map_err(|e| format!("Cannot read {path}: {e}"))?;
    let name = p.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
    parse_firmware(&name, &bytes).map_err(|e| e.to_string())
}

#[tauri::command(async)]
pub fn read_file(path: String) -> Result<FileRow, String> {
    Ok(file_row(Path::new(&path), &load(&path)?))
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct CandidatesRow {
    pub esp: Option<FileRow>,
    pub esp_wifi: Option<FileRow>,
    pub adv: Option<FileRow>,
    pub dir: String,
    pub translocated: bool,
    pub from_zip: bool,
}

#[tauri::command(async)]
pub fn candidates() -> CandidatesRow {
    let dir = app_dir();
    let c = find_candidates(&dir);
    let row = |f: &FirmwareFile| file_row(&dir.join(&f.name), f);
    CandidatesRow { esp: c.esp.as_ref().map(row), esp_wifi: c.esp_wifi.as_ref().map(row), adv: c.adv.as_ref().map(row), dir: dir.to_string_lossy().into_owned(), translocated: translocated(&dir), from_zip: run_from_zip(&dir) }
}

fn stub_path(app: &AppHandle) -> Option<PathBuf> {
    find_stub(app.path().resource_dir().ok().as_deref())
}

#[tauri::command]
pub fn stub_present(app: AppHandle) -> bool {
    stub_path(&app).and_then(|p| load_stub(&p)).is_some()
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct FlashResult {
    pub checked: Option<bool>,
    pub used_stub: bool,
    pub seconds: f32,
}

/// Opens a port for a job; a port that has just appeared may not open at once, so it is tried again for a second
/// before giving up (nothing has been sent yet).
fn open_port(port: &str) -> Result<SerialportLink, String> {
    let mut tries = 0;
    loop {
        match SerialportLink::open(port, 115_200) {
            Ok(l) => return Ok(l),
            Err(_) if tries < 4 => {
                tries += 1;
                std::thread::sleep(Duration::from_millis(250));
            }
            Err(e) => return Err(format!("Cannot open {port}: {e}")),
        }
    }
}

#[tauri::command(async)]
pub fn flash(app: AppHandle, jobs: State<'_, Jobs>, port: String, path: Option<String>, erase: String, force: bool) -> Result<FlashResult, String> {
    let _job = jobs.start_job("flash")?;
    let fw = path.as_deref().map(load).transpose()?;
    if let Some(f) = &fw {
        if f.by_size_only && !force {
            return Err(format!("{} has no GBSC header or version marker.", f.name));
        }
    }
    let kind = list_ports().into_iter().find(|(p, _)| p.name == port).map(|(_, k)| k).ok_or("The device is no longer connected.")?;
    if let Some(why) = refuse_reason(kind) {
        return Err(why.into());
    }
    let want = if fw.as_ref().is_some_and(|f| f.kind == FirmwareKind::Adv) { PortKind::Adv } else { PortKind::Esp };
    // an unconfirmed USB modem: the UI asked the user before this call
    let kind = if kind == PortKind::AdvUnconfirmed { PortKind::Adv } else { kind };
    if kind != want {
        return Err("This file is not for this device.".into());
    }
    let mut link = open_port(&port)?;
    let mut progress = emitter(&app, "flash");
    if want == PortKind::Adv {
        adv_flash(&mut link, fw.as_ref().unwrap(), &mut progress).map_err(|e| e.user_text())?;
        return Ok(FlashResult { checked: None, used_stub: false, seconds: 0.0 });
    }
    let stub = stub_path(&app).and_then(|p| load_stub(&p));
    let erase = match erase.as_str() {
        "user" => Erase::User,
        "wifi" => Erase::Wifi,
        "all" => Erase::All,
        _ => Erase::None,
    };
    let r = esp_flash(&mut link, fw.as_ref(), stub.as_ref(), &EspOptions { erase, ..EspOptions::default() }, &mut progress).map_err(|e| e.user_text())?;
    Ok(FlashResult { checked: r.md5_ok, used_stub: r.used_stub, seconds: r.seconds })
}

#[tauri::command(async)]
pub fn find_units() -> Vec<String> {
    core_find_units(Duration::from_secs(3))
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct UnitRow {
    pub fw: String,
    pub board: String,
    pub adv_version: Option<String>,
    pub adv_update_available: bool,
    pub safe_mode: bool,
}

#[tauri::command(async)]
pub fn unit_info(host: String) -> Result<UnitRow, String> {
    let i = Unit::new(&host).info().map_err(|e| e.to_string())?;
    Ok(UnitRow { fw: i.fw, board: i.board, adv_version: i.adv_version, adv_update_available: i.adv_update_available, safe_mode: i.safe_mode })
}

#[tauri::command(async)]
pub fn unit_update(app: AppHandle, jobs: State<'_, Jobs>, host: String, path: String) -> Result<String, String> {
    let _job = jobs.start_job("update")?;
    let fw = load(&path)?;
    if fw.kind != FirmwareKind::Esp {
        return Err("This is not a GBS-Control image.".into());
    }
    Unit::new(&host).update(&fw, &mut emitter(&app, "update")).map_err(|e| e.to_string())
}

#[tauri::command(async)]
pub fn unit_update_adv(app: AppHandle, jobs: State<'_, Jobs>, host: String) -> Result<String, String> {
    let _job = jobs.start_job("update-adv")?;
    Unit::new(&host).update_adv_controller(&mut emitter(&app, "update-adv")).map_err(|e| e.to_string())
}

#[tauri::command(async)]
pub fn adv_verify(jobs: State<'_, Jobs>, host: String, path: String) -> Result<bool, String> {
    let _job = jobs.start_job("check")?;
    Unit::new(&host).adv_verify(&load(&path)?).map_err(|e| e.to_string())
}

#[tauri::command(async)]
pub fn unit_backup(jobs: State<'_, Jobs>, host: String, save_path: String) -> Result<(), String> {
    let _job = jobs.start_job("backup")?;
    let text = Unit::new(&host).backup().map_err(|e| e.to_string())?;
    std::fs::write(&save_path, text).map_err(|e| format!("Cannot write {save_path}: {e}"))
}

#[tauri::command(async)]
pub fn read_text(path: String) -> Result<String, String> {
    std::fs::read_to_string(&path).map_err(|e| format!("Cannot read {path}: {e}"))
}

#[tauri::command(async)]
pub fn unit_restore(jobs: State<'_, Jobs>, host: String, text: String) -> Result<(), String> {
    let _job = jobs.start_job("restore")?;
    Unit::new(&host).restore(&text).map_err(|e| e.to_string())
}

#[derive(Debug, Clone, Serialize)]
pub struct ProfileRow {
    pub id: String,
    pub name: String,
    pub region: String,
}

#[tauri::command(async)]
pub fn unit_profiles(host: String) -> Result<Vec<ProfileRow>, String> {
    let list = Unit::new(&host).profiles().map_err(|e| e.to_string())?;
    Ok(list.into_iter().map(|p| ProfileRow { id: p.id, name: p.name, region: p.region }).collect())
}

#[tauri::command(async)]
pub fn unit_profile_export(jobs: State<'_, Jobs>, host: String, id: String, save_path: String) -> Result<(), String> {
    let _job = jobs.start_job("export")?;
    let bytes = Unit::new(&host).profile_export(&id).map_err(|e| e.to_string())?;
    std::fs::write(&save_path, bytes).map_err(|e| format!("Cannot write {save_path}: {e}"))
}

#[tauri::command(async)]
pub fn unit_profile_import(jobs: State<'_, Jobs>, host: String, path: String) -> Result<(), String> {
    let _job = jobs.start_job("import")?;
    let p = Path::new(&path);
    let bytes = std::fs::read(p).map_err(|e| format!("Cannot read {path}: {e}"))?;
    let name = p.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
    Unit::new(&host).profile_import(&name, &bytes).map_err(|e| e.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn info(name: &str) -> PortInfo {
        PortInfo { name: name.into(), ..Default::default() }
    }

    #[test]
    fn device_row_labels() {
        let adv = device_row(&info("/dev/cu.usbmodem1"), PortKind::Adv);
        assert_eq!((adv.kind, adv.label), ("adv", "ADV Controller (bootloader)"));
        assert_eq!(adv.refuse, None);
        let console = device_row(&info("/dev/cu.usbmodem2"), PortKind::AdvConsole);
        assert!(console.refuse.unwrap().contains("HCMGBoot"));
        assert_eq!(device_row(&info("COM3"), PortKind::Esp).label, "GBS-Control (ESP8266)");
    }

    #[test]
    fn an_unconfirmed_usbmodem_asks_before_flashing() {
        let row = device_row(&info("/dev/cu.usbmodemX"), PortKind::AdvUnconfirmed);
        assert_eq!(row.kind, "adv");
        assert!(row.label.contains("maybe"), "{}", row.label);
        assert!(row.confirm.unwrap().contains("Arduino"));
        assert_eq!(device_row(&info("/dev/cu.usbmodem1"), PortKind::Adv).confirm, None);
    }

    #[test]
    fn one_job_at_a_time() {
        let jobs = Jobs::default();
        let first = jobs.start_job("flash").unwrap();
        assert_eq!(jobs.start_job("update").err().as_deref(), Some("job_busy"));
        drop(first);
        assert!(jobs.start_job("update").is_ok());
    }
}
