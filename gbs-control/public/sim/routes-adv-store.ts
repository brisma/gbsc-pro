// ADV presets and macros stored on the device (adv-store-web.cpp): list, text, upload (multipart, 202 then
// s:status.adv_store), delete; the text rules of adv-store.cpp (advYamlFeed)
import type { Ctx } from "./http.ts";
import { arg, sendJson, parseMultipart } from "./http.ts";
import type { Sim } from "./server.ts";

export type AdvKind = "preset" | "macro";
const MAX = { preset: 16, macro: 32 }, MAX_BYTES = { preset: 8192, macro: 2048 };
const PREFIX = { preset: "p", macro: "m" };

export interface AdvCheck { ok: boolean; why: string | null; name: string; description: string; count: number }

function scalar(s: string, cap: number): string {
  let v = s.trim();
  if (v.length >= 2 && (v[0] === "'" || v[0] === '"') && v.at(-1) === v[0]) v = v.slice(1, -1);
  const b = Buffer.from(v, "utf8");
  if (b.length <= cap) return v;
  let n = cap;
  while (n && (b[n] & 0xc0) === 0x80) n--;
  return b.subarray(0, n).toString("utf8");
}

export function advCheck(data: Buffer, maxBytes: number): AdvCheck {
  const r: AdvCheck = { ok: false, why: null, name: "", description: "", count: 0 };
  if (data.length > maxBytes) return { ...r, why: "size" };
  for (const b of data) if ((b < 0x20 && b !== 0x0a && b !== 0x0d && b !== 0x09) || b === 0x7f) return { ...r, why: "control" };
  for (const raw of data.toString("utf8").split("\n")) {
    const l = raw.replace(/\r/g, "");
    if (Buffer.byteLength(l, "utf8") > 127) return { ...r, why: "line" };
    if (l.startsWith("[")) return { ...r, why: "section" };
    if (l.startsWith("name:")) { r.name = scalar(l.slice(5), 32); continue; }
    if (l.startsWith("description:")) { r.description = scalar(l.slice(12), 64); continue; }
    const t = l.trimStart();
    if ((/^['"]0[xX]/.test(t) && t.includes(":")) || t.startsWith("- ")) r.count++;
  }
  if (!r.name) return { ...r, why: "name" };
  return { ...r, ok: true };
}

export class AdvStore {
  files = { preset: new Map<number, string>(), macro: new Map<number, string>() };
  busy = false;
}

const id = (k: AdvKind, i: number) => PREFIX[k] + String(i).padStart(2, "0");
function parseId(k: AdvKind, s: string | null): number {
  const m = s ? new RegExp(`^${PREFIX[k]}(\\d\\d)$`).exec(s) : null;
  const i = m ? Number(m[1]) : -1;
  return i >= 0 && i < MAX[k] ? i : -1;
}
const allowed = (sim: Sim) => sim.state.devmode && sim.state.board === "pro";

export function listGet(k: AdvKind) {
  return (sim: Sim, ctx: Ctx): void => {
    if (!allowed(sim)) return sendJson(ctx, 404, { error: "none" });
    const items = [...sim.adv.files[k].entries()].sort((a, b) => a[0] - b[0]).map(([i, text]) => {
      const c = advCheck(Buffer.from(text, "utf8"), MAX_BYTES[k]);
      return { id: id(k, i), name: c.name, description: c.description, count: c.count, bytes: Buffer.byteLength(text, "utf8") };
    });
    sendJson(ctx, 200, { items });
  };
}

export function textGet(k: AdvKind) {
  return (sim: Sim, ctx: Ctx): void => {
    if (!allowed(sim)) return sendJson(ctx, 404, { error: "none" });
    const text = sim.adv.files[k].get(parseId(k, arg(ctx, "id")));
    if (text === undefined) return sendJson(ctx, 404, { error: "unknown" });
    ctx.res.writeHead(200, { "Content-Type": "text/yaml", "Cache-Control": "no-store" });
    ctx.res.end(text);
  };
}

export function savePost(k: AdvKind) {
  return (sim: Sim, ctx: Ctx): void => {
    if (!allowed(sim)) return sendJson(ctx, 404, { error: "none" });
    if (sim.adv.busy) return sendJson(ctx, 409, { error: "busy" });
    const parts = parseMultipart(ctx.req, ctx.body);
    const file = parts.find((p) => p.filename !== null);
    if (!file || !file.data.length) return sendJson(ctx, 400, { ok: 0, error: "invalid" });
    if (file.data.length > MAX_BYTES[k]) return sendJson(ctx, 400, { ok: 0, error: "size" });
    if (sim.faults.has("advSpace")) return sendJson(ctx, 507, { error: "space" });
    const given = parts.find((p) => p.filename === null && p.name === "id")?.data.toString("utf8") || arg(ctx, "id");
    let i = given ? parseId(k, given) : -1;
    if (given && i < 0) return sendJson(ctx, 400, { ok: 0, error: "id" });
    if (!given) for (let n = 0; n < MAX[k] && i < 0; n++) if (!sim.adv.files[k].has(n)) i = n;
    if (i < 0) return sendJson(ctx, 409, { error: "full" });
    sim.adv.busy = true;
    sendJson(ctx, 202, { ok: 1, id: id(k, i) });
    sim.later(sim.opDelayMs, () => {
      sim.adv.busy = false;
      const c = advCheck(file.data, MAX_BYTES[k]);
      if (!c.ok) { sim.state.log("adv", "info", `adv store: ${id(k, i)} refused, ${c.why}`); sim.state.send(`s:status.adv_store=error:${c.why}`); return; }
      sim.adv.files[k].set(i, file.data.toString("utf8"));
      sim.state.log("adv", "info", `adv store: ${id(k, i)} saved "${c.name}"`);
      sim.state.send("s:status.adv_store=ok");
    });
  };
}

export function deletePost(k: AdvKind) {
  return (sim: Sim, ctx: Ctx): void => {
    if (!allowed(sim)) return sendJson(ctx, 404, { error: "none" });
    const i = parseId(k, arg(ctx, "id"));
    if (i < 0 || !sim.adv.files[k].has(i)) return sendJson(ctx, 404, { error: "unknown" });
    if (sim.adv.busy) return sendJson(ctx, 409, { error: "busy" });
    sim.adv.busy = true;
    sendJson(ctx, 202, { ok: 1 });
    sim.later(sim.opDelayMs, () => {
      sim.adv.busy = false;
      sim.adv.files[k].delete(i);
      sim.state.send("s:status.adv_store=ok");
    });
  };
}

// Backup sections ([adv] marker, verbatim YAML) and their restore
export function advBackupText(sim: Sim): string {
  let t = "[adv]\n";
  for (const k of ["preset", "macro"] as AdvKind[])
    for (const [i, text] of [...sim.adv.files[k].entries()].sort((a, b) => a[0] - b[0]))
      t += `[adv ${k} ${id(k, i)}]\n${text.endsWith("\n") ? text : text + "\n"}`;
  return t;
}

export function advRestore(sim: Sim, text: string): void {
  if (!/^\[adv\]$/m.test(text)) return;
  sim.adv.files.preset.clear();
  sim.adv.files.macro.clear();
  let cur: { k: AdvKind; i: number; lines: string[] } | null = null;
  const close = () => { if (cur) sim.adv.files[cur.k].set(cur.i, cur.lines.filter((l) => l.length).join("\n") + "\n"); cur = null; };
  for (const line of text.split("\n")) {
    if (line.startsWith("[")) {
      close();
      const m = /^\[adv (preset|macro) ([pm]\d\d)\]$/.exec(line);
      if (m) { const k = m[1] as AdvKind; const i = parseId(k, m[2]); if (i >= 0) cur = { k, i, lines: [] }; }
      continue;
    }
    cur?.lines.push(line);
  }
  close();
}
