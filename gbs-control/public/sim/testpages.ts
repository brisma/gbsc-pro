// GET /__sim/test-page?case=<name>: tiny pages that exercise the Playwright harness checks
import type { Ctx } from "./http.ts";
import type { Sim } from "./server.ts";

const CASES: Record<string, string> = {
  outside: `<p>outside</p><script>fetch("https://example.com/x").catch(()=>{});try{new WebSocket("ws://example.com/")}catch(e){}</script>`,
  "console-error": `<p>console</p><script>console.error("deliberate error for the harness test")</script>`,
  "three-fetches": `<p>three</p><script>for(let i=0;i<3;i++)fetch("/api/settings").catch(()=>{})</script>`,
  "wide-child": `<div style="width:200px;overflow:visible"><div style="width:400px;height:20px;background:#333">wide</div></div>`,
  overlap: `<button style="position:absolute;left:10px;top:10px;width:100px;height:48px">Under</button>
            <div style="position:absolute;left:0;top:0;width:200px;height:80px;background:#222"></div>`,
  // a scrolling list under a fixed bottom bar: the last button scrolls clear when the list has room below it
  "under-bar-scrolls": `<div style="position:fixed;inset:0 0 60px 0;overflow-y:auto"><div style="height:900px"></div><button style="height:48px">Last</button><div style="height:80px"></div></div>
            <nav style="position:fixed;left:0;right:0;bottom:0;height:60px;background:#222"></nav>`,
  // the same without room: the bar overlays the scroll area and the last button can never leave it
  "under-bar-stuck": `<div style="position:fixed;inset:0;overflow-y:auto"><div style="height:900px"></div><button style="height:48px;display:block">Stuck</button></div>
            <nav style="position:fixed;left:0;right:0;bottom:0;height:60px;background:#222"></nav>`,
  "small-button": `<button style="width:30px;height:30px;padding:0">x</button>`,
  "fetch-404": `<p>404</p><script>fetch("/api/nope").catch(()=>{})</script>`,
};

export function testPageGet(_sim: Sim, ctx: Ctx): void {
  const c = ctx.url.searchParams.get("case") || "";
  const body = CASES[c];
  ctx.res.writeHead(body ? 200 : 404, { "Content-Type": "text/html" });
  ctx.res.end(`<!doctype html><html lang="en"><head><meta charset="utf-8"><link rel="icon" href="data:,"><title>harness ${c}</title></head><body>${body ?? "unknown case"}</body></html>`);
}
