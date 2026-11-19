// Recorded air battles for the 3D game's air battle scene, before the battle bridge and the air sim reach the Unreal
// rules bundle (ruling 238: Pearl Harbor and Midway). Each is a set piece fought out by the computer on both sides
// and written as the bridge would hand it to the 3D game, a view every second of battle time: the 3D scene replays it
// ("-TheaterAirBattle=replay:oahu") so that it can be built, run and photographed on the Mac. Nothing is booked.
//
// The campaign of the browser branches has no Pacific yet, so each set piece is the ships tests' stand-in strike (the
// Home Fleet's carriers on the German Battle Fleet) turned into the record's: the raiders and the defenders given the
// record's navies, names and aircraft, the ships laid at the stage's berths by layShips, the defenders rising from the
// stage's fields. The counts are the stand-in's, near enough the record's for pictures; the raid card and the
// campaign never see them.
//
//   npx esbuild tools/air-record/record.ts --bundle --platform=node --format=esm --outfile=dist/air-record.mjs --log-level=warning
//   node dist/air-record.mjs <out folder>          (writes oahu.json, midway-fleet.json)

import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { createGame } from '../../src/campaign/game';
import { syncWings } from '../../src/campaign/carriers';
import { sail, zoneOf } from '../../src/campaign/navy';
import { startOpen } from '../../src/campaign/staff';
import { beginHuman, endHuman, humanUp, stepAI } from '../../src/campaign/turn';
import type { Fleet, Game, ShipClass } from '../../src/campaign/types';
import { SKY } from '../../src/battle/air/book';
import { AirSim } from '../../src/battle/air/sim';
import { deckMix, deckModels, layShips } from '../../src/battle/air/ships';
import { onBoard, STAGES } from '../../src/battle/air/stages';
import { airTicket, raidGroups } from '../../src/battle/air/ticket';
import { bomberGuns, fighterGuns } from '../../src/battle/air/guns';
import type { AirUnit, AirPlace } from '../../src/battle/air/types';
import type { FieldTicket, ForceTicket, Side } from '../../src/battle/core/types';

/** A grand campaign with Britain at the table, played until Britain is at war with Germany (tests/carriers.test.ts). */
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

interface Navy { nat: string; adj: string; color: string }
const JAP: Navy = { nat: 'JAP', adj: 'Japanese', color: '#c8b27a' };
const USA: Navy = { nat: 'USA', adj: 'American', color: '#6b7a4a' };

/** The record's ships of a navy that day, by class (the campaign's classes only). */
type Ships = Record<ShipClass, number>;

interface SetPiece {
  key: string;
  kind: 'harbour' | 'fleet';
  stage: string;
  when: { year: number; month: number };
  /** The stage turned so the raid comes on from the south edge: radians, as the ticket turns the map. */
  turn: number;
  att: Navy;
  def: Navy;
  title: string;
  date: string;
  /** The raiders' groups, by the stand-in's order, renamed. */
  attNames: string[];
  /** The defenders' fighters: names and numbers (the stand-in's fighters, renamed and cut or raised to these). */
  defFighters: { name: string; n: number; wait: boolean }[];
  /** The ships under attack. */
  ships: Ships;
  /** The raiders' boxes and fighters as the record names them, by what they do. */
  byStrike: { ftr: string; torpedo: string; level: string; dive: string };
  /** The fixture's own forecast where the stand-in has no fighters to meet the raid (Midway's air patrol). */
  forecast?: Partial<AirPlace['forecast']>;
}

const PIECES: SetPiece[] = [
  {
    key: 'oahu', kind: 'harbour', stage: 'oahu', when: { year: 1941, month: 11 }, turn: Math.PI,
    att: JAP, def: USA, title: 'The raid on Pearl Harbor', date: 'December 1941',
    attNames: ['Akagi air group', 'Kaga air group', 'Soryu air group', 'Hiryu air group', 'Shokaku air group', 'Zuikaku air group'],
    defFighters: [{ name: '15th Pursuit Group', n: 14, wait: true }, { name: '18th Pursuit Group', n: 10, wait: true }],
    ships: { bb: 8, cv: 0, ca: 4, dd: 12, ss: 2 },
    byStrike: { ftr: 'First Air Fleet fighters', torpedo: 'Akagi and Kaga torpedo bombers', level: 'Soryu and Hiryu level bombers', dive: 'Shokaku and Zuikaku dive bombers' },
  },
  {
    key: 'midway-fleet', kind: 'fleet', stage: 'midway', when: { year: 1942, month: 5 }, turn: 0.7,
    att: USA, def: JAP, title: 'The strike on the Mobile Force', date: 'June 1942',
    attNames: ['Yorktown air group', 'Enterprise air group', 'Hornet air group'],
    defFighters: [{ name: 'Akagi fighter patrol', n: 12, wait: false }, { name: 'Kaga fighter patrol', n: 12, wait: false }, { name: 'Hiryu fighter patrol', n: 12, wait: false }],
    ships: { bb: 2, cv: 4, ca: 2, dd: 12, ss: 0 },
    forecast: { foe: 6, ftr: 4, bmr: 45 },
    byStrike: { ftr: 'Fighting Three and Fighting Six', torpedo: 'Torpedo Three, Six and Eight', level: 'Torpedo Three, Six and Eight', dive: 'Bombing Three and Scouting Six' },
  },
];

