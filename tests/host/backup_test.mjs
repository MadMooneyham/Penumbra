// SPDX-License-Identifier: GPL-3.0-only
// Backups end to end: the Backup page's own code (web/backup/backup.js) against the browser build
// (penumbra.wasm: the whole app, firmware/src/app/backup.c answering over SysEx). A backup read whole, its
// file made and read back; a new unit (nothing saved) restored from it, byte for byte, after starting
// again by itself; and parts this Penumbra must not take (another firmware's settings, a journey from a
// newer format, the panel's calibration) refused with nothing saved.
//   node tests/host/backup_test.mjs build/emu/penumbra.wasm
import fs from "fs";
import * as B from "../../web/backup/backup.js";

const [wasmPath] = process.argv.slice(2);
let mem;
const clock = { clock_time_get: (id, p, out) => { new DataView(mem.buffer).setBigUint64(out, 0n, true); return 0; } };
const stub = new Proxy(clock, { get: (t, k) => (k in t ? t[k] : () => 0) });
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), { wasi_snapshot_preview1: stub });
const ex = instance.exports;
mem = ex.memory;
ex._initialize();
ex.web_boot();

let fails = 0;
const check = (ok, what) => { console.log(`${ok ? "  ok" : "FAIL"} ${what}`); if (!ok) fails++; };
const render = (ms) => { for (let d = 0; d < Math.round(ms * 44.1); d += 128) ex.web_render(128); };
const store = (o) => { const n = ex.web_store_len(o); return n < 0 ? null : new Uint8Array(mem.buffer, ex.web_store(o), n).slice(); };
const same = (a, b) => !!a && !!b && a.length === b.length && a.every((v, i) => v === b[i]);
const link = new B.Link((f) => {
  const b = f.slice(1, -1);
  new Uint8Array(mem.buffer, ex.web_sysex_in(), b.length).set(b);
  ex.web_sysex(b.length);
  const n = ex.web_said_len();
  const heard = new Uint8Array(mem.buffer, ex.web_said_buf(), n).slice();
  ex.web_said_clear();
  setTimeout(() => link.heard(heard), 0);
  return true;
});

render(500);
ex.web_buttons(1 << 7);                           // SAVE: the settings, the journey playing, the loop
render(60);
ex.web_buttons(0);
render(200);
// the user journeys: slot 3 a copy of the journey playing (its size is at bytes 6-7)
const work = store(8), jsize = work[6] | (work[7] << 8);
const user = new Uint8Array(8 * jsize);
user.set(work.slice(0, jsize), 2 * jsize);
new Uint8Array(mem.buffer, ex.web_store(9), user.length).set(user);
ex.web_store_set_len(9, user.length);

const h = await B.hello(link);
check(h.version.length > 0 && h.journeyFormat >= 10, `HELLO: Penumbra ${h.version}, journey format ${h.journeyFormat}`);
check(h.sizes[0] > 0 && h.sizes[8] > 0 && h.sizes[9] === user.length, `what it keeps: ${JSON.stringify(h.sizes)}`);
const got = await B.readAll(link);
const kept = {};
for (const p of B.PARTS) kept[p.obj] = store(p.obj);
check(B.PARTS.every((p) => !kept[p.obj] || same(got.parts[p.obj], kept[p.obj])), "read: every part as the unit keeps it");
check(!(4 in got.parts), "read: not the panel's calibration");

const text = B.toFile(got.hello, got.parts, new Date("2026-11-16T12:00:00Z"));
const back = B.fromFile(text);
check(Object.keys(got.parts).every((o) => same(back.parts[o], got.parts[o])) && back.firmware === h.version, "the file: read back as made");
check(/^penumbra-backup-\d{4}-\d\d-\d\d\.penumbra$/.test(B.fileName()), `the file's name: ${B.fileName()}`);
let bad = false;
try { B.fromFile('{"hello": 1}'); } catch { bad = true; }
check(bad, "a file that is not a backup: refused");

// a new unit: nothing saved
for (let o = 0; o < ex.web_nobj(); o++) ex.web_store_set_len(o, -1);
ex.web_boot();
render(300);
check(store(9) === null, "a new unit: no user journeys");
const r = await B.writeAll(link, back.parts);
check(Object.values(r).every((s) => s === 0), `restore: every part taken (${JSON.stringify(r)})`);
render(100);                                        // (it starts again by itself)
check(B.PARTS.every((p) => !back.parts[p.obj] || same(store(p.obj), back.parts[p.obj])), "restored: every part byte for byte");
render(3000);                                       // (nothing saved over it after it starts again)
check(same(store(8), back.parts[8]) && same(store(0), back.parts[0]), "after starting again: the journey playing and the settings as restored");

// what it must refuse
const set = back.parts[0].slice();
set[0] ^= 0xFF;                                     // (another firmware's settings: another magic)
const jr = back.parts[8].slice();
jr[4] = 99;                                         // (a journey from a newer format)
const r2 = await B.writeAll(link, { 0: set, 8: jr });
check(r2[0] === 4 && r2[8] === 4, `refused: another firmware's settings, a newer journey (${JSON.stringify(r2)})`);
check(!same(store(0), set) && !same(store(8), jr) && store(0)[0] === back.parts[0][0] && store(8)[4] === back.parts[8][4],
      "refused: neither saved");
const p = await link.ask(B.WRITE, [4, 0, 0, 4, 0, 1, 2, 3, 4]);
check(p[0] === 1, "the panel's calibration: not a part");
render(25000);                                      // (nothing restored: it lets go, saves again)

process.exit(fails ? 1 : 0);
