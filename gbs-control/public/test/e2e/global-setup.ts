// Builds dist/app.html once before the run (the simulator serves it); gen/ must exist (npm run gen)
import { execFileSync } from "node:child_process";
import { existsSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

export default function globalSetup(): void {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  if (!existsSync(join(root, "gen", "manifest.json"))) throw new Error("public/gen is missing: run npm run gen first");
  execFileSync(process.execPath, [join(root, "tools", "build-app.mjs")], { stdio: "inherit", cwd: root });
  // the row components' kitchen sink (served at /__sim/components), outside the size gate
  execFileSync(process.execPath, [join(root, "tools", "build-app.mjs"), "--entry", "app/dev/components.tsx", "--out", "dist/components.html", "--no-gate"],
               { stdio: "inherit", cwd: root });
}
