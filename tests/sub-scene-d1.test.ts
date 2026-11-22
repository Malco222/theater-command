// The month's submarine scene (form D1, ruling 241): the stager (unreal/TheaterCommand/Rules/sub-scene.ts).
// Look only: it reads the month's booked facts and writes a staged recording in the sea scene's recording format.
// These check that it is deterministic, picks the scene by D1's trigger order, gives every navy a sensible look,
// draws no kill that is not booked, and writes what the sea scene's reader expects (field-side-read §3, form §5).
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { NAVY } from '../src/campaign/navy';
import type { Game } from '../src/campaign/types';
import {
  CLEAR, factsOf, looksOf, pickScene, seaPlace, stagePick, stageScene, subSceneOf,
  type MonthFacts, type SeaFact, type Staged, type StagedNote,
} from '../unreal/TheaterCommand/Rules/sub-scene';
import { SAMPLES } from '../unreal/TheaterCommand/Tools/sub_scene/sample-facts';

const NAMES: Record<string, { name: string; adj: string }> = {
  UK: { name: 'United Kingdom', adj: 'British' }, GER: { name: 'Germany', adj: 'German' }, ITA: { name: 'Italy', adj: 'Italian' },
  USA: { name: 'United States', adj: 'American' }, JAP: { name: 'Japan', adj: 'Japanese' }, SOV: { name: 'Soviet Union', adj: 'Soviet' },
  FRA: { name: 'France', adj: 'French' }, HOL: { name: 'Netherlands', adj: 'Dutch' },
};
const FOES: Record<string, string[]> = {
  UK: ['GER', 'ITA', 'JAP'], GER: ['UK', 'SOV', 'USA', 'FRA', 'HOL'], ITA: ['UK', 'FRA'], SOV: ['GER'], USA: ['JAP', 'GER'],
  JAP: ['USA', 'UK', 'HOL'], FRA: ['GER', 'ITA'], HOL: ['GER', 'JAP'],
};
/** turn: September 1939 is 1. */
const T = (year: number, month1: number): number => (year - 1939) * 12 + month1 - 9 + 1;
const month = (human: string, turn: number, facts: SeaFact[], seed = 4242): MonthFacts => ({ human, seed, turn: turn + 1, facts, foes: FOES, names: NAMES });
const noteOf = (r: Staged): StagedNote => r.field.staged as StagedNote;

type Unit = Record<string, unknown> & { id: number; extra?: Record<string, unknown> };
type Ev = { t: number; what: string; unit?: number; extra?: Record<string, unknown> };
type View = { t: number; units: Unit[]; events: Ev[]; zones?: unknown[] };

/** The views as the reader keeps them: each piece whole, what a view leaves out standing as it was last told. */
function playOut(r: Staged): { t: number; units: Map<number, Unit> }[] {
  const now = new Map<number, Unit>();
  return (r.views as unknown as View[]).map((v) => {
    for (const u of v.units) {
      const was = now.get(u.id);
      now.set(u.id, was ? { ...was, ...u, extra: { ...(was.extra ?? {}), ...(u.extra ?? {}) } } : { ...u });
    }
    return { t: v.t, units: new Map([...now].map(([k, u]) => [k, { ...u, extra: { ...u.extra } }])) };
  });
}
const eventsOf = (r: Staged): Ev[] => (r.views as unknown as View[]).flatMap((v) => v.events);

/** A piece's place, heading and speed at a moment, between the views either side (as the scene eases them). */
function poseAt(played: ReturnType<typeof playOut>, id: number, t: number): { x: number; y: number; h: number; v: number } {
  let i = 0;
  while (i + 1 < played.length && played[i + 1].t <= t) i++;
  const a = played[i].units.get(id)!, b = (played[Math.min(i + 1, played.length - 1)].units.get(id) ?? a);
  const span = played[Math.min(i + 1, played.length - 1)].t - played[i].t;
  const f = span > 0 ? Math.max(0, Math.min(1, (t - played[i].t) / span)) : 0;
  const n = (u: Unit, k: string): number => Number(u[k]);
  let dh = n(b, 'face') - n(a, 'face');
  if (dh > Math.PI) dh -= 2 * Math.PI;
  if (dh < -Math.PI) dh += 2 * Math.PI;
  return { x: n(a, 'x') + (n(b, 'x') - n(a, 'x')) * f, y: n(a, 'y') + (n(b, 'y') - n(a, 'y')) * f, h: n(a, 'face') + dh * f, v: n(a, 'speed') + (n(b, 'speed') - n(a, 'speed')) * f };
}

