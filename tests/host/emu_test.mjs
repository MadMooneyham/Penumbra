// SPDX-License-Identifier: GPL-3.0-only
// The browser build (penumbra.wasm) against the host simulator (tests/emu/emu.pen): boot, half a second of the
// drone, a VOX note (white key 5), a BLOOM (black key 7), three seconds of it; the audio must match the
// simulator's WAV to its 16-bit rounding, and the screen and the lights must be drawn. From X0X's emu test.
//   node tests/host/emu_test.mjs build/emu/penumbra.wasm HOST.wav
import fs from "fs";
const [wasmPath, wavPath] = process.argv.slice(2);
let mem;
const clock = { clock_time_get: (id, p, out) => { new DataView(mem.buffer).setBigUint64(out, 0n, true); return 0; } };
const stub = new Proxy(clock, { get: (t, k) => (k in t ? t[k] : () => 0) });
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), { wasi_snapshot_preview1: stub });
const ex = instance.exports;
mem = ex.memory;
ex._initialize();
ex.web_boot();
const out = [];
const render = (ms) => {
  const n = Math.round(ms * 44.1);
  for (let d = 0; d < n; d += 128) {
    const k = Math.min(128, n - d);
    ex.web_render(k);
    out.push(...new Float32Array(mem.buffer, ex.web_out_l(), k));
  }
};
// as the host script's tapkey (60 ms held, 30 released): white key 5 (key 7), black key 7 (key 15)
const tapkey = (k) => { ex.web_keys(1 << k); render(60); ex.web_keys(0); render(30); };
render(500);
tapkey(7);
tapkey(15);
render(3000);
const wav = fs.readFileSync(wavPath);
const host = new Int16Array(wav.buffer, wav.byteOffset + 44, (wav.length - 44) >> 1).filter((_, i) => i % 2 === 0);
const n = Math.min(host.length, out.length);
let diff = 0, peak = 0;
for (let i = 0; i < n; i++) { diff = Math.max(diff, Math.abs(host[i] / 32767 - out[i])); peak = Math.max(peak, Math.abs(out[i])); }
const fb = new Uint16Array(mem.buffer, ex.web_fb(), 240 * 240);
const drawn = fb.some((p) => p !== 0), lit = ex.web_lit_keys() !== 0;
console.log(`emu: ${n} samples, peak ${peak.toFixed(3)}, max diff from the host ${diff.toExponential(2)}, screen ${drawn}, lights ${lit}`);
process.exit(n > 160000 && peak > 0.02 && diff < 1e-4 && drawn && lit ? 0 : 1);
