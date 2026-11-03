// Developer › ADV Controller › Manual and console: one register of a chip map with its eight bits, Read and Write;
// the console takes the ADV Controller CLI syntax (regs/cli.ts): writes go through the planner (0x0E and read-only
// refused with the reason, dangerous ones asked), reads and dumps through dev.adv_read one at a time.
import { signal } from "@preact/signals";
import { Icon } from "../../../art/icon.tsx";
import { device } from "../../../store/app.ts";
import { confirm } from "../../../shell/Confirm.tsx";
import { parseCli, CLI_HELP } from "../../../regs/cli.ts";
import { planAdvWrites, type AdvChange } from "../../../regs/planner.ts";
import type { Regmap } from "../../../regs/regmap.ts";
import { SysCols } from "../../system/parts.tsx";
import { useRegmap, DANGER_TEXT } from "./AdvRegs.tsx";
import { advRead, advWrite, advBusyUpdating, advCanRead, logLine, hx, AdvReadError } from "./adv-io.ts";
import { ConsoleLines } from "./ConsoleDrawer.tsx";

const MAPN: Record<string, string> = { user_sub_map: "Main", user_sub_map_2: "ACE", vpp_map: "VPP", main: "Main" };
const man = signal({ map: 0, reg: 0, value: null as number | null });
const cmd = signal("");
const busy = signal(false);

const where = (dev: number, map: number, reg: number) => (dev === 0x42 && map ? `${hx(dev)}/${hx(map)} ${hx(reg)}` : `${hx(dev)} ${hx(reg)}`);
const readErr = (e: unknown) => (e instanceof AdvReadError ? (e.state === "error" ? "no answer from the chip" : e.state) : String(e));

// One change through the planner: false when refused or cancelled
async function writeOne(rm: Regmap, c: AdvChange): Promise<boolean> {
  if (advBusyUpdating()) { logLine("err", "refused: the ADV Controller is updating"); return false; }
  const plan = planAdvWrites([c], rm);
  if (plan.skipped.length) { logLine("err", `refused: ${plan.skipped[0].reason}`); return false; }
  if (plan.dangerous.length && !(await confirm({ icon: "warning", title: `Write 0x${hx(c.reg)}?`, body: `This register can reset or power down the chip: ${DANGER_TEXT}.`, ok: "Write anyway" }))) {
    logLine("sys", "not written");
    return false;
  }
  for (const r of plan.requests) if (!(await advWrite(r))) return false;
  return true;
}

async function readOne(dev: number, map: number, reg: number): Promise<number | null> {
  try {
    const [v] = await advRead({ dev, map, reg, count: 1 });
    logLine("ok", `${where(dev, map, reg)} = 0x${hx(v)}`);
    return v;
  } catch (e) { logLine("err", `${where(dev, map, reg)}: ${readErr(e)}`); return null; }
}

async function run(rm: Regmap, line: string): Promise<void> {
  if (!line.trim()) return;
  logLine("cmd", `> ${line.trim()}`);
  const c = parseCli(line);
  switch (c.kind) {
    case "error": logLine("err", c.message); return;
    case "help": for (const h of CLI_HELP) logLine("sys", h); return;
    case "usb": logLine("sys", `${c.name} exists only on the ADV Controller's USB console`); return;
    case "info": {
      const a = device.info.value?.adv_controller;
      if (!a) { logLine("err", "no ADV Controller on this board"); return; }
      logLine("sys", `ADV Controller ${a.version ?? "?"} · ${a.status ?? "?"} · caps ${(a.caps ?? []).join(", ") || "none"}`);
      if (a.signal) logLine("sys", `signal ${JSON.stringify(a.signal)}`);
      return;
    }
    case "write": if (await writeOne(rm, c.change)) await readOne(c.change.i2c, c.change.map, c.change.reg); return;
    case "read": await readOne(c.dev, c.map, c.reg); return;
    case "dump": {
      const values: number[] = [];
      try {
        for (let r = c.reg; r < c.reg + c.count; r += 32) values.push(...(await advRead({ dev: c.dev, map: c.map, reg: r, count: Math.min(32, c.reg + c.count - r) })));
      } catch (e) { logLine("err", `dump: ${readErr(e)}`); return; }
      logLine("sys", `dump ${hx(c.dev)}${c.map ? ` map ${hx(c.map)}` : ""}, ${c.count} registers`);
      for (let i = 0; i < values.length; i += 16)
        logLine("dump", `${hx(c.reg + i)}: ${values.slice(i, i + 16).map(hx).join(" ")}`);
      return;
    }
  }
}

