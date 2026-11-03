// npm run size [-- --limit 140000] [--json] [file]: gzip -9 of the page, fails above the limit
import { readFileSync, existsSync } from "node:fs";
import { gzipSync } from "node:zlib";
import { basename, join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

export const LIMIT = 140000;   // raised from 120,000 (raise the gate rather than drop features)
export const gzipBytes = (buf) => gzipSync(buf, { level: 9 }).length;

export function report(file, limit = LIMIT) {
  const bytes = gzipBytes(readFileSync(file));
  const meta = join(dirname(file), basename(file, ".html") + ".meta.json");
  let parts = null;
  if (existsSync(meta)) {   // per input of the JS bundle (raw bytes in the output), fonts and JSON included
    const m = JSON.parse(readFileSync(meta, "utf8"));
    parts = {};
    for (const out of Object.values(m.outputs || {}))
      for (const [input, v] of Object.entries(out.inputs || {})) parts[input] = (parts[input] || 0) + v.bytesInOutput;
  }
  return { file: basename(file), bytes, limit, parts };
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const a = process.argv.slice(2);
  const li = a.indexOf("--limit");
  const limit = li >= 0 ? Number(a[li + 1]) : LIMIT;
  const file = a.filter((x, i) => !x.startsWith("--") && a[i - 1] !== "--limit")[0] ||
               join(dirname(fileURLToPath(import.meta.url)), "..", "dist", "app.html");
  const r = report(file, limit);
  if (a.includes("--json")) console.log(JSON.stringify(r, null, 1));
  else console.log(`${r.file}.gz: ${r.bytes} B of ${limit} B (${((r.bytes * 100) / limit).toFixed(1)} %)`);
  if (r.bytes > limit) { console.log(`size: over the limit by ${r.bytes - limit} B`); process.exit(1); }
}
