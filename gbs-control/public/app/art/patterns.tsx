// Test-pattern thumbnails of the look choices, drawn as pixel SVGs instead of a canvas so they render the same
// everywhere (screenshots in the container too)
type Px = [number, number, number, number, string];   // x, y, w, h, colour
const svg = (w: number, h: number, px: Px[]) =>
  "data:image/svg+xml;utf8," + encodeURIComponent(`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${w} ${h}" width="${w}" height="${h}" shape-rendering="crispEdges">${px.map(([x, y, a, b, c]) => `<rect x="${x}" y="${y}" width="${a}" height="${b}" fill="${c}"/>`).join("")}</svg>`);

const BG = "#16181b", INK = "#e8e6e1", ACCENT = "#f6d21a";
const cache = new Map<string, string>();
const once = (k: string, f: () => string) => { if (!cache.has(k)) cache.set(k, f()); return cache.get(k)!; };

export type Pattern = "pix" | "bar" | "comb" | "bars";
export function patternUrl(kind: Pattern): string {
  return once(kind, () => {
    const px: Px[] = [];
    if (kind === "bars") {   // colour bars over a grey ramp with a small white chevron
      const bars = ["#c0c0c0", "#c0c000", "#00c0c0", "#00c000", "#c000c0", "#c00000", "#0000c0"];
      bars.forEach((b, i) => px.push([Math.round((i * 24) / 7), 0, Math.ceil(24 / 7), 10, b]));
      for (let x = 0; x < 24; x++) { const v = Math.round((x / 23) * 255); px.push([x, 10, 1, 5, `rgb(${v},${v},${v})`]); }
      for (const [x, y] of [[3, 3], [4, 4], [5, 5], [6, 6], [7, 5], [8, 4], [9, 3]]) px.push([x + 6, y, 1, 1, "#fff"]);
      return svg(24, 15, px);
    }
    px.push([0, 0, 16, 9, BG]);
    if (kind === "pix") {
      for (let i = 0; i < 8; i++) px.push([1 + i, 7 - i, 2, 1, ACCENT]);
      for (let y = 1; y < 8; y++) for (let x = 10; x < 15; x++) if ((x + y) % 2) px.push([x, y, 1, 1, INK]);
    }
    if (kind === "bar") px.push([6, 0, 4, 9, INK]);
    if (kind === "comb") for (let y = 0; y < 9; y++) px.push([y % 2 ? 7 : 5, y, 4, 1, INK]);
    return svg(16, 9, px);
  });
}
