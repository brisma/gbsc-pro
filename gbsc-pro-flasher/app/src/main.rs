// GBSC PRO Flasher: the desktop app (Tauri 2). Behaviour lives in gbsc-core; this crate wires commands and events.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]
mod commands;
mod hotplug;

fn main() {
    tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .manage(commands::Jobs::default())
        .setup(|app| {
            hotplug::start(app.handle().clone());
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::list_devices,
            commands::read_file,
            commands::candidates,
            commands::stub_present,
            commands::flash,
            commands::find_units,
            commands::unit_info,
            commands::unit_update,
            commands::unit_update_adv,
            commands::adv_verify,
            commands::unit_backup,
            commands::read_text,
            commands::unit_restore,
            commands::unit_profiles,
            commands::unit_profile_export,
            commands::unit_profile_import,
        ])
        .run(tauri::generate_context!())
        .expect("error while running GBSC PRO Flasher");
}
