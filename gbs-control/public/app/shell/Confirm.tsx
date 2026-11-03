// Confirmation sheet: confirm({...}) resolves true on the OK button, false on Cancel, Escape or the shade
import { openSheet, closeSheet, SheetButtons } from "./Sheet.tsx";

export interface ConfirmSpec { icon: string; title: string; body: string; ok: string }

export function confirm(c: ConfirmSpec): Promise<boolean> {
  return new Promise((resolve) => {
    let done = false;
    const finish = (v: boolean) => { if (done) return; done = true; resolve(v); };
    openSheet({
      title: c.title, icon: c.icon, onClose: () => finish(false),
      body: () => (
        <>
          <p>{c.body}</p>
          <SheetButtons>
            <button type="button" class="btn ghost" onClick={() => closeSheet()}>Cancel</button>
            <button type="button" class="btn pri" onClick={() => { finish(true); closeSheet(); }}>{c.ok}</button>
          </SheetButtons>
        </>
      ),
    });
  });
}
