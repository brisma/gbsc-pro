// Row controls: toggle, stepper, choice (segments, or a list when they would not fit), action, live value, link, note
import { useState, useEffect } from "preact/hooks";
import { Icon } from "../art/icon.tsx";
import { device, mode, pred, desk } from "../store/app.ts";
import { optionsFor, registry, menu, type MenuNode } from "../model/menu.ts";
import { formatSetting, formatLive, onOff } from "../model/format.ts";
import { confirm } from "../shell/Confirm.tsx";
import { openSheet, closeSheet, SheetButtons } from "../shell/Sheet.tsx";
import { navigate } from "../shell/router.ts";
import { useHold } from "../shell/hold.ts";
import { T } from "../texts/ui.ts";
import type { Row } from "../layout/types.ts";
import { Frame } from "./Frame.tsx";
import { useAcc } from "./accordion.ts";
import { RowView } from "./Row.tsx";
import { rowNode, rowSetting, rowAction, actionInfo, splitRef } from "./resolve.ts";

export interface RowProps { row: Row; label: string; reason: string | null; head?: boolean }

// Menu functions the web runs as device actions (pro/api/api-actions-table.h)
export const FN_ACTIONS: Record<string, string> = {
  picture_reset: "picture.reset", geometry_reset: "screen.reset", decoder_reset: "adv.decoder_reset", termination: "input.termination",
};

const num = (v: unknown) => Number(v ?? 0);
const write = async (key: string, v: number) => { await device.writeSettings([[key, v]]); };

// The device's refusal codes in the TV's words where it has them
const REFUSED: Record<string, () => string> = {
  not_rgb_scart: () => menu.reasons.rgbs, busy: () => "The device is busy: try again", in_use: () => "Not now: the device is using it",
};

export async function runAction(name: string, data?: string): Promise<boolean> {
  const r = await device.action(name, data);
  if (!r.ok) {
    let code = "";
    try { code = String((await r.json()).error ?? ""); } catch { /* no JSON */ }
    device.toast(REFUSED[code]?.() ?? `Not done (${code || "HTTP " + r.status})`, "error");
  }
  return r.ok;
}

// Keeps /api/live polling while the row is on screen
export function useLive(lv: string | undefined): unknown {
  useEffect(() => (lv ? device.wantLive() : undefined), [lv]);
  return lv ? device.live.value[lv] : undefined;
}

export function ToggleRow({ row, label, reason }: RowProps) {
  const node = rowNode(row);
  const key = rowSetting(row);
  const fn = node?.type === "toggle_fn" ? node.fn : undefined;
  // a device toggle action (Developer): its /api/live readback is the state, the action flips it
  const act = node?.type === "action" && node.live ? node.action : undefined;
  const lv = useLive(act ? node!.live : undefined);
  const on = act ? Number(lv ?? 0) > 0 : fn === "termination" ? !!device.info.value?.input?.termination : num(device.values.value[key ?? ""]) !== 0;
  const flip = async () => {
    if (reason) return;
    if (act) { await runAction(act); return; }
    if (fn && FN_ACTIONS[fn]) { if (await runAction(FN_ACTIONS[fn], on ? "0" : "1")) void device.client.info(0).then((i) => { device.info.value = i; }); return; }
    if (key) await write(key, on ? 0 : 1);
  };
  return (
    <Frame row={row} label={label} reason={reason}>
      <button type="button" role="switch" aria-checked={on} aria-label={label} class={`tg${on ? " on" : ""}`} disabled={!!reason} onClick={() => void flip()} />
    </Frame>
  );
}

function StepButton({ dir, label, onStep, disabled }: { dir: -1 | 1; label: string; onStep: () => void; disabled: boolean }) {
  const hold = useHold(onStep);
  return (
    <button type="button" class="a" aria-label={`${dir < 0 ? "Decrease" : "Increase"} ${label}`} disabled={disabled} {...hold}>
      <Icon name={dir < 0 ? "remove" : "add"} />
    </button>
  );
}

export function StepperRow(p: RowProps) {
  return rowNode(p.row)?.type === "stepper" ? <ActionStepper {...p} /> : <SettingStepper {...p} />;
}

// A menu STEPPER: two actions and maybe a live readback; without one only − and + show
function ActionStepper({ row, label, reason }: RowProps) {
  const node = rowNode(row)!;
  const v = useLive(node.live);
  return (
    <Frame row={row} label={label} reason={reason}>
      <div class={`stp${node.live ? "" : " nolive"}`}>
        <StepButton dir={-1} label={label} disabled={!!reason} onStep={() => void runAction(node.action!)} />
        {node.live ? <b aria-live="polite">{formatLive(node.live, v)}</b> : null}
        <StepButton dir={1} label={label} disabled={!!reason} onStep={() => void runAction(node.action_inc!)} />
      </div>
    </Frame>
  );
}

