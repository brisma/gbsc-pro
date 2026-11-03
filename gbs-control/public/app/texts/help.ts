// The longer "?" texts of the web page (the menu keeps its short hints), keyed by menu.json id. A row without an entry shows the menu hint.
export const HELP: Record<string, [string, string]> = {
  "input/source": ["Source", "Which cable the console is plugged into. Auto tries the inputs in turn (VGA, RGB SCART / SoG / Component, then S-Video and Composite) and keeps the first that carries a stable picture. If you choose a cable by hand, Auto comes back once the signal has been gone for the Wait before search time (2 seconds by default)."],
  "output/resolution": ["Resolution", "What the TV receives. 1080p suits most TVs. 960p/1024p gives every source line the same height, so scanlines look even (960p at 60 Hz, 1024p at 50 Hz). 1200p does it at 60 Hz only. 720p helps older HD TVs. The change applies at once."],
  "picture/scanlines": ["Scanlines", "Dark lines between the picture lines, like on a CRT. Light keeps the picture bright; Strong looks closer to a CRT but darkens the image. Fine strength is in Advanced (Scanline strength)."],
  "picture/filter": ["Filter", "How the edges of pixels are drawn when the picture is scaled. Normal is the scaler's default. Sharp keeps square, crisp pixels. Smooth softens them and brings back the blending that games drew with dithering."],
  "input/2x": ["2X", "Line doubler for S-Video and Composite: the decoder turns 480i/576i into 480p/576p before the scaler. It has no frame memory, so interlaced games may flicker: for them keep 2X off and let the scaler deinterlace. 2X smooth makes motion softer."],
  "output/smooth-scrolling": ["Smooth scrolling", "Locks the output frame rate to the console (frame lock), so scrolling has no stutter and no tearing. A few TVs blank out when the rate is not exactly 50 or 60 Hz: if yours does, turn it off or try the other Lock method."],
  "output/hdmi-range": ["HDMI range", "If blacks look grey or the picture looks washed out, try Full; if dark areas look crushed, try Limited. Auto picks Limited for TV resolutions and Full for PC ones."],
  "picture/brightness": ["Brightness", "Brightness of the final picture for every input. 50 is neutral."],
  "input/video-standard": ["Video standard", "Colour system of the S-Video or Composite signal. Auto finds it by itself. If blacks look grey on an NTSC console, use Auto 7.5 IRE."],
  "input/sync-cleanup": ["Sync cleanup", "Cleans the sync carried on the SCART or luma line before the scaler. Keep it on; turn it off only if the picture rolls or drops."],
  "screen/position": ["Position", "Moves the picture on the TV. Hold an arrow to keep moving. Reset screen puts it back."],
  "screen/size": ["Size", "Makes the picture wider or taller. Hold an arrow to keep going."],
  "screen/edge-mask": ["Edge mask", "Hides junk at the borders of the picture (lines of noise or colour some consoles draw outside the image)."],
  "picture/deinterlace": ["Deinterlace", "How interlaced pictures (480i, 576i) are shown. Adaptive is sharper on still images; Bob avoids combing on fast motion."],
  "quick/profile": ["Profile", "A profile keeps every picture setting for one console or game. With Auto-load on, the right profile loads by itself when its signal comes back."],
  "profiles/auto-load": ["Auto-load", "Loads the profile whose signal matches the one coming in (input, lines and timing). When another profile matches, unsaved changes of the one in use are discarded and a message says so; the profile already in use is never reloaded. With no profile for the signal, Defaults load when the profile in use was saved on another input."],
  "quick/tv-osd": ["TV OSD", "On the standard board there is no menu chip: small icons on the TV show what you change."],
  "input/sampling/gain": ["Gain", "Signal level of the RGB inputs. If the picture is dark, raise it; if it looks washed out, lower it. Hold − or + to keep going."],
  "input/decoder/brightness": ["Brightness (decoder)", "Brightness of the S-Video and Composite decoder, applied before Picture › Brightness (which acts on every input). Use one of the two."],
  "input/decoder/contrast": ["Contrast (decoder)", "Contrast of the S-Video and Composite decoder, applied before Picture › Contrast (which acts on every input)."],
  "input/decoder/saturation": ["Saturation (decoder)", "Colour of the S-Video and Composite decoder, applied before Picture › Saturation (which acts on every input)."],
  "input/decoder/hue": ["Hue (decoder)", "Shifts the colours of S-Video and Composite (skin tones), in the decoder before Picture. Picture has no hue."],
  "input/decoder/adaptive-contrast": ["Adaptive contrast", "The decoder changes contrast with the scene: more punch in dark scenes, less clipping in bright ones. S-Video and Composite only."],
  "output/analog-out": ["Analog out", "Signal on the analog port: RGB for PC monitors, or YPbPr for the component input of a TV."],
  "input/75-ohm-sync": ["75 ohm sync", "Adds the 75 ohm load on the sync line for luma-sync cables. It is not stored: it resets at restart."],
  "menu-mode": ["Menu mode", "Easy or Advanced for the menu on the TV and the OLED. It does not change this page: the switch at the top does that."],
  "system/status-pop-up": ["Status pop-up", "A small box on the TV when the input or the profile changes."],
  "system/sound/volume": ["Volume", "Volume of the audio output. The remote keys change it too."],
  "system/factory-reset": ["Factory reset", "Erases settings and profiles and goes back to the hotspot."],
  "system/board-type": ["Board type", "Auto finds the board. Change it only if features look wrong; restart to apply."],
};

// "input/video-standard~adv.format_sv" -> "input/video-standard"; quick rows use the text of the row they repeat
export function helpFor(id: string, quickTwin?: string): [string, string] | undefined {
  const base = (k: string) => k.split("~")[0];
  return HELP[id] ?? HELP[base(id)] ?? (quickTwin ? HELP[quickTwin] ?? HELP[base(quickTwin)] : undefined);
}
