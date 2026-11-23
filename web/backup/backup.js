// SPDX-License-Identifier: GPL-3.0-only
// Penumbra's backups: the FM-1's side is firmware/src/app/backup.c (SysEx in Felucca's update framing:
// F0, 7-bit packed 00 59 CMD LEN24 BODY CHK, F7). A backup is a readable file, JSON, each part's bytes
// in base64, named by what it is. Used by the Backup page (Web MIDI), the emulator (its worklet), and
// tests/host/backup_test.mjs (node, against the browser build).

export const HELLO = 0x50, READ = 0x51, WRITE = 0x52, COMMIT = 0x53, RESTART = 0x54, JOURNEY = 0x55;
export const JPLAY = 0x7F;                                     // JOURNEY's dest: playing now (0..7 a user slot)
export const CHUNK = 256;
// the parts, in the order a restore writes them (the unit starts again after the last)
export const PARTS = [
  { obj: 9, key: "journeys", name: "The user journeys", what: "the 8 USER slots" },
  { obj: 11, key: "sounds", name: "The sound library", what: "the 8 SOUNDS slots" },
  { obj: 8, key: "playing", name: "The journey playing", what: "its scenes, its sound, A and B" },
  { obj: 10, key: "loop", name: "The loop", what: "its layers and their settings" },
  { obj: 0, key: "settings", name: "The settings", what: "SYSTEM, FX, tempo, MIDI" },
];
export const STATUS = ["ok", "not a part", "nothing saved", "out of order", "not one this Penumbra takes", "not saved"];

export function pack7(data) {
  const out = []; let acc = 0, nb = 0;
  for (const b of data) {
    acc |= b << nb; nb += 8;
    while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; }
  }
  if (nb) out.push(acc & 0x7F);
  return out;
}
export function unpack7(s) {
  const out = []; let acc = 0, nb = 0;
  for (const b of s) {
    acc |= (b & 0x7F) << nb; nb += 7;
    while (nb >= 8) { out.push(acc & 0xFF); acc >>>= 8; nb -= 8; }
  }
  return out;
}

// one message, F0..F7
export function frame(cmd, body = []) {
  const n = body.length, m = [0x00, 0x59, cmd, n & 0xFF, (n >> 8) & 0xFF, 0, ...body];
  let s = 0;
  for (const b of body) s += b;
  m.push(~s & 0xFF);
  return [0xF0, ...pack7(m), 0xF7];
}
// a message from the FM-1 (F0..F7): { cmd, body } if it is a backup's, whole
export function parse(f) {
  if (f.length < 4 || f[0] !== 0xF0 || f[f.length - 1] !== 0xF7) return null;
  const d = unpack7(Array.from(f.slice(1, -1)));
  if (d.length < 7 || d[0] !== 0 || d[1] !== 0x59 || d[2] < HELLO || d[2] > JOURNEY) return null;
  const n = d[3] | (d[4] << 8) | (d[5] << 16);
  if (n + 7 > d.length) return null;
  const body = d.slice(6, 6 + n);
  let s = 0;
  for (const b of body) s += b;
  if ((~s & 0xFF) !== d[6 + n]) return null;
  return { cmd: d[2], body };
}
// every F0..F7 in a run of bytes
export function frames(bytes) {
  const out = [];
  for (let i = 0; i < bytes.length; i++) {
    if (bytes[i] !== 0xF0) continue;
    let j = i + 1;
    while (j < bytes.length && bytes[j] !== 0xF7) j++;
    if (j < bytes.length) out.push(bytes.slice(i, j + 1));
    i = j;
  }
  return out;
}

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
function fail(code, msg) { const e = new Error(msg); e.code = code; return e; }

