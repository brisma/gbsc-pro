// A numbered step of a procedure: done steps fold into one summary line with Change, the current one is open, later
// ones wait.
import type { ComponentChildren } from "preact";
import { Icon } from "../../../gbs-control/public/app/art/icon.tsx";

export type StepState = "done" | "now" | "wait";

export function Step({ n, title, state, summary, onChange, children }:
  { n: number | "end"; title: string; state: StepState; summary?: string; onChange?: () => void; children?: ComponentChildren }) {
  return (
    <div class={`f-step ${state}`}>
      <span class="f-num">{state === "done" ? <Icon name="check" /> : n === "end" ? <Icon name="flag" /> : n}</span>
      <div class="f-card">
        <div class="f-sh"><b>{title}</b>{state === "done" && onChange && <button class="f-lnk" onClick={onChange}>Change</button>}</div>
        {state === "done" && summary && <div class="f-sum">{summary}</div>}
        {state === "now" && children}
        {state === "wait" && summary && <div class="f-sum">{summary}</div>}
      </div>
    </div>
  );
}
