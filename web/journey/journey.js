// SPDX-License-Identifier: GPL-3.0-only
// Penumbra's journey editor: a journey's bytes (journey_t, as the firmware keeps it) read, its scenes
// gathered from any journey, put in order, each given how long it stays or skipped, and the bytes made
// again. Where the fields are comes from presets.json (host/penumbra_host --presets: the tests check it is
// this firmware's); only the scenes, their order and count, TRAVELS and the name change here. Everything
// else (the sound, the drone, PAN, the kick) stays the journey it started from.
// Used by the Journeys page (index.html) and tests/host/journey_web_test.mjs (node, against the browser build).

export const ROOTS = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
export const SCALES = ["IONIAN", "DORIAN", "PHRYGIAN", "LYDIAN", "MIXOLYDIAN", "AEOLIAN", "HARM MINOR"];
export const STAY_BARS = [0, 4, 8, 16, 32, 64, 128];      // a scene's STAYS (0: as TRAVELS)
export const TRAVEL_BARS = [0, 8, 16, 32, 64];            // the journey's TRAVELS (0: by hand)

export const fromHex = (h) => Uint8Array.from(h.match(/../g) || [], (x) => parseInt(x, 16));
export const toHex = (u8) => Array.from(u8, (b) => b.toString(16).padStart(2, "0")).join("");

// the layout and the presets: presets.json, parsed
export function readPresets(json) {
  const p = typeof json === "string" ? JSON.parse(json) : json;
  if (!p || !p.at || !Array.isArray(p.presets)) throw new Error("presets.json: not the journey editor's");
  return { layout: { format: p.format, size: p.size, max: p.scenes_max, nameLen: p.name_len, ...p.at },
           presets: p.presets.map((x) => ({ name: x.name, bytes: fromHex(x.hex) })) };
}

const MAGIC = 0x314A4E50;                                  // "PNJ1"
export function isJourney(L, b) {
  if (!b || b.length < L.size) return false;
  const dv = new DataView(b.buffer, b.byteOffset, b.length);
  return dv.getUint32(0, true) === MAGIC && dv.getUint16(4, true) === L.format && dv.getUint16(6, true) === L.size;
}

// a scene: its 16 bytes, and what it is
export function sceneKey(L, s) { return `${ROOTS[s[L.root] % 12]} ${SCALES[s[L.scale] % SCALES.length]}`; }
export function sceneStay(L, s) { return s[L.skip] ? -1 : (s[L.stays] < STAY_BARS.length ? s[L.stays] : 0); }   // -1 skipped
export function stayName(v) { return v < 0 ? "Skipped" : v === 0 ? "As TRAVELS" : `${STAY_BARS[v]} bars`; }

export class Journey {
  constructor(L, bytes) {
    if (!isJourney(L, bytes)) throw new Error("Not a journey this editor knows (another Penumbra's format?)");
    this.L = L;
    this.bytes = Uint8Array.from(bytes.slice(0, L.size));
    const n = Math.max(1, Math.min(L.max, this.bytes[L.nscenes]));
    this.scenes = [];
    for (let i = 0; i < n; i++) this.scenes.push(this.sceneAt(i));
  }
  sceneAt(i) { const o = this.L.scene + i * this.L.scene_size; return this.bytes.slice(o, o + this.L.scene_size); }
  get name() {
    const a = this.bytes.slice(this.L.name, this.L.name + this.L.nameLen);
    return String.fromCharCode(...a).replace(/\0.*$/s, "").trim();
  }
  set name(v) {
    const s = cleanName(v, this.L.nameLen - 1);
    for (let i = 0; i < this.L.nameLen; i++) this.bytes[this.L.name + i] = i < s.length ? s.charCodeAt(i) : 0;
  }
  get travel() { return this.bytes[this.L.travel] % TRAVEL_BARS.length; }
  set travel(v) { this.bytes[this.L.travel] = Math.max(0, Math.min(TRAVEL_BARS.length - 1, v | 0)); }
  // the scene list's edits
  add(scene) { if (this.scenes.length >= this.L.max) return false; const s = Uint8Array.from(scene); s[this.L.skip] = 0; this.scenes.push(s); return true; }
  remove(i) { if (this.scenes.length <= 1) return false; this.scenes.splice(i, 1); return true; }
  move(from, to) {
    if (from === to || from < 0 || to < 0 || from >= this.scenes.length || to >= this.scenes.length) return false;
    const [s] = this.scenes.splice(from, 1);
    this.scenes.splice(to, 0, s);
    return true;
  }
  setStay(i, v) {                                          // -1 skipped, 0 as TRAVELS, 1.. STAY_BARS's
    const s = this.scenes[i], L = this.L;
    if (v < 0) { s[L.skip] = 1; s[L.stays] = 0; } else { s[L.skip] = 0; s[L.stays] = Math.min(STAY_BARS.length - 1, v); }
  }
  playing() { return this.scenes.filter((s) => !s[this.L.skip]).length; }
  // the bytes, as the firmware takes them (it starts at the first scene)
  toBytes() {
    const b = Uint8Array.from(this.bytes), L = this.L, z = new Uint8Array(L.scene_size);
    b[L.nscenes] = this.scenes.length;
    b[L.nscenes + 2] = 0;                                  // (at: the scene it is in)
    for (let i = 0; i < L.max; i++) b.set(i < this.scenes.length ? this.scenes[i] : z, L.scene + i * L.scene_size);
    return b;
  }
}
export function cleanName(v, max = 11) {
  return String(v || "").toUpperCase().replace(/[^ -Z]/g, " ").replace(/\s+/g, " ").trimStart().slice(0, max);
}

// how long a journey travelling by itself takes, once round, at a tempo (4/4): null if it never moves on by itself
export function roundMinutes(j, bpm) {
  let bars = 0;
  for (const s of j.scenes) {
    if (s[j.L.skip]) continue;
    const v = s[j.L.stays] < STAY_BARS.length ? s[j.L.stays] : 0, b = v ? STAY_BARS[v] : TRAVEL_BARS[j.travel];
    if (!b) return null;
    bars += b;
  }
  return bars ? (bars * 4 * 60) / bpm / 60 : null;
}

// ---- the file: one journey, readable JSON
export function toFile(j) {
  return JSON.stringify({ what: "A Penumbra journey (madmooneyham.github.io/Penumbra/journey/)", penumbra_journey: 1,
                          format: j.L.format, name: j.name, hex: toHex(j.toBytes()) }, null, 2) + "\n";
}
export function fromFile(L, text) {
  let o;
  try { o = JSON.parse(text); } catch { throw new Error("This is not a Penumbra journey (not JSON)."); }
  if (!o || o.penumbra_journey !== 1 || typeof o.hex !== "string") throw new Error("This is not a Penumbra journey.");
  return new Journey(L, fromHex(o.hex));
}
export function fileName(j) { return `${(j.name || "journey").toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "") || "journey"}.penumbra-journey`; }