/** The stand-in strike's game: the Home Fleet's carriers against the German Battle Fleet in harbour, or at sea. */
function standIn(kind: 'harbour' | 'fleet', carriers: number): Game {
  const g = britain(), home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
  home.ships.cv = carriers;
  if (kind === 'harbour') {
    sail(g, foe, foe.home, 'port');
    const z = g.seas.find((s) => s.coast.includes(foe.base))!;
    home.zone = z.id; home.stance = 'patrol';
  } else {
    for (const f of [home, foe]) { f.zone = zoneOf(g, 'natl'); f.stance = 'patrol'; f.lane = undefined; }
  }
  syncWings(g);
  return g;
}

/** The stand-in's ticket, turned into the record's set piece. */
function ticketOf(P: SetPiece, seed: number): FieldTicket<AirPlace> {
  const g = standIn(P.kind, P.attNames.length);
  g.seed = seed; g.flags.airBattles = 1;
  const foe = named(g, 'Battle Fleet');
  const target = P.kind === 'harbour' ? foe.base : foe.id;
  const ids = raidGroups(g, P.kind, target, 'UK');
  const { ticket, why } = airTicket(g, P.kind, target, ids, 'UK', { ships: true });
  if (!ticket) throw new Error(why ?? 'no ticket');
  const t = ticket, place = t.place, S = STAGES[P.stage];
  const st = { key: P.stage, aim: S.aim[P.kind]!, turn: P.turn };

  // The raiders: the record's navy and air groups.
  t.att = { ...t.att, nat: P.att.nat, adj: P.att.adj, color: P.att.color };
  const groups = [...new Set(t.att.forces.map((f) => f.corps))];
  for (const f of t.att.forces) { f.nat = P.att.nat; f.name = P.attNames[groups.indexOf(f.corps) % P.attNames.length]; }

  // The defenders: the record's fighters, from the stage's fighter fields (or over their own fleet at sea).
  const fields = S.fields.filter((x) => x.kind === 'fighters');
  const proto: ForceTicket | undefined = t.def.forces[0];
  const def: ForceTicket[] = [];
  const starts = { ...place.starts };
  for (const id of t.def.forces.map((f) => f.corps)) delete starts[id];
  P.defFighters.forEach((d, k) => {
    const corps = 9000 + k;
    def.push({ corps, name: d.name, nat: P.def.nat, kind: 'ftr', divs: 0, men: d.n, str: 1, org: proto?.org ?? 1, xp: proto?.xp ?? 0.3, share: (proto?.share ?? 0.3) / Math.max(1, P.defFighters.length) });
    const [x, y] = P.kind === 'harbour' && fields.length ? onBoard(st, fields[k % fields.length].at, SKY.target) : [SKY.target.x + 1500 * Math.sin(k * 2.1), SKY.target.y + 1500 * Math.cos(k * 2.1)];
    starts[corps] = { x, y, band: d.wait ? 0 : 1, fuel: SKY.fuel.home, wait: d.wait };
  });
  t.def = { ...t.def, nat: P.def.nat, adj: P.def.adj, color: P.def.color, forces: def };

  // The ships at the stage's berths or stations, in the record's month: the record's names where the record has them.
  const ships = place.ships!;
  const fleetId = ships.law.fleets[0]?.id ?? 1;
  ships.law = { ...ships.law, harbour: P.kind === 'harbour', have: { ...P.ships }, surprise: P.kind === 'harbour', fleets: [{ id: fleetId, nat: P.def.nat, name: P.kind === 'harbour' ? 'Pacific Fleet' : 'Mobile Force', ships: { ...P.ships } }] };
  ships.stage = st;
  const nations = { nations: { [P.att.nat]: { adj: P.att.adj }, [P.def.nat]: { adj: P.def.adj } } } as unknown as Game;
  ships.pieces = layShips(nations, ships.law, P.kind, st, SKY.target, P.when);
  const md = deckModels(P.att.nat, P.when);
  ships.wings = groups.map((corps) => ({ corps, ftr: md.ftr, parts: deckMix(P.att.nat, P.when, P.kind).map(([strike, share]) => ({ strike, share, model: md[strike], band: (strike === 'dive' ? 2 : 1) as 0 | 1 | 2 })) }));
  ships.surprise = P.kind === 'harbour';

  // The place: the record's date, navies, aircraft and guns; the raid from the sea.
  place.nat = P.att.nat;
  place.when = P.when;
  place.from = 'sea';
  place.starts = starts;
  place.name = P.kind === 'harbour' ? 'Pearl Harbor' : 'Midway';
  place.models = {
    [P.att.nat]: { ftr: md.ftr, bmr: md.dive },
    [P.def.nat]: { ftr: P.def.nat === 'USA' ? 'Warhawk' : 'Zero', bmr: P.def.nat === 'USA' ? 'B17' : 'G4M' },
  };
  place.guns = Object.fromEntries(Object.entries(place.models).map(([n, k]) => [n, { ftr: fighterGuns(k.ftr, n, P.when), bmr: bomberGuns(k.bmr, n, P.when) }]));
  place.ground = { land: [], rivers: [], borders: [], target: [], terrain: 'sea', inland: false };
  if (P.forecast) place.forecast = { ...place.forecast, ...P.forecast };
  t.title = P.title;
  t.date = P.date;
  return t;
}

