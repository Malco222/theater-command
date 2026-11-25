// Recorded fleet actions for the 3D game's sea battle scene, before the battle bridge's sea calls reach the Unreal
// rules bundle (the ship behaviour plan's B0: the sea calls in battle-bridge.ts are written but nothing calls them).
// Each is a fleet action fought out by the computer on both sides and written as the bridge would hand it to the 3D
// game (TC.seaStart's field, then each TC.battleView): a view every `step` battle seconds, with every event between.
// The 3D scene replays it ("-TheaterSeaBattle=two-lines") so that it can be built, tested in the cloud (Tools/sea_show)
// and photographed on the Mac. Nothing is booked: the campaign never reads it.
//
// The set pieces are fleets the field already fights: the practice actions' (practice.ts) and one Pacific action given
// the record's navies. Their counts are the field's, near enough the record's for pictures.
//
//   npx esbuild tools/sea-record/record.ts --bundle --platform=node --format=esm --outfile=dist/sea-record.mjs --log-level=warning
//   node dist/sea-record.mjs <out folder> [key]    (writes two-lines.json, light-ships.json, samar.json, and with men in
//                                                    the water two-lines-sailors.json and samar-sailors.json; or the one key)

import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import type { FieldEvent, Side, Zone } from '../../src/battle/core/types';
import { SEA, waterOf } from '../../src/battle/sea/numbers';
import { SeaSim } from '../../src/battle/sea/sim';
import { plainTicket, type PlainFleet } from '../../src/battle/sea/ticket';
import { seaLoop } from '../../src/battle/sea/cues';
import { PRACTICE } from '../../src/battle/sea/practice';
import type { SeaTicket, SeaUnit } from '../../src/battle/sea/types';

interface Navy { nat: string; adj: string }
const NAVY: Record<string, Navy> = {
  UK: { nat: 'UK', adj: 'British' }, GER: { nat: 'GER', adj: 'German' }, ITA: { nat: 'ITA', adj: 'Italian' },
  USA: { nat: 'USA', adj: 'American' }, JAP: { nat: 'JAP', adj: 'Japanese' },
};

interface SetPiece {
  key: string;
  title: string;
  /** The month the scene dresses the ships and the sky for (the record's, or the practice card's era). */
  when: { year: number; month: number };
  date: string;
  sea: string;
  /** Roughly where on the globe, for the sun and the sea's colour: latitude, longitude. */
  latLon: [number, number];
  att: PlainFleet[];
  def: PlainFleet[];
  air?: Partial<Record<Side, number>>;
  /**
   * Men in the water (release 2's sailor rules): on, in the water of 'sea' (the campaign's sea id) in the piece's month.
   * 'early': a side whose fleets stand at "Abandon early"; 'rescue': a side whose last destroyer piece afloat (not the
   * wreck's own) the recording sends, as the player would, to the first of its side's wrecks. The staff's calls
   * otherwise, as in every other recording.
   */
  sailors?: { sea: string; early?: Side; rescue?: Side };
}

const practice = (name: string) => PRACTICE.find((p) => p.name === name)!;

