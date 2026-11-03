// Update words: the device's reason codes (ota-plan.cpp kWords, update-device.cpp, update-trailer.cpp) said in
// full, the channel names and the "Last result" line
export const CHANNELS = ["stable", "beta"] as const;
export type Channel = (typeof CHANNELS)[number];
export const channelLabel = (c: string) => (c ? c[0].toUpperCase() + c.slice(1) : "");

const WORDS: Record<string, string> = {
  no_wifi: "the device is not on your WiFi (hotspot mode)", dns: "the update server name was not found", connect: "no connection to the update server",
  timeout: "the server stopped answering", short: "the download was cut off", http: "the server answered with an error",
  manifest: "the server's list was not readable", size: "the file is not the expected size", sha: "the checksum did not match",
  format: "the file is not a firmware image", trailer: "the image ends in a way the device does not know", too_big: "the image is too big for this device",
  verify: "the image did not verify", signature: "the image signature was refused", memory: "not enough free memory", busy: "another update is running",
  space: "not enough flash space", write: "a flash write failed", incomplete: "the upload was cut off", changed: "the offer changed: check again",
};
export const reasonWords = (code: string) => WORDS[code] ?? code;

// status.update_available / update.last: "<version>" | "none" | "error:<code>" | ""
export function lastResultText(last: string): string {
  if (!last) return "Not checked yet";
  if (last === "none") return "Up to date";
  if (last.startsWith("error:")) return `Check failed: ${reasonWords(last.slice(6))}`;
  return `${last} offered`;
}
export function checkResultText(v: string): string {
  if (v === "none") return "Up to date";
  if (v.startsWith("error:")) return `Check failed: ${reasonWords(v.slice(6))}`;
  return `${v} is available`;
}

// A refused upload or restore before the body was taken (503 with X-GBSC-Busy)
export function busyText(kind: string | null, what = "installed"): string {
  if (kind === "memory") return `Not ${what}: not enough free memory on the device right now. Close other pages open on the device, wait a few seconds and try again (a restart frees memory too).`;
  if (kind === "update") return `Not ${what}: another update is running.`;
  return "The device is busy: try again in a moment.";
}
