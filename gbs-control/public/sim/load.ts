// Reads public/gen/ (npm run gen) once per simulator
import { readFileSync, existsSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import type { Gen } from "./types.ts";

export const defaultGenDir = join(dirname(fileURLToPath(import.meta.url)), "..", "gen");

export function loadGen(dir = defaultGenDir): Gen {
  if (!existsSync(join(dir, "manifest.json"))) throw new Error(`simulator: ${dir} is missing, run npm run gen`);
  const text = (f: string) => readFileSync(join(dir, f), "utf8");
  const json = (f: string) => JSON.parse(text(f));
  const schema: Record<string, string> = {}, actions: Record<string, string> = {};
  for (const b of ["pro", "standard"]) for (const d of ["", ".dev"]) {
    schema[b + d] = text(`schema.${b}${d}.json`);
    actions[b + d] = text(`actions.${b}${d}.json`);
  }
  return {
    dir, schema, actions,
    actionsAll: json("actions-all.json"),
    values: { pro: json("values.pro.json"), standard: json("values.standard.json") },
    registry: json("registry.json"), routes: json("routes.json"), menu: json("menu.json"),
    logsample: json("logsample.json"), catalog: json("catalog.json"),
  };
}
