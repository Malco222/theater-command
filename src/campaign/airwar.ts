// The air war as its own game (the grand campaign): what both interfaces read, and the orders they give.
//
// There are three kinds of piece. An airfield is a place on the map with a size: aircraft take off from it and
// come back to it, engineers lay new ones out, and the treasury enlarges them. An air group is fighters or
// bombers, never both, based on one airfield. A raid is planned from its target: the planner says which groups
// could fly it, what will come up to meet it, the chance of success and the losses to expect, and what flying
// it together from several airfields is worth. It is flown the moment the order is given, and each group flies
// one raid a month.
//
// Between raids a fighter group patrols (it meets any raid within its reach and supports the armies), sticks
// with one bomber group as its escort, or rests. A bomber group supports the armies or rests. Any group can
// also send a few aircraft to photograph a province once a month.
//
// airfields() draws the map; airfieldCard() opens one airfield; groupJobs() and setGroupJob() are a group's
// standing orders; raidKinds(), raidTargets() and planRaid() are the planner; launchRaid() flies it; scout()
// is a reconnaissance flight; groupBases() says where a group could move. The engine is air.ts.

import { Rng, hashString } from '../core/rng';
import type { TicketLine } from '../core/ticket';
import type { AirFleet, AirMission, AirReport, Game } from './types';
import {
  AIR, AIRWAR, afloat, airFleetsOf, airPool, airRange, airWar, airfieldsHold, assessAir, carrierOf, carrierRules, cover, crowding,
  fightersOver, fly, forecastAir, groupAt, gunsAt, homeOf, isOffensive, kmTo, kmToSea, setMission, shipsInPort, whyNotBase, whyNotMission,
} from './air';
import { fleetKnown } from './carriers';
import { allied, atWar, headline } from './game';
import { SIGHT, corpsKnown, sightOf } from './intel';
import { CLASSES, NAVY, atSea, shipList, surfaceShips } from './navy';
import { onNet, railCut, railsAt } from './rails';

export type RaidKind = 'bomb' | 'night' | 'ground' | 'superiority' | 'strike' | 'fleet' | 'harbour' | 'recon';

const num = (n: number): string => Math.round(n).toLocaleString('en-US');
const pct = (x: number): string => `${Math.round(x * 100)}%`;
const plural = (n: number, one: string, many = `${one}s`): string => `${num(n)} ${Math.round(n) === 1 ? one : many}`;
const list = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const kindRow = (k: RaidKind) => AIRWAR.raids.kinds.find((x) => x.key === k)!;
/** What a raid goes for, in words. A fleet is named only to someone who has it identified. */
function placeOf(g: Game, kind: RaidKind, target: number, viewer: string | null = null): string {
  if (kind === 'strike') return `the ${g.seas[target]?.name ?? 'sea'}`;
  if (kind === 'fleet') {
    const t = g.fleets.find((x) => x.id === target);
    if (!t) return 'a fleet';
    return fleetKnown(g, t, viewer) === 'exact' ? `the ${t.name}` : `${g.nations[t.nat].adj} ships in the ${g.seas[t.zone].name}`;
  }
  if (kind === 'harbour') return `the harbour of ${g.regions[target]?.name ?? ''}`;
  return g.regions[target]?.name ?? '';
}
/** The kind of raid a group's orders are, for naming what it flew against. */
const kindFlown = (m: AirMission): RaidKind => (m === 'strike' || m === 'fleet' || m === 'harbour' ? m : 'bomb');
/** Where a group flies from, in words: its airfield's province, or the fleet whose carriers it is aboard. */
const homeName = (g: Game, f: AirFleet): string => carrierOf(g, f)?.name ?? g.regions[f.base].name;
/** How far one group is from another's base: nothing where they share an airfield or a fleet. */
function apart(g: Game, a: AirFleet, b: AirFleet): number {
  if (homeOf(a) === homeOf(b)) return 0;
  const c = carrierOf(g, b);
  if (!c) return afloat(b) ? Infinity : kmTo(g, a, b.base);
  return atSea(c) ? kmToSea(g, a, c.zone) : c.base >= 0 ? kmTo(g, a, c.base) : Infinity;
}
/** How far a group has to fly to the target of a raid. */
function distTo(g: Game, f: AirFleet, kind: RaidKind, target: number): number {
  if (kind === 'strike') return kmToSea(g, f, target);
  if (kind === 'fleet') { const t = g.fleets.find((x) => x.id === target); return t ? kmToSea(g, f, t.zone) : Infinity; }
  return kmTo(g, f, target);
}
/** The engine's mission for a kind of raid. A reconnaissance is not a mission: it is flown beside one. */
const missionOf = (kind: RaidKind): AirMission => (kind === 'recon' ? 'support' : kind);
const kindOf = (f: AirFleet): 'ftr' | 'bmr' | 'mixed' => f.kind ?? (f.bmr <= 0 ? 'ftr' : f.ftr <= 0 ? 'bmr' : 'mixed');
const flying = (f: AirFleet): boolean => f.ftr + f.bmr > 0;

// ---- Airfields ----------------------------------------------------------------------------------------

/** The word for an airfield that serves this many aircraft, and its size from 1 (a landing ground) to 4 for a map to draw. */
export function fieldWord(holds: number): { word: string; size: number } {
  const W = AIRWAR.fields.words;
  const i = W.findIndex(([most]) => holds <= most);
  const k = i < 0 ? W.length - 1 : i;
  return { word: W[k][1], size: k + 1 };
}

/** One airfield as a map draws it. groups and aircraft are -1 where the fog of war hides what flies from it. */
export interface AirfieldRow {
  region: number;
  name: string;
  x: number;
  y: number;
  holder: string;
  side: 'own' | 'ally' | 'foe' | 'other';
  /** How many aircraft it serves, the word for a field of that size, and the size from 1 to 4. */
  holds: number;
  word: string;
  size: number;
  groups: number;
  aircraft: number;
  /** More aircraft are based here than the field serves: not all of them can fly. */
  crowded: boolean;
}

