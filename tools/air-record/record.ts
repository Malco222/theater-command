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
//   node dist/air-record.mjs <out folder> [key]    (writes oahu.json, midway-fleet.json, bremen-b17.json, troops-1941.json; or the one key)

import { mkdirSync, writeFileSync } from 'node:fs';
import { loadGame, saveGame } from '../../src/campaign/save';
import { runTurnAI } from '../../src/campaign/turn';
import { join } from 'node:path';
import { createGame, setWar } from '../../src/campaign/game';
import { planRaid, raidTargets } from '../../src/campaign/airwar';
import { syncWings } from '../../src/campaign/carriers';
import { sail, zoneOf } from '../../src/campaign/navy';
import { startOpen } from '../../src/campaign/staff';
import { beginHuman, endHuman, humanUp, stepAI } from '../../src/campaign/turn';
import type { Fleet, Game, ShipClass } from '../../src/campaign/types';
import { SKY } from '../../src/battle/air/book';
import { AirSim } from '../../src/battle/air/sim';
import { deckMix, deckModels, layShips } from '../../src/battle/air/ships';
import { onBoard, STAGES } from '../../src/battle/air/stages';
import { cloudOf, skyWord } from '../../src/battle/air/sky';
import { defenceOf, experience, leanOf, STYLES } from '../../src/battle/air/styles';
import { airTicket, flyKinds, raidGroups, seaGround } from '../../src/battle/air/ticket';
import { bomberGuns, fighterGuns } from '../../src/battle/air/guns';
import { seatsOf } from '../../src/campaign/bombers';
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

