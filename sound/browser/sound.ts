// Sound for the browser game. One small module: the page asks for a sound by name and this plays it.
//
// The sounds are MP3s written into the page by build.mjs as two globals:
//   window.__TC_SOUNDS      { name: "data:audio/mpeg;base64,..." }
//   window.__TC_SOUND_LIST  the rows of sound/browser-set/browser-sounds.json
// If either is missing (a build without sound, or the rules run under test with no page), every call here does
// nothing and nothing fails.
//
// To check the wiring without ears: set window.__TC_SOUND_LOG = [] before the game starts, and every sound asked for
// is written to it as "name" (or "loop:name", "stop:name"), whether or not the browser will play it.
//
// The rules it keeps, from the sound brief:
//   - A browser will not let a page make a sound until the player has clicked or pressed a key. unlock() is called
//     from the first click; before it, calls are dropped silently.
//   - The same sound is never started more than eight times a second (the UI director's rule), and never more than
//     eight sounds play at once.
//   - Each play is shifted a little in pitch and loudness, so a click heard a hundred times is never quite the same.
//   - A call (the band) cuts the call before it: two tunes never play over each other.
//   - One volume for effects, remembered between visits, and an on/off switch.

type Row = { name: string; bus: 'interface' | 'calls' | 'world' | 'ambience'; gain: number; seconds: number; loop?: { from: number; length: number } };
type Playing = { src: AudioBufferSourceNode; gain: GainNode; name: string; bus: Row['bus'] };

const w = (typeof window === 'undefined' ? {} : window) as unknown as { __TC_SOUNDS?: Record<string, string>; __TC_SOUND_LIST?: Row[]; __TC_SOUND_LOG?: string[]; AudioContext?: typeof AudioContext; webkitAudioContext?: typeof AudioContext };
const data = w.__TC_SOUNDS ?? {};
const rows = new Map<string, Row>((w.__TC_SOUND_LIST ?? []).map((r) => [r.name, r]));
const buffers = new Map<string, AudioBuffer>();
const decoding = new Map<string, Promise<AudioBuffer | null>>();
const lastStart = new Map<string, number>();
const lastVoice = new Map<string, number>();
const loops = new Map<string, Playing>();
let playing: Playing[] = [];
let ctx: AudioContext | null = null;
let master: GainNode | null = null;
let on = true;
let effects = 0.8;
const KEY = 'tc-sound';

try {
  const saved = JSON.parse(localStorage.getItem(KEY) ?? 'null') as { on?: boolean; effects?: number } | null;
  if (saved) { on = saved.on !== false; if (typeof saved.effects === 'number') effects = Math.min(1, Math.max(0, saved.effects)); }
} catch { /* storage is off: the settings last as long as the page */ }
const remember = (): void => { try { localStorage.setItem(KEY, JSON.stringify({ on, effects })); } catch { /* as above */ } };
const db = (d: number): number => Math.pow(10, d / 20);

function decode(name: string): Promise<AudioBuffer | null> {
  const ready = buffers.get(name);
  if (ready) return Promise.resolve(ready);
  let wait = decoding.get(name);
  if (!wait) {
    const uri = data[name];
    if (!uri || !ctx) return Promise.resolve(null);
    const bin = atob(uri.slice(uri.indexOf(',') + 1)), bytes = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) bytes[i] = bin.charCodeAt(i);
    wait = ctx.decodeAudioData(bytes.buffer).then((b) => { buffers.set(name, b); return b; }, () => null);
    decoding.set(name, wait);
  }
  return wait;
}

/** Call from the first click or key press. Safe to call every time. */
export function unlock(): void {
  if (!ctx) {
    const Ctx = w.AudioContext ?? w.webkitAudioContext;
    if (!Ctx || !rows.size) return;
    ctx = new Ctx();
    master = ctx.createGain();
    master.gain.value = on ? effects : 0;
    master.connect(ctx.destination);
    // The desk's sounds are wanted at once, so they are made ready now; the rest are made ready when first asked for.
    for (const r of rows.values()) if (r.bus === 'interface' && r.seconds < 1.5) void decode(r.name);
  }
  if (ctx.state === 'suspended') void ctx.resume();
}

