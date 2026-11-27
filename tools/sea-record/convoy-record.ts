// Recorded convoy nights for the 3D game's sea scene (release 2; ruling 241 D2): the wolf pack and convoy field
// (src/battle/convoy) fought out by the computer on both sides and written in the scene's names, as tools/sea-record's
// fleet actions are (form.md §5, field-side-read.md §3; SeaFeed.cpp ReadField, ReadPiece, ReadEvent). The scene replays
// them ("-TheaterSeaBattle=<key>") so that the field's own nights can be built against, tested in the cloud
// (Tools/sea_show) and photographed on the Mac. Nothing is booked: the campaign never reads them.
//
// A night runs from three to eighteen hours, so a recording is its busiest stretch, not the whole of it: the window of
// 'span' battle seconds holding the most of what the scene must draw (torpedo salvoes and hits, ships going down, boats
// forced up, patterns and ahead-thrown salvoes, deck guns, star shell), from a lead of 'lead' seconds before it. The
// clock is the night's own battle time (field.window says where the window lies in it); the last view of the window
// carries the night's end, so the after card is the whole night's. Positions are moved, not scaled, so that the convoy's
// centre stands at the field's middle at the window's start (field.window.shift says by how much).
//
// The set pieces are the field's practice nights (practice.ts), each fought on 'tries' seeds and kept on the one that
// shows the most.
//
//   npx esbuild tools/sea-record/convoy-record.ts --bundle --platform=node --format=esm --outfile=dist/convoy-record.mjs --log-level=warning
//   node dist/convoy-record.mjs <out folder> [key]

import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import type { FieldEvent, Side, Zone } from '../../src/battle/core/types';
import { CONVOY } from '../../src/battle/convoy/numbers';
import { ConvoySim, mix } from '../../src/battle/convoy/sim';
import { CONVOY_PRACTICE, convoyPracticeTicket } from '../../src/battle/convoy/practice';
import type { ConvoyTicket, ConvoyUnit } from '../../src/battle/convoy/types';

const B = CONVOY.boat, E = CONVOY.escort;

interface SetPiece {
  key: string;
  /** The practice night's title (practice.ts). */
  card: string;
  /** Roughly where on the globe, for the sun and the sea's colour: latitude, longitude. */
  latLon: [number, number];
}

const PIECES: SetPiece[] = [
  // Six boats homed onto a convoy whose four escorts have sonar and nothing else: boats on the surface in the dark.
  { key: 'convoy-first-pack', card: 'The first pack', latLon: [58.0, -22.0] },
  // Fifteen boats against nine escorts with radar, ahead-throwers and group training: hunts, patterns, boats forced up.
  { key: 'convoy-hunters', card: 'The hunters trained', latLon: [57.0, -27.0] },
  // Six boats against twelve escorts with every kit there is.
  { key: 'convoy-hunted', card: 'Hunted', latLon: [52.0, -18.0] },
];

const FIELD = { size: 30000, edge: 5000 };
const ADJ: Record<string, string> = { UK: 'British', GER: 'German', ITA: 'Italian', USA: 'American', JAP: 'Japanese', FRA: 'French', SOV: 'Soviet' };

const r1 = (x: number): number => Math.round(x * 10) / 10;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;

/** What of a piece never changes in a night: sent the first time it shows, not in every view. */
const STILL = ['kind', 'name', 'nat', 'figures0', 'w'] as const;
const STILL_EXTRA = ['cls', 'ships0', 'fleet', 'gunMm', 'column'] as const;

/** One piece in the scene's names, moved by the window's shift. */
function unitOut(u: ConvoyUnit, ox: number, oy: number): Record<string, unknown> {
  const e = u.extra;
  const extra: Record<string, unknown> = { cls: e.cls, ships0: e.ships0, ships: e.ships, sunk: e.sunk, fleet: e.fleet };
  if (e.cls === 'ss') {
    extra.dive = e.dive; extra.keel = e.keel; extra.scope = !!e.scope; extra.tubes = e.tubes; extra.gunMm = e.gunMm;
    extra.heard = e.heard ?? [];
  }
  if (e.gun !== undefined) extra.gun = e.gun;
  if (e.echo) extra.echo = { on: e.echo.on, kind: e.echo.kind, range: Math.round(e.echo.range), bearing: r3(e.echo.bearing), err: r3(e.echo.err) };
  if (e.huntPhase) { extra.huntPhase = e.huntPhase; if (e.huntUntil !== undefined) extra.huntUntil = r1(e.huntUntil); }
  if (e.column !== undefined) extra.column = e.column;
  return {
    id: u.id, side: u.side, kind: u.kind, name: u.name, nat: u.nat, x: r1(u.x - ox), y: r1(u.y - oy), face: r3(u.face), str: r3(u.str), mor: r3(u.mor), state: u.state,
    figures0: u.figures0, figures: u.figures, w: u.w, d: r1(u.d), act: u.act, speed: r1(u.speed), engaged: u.engaged ?? -1, extra,
  };
}

