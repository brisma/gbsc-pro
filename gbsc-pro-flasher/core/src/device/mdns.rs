//! Finding a unit: every unit answers to gbscontrol.local (mDNS, service _http._tcp, instance "gbscontrol").
use std::collections::BTreeSet;
use std::net::ToSocketAddrs;
use std::sync::mpsc;
use std::time::{Duration, Instant};

pub fn find_units(timeout: Duration) -> Vec<String> {
    let mut found = BTreeSet::new();
    // the system resolver first (macOS, Windows 10+, Linux with nss-mdns); it can block, so in a thread
    let (tx, rx) = mpsc::channel();
    std::thread::spawn(move || {
        let ips: Vec<String> = ("gbscontrol.local", 80)
            .to_socket_addrs()
            .map(|a| a.filter(|s| s.is_ipv4()).map(|s| s.ip().to_string()).collect())
            .unwrap_or_default();
        let _ = tx.send(ips);
    });
    if let Ok(ips) = rx.recv_timeout(timeout.min(Duration::from_secs(2))) {
        found.extend(ips);
    }
    if let Ok(daemon) = mdns_sd::ServiceDaemon::new() {
        if let Ok(events) = daemon.browse("_http._tcp.local.") {
            let deadline = Instant::now() + timeout;
            while let Some(left) = deadline.checked_duration_since(Instant::now()) {
                match events.recv_timeout(left) {
                    Ok(mdns_sd::ServiceEvent::ServiceResolved(s)) if s.get_fullname().starts_with("gbscontrol.") => {
                        found.extend(s.get_addresses_v4().iter().map(|a| a.to_string()));
                    }
                    Ok(_) => {}
                    Err(_) => break,
                }
            }
        }
        let _ = daemon.shutdown();
    }
    found.into_iter().collect()
}
