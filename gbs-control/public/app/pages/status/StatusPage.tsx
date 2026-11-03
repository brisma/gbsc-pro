// Status, one page: the Console (category filter with the debug lines, pause, copy, download, clear; it follows the
// newest line until the reader scrolls up), Signal and history (the state as facts and the events from /api/events),
// Diagnostics (facts and "Copy for an issue").
import { signal } from "@preact/signals";
import { useEffect, useRef, useState } from "preact/hooks";
import { Icon } from "../../art/icon.tsx";
import { device, desk, board, unsaved, mode, pred } from "../../store/app.ts";
import { openSheet, closeSheet, SheetButtons } from "../../shell/Sheet.tsx";
import { download, copyText } from "../../shell/files.ts";
import { eventText, ago } from "../../texts/events.ts";
import { rowOptions } from "../../rows/controls.tsx";
import { choiceValue, choiceOptions } from "../../model/choices.ts";
import { profileLabel } from "../../store/profiles.ts";
import { activeProfile } from "../profiles/ops.tsx";
import { Card, GroupBand } from "../../sections/Card.tsx";
import type { LogEntry } from "../../store/device.ts";
import { consoleLines, formatLine, issueReport, CATEGORIES } from "./console-model.ts";

const cat = signal("all");
const debug = signal(true);
const pausedAt = signal<number | null>(null);
const follow = signal(true);   // keep the newest line in view: on until the reader scrolls up, on again at the bottom
const clearedKey = signal<string | null>(null);   // the last line when Clear was pressed (view only)
const allEvents = signal(false);   // History shows the newest few until asked for all
const HISTORY_SHORT = 5;

const keyOf = (l: LogEntry) => `${l.t}|${l.c}|${l.m}`;
const inputName = (n: number) => rowOptions(undefined, "input.source").find((o) => o.v === n)?.n ?? `input ${n}`;

function visibleLines(): LogEntry[] {
  const all = device.log.value;
  const from = clearedKey.value ? all.findIndex((l) => keyOf(l) === clearedKey.value) + 1 : 0;
  return consoleLines(all.slice(from), { cat: cat.value, debug: debug.value, pausedAt: pausedAt.value === null ? null : Math.max(0, pausedAt.value - from) });
}

function ConsoleCard() {
  const [menu, setMenu] = useState(false);
  const box = useRef<HTMLDivElement>(null);
  const lines = visibleLines();
  const paused = pausedAt.value !== null;
  const isDesk = desk.value;
  useEffect(() => {   // the newest line in view
    const b = box.current;
    if (b && follow.value && !paused) b.scrollTop = b.scrollHeight;
  }, [lines.length, paused, follow.value]);
  const onScroll = () => { const b = box.current; if (b) follow.value = b.scrollHeight - b.scrollTop - b.clientHeight < 24; };
  const ICON = { pause: "pause", resume: "play_arrow", copy: "content_copy", download: "download", clear: "delete_sweep" };   // @icons
  const tool = (icon: string, label: string, run: () => void, on?: boolean) => (
    <button type="button" class={`btn ico${on ? " on" : ""}`} aria-label={label} title={label} aria-pressed={on === undefined ? undefined : on} onClick={run}><Icon name={icon} /></button>
  );
  const text = () => lines.map(formatLine).join("\n") + "\n";
  const name = cat.value === "all" ? "All" : cat.value;
  const pick = (c: string) => (
    <button type="button" role="option" aria-selected={cat.value === c} class="mi" onClick={() => { cat.value = c; setMenu(false); }}>
      {cat.value === c ? <Icon name="check" /> : <span />}<span><b>{c === "all" ? "All categories" : c}</b></span>
    </button>
  );
  const filter = (
    <span class="mwrap">
      <button type="button" class="btn" aria-haspopup="listbox" aria-expanded={menu} aria-label={`Category: ${name}`} onClick={() => setMenu(!menu)}><Icon name="filter_list" />{name}</button>
      {menu ? (
        <>
          <div class="scrim fix" onClick={() => setMenu(false)} />
          <div class="pmenu left" role="listbox" aria-label="Category">
            {["all", ...CATEGORIES].map(pick)}
            <div class="msep" />
            <button type="button" class="mi" role="option" aria-selected={debug.value} aria-label="Debug lines" onClick={() => { debug.value = !debug.value; }}>
              {debug.value ? <Icon name="check" /> : <span />}<span><b>Debug lines</b><small>The device prints them in Dev mode</small></span>
            </button>
          </div>
        </>
      ) : null}
    </span>
  );
  const time = (t: number) => (isDesk ? `[${String(Math.floor(t / 1000)).padStart(6, " ")}.${String(t % 1000).padStart(3, "0")}]` : (t / 1000).toFixed(1).padStart(7, " "));
  return (
    <Card icon="terminal" title="Console" sub="Live messages from the device" section="console" actionsCls="con" actions={[
      filter,
      tool(paused ? ICON.resume : ICON.pause, paused ? "Resume" : "Pause", () => { pausedAt.value = paused ? null : device.log.value.length; }, paused),
      tool(ICON.copy, "Copy", async () => device.toast((await copyText(text())) ? `Copied ${lines.length} lines` : "Copy failed: select the text instead", "info")),
      tool(ICON.download, "Download", () => download(text(), `gbsc-log-${new Date().toISOString().slice(0, 19).replace(/[:T]/g, "-")}.txt`, "text/plain")),
      tool(ICON.clear, "Clear", () => { const all = device.log.value; clearedKey.value = all.length ? keyOf(all[all.length - 1]) : null; }),
    ]}>
      <div class="log" ref={box} role="log" aria-live="off" tabIndex={0} onScroll={onScroll} data-truncate>
        {lines.map((l) => (
          <div key={keyOf(l)}><span class="t">{time(l.t)}</span>{" "}
            <span class={l.l === "warn" ? "w" : "c"}>{l.c.padEnd(7, " ")}</span>{" "}<span class={l.l === "debug" ? "d" : ""}>{l.m}</span></div>
        ))}
      </div>
    </Card>
  );
}

