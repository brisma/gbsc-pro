//! Live device list: USB plug events (nusb) trigger a rescan after 300 ms and again after 1.5 s (the serial node
//! appears after the USB event, on Windows especially); a 2 s poll covers platforms where watching fails.
use crate::commands::{rows, DeviceRow};
use futures_lite::StreamExt;
use std::sync::mpsc;
use std::time::{Duration, Instant};
use tauri::{AppHandle, Emitter};

pub fn start(app: AppHandle) {
    let (tx, rx) = mpsc::channel::<()>();
    std::thread::spawn(move || {
        if let Ok(mut watch) = nusb::watch_devices() {
            futures_lite::future::block_on(async {
                while watch.next().await.is_some() {
                    if tx.send(()).is_err() {
                        break;
                    }
                }
            });
        }
    });
    std::thread::spawn(move || {
        let mut last: Vec<DeviceRow> = Vec::new();
        let mut rescans: Vec<Instant> = Vec::new();
        loop {
            let next = rescans.iter().min().copied().unwrap_or_else(|| Instant::now() + Duration::from_secs(2));
            match rx.recv_timeout(next.saturating_duration_since(Instant::now())) {
                Ok(()) => {
                    let now = Instant::now();
                    rescans = vec![now + Duration::from_millis(300), now + Duration::from_millis(1500)];
                    continue;
                }
                Err(mpsc::RecvTimeoutError::Timeout) => {
                    let now = Instant::now();
                    rescans.retain(|t| *t > now);
                }
                Err(mpsc::RecvTimeoutError::Disconnected) => {
                    std::thread::sleep(Duration::from_secs(2)); // no watcher: poll
                }
            }
            let now_rows = rows();
            if now_rows != last {
                let _ = app.emit("devices-changed", &now_rows);
                last = now_rows;
            }
        }
    });
}