function SettingStepper({ row, label, reason }: RowProps) {
  const node = rowNode(row);
  const [menuOpen, setMenu] = useState(false);
  const key = rowSetting(row) ?? "";
  const e = registry.entry(key);
  const step = node?.step ?? 1;
  const cur = num(device.values.value[key]);
  const set = (v: number) => { const c = Math.max(e?.min ?? 0, Math.min(e?.max ?? 255, v)); if (c !== num(device.values.value[key])) void write(key, c); };
  const shown = node ? formatSetting(node, cur) : String(cur);
  const def = e?.def ?? 0;
  return (
    <Frame row={row} label={label} reason={reason}>
      <div class="stp" style="position:relative">
        <StepButton dir={-1} label={label} disabled={!!reason} onStep={() => set(num(device.values.value[key]) - step)} />
        <button type="button" class="stv" aria-label={`${label} ${shown}`} aria-haspopup="menu" aria-expanded={menuOpen} onClick={() => setMenu(!menuOpen)}><b>{shown}</b></button>
        <StepButton dir={1} label={label} disabled={!!reason} onStep={() => set(num(device.values.value[key]) + step)} />
        {menuOpen ? (
          <>
            <div class="scrim" onClick={() => setMenu(false)} />
            <div class="menu" role="menu">
              <button type="button" role="menuitem" onClick={() => { setMenu(false); set(def); }}>
                <span>{T.resetTo(node ? formatSetting(node, def) : String(def))}</span><Icon name="settings_backup_restore" />
              </button>
            </div>
          </>
        ) : null}
      </div>
    </Frame>
  );
}

// Options of a setting row as menuStepFrom offers them in this mode
export function rowOptions(node: MenuNode | undefined, key: string): { n: string; v: number }[] {
  const s = pred.value;
  const fake = node ?? ({ id: key, type: "setting", label: key, help: "", flags: [], pred: "none", setting: key } as MenuNode);
  return optionsFor(fake, { mode: mode.value, caps: s.caps, current: num(device.values.value[key]), profileActive: s.profileActive });
}

// Segments: every option in sight, one click each. Too many or too long: the row keeps a list.
export interface SegOpt { label: string; on: boolean; pick: () => void }
export const segFits = (labels: string[]) => labels.length <= 7 && labels.join("").length <= 44;
// On the phone a wide set of segments takes a line of its own
export const segWide = (labels: string[]) => labels.length > 2 || labels.join("").length > 10;

export function Seg({ label, opts, disabled }: { label: string; opts: SegOpt[]; disabled?: boolean }) {
  return (
    <div class={`seg n${opts.length}`} role="radiogroup" aria-label={label}>
      {opts.map((o) => <button type="button" role="radio" aria-checked={o.on} class={o.on ? "on" : ""} disabled={disabled} onClick={o.pick}>{o.label}</button>)}
    </div>
  );
}

export function ChoiceRow({ row, label, reason }: RowProps) {
  const node = rowNode(row);
  const key = rowSetting(row) ?? "";
  const [open, setOpen] = useState(false);
  const opts = rowOptions(node, key);
  const cur = num(device.values.value[key]);
  const names = opts.map((o) => o.n);
  if (row.look === "grid9" && opts.length === 9)   // nine places on the screen: the name and a 3×3 grid
    return (
      <Frame row={row} label={label} reason={reason} wide cls="g9w">
        <span class="g9">
          <span class="val">{opts.find((o) => o.v === cur)?.n ?? ""}</span>
          <span class="grid" role="radiogroup" aria-label={label}>
            {opts.map((o) => <button type="button" role="radio" aria-checked={o.v === cur} aria-label={o.n} title={o.n} class={o.v === cur ? "on" : ""} disabled={!!reason}
                                     onClick={() => { if (o.v !== cur) void write(key, o.v); }} />)}
          </span>
        </span>
      </Frame>
    );
  if (segFits(names))
    return (
      <Frame row={row} label={label} reason={reason} wide={segWide(names)}>
        <Seg label={label} disabled={!!reason} opts={opts.map((o) => ({ label: o.n, on: o.v === cur, pick: () => { if (o.v !== cur) void write(key, o.v); } }))} />
      </Frame>
    );
  const curLabel = node ? formatSetting(node, cur) : String(cur);
  return (
    <Frame row={row} label={label} reason={reason}>
      <span style="position:relative;display:inline-flex">
        <button type="button" class="mv" aria-haspopup="listbox" aria-expanded={open} aria-label={`${label}: ${curLabel}`} disabled={!!reason} onClick={() => setOpen(!open)}>
          {curLabel} <Icon name="expand_more" />
        </button>
        {open ? (
          <>
            <div class="scrim" onClick={() => setOpen(false)} />
            <div class="menu" role="listbox" aria-label={label}>
              {opts.map((o) => (
                <button type="button" role="option" aria-selected={o.v === cur} class={o.v === cur ? "on" : ""}
                        onClick={() => { setOpen(false); if (o.v !== cur) void write(key, o.v); }}>
                  <span>{o.n}</span>{o.v === cur ? <Icon name="check" /> : null}
                </button>
              ))}
            </div>
          </>
        ) : null}
      </span>
    </Frame>
  );
}

