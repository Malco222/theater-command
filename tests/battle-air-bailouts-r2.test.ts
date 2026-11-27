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
import { backAmong, backOf, crewOf, fateOf, freshMemory, GOT, HOWS, ramShare, restOut, type Scene } from '../src/battle/air/fates';
import { AIR } from '../src/campaign/air';
import { bomberGuns } from '../src/battle/air/guns';
import { airBattle, airLooks, AirSim, MORE_LOOKS } from '../src/battle/air/sim';
import { airTicket, raidGroups } from '../src/battle/air/ticket';
import { drawFall } from '../src/battle/air/falls';
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
/** A month inside a row of the meant-ram table (book.ts SKY.fate.ram.meant), as { year, month } (months from 0). */
const inside = (r: { from: number; to: number }) => {
  const d = (r.from + Math.min(r.to, r.from + 2)) / 2, year = Math.floor(d);
  return { year, month: Math.floor((d - year) * 12 + 1e-9) };
};
/** The fates test's Kent raid at the month, its attacker named American and flying B-17s, its defender named `nat` (parts (a) and (j)). */
const kentAs = (nat: string, when: { year: number; month: number }): Ticket => {
  const t = structuredClone(TK[1]), m0 = t.place.models!, g0 = t.place.guns!;
  t.place.when = when;
  t.place.nat = 'USA';
  t.att.forces = t.att.forces.map((f) => ({ ...f, nat: 'USA' }));
  t.def.forces = t.def.forces.map((f) => ({ ...f, nat }));
  t.place.models = { USA: { ftr: m0.GER.ftr, bmr: 'B17' }, [nat]: m0.UK };
  t.place.guns = { USA: { ftr: g0.GER.ftr, bmr: bomberGuns('B17', 'USA', when) }, [nat]: g0.UK };
  return t;
};
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

/** What the head 78fd87433 gave for each battle of the set, switch off: FNV-1a over every tick's view(), then history(), outcome() and over.lines (step 0, before any change).
 * On release 2's pass 2 (the assembler, 27 November 2026): the sixteen turn-30 raids (FRA-Rhineland, UK-Ruhr) take what the
 * pass-2 head before this join (next-av 839dfaa5f) gave, because pass 2's own campaign at turn 30 hands those raids other
 * tickets; all 80 battles hash the same there and with this join, switch off. The other 64 are 78fd87433's, unchanged. */
