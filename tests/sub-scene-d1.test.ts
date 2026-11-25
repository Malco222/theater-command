// The month's submarine scene (form D1, ruling 241): the stager (unreal/TheaterCommand/Rules/sub-scene.ts).
// Look only: it reads the month's booked facts and writes a staged recording in the sea scene's recording format.
// These check that it is deterministic, picks the scene by D1's trigger order, gives every navy a sensible look,
// draws no kill that is not booked, and writes what the sea scene's reader expects (field-side-read §3, form §5).
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { runTurnAI } from '../src/campaign/turn';
import { startOpen } from '../src/campaign/staff';
import { NAVY, navalUpkeep, subScene } from '../src/campaign/navy';
import type { Game, SubLook as RecordLook } from '../src/campaign/types';
import {
  AHEAD_RANGE, CLEAR, KILL_NEAR, TRACK_SPARE, factsOf, hisLoss, looksOf, pickScene, scenesOf, seaPlace, stagePick, stageScene, subSceneOf,
  type MonthFacts, type SeaFact, type Staged, type StagedNote, type SubLook, type SubLookFleet, type SubLookSaved,
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
/** A text's length and a 53-bit digest of it (cyrb53), to hold a recording to the bytes it was. */
function digest(s: string): string {
  let h1 = 0xdeadbeef, h2 = 0x41c6ce57;
  for (let i = 0; i < s.length; i++) { const c = s.charCodeAt(i); h1 = Math.imul(h1 ^ c, 2654435761); h2 = Math.imul(h2 ^ c, 1597334677); }
  h1 = Math.imul(h1 ^ (h1 >>> 16), 2246822507) ^ Math.imul(h2 ^ (h2 >>> 13), 3266489909);
  h2 = Math.imul(h2 ^ (h2 >>> 16), 2246822507) ^ Math.imul(h1 ^ (h1 >>> 13), 3266489909);
  return `${s.length}:${(4294967296 * (2097151 & h2) + (h1 >>> 0)).toString(36)}`;
}
// A British convoy's night in the North Atlantic without the record's kit (May 1943, September 1942, October 1940; the
// kit test's months), as the stager wrote it at f1ce92d8, before its events followed the kit.
const DIGEST_1943 = '539451:4st6msd5e5', DIGEST_1942 = '642089:22gdvhkyu57', DIGEST_1940 = '409661:7t8ktcw4qr';

// The stager's mirror of the rules' record (sub-scene.ts SubLook) and the record itself (types.ts SubLook, the rules'
// 21a3a759) are held equal at compile time: npx tsc --noEmit fails here when either moves. Assignable both ways (which
// also holds each field's type, and whether it is optional, at every depth); and field for field at every depth, the
// record, its hunts and their hunters and quarry, its torpedoings, its convoys, their seas and those seas' raiders and
// escorts (assignability lets an optional field on one side alone by: this catches it, and names where it is). Today's
// record is also one the stager reads as a save may hold it (SubLookSaved, which an older save's rows fit too).
type Both<A, B> = [A] extends [B] ? ([B] extends [A] ? true : false) : false;
/** Where A's and B's fields part, at any depth, as paths ('SubLook.convoys[].fought'); never where they are the same throughout. */
type Apart<A, B, P extends string> =
  [A] extends [readonly (infer X)[]] ? ([B] extends [readonly (infer Y)[]] ? Apart<X, Y, `${P}[]`> : P)
  : [B] extends [readonly unknown[]] ? P
  : [A] extends [object] ? ([B] extends [object]
    ? `${P}.${(Exclude<keyof A, keyof B> | Exclude<keyof B, keyof A>) & string}`
      | { [K in keyof A & keyof B & string]-?: ({} extends Pick<A, K> ? 1 : 0) extends ({} extends Pick<B, K> ? 1 : 0) ? Apart<NonNullable<A[K]>, NonNullable<B[K]>, `${P}.${K}`> : `${P}.${K}?` }[keyof A & keyof B & string]
    : P)
  : [B] extends [object] ? P : never;
/** true where the two have the same fields throughout; else the paths where they part (tsc names them in its error). */
type SameFields<A, B> = [Apart<A, B, 'SubLook'>] extends [never] ? true : Apart<A, B, 'SubLook'>;
const MIRROR_IS_RECORD: Both<SubLook, RecordLook> = true;
const SAME_FIELDS: SameFields<SubLook, RecordLook> = true;
const toMirror = (x: RecordLook): SubLook => x;
const asSaved = (x: RecordLook): SubLookSaved => x;
/** A record into the game as a save may hold it (an older save's rows have no couldNat, byNat or heavy). */
const keepLook = (g: Game, ...looks: SubLookSaved[]): void => { (g as { subLook?: unknown }).subLook = looks; };

type Unit = Record<string, unknown> & { id: number; extra?: Record<string, unknown> };
type Ev = { t: number; what: string; unit?: number; text?: string; extra?: Record<string, unknown> };
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
  // An aircraft's kill: one aircraft, seen running in, and she is still on the surface when it lets go (`running` on).
  for (const a of air) {
    const run = Number(a.extra!.running), down = lost.find((s) => s.unit === a.unit)!;
    expect([a.extra!.n, run >= 20, typeof a.extra!.kind]).toEqual([1, true, 'string']);
    expect(down.t).toBeGreaterThan(a.t + run);
    const v = (r.views as unknown as View[]).filter((x) => x.t <= a.t + run).pop()!;
    expect(played.find((p) => p.t === v.t)!.units.get(a.unit!)!.extra!.dive, `${r.key}: the boat as the aircraft lets go`).toBe('surfaced');
  }
  // The booked run: the escort's last run on her before the blow. Every other pattern's charges burst CLEAR of every boat.
  const killRuns = new Set<Ev>();
  for (const h of hits) {
    const runs = events.filter((e) => e.what === 'sea:depthCharge' && e.unit === h.extra!.from && e.extra!.on === h.unit && e.t <= h.t);
    killRuns.add(runs[runs.length - 1]);
  }
  const dd = field.classes.dd;
  // The booked blow lands on her: as the sea scene takes it (SeaShow.cpp sea:depthChargeHit), that escort's charge
  // bursting nearest the blow's moment, which must be the kill run's booked one, bursting then within KILL_NEAR of her
  // (views to a tenth of a metre, eased between them: 3 m more); an ahead-thrower's salvo aimed on her, AHEAD_RANGE
  // before the escort as she fires.
  for (const h of hits) {
    const run = [...killRuns].find((e) => e.unit === h.extra!.from && e.extra!.on === h.unit)!;
    const q = poseAt(played, h.unit!, h.t);
    const where = `${r.key}: the blow on ${h.unit} at ${h.t} s`;
    if (run.extra!.kind === 'ahead') {
      const x = run.extra!, d = poseAt(played, run.unit!, run.t);
      expect(Math.hypot(Number(x.x) - q.x, Number(x.y) - q.y), where).toBeLessThanOrEqual(KILL_NEAR + 3);
      expect(Math.hypot(d.x + Math.sin(d.h) * AHEAD_RANGE - Number(x.x), d.y + Math.cos(d.h) * AHEAD_RANGE - Number(x.y)), where).toBeLessThanOrEqual(KILL_NEAR + 3);
      expect(h.t - run.t).toBeLessThanOrEqual(60);
      continue;
    }
    let best: { x: number; y: number; t: number; mine: boolean } | null = null;
    for (const e of events) {
      if (e.what !== 'sea:depthCharge' || e.unit !== h.extra!.from || e.extra!.kind !== 'pattern') continue;
      const x = e.extra!;
      for (const c of chargesOf(poseAt(played, e.unit!, e.t), dd.len, dd.beam, x.n as number, x.spread as number, x.sinking as number, e.t)) {
        if (Math.abs(c.t - h.t) <= 15 && (!best || Math.abs(c.t - h.t) < Math.abs(best.t - h.t))) best = { ...c, mine: e === run };
      }
    }
    expect(best, where).not.toBeNull();
    expect(best!.mine, where).toBe(true);
    expect(Math.abs(best!.t - h.t), where).toBeLessThanOrEqual(0.5);
    expect(Math.hypot(best!.x - q.x, best!.y - q.y), where).toBeLessThanOrEqual(KILL_NEAR + 3);
  }
  // A scuttled boat's rescuer stands by near where she went down.
  for (const e of events.filter((x) => x.what === 'sea:rescue')) {
    const lost = lost0(events, e.extra!.on as number), by = poseAt(played, e.extra!.by as number, e.t), grave = poseAt(played, e.extra!.on as number, lost);
    expect(Math.hypot(by.x - grave.x, by.y - grave.y), `${r.key}: the rescue of ${e.extra!.on}`).toBeLessThanOrEqual(400);
    expect(by.v, `${r.key}: the rescuer of ${e.extra!.on} stopped`).toBeLessThanOrEqual(1);
  }
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

/** When a piece was lost (its sea:sunk). */
const lost0 = (events: Ev[], id: number): number => events.find((e) => e.what === 'sea:sunk' && e.unit === id)!.t;

/**
 * Nothing the scene shows comes after its tracks: every event, beat and burst lies inside the views, the views end at
 * least TRACK_SPARE before the tracks' 900 s, and no piece under way stands still from one view to the next (a track
 * stands still at its end).
 */
function expectInsideTracks(r: Staged): void {
  const views = r.views as unknown as View[], played = playOut(r), events = eventsOf(r), field = r.field as Record<string, any>;
  const last = views[views.length - 1].t;
  expect(last, r.key).toBeLessThanOrEqual(900 - TRACK_SPARE);
  for (const e of events) expect(e.t, `${r.key}: ${e.what}`).toBeLessThanOrEqual(last);
  for (const b of noteOf(r).beats) expect(b.to, `${r.key}: beat ${b.what}`).toBeLessThanOrEqual(last);
  for (const e of events) {
    if (e.what !== 'sea:depthCharge' || e.extra!.kind !== 'pattern') continue;
    const x = e.extra!, d = poseAt(played, e.unit!, e.t);
    expect(e.t + (x.spread as number) / Math.max(3, d.v) + (x.sinking as number), `${r.key}: the pattern at ${e.t} s`).toBeLessThanOrEqual(last);
  }
  for (let i = 1; i < played.length; i++) {
    for (const [id, u] of played[i].units) {
      const w = played[i - 1].units.get(id)!;
      if (u.state === 'gone' || Number(u.speed) <= 0.5) continue;
      expect(u.x !== w.x || u.y !== w.y, `${r.key}: piece ${id} under way at ${played[i].t} s stands still`).toBe(true);
    }
  }
  expect(field.staged.seconds).toBeGreaterThan(0);
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
      expectInsideTracks(r);
      const convoy = s.facts.facts.find((f) => f.kind === 'convoy')!;
      expect(noteOf(r).case).toBe('convoy');
      expectKillsAsBooked(r, convoy.boats ?? 0);
      // The caption: the month's tons and ships at about 5,200 GRT a ship; "by submarines" only where boats alone raided
      // (the Mediterranean sample's Libya convoys are raided by Malta too).
      const ships = Math.max(1, Math.round((convoy.tons ?? 0) / 5200));
      const bySubs = convoy.raidBy!.boats && !convoy.raidBy!.surface && !convoy.raidBy!.base;
      expect(bySubs).toBe(s.file !== 'sub-mediterranean-1942.json');
      expect(noteOf(r).caption.lines[0]).toContain(`${(convoy.tons ?? 0).toLocaleString('en-US')} tons of shipping sunk ${bySubs ? 'by submarines' : 'on the convoy routes'}, about ${ships.toLocaleString('en-US')} ship`);
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
      expectInsideTracks(r);
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
      for (const r of [raid, guard]) { expectReadable(r); expectKillsAsBooked(r, 1); expectInsideTracks(r); }
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
      expectInsideTracks(r);
    }
  });

  it('keeps every run, burst and beat inside the tracks where the escorts are few, each kill on its boat', () => {
    // The review's cases: a battleship lost with no destroyers and two or three boats sunk in the same sea; hunts by a
    // navy of small hunting groups; British convoys with three kills; and the review's own month (seed 1).
    const heavy = (k: number, dd: number, year: number, m1: number, sea: string, me: string, foe: string, seed: number): MonthFacts => {
      const t = T(year, m1);
      return month(me, t, [
        { kind: 'torpedo', sea, turn: t, week: 3, by: '', on: me, cls: 'bb', fleet: 'the Fleet', ships: { bb: 2, ca: 2, dd }, boatsAt: { [foe]: 4 } },
        { kind: 'hunt', sea, turn: t, week: 2, by: me, on: foe, boats: k, escortsLost: 0 }], seed);
    };
    const hunt = (k: number, year: number, m1: number, sea: string, by: string, on: string, me: string, seed: number): MonthFacts => {
      const t = T(year, m1);
      return month(me, t, [{ kind: 'hunt', sea, turn: t, week: 2, by, on, boats: k, escortsLost: 0 }], seed);
    };
    const convoy = (k: number, year: number, m1: number, seed: number): MonthFacts => {
      const t = T(year, m1);
      return month('UK', t, [{ kind: 'convoy', sea: 'natl', turn: t, week: 2, by: '', on: 'UK', route: 'atlantic', routeName: 'the Atlantic convoys', tons: 60000, boats: k, escortsLost: 0, boatsAt: { GER: 10 } }], seed);
    };
    const all: { m: MonthFacts; booked: number }[] = [{ m: heavy(2, 0, 1941, 11, 'emed', 'UK', 'GER', 1), booked: 2 }];
    for (const seed of [2, 3, 4]) {
      for (const k of [2, 3]) {
        all.push({ m: heavy(k, seed % 2, 1941, 11, 'emed', 'UK', 'GER', seed), booked: k });
        all.push({ m: heavy(k, 0, 1944, 6, 'philsea', 'JAP', 'USA', seed), booked: k });
        all.push({ m: hunt(k, 1942, 6, 'central', 'ITA', 'UK', 'UK', seed), booked: k });
        all.push({ m: hunt(k, 1944, 8, 'luzonstrait', 'JAP', 'USA', 'JAP', seed), booked: k });
      }
      all.push({ m: convoy(3, 1941, 6, seed), booked: 3 });
      all.push({ m: convoy(3, 1943, 5, seed), booked: 3 });
    }
    for (const c of all) {
      const r = stageScene(c.m)!;
      expectReadable(r);
      expectKillsAsBooked(r, c.booked);
      expectInsideTracks(r);
    }
    // The review's month: both kills inside the first ten minutes, each by her own destroyer.
    const r = stageScene(all[0].m)!, ev = eventsOf(r);
    const blows = ev.filter((e) => e.what === 'sea:depthChargeHit');
    expect(blows.length).toBe(2);
    expect(new Set(blows.map((e) => e.extra!.from)).size).toBe(2);
    expect(Math.max(...ev.map((e) => e.t))).toBeLessThan(600);
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
    expectKillsAsBooked(rec, 2);
    expectInsideTracks(rec);
    expect(out.recording).toBe(subSceneOf(g, NAVY.convoys).recording);
    // From the mark on there is nothing new: no scene.
    expect(subSceneOf(g, NAVY.convoys, out.mark).recording).toBeNull();
    expect(JSON.stringify(g)).toBe(before);
    // Staged by hand from the same pick, the same bytes.
    expect(JSON.stringify(stagePick(m, pickScene(m)!))).toBe(out.recording);
  });

  it('says "by submarines" only where boats alone raided, names the boats lost only by the record, and stages no scene where no boat raided', () => {
    const t = T(1942, 3);
    const base: SeaFact = { kind: 'convoy', sea: 'central', turn: t, week: 2, by: '', on: 'ITA', route: 'libya', routeName: 'the Libya convoys', tons: 24000, boats: 2, escortsLost: 0 };
    const capOf = (f: SeaFact, human = 'ITA'): string[] | null => { const r = stageScene(month(human, t, [f])); return r ? noteOf(r).caption.lines : null; };
    // Malta's raid alone, or a surface raider alone: no boat raided, so no submarine scene, for either side.
    expect(capOf({ ...base, raidBy: { boats: false, surface: false, base: true } })).toBeNull();
    expect(capOf({ ...base, raidBy: { boats: false, surface: true, base: false } })).toBeNull();
    expect(capOf({ ...base, raidBy: { boats: false, surface: false, base: true } }, 'UK')).toBeNull();
    expect(capOf({ ...base, boatsAt: {} })).toBeNull();
    // ... and the trigger order goes on, here to his hunt.
    const hunt: SeaFact = { kind: 'hunt', sea: 'central', turn: t, week: 1, by: 'ITA', on: 'UK', boats: 1, escortsLost: 0 };
    expect(pickScene(month('ITA', t, [{ ...base, raidBy: { boats: false, surface: false, base: true } }, hunt]))!.case).toBe('hunt');
    // Boats with Malta, or with a surface raider: a scene, its tons "sunk on the convoy routes".
    for (const raidBy of [{ boats: true, surface: false, base: true }, { boats: true, surface: true, base: false }]) {
      const lines = capOf({ ...base, boatsAt: { UK: 2 }, raidBy })!;
      expect(lines[0]).toBe('The Libya convoys, March 1942: 24,000 tons of shipping sunk on the convoy routes, about 5 ships at about 5,200 tons a ship.');
      expect(lines.join(' ')).not.toContain('by submarines');
    }
    // Boats alone: "by submarines"; where nothing says who raided, the boats at sea do.
    const boatsOnly = { ...base, boatsAt: { UK: 2 }, raidBy: { boats: true, surface: false, base: false } };
    expect(capOf(boatsOnly)![0]).toContain('24,000 tons of shipping sunk by submarines');
    expect(capOf({ ...base, boatsAt: { UK: 2 } })![0]).toContain('sunk by submarines');
    // The boats lost (the route's month, any raider's): their nation only where the record shows them one nation's.
    expect(capOf(boatsOnly)![1]).toBe('2 submarines sunk.');
    expect(capOf({ ...boatsOnly, raiders: ['UK'], boatsNat: 'UK' })![1]).toBe('2 British submarines sunk.');
    // Where the record shows the boats lost another raider's, his raid draws none of his lost (and is no loss of his);
    // his convoy's night draws the lost boats' navy where it raided there.
    const atl: SeaFact = { kind: 'convoy', sea: 'natl', turn: t, week: 2, by: '', on: 'UK', route: 'atlantic', routeName: 'the Atlantic convoys', tons: 90000, boats: 2, escortsLost: 0,
      raidBy: { boats: true, surface: false, base: false }, raiders: ['GER', 'ITA'], escorts: ['UK'], escortShips: 6, airGuard: 1, tonsBy: { natl: 90000 }, boatsNat: 'GER' };
    const itaRaid = pickScene(month('ITA', t, [atl]))!;
    expect([itaRaid.case, itaRaid.me, itaRaid.boats, itaRaid.kills, hisLoss(itaRaid)]).toEqual(['convoy', 'att', 'ITA', 0, false]);
    expect(pickScene(month('ITA', t, [atl]), 'mine')).toBeNull();
    expect([pickScene(month('GER', t, [atl]))!.kills, pickScene(month('UK', t, [atl]))!.boats]).toEqual([2, 'GER']);
    expect(pickScene(month('UK', t, [{ ...atl, boatsNat: 'ITA' }]))!.boats).toBe('ITA');
    expectKillsAsBooked(stageScene(month('ITA', t, [atl]))!, 0);
  });

  it('reads who raided a convoy month from the game: Malta\'s raid alone gives no scene, boats with a fleet no "by submarines"', () => {
    const g: Game = startOpen(createGame(5, 'UK'));
    g.human = 'ITA';
    g.nations.ITA.atWar = ['UK'];
    g.nations.UK.atWar = [...g.nations.UK.atWar, 'ITA'];
    g.headlines.push({ turn: g.turn, week: 3, text: 'The Libya convoys: 24,000 tons of shipping sunk this month.', kind: 'info', nat: 'ITA', arm: 'sea', lost: 'ITA' });
    const convoyOf = (): SeaFact => factsOf(g, NAVY.convoys).facts.find((f) => f.kind === 'convoy')!;
    expect(convoyOf().raidBy).toEqual({ boats: false, surface: false, base: true, sure: false });
    expect(subSceneOf(g, NAVY.convoys).recording).toBeNull();
    // The Mediterranean Fleet (its battleships and its boats) on patrol across the route: a scene, not "by submarines".
    g.fleets.find((f) => f.nat === 'UK' && f.name === 'Mediterranean Fleet')!.zone = g.seas.findIndex((z) => z.key === 'central');
    expect(convoyOf().raidBy).toEqual({ boats: true, surface: true, base: true, sure: false });
    const out = subSceneOf(g, NAVY.convoys);
    expect([out.pick?.case, out.pick?.boats, out.pick?.me]).toEqual(['convoy', 'UK', 'def']);
    const rec = JSON.parse(out.recording!) as Staged;
    expect(noteOf(rec).caption.lines[0]).toContain('24,000 tons of shipping sunk on the convoy routes');
    expectReadable(rec);
    expectKillsAsBooked(rec, 0);
    expectInsideTracks(rec);
  });

  it('serves the viewer\'s card setting: every month, his losses only, or never', () => {
    const t = T(1942, 6);
    const kill: SeaFact = { kind: 'torpedo', sea: 'central', turn: t, week: 2, by: '', on: 'ITA', cls: 'ca', fleet: 'the 3rd Division', boatsAt: { UK: 3 } };
    const convoy: SeaFact = { kind: 'convoy', sea: 'natl', turn: t, week: 4, by: '', on: 'UK', route: 'atlantic', routeName: 'the Atlantic convoys', tons: 90000, boats: 1, escortsLost: 0, boatsAt: { GER: 12 } };
    const huntKill: SeaFact = { kind: 'hunt', sea: 'wapp', turn: t, week: 1, by: 'UK', on: 'GER', boats: 2, escortsLost: 1 };
    const huntLost: SeaFact = { kind: 'hunt', sea: 'emed', turn: t, week: 1, by: 'ITA', on: 'UK', boats: 1, escortsLost: 0 };
    const caseOf = (fs: SeaFact[], view: 'all' | 'mine' | 'never'): string | null => pickScene(month('UK', t, fs), view)?.case ?? null;
    // Every month: his boats' kill leads; his losses only: his convoy's merchants; never: none.
    expect(caseOf([kill, convoy, huntKill], 'all')).toBe('boatsKill');
    expect(caseOf([kill, convoy, huntKill], 'mine')).toBe('convoy');
    expect(caseOf([kill, convoy, huntKill], 'never')).toBeNull();
    expect(pickScene(month('UK', t, [kill, convoy, huntKill]))!.case).toBe('boatsKill');
    // His hunters' kill is no loss of his; his boats hunted are, and so is his boats' kill where his boats were sunk there too.
    expect(caseOf([kill, huntKill], 'mine')).toBeNull();
    const lost = pickScene(month('UK', t, [kill, huntKill, huntLost]), 'mine')!;
    expect([lost.case, lost.me, lost.fact]).toEqual(['hunt', 'att', huntLost]);
    expect(caseOf([kill, { ...huntLost, sea: 'central' }], 'mine')).toBe('boatsKill');
    // The order is D1's either way: every scene the month holds, and 'mine' takes the first that is his loss.
    const every = scenesOf(month('UK', t, [kill, convoy, huntKill, huntLost]));
    expect(every.map((p) => p.case)).toEqual(['boatsKill', 'convoy', 'hunt', 'hunt']);
    expect(every.map(hisLoss)).toEqual([false, true, false, true]);
    const r = stageScene(month('UK', t, [kill, convoy, huntKill]), 'mine')!;
    expect(noteOf(r).case).toBe('convoy');
    expectReadable(r);
    // Through the bridge's call, on a game: his hunters' kill shows every month and not under 'mine'; his convoy's month does.
    const g: Game = startOpen(createGame(5, 'UK'));
    const natl = g.seas.find((z) => z.key === 'natl')!.name;
    g.headlines.push({ turn: g.turn, week: 2, text: `Escorts of the Western Approaches sink 2 German submarines in the ${natl}.`, kind: 'battle', nat: 'UK', arm: 'sea', won: 'UK', lost: 'GER' });
    expect(subSceneOf(g, NAVY.convoys).pick?.case).toBe('hunt');
    expect(subSceneOf(g, NAVY.convoys, undefined, 'all').pick?.case).toBe('hunt');
    expect(subSceneOf(g, NAVY.convoys, undefined, 'mine')).toEqual({ mark: g.headlines.length, pick: null, recording: null });
    g.headlines.push({ turn: g.turn, week: 3, text: 'The Atlantic convoys: 156,000 tons of shipping sunk this month, 2 submarines destroyed.', kind: 'info', nat: 'UK', arm: 'sea', lost: 'UK' });
    expect(subSceneOf(g, NAVY.convoys, undefined, 'mine').pick?.case).toBe('convoy');
    expect(subSceneOf(g, NAVY.convoys, undefined, 'never').recording).toBeNull();
  });

  it('reads the rules\' record (g.subLook) where it has the month\'s rows, and stands in without them', () => {
    const g = startOpen(createGame(5, 'UK'));
    const zone = (k: string): number => g.seas.findIndex((z) => z.key === k);
    const [wapp, natl, irish] = ['wapp', 'natl', 'irish'].map(zone);
    const fleet = (nat: string, name: string) => g.fleets.find((f) => f.nat === nat && f.name === name)!;
    const home = fleet('UK', 'Home Fleet'), wa = fleet('UK', 'Western Approaches Fleet');
    const uboats = fleet('GER', 'U-boat Fleet'), battle = fleet('GER', 'Battle Fleet'), itaBoats = fleet('ITA', 'Submarine Fleet');
    const at = g.seas[natl].name;
    g.headlines.push({ turn: g.turn, week: 1, text: `A submarine torpedoes and sinks a battleship of the Home Fleet in the ${at}: the fleet has too few destroyers to screen its heavy ships.`, kind: 'battle', nat: 'UK', arm: 'sea', lost: 'UK' });
    g.headlines.push({ turn: g.turn, week: 2, text: `Escorts of the Western Approaches sink 2 German submarines in the ${at}, for the loss of 1 destroyer.`, kind: 'battle', nat: 'UK', arm: 'sea', won: 'UK', lost: 'GER' });
    g.headlines.push({ turn: g.turn, week: 3, text: 'The Atlantic convoys: 156,000 tons of shipping sunk this month, 2 submarines destroyed.', kind: 'info', nat: 'UK', arm: 'sea', lost: 'UK' });
    // The record as navy.ts writes it (types.ts SubLook, claude/convoy-hook 21a3a759): Britain's month.
    const look: SubLook = {
      turn: g.turn, nat: 'UK',
      hunts: [{ zone: natl, hunters: [{ fleet: wa.id, nat: 'UK', n: 5 }], air: 4, quarry: [{ fleet: uboats.id, nat: 'GER', n: 6 }], sunk: 2, cost: 1 }],
      torpedoes: [{ fleet: home.id, zone: natl, cls: 'bb', sunk: true, could: [uboats.id, itaBoats.id], by: itaBoats.id, couldNat: ['GER', 'ITA'], byNat: 'ITA' }],
      convoys: [{ route: 'atlantic', tons: 156000, seas: [
        { zone: wapp, raiders: [{ fleet: battle.id, nat: 'GER', n: 0, heavy: battle.ships.bb + battle.ships.cv + battle.ships.ca }], escorts: [{ fleet: wa.id, nat: 'UK', n: 30 }], air: 6, tons: 90000 },
        { zone: natl, raiders: [{ fleet: uboats.id, nat: 'GER', n: 6, heavy: 0 }], escorts: [{ fleet: wa.id, nat: 'UK', n: 3 }], air: 2, tons: 60000 },
        { zone: irish, raiders: [], escorts: [], air: 1, tons: 6000 }] }],
    };
    // No record, or only another month's rows: the stand-ins (the boats at sea, the fleets, the data rows).
    const bare = factsOf(g, NAVY.convoys);
    expect(bare.facts.map((f) => [f.kind, f.attackers, f.hunters, f.raiders])).toEqual([['torpedo', undefined, undefined, undefined], ['hunt', undefined, undefined, undefined], ['convoy', undefined, undefined, undefined]]);
    expect(bare.facts[2].raidBy).toEqual({ boats: true, surface: false, base: false, sure: false });
    g.subLook = [{ ...look, turn: g.turn + 5 }];
    expect(factsOf(g, NAVY.convoys)).toEqual(bare);
    // This month's rows: read from the game, as the bridge hands it over, or passed in.
    g.subLook = [look];
    const before = JSON.stringify(g);
    const m = factsOf(g, NAVY.convoys);
    expect(factsOf({ ...g, subLook: undefined } as Game, NAVY.convoys, undefined, [look])).toEqual(m);
    expect(battle.ships.bb + battle.ships.cv + battle.ships.ca).toBeGreaterThan(0);
    const [tor, hunt, convoy] = m.facts;
    expect([tor.attackers, tor.pick]).toEqual([['ITA', 'GER'], 'rules']);
    expect(hunt.hunters).toEqual({ ships: 5, aircraft: 4 * 0.15 });
    expect(hunt.quarry).toEqual({ GER: 6 });
    // The convoy's sea is where its boats raided (not the most tons, a surface raider's); a surface raider there: no "by submarines".
    expect([convoy.sea, convoy.raiders, convoy.escorts, convoy.escortShips, convoy.airGuard, convoy.boatsNat]).toEqual(['natl', ['GER'], ['UK'], 3, 2, 'GER']);
    expect(convoy.tonsBy).toEqual({ wapp: 90000, natl: 60000, irish: 6000 });
    expect(convoy.raidBy).toEqual({ boats: true, surface: true, base: false, sure: true });
    expect(convoy.hunters).toEqual(hunt.hunters);
    // Staged from it: the torpedoing's picked attacker's boats; the record's numbers; no stand-ins where it spoke.
    const heavy = pickScene(m)!;
    expect([heavy.case, heavy.boats, heavy.source]).toEqual(['heavyLost', 'ITA', 'record']);
    const out = subSceneOf(g, NAVY.convoys);
    expect(out.pick).toEqual({ case: 'heavyLost', sea: 'natl', me: 'def', boats: 'ITA', guard: 'UK' });
    expect(JSON.stringify(g)).toBe(before);
    const rh = JSON.parse(out.recording!) as Staged;
    expectReadable(rh); expectKillsAsBooked(rh, 0); expectInsideTracks(rh);
    expect((rh.field as any).att.nat).toBe('ITA');
    const rc = stageScene({ ...m, facts: [convoy] })!;
    expectReadable(rc); expectKillsAsBooked(rc, 2); expectInsideTracks(rc);
    const nc = noteOf(rc);
    expect(nc.caption.lines).toEqual(['The Atlantic convoys, September 1939: 156,000 tons of shipping sunk on the convoy routes, about 30 ships at about 5,200 tons a ship.', '2 German submarines sunk.']);
    expect(nc.drawn.escorts).toBeLessThanOrEqual(3);
    expect(nc.drawn.hits).toBe(3);
    expect(nc.standIns).toEqual([]);
    const rt = stageScene({ ...m, facts: [hunt] })!;
    expectReadable(rt); expectKillsAsBooked(rt, 2); expectInsideTracks(rt);
    const nt = noteOf(rt);
    expect([nt.drawn.boats, nt.drawn.kills]).toEqual([3, 2]);
    expect(nt.drawn.escorts).toBeLessThanOrEqual(5);
    expect(nt.standIns).toEqual([]);
    expect(nt.caption.lines).toEqual([`2 German submarines sunk in the ${seaPlace('natl').name}, September 1939, for the loss of 1 destroyer.`]);
    // A hunt whose quarry was more than one nation's names none.
    const mixed = noteOf(stageScene({ ...m, facts: [{ ...hunt, quarry: { GER: 6, ITA: 2 } }] })!);
    expect(mixed.caption.lines).toEqual([`2 submarines sunk in the ${seaPlace('natl').name}, September 1939, for the loss of 1 destroyer.`]);
  });

  it('carries a convoy scene\'s sea kit and air cover from the record into the field, and writes neither where the record has none', () => {
    // The cover's kinds are the mirror's own: when the rules add one, only the mirror's union changes.
    const COVER_FROM_MIRROR: Both<NonNullable<SeaFact['cover']>, NonNullable<SubLook['convoys'][number]['seas'][number]['cover']>> = true;
    expect(COVER_FROM_MIRROR).toBe(true);
    const g = startOpen(createGame(5, 'UK'));
    const zone = (k: string): number => g.seas.findIndex((z) => z.key === k);
    const [wapp, natl, irish] = ['wapp', 'natl', 'irish'].map(zone);
    const fleet = (nat: string, name: string) => g.fleets.find((f) => f.nat === nat && f.name === name)!;
    const home = fleet('UK', 'Home Fleet'), wa = fleet('UK', 'Western Approaches Fleet');
    const uboats = fleet('GER', 'U-boat Fleet'), battle = fleet('GER', 'Battle Fleet');
    const at = g.seas[natl].name;
    g.headlines.push({ turn: g.turn, week: 1, text: `A submarine torpedoes and sinks a battleship of the Home Fleet in the ${at}: the fleet has too few destroyers to screen its heavy ships.`, kind: 'battle', nat: 'UK', arm: 'sea', lost: 'UK' });
    g.headlines.push({ turn: g.turn, week: 2, text: `Escorts of the Western Approaches sink 2 German submarines in the ${at}, for the loss of 1 destroyer.`, kind: 'battle', nat: 'UK', arm: 'sea', won: 'UK', lost: 'GER' });
    g.headlines.push({ turn: g.turn, week: 3, text: 'The Atlantic convoys: 156,000 tons of shipping sunk this month, 2 submarines destroyed.', kind: 'info', nat: 'UK', arm: 'sea', lost: 'UK' });
    // Britain's month with Units decide on, as a save written before the rules kept each answering navy's kit holds it:
    // each convoy sea's kit (without group training), air cover and boats sunk, and no kits. The scene's sea is the
    // North Atlantic, where the boats raided; the other seas' kit and cover differ, so a wrong sea would show.
    const look: SubLookSaved = {
      turn: g.turn, nat: 'UK',
      hunts: [{ zone: natl, hunters: [{ fleet: wa.id, nat: 'UK', n: 5 }], air: 4, quarry: [{ fleet: uboats.id, nat: 'GER', n: 6 }], sunk: 2, cost: 1 }],
      torpedoes: [{ fleet: home.id, zone: natl, cls: 'bb', sunk: true, could: [uboats.id], by: uboats.id, couldNat: ['GER'], byNat: 'GER' }],
      convoys: [{ route: 'atlantic', tons: 156000, seas: [
        { zone: wapp, raiders: [{ fleet: battle.id, nat: 'GER', n: 0, heavy: battle.ships.bb + battle.ships.cv + battle.ships.ca }], escorts: [{ fleet: wa.id, nat: 'UK', n: 30 }], air: 6, tons: 90000, kit: { asdic: true, radar: true, hedgehog: false, hfdf: false }, cover: 'carrier', lost: 0 },
        { zone: natl, raiders: [{ fleet: uboats.id, nat: 'GER', n: 6, heavy: 0 }], escorts: [{ fleet: wa.id, nat: 'UK', n: 3 }], air: 2, tons: 60000, kit: { asdic: true, radar: false, hedgehog: true, hfdf: false }, cover: 'shore', lost: 2 },
        { zone: irish, raiders: [], escorts: [], air: 1, tons: 6000, kit: { asdic: true, radar: false, hedgehog: false, hfdf: false }, cover: 'none', lost: 0 }] }],
    };
    keepLook(g, look);
    const before = JSON.stringify(g);
    const m = factsOf(g, NAVY.convoys);
    expect(JSON.stringify(g)).toBe(before);
    const convoy = m.facts.find((f) => f.kind === 'convoy')!;
    expect([convoy.sea, convoy.kit, convoy.cover]).toEqual(['natl', { asdic: true, radar: false, hedgehog: true, hfdf: false }, 'shore']);
    for (const f of m.facts.filter((x) => x.kind !== 'convoy')) expect(['kit' in f, 'cover' in f]).toEqual([false, false]);
    // Staged: the convoy's night carries its sea's kit, keyed by the side whose escorts it draws, and its cover word.
    // The torpedoing's and the hunt's never do.
    const scenes = scenesOf(m);
    expect(scenes.map((p) => p.case)).toEqual(['heavyLost', 'convoy', 'hunt']);
    for (const p of scenes) {
      const field = stagePick(m, p).field as Record<string, any>;
      if (p.case !== 'convoy') { expect(['kit' in field, 'cover' in field]).toEqual([false, false]); continue; }
      expect(field.def.nat).toBe('UK');
      expect(field.kit).toEqual({ UK: { asdic: true, radar: false, hedgehog: true, groups: false } });
      expect(field.cover).toBe('shore');
    }
    // ... and only a convoy's night: a hunt or a torpedoing made by hand with a kit and cover writes neither.
    const by = (kind: string): SeaFact => m.facts.find((f) => f.kind === kind)!;
    for (const f of [by('torpedo'), by('hunt')]) {
      const forced = { ...f, kit: { asdic: true, radar: true, hedgehog: true, hfdf: false }, cover: 'carrier' as const };
      const field = stageScene({ ...m, facts: [forced] })!.field;
      expect(['kit' in field, 'cover' in field]).toEqual([false, false]);
    }
    // Through the bridge's call: the convoy's night, read from the game.
    g.headlines.splice(-3, 1);
    const withKit = subSceneOf(g, NAVY.convoys);
    expect(withKit.pick?.case).toBe('convoy');
    const rc = JSON.parse(withKit.recording!) as Staged;
    expectReadable(rc);
    expect([(rc.field as any).kit, (rc.field as any).cover]).toEqual([{ UK: { asdic: true, radar: false, hedgehog: true, groups: false } }, 'shore']);
    // The same month with the record's kit and cover taken away (Units decide off, an older save): neither key, and
    // the rows' night. The record's kit gives the British escorts an ahead-thrower that September 1939's row has not,
    // and the escorts follow the kit; so the night without it is byte for byte the night of a kit that agrees with the
    // row (no radar, no ahead-thrower), less the two keys.
    keepLook(g, { ...look, convoys: look.convoys.map((c) => ({ ...c, seas: c.seas.map(({ kit, cover, ...s }) => s) })) });
    const without = subSceneOf(g, NAVY.convoys);
    const r0 = JSON.parse(without.recording!) as Staged;
    expect(['kit' in r0.field, 'cover' in r0.field]).toEqual([false, false]);
    const aheadRun = (r: Staged): boolean => eventsOf(r).some((e) => e.what === 'sea:depthCharge' && e.extra!.kind === 'ahead');
    expect([aheadRun(rc), aheadRun(r0)]).toEqual([true, false]);
    keepLook(g, { ...look, convoys: look.convoys.map((c) => ({ ...c, seas: c.seas.map((s) => (s.zone === natl ? { ...s, kit: { ...s.kit!, hedgehog: false } } : s)) })) });
    const asRow = JSON.parse(subSceneOf(g, NAVY.convoys).recording!) as Staged;
    delete asRow.field.kit; delete asRow.field.cover;
    expect(JSON.stringify(asRow)).toBe(without.recording);
    expect(without.pick).toEqual(withKit.pick);
    // 'none' is the record's word for a sea without air cover, not a missing word: it reaches the field as it stands,
    // where a record without the word leaves the scene to the historian's mix.
    keepLook(g, { ...look, convoys: look.convoys.map((c) => ({ ...c, seas: c.seas.map((s) => (s.zone === natl ? { ...s, cover: 'none' as const } : s)) })) });
    const bare = JSON.parse(subSceneOf(g, NAVY.convoys).recording!) as Staged;
    expect([(bare.field as any).kit, (bare.field as any).cover]).toEqual([{ UK: { asdic: true, radar: false, hedgehog: true, groups: false } }, 'none']);
    // A kit without kits that has group training keeps it, still keyed by the guard's nation.
    const trained = { ...convoy, kit: { ...convoy.kit!, groups: true } };
    expect(stageScene({ ...m, facts: [trained] })!.field.kit).toEqual({ UK: { asdic: true, radar: false, hedgehog: true, groups: true } });
  });

  it('keys a convoy night\'s kit by each navy that answered, as the record\'s kits hold them, with their group training', () => {
    const g = startOpen(createGame(5, 'UK'));
    const zone = (k: string): number => g.seas.findIndex((z) => z.key === k);
    const [wapp, natl] = ['wapp', 'natl'].map(zone);
    const fleet = (nat: string, name: string) => g.fleets.find((f) => f.nat === nat && f.name === name)!;
    const wa = fleet('UK', 'Western Approaches Fleet'), uboats = fleet('GER', 'U-boat Fleet');
    g.headlines.push({ turn: g.turn, week: 3, text: 'The Atlantic convoys: 156,000 tons of shipping sunk this month, 2 submarines destroyed.', kind: 'info', nat: 'UK', arm: 'sea', lost: 'UK' });
    // Britain's month as the rules write it since they keep each answering navy's kit: in the North Atlantic, the
    // scene's sea, British and American escorts answered, each navy with its own research; in the Western Approaches,
    // the British alone, with other kit, so that a wrong sea would show. `kit` is the navy with most destroyers
    // answering there (here the Americans, whose hunting groups there are not the convoy's escorts): the scene keys by kits.
    const KIT = { asdic: true, hfdf: false };
    const uk = { ...KIT, radar: true, hedgehog: true, groups: true }, usa = { ...KIT, radar: false, hedgehog: false, groups: false };
    const look = (escorts: SubLookFleet[], kits: Record<string, typeof uk> = { UK: { ...uk }, USA: { ...usa } }, kit = { ...usa }): SubLook => ({
      turn: g.turn, nat: 'UK', hunts: [], torpedoes: [],
      convoys: [{ route: 'atlantic', tons: 156000, seas: [
        { zone: wapp, raiders: [], escorts: [{ fleet: wa.id, nat: 'UK', n: 9 }], air: 6, tons: 0, kit: { ...uk, radar: false }, kits: { UK: { ...uk, radar: false } }, cover: 'carrier', lost: 0 },
        { zone: natl, raiders: [{ fleet: uboats.id, nat: 'GER', n: 6, heavy: 0 }], escorts, air: 2, tons: 156000, kit, kits, cover: 'shore', lost: 2 }] }],
    });
    keepLook(g, look([{ fleet: wa.id, nat: 'UK', n: 3 }, { fleet: 9002, nat: 'USA', n: 2 }]));
    const m = factsOf(g, NAVY.convoys);
    const convoy = m.facts.find((f) => f.kind === 'convoy')!;
    expect([convoy.sea, convoy.kit, convoy.kits]).toEqual(['natl', usa, { UK: uk, USA: usa }]);
    // Staged: one entry for each navy that answered, its own kit and group training, without the direction finder;
    // the navy whose escorts are drawn (the most there, British) has its entry.
    const want = { UK: { asdic: true, radar: true, hedgehog: true, groups: true }, USA: { asdic: true, radar: false, hedgehog: false, groups: false } };
    const drawn = (r: Staged): string[] => [...new Set([...playOut(r)[0].units.values()].filter((u) => u.side === 'def' && u.kind === 'dd').map((u) => u.nat as string))];
    const rc = JSON.parse(subSceneOf(g, NAVY.convoys).recording!) as Staged;
    expectReadable(rc);
    expect([(rc.field as any).def.nat, (rc.field as any).kit, drawn(rc)]).toEqual(['UK', want, ['UK']]);
    // With more American destroyers there, American escorts are drawn, and the kit is the same by navy.
    keepLook(g, look([{ fleet: wa.id, nat: 'UK', n: 3 }, { fleet: 9002, nat: 'USA', n: 5 }]));
    const ru = JSON.parse(subSceneOf(g, NAVY.convoys).recording!) as Staged;
    expect([(ru.field as any).def.nat, (ru.field as any).kit, drawn(ru)]).toEqual(['UK', want, ['USA']]);
    // The escorts drawn are always a navy that answered. American destroyers that only guard the route loosely in the
    // sea (on patrol there, or escorting another route) are in its escorts but did not answer: the British close escort,
    // staged in the Western Approaches, is drawn, and the field's kit is the British alone.
    keepLook(g, look([{ fleet: 9002, nat: 'USA', n: 30 }], { UK: { ...uk } }, { ...uk }));
    const rl = JSON.parse(subSceneOf(g, NAVY.convoys).recording!) as Staged;
    expect([(rl.field as any).def.nat, (rl.field as any).kit, drawn(rl)]).toEqual(['UK', { UK: want.UK }, ['UK']]);
    // Where the sea names no escort of its own and more than one navy answered (close escorts staged in the route's
    // other seas), the navy whose kit is the record's kit (the one with most destroyers answering) is drawn, whichever
    // comes first in the kits.
    for (const [kit, nat] of [[usa, 'USA'], [uk, 'UK']] as const) {
      keepLook(g, look([], { UK: { ...uk }, USA: { ...usa } }, { ...kit }));
      const rk = JSON.parse(subSceneOf(g, NAVY.convoys).recording!) as Staged;
      expect([(rk.field as any).kit, drawn(rk)]).toEqual([want, [nat]]);
    }
  });

  it('stages the drawn escorts\' radar and ahead-throwers by their own navy\'s kit where the record has one, and by the rows as before where it has none', () => {
    const KIT = { asdic: true, hfdf: false, groups: false };
    const bare = { ...KIT, radar: false, hedgehog: false }, full = { ...KIT, radar: true, hedgehog: true };
    const night = (year: number, m1: number, extra: Partial<SeaFact> = {}, seed = 1): MonthFacts => {
      const t = T(year, m1);
      return month('UK', t, [{ kind: 'convoy', sea: 'natl', turn: t, week: 2, by: '', on: 'UK', route: 'atlantic', routeName: 'the Atlantic convoys', tons: 60000, boats: 3, escortsLost: 0, boatsAt: { GER: 10 }, hunters: { ships: 6, aircraft: 0 }, ...extra }], seed);
    };
    // (early: the surfaced boats that fired and crash-dived before their fans' first hit could tell them to, 2 s after
    // the first torpedo's run; only a screen with radar, turning on them as they fire, sends them down that soon)
    const tally = (r: Staged) => {
      const ev = eventsOf(r);
      const fans = ev.filter((e) => e.what === 'sea:torpedo'), firstHit = Math.min(...fans.map((e) => e.t + (e.extra!.running as number)));
      return {
        radar: ev.filter((e) => e.what === 'sea:sonar' && e.extra!.kind === 'radar').length,
        dive: ev.filter((e) => e.what === 'sea:dive' && e.extra!.why === 'radar').length,
        early: ev.filter((e) => e.what === 'sea:dive' && e.text!.endsWith('crash-dives.') && fans.some((x) => x.unit === e.unit) && e.t < firstHit + 1.5).length,
        ahead: ev.filter((e) => e.what === 'sea:depthCharge' && e.extra!.kind === 'ahead').map((e) => e.text),
        star: ev.filter((e) => e.what === 'sea:starshell').length,
      };
    };
    const drawn = (r: Staged): string[] => [...new Set([...playOut(r)[0].units.values()].filter((u) => u.side === 'def' && u.kind === 'dd').map((u) => u.nat as string))];
    const staged = (m: MonthFacts): Staged => { const r = stageScene(m)!; expectReadable(r); expectKillsAsBooked(r, 3); expectInsideTracks(r); return r; };
    /** The recording less its kit: what the events and pieces were. */
    const less = (r: Staged): string => { const x = JSON.parse(JSON.stringify(r)) as Staged; delete x.field.kit; return JSON.stringify(x); };
    // Without a kit: the rows, byte for byte as before this change (the recordings' digests as the stager wrote them
    // at f1ce92d8). May 1943's British row has radar and Hedgehog, September 1942's radar only, October 1940's neither.
    const before: [number, number, string][] = [[1943, 5, DIGEST_1943], [1942, 9, DIGEST_1942], [1940, 10, DIGEST_1940]];
    for (const [y, m1, d] of before) expect(`${y}-${m1} ${digest(JSON.stringify(staged(night(y, m1))))}`).toBe(`${y}-${m1} ${d}`);
    const row = staged(night(1943, 5));
    const r0 = tally(row);
    expect([r0.radar > 0, r0.dive > 0, r0.early > 0, r0.ahead.length > 0, r0.star > 0]).toEqual([true, true, true, true, true]);
    expect(r0.ahead.every((x) => x === 'A frigate fires her Hedgehog.')).toBe(true);
    // The British kit without radar or hedgehog: no radar contact, no boat diving for radar or going down before her
    // hits tell, no ahead-thrower run; the starshell is the row's still (the record's kit says nothing of it).
    const none = staged(night(1943, 5, { kits: { UK: bare } })), t1 = tally(none);
    expect([t1.radar, t1.dive, t1.early, t1.ahead, t1.star > 0]).toEqual([0, 0, 0, [], true]);
    expect([noteOf(none).looks.radar, noteOf(none).looks.ahead, none.field.kit]).toEqual([false, null, { UK: { asdic: true, radar: false, hedgehog: false, groups: false } }]);
    // A kit that agrees with the row stages the row's night: the same bytes but for the field's kit.
    expect(less(staged(night(1943, 5, { kits: { UK: full } })))).toBe(JSON.stringify(row));
    // The drawn escorts' own navy's: American escorts drawn (the most there) follow the American kit, British the British.
    expect(tally(staged(night(1943, 5, { escorts: ['USA', 'UK'], kits: { UK: full, USA: bare } })))).toMatchObject({ radar: 0, dive: 0, early: 0, ahead: [] });
    expect(tally(staged(night(1943, 5, { escorts: ['UK', 'USA'], kits: { UK: bare, USA: full } })))).toMatchObject({ radar: 0, dive: 0, early: 0, ahead: [] });
    expect(less(staged(night(1943, 5, { escorts: ['UK', 'USA'], kits: { UK: full, USA: bare } })))).toBe(less(staged(night(1943, 5, { escorts: ['UK', 'USA'] }))));
    // An older record's kit (no kits) is the guard's: it holds for British escorts, not for American ones drawn there.
    expect(tally(staged(night(1943, 5, { kit: { asdic: true, radar: false, hedgehog: false, hfdf: false } })))).toMatchObject({ radar: 0, dive: 0, early: 0, ahead: [] });
    const ally = night(1943, 5, { escorts: ['USA'], kit: { ...bare } });
    expect(less(staged(ally))).toBe(JSON.stringify(staged(night(1943, 5, { escorts: ['USA'] }))));
    expect(stageScene(ally)!.field.kit).toEqual({ UK: { asdic: true, radar: false, hedgehog: false, groups: false } });
    // A navy the record's kits leave out is not drawn: neither one that only guards the route loosely in the sea, nor
    // the guard where only an ally answered. The escorts drawn are a navy that answered, with its own kit, and the
    // field names only the navies that answered.
    for (const [extra, nat] of [[{ escorts: ['USA'], kits: { UK: bare } }, 'UK'], [{ kits: { USA: bare } }, 'USA']] as [Partial<SeaFact>, string][]) {
      const left = staged(night(1943, 5, extra));
      expect([drawn(left), Object.keys(left.field.kit as object), tally(left)]).toMatchObject([[nat], [nat], { radar: 0, dive: 0, early: 0, ahead: [] }]);
    }
    // With hedgehog where the row has no ahead-thrower (September 1942's corvettes): ahead-thrower runs, in plain words.
    const hh = tally(staged(night(1942, 9, { kits: { UK: full } })));
    expect(tally(staged(night(1942, 9))).ahead).toEqual([]);
    expect(hh.ahead.length).toBeGreaterThan(0);
    expect(hh.ahead.every((x) => x === 'A corvette fires her ahead-throwing weapon.')).toBe(true);
    // With radar where the row has none (October 1940's corvettes): radar contacts and boats diving for it.
    expect(tally(staged(night(1940, 10)))).toMatchObject({ radar: 0, dive: 0, early: 0 });
    const rr = tally(staged(night(1940, 10, { kits: { UK: { ...bare, radar: true } } })));
    expect([rr.radar > 0, rr.dive > 0, rr.early > 0, rr.ahead]).toEqual([true, true, true, []]);
    // A fleet's own screen and a hunt keep their looks whatever kit a fact made by hand carries.
    for (const f of [
      { kind: 'torpedo', sea: 'natl', turn: T(1943, 5), week: 4, by: '', on: 'UK', cls: 'bb', fleet: 'the Home Fleet', ships: { bb: 3, dd: 8 }, boatsAt: { GER: 2 } },
      { kind: 'hunt', sea: 'natl', turn: T(1943, 5), week: 2, by: 'UK', on: 'GER', boats: 2, escortsLost: 0 },
    ] as SeaFact[]) {
      const own = stageScene(month('UK', T(1943, 5), [f]))!, made = stageScene(month('UK', T(1943, 5), [{ ...f, kit: { ...bare }, kits: { UK: bare } }]));
      expect([noteOf(own).case, JSON.stringify(made)]).toEqual([f.kind === 'hunt' ? 'hunt' : 'heavyLost', JSON.stringify(own)]);
    }
  });

  it('holds to the record\'s own word (review of 22 November): the picked attacker, one fleet a scene, who raided, the boats lost, whose kills', () => {
    const g = startOpen(createGame(5, 'UK'));
    g.nations.ITA.atWar = [...new Set([...g.nations.ITA.atWar, 'UK'])];
    g.nations.UK.atWar = [...new Set([...g.nations.UK.atWar, 'ITA'])];
    const zone = (k: string): number => g.seas.findIndex((z) => z.key === k);
    const [wapp, natl, irish] = ['wapp', 'natl', 'irish'].map(zone);
    const fleet = (nat: string, name: string) => g.fleets.find((f) => f.nat === nat && f.name === name)!;
    const home = fleet('UK', 'Home Fleet'), wa = fleet('UK', 'Western Approaches Fleet');
    const uboats = fleet('GER', 'U-boat Fleet'), battle = fleet('GER', 'Battle Fleet'), itaBoats = fleet('ITA', 'Submarine Fleet');
    const at = g.seas[natl].name;
    const torpedo = (name: string, cls: string, week: number) => ({ turn: g.turn, week, text: `A submarine torpedoes and sinks a ${cls} of the ${name} in the ${at}: the fleet has too few destroyers to screen its heavy ships.`, kind: 'battle' as const, nat: 'UK', arm: 'sea' as const, lost: 'UK' });
    const mark = g.headlines.length;
    g.headlines.push(torpedo('Home Fleet', 'battleship', 1));
    // (the record names the nations itself, couldNat and byNat; the fleet 9001 is one gone from the list since)
    const NAT: Record<number, string> = { [uboats.id]: 'GER', [itaBoats.id]: 'ITA', 9001: 'ITA' };
    const look = (by: number | null, could = [uboats.id, itaBoats.id]): SubLook => ({ turn: g.turn, nat: 'UK', hunts: [], convoys: [],
      torpedoes: [{ fleet: home.id, zone: natl, cls: 'bb', sunk: true, could, by, couldNat: could.map((id) => NAT[id]), byNat: by === null ? null : NAT[by] }] });
    /** The same torpedoing as an older save's record holds it (written before the rules' 21a3a759): no couldNat, no byNat. */
    const older = (by: number | null, could = [uboats.id, itaBoats.id]): SubLookSaved => ({ turn: g.turn, nat: 'UK', hunts: [], convoys: [], torpedoes: [{ fleet: home.id, zone: natl, cls: 'bb', could, by }] });
    const pickFor = (human: string) => { g.human = human; const p = pickScene(factsOf(g, NAVY.convoys, mark)); g.human = 'UK'; return p ? [p.case, p.boats, p.source] : null; };
    // 1. The rules picked the German boats: an Italian player is shown no kill of his, a German one is, and a British
    // one sees German boats; the Italian boats only where the rules picked them.
    g.subLook = [look(uboats.id)];
    expect([pickFor('ITA'), pickFor('GER'), pickFor('UK')]).toEqual([null, ['boatsKill', 'GER', 'record'], ['heavyLost', 'GER', 'record']]);
    g.subLook = [look(itaBoats.id)];
    expect([pickFor('ITA'), pickFor('GER'), pickFor('UK')]).toEqual([['boatsKill', 'ITA', 'record'], null, ['heavyLost', 'ITA', 'record']]);
    // ... the pick's fleet struck from the list since and named nowhere else in the record: its nation is the record's
    // own byNat (round 3: no longer looked for on the list or in the record's other rows).
    g.subLook = [look(9001, [uboats.id, 9001])];
    const named = factsOf(g, NAVY.convoys, mark).facts[0];
    expect([named.attackers, named.pick]).toEqual([['ITA', 'GER'], 'rules']);
    expect([pickFor('ITA'), pickFor('GER'), pickFor('UK')]).toEqual([['boatsKill', 'ITA', 'record'], null, ['heavyLost', 'ITA', 'record']]);
    // ... an older save's record, without the nations: the pick is not known, and no one's boats are shown the kill (his
    // loss is staged with a stand-in, said so). Its other rows are not searched for the nation any more.
    const gone = { fleet: 9001, nat: 'ITA', n: 4 };
    keepLook(g, { ...older(9001, [uboats.id, 9001]), hunts: [{ zone: natl, hunters: [{ fleet: wa.id, nat: 'UK', n: 4 }], air: 2, quarry: [gone], sunk: 0, cost: 0 }] });
    const unknown = factsOf(g, NAVY.convoys, mark).facts[0];
    expect([unknown.attackers, unknown.pick]).toEqual([[], 'unknown']);
    // (no boats of his foes at sea there as the month closed: the basin's rows stand in)
    expect([pickFor('ITA'), pickFor('GER'), pickFor('UK')]).toEqual([null, null, ['heavyLost', 'GER', 'rows']]);
    expect(noteOf(stagePick(factsOf(g, NAVY.convoys, mark), pickScene(factsOf(g, NAVY.convoys, mark))!)).standIns[0]).toContain("the boats' navy");
    keepLook(g, older(itaBoats.id));
    expect([factsOf(g, NAVY.convoys, mark).facts[0].pick, pickFor('ITA'), pickFor('GER'), pickFor('UK')]).toEqual(['unknown', null, null, ['heavyLost', 'GER', 'rows']]);
    // ... no pick (byNat null, as `by`): the one case where the scene is still unsure which boats fired. It stands in one
    // of those the record names as able to have fired, and says so; an older record's no-pick names none, and no one's
    // boats are shown the kill.
    g.subLook = [look(null)];
    const none = factsOf(g, NAVY.convoys, mark).facts[0];
    expect([none.attackers, none.pick]).toEqual([['GER', 'ITA'], 'none']);
    expect([pickFor('ITA'), pickFor('GER'), pickFor('UK')]).toEqual([['boatsKill', 'ITA', 'fleets'], ['boatsKill', 'GER', 'fleets'], ['heavyLost', 'GER', 'fleets']]);
    keepLook(g, older(null));
    expect([factsOf(g, NAVY.convoys, mark).facts[0].attackers, pickFor('ITA'), pickFor('GER'), pickFor('UK')]).toEqual([[], null, null, ['heavyLost', 'GER', 'rows']]);
    // 2. Two fleets' heavy ships torpedoed there: the scene draws the picked one's fleet alone, and the caption names
    // the other on a line of its own.
    g.subLook = [];
    g.headlines.push(torpedo('Western Approaches Fleet', 'cruiser', 3));
    const two = factsOf(g, NAVY.convoys, mark);
    const p2 = pickScene(two)!;
    expect([p2.case, p2.heavy]).toEqual(['heavyLost', ['bb']]);
    expect(noteOf(stagePick(two, p2)).caption.lines.slice(0, 2)).toEqual([`A battleship of the Home Fleet torpedoed and sunk in the ${at}, September 1939.`, 'Also torpedoed and sunk there: a cruiser of the Western Approaches Fleet.']);
    // 5. Another navy's hunt there: in the caption's count, not drawn as his fleet's screen's kills.
    g.headlines.push({ turn: g.turn, week: 4, text: `The month in the air and at sea, the United States: 2 German submarines sunk in the ${at}.`, kind: 'info', nat: 'USA' });
    const usa = factsOf(g, NAVY.convoys, mark);
    const p5 = pickScene(usa)!;
    expect([p5.case, p5.boats, p5.kills, p5.sunkThere]).toEqual(['heavyLost', 'GER', 0, 2]);
    const r5 = stagePick(usa, p5);
    expect(noteOf(r5).caption.lines[2]).toBe('2 German submarines sunk there this month.');
    expectKillsAsBooked(r5, 0);
    g.headlines.splice(mark);
    // 3. Who raided, from an older save's record (raiders without `heavy`): a raider with no boats as it raided was a
    // surface raider, however the fleet list stands now; a boat raider whose fleet is gone, or has lost ships since,
    // cannot be settled: no "by submarines".
    g.headlines.push({ turn: g.turn, week: 3, text: 'The Atlantic convoys: 156,000 tons of shipping sunk this month.', kind: 'info', nat: 'UK', arm: 'sea', lost: 'UK' });
    const convoyLook = (raiders: { zone: number; raiders: { fleet: number; nat: string; n: number; heavy?: number }[]; tons: number }[]): SubLook => ({ turn: g.turn, nat: 'UK', hunts: [], torpedoes: [],
      convoys: [{ route: 'atlantic', tons: 156000, seas: raiders.map((x) => ({ ...x, escorts: [{ fleet: wa.id, nat: 'UK', n: 6 }], air: 2 })) }] });
    const cap = (): [SeaFact['raidBy'], string] => { const m = factsOf(g, NAVY.convoys, mark); const f = m.facts[0]; return [f.raidBy, noteOf(stagePick(m, pickScene(m)!)).caption.lines[0]]; };
    g.subLook = [convoyLook([{ zone: wapp, raiders: [{ fleet: battle.id, nat: 'GER', n: 0 }], tons: 90000 }, { zone: natl, raiders: [{ fleet: uboats.id, nat: 'GER', n: 6 }], tons: 66000 }])];
    const keep = { ...battle.ships };
    battle.ships.bb = 0; battle.ships.ca = 0; battle.ships.cv = 0;
    expect(cap()).toEqual([{ boats: true, surface: true, base: false, sure: true }, expect.stringContaining('sunk on the convoy routes')]);
    Object.assign(battle.ships, keep);
    g.fleets.splice(g.fleets.indexOf(battle), 1);
    expect(cap()[0]).toEqual({ boats: true, surface: true, base: false, sure: true });
    g.fleets.push(battle);
    // (boats alone, the fleet still on the list and no loss of its told: "by submarines"; its fleet gone: not settled)
    g.subLook = [convoyLook([{ zone: natl, raiders: [{ fleet: uboats.id, nat: 'GER', n: 6 }], tons: 156000 }])];
    expect(cap()).toEqual([{ boats: true, surface: false, base: false, sure: true }, expect.stringContaining('sunk by submarines')]);
    g.subLook = [convoyLook([{ zone: natl, raiders: [{ fleet: 9002, nat: 'GER', n: 6 }], tons: 156000 }])];
    expect(cap()).toEqual([{ boats: true, surface: false, base: false, sure: false }, expect.stringContaining('sunk on the convoy routes')]);
    // (the record's own heavy-ship count, where the rules give it, settles it)
    g.subLook = [convoyLook([{ zone: natl, raiders: [{ fleet: 9002, nat: 'GER', n: 6, heavy: 0 }], tons: 156000 }])];
    expect(cap()[0]).toEqual({ boats: true, surface: false, base: false, sure: true });
    // ... and as the rules write it since 21a3a759 (every raider with `heavy`, 0 included), who raided is read off the
    // rows alone: a raider whose ships the news says were lost since, or whose heavy ships the list no longer shows,
    // still reads as it raided; a raider with boats and heavy ships both raided as both.
    g.headlines.push({ turn: g.turn, week: 4, text: 'The U-boat Fleet has lost 2 boats this month.', kind: 'info', nat: 'GER' });
    g.subLook = [convoyLook([{ zone: natl, raiders: [{ fleet: uboats.id, nat: 'GER', n: 6 }], tons: 156000 }])];
    expect(cap()[0]).toEqual({ boats: true, surface: false, base: false, sure: false });
    g.subLook = [convoyLook([{ zone: natl, raiders: [{ fleet: uboats.id, nat: 'GER', n: 6, heavy: 0 }], tons: 156000 }])];
    expect(cap()).toEqual([{ boats: true, surface: false, base: false, sure: true }, expect.stringContaining('sunk by submarines')]);
    battle.ships.bb = 0; battle.ships.ca = 0; battle.ships.cv = 0;
    g.subLook = [convoyLook([{ zone: wapp, raiders: [{ fleet: battle.id, nat: 'GER', n: 0, heavy: 3 }], tons: 90000 }, { zone: natl, raiders: [{ fleet: uboats.id, nat: 'GER', n: 6, heavy: 0 }], tons: 66000 }])];
    expect(cap()).toEqual([{ boats: true, surface: true, base: false, sure: true }, expect.stringContaining('sunk on the convoy routes')]);
    Object.assign(battle.ships, keep);
    g.subLook = [convoyLook([{ zone: natl, raiders: [{ fleet: 9002, nat: 'GER', n: 4, heavy: 1 }], tons: 156000 }])];
    expect(cap()).toEqual([{ boats: true, surface: true, base: false, sure: true }, expect.stringContaining('sunk on the convoy routes')]);
    expect(noteOf(stagePick(factsOf(g, NAVY.convoys, mark), pickScene(factsOf(g, NAVY.convoys, mark))!)).standIns.filter((x) => x.startsWith('who raided'))).toEqual([]);
    g.headlines.splice(mark);
    // 6. Under half a ship's tons, the ships are not counted.
    const small: SeaFact = { kind: 'convoy', sea: 'central', turn: T(1942, 3), week: 2, by: '', on: 'ITA', route: 'libya', routeName: 'the Libya convoys', tons: 2000, boats: 0, escortsLost: 0, raidBy: { boats: true, surface: false, base: false } };
    expect(noteOf(stageScene(month('ITA', T(1942, 3), [small]))!).caption.lines).toEqual(['The Libya convoys, March 1942: 2,000 tons of shipping sunk by submarines.']);
  });

  it('reads an older save\'s history record, whose raiders are counted after the convoy\'s boats were lost (review of 22 November; the bound kept in round 3)', () => {
    const g = createGame(5, 'UK');
    expect((g.flags as { open?: number }).open).not.toBe(1);
    g.nations.ITA.atWar = [...new Set([...g.nations.ITA.atWar, 'UK'])];
    g.nations.UK.atWar = [...new Set([...g.nations.UK.atWar, 'ITA'])];
    const natl = g.seas.findIndex((z) => z.key === 'natl');
    const fleet = (nat: string, name: string) => g.fleets.find((f) => f.nat === nat && f.name === name)!;
    const uboats = fleet('GER', 'U-boat Fleet'), itaBoats = fleet('ITA', 'Submarine Fleet'), wa = fleet('UK', 'Western Approaches Fleet');
    for (const f of [uboats, itaBoats]) { f.ships.bb = 0; f.ships.ca = 0; f.ships.cv = 0; }
    const mark = g.headlines.length;
    const month1 = (lost: number, raiders: { fleet: number; nat: string; n: number }[]) => {
      g.headlines.splice(mark);
      g.headlines.push({ turn: g.turn, week: 3, text: `The Atlantic convoys: 176,000 tons of shipping sunk this month, ${lost} submarine${lost === 1 ? '' : 's'} destroyed.`, kind: 'info', nat: 'UK', arm: 'sea', lost: 'UK' });
      g.subLook = [{ turn: g.turn, nat: 'UK', hunts: [], torpedoes: [], convoys: [{ route: 'atlantic', tons: 176000, seas: [{ zone: natl, raiders, escorts: [{ fleet: wa.id, nat: 'UK', n: 8 }], air: 2, tons: 176000 }] }] }];
      const m = factsOf(g, NAVY.convoys, mark);
      return { f: m.facts[0], scenes: scenesOf(m), lines: scenesOf(m).length ? noteOf(stagePick(m, scenesOf(m)[0])).caption.lines : [] };
    };
    // A German and an Italian boat lost: the Italian fleet's n is 0 after its one boat went, and it still raided: no nation named.
    const both = month1(2, [{ fleet: uboats.id, nat: 'GER', n: 19 }, { fleet: itaBoats.id, nat: 'ITA', n: 0 }]);
    expect([both.f.raidBy, both.f.raiders, both.f.boatsNat]).toEqual([{ boats: true, surface: false, base: false, sure: true }, ['GER', 'ITA'], undefined]);
    expect(both.lines).toEqual(['The Atlantic convoys, September 1939: 176,000 tons of shipping sunk by submarines, about 34 ships at about 5,200 tons a ship.', '2 submarines sunk.']);
    // A lone raiding group wiped out: its boats raided, and the month has its scene, for him and as his loss.
    const lone = month1(1, [{ fleet: uboats.id, nat: 'GER', n: 0 }]);
    expect([lone.f.raidBy, lone.f.boatsNat, lone.scenes.map((p) => [p.case, p.boats, p.kills])]).toEqual([{ boats: true, surface: false, base: false, sure: true }, 'GER', [['convoy', 'GER', 1]]]);
    expect(lone.lines[1]).toBe('1 German submarine sunk.');
    // (with no boats lost, a raider of n 0 had none as it raided: a surface raider, and no submarine scene alone)
    expect(month1(0, [{ fleet: uboats.id, nat: 'GER', n: 0 }]).scenes).toEqual([]);
  });

  it('reads a history campaign\'s record as the rules write it since 21a3a759: each raider\'s boats as the month began, and its heavy ships (round 3, finding 4)', () => {
    const g = createGame(5, 'UK');
    expect((g.flags as { open?: number }).open).not.toBe(1);
    g.nations.ITA.atWar = [...new Set([...g.nations.ITA.atWar, 'UK'])];
    g.nations.UK.atWar = [...new Set([...g.nations.UK.atWar, 'ITA'])];
    const natl = g.seas.findIndex((z) => z.key === 'natl');
    const fleet = (nat: string, name: string) => g.fleets.find((f) => f.nat === nat && f.name === name)!;
    const uboats = fleet('GER', 'U-boat Fleet'), itaBoats = fleet('ITA', 'Submarine Fleet'), wa = fleet('UK', 'Western Approaches Fleet');
    const mark = g.headlines.length;
    const month1 = (lost: number, raiders: { fleet: number; nat: string; n: number; heavy?: number }[]) => {
      g.headlines.splice(mark);
      g.headlines.push({ turn: g.turn, week: 3, text: `The Atlantic convoys: 176,000 tons of shipping sunk this month${lost ? `, ${lost} submarine${lost === 1 ? '' : 's'} destroyed` : ''}.`, kind: 'info', nat: 'UK', arm: 'sea', lost: 'UK' });
      keepLook(g, { turn: g.turn, nat: 'UK', hunts: [], torpedoes: [], convoys: [{ route: 'atlantic', tons: 176000, seas: [{ zone: natl, raiders, escorts: [{ fleet: wa.id, nat: 'UK', n: 8 }], air: 2, tons: 176000 }] }] });
      const m = factsOf(g, NAVY.convoys, mark);
      const scenes = scenesOf(m);
      return { f: m.facts[0], scenes: scenes.map((p) => [p.case, p.boats, p.kills]), lines: scenes.length ? noteOf(stagePick(m, scenes[0])).caption.lines : [], standIns: scenes.length ? noteOf(stagePick(m, scenes[0])).standIns : [] };
    };
    const sure = { boats: true, surface: false, base: false, sure: true };
    // A German and an Italian boat lost, each fleet with its boats as the month began: both raided, and no nation is named.
    const both = month1(2, [{ fleet: uboats.id, nat: 'GER', n: 20, heavy: 0 }, { fleet: itaBoats.id, nat: 'ITA', n: 1, heavy: 0 }]);
    expect([both.f.raidBy, both.f.raiders, both.f.boatsNat]).toEqual([sure, ['GER', 'ITA'], undefined]);
    expect(both.lines).toEqual(['The Atlantic convoys, September 1939: 176,000 tons of shipping sunk by submarines, about 34 ships at about 5,200 tons a ship.', '2 submarines sunk.']);
    // A lone raiding group wiped out, and struck from the list since: its row still shows the boat it raided with, so
    // the record settles it: "by submarines", the boat named German, the scene his convoy's.
    const lone = month1(1, [{ fleet: 9003, nat: 'GER', n: 1, heavy: 0 }]);
    expect([lone.f.raidBy, lone.f.raiders, lone.f.boatsNat, lone.scenes]).toEqual([sure, ['GER'], 'GER', [['convoy', 'GER', 1]]]);
    expect(lone.lines).toEqual(['The Atlantic convoys, September 1939: 176,000 tons of shipping sunk by submarines, about 34 ships at about 5,200 tons a ship.', '1 German submarine sunk.']);
    expect(lone.standIns.filter((x) => x.startsWith('who raided'))).toEqual([]);
    // (the same month as an older save holds it, n 0 after the roll and no heavy: the bound holds, and with the fleet gone
    // who raided is not settled: no "by submarines", said so)
    const loneOld = month1(1, [{ fleet: 9003, nat: 'GER', n: 0 }]);
    expect([loneOld.f.raidBy, loneOld.f.boatsNat, loneOld.scenes]).toEqual([{ ...sure, sure: false }, 'GER', [['convoy', 'GER', 1]]]);
    expect(loneOld.lines[0]).toContain('sunk on the convoy routes');
    expect(loneOld.standIns).toContain("who raided: an older save's record, without the raiders' heavy ships; the fleets as the scene is read");
    // A surface raider (no boats as the month began, heavy ships) and boats alongside: both raided, read off the rows.
    const mixed = month1(1, [{ fleet: 9004, nat: 'GER', n: 0, heavy: 2 }, { fleet: uboats.id, nat: 'GER', n: 6, heavy: 0 }]);
    expect([mixed.f.raidBy, mixed.f.raiders, mixed.f.boatsNat]).toEqual([{ ...sure, surface: true }, ['GER'], 'GER']);
    expect(mixed.lines[0]).toContain('sunk on the convoy routes');
    // A surface raider alone, no boats lost: no submarine scene (as before; now read off its heavy ships, not guessed).
    expect(month1(0, [{ fleet: 9004, nat: 'GER', n: 0, heavy: 2 }]).f.raidBy).toEqual({ boats: false, surface: true, base: false, sure: true });
    expect(month1(0, [{ fleet: 9004, nat: 'GER', n: 0, heavy: 2 }]).scenes).toEqual([]);
    // The bound: a history route's boats lost came off its raiders, so rows that count fewer boats than were lost are not
    // the month's opening count (an older record's rows, whatever they carry): read with the bound, as round 2 read them.
    const short = month1(1, [{ fleet: uboats.id, nat: 'GER', n: 0, heavy: 0 }]);
    expect([short.f.raidBy, short.f.raiders, short.f.boatsNat, short.scenes]).toEqual([sure, ['GER'], 'GER', [['convoy', 'GER', 1]]]);
  });

  it('mirrors the rules\' record both ways, field for field at every depth (round 3; a compile-time check, above)', () => {
    expect([MIRROR_IS_RECORD, SAME_FIELDS]).toEqual([true, true]);
  });

  it('reads a torpedoing as the real rules record it: the pick\'s nation, and the nations that could have fired (round 3)', () => {
    const g = startOpen(createGame(3, 'UK'));
    const home = g.fleets.find((f) => f.nat === 'UK' && f.ships.bb > 0)!;
    const boats = g.fleets.find((f) => f.nat === 'GER' && f.ships.ss > 0)!;
    const mark = g.headlines.length;
    subScene.on = true;
    try {
      // (a torpedoing made likely, as the record's own test makes it: the fleet without a screen among the boats)
      for (let i = 0; i < 60 && !(g.subLook ?? []).some((l) => l.torpedoes.length); i++) {
        home.ships.dd = 0; boats.zone = home.zone; boats.stance = 'raid';
        navalUpkeep(g, 'UK');
        if (!(g.subLook ?? []).some((l) => l.torpedoes.length)) g.turn++;
      }
    } finally { subScene.on = false; }
    // (the record read through the mirror's type: the two are one shape at compile time, above; at run time they are
    // the same objects, so sending them back and forth would check nothing)
    const rec = (g.subLook ?? []).map(toMirror);
    const tp = rec.flatMap((l) => l.torpedoes);
    expect(tp.length).toBeGreaterThan(0);
    for (const t of tp) expect([t.couldNat.length, t.byNat]).toEqual([t.could.length, t.by === null ? null : t.couldNat[t.could.indexOf(t.by)]]);
    for (const x of rec.flatMap((l) => l.convoys.flatMap((c) => c.seas.flatMap((s) => s.raiders)))) expect(typeof x.heavy).toBe('number');
    // The stager reads them as the record names them: each torpedoing's attackers are its nations, the pick's first.
    const m = factsOf(g, NAVY.convoys, mark, (g.subLook ?? []).map(asSaved));
    const tors = m.facts.filter((f) => f.kind === 'torpedo' && f.attackers);
    expect(tors.length).toBeGreaterThan(0);
    for (const f of tors) {
      const r = tp.find((t) => t.zone === g.seas.findIndex((z) => z.key === f.sea) && t.cls === f.cls)!;
      expect([f.pick, f.attackers![0]]).toEqual([r.by === null ? 'none' : 'rules', r.byNat ?? r.couldNat[0]]);
    }
  });

  it('reads the convoys the real rules record over real months, a history campaign and a grand one: who raided is settled, and the raiders\' opening boats are no fewer than the boats lost (round 3, finding 4)', () => {
    for (const [grand, g] of [[false, createGame(5, 'UK')], [true, startOpen(createGame(3, 'UK'))]] as const) {
      expect((g.flags as { open?: number }).open === 1).toBe(grand);
      let read = 0, lost = 0;
      subScene.on = true;
      try {
        for (let i = 0; i < 3; i++) {
          const mark = g.headlines.length;
          runTurnAI(g);
          const convoys = factsOf(g, NAVY.convoys, mark).facts.filter((f) => f.kind === 'convoy');
          expect(convoys.length).toBeGreaterThan(0);
          for (const f of convoys) {
            // (built from the record: the record's route, and the raiders it gives; the boats left on the route)
            const r = (g.subLook ?? []).find((l) => l.nat === f.on && l.turn === f.turn)?.convoys.find((c) => c.route === f.route);
            expect([!!r, Array.isArray(f.raiders)]).toEqual([true, true]);
            const all = r!.seas.flatMap((s) => s.raiders);
            expect(all.length).toBeGreaterThan(0);
            for (const x of all) expect(typeof x.heavy).toBe('number');
            expect(f.raidBy?.sure).toBe(true);
            expect(all.reduce((s, x) => s + x.n, 0)).toBeGreaterThanOrEqual(f.boats ?? 0);
            read++; lost += f.boats ?? 0;
          }
        }
      } finally { subScene.on = false; }
      // (three months' convoys read, and boats lost among them, so the bound above is met with something to bound)
      expect(read).toBeGreaterThanOrEqual(3);
      expect(lost).toBeGreaterThan(0);
    }
  });
});
