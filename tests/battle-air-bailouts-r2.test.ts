// The looks of a fall, round 1 (release 2, behind its own switch 'bailouts', off as shipped). The owner, 26 November
// 2026: "if a fighter gets shot down the pilot can bail out and there will be a parachute"; "for a bomber have the max
// number of people on the plane and then have a certain amount bail out depending on the damage and where it was hit".
// The design director's rulings of the same day: looks first; 'crew' on every fall; "The look shows a parachute for
// every man who got out, whether or not he flies again; whether he flies again stays the count's (back)"; one
// getting-out rule across every way down, reading height; crews by kit, never the calendar; and her answers D1, D3, D4,
// D8, D9, D10 and D11 (design DESIGN-LOOKS-1-FINAL.md, section 10).
//
// Looks only. With the switch off every battle of the set below is byte for byte what the head 78fd87433 gave (every
// tick's view(), history(), outcome() and over.lines, hashed: BASELINE). With it on, the count is untouched battle by
// battle, the rules' streams, the pilots-back stream and the look stream throw the same dice, and only who is seen to
// get out, and the new 'crew', differ. The figures the tests set (out.glide 0.5, 0.95) are test figures, not rulings.
import { describe, expect, it } from 'vitest';
import { planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { syncWings } from '../src/campaign/carriers';
import { createGame, setWar } from '../src/campaign/game';
import { sail, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import type { FieldEvent } from '../src/battle/core/types';
import { SKY } from '../src/battle/air/book';
import { crewOf, fateOf, freshMemory, GOT, HOWS, type Scene } from '../src/battle/air/fates';
import { bomberGuns } from '../src/battle/air/guns';
import { airBattle, airLooks, AirSim, MORE_LOOKS } from '../src/battle/air/sim';
import { airTicket, raidGroups } from '../src/battle/air/ticket';
import type { Fall } from '../src/battle/air/types';
import { Rng } from '../src/core/rng';
import { digest, fly, runs as pilotRuns, type Run, type Ticket } from './battle-air-pilot-raids';

const WARS: Record<string, string[]> = { GER: ['UK', 'FRA'], UK: ['GER'], FRA: ['GER'] };

/** As tests/battle-air-fates.test.ts raid(): the staff's raid on a named province with every fighter group that can go by day. */
function raid(nat: string, kind: RaidKind, name: string, turn = 0): Ticket {
  const g: Game = startOpen(createGame(7, nat));
  for (const w of WARS[nat]) setWar(g, nat, w);
  if (turn) g.turn = turn;
  const t = raidTargets(g, kind, nat).find((x) => planRaid(g, kind, x.id, undefined, nat).place === name);
  if (!t) throw new Error(`no ${kind} raid on ${name} for ${nat}`);
  const day = planRaid(g, kind, t.id, undefined, nat);
  const ids = [...day.picked, ...day.seats.filter((s) => !s.why && !s.picked && s.kind === 'ftr').map((s) => s.id)];
  return airTicket(g, kind, t.id, ids, nat).ticket!;
}

/** As tests/battle-air-single-engine.test.ts ticketOf(k): the k-th German raid on Alsace (the He 111 of 1939), its bombers flown as 'model'. */
function alsaceOf(k: number, model?: string): Ticket {
  const g = startOpen(createGame(7, 'GER'));
  setWar(g, 'GER', 'UK'); setWar(g, 'GER', 'FRA');
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  const t = raidTargets(g, 'bomb', 'GER').find((x) => planRaid(g, 'bomb', x.id, undefined, 'GER').place === 'Alsace')!;
  const tk = structuredClone(airTicket(g, 'bomb', t.id, raidGroups(g, 'bomb', t.id, 'GER'), 'GER').ticket!);
  if (model) tk.place.models!.GER.bmr = model;
  return tk;
}

/** As tests/battle-air-ships.test.ts britain(): a grand campaign with Britain at the table, played until Britain is at war with Germany. */
function britain(seed = 3): Game {
  const g = startOpen(createGame(seed, 'UK'));
  for (let m = 0; m < 12; m++) {
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    beginHuman(g);
    if (g.nations.UK.atWar.length && g.nations.GER.atWar.includes('UK')) return g;
    endHuman(g);
  }
  throw new Error('Britain never went to war');
}
const named = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;
/** As tests/battle-air-ships.test.ts BASE: the Home Fleet's strike on the Battle Fleet's harbour, and on the Battle Fleet at sea with two carriers of its own. The Fleet Air Arm's fighter there is the Fulmar. */
function strikes(): Record<'harbour' | 'carriers', Ticket> {
  const harbour = britain(), carriers = britain();
  {
    const g = harbour, home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
    home.ships.cv = 6;
    sail(g, foe, foe.home, 'port');
    const z = g.seas.find((s) => s.coast.includes(foe.base))!;
    home.zone = z.id; home.stance = 'patrol'; syncWings(g);
  }
  {
    const g = carriers, home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
    home.ships.cv = 6; foe.ships.cv = 2;
    for (const f of [home, foe]) { f.zone = zoneOf(g, 'natl'); f.stance = 'patrol'; f.lane = undefined; }
    syncWings(g);
  }
  const out = {} as Record<'harbour' | 'carriers', Ticket>;
  for (const [key, g0] of [['harbour', harbour], ['carriers', carriers]] as const) {
    const g = loadGame(saveGame(g0))!, kind = key === 'harbour' ? 'harbour' : 'fleet';
    g.seed = 7; g.flags.airBattles = 0;
    const foe = named(g, 'Battle Fleet');
    const target = kind === 'harbour' ? foe.base : foe.id;
    const { ticket, why } = airTicket(g, kind, target, raidGroups(g, kind, target, 'UK'), 'UK', { ships: true });
    if (!ticket) throw new Error(why ?? 'no ticket');
    out[key] = ticket;
  }
  return out;
}

/**
 * The set: the four raids of tests/battle-air-fates.test.ts at seeds +0 to +7 (both sides, sea and land, every band);
 * the 25 runs of tests/battle-air-pilot-raids.ts; the fates test's B-17 clone, as dated (1939) and in August 1944 with
 * its kit remade (the B-17G); the eight Alsace raids of tests/battle-air-single-engine.test.ts (the He 111P of 1939);
 * Kent in September 1940 (the He 111H); a night raid of Lancasters; the Home Fleet's harbour raid and fleet strike
 * (the Fulmar). Stragglers fall in the B-17 boxes.
 */
interface Battle { key: string; run: Run }
const RAIDS: [string, RaidKind, string, number][] = [['GER', 'bomb', 'Alsace', 0], ['GER', 'bomb', 'Kent', 0], ['FRA', 'bomb', 'Rhineland', 30], ['UK', 'bomb', 'Ruhr', 30]];
const TK = RAIDS.map(([nat, kind, name, turn]) => raid(nat, kind, name, turn));
const at = (key: string, tk: Ticket, k: number): Battle => ({ key: `${key}:${k}`, run: { key: '', ticket: { ...tk, seed: tk.seed + k }, play: 'ai' } });
const B17 = (() => { const t = structuredClone(TK[0]); for (const nat of Object.keys(t.place.models ?? {})) t.place.models![nat].bmr = 'B17'; return t; })();
const B44 = (() => {
  const t = structuredClone(B17);
  t.place.when = { year: 1944, month: 7 };
  for (const nat of Object.keys(t.place.guns ?? {})) t.place.guns![nat].bmr = bomberGuns('B17', nat, t.place.when);
  return t;
})();
const KENT40 = raid('GER', 'bomb', 'Kent', 14);
const DARK = (() => { const t = structuredClone(TK[0]); t.place.night = true; t.place.models!.GER.bmr = 'Lancaster'; return t; })();
const SHIPS = strikes();
const SET: Battle[] = [
  ...RAIDS.flatMap(([nat, , name], i) => Array.from({ length: 8 }, (_, k) => at(`${nat}-${name}`, TK[i], k))),
  ...pilotRuns().map((r) => ({ key: `pilot-${r.key}`, run: r })),
  ...[0, 1, 2].flatMap((k) => [at('b17-1939', B17, k), at('b17-1944', B44, k)]),
  ...Array.from({ length: 8 }, (_, k) => ({ key: `alsace-he111p:${k}`, run: { key: '', ticket: alsaceOf(k), play: 'ai' as const } })),
  ...[0, 1, 2].map((k) => at('kent-1940-he111h', KENT40, k)),
  ...[0, 1].map((k) => at('night', DARK, k)),
  ...[0, 1].flatMap((k) => [at('harbour', SHIPS.harbour, k), at('carriers', SHIPS.carriers, k)]),
];

/** What the head 78fd87433 gave for each battle of the set, switch off: FNV-1a over every tick's view(), then history(), outcome() and over.lines (step 0, before any change). */
const BASELINE: Record<string, number> = {
  'GER-Alsace:0': 2225686532, 'GER-Alsace:1': 225243303, 'GER-Alsace:2': 2335731105, 'GER-Alsace:3': 4219053814,
  'GER-Alsace:4': 2253648796, 'GER-Alsace:5': 1975599040, 'GER-Alsace:6': 4182902111, 'GER-Alsace:7': 377184919,
  'GER-Kent:0': 3148912142, 'GER-Kent:1': 891888083, 'GER-Kent:2': 657927157, 'GER-Kent:3': 4249233553,
  'GER-Kent:4': 4048561157, 'GER-Kent:5': 3501605111, 'GER-Kent:6': 447398267, 'GER-Kent:7': 1407443564,
  'FRA-Rhineland:0': 2021256632, 'FRA-Rhineland:1': 3001855544, 'FRA-Rhineland:2': 43399771, 'FRA-Rhineland:3': 720335385,
  'FRA-Rhineland:4': 3399896426, 'FRA-Rhineland:5': 1493310594, 'FRA-Rhineland:6': 3629038974, 'FRA-Rhineland:7': 2738995837,
  'UK-Ruhr:0': 3464559669, 'UK-Ruhr:1': 4250325863, 'UK-Ruhr:2': 1626790790, 'UK-Ruhr:3': 3071090043,
  'UK-Ruhr:4': 2349952649, 'UK-Ruhr:5': 1333933345, 'UK-Ruhr:6': 204351070, 'UK-Ruhr:7': 1860839074,
  'pilot-kent:0': 3148912142, 'pilot-alsace:0': 2225686532, 'pilot-twins:0': 2596175145, 'pilot-order:0': 3655155787,
  'pilot-sky:0': 1043205276, 'pilot-kent:1': 891888083, 'pilot-alsace:1': 225243303, 'pilot-twins:1': 480628126,
  'pilot-order:1': 979056622, 'pilot-sky:1': 3433876588, 'pilot-kent:2': 657927157, 'pilot-alsace:2': 2335731105,
  'pilot-twins:2': 1054127372, 'pilot-order:2': 1609556579, 'pilot-sky:2': 3575859363, 'pilot-kent:3': 4249233553,
  'pilot-alsace:3': 4219053814, 'pilot-twins:3': 1653650169, 'pilot-order:3': 182397397, 'pilot-sky:3': 2226749047,
  'pilot-kent:4': 4048561157, 'pilot-alsace:4': 2253648796, 'pilot-twins:4': 480984002, 'pilot-order:4': 2581192927,
  'pilot-sky:4': 1467047576, 'b17-1939:0': 3179033158, 'b17-1944:0': 3652871405, 'b17-1939:1': 2102885015,
  'b17-1944:1': 1532733166, 'b17-1939:2': 3975577571, 'b17-1944:2': 1701481549, 'alsace-he111p:0': 2225686532,
  'alsace-he111p:1': 2140052530, 'alsace-he111p:2': 270920006, 'alsace-he111p:3': 3898253096, 'alsace-he111p:4': 2840937688,
  'alsace-he111p:5': 2027141903, 'alsace-he111p:6': 3021589, 'alsace-he111p:7': 2543374693, 'kent-1940-he111h:0': 462132052,
  'kent-1940-he111h:1': 3001818742, 'kent-1940-he111h:2': 4214231639, 'night:0': 2069444584, 'night:1': 1238753718,
  'harbour:0': 3921217764, 'carriers:0': 1169743217, 'harbour:1': 1881678120, 'carriers:1': 2407036934,
};

/** FNV-1a over a string, carried on from `h`. */
function fnv(str: string, h = 2166136261): number {
  for (let i = 0; i < str.length; i++) { h ^= str.charCodeAt(i); h = Math.imul(h, 16777619); }
  return h >>> 0;
}
/** The whole battle, hashed: every tick's view(), then history(), outcome() and over.lines. */
function whole(b: Battle): { hash: number; views: number; sim: AirSim } {
  let h = 2166136261, views = 0;
  const sim = fly(b.run, (s) => { h = fnv(JSON.stringify(s.view()), h); views++; });
  h = fnv(JSON.stringify(sim.history()), h);
  h = fnv(JSON.stringify(sim.outcome()), h);
  h = fnv(JSON.stringify(sim.over!.lines), h);
  return { hash: h, views, sim };
}

/** Flies with the switch as given (set the bridge's way, TC.airLooks: airLooks), and puts it back off. */
function flown(b: Battle, on: boolean, each?: (s: AirSim) => void): AirSim {
  airLooks(on ? ['bailouts'] : []);
  try { return fly(b.run, each); } finally { airLooks([]); }
}

// The dice counter (the design's section 8, proof 1): every draw goes through next (rng.ts), so each stream's count
// of draws and a hash of what they were, by its fork path from the battle's own seed stream ('root').
type Tally = Map<string, { n: number; h: number }>;
function counted<T>(go: () => T): { out: T; tally: Tally } {
  const tagOf = new WeakMap<Rng, string>(), tally: Tally = new Map();
  const next0 = Rng.prototype.next, fork0 = Rng.prototype.fork;
  Rng.prototype.next = function (this: Rng) {
    const v = next0.call(this), t = tagOf.get(this) ?? 'root';
    const r = tally.get(t) ?? { n: 0, h: 2166136261 };
    r.n++; r.h = Math.imul(r.h ^ Math.floor(v * 4294967296), 16777619) >>> 0;
    tally.set(t, r);
    return v;
  };
  Rng.prototype.fork = function (this: Rng, label: number) { const c = fork0.call(this, label); tagOf.set(c, `${tagOf.get(this) ?? 'root'}/${label}`); return c; };
  try { return { out: go(), tally }; } finally { Rng.prototype.next = next0; Rng.prototype.fork = fork0; }
}

type Lost = Fall & { air: 'ftr' | 'bmr'; by?: number | 'flak' | null; strag?: number; how: NonNullable<Fall['how']>; dur: number };
const lostOf = (h: FieldEvent[]) => h.filter((e) => e.what === 'air:lost');
const fallOf = (e: FieldEvent) => e.extra as unknown as Lost;
/** A history without who is seen to get out: on every 'air:lost' the text blanked and chutes, chuteAt, chuteCue, out, captured and crew taken out (proof 3). */
const stripped = (h: FieldEvent[]) => h.map((e) => {
  if (e.what !== 'air:lost') return e;
  const x = { ...(e.extra ?? {}) } as Record<string, unknown>;
  for (const k of ['chutes', 'chuteAt', 'chuteCue', 'out', 'captured', 'crew']) delete x[k];
  return { ...e, text: '', extra: x };
});

/** One battle flown off and on, counted, and kept small: what the tests below read. */
interface Pair {
  key: string;
  off: { outcome: unknown; lines: string[]; units: [number, number, number][]; lost: FieldEvent[]; strip: number; digest: string; tally: Tally };
  on: { outcome: unknown; lines: string[]; units: [number, number, number][]; lost: FieldEvent[]; strip: number; digest: string; tally: Tally };
  models: Record<number, string>;
  nat: Record<number, string>;
  when: { year: number; month: number };
  /** Every bomber model flown: whether its crew by kit is its crew by date. */
  sameCrew: boolean;
}
function keep(s: AirSim, tally: Tally): Pair['off'] {
  const h = s.history();
  return {
    outcome: s.outcome(), lines: [...s.over!.lines], units: s.units.map((u) => [u.id, u.n, u.back]),
    lost: lostOf(h), strip: fnv(JSON.stringify(stripped(h))), digest: digest(s), tally,
  };
}
function pairOf(b: Battle): Pair {
  const off = counted(() => flown(b, false)), on = counted(() => flown(b, true));
  const s = off.out, when = b.run.ticket.place.when ?? { year: 1939, month: 8 };
  const sameCrew = s.units.filter((u) => u.air === 'bmr').every((u) => {
    const kit = (b.run.ticket.place.guns?.[u.nat]?.bmr ?? bomberGuns(u.model, u.nat, when)).mark;
    return crewOf(u.model, when, kit) === crewOf(u.model, when);
  });
  return {
    key: b.key, off: keep(off.out, off.tally), on: keep(on.out, on.tally),
    models: Object.fromEntries(s.units.map((u) => [u.id, u.model])), nat: Object.fromEntries(s.units.map((u) => [u.id, u.nat])), when, sameCrew,
  };
}
const pairs = new Map<string, Pair>();
const pairFor = (b: Battle): Pair => { if (!pairs.has(b.key)) pairs.set(b.key, pairOf(b)); return pairs.get(b.key)!; };
const allPairs = (): Pair[] => SET.map(pairFor);
const CHUNK = 10;
const chunks: Battle[][] = Array.from({ length: Math.ceil(SET.length / CHUNK) }, (_, i) => SET.slice(i * CHUNK, (i + 1) * CHUNK));

/** A scene to draw from (as tests/battle-air-fates.test.ts scene()), here with the switch on. */
const scene = (o: Partial<Scene> = {}): Scene => ({
  air: 'bmr', engines: 2, nat: 'USA', model: 'Bomber:USA', when: { year: 1942, month: 6 }, gun: 'hmg', flak: null, pass: null,
  band: 1, alt: 4_500, loaded: false, strag: null, sea: false, night: false, back: false, over: 'enemy', crew: 5, near: null, bailouts: true, ...o,
});
const fighter = (o: Partial<Scene> = {}): Scene => scene({ air: 'ftr', engines: 1, nat: 'UK', model: 'Hurricane', gun: 'cannon20', crew: 1, ...o });
/** Book figures a test sets, put back after it. */
function withBook<T>(set: () => void, go: () => T): T {
  const was = { glide: SKY.fate.out.glide, base: structuredClone(SKY.fate.base), crews: SKY.fall.crews, dated: SKY.fall.crewsDated, byKit: SKY.fall.crewsByKit };
  try { set(); return go(); } finally {
    SKY.fate.out.glide = was.glide; SKY.fate.base.ftr = was.base.ftr; SKY.fate.base.bmr = was.base.bmr;
    SKY.fall.crews = was.crews; SKY.fall.crewsDated = was.dated; SKY.fall.crewsByKit = was.byKit;
  }
}

describe('the switch: off as shipped, set the flyKinds way', () => {
  it('is off unless asked: airLooks answers none, a battle opens with it off, and no air:lost carries crew', () => {
    expect(MORE_LOOKS).toEqual(['bailouts']);
    expect(airLooks()).toEqual([]);
    expect(airBattle(TK[0]).bailouts).toBe(false);
    expect(airLooks(['bailouts', 'nonsense'])).toEqual(['bailouts']);
    expect(new AirSim(TK[0]).bailouts).toBe(true);
    expect(airLooks([])).toEqual([]);
    expect(airBattle(TK[0], { bailouts: true }).bailouts).toBe(true);
    (globalThis as { TC_LOOKS?: string }).TC_LOOKS = 'bailouts';
    try { expect(airLooks()).toEqual(['bailouts']); expect(airBattle(TK[0]).bailouts).toBe(true); } finally { delete (globalThis as { TC_LOOKS?: string }).TC_LOOKS; }
    expect(airLooks()).toEqual([]);
    const s = flown(SET[0], false);
    expect(lostOf(s.history()).length).toBeGreaterThan(0);
    for (const e of lostOf(s.history())) expect('crew' in (e.extra ?? {})).toBe(false);
  });
  it('a battle keeps its switch to the end: it is read once, when the battle opens', () => {
    airLooks(['bailouts']);
    let s: AirSim;
    try { s = airBattle(TK[0]); } finally { airLooks([]); }
    s.ai.att = true; s.ai.def = true;
    while (!s.over) s.step();
    expect(s.bailouts).toBe(true);
    for (const e of lostOf(s.history())) expect(typeof fallOf(e).crew).toBe('number');
  });
});

describe('(a) switch off: every battle of the set is byte for byte what the head 78fd87433 gave', () => {
  it('the set is the 80 battles the baseline holds', () => {
    expect(SET.map((b) => b.key)).toEqual(Object.keys(BASELINE));
  });
  it.each(chunks.map((c, i) => [i, c] as const))('battles chunk %i: every tick\'s view(), history(), outcome() and over.lines hash to the baseline', (_i, c) => {
    for (const b of c) {
      const w = whole(b);
      expect(w.hash, b.key).toBe(BASELINE[b.key]);
      expect(w.views).toBeGreaterThan(100);
    }
  });
});

describe('(b) switch on: the count is untouched, battle by battle', () => {
  it.each(chunks.map((c, i) => [i, c] as const))('battles chunk %i: outcome(), every group\'s n and back, over.lines, every loss\'s time, unit, side, back and by, and the pilot raids\' digest, equal to switch off', (_i, c) => {
    for (const b of c) {
      const p = pairFor(b);
      expect(p.on.outcome, b.key).toEqual(p.off.outcome);
      expect(p.on.units, b.key).toEqual(p.off.units);
      expect(p.on.lines, b.key).toEqual(p.off.lines);
      const when = (l: FieldEvent[]) => l.map((e) => [e.t, e.unit, e.side, fallOf(e).back, fallOf(e).by]);
      expect(when(p.on.lost), b.key).toEqual(when(p.off.lost));
      expect(p.on.digest, b.key).toBe(p.off.digest);
      // And everything else (the design's proof 3): the history with who got out taken off every loss, equal.
      expect(p.on.strip, b.key).toBe(p.off.strip);
    }
  });
});

describe('(c) switch on: the rules\' streams, the pilots-back stream and the look stream throw the same dice', () => {
  /**
   * Equal in count and hash, switch on against off: 'root/41' (the look stream) and every stream not under 'root/41/'
   * (the main stream, fork 7 for the pilots back, 11, 19, 23, 43, 47, 59, 61, 73, ...). 'root/41/53' (a bomber's sixth
   * man and after, looks only) only where every bomber model flown has the same crew by kit as by date. Streams under
   * 'root/41/83' only with the switch on.
   */
  function compare(off: Tally, on: Tally, sameCrew: boolean, key: string): number {
    let n = 0;
    const tags = new Set([...off.keys(), ...on.keys()]);
    for (const t of tags) {
      if (t.startsWith('root/41/83')) { expect(off.has(t), `${key} ${t}`).toBe(false); continue; }
      if (t.startsWith('root/41/') && !(t === 'root/41/53' && sameCrew)) continue;
      expect(on.get(t), `${key} ${t}`).toEqual(off.get(t));
      n++;
    }
    expect(off.get('root/7')?.n ?? 0, key).toBeGreaterThan(0);
    expect(off.get('root/41')?.n ?? 0, key).toBeGreaterThan(0);
    return n;
  }
  it.each(chunks.map((c, i) => [i, c] as const))('as shipped, battles chunk %i', (i, c) => {
    let tags = 0;
    for (const b of c) {
      const p = pairFor(b);
      const n = compare(p.off.tally, p.on.tally, p.sameCrew, b.key);
      expect(n).toBeGreaterThan(5);
      tags += n;
    }
    console.log(`dice counter, chunk ${i}: ${c.length} battles, ${tags} streams compared equal`);
  });
  it('the crews differ by kit and by date only in the 1944 B-17s (10 against 9) and the 1940 He 111Hs (5 against 4); 41/53 moves in the B-17s, where a sixth man is drawn', () => {
    expect(SET.filter((b) => !pairFor(b).sameCrew).map((b) => b.key)).toEqual(SET.map((b) => b.key).filter((k) => /^(b17-1944|kent-1940-he111h):/.test(k)));
    const b44 = SET.filter((b) => b.key.startsWith('b17-1944')).map(pairFor);
    expect(b44.some((p) => JSON.stringify(p.on.tally.get('root/41/53')) !== JSON.stringify(p.off.tally.get('root/41/53')))).toBe(true);
    // A He 111 of four or five throws no die past the fifth man: 41/53 equal there too.
    for (const p of SET.filter((b) => b.key.startsWith('kent-1940-he111h')).map(pairFor)) expect(p.on.tally.get('root/41/53')).toEqual(p.off.tally.get('root/41/53'));
  });
  it('fork 83 is drawn on with the switch on (fighters not brought back), never with it off', () => {
    let on = 0;
    for (const p of allPairs()) { on += p.on.tally.get('root/41/83')?.n ?? 0; expect(p.off.tally.has('root/41/83')).toBe(false); }
    expect(on).toBeGreaterThan(1000);
  });
  it.each([[0, chunks.slice(0, 3).flat()], [1, chunks.slice(3, 6).flat()], [2, chunks.slice(6).flat()]] as const)('with the test figure out.glide 0.5 (nobody\'s ruling), part %i: the same streams equal, and the glides\' jumps on 41/83', (_i, bs) => {
    let jumps = 0;
    withBook(() => { SKY.fate.out.glide = 0.5; }, () => {
      for (const b of bs) {
        const p = pairFor(b);
        const on = counted(() => flown(b, true));
        compare(p.off.tally, on.tally, p.sameCrew, b.key);
        for (const e of lostOf(on.out.history())) if (fallOf(e).air === 'bmr' && fallOf(e).how === 'glide' && fallOf(e).chutes > 0) jumps++;
      }
    });
    expect(jumps).toBeGreaterThan(0);
  });
});

describe('(d) on every fall, switch on: never more seen out than are aboard', () => {
  it('crew on every fall, both kinds; chutes + out <= crew; each canopy has its moment; a pilot brought back under his parachute; captured as ever; nothing that blew up shows one', () => {
    let falls = 0, ftr = 0, bmr = 0;
    for (const p of allPairs()) for (const e of p.on.lost) {
      const f = fallOf(e);
      falls++; if (f.air === 'ftr') ftr++; else bmr++;
      expect(typeof f.crew, p.key).toBe('number');
      if (f.air === 'ftr') expect(f.crew, p.models[e.unit!]).toBe(p.models[e.unit!] === 'Fulmar' ? 2 : 1);
      expect(f.chutes + (f.out ?? 0), JSON.stringify(f)).toBeLessThanOrEqual(f.crew!);
      const ts = f.chuteAt ?? [];
      expect(ts.length).toBe(f.chutes);
      expect(ts).toEqual([...ts].sort((a, b) => a - b));
      for (const t of ts) expect(t).toBeLessThanOrEqual(Math.max(0.5, (f.boom ?? f.dur) - 0.5));
      // The pilot brought back is always under his parachute; one parachute on a one-man fighter (a Fulmar's second man may make two).
      if (f.back) { expect(f.chutes).toBeGreaterThanOrEqual(1); if (f.crew === 1) expect(f.chutes).toBe(1); }
      expect(f.captured).toBe(f.back || f.over !== 'enemy' ? 0 : f.chutes + (f.out ?? 0));
      if (f.how === 'explode') { expect(f.chutes).toBe(0); expect(f.chuteAt).toBeUndefined(); }
      if (f.chutes) expect((e.extra as { chuteCue?: string }).chuteCue).toBe(SKY.cues.chutes);
    }
    expect(ftr).toBeGreaterThan(1000);
    expect(bmr).toBeGreaterThan(1000);
    expect(falls).toBe(ftr + bmr);
  });
  it('10,000 draws with the switch on and out.glide at the test figure 0.95: nothing that blew up shows a parachute, and never more out than aboard', () => {
    withBook(() => { SKY.fate.out.glide = 0.95; }, () => {
      const rng = new Rng(83);
      let blew = 0;
      for (let i = 0; i < 10_000; i++) {
        const sc = i % 3 === 0 ? fighter({ over: i % 2 ? 'own' : 'enemy', back: i % 7 === 0, crew: i % 5 === 0 ? 2 : 1, band: (i % 3) as 0 | 1 | 2 }) : scene({ crew: [2, 3, 4, 5, 7, 10][i % 6], band: (i % 3) as 0 | 1 | 2, sea: i % 4 === 0 });
        const f = fateOf(rng, sc, freshMemory());
        expect(f.chutes + (f.out ?? 0)).toBeLessThanOrEqual(sc.crew);
        if (f.how === 'explode') { blew++; expect(f.chutes).toBe(0); expect(f.chuteAt).toBeUndefined(); }
      }
      expect(blew).toBeGreaterThan(300);
    });
  });
});

describe('(e) switch-on twins of the shipped lines (none of which is edited)', () => {
  const when = (year: number, month: number) => ({ year, month });
  it('twin of tests/battle-air-fates.test.ts:586 ([crewOf He111 1940/11, 1941/0] equal [4, 5]): by the kit, [5, 5] for the He 111H; [4, 4] for the He 111P whatever the date', () => {
    expect([crewOf('He111', when(1940, 11), 'He 111H'), crewOf('He111', when(1941, 0), 'He 111H')]).toEqual([5, 5]);
    expect([crewOf('He111', when(1939, 8), 'He 111P'), crewOf('He111', when(1941, 0), 'He 111P')]).toEqual([4, 4]);
    expect(crewOf('He111', when(1944, 0), 'He 111H-16')).toBe(5);
    expect(bomberGuns('He111', 'GER', when(1940, 11)).mark).toBe('He 111H');
    expect(bomberGuns('He111', 'GER', when(1939, 8)).mark).toBe('He 111P');
    // The model guard: a dive bomber flown on a 1939 German ticket (kit 'He 111P') is still two.
    expect(crewOf('D3A', when(1939, 8), 'He 111P')).toBe(2);
  });
  it('twin of tests/battle-air-fates.test.ts:589 (B-17 at Dec 1943, Jun 1944, Jul 1944 equal [10, 10, 9]): each with its mark, [10, 10, 10]', () => {
    const w = [when(1943, 11), when(1944, 5), when(1944, 6)];
    expect(w.map((x) => bomberGuns('B17', 'USA', x).mark)).toEqual(['B-17G', 'B-17G', 'B-17G']);
    expect(w.map((x) => crewOf('B17', x, bomberGuns('B17', 'USA', x).mark))).toEqual([10, 10, 10]);
    for (const m of ['B-17E', 'B-17F', 'B-17G']) expect(crewOf('B17', when(1944, 8), m)).toBe(10);
  });
  it('twin of tests/battle-air-fates.test.ts:594-611 (never more out than aboard; every man can get out; a glide climbs out at most crew, at least crew - ceil(crew/3) + 1): as shipped the same; with the test figure out.glide 0.5 at the high band over land, men jump and the landing keeps whoever stayed', () => {
    for (const crew of [2, 3, 4, 5, 7, 10]) {
      const rng = new Rng(100 + crew);
      let most = 0, mostOut = 0, leastOut = crew;
      for (let i = 0; i < 6_000; i++) {
        const f = fateOf(rng, scene({ crew, band: 2, alt: 7_000 }), freshMemory());
        expect(f.chutes + (f.out ?? 0)).toBeLessThanOrEqual(crew);
        expect((f.chuteAt ?? []).length).toBe(f.chutes);
        most = Math.max(most, f.chutes);
        if (f.how === 'glide') { mostOut = Math.max(mostOut, f.out ?? 0); leastOut = Math.min(leastOut, f.out ?? 0); expect(f.chutes).toBe(0); }
      }
      expect(most, `crew ${crew}`).toBe(crew);
      expect(mostOut, `crew ${crew}`).toBe(crew);
      expect(leastOut, `crew ${crew}`).toBeGreaterThanOrEqual(crew - Math.ceil(crew / 3) + 1);
    }
    withBook(() => { SKY.fate.out.glide = 0.5; }, () => {
      for (const crew of [2, 4, 5, 10]) {
        const rng = new Rng(200 + crew);
        let jumped = 0, glides = 0;
        for (let i = 0; i < 6_000; i++) {
          const f = fateOf(rng, scene({ crew, band: 2, alt: 7_000 }), freshMemory());
          expect(f.chutes + (f.out ?? 0)).toBeLessThanOrEqual(crew);
          expect((f.chuteAt ?? []).length).toBe(f.chutes);
          if (f.how !== 'glide') continue;
          glides++;
          if (f.chutes) jumped++;
          const out = f.out ?? 0;
          expect(out).toBeGreaterThanOrEqual(0);
          expect(out).toBeLessThanOrEqual(crew - f.chutes);
          // Whoever stayed climbs out, less those lost in the landing (today's one die, at most a third short).
          expect(out).toBeGreaterThanOrEqual(Math.max(0, crew - f.chutes - Math.ceil(crew / 3) + 1));
        }
        expect(glides).toBeGreaterThan(100);
        expect(jumped / glides, `crew ${crew}`).toBeGreaterThan(0.5);
      }
      // Over the sea nobody jumps from a glide (hole H-GLIDE-SEA): the crew stays for the ditching.
      const rng = new Rng(5);
      for (let i = 0; i < 3_000; i++) { const f = fateOf(rng, scene({ crew: 5, band: 2, alt: 7_000, sea: true }), freshMemory()); if (f.how === 'glide') expect(f.chutes).toBe(0); }
    });
  });
  it('twin of tests/battle-air-fates.test.ts:615-638 (the crew never moves another look; a smaller crew is the first of the same men): the same with the switch on, as shipped and with out.glide 0.5, where G\'s men are the first of the same men too', () => {
    const less = (f: ReturnType<typeof fateOf>) => { const { chutes: _c, chuteAt: _a, out: _o, ...rest } = f; return rest; };
    for (const glide of [0, 0.5]) withBook(() => { SKY.fate.out.glide = glide; }, () => {
      let bigger = 0;
      for (let seed = 1; seed <= 400; seed++) {
        const fates = [2, 5, 10].map((crew) => {
          const rng = new Rng(seed), mem = freshMemory();
          const three = [0, 1, 2].map(() => fateOf(rng, scene({ crew, band: 2, alt: 7_000, model: 'B17', engines: 4 }), mem));
          return { three, state: rng.state };
        });
        const [two, five, ten] = fates;
        expect(two.state).toBe(five.state);
        expect(ten.state).toBe(five.state);
        for (let i = 0; i < 3; i++) {
          const a = two.three[i], b = five.three[i], c = ten.three[i];
          expect(less(a)).toEqual(less(b));
          expect(less(c)).toEqual(less(b));
          expect(a.chuteAt ?? []).toEqual((b.chuteAt ?? []).slice(0, a.chutes));
          expect((c.chuteAt ?? []).slice(0, b.chutes)).toEqual(b.chuteAt ?? []);
          expect(c.chutes).toBeGreaterThanOrEqual(b.chutes);
          if (c.chutes > 5) bigger++;
        }
      }
      expect(bigger).toBeGreaterThan(20);
    });
  });
  it('twin of tests/battle-air-fates.test.ts:245-252 (a pilot the rules save gets out under one parachute, from a way he can get out of): the same with the switch on, either ground', () => {
    const rng = new Rng(9);
    for (let i = 0; i < 500; i++) {
      const f = fateOf(rng, fighter({ back: true, over: i % 2 ? 'own' : 'enemy' }), freshMemory());
      expect(SKY.fate.saved as readonly string[]).toContain(f.how);
      expect(f.chutes).toBe(1);
    }
  });
  it('twin of tests/battle-air-fates.test.ts:111-124 (a saved pilot always under a parachute, never blown up; each canopy before the end): the same on the fates raids with the switch on, the Fulmar\'s second man apart', () => {
    for (const p of SET.filter((b) => /^(GER|FRA|UK)-/.test(b.key)).map(pairFor)) for (const e of p.on.lost) {
      const f = fallOf(e);
      expect(f.how).toBeDefined();
      expect(f.dur).toBeGreaterThan(0);
      if (f.back) { expect((SKY.fate.saved as readonly string[]).includes(f.how) || f.how === 'collide').toBe(true); expect(f.chutes).toBe(f.crew === 1 ? 1 : f.chutes); expect(f.chutes).toBeGreaterThanOrEqual(1); }
      if (f.how === 'explode') { expect(f.end).toBe('air'); expect(f.chutes).toBe(0); }
      if (f.ditch) { expect(f.how).toBe('glide'); expect(f.end).toBe('sea'); }
      for (const t of f.chuteAt ?? []) expect(t).toBeLessThanOrEqual(f.boom ?? f.dur);
      expect((f.chuteAt ?? []).length).toBe(f.chutes);
      if (f.how !== 'collide') expect(e.text.length).toBeGreaterThan(10);
    }
  });
  it('twin of tests/battle-air.test.ts:313-333 (one fall per aircraft lost; the pilots back are the ones under a parachute; a bomber at most SKY.fall.crew; over by side): the same on the Alsace raid with the switch on (a 1939 He 111P by kit: crew 4)', () => {
    let falls = 0;
    for (const p of SET.filter((b) => b.key.startsWith('GER-Alsace:')).slice(0, 6).map(pairFor)) {
      const out = p.on.outcome as { groups: { lostFtr: number; lostBmr: number; back: number }[] };
      falls += p.on.lost.length;
      expect(p.on.lost.length).toBe(out.groups.reduce((n, g) => n + g.lostFtr + g.lostBmr, 0));
      expect(p.on.lost.filter((e) => fallOf(e).back).length).toBe(out.groups.reduce((n, g) => n + g.back, 0));
      for (const e of p.on.lost) {
        const x = fallOf(e);
        expect(['explode', 'burn', 'spin']).toContain(x.kind);
        if (x.back) { expect(x.kind).not.toBe('explode'); expect(x.chutes).toBeGreaterThan(0); }
        if (x.air === 'bmr') { expect(x.chutes).toBeLessThanOrEqual(SKY.fall.crew); expect(x.crew).toBe(4); }
        expect(x.over).toBe(e.side === 'att' ? 'enemy' : 'own');
      }
    }
    expect(falls).toBeGreaterThan(50);
  });
  it('twin of tests/battle-air-fates.test.ts:640-679 (whatever the crews, the counts and every other look are the same; strip list at :647): the strip list takes crew too, and crewsByKit is set and put back beside crews and crewsDated', () => {
    const b17 = structuredClone(TK[0]);
    for (const nat of Object.keys(b17.place.models ?? {})) b17.place.models![nat].bmr = 'B17';
    const strip = (s: AirSim) => s.history().map((e) => {
      const x = { ...(e.extra ?? {}) } as Record<string, unknown>;
      const lost = e.what === 'air:lost';
      if (lost) for (const k of ['chutes', 'chuteAt', 'chuteCue', 'out', 'captured', 'crew']) delete x[k];
      return { ...e, text: lost ? '' : e.text, extra: x };
    });
    const go = (k: number) => { const s = airBattle({ ...b17, seed: b17.seed + k }, { show: true, bailouts: true }); s.ai.att = true; s.ai.def = true; while (!s.over) s.step(); return s; };
    const seenOut = (s: AirSim) => lostOf(s.history()).filter((e) => fallOf(e).air === 'bmr').map((e) => fallOf(e).chutes + (fallOf(e).out ?? 0));
    let most = 0, lines = 0;
    for (let k = 0; k < 6; k++) {
      const real = go(k);
      for (const e of lostOf(real.history())) {
        const f = fallOf(e);
        if (f.air !== 'bmr') continue;
        expect(f.crew).toBe(10);
        expect(f.chutes + (f.out ?? 0)).toBeLessThanOrEqual(10);
        most = Math.max(most, f.chutes + (f.out ?? 0));
        if (f.chutes > 5 && f.how !== 'collide') { lines++; expect(e.text).toContain(`, ${f.chutes} parachutes`); }
      }
      const ones = withBook(() => { SKY.fall.crews = { Bomber: 1 }; SKY.fall.crewsDated = []; SKY.fall.crewsByKit = []; }, () => go(k));
      const twelves = withBook(() => { SKY.fall.crews = { Bomber: 12 }; SKY.fall.crewsDated = []; SKY.fall.crewsByKit = []; }, () => go(k));
      for (const other of [ones, twelves]) {
        expect(other.outcome()).toEqual(real.outcome());
        expect(other.over!.lines).toEqual(real.over!.lines);
        expect(strip(other)).toEqual(strip(real));
      }
      expect(Math.max(...seenOut(ones))).toBe(1);
      expect(Math.max(...seenOut(twelves))).toBeGreaterThan(Math.max(...seenOut(real)) - 1);
      expect(Math.max(...seenOut(twelves))).toBeLessThanOrEqual(12);
    }
    expect(most).toBeGreaterThan(5);
    expect(lines).toBeGreaterThan(0);
  });

  /** As tests/battle-air-single-engine.test.ts:50-56 bare(), its strip list at :53 with crew added. */
  const WHO_OUT = /(, (one|\d+) parachutes?)?(, (one man|\d+ men) climbing out)?\.$/;
  const bare = (h: FieldEvent[]) => h.map((e) => {
    const { eng: _e, prop: _p, ...all } = (e.extra ?? {}) as Record<string, unknown>;
    const lost = e.what === 'air:lost', bomber = lost && all.air === 'bmr';
    const { chutes: _c, chuteAt: _a, chuteCue: _q, out: _o, captured: _k, crew: _n, ...rest } = all;
    const text = e.what === 'air:straggle' ? '' : bomber || (lost && all.how === 'collide') ? e.text.replace(WHO_OUT, '.') : e.text;
    return { t: e.t, what: e.what, unit: e.unit, side: e.side, cue: e.cue, text, extra: bomber ? rest : all };
  });
  const single = (tk: Ticket) => { const s = airBattle(tk, { show: true, bailouts: true }); s.ai.att = true; s.ai.def = true; while (!s.over) s.step(); return s; };
  const bombersLost = (s: AirSim) => lostOf(s.history()).filter((e) => fallOf(e).air === 'bmr');
  it('twin of tests/battle-air-single-engine.test.ts:77 (a two-seater\'s battle equals the twin\'s, bare() strip list at :53): the same with the switch on, crew stripped too', () => {
    for (let k = 0; k < 8; k++) {
      const twin = single(alsaceOf(k)), one = single(alsaceOf(k, 'D3A'));
      expect(one.outcome(), `run ${k}`).toEqual(twin.outcome());
      expect(one.over!.lines).toEqual(twin.over!.lines);
      expect(bare(one.history())).toEqual(bare(twin.history()));
    }
  });
  it('twin of tests/battle-air-single-engine.test.ts:90-121 with :98-100 (the twin\'s chutes + out <= 4, "the He 111 of 1939"): <= 4 where the run\'s mark is the He 111P, asserted per run; the two-seater\'s men the first two of the twin\'s', () => {
    let losses = 0, both = 0;
    for (let k = 0; k < 8; k++) {
      const tk = alsaceOf(k);
      expect(tk.place.guns!.GER.bmr!.mark, `run ${k}`).toBe('He 111P');
      const twin = bombersLost(single(tk)), one = bombersLost(single(alsaceOf(k, 'D3A')));
      expect(one.length).toBe(twin.length);
      one.forEach((e, i) => {
        const a = fallOf(e), b = fallOf(twin[i]);
        losses++;
        expect(a.crew).toBe(2);
        expect(b.crew).toBe(4);
        expect(a.chutes + (a.out ?? 0)).toBeLessThanOrEqual(2);
        expect(b.chutes + (b.out ?? 0)).toBeLessThanOrEqual(4);
        expect(a.chutes).toBeLessThanOrEqual(b.chutes);
        expect(a.chuteAt ?? []).toEqual((b.chuteAt ?? []).slice(0, a.chutes));
        expect(a.captured).toBe(a.chutes + (a.out ?? 0));
        if (a.chutes + (a.out ?? 0) === 2) both++;
      });
    }
    expect(losses).toBeGreaterThan(100);
    expect(both).toBeGreaterThan(5);
  });
});

describe('(f) each ruling seen to bite, switch on', () => {
  const share = (n: number, d: number) => Math.round((1000 * n) / Math.max(1, d)) / 10;
  it('D1: a fighter pilot not brought back shows a parachute over his own ground too, where with the switch off he never does; the shares by side, reported', () => {
    const c = { off: { def: [0, 0], att: [0, 0], defNot: 0 }, on: { def: [0, 0], att: [0, 0], defNot: 0 } };
    for (const p of allPairs()) for (const k of ['off', 'on'] as const) for (const e of p[k].lost) {
      const f = fallOf(e);
      if (f.air !== 'ftr' || p.models[e.unit!] === 'Fulmar') continue;
      const side = e.side as 'att' | 'def';
      c[k][side][0]++; if (f.chutes) c[k][side][1]++;
      if (side === 'def' && !f.back && f.chutes) c[k].defNot++;
    }
    expect(c.off.defNot).toBe(0);
    expect(c.on.defNot).toBeGreaterThan(50);
    expect(c.on.def[1]).toBeGreaterThan(c.off.def[1]);
    // The design's expectation (section 4.4: about 66% of defender fighter losses with a parachute at middle height; height now read too).
    const rows = { off: { def: share(c.off.def[1], c.off.def[0]), att: share(c.off.att[1], c.off.att[0]) }, on: { def: share(c.on.def[1], c.on.def[0]), att: share(c.on.att[1], c.on.att[0]) } };
    console.log(`fighter losses with a parachute, % (falls): defender off ${rows.off.def} (${c.off.def[0]}), on ${rows.on.def} (${c.on.def[0]}); attacker off ${rows.off.att} (${c.off.att[0]}), on ${rows.on.att} (${c.on.att[0]})`);
    expect(rows.on.def).toBeGreaterThan(55);
    expect(rows.on.def).toBeLessThan(75);
  });
  it('D1 and D3: in unit draws the pilot not brought back gets out at the share by the way down times outBand, the same on either ground; a forced landing climbs out at 0.6 on either ground', () => {
    const SV = ['flamer', 'burn', 'wing', 'tail', 'spin'] as const;
    for (const over of ['own', 'enemy'] as const) {
      for (const band of [0, 1, 2] as const) {
        const rng = new Rng(31 + band), got: Record<string, [number, number]> = {};
        for (let i = 0; i < 20_000; i++) {
          const f = fateOf(rng, fighter({ over, band, alt: SKY.bands.alt[band] }), freshMemory());
          const r = (got[f.how] ??= [0, 0]); r[0]++; if (f.chutes) r[1]++;
          if (f.how === 'glide') { expect(f.chutes).toBe(0); }
        }
        for (const h of SV) {
          const [n, k] = got[h];
          const want = Math.min(0.95, SKY.fate.pilotOut[h] * SKY.fate.outBand[band]);
          expect(Math.abs(k / n - want), `${over} band ${band} ${h}: ${k}/${n} against ${want}`).toBeLessThan(4 * Math.sqrt((want * (1 - want)) / n) + 0.01);
        }
        for (const h of ['explode', 'pilot']) if (got[h]) expect(got[h][1]).toBe(0);
      }
      const rng = new Rng(41);
      let glides = 0, out = 0;
      withBook(() => { SKY.fate.base.ftr = [0, 0, 0, 0, 0, 0, 0, 1, 0]; }, () => {
        for (let i = 0; i < 4_000; i++) { const f = fateOf(rng, fighter({ over, band: 1 }), freshMemory()); glides++; if (f.out) out++; expect(f.chutes).toBe(0); }
      });
      expect(Math.abs(out / glides - SKY.fate.ftrLand), over).toBeLessThan(0.03);
    }
  });
  it('the design\'s section 4.3: over the enemy\'s ground a pilot who rides a glide down keeps the look stream\'s climb-out die (as shipped, out.glide 0, his out is what it is switch off); over his own ground it is drawn on fork 83', () => {
    let enemy = 0, own = 0, ownOut = 0;
    withBook(() => { SKY.fate.base.ftr = [0, 0, 0, 0, 0, 0, 0, 1, 0]; }, () => {
      for (let seed = 1; seed <= 2_000; seed++) {
        const over = seed % 2 ? 'enemy' as const : 'own' as const;
        const go = (bailouts: boolean) => fateOf(new Rng(seed), fighter({ over, band: 1, bailouts }), freshMemory());
        const a = go(false), b = go(true);
        expect(b.how).toBe('glide');
        expect(b.chutes).toBe(0);
        if (over === 'enemy') { enemy++; expect(b.out ?? 0, `seed ${seed}`).toBe(a.out ?? 0); }
        else { own++; expect(a.out ?? 0).toBe(0); if (b.out) ownOut++; }
      }
    });
    expect(enemy).toBe(1_000);
    expect(Math.abs(ownOut / own - SKY.fate.ftrLand)).toBeLessThan(0.05);
    // And in the battles of the set: every switch-on fighter glide over the enemy's ground, its pilot not brought back, shows the switch-off out.
    let seen = 0;
    for (const p of allPairs()) p.on.lost.forEach((e, i) => {
      const f = fallOf(e), o = fallOf(p.off.lost[i]);
      if (f.air !== 'ftr' || f.how !== 'glide' || f.back || f.over !== 'enemy' || f.crew !== 1) return;
      seen++;
      expect(f.out ?? 0, p.key).toBe(o.out ?? 0);
    });
    expect(seen).toBeGreaterThan(0);
  });
  it('D11: a fighter pilot not brought back gets out of a breakup or a collision at out times outBand (0.1 each today), on either ground', () => {
    for (const over of ['own', 'enemy'] as const) {
      const rng = new Rng(77);
      let n = 0, k = 0, cn = 0, ck = 0;
      withBook(() => { SKY.fate.base.ftr = [0, 0, 0, 0, 0, 0, 0, 0, 1]; }, () => {
        for (let i = 0; i < 20_000; i++) { const f = fateOf(rng, fighter({ over, band: 2 }), freshMemory()); expect(f.how).toBe('breakup'); n++; if (f.chutes) k++; }
      });
      for (let i = 0; i < 20_000; i++) { const f = fateOf(rng, fighter({ over, band: 2 }), freshMemory(), { fid: 1, cause: 'ram' }); expect(f.how).toBe('collide'); cn++; if (f.chutes) ck++; }
      const want = SKY.fate.out.breakup * SKY.fate.outBand[2];
      expect(Math.abs(k / n - want), `${over} breakup`).toBeLessThan(0.012);
      expect(Math.abs(ck / cn - SKY.fate.out.collide * SKY.fate.outBand[2]), `${over} collide`).toBeLessThan(0.012);
    }
  });
  it('the switch on draws nothing on the look stream itself: the same dice give the same way down, and the stream ends where it ends switch off', () => {
    const less = (f: ReturnType<typeof fateOf>) => { const { chutes: _c, chuteAt: _a, out: _o, ...rest } = f; return rest; };
    let differ = 0;
    for (let seed = 1; seed <= 600; seed++) {
      const go = (bailouts: boolean) => {
        const rng = new Rng(seed), mem = freshMemory();
        const four = [fighter({ over: 'own', bailouts }), fighter({ over: 'enemy', bailouts }), fighter({ back: true, crew: bailouts ? 2 : 1, bailouts }), scene({ bailouts })].map((sc) => fateOf(rng, sc, mem));
        return { four, state: rng.state };
      };
      const a = go(false), b = go(true);
      expect(b.state).toBe(a.state);
      for (let i = 0; i < 4; i++) { expect(less(b.four[i])).toEqual(less(a.four[i])); if (b.four[i].chutes !== a.four[i].chutes) differ++; }
    }
    expect(differ).toBeGreaterThan(50);
  });
  it('a glide with the test figure out.glide 0.5 shows jumpers over land and the landing keeps whoever stayed; glides at sea show none', () => {
    let land = 0, jumped = 0, sea = 0;
    withBook(() => { SKY.fate.out.glide = 0.5; }, () => {
      for (const b of SET.filter((x) => /^(b17-|GER-Kent|kent-1940|harbour|carriers)/.test(x.key))) {
        for (const e of lostOf(flown(b, true).history())) {
          const f = fallOf(e);
          if (f.how !== 'glide') continue;
          expect(f.chutes + (f.out ?? 0)).toBeLessThanOrEqual(f.crew!);
          if (f.end === 'sea') { sea++; expect(f.chutes).toBe(0); continue; }
          if (f.air !== 'bmr') continue;
          land++;
          if (f.chutes) jumped++;
          expect(f.out ?? 0).toBeLessThanOrEqual(f.crew! - f.chutes);
          expect(f.out ?? 0).toBeGreaterThanOrEqual(Math.max(0, f.crew! - f.chutes - Math.ceil(f.crew! / 3) + 1));
        }
      }
    });
    expect(land).toBeGreaterThan(10);
    expect(jumped).toBeGreaterThan(5);
    expect(sea).toBeGreaterThan(0);
  });
  it('as shipped (out.glide 0) a bomber glide is what it was: no parachute, the same men climbing out where the crew is the same (where the kit gives one more, the same one die read against it)', () => {
    let glides = 0, moved = 0;
    for (const p of allPairs()) p.on.lost.forEach((e, i) => {
      const f = fallOf(e), o = fallOf(p.off.lost[i]);
      if (f.air !== 'bmr' || f.how !== 'glide') return;
      glides++;
      const byDate = crewOf(p.models[e.unit!], p.when);
      expect([f.chutes, f.chuteAt, o.chutes, o.chuteAt]).toEqual([0, undefined, 0, undefined]);
      if (f.crew === byDate) expect(f.out, p.key).toBe(o.out);
      else {
        moved++;
        expect(f.out ?? 0).toBeLessThanOrEqual(f.crew!);
        expect(f.out ?? 0).toBeGreaterThanOrEqual(f.crew! - Math.ceil(f.crew! / 3) + 1);
      }
    });
    expect(glides).toBeGreaterThan(20);
    expect(moved).toBeGreaterThan(0);
  });
  it('D4: crews follow the kit, never the month: the He 111P 4, the He 111H 5, the B-17 10 in 1939 and in August 1944 (where by the month it was 9)', () => {
    const crews = (prefix: string, model: string) => {
      const seen = new Set<number>(), offSeen = new Set<number>();
      for (const p of SET.filter((b) => b.key.startsWith(prefix)).map(pairFor)) {
        p.on.lost.forEach((e, i) => {
          if (fallOf(e).air !== 'bmr' || p.models[e.unit!] !== model) return;
          seen.add(fallOf(e).crew!);
          const o = fallOf(p.off.lost[i]);
          offSeen.add(o.chutes + (o.out ?? 0));
        });
      }
      return { seen: [...seen], most: Math.max(...offSeen) };
    };
    expect(crews('alsace-he111p', 'He111').seen).toEqual([4]);
    expect(crews('GER-Kent', 'He111').seen).toEqual([4]);
    expect(crews('kent-1940-he111h', 'He111').seen).toEqual([5]);
    expect(crews('b17-1939', 'B17').seen).toEqual([10]);
    const b44 = crews('b17-1944', 'B17');
    expect(b44.seen).toEqual([10]);
    expect(b44.most).toBeLessThanOrEqual(9);
    // And the 1940 He 111H shows its fifth man out, where by the month it showed at most four.
    let five = 0;
    for (const p of SET.filter((b) => b.key.startsWith('kent-1940-he111h')).map(pairFor)) for (const e of p.on.lost) if (fallOf(e).air === 'bmr' && fallOf(e).chutes + (fallOf(e).out ?? 0) === 5) five++;
    expect(five).toBeGreaterThan(0);
    // And the 1944 B-17's tenth man: out of every way but a glide, the same nine men's dice and one more, drawn on 41/53.
    let more = 0;
    for (const p of SET.filter((b) => b.key.startsWith('b17-1944')).map(pairFor)) p.on.lost.forEach((e, i) => {
      const f = fallOf(e), o = fallOf(p.off.lost[i]);
      if (f.air !== 'bmr' || f.how === 'glide') return;
      expect(f.chutes).toBeGreaterThanOrEqual(o.chutes);
      expect(f.chutes - o.chutes).toBeLessThanOrEqual(1);
      // (Its canopy's moment, and those of the men past the fifth, come one die later on 41/53: looks only.)
      if (f.chutes > o.chutes) more++;
    });
    expect(more).toBeGreaterThan(0);
  });
  it('D8: the two-seat Fulmar falls with crew 2 (hole H-FULMAR), its second man after the pilot\'s draws (by the crew\'s rule since the design director\'s 18:44Z: part h); every other fighter 1', () => {
    let fulmars = 0, others = 0;
    for (const p of SET.filter((b) => /^(harbour|carriers)/.test(b.key)).map(pairFor)) for (const e of p.on.lost) {
      if (fallOf(e).air !== 'ftr') continue;
      if (p.models[e.unit!] === 'Fulmar') { fulmars++; expect(fallOf(e).crew).toBe(2); } else { others++; expect(fallOf(e).crew).toBe(1); }
    }
    expect(fulmars).toBeGreaterThan(0);
    expect(others).toBeGreaterThan(0);
    // Unit draws: both men out sometimes, under parachutes or climbing out; with the pilot brought back, both out (part h).
    const rng = new Rng(13);
    let two = 0, backTwo = 0;
    for (let i = 0; i < 6_000; i++) {
      const back = i % 2 === 0;
      const f = fateOf(rng, fighter({ model: 'Fulmar', crew: 2, back, over: i % 4 < 2 ? 'own' : 'enemy' }), freshMemory());
      expect(f.chutes + (f.out ?? 0)).toBeLessThanOrEqual(2);
      if (back) expect(f.chutes).toBeGreaterThanOrEqual(1);
      if (f.chutes + (f.out ?? 0) === 2) { two++; if (back) backTwo++; }
    }
    expect(two).toBeGreaterThan(300);
    expect(backTwo).toBeGreaterThan(300);
    // Fork 83 is the look stream's own: the label the rule draws on.
    expect(GOT).toBe(83);
  });
});

describe('(g) after the sceptics: the glide reads its own height, and the crew reads the ticket\'s kit', () => {
  it('a glide\'s jump reads outBand at the scene\'s own band, low, middle and high (the test figure out.glide 0.5): the fighter pilot over his own ground and each of a bomber\'s men', () => {
    withBook(() => { SKY.fate.out.glide = 0.5; SKY.fate.base.ftr = [0, 0, 0, 0, 0, 0, 0, 1, 0]; SKY.fate.base.bmr = [0, 0, 0, 0, 0, 0, 0, 1, 0]; }, () => {
      for (const band of [0, 1, 2] as const) {
        const want = Math.min(0.95, 0.5 * SKY.fate.outBand[band]);
        const rng = new Rng(500 + band);
        let n = 0, k = 0;
        for (let i = 0; i < 8_000; i++) {
          const f = fateOf(rng, fighter({ over: 'own', band, alt: SKY.bands.alt[band] }), freshMemory());
          expect(f.how).toBe('glide');
          n++; if (f.chutes) k++;
        }
        expect(Math.abs(k / n - want), `fighter band ${band}: ${k}/${n} against ${want}`).toBeLessThan(4 * Math.sqrt((want * (1 - want)) / n) + 0.01);
        const brng = new Rng(600 + band);
        let men = 0, up = 0;
        for (let i = 0; i < 2_000; i++) {
          const f = fateOf(brng, scene({ crew: 5, band, alt: SKY.bands.alt[band] }), freshMemory());
          expect(f.how).toBe('glide');
          men += 5; up += f.chutes;
        }
        expect(Math.abs(up / men - want), `bomber band ${band}: ${up}/${men} against ${want}`).toBeLessThan(4 * Math.sqrt((want * (1 - want)) / men) + 0.01);
      }
    });
  });
  it('a bomber\'s crew follows the guns its ticket carries, not the date: a 1939 Alsace raid flying the He 111H\'s guns falls with crew 5, as shipped with 4', () => {
    const tk = alsaceOf(0);
    const kit = bomberGuns('He111', 'GER', { year: 1941, month: 0 });
    expect(kit.mark).toBe('He 111H');
    expect(bomberGuns('He111', 'GER', tk.place.when ?? { year: 1939, month: 8 }).mark).toBe('He 111P');
    const swapped = structuredClone(tk);
    swapped.place.guns!.GER.bmr = kit;
    const crews = (t: Ticket): number[] => {
      const seen = new Set<number>();
      for (const e of lostOf(flown({ key: 'kit', run: { key: '', ticket: t, play: 'ai' } }, true).history())) if (fallOf(e).air === 'bmr') seen.add(fallOf(e).crew!);
      return [...seen];
    };
    expect(crews(tk)).toEqual([4]);
    expect(crews(swapped)).toEqual([5]);
  });
});

// The design director, 26 November 2026 18:44Z, item 1: "The Fulmar's second man is crew, so he gets out by the crew's
// one rule (the bomber men's, including pilot killed at 0.35), not the pilot's. And the look never shows a fate the
// count does not make: where the count says back, he shows as his pilot shows, so no parachute into enemy ground with
// captured 0." Sizes fixed before running: 10,000 draws a cell.
describe('(h) the design director\'s 18:44Z, item 1: the Fulmar\'s second man is crew', () => {
  const WAYS = ['explode', 'flamer', 'burn', 'wing', 'tail', 'pilot', 'spin', 'glide', 'breakup', 'collide'] as const;
  const only = (h: string) => HOWS.map((x) => (x === h ? 1 : 0));
  const N = 10_000;
  /** Within 4 standard errors of a share (exactly none where the share is 0). */
  const near = (k: number, n: number, want: number, what: string) =>
    expect(Math.abs(k / n - want), `${what}: ${k}/${n} against ${want}`).toBeLessThanOrEqual(4 * Math.sqrt((want * (1 - want)) / n) + 1e-12);
  it('his pilot not brought back, he gets out by the crew\'s rule for each way down, pilot killed included (out[how] x outBand, the bomber men\'s), at each band on either ground; a bomber\'s men read the same', () => {
    let cells = 0;
    for (const how of WAYS) for (const over of ['own', 'enemy'] as const) for (const band of [0, 1, 2] as const) {
      const pair = how === 'collide' ? { fid: 1, cause: 'ram' as const } : undefined;
      const want = Math.min(0.95, SKY.fate.out[how] * SKY.fate.outBand[band]);
      withBook(() => { if (how !== 'collide') { SKY.fate.base.ftr = only(how); SKY.fate.base.bmr = only(how); } }, () => {
        let chute = 0, climb = 0;
        for (let seed = 1; seed <= N; seed++) {
          const go = (crew: number) => fateOf(new Rng(seed * 31 + band), fighter({ model: 'Fulmar', crew, over, band, alt: SKY.bands.alt[band] }), freshMemory(), pair);
          const one = go(1), two = go(2);
          expect(two.how).toBe(how);
          expect(two.chutes + (two.out ?? 0)).toBeLessThanOrEqual(2);
          // The pilot's draws come first on fork 83, so the crew of 1 on the same dice is the pilot alone: the rest is the second man.
          const dc = two.chutes - one.chutes, dout = (two.out ?? 0) - (one.out ?? 0);
          expect([dc >= 0, dout >= 0, dc + dout <= 1]).toEqual([true, true, true]);
          chute += dc; climb += dout;
        }
        near(chute, N, want, `second man, ${how} ${over} band ${band}`);
        // A forced landing keeps whoever stayed, less the landing's one die: for a crew of 2 it loses none (int(1)).
        expect(climb, `${how} ${over} band ${band}`).toBe(how === 'glide' ? N - chute : 0);
        // The bomber's men, the same way down and height: the same rule.
        const rng = new Rng(900 + band);
        let men = 0, up = 0;
        for (let i = 0; i < N / 5; i++) { const f = fateOf(rng, scene({ crew: 5, over, band, alt: SKY.bands.alt[band] }), freshMemory(), pair); expect(f.how).toBe(how); men += 5; up += f.chutes; }
        near(up, men, want, `bomber's men, ${how} ${over} band ${band}`);
      });
      cells++;
    }
    expect(cells).toBe(60);
  });
  it('his pilot brought back, his way out is his pilot\'s on every draw: under a parachute, his canopy after the pilot\'s, and the pilot\'s canopy where it was', () => {
    let n = 0;
    for (let seed = 1; seed <= N; seed++) {
      const over = seed % 2 ? 'own' as const : 'enemy' as const, band = (seed % 3) as 0 | 1 | 2;
      const pair = seed % 5 === 0 ? { fid: 1, cause: 'ram' as const } : undefined;
      const go = (crew: number) => fateOf(new Rng(seed), fighter({ model: 'Fulmar', crew, back: true, over, band, alt: SKY.bands.alt[band] }), freshMemory(), pair);
      const one = go(1), two = go(2);
      expect([two.chutes, two.out ?? 0, two.chuteAt?.length], `seed ${seed}`).toEqual([2, 0, 2]);
      expect(two.chuteAt![0]).toBe(one.chuteAt![0]);
      expect(two.chuteAt![1]).toBeGreaterThanOrEqual(two.chuteAt![0]);
      n++;
    }
    expect(n).toBe(N);
  });
  it('no fall of the set shows a fate the count does not make: brought back, every man of the crew under his parachute (none left aboard to be lost or taken) and captured 0; not brought back, captured by its formula; never more out than aboard', () => {
    let back = 0, fulmarBack = 0, falls = 0;
    for (const p of allPairs()) for (const e of p.on.lost) {
      const f = fallOf(e);
      falls++;
      expect(f.chutes + (f.out ?? 0), p.key).toBeLessThanOrEqual(f.crew!);
      if (f.back) {
        back++; if (f.crew === 2) fulmarBack++;
        expect([f.chutes, f.out ?? 0, f.captured], `${p.key} t ${e.t}`).toEqual([f.crew, 0, 0]);
      } else expect(f.captured, p.key).toBe(f.over === 'enemy' ? f.chutes + (f.out ?? 0) : 0);
    }
    console.log(`(h) falls ${falls}; brought back ${back}, of them Fulmars ${fulmarBack}`);
    expect(back).toBeGreaterThan(500);
  });
});
