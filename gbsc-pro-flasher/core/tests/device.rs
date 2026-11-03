// A unit over WiFi, against the web app's device simulator
mod fakes;
use fakes::sim::Sim;
use gbsc_core::device::{backup_file_name, DeviceError};
use gbsc_core::image::{parse_firmware, FirmwareFile};
use gbsc_core::progress::Progress;

fn esp_file(version: &str) -> FirmwareFile {
    let mut v = vec![0xE9, 0x01, 0x00, 0x20];
    v.resize(300 * 1024, 0x33);
    let mark = format!("GBSCVER={version};");
    v[4096..4096 + mark.len()].copy_from_slice(mark.as_bytes());
    parse_firmware(&format!("gbsc-pro-{version}.bin"), &v).unwrap()
}

#[test]
fn info_reads_fw_and_board() {
    let sim = Sim::start();
    let i = sim.unit().info().unwrap();
    assert_eq!(i.board, "pro");
    assert!(!i.fw.is_empty());
    assert_eq!(i.adv_version.as_deref(), Some("3.0.0"));
    assert_eq!(i.adv_build, Some(0x446F_C362));
}

#[test]
fn update_streams_progress_and_returns_new_version() {
    let sim = Sim::start();
    let mut pcts = Vec::new();
    let v = sim.unit().update(&esp_file("3.0.1-dev0"), &mut |p| {
        if let Progress::Bytes { done, total: 100 } = p {
            pcts.push(done)
        }
    });
    assert_eq!(v.unwrap(), "3.0.1-dev0");
    assert!(pcts.contains(&50) && pcts.contains(&100), "{pcts:?}");
}

#[test]
fn update_reports_failure_when_version_unchanged() {
    let sim = Sim::start();
    sim.fault("updateNotApplied");
    let r = sim.unit().update(&esp_file("3.0.1-dev0"), &mut |_| {});
    assert!(matches!(r, Err(DeviceError::NotApplied { .. })), "{r:?}");
}

#[test]
fn info_tells_safe_mode() {
    let sim = Sim::start();
    assert!(!sim.unit().info().unwrap().safe_mode);
    sim.scenario("safe-mode");
    assert!(sim.unit().info().unwrap().safe_mode);
}

#[test]
fn update_in_safe_mode_polls_info() {
    let sim = Sim::start();
    sim.scenario("safe-mode");
    assert_eq!(sim.unit().update(&esp_file("3.0.1-dev0"), &mut |_| {}).unwrap(), "3.0.1-dev0");
}

#[test]
fn a_failed_install_in_safe_mode_tells_the_reason() {
    // no WebSocket in safe mode: the reason is in /api/info upload, the unit does not restart
    let sim = Sim::start();
    sim.scenario("safe-mode");
    sim.fault("uploadFail");
    let t = std::time::Instant::now();
    let r = sim.unit().update(&esp_file("3.0.1-dev0"), &mut |_| {});
    assert!(matches!(r, Err(DeviceError::Failed(ref why)) if why == "verify"), "{r:?}");
    assert!(t.elapsed() < std::time::Duration::from_secs(30), "{:?}", t.elapsed());
}

#[test]
fn adv_update_polls_until_done() {
    let sim = Sim::start();
    sim.scenario("adv-update-offer");
    let u = sim.unit();
    assert!(u.info().unwrap().adv_update_available);
    assert_eq!(u.update_adv_controller(&mut |_| {}).unwrap(), "3.0.0");
}

#[test]
fn post_without_x_gbsc_is_refused() {
    let sim = Sim::start();
    let (st, _) = sim.raw("POST", "/api/actions/system.restart", &[], "");
    assert_eq!(st, 403);
    sim.unit().restore(&sim.unit().backup().unwrap()).unwrap(); // the client's POSTs pass the same guard
}

#[test]
fn busy_503_is_retried() {
    let sim = Sim::start();
    sim.fault("busyOnce");
    sim.unit().info().unwrap();
}

#[test]
fn backup_then_restore_roundtrip() {
    let sim = Sim::start();
    let u = sim.unit();
    let text = u.backup().unwrap();
    assert!(text.starts_with("# gbsc-backup 1"));
    assert!(matches!(u.restore("not a backup"), Err(DeviceError::Failed(_)))); // first: a good restore restarts the unit
    u.restore(&text).unwrap();
}

