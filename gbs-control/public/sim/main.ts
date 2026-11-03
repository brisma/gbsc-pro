// npm run sim [-- --board standard --port 8080 --ws 8081 --page dist/app.html]
import { startSim } from "./server.ts";

const a = process.argv.slice(2);
const opt = (name: string) => { const i = a.indexOf(name); return i >= 0 ? a[i + 1] : undefined; };
const sim = await startSim({
  board: (opt("--board") as "pro" | "standard") ?? "pro",
  port: Number(opt("--port") ?? 8080), wsPort: Number(opt("--ws") ?? 8081), pagePath: opt("--page"),
});
console.log(`simulator: page ${sim.url}/  websocket ${sim.wsUrl}  (board ${sim.state.board}; Ctrl+C stops)`);
