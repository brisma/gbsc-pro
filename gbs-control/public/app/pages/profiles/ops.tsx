// Profile operations bound to the device store: each waits for its status.profile_op, a failure goes to the page's
// warning box (Profiles) or a toast (elsewhere); loading asks Save / Discard over unsaved changes
import { signal } from "@preact/signals";
import { device, unsaved } from "../../store/app.ts";
import { runOp, activeProfileOf, profileLabel, opErrorText } from "../../store/profiles.ts";
import { openSheet, closeSheet, SheetButtons } from "../../shell/Sheet.tsx";
import { confirm } from "../../shell/Confirm.tsx";
import { route } from "../../shell/router.ts";
import type { ProfileEntry } from "../../store/device.ts";

export const profileError = signal("");   // the warning box at the top of Profiles

export const activeProfile = (): ProfileEntry | undefined => activeProfileOf(device.profiles.value);

export async function op(path: string, form?: Record<string, string> | FormData): Promise<string> {
  const r = await runOp({ post: (p, f) => device.client.post(p, f), profileOp: device.profileOp, reload: () => device.reloadProfiles() }, path, form);
  console.debug(`[profiles] ${path}: ${r}`);
  if (r !== "ok") {
    if (route.value.page === "profiles") profileError.value = opErrorText(r);
    else device.toast(opErrorText(r), "error");
  } else profileError.value = "";
  return r;
}

// Discard changes: the active profile loaded again as it was saved (the menu's Profiles > Discard changes)
export async function discardChanges(): Promise<void> {
  const act = activeProfile();
  if (!act || !unsaved.value) return;
  const label = profileLabel(act);
  if (await confirm({ icon: "history", title: `Discard the changes to ${label}?`, body: `${label} loads again as it was saved. What you changed since is lost.`, ok: "Discard changes" }))
    void op("/api/profiles/load", { id: act.id });
}

// Load a profile (or Defaults with id ""), asking Save / Discard when the active one has unsaved changes
export function loadProfile(id: string, name: string): void {
  const go = () => (id ? op("/api/profiles/load", { id }) : op("/api/profiles/defaults", {}));
  const act = activeProfile();
  if (!unsaved.value || !act) { void go(); return; }
  openSheet({
    title: "Unsaved changes", icon: "bookmark",
    body: () => (
      <>
        <p><b>{profileLabel(act)}</b> has changes that are not saved. Save them before loading <b>{name}</b>?</p>
        <SheetButtons>
          <button type="button" class="btn ghost" onClick={closeSheet}>Cancel</button>
          <button type="button" class="btn" onClick={() => { closeSheet(); void go(); }}>Discard</button>
          <button type="button" class="btn pri" onClick={async () => { closeSheet(); if (await op("/api/profiles/save", { id: act.id }) === "ok") void go(); }}>Save</button>
        </SheetButtons>
      </>
    ),
  });
}
