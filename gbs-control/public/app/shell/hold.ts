// Hold to repeat (remote arrows, steppers, pads): the press acts at once, again after 380 ms, then every 300 ms
// (the device queue takes ~3 keys a second comfortably); Enter/Space act once per key event
import { useRef, useEffect } from "preact/hooks";

export const HOLD_FIRST_MS = 380, HOLD_EVERY_MS = 300;

export function useHold(fn: () => void) {
  const t = useRef<{ first: number | null; every: number | null }>({ first: null, every: null });
  const latest = useRef(fn);
  latest.current = fn;
  const stop = () => {
    if (t.current.first !== null) window.clearTimeout(t.current.first);
    if (t.current.every !== null) window.clearInterval(t.current.every);
    t.current = { first: null, every: null };
  };
  useEffect(() => stop, []);
  return {
    onPointerDown: (e: PointerEvent) => {
      if (e.button !== 0) return;
      stop();
      latest.current();
      t.current.first = window.setTimeout(() => { t.current.every = window.setInterval(() => latest.current(), HOLD_EVERY_MS); }, HOLD_FIRST_MS);
    },
    onPointerUp: stop, onPointerLeave: stop, onPointerCancel: stop,
    onContextMenu: (e: Event) => e.preventDefault(),
    onKeyDown: (e: KeyboardEvent) => { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); latest.current(); } },
  };
}