/** Where each charge of a pattern bursts, as the sea scene lays them (SeaShow.cpp sea:depthCharge). */
function chargesOf(drop: { x: number; y: number; h: number; v: number }, len: number, beam: number, n: number, spread: number, sinking: number, t: number) {
  const sp = Math.max(3, drop.v), run = spread / sp;
  const ax = Math.sin(drop.h), ay = Math.cos(drop.h), rx = Math.cos(drop.h), ry = -Math.sin(drop.h);
  const stern = { x: drop.x - ax * len * 0.5, y: drop.y - ay * len * 0.5 };
  return Array.from({ length: n }, (_, k) => {
    const share = n > 1 ? k / (n - 1) : 0;
    const across = k % 2 === 0 ? 0 : (Math.floor(k / 2) % 2 === 0 ? 1 : -1) * (spread * 0.5 - beam * 0.5);
    return { x: stern.x + ax * share * spread + rx * across, y: stern.y + ay * share * spread + ry * across, t: t + share * run + sinking };
  });
}

const EVENT_NAMES = new Set(['sea:report', 'sea:torpedo', 'sea:torpedoHit', 'sea:sunk', 'gone', 'sea:dive', 'sea:sonar', 'sea:depthCharge', 'sea:depthChargeHit', 'sea:starshell', 'sea:rescue', 'sea:air']);
const UNIT_FIELDS = ['id', 'side', 'kind', 'name', 'nat', 'x', 'y', 'face', 'str', 'mor', 'state', 'figures0', 'figures', 'w', 'd', 'act', 'speed', 'engaged', 'loop', 'extra'];

