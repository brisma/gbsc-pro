// The console's view of the log: category filter, debug lines (only /api/log records carry a level; WebSocket l:
// lines have none), pause (the lines up to the moment it was paused), and the "Copy for an issue" report
import type { LogEntry } from "../../store/device.ts";

export const CATEGORIES = ["gbs", "sys", "input", "scan", "sync", "preset", "profile", "adv", "link", "api", "wifi", "update", "menu"];

export interface ConsoleOpts { cat: string; debug: boolean; pausedAt: number | null }

export function consoleLines(lines: LogEntry[], o: ConsoleOpts): LogEntry[] {
  const base = o.pausedAt === null ? lines : lines.slice(0, o.pausedAt);
  return base.filter((l) => (o.cat === "all" || l.c === o.cat) && (o.debug || l.l !== "debug"));
}

// logFormatLine (pro/log/log-ring.cpp): "[%6lu.%03lu] %-10s %s"
export function formatLine(l: LogEntry): string {
  const sec = String(Math.floor(l.t / 1000)).padStart(6, " "), ms = String(l.t % 1000).padStart(3, "0");
  return `[${sec}.${ms}] ${`[${l.c}]`.padEnd(10, " ")} ${l.m}`;
}

// Markdown for a GitHub issue: versions, board, the last crash (epc and stack PCs for addr2line), the last 50 lines.
// The report is pasted in public: the name of the WiFi network the unit is on is taken out of the lines
export function issueReport(info: any, lines: LogEntry[]): string {
  const c = info?.crash;
  const ssid: string = info?.wifi?.ssid ?? "";
  const line = (l: LogEntry) => { const t = formatLine(l); return ssid ? t.split(ssid).join("(network)") : t; };
  const crash = c ? `cause ${c.cause}, epc1 0x${c.epc1}, epc2 0x${c.epc2}, epc3 0x${c.epc3}, excvaddr 0x${c.excvaddr}, depc 0x${c.depc}\nstack PCs: ${c.pcs || "-"}` : "none recorded";
  return [
    "### Device", `- GBS-Control ${info?.fw ?? "?"}`, `- ADV Controller ${info?.adv_controller?.version ?? info?.adv_controller?.status ?? "-"}`,
    `- Board: ${info?.board ?? "?"}`, "", "### Last crash", "```", crash, "```", "", "### Console (last 50 lines)", "```",
    ...lines.slice(-50).map(line), "```", "",
  ].join("\n");
}
