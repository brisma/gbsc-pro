// The System sections that draw themselves, attached to the page description at start (layout/pages.ts stays data
// only, readable by the Node unit tests)
import type { ComponentType } from "preact";
import { pageById } from "../../layout/pages.ts";
import { WifiSection } from "./WifiSection.tsx";
import { UpdateSection, UpdateActions } from "./UpdateSection.tsx";
import { BackupSection } from "./BackupSection.tsx";
import "./after-restart.ts";

const RENDERS: Record<string, ComponentType<{ desk: boolean }>> = { wifi: WifiSection, update: UpdateSection, backup: BackupSection };
for (const s of pageById("system")?.sections ?? []) {
  if (RENDERS[s.id]) s.render = RENDERS[s.id];
  if (s.id === "update") s.acts = UpdateActions;
}