/** What the sea scene's reader expects of a staged recording (SeaFeed.cpp; field-side-read §3; form §5). */
function expectReadable(r: Staged): void {
  expect(Object.keys(r).sort()).toEqual(['after', 'field', 'key', 'recording', 'seed', 'step', 'views', 'what']);
  expect(r.recording).toBe(true);
  const text = JSON.stringify(r);
  expect(text.length).toBeLessThan(2 * 1024 * 1024);
  // No depth field named `d` or `depth`: a boat's depth is extra.keel; `d` is every piece's column length.
  const keys = new Set<string>();
  const walk = (o: unknown): void => {
    if (Array.isArray(o)) o.forEach(walk);
    else if (o && typeof o === 'object') for (const [k, v] of Object.entries(o)) { keys.add(k); walk(v); }
  };
  walk(r);
  expect(keys.has('depth')).toBe(false);
  const field = r.field as Record<string, any>;
  const ssLen = field.classes.ss.len;
  // The light: heights and bearings in degrees; the moon's share lit 0-1; vis metres.
  const light = field.light;
  for (const body of [light.sun, light.moon]) {
    expect(body.up).toBeGreaterThanOrEqual(-90);
    expect(body.up).toBeLessThanOrEqual(90);
    expect(body.from).toBeGreaterThanOrEqual(0);
    expect(body.from).toBeLessThan(360);
  }
  expect(light.moon.lit).toBeGreaterThanOrEqual(0);
  expect(light.moon.lit).toBeLessThanOrEqual(1);
  expect(light.vis).toBeGreaterThan(0);
  expect(field.subs.scope).toBeGreaterThan(0);
  expect(field.subs.crush).toBeGreaterThan(field.subs.deep);
  // The views: every step, each piece whole the first time it shows.
  const views = r.views as unknown as View[];
  const seen = new Set<number>();
  views.forEach((v, i) => {
    expect(v.t).toBeCloseTo(i * r.step, 6);
    for (const u of v.units) if (!seen.has(u.id)) { seen.add(u.id); for (const k of UNIT_FIELDS) expect(u, `piece ${u.id} ${k}`).toHaveProperty(k); }
  });
  const played = playOut(r);
  const events = eventsOf(r);
  const sunkAt = new Map<number, number>(), sunkView = new Map<number, number>();
  for (const v of views) for (const e of v.events) if (e.what === 'sea:sunk' && e.unit !== undefined) { sunkAt.set(e.unit, e.t); sunkView.set(e.unit, v.t); }
  // In every view: a live boat's dive, keel and scope, and a lost piece's sunk (the reader takes these afresh), from
  // the view its sea:sunk comes in.
  views.forEach((v, i) => {
    const told = new Map(v.units.map((u) => [u.id, u]));
    for (const [id, u] of played[i].units) {
      const x = told.get(id)?.extra ?? {};
      const gone = (sunkView.get(id) ?? Infinity) <= v.t + 1e-9;
      if (gone) expect(x.sunk, `view ${v.t} piece ${id} sunk`).toBe(1);
      else if (u.kind === 'ss') {
        expect(['surfaced', 'periscope', 'deep']).toContain(x.dive);
        expect(typeof x.keel).toBe('number');
        expect(x.keel as number).toBeLessThanOrEqual(field.subs.crush);
        expect(typeof x.scope).toBe('boolean');
      }
      if (u.kind === 'ss') expect(u.d, 'a boat\'s d is her column length, not her depth').toBe(ssLen);
    }
  });
  // The events, by the names the reader acts on.
  const ids = new Set(played[played.length - 1].units.keys());
  for (const e of events) {
    expect(EVENT_NAMES.has(e.what), e.what).toBe(true);
    const x = e.extra ?? {};
    if (e.what === 'sea:torpedo') { expect(x.spread as number).toBeGreaterThan(0); expect(x.spread as number).toBeLessThanOrEqual(40); expect(ids.has(x.target as number)).toBe(true); }
    if (e.what === 'sea:report') expect(ids.has(x.at as number)).toBe(true);
    if (e.what === 'sea:rescue') { expect(sunkAt.has(x.on as number)).toBe(true); expect(e.t).toBeGreaterThanOrEqual(sunkAt.get(x.on as number)!); }
    if (e.what === 'sea:sunk' && x.how !== undefined) expect(['crushed', 'scuttled']).toContain(x.how);
    if (e.what === 'sea:dive') { expect(['surfaced', 'periscope', 'deep']).toContain(x.dive); expect(typeof x.keel).toBe('number'); }
    if (e.what === 'sea:depthCharge') { expect(['pattern', 'ahead']).toContain(x.kind); expect(ids.has(x.on as number)).toBe(true); }
    if (e.what === 'sea:depthChargeHit') expect(ids.has(x.from as number)).toBe(true);
    if (e.what === 'sea:sonar') for (const k of ['on', 'kind', 'range', 'bearing', 'err']) expect(x).toHaveProperty(k);
  }
  // The beats: 30-45 s on screen, and a caption carrying the historian's check.
  const st = noteOf(r);
  expect(st.seconds).toBeGreaterThanOrEqual(30);
  expect(st.seconds).toBeLessThanOrEqual(45);
  expect(st.beats.reduce((s, b) => s + b.screen, 0)).toBeCloseTo(st.seconds, 6);
  for (const b of st.beats) { expect(b.to).toBeGreaterThan(b.from); expect(b.screen).toBeGreaterThan(0); }
  expect(st.caption.check).toBe('to be checked by the historian before shipping');
}

