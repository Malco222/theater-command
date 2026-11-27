// The shelling scene (sea-scene/shore-guns/plan.md, rounds R0 and R1): the stager, unreal/TheaterCommand/Rules/shore-scene.ts.
// Look only: it reads a ShoreReport (and the map's coast) and writes a staged recording in the sea scene's recording
// format. These hold the plan's R1 tests: it is deterministic; it fixes the outcome from the report first (the share
// clamped, the outcome table for troops, town and city, names only from the hull rows, bores 76-460 mm); the cast (no
// carrier or submarine, none or one battleship, fifty destroyers within the caps); drift (the map changed since the
// shelling changes nothing); nothing on screen it must not show (no balance, no digit, percent or "you" in the words);
// its shape (30-45 s, windows at pace, shots in their windows, under 2 MB, the names SeaFeed.cpp reads); the coast of
// every coastal province and sea; picking the month's scene (the UI director's order, night and gale skipped, allies
// never, TC.subScene's bytes when none is picked); and that it draws no die and changes no game and no save.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { NAVY, fleetName, keptAfloat, wholeShares } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { hullAt, type HullRow } from '../src/campaign/hulls';
import { Rng, hashString } from '../src/core/rng';
import { WEATHER } from '../src/campaign/weather';
import type { Game, Ships } from '../src/campaign/types';
import LOOKS from '../unreal/TheaterCommand/Rules/shore-scene-looks.json';
import HULL_ROWS from '../data/campaign/hulls.json';
import FLEET_ROWS from '../data/campaign/fleets.json';
import SIEGE_ROWS from '../data/campaign/siege.json';
import seaFeedText from '../unreal/TheaterCommand/Source/TheaterCommand/Battle/SeaFeed.cpp?raw';
import { subSceneOf } from '../unreal/TheaterCommand/Rules/sub-scene';
import {
  MONTH_ORDER, SHOW_FIRST, bandShare, boreOf, captionOf, coastOf, cruiserMix, firedOf, fortOf, keptFromWords, keptOf, layCoast,
  monthSeaOf, offLand, onLand, seedOf, shoreFactsOf, shoreRecord, shoreSceneOf, shoreScenesOf, stageShore, struckOf, struckShips, sunkOf,
  type Chart, type GunCls, type LookedReport, type ShoreFacts, type ShoreNote, type ShoreShow, type Staged,
} from '../unreal/TheaterCommand/Rules/shore-scene';
import { SAMPLES, sampleFacts, sampleGame, sampleReport, turnOf, type ShoreSample } from '../unreal/TheaterCommand/Tools/shore_scene/sample-reports';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
type X = Record<string, any>;
interface Ev { t: number; what: string; unit?: number; side?: string; x?: number; y?: number; cue?: string; text: string; extra?: X }
type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

const L = LOOKS as unknown as {
  beats: { pace: number; least: number; most: number; after: [number, number]; fire0: number; fireEnd: number };
  bands: { bb: [number, number]; ca: [number, number]; dd: [number, number]; clear: number; speed: number; spacing: Record<GunCls, number>; column: number };
  classes: Record<GunCls, { len: number }>;
  most: { bb: number; ca: number; dd: number; divs: number; fires: number };
  hits: { dd: number; ca: number; kept: Record<'ca' | 'dd', number>; struck: { hits: number; per: number } };
  cadence: Record<GunCls, number>;
  guns: { navies: Record<string, unknown> };
  town: Record<string, { r: number }>;
};
const BOMBARD = (FLEET_ROWS as unknown as { bombard: { troops: { max: number }; city: { max: number } } }).bombard;
const BREACH = (SIEGE_ROWS as unknown as { breach: { shell: number; most: number } }).breach;
const HULLS = HULL_ROWS as unknown as { navies: Record<string, { bb?: HullRow[] }> };

/** The closing words' second line, as the UI director's marks of 26 November 13:42Z give them. */
const WORDS = {
  troops: ['The guns found little to hit.', 'The shelling did them little harm.', 'The shelling shook them.', 'The shelling left them badly shaken.'],
  town: ['The shells broke down little of them.', 'The shells broke down part of them.', 'The shells broke down much of them.', 'There was little left to break.'],
  city: ['The guns found little to hit.', 'The shelling stopped little of their output for a month.', 'The shelling stopped part of their output for a month.', 'The shelling stopped much of their output for a month.'],
  fort: 'The shore batteries answered, but hit no ship.',
  // (battleships struck, never sunk: the rules' own words, without the readiness figure; awaiting the UI director's marks)
  struck: ['The battleship was hit, but not sunk.', 'The battleships were hit, and none was sunk.'],
};
/** Odds or a share in words: never on screen (as no digit and no percent sign). */
const NO_ODDS = /per ?cent|percent|odds|chance|likely|probab/i;
const ALL: ShoreShow = { kinds: ['troops', 'town', 'city'], coast: true };
const SHOT_KINDS = ['SeaLine', 'Broadside', 'BehindTurret', 'HillEye', 'SpotterEye', 'FortEye', 'ShoreGlass', 'Hit', 'Sinking', 'SeaWide'];

const ships = (s: Partial<Ships>): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...s });
const sampleNamed = (file: string): ShoreSample => SAMPLES.find((s) => s.file === file)!;
const SHOWN = SAMPLES.filter((s) => !s.skip);
const games = new Map<string, Game>();
/** The default chart for a sample's player, opened once (the stager only reads it). */
const gameFor = (human: string): Game => { let g = games.get(human); if (!g) games.set(human, (g = sampleGame(human))); return g; };
const factsFor = (s: ShoreSample, id = 1): ShoreFacts => sampleFacts(gameFor(s.human), s, id);
const staged = new Map<string, Staged>();
/** A sample staged (once). */
function stagedOf(s: ShoreSample): Staged {
  let r = staged.get(s.file);
  if (!r) {
    const out = stageShore(factsFor(s));
    if (!out) throw new Error(`${s.file}: no scene`);
    staged.set(s.file, (r = out));
  }
  return r;
}
const noteOf = (r: Staged): ShoreNote => r.field.staged as ShoreNote;
const eventsOf = (r: Staged): Ev[] => r.views.flatMap((v) => (v.events as Ev[]) ?? []);
/** Every piece as first shown (whole). */
function piecesOf(r: Staged): Map<number, X> {
  const out = new Map<number, X>();
  for (const v of r.views) for (const u of v.units as X[]) if (!out.has(u.id)) out.set(u.id, u);
  return out;
}
const coastOfRec = (r: Staged): X => (r.field as X).coast;
/** The report's share on target: look.outcome.share as it is, else the report's own figures (bandShare). */
const shareOn = (rep: LookedReport): number => (rep.look ? Math.max(0, Math.min(1, rep.look.outcome.share)) : bandShare(rep));
/** The shells on target the plan's R1.2 point 4 asks: round(N(0.25 + 0.5 s)) when s > 0, none when the cut is 0. */
const onWanted = (rep: LookedReport, shells: number): number => (shareOn(rep) > 0 && (rep.what !== 'troops' || rep.cut > 0) ? Math.round(shells * (0.25 + 0.5 * shareOn(rep))) : 0);
/** Whether a point lies in a strip (its length along `face` degrees, its width across), to a metre or two. */
function inStrip(p: [number, number], s: X): boolean {
  const fa = (s.face * Math.PI) / 180, dx = p[0] - s.x, dy = p[1] - s.y;
  const a = dx * Math.sin(fa) + dy * Math.cos(fa), b = dx * Math.cos(fa) - dy * Math.sin(fa);
  return Math.abs(a) <= s.len / 2 + 2 && Math.abs(b) <= s.wide / 2 + 2;
}
/** A text's length and a 53-bit digest of it (cyrb53, as tests/sub-scene-d1.test.ts), to compare saves by. */
function digest(s: string): string {
  let h1 = 0xdeadbeef, h2 = 0x41c6ce57;
  for (let i = 0; i < s.length; i++) { const c = s.charCodeAt(i); h1 = Math.imul(h1 ^ c, 2654435761); h2 = Math.imul(h2 ^ c, 1597334677); }
  h1 = Math.imul(h1 ^ (h1 >>> 16), 2246822507) ^ Math.imul(h2 ^ (h2 >>> 13), 3266489909);
  h2 = Math.imul(h2 ^ (h2 >>> 16), 2246822507) ^ Math.imul(h1 ^ (h1 >>> 13), 3266489909);
  return `${s.length}:${(4294967296 * (2097151 & h2) + (h1 >>> 0)).toString(36)}`;
}