/** An event moved by the window's shift (its place, and an aim or a star shell's burst in its extra). */
function eventOut(e: FieldEvent, ox: number, oy: number): Record<string, unknown> {
  const out: Record<string, unknown> = { ...e, t: r1(e.t) };
  if (typeof e.x === 'number') out.x = r1(e.x - ox);
  if (typeof e.y === 'number') out.y = r1(e.y - oy);
  const x = e.extra as Record<string, unknown> | undefined;
  if (x && typeof x.x === 'number' && typeof x.y === 'number') out.extra = { ...x, x: r1((x.x as number) - ox), y: r1((x.y as number) - oy) };
  return out;
}

/** How much an event shows of what the scene must draw. */
function weightOf(e: FieldEvent): number {
  const x = (e.extra ?? {}) as Record<string, unknown>;
  switch (e.what) {
    case 'sea:sunk': return 5;
    case 'sea:torpedoHit': return 3;
    case 'sea:torpedo': return 2;
    case 'sea:depthCharge': return x.kind === 'ahead' ? 3 : 2;
    case 'sea:depthChargeHit': return 2;
    case 'sea:dive': return x.why === 'forced' ? 5 : 0.2;
    case 'sea:surrender': case 'sea:abandon': return 4;
    case 'sea:gun': return 2;
    case 'sea:gunHit': return 1;
    case 'sea:starshell': return 1.5;
    case 'sea:sonar': return x.lost ? 0.2 : 0.5;
    case 'sea:rescue': return 1;
    default: return 0;
  }
}

interface Night { seed: number; sim: ConvoySim; events: FieldEvent[] }

function ticketOf(P: SetPiece, seed: number): ConvoyTicket {
  const card = CONVOY_PRACTICE.find((c) => c.title === P.card);
  if (!card) throw new Error(`no practice night '${P.card}'`);
  return convoyPracticeTicket(card, seed, null);
}

/**
 * The whole night fought by the computer on both sides, its events kept (the views thrown away): those of pieces near
 * enough the convoy to stand on the field a window centred on it gives (a hunt far astern is not drawn).
 */
function fightOut(P: SetPiece, seed: number): Night {
  const sim = new ConvoySim(ticketOf(P, seed));
  const events: FieldEvent[] = [];
  const near = FIELD.size / 2 - 2500;
  let guard = 0;
  while (!sim.over && guard++ < 100_000) {
    sim.advance();
    const v = sim.view();
    if (!v.events.length) continue;
    const mv = v.units.filter((u) => u.kind === 'mv' && u.state !== 'gone');
    const cx = mv.reduce((a, u) => a + u.x, 0) / Math.max(1, mv.length), cy = mv.reduce((a, u) => a + u.y, 0) / Math.max(1, mv.length);
    const at = new Map(v.units.map((u) => [u.id, u]));
    for (const e of v.events) {
      const u = e.unit !== undefined ? at.get(e.unit) : undefined;
      if (!u || (Math.abs(u.x - cx) <= near && Math.abs(u.y - cy) <= near)) events.push(e);
    }
  }
  return { seed, sim, events };
}

/** The busiest window of 'span' seconds and how much it shows (each kind's count capped, so one long hunt is not all). */
function busiest(events: FieldEvent[], span: number, lead: number): { from: number; to: number; score: number } {
  const marks = events.filter((e) => weightOf(e) > 0);
  let best = { from: 0, to: span, score: -1 };
  for (const m of marks) {
    const from = Math.max(0, m.t - lead), to = from + span;
    const by = new Map<string, number>();
    for (const e of marks) {
      if (e.t < from || e.t > to) continue;
      const k = e.what === 'sea:dive' && (e.extra as { why?: string })?.why === 'forced' ? 'forced' : e.what;
      by.set(k, Math.min(k === 'sea:sonar' ? 6 : 8, (by.get(k) ?? 0) + weightOf(e)));
    }
    // (and a window that shows each of the night's main beats is worth more than one that shows one of them often)
    let score = 0;
    for (const v of by.values()) score += v;
    for (const k of ['sea:torpedo', 'sea:torpedoHit', 'sea:sunk', 'sea:depthCharge', 'forced']) if (by.has(k)) score += 4;
    if (score > best.score) best = { from, to, score };
  }
  return best;
}

