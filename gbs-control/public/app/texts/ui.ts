// UI strings of the web app in one table (translations later; English as the menu)
export const T = {
  run: "Run",
  reset: "Reset",
  resetTo: (v: string) => `Reset to ${v}`,
  holdToRepeat: "Hold an arrow to repeat",
  help: (label: string) => `Help: ${label}`,
  close: "Close",
  cancel: "Cancel",
  inSettings: (where: string, advanced: boolean) => `In ${where}${advanced ? " · Advanced" : ""}`,
  auto: "Auto",
  autoHint: "finds the cable by itself",
  signal: "signal",
  searching: "Auto · searching…",
  custom: "Custom",
  notSent: (what: string, status: number) => `${what}: not applied (HTTP ${status})`,
} as const;