/** No kill drawn unless booked: as many boats lost as booked (to three), each by its blow, and every other pattern clear. */
function expectKillsAsBooked(r: Staged, booked: number): void {
  const st = noteOf(r), events = eventsOf(r), played = playOut(r);
  const field = r.field as Record<string, any>;
  const boats = [...played[0].units.values()].filter((u) => u.kind === 'ss').map((u) => u.id);
  const lost = events.filter((e) => e.what === 'sea:sunk' && boats.includes(e.unit!));
  expect(lost.length).toBe(Math.min(3, booked));
  expect(st.drawn.kills).toBe(Math.min(3, booked));
  const hits = events.filter((e) => e.what === 'sea:depthChargeHit');
  const air = events.filter((e) => e.what === 'sea:air');
  expect(hits.length + air.length).toBe(lost.length);
  if (!booked) expect(events.some((e) => e.what === 'sea:rescue')).toBe(false);
  for (const s of lost) expect([...hits, ...air].some((h) => h.unit === s.unit && h.t <= s.t)).toBe(true);
  // The booked run: the escort's last run on her before the blow. Every other pattern's charges burst CLEAR of every boat.
  const killRuns = new Set<Ev>();
  for (const h of hits) {
    const runs = events.filter((e) => e.what === 'sea:depthCharge' && e.unit === h.extra!.from && e.extra!.on === h.unit && e.t <= h.t);
    killRuns.add(runs[runs.length - 1]);
  }
  const dd = field.classes.dd;
  for (const e of events) {
    if (e.what !== 'sea:depthCharge' || killRuns.has(e)) continue;
    const x = e.extra!;
    expect(x.kind).toBe('pattern');
    const drop = poseAt(played, e.unit!, e.t);
    for (const c of chargesOf(drop, dd.len, dd.beam, x.n as number, x.spread as number, x.sinking as number, e.t)) {
      for (const b of boats) {
        const gone = lost.find((s) => s.unit === b);
        if (gone && gone.t <= c.t) continue;
        const q = poseAt(played, b, c.t);
        expect(Math.hypot(q.x - c.x, q.y - c.y), `${r.key}: charge of the run on ${x.on} at ${c.t.toFixed(1)} s from boat ${b}`).toBeGreaterThanOrEqual(CLEAR - 5);
      }
    }
  }
}