/** The plan's outcome table (R3), read from a staged recording against its report. */
function expectTable(r: Staged, rep: LookedReport): void {
  const ev = eventsOf(r), note = noteOf(r), d = note.drawn;
  const pieces = piecesOf(r);
  const battery = ev.filter((e) => e.what === 'sea:battery'), sunkEv = ev.filter((e) => e.what === 'sea:sunk'), hits = ev.filter((e) => e.what === 'sea:gunHit');
  const fort = fortOf(rep);
  // No fort: no battery round, no ship hit or sunk.
  if (!(fort.level > 0 && fort.fired)) {
    expect([battery.length, sunkEv.length, hits.length, d.batteryRounds]).toEqual([0, 0, 0, 0]);
  } else {
    expect(battery.length).toBe(d.batteryRounds);
    expect(battery.length).toBeGreaterThanOrEqual(3);
    // short, over and straddle first, on the same ship
    expect(battery.slice().sort((a, b) => a.t - b.t).slice(0, 3).map((e) => e.extra!.kind)).toEqual(['short', 'over', 'straddle']);
  }
  expect(battery.filter((e) => e.extra!.kind === 'hit').length).toBe(hits.length);
  expect(d.batteryHits).toBe(hits.length);
  // Ships sunk by class as the report has them (within those drawn), each hit first as often as its class takes.
  const sunk = sunkOf(rep);
  expect(sunkEv.filter((e) => e.extra!.cls === 'ca').length).toBe(Math.min(sunk.ca ?? 0, d.ca));
  expect(sunkEv.filter((e) => e.extra!.cls === 'dd').length).toBe(Math.min(sunk.dd ?? 0, d.dd));
  expect(sunkEv.some((e) => e.extra!.cls === 'bb')).toBe(false);
  for (const e of sunkEv) {
    const before = hits.filter((h) => h.unit === e.unit && h.extra!.ship === e.extra!.ship && h.t < e.t);
    expect(before.length).toBeGreaterThanOrEqual(e.extra!.cls === 'dd' ? L.hits.dd : L.hits.ca);
  }
  // Kept afloat (or a battleship struck): hit once or twice, as many times as its class takes, a deck fire, never sunk
  // and never burning.
  const sunkKey = new Set(sunkEv.map((e) => `${e.unit}:${e.extra!.ship}`));
  const hitKeys = [...new Set(hits.map((h) => `${h.unit}:${h.extra!.ship}`))].filter((k) => !sunkKey.has(k));
  for (const k of hitKeys) {
    const n = hits.filter((h) => `${h.unit}:${h.extra!.ship}` === k).length;
    const cls = pieces.get(Number(k.split(':')[0]))!.kind as GunCls;
    expect(n).toBeGreaterThanOrEqual(1);
    expect(n).toBeLessThanOrEqual(2);
    expect(n).toBe(cls === 'bb' ? L.hits.struck.hits : L.hits.kept[cls]);
    expect(ev.some((e) => e.what === 'sea:fire' && `${e.unit}:${e.extra!.ship}` === k && e.extra!.kind === 'deck')).toBe(true);
  }
  expect(ev.some((e) => e.what === 'sea:burning')).toBe(false);
  const kept = keptOf(rep);
  const keptBy = (c: GunCls): number => hitKeys.filter((k) => pieces.get(Number(k.split(':')[0]))!.kind === c).length;
  if (fort.level > 0 && fort.fired) {
    expect(keptBy('ca')).toBe(Math.min(kept.ca ?? 0, d.ca - (sunk.ca ?? 0)));
    expect(keptBy('dd')).toBe(Math.min(kept.dd ?? 0, d.dd - (sunk.dd ?? 0)));
  }
  // Kept afloat: cruisers and destroyers only (as the rules keep them); battleships straddled at most, unless the
  // rules' look says they were struck (outcome.struck, the navy's ruling 3): hits and smoke then, never sunk.
  expect(d.kept).toEqual({ ca: keptBy('ca'), dd: keptBy('dd') });
  expect(keptBy('bb')).toBe(fort.level > 0 && fort.fired ? struckShips(rep, d.bb) : 0);
  expect(d.struck).toBe(keptBy('bb'));
  if (!rep.look || struckOf(rep) <= 0) expect(keptBy('bb')).toBe(0);
  // Falls: the shells on target as the share asks, by hash rank so the count is exact; in the strips or the town.
  const shore = ev.filter((e) => e.what === 'sea:shore');
  expect(shore.reduce((a, e) => a + e.extra!.n, 0)).toBe(d.shells);
  expect(shore.reduce((a, e) => a + e.extra!.lay.on, 0)).toBe(d.on);
  expect(d.on).toBe(onWanted(rep, d.shells));
  expect(d.on + d.sea + d.beach + d.over).toBe(d.shells);
  const c = coastOfRec(r);
  const falls = shore.flatMap((e) => (e.extra!.falls ?? []) as [number, number, string][]);
  expect(falls.length).toBe(d.points);
  for (const p of falls.filter((x) => x[2] === 'on')) {
    if (rep.what === 'troops') expect(c.target.strips.some((s: X) => inStrip([p[0], p[1]], s))).toBe(true);
    else expect(Math.hypot(p[0] - c.target.x, p[1] - c.target.y)).toBeLessThanOrEqual(c.target.r + 2);
  }
  // Ground fires: only with s > 0; none on concrete; at most three on troops.
  const ground = ev.filter((e) => e.what === 'sea:fire' && e.unit === undefined);
  if (bandShare(rep) <= 0 || d.on === 0) expect(ground.length).toBe(0);
  if (rep.what === 'troops') {
    expect(ground.length).toBeLessThanOrEqual(L.most.fires);
    for (const e of ground) expect(c.target.strips.filter((s: X) => s.kind === 'fort').some((s: X) => inStrip([e.extra!.x, e.extra!.y], s))).toBe(false);
  }
}

describe('the shelling scene (shore guns R1): determinism', () => {
  it('stages the same report to the same bytes; another seed moves the looks and never the outcome; the seed is look.seed or the hash', () => {
    for (const s of SHOWN) {
      const a = JSON.stringify(stagedOf(s));
      // (a game opened afresh, the same report: the same bytes)
      expect(JSON.stringify(stageShore(sampleFacts(sampleGame(s.human), s, 1)))).toBe(a);
      const f = factsFor(s);
      const other: ShoreFacts = f.rep.look ? { ...f, rep: { ...f.rep, look: { ...f.rep.look, seed: (f.rep.look.seed ^ 0x5a5a5a5) >>> 0 } } } : { ...f, seed: f.seed + 7 };
      const r1 = stageShore(other);
      expect(r1, s.file).not.toBeNull();
      expect(JSON.stringify(r1)).not.toBe(a);
      const n0 = noteOf(stagedOf(s)), n1 = noteOf(r1!);
      // (every count: the ships, the losses, the salvos and shells, those on target and each other fall, the batteries'
      // rounds and hits, the strips, fires and palls; only where and when they fall moves)
      const outcome = (n: ShoreNote): unknown => ({ fired: n.drawn.fired, bb: n.drawn.bb, ca: n.drawn.ca, dd: n.drawn.dd, pieces: n.drawn.pieces, divisions: n.drawn.divisions,
        sunk: n.drawn.sunk, kept: n.drawn.kept, struck: n.drawn.struck, salvos: n.drawn.salvos, shells: n.drawn.shells, on: n.drawn.on, sea: n.drawn.sea,
        beach: n.drawn.beach, over: n.drawn.over, batteryRounds: n.drawn.batteryRounds, batteryHits: n.drawn.batteryHits,
        strips: n.drawn.strips, fires: n.drawn.fires, palls: n.drawn.palls });
      expect(outcome(n1), s.file).toEqual(outcome(n0));
      expect(n1.caption).toEqual(n0.caption);
      const { hour: _h0, ...facts0 } = n0.facts, { hour: _h1, ...facts1 } = n1.facts;
      expect(facts1).toEqual(facts0);
      for (const n of [n0, n1]) expect(n.drawn.on).toBe(onWanted(f.rep, n.drawn.shells));
      // the seed: look.seed as it is (the navy's hash), else hash32 of "<g.seed>:<turn>:shore:<id>"
      expect(stagedOf(s).seed).toBe(seedOf(f));
      if (f.rep.look) expect(stagedOf(s).seed).toBe(f.rep.look.seed >>> 0);
    }
    // Without a look, the seed is the campaign's, the month's and the report's: another report id moves it.
    const azores = factsFor(sampleNamed('shore-volcanic-island-open.json'));
    expect(seedOf(azores)).not.toBe(seedOf({ ...azores, rep: { ...azores.rep, id: 2 } }));
  });

  it('fires look.salvos exactly where the look gives it: shared over every ship that fired by her rate, whole, the drawn ships their own shares, each inside the guns\' span, whatever the seed', () => {
    const fire0 = L.beats.fire0, fireEnd = L.beats.fireEnd;
    let whole = 0;
    for (const s of SHOWN) {
      const f = factsFor(s);
      if (!f.rep.look) continue;
      const fired = firedOf(f.rep);
      for (const salvos of [7, 40, 100, 399, 400, 1000]) {
        const rep = { ...f.rep, look: { ...f.rep.look, salvos } } as LookedReport;
        const r = stageShore({ ...f, rep });
        expect(r, `${s.file} ${salvos}`).not.toBeNull();
        const d = noteOf(r!).drawn;
        // (the shares as the plan's R1.2 point 4 asks: by class by rate, then ship by ship; the ships drawn are each
        // class's first, a division's ships in turn)
        const byClass = wholeShares(salvos, [fired.bb / L.cadence.bb, fired.ca / L.cadence.ca, fired.dd / L.cadence.dd]);
        const per = (['bb', 'ca', 'dd'] as GunCls[]).map((c, i) => wholeShares(byClass[i], Array.from({ length: fired[c] }, () => 1)));
        const want = per[0].slice(0, d.bb).reduce((a, x) => a + x, 0) + per[1].slice(0, d.ca).reduce((a, x) => a + x, 0) + per[2].slice(0, d.dd).reduce((a, x) => a + x, 0);
        expect(d.salvos, `${s.file} ${salvos}`).toBe(want);
        if (d.bb === fired.bb && d.ca === fired.ca && d.dd === fired.dd) { expect(d.salvos).toBe(salvos); whole++; }
        const shore = eventsOf(r!).filter((e) => e.what === 'sea:shore');
        expect(shore.length).toBe(d.salvos);
        for (const e of shore) { expect(e.t).toBeGreaterThanOrEqual(fire0); expect(e.t).toBeLessThanOrEqual(fireEnd); }
        expectTable(r!, rep);
        // another seed: the same counts
        const other = stageShore({ ...f, rep: { ...rep, look: { ...rep.look!, seed: (rep.look!.seed ^ 0x2468ace) >>> 0 } } })!;
        const n1 = noteOf(other).drawn;
        expect([n1.salvos, n1.shells, n1.on, n1.sea, n1.beach, n1.over, n1.batteryRounds, n1.batteryHits]).toEqual([d.salvos, d.shells, d.on, d.sea, d.beach, d.over, d.batteryRounds, d.batteryHits]);
      }
    }
    // (the samples where every ship that fired is drawn give look.salvos itself)
    expect(whole).toBeGreaterThan(0);
    // Without look.salvos, at the cadence: every salvo inside the guns' span too.
    for (const s of SHOWN) for (const e of eventsOf(stagedOf(s)).filter((x) => x.what === 'sea:shore')) { expect(e.t).toBeGreaterThanOrEqual(fire0); expect(e.t).toBeLessThanOrEqual(fireEnd); }
  });
});