function Fact({ k, v, d, pct }: { k: string; v: string; d: string; pct?: number }) {
  return (
    <div class="fact"><span class="fk">{k}</span><b class="fv">{v}</b>
      {pct === undefined ? null : <div class="meter"><i style={`width:${Math.max(0, Math.min(100, pct))}%`} /></div>}<span class="fd">{d}</span></div>
  );
}

const hz = (v: unknown) => `${(Number(v) / 10).toFixed(1)} Hz`;

function SignalCard() {
  const i = device.info.value;
  const v = device.values.value;
  const g = i?.signal ?? {};
  const adv = i?.adv_controller;
  const ev = device.events.value;
  const now = Number(i?.net?.uptime ?? 0) * 1000;
  const lastFound = [...ev].reverse().find((e) => e.type === "input_found" || e.type === "input_changed");
  const lost = ev.filter((e) => e.type === "input_lost");
  const searching = i?.input?.state === "searching";
  const auto = Number(v["input.source"] ?? 0) === 0;
  const resIdx = choiceValue("resolution", v, { basePreset: device.profiles.value?.base });
  const res = choiceOptions("resolution", { mode: mode.value, caps: pred.value.caps, current: resIdx }).find((o) => o.index === resIdx)?.label ?? "Custom";
  const lock = Number(v["output.frame_lock"] ?? 0) !== 0;
  const since = (ms: number) => ago(ms, now).replace(" ago", "");   // "now" for the first seconds
  const p = activeProfile();
  const names = device.profiles.value?.profiles ?? [];
  const ctx = { inputName, profileName: (id: string) => { const x = names.find((n) => n.id === id); return x ? profileLabel(x) : ""; } };
  const every = [...ev].reverse();
  const hist = allEvents.value ? every : every.slice(0, HISTORY_SHORT);
  return (
    <Card icon="sensors" title="Signal and history" sub="Input, output, profile, sync and events" section="signal">
      <div class="facts">
        <Fact k="Input" v={searching ? "Searching" : inputName(Number(i?.input?.active ?? 0))}
              d={`${auto ? "Auto" : "chosen by hand"}${i?.input?.termination ? ", 75 ohm on" : ""}`} />
        <Fact k="Signal" v={g.none ? "No signal" : `${g.lines}${g.interlaced ? "i" : "p"} ${hz(g.src_hz10)}`}
              d={searching ? "still looking" : g.none ? "nothing on this input" : lastFound && since(lastFound.ms) !== "now" ? `stable for ${since(lastFound.ms)}` : "stable"} />
        <Fact k="Output" v={res} d={[Number(g.out_hz10) ? hz(g.out_hz10) : "", lock ? "frame lock" : ""].filter(Boolean).join(", ") || "rate not measured"} />
        <Fact k="Profile" v={p ? profileLabel(p) : "Defaults"} d={unsaved.value ? "unsaved changes" : p ? "saved" : "no profile loaded"} />
        <Fact k="Sync losses" v={lost.length ? String(lost.length) : "None"}
              d={lost.length ? `last one ${since(lost[lost.length - 1].ms) === "now" ? "just now" : ago(lost[lost.length - 1].ms, now)}` : "in the history below"} />
        {board.value === "pro" ? (
          <Fact k="ADV Controller"
                v={adv?.status !== "ok" ? "Not answering" : adv?.signal?.powered ? (adv.signal.lock ? `Locked${adv.signal.standard ? ", " + adv.signal.standard : ""}` : "No lock") : "Decoder off"}
                d={adv?.version ? `${adv.version}, link ${adv.status}` : adv?.status ?? "no answer"} />
        ) : null}
      </div>
      <GroupBand title="History" action={every.length > HISTORY_SHORT
        ? <button type="button" class="btn" aria-expanded={allEvents.value} onClick={() => { allEvents.value = !allEvents.value; }}>
            <Icon name={allEvents.value ? "expand_less" : "expand_more"} />{allEvents.value ? "Show fewer" : `Show all ${every.length}`}</button>
        : undefined} />
      {hist.length ? hist.map((e) => {
        const t = eventText(e, ctx);
        return <div key={e.seq} class={`rw ev ${t.cls}`}><span class="ic"><Icon name={t.icon} /></span><span class="lb"><span>{t.title}</span></span>{t.sub ? <span class="ds">{t.sub}</span> : null}<time class="c">{ago(e.ms, now)}</time></div>;
      }) : <div class="rw note"><span class="ic"><Icon name="info" /></span><span class="ds">Nothing yet since the last restart</span></div>}
    </Card>
  );
}

