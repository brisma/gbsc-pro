// PC console drawer under the Decoder and Encoder sections: the last line when closed, the ADV log when open (reads,
// writes, skipped registers); the Manual section has the full console
import { signal } from "@preact/signals";
import { Icon } from "../../../art/icon.tsx";
import { advLog } from "./adv-io.ts";

const open = signal(false);

export function ConsoleLines({ max }: { max: number }) {
  return (
    <div class="log advlog" role="log" aria-label="ADV console">
      {advLog.value.slice(-max).map((l) => <div class={`l ${l.kind}`}>{l.text}</div>)}
    </div>
  );
}

export function ConsoleDrawer() {
  const last = advLog.value[advLog.value.length - 1];
  return (
    <div class={`adrawer${open.value ? " open" : ""}`}>
      <button type="button" class="adh" aria-expanded={open.value} onClick={() => { open.value = !open.value; }}>
        <Icon name="terminal" /><b>Console</b><small>{last ? last.text : "Nothing sent yet"}</small><Icon name={open.value ? "expand_more" : "expand_less"} />
      </button>
      {open.value ? <ConsoleLines max={60} /> : null}
    </div>
  );
}