describe('the shelling scene (shore guns R1): the outcome first', () => {
  it('clamps the share s to 0-1 (a cut of 1.15 times the most) and words the result in thirds', () => {
    const tm = BOMBARD.troops.max, cm = BOMBARD.city.max;
    const az = factsFor(sampleNamed('shore-volcanic-island-open.json'));
    const yo = factsFor(sampleNamed('shore-city-far-inland.json'));
    const tb = factsFor(sampleNamed('shore-north-africa-battleship.json'));
    expect(bandShare({ ...az.rep, cut: 1.15 * tm })).toBe(1);
    expect(bandShare({ ...az.rep, cut: -0.1 })).toBe(0);
    expect(bandShare({ ...yo.rep, output: 1.15 * cm })).toBe(1);
    expect(bandShare({ ...tb.rep, works: BREACH.most })).toBe(1);
    expect(bandShare({ ...tb.rep, works: undefined })).toBe(0);
    const line2 = (f: ShoreFacts, over: Partial<LookedReport>): string => captionOf({ ...f, rep: { ...f.rep, ...over } }, 'att')[1];
    // troops: nothing (cut 0), under a third, a third to two thirds, two thirds and over (the cut over the most clamped)
    expect([0, 0.2, 0.5, 0.8, 1.15].map((x) => line2(az, { cut: x * tm }))).toEqual([WORDS.troops[0], WORDS.troops[1], WORDS.troops[2], WORDS.troops[3], WORDS.troops[3]]);
    // a town's works over the breach a shelling makes at most; works already at the most
    const sh = BREACH.shell;
    expect([0.1, 0.5, 0.9, 1.5].map((x) => line2(tb, { works: x * sh }))).toEqual([WORDS.town[0], WORDS.town[1], WORDS.town[2], WORDS.town[2]]);
    expect(line2(tb, { works: 0, result: 'The works of Tobruk were already broken down as far as shells can break them.' })).toBe(WORDS.town[3]);
    // a city: output 0, then thirds
    expect([0, 0.2, 0.5, 0.8, 1.15].map((x) => line2(yo, { output: x * cm }))).toEqual([WORDS.city[0], WORDS.city[1], WORDS.city[2], WORDS.city[3], WORDS.city[3]]);
    // the stager's staged share is clamped too
    const big = stageShore({ ...az, rep: { ...az.rep, cut: 1.15 * tm } })!;
    expect(noteOf(big).facts.share).toBe(1);
    expect(noteOf(big).drawn.on).toBe(Math.round(noteOf(big).drawn.shells * 0.75));
  });

  it('writes the outcome table for every sample, troops, town and city, with and without a fort, a loss or a ship kept', () => {
    for (const s of SHOWN) expectTable(stagedOf(s), factsFor(s).rep);
    // The samples between them hold every row: no fort; a fort that sank none; ships sunk; a cruiser and a battleship kept.
    const reps = SHOWN.map((s) => factsFor(s).rep);
    expect(reps.some((r) => fortOf(r).level === 0)).toBe(true);
    expect(reps.some((r) => fortOf(r).level > 0 && !(sunkOf(r).dd ?? 0) && !(sunkOf(r).ca ?? 0) && !(keptOf(r).ca ?? 0) && !(keptOf(r).dd ?? 0) && !struckOf(r))).toBe(true);
    expect(noteOf(stagedOf(sampleNamed('shore-normandy-dunes-village.json'))).drawn).toMatchObject({ sunk: { ca: 0, dd: 1 }, kept: { ca: 1, dd: 0 }, struck: 0 });
    expect(noteOf(stagedOf(sampleNamed('shore-north-africa-battleship.json'))).drawn).toMatchObject({ sunk: { ca: 0, dd: 0 }, kept: { ca: 0, dd: 0 }, struck: 1 });
    expect(noteOf(stagedOf(sampleNamed('shore-med-harbour-cruisers.json'))).drawn).toMatchObject({ batteryHits: 0, sunk: { ca: 0, dd: 0 } });
    // Troops with a cut of 0: nothing on target ("The guns found little to hit"), no fire.
    const az = factsFor(sampleNamed('shore-volcanic-island-open.json'));
    const none = stageShore({ ...az, rep: { ...az.rep, cut: 0 } })!;
    expect(noteOf(none).drawn.on).toBe(0);
    expect(noteOf(none).caption.lines[1]).toBe(WORDS.troops[0]);
    expectTable(none, { ...az.rep, cut: 0 });
    // A city whose output nothing stopped: nothing on target and no fire.
    const yo = factsFor(sampleNamed('shore-city-far-inland.json'));
    const idle = stageShore({ ...yo, rep: { ...yo.rep, output: 0 } })!;
    expect([noteOf(idle).drawn.on, noteOf(idle).drawn.fires, noteOf(idle).drawn.palls]).toEqual([0, 0, 0]);
    expectTable(idle, { ...yo.rep, output: 0 });
    // Fortress troops: some strips under concrete, and no fire on them.
    const coral = stagedOf(sampleNamed('shore-coral-island-carriers.json'));
    expect(coastOfRec(coral).target.strips.some((x: X) => x.kind === 'fort')).toBe(true);
    // A battleship struck only where the look's outcome.struck says: without a look, a battleship in the hit words is not
    // drawn hit; with a look, a battleship in its kept (which the rules never fill) is not either, and struck 0 draws none.
    const tb = factsFor(sampleNamed('shore-north-africa-battleship.json'));
    const { look: _l, ...bare } = tb.rep;
    const noLook = stageShore({ ...tb, rep: { ...bare, result: `${bare.result} A battleship of the Mediterranean Fleet was badly hit, but the repair parties kept the ship afloat.` } as LookedReport })!;
    expect(noteOf(noLook).drawn.struck).toBe(0);
    expect(noteOf(noLook).drawn.kept).toEqual({ ca: 0, dd: 0 });
    expectTable(noLook, { ...bare } as LookedReport);
    const keptBb = { ...tb.rep, look: { ...tb.rep.look!, outcome: { ...tb.rep.look!.outcome, kept: { bb: 1 }, struck: undefined } } } as LookedReport;
    const notStruck = stageShore({ ...tb, rep: keptBb })!;
    expect(noteOf(notStruck).drawn).toMatchObject({ kept: { ca: 0, dd: 0 }, struck: 0, batteryHits: 0 });
    expectTable(notStruck, keptBb);
    expect(noteOf(notStruck).caption.lines[2]).toBe(WORDS.fort);
  });

  it('names battleships only from the hull rows of the report\'s month or look.fired, and none in a scene of his coast', () => {
    for (const s of SHOWN) {
      const f = factsFor(s), r = stagedOf(s);
      const me = (r.views[0] as X).me as 'att' | 'def';
      const rows = (HULLS.navies[f.rep.nat]?.bb ?? []).map((h) => hullAt(h, f.rep.turn).name);
      const fired = (f.rep.look?.fired ?? []).filter((x) => x.cls === 'bb').map((x) => x.name).filter((x): x is string => !!x);
      const bbs = [...piecesOf(r).values()].filter((u) => u.kind === 'bb').map((u) => u.name as string);
      for (const n of bbs) {
        if (me === 'def' || /^Battleship \d+$/.test(n)) { expect(n, s.file).toMatch(/^Battleship \d+$/); continue; }
        expect(f.rep.look ? fired : rows, s.file).toContain(n);
      }
      // The words name no ship: no battleship's name in the caption, nor in a scene of his coast in any event's text.
      const names = [...new Set([...rows, ...fired])];
      for (const n of names) expect(noteOf(r).caption.lines.join(' ')).not.toContain(n);
      if (me === 'def') for (const e of eventsOf(r)) for (const n of names) expect(e.text).not.toContain(n);
    }
    // Without a look, his own battleships are named from the hull rows as they stood that month.
    const az = sampleNamed('shore-volcanic-island-open.json');
    const f = factsFor(az);
    expect(f.hulls.length).toBeGreaterThan(0);
    const bbs = [...piecesOf(stagedOf(az)).values()].filter((u) => u.kind === 'bb').map((u) => u.name);
    expect(bbs).toEqual(f.hulls.slice(0, bbs.length));
  });

  it('gives every navy, class and year a bore of 76 to 460 mm, and holds a reported bore to that', () => {
    const navies = [...Object.keys(L.guns.navies).filter((k) => !k.startsWith('_')), 'HOL', 'CHN', 'POL'];
    for (const nat of navies) for (let year = 1939; year <= 1945; year++) for (const m of [1, 7]) {
      const turn = Math.max(1, turnOf(year, m));
      for (const cls of ['bb', 'ca', 'dd'] as GunCls[]) {
        const mixes = cls === 'ca' ? cruiserMix(nat, turn, 12).length : 1;
        for (let mixAt = 0; mixAt < mixes; mixAt++) {
          const b = boreOf(cls, nat, turn, null, mixAt);
          expect(b.mm, `${nat} ${cls} ${year}`).toBeGreaterThanOrEqual(76);
          expect(b.mm).toBeLessThanOrEqual(460);
          expect(b.guns).toBeGreaterThanOrEqual(1);
        }
      }
      for (const h of HULLS.navies[nat]?.bb ?? []) {
        const b = boreOf('bb', nat, turn, hullAt(h, turn).name);
        expect(b.mm, `${nat} ${hullAt(h, turn).name}`).toBeGreaterThanOrEqual(76);
        expect(b.mm).toBeLessThanOrEqual(460);
      }
    }
    // A bore the look reports past the bounds is held to them.
    const nv = sampleNamed('shore-arctic-fjord-destroyers.json');
    const f = factsFor(nv);
    const wide = stageShore({ ...f, rep: { ...f.rep, look: { ...f.rep.look!, fired: f.rep.look!.fired.map((x, i) => ({ ...x, mm: i % 2 ? 600 : 30 })) } } })!;
    for (const u of piecesOf(wide).values()) { expect(u.extra.mainMm).toBeGreaterThanOrEqual(76); expect(u.extra.mainMm).toBeLessThanOrEqual(460); }
  });

  it('reads the look as the navy built it (690599783): no bore or name stood in and said; spotting only from its air; cloud only under a closed sky; sea normal is the scene\'s own', () => {
    // No bore in look.fired (as the navy writes it until the historian's bores): the stand-in rows, said.
    const malta = stagedOf(sampleNamed('shore-med-harbour-cruisers.json'));
    const maltaF = factsFor(sampleNamed('shore-med-harbour-cruisers.json'));
    expect(maltaF.rep.look!.fired.every((x) => x.mm === undefined && x.guns === undefined && x.name === undefined)).toBe(true);
    const mix = cruiserMix('ITA', maltaF.rep.turn, 7);
    const mms = [...piecesOf(malta).values()].map((u) => u.extra.mainMm as number).sort((a, b) => a - b);
    const want = mix.flatMap((n, m) => Array.from({ length: n }, () => boreOf('ca', 'ITA', maltaF.rep.turn, null, m).mm)).sort((a, b) => a - b);
    expect(mms).toEqual(want);
    expect(noteOf(malta).standIns.some((x) => /cruisers' and destroyers' bores/.test(x))).toBe(true);
    // A battleship named but given no bore: her row by name (Warspite), said.
    const tob = stagedOf(sampleNamed('shore-north-africa-battleship.json'));
    expect([...piecesOf(tob).values()].map((u) => [u.name, u.extra.mainMm, u.extra.guns])).toEqual([['Warspite', 381, 8]]);
    expect(noteOf(tob).standIns.some((x) => /by her name/.test(x))).toBe(true);
    // Bores the look gives are taken as they are, and no bore stand-in is said.
    const nor = stagedOf(sampleNamed('shore-normandy-dunes-village.json'));
    expect([...piecesOf(nor).values()].map((u) => u.extra.mainMm)).toEqual([406, 406, 203, 203, 127]);
    expect(noteOf(nor).standIns.some((x) => /bore/.test(x))).toBe(false);
    // Spotting: the carriers' aircraft (Tarawa), the ships' floatplanes under a sky its side has (Tobruk, Normandy); none
    // where the look's air is null (Narvik) or the look says no spotting (Malta: shoreGuns off), and only then a sea:spot.
    const spotOf = (file: string): [string, boolean] => { const r = stagedOf(sampleNamed(file)); return [coastOfRec(r).air.spot, eventsOf(r).some((e) => e.what === 'sea:spot')]; };
    expect(spotOf('shore-coral-island-carriers.json')).toEqual(['carrier', true]);
    expect(spotOf('shore-north-africa-battleship.json')).toEqual(['floatplane', true]);
    expect(spotOf('shore-normandy-dunes-village.json')).toEqual(['floatplane', true]);
    expect(spotOf('shore-arctic-fjord-destroyers.json')).toEqual(['none', false]);
    expect(spotOf('shore-med-harbour-cruisers.json')).toEqual(['none', false]);
    expect(noteOf(malta).standIns.some((x) => /^spotting: none/.test(x))).toBe(true);
    expect(noteOf(tob).standIns.some((x) => /^spotting: none/.test(x))).toBe(false);
    // Cloud only under a closed sky (Narvik); a sky held or won is who has the air, so the sea scene's own; the sea
    // 'normal' writes no swell (the sea scene's own).
    expect((stagedOf(sampleNamed('shore-arctic-fjord-destroyers.json')).field as X).sky).toMatchObject({ cover: 0.9, kind: 'deck', word: 'closed' });
    for (const s of SHOWN) {
      const r = stagedOf(s), f = factsFor(s);
      expect('sea' in r.field, s.file).toBe(false);
      if (f.rep.look?.conditions.sky !== 'closed') expect('sky' in r.field, s.file).toBe(false);
    }
    // The battleships' readiness struck and whose order it was: kept in the facts, never drawn or worded.
    expect(noteOf(tob).facts).toMatchObject({ by: 'hand', struck: 0.04 });
    expect(noteOf(tob).caption.lines.join(' ')).not.toMatch(/hand|staff|struck/);
  });

  it('reads the ships kept afloat back from the rules\' own words (fleets.ts keptAfloat, past form)', () => {
    const cases: Partial<Ships>[] = [{ ca: 1 }, { dd: 1 }, { ca: 1, dd: 2 }, { dd: 12 }, { ca: 3 }, { bb: 1, ca: 2 }];
    for (const name of ['Atlantic Fleet', '1st Fleet', '2nd Escort Fleet', 'Mediterranean Fleet']) for (const c of cases) {
      const words = keptAfloat({ name }, ships(c), 'fleet', true);
      const result = `The shelling cut the organisation of the 2 corps at Normandy by 17%. The shore batteries sank a destroyer. ${words}`;
      const want = Object.fromEntries(Object.entries(c).filter(([, n]) => (n ?? 0) > 0));
      expect(keptFromWords(result), words).toEqual(want);
    }
    expect(keptFromWords('The shelling cut the organisation of the 1st Corps at Azores by 7%.')).toEqual({});
    // keptOf: cruisers and destroyers only, as the rules keep them (bombard lays the reply on ca and dd alone); a
    // battleship hit is the look's outcome.struck, one battleship drawn hit for each hits.struck.per of it, at most those drawn.
    const tb = factsFor(sampleNamed('shore-north-africa-battleship.json')).rep;
    expect(keptOf(tb)).toEqual({ ca: 0, dd: 0 });
    expect([struckOf(tb), struckShips(tb, 1), struckShips(tb, 0), struckShips(tb, 6)]).toEqual([0.04, 1, 0, 2]);
    const { look: _l, ...bare } = tb;
    expect(keptOf({ ...bare, result: keptAfloat({ name: 'Mediterranean Fleet' }, ships({ bb: 1 }), 'fleet', true) } as LookedReport)).toEqual({ ca: 0, dd: 0 });
    expect(struckOf(bare as LookedReport)).toBe(0);
    expect(keptOf({ ...tb, look: { ...tb.look!, outcome: { ...tb.look!.outcome, kept: { bb: 1, ca: 1 } } } } as LookedReport)).toEqual({ ca: 1, dd: 0 });
  });
});