const PIECES: SetPiece[] = [
  {
    // The first practice card as it stands: two battle lines, the Home Fleet's three battleships against the German
    // fleet's two, cruisers and destroyers on both sides (practice.ts), in the North Sea.
    key: 'two-lines', title: 'Two battle lines', when: { year: 1941, month: 4 }, date: 'Practice', sea: practice('Two battle lines').sea, latLon: [57.5, 3.5],
    att: practice('Two battle lines').att, def: practice('Two battle lines').def,
  },
  {
    // The fifth practice card: Force H's cruisers and destroyers against three Italian battleships (practice.ts), the
    // make-up where the destroyers make their torpedo runs and light ships break off and turn for home under fire.
    key: 'light-ships', title: 'Light ships against battleships', when: { year: 1941, month: 6 }, date: 'Practice', sea: practice('Light ships against battleships').sea, latLon: [37.0, 15.5],
    att: practice('Light ships against battleships').att, def: practice('Light ships against battleships').def,
  },
  {
    // The action off Samar, 25 October 1944: Kurita's Centre Force (four battleships, six heavy and two light cruisers,
    // eleven destroyers) came on Taffy 3 (six escort carriers, three destroyers and four destroyer escorts). The light
    // cruisers sail as cruisers and the escorts as destroyers, as the field counts ships. An escort carrier's deck is
    // worth about a third of a fleet carrier's (a quarter of the aircraft, slow, unarmoured): deck 0.35.
    key: 'samar', title: 'The action off Samar', when: { year: 1944, month: 9 }, date: 'October 1944', sea: 'Philippine Sea', latLon: [11.7, 126.2],
    att: [{ name: 'Centre Force', nat: 'JAP', ships: { bb: 4, ca: 8, dd: 11 }, admiral: 2 }],
    def: [{ name: 'Taffy 3', nat: 'USA', ships: { cv: 6, dd: 7 }, deck: 0.35, bombers: 6 * 12, admiral: 3 }],
  },
];
// The same two actions with men in the water, for the scene's sailors (sea:men, sea:oil, sea:abandon, sea:rescue, and the
// after-action counts): the North Sea in May is cold, the Philippine Sea in October warm. In 'two-lines-sailors' the
// British stand at "Abandon early" and send destroyers to their first wreck; off Samar the Japanese men refuse an
// American pick-up.
PIECES.push(
  { ...PIECES[0], key: 'two-lines-sailors', sailors: { sea: 'nsea', early: 'att', rescue: 'att' } },
  { ...PIECES[2], key: 'samar-sailors', sailors: { sea: 'philsea' } },
);

const r1 = (x: number): number => Math.round(x * 10) / 10;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;

/**
 * One piece as the bridge hands it to the 3D game (battle-bridge.ts unitOut, for a sea piece), with what is always the
 * same for a fleet action left out: no order (the computer's own), no cover, no drops. The scene's reader takes a
 * missing field as its default, so the live feed and a recording read alike.
 */
/** What of a piece never changes in a fight: sent the first time it shows (and if it ever changes), not in every view. */
const STILL = ['kind', 'name', 'nat', 'figures0', 'w'] as const;
const STILL_EXTRA = ['cls', 'ships0', 'fleet'] as const;

function unitOut(u: SeaUnit): Record<string, unknown> {
  const e = u.extra;
  const extra: Record<string, unknown> = { cls: e.cls, ships0: e.ships0, ships: e.ships, sunk: e.sunk, way: r1(e.way), speed: e.speed, target: e.target, fleet: e.fleet };
  if (e.planes !== undefined) extra.planes = r3(e.planes);
  if (e.screen) extra.screen = true;
  if (e.run !== undefined) extra.run = e.run;
  if (e.burnt) extra.burnt = e.burnt;
  // The look fields (the ship plan's B1): passed as the bridge passes extra, whole.
  for (const k of ['foe', 'helm', 'lineAt', 'runPhase', 'runUntil', 'combFrom', 'combUntil', 'heldFor', 'heldForName', 'clearFrom', 'clearFromName', 'clearUntil', 'ordered', 'smoke', 'flight', 'cover'] as const) if (e[k] !== undefined) extra[k] = typeof e[k] === 'number' ? r3(e[k] as number) : e[k];
  const out: Record<string, unknown> = {
    id: u.id, side: u.side, kind: u.kind, name: u.name, nat: u.nat, x: r1(u.x), y: r1(u.y), face: r3(u.face), str: r3(u.str), mor: r3(u.mor), state: u.state,
    figures0: u.figures0, figures: u.figures, w: u.w, d: r1(u.d), act: u.act, speed: r1(u.speed), engaged: u.engaged ?? -1, loop: seaLoop(e.cls, u.act), extra,
  };
  const strike = u.abilities?.find((a) => a.key === 'strike');
  if (strike) out.abilities = [{ key: 'strike', ready: r1(strike.ready), winding: strike.winding }];
  return out;
}

