// Per-browser preferences in localStorage (private windows and blocked storage throw or forget: never fatal)

export type PersistKey = "gbsc.mode" | "gbsc.quickPins" | "gbsc.profileGuide" | "gbsc.consoleFilter" | "gbsc.lastPreset";

function storage(): Storage | null {
  try { return typeof localStorage === "undefined" ? null : localStorage; } catch { return null; }
}

export function readPref<T>(key: PersistKey, fallback: T): T {
  try {
    const raw = storage()?.getItem(key);
    return raw === null || raw === undefined ? fallback : (JSON.parse(raw) as T);
  } catch {
    return fallback;
  }
}

export function writePref<T>(key: PersistKey, value: T): void {
  try { storage()?.setItem(key, JSON.stringify(value)); } catch { /* storage full or blocked: keep the session value */ }
}