function uptime(s: number): string {
  const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60);
  return h ? `${h} h ${m} min` : m ? `${m} min` : `${s} s`;
}

function DiagnosticsCard() {
  const i = device.info.value ?? {};
  const w = i.wifi ?? {}, n = i.net ?? {};
  const rssi = Number(w.rssi ?? 0);
  return (
    <Card icon="monitor_heart" title="Diagnostics" sub="Memory, WiFi signal, crashes" section="diagnostics" actionsCls="row"
          actions={[<button type="button" class="btn" onClick={openIssue}><Icon name="content_copy" />Copy for an issue</button>]}>
      <div class="facts">
        <Fact k="Memory" v={`${(Number(i.heap ?? 0) / 1024).toFixed(1)} KB free`} pct={(Number(i.heap ?? 0) / 40960) * 100} d={`largest block ${(Number(i.max_block ?? 0) / 1024).toFixed(1)} KB`} />
        {rssi && String(w.mode ?? "").toLowerCase() !== "ap" ? <Fact k="WiFi signal" v={`${rssi} dBm`} pct={((rssi + 95) / 50) * 100} d={rssi >= -60 ? "good" : rssi >= -72 ? "fair" : "weak"} />
          : <Fact k="WiFi signal" v="Hotspot" d="phones and PCs join the device" />}
        <Fact k="Uptime" v={uptime(Number(n.uptime ?? 0))} d={`last restart: ${String(n.reset ?? "unknown").toLowerCase()}`} />
        <Fact k="Last crash" v={i.crash ? `Exception ${i.crash.cause}` : "None"} d={i.crash ? `epc1 ${i.crash.epc1}` : "since the last flash"} />
        <Fact k="GBS-Control" v={String(i.fw ?? "unknown")} d={[i.update?.channel ?? "", i.board ? `${i.board} board` : ""].filter(Boolean).join(", ")} />
        <Fact k="Clock chip" v={i.clockgen?.detected ? "Si5351 found" : "Not found"} d={i.clockgen?.detected ? "external clock" : "internal clock"} />
      </div>
    </Card>
  );
}

function openIssue(): void {
  const text = issueReport(device.info.value, device.log.value);
  void copyText(text).then((ok) => { if (ok) device.toast("Report copied: paste it in the issue", "info"); });
  openSheet({
    title: "Copy for an issue", icon: "content_copy",
    body: () => (
      <>
        <p>Copied to the clipboard; if not, select the text below.</p>
        <textarea class="report" readOnly aria-label="Report" value={text} rows={10} />
        <SheetButtons>
          <button type="button" class="btn ghost" onClick={() => download(text, "gbsc-issue-report.md", "text/markdown")}>Download</button>
          <button type="button" class="btn" onClick={closeSheet}>Close</button>
        </SheetButtons>
      </>
    ),
  });
}

export function StatusPage() {
  return <><ConsoleCard /><SignalCard /><DiagnosticsCard /></>;
}
