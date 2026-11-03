// The ADV Manager YAML subset (shared/adv-regmap presets and macros, files exported by ADV Manager): presets name, description, devices -> chip ->
// maps -> map -> map_value, registers {'0xNN': '0xNN'}; macros name, description, commands (w / wm lines run, any
// other line is kept verbatim and marked "not run"). The legacy flat preset layout is refused.
import type { AdvChange } from "./planner.ts";

export interface PresetMap { id: string; mapValue: number; regs: [number, number][] }
export interface PresetDoc { kind: "preset"; name: string; description?: string; devices: { chip: string; maps: PresetMap[] }[] }
export interface MacroCommand { text: string; run: boolean; write?: AdvChange }
export interface MacroDoc { kind: "macro"; name: string; description?: string; commands: MacroCommand[] }
export type AdvDoc = PresetDoc | MacroDoc;
export type ParseResult = { ok: true; doc: AdvDoc } | { ok: false; error: string };

const LEGACY = "old ADV Manager preset: open and save it in ADV Manager first";

type Node = string | Node[] | Map<string, Node>;

function unquote(s: string): string {
  const t = s.trim();
  if (t.length >= 2 && t[0] === "'" && t.endsWith("'")) return t.slice(1, -1).replace(/''/g, "'");
  if (t.length >= 2 && t[0] === '"' && t.endsWith('"')) return t.slice(1, -1).replace(/\\(["\\])/g, "$1");
  return t.replace(/\s+#.*$/, "");   // a comment after a plain value
}

// key: value / key: (block) / - item, by indentation; enough for the ADV Manager files
function parseBlock(lines: { ind: number; text: string }[], i: number, ind: number): [Node, number] {
  if (i < lines.length && lines[i].text.startsWith("- ")) {
    const list: Node[] = [];
    while (i < lines.length && lines[i].ind === ind && lines[i].text.startsWith("- ")) { list.push(unquote(lines[i].text.slice(2))); i++; }
    return [list, i];
  }
  const map = new Map<string, Node>();
  while (i < lines.length && lines[i].ind === ind) {
    const t = lines[i].text;
    const m = /^('[^']*'|"[^"]*"|[^:]+?)\s*:(?:\s+(.*))?$/.exec(t);
    if (!m) throw new Error(`line "${t}" is not key: value`);
    const key = unquote(m[1]);
    i++;
    if (m[2] !== undefined && m[2] !== "") { map.set(key, unquote(m[2])); continue; }
    if (i < lines.length && (lines[i].ind > ind || (lines[i].ind === ind && lines[i].text.startsWith("- ")))) {
      const [child, next] = parseBlock(lines, i, lines[i].ind);
      map.set(key, child);
      i = next;
    } else map.set(key, "");
  }
  return [map, i];
}

const num = (s: Node | undefined): number => { const t = String(s ?? "0"); return /^0x/i.test(t) ? parseInt(t, 16) : parseInt(t, 10); };
const asMap = (n: Node | undefined): Map<string, Node> => (n instanceof Map ? n : new Map());

function command(text: string): MacroCommand {
  const p = text.trim().split(/\s+/);
  const hx = (s: string) => (/^[0-9a-fA-F]{1,2}$/.test(s) ? parseInt(s, 16) : NaN);
  let write: AdvChange | undefined;
  if (p[0] === "w" && p.length === 4) write = { i2c: hx(p[1]), map: 0, reg: hx(p[2]), value: hx(p[3]) };
  if (p[0] === "wm" && p.length === 5) write = { i2c: hx(p[1]), map: hx(p[2]), reg: hx(p[3]), value: hx(p[4]) };
  if (write && Object.values(write).some((v) => Number.isNaN(v))) write = undefined;
  return write ? { text: text.trim(), run: true, write } : { text: text.trim(), run: false };
}

export function parseAdvYaml(text: string): ParseResult {
  const lines = text.replace(/\r/g, "").split("\n")
    .filter((l) => l.trim() && !l.trim().startsWith("#"))
    .map((l) => ({ ind: l.length - l.trimStart().length, text: l.trim() }));
  let root: Map<string, Node>;
  try { root = asMap(lines.length ? parseBlock(lines, 0, lines[0].ind)[0] : new Map()); }
  catch (e) { return { ok: false, error: (e as Error).message }; }
  const name = String(root.get("name") ?? "");
  const description = root.has("description") ? String(root.get("description")) : undefined;
  if (root.has("commands")) {
    const cmds = root.get("commands");
    return { ok: true, doc: { kind: "macro", name, description, commands: (Array.isArray(cmds) ? cmds : []).map((c) => command(String(c))) } };
  }
  if (root.has("devices")) {
    const devices: PresetDoc["devices"] = [];
    for (const [chip, d] of asMap(root.get("devices"))) {
      const maps: PresetMap[] = [];
      for (const [id, mp] of asMap(asMap(d).get("maps"))) {
        if (id === "default") return { ok: false, error: LEGACY };
        const regs: [number, number][] = [...asMap(asMap(mp).get("registers"))].map(([k, v]) => [num(k), num(v)]);
        maps.push({ id, mapValue: num(asMap(mp).get("map_value")), regs });
      }
      devices.push({ chip, maps });
    }
    return { ok: true, doc: { kind: "preset", name, description, devices } };
  }
  if (root.has("registers")) return { ok: false, error: LEGACY };
  return { ok: false, error: "not an ADV Manager preset or macro" };
}

// Plain only when YAML (PyYAML in ADV Manager) reads it back as the same string: never a bool, null or number word
const SPECIAL = /^(y|yes|n|no|true|false|on|off|null|~)$/i;
const NUMBER = /^[-+]?(\d|\.\d)|^0x/i;
function scalar(s: string): string {
  const plain = /^[A-Za-z0-9][A-Za-z0-9 _().,+/-]*$/.test(s) && !s.endsWith(" ") && !SPECIAL.test(s) && !NUMBER.test(s);
  return plain ? s : `'${s.replace(/'/g, "''")}'`;
}
const hx = (n: number) => `'0x${n.toString(16).toUpperCase().padStart(2, "0")}'`;

export function writeAdvYaml(doc: AdvDoc): string {
  let t = `name: ${scalar(doc.name)}\n`;
  if (doc.description !== undefined) t += `description: ${scalar(doc.description)}\n`;
  if (doc.kind === "macro") return t + "commands:\n" + doc.commands.map((c) => `  - ${scalar(c.text)}\n`).join("");
  t += "devices:\n";
  for (const d of doc.devices) {
    t += `  ${d.chip}:\n    maps:\n`;
    for (const m of d.maps) {
      t += `      ${m.id}:\n        map_value: ${hx(m.mapValue)}\n        registers:\n`;
      for (const [r, v] of m.regs) t += `          ${hx(r)}: ${hx(v)}\n`;
    }
  }
  return t;
}
