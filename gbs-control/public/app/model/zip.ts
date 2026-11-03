// Zip files for profile import and export (single or many, with zip). The writer stores the entries (the profile
// texts are small and the browser does not need a compressor); the reader takes stored and deflated entries,
// inflating with DecompressionStream("deflate-raw").

export interface ZipEntry { name: string; data: Uint8Array }

const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; }
  return t;
})();

export function crc32(d: Uint8Array): number {
  let c = 0xffffffff;
  for (let i = 0; i < d.length; i++) c = CRC_TABLE[(c ^ d[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

export function zipWrite(files: ZipEntry[]): Uint8Array {
  const enc = new TextEncoder();
  const chunks: Uint8Array[] = [], central: Uint8Array[] = [];
  let off = 0;
  const hdr = (size: number) => { const b = new Uint8Array(size); return { b, v: new DataView(b.buffer) }; };
  for (const f of files) {
    const name = enc.encode(f.name), crc = crc32(f.data);
    const l = hdr(30);
    l.v.setUint32(0, 0x04034b50, true); l.v.setUint16(4, 20, true); l.v.setUint16(6, 0x0800, true);   // UTF-8 names
    l.v.setUint32(14, crc, true); l.v.setUint32(18, f.data.length, true); l.v.setUint32(22, f.data.length, true);
    l.v.setUint16(26, name.length, true);
    const c = hdr(46);
    c.v.setUint32(0, 0x02014b50, true); c.v.setUint16(4, 20, true); c.v.setUint16(6, 20, true); c.v.setUint16(8, 0x0800, true);
    c.v.setUint32(16, crc, true); c.v.setUint32(20, f.data.length, true); c.v.setUint32(24, f.data.length, true);
    c.v.setUint16(28, name.length, true); c.v.setUint32(42, off, true);
    chunks.push(l.b, name, f.data);
    central.push(c.b, name);
    off += 30 + name.length + f.data.length;
  }
  const csize = central.reduce((n, b) => n + b.length, 0);
  const e = hdr(22);
  e.v.setUint32(0, 0x06054b50, true); e.v.setUint16(8, files.length, true); e.v.setUint16(10, files.length, true);
  e.v.setUint32(12, csize, true); e.v.setUint32(16, off, true);
  const all = [...chunks, ...central, e.b];
  const out = new Uint8Array(all.reduce((n, b) => n + b.length, 0));
  let p = 0;
  for (const b of all) { out.set(b, p); p += b.length; }
  return out;
}

async function inflateRaw(d: Uint8Array): Promise<Uint8Array> {
  const DS = (globalThis as any).DecompressionStream;
  if (!DS) throw new Error("this browser cannot read compressed zip files");
  const s = new Blob([d as BlobPart]).stream().pipeThrough(new DS("deflate-raw"));
  return new Uint8Array(await new Response(s).arrayBuffer());
}

// The entries of a zip, from its central directory (directories skipped)
export async function zipRead(z: Uint8Array): Promise<ZipEntry[]> {
  const v = new DataView(z.buffer, z.byteOffset, z.byteLength);
  let eocd = -1;
  for (let i = z.length - 22; i >= Math.max(0, z.length - 65557); i--) if (v.getUint32(i, true) === 0x06054b50) { eocd = i; break; }
  if (eocd < 0) throw new Error("not a zip file");
  const count = v.getUint16(eocd + 10, true);
  let p = v.getUint32(eocd + 16, true);
  const dec = new TextDecoder();
  const out: ZipEntry[] = [];
  for (let n = 0; n < count; n++) {
    if (v.getUint32(p, true) !== 0x02014b50) throw new Error("not a zip file (bad central directory)");
    const method = v.getUint16(p + 10, true), csize = v.getUint32(p + 20, true);
    const nlen = v.getUint16(p + 28, true), xlen = v.getUint16(p + 30, true), clen = v.getUint16(p + 32, true);
    const local = v.getUint32(p + 42, true);
    const name = dec.decode(z.subarray(p + 46, p + 46 + nlen));
    p += 46 + nlen + xlen + clen;
    if (name.endsWith("/")) continue;
    const start = local + 30 + v.getUint16(local + 26, true) + v.getUint16(local + 28, true);
    const body = z.subarray(start, start + csize);
    if (method === 0) out.push({ name, data: body.slice() });
    else if (method === 8) out.push({ name, data: await inflateRaw(body) });
    else throw new Error(`${name}: compression method ${method} is not supported`);
  }
  return out;
}
