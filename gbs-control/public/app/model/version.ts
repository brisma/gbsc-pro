// Firmware versions. Frozen from 3.0.0, the same order on the device (update-manifest.cpp versionCompare) and in
// tools/ota/version.py: X.Y.Z, then "-devN" < "-betaN" < no suffix with N numeric; any other suffix sorts below
// "-dev" (a local build).

function parse(v: string): [number, number, number, number, number, string] {
  const n = [0, 0, 0];
  let i = 0;
  for (let k = 0; k < 3; k++) {
    const m = /^\d+/.exec(v.substring(i));
    if (m) { n[k] = parseInt(m[0], 10); i += m[0].length; }
    if (v[i] === ".") i++;
  }
  const rest = v.substring(i);
  const s = /^-(beta|dev)(\d+)$/.exec(rest);
  const rank = !rest.startsWith("-") ? 3 : s ? (s[1] === "beta" ? 2 : 1) : 0;
  return [n[0], n[1], n[2], rank, s ? parseInt(s[2], 10) : 0, rest];
}

export function compareVersions(a: string, b: string): number {
  const x = parse(a), y = parse(b);
  for (let i = 0; i < 5; i++) if (x[i] !== y[i]) return x[i] < y[i] ? -1 : 1;
  if ((x[3] === 0 || x[3] === 3) && x[5] !== y[5]) return x[5] < y[5] ? -1 : 1;   // the rest by text, as strcmp
  return 0;
}

const MARK = /GBSCVER=([0-9A-Za-z.\-]+);/;

async function scan(chunks: AsyncIterable<Uint8Array> | ReadableStreamDefaultReader<Uint8Array>): Promise<string | null> {
  const dec = new TextDecoder("latin1");
  let carry = "";
  const reader = "read" in chunks ? chunks : null;
  const next = async (): Promise<Uint8Array | null> => {
    if (!reader) return null;
    const r = await reader.read();
    return r.done ? null : r.value;
  };
  try {
    for (;;) {
      const v = await next();
      if (!v) break;
      const text = carry + dec.decode(v, { stream: true });
      const m = MARK.exec(text);
      if (m) return m[1];
      carry = text.slice(-40);
    }
  } catch { /* the trailer of an older signed image ends the gzip stream with an error: expected */ }
  return null;
}

// GBSCVER=<version>; inside the image (gzip or raw); null when absent or when the browser cannot inflate
export async function readImageVersion(file: Blob): Promise<string | null> {
  const head = new Uint8Array(await file.slice(0, 2).arrayBuffer());
  const gz = head[0] === 0x1f && head[1] === 0x8b;
  if (!gz) return scan(file.stream().getReader());
  const DS = (globalThis as any).DecompressionStream;
  if (!DS) return null;
  return scan(file.stream().pipeThrough(new DS("gzip")).getReader() as ReadableStreamDefaultReader<Uint8Array>);
}

// The recommended offer the shell shows (header button, System dot): /api/info update.offer, else the last
// s:status.update_available; the device recommends only newer builds, so "none", errors and the running version
// (a status line left over from before an install) are no offer
export function offerVersion(infoOffer: string, wsAvailable: string, fw: string): string {
  const ws = wsAvailable && wsAvailable !== "none" && !wsAvailable.startsWith("error:") ? wsAvailable : "";
  const v = infoOffer || ws;
  return v && (!fw || compareVersions(v, fw) > 0) ? v : "";
}