const r1 = (x: number): number => Math.round(x * 10) / 10;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;

/** One air unit as the bridge hands it to the 3D game (battle-bridge.ts unitOut, for an air group). */
function unitOut(u: AirUnit): Record<string, unknown> {
  const loop = u.act === 'idle' || u.act === 'down' ? '' : SKY.cues.loops[u.air];
  const extra = { ...(u.extra ?? {}), n: u.n, n0: u.n0, band: u.band, alt: Math.round(u.alt), job: u.job.do, per: u.per };
  return {
    id: u.id, side: u.side, kind: u.kind, name: u.name, x: r1(u.x), y: r1(u.y), face: r3(u.face), str: r3(u.str), mor: r3(u.mor), state: u.state,
    figures0: u.figures0, figures: u.figures, w: u.w, d: u.d, act: u.act, speed: r1(u.speed), engaged: u.engaged ?? -1, cover: u.cover ?? '', loop, order: null, extra,
  };
}

/** What the air sim offers the screens beyond the shared view (the bridge diff for land: the top bar and the aim slip). */
function skyOut(s: AirSim, me: Side, called: Record<number, string>): Record<string, unknown> {
  const near = (side: Side) => { const n = s.nearWhy(side); return { at: r3(n.at), why: n.why }; };
  const sc = s.score();
  return {
    chance: r3(s.staffChance()), fired: s.fired, winding: s.winding(), near: { att: near('att'), def: near('def') },
    score: { lost: sc.lost, worth: Math.round(sc.worth), cut: sc.cut, ruined: sc.ruined, ratio: r3(sc.ratio), up: sc.up },
    called, me,
  };
}

interface Fought { s: AirSim; t: FieldTicket<AirPlace>; views: Record<string, unknown>[] }

/** One set piece fought out by the computer on both sides, recorded a view every `step` battle seconds. */
function fight(P: SetPiece, seed: number, step: number): Fought {
  const t = ticketOf(P, seed);
  // The player's seat is the raider's, as in play; the computer flies both sides for the recording.
  t.att.human = true; t.def.human = false;
  const s = new AirSim(t, { show: true });
  s.ai.att = true; s.ai.def = true;
  for (const u of s.units) if (u.side === 'att') u.name = u.air === 'ftr' ? P.byStrike.ftr : P.byStrike[(u.strike ?? 'level') as 'torpedo' | 'level' | 'dive'];
  const me: Side = 'att';
  const views: Record<string, unknown>[] = [];
  const named = new Map<number, string>();
  let next = 0, events: unknown[] = [], contact: number | null = null;
  const take = (): void => {
    const v = s.view();
    events.push(...v.events);
    for (const e of v.events) if (e.what === 'contact' && contact === null) contact = e.t;
    if (s.t + 1e-9 < next && !s.over) return;
    next += step;
    // The enemy named with its country word wherever the field names it: sent when a unit first shows or its name changes.
    const called: Record<number, string> = {};
    for (const u of v.units) { const c = s.called(u as AirUnit); if (named.get(u.id) !== c) { named.set(u.id, c); called[u.id] = c; } }
    const sky = skyOut(s, me, called);
    views.push({
      t: r1(v.t), limit: v.limit, speed: 1, units: v.units.map((u) => unitOut(u as AirUnit)), balance: { att: r3(v.balance.att), def: r3(v.balance.def) }, events, zones: v.zones, ai: v.ai, over: v.over, me,
      // (AirSim.view() fills these on the air branch since 2f3a4923, in land's shape; the bridge's viewOut passes them.)
      breaking: { att: r3(s.nearBreak('att')), def: r3(s.nearBreak('def')) }, contact,
      ...((v as { ships?: unknown }).ships ? { ships: (v as { ships?: unknown }).ships } : {}), sky,
    });
    events = [];
  };
  take();
  let guard = 0;
  while (!s.over && guard++ < 200_000) { s.step(); take(); }
  return { s, t, views };
}

