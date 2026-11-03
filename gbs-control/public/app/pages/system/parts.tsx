// Pieces of the System sections that draw themselves (WiFi, Update, Backup): a group of rows with its band, a
// progress bar kept in place while the value moves, a message row
import type { ComponentChildren } from "preact";
import { Icon } from "../../art/icon.tsx";
import { GroupBand } from "../../sections/Card.tsx";

export function SysGroup({ title, action, children }: { title?: string; action?: ComponentChildren; children: ComponentChildren }) {
  return <>{title ? <GroupBand title={title} action={action} /> : null}{children}</>;
}

// One column on PC and phone: the left part, then the right part (the Developer tools)
export function SysCols({ left, right }: { desk: boolean; left: ComponentChildren; right: ComponentChildren; spaceLeft?: boolean }) {
  return <>{left}{right}</>;
}

// The same element for every value (no re-mount): only the width and aria-valuenow change; the percentage beside it
export function Progress({ pct, label }: { pct: number; label: string }) {
  const v = Math.max(0, Math.min(100, Math.round(pct)));
  return (
    <div class="pbar">
      <div class="prog" role="progressbar" aria-label={label} aria-valuemin={0} aria-valuemax={100} aria-valuenow={v}>
        <i style={{ width: `${v}%` }} />
      </div>
      <b>{v}%</b>
    </div>
  );
}

// An outcome (tone) is announced; a plain line (no tone) is not a live region
export function MsgRow({ icon, text, tone }: { icon: string; text: string; tone?: "bad" | "good" }) {
  return (
    <div class={`rw note msg${tone ? " " + tone : ""}`} role={tone === "bad" ? "alert" : tone === "good" ? "status" : undefined}>
      <span class="ic"><Icon name={icon} /></span>
      <span class="ds">{text}</span>
    </div>
  );
}