function fieldRow(g: Game, id: number, viewer: string | null, sight: Uint8Array): AirfieldRow | null {
  const holds = airfieldsHold(g, id);
  if (holds <= 0) return null;
  const r = g.regions[id];
  const side = viewer === null ? 'other' : r.ctrl === viewer ? 'own' : allied(g, r.ctrl, viewer) ? 'ally' : atWar(g, r.ctrl, viewer) ? 'foe' : 'other';
  const based = g.airFleets.filter((f) => f.base === id && !afloat(f) && flying(f));
  const known = side === 'own' || side === 'ally' || sight[id] === SIGHT.full;
  const aircraft = based.reduce((s, f) => s + f.ftr + f.bmr, 0);
  return {
    region: id, name: r.name, x: r.x, y: r.y, holder: r.ctrl, side, holds, ...fieldWord(holds),
    groups: known ? based.length : -1, aircraft: known ? aircraft : -1, crowded: known && aircraft > holds,
  };
}

/** Every airfield on the map, for a map to draw. Empty outside the grand campaign's air war, where airfields are not places. */
export function airfields(g: Game, viewer: string | null = g.human): AirfieldRow[] {
  if (!airWar(g)) return [];
  const sight = sightOf(g, viewer);
  const out: AirfieldRow[] = [];
  for (const r of g.regions) { const row = fieldRow(g, r.id, viewer, sight); if (row) out.push(row); }
  return out;
}

// ---- Air groups ---------------------------------------------------------------------------------------

/** One air group as a card shows it. */
export interface GroupRow {
  id: number;
  nat: string;
  name: string;
  /** The command it was formed from, or '' where it is that command. */
  command: string;
  kind: 'ftr' | 'bmr' | 'mixed';
  ftr: number;
  bmr: number;
  aircraft: number;
  /** The strength it is kept at. */
  est: number;
  /** 0..1: how fit it is to fly. */
  ready: number;
  leader: string | null;
  level: number;
  base: number;
  baseName: string;
  /** The fleet whose carriers it flies from, or -1 for a group on an airfield; and where it is, for a map: a sea (or -1) and a point. */
  ship: number;
  sea: number;
  x: number;
  y: number;
  /** What it is doing, in words, and as a key: on patrol, escorting, resting, or away on a raid. */
  doing: string;
  job: 'patrol' | 'escort' | 'rest' | 'raid';
  /** The bomber group a fighter group sticks with, or -1. */
  escorts: number;
  /** It has flown its raid, or its reconnaissance, this month. */
  flew: boolean;
  scouted: boolean;
  /** How far it reaches, in kilometres: its fighters or its bombers. */
  range: number;
  /** The share of it that can fly from its airfield: under 1 where the field is crowded. */
  crowd: number;
  mine: boolean;
}

/** The bomber group a fighter group is ordered to stick with, if that group still flies. */
const escorted = (g: Game, f: AirFleet): AirFleet | null => (f.escorts === undefined ? null : g.airFleets.find((x) => x.id === f.escorts && x.nat === f.nat && x.bmr > 0) ?? null);

export function groupRow(g: Game, f: AirFleet, viewer: string | null = g.human): GroupRow {
  const R = airRange(f);
  const kind = kindOf(f);
  const away = f.flew === g.turn && isOffensive(f.mission);
  const with_ = escorted(g, f);
  const place = placeOf(g, kindFlown(f.mission), f.target, viewer);
  const deck = afloat(f), at = groupAt(g, f);
  const doing = away ? `Flew against ${place} this month: back ${deck ? 'aboard' : 'at its airfield'} next month`
    : isOffensive(f.mission) ? `Ordered against ${place}: flies when the month ends`
    : f.mission === 'rest' ? 'Resting and refitting'
    : kind === 'bmr' ? (deck ? 'Ranged on deck, ready for a strike' : 'Supporting the armies, and ready for a raid')
    : with_ ? `On patrol, and sticking with the ${with_.name} as its escort`
    : deck ? 'On patrol over its fleet: meets any raid on the ships in its sea' : 'On patrol: meets any raid within its reach';
  return {
    id: f.id, nat: f.nat, name: f.name, command: f.parent && f.parent !== f.name ? f.parent : '', kind,
    ftr: f.ftr, bmr: f.bmr, aircraft: f.ftr + f.bmr, est: f.est.ftr + f.est.bmr, ready: f.ready,
    leader: f.leader?.name ?? null, level: f.leader?.level ?? 0, base: f.base, baseName: homeName(g, f), ship: f.ship ?? -1, sea: at.sea, x: at.x, y: at.y,
    doing, job: away || isOffensive(f.mission) ? 'raid' : f.mission === 'rest' ? 'rest' : with_ ? 'escort' : 'patrol',
    escorts: with_ ? with_.id : -1, flew: f.flew === g.turn, scouted: f.scouted === g.turn,
    range: kind === 'bmr' ? R.bmr : kind === 'ftr' ? R.ftr : Math.max(R.ftr, R.bmr), crowd: crowding(g, f), mine: f.nat === viewer,
  };
}

/** A nation's air groups, in the order of its airfields. */
export const airGroups = (g: Game, nat: string): GroupRow[] => airFleetsOf(g, nat).filter((f) => flying(f) || f.est.ftr + f.est.bmr > 0).map((f) => groupRow(g, f, nat));

/** One airfield opened: what it is, and the air groups based there as far as the viewer can know. */
export interface AirfieldCard {
  region: number;
  name: string;
  /** Null where there is no airfield. */
  field: AirfieldRow | null;
  /** What it is and what it serves, in a sentence. */
  note: string;
  /** How it can be made, or made larger. */
  build: string;
  /** The fog of war hides what flies from here. */
  hidden: boolean;
  rows: GroupRow[];
}

export function airfieldCard(g: Game, region: number, viewer: string | null = g.human): AirfieldCard | null {
  const r = g.regions[region];
  if (!r || !airWar(g)) return null;
  const field = fieldRow(g, region, viewer, sightOf(g, viewer));
  const E = AIRWAR.fields.engineers, built = g.flags[`wk:field:${region}`] ?? 0;
  const build = built < E
    ? `An engineer corps standing here can ${field ? 'enlarge it' : 'lay one out'} in a month, as far as ${E} levels built. The treasury builds larger ones from the arsenal.`
    : 'It is as large as field engineers can make it. The treasury builds larger ones from the arsenal.';
  if (!field) return { region, name: r.name, field: null, note: `There is no airfield at ${r.name}: no aircraft can be based here.`, build, hidden: false, rows: [] };
  const hidden = field.groups < 0;
  const rows = hidden ? [] : g.airFleets.filter((f) => f.base === region && !afloat(f) && flying(f)).map((f) => groupRow(g, f, viewer));
  const used = rows.reduce((s, x) => s + x.aircraft, 0);
  const note = `${field.word}: it serves ${num(field.holds)} aircraft${hidden ? '. What flies from here is not known: scout it from the air, or put an agent on watch'
    : `, and ${used > 0 ? `${num(used)} are based here` : 'none are based here'}${field.crowded ? `. It is crowded: only ${pct(field.holds / used)} of each group can fly` : ''}`}.`;
  return { region, name: r.name, field, note, build, hidden, rows };
}

