// Network profiles. "device" from a device recording (a Pro on WiFi at -58 dBm): small answers 32-46 ms, JSON bodies
// 15-35 KB/s (schema 11.6 KB in 366 ms, log 4.6 KB in 264 ms).
// "slow": a GBSC searching for a signal answered /api/info in 0.5-4.4 s; 6 s covers the worst case.
export type NetProfile = "fast" | "device" | "slow";
export const PROFILES: Record<NetProfile, { latencyMs: number; bytesPerSec: number }> = {
  fast: { latencyMs: 0, bytesPerSec: 0 },
  device: { latencyMs: 35, bytesPerSec: 25000 },
  slow: { latencyMs: 6000, bytesPerSec: 0 },
};
export const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));