// a conversation with one FM-1: send(bytes F0..F7) goes to it, heard(bytes) is what comes back
export class Link {
  constructor(send) { this.send = send; this.q = []; this.waiter = null; }
  heard(bytes) {
    for (const f of frames(bytes)) {
      const m = parse(f);
      if (!m) continue;
      this.q.push(m);
      if (this.waiter) { const w = this.waiter; this.waiter = null; w(); }
    }
  }
  async next(cmd, ms) {
    const end = Date.now() + ms;
    for (;;) {
      const i = this.q.findIndex((m) => m.cmd === cmd);
      if (i >= 0) return this.q.splice(i, 1)[0];
      const left = end - Date.now();
      if (left <= 0) return null;
      let tm;
      await Promise.race([new Promise((r) => { this.waiter = r; }), new Promise((r) => { tm = setTimeout(r, left); })]);
      clearTimeout(tm);
      this.waiter = null;
    }
  }
  // one command, its answer's body (tries 3 times)
  async ask(cmd, body = [], ms = 1500, tries = 3) {
    for (let t = 0; t < tries; t++) {
      this.q = this.q.filter((m) => m.cmd !== cmd);
      if (this.send(frame(cmd, body)) === false) throw fail("lost", "The FM-1 went away.");
      const m = await this.next(cmd, ms);
      if (m) return m.body;
    }
    throw fail("noanswer", "The FM-1 did not answer.");
  }
}

// HELLO: who it is and what it keeps
export function readHello(b) {
  if (b.length < 12 || b[0] !== 1) throw fail("proto", "This Penumbra speaks another backup protocol.");
  const version = String.fromCharCode(...b.slice(1, 9)).replace(/\0+$/, "");
  const sizes = {};
  for (let i = 0, k = 12; i < b[11] && k + 2 < b.length; i++, k += 3) sizes[b[k]] = b[k + 1] | (b[k + 2] << 8);
  return { version, journeyFormat: b[9], settingsFormat: b[10], sizes };
}
export async function hello(link, ms = 1500, tries = 3) { return readHello(await link.ask(HELLO, [], ms, tries)); }

// everything the FM-1 keeps: { hello, parts: { obj: Uint8Array } }
export async function readAll(link, progress = () => {}) {
  const h = await hello(link);
  const parts = {};
  const total = PARTS.reduce((s, p) => s + (h.sizes[p.obj] || 0), 0) || 1;
  let done = 0;
  for (const p of PARTS) {
    const size = h.sizes[p.obj] || 0;
    if (!size) continue;
    const bytes = new Uint8Array(size);
    for (let off = 0; off < size;) {
      const len = Math.min(CHUNK, size - off);
      const b = await link.ask(READ, [p.obj, off & 0xFF, off >> 8, len & 0xFF, len >> 8]);
      if (b[0] !== 0) throw fail("read", `${p.name}: ${STATUS[b[0]] || b[0]}.`);
      const at = b[2] | (b[3] << 8), data = b.slice(6);
      if (at !== off || !data.length) continue;               // (asked again)
      bytes.set(data.slice(0, size - off), off);
      off += data.length;
      done += data.length;
      progress(done / total);
    }
    parts[p.obj] = bytes;
  }
  return { hello: h, parts };
}

// put parts back (some of them: { obj: Uint8Array }), then the FM-1 starts again with them
export async function writeAll(link, parts, progress = () => {}) {
  await hello(link);
  const list = PARTS.filter((p) => parts[p.obj]);
  const total = list.reduce((s, p) => s + parts[p.obj].length, 0) || 1;
  let done = 0;
  const results = {};
  for (const p of list) {
    const bytes = parts[p.obj], n = bytes.length;
    for (let off = 0; off < n;) {
      const k = Math.min(CHUNK, n - off);
      const b = await link.ask(WRITE, [p.obj, off & 0xFF, off >> 8, n & 0xFF, n >> 8, ...bytes.slice(off, off + k)]);
      const have = b[2] | (b[3] << 8);
      if (b[0] === 0 && have === off + k) { off += k; done += k; progress(done / total); }
      else if (b[0] === 3 && off) off = 0;                    // (out of order: from the start)
      else throw fail("write", `${p.name}: ${STATUS[b[0]] || b[0]}.`);
    }
    const c = await link.ask(COMMIT, [p.obj], 4000);         // (a flash write)
    results[p.obj] = c[0];
  }
  if (Object.values(results).some((s) => s === 0)) {
    try { await link.ask(RESTART, [], 1000, 1); } catch { /* it may already be starting again */ }
  }
  return results;
}