// ---- Standing orders ----------------------------------------------------------------------------------

/** Something a group can be left doing between raids: the key to give setGroupJob(), what it would mean here, and why it cannot be chosen. */
export interface GroupJob { key: string; label: string; on: boolean; why: string | null; forecast: string }

const mine = (g: Game, id: number, nat: string | null): AirFleet | null => {
  const f = g.airFleets.find((x) => x.id === id);
  return f && nat !== null && f.nat === nat && airWar(g) ? f : null;
};

/** What one of a nation's air groups can be left doing: patrol, stick with a bomber group as its escort, or rest. Each says what it would mean. */
export function groupJobs(g: Game, id: number, nat: string | null = g.human): GroupJob[] {
  const f = mine(g, id, nat);
  if (!f) return [];
  const R = airRange(f), kind = kindOf(f);
  const deck = carrierOf(g, f), home = deck ? `the ${deck.name}` : g.regions[f.base].name;
  const away = f.flew === g.turn && isOffensive(f.mission);
  const gone = away ? `It has flown its raid this month, and is back ${deck ? 'aboard' : 'at its airfield'} next month.` : null;
  const out: GroupJob[] = [];
  const with_ = escorted(g, f);
  const idle = !away && f.mission === 'support';
  const far = `${num(R.ftr * AIR.range.fullAt)} km`;
  if (kind === 'bmr') {
    out.push(deck
      ? { key: 'patrol', label: 'Stand ready for a strike', on: idle, why: gone, forecast: `Ranged on deck aboard ${home}. Its strike aircraft reach every coast of the sea the fleet is in, ${num(R.bmr)} km inland from it, and the seas next to it: plan a strike whenever a fleet is found or a harbour is in reach.` }
      : { key: 'patrol', label: 'Support the armies', on: idle, why: gone, forecast: `Stands ready at ${home}. Its bombers reach ${num(R.bmr)} km: plan a raid for it whenever there is a target worth the losses.` });
  } else {
    const under = g.regions.filter((r) => allied(g, r.ctrl, f.nat) && cover(kmTo(g, f, r.id), R.ftr) > 0).length;
    out.push(deck
      ? {
        key: 'patrol', label: 'Cover the fleet', on: idle && !with_, why: gone,
        forecast: `Meets any raid on the ships in the sea its fleet is in, and any raid on coasts within ${num(R.ftr)} km of it. It also keeps the enemy's coastal bombers off the fleet month by month. About ${num(f.ftr)} fighters can go up.`,
      }
      : {
        key: 'patrol', label: 'Patrol the sky', on: idle && !with_, why: gone,
        forecast: `Meets any raid within ${num(R.ftr)} km of ${home}, at full effect to ${far}: ${plural(under, 'friendly province')} under its cover. About ${num(f.ftr * crowding(g, f))} fighters can go up.`,
      });
    const bombers = airFleetsOf(g, f.nat).filter((b) => b.id !== f.id && kindOf(b) !== 'ftr' && b.bmr > 0 && apart(g, f, b) <= R.ftr);
    for (const b of bombers) {
      const d = apart(g, f, b);
      // The same deck or the same airfield, or a group somewhere else within its reach: a carrier at sea is at no distance from the coasts of its sea.
      const same = f.ship !== undefined ? b.ship === f.ship : b.ship === undefined && b.base === f.base;
      const where = same ? (deck ? ' from the same fleet' : ' from the same airfield') : ` (${homeName(g, b)}, ${d > 0 ? `${num(d)} km away` : 'within its reach'})`;
      out.push({
        key: `escort:${b.id}`, label: `Escort the ${b.name}`, on: idle && with_?.id === b.id, why: gone,
        forecast: `Sticks with the ${b.name}${where}: it goes on every day raid they fly within ${num(R.ftr)} km of ${home}, at full effect to ${far}. Beyond that the bombers go alone. Until they fly it patrols.`,
      });
    }
    if (!bombers.length) out.push({ key: 'escort', label: 'Escort a bomber group', on: false, why: `No bomber group is based within ${num(R.ftr)} km of ${home}. Fighters can still be picked for any raid when it is planned.`, forecast: '' });
  }
  const R2 = AIR.ready;
  out.push({ key: 'rest', label: 'Rest and refit', on: f.mission === 'rest', why: null, forecast: deck
    ? `Stands down: it regains ${pct(R2.rest)} readiness a month in place of ${pct(R2.support)}. It does not fly, and does not meet raids. New aircraft come aboard only while the fleet lies in harbour.`
    : `Stands down: it regains ${pct(R2.rest)} readiness a month in place of ${pct(R2.support)}, and gets replacement aircraft first. It does not fly, and does not meet raids.` });
  return out;
}

/** Gives one of a nation's air groups its standing order: 'patrol', 'escort:<bomber group id>' or 'rest'. */
export function setGroupJob(g: Game, id: number, key: string, nat: string | null = g.human): boolean {
  const f = mine(g, id, nat);
  const job = f ? groupJobs(g, id, nat).find((j) => j.key === key) : undefined;
  if (!f || !job || job.why) return false;
  if (key === 'rest') { delete f.escorts; return setMission(g, f, 'rest'); }
  if (key === 'patrol') delete f.escorts; else f.escorts = Number(key.split(':')[1]);
  return setMission(g, f, 'support');
}

/** Where a group could move to: the airfields in reach, nearest first, with the room each has. */
export interface BaseOption { region: number; name: string; km: number; word: string; holds: number; free: number; why: string | null }

export function groupBases(g: Game, id: number, nat: string | null = g.human): BaseOption[] {
  const f = mine(g, id, nat);
  // A carrier wing has no airfield to change: it goes where its fleet goes.
  if (!f || afloat(f)) return [];
  const out: BaseOption[] = [];
  for (const r of g.regions) {
    const holds = airfieldsHold(g, r.id);
    if (holds <= 0 || r.id === f.base || !allied(g, f.nat, r.ctrl)) continue;
    const d = kmTo(g, f, r.id);
    if (d > AIR.ferry) continue;
    const there = g.airFleets.reduce((s, x) => s + (x.base === r.id && !afloat(x) ? x.ftr + x.bmr : 0), 0);
    out.push({ region: r.id, name: r.name, km: d, word: fieldWord(holds).word, holds, free: Math.max(0, holds - there), why: whyNotBase(g, f, r) });
  }
  return out.sort((a, b) => a.km - b.km);
}

