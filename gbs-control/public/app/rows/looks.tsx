// Thumbnails and the few words of the look choices, and the help pictures
import { patternUrl } from "../art/patterns.tsx";
import { scanlineDarkness } from "../model/scanlines.ts";

export type Look = "filter" | "scanlines" | "deint" | "hdmi";

const WORDS: Record<Look, Record<string, string>> = {
  filter: { Normal: "scaler default", Sharp: "crisp pixels", Smooth: "soft, blends dither" },
  scanlines: { Off: "no lines", Light: "keeps it bright", Medium: "balanced", Strong: "closest to a CRT", Custom: "Advanced level",
               "10": "Light", "20": "between", "30": "Medium", "40": "between", "50": "Strong", "60": "darkest" },
  deint: { Adaptive: "sharp, may comb on motion", Bob: "soft, no combing" },
  hdmi: { Auto: "picks for you", Full: "PC levels, deep black", Limited: "TV levels", "Limited SD": "TV levels, SD modes" },
};
export const lookWords = (look: Look, label: string) => WORDS[look][label] ?? "";

// Scanline darkness of a card (level / 60 * .8)
function scanAlpha(label: string, strength: number | null): number {
  const lv = ({ Off: 0, Light: 10, Medium: 30, Strong: 50 } as Record<string, number>)[label] ??
    (strength !== null ? scanlineDarkness(strength) : Number(label) || 20);
  return (lv / 60) * 0.8;
}

export function LookThumb({ look, label, strength = null }: { look: Look; label: string; strength?: number | null }) {
  if (look === "filter")
    return <img alt="" src={patternUrl("pix")} style={`image-rendering:${label === "Sharp" ? "pixelated" : "auto"};filter:${label === "Smooth" ? "blur(1.6px)" : label === "Normal" ? "blur(.5px)" : "none"}`} />;
  if (look === "scanlines")
    return <span class="scimg" style={`--sa:${scanAlpha(label, strength).toFixed(2)}`}><img alt="" src={patternUrl("bars")} style="image-rendering:pixelated" /></span>;
  if (look === "deint")
    return <img alt="" src={patternUrl(label === "Bob" ? "bar" : "comb")} style={`image-rendering:pixelated;filter:${label === "Bob" ? "blur(.7px)" : "none"}`} />;
  return <span class={`himg ${label === "Full" ? "full" : label === "Auto" ? "auto" : "lim"}`} />;
}

// The pictures of the Scanlines and Filter help
export function HelpPicture({ look }: { look: string }) {
  if (look === "scanlines")
    return (
      <div class="prev" style="grid-template-columns:repeat(4,1fr)">
        {([["Off", 0], ["Light", 0.22], ["Medium", 0.42], ["Strong", 0.65]] as [string, number][]).map(([n, a]) => (
          <div><span class="sc" style={`--sa:${a}`}><img alt="" src={patternUrl("bars")} style="image-rendering:pixelated" /></span>{n}</div>
        ))}
      </div>
    );
  return (
    <div class="prev">
      {([["Normal", "auto", "none"], ["Sharp", "pixelated", "none"], ["Smooth", "auto", "blur(1.4px)"]] as [string, string, string][]).map(([n, ir, f]) => (
        <div><img alt="" src={patternUrl("bars")} style={`image-rendering:${ir};filter:${f}`} />{n}</div>
      ))}
    </div>
  );
}

// Resolution cards: what each output is for
export const RES_HELP: Record<string, [string, string]> = {
  "Downscale": ["Smaller picture", "for low resolution screens"], "480p/576p": ["Line doubled", "SD screens and capture cards"],
  "720p": ["HD ready", "older HD TVs"], "960p fixed": ["Always 960p", "4 lines per line at 60 Hz"],
  "960p/1024p": ["Even scanlines", "960p at 60 Hz, 1024p at 50 Hz"], "1024p fixed": ["Always 1024p", "4 lines per line at 50 Hz"],
  "1080p": ["Most TVs", "the safe choice"], "1200p": ["Even scanlines ×5", "at 60 Hz, on monitors that accept it"], "HD bypass": ["Passes HD through", "for HD sources"],
};
