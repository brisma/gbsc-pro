// Profile files (single or many, with zip): one profile downloads its .txt; Export all builds one zip in the browser;
// Import takes one or more .txt files or zips and imports them one by one through the device's import route, then shows
// what came in and what was skipped (no firmware change)
import { device } from "../../store/app.ts";
import { runOp, opErrorText, profileLabel } from "../../store/profiles.ts";
import { zipWrite, zipRead, type ZipEntry } from "../../model/zip.ts";
import { openSheet, closeSheet, SheetButtons } from "../../shell/Sheet.tsx";
import { download } from "../../shell/files.ts";
import type { ProfileEntry } from "../../store/device.ts";


// The device's file name (Content-Disposition), else one from the label
async function fetchExport(p: ProfileEntry): Promise<ZipEntry | null> {
  const r = await device.client.get(`/api/profiles/export?id=${encodeURIComponent(p.id)}`);
  if (!r.ok) { device.toast(`${profileLabel(p)}: not exported (HTTP ${r.status})`, "error"); return null; }
  const cd = r.headers.get("Content-Disposition") ?? "";
  const name = /filename="([^"]+)"/.exec(cd)?.[1] ?? `${p.name.replace(/[/:' ]/g, "_")}.gbscprofile.txt`;
  return { name, data: new Uint8Array(await r.arrayBuffer()) };
}

export async function exportOne(p: ProfileEntry): Promise<void> {
  const f = await fetchExport(p);
  if (f) download(f.data as BlobPart, f.name, "text/plain");
}

export async function exportAll(): Promise<void> {
  const list = device.profiles.value?.profiles ?? [];
  const files: ZipEntry[] = [];
  for (const p of list) { const f = await fetchExport(p); if (f) files.push(f); }   // one at a time: the device's discipline
  if (!files.length) return;
  download(zipWrite(files) as BlobPart, "gbsc-profiles.zip", "application/zip");
  console.debug(`[profiles] exported ${files.length} profile(s) into a zip`);
}

async function expand(files: File[]): Promise<{ entries: ZipEntry[]; bad: [string, string][] }> {
  const entries: ZipEntry[] = [], bad: [string, string][] = [];
  for (const f of files) {
    const data = new Uint8Array(await f.arrayBuffer());
    const isZip = data[0] === 0x50 && data[1] === 0x4b;
    if (!isZip) { entries.push({ name: f.name, data }); continue; }
    try { entries.push(...(await zipRead(data))); } catch (e) { bad.push([f.name, String((e as Error).message)]); }
  }
  return { entries, bad };
}

export async function importFiles(files: File[]): Promise<void> {
  const { entries, bad } = await expand(files);
  let ok = 0;
  for (const e of entries) {
    const form = new FormData();
    form.append("file", new Blob([e.data as BlobPart], { type: "text/plain" }), e.name.split("/").pop() ?? e.name);
    const r = await runOp({ post: (p, f) => device.client.post(p, f), profileOp: device.profileOp, reload: () => device.reloadProfiles() },
                          "/api/profiles/import", form);
    if (r === "ok") ok++; else bad.push([e.name, r]);
    if (r === "limit") { for (const rest of entries.slice(entries.indexOf(e) + 1)) bad.push([rest.name, "limit"]); break; }
  }
  console.debug(`[profiles] import: ${ok} imported, ${bad.length} skipped`);
  openSheet({
    title: "Import", icon: "upload_file",
    body: () => (
      <>
        <p><b>{ok} imported</b>{bad.length ? `, ${bad.length} skipped` : ""}.</p>
        {bad.length ? <ul class="skips">{bad.map(([n, why]) => <li><b>{n}</b>: {why.startsWith("not a zip") ? why : opErrorText(why)}</li>)}</ul> : null}
        <SheetButtons><button type="button" class="btn" onClick={closeSheet}>Close</button></SheetButtons>
      </>
    ),
  });
}