// ---- Planning a raid ------------------------------------------------------------------------------------

/** Why this group cannot go on this raid, or null if it can. */
function whyNotSeat(g: Game, f: AirFleet, kind: RaidKind, target: number): string | null {
  if (!flying(f)) return 'It has no aircraft fit to fly.';
  if (kind === 'recon') {
    const r = g.regions[target];
    if (!r) return 'Pick a province.';
    if (f.scouted === g.turn) return 'It has flown its reconnaissance this month.';
    if (!atWar(g, f.nat, r.ctrl)) return 'Only ground held by a nation you are at war with is photographed.';
    const R = airRange(f);
    return kmTo(g, f, target) <= (f.bmr > 0 ? R.bmr : R.ftr) ? null : 'It is out of range.';
  }
  if (f.flew === g.turn) return 'It has flown its raid this month.';
  if (f.mission === 'rest') return 'It is resting and refitting.';
  if (isOffensive(f.mission)) return 'It already has orders for the month.';
  return whyNotMission(g, f, kind, target);
}

/** A nation's groups that could still fly a raid this month. */
const able = (g: Game, nat: string): AirFleet[] => airFleetsOf(g, nat).filter((f) => flying(f));

/** A kind of raid on the planner's menu: how many targets are in reach of a group that can still fly, and why there are none. */
export interface RaidKindView { key: RaidKind; label: string; short: string; blurb: string; targets: number; why: string | null }

/** Everything a kind of raid could be aimed at: seas, provinces, the enemy fleets at sea that the nation's side has found, or the ports where enemy warships lie. */
function targetIds(g: Game, kind: RaidKind, nat: string): number[] {
  if (kind === 'strike') return g.seas.map((z) => z.id);
  if (kind === 'fleet') return carrierRules(g) ? g.fleets.filter((x) => atSea(x) && surfaceShips(x) > 0 && atWar(g, nat, x.nat) && fleetKnown(g, x, nat) !== 'none').map((x) => x.id) : [];
  if (kind === 'harbour') return carrierRules(g) ? [...new Set(g.fleets.filter((x) => !atSea(x) && x.base >= 0 && surfaceShips(x) > 0 && atWar(g, nat, x.nat)).map((x) => x.base))] : [];
  return g.regions.map((r) => r.id);
}

/** True if the viewer knows of enemy troops in this region: a raid on troops is planned only against an army that has been seen. */
function troopsSeen(g: Game, nat: string, region: number, sight: Uint8Array): boolean {
  return sight[region] !== SIGHT.none && g.regions[region].units.some((u) => !u.dead && atWar(g, nat, u.nat));
}

/** True if foreign troops stand in this region that the viewer has not identified this month. */
function unidentified(g: Game, nat: string, region: number): boolean {
  return g.regions[region].units.some((u) => !u.dead && !allied(g, u.nat, nat) && corpsKnown(g, u, nat).level !== 'exact');
}

function reachable(g: Game, nat: string, kind: RaidKind): Map<number, AirFleet[]> {
  const out = new Map<number, AirFleet[]>();
  const fleets = able(g, nat);
  const needs = kindRow(kind).needs;
  const sight = kind === 'ground' || kind === 'recon' ? sightOf(g, nat) : null;
  for (const t of targetIds(g, kind, nat)) {
    if (kind === 'ground' && !troopsSeen(g, nat, t, sight!)) continue;
    // Ground in full view has something to show a camera only where an army there is not identified, or known from an old report.
    if (kind === 'recon' && sight![t] === SIGHT.full && !unidentified(g, nat, t)) continue;
    const can = fleets.filter((f) => whyNotSeat(g, f, kind, t) === null);
    // A raid that needs bombers is not offered where only fighters reach.
    if (!can.length || (needs === 'bmr' && !can.some((f) => f.bmr > 0))) continue;
    out.set(t, can);
  }
  return out;
}

/** The planner's menu: every kind of raid, with the number of targets a group could still fly against this month. */
export function raidKinds(g: Game, nat: string | null = g.human): RaidKindView[] {
  if (!airWar(g) || nat === null) return [];
  const peace = !g.nations[nat].atWar.length;
  return AIRWAR.raids.kinds.map((k) => {
    const n = peace ? 0 : reachable(g, nat, k.key as RaidKind).size;
    return { key: k.key as RaidKind, label: k.label, short: k.short, blurb: k.blurb, targets: n, why: peace ? 'The nation is at peace.' : n ? null : 'No group that can still fly this month has a target in reach.' };
  });
}

/** A target for one kind of raid: where it is, how far from the nearest group that could fly, how many groups could, and what is there. */
export interface RaidTarget {
  /** A province, a sea (a shipping strike) or a fleet (a strike on one fleet): what to give planRaid(). */
  id: number;
  sea: boolean;
  /** The sea a strike is fought over, or -1; and for a strike on a fleet, whether the fleet is identified or only reported. */
  zone: number;
  known: 'exact' | 'report';
  name: string;
  holder: string;
  x: number;
  y: number;
  km: number;
  groups: number;
  what: string;
}

/** The fighters a fleet's own carriers have on patrol over it. */
const deckFighters = (g: Game, fleet: number): number => g.airFleets.reduce((s, w) => s + (w.ship === fleet && w.mission === 'support' && w.flew !== g.turn ? w.ftr : 0), 0);

