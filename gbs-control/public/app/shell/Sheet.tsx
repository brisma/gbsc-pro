// Bottom sheet on the phone, centred dialog on PC: one at a time, Escape or the shade closes it,
// focus moves in and goes back to where it was
import { signal } from "@preact/signals";
import { useEffect, useRef } from "preact/hooks";
import type { ComponentChildren, VNode } from "preact";
import { Icon } from "../art/icon.tsx";

// sub: one line under the title (what the sheet is about)
export interface SheetSpec { title: string; icon: string; sub?: string; body: () => VNode; onClose?: () => void }
export const sheet = signal<SheetSpec | null>(null);
let returnFocus: HTMLElement | null = null;

// from: the control that opened it (WebKit does not focus a clicked button, so activeElement is not enough)
export function openSheet(s: SheetSpec, from?: EventTarget | null): void {
  returnFocus = (from instanceof HTMLElement ? from : (document.activeElement as HTMLElement)) ?? null;
  sheet.value = s;
}
export function closeSheet(): void {
  const s = sheet.value;
  sheet.value = null;
  s?.onClose?.();
  const back = returnFocus;
  returnFocus = null;
  if (back && document.contains(back)) window.setTimeout(() => back.focus(), 0);
}

// One listener for the whole app: Escape closes the open sheet even before its first effect ran
if (typeof window !== "undefined")
  window.addEventListener("keydown", (e) => { if (e.key === "Escape" && sheet.value) { e.preventDefault(); closeSheet(); } });

export function SheetHost() {
  const s = sheet.value;
  const box = useRef<HTMLDivElement>(null);
  useEffect(() => {
    if (!s) return;
    const first = box.current?.querySelector<HTMLElement>("button, a[href], input, select, textarea, [tabindex='0']");
    first?.focus();
  }, [s]);
  if (!s) return null;
  return (
    <>
      <div class="shade" onClick={closeSheet} />
      <div class="sheet" role="dialog" aria-modal="true" aria-labelledby="sheet-title" ref={box}>
        <div class="grab" />
        <h3 id="sheet-title"><Icon name={s.icon} />{s.title}</h3>
        {s.sub ? <p class="shsub">{s.sub}</p> : null}
        {s.body()}
      </div>
    </>
  );
}

export function SheetButtons({ children }: { children: ComponentChildren }) {
  return <div class="row2">{children}</div>;
}