/** How well a fight shows the record (the recording takes the luckiest of a run of battles, not a changed rule). */
function likeRecord(P: SetPiece, f: Fought): number {
  const last = f.views[f.views.length - 1] as { ships?: { pieces: { cls: string; sunk: unknown; name: string | null }[] } };
  const down = (c: string) => last.ships?.pieces.filter((p) => p.cls === c && p.sunk).length ?? 0;
  const big = f.views.some((v) => (v.events as { what: string; extra?: { big?: boolean } }[]).some((e) => e.what === 'air:ship-sunk' && e.extra?.big));
  return P.kind === 'harbour' ? down('bb') * 10 + (big ? 15 : 0) + down('dd') + down('ca') : down('cv') * 10 + (big ? 5 : 0) + down('bb') + down('ca');
}

function record(P: SetPiece, outDir: string, step = 1.0, tries = 60): void {
  let best: Fought | null = null, bestScore = -1, bestSeed = 0;
  for (let k = 0; k < tries; k++) {
    const seed = 238 + k * 7919;
    const f = fight(P, seed, step), sc = likeRecord(P, f);
    if (sc > bestScore) { best = f; bestScore = sc; bestSeed = seed; }
  }
  const { s, t, views } = best!;
  const over = s.over!;
  const lost = (side: Side, k: 'ftr' | 'bmr') => s.units.filter((u) => u.side === side && u.air === k).reduce((a, u) => a + u.n0 - u.n, 0);
  const groups = s.units.map((u) => ({ name: s.called(u), side: u.side, kind: u.air, flew: u.n0, lost: u.n0 - u.n }));
  // The after card as the replay shows it: the battle's own lines (the campaign writes the real card from what it books).
  const after = { lines: over.lines, level: over.level, reckoning: '', groups: groups.map((x) => ({ ...x, words: `lost ${x.lost} of the ${x.flew} that flew` })), lost: { att: { ftr: lost('att', 'ftr'), bmr: lost('att', 'bmr') }, def: { ftr: lost('def', 'ftr'), bmr: lost('def', 'bmr') } } };
  const sky = { w: SKY.w, h: SKY.h, scale: SKY.scale, target: SKY.target, flakR: SKY.flakR, bands: SKY.bands.alt, place: t.place, title: t.title, date: t.date, att: { nat: t.att.nat, adj: t.att.adj, human: t.att.human }, def: { nat: t.def.nat, adj: t.def.adj, human: t.def.human } };
  // (each view carries the ships only where they changed since the view before: the scene keeps the last it was given)
  let shipsBefore = '';
  const thin = views.map((v) => {
    const now = JSON.stringify(v.ships ?? null);
    if (now !== shipsBefore) { shipsBefore = now; return v; }
    const { ships: _same, ...rest } = v;
    return rest;
  });
  const out = { what: 'RECORDING: an air battle fought by the computer on both sides for the 3D scene to replay (tools/air-record). Not a booked result: the campaign never reads it. Each view carries the ships only where they changed since the view before.', recording: true, key: P.key, seed: bestSeed, step, sky, views: thin, after };
  const file = join(outDir, `${P.key}.json`);
  writeFileSync(file, JSON.stringify(out));
  const last = views[views.length - 1] as { ships?: { pieces: { cls: string; sunk: unknown; name: string | null; how?: string }[] } };
  const sunk = last.ships?.pieces.filter((p) => p.sunk).map((p) => `${p.cls}${p.name ? ' ' + p.name : ''}`) ?? [];
  console.log(`${P.key} (seed ${bestSeed}, like the record ${bestScore}): ${views.length} views, ${over.seconds.toFixed(0)} battle s, ${over.level}, lost att ${JSON.stringify(after.lost.att)} def ${JSON.stringify(after.lost.def)}, down: ${sunk.join(', ')}; ${(JSON.stringify(out).length / 1e6).toFixed(2)} MB`);
}

const outDir = process.argv[2] ?? 'dist/air-record';
mkdirSync(outDir, { recursive: true });
for (const P of PIECES) record(P, outDir);