export const PIECES: SetPiece[] = [
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

/**
 * A day raid over Germany (Brian, 20 November: B-17s with their escort meeting German fighters as in a real battle, to
 * set beside the modelling tool's B-17 clip). No set piece of ruling 238: a recording for the scene to replay only. The
 * stand-in is the browser campaign's own British day raid on the target (the air battle tests' small escort against
 * many fighters), its raid flown by the Eighth Air Force's B-17s and P-51Bs of the record's month and met by the Fw 190s
 * of the defence. It has no stage: the scene lays the board's own coast and land under it, as for any raid in Europe.
 */
interface DayRaid {
  key: string;
  /** The province the stand-in's raid is planned on. */
  target: string;
  when: { year: number; month: number };
  title: string;
  date: string;
  /** The target's latitude and longitude, for the month's cloud and the scene's sun (place.lat). */
  latLon: [number, number];
  /** The day's own cloud, where the record's weather is known (else the month's table, as cumulus): its kind, base and
   * tops, and how much of the sky it covers. */
  cloud?: { kind: 'deck' | 'cumulus'; base: number; top: number; cover: number };
  /** The record's groups, by the stand-in's order. */
  bombers: string[];
  escort: string[];
  defenders: string[];
  models: Record<string, { ftr: string; bmr: string }>;
}

const RAIDS: DayRaid[] = [
  {
    key: 'bremen-b17', target: 'Bremen', when: { year: 1943, month: 11 }, title: 'The raid on Bremen', date: 'December 1943', latLon: [53.08, 8.8],
    // (13 and 16 December 1943: Bremen bombed through an undercast on the pathfinders' radar; the art director on round
    // 18c, a stratocumulus deck far under the bombers, tops 1.5 to 2 km, the ground seen through its breaks)
    // (its cover the art director's on round 18d, about three quarters, the ground through the breaks: the 3D game's
    // deck at 0.77 is the one she passed; her winter-deck steer, one lid at 1.8 km with the odd cell 100 to 200 m proud
    // of it: the tops the cells', 2 km)
    cloud: { kind: 'deck', base: 900, top: 2000, cover: 0.77 },
    bombers: ['303rd Bomb Group', '91st Bomb Group', '384th Bomb Group'], escort: ['354th Fighter Group', '357th Fighter Group'],
    defenders: ['Jagdgeschwader 1', 'Jagdgeschwader 11'],
    models: { USA: { ftr: 'Mustang', bmr: 'B17' }, GER: { ftr: 'Fw190', bmr: 'He111' } },
  },
];

/**
 * A raid on troops (release 2; Brian, 24 November 2026: "if a bomber group is bombing troops instead of a factory or
 * town and they get intercepted than i want that playable"): no stand-in, the real thing. A free war the computer plays
 * from September 1939 to the month named, then the raid on troops the raider's staff would send (raidGroups) at the
 * enemy's troops where its fighters rise to meet it, with bombers and an escort that reaches the target with fuel to
 * spare: the one with the most enemy fighters in reach, a column on the march first. The switch for the kind is turned on for the recording only.
 */
interface TroopRaid { key: string; nat: string; foe: string; seed: number; turn: number }

const TROOP_RAIDS: TroopRaid[] = [{ key: 'troops-1941', nat: 'GER', foe: 'SOV', seed: 7, turn: 28 }];

const wars = new Map<string, string>();
function troopTicketOf(R: TroopRaid, seed: number): FieldTicket<AirPlace> {
  const at = `${R.seed}:${R.turn}`;
  if (!wars.has(at)) {
    const w = startOpen(createGame(R.seed, null));
    while (w.turn < R.turn) runTurnAI(w);
    wars.set(at, saveGame(w));
  }
  const g = loadGame(wars.get(at)!)!;
  g.flags.airBattles = (g.flags.airBattles ?? 0) + (seed % 997);
  flyKinds(['ground']);
  const offered = raidTargets(g, 'ground', R.nat).filter((x) => g.regions[x.id].units.some((c) => !c.dead && c.nat === R.foe)).map((x) => {
    const ids = raidGroups(g, 'ground', x.id, R.nat);
    return { ids, target: x.id, t: airTicket(g, 'ground', x.id, ids, R.nat).ticket };
  }).filter((x) => x.t && x.t.att.forces.some((f) => f.kind === 'bmr') && x.t.def.forces.length
    // (an escort with the reach to go down on the troops once its bombers have bombed)
    && x.t.att.forces.some((f) => f.kind === 'ftr' && (x.t!.place.starts[f.corps]?.fuel ?? 0) >= SKY.fuel.full));
  const men = (x: (typeof offered)[number]): number => x.t!.def.forces.reduce((a, f) => a + f.men, 0);
  offered.sort((a, b) => (a.t!.place.marks?.posture === 'march' ? 0 : 1) - (b.t!.place.marks?.posture === 'march' ? 0 : 1) || men(b) - men(a) || a.target - b.target);
  flyKinds([]);
  if (!offered.length) throw new Error(`no intercepted raid on ${R.foe} troops for ${R.nat}`);
  return offered[0].t!;
}

/** The stand-in day raid's ticket, turned into the record's. */
function raidTicketOf(R: DayRaid, seed: number): FieldTicket<AirPlace> {
  const g = startOpen(createGame(7, 'UK'));
  setWar(g, 'UK', 'GER');
  g.seed = seed; g.flags.airBattles = 1;
  const at = raidTargets(g, 'bomb', 'UK').find((x) => planRaid(g, 'bomb', x.id, undefined, 'UK').place === R.target);
  if (!at) throw new Error(`no day raid on ${R.target}`);
  const day = planRaid(g, 'bomb', at.id, undefined, 'UK');
  const ids = [...day.picked, ...day.seats.filter((x) => !x.why && !x.picked && x.kind === 'ftr').map((x) => x.id)];
  const { ticket, why } = airTicket(g, 'bomb', at.id, ids, 'UK');
  if (!ticket) throw new Error(why ?? 'no ticket');
  const t = ticket, place = t.place;
  let b = 0, f = 0, d = 0;
  t.att = { ...t.att, nat: USA.nat, adj: USA.adj, color: USA.color };
  for (const x of t.att.forces) { x.nat = USA.nat; x.name = x.kind === 'bmr' ? R.bombers[b++ % R.bombers.length] : R.escort[f++ % R.escort.length]; }
  t.def = { ...t.def, nat: 'GER', adj: 'German' };
  for (const x of t.def.forces) { x.nat = 'GER'; x.name = R.defenders[d++ % R.defenders.length]; }
  place.nat = USA.nat;
  place.when = R.when;
  place.models = R.models;
  place.guns = Object.fromEntries(Object.entries(place.models).map(([n, k]) => [n, { ftr: fighterGuns(k.ftr, n, R.when), bmr: bomberGuns(k.bmr, n, R.when) }]));
  place.cloud = { ...cloudOf(...mapPointOf(...R.latLon), R.when, t.seed), kind: 'cumulus', ...(R.cloud ?? {}) };
  place.cloud.sky = skyWord(place.cloud.cover);
  // (the place's latitude, for the scene's sun over a raid with no stage: AirLand::SunAt)
  Object.assign(place, { lat: R.latLon[0] });
  // Its bombers one by one, as an American group's (campaign/bombers.ts: names, nose art, missions and kills), so the
  // battle can say which bomber falls and whose gunners brought a fighter down (the air thread, 21 November 04:12Z:
  // a gunner's kill to film). The stand-in's groups are Britain's; each is mustered again as an American group flying from land, its record kept.
  const bombers: NonNullable<AirPlace['bombers']> = {};
  for (const x of t.att.forces) {
    const f = x.kind === 'bmr' ? g.airFleets.find((a) => a.id === x.corps) : undefined;
    const seats = f ? seatsOf(g, { ...f, nat: USA.nat, bombers: undefined, ship: undefined, ashore: undefined }, x.men, t.seed) : null;
    if (seats) bombers[x.corps] = seats;
  }
  if (Object.keys(bombers).length) place.bombers = bombers;
  else delete place.bombers;
  t.title = R.title;
  t.date = R.date;
  return t;
}

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

/** A stage point (metres east and north of its origin) on the globe: latitude and longitude, near enough for the sky. */
function stageLatLon(origin: [number, number], p: [number, number]): [number, number] {
  const lat = origin[0] + p[1] / 111_320;
  return [lat, origin[1] + p[0] / (111_320 * Math.cos((lat * Math.PI) / 180))];
}

/** The campaign map's point for a latitude and longitude: sky.ts's latLonOf turned round (x = (lon + 11) cos 50 x 40). */
function mapPointOf(lat: number, lon: number): [number, number] {
  return [(lon + 11) * Math.cos((50 * Math.PI) / 180) * 40, (71.4 - lat) * 40];
}

/** The stand-in's ticket, turned into the record's set piece. */
export function ticketOf(P: SetPiece, seed: number): FieldTicket<AirPlace> {
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
  // At sea the carriers' patrols stand over their fleet as the ticket stands any fleet's own fighters (the tactics
  // panel's S1): in the air, cap.ahead out toward the strike, warned by the lookouts; each carrier's a little apart.
  const raid = Object.values(starts), rx = raid.reduce((a, p) => a + p.x, 0) / raid.length - SKY.target.x, ry = raid.reduce((a, p) => a + p.y, 0) / raid.length - SKY.target.y;
  const ux = rx / (Math.hypot(rx, ry) || 1), uy = ry / (Math.hypot(rx, ry) || 1);
  P.defFighters.forEach((d, k) => {
    const corps = 9000 + k;
    def.push({ corps, name: d.name, nat: P.def.nat, kind: 'ftr', divs: 0, men: d.n, str: 1, org: proto?.org ?? 1, xp: proto?.xp ?? 0.3, share: (proto?.share ?? 0.3) / Math.max(1, P.defFighters.length) });
    if (P.kind === 'fleet') {
      const side = (k - (P.defFighters.length - 1) / 2) * 1_500;
      starts[corps] = { x: SKY.target.x + ux * STYLES.cap.ahead - uy * side, y: SKY.target.y + uy * STYLES.cap.ahead + ux * side, band: 1, fuel: SKY.fuel.home, wait: false, over: true };
      return;
    }
    const [x, y] = fields.length ? onBoard(st, fields[k % fields.length].at, SKY.target) : [SKY.target.x + 1500 * Math.sin(k * 2.1), SKY.target.y + 1500 * Math.cos(k * 2.1)];
    starts[corps] = { x, y, band: d.wait ? 0 : 1, fuel: SKY.fuel.home, wait: d.wait };
  });
  t.def = { ...t.def, nat: P.def.nat, adj: P.def.adj, color: P.def.color, forces: def };
  // The record's defence flies its own navy's style (the lean table at stops 1 to 3), not the stand-in's.
  place.defence = defenceOf(leanOf(P.def.nat, { xp: experience(def), warning: false, gci: false, escort: 0, defence: 0 }));

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
  // Open water under the stage, with its true north: the map's north turned as the stage is (AirGround.north; the
  // ticket's own strike at sea lays it the same way).
  place.ground = seaGround(P.turn);
  if (P.forecast) place.forecast = { ...place.forecast, ...P.forecast };
  // The sky: the cloud table's base and tops for the stage's own place and the record's month, drawn from this battle's
  // seed (the stand-in's target would give the North Sea's). The kind is history's, not the table's chance: both days
  // had scattered cumulus and no deck (cloud banked on Oahu's mountains with the harbour open; broken cloud over the
  // Mobile Force), as the art director's ruling of 19 November names them, about three eighths of the sky (the look she
  // passed on round 18c).
  const [lat, lon] = stageLatLon(S.origin, st.aim);
  place.cloud = { ...cloudOf(...mapPointOf(lat, lon), P.when, t.seed), kind: 'cumulus', cover: 0.39, sky: skyWord(0.39) };
  t.title = P.title;
  t.date = P.date;
  return t;
}

const r1 = (x: number): number => Math.round(x * 10) / 10;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;

/** One air unit as the bridge hands it to the 3D game (battle-bridge.ts unitOut, for an air group). */
function unitOut(u: AirUnit): Record<string, unknown> {
  const loop = u.act === 'idle' || u.act === 'down' ? '' : SKY.cues.loops[u.air];
  // (corps: the campaign group, as the bridge's unitOut sends it, so a recording's units name their group too.)
  const extra = { ...(u.extra ?? {}), n: u.n, n0: u.n0, band: u.band, alt: Math.round(u.alt), job: u.job.do, per: u.per, corps: u.corps };
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
function fight(P: SetPiece | DayRaid | TroopRaid, seed: number, step: number): Fought {
  const t = 'byStrike' in P ? ticketOf(P, seed) : 'foe' in P ? troopTicketOf(P, seed) : raidTicketOf(P, seed);
  // The player's seat is the raider's, as in play; the computer flies both sides for the recording.
  t.att.human = true; t.def.human = false;
  const s = new AirSim(t, { show: true });
  s.ai.att = true; s.ai.def = true;
  if ('byStrike' in P) for (const u of s.units) if (u.side === 'att') u.name = u.air === 'ftr' ? P.byStrike.ftr : P.byStrike[(u.strike ?? 'level') as 'torpedo' | 'level' | 'dive'];
  const me: Side = 'att';
  const views: Record<string, unknown>[] = [];
  const named = new Map<number, string>();
  let next = 0, events: unknown[] = [], contact: number | null = null;
  // A raid on troops: each view of the sim carries the pieces changed since the one before, so those of the views not
  // recorded are gathered into the next that is (the latest state of each).
  const marks = new Map<number, unknown>();
  const take = (): void => {
    const v = s.view();
    events.push(...v.events);
    for (const p of v.marks?.pieces ?? []) marks.set(p.id, p);
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
      ...(v.marks ? { marks: { ...v.marks, pieces: [...marks.values()] } } : {}),
    });
    events = [];
    marks.clear();
  };
  take();
  let guard = 0;
  while (!s.over && guard++ < 200_000) { s.step(); take(); }
  return { s, t, views };
}