function start(name: string, buffer: AudioBuffer, row: Row, gainDb: number, delay: number, loop: boolean, fade: number): Playing | null {
  if (!ctx || !master) return null;
  const now = ctx.currentTime, at = now + delay;
  const src = ctx.createBufferSource(), gain = ctx.createGain();
  src.buffer = buffer;
  const level = db(row.gain + gainDb) * (loop ? 1 : 0.85 + Math.random() * 0.15);
  if (loop && row.loop) {
    src.loop = true; src.loopStart = row.loop.from; src.loopEnd = row.loop.from + row.loop.length;
    gain.gain.setValueAtTime(0, at); gain.gain.linearRampToValueAtTime(level, at + fade);
    src.connect(gain).connect(master);
    src.start(at, row.loop.from + Math.random() * row.loop.length * 0.9);
  } else {
    if (row.bus !== 'calls') src.playbackRate.value = 0.97 + Math.random() * 0.06;
    gain.gain.value = level;
    src.connect(gain).connect(master);
    src.start(at);
  }
  const p: Playing = { src, gain, name, bus: row.bus };
  src.onended = () => { playing = playing.filter((q) => q !== p); };
  playing.push(p);
  return p;
}
function fadeOut(p: Playing, seconds: number): void {
  if (!ctx) return;
  const now = ctx.currentTime;
  p.gain.gain.cancelScheduledValues(now);
  p.gain.gain.setValueAtTime(p.gain.gain.value, now);
  p.gain.gain.linearRampToValueAtTime(0, now + seconds);
  try { p.src.stop(now + seconds + 0.02); } catch { /* it had already stopped */ }
}

/** Plays a sound once. gain is in dB against the mix; delay is in seconds. */
export function play(name: string, opts: { gain?: number; delay?: number } = {}): void {
  const row = rows.get(name);
  const now = performance.now(), last = lastStart.get(name) ?? -1e9;
  if (!row || now - last < 125) return;
  lastStart.set(name, now);
  w.__TC_SOUND_LOG?.push(name);
  if (!ctx || !on) return;
  void decode(name).then((buffer) => {
    if (!buffer || !ctx) return;
    if (row.bus === 'calls') for (const p of playing) if (p.bus === 'calls') fadeOut(p, 0.25);
    const oneShots = playing.filter((p) => !loops.has(p.name));
    if (oneShots.length >= 8) fadeOut(oneShots[0], 0.05);
    start(name, buffer, row, opts.gain ?? 0, opts.delay ?? 0, false, 0);
  });
}

/** One of a sound's numbered voices ("flak_burst", 4), never the one used last. */
export function voice(base: string, count: number): string {
  const last = lastVoice.get(base) ?? 0;
  let k = 1 + Math.floor(Math.random() * count);
  if (count > 1 && k === last) k = 1 + (k % count);
  lastVoice.set(base, k);
  return `${base}_0${k}`;
}

/** Starts a sound that goes round until stopLoop. Starting one that is already going does nothing. */
export function startLoop(name: string, gainDb = 0, fade = 0.5): void {
  const row = rows.get(name);
  if (!row || !row.loop || loops.has(name)) return;
  w.__TC_SOUND_LOG?.push('loop:' + name);
  if (!ctx || !on) return;
  void decode(name).then((buffer) => {
    if (!buffer || loops.has(name)) return;
    const p = start(name, buffer, row, gainDb, 0, true, fade);
    if (p) loops.set(name, p);
  });
}
export function stopLoop(name: string, fade = 1): void {
  const p = loops.get(name);
  if (rows.has(name)) w.__TC_SOUND_LOG?.push('stop:' + name);
  if (!p) return;
  loops.delete(name);
  fadeOut(p, fade);
}

export function soundOn(): boolean { return on; }
export function effectsVolume(): number { return effects; }
export function setSoundOn(next: boolean): void {
  on = next; remember();
  if (master && ctx) master.gain.setTargetAtTime(on ? effects : 0, ctx.currentTime, 0.05);
  if (!on) for (const name of [...loops.keys()]) stopLoop(name, 0.2);
}
export function setEffectsVolume(v: number): void {
  effects = Math.min(1, Math.max(0, v)); remember();
  if (master && ctx && on) master.gain.setTargetAtTime(effects, ctx.currentTime, 0.05);
}
