// WebSocket on its own port, as the device's port 81: subprotocol "arduino", handshake needs an allowed Origin
import { WebSocketServer, type WebSocket } from "ws";
import type { AddressInfo } from "node:net";
import { originAllowed } from "./guard.ts";

export class WsHub {
  server: WebSocketServer | null = null;
  port = 0;
  refuse = false;   // fault wsRefuse
  silent = false;   // fault wsSilent: the sockets stay open but nothing is sent (a WiFi blip)
  private extraHosts: () => string;
  private blocked: () => boolean;   // safe mode: the device runs no WebSocket server

  constructor(extraHosts: () => string, blocked: () => boolean = () => false) { this.extraHosts = extraHosts; this.blocked = blocked; }

  start(port: number): Promise<number> {
    return new Promise((resolve) => {
      this.server = new WebSocketServer({
        port, host: "127.0.0.1",
        handleProtocols: (protocols) => (protocols.has("arduino") ? "arduino" : false),
        verifyClient: (info, done) => {
          if (this.refuse || this.blocked()) return done(false, 503);
          const origin = info.req.headers.origin;
          done(originAllowed(origin, this.extraHosts()), 403);
        },
      });
      this.server.on("listening", () => { this.port = (this.server!.address() as AddressInfo).port; resolve(this.port); });
    });
  }

  broadcast(msg: string): void {
    if (!this.server || this.silent) return;
    for (const c of this.server.clients) if (c.readyState === 1) c.send(msg);
  }

  dropAll(): void {
    if (!this.server) return;
    for (const c of this.server.clients) (c as WebSocket).terminate();
  }

  stop(): Promise<void> {
    return new Promise((resolve) => {
      if (!this.server) return resolve();
      this.dropAll();
      this.server.close(() => resolve());
    });
  }
}
