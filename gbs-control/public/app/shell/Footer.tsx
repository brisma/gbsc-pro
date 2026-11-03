// Page footer: what runs on the device (firmware versions, the ADV Controller only where there is one, the address)
// and where the project lives. PC: one line on the bottom edge. Phone: at the end of the page, centred.
import { Icon } from "../art/icon.tsx";
import { device, board, offer } from "../store/app.ts";
import { route, hashOf } from "./router.ts";

export const REPO = "https://github.com/brisma/gbsc-pro";

export function Footer() {
  const info = device.info.value;
  const host = device.wifi.value?.hostname ?? "";
  const update = hashOf({ page: "system", section: "update", id: null });
  const onUpdate = route.value.page === "system" && route.value.section === "update";
  const adv = board.value === "pro" ? info?.adv_controller?.version : undefined;
  return (
    <footer class="foot">
      <div class="fvers">
        {info?.fw ? <a class="fi" href={update}><span>GBS-Control</span><b>{info.fw}</b>{offer.value && !onUpdate ? <span class="tag">{offer.value} available</span> : null}</a> : null}
        {adv ? <a class="fi" href={update}><span>ADV Controller</span><b>{adv}</b></a> : null}
        {host ? <a class="fi" href={hashOf({ page: "system", section: "wifi", id: null })}><span>Address</span><b>{host}</b></a> : null}
      </div>
      <nav class="flk" aria-label="Project">
        <a href={REPO} target="_blank" rel="noopener"><Icon name="code" />GitHub</a>
        <a href={`${REPO}/issues`} target="_blank" rel="noopener"><Icon name="bug_report" />Report a problem</a>
        <a href={`${REPO}/releases`} target="_blank" rel="noopener"><Icon name="new_releases" />Release notes</a>
      </nav>
    </footer>
  );
}
