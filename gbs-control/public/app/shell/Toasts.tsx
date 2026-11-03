// The latest toast of the store (one at a time); it never takes a status role (the strip has it)
import { device } from "../store/app.ts";
import { Icon } from "../art/icon.tsx";

export function Toasts() {
  const t = device.toasts.value.at(-1);
  return (
    <div aria-live="polite" class="toasts">
      {t ? <div class={`toast${t.kind === "error" ? " err" : ""}`} key={t.id}><Icon name={t.kind === "error" ? "warning" : "check_circle"} />{t.text}</div> : null}
    </div>
  );
}