function whatIsThere(g: Game, nat: string, kind: RaidKind, t: number, sight: Uint8Array): string {
  if (kind === 'strike') {
    // Only the warships the nation's side has identified are counted: the rest are a report, or not known at all.
    let ships = 0, reports = 0;
    for (const f of g.fleets) {
      if (f.zone !== t || !atSea(f) || !atWar(g, nat, f.nat) || !surfaceShips(f)) continue;
      const k = fleetKnown(g, f, nat);
      if (k === 'exact') ships += surfaceShips(f); else if (k === 'report') reports++;
    }
    return ships ? `${plural(ships, 'warship')} at sea${reports ? ', more reported' : ''}, and merchant shipping` : reports ? 'Ships reported at sea, and merchant shipping' : 'Merchant shipping';
  }
  if (kind === 'fleet') {
    const f = g.fleets.find((x) => x.id === t);
    if (!f) return '';
    if (fleetKnown(g, f, nat) !== 'exact') return 'Reported, not identified: a strike sent on a report finds less of it';
    const up = deckFighters(g, f.id);
    return `${shipList(f.ships)}${f.ships.cv > 0 ? (up > 0 ? `, about ${num(up)} carrier fighters over it` : ', its fighters away or spent') : ''}`;
  }
  if (kind === 'harbour') {
    const there = shipsInPort(g, t, nat);
    const seen = there.filter((f) => fleetKnown(g, f, nat) === 'exact');
    const bits = seen.map((f) => `${f.name}: ${shipList(f.ships)}`);
    if (seen.length < there.length) bits.push(seen.length ? 'more ships not identified' : 'Warships in harbour, not identified');
    if (gunsAt(g, t)) bits.push(`level ${gunsAt(g, t)} guns`);
    return bits.join(', ');
  }
  const r = g.regions[t];
  if (kind === 'bomb' || kind === 'night') {
    const bits = [r.fac > 0 ? plural(r.fac, 'factory', 'factories') : '', onNet(t) && !railCut(g, t) ? (railsAt(t).length >= 3 ? 'a railway junction' : 'a railway') : '', gunsAt(g, t) ? `level ${gunsAt(g, t)} guns` : ''];
    return bits.filter(Boolean).join(', ');
  }
  if (kind === 'ground') {
    const foes = r.units.filter((u) => !u.dead && atWar(g, nat, u.nat));
    const known = foes.every((c) => corpsKnown(g, c, nat).level === 'exact');
    return sight[t] === SIGHT.full && known ? `${plural(foes.length, 'corps', 'corps')}, ${plural(foes.reduce((s, c) => s + c.divs, 0), 'division')}${foes.some((c) => c.dug) ? ', dug in' : ''}`
      : sight[t] === SIGHT.full ? `${plural(foes.length, 'corps', 'corps')} seen, not identified${foes.some((c) => c.dug) ? ', dug in' : ''}` : `${plural(foes.length, 'corps', 'corps')} seen`;
  }
  if (kind === 'superiority') {
    const based = g.airFleets.filter((f) => f.base === t && !afloat(f) && flying(f) && atWar(g, f.nat, nat)).length;
    const up = fightersOver(g, t, nat);
    return [based ? `${plural(based, 'air group')} based there` : airfieldsHold(g, t) > 0 ? 'an empty airfield' : 'no airfield', up >= 1 ? `about ${num(up)} fighters can come up` : 'no fighters in reach'].join(', ');
  }
  if (sight[t] === SIGHT.full) {
    const foes = g.regions[t].units.filter((u) => !u.dead && !allied(g, u.nat, nat));
    const old = foes.filter((c) => corpsKnown(g, c, nat).level === 'report').length;
    return old === foes.length ? `${plural(foes.length, 'corps', 'corps')} known only from old reports` : `${plural(foes.length, 'corps', 'corps')} in view, ${old ? `${foes.length - old} of them not identified` : 'not identified'}`;
  }
  return sight[t] === SIGHT.glimpse ? 'Only a flag and a count are known' : 'Nothing is known of what stands there';
}

/** Every target for one kind of raid that a group could still fly against this month, nearest first. With `lead`, only those that one group can fly against. */
export function raidTargets(g: Game, kind: RaidKind, nat: string | null = g.human, lead = -1): RaidTarget[] {
  if (!airWar(g) || nat === null || !g.nations[nat].atWar.length) return [];
  const sea = kind === 'strike' || kind === 'fleet';
  const sight = sightOf(g, nat);
  const out: RaidTarget[] = [];
  for (const [t, can] of reachable(g, nat, kind)) {
    if (lead >= 0 && !can.some((f) => f.id === lead)) continue;
    const quarry = kind === 'fleet' ? g.fleets.find((x) => x.id === t)! : null;
    const zone = quarry ? quarry.zone : kind === 'strike' ? t : -1;
    const p = sea ? g.seas[zone] : g.regions[t];
    // With a group leading, the distance is its own; otherwise the nearest group's.
    const d = Math.min(...can.filter((f) => lead < 0 || f.id === lead).map((f) => distTo(g, f, kind, t)));
    const known = quarry && fleetKnown(g, quarry, nat) !== 'exact' ? 'report' : 'exact';
    const name = kind === 'fleet' ? placeOf(g, kind, t, nat).replace(/^the /, '') : p.name;
    out.push({ id: t, sea, zone, known, name, holder: quarry ? quarry.nat : sea ? '' : g.regions[t].ctrl, x: p.x, y: p.y, km: d, groups: can.length, what: whatIsThere(g, nat, kind, t, sight) });
  }
  return out.sort((a, b) => a.km - b.km || a.id - b.id);
}

/** One of the nation's groups on the planner: whether it can go, how far it has to fly, and whether it is going. */
export interface RaidSeat {
  id: number;
  name: string;
  kind: 'ftr' | 'bmr' | 'mixed';
  aircraft: number;
  base: number;
  baseName: string;
  /** The fleet whose carriers it flies from, or -1. */
  ship: number;
  km: number;
  /** 0..1: how much of a fighter group counts over the target at this distance (1 for bombers in range). */
  effect: number;
  ready: number;
  picked: boolean;
  /** It goes because it is ordered to stick with a bomber group that is going. */
  locked: boolean;
  /** Why it cannot go, or null. */
  why: string | null;
  /** What going means for it, in a few words. */
  note: string;
}

/** A raid weighed up before it is flown. */
export interface RaidPlan {
  kind: RaidKind;
  target: number;
  place: string;
  title: string;
  /** Why it cannot be flown as picked, or null. */
  why: string | null;
  /** Every group of the nation, those that can go first, nearest first. */
  seats: RaidSeat[];
  /** The groups going: the ones asked for that can, and the escorts ordered to stick with them. */
  picked: number[];
  bombers: number;
  fighters: number;
  /** The enemy fighters that will come up, and how many of each group can reach the raid. */
  meets: { name: string; nat: string; baseName: string; afloat: boolean; ftr: number; able: number }[];
  lines: TicketLine[];
  /** 0..1: the chance the raid costs the enemy more than it costs the raiders, and that in words. */
  success: number;
  verdict: string;
  /** The share of the bombers likely to be lost, and the aircraft each side is likely to lose. */
  bomberLoss: number;
  bomberLossN: number;
  fighterLossN: number;
  foeLossN: number;
  /** What the raid should do, a sentence each. */
  effects: string[];
  /** What flying it together is worth beside sending each bomber group alone, or '' where only one goes. */
  massed: string;
  /** What the raid costs at home: fighter groups taken off patrol for the rest of the month. */
  offPatrol: string;
}

