// Easy · Advanced · Dev: the browser mode; Dev = Advanced + system.devmode (never system.menu_mode)
import { mode, devOn, devmode, device } from "../store/app.ts";
import { route, navigate } from "./router.ts";

type Seg = "easy" | "advanced" | "dev";

function choose(seg: Seg): void {
  const was = devOn.value;
  if (seg === "dev") {
    mode.value = "advanced";
    if (!devmode.value) void device.writeSettings([["system.devmode", 1]]);
    return;
  }
  mode.value = seg;
  if (devmode.value) void device.writeSettings([["system.devmode", 0]]);
  if (was && route.value.page === "dev") navigate("system");
}

export function ModeSwitch({ long }: { long: boolean }) {
  const on: Seg = devOn.value ? "dev" : mode.value;
  const segs: [Seg, string, string][] = [["easy", "Easy", ""], ["advanced", long ? "Advanced" : "Adv", ""],
    ["dev", "Dev", "Advanced + Developer menu (web and TV)"]];
  const onKey = (e: KeyboardEvent) => {
    const i = segs.findIndex(([k]) => k === on);
    const d = e.key === "ArrowRight" || e.key === "ArrowDown" ? 1 : e.key === "ArrowLeft" || e.key === "ArrowUp" ? -1 : 0;
    if (!d) return;
    e.preventDefault();
    const next = segs[(i + d + segs.length) % segs.length][0];
    choose(next);
    window.setTimeout(() => (document.querySelector(`.mode [data-seg="${next}"]`) as HTMLElement | null)?.focus(), 0);
  };
  return (
    <span class="mode" role="radiogroup" aria-label="Mode" onKeyDown={onKey}>
      {segs.map(([k, n, t]) => (
        <button type="button" role="radio" data-seg={k} aria-checked={on === k} tabIndex={on === k ? 0 : -1} class={on === k ? "on" : ""}
                title={t || undefined} onClick={() => choose(k)}>{n}</button>
      ))}
    </span>
  );
}
