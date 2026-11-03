// Port of Admission::decide (pro/api/api-admission.cpp) and the exempt routes of AdmissionHandler (api-web.cpp)
export type Verdict = "admit" | "update" | "memory" | "load";

export const HEAP_LOW = 10000, HEAP_OK = 12000, BLOCK_LOW = 1536, BLOCK_OK = 2560, UPLOAD_BLOCK = 5120, MAX_CONNECTIONS = 3;

export class Admission {
  low = false;
  // connections: admitted connections still open, the new one included
  decide(freeHeap: number, maxBlock: number, connections: number, updateBusy: boolean, isUpload: boolean): Verdict {
    if (updateBusy) return "update";
    if (this.low) this.low = !(freeHeap >= HEAP_OK && maxBlock >= BLOCK_OK);
    else this.low = freeHeap < HEAP_LOW || maxBlock < BLOCK_LOW;
    if (this.low) return "memory";
    if (isUpload) {
      if (maxBlock < UPLOAD_BLOCK) return "memory";
      if (connections > 2) return "load";
    } else if (connections > MAX_CONNECTIONS) return "load";
    return "admit";
  }
}

// Never refused: small info, the log tool when memory is short, and the restart (a way out of every state)
export function exempt(method: string, path: string): boolean {
  return (method === "GET" && (path === "/api/info" || path === "/api/log")) ||
         (method === "POST" && path === "/api/actions/system.restart");
}