describe('the shelling scene (shore guns R1): the cast', () => {
  it('draws no carrier or submarine; none or one battleship; fifty destroyers within the caps; drawn no more than fired, equal under the caps', () => {
    for (const s of SHOWN) {
      const f = factsFor(s), r = stagedOf(s), d = noteOf(r).drawn;
      const kinds = [...piecesOf(r).values()].map((u) => u.kind);
      expect(kinds.every((k) => k === 'bb' || k === 'ca' || k === 'dd'), s.file).toBe(true);
      const fired = firedOf(f.rep);
      expect(d.fired).toEqual(fired);
      expect(d.bb).toBe(Math.min(fired.bb, L.most.bb));
      expect(d.ca).toBe(Math.min(fired.ca, L.most.ca));
      expect(d.divisions).toBe(Math.min(Math.ceil(fired.dd / 4), L.most.dd));
      expect(d.dd).toBe(Math.min(fired.dd, d.divisions * 4));
      expect(d.pieces).toBe(d.bb + d.ca + d.divisions);
      expect(kinds.filter((k) => k === 'bb').length).toBe(d.bb);
      expect(kinds.filter((k) => k === 'ca').length).toBe(d.ca);
      for (const u of piecesOf(r).values()) if (u.kind === 'dd') expect(u.figures0).toBeLessThanOrEqual(4);
      // the heaviest class leads
      expect(piecesOf(r).get(1)!.kind).toBe(d.bb ? 'bb' : d.ca ? 'ca' : 'dd');
    }
    const coral = factsFor(sampleNamed('shore-coral-island-carriers.json'));
    expect([coral.rep.ships.cv, coral.rep.ships.ss]).toEqual([2, 2]);
    expect(firedOf(coral.rep)).toEqual({ bb: 2, ca: 3, dd: 8 });
    // Carriers and submarines never fire, without a look as well.
    const { look: _l, ...bare } = coral.rep;
    expect(firedOf(bare as LookedReport)).toEqual({ bb: 2, ca: 3, dd: 8 });
    expect(noteOf(stagedOf(sampleNamed('shore-med-harbour-cruisers.json'))).drawn).toMatchObject({ bb: 0, ca: 7, dd: 0, pieces: 7 });
    expect(noteOf(stagedOf(sampleNamed('shore-north-africa-battleship.json'))).drawn).toMatchObject({ bb: 1, ca: 0, dd: 0, pieces: 1 });
    const fifty = stagedOf(sampleNamed('shore-arctic-fjord-destroyers.json'));
    expect(noteOf(fifty).drawn).toMatchObject({ fired: { bb: 0, ca: 0, dd: 50 }, dd: 24, divisions: 6, pieces: 6 });
    expect(noteOf(fifty).standIns.some((x) => /out of frame/.test(x))).toBe(true);
  });
});