const confirmText = (label: string): { title: string; body: string; ok: string; icon?: string } =>
  ({ title: `${label}?`, body: "The values go back to their defaults.", ok: label.startsWith("Reset") ? T.reset : T.run });

export function ActionRow({ row, label, reason, head }: RowProps) {
  const node = rowNode(row);
  const name = (node?.fn ? FN_ACTIONS[node.fn] : undefined) ?? rowAction(row);
  const info = actionInfo(name);
  const asks = !!row.confirm || node?.flags.includes("confirm") || info?.flags.includes("confirm");
  const live = useLive(node?.live);
  const button = row.button ?? (label.startsWith("Reset") ? T.reset : T.run);
  const go = async () => {
    if (!name || reason) return;
    if (asks) {
      const c = row.confirm ?? confirmText(label);
      if (!(await confirm({ icon: c.icon ?? row.icon, title: c.title, body: c.body, ok: c.ok }))) return;
    }
    await runAction(name);
  };
  if (head)
    return <button type="button" class="btn" data-row={`action:${row.ref ?? label}`} disabled={!!reason || !name} onClick={() => void go()}><Icon name={row.icon} />{label}</button>;
  return (
    <Frame row={row} label={label} reason={reason} sub={node?.live ? formatLive(node.live, live) : row.sub}>
      <button type="button" class={`btn${row.danger ? " danger" : ""}`} disabled={!!reason || !name} onClick={() => void go()}>{row.bicon ? <Icon name={row.bicon} /> : null}{button}</button>
    </Frame>
  );
}

export function LiveRow({ row, label, reason }: RowProps) {
  const [kind, id] = row.ref ? splitRef(row.ref) : ["", ""];
  const lv = useLive(kind === "live" ? id : undefined);
  const v = kind === "live" ? formatLive(id, lv) : kind === "text" ? textValue(id) : "";
  return <Frame row={row} label={label} reason={reason}><span class="val">{v}</span></Frame>;
}

// text:<source>: fields of /api/wifi and /api/info
export function textValue(src: string): string {
  const w = device.wifi.value, i = device.info.value;
  switch (src) {
    case "wifi.mode": return w ? (w.mode === "AP" ? "Hotspot" : "Your WiFi") : "";
    case "wifi.ssid": return w?.ssid ?? "";
    case "wifi.ip": return w?.ip ?? "";
    case "wifi.hostname": return w?.hostname ?? "";
    case "wifi.ap_password": return w?.ap_password ?? "";
    case "wifi.extra_hosts": return w?.extra_hosts ?? "";
    case "info.fw": return i?.fw ?? "";
    case "info.adv_controller": return i?.adv_controller?.version ?? (i?.adv_controller?.status ?? "");
    case "info.update.channel": return i?.update?.channel ?? "";
    case "info.update.last": return i?.update?.last ?? "";
    case "info.update.offer": return i?.update?.offer?.version ?? "";
    case "info.update.manual": return i?.update?.manual?.version ?? "";
    case "profile.active": return device.profiles.value?.profiles.find((p) => p.id === device.profiles.value?.active)?.name ?? "";
    default: return "";
  }
}