function ManualPanel({ rm }: { rm: Regmap }) {
  const maps = rm.chips.flatMap((c) => c.maps.map((m) => ({ c, m })));
  const M = man.value, { m } = maps[M.map], reg = m.regs.find((r) => r.addr === M.reg);
  const setReg = (g: number) => { man.value = { ...M, reg: (g + 256) % 256, value: null }; };
  const read = async () => { busy.value = true; const v = await readOne(m.i2c, m.map, M.reg); busy.value = false; man.value = { ...man.value, value: v }; };
  const write = async () => {
    if (M.value === null) return;
    busy.value = true;
    if (await writeOne(rm, { i2c: m.i2c, map: m.map, reg: M.reg, value: M.value })) man.value = { ...man.value, value: await readOne(m.i2c, m.map, M.reg) };
    busy.value = false;
  };
  return (
    <div class="sgrp manual">
      <div class="sec">Register</div>
      <div class="grp">
        <div class="mgrid mapsel" role="radiogroup" aria-label="Register map">
          {maps.map((x, i) => <button type="button" role="radio" aria-checked={M.map === i} class={M.map === i ? "on" : ""} onClick={() => { man.value = { ...M, map: i, value: null }; }}>{x.c.id} {MAPN[x.m.id] ?? x.m.id}</button>)}
        </div>
        <div class="rw noic"><span class="lb"><span class="mreg">0x{hx(M.reg)}{reg ? ` · ${reg.name}` : ""}<span class="sub">{reg ? (reg.readonly ? "read-only" : reg.danger === "warn" ? "resets or powers down the chip" : reg.desc) : "not in the map"}</span></span></span>
          <div class="c"><div class="stp">
            <button type="button" class="a" aria-label="Previous register" onClick={() => setReg(M.reg - 1)}><Icon name="remove" /></button>
            <b class="mono">{hx(M.reg)}</b>
            <button type="button" class="a" aria-label="Next register" onClick={() => setReg(M.reg + 1)}><Icon name="add" /></button>
          </div></div>
        </div>
        <div class="bits mbits">{[7, 6, 5, 4, 3, 2, 1, 0].map((b) => {
          const on = M.value !== null && ((M.value >> b) & 1) === 1;
          return <button type="button" role="switch" aria-checked={on} aria-label={`Bit ${b}`} class={on ? "on" : ""} disabled={M.value === null}
                         onClick={() => { man.value = { ...man.value, value: (man.value.value ?? 0) ^ (1 << b) }; }}>{on ? 1 : 0}<i>{b}</i></button>;
        })}</div>
        {reg && M.value !== null ? (
          <div class="mfields" aria-label="Fields of this register">
            {reg.fields.filter((f) => f.type !== "r" && f.type !== "u").map((f) => {   // reserved and unknown bits left out
              const x = (M.value! >> f.lsb) & ((1 << (f.msb - f.lsb + 1)) - 1);
              const text = f.values[String(x)];
              return (
                <div class="mf"><b class="mono">{f.name.replace(/\[.*\]$/, "")}</b><small>{f.msb === f.lsb ? `bit ${f.lsb}` : `bits ${f.msb}–${f.lsb}`}</small>
                  <span class="mono">{x}{text ? ` · ${text}` : ""}</span></div>
              );
            })}
          </div>
        ) : null}
        <div class="rw noic mrow">
          <span class="lb"><span>Value <b class="mono mval">{M.value === null ? "—" : `0x${hx(M.value)}`}</b></span></span>
          <span class="pact">
            <button type="button" class="btn ghost" disabled={busy.value} onClick={() => void read()}><Icon name="download" />Read</button>
            <button type="button" class="btn pri" disabled={busy.value || M.value === null || !!reg?.readonly} onClick={() => void write()}><Icon name="upload" />Write</button>
          </span>
        </div>
      </div>
    </div>
  );
}

function ConsolePanel({ rm }: { rm: Regmap }) {
  const send = () => { const l = cmd.value; cmd.value = ""; void run(rm, l); };
  return (
    <div class="sgrp">
      <div class="sec">Console</div>
      <ConsoleLines max={200} />
      <div class="acmd">
        <input class="fin mono" aria-label="Command" placeholder="w 42 0A 10 · help" autocomplete="off" spellcheck={false} value={cmd.value}
               onInput={(e) => { cmd.value = (e.currentTarget as HTMLInputElement).value; }} onKeyDown={(e) => { if (e.key === "Enter") send(); }} />
        <button type="button" class="btn pri" onClick={send}><Icon name="send" />Send</button>
      </div>
    </div>
  );
}

export function ManualSection({ desk }: { desk: boolean }) {
  const rm = useRegmap();
  if (!advCanRead()) return <div class="rawpad"><div class="grp"><div class="rw msg"><span class="ic"><Icon name="system_update" /></span><span class="lb"><span>Update the ADV Controller (System › Update) to read and change its registers here.</span></span></div></div></div>;
  if (rm === undefined) return <div class="rawpad"><p class="note">Loading the register map…</p></div>;
  if (!rm) return <div class="rawpad"><p class="note">This firmware was built without the register map.</p></div>;
  return <div class="advw advmanual"><SysCols desk={desk} left={<ManualPanel rm={rm} />} right={<ConsolePanel rm={rm} />} /></div>;
}
