// Request/response helpers shared by the route files
import type http from "node:http";
import zlib from "node:zlib";

export interface Ctx {
  req: http.IncomingMessage; res: http.ServerResponse; url: URL; body: Buffer;
  form: [string, string][];   // POST body params (urlencoded), like AsyncWebParameter isPost()
  query: [string, string][];
}

export function arg(ctx: Ctx, name: string): string | null {
  const hit = ctx.form.find(([k]) => k === name) || ctx.query.find(([k]) => k === name);
  return hit ? hit[1] : null;
}

export function sendJson(ctx: Ctx, code: number, body: unknown): void {
  const text = typeof body === "string" ? body : JSON.stringify(body);
  ctx.res.writeHead(code, { "Content-Type": "application/json", "Cache-Control": "no-store" });
  ctx.res.end(text);
}

export function sendGzip(ctx: Ctx, code: number, type: string, text: string): void {
  ctx.res.writeHead(code, { "Content-Type": type, "Content-Encoding": "gzip", "Cache-Control": "no-cache" });
  ctx.res.end(zlib.gzipSync(Buffer.from(text, "utf8"), { level: 9 }));
}

export function parseForm(req: http.IncomingMessage, body: Buffer): [string, string][] {
  const type = String(req.headers["content-type"] || "");
  if (!type.startsWith("application/x-www-form-urlencoded")) return [];
  return [...new URLSearchParams(body.toString("utf8")).entries()];
}

export interface Part { name: string; filename: string | null; data: Buffer }

// multipart/form-data (uploads); the device streams the file part, the simulator has the whole body
export function parseMultipart(req: http.IncomingMessage, body: Buffer): Part[] {
  const m = /boundary=(?:"([^"]+)"|([^;]+))/.exec(String(req.headers["content-type"] || ""));
  if (!m) return [];
  const boundary = Buffer.from("--" + (m[1] || m[2]));
  const parts: Part[] = [];
  let pos = body.indexOf(boundary);
  while (pos >= 0) {
    const start = pos + boundary.length;
    if (body.slice(start, start + 2).toString() === "--") break;
    const headEnd = body.indexOf("\r\n\r\n", start);
    if (headEnd < 0) break;
    const head = body.slice(start + 2, headEnd).toString("utf8");
    const next = body.indexOf(boundary, headEnd + 4);
    if (next < 0) break;
    const data = body.slice(headEnd + 4, next - 2);
    const name = /name="([^"]*)"/.exec(head)?.[1] ?? "";
    const fn = /filename="([^"]*)"/.exec(head);
    parts.push({ name, filename: fn ? fn[1] : null, data });
    pos = next;
  }
  return parts;
}