describe('the shelling scene (shore guns R1): drift', () => {
  it('stages from the report, not today\'s map: the coast retaken, the fort razed or the fleet\'s battleships lost since change nothing', () => {
    for (const file of ['shore-med-harbour-cruisers.json', 'shore-volcanic-island-open.json', 'shore-channel-chalk-his-coast.json', 'shore-north-africa-battleship.json']) {
      const s = sampleNamed(file);
      const g = sampleGame(s.human);
      const rep = sampleReport(g, s, 7);
      g.shoreReports.push(rep);
      const before = shoreSceneOf(g, 7);
      expect(before.recording, file).not.toBeNull();
      const r = g.regions[rep.region];
      // retaken by the shelling nation, its fort razed, and the fleet's battleships sunk the month after
      r.ctrl = rep.nat; r.owner = rep.nat; r.fort = null;
      (HULLS.navies[rep.nat]?.bb ?? []).forEach((h, i) => { if (hullAt(h, rep.turn).fleet === rep.fleet) g.flags[`hull:${rep.nat}:bb:${i}`] = rep.turn + 1; });
      for (const fl of g.fleets) if (fl.nat === rep.nat && fl.name === rep.fleet) fl.ships.bb = 0;
      g.turn = rep.turn + 2;
      const after = shoreSceneOf(g, 7);
      expect(after, file).toEqual(before);
      const rec = JSON.parse(after.recording!) as Staged;
      expect((rec.field as X).def.nat).toBe(rep.defNat);
      expect((rec.views[0] as X).me).toBe(rep.nat === s.human ? 'att' : 'def');
    }
  });
});

describe('the shelling scene (shore guns R1): never on screen', () => {
  it('shows no balance, and no digit, percent or "you" in its words but the date\'s and the fleet\'s name (1st Fleet, 2nd Escort Fleet)', () => {
    const variants: { f: ShoreFacts; fleet: string }[] = [];
    for (const s of SHOWN) {
      const f = factsFor(s);
      variants.push({ f, fleet: f.rep.fleet });
      for (const fleet of ['1st Fleet', '2nd Escort Fleet']) variants.push({ f: { ...f, rep: { ...f.rep, fleet } }, fleet });
    }
    for (const { f, fleet } of variants) {
      const r = stageShore(f)!;
      const me = (r.views[0] as X).me as 'att' | 'def';
      for (const v of r.views) expect('lost' in v).toBe(false);
      expect(r.views.slice(0, -1).every((v) => v.over === null)).toBe(true);
      const lines = noteOf(r).caption.lines;
      expect((r.views[r.views.length - 1].over as X).lines).toEqual(lines);
      expect(r.after.lines).toEqual(lines);
      const year = String((r.field as X).when.year);
      const text = lines.join(' ');
      // the fleet as fleetName gives it in his own scene, "the <Adj> <fleet>" in a scene of his coast
      expect(lines[0]).toContain(me === 'att' ? fleetName({ name: fleet }) : `the ${f.names[f.rep.nat].adj} ${fleet}`);
      expect(text.split(fleet).join('').split(year).join(''), text).not.toMatch(/\d/);
      expect(text).not.toContain('%');
      expect(text, text).not.toMatch(NO_ODDS);
      expect(text).not.toMatch(/\byou(r)?\b/i);
      for (const e of eventsOf(r)) {
        expect(e.text, e.text).not.toMatch(/\d|%/);
        expect(e.text, e.text).not.toMatch(NO_ODDS);
        expect(e.text).not.toMatch(/\byou(r)?\b/i);
      }
    }
    // The marks' own forms.
    expect(noteOf(stagedOf(sampleNamed('shore-normandy-dunes-village.json'))).caption.lines).toEqual([
      'The German troops in Normandy shelled by the Atlantic Fleet from the English Channel, June 1944, for the loss of a destroyer.',
      WORDS.troops[2],
      'A cruiser was badly hit, but the repair parties kept the ship afloat.',
    ]);
    expect(noteOf(stagedOf(sampleNamed('shore-med-harbour-cruisers.json'))).caption.lines).toEqual([
      'The defences of Valletta shelled by the 2nd Fleet from the Central Mediterranean, August 1940.',
      WORDS.town[0],
      WORDS.fort,
    ]);
    expect(noteOf(stagedOf(sampleNamed('shore-north-africa-battleship.json'))).caption.lines).toEqual([
      'The defences of Tobruk shelled by the Mediterranean Fleet from the Libyan Sea, January 1941.',
      WORDS.town[1],
      WORDS.struck[0],
    ]);
    expect(noteOf(stagedOf(sampleNamed('shore-city-far-inland.json'))).caption.lines).toEqual([
      'The factories and docks of Yorkshire shelled by the Battle Fleet from the North Sea, October 1940.',
      WORDS.city[3],
    ]);
    expect(noteOf(stagedOf(sampleNamed('shore-channel-chalk-his-coast.json'))).caption.lines[0]).toMatch(/^The British troops in Kent shelled by the German Battle Fleet from /);
    // no line 3 without a fort, nor where ships were sunk and none kept
    expect(noteOf(stagedOf(sampleNamed('shore-volcanic-island-open.json'))).caption.lines.length).toBe(2);
    expect(noteOf(stagedOf(sampleNamed('shore-coral-island-carriers.json'))).caption.lines.length).toBe(2);
  });
});

/** The text of a C++ function in SeaFeed.cpp, from its signature to its closing brace. */
function cppBody(sig: string): string {
  const at = seaFeedText.indexOf(sig);
  if (at < 0) throw new Error(`SeaFeed.cpp: no ${sig}`);
  let i = seaFeedText.indexOf('{', at), depth = 0;
  const from = i;
  for (; i < seaFeedText.length; i++) {
    if (seaFeedText[i] === '{') depth++;
    else if (seaFeedText[i] === '}' && --depth === 0) break;
  }
  return seaFeedText.slice(from, i + 1);
}
/** The names a C++ body reads, by the object each is read from (Var["a"]["b"]: a from Var's object, b from a's). */
function namesRead(body: string, objectOf: Record<string, string>): Map<string, Set<string>> {
  const out = new Map<string, Set<string>>();
  const add = (o: string, k: string): void => { let s = out.get(o); if (!s) out.set(o, (s = new Set())); s.add(k); };
  for (const m of body.matchAll(/\b([A-Za-z]+)((?:\["[A-Za-z]+"\])+)/g)) {
    const obj = objectOf[m[1]];
    if (!obj) continue;
    const keys = [...m[2].matchAll(/\["([A-Za-z]+)"\]/g)].map((x) => x[1]);
    add(obj, keys[0]);
    for (let k = 1; k < keys.length; k++) add(keys[k - 1], keys[k]);
  }
  return out;
}
const sorted = (s: Iterable<string> | undefined): string[] => [...(s ?? [])].sort();

