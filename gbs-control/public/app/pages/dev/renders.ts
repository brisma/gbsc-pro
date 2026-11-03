// The Developer sections that draw themselves, attached to the page description at start (layout/pages.ts stays
// data only, readable by the Node unit tests)
import type { ComponentType } from "preact";
import { pageById } from "../../layout/pages.ts";
import { RegistersSection } from "./Registers.tsx";
import { DecoderSection, EncoderSection } from "./adv/AdvRegs.tsx";
import { PresetsSection } from "./adv/Presets.tsx";
import { ManualSection } from "./adv/Manual.tsx";

const RENDERS: Record<string, ComponentType<{ desk: boolean }>> = { registers: RegistersSection, decoder: DecoderSection, encoder: EncoderSection, presets: PresetsSection, manual: ManualSection };
for (const s of pageById("dev")?.sections ?? []) if (RENDERS[s.id]) s.render = RENDERS[s.id];
