// Profile helpers shared by Quick and Profiles (no JSX, no app store: unit-tested in Node)
import type { ReadonlySignal } from "@preact/signals";
import { effect } from "@preact/signals";
import type { ProfileEntry, ProfilesJson } from "./device.ts";

// What messages name (profileLabel): the name with its region once ("Mega Drive · PAL"); no region, the name
export function profileLabel(p: Pick<ProfileEntry, "name" | "region">): string {
  if (!p.region || p.region === "None" || p.name.endsWith(" " + p.region)) return p.name;
  return `${p.name} · ${p.region}`;
}

export const activeProfileOf = (pj: ProfilesJson | null): ProfileEntry | undefined => pj?.profiles.find((p) => p.id === pj.active);

export interface OpDeps {
  post: (path: string, form?: Record<string, string> | FormData) => Promise<Response>;
  profileOp: ReadonlySignal<{ seq: number; result: string }>;
  reload: () => Promise<void>;
  timeoutMs?: number;
}

// One profile operation: the POST (202 = queued), then the next status.profile_op (10 s), then the list again (a
// result from another client looks the same; the reload shows the truth). "ok", or the reason ("limit", "busy"...)
export async function runOp(d: OpDeps, path: string, form?: Record<string, string> | FormData): Promise<string> {
  const startSeq = d.profileOp.value.seq;
  const r = await d.post(path, form);
  if (r.status !== 202) {
    let why = `HTTP ${r.status}`;
    try {
      const j = await r.json();
      if (j.errors) why = Object.entries(j.errors).map(([k, v]) => `${k} ${v}`).join(", ");
      else if (j.error) why = String(j.error);
    } catch { /* no JSON */ }
    return r.ok ? "ok" : why;
  }
  const result = await new Promise<string>((resolve) => {
    let dispose = () => {};
    const timer = setTimeout(() => { dispose(); resolve("timeout"); }, d.timeoutMs ?? 10000);
    dispose = effect(() => {
      const op = d.profileOp.value;
      if (op.seq !== startSeq) { clearTimeout(timer); queueMicrotask(() => dispose()); resolve(op.result); }
    });
  });
  await d.reload();
  return result === "ok" ? "ok" : result.replace(/^error:/, "");
}

// The page-level warning for a failed operation (profile-store.cpp reasons)
export function opErrorText(reason: string): string {
  switch (reason) {
    case "limit": return "Profiles full: 36 of 36. Delete a profile or replace one instead.";
    case "storage_full": return "The device storage is full. Delete a profile to make room.";
    case "busy": return "The device is busy: try again.";
    case "timeout": return "No answer from the device. Check the list below.";
    case "unknown": return "That profile is gone (another client changed the list).";
    case "invalid": return "Not a GBSC PRO profile file.";
    default: return `The operation failed (${reason}).`;
  }
}