#[test]
fn profile_export_import() {
    let sim = Sim::start();
    let u = sim.unit();
    let (st, _) = sim.raw("POST", "/api/profiles/saveas", &[("X-GBSC", "1"), ("Content-Type", "application/x-www-form-urlencoded")], "name=Mega%20Drive");
    assert_eq!(st, 202);
    let mut list = Vec::new();
    for _ in 0..50 {
        list = u.profiles().unwrap();
        if !list.is_empty() {
            break;
        }
        std::thread::sleep(std::time::Duration::from_millis(50));
    }
    assert_eq!(list[0].name, "Mega Drive");
    let text = u.profile_export(&list[0].id).unwrap();
    assert!(!text.is_empty());
    u.profile_import("Mega_Drive.gbscprofile.txt", &text).unwrap();
    assert_eq!(u.profiles().unwrap().len(), 2);
}

#[test]
fn adv_verify_compares_build_with_header_crc() {
    let sim = Sim::start();
    let mut fw = esp_file("3.0.0");
    fw.adv_crc = Some(0x446F_C362);
    assert!(sim.unit().adv_verify(&fw).unwrap());
    fw.adv_crc = Some(1);
    assert!(!sim.unit().adv_verify(&fw).unwrap());
}

#[test]
fn backup_file_name_format() {
    assert_eq!(backup_file_name("192.168.0.98", "2026-10-22"), "gbsc-backup-192.168.0.98-2026-10-22.txt");
}

#[test]
fn a_slow_unit_still_answers() {
    // a GBSC searching for a signal answered /api/info in up to 4.4 s
    let sim = Sim::start();
    sim.net("slow");
    assert_eq!(sim.unit().info().unwrap().board, "pro");
}

#[test]
fn update_reports_a_version_other_than_the_file() {
    // the simulator restarts with 3.0.1-dev0: a 3.0.2 file did not end up on the unit
    let sim = Sim::start();
    let r = sim.unit().update(&esp_file("3.0.2"), &mut |_| {});
    assert!(matches!(r, Err(DeviceError::NotApplied { ref version }) if version == "3.0.1-dev0"), "{r:?}");
}

#[test]
fn adv_update_done_needs_a_change_first() {
    use gbsc_core::device::adv_update_finished;
    // a "done" left from an earlier run, before this one started, is not this run's result
    assert!(!adv_update_finished("done", false, "done", "ok", "3.0.0"));
    assert!(adv_update_finished("done", true, "done", "ok", "3.0.0"));
    assert!(adv_update_finished("idle", false, "done", "ok", "3.0.0"));
    assert!(!adv_update_finished("idle", true, "done", "checking", ""));
}

#[test]
fn adv_update_failure_needs_a_change_first() {
    use gbsc_core::device::adv_update_failure;
    // the device applies the action on its next loop: the first poll can still show the last run's failure
    assert_eq!(adv_update_failure("failed:erase", false, "failed:erase"), None);
    assert_eq!(adv_update_failure("failed:erase", true, "failed:erase"), Some("erase")); // running in between
    assert_eq!(adv_update_failure("failed:erase", false, "failed:busy"), Some("busy"));
    assert_eq!(adv_update_failure("idle", false, "failed:send"), Some("send"));
    assert_eq!(adv_update_failure("idle", false, "running"), None);
}

#[test]
fn a_host_name_gets_the_status_socket() {
    // gbscontrol.local is a name, not an IP: without the WebSocket a refused restore would read as a success
    let sim = Sim::start();
    let mut u = gbsc_core::device::Unit::with_ports("localhost", sim.port, sim.ws);
    u.poll = std::time::Duration::from_millis(100);
    let r = u.restore("not a backup");
    assert!(matches!(r, Err(DeviceError::Failed(_))), "{r:?}");
}

#[test]
fn unreachable_on_macos_points_at_the_local_network_permission() {
    // macOS answers EHOSTUNREACH while the app has no Local Network decision yet (first start, a new build)
    let e = "io: No route to host (os error 65)";
    assert!(gbsc_core::device::unreachable_hint(e, true).contains("Local Network"));
    assert_eq!(gbsc_core::device::unreachable_hint(e, false), "Check that it is on and on this network.");
    assert_eq!(gbsc_core::device::unreachable_hint("io: Connection refused (os error 61)", true), "Check that it is on and on this network.");
}
