// The ADV Controller CLI syntax on the page (adv-controller/src/adv_cli.c): w AA RR VV, wm AA MM RR VV, r AA RR,
// rm AA MM RR, d AA [RR NN], dm AA MM [RR NN], info, help; log and update exist only on the USB console. Addresses:
// 42 (ADV7280, maps 00/20/40), 56 (ADV7391), 84 (VPP); hex without 0x.
import type { AdvChange } from "./planner.ts";

export type CliCommand =
  | { kind: "write"; change: AdvChange }
  | { kind: "read"; dev: number; map: number; reg: number }
  | { kind: "dump"; dev: number; map: number; reg: number; count: number }
  | { kind: "info" } | { kind: "help" } | { kind: "usb"; name: string }
  | { kind: "error"; message: string };

export const CLI_HELP = [
  "w AA RR VV       write a register", "wm AA MM RR VV   write in a map (42: 00, 20, 40)", "r AA RR          read a register",
  "rm AA MM RR      read in a map", "d AA [RR NN]     dump (all 256 without RR NN)", "dm AA MM [RR NN] dump in a map",
  "info             ADV Controller version and state", "AA: 42 decoder, 56 encoder, 84 VPP · hex without 0x",
];

const DEVS = [0x42, 0x56, 0x84];
const err = (message: string): CliCommand => ({ kind: "error", message });

export function parseCli(line: string): CliCommand {
  const t = line.trim().split(/\s+/).filter(Boolean);
  const op = (t[0] ?? "").toLowerCase();
  if (!op) return err("type a command (help lists them)");
  if (op === "help") return t.length === 1 ? { kind: "help" } : err("help takes no arguments");
  if (op === "info") return t.length === 1 ? { kind: "info" } : err("info takes no arguments");
  if (op === "log" || op === "update") return { kind: "usb", name: op };
  const shape: Record<string, number[]> = { w: [3], wm: [4], r: [2], rm: [3], d: [1, 3], dm: [2, 4] };
  if (!shape[op]) return err(`unknown command "${op}" (help lists them)`);
  const args = t.slice(1);
  if (!shape[op].includes(args.length)) return err(`${op}: wrong number of values (help shows the syntax)`);
  const n = args.map((a) => (/^[0-9a-fA-F]{1,2}$/.test(a) ? parseInt(a, 16) : NaN));
  if (n.some((v) => Number.isNaN(v))) return err(`${op}: values are hex bytes without 0x`);
  const dev = n[0];
  if (!DEVS.includes(dev)) return err(`${op}: address ${args[0]} is not 42, 56 or 84`);
  const mapped = op.endsWith("m");
  const map = mapped ? n[1] : 0;
  if (mapped && (dev !== 0x42 || ![0x00, 0x20, 0x40].includes(map))) return err(`${op}: maps exist only on 42 (00, 20, 40)`);
  const rest = n.slice(mapped ? 2 : 1);
  if (op === "w" || op === "wm") return { kind: "write", change: { i2c: dev, map, reg: rest[0], value: rest[1] } };
  if (op === "r" || op === "rm") return { kind: "read", dev, map, reg: rest[0] };
  const reg = rest.length ? rest[0] : 0, count = rest.length ? rest[1] : 256;
  if (!count) return err(`${op}: count 0`);
  if (reg + count > 0x100) return err(`${op}: the range passes 0xFF`);
  return { kind: "dump", dev, map, reg, count };
}