const VERDICT: [number, string][] = [[0.8, 'Very likely to succeed'], [0.6, 'Likely to succeed'], [0.4, 'It could go either way'], [0.2, 'Likely to fail'], [0, 'Very likely to fail']];

function effectsOf(g: Game, kind: RaidKind, target: number, fc: ReturnType<typeof forecastAir>, nat: string): string[] {
  const out: string[] = [];
  const place = placeOf(g, kind, target);
  if (kind === 'bomb' || kind === 'night') {
    const r = g.regions[target];
    out.push(r.fac > 0 ? `Should stop about ${pct(fc.cut)} of ${place}'s output for a month: ${(fc.nation * 100).toFixed(1)}% of ${g.nations[r.ctrl].adj} production.` : `There is no industry at ${place}: the bombers go for its rail yards.`);
    if (fc.rail) out.push(`About a ${pct(fc.rail)} chance of cutting the railway through ${place}, which stays cut until engineers relay it.`);
    if (fc.ruin) out.push(`About a ${pct(fc.ruin)} chance of wrecking a factory outright.`);
  } else if (kind === 'superiority') {
    out.push(`Fighter odds ${fc.odds >= 9 ? 'overwhelming' : `${fc.odds.toFixed(1)} to 1`} going in. The sky is won when your fighters end the month with ${AIR.sky.win} times what the defender can still put up.`);
    if (fc.cut > 0) out.push(`The air groups based at ${place} should lose about ${pct(fc.cut)} of their readiness, and aircraft on the ground.`);
  } else if (kind === 'strike') {
    out.push(`Warships caught at sea there could lose up to ${pct(fc.cut)} of their number, and merchant ships in those waters will be sunk.`);
  } else if (kind === 'fleet' || kind === 'harbour') {
    const sunk = fc.ships, hits = fc.hits ?? 0;
    const word = (c: (typeof CLASSES)[number], n: number): string => (n === 1 ? NAVY.classes[c].one : NAVY.classes[c].many);
    const sure = sunk ? CLASSES.filter((c) => sunk[c] >= 1).map((c) => `${Math.floor(sunk[c])} ${word(c, Math.floor(sunk[c]))}`) : [];
    const maybe = sunk ? CLASSES.filter((c) => sunk[c] % 1 >= 0.15).map((c) => `${pct(sunk[c] % 1)} for ${sunk[c] >= 1 ? 'another' : 'a'} ${word(c, 1)}`) : [];
    out.push(`About ${hits >= 10 ? num(hits) : hits.toFixed(1)} bombs and torpedoes should hit. ${sure.length ? `That should sink ${list(sure)}` : maybe.length ? 'No ship is sure to go down' : 'Too few to sink a ship'}${maybe.length ? `, with a chance of ${list(maybe)}` : ''}. Carriers and battleships are aimed at first, and every ship hit leaves the fleet less fit for sea.`);
    if (sunk && sunk.cv > 0.15) out.push('A carrier that goes down takes most of its aircraft with it.');
  } else if (kind === 'ground') {
    const dug = g.regions[target].units.some((u) => !u.dead && u.dug && atWar(g, nat, u.nat));
    out.push(`The troops at ${place} should lose about ${pct(fc.cut)} of their organisation${dug ? ' (less where they are dug in)' : ''}, and some of their men and guns. They fight the weaker for it until they have rested: attack while it lasts.`);
  }
  return out;
}

/**
 * A raid weighed up for the planner. `ids` are the groups asked to fly it; left out, the staff proposes every bomber
 * group that can reach (for a raid without bombers, the nearest fighter group). Fighter groups ordered to stick with
 * a bomber group that is going go too. Nothing is changed: launchRaid() flies it.
 */
