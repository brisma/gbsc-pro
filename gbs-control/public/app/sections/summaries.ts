// The line under a card title says what the card is for (the section's hint). A section whose line is its state
// (Update: up to date, on offer, installing) gives it here.
import { updateSummary } from "../pages/system/UpdateSection.tsx";

export const SUBTITLES: Record<string, () => string> = { update: updateSummary };