const BASELINE: Record<string, number> = {
  'GER-Alsace:0': 2225686532, 'GER-Alsace:1': 225243303, 'GER-Alsace:2': 2335731105, 'GER-Alsace:3': 4219053814,
  'GER-Alsace:4': 2253648796, 'GER-Alsace:5': 1975599040, 'GER-Alsace:6': 4182902111, 'GER-Alsace:7': 377184919,
  'GER-Kent:0': 3148912142, 'GER-Kent:1': 891888083, 'GER-Kent:2': 657927157, 'GER-Kent:3': 4249233553,
  'GER-Kent:4': 4048561157, 'GER-Kent:5': 3501605111, 'GER-Kent:6': 447398267, 'GER-Kent:7': 1407443564,
  'FRA-Rhineland:0': 3815780284, 'FRA-Rhineland:1': 3477971932, 'FRA-Rhineland:2': 3755573771, 'FRA-Rhineland:3': 1832780923,
  'FRA-Rhineland:4': 3903087796, 'FRA-Rhineland:5': 2302530418, 'FRA-Rhineland:6': 3876039490, 'FRA-Rhineland:7': 1530121251,
  'UK-Ruhr:0': 1130731809, 'UK-Ruhr:1': 987516315, 'UK-Ruhr:2': 888497646, 'UK-Ruhr:3': 3694235281,
  'UK-Ruhr:4': 2746919971, 'UK-Ruhr:5': 2688296919, 'UK-Ruhr:6': 1648467190, 'UK-Ruhr:7': 1077192620,
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

/**
 * The set above holds no ram on purpose, so with the switch off nothing there reads a field a ram on purpose might
 * carry (the third mend's sceptic, ca: a fall's B carried without the switch survived part (a)). These are the five
 * rows of the meant-ram table (book.ts SKY.fate.ram.meant), each Kent as part (j) flies it at a month inside the row,
 * battles 0 to 9, switch off: the same hash as BASELINE (every tick's view(), history(), outcome(), over.lines), taken
 * at the head 78fd87433 (and the same at a8a34088f). 116 falls of these 50 battles are rams on purpose.
 */
const MEANT_OFF: Record<string, number[]> = {
  'meant-SOV-1941.42': [3406350152, 2465839585, 3359667683, 49872625, 2585266030, 1686629021, 462960260, 60698100, 4064559928, 3066600255],
  'meant-SOV-1943.00': [462501890, 841922219, 3114041673, 2522016623, 293032566, 1030479687, 2847124662, 3067042210, 1499830868, 1257155935],
  'meant-JAP-1944.42': [3852909730, 1554199103, 1032019029, 2455711495, 1097591905, 1589629547, 2629176832, 1545934976, 1579429210, 2570514602],
  'meant-JAP-1944.83': [3717678574, 3217599065, 3793129163, 3642867215, 1097591905, 1589629547, 2629176832, 1545934976, 3262617850, 1893762008],
  'meant-GER-1944.00': [2160119284, 3253853610, 3341831804, 323899235, 608852728, 401444141, 1151134842, 3807904345, 2959671501, 1807603846],
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
/** A fighter's scene, with the count's own share back for its ground (the ticket's saved by side, ticket.ts: AIR.pilots home and away). */
const fighter = (o: Partial<Scene> = {}): Scene => scene({ air: 'ftr', engines: 1, nat: 'UK', model: 'Hurricane', gun: 'cannon20', crew: 1, backShare: o.over === 'own' ? AIR.pilots.home : AIR.pilots.away, ...o });
/** Book figures a test sets, put back after it. */
function withBook<T>(set: () => void, go: () => T): T {
  const was = { glide: SKY.fate.out.glide, base: structuredClone(SKY.fate.base), crews: SKY.fall.crews, dated: SKY.fall.crewsDated, byKit: SKY.fall.crewsByKit, meant: SKY.fate.ram.meant, acc: [SKY.fate.ram.headon, SKY.fate.ram.other, SKY.fate.ram.tangle], landTwo: SKY.fate.landTwo };
  try { set(); return go(); } finally {
    SKY.fate.out.glide = was.glide; SKY.fate.base.ftr = was.base.ftr; SKY.fate.base.bmr = was.base.bmr;
    SKY.fall.crews = was.crews; SKY.fall.crewsDated = was.dated; SKY.fall.crewsByKit = was.byKit;
    SKY.fate.ram.meant = was.meant; [SKY.fate.ram.headon, SKY.fate.ram.other, SKY.fate.ram.tangle] = was.acc; SKY.fate.landTwo = was.landTwo;
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
  for (const r of SKY.fate.ram.meant) {
    const key = `meant-${r.nat}-${r.from.toFixed(2)}`, when = inside(r);
    it(`the rams on purpose, switch off: Kent as part (j) flies it, ${r.nat} at ${when.year}/${when.month}, battles 0 to 9 hash as the head 78fd87433 gave`, () => {
      const tk = kentAs(r.nat, when);
      let meant = 0;
      const got = Array.from({ length: 10 }, (_, k) => {
        const w = whole(at(key, tk, k));
        meant += lostOf(w.sim.history()).filter((e) => fallOf(e).meant).length;
        return w.hash;
      });
      expect(got, key).toEqual(MEANT_OFF[key]);
      expect(meant, `${key}: rams on purpose in the ten battles`).toBeGreaterThan(0);
    });
  }
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
  it('D1, priced once (18:47Z): fighter losses with a parachute by side, switch off and on, split by brought back and not; a pilot brought back always under his parachute, and over his own ground a pilot not brought back now rarely (the men brought back are inside the share)', () => {
    type Row = { n: number; c: number };
    const z = (): Row => ({ n: 0, c: 0 });
    const c = { off: { def: z(), att: z(), defBack: z(), defNot: z(), attBack: z(), attNot: z() }, on: { def: z(), att: z(), defBack: z(), defNot: z(), attBack: z(), attNot: z() } };
    for (const p of allPairs()) for (const k of ['off', 'on'] as const) for (const e of p[k].lost) {
      const f = fallOf(e);
      if (f.air !== 'ftr' || p.models[e.unit!] === 'Fulmar') continue;
      const side = e.side as 'att' | 'def', add = (r: Row) => { r.n++; if (f.chutes) r.c++; };
      add(c[k][side]); add(c[k][`${side}${f.back ? 'Back' : 'Not'}`]);
    }
    const pc = (r: Row) => `${Math.round((1000 * r.c) / Math.max(1, r.n)) / 10}% (${r.c}/${r.n})`;
    for (const k of ['off', 'on'] as const) console.log(`fighter losses with a parachute, switch ${k}: defender ${pc(c[k].def)} [brought back ${pc(c[k].defBack)}, not ${pc(c[k].defNot)}]; attacker ${pc(c[k].att)} [brought back ${pc(c[k].attBack)}, not ${pc(c[k].attNot)}]`);
    // The count is the same off and on, and every pilot brought back is under his parachute either way.
    for (const r of ['def', 'att', 'defBack', 'defNot', 'attBack', 'attNot'] as const) expect(c.on[r].n).toBe(c.off[r].n);
    expect([c.on.defBack.c, c.on.attBack.c]).toEqual([c.on.defBack.n, c.on.attBack.n]);
    // Switch off a defender not brought back never shows one; on, a few do (where the share outruns the men brought back: high spins, glides' jumps, breakups).
    expect(c.off.defNot.c).toBe(0);
    expect(c.on.defNot.c).toBeGreaterThan(0);
    // No longer "did he get out" read twice: the defender's share stays near what the count alone makes (52.4% off, 65.5% at 7033d0bbf).
    expect(c.on.def.c / c.on.def.n).toBeLessThan(0.56);
  });
  it('D1 and D3: in unit draws the pilot not brought back gets out at the share by the way down times outBand, the same on either ground; a forced landing climbs out at 0.6 on either ground (on a side none of whose pilots come back, backShare 0, so that B is 0 and R is S: the 18:47Z pricing is part i)', () => {
    const SV = ['flamer', 'burn', 'wing', 'tail', 'spin'] as const;
    for (const over of ['own', 'enemy'] as const) {
      for (const band of [0, 1, 2] as const) {
        const rng = new Rng(31 + band), got: Record<string, [number, number]> = {};
        for (let i = 0; i < 20_000; i++) {
          const f = fateOf(rng, fighter({ over, band, alt: SKY.bands.alt[band], backShare: 0 }), freshMemory());
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
  for (const how of WAYS) it(`his pilot not brought back, he gets out by the crew's rule, way down ${how} (out[how] x outBand, the bomber men's; pilot killed included), at each band on either ground; a bomber's men read the same`, () => {
    let cells = 0;
    for (const over of ['own', 'enemy'] as const) for (const band of [0, 1, 2] as const) {
      const pair = how === 'collide' ? { fid: 1, cause: 'ram' as const } : undefined;
      const want = Math.min(0.95, SKY.fate.out[how] * SKY.fate.outBand[band]);
      withBook(() => { if (how !== 'collide') { SKY.fate.base.ftr = only(how); SKY.fate.base.bmr = only(how); } }, () => {
        let chute = 0, climb = 0;
        for (let seed = 1; seed <= N; seed++) {
          // (A side none of whose pilots come back, backShare 0: then B is 0 and R is S, so the crew's rule is seen alone; part i prices it.)
          const go = (crew: number) => fateOf(new Rng(seed * 31 + band), fighter({ model: 'Fulmar', crew, over, band, alt: SKY.bands.alt[band], backShare: 0 }), freshMemory(), pair);
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
    expect(cells).toBe(6);
  });
  it('his pilot brought back, his way out is his pilot\'s on every draw: under a parachute, his canopy after the pilot\'s, and the pilot\'s canopy where it was', () => {
    let n = 0, gapped = 0;
    for (let seed = 1; seed <= N; seed++) {
      const over = seed % 2 ? 'own' as const : 'enemy' as const, band = (seed % 3) as 0 | 1 | 2;
      const pair = seed % 5 === 0 ? { fid: 1, cause: 'ram' as const } : undefined;
      const go = (crew: number) => fateOf(new Rng(seed), fighter({ model: 'Fulmar', crew, back: true, over, band, alt: SKY.bands.alt[band] }), freshMemory(), pair);
      const one = go(1), two = go(2);
      expect([two.chutes, two.out ?? 0, two.chuteAt?.length], `seed ${seed}`).toEqual([2, 0, 2]);
      expect(two.chuteAt![0]).toBe(one.chuteAt![0]);
      expect(two.chuteAt![1]).toBeGreaterThanOrEqual(two.chuteAt![0]);
      // One canopy gap after the pilot's (fates.ts), unless held just before the end (both capped at upTo - 0.5).
      const cap = Math.round(Math.max(0.5, (two.boom ?? two.dur) - 0.5) * 10) / 10;
      if (two.chuteAt![1] !== cap) { gapped++; expect(two.chuteAt![1] - two.chuteAt![0], `seed ${seed}`).toBeGreaterThanOrEqual(SKY.fate.chute.gap[0] - 1e-9); }
      n++;
    }
    expect(n).toBe(N);
    expect(gapped).toBeGreaterThan(N / 2);
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

// The design director, 26 November 2026 18:47Z, item 2: "D3's share times the band is the share of men shown getting
// out of such falls IN ALL, with the men counted back inside it, never on top. So fork 83 draws a not-back man out at
// the remainder: (D3 x band - share back x share of the back shown out) / (1 - share back), floored at 0." Built for
// every way down by that principle (S the rule's share by way down and height; B the share of such falls' men the
// count brings back, read from the count's own share for the fall and the scene's weights; K 1; R the rest). A
// collision is left as built (its B is made by the pairing outside the scene: an open reading). Sizes fixed before
// running: 90,000 draws for each way down, band and ground (each cell asserted to hold 20,000 falls or more); the
// Fulmar's second man 20,000 a cell.
describe('(i) the design director\'s 18:47Z, item 2: priced once', () => {
  const near = (k: number, n: number, want: number, what: string) =>
    expect(Math.abs(k / n - want), `${what}: ${k}/${n} against ${want}`).toBeLessThanOrEqual(4 * Math.sqrt((want * (1 - want)) / n) + 1e-12);
  const F = SKY.fate;
  /** The rule's share S of a fighter pilot getting out, by the way down and band, over land (book.ts): D3, D11, the forced landing. */
  const S = (how: string, band: 0 | 1 | 2): number => {
    const b = F.outBand[band];
    if (how === 'glide') { const j = Math.min(0.95, F.out.glide * b); return j + (1 - j) * F.ftrLand; }
    if ((F.saved as readonly string[]).includes(how)) return Math.min(0.95, F.pilotOut[how as keyof typeof F.pilotOut] * b);
    return how === 'breakup' ? Math.min(0.95, F.out.breakup * b) : 0;
  };
  const ROW = F.base.ftr;
  for (const over of ['own', 'enemy'] as const) for (const band of [0, 1, 2] as const) {
    it(`the pilot, over ${over === 'own' ? 'his own' : 'the enemy\'s'} ground at band ${band}: for every way down, all the men of such falls shown out at S (B where the men brought back outrun it), the men not brought back at R, the share brought back B`, () => {
      const s = over === 'own' ? AIR.pilots.home : AIR.pilots.away;
      for (const [i, how] of HOWS.entries()) {
        // A test row (the book's fighter row with this way down thirty times as likely), so each cell is well filled; B is the scene's own reading of it.
        withBook(() => { SKY.fate.base.ftr = ROW.map((w, j) => (j === i ? w * 30 : w)); }, () => {
          const sc = (back: boolean) => fighter({ over, band, alt: SKY.bands.alt[band], back });
          const B = backOf(sc(false), freshMemory(), how), want = S(how, band), R = restOut(want, B, 1);
          const E = B + (1 - B) * R;
          if (R > 0) expect(Math.abs(E - want)).toBeLessThan(1e-12);
          const rng = new Rng(7_000 + 10 * i + band), die = new Rng(8_000 + 10 * i + band);
          let n = 0, shown = 0, back = 0, not = 0, notShown = 0;
          for (let d = 0; d < 90_000; d++) {
            const isBack = die.chance(s);
            const f = fateOf(rng, sc(isBack), freshMemory());
            if (f.how !== how) continue;
            const o = f.chutes + (f.out ?? 0);
            expect(o).toBeLessThanOrEqual(1);
            n++; shown += o;
            if (isBack) { back++; expect(f.chutes).toBe(1); } else { not++; notShown += o; }
          }
          const what = `${over} band ${band} ${how} (S ${want.toFixed(3)}, B ${B.toFixed(4)}, R ${R.toFixed(3)})`;
          expect(n, what).toBeGreaterThanOrEqual(20_000);
          near(back, n, B, `${what}, share brought back`);
          near(shown, n, E, `${what}, all men shown out`);
          if (not) near(notShown, not, R, `${what}, men not brought back shown out`);
        });
      }
    });
  }
  it('the Fulmar\'s second man, each way his pilot can get out of, each band and ground: all such men shown out at his crew\'s S2 (B where the men brought back outrun it), those not brought back at R2 (a one-way test row: there B is the count\'s share itself)', () => {
    let cells = 0;
    for (const over of ['own', 'enemy'] as const) for (const band of [0, 1, 2] as const) for (const how of F.saved) {
      const s = over === 'own' ? AIR.pilots.home : AIR.pilots.away;
      withBook(() => { SKY.fate.base.ftr = HOWS.map((x) => (x === how ? 1 : 0)); }, () => {
        const sc = (crew: number, back: boolean) => fighter({ model: 'Fulmar', crew, over, band, alt: SKY.bands.alt[band], back });
        const B = backOf(sc(2, false), freshMemory(), how), S2 = Math.min(0.95, F.out[how] * F.outBand[band]), R2 = restOut(S2, B, 1);
        expect(Math.abs(B - s)).toBeLessThan(1e-12);
        let n = 0, shown = 0, not = 0, notShown = 0;
        for (let seed = 1; seed <= 20_000; seed++) {
          const isBack = new Rng(50_000 + seed).chance(s);
          const one = fateOf(new Rng(seed), sc(1, isBack), freshMemory()), two = fateOf(new Rng(seed), sc(2, isBack), freshMemory());
          const man = two.chutes + (two.out ?? 0) - one.chutes - (one.out ?? 0);
          expect(man === 0 || man === 1).toBe(true);
          n++; shown += man;
          if (isBack) expect(man).toBe(1); else { not++; notShown += man; }
        }
        const what = `Fulmar's second man, ${over} band ${band} ${how} (S2 ${S2.toFixed(3)}, B ${B}, R2 ${R2.toFixed(3)})`;
        near(shown, n, B + (1 - B) * R2, `${what}, all`);
        near(notShown, not, R2, `${what}, not brought back`);
      });
      cells++;
    }
    expect(cells).toBe(30);
  });
  it('the floor at 0: on the book as it stands, wherever the men brought back outrun the share (S < B x K), R is 0, never below; elsewhere (S - B) / (1 - B)', () => {
    let floored = 0, open = 0;
    for (const over of ['own', 'enemy'] as const) for (const band of [0, 1, 2] as const) for (const how of HOWS) {
      const B = backOf(fighter({ over, band }), freshMemory(), how), want = S(how, band), R = restOut(want, B, 1);
      expect(R, `${over} ${band} ${how}`).toBeGreaterThanOrEqual(0);
      if (want < B) { floored++; expect(R).toBe(0); } else { open++; expect(Math.abs(R - (want - B) / (1 - B))).toBeLessThan(1e-12); }
      // A way a pilot brought back never takes: B 0, R is S.
      if (!(F.saved as readonly string[]).includes(how)) expect([B, R]).toEqual([0, want]);
    }
    console.log(`(i) the floor: ${floored} cells floored at 0, ${open} not (fighter book row, cannon20, 54 cells)`);
    expect(floored).toBeGreaterThan(0);
    expect(restOut(0.2, 1, 1)).toBe(0);
    expect(() => fateOf(new Rng(1), { ...fighter(), backShare: undefined }, freshMemory())).toThrow(/share back/);
  });
  it('B follows the count\'s own share for the fall, never a pasted one: Kent flown with the defender\'s share back set to 0 and to 0.9 (test figures on the ticket): at 0 the defender\'s pilots get out at the full share, at 0.9 not one not brought back in a way a pilot can get out of', () => {
    const kent = (def: number, k: number): Battle => {
      const t = structuredClone(TK[1]);
      t.place.saved = { ...t.place.saved, def };
      return at(`kent-saved-${def}`, t, k);
    };
    const tally = (def: number) => {
      let n = 0, c = 0, back = 0;
      for (const k of [0, 1, 2, 3]) for (const e of lostOf(flown(kent(def, k), true).history())) {
        const f = fallOf(e);
        if (f.air !== 'ftr' || e.side !== 'def' || !(F.saved as readonly string[]).includes(f.how)) continue;
        if (f.back) { back++; continue; }
        n++; if (f.chutes) c++;
      }
      return { n, c, back };
    };
    const none = tally(0), most = tally(0.9);
    console.log(`(i) Kent, defenders not brought back in a way a pilot can get out of: share back 0, ${none.c}/${none.n} under a parachute (${none.back} back); share back 0.9, ${most.c}/${most.n} (${most.back} back)`);
    expect(none.back).toBe(0);
    expect(none.n).toBeGreaterThan(40);
    expect(none.c / none.n).toBeGreaterThan(0.25);
    expect(most.n).toBeGreaterThan(0);
    expect(most.c).toBe(0);
  });
});

// The design director, 27 November 2026 01:00Z, reading 1: "For the meant-ram rows: if the code can bring a meant
// rammer back, read B there by the same formula; if it cannot, leave it as built." It can (sim.ts fated: a ram on
// purpose is weighed m whether or not the pilot is back; only the accident's p is 0 for a pilot brought back). So a
// rammer's B is read by the saved ways' formula (fates.ts backAmong) over the pairing's own weights, and a rammer not
// brought back is drawn at R = max(0, (S - B x K) / (1 - B)), K 1. A collision by accident, or of two of one group,
// never pairs a pilot brought back: B 0, R = S, as built.
// The scenes: no battle of the set flies a meant row (none of its rammers' nations and months is in the table), so each
// row is flown on the fates test's Kent raid with its sides named again (the attacker's bombers flown as American
// B-17s, the defender's Hurricanes under the row's nation) at a month inside the row. Sizes fixed before running: 60
// battles a row at the book's own shares (each row's n is printed; no row reaches 2,000 rammers: about two a battle at
// the highest share, 0.2), and one row, the Soviet 1941-42, flown again with every row's share set to the test figure
// 1 (every fighter lost in the same moment as the bomber it fired on rams it), 270 battles, which reaches 2,000 (first
// written at 240 battles: its own size check failed at 1,959 rammers, every share check having passed, so raised to 270).
describe('(j) the design director\'s 01:00Z, reading 1: a ram on purpose reads its B', () => {
  const near = (k: number, n: number, want: number, what: string) =>
    expect(Math.abs(k / n - want), `${what}: ${k}/${n} against ${want}`).toBeLessThanOrEqual(4 * Math.sqrt((want * (1 - want)) / n) + 1e-12);
  const F = SKY.fate;
  type Tot = { n: number; back: number; shown: number; not: number; notShown: number; accN: number; accShown: number; s: number; B: number; R: number[] };
  const tot = new Map<string, Tot>();
  /** Flies k0 .. k0 + n - 1 of a row's battles, switch on, and checks every fall a ram on purpose made. */
  const flyRow = (key: string, nat: string, when: { year: number; month: number }, p: number, k0: number, n: number) => {
    const tk = kentAs(nat, when), s = tk.place.saved.def;
    // B by the formula, over the pairing's weights for a ram on purpose: m for a pilot brought back and m for one not.
    const B = backAmong(s, p, 1, p, 1), R = ([0, 1, 2] as const).map((b) => restOut(Math.min(0.95, F.out.collide * F.outBand[b]), B, 1));
    const t = tot.get(key) ?? { n: 0, back: 0, shown: 0, not: 0, notShown: 0, accN: 0, accShown: 0, s, B, R };
    for (let k = k0; k < k0 + n; k++) {
      const h = flown(at(key, tk, k), true).history();
      for (const e of lostOf(h)) {
        const f = fallOf(e);
        expect(f.chutes + (f.out ?? 0), `${key}:${k}`).toBeLessThanOrEqual(f.crew!);
        expect(f.captured).toBe(f.back || f.over !== 'enemy' ? 0 : f.chutes + (f.out ?? 0));
        if (f.air !== 'ftr' || f.how !== 'collide') continue;
        if (!f.meant) { if (!f.back && e.side === 'def') { t.accN++; if (f.chutes) t.accShown++; } continue; }
        // Every rammer here is a defender, over his own ground (no attacker in these raids fires on a bomber).
        expect([e.side, f.over]).toEqual(['def', 'own']);
        t.n++;
        const o = f.chutes + (f.out ?? 0);
        t.shown += o;
        // K: a rammer the count brings back is under his parachute, as every pilot brought back is.
        if (f.back) { t.back++; expect([f.chutes, f.out ?? 0, f.chuteAt?.length], `${key}:${k} t ${e.t}`).toEqual([1, 0, 1]); }
        else {
          t.not++; t.notShown += o;
          // Over his own ground the men brought back outrun the share at every height (S at most 0.11, B about 0.5): R is 0.
          if (Math.max(...R) === 0) expect(o, `${key}:${k} t ${e.t}: a rammer not brought back, R 0`).toBe(0);
        }
      }
    }
    tot.set(key, t);
  };
  const ROWS = F.ram.meant.map((r) => ({ r, key: `meant-${r.nat}-${r.from.toFixed(2)}`, when: inside(r) }));
  it('the rows are the book\'s, each read at its month (ramShare), and the code can bring a rammer back: the pairing weighs a ram on purpose the same for a pilot brought back and one not, so B is the count\'s own share', () => {
    expect(ROWS.length).toBe(5);
    for (const { r, when } of ROWS) {
      expect(ramShare(r.nat, when, 4, r.pass), `${r.nat} ${when.year}/${when.month}`).toBe(r.p);
      const s = AIR.pilots.home;
      expect(Math.abs(backAmong(s, r.p, 1, r.p, 1) - s)).toBeLessThan(1e-12);
    }
  });
  for (const { r, key, when } of ROWS) for (const k0 of [0, 30]) {
    it(`${r.nat} from ${r.from.toFixed(2)} (p ${r.p}${r.four ? ', four-engined' : ''}${r.pass ? `, pass ${r.pass}` : ''}), at ${when.year}/${when.month}, battles ${k0} to ${k0 + 29}: every rammer brought back under his parachute; every one not brought back over his own ground shows none (R 0); chutes + out <= crew and captured as ever on every fall`, () => {
      flyRow(key, r.nat, when, r.p, k0, 30);
    });
  }
  const TF = 'meant-SOV-test-figure-1';
  for (const k0 of [0, 30, 60, 90, 120, 150, 180, 210, 240]) {
    it(`the Soviet 1941-42 row with every row's share at the test figure 1, battles ${k0} to ${k0 + 29}: the same checks on every fall`, () => {
      withBook(() => { SKY.fate.ram.meant = SKY.fate.ram.meant.map((m) => ({ ...m, p: 1 })); }, () => flyRow(TF, 'SOV', ROWS[0].when, 1, k0, 30));
    });
  }
  it('the shares: of all rammers on purpose, the share brought back at B and the share shown out at B + (1 - B) x R (B, since R is 0 over his own ground); those not brought back at R; and a collision by accident still at its full share S', () => {
    for (const key of [...ROWS.map((x) => x.key), TF]) {
      const t = tot.get(key)!;
      expect(t, key).toBeDefined();
      const what = `${key} (s ${t.s}, B ${t.B.toFixed(4)}, R by band ${t.R.map((x) => x.toFixed(3)).join(' ')})`;
      console.log(`(j) ${what}: rammers ${t.n}, brought back ${t.back}, shown out ${t.shown}; not brought back ${t.not}, shown out ${t.notShown}; by accident, defenders not brought back ${t.accN}, shown out ${t.accShown}`);
      expect(t.n, key).toBeGreaterThan(0);
      near(t.back, t.n, t.B, `${what}, brought back`);
      near(t.shown, t.n, t.B + (1 - t.B) * Math.max(...t.R), `${what}, all shown out`);
      near(t.notShown, Math.max(1, t.not), Math.max(...t.R), `${what}, not brought back shown out`);
    }
    expect(tot.get(TF)!.n).toBeGreaterThanOrEqual(2_000);
    // By accident: unit draws, each band and ground, 20,000 a cell: a pilot not brought back at S itself (B 0), as at a8a34088f.
    for (const over of ['own', 'enemy'] as const) for (const band of [0, 1, 2] as const) {
      const rng = new Rng(1_300 + band + (over === 'own' ? 0 : 10)), want = Math.min(0.95, F.out.collide * F.outBand[band]);
      let k = 0;
      for (let i = 0; i < 20_000; i++) { const f = fateOf(rng, fighter({ over, band, alt: SKY.bands.alt[band] }), freshMemory(), { fid: 1, cause: 'ram' }); expect(f.how).toBe('collide'); if (f.chutes) k++; }
      near(k, 20_000, want, `by accident, ${over} band ${band}`);
    }
    // And in the battles: defenders not brought back from a collision by accident show a parachute (at S, not R = 0).
    const acc = [...tot.values()].reduce((a, t) => [a[0] + t.accN, a[1] + t.accShown], [0, 0]);
    console.log(`(j) collisions by accident, defenders not brought back, all rows: ${acc[1]}/${acc[0]} under a parachute`);
  });
  // Collisions by accident in battles: the fates test's Kent raid as it is (no meant row), the accident's shares at the
  // test figure 1, 90 battles (first written at 30: it could not tell S from R = 0 at 132 falls, its planted fault MJ2
  // surviving, so raised before it was committed).
  const acc = { n: 0, k: 0, want: 0, v: 0 };
  for (const k0 of [0, 30, 60]) {
    it(`collisions by accident in battles, Kent at the accident's test figure 1, battles ${k0} to ${k0 + 29}: no ram on purpose`, () => {
      withBook(() => { SKY.fate.ram.headon = 1; SKY.fate.ram.other = 1; SKY.fate.ram.tangle = 1; }, () => {
        for (let i = k0; i < k0 + 30; i++) for (const e of lostOf(flown(at('kent-accident', TK[1], i), true).history())) {
          const f = fallOf(e);
          if (f.air !== 'ftr' || f.how !== 'collide' || f.back || e.side !== 'def') continue;
          expect(f.meant).toBeUndefined();
          const alt = (e.extra as { alt: number }).alt, band = [0, 1, 2].reduce((b, j) => (Math.abs(SKY.bands.alt[j] - alt) < Math.abs(SKY.bands.alt[b] - alt) ? j : b), 0);
          const S = Math.min(0.95, F.out.collide * F.outBand[band]);
          acc.n++; acc.want += S; acc.v += S * (1 - S); if (f.chutes) acc.k++;
        }
      });
    });
  }
  it('collisions by accident in battles: a defender not brought back shown out at S itself (by the band nearest his height), never at the R = 0 a B would give', () => {
    console.log(`(j) by accident (test figures 1), Kent, defenders not brought back: ${acc.k}/${acc.n} under a parachute against ${acc.want.toFixed(1)} at S`);
    expect(acc.n).toBeGreaterThan(300);
    expect(Math.abs(acc.k - acc.want)).toBeLessThanOrEqual(4 * Math.sqrt(acc.v));
  });
  it('the look of a ram on purpose in unit draws, each band and ground, 20,000 a cell (B carried as the pairing carries it, Scene.meantBack): brought back at B, all shown out at B + (1 - B) x R, those not brought back at R', () => {
    for (const over of ['own', 'enemy'] as const) for (const band of [0, 1, 2] as const) {
      const s = over === 'own' ? AIR.pilots.home : AIR.pilots.away, B = backAmong(s, 0.2, 1, 0.2, 1);
      const S = Math.min(0.95, F.out.collide * F.outBand[band]), R = restOut(S, B, 1);
      const rng = new Rng(1_500 + band + (over === 'own' ? 0 : 10)), die = new Rng(1_600 + band + (over === 'own' ? 0 : 10));
      let back = 0, shown = 0, not = 0, notShown = 0;
      for (let i = 0; i < 20_000; i++) {
        const isBack = die.chance(B);
        const f = fateOf(rng, fighter({ over, band, alt: SKY.bands.alt[band], back: isBack, meantBack: B }), freshMemory(), { fid: 1, cause: 'ram' });
        expect(f.how).toBe('collide');
        expect(f.chutes).toBeLessThanOrEqual(1);
        shown += f.chutes;
        if (isBack) { back++; expect(f.chutes).toBe(1); } else { not++; notShown += f.chutes; }
      }
      const what = `${over} band ${band} (S ${S.toFixed(3)}, B ${B.toFixed(4)}, R ${R.toFixed(4)})`;
      near(back, 20_000, B, `${what}, brought back`);
      near(shown, 20_000, B + (1 - B) * R, `${what}, all shown out`);
      near(notShown, not, R, `${what}, not brought back`);
    }
  });
  // The third mend's sceptic, ca: the test above feeds Scene.meantBack by hand, and the battles above have rammers only
  // over their own ground, where R is 0 for any B near the count's share; so nothing read the B the pairing itself
  // hands the look (a B from 1 - s, or from the bomber's share, survived). These read it through fated() itself: the
  // fates test's Kent raid opened with the switch on, its first defending fighter named Soviet at February 1942 (the
  // table's first row, p 0.2) firing head-on on a bomber, the pair lost in one moment, again and again.
  /** Rams on purpose drawn through fated(): the B the pairing carried into each rammer's scene, and the share of rammers not brought back shown out. */
  const ramsThrough = (over: 'own' | 'enemy', band: 0 | 1 | 2, want: number, seed: number) => {
    const sim = airBattle(TK[1], { bailouts: true });
    // The sim's own fields, reached past TypeScript's private: the month it reads, the moment's memory, the look's words.
    const S = sim as unknown as { when: { year: number; month: number }; recent: unknown[]; fated(lost: unknown[]): void; sceneOf(L: unknown, near: unknown, meantBack?: number): Scene; say: unknown; shownSay: unknown };
    S.when = { year: 1942, month: 2 };
    const a = sim.units.find((u) => u.air === 'ftr' && u.side === 'def')!, b = sim.units.find((u) => u.air === 'bmr' && u.side === 'att')!;
    a.nat = 'SOV'; a.band = band; b.band = band;
    a.fire = [{ on: b.id, rate: 1, pass: 'headon' } as (typeof a.fire)[number]];
    a.contacts = [];
    const carried: number[] = [];
    const sceneOf0 = S.sceneOf.bind(sim);
    S.sceneOf = (L: unknown, near: unknown, meantBack?: number) => { if ((L as { u: unknown }).u === a && meantBack !== undefined) carried.push(meantBack); return sceneOf0(L, near, meantBack); };
    let got: Lost[] = [];
    S.say = (what: string, _u: unknown, _t: string, extra?: unknown) => { if (what === 'air:lost') got.push(extra as Lost); };
    S.shownSay = () => {};
    const saved = over === 'own' ? AIR.pilots.home : AIR.pilots.away, rng = new Rng(seed);
    let meant = 0, not = 0, notShown = 0, fid = 1, tries = 0;
    while (meant < want && tries++ < 400_000) {
      S.recent = [];
      got = [];
      S.fated([
        { u: a, fall: drawFall(rng, 'ftr', over, saved), by: b.id, s: null, alt: SKY.bands.alt[band], fid: fid++, seat: -1, gun: null, models: 1, saved },
        { u: b, fall: drawFall(rng, 'bmr', over === 'own' ? 'enemy' : 'own', 0), by: a.id, s: null, alt: SKY.bands.alt[band], fid: fid++, seat: -1, gun: null, models: 1, saved: 0 },
      ]);
      for (const e of got) if (e.air === 'ftr' && e.how === 'collide' && e.meant) { meant++; if (!e.back) { not++; notShown += e.chutes + (e.out ?? 0); } }
    }
    return { saved, carried, meant, not, notShown };
  };
  for (const over of ['own', 'enemy'] as const) it(`the B the pairing hands the look, ${over} ground: every ram on purpose carries B by the formula over the pairing's own weights, the count's own share for its fall`, () => {
    const t = ramsThrough(over, 1, 500, 4_200 + (over === 'own' ? 0 : 1));
    expect(t.meant).toBeGreaterThanOrEqual(500);
    expect(t.carried.length).toBe(t.meant);
    for (const B of t.carried) expect(Math.abs(B - t.saved), `${over}: B ${B} against the count's share ${t.saved}`).toBeLessThan(1e-12);
  });
  // The 20,000 are drawn with the table's shares at the test figure 1 (as the Soviet row above), so that every pair the
  // pairing weighs is a ram on purpose: the same 20,000 rams in a fifth of the draws (at the row's 0.2 it ran 85 s at a
  // load of 31). The pairing weighs a pilot brought back and one not the same at any share, so B is the same.
  it('through fated(), the enemy\'s ground, high band (the one cell where R > 0), 20,000 rams on purpose (the table\'s shares at the test figure 1): a rammer not brought back shown out at R', () => {
    const t = withBook(() => { SKY.fate.ram.meant = SKY.fate.ram.meant.map((m) => ({ ...m, p: 1 })); }, () => ramsThrough('enemy', 2, 20_000, 4_300));
    expect(t.carried.length).toBe(t.meant);
    for (const B of t.carried) expect(Math.abs(B - t.saved)).toBeLessThan(1e-12);
    const S = Math.min(0.95, F.out.collide * F.outBand[2]), R = restOut(S, t.saved, 1);
    console.log(`(j) through fated(), enemy high: rams on purpose ${t.meant}, not brought back ${t.not}, shown out ${t.notShown} (${(t.notShown / t.not).toFixed(4)} against R ${R.toFixed(4)})`);
    expect(t.meant).toBeGreaterThanOrEqual(20_000);
    expect(R).toBeGreaterThan(0);
    near(t.notShown, t.not, R, 'enemy high, not brought back');
  });
});

// The design director, 01:00Z, reading 5: "the landing's 0 for a crew of 2 must be the crew rule's own sized figure. If it
// is an empty cell falling to 0, mark it G, read it at both ends, and the historian sizes it." It is: the landing's one
// die, int(max(1, ceil(crew / 3))), was sized by no source and for a crew of 2 runs out at 0. So the hole H-LAND-TWO
// (book.ts SKY.fate.landTwo, G) holds today's 0, read by the crew's rule for a crew of 2 only, with the switch on.
describe('(k) the design director\'s 01:00Z, reading 5: the landing\'s figure for a crew of two, a named hole read at both ends', () => {
  const glides = () => HOWS.map((x) => (x === 'glide' ? 1 : 0));
  it('the hole holds today\'s value, 0, and at 0 the landing is die for die what the formula gives, every crew, switch on and off', () => {
    expect(SKY.fate.landTwo).toBe(0);
    for (const crew of [2, 3, 4, 5, 7, 10]) for (const bailouts of [false, true]) for (let seed = 1; seed <= 300; seed++) {
      withBook(() => { SKY.fate.base.bmr = glides(); }, () => {
        const f = fateOf(new Rng(seed), scene({ crew, bailouts }), freshMemory());
        const g = fateOf(new Rng(seed), scene({ crew, bailouts: false }), freshMemory());
        expect(f).toEqual(g);
      });
    }
  });
  it('read at both ends (0: nobody of two lost; 1: one of the two always lost): the Fulmar\'s second man after a glide, and a two-man bomber\'s glide; crews of 3, 4, 5 and 10 and the switch off the same at both ends', () => {
    const N = 4_000;
    const ends = (v: number) => withBook(() => { SKY.fate.landTwo = v; SKY.fate.base.ftr = glides(); SKY.fate.base.bmr = glides(); }, () => {
      let second = 0, dive = 0;
      const others: unknown[] = [];
      for (let seed = 1; seed <= N; seed++) {
        const over = seed % 2 ? 'own' as const : 'enemy' as const;
        // The Fulmar, pilot not brought back (a glide: B 0), crew 1 and 2 on the same dice: the rest is the second man.
        const one = fateOf(new Rng(seed), fighter({ model: 'Fulmar', crew: 1, over }), freshMemory());
        const two = fateOf(new Rng(seed), fighter({ model: 'Fulmar', crew: 2, over }), freshMemory());
        expect(two.how).toBe('glide');
        second += (two.out ?? 0) + two.chutes - (one.out ?? 0) - one.chutes;
        const b = fateOf(new Rng(seed), scene({ crew: 2 }), freshMemory());
        expect(b.how).toBe('glide');
        dive += b.out ?? 0;
        if (seed <= 300) for (const crew of [3, 4, 5, 10]) others.push(fateOf(new Rng(seed), scene({ crew }), freshMemory()));
        if (seed <= 300) others.push(fateOf(new Rng(seed), scene({ crew: 2, bailouts: false }), freshMemory()));
      }
      return { second, dive, others };
    });
    const lo = ends(0), hi = ends(1);
    console.log(`(k) H-LAND-TWO at 0: the Fulmar's second man out of ${lo.second}/${N} glides, a two-man bomber's men out ${lo.dive}/${2 * N}; at 1: ${hi.second}/${N}, ${hi.dive}/${2 * N}`);
    // At 0 (today) nobody of two is lost: he always climbs out, and both bomber men do (out.glide 0: nobody jumps).
    expect([lo.second, lo.dive]).toEqual([N, 2 * N]);
    // At 1 one of the two is always lost: the second man never climbs out, a two-man bomber brings one man out.
    expect([hi.second, hi.dive]).toEqual([0, N]);
    // Every other crew, and the switch off, the same at both ends.
    expect(hi.others).toEqual(lo.others);
  });
});