export function planRaid(g: Game, kind: RaidKind, target: number, ids?: number[], nat: string | null = g.human): RaidPlan {
  const row = kindRow(kind);
  const place = placeOf(g, kind, target, nat);
  const quarry = kind === 'fleet' ? g.fleets.find((x) => x.id === target) ?? null : null;
  const plan: RaidPlan = {
    kind, target, place, title: `${row.short}: ${place}`, why: null, seats: [], picked: [], bombers: 0, fighters: 0, meets: [], lines: [],
    success: 0, verdict: '', bomberLoss: 0, bomberLossN: 0, fighterLossN: 0, foeLossN: 0, effects: [], massed: '', offPatrol: '',
  };
  if (!airWar(g) || nat === null) { plan.why = 'Raids are planned only in the grand campaign.'; return plan; }
  if (!(kind === 'strike' ? g.seas[target] : kind === 'fleet' ? quarry : g.regions[target])) { plan.why = kind === 'strike' ? 'Pick a sea.' : kind === 'fleet' ? 'Pick a fleet.' : 'Pick a province.'; return plan; }
  if (!g.nations[nat].atWar.length) { plan.why = 'The nation is at peace.'; return plan; }
  if (quarry && fleetKnown(g, quarry, nat) === 'none') { plan.why = 'No fleet has been found there. Search that sea first: a fleet has to be found before it can be struck.'; return plan; }
  if (kind === 'ground' && !troopsSeen(g, nat, target, sightOf(g, nat))) { plan.why = 'No enemy troops have been seen there.'; return plan; }
  const fleets = airFleetsOf(g, nat).filter((f) => flying(f) || f.est.ftr + f.est.bmr > 0);
  const whys = new Map(fleets.map((f) => [f.id, whyNotSeat(g, f, kind, target)]));
  const can = fleets.filter((f) => whys.get(f.id) === null);
  const dist = (f: AirFleet): number => distTo(g, f, kind, target);
  // Who goes.
  let going: AirFleet[];
  if (ids) going = can.filter((f) => ids.includes(f.id));
  else {
    // The staff's proposal: what the nearest airfield can send. The player adds groups from other airfields to it.
    const near = [...can].sort((a, b) => dist(a) - dist(b) || a.id - b.id);
    // A fight for the sky is the fighters' battle first; every other raid is the bombers'.
    const lead = kind === 'recon' ? near[0] : kind === 'superiority' ? near.find((f) => f.ftr > 0) ?? near[0] : near.find((f) => f.bmr > 0) ?? (row.needs === 'any' ? near[0] : undefined);
    const same = lead ? can.filter((f) => homeOf(f) === homeOf(lead) && (lead.bmr > 0 ? f.bmr > 0 : f.ftr > 0)) : [];
    going = kind === 'recon' ? same.slice(0, 1) : same;
  }
  if (kind === 'recon') going = going.slice(0, 1);
  const locked = new Set<number>();
  if (kind !== 'recon' && kind !== 'night') for (const f of can) {
    if (going.includes(f) || f.escorts === undefined || !going.some((b) => b.id === f.escorts && b.bmr > 0)) continue;
    going.push(f);
    locked.add(f.id);
  }
  plan.picked = going.map((f) => f.id);
  plan.seats = fleets.map((f): RaidSeat => {
    const d = dist(f), R = airRange(f), k = kindOf(f);
    const effect = k === 'bmr' ? (d <= R.bmr ? 1 : 0) : kind === 'night' ? 0 : cover(d, R.ftr);
    const why = whys.get(f.id) ?? null;
    const note = why ? '' : kind === 'recon' ? `${Math.min(AIRWAR.recon.aircraft, f.ftr + f.bmr)} of its aircraft go` : k === 'bmr' ? `${num(d)} km to fly` : effect >= 1 ? `${num(d)} km: well inside its range` : `${num(d)} km: at ${pct(effect)} effect over the target`;
    return { id: f.id, name: f.name, kind: k, aircraft: f.ftr + f.bmr, base: f.base, baseName: homeName(g, f), ship: f.ship ?? -1, km: d, effect, ready: f.ready, picked: going.includes(f), locked: locked.has(f.id), why, note };
  }).sort((a, b) => (a.why ? 1 : 0) - (b.why ? 1 : 0) || a.km - b.km || a.id - b.id);
  plan.bombers = going.reduce((s, f) => s + f.bmr, 0);
  plan.fighters = going.reduce((s, f) => s + f.ftr, 0);
  if (!going.length) { plan.why = can.length ? 'Pick at least one air group.' : 'No group that can still fly this month reaches it.'; return plan; }

  if (kind === 'recon') {
    const f = going[0];
    const n = Math.min(AIRWAR.recon.aircraft, f.ftr + f.bmr);
    const risk = reconRisk(g, nat, target);
    plan.fighters = f.ftr > 0 ? n : 0;
    plan.bombers = f.ftr > 0 ? 0 : n;
    plan.success = 1 - risk;
    plan.verdict = risk <= 0 ? 'Nothing will come up to stop it' : risk < 0.15 ? 'A small risk from fighters on patrol there' : 'Fighters on patrol there will take a toll';
    const lost = n * risk;
    if (f.ftr > 0) plan.fighterLossN = Math.round(lost); else plan.bomberLossN = Math.round(lost);
    plan.bomberLoss = f.ftr > 0 ? 0 : risk;
    plan.effects = [`Every army at ${place} and in the provinces beside it is identified, corps by corps. The report is exact this month and goes stale after.`, `The ${f.name} still flies its raid and keeps its patrol: this takes only ${n} aircraft.`];
    return plan;
  }

  const m = missionOf(kind);
  if (row.needs === 'bmr' && plan.bombers <= 0) { plan.why = 'Pick at least one bomber group: fighters only go as their escort.'; return plan; }
  const a = assessAir(g, going, m, target);
  if (a.why) { plan.why = a.why; return plan; }
  const fc = forecastAir(g, a, m, target);
  plan.meets = a.def.map((x) => ({ name: x.f.name, nat: x.f.nat, baseName: homeName(g, x.f), afloat: afloat(x.f), ftr: x.f.ftr, able: Math.round(x.f.ftr * x.w) }));
  plan.lines = a.lines;
  plan.success = fc.success;
  plan.verdict = VERDICT.find(([least]) => fc.success >= least)![1];
  plan.bomberLoss = fc.bomberLoss;
  plan.bomberLossN = Math.round(fc.bmrLoss);
  plan.fighterLossN = Math.round(fc.ftrLoss);
  plan.foeLossN = Math.round(fc.foeLoss);
  plan.effects = effectsOf(g, kind, target, fc, nat);
  // What meeting up is worth: each bomber group alone, with the fighters from its own airfield, beside all of them together.
  const bombers = going.filter((f) => f.bmr > 0);
  if (bombers.length >= 2) {
    const alone = bombers.map((b) => assessAir(g, [b, ...going.filter((x) => x.bmr <= 0 && homeOf(x) === homeOf(b))], m, target)).filter((x) => !x.why).map((x) => x.bomberLoss);
    if (alone.length) {
      const lo = Math.min(...alone), hi = Math.max(...alone);
      const fields = new Set(bombers.map(homeOf)).size;
      const from = bombers.some(afloat) ? plural(fields, 'fleet or airfield', 'fleets and airfields') : plural(fields, 'airfield');
      plan.massed = hi > a.bomberLoss + 0.005
        ? `Meeting up pays: ${plural(bombers.length, 'bomber group')} from ${from} flying together should lose about ${pct(a.bomberLoss)} of their bombers. Sent one group at a time it would be ${pct(lo)}${pct(lo) !== pct(hi) ? ` to ${pct(hi)}` : ''}.`
        : `${plural(bombers.length, 'bomber group')} fly together from ${from}. Here it changes the losses little: ${a.D < 1 ? 'no fighters will come up' : 'the defenders are too many, or the escort too strong, for numbers to tell'}.`;
    }
  }
  const off = going.filter((f) => f.ftr > 0 && f.mission === 'support' && kindOf(f) !== 'bmr');
  if (off.length) plan.offPatrol = `${list(off.map((f) => `the ${f.name}`))} ${off.length === 1 ? 'leaves its patrol' : 'leave their patrols'} for the rest of the month: the sky over ${list([...new Set(off.map((f) => (afloat(f) ? `the ${homeName(g, f)}` : g.regions[f.base].name)))])} has that much less cover until next month.`;
  if (off.some(afloat)) plan.offPatrol += ` A fleet whose fighters are away is open to the enemy's strike until they are back.`;
  return plan;
}

