// Material Symbols Rounded icon from the embedded subset (tools/icons-app.mjs): the codepoint, not the ligature
// (the subset carries no ligature table). An icon missing from the subset fails the build, not the page.
import iconMap from "virtual:icons";

export function iconChar(name: string): string {
  const cp = (iconMap as Record<string, number>)[name];
  if (!cp) { console.error(`icon "${name}" is not in the subset (tools/icons-app.mjs did not see it)`); return ""; }
  return String.fromCodePoint(cp);
}

export function Icon({ name, fill, cls }: { name: string; fill?: boolean; cls?: string }) {
  return <span class={`ms${fill ? " fill" : ""}${cls ? " " + cls : ""}`} aria-hidden="true">{iconChar(name)}</span>;
}
