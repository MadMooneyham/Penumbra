// SPDX-License-Identifier: GPL-3.0-only
// The Journeys page end to end: its own code (web/journey/journey.js, web/backup/backup.js) against the
// browser build (penumbra.wasm: backup.c's JOURNEY answering over SysEx). A journey made of scenes from two
// presets, ordered, one skipped, one with its own time, named: into user slot 3 byte for byte as the page made
// it; then playing now; its file read back as made; and journeys this Penumbra must not take refused.
//   node tests/host/journey_web_test.mjs build/emu/penumbra.wasm
import fs from "fs";
import * as B from "../../web/backup/backup.js";
import * as J from "../../web/journey/journey.js";

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

const { layout: L, presets } = J.readPresets(fs.readFileSync(new URL("../../web/journey/presets.json", import.meta.url), "utf8"));
const h = await B.hello(link);
check(h.journeyFormat === L.format, `presets.json's format (${L.format}) is this Penumbra's (${h.journeyFormat})`);

// ECLIPSE's sound, its scenes 1 and 3, PENUMBRA's 2 between them
const j = new J.Journey(L, presets[0].bytes), pen = new J.Journey(L, presets[1].bytes);
const e1 = j.scenes[0], e3 = j.scenes[2], p2 = pen.scenes[1];
j.remove(3); j.remove(1);                              // (ECLIPSE 1, 3)
j.add(p2);
j.move(2, 1);                                          // (1, PENUMBRA 2, 3)
j.setStay(0, 3);                                       // (16 bars)
j.setStay(2, -1);                                      // (skipped)
j.travel = 2;
j.name = "my night";
check(j.name === "MY NIGHT" && j.scenes.length === 3 && j.playing() === 2, `the journey: ${j.name}, ${j.scenes.length} scenes, ${j.playing()} playing`);
check(J.sceneKey(L, j.scenes[1]) === J.sceneKey(L, p2) && J.stayName(J.sceneStay(L, j.scenes[0])) === "16 bars", "its scenes: in order, their times");
const m = J.roundMinutes(j, 90);
check(m !== null && Math.abs(m - (16 + 16) * 4 / 90) < 1e-9, `once round at 90 BPM: ${m && m.toFixed(2)} minutes`);
const bytes = j.toBytes();

await B.putJourney(link, 2, bytes);
render(100);
const u = store(9), slot = u && u.slice(2 * L.size, 3 * L.size);
check(!!slot && same(slot, bytes), "into USER 3: byte for byte as the page made it");
const back = new J.Journey(L, slot);
check(back.name === "MY NIGHT" && back.scenes.length === 3 && back.travel === 2 && same(back.scenes[1], p2) &&
      back.scenes[2][L.skip] === 1 && same(back.scenes[0].slice(0, 2), e1.slice(0, 2)) && same(back.scenes[2].slice(0, 2), e3.slice(0, 2)),
      "read back: its name, scenes, TRAVELS, the skipped one");

await B.putJourney(link, B.JPLAY, bytes);
render(200);
ex.web_buttons(1 << 7);                                 // SAVE: the journey playing
render(60);
ex.web_buttons(0);
render(200);
const w = store(8);
check(!!w && new J.Journey(L, w).name === "MY NIGHT" && new J.Journey(L, w).scenes.length === 3, "playing now: MY NIGHT, its 3 scenes");

const f = J.toFile(j), g = J.fromFile(L, f);
check(same(g.toBytes(), bytes) && /^my-night\.penumbra-journey$/.test(J.fileName(j)), `the file: read back as made (${J.fileName(j)})`);

// a 1.11 / 1.12 journey (format 12, 316 bytes): the page widens it, and the FM-1 takes it as it was sent too
const old = bytes.slice(0, J.SIZE_12);
new DataView(old.buffer).setUint16(4, 12, true);
new DataView(old.buffer).setUint16(6, J.SIZE_12, true);
const wide = new J.Journey(L, old);
check(wide.name === "MY NIGHT" && wide.scenes.length === 3 && wide.toBytes().length === L.size, `a 1.12 journey: opened, widened to ${L.size} bytes`);
await B.putJourney(link, 5, old);
render(100);
const u6 = store(9).slice(5 * L.size, 6 * L.size), dv6 = new DataView(u6.buffer);
check(dv6.getUint16(4, true) === L.format && dv6.getUint16(6, true) === L.size && new J.Journey(L, u6).scenes[2][L.skip] === 1,
      "a 1.12 journey sent as it was: into USER 6, format 13, its skipped scene kept");

// what it must refuse
const newer = bytes.slice(); newer[4] = 99;
let r = null;
try { await B.putJourney(link, 4, newer); } catch (e) { r = e.message; }
check(r === B.STATUS[4], `a journey of a newer format: refused (${r})`);
const u2 = store(9);
check(!!u2 && u2.slice(4 * L.size, 5 * L.size).every((b, i) => b === u[4 * L.size + i]), "refused: USER 5 as it was");
r = null;
try { await B.putJourney(link, 9, bytes); } catch (e) { r = e.message; }
check(r === B.STATUS[1], `a slot that is not one: refused (${r})`);
r = null;
try { await B.putJourney(link, 1, bytes.slice(0, 100)); } catch (e) { r = e.message; }
check(r === B.STATUS[4], `a journey cut short: refused (${r})`);

process.exit(fails ? 1 : 0);