/**
 * Flies a raid the planner has weighed up, at once: the report is filed and its news told. Every group that flew is
 * away for the rest of the month (it flies no second raid and meets no raid on its own sky) and is back at its airfield,
 * on patrol or on escort as it was, when the nation's next month begins. Null if the raid cannot be flown as picked.
 */
export function launchRaid(g: Game, kind: RaidKind, target: number, ids: number[], nat: string | null = g.human): AirReport | null {
  if (kind === 'recon' || nat === null) return null;
  const plan = planRaid(g, kind, target, ids, nat);
  if (plan.why) return null;
  const fleets = plan.picked.map((id) => g.airFleets.find((f) => f.id === id)!);
  const m = missionOf(kind);
  const rep = fly(g, nat, m, target, fleets, `:${(g.flags.airBattles ?? 0) + 1}`);
  if (!rep) return null;
  for (const f of fleets) { f.mission = m; f.target = target; f.flew = g.turn; }
  return rep;
}

// ---- Scouting from the air --------------------------------------------------------------------------------

/** The share of a reconnaissance flight likely to be lost: nothing where no enemy fighter patrols the target. */
function reconRisk(g: Game, nat: string, target: number): number {
  const up = fightersOver(g, target, nat);
  return up <= 0 ? 0 : Math.min(1, up / (up + 200)) * AIRWAR.recon.loss;
}

/** One side's part in a raid that has been flown: who it was, where it flew from, what it had and what it lost. */
export interface ReplaySide { id: number; name: string; nat: string; base: number; baseName: string; x: number; y: number; ftr: number; bmr: number; lostFtr: number; lostBmr: number }

/** A raid that has been flown, with everything needed to show it again: the target, the raiders and where each took off, the fighters that rose and from where, and the losses. */
export interface RaidReplay {
  id: number;
  turn: number;
  kind: AirMission;
  /** The province raided, or -1 for a strike at sea, and the sea struck, or -1. */
  region: number;
  zone: number;
  place: string;
  x: number;
  y: number;
  attNat: string;
  defNat: string;
  attackers: ReplaySide[];
  defenders: ReplaySide[];
  won: boolean;
  level: string;
  result: string;
}

/** The raids flown in one month (this month unless told otherwise), oldest first, as a viewer may know them: his own side's, and those flown against it. With nobody at the controls, all of them. */
export function raidReplays(g: Game, turn = g.turn, viewer: string | null = g.human): RaidReplay[] {
  const side = (xs: AirReport['att']): ReplaySide[] => xs.map((s) => {
    const f = s.id === undefined ? g.airFleets.find((y) => y.name === s.fleet && y.nat === s.nat) : g.airFleets.find((y) => y.id === s.id);
    const base = s.base ?? f?.base ?? -1, r = g.regions[base];
    // A carrier wing took off from where its fleet was, not from a province.
    if (s.at) return { id: s.id ?? f?.id ?? -1, name: s.fleet, nat: s.nat, base: -1, baseName: 'its carriers', x: s.at[0], y: s.at[1], ftr: s.ftr, bmr: s.bmr, lostFtr: s.lostFtr, lostBmr: s.lostBmr };
    return { id: s.id ?? f?.id ?? -1, name: s.fleet, nat: s.nat, base, baseName: r?.name ?? '', x: r?.x ?? 0, y: r?.y ?? 0, ftr: s.ftr, bmr: s.bmr, lostFtr: s.lostFtr, lostBmr: s.lostBmr };
  });
  const mineToo = (nat: string): boolean => viewer === null || (!!nat && allied(g, nat, viewer));
  return g.airReports.filter((r) => r.turn === turn && (mineToo(r.attNat) || mineToo(r.defNat))).map((r) => {
    const p = r.zone >= 0 ? g.seas[r.zone] : g.regions[r.region];
    return { id: r.id, turn: r.turn, kind: r.mission, region: r.region, zone: r.zone, place: r.place, x: p?.x ?? 0, y: p?.y ?? 0, attNat: r.attNat, defNat: r.defNat, attackers: side(r.att), defenders: side(r.def), won: r.won, level: r.level, result: r.result };
  });
}

/**
 * A reconnaissance flight: a few aircraft of one group photograph a province. Every army there and in the provinces
 * beside it is identified this month, as an agent on watch would identify it. One flight a month for each group,
 * beside its raid and without leaving its patrol.
 */
export function scout(g: Game, id: number, target: number, nat: string | null = g.human): { ok: boolean; text: string; lost: number } {
  const f = mine(g, id, nat);
  const why = !f ? 'That group is not yours to send.' : !g.nations[f.nat].atWar.length ? 'The nation is at peace.' : whyNotSeat(g, f, 'recon', target);
  if (!f || why) return { ok: false, text: why ?? '', lost: 0 };
  const r = g.regions[target];
  const n = Math.min(AIRWAR.recon.aircraft, f.ftr + f.bmr);
  const rng = new Rng(hashString(`air:${g.seed}:${g.turn}:recon:${f.id}:${target}`));
  const lost = Math.min(n, rng.roll(n * reconRisk(g, f.nat, target)));
  const p = airPool(g, f.nat);
  if (f.ftr > 0) { f.ftr -= lost; f.lost.ftr += lost; p.lostFtr += lost; p.lostPilots += lost; }
  else { f.bmr -= lost; f.lost.bmr += lost; p.lostBmr += lost; }
  f.ready = Math.max(AIR.ready.floor, f.ready - AIRWAR.recon.ready);
  f.scouted = g.turn;
  // Last month's photographs are out of date: the armies have moved.
  const kept: Record<number, number> = {};
  for (const [k, turn] of Object.entries(g.intel.scout ?? {})) if (turn === g.turn) kept[+k] = turn;
  kept[target] = g.turn;
  g.intel.scout = kept;
  const foes = r.units.filter((u) => !u.dead && atWar(g, f.nat, u.nat));
  const seen = foes.length ? `${plural(foes.length, 'corps', 'corps')}, ${plural(foes.reduce((s, c) => s + c.divs, 0), 'division')}${foes.some((c) => c.dug) ? ', dug in' : ''}` : 'no troops there';
  const text = `Aircraft of the ${f.name} photographed ${r.name}: ${seen}.${lost ? ` ${plural(lost, 'aircraft', 'aircraft')} did not come back.` : ''}`;
  if (f.nat === g.human) headline(g, 'info', text, f.nat, r.id);
  return { ok: true, text, lost };
}
