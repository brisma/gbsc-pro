// The ADV preset and macro store on the device (/api/adv/*): lists, texts, saves (multipart, 202 then
// s:status.adv_store) and deletes, with the reasons said in words
import { signal } from "@preact/signals";
import { device } from "../../../store/app.ts";
import { logLine } from "./adv-io.ts";

export type StoreKind = "preset" | "macro";
export interface StoreItem { id: string; name: string; description: string; count: number; bytes: number }
export const storeLists = signal<{ preset: StoreItem[]; macro: StoreItem[] } | null>(null);

const REASONS: Record<string, string> = {
  size: "the file is too big (presets 8 KB, macros 2 KB)", invalid: "the file is empty or could not be written", id: "unknown id",
  busy: "the device is busy with another save: try again", full: "the device holds 16 presets and 32 macros at most",
  space: "not enough space on the device (profiles keep their reserve)", control: "the text has control characters",
  line: "a line is longer than 127 characters", section: "a line starts with [", name: "the file has no name line",
  write: "the device could not write the file", memory: "the device is short on memory: try again",
};
export const reasonText = (r: string) => REASONS[r] ?? r;

export async function loadLists(): Promise<void> {
  try {
    const [p, m] = await Promise.all([device.client.getJson<{ items: StoreItem[] }>("/api/adv/presets"), device.client.getJson<{ items: StoreItem[] }>("/api/adv/macros")]);
    storeLists.value = { preset: p.items, macro: m.items };
  } catch (e) { logLine("err", `store list: ${String(e)}`); }
}

export async function storeText(kind: StoreKind, id: string): Promise<string | null> {
  const res = await device.client.get(`/api/adv/${kind}?id=${id}`);
  return res.ok ? await res.text() : null;
}

async function result(seq: number): Promise<string> {
  for (let i = 0; i < 50; i++) {
    if (device.advStore.value.seq !== seq) return device.advStore.value.result;
    await new Promise((r) => setTimeout(r, 100));
  }
  return "error:timeout";
}

// true when saved; a refusal is told in a toast with its reason
export async function storeSave(kind: StoreKind, text: string, fileName: string, id?: string): Promise<boolean> {
  const form = new FormData();
  if (id) form.append("id", id);
  form.append("file", new Blob([text], { type: "application/x-yaml" }), fileName);
  const seq = device.advStore.value.seq;
  const res = await device.client.post(`/api/adv/${kind}`, form);
  let why: string | null = null;
  if (res.status !== 202) {
    const body = await res.json().catch(() => ({} as { error?: string }));
    why = body.error ?? `HTTP ${res.status}`;
  } else {
    const r = await result(seq);
    if (r === "error:timeout") {   // no s:status.adv_store (the WebSocket is down): the list tells whether it landed
      await loadLists();
      const name = /^name:\s*(.*)$/m.exec(text)?.[1]?.trim().replace(/^(['"])(.*)\1$/, "$2") ?? "";
      const id = (await res.json().catch(() => ({} as { id?: string }))).id ?? "";
      const it = storeLists.value?.[kind].find((x) => x.id === id);
      if (it && it.name === name) { logLine("ok", `saved ${kind} ${fileName} (checked in the list: no status message)`); return true; }
      why = "timeout";
    } else if (r !== "ok") why = r.replace(/^error:/, "");
  }
  if (why) { device.toast(`Not saved: ${reasonText(why)}`, "error"); logLine("err", `save ${kind}: ${why}`); return false; }
  logLine("ok", `saved ${kind} ${fileName}`);
  await loadLists();
  return true;
}

export async function storeDelete(kind: StoreKind, id: string): Promise<boolean> {
  const seq = device.advStore.value.seq;
  const res = await device.client.post(`/api/adv/${kind}/delete`, { id });
  const r = res.status === 202 ? await result(seq) : `error:HTTP ${res.status}`;
  if (r !== "ok") { device.toast(`Not deleted: ${reasonText(r.replace(/^error:/, ""))}`, "error"); return false; }
  await loadLists();
  return true;
}
