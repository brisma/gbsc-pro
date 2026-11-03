// After a firmware install or a restore the device restarts: when it is back, the page reloads, so it runs the new
// firmware's own page and data instead of the old bundle (also ends a same-version install)
import { effect } from "@preact/signals";
import { device } from "../../store/app.ts";

let finished = false;   // status.update=done or status.restore=ok seen: a restart follows
let away = false;       // the connection dropped after that

effect(() => {
  const u = device.update.value;
  if (u.progress === "done" || u.restore === "ok") finished = true;
});
effect(() => {
  const c = device.connection.value;
  if (!finished) return;
  if (c !== "ok") { away = true; return; }
  if (away) {
    console.debug("[update] the device is back after the restart: reloading the page");
    location.reload();
  }
});
