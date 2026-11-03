// Settings registry embedded at build time (gen/registry.json, trimmed by tools/build-app.mjs): types, ranges,
// option lists with their Advanced-only and capability marks, and the bridged keys (no WebSocket echo)

export interface RegOption { n: string; v: number; advanced?: boolean; caps?: number }
export interface RegEntry {
  key: string;
  type: "bool" | "u8" | "u16" | "i8" | "i16" | "enum" | "str" | string;
  min?: number;
  max?: number;
  def?: number;
  maxLen?: number;         // strings (registry.json)
  step?: number;
  options?: RegOption[];
  bridged?: boolean;
  ro?: boolean;
  secret?: boolean;
  dev?: boolean;
  caps?: number;
}
export interface Registry { settings: RegEntry[] }

export interface RegistryIndex {
  entry(key: string): RegEntry | undefined;
  optionValue(key: string, label: string): number | undefined;   // enum label (WebSocket echo) -> value
  optionLabel(key: string, v: number): string | undefined;
  bridged(key: string): boolean;
}

export function indexRegistry(r: Registry): RegistryIndex {
  const byKey = new Map(r.settings.map((e) => [e.key, e]));
  return {
    entry: (k) => byKey.get(k),
    optionValue: (k, label) => byKey.get(k)?.options?.find((o) => o.n === label)?.v,
    optionLabel: (k, v) => byKey.get(k)?.options?.find((o) => o.v === v)?.n,
    bridged: (k) => !!byKey.get(k)?.bridged,
  };
}