function record(P: SetPiece, outDir: string, span = 720, lead = 45, tries = 30): void {
  let best: Night | null = null, win = { from: 0, to: span, score: -Infinity };
  for (let k = 0; k < tries; k++) {
    const seed = 241 + k * 7919;
    const n = fightOut(P, seed), w = busiest(n.events, span, lead);
    if (w.score > win.score) { best = n; win = w; }
  }
  const night = best!;
  // The night again on the same seed (the field is the same night watched or not, tests/convoy-field-r2), its views
  // written within the window.
  const t = ticketOf(P, night.seed);
  const s = new ConvoySim(t);
  const views: Record<string, unknown>[] = [];
  const still = new Map<number, string>();
  const heardWas = new Map<number, string>();
  let ox = 0, oy = 0, zonesBefore = '';
  const thin = (u: ConvoyUnit): Record<string, unknown> => {
    const o = unitOut(u, ox, oy), x = o.extra as Record<string, unknown>;
    const key = JSON.stringify([...STILL.map((k) => o[k]), ...STILL_EXTRA.map((k) => x[k])]);
    if (still.get(u.id) === key) { for (const k of STILL) delete o[k]; for (const k of STILL_EXTRA) delete x[k]; }
    else still.set(u.id, key);
    // (a boat's hunters written only as they change, as the scene's reader keeps them: SeaFeed.cpp ReadPiece)
    if (Array.isArray(x.heard)) { const h = JSON.stringify(x.heard); if (heardWas.get(u.id) === h) delete x.heard; else heardWas.set(u.id, h); }
    return o;
  };
  const counted = (): { att: number; def: number } => {
    const boats = s.units.filter((u) => u.kind === 'ss').length, mv = s.units.filter((u) => u.kind === 'mv').length;
    const ships = s.units.filter((u) => u.kind === 'dd').reduce((n, u) => n + u.extra.ships0, 0);
    const lostB = Object.values(s.counts.ss).reduce((a, b) => a + b, 0), lostE = Object.values(s.counts.dd).reduce((a, b) => a + b, 0);
    return { att: r3(lostB / Math.max(1, boats)), def: r3((s.counts.mv + lostE) / Math.max(1, mv + ships)) };
  };
  // The window's frames as the field shows them, kept whole until it is known which pieces the scene is given.
  interface Frame { v: ReturnType<ConvoySim['view']>; events: FieldEvent[]; lost: { att: number; def: number } }
  const frames: Frame[] = [];
  let guard = 0, pending: FieldEvent[] = [];
  while (!s.over && guard++ < 100_000) {
    s.advance();
    const v = s.view();
    if (s.worked < win.from) continue;
    if (!frames.length) {
      // The convoy's centre at the window's start, put at the field's middle.
      const mv = v.units.filter((u) => u.kind === 'mv' && u.state !== 'gone');
      const cx = mv.reduce((a, u) => a + u.x, 0) / Math.max(1, mv.length), cy = mv.reduce((a, u) => a + u.y, 0) / Math.max(1, mv.length);
      ox = Math.round(cx - FIELD.size / 2); oy = Math.round(cy - FIELD.size / 2);
    }
    pending.push(...v.events);
    frames.push({ v, events: pending, lost: counted() });
    pending = [];
    if (s.worked >= win.to || s.over) break;
  }
  // The pieces the scene is given: every one on the field at some moment of the window. A hunt that has fallen far astern
  // of the convoy (a trained group holds a contact for hours: the convoy steams 13 km an hour on) stands off the sea
  // raised, and is left out with its events; the after card's counts are still the whole night's.
  const given = new Set<number>();
  const onField = (x: number, y: number): boolean => x >= 0 && x <= FIELD.size && y >= 0 && y <= FIELD.size;
  for (const f of frames) for (const u of f.v.units) if (onField(u.x - ox, u.y - oy)) given.add(u.id);
  const names = (e: FieldEvent): number[] => {
    const x = (e.extra ?? {}) as Record<string, unknown>;
    return [e.unit, x.on, x.from, x.by, x.target].filter((id): id is number => typeof id === 'number');
  };
  let dropped = 0;
  for (const f of frames) { const n = f.events.length; f.events = f.events.filter((e) => names(e).every((id) => given.has(id))); dropped += n - f.events.length; }
  for (const f of frames) {
    const v = f.v;
    const zones = v.zones.map((z: Zone) => ({ ...z, x: r1(z.x - ox), y: r1(z.y - oy) }));
    const zNow = JSON.stringify(zones);
    const view: Record<string, unknown> = {
      t: r1(v.t), limit: v.limit, speed: 1, units: v.units.filter((u) => given.has(u.id)).map((u) => thin(u as ConvoyUnit)), balance: { att: r3(v.balance.att), def: r3(v.balance.def) },
      events: f.events.map((e) => eventOut(e, ox, oy)), ai: v.ai, over: null, me: 'def', contact: v.contact ?? null,
      lost: f.lost, broken: null,
    };
    if (zNow !== zonesBefore) { zonesBefore = zNow; view.zones = zones; }
    views.push(view);
  }
  // The rest of the night worked out unwatched, for the after card (the whole night's end).
  while (!s.over && guard++ < 200_000) { s.advance(); s.view(); }
  const over = s.over!;
  views[views.length - 1].over = { winner: over.winner ?? null, level: over.level, lines: over.lines };
  const sideOut = (side: Side) => ({ nat: t[side].nat, adj: ADJ[t[side].nat] ?? t[side].adj, human: false, fleets: t.place.fleets.filter((f) => f.side === side).map((f) => ({ id: f.id, name: f.name, n: f.n })) });
  const L = t.place.light;
  const field = {
    size: FIELD.size, edge: FIELD.edge, zoneName: t.place.seaName, title: t.title, date: t.date, when: t.place.when, lat: P.latLon[0], lon: P.latLon[1],
    att: sideOut('att'), def: sideOut('def'),
    classes: {
      dd: { len: E.len, beam: E.beam, speed: E.top, turn: 0.12, reach: E.gun },
      ss: { len: B.len, beam: B.beam, speed: B.surfaced, turn: 0.1, reach: CONVOY.surface.gunAt },
      mv: { len: 135, beam: 17.5, speed: CONVOY.convoy.speed, turn: 0.05, reach: 0 },
    },
    torp: { travel: Math.round(B.fireAt / CONVOY.torpedo.speed), radius: CONVOY.torpedo.column },
    // The night's light as the field drew it (moves the odds: vis), and the boats' and the charges' own.
    light: { sun: L.sun, moon: L.moon, vis: L.vis },
    night: { hours: L.hours, day: L.day },
    subs: { scope: B.scope, deep: B.deep, crush: B.crush, dived: B.dived },
    dc: { sink: 3, lethal: 6.4 },
    // Each navy's kit by its own research (the scene's kitLook: asdic, radar, ahead-throwers, group training).
    kit: t.place.kit,
    // The sea's wind: a look only (the field has none), hashed from the seed.
    wind: { from: r3(mix(night.seed ^ 0x51ed) * 2 * Math.PI), speed: r1(4 + 9 * mix(night.seed ^ 0x2c1b)) },
    window: { from: views[0].t, to: views[views.length - 1].t, night: s.limit, shift: { x: ox, y: oy } },
  };
  const x = over.extra as { counts?: unknown; tally?: unknown } | undefined;
  const after = { lines: over.lines, level: over.level, winner: over.winner ?? null, seconds: r1(over.seconds ?? s.worked), counts: x?.counts, tally: x?.tally };
  const out = {
    what: "RECORDING: a convoy night fought by the computer on both sides for the 3D scene to replay (tools/sea-record/convoy-record). Not a booked result: the campaign never reads it. The views are the night's busiest stretch (field.window); the last carries the whole night's end. A piece's still fields (kind, name, nat, figures0, w; extra.cls, ships0, fleet, gunMm, column) and a boat's heard come only when they change; zones only where they changed since the view before.",
    recording: true, key: P.key, seed: night.seed, step: CONVOY.tick, field, views, after,
  };
  const file = join(outDir, `${P.key}.json`);
  const text = JSON.stringify(out);
  writeFileSync(file, text);
  const evs = views.flatMap((v) => v.events as { what: string; extra?: Record<string, unknown> }[]);
  const n = (what: string, f?: (e: { extra?: Record<string, unknown> }) => boolean): number => evs.filter((e) => e.what === what && (!f || f(e))).length;
  console.log(`${P.key} (seed ${night.seed}, showing ${win.score.toFixed(1)}): ${views.length} views, ${field.window.from}-${field.window.to} of ${s.limit} s; `
    + `salvoes ${n('sea:torpedo')} (hits ${n('sea:torpedoHit')}), sunk ${n('sea:sunk')}, patterns ${n('sea:depthCharge', (e) => e.extra?.kind === 'pattern')}, ahead ${n('sea:depthCharge', (e) => e.extra?.kind === 'ahead')}, `
    + `forced up ${n('sea:dive', (e) => e.extra?.why === 'forced')}, guns ${n('sea:gun')}, star shell ${n('sea:starshell')}, abandon ${n('sea:abandon')}; ${given.size} pieces, ${dropped} events off the field left out; night ${JSON.stringify(x?.counts)}; ${(text.length / 1e6).toFixed(2)} MB`);
}

const outDir = process.argv[2] ?? 'dist/convoy-record';
mkdirSync(outDir, { recursive: true });
const only = process.argv[3];
for (const P of PIECES) if (!only || P.key === only) record(P, outDir);