describe('the submarine scene stager (form D1)', () => {
  it('is deterministic: the same facts give the same bytes, and other seeds another look', () => {
    for (const s of SAMPLES) {
      const a = JSON.stringify(stageScene(s.facts));
      const b = JSON.stringify(stageScene(JSON.parse(JSON.stringify(s.facts)) as MonthFacts));
      expect(a).toBe(b);
      expect(JSON.stringify(stageScene({ ...s.facts, seed: s.facts.seed + 1 }))).not.toBe(a);
    }
  });

  it('stages the three samples as the reader expects, each under 2 MB, with their kills as booked', () => {
    for (const s of SAMPLES) {
      const r = stageScene(s.facts)!;
      expect(r, s.file).not.toBeNull();
      expectReadable(r);
      const convoy = s.facts.facts.find((f) => f.kind === 'convoy')!;
      expect(noteOf(r).case).toBe('convoy');
      expectKillsAsBooked(r, convoy.boats ?? 0);
      // The caption: the month's tons and ships at about 5,200 GRT a ship.
      const ships = Math.max(1, Math.round((convoy.tons ?? 0) / 5200));
      expect(noteOf(r).caption.lines[0]).toContain(`${(convoy.tons ?? 0).toLocaleString('en-US')} tons of shipping sunk by submarines, about ${ships.toLocaleString('en-US')} ship`);
      expect(noteOf(r).caption.lines[0]).toContain('5,200 tons a ship');
    }
  });

  it('picks by the trigger order: his heavy ship lost, his boats\' kill, his convoy month, his largest hunt', () => {
    const t = T(1942, 6);
    const heavy: SeaFact = { kind: 'torpedo', sea: 'emed', turn: t, week: 3, by: '', on: 'UK', cls: 'bb', fleet: 'the Mediterranean Fleet', boatsAt: { GER: 2 } };
    const kill: SeaFact = { kind: 'torpedo', sea: 'central', turn: t, week: 2, by: '', on: 'ITA', cls: 'ca', fleet: 'the 3rd Division', boatsAt: { UK: 3 } };
    const convoy: SeaFact = { kind: 'convoy', sea: 'natl', turn: t, week: 4, by: '', on: 'UK', route: 'atlantic', routeName: 'the Atlantic convoys', tons: 90000, boats: 1, escortsLost: 0, boatsAt: { GER: 12 } };
    const huntSmall: SeaFact = { kind: 'hunt', sea: 'nsea', turn: t, week: 1, by: 'UK', on: 'GER', boats: 1, escortsLost: 0 };
    const huntBig: SeaFact = { kind: 'hunt', sea: 'wapp', turn: t, week: 1, by: 'UK', on: 'GER', boats: 2, escortsLost: 1 };
    const all = [huntSmall, convoy, kill, huntBig, heavy];
    const caseOf = (fs: SeaFact[]): string | null => pickScene(month('UK', t, fs))?.case ?? null;
    expect(caseOf(all)).toBe('heavyLost');
    expect(caseOf(all.filter((f) => f !== heavy))).toBe('boatsKill');
    expect(caseOf([huntSmall, convoy, huntBig])).toBe('convoy');
    expect(caseOf([huntSmall, huntBig])).toBe('hunt');
    expect(pickScene(month('UK', t, [huntSmall, huntBig]))!.fact).toBe(huntBig);
    expect(caseOf([])).toBeNull();
    // His boats' kill needs his boats there: an Italian cruiser lost where no British boat is at sea is not his.
    expect(caseOf([{ ...kill, boatsAt: { FRA: 1 } }])).toBeNull();
    // His nation on either side: the raider of a convoy, the hunted in a hunt.
    const raid = pickScene(month('GER', t, [convoy]))!;
    expect([raid.case, raid.me, raid.boats, raid.guard]).toEqual(['convoy', 'att', 'GER', 'UK']);
    const hunted = pickScene(month('GER', t, [huntBig]))!;
    expect([hunted.case, hunted.me, hunted.boats, hunted.guard]).toEqual(['hunt', 'att', 'GER', 'UK']);
    const guard = pickScene(month('UK', t, [convoy]))!;
    expect([guard.me, guard.boats]).toEqual(['def', 'GER']);
    // A computer-only month (no player) holds no scene.
    expect(pickScene({ ...month('UK', t, all), human: '' })).toBeNull();
    // Nothing above nothing: a convoy month with no tons sunk, a hunt with no boat sunk.
    expect(caseOf([{ ...convoy, tons: 0 }, { ...huntSmall, boats: 0 }])).toBeNull();
  });

  it('stages each trigger case readably, with its kills as booked', () => {
    const t = T(1943, 5);
    const cases: { want: string; m: MonthFacts; booked: number }[] = [
      { want: 'heavyLost', booked: 0, m: month('UK', T(1941, 11), [{ kind: 'torpedo', sea: 'emed', turn: T(1941, 11), week: 4, by: '', on: 'UK', cls: 'bb', fleet: 'the Mediterranean Fleet', ships: { bb: 3, dd: 8 }, boatsAt: { GER: 2 } }]) },
      { want: 'boatsKill', booked: 1, m: month('USA', T(1944, 6), [
        { kind: 'torpedo', sea: 'philsea', turn: T(1944, 6), week: 3, by: '', on: 'JAP', cls: 'cv', fleet: 'the Mobile Fleet', ships: { cv: 5, bb: 5, ca: 10, dd: 20 }, boatsAt: { USA: 4 } },
        { kind: 'torpedo', sea: 'philsea', turn: T(1944, 6), week: 3, by: '', on: 'JAP', cls: 'cv', fleet: 'the Mobile Fleet', boatsAt: { USA: 4 } },
        { kind: 'hunt', sea: 'philsea', turn: T(1944, 6), week: 3, by: 'JAP', on: 'USA', boats: 1 }]) },
      { want: 'convoy', booked: 3, m: month('GER', t, [{ kind: 'convoy', sea: 'natl', turn: t, week: 3, by: '', on: 'UK', route: 'atlantic', routeName: 'the Atlantic convoys', tons: 40000, boats: 3, escortsLost: 0, boatsAt: { GER: 30 }, hunters: { ships: 1, aircraft: 2 } }]) },
      { want: 'hunt', booked: 3, m: month('UK', t, [{ kind: 'hunt', sea: 'midnatl', turn: t, week: 2, by: 'UK', on: 'GER', boats: 3, escortsLost: 0, hunters: { ships: 3, aircraft: 1 } }]) },
      { want: 'hunt', booked: 5, m: month('GER', t, [{ kind: 'hunt', sea: 'natl', turn: t, week: 2, by: 'UK', on: 'GER', boats: 5, escortsLost: 1 }]) },
    ];
    for (const c of cases) {
      const r = stageScene(c.m)!;
      expect(noteOf(r).case).toBe(c.want);
      expectReadable(r);
      expectKillsAsBooked(r, c.booked);
    }
    // The heavy cases draw the class lost, up to three heavy ships; the boats' kill, both carriers torpedoed.
    expect(noteOf(stageScene(cases[0].m)!).drawn.heavy).toBeLessThanOrEqual(3);
    expect(noteOf(stageScene(cases[1].m)!).drawn.hits).toBe(2);
    // Kills by aircraft where the kill's share is theirs: two of the three in the air-guarded convoy month.
    expect(noteOf(stageScene(cases[2].m)!).drawn.byAir).toBe(2);
    expect(eventsOf(stageScene(cases[2].m)!).filter((e) => e.what === 'sea:air').length).toBe(2);
  });

  it('gives every navy a sensible look, a small navy the default rows', () => {
    const navies: { nat: string; sea: string; year: number; enemy: string }[] = [
      { nat: 'GER', sea: 'natl', year: 1941, enemy: 'UK' },
      { nat: 'ITA', sea: 'central', year: 1941, enemy: 'UK' },
      { nat: 'JAP', sea: 'coral', year: 1942, enemy: 'USA' },
      { nat: 'USA', sea: 'luzonstrait', year: 1944, enemy: 'JAP' },
      { nat: 'UK', sea: 'central', year: 1942, enemy: 'ITA' },
      { nat: 'SOV', sea: 'baltic', year: 1942, enemy: 'GER' },
      { nat: 'FRA', sea: 'nsea', year: 1940, enemy: 'GER' },
      { nat: 'HOL', sea: 'java', year: 1942, enemy: 'JAP' },
    ];
    for (const n of navies) {
      const at = (n.year - 1939) * 12 + 5;
      const l = looksOf(n.nat, at, seaPlace(n.sea).basin);
      expect(l.boat.len, n.nat).toBeGreaterThanOrEqual(40);
      expect(l.boat.len).toBeLessThanOrEqual(130);
      expect(l.boat.surf).toBeGreaterThan(l.boat.dived);
      expect(l.boat.dived).toBeGreaterThan(1);
      expect(l.boat.crush).toBeGreaterThan(l.boat.deep);
      expect(l.boat.deep).toBeGreaterThanOrEqual(40);
      expect(l.boat.fan).toBeLessThanOrEqual(l.boat.tubes);
      expect(l.fish.speed).toBeGreaterThan(10);
      expect(l.fish.speed).toBeLessThan(30);
      expect(l.escort.speed).toBeGreaterThan(5);
      expect(l.escort.speed).toBeLessThan(20);
      expect(l.escort.set[1]).toBeGreaterThan(l.escort.set[0]);
      expect(l.escort.contact[0]).toBeGreaterThan(0);
      expect(l.air.share).toBeGreaterThanOrEqual(0);
      expect(l.air.share).toBeLessThan(1);
      // Her boats raid an enemy convoy, and her own convoy is raided: both stage and read.
      const t = T(n.year, 6);
      const raid = stageScene(month(n.nat, t, [{ kind: 'convoy', sea: n.sea, turn: t, week: 2, by: '', on: n.enemy, route: 'sample:x', routeName: 'the convoys', tons: 11000, boats: 1, escortsLost: 0, boatsAt: { [n.nat]: 2 } }]))!;
      const guard = stageScene(month(n.nat, t, [{ kind: 'convoy', sea: n.sea, turn: t, week: 2, by: '', on: n.nat, route: 'sample:y', routeName: 'the convoys', tons: 11000, boats: 1, escortsLost: 0, boatsAt: { [n.enemy]: 2 } }]))!;
      for (const r of [raid, guard]) { expectReadable(r); expectKillsAsBooked(r, 1); }
      expect((raid.field as any).classes.ss.name).toBe(l.boat.cls);
      expect((guard.field as any).classes.dd.name).toBe(looksOf(n.nat, at, seaPlace(n.sea).basin).escort.cls);
    }
    // A small navy has no rows of its own: the default's.
    expect(looksOf('HOL', 36, 'pacific').boat).toEqual(looksOf('_default', 36, 'pacific').boat);
    // By the year: the British escorts' ahead-thrown weapon from 1943, not before.
    expect(looksOf('UK', (1941 - 1939) * 12 + 5, 'atlantic').escort.ahead).toBeNull();
    expect(looksOf('UK', (1943 - 1939) * 12 + 5, 'atlantic').escort.ahead).toBe('Hedgehog');
  });

  it('draws no kill unless one is booked', () => {
    const t = T(1942, 9);
    for (const seed of [1, 2, 3, 4, 5]) {
      const r = stageScene(month('UK', t, [{ kind: 'convoy', sea: 'central', turn: t, week: 2, by: '', on: 'ITA', route: 'libya', routeName: 'the Libya convoys', tons: 26000, boats: 0, escortsLost: 0, boatsAt: { UK: 3 } }], seed))!;
      const ev = eventsOf(r);
      expect(ev.some((e) => e.what === 'sea:depthChargeHit' || e.what === 'sea:air' || e.what === 'sea:rescue')).toBe(false);
      expect(ev.filter((e) => e.what === 'sea:sunk' && (e.extra as { cls?: string }).cls === 'ss').length).toBe(0);
      expectKillsAsBooked(r, 0);
    }
  });

  it('reads the month from the game and writes nothing to it', () => {
    const g: Game = startOpen(createGame(5, 'UK'));
    const natl = g.seas.findIndex((z) => z.key === 'natl');
    expect(natl).toBeGreaterThanOrEqual(0);
    const seaName = g.seas[natl].name;
    // The news as huntSubs and convoyWar write it (navy.ts 1955, 2099), dated this month.
    g.headlines.push({ turn: g.turn, week: 2, text: `Escorts of the Western Approaches sink 2 German submarines in the ${seaName}, for the loss of 1 destroyer.`, kind: 'battle', nat: 'UK', arm: 'sea', won: 'UK', lost: 'GER' });
    g.headlines.push({ turn: g.turn, week: 3, text: 'The Atlantic convoys: 156,000 tons of shipping sunk this month, 2 submarines destroyed.', kind: 'info', nat: 'UK', arm: 'sea', lost: 'UK' });
    const before = JSON.stringify(g);
    const m = factsOf(g, NAVY.convoys);
    expect(m.human).toBe('UK');
    const hunt = m.facts.find((f) => f.kind === 'hunt')!;
    expect([hunt.sea, hunt.by, hunt.on, hunt.boats, hunt.escortsLost]).toEqual(['natl', 'UK', 'GER', 2, 1]);
    const convoy = m.facts.find((f) => f.kind === 'convoy')!;
    expect([convoy.route, convoy.on, convoy.tons, convoy.boats]).toEqual(['atlantic', 'UK', 156000, 2]);
    const out = subSceneOf(g, NAVY.convoys);
    expect(out.mark).toBe(g.headlines.length);
    expect(out.pick?.case).toBe('convoy');
    const rec = JSON.parse(out.recording!) as Staged;
    expectReadable(rec);
    expect(out.recording).toBe(subSceneOf(g, NAVY.convoys).recording);
    // From the mark on there is nothing new: no scene.
    expect(subSceneOf(g, NAVY.convoys, out.mark).recording).toBeNull();
    expect(JSON.stringify(g)).toBe(before);
    // Staged by hand from the same pick, the same bytes.
    expect(JSON.stringify(stagePick(m, pickScene(m)!))).toBe(out.recording);
  });
});