function ticketOf(P: SetPiece, seed: number): SeaTicket {
  const water = P.sailors ? waterOf(P.sailors.sea, P.when.month) : null;
  if (P.sailors && !water) throw new Error(`no water band for ${P.sailors.sea}`);
  const early = P.sailors?.early ? P.att.concat(P.def).map((_, i) => i + 1).filter((id) => (id <= P.att.length ? 'att' : 'def') === P.sailors!.early) : undefined;
  // With release 2's own water on (Brian, 25 November 12:52Z: captains mind their own side's torpedoes; the release 2
  // assembler ships the switch on): set SEA_OWN_WATER=0 for the field as it was before it.
  const t = plainTicket({ att: P.att, def: P.def, air: P.air, seed, human: null, zoneName: P.sea, title: P.title, ...(water ? { sailors: { water, ...(early ? { early } : {}) } } : {}), ownWater: process.env.SEA_OWN_WATER !== '0' });
  const a = NAVY[t.att.nat] ?? { nat: t.att.nat, adj: t.att.nat }, d = NAVY[t.def.nat] ?? { nat: t.def.nat, adj: t.def.nat };
  t.att = { ...t.att, adj: a.adj };
  t.def = { ...t.def, adj: d.adj };
  t.date = P.date;
  return t;
}

interface Fought { s: SeaSim; t: SeaTicket; views: Record<string, unknown>[]; events: FieldEvent[] }

/** One action fought out by the computer on both sides, recorded a view every `step` battle seconds. */
function fight(P: SetPiece, seed: number, step: number): Fought {
  const t = ticketOf(P, seed);
  const s = new SeaSim(t);
  s.ai.att = true; s.ai.def = true;
  const views: Record<string, unknown>[] = [];
  const all: FieldEvent[] = [];
  let next = 0, events: FieldEvent[] = [], zonesBefore = '';
  const still = new Map<number, string>();
  /** A piece as the view carries it: its still fields only where they are new. */
  const thin = (u: SeaUnit): Record<string, unknown> => {
    const o = unitOut(u), x = o.extra as Record<string, unknown>;
    const key = JSON.stringify([...STILL.map((k) => o[k]), ...STILL_EXTRA.map((k) => x[k])]);
    if (still.get(u.id) === key) { for (const k of STILL) delete o[k]; for (const k of STILL_EXTRA) delete x[k]; }
    else still.set(u.id, key);
    return o;
  };
  // The player's own order, scripted (SetPiece.sailors.rescue): at the side's first wreck, its last destroyer piece afloat
  // goes to pick men up, from the next second.
  const by = P.sailors?.rescue;
  let sent = false;
  const take = (): void => {
    const v = s.view();
    events.push(...v.events);
    all.push(...v.events);
    const m = by && !sent ? v.events.find((e) => e.what === 'sea:men' && e.side === by) : undefined;
    if (m && by) {
      const dd = s.units.filter((u) => u.side === by && u.extra.cls === 'dd' && u.state === 'steady' && u.id !== m.unit).at(-1);
      if (dd) sent = s.command({ do: 'sea:rescue', units: [dd.id], wreck: { unit: m.unit!, ship: (m.extra as { ship: number }).ship } }, by);
    }
    if (s.t + 1e-9 < next && !s.over) return;
    next += step;
    // Zones only where they changed since the view before (most views carry none): the scene keeps the last it was given.
    const zones = v.zones.map((z: Zone) => ({ ...z }));
    const zNow = JSON.stringify(zones);
    const view: Record<string, unknown> = {
      t: r1(v.t), limit: v.limit, speed: 1, units: v.units.map((u) => thin(u as SeaUnit)), balance: { att: r3(v.balance.att), def: r3(v.balance.def) },
      events: events.map((e) => ({ ...e, t: r1(e.t) })), ai: v.ai, over: v.over, me: 'att',
      breaking: { att: r3(v.breaking?.att ?? 0), def: r3(v.breaking?.def ?? 0) }, contact: v.contact ?? null,
      // The tug-of-war bar's inputs (release 2): each side's share of its weight lost as the result counts it
      // (SeaSim.lostShare: ships out of the fight afloat, hull owed and salvoes unpaid included, which balance leaves
      // out), and the side that broke off, null before. At the limit the leader is the attacker while
      // (lost.def + 0.05) / (lost.att + 0.05) is 1 or more; once a side breaks the other has won. Look only.
      lost: { att: r3(s.lostShare('att')), def: r3(s.lostShare('def')) }, broken: s.brokenSide,
    };
    if (zNow !== zonesBefore) { zonesBefore = zNow; view.zones = zones; }
    views.push(view);
    events = [];
  };
  take();
  let guard = 0;
  while (!s.over && guard++ < 200_000) { s.step(); take(); }
  return { s, t, views, events: all };
}

