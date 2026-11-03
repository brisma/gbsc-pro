// Connector drawings (original art drawn for GBSC PRO), as data URLs for <img>. Index = InputType (1 RGB SCART ...
// 6 Composite).
const svgUrl = (s: string) => "data:image/svg+xml;utf8," + encodeURIComponent(s).replace(/\(/g, "%28").replace(/\)/g, "%29").replace(/'/g, "%27");
const SVG = (vb: string, body: string) => `<svg xmlns="http://www.w3.org/2000/svg" viewBox="${vb}">${body}</svg>`;
const shadow = (cx: number, cy: number, rx: number) => `<ellipse cx="${cx}" cy="${cy}" rx="${rx}" ry="6" fill="#000" opacity=".28"/>`;
const rca = (x: number, y: number, col: string, rot: number) => `<g transform="translate(${x} ${y}) rotate(${rot})"><rect x="-6" y="0" width="12" height="40" rx="3" fill="#18191c"/><rect x="-8" y="-20" width="16" height="22" rx="4" fill="${col}"/><rect x="-8" y="-20" width="5" height="22" rx="2" fill="#fff" opacity=".22"/><rect x="-5.5" y="-34" width="11" height="15" rx="2" fill="url(#m)"/><rect x="-1.6" y="-44" width="3.2" height="12" rx="1.6" fill="#d4af37"/></g>`;
const metal = `<linearGradient id="m" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#8e9198"/><stop offset=".5" stop-color="#eef0f3"/><stop offset="1" stop-color="#8e9198"/></linearGradient>`;
const range = (n: number) => Array.from({ length: n }, (_, i) => i);

const ART: Record<string, string> = {
  scart: SVG("0 0 120 80", `<defs><linearGradient id="b" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#3b3e45"/><stop offset="1" stop-color="#16171a"/></linearGradient></defs>${shadow(60, 72, 46)}<path d="M2 50 C18 50 22 44 30 42" stroke="#111" stroke-width="8" fill="none" stroke-linecap="round"/><path d="M26 14 H100 L112 26 V60 Q112 64 108 64 H30 Q26 64 26 60 Z" fill="url(#b)" stroke="#5a5d65"/><path d="M34 22 H96 L104 30 V54 H34 Z" fill="#0c0c0e"/>${range(10).map((i) => `<rect x="${39 + i * 6}" y="32" width="3" height="4" rx=".8" fill="#e2b93b"/>`).join("")}${range(10).map((i) => `<rect x="${42 + i * 6}" y="43" width="3" height="4" rx=".8" fill="#e2b93b"/>`).join("")}`),
  vga: SVG("0 0 120 80", `<defs>${metal}<linearGradient id="b" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4f7dff"/><stop offset="1" stop-color="#1e48c2"/></linearGradient></defs>${shadow(60, 72, 50)}<rect x="10" y="20" width="100" height="42" rx="10" fill="url(#b)"/><circle cx="20" cy="41" r="6" fill="url(#m)"/><circle cx="100" cy="41" r="6" fill="url(#m)"/><path d="M34 26 H86 L80 56 H40 Z" fill="url(#m)" stroke="#7d8087"/><path d="M38 29 H82 L77 53 H43 Z" fill="#111827"/>${[0, 1, 2].map((r) => range(5).map((i) => `<circle cx="${48 + i * 6 + (r === 1 ? 3 : 0)}" cy="${35 + r * 7}" r="1.7" fill="#d4af37"/>`).join("")).join("")}`),
  yuv: SVG("0 0 120 80", `<defs>${metal}</defs>${shadow(60, 74, 46)}${rca(36, 52, "#22a35a", -18)}${rca(60, 50, "#2e6fe0", 0)}${rca(84, 52, "#e0342e", 18)}`),
  rgbs: SVG("0 0 120 80", `<defs>${metal}</defs>${shadow(60, 74, 46)}${rca(36, 52, "#e0342e", -18)}${rca(60, 50, "#22a35a", 0)}${rca(84, 52, "#2e6fe0", 18)}`),
  av: SVG("0 0 120 80", `<defs>${metal}</defs>${shadow(60, 74, 46)}${rca(40, 54, "#f4f4f4", -14)}${rca(80, 54, "#e0342e", 14)}<g transform="scale(1.15) translate(-8 -6)">${rca(60, 52, "#f2c230", 0)}</g>`),
  sv: SVG("0 0 120 80", `<defs>${metal}<radialGradient id="g" cx=".4" cy=".35" r=".8"><stop offset="0" stop-color="#3a3d44"/><stop offset="1" stop-color="#0f1013"/></radialGradient></defs>${shadow(60, 74, 34)}<path d="M60 66 V80" stroke="#111" stroke-width="10"/><circle cx="60" cy="38" r="30" fill="url(#g)"/><circle cx="60" cy="38" r="22" fill="url(#m)"/><circle cx="60" cy="38" r="19" fill="#111214"/>${[[52, 32], [68, 32], [50, 44], [70, 44]].map(([a, b]) => `<circle cx="${a}" cy="${b}" r="2.4" fill="#e2b93b"/>`).join("")}<rect x="56" y="47" width="8" height="8" rx="1" fill="#5a5d65"/>`),
};

// InputType -> drawing (scart, rgbs, vga, yuv, sv, av)
const BY_INPUT = ["", "scart", "rgbs", "vga", "yuv", "sv", "av"];
const cache = new Map<string, string>();
export function connectorUrl(input: number): string {
  const k = BY_INPUT[input] ?? "";
  if (!k) return "";
  if (!cache.has(k)) cache.set(k, svgUrl(ART[k]));
  return cache.get(k)!;
}
