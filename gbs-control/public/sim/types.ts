// Shapes of the files in public/gen/ (written by test_web_export) and of the simulator's options
export type Board = "pro" | "standard";
export type ScenarioName = string;

export interface RegOption { n: string; v: number; caps: number; advanced: boolean }
export interface RegEntry {
  key: string; type: "bool" | "u8" | "u16" | "enum" | "str"; scope: "device" | "profile";
  min?: number; max?: number; def?: number; maxLen?: number;
  dev: boolean; caps: number; ro: boolean; secret: boolean; bridged: boolean; bridge: null | "toggle" | number[];
  options: RegOption[];
}
export interface Registry { boolWords: { true: string[]; false: string[] }; settings: RegEntry[] }
export interface ActionParam { name: string; type: string; len?: number; max_len?: number; max?: number; values?: string[]; optional?: number }
export interface ActionAll { name: string; caps: number; kind: string; flags: string[]; params: ActionParam[] }
export interface RouteRow { path: string; methods: string[]; match: "exact" | "prefix" }
export interface Routes { normal: RouteRow[]; safe: RouteRow[] }

export interface Gen {
  dir: string;
  schema: Record<string, string>;       // "pro" | "pro.dev" | "standard" | "standard.dev" → raw JSON text
  actions: Record<string, string>;
  actionsAll: ActionAll[];
  values: Record<Board, Record<string, number | string>>;
  registry: Registry;
  routes: Routes;
  menu: any;
  logsample: { record: any; line: string }[];
  catalog: string[];
}

export interface SimOptions {
  port?: number; wsPort?: number; board?: Board; genDir?: string; pagePath?: string;
  net?: "fast" | "device"; scenario?: ScenarioName; nextFw?: string;
}

// platform caps (pro/settings/settings-types.h)
export const CAP_ADV = 1, CAP_OSD = 2, CAP_AUDIO = 4, CAP_IR = 8, CAP_OLED = 16, CAP_ENCODER = 32, CAP_LED = 64, CAP_ORIGINAL = 128;
export const BOARD_CAPS: Record<Board, number> = {
  pro: CAP_ADV | CAP_OSD | CAP_AUDIO | CAP_IR | CAP_OLED | CAP_ENCODER,
  standard: CAP_ORIGINAL | CAP_LED,
};