/** How well a fight shows the record (the recording takes the luckiest of a run of battles, not a changed rule). */
function likeRecord(P: SetPiece | DayRaid | TroopRaid, f: Fought): number {
  // A raid on troops: the bombs and the passes both on the troops, and the hardest fight on the way.
  if ('foe' in P) { const o = f.s.outcome(); return o.dropped > 0 && (o.strafed ?? 0) > 0 ? f.s.units.reduce((a, u) => a + u.n0 - u.n, 0) : -1; }
  // A day raid: the bombs on the target and the hardest fight on the way (every aircraft lost, either side).
  if (!('byStrike' in P)) return f.s.outcome().dropped > 0 ? f.s.units.reduce((a, u) => a + u.n0 - u.n, 0) : -1;
  const last = f.views[f.views.length - 1] as { ships?: { pieces: { cls: string; sunk: unknown; name: string | null }[] } };
  const down = (c: string) => last.ships?.pieces.filter((p) => p.cls === c && p.sunk).length ?? 0;
  const big = f.views.some((v) => (v.events as { what: string; extra?: { big?: boolean } }[]).some((e) => e.what === 'air:ship-sunk' && e.extra?.big));
  return P.kind === 'harbour' ? down('bb') * 10 + (big ? 15 : 0) + down('dd') + down('ca') : down('cv') * 10 + (big ? 5 : 0) + down('bb') + down('ca');
}

function record(P: SetPiece | DayRaid | TroopRaid, outDir: string, step = 1.0, tries = 60): void {
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

// (Run from the command line; a test imports the set pieces without recording them: vitest sets VITEST.)
if (!process.env.VITEST) {
  const outDir = process.argv[2] ?? 'dist/air-record';
  mkdirSync(outDir, { recursive: true });
  const only = process.argv[3];
  for (const P of [...PIECES, ...RAIDS, ...TROOP_RAIDS]) if (!only || P.key === only) record(P, outDir);
}