// one journey (its bytes, journey_t) into user slot dest (0..7, saved) or playing now (JPLAY); no restart.
// The answer's status: 0 ok, else STATUS's
export async function putJourney(link, dest, bytes, progress = () => {}) {
  const n = bytes.length;
  for (let off = 0; off < n;) {
    const k = Math.min(CHUNK, n - off);
    const b = await link.ask(JOURNEY, [dest, off & 0xFF, off >> 8, n & 0xFF, n >> 8, ...bytes.slice(off, off + k)], 4000);
    const have = b[2] | (b[3] << 8);
    if (b[0] === 0 && have === off + k) { off += k; progress(off / n); }
    else if (b[0] === 3 && off) off = 0;
    else throw fail("journey", STATUS[b[0]] || String(b[0]));
  }
  return 0;
}

// ---- the file
const b64 = (u8) => { let s = ""; for (const c of u8) s += String.fromCharCode(c); return btoa(s); };
const unb64 = (s) => Uint8Array.from(atob(s), (c) => c.charCodeAt(0));

export function fileName(date = new Date()) {
  const d = (n) => String(n).padStart(2, "0");
  return `penumbra-backup-${date.getFullYear()}-${d(date.getMonth() + 1)}-${d(date.getDate())}.penumbra`;
}
export function toFile(h, parts, date = new Date()) {
  const out = {
    what: "A backup of Penumbra, for the M-VAVE FM-1 (madmooneyham.github.io/Penumbra/backup/)",
    penumbra_backup: 1,
    firmware: h.version,
    journey_format: h.journeyFormat,
    settings_format: h.settingsFormat,
    saved: date.toISOString(),
    parts: {},
  };
  for (const p of PARTS)
    if (parts[p.obj]) out.parts[p.key] = { name: p.name, object: p.obj, bytes: parts[p.obj].length, data: b64(parts[p.obj]) };
  return JSON.stringify(out, null, 2) + "\n";
}
export function fromFile(text) {
  let j;
  try { j = JSON.parse(text); } catch { throw fail("file", "This is not a Penumbra backup (not JSON)."); }
  if (!j || j.penumbra_backup !== 1 || typeof j.parts !== "object") throw fail("file", "This is not a Penumbra backup.");
  const parts = {};
  for (const p of PARTS) {
    const e = j.parts[p.key];
    if (!e) continue;
    const u8 = unb64(e.data || "");
    if (e.object !== p.obj || u8.length !== e.bytes || u8.length > 3840) throw fail("file", `${p.name}: damaged in the file.`);
    parts[p.obj] = u8;
  }
  return { firmware: String(j.firmware || "?"), journeyFormat: j.journey_format | 0, saved: j.saved ? new Date(j.saved) : null, parts };
}

// ---- Web MIDI: the FM-1 that answers HELLO
export async function findFM1(access, log = () => {}) {
  const outs = [...access.outputs.values()];
  for (const input of access.inputs.values()) {
    if (input.state === "disconnected") continue;
    if (!/fm-1|felucca|composite|sinco|usb-midi|penumbra/i.test(input.name || "")) continue;   // never other gear
    const output = outs.find((o) => o.name === input.name) || (outs.length === 1 ? outs[0] : null);
    if (!output) continue;
    try { await input.open(); await output.open(); } catch { continue; }
    const link = new Link((b) => { try { output.send(b); return true; } catch { return false; } });
    input.onmidimessage = (ev) => { if (ev.data[0] === 0xF0) link.heard(ev.data); };
    link.close = () => { input.onmidimessage = null; };
    try {
      const h = await hello(link, 800, 2);
      log(`found ${input.name}: Penumbra ${h.version}`);
      return { link, hello: h, name: input.name };
    } catch (e) {
      log(`${input.name}: ${e.message}`);
      link.close();
    }
  }
  return null;
}
export { sleep };
