// Shared by the simulator unit tests: raw HTTP (to set Host/Origin), a WebSocket client that collects messages
import http from "node:http";
import zlib from "node:zlib";
import { WebSocket } from "ws";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

export const publicDir = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
export const fixturePage = join(publicDir, "test", "fixtures", "page.html");

export interface Res { status: number; headers: http.IncomingHttpHeaders; text: string; json: any }

export function req(base: string, method: string, path: string,
                    opts: { headers?: Record<string, string>; form?: Record<string, string> | [string, string][]; body?: string | Buffer } = {}): Promise<Res> {
  const u = new URL(path, base);
  let body = opts.body;
  const headers: Record<string, string> = { ...(opts.headers || {}) };
  if (opts.form) {
    const pairs = Array.isArray(opts.form) ? opts.form : Object.entries(opts.form);
    body = pairs.map(([k, v]) => encodeURIComponent(k) + "=" + encodeURIComponent(v)).join("&");
    headers["content-type"] = "application/x-www-form-urlencoded";
  }
  if (method === "POST" && !("x-gbsc" in Object.fromEntries(Object.entries(headers).map(([k, v]) => [k.toLowerCase(), v]))))
    headers["X-GBSC"] = "1";
  return new Promise((resolve, reject) => {
    const r = http.request({ host: u.hostname, port: u.port, path: u.pathname + u.search, method, headers }, (res) => {
      const chunks: Buffer[] = [];
      res.on("data", (c) => chunks.push(c));
      res.on("end", () => {
        let buf = Buffer.concat(chunks);
        if (res.headers["content-encoding"] === "gzip") buf = zlib.gunzipSync(buf);
        const text = buf.toString("utf8");
        let json: any = undefined;
        try { json = JSON.parse(text); } catch {}
        resolve({ status: res.statusCode || 0, headers: res.headers, text, json });
      });
    });
    r.on("error", reject);
    if (body !== undefined) r.write(body);
    r.end();
  });
}

export interface WsClient { ws: WebSocket; messages: string[]; waitFor(re: RegExp, ms?: number): Promise<string>; close(): void }

export function wsConnect(url: string, origin = "http://127.0.0.1"): Promise<WsClient> {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(url, ["arduino"], { headers: { Origin: origin } });
    const messages: string[] = [];
    ws.on("message", (d) => messages.push(d.toString()));
    ws.on("error", reject);
    ws.on("open", () => resolve({
      ws, messages,
      waitFor(re, ms = 2500) {
        return new Promise((ok, fail) => {
          const t0 = Date.now();
          const tick = () => {
            const hit = messages.find((m) => re.test(m));
            if (hit) return ok(hit);
            if (Date.now() - t0 > ms) return fail(new Error(`no WS message matching ${re} in ${ms} ms: ${messages.slice(-5).join(" | ")}`));
            setTimeout(tick, 20);
          };
          tick();
        });
      },
      close() { ws.close(); },
    }));
  });
}

// multipart/form-data body with one file part (uploads: firmware, restore, profile import)
export function multipart(field: string, filename: string, data: Buffer | string, extra: Record<string, string> = {}): { body: Buffer; type: string } {
  const b = "----gbscsim" + Math.random().toString(16).slice(2);
  const parts: Buffer[] = [];
  for (const [k, v] of Object.entries(extra))
    parts.push(Buffer.from(`--${b}\r\nContent-Disposition: form-data; name="${k}"\r\n\r\n${v}\r\n`));
  parts.push(Buffer.from(`--${b}\r\nContent-Disposition: form-data; name="${field}"; filename="${filename}"\r\nContent-Type: application/octet-stream\r\n\r\n`));
  parts.push(Buffer.isBuffer(data) ? data : Buffer.from(data));
  parts.push(Buffer.from(`\r\n--${b}--\r\n`));
  return { body: Buffer.concat(parts), type: `multipart/form-data; boundary=${b}` };
}

export const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));
