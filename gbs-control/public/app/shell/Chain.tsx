// The signal chain on top of every page, one card: its title line (Signal, the profile in use) and Input -> Scaler ->
// Picture -> Output on one cable. Each stage opens its Settings section; the stage of the open section is ringed.
// With unsaved changes the notice under the header names the profile, so the title line leaves it out.
import { Icon } from "../art/icon.tsx";
import { connectorUrl } from "../art/connectors.tsx";
import { device, mode, pred, unsaved } from "../store/app.ts";
import { route, hashOf } from "./router.ts";
import { choiceValue, choiceOptions } from "../model/choices.ts";
import { scanlineValue } from "../model/scanlines.ts";
import { menu } from "../model/menu.ts";
import { chain, STAGE_OF_SECTION, type Chain as ChainModel, type StageId } from "../model/chain.ts";
import { profileLabel } from "../store/profiles.ts";
import { activeProfile } from "../pages/profiles/ops.tsx";
import { rowOptions } from "../rows/controls.tsx";

const STAGE_ICON: Record<StageId, string> = { input: "cable", scaler: "memory", picture: "auto_awesome", output: "tv" };   // @icons

// The chain of now, from the device state (also behind the state chip of the header)
export function chainNow(): { model: ChainModel; cable: number } {
  const info = device.info.value, v = device.values.value, c = device.connection.value;
  const cable = Number(info?.input?.active ?? 0) || Number(v["input.source"] ?? 0);
  const filterIdx = choiceValue("filter", v);
  const resIdx = choiceValue("resolution", v, { basePreset: device.profiles.value?.base });
  const model = chain({
    offline: c === "reconnecting" || c === "offline",
    searching: info?.input?.state === "searching",
    signal: info?.signal,
    cable: rowOptions(undefined, "input.source").find((o) => o.v === cable)?.n ?? "Auto",
    resolution: choiceOptions("resolution", { mode: mode.value, caps: pred.value.caps, current: resIdx }).find((o) => o.index === resIdx)?.label ?? "Custom",
    filter: filterIdx >= 0 ? menu.choices.filter[filterIdx].label : "Custom",
    scanlines: scanlineValue(mode.value, v),
    frameLock: Number(v["output.frame_lock"] ?? 0) !== 0,
    ypbpr: Number(v["output.component"] ?? 0) !== 0,
  });
  return { model, cable };
}

export function Chain() {
  const { model, cable } = chainNow();
  const r = route.value;
  const here = r.page === "settings" ? STAGE_OF_SECTION[r.section ?? ""] : undefined;
  const prof = activeProfile();
  const uns = unsaved.value;
  return (
    <section class="chainblk" aria-label="Signal">
      <div class="chtitle">
        <h2>Signal</h2>
        {uns ? null : (
          <a class="pf" href={hashOf({ page: "profiles", section: null, id: null })} aria-label={`Profile: ${prof ? profileLabel(prof) : "Defaults"}`}>
            <Icon name="bookmark" />{prof ? profileLabel(prof).split(" · ")[0] : "Defaults"}
          </a>
        )}
      </div>
      <div class={`chain st-${model.state}`}>
        {model.stages.flatMap((s, i) => [
          i ? <i class={`cb ${model.cables[i - 1]}`} aria-hidden="true" /> : null,
          <a class={`st${s.dim ? " dim" : ""}${here === s.id ? " here" : ""}`} href={hashOf({ page: "settings", section: s.id, id: null })}
             aria-current={here === s.id ? "true" : undefined} aria-label={`${s.name}: ${s.value}, ${s.detail}`}>
            <span class="ib">
              {s.id === "input" && cable ? <img alt="" src={connectorUrl(cable)} /> : <Icon name={STAGE_ICON[s.id]} />}
              {s.tone === "bad" ? <i class="red" /> : null}
            </span>
            <span class="k">{s.name}</span><b class="v">{s.value}</b><small class={`d${s.tone ? " " + s.tone : ""}`}>{s.detail}</small>
          </a>,
        ])}
      </div>
    </section>
  );
}