// A link: to a route, or (a menu sub without a route) an accordion with the sub's own rows under it (Auto search,
// Adaptive contrast)
export function LinkRow({ row, label, reason }: RowProps) {
  const node = rowNode(row);
  const [open, flip] = useAcc(`link:${row.ref ?? label}`);
  const inline = !row.to && node?.type === "sub" && !!node.children?.length;
  const go = () => {
    if (reason) return;
    if (inline) { flip(); return; }
    if (row.to) { const [page, sec] = row.to.replace(/^#\//, "").split("/"); navigate(page as never, sec ?? null); }
  };
  const kids = inline ? (node!.children ?? []).filter((k) => k.type !== "header") : [];
  const sub = row.sub ?? (inline ? subSummary(node!.id, kids) : undefined);
  return (
    <Frame row={row} label={label} reason={reason} lnk onRow={go} open={open} sub={inline ? undefined : sub}
           panel={open ? <div class="inl">{kids.map((k) => { const r = rowForChild(k); return r ? <RowView key={k.id} row={r} /> : null; })}</div> : null}>
      <button type="button" class="val" aria-label={inline ? `${label}: ${sub ?? ""}` : label} aria-expanded={inline ? open : undefined}
              onClick={(e) => { e.stopPropagation(); go(); }}>
        {inline ? sub : null} <Icon name={inline ? (open ? "expand_less" : "expand_more") : "chevron_right"} />
      </button>
    </Frame>
  );
}

// What the switches of a sub count, said when all of them are on ("6 inputs" instead of "6 of 6")
const SUB_NOUNS: Record<string, string> = { "input/auto-search": "inputs" };

// "On"/"Off" of a sub's Enabled switch, else how many of its switches are on ("5 of 6")
function subSummary(id: string, kids: MenuNode[]): string {
  const v = device.values.value;
  const bools = kids.filter((k) => k.type === "setting" && k.setting && registry.entry(k.setting)?.type === "bool");
  const en = bools.find((k) => /^enabled$/i.test(k.label));
  if (en) return num(v[en.setting!]) ? "On" : "Off";
  if (!bools.length) return "";
  const on = bools.filter((k) => num(v[k.setting!]) !== 0).length;
  return on === bools.length && SUB_NOUNS[id] ? `${on} ${SUB_NOUNS[id]}` : `${on} of ${bools.length}`;
}

// The row a sub's child draws inside an accordion
function rowForChild(k: MenuNode): Row | null {
  const ref = `menu:${k.id}` as const;
  if (k.type === "setting" && k.setting) {
    const e = registry.entry(k.setting);
    if (e?.type === "bool") return { kind: "toggle", ref, icon: "toggle_on" };
    if (e?.type === "enum" || k.options?.length) return { kind: "inline", ref, icon: "tune" };
    return { kind: "stepper", ref, icon: "tune" };
  }
  if (k.type === "action") return { kind: "action", ref, icon: "settings_backup_restore" };
  return null;
}

// A write-only string setting (OTA password): never shown, "Set" or "Not set" from the device's mask, a sheet to
// change it
export function SecretRow({ row, label, reason }: RowProps) {
  const key = row.secret ?? "";
  const set = String(device.values.value[key] ?? "") !== "";
  const max = registry.entry(key)?.maxLen ?? 32;
  return (
    <Frame row={row} label={label} reason={reason}>
      <span class="val">{set ? "Set" : "Not set"}</span>
      <button type="button" class="btn" aria-label={`Change ${label}`} disabled={!!reason}
              onClick={(e) => openSheet({ title: label, icon: row.icon, body: () => <SecretForm k={key} max={max} label={label} /> }, e.currentTarget)}>Change</button>
    </Frame>
  );
}

function SecretForm({ k, max, label }: { k: string; max: number; label: string }) {
  const [v, setV] = useState("");
  const [busy, setBusy] = useState(false);
  const save = async () => {
    setBusy(true);
    const r = await device.writeSettings([[k, v]]);
    setBusy(false);
    console.debug(`[secret] ${k}: ${r.ok ? "saved" : r.error}`);
    if (r.ok) closeSheet();
  };
  return (
    <>
      <p>The device keeps it and never sends it back: this page cannot show it.</p>
      <div class="field">
        <label for="secret-new">New password</label>
        <input id="secret-new" class="in" type="password" autocomplete="new-password" maxLength={max} value={v} onInput={(e) => setV((e.target as HTMLInputElement).value)} />
        <small class="hint">Up to {max} characters</small>
      </div>
      <SheetButtons>
        <button type="button" class="btn ghost" onClick={closeSheet}>{T.cancel}</button>
        <button type="button" class="btn pri" disabled={!v || busy} onClick={() => void save()}>Save</button>
      </SheetButtons>
    </>
  );
}

export function NoteRow({ row }: { row: Row }) {
  return <div class="rw note"><span class="ic"><Icon name={row.icon || "info"} /></span><span class="ds">{row.text}</span></div>;
}

export { onOff, menu, desk };