/** How much a fight shows of what the scene must draw: ships sunk and burning, torpedo runs and hits, strikes, a chase. */
function showing(f: Fought): number {
  const n = (what: string): number => f.events.filter((e) => e.what === what).length;
  const seconds = f.s.t;
  // (a torpedo salvo counts by how full its bank was: the scene draws that share of its tracks, none at an empty bank)
  const tracks = f.events.filter((e) => e.what === 'sea:torpedo' && Number((e.extra as { share?: number } | undefined)?.share ?? 1) > 0).length;
  return n('sea:sunk') * 4 + Math.min(3, n('sea:burning')) * 3 + Math.min(6, tracks) * 2 + Math.min(4, n('sea:torpedoHit')) * 2
    + Math.min(4, n('sea:strike')) * 2 + (n('routing') > 0 ? 2 : 0) - Math.max(0, seconds - 1100) / 100;
}

function record(P: SetPiece, outDir: string, step = 2.0, tries = 60): void {
  let best: Fought | null = null, bestScore = -Infinity, bestSeed = 0;
  for (let k = 0; k < tries; k++) {
    const seed = 239 + k * 7919;
    const f = fight(P, seed, step), sc = showing(f);
    if (sc > bestScore) { best = f; bestScore = sc; bestSeed = seed; }
  }
  const { s, t, views, events } = best!;
  const over = s.over!;
  const sideOut = (side: Side) => ({ nat: t[side].nat, adj: t[side].adj, human: t[side].human, fleets: t.place.fleets.filter((f) => f.side === side).map((f) => ({ id: f.id, name: f.name, ships: f.ships })) });
  const field = {
    size: SEA.field.size, edge: SEA.field.edge, zoneName: t.place.zoneName,
    // (what TC.seaStart's field leaves to the ticket: the scene's title, date and navies; and the look's own inputs)
    title: t.title, date: t.date, when: P.when, lat: P.latLon[0], lon: P.latLon[1], att: sideOut('att'), def: sideOut('def'),
    classes: Object.fromEntries(Object.entries(SEA.classes).map(([c, k]) => [c, { len: k.len, beam: k.beam, speed: k.speed, turn: k.turn, reach: k.reach }])),
    torp: { travel: SEA.torp.travel, radius: SEA.torp.radius }, strike: { windup: SEA.strike.windup, radius: SEA.strike.radius },
    // The look's wind (L6): the battle's own bearing it blows from and its speed (SeaSim.wind, windSpeed: hashes of the seed).
    wind: { from: s.wind, speed: s.windSpeed },
  };
  // With men in the water: the ships each fleet's own call abandoned, what the side holding the water picked up after the
  // fight (for the closing shot), and each side's men saved, lost and taken prisoner, with the calls made.
  const x = over.extra as { abandoned?: unknown; rescue?: unknown; sailors?: unknown } | undefined;
  const after = { lines: over.lines, level: over.level, winner: over.winner ?? null, seconds: r1(s.t), ...(x?.sailors ? { abandoned: x.abandoned, rescue: x.rescue, sailors: x.sailors } : {}) };
  const out = { what: 'RECORDING: a fleet action fought by the computer on both sides for the 3D scene to replay (tools/sea-record). Not a booked result: the campaign never reads it. Each view carries the zones only where they changed since the view before, and a piece\'s still fields (kind, name, nat, figures0, w; extra.cls, ships0, fleet) only the first time it shows.', recording: true, key: P.key, seed: bestSeed, step, field, views, after };
  const file = join(outDir, `${P.key}.json`);
  const text = JSON.stringify(out);
  writeFileSync(file, text);
  const n = (what: string): number => events.filter((e) => e.what === what).length;
  console.log(`${P.key} (seed ${bestSeed}, showing ${bestScore.toFixed(1)}): ${views.length} views, ${s.t.toFixed(0)} battle s, ${over.level}; sunk ${n('sea:sunk')}, burning ${n('sea:burning')}, torpedo salvos ${n('sea:torpedo')} (hits ${n('sea:torpedoHit')}), strikes ${n('ability')} (landed ${n('sea:strike')}), coast raids ${n('sea:air')}, routing ${n('routing')}; ${(text.length / 1e6).toFixed(2)} MB`);
}

const outDir = process.argv[2] ?? 'dist/sea-record';
mkdirSync(outDir, { recursive: true });
const only = process.argv[3];
for (const P of PIECES) if (!only || P.key === only) record(P, outDir);