describe('the shelling scene (shore guns R1): shape', () => {
  it('runs 30-45 s for every sample, windows at pace, shots inside them, under 2 MB', () => {
    for (const s of SHOWN) {
      const r = stagedOf(s), n = noteOf(r);
      const ids = new Set(piecesOf(r).keys());
      expect(n.seconds, s.file).toBeGreaterThanOrEqual(L.beats.least);
      expect(n.seconds, s.file).toBeLessThanOrEqual(L.beats.most);
      expect(n.beats.reduce((a, b) => a + b.screen, 0)).toBe(n.seconds);
      let last = -1;
      for (const b of n.beats) {
        // (SeaFeed.cpp keeps a window only in order, at least a tenth of a second long and given a tenth or more)
        expect(b.from, `${s.file} ${b.what}`).toBeGreaterThanOrEqual(last);
        expect(b.to - b.from).toBeGreaterThanOrEqual(0.1);
        expect(b.screen).toBeGreaterThanOrEqual(1);
        expect(b.to - b.from, `${s.file} ${b.what}`).toBeLessThanOrEqual(L.beats.pace * b.screen + 0.1);
        expect(b.shots.length).toBeGreaterThan(0);
        expect(b.shots.length).toBeLessThanOrEqual(16);
        for (const x of b.shots) {
          expect(x.at).toBeGreaterThanOrEqual(b.from);
          expect(x.at).toBeLessThanOrEqual(b.to);
          expect(SHOT_KINDS).toContain(x.kind);
          if (x.subject !== undefined) expect(ids.has(x.subject)).toBe(true);
        }
        last = b.to;
      }
      expect(n.beats[n.beats.length - 1].to).toBeLessThanOrEqual(r.after.seconds);
      expect(JSON.stringify(r).length).toBeLessThan(2 * 1024 * 1024);
    }
    // Every window kind of the shot list shows somewhere: the fort's, the glass, the hit and the sinking among them.
    const shots = new Set(SHOWN.flatMap((s) => noteOf(stagedOf(s)).beats.flatMap((b) => b.shots.map((x) => x.kind))));
    expect([...shots].sort()).toEqual([...SHOT_KINDS].sort());
  });

  it('writes exactly the names SeaFeed.cpp\'s shore reader reads: field.coast, the four events\' extras, the shots, a piece\'s mainMm and guns', () => {
    const recs = SHOWN.map(stagedOf);
    // field.coast and what it holds
    const coastRead = namesRead(cppBody('bool ReadCoast(const Json& J, Coast& Out)'), { C: 'coast', Town: 'town', Quay: 'quay', Target: 'target', S: 'strip', Fort: 'fort', B: 'battery', Air: 'air' });
    const written = new Map<string, Set<string>>();
    const add = (o: string, obj: X | undefined): void => { if (!obj) return; let s = written.get(o); if (!s) written.set(o, (s = new Set())); for (const k of Object.keys(obj)) s.add(k); };
    for (const r of recs) {
      const c = coastOfRec(r);
      add('coast', c); add('at', c.at); add('town', c.town); add('quay', c.town?.quay); add('target', c.target); add('fort', c.fort); add('air', c.air);
      for (const x of c.target.strips ?? []) add('strip', x);
      for (const x of c.fort.batteries) add('battery', x);
    }
    expect(sorted(written.keys())).toEqual(sorted(coastRead.keys()));
    for (const [o, keys] of coastRead) expect(sorted(written.get(o)), `field.coast ${o}`).toEqual(sorted(keys));
    // the events' extras: every name written is read; read and not written, only the battery's optional `dud`
    const evRead = namesRead(cppBody('bool ReadShoreEvent(const Json& E, ShoreEvent& Out)'), { E: 'event', X: 'extra', Lay: 'lay' });
    const extra = new Set<string>(), lay = new Set<string>();
    for (const r of recs) for (const e of eventsOf(r)) {
      if (!['sea:shore', 'sea:battery', 'sea:fire', 'sea:spot'].includes(e.what)) continue;
      for (const k of Object.keys(e.extra ?? {})) extra.add(k);
      for (const k of Object.keys(e.extra?.lay ?? {})) lay.add(k);
    }
    expect([...extra].filter((k) => !evRead.get('extra')!.has(k))).toEqual([]);
    expect([...evRead.get('extra')!].filter((k) => !extra.has(k))).toEqual(['dud']);
    expect(sorted(lay)).toEqual(sorted(evRead.get('lay')));
    expect(sorted(evRead.get('event'))).toEqual(['extra', 't', 'unit', 'what']);
    // the shots
    const shotRead = namesRead(cppBody('void ReadShots(const Json& J, std::vector<Shot>& Out)'), { S: 'shot' });
    const shotKeys = new Set(recs.flatMap((r) => noteOf(r).beats.flatMap((b) => b.shots.flatMap((x) => Object.keys(x)))));
    expect(sorted(shotKeys)).toEqual(sorted(shotRead.get('shot')));
    // a piece's bore and guns, read under the switch only (ReadView), and the coast and shots likewise (ReadRecording)
    const view = cppBody('bool ReadView(const Json& J, Memory& Kept, Frame& Out, bool bShore)');
    expect(view).toMatch(/bShore && X\["mainMm"\]/);
    expect(view).toMatch(/X\["guns"\]/);
    expect(view).toMatch(/bShore && ReadShoreEvent\(/);
    const rec = cppBody('bool ReadRecording(const std::string& Source, Recording& Out, std::string& OutError, bool bShore)');
    expect(rec).toMatch(/if \(bShore\) ReadShots\(B\["shots"\]/);
    expect(rec).toMatch(/if \(bShore\) ReadCoast\(J\["field"\]/);
    for (const r of recs) for (const u of piecesOf(r).values()) {
      expect(u.extra.mainMm).toBeGreaterThanOrEqual(75);
      expect(u.extra.mainMm).toBeLessThanOrEqual(460);
      expect(u.extra.guns).toBeGreaterThanOrEqual(1);
      expect(u.engaged).toBe(-1);
      expect('gun' in u).toBe(false);
    }
  });
});

describe('the shelling scene (shore guns R1): the coast', () => {
  it('lays every coastal province and each of its seas: a bearing, water to seaward, its own edge, the line 1.5 km off, batteries, strips and town ashore', () => {
    const g = startOpen(createGame(5, 'UK'));
    const T_END = L.beats.after[1];
    const half = (c: GunCls, n: number, tail = 0): number => Math.ceil((L.bands.speed * T_END) / 2 + ((n - 1) / 2) * L.bands.spacing[c] + tail + L.classes[c].len / 2 + 250);
    // (each class's track at the caps: six battleships, eight cruisers, six divisions of four destroyers)
    const H: Record<GunCls, number> = { bb: half('bb', L.most.bb), ca: half('ca', L.most.ca), dd: half('dd', L.most.dd, 3 * L.bands.column) };
    const hash = (s: string): number => { let x = 2166136261; for (let i = 0; i < s.length; i++) { x ^= s.charCodeAt(i); x = Math.imul(x, 16777619) >>> 0; } return x / 4294967296; };
    const bad: string[] = [], near: string[] = [];
    let pairs = 0;
    for (const r of g.regions) for (const z of r.seas ?? []) {
      pairs++;
      const name = `${r.name}/${g.seas[z].key}`;
      const c = coastOf(g as unknown as Chart, r.id, z);
      const why: string[] = [];
      if (!Number.isFinite(c.bearing)) { bad.push(`${name} bearing`); continue; }
      const ux = Math.sin((c.bearing * Math.PI) / 180), uy = Math.cos((c.bearing * Math.PI) / 180);
      if (offLand(c, c.at.x + ux * 300, c.at.y + uy * 300) <= 0) why.push('no water to seaward');
      if (offLand(c, c.at.x - ux * 300, c.at.y - uy * 300) >= 0) why.push('no land inland');
      if (c.how === 'near') near.push(name); else if (!c.ownEdge) why.push('not its own edge');
      const lay = layCoast(c, { classes: ['bb', 'ca', 'dd'], halves: H, bores: [152, 203, 254], strips: L.most.divs, town: { r: L.town.town.r, at: r.port ? 'waterfront' : 'behind' } }, (l) => hash(`${r.id}:${z}:${l}`));
      for (const k of ['bb', 'ca', 'dd'] as GunCls[]) {
        const line = lay.lines[k];
        if (!line) { why.push(`no ${k} line`); continue; }
        for (let s = line.s - H[k]; s <= line.s + H[k]; s += 100) {
          const x = c.at.x + ux * line.d + uy * s, y = c.at.y + uy * line.d - ux * s;
          const off = offLand(c, x, y);
          if (off < L.bands.clear) { why.push(`${k} line ${Math.round(off)} m off`); break; }
        }
      }
      for (const b of lay.batteries) if (!onLand(c.land, b.x, b.y)) why.push('battery afloat');
      for (const s of lay.strips) if (!onLand(c.land, s.x, s.y)) why.push('strip afloat');
      if (lay.town && !onLand(c.land, lay.town.x, lay.town.y)) why.push('town afloat');
      if (lay.batteries.length !== 3 || lay.strips.length !== L.most.divs || !lay.town) why.push('pieces missing');
      if (why.length) bad.push(`${name} (${c.how}): ${why.join(', ')}`);
    }
    expect(pairs).toBeGreaterThan(400);
    expect(bad).toEqual([]);
    // (one province's coast lies on its neighbour's polygon: its waterfront is the first water past it)
    expect(near.length).toBeLessThanOrEqual(1);
  }, 300_000);
});

/** A report for the picking games: a sample's, re-dated to this month, by and on the nations given. */
function pickRep(g: Game, file: string, id: number, o: { nat: string; defNat: string; hour?: number; sea?: 'normal' | 'gale' }): LookedReport {
  const s = sampleNamed(file);
  const rep = sampleReport(g, s, id);
  const look = rep.look ? { ...rep.look, conditions: { ...rep.look.conditions, hour: o.hour ?? 12, sea: o.sea ?? 'normal' } } : undefined;
  return { ...rep, turn: g.turn, nat: o.nat, defNat: o.defNat, ...(look ? { look } : {}) } as LookedReport;
}
const natl = (g: Game): string => g.seas.find((z) => z.key === 'natl')!.name;
const heavyLost = (g: Game): Game['headlines'][number] => ({ turn: g.turn, week: 1, text: `A submarine torpedoes and sinks a cruiser of the Home Fleet in the ${natl(g)}: the fleet has too few destroyers to screen its heavy ships.`, kind: 'battle', nat: 'UK', arm: 'sea', lost: 'UK' });
const huntNews = (g: Game): Game['headlines'][number] => ({ turn: g.turn, week: 2, text: `Escorts of the Western Approaches sink 2 German submarines in the ${natl(g)}.`, kind: 'battle', nat: 'UK', arm: 'sea', won: 'UK', lost: 'GER' });
const convoyNews = (g: Game): Game['headlines'][number] => ({ turn: g.turn, week: 3, text: 'The Atlantic convoys: 156,000 tons of shipping sunk this month, 2 submarines destroyed.', kind: 'info', nat: 'UK', arm: 'sea', lost: 'UK' });
/** Britain's month: his shelling with a destroyer lost (1), his shellings without loss (2, 3), his coast shelled (4), an
 * ally's shelling (5) and an ally's coast (6), his shelling by night (7) and in a gale (8), his town shelled (9). */
function britainMonth(): Game {
  const g = startOpen(createGame(5, 'UK'));
  g.shoreReports.push(
    pickRep(g, 'shore-normandy-dunes-village.json', 1, { nat: 'UK', defNat: 'GER' }),
    pickRep(g, 'shore-arctic-fjord-destroyers.json', 2, { nat: 'UK', defNat: 'GER' }),
    pickRep(g, 'shore-volcanic-island-open.json', 3, { nat: 'UK', defNat: 'POR' }),
    pickRep(g, 'shore-channel-chalk-his-coast.json', 4, { nat: 'GER', defNat: 'UK' }),
    pickRep(g, 'shore-arctic-fjord-destroyers.json', 5, { nat: 'FRA', defNat: 'GER' }),
    pickRep(g, 'shore-channel-chalk-his-coast.json', 6, { nat: 'GER', defNat: 'FRA' }),
    pickRep(g, 'shore-night.json', 7, { nat: 'UK', defNat: 'GER', hour: 23 }),
    pickRep(g, 'shore-gale.json', 8, { nat: 'UK', defNat: 'GER', sea: 'gale' }),
    pickRep(g, 'shore-north-africa-battleship.json', 9, { nat: 'UK', defNat: 'ITA' }),
  );
  return g;
}
const ids = (g: Game, o: Parameters<typeof shoreScenesOf>[1] = {}): number[] => shoreScenesOf(g, o).map((p) => p.id);
const same = (a: unknown, b: unknown): void => expect(JSON.stringify(a)).toBe(JSON.stringify(b));

describe('the shelling scene (shore guns R1): picking', () => {
  it('skips night and gale, never an ally\'s shelling or coast, and sorts by case, ships lost, share, first report', () => {
    const g = britainMonth();
    expect(shoreSceneOf(g, 7)).toMatchObject({ pick: { skip: 'night' }, recording: null });
    expect(shoreSceneOf(g, 8)).toMatchObject({ pick: { skip: 'gale' }, recording: null });
    expect(stageShore(factsFor(sampleNamed('shore-night.json')))).toBeNull();
    expect(stageShore(factsFor(sampleNamed('shore-gale.json')))).toBeNull();
    // (the first drawn round shows troops shelled by his nation; with every kind and his coast, the town and his coast too)
    expect(ids(g)).toEqual([1, 3, 2]);
    expect(ids(g, { show: SHOW_FIRST })).toEqual([1, 3, 2]);
    expect(shoreScenesOf(g, { show: ALL }).map((p) => [p.id, p.case])).toEqual([[1, 'shoreLoss'], [9, 'shore'], [3, 'shore'], [2, 'shore'], [4, 'shoreCoast']]);
    // within a case: the largest share first (Tobruk's look 0.35, the Azores' cut 0.07 of the most 0.3, Narvik's look
    // 0.12), ties by the first report
    expect(shoreScenesOf(g, { show: ALL }).filter((p) => p.case === 'shore').map((p) => Math.round(p.share * 1000))).toEqual([350, 233, 120]);
    g.shoreReports.push({ ...g.shoreReports[1], id: 10 });
    expect(ids(g)).toEqual([1, 3, 2, 10]);
    // a report already played, or one before the card's mark, is not picked
    expect(ids(g, { played: new Set([1, 2]) })).toEqual([3, 10]);
    expect(ids(g, { window: { from: g.shoreReports.length } })).toEqual([]);
    expect(ids(g, { window: { from: 4 } })).toEqual([10]);
    expect(ids(g, { window: { month0: g.turn + 1 } })).toEqual([]);
    // with no human, nothing
    expect(shoreScenesOf({ ...g, human: null } as unknown as Game)).toEqual([]);
  });

  it('picks in the UI director\'s order among the submarine scene\'s cases, and gives TC.subScene\'s answer where it picks none', () => {
    expect(MONTH_ORDER).toEqual(['heavyLost', 'shoreLoss', 'boatsKill', 'shore', 'convoy', 'shoreCoast', 'hunt']);
    const g = britainMonth();
    const sea = (o: Parameters<typeof monthSeaOf>[4] = {}, view: 'all' | 'mine' | 'never' = 'all') => monthSeaOf(g, NAVY.convoys, undefined, view, o);
    const sub = (view: 'all' | 'mine' | 'never' = 'all') => subSceneOf(g, NAVY.convoys, undefined, view);
    // His shelling with a ship lost leads; its bytes are its own scene's.
    const a = sea();
    expect(a.pick).toMatchObject({ case: 'shoreLoss', id: 1, me: 'att', what: 'troops' });
    expect(a.mark).toBe(g.headlines.length);
    expect(a.recording).toBe(shoreSceneOf(g, 1).recording);
    expect(sea({}, 'mine').pick).toMatchObject({ case: 'shoreLoss', id: 1 });
    // Played: his shelling, by share; MY LOSSES ONLY takes none of them (no ship lost), so TC.subScene's.
    expect(sea({ played: new Set([1]) }).pick).toMatchObject({ case: 'shore', id: 3 });
    same(sea({ played: new Set([1]) }, 'mine'), sub('mine'));
    same(sea({}, 'never'), sub('never'));
    // His largest hunt comes after his shellings and his coast shelled.
    g.headlines.push(huntNews(g));
    expect(sub().pick?.case).toBe('hunt');
    expect(sea({ played: new Set([1]) }).pick).toMatchObject({ case: 'shore', id: 3 });
    expect(sea({ played: new Set([1, 2, 3]), show: ALL }).pick).toMatchObject({ case: 'shore', id: 9 });
    expect(sea({ played: new Set([1, 2, 3, 9]), show: ALL }).pick).toMatchObject({ case: 'shoreCoast', id: 4, me: 'def' });
    same(sea({ played: new Set([1, 2, 3]) }), sub());
    // His convoy month comes after his shelling and before his coast shelled.
    g.headlines.push(convoyNews(g));
    expect(sub().pick?.case).toBe('convoy');
    expect(sea({ played: new Set([1]) }).pick).toMatchObject({ case: 'shore', id: 3 });
    same(sea({ played: new Set([1, 2, 3, 9]), show: ALL }), sub());
    // MY LOSSES ONLY takes his shelling with a ship lost, never his coast shelled.
    expect(sea({ show: ALL }, 'mine').pick).toMatchObject({ case: 'shoreLoss', id: 1 });
    same(sea({ played: new Set([1]), show: ALL }, 'mine'), sub('mine'));
    // His heavy ship lost at sea leads everything.
    g.headlines.push(heavyLost(g));
    expect(sub().pick?.case).toBe('heavyLost');
    same(sea({ show: ALL }), sub());
    same(sea({ show: ALL }, 'mine'), sub('mine'));
    // A coast scene is 'def': the fleet by its nation, the caption his troops'.
    const coast = JSON.parse(shoreSceneOf(g, 4).recording!) as Staged;
    expect((coast.views[0] as X).me).toBe('def');
    expect(noteOf(coast).caption.lines[0]).toMatch(/^The British troops in Kent shelled by the German Battle Fleet from /);
  });

  it('serves the bridge: TC.monthSea picks as the stager does, keeps a played report out, and gives TC.subScene\'s bytes where it picks none', () => {
    const g = britainMonth();
    g.headlines.push(huntNews(g));
    TC().load(saveGame(g));
    const first = TC().monthSea() as X;
    expect(first.pick).toMatchObject({ case: 'shoreLoss', id: 1 });
    expect(first.recording).toBe(monthSeaOf(g, NAVY.convoys).recording);
    // his SHELL order's scene, played at once: not picked again
    const one = TC().shoreScene(1) as X;
    expect(one.recording).toBe(first.recording);
    expect((TC().monthSea() as X).pick).toMatchObject({ case: 'shore', id: 3 });
    for (const id of [2, 3]) TC().shoreScene(id);
    expect((TC().shoreScene(7) as X).recording).toBeNull();
    same(TC().monthSea(), TC().subScene());
    expect((TC().monthSea(undefined, 'all', ALL) as X).pick).toMatchObject({ case: 'shore', id: 9 });
    // a game with only an ally's shellings, his coast (not drawn yet), night and gale: TC.subScene's bytes, every view and mark
    const h = startOpen(createGame(5, 'UK'));
    h.shoreReports.push(...britainMonth().shoreReports.filter((r) => [4, 5, 6, 7, 8].includes(r.id)));
    h.headlines.push(huntNews(h), convoyNews(h));
    TC().load(saveGame(h));
    for (const view of [undefined, 'all', 'mine', 'never'] as const) for (const from of [undefined, 0, h.headlines.length - 1, h.headlines.length]) {
      same(TC().monthSea(from, view), TC().subScene(from, view));
    }
    same(monthSeaOf(h, NAVY.convoys), subSceneOf(h, NAVY.convoys));
    // the 3D's scene record setting: on and off, outside the save
    const text = (TC().save() as X).text as string;
    expect(TC().shoreSceneOn(true)).toEqual({ on: true });
    expect(shoreRecord.on).toBe(true);
    expect((TC().save() as X).text).toBe(text);
    expect(TC().shoreSceneOn(false)).toEqual({ on: false });
    expect(shoreRecord.on).toBe(false);
  });
});

/**
 * Runs `fn` and says how many draws were made on every stream of dice (by the state it began in), and whether
 * Math.random was called (the damage-control test's helper, tests/damage-control-240.test.ts).
 */
function dice<T>(fn: () => T): { out: T; streams: Map<number, number>; random: number } {
  const next = Rng.prototype.next, random = Math.random;
  const seen = new Map<Rng, { start: number; n: number }>();
  let calls = 0;
  Rng.prototype.next = function (this: Rng): number {
    let s = seen.get(this);
    if (!s) seen.set(this, (s = { start: this.state, n: 0 }));
    s.n++;
    return next.call(this);
  };
  Math.random = () => { calls++; return random(); };
  try {
    const out = fn();
    return { out, streams: new Map([...seen.values()].map((s) => [s.start, s.n])), random: calls };
  } finally { Rng.prototype.next = next; Math.random = random; }
}

describe('the shelling scene (shore guns R1): the game and the save', () => {
  it('reads the game and writes nothing to it, and draws no die (only the month\'s keyed sky and gale hashes, where heavy seas are on)', () => {
    const g = britainMonth();
    g.headlines.push(huntNews(g), convoyNews(g));
    // without a look, the gale is galeSeas' (heavyseas.ts): with heavy seas on, the month's sky deal and gale, each a keyed
    // stream of the seed, the month and the zone (weather.ts skiesOf, heavyseas.ts), never the game's dice
    g.flags.heavySeas = 1;
    for (const r of g.shoreReports.slice(0, 3)) { const { look: _l, ...bare } = r as LookedReport; g.shoreReports.push({ ...bare, id: r.id + 100 }); }
    const before = JSON.stringify(g), save = saveGame(g), state = g.rng.state;
    const { streams, random } = dice(() => {
      for (const r of g.shoreReports) { shoreSceneOf(g, r.id); const f = shoreFactsOf(g, r); stageShore(f, 'att'); stageShore(f, 'def'); }
      for (const view of ['all', 'mine', 'never'] as const) for (const show of [SHOW_FIRST, ALL]) monthSeaOf(g, NAVY.convoys, undefined, view, { show });
      shoreScenesOf(g, { show: ALL });
    });
    expect(random).toBe(0);
    expect(g.rng.state).toBe(state);
    const keyed = (k: string): number => (hashString(k) >>> 0) || 0x9e3779b9;
    const month = new Set([...g.seas.map((z) => keyed(`gale:${g.seed}:${g.turn}:${z.key}`)), ...WEATHER.zones.map((z) => keyed(`sky:${g.seed}:${g.turn}:${z.key}`))]);
    expect(streams.size).toBeGreaterThan(0);
    for (const [start, n] of streams) { expect(month.has(start), `a stream from ${start}`).toBe(true); expect(n).toBe(1); }
    expect(JSON.stringify(g)).toBe(before);
    expect(saveGame(g)).toBe(save);
  });

  it('keeps look within the 80 shore reports a save keeps (save.ts), the setting on or off', () => {
    const g = startOpen(createGame(5, 'UK'));
    const base = sampleReport(g, sampleNamed('shore-normandy-dunes-village.json'), 0);
    for (let i = 1; i <= 100; i++) g.shoreReports.push({ ...base, id: i, turn: g.turn, look: { ...base.look!, seed: i } } as LookedReport);
    const off = saveGame(g);
    shoreRecord.on = true;
    try { expect(saveGame(g)).toBe(off); } finally { shoreRecord.on = false; }
    const back = loadGame(off)!;
    expect(back.shoreReports.length).toBe(80);
    expect(back.shoreReports.map((r) => r.id)).toEqual(Array.from({ length: 80 }, (_, i) => i + 21));
    expect((back.shoreReports as LookedReport[]).every((r, i) => r.look?.seed === i + 21)).toBe(true);
  });

  // The design's 6-war A/B, within this tree: each war played twice from its seed, the second with the stager, the
  // bridge's calls and the 3D's setting (off) at every month; the saves are the same text month by month, so the stager
  // and its calls write nothing to the game. (This compares this tree with itself; the save against d6ebeb08b's, through
  // the bridge's own month, is the pinned test below.)
  const WARS: [number, string][] = [[11, 'UK'], [12, 'GER'], [13, 'ITA'], [14, 'USA'], [15, 'FRA'], [16, 'SOV']];
  const MONTHS = 8;
  let stagedInWars = 0;
  for (const [seed, human] of WARS) {
    it(`leaves the save as it was, month by month, with the setting off (war ${seed}, ${human})`, () => {
      const a = startOpen(createGame(seed, human));
      const want: string[] = [];
      for (let m = 0; m < MONTHS; m++) { runTurnAI(a); want.push(digest(saveGame(a))); }
      const b = startOpen(createGame(seed, human));
      shoreRecord.on = false;
      for (let m = 0; m < MONTHS; m++) {
        runTurnAI(b);
        TC().shoreSceneOn(false);
        for (const r of b.shoreReports) if (shoreSceneOf(b, r.id).recording) stagedInWars++;
        for (const view of ['all', 'mine', 'never'] as const) for (const show of [SHOW_FIRST, ALL]) monthSeaOf(b, NAVY.convoys, undefined, view, { show });
        expect(digest(saveGame(b)), `month ${m + 1}`).toBe(want[m]);
      }
      // and through the bridge, on its own copy: the calls change nothing it would save
      TC().load(saveGame(b));
      const text = (TC().save() as X).text as string;
      TC().monthSea(); TC().monthSea(undefined, 'all', ALL);
      for (const r of b.shoreReports) TC().shoreScene(r.id);
      TC().shoreSceneOn(false);
      expect((TC().save() as X).text).toBe(text);
      // (by the last war, real shellings have been staged: the AI's, in most of the wars)
      if (seed === WARS[WARS.length - 1][0]) expect(stagedInWars).toBeGreaterThan(0);
    }, 300_000);
  }

  // The save against d6ebeb08b's (the line this was built on), through the bridge's own month: each war opened with
  // TC.start and played with TC.endMonth (where the month card's shelling window is now kept), with the setting off and
  // the bridge's three calls made at the start and after every month. Its save's digest, at the start and after each of
  // twelve months, is the one d6ebeb08b gives (pinned here: read on d6ebeb08b by TC.start, TC.endMonth and TC.save alone,
  // which it has; the digest is cyrb53, as above). A later change to the rules moves these on purpose: pin them again
  // from that change's own base.
  const PINNED_D6EBEB08B: Record<string, string[]> = {
    '11:UK': ['305849:1vw0uundnqj', '340749:88mn13a1lp', '358259:14cfexcbres', '383876:24sr43ptcnj', '401826:eca4iao7rq', '417779:1h0iftdsbyt', '474339:1mp8kfcp5ip', '560181:gqtkq62gmh', '635462:67ndfpu1ll', '685718:jdi3suchym', '711497:224pb2k7jih', '734684:236fkgv8t3r', '733459:nfro9qktg6'],
    '12:GER': ['162910:j6e0kt4cu4', '267290:9lyt3ryqxz', '278662:1qqp7dwi4mn', '287458:27m75v2vt06', '297035:1svoo5uxb7q', '306511:18q2y7j9om1', '323790:2cu6zx1ojvv', '327853:195ph4umity', '342199:mmex1pkqsl', '358506:ze0d8k43r', '371606:zr8gaqlt46', '392666:v3cbo87jpw', '404275:18224a5m2g7'],
    '13:ITA': ['235720:xom1khgsiu', '329315:9mw5uc0ex9', '347884:7boc6s4pnv', '362512:wswhscd34k', '393736:13g7v6hvgoq', '414994:675dw2p6fv', '472712:evf9aggxqv', '529928:19r0gdtjz4m', '582642:2f9run6wf8i', '609656:e9lhicxzqd', '659009:1q4q92mdhoc', '701231:jw1n5eqamt', '726247:2pyr5ltasu'],
    '14:USA': ['120851:1ph2ti0gbub', '308020:3da8rk2p40', '335320:q93e0htvd1', '356532:13gjzykvyx', '377567:t8j9ttejmw', '406298:2c4hc4bb9qa', '426531:uiy0kk3z9', '455208:22nugusg3gk', '512966:1zvgq6skd9g', '589524:8ew6fp86zf', '638757:uvr2ja5g9', '673647:97lkhmq19', '705928:l3p8vmvagx'],
    '15:FRA': ['288219:bhvcuh1m7c', '334405:1g7gd21lk6t', '355388:17mr9bj24v4', '375014:1h407mewwzd', '397362:1dgd228uxv2', '418212:180qorkp8sq', '443893:7k6vz5aj36', '460719:1avts0arkiu', '477782:1hi67f79600', '556772:124vr5fnggt', '618603:1iafg7jh55j', '651712:2bylnepneav', '693067:1hilgt25ict'],
    '16:SOV': ['277249:2563cg6pett', '333843:2ck8hyoyqhw', '355632:1s6h75c6457', '372348:21dqxyiuxmo', '393992:1ntc2t1w10j', '410447:1nymtwad00f', '433125:1klwer5k3f5', '451072:1jnvd9y3heu', '492307:2f93lq69509', '554450:26whvocpang', '591146:1loz9gzzv2u', '648793:14tehuy95ki', '641944:pea5sbsroi'],
  };
  let stagedOnBridge = 0;
  for (const [seed, human] of WARS) {
    it(`gives the save d6ebeb08b gives, month by month through TC.start and TC.endMonth, the setting off (war ${seed}, ${human})`, () => {
      TC().start({ human, seed, view: 'grand' });
      const calls = (): string => {
        TC().shoreSceneOn(false);
        const text = (TC().save() as X).text as string;
        for (const view of [undefined, 'all', 'mine', 'never'] as const) { TC().monthSea(undefined, view); TC().monthSea(undefined, view, ALL); }
        for (const r of loadGame(text)!.shoreReports) if ((TC().shoreScene(r.id) as X).recording) stagedOnBridge++;
        TC().shoreSceneOn(false);
        const after = (TC().save() as X).text as string;
        expect(after).toBe(text);
        return digest(after);
      };
      const got = [calls()];
      for (let m = 0; m < 12; m++) { TC().endMonth(); got.push(calls()); }
      expect(got).toEqual(PINNED_D6EBEB08B[`${seed}:${human}`]);
      // (by the last war, real shellings have been staged through the bridge: the computer's, in most of the wars)
      if (seed === WARS[WARS.length - 1][0]) expect(stagedOnBridge).toBeGreaterThan(0);
    }, 300_000);
  }
});
