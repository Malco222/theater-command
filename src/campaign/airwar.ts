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

import { armedAt, strikeWords } from './atom';
import { roleWord, shortName } from './airnames';
import { calledName } from './airnicks';
import { nicknamesOn } from './nicknames';
import { tenthPct } from '../core/math';
import { Rng, hashString } from '../core/rng';
import { turnToDate, type TicketLine } from '../core/ticket';
import type { AirFleet, AirMission, AirReport, Game } from './types';
import type { FoughtAir } from './airfought';
import {
  AIR, AIRWAR, CLOSE, afloat, pilotWords, schoolWords, airFleetsOf, airPool, airRange, airWar, airfieldsHold, assessAir, carrierOf, carrierRules, closeRules, cover, crowding, fightersOver, fly, forecastAir, groupAt, guardsLand, gunsAt, gunsStop, homeOf, isOffensive, kmTo, kmToSea, overOcean, setMission, shipsInPort, skyZone, smallOn, whyNotBase, whyNotMission, whyNotShipAir, flakGuns, noneSeen, seenAgo, squadronsOf, standingGuns, strafeNone, type AirForecast,
} from './air';
import { damageRules } from './citydamage';
import { PRICE_ASHORE, decksFor, fleetKnown, shoresOf, whyNoDeck, whyNoShore, whyNotAshore, type DeckOption } from './carriers';
import { DEFENCE, allied, atWar, headline, theWords, placeName, placeOwns, worksOf } from './game';
import { raidCardLines } from './fighters';
import { SIGHT, corpsKnown, sightOf } from './intel';
import { CLASSES, NAVY, aswKit, atSea, shipList, surfaceShips } from './navy';
import { patrolSeas, watchWith } from './seapatrol';
import { airKnown, airMetWords, seaSeenWords } from './seasight';
import { airfieldOpenTo } from './prizes';
import { seaWhat, seaWhere } from './sea-words';
import { onNet, railCut, railsAt } from './rails';
import { besiegerOf, siegeRules, siegeView, townHeld, townHitText, townWorksPhrase, type TownHit } from './siege';
import { townName } from './towns';
import { shipRule } from './tech';
import { forecastLine, grounded, skyOf, skyRaidLine, weatherOn, whyNotSky, zoneName, zoneOfGroup, zoneOfRegion } from './weather';
import { whyNotField, wrecked } from './airversion';
import { wayWords } from './ferry';
import { strafeSection, type StrafeSection } from './strafers';
import { cityDefences, lightsAt } from './citydefences';
import { fieldKnown, fieldRules, fieldView } from './fieldstate';
import { band, bandNote, fogBits, fogged, knownDefence, knownLines, raidFogOn, rememberSkies, skyKnown } from './airfog';

export type RaidKind = 'bomb' | 'night' | 'ground' | 'line' | 'town' | 'superiority' | 'strike' | 'fleet' | 'harbour' | 'recon' | 'airfield';

const num = (n: number): string => Math.round(n).toLocaleString('en-US');
const pct = (x: number): string => `${Math.round(x * 100)}%`;
const plural = (n: number, one: string, many = `${one}s`): string => `${num(n)} ${Math.round(n) === 1 ? one : many}`;
const list = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const kindRow = (k: RaidKind) => AIRWAR.raids.kinds.find((x) => x.key === k)!;
/**
 * The older strike at shipping: what the warships caught at sea there should lose, as the raid card and the air order's
 * card say it (ruling 240, rider 4; the UI director, 22 November 12:53Z). With the dc switch on, the share is the fight's
 * own expected loss (forecastAir, the design director's rider 4 of 11:12Z), in the card's forecast voice: "should lose
 * about 3%", or "almost none" where that rounds to nothing, and "those waters", as the sentence has said "there". With
 * the switch off, release 1's words stand as each card had them: "could lose up to", and 'these' waters on the air
 * order's card (logged for release 2).
 */
export function shippingWords(g: Game, cut: number, waters: 'those' | 'these' = 'those'): string {
  return `Warships caught at sea there ${shipsLose(g, cut)} of their number, and merchant ships in ${shipRule(g, 'dc') ? 'those' : waters} waters will be sunk.`;
}

/** The verb and the share of shippingWords, for a card of another shape (the 3D game's air order page): "should lose about
 * 3%" or "should lose almost none" with the dc switch on, release 1's "could lose up to 3%" with it off. */
export function shipsLose(g: Game, cut: number): string {
  const n = Math.round(cut * 100);
  return shipRule(g, 'dc') ? `should lose ${n > 0 ? `about ${n}%` : 'almost none'}` : `could lose up to ${n}%`;
}
/** What a raid goes for, in words. A fleet is named only to someone who has it identified. */
function placeOf(g: Game, kind: RaidKind, target: number, viewer: string | null = null): string {
  if (kind === 'strike') return g.seas[target] ? seaWhat(g.seas[target].name) : 'the sea';
  if (kind === 'fleet') {
    const t = g.fleets.find((x) => x.id === target);
    if (!t) return 'a fleet';
    return fleetKnown(g, t, viewer) === 'exact' ? `the ${t.name}` : `${g.nations[t.nat].adj} ships ${seaWhere(g.seas[t.zone].name)}`;
  }
  // (The place with its article: "the harbour of the Highlands", the UI director, 25 November 2026, 15:04Z.)
  if (kind === 'harbour') { const p = g.regions[target]?.name ?? ''; return /\bHarbou?r$/.test(p) ? p : `the harbour of ${placeName(p)}`; }
  // (The town with its article where the map's list gives it one: "the defences of the Ohio Valley", 15:42Z.)
  if (kind === 'town') return g.regions[target] ? `the defences of ${placeName(townName(g.regions[target]))}` : '';
  // (Release 2's airfield as a target, with the place's article as every air sentence takes it: the UI director's mark (g).)
  if (kind === 'airfield') return g.regions[target] ? `the airfield at ${placeName(g.regions[target].name)}` : '';
  return g.regions[target]?.name ?? '';
}
/** The kind of raid a group's orders are, for naming what it flew against. */
const kindFlown = (m: AirMission): RaidKind => (m === 'strike' || m === 'fleet' || m === 'harbour' || m === 'town' || m === 'airfield' ? m : 'bomb');
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
const monthsOf = (m: number): string => { const v = Math.round(m * 10) / 10; return v === 1 ? 'a month' : `${v} months`; };

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

/** Every airfield on the map, for a map to draw. Empty outside the grand campaign's air war, where airfields are not places. 'seen' is the viewer's sightOf where the caller has it already. */
export function airfields(g: Game, viewer: string | null = g.human, seen?: Uint8Array): AirfieldRow[] {
  if (!airWar(g)) return [];
  const sight = seen ?? sightOf(g, viewer);
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
  /** The province of its airfield, or -1 for a wing aboard its fleet: a wing aboard is at no airfield, and is never counted or drawn at one. */
  base: number;
  baseName: string;
  /** For a wing aboard: the province of the harbour its fleet lies in, or -1 at sea or at a base off the map. -1 for every other group. */
  port: number;
  /** For a carrier wing ordered ashore: the fleet whose decks it left, and that fleet's name ('' if it is gone). -1 and '' for every other group. */
  ashore: number;
  fleetName: string;
  /** Where it is, in a line: "Aboard the Atlantic Fleet", "Ashore at Norfolk", or "At Kent" for a group that was never a wing. */
  where: string;
  /**
   * A carrier wing's one order of its own (ruling 175(b)): 'ashore' for a wing aboard, 'aboard' for one ashore, with
   * the sentence that stands by the order before it is given and why it cannot be given now (null where it can).
   * Null for every other group. Where to: airGroupBases() for a wing aboard, airGroupDecks() for one ashore.
   */
  wingMove: { order: 'ashore' | 'aboard'; price: string; why: string | null } | null;
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
  /** Weather (ruling 143): only when the sky over its airfield is closed, why it is grounded and next month's chances. */
  sky?: string[];
  /** Under Units decide (the UI director 24 Nov 18:10Z): its pilots in a sentence, "Seasoned pilots, 250 hours' training."; '' off. */
  pilots: string;
  /** The refit note, only in the month new pilots took its grade down a step: "14 new pilots joined this month."; else ''. */
  joined: string;
  /** The viewer's own group only, under Units decide: its nation's schools and fighter doctrine, said first on its page; else ''. */
  schools: string;
  /**
   * Release 2, the switch 'ferry' (ferry.ts): one of the viewer's own groups on its way to another field by stages, where
   * it is going, the hops still to fly (0 when it goes crated by sea), and whether it goes by sea. Never set otherwise.
   */
  way?: { to: number; toName: string; left: number; sea: boolean };
  /**
   * (Release 2, better air unit names) Its role in a plain word, 'fighters' or 'bombers', where its name does not say it
   * (absent where it does), and its short form for a map counter. Words only.
   */
  role?: string;
  short?: string;
  /** (Release 2, earned names) Its name with its earned name set as its arm sets it, and its earned colours; absent until earned. */
  called?: string;
  colours?: { scheme: number; tier: number; since: number };
}

/** A sea patrol's line on its group's row (Q8, the UI director's words): the sea, and the share of the waters it reaches that its side's patrols watch. */
function patrolDoing(g: Game, f: AirFleet, z: number, w = watchWith(g, f, z)): string {
  return `Patrol ${seaWhat(g.seas[z].name)} against submarines: watches about ${pct(w.share)} of the waters it reaches`;
}

/** The bomber group a fighter group is ordered to stick with, if that group still flies. */
const escorted = (g: Game, f: AirFleet): AirFleet | null => (f.escorts === undefined ? null : g.airFleets.find((x) => x.id === f.escorts && x.nat === f.nat && x.bmr > 0) ?? null);

export function groupRow(g: Game, f: AirFleet, viewer: string | null = g.human): GroupRow {
  const R = airRange(f, g);
  const kind = kindOf(f);
  const away = f.flew === g.turn && isOffensive(f.mission);
  const with_ = escorted(g, f);
  const place = placeOf(g, kindFlown(f.mission), f.target, viewer);
  const deck = afloat(f), at = groupAt(g, f);
  const fleet = deck ? carrierOf(g, f) : f.ashore !== undefined ? g.fleets.find((c) => c.id === f.ashore) ?? null : null;
  const own = f.nat === viewer;
  // (On its way to another field by stages, ferry.ts: out of the fight until it gets there. Only that switch writes `way`.)
  // (The place with its article, as the campaign's line says it: "Ordered against the Midlands". The UI director,
  // 25 November 2026, 14:06Z.)
  const doing = f.way !== undefined ? (own ? `${wayWords(g, f)}: out of the fight until it gets there` : 'On its way to another airfield: out of the fight')
    : away ? `Flew against ${placeName(place)} this month: back ${deck ? 'aboard' : 'at its airfield'} next month`
    : isOffensive(f.mission) ? `Ordered against ${placeName(place)}: flies when the month ends`
    : f.mission === 'rest' ? 'Resting and refitting'
    // (Release 2, the sea patrol: the share of the waters it reaches that its side's patrols watch.)
    : f.mission === 'seapatrol' && g.seas[f.target] ? patrolDoing(g, f, f.target)
    : kind === 'bmr' ? (deck ? 'Ranged on deck, ready for a strike' : 'Supporting the armies, and ready for a raid')
    // (Ground support, air.json 'close': it is held for the armies and meets no raid, so its row must not say it is on patrol.)
    : f.close && !deck ? 'On ground support: on call over its own side\'s battles within its reach, and meets no raid'
    : with_ ? `On patrol, and sticking with the ${with_.name} as its escort`
    : deck ? (fleet && !atSea(fleet) ? `In harbour at ${placeName(fleet.port)}: on patrol over its fleet` : 'On patrol over its fleet: meets any raid on the ships in its sea') : 'On patrol: meets any raid within its reach';
  return {
    id: f.id, nat: f.nat, name: f.name, command: f.parent && f.parent !== f.name ? f.parent : '', kind,
    ftr: f.ftr, bmr: f.bmr, aircraft: f.ftr + f.bmr, est: f.est.ftr + f.est.bmr, ready: f.ready,
    leader: f.leader?.name ?? null, level: f.leader?.level ?? 0, base: deck ? -1 : f.base, baseName: homeName(g, f),
    port: deck && fleet && !atSea(fleet) ? fleet.base : -1, ashore: f.ashore ?? -1, fleetName: f.ashore !== undefined ? fleet?.name ?? '' : '',
    where: deck ? `Aboard the ${homeName(g, f)}` : f.ashore !== undefined ? `Ashore at ${placeName(g.regions[f.base].name)}` : `At ${placeName(g.regions[f.base].name)}`,
    wingMove: !own ? null : deck ? { order: 'ashore', price: PRICE_ASHORE, why: whyNoShore(g, f) } : f.ashore !== undefined ? { order: 'aboard', price: PRICE_ABOARD, why: whyNoDeck(g, f) } : null,
    ship: f.ship ?? -1, sea: at.sea, x: at.x, y: at.y,
    doing, job: away || isOffensive(f.mission) ? 'raid' : f.mission === 'rest' ? 'rest' : with_ ? 'escort' : 'patrol',
    escorts: with_ ? with_.id : -1, flew: f.flew === g.turn, scouted: f.scouted === g.turn,
    range: kind === 'bmr' ? R.bmr : kind === 'ftr' ? R.ftr : Math.max(R.ftr, R.bmr), crowd: crowding(g, f), mine: f.nat === viewer,
    ...pilotWords(g, f), schools: own ? schoolWords(g, f) : '',
    // Weather (ruling 127, 143): a grounded group's card says why, and next month's chances.
    ...(grounded(g, f) ? { sky: [whyNotSky(g, f, -1)!, forecastLine(g, zoneOfGroup(g, f), g.turn + 1, 'Next month')] } : wrecked(g, f) && f.way === undefined ? { sky: [whyNotField(g, f)!] } : {}),
    ...(f.way !== undefined && own ? { way: { to: f.way.to, toName: g.regions[f.way.to]?.name ?? '', left: f.way.sea ? 0 : f.way.stops.length, sea: !!f.way.sea } } : {}),
    // (Release 2: the role beside the name where the name does not say it, the counter's short form, the earned name and colours.)
    ...airWords(g, f),
  };
}

/**
 * A group's words for the card and the counter (release 2, airnames.ts and airnicks.ts): read by no rule. Each word
 * belongs to its own switch: the role word to 'airNames' (roleWord), and the called name, which sets an earned name
 * beside the group's own in its arm's manner, to 'nicknames', as the names corps earn are shown only under that switch.
 */
export function airWords(g: Game, f: AirFleet): { role?: string; short: string; called?: string; colours?: { scheme: number; tier: number; since: number } } {
  const role = kindOf(f) === 'mixed' ? undefined : roleWord(g, f);
  const called = f.nick && nicknamesOn(g) ? calledName(f) : undefined;
  return { ...(role ? { role } : {}), short: shortName(f), ...(called ? { called } : {}), ...(f.colours ? { colours: { ...f.colours } } : {}) };
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
  /** The note's last sentence by itself, or '': carriers lying in the harbour here keep their aircraft aboard (ruling 176(b)). */
  aboard: string;
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
  if (!field) return { region, name: r.name, field: null, note: `There is no airfield at ${placeName(r.name)}: no aircraft can be based here.`, build, aboard: '', hidden: false, rows: [] };
  const hidden = field.groups < 0;
  const rows = hidden ? [] : g.airFleets.filter((f) => f.base === region && !afloat(f) && flying(f)).map((f) => groupRow(g, f, viewer));
  const used = rows.reduce((s, x) => s + x.aircraft, 0);
  // Carriers lying in the harbour here keep their aircraft aboard: said, so that nobody looks for them on the field (ruling 176(b)).
  const inPort = hidden ? [] : g.fleets.filter((c) => c.base === region && !atSea(c) && c.ships.cv > 0 && viewer !== null && allied(g, c.nat, viewer) && g.airFleets.some((w) => w.ship === c.id && w.ftr + w.bmr > 0));
  const aboard = inPort.length ? ` The aircraft of the ${inPort.length === 1 ? inPort[0].name : inPort.map((c) => c.name).join(' and the ')}, in harbour here, are aboard ${inPort.length === 1 ? 'its' : 'their'} carriers.` : '';
  const note = `${field.word}: it has room for ${num(field.holds)} aircraft${hidden ? '. What flies from here is not known: scout it from the air, or put an agent on watch'
    : `, and ${used > 0 ? `${num(used)} are based here` : 'none are based here'}${field.crowded ? `. It is crowded: only ${pct(field.holds / used)} of each group can fly` : ''}`}.${aboard}`;
  return { region, name: r.name, field, note, build, aboard: aboard.trim(), hidden, rows };
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
  const R = airRange(f, g), kind = kindOf(f);
  const deck = carrierOf(g, f), home = deck ? `the ${deck.name}` : placeName(g.regions[f.base].name);
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
    // The provinces whose own ground it guards (ruling 225): its country's, and an ally's where it is based there or next door by land.
    const under = g.regions.filter((r) => guardsLand(g, f, r.id, r.ctrl) && cover(kmTo(g, f, r.id), R.ftr) > 0).length;
    out.push(deck
      ? {
        key: 'patrol', label: 'Cover the fleet', on: idle && !with_, why: gone,
        forecast: `Meets any raid on the ships in the sea its fleet is in or the harbour it lies in, and any raid on troops on coasts within ${num(R.ftr)} km of it. It also keeps the enemy's coastal bombers off the fleet month by month. About ${num(f.ftr)} fighters can go up.`,
      }
      : {
        key: 'patrol', label: 'Patrol the sky', on: idle && !with_ && !f.close, why: gone,
        forecast: `Meets any raid within ${num(R.ftr)} km of ${home}, at full effect to ${far}: on troops and ships anywhere in that reach, and on the towns and works of ${plural(under, 'province')} it guards. About ${num(f.ftr * crowding(g, f))} fighters can go up.`,
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
  // The sea patrol against submarines (release 2, Units decide, once it is live; seapatrol.ts): a land bomber
  // group watches one sea of its side's convoys and sea lanes all month, and flies nothing else. (Each order says what
  // its side last saw there, with the month, through the panel's one function, the sight its staff counts from
  // (seasight.ts boatsKnown and airKnown; the design director's ruling of 24 November 2026 17:54Z, Q4 (a)): the boats
  // last seen there, and where its own side's boats have met the enemy's aircraft there, what they met. With the
  // player's fog of war lifted, both show the truth.)
  if (kind === 'bmr' && !deck) for (const z of patrolSeas(g, f)) {
    const w = watchWith(g, f, z), sea = g.seas[z];
    const met = airKnown(g, f.nat, z) ? ` ${airMetWords(g, f.nat, z)}.` : '';
    out.push({
      key: `sea:${z}`, label: `Patrol ${seaWhat(sea.name)} against submarines`, on: !away && f.mission === 'seapatrol' && f.target === z, why: gone,
      forecast: `${patrolDoing(g, f, z, w)}. Its aircraft attack the boats they find there${aswKit(g, f.nat).radar && aswKit(g, f.nat).asdic ? ', by night and in fog too, with their radar' : ', and keep the rest down; without sea-search radar and depth charges they sink few'}. While it patrols it flies no raid, meets no raid and escorts no bomber, and regains ${pct(AIR.ready.flying)} readiness a month in place of ${pct(AIR.ready.support)}. ${seaSeenWords(g, f.nat, z)}.${met}`,
    });
  }
  // Close air support (Brian's ask of 2026-11-18, air.json 'close'): a land fighter group flies over its own side's
  // battles in reach, in place of meeting raids and escorting.
  if (closeRules(g) && kind !== 'bmr' && !deck) out.push({
    key: 'close', label: 'Ground support', on: idle && !!f.close, why: gone,
    forecast: `Flies over your own battles within ${num(R.ftr)} km of ${home}, at full effect to ${far}, strafing and bombing the enemy's troops: one battle a week, and only while its readiness is ${pct(CLOSE.least)} or more. Each battle costs it ${pct(CLOSE.ready)} readiness and some of its aircraft, twice as many where the enemy holds the sky. While it does this it is off air defence: it meets no raid and escorts no bomber.`,
  });
  const R2 = AIR.ready;
  out.push({ key: 'rest', label: 'Rest and refit', on: f.mission === 'rest', why: null, forecast: deck
    ? `Stands down: it regains ${pct(R2.rest)} readiness a month in place of ${pct(R2.support)}. It does not fly, and does not meet raids. New aircraft come aboard only while the fleet lies in harbour.`
    : `Stands down: it regains ${pct(R2.rest)} readiness a month in place of ${pct(R2.support)}, and gets replacement aircraft first. It does not fly, and does not meet raids.` });
  return out;
}

/**
 * Why a group on a standing bombing campaign has no raid of its own to pick (the playtester's finding 4 of 1.5.1; the UI
 * director's words, 25 November 2026, 05:45Z): it flies with the campaign by night or by day, and the standing order
 * that frees it is named by its button and its row as the group's page shows them ('Support the armies', or for a
 * carrier wing 'Stand ready for a strike'). Null for any other group.
 */
export function campaignWhy(g: Game, id: number, nat: string | null = g.human): string | null {
  const f = mine(g, id, nat);
  if (!f || f.flew === g.turn || (f.mission !== 'night' && f.mission !== 'bomb')) return null;
  const place = placeOf(g, 'bomb', f.target, nat);
  const free = groupJobs(g, id, nat).find((j) => j.key === 'patrol');
  if (!place || !free) return null;
  return `It flies with the ${f.mission === 'night' ? 'night' : 'day'} campaign over ${placeName(place)}. To give it a raid of its own, first press GIVE THIS ORDER on '${free.label}', which takes it out of the campaign.`;
}

/** Gives one of a nation's air groups its standing order: 'patrol', 'escort:<bomber group id>', 'close' (ground support, where its switch is on), 'sea:<sea id>' (a sea patrol against submarines, once the patrol is live) or 'rest'. */
export function setGroupJob(g: Game, id: number, key: string, nat: string | null = g.human): boolean {
  const f = mine(g, id, nat);
  const job = f ? groupJobs(g, id, nat).find((j) => j.key === key) : undefined;
  if (!f || !job || job.why) return false;
  if (key === 'rest') { delete f.escorts; delete f.close; return setMission(g, f, 'rest'); }
  if (key === 'close') { delete f.escorts; f.close = true; return setMission(g, f, 'support'); }
  delete f.close;
  if (key.startsWith('sea:')) { delete f.escorts; return setMission(g, f, 'seapatrol', Number(key.split(':')[1])); }
  if (key === 'patrol') delete f.escorts; else f.escorts = Number(key.split(':')[1]);
  return setMission(g, f, 'support');
}

/**
 * Where a group could move to: the airfields in reach, nearest first, with the room each has; then, for a land group,
 * the friendly airfields across an ocean, where it goes by sea (`sea`: one group of a nation a month, ruling 133(b)).
 */
export interface BaseOption { region: number; name: string; km: number; word: string; holds: number; free: number; why: string | null; sea?: boolean }

export function groupBases(g: Game, id: number, nat: string | null = g.human): BaseOption[] {
  const f = mine(g, id, nat);
  if (!f) return [];
  const out: BaseOption[] = [];
  // A carrier wing goes where its fleet goes: the fields it could be sent ashore to are the one of the harbour its
  // fleet lies in, or the friendly ones on the sea it is in (ruling 175(b)).
  if (afloat(f)) {
    const c = carrierOf(g, f);
    for (const id of c ? shoresOf(g, c) : []) {
      const r = g.regions[id], holds = airfieldsHold(g, id);
      if (holds <= 0 || !allied(g, f.nat, r.ctrl)) continue;
      const there = g.airFleets.reduce((s, x) => s + (x.base === id && !afloat(x) ? x.ftr + x.bmr : 0), 0);
      out.push({ region: id, name: r.name, km: 0, word: fieldWord(holds).word, holds, free: Math.max(0, holds - there), why: whyNotAshore(g, f, r) });
    }
    return out.sort((a, b) => (a.why ? 1 : 0) - (b.why ? 1 : 0) || b.free - a.free || a.region - b.region);
  }
  for (const r of g.regions) {
    const holds = airfieldsHold(g, r.id);
    // (A field our side's siege has the use of, in a province the enemy still holds, is listed as the map offers it:
    // the code checker on 85.1, Helsinki under a Soviet siege.)
    if (holds <= 0 || r.id === f.base || (!allied(g, f.nat, r.ctrl) && !airfieldOpenTo(g, f.nat, r))) continue;
    const d = kmTo(g, f, r.id), sea = overOcean(g, f, r);
    if (!sea && d > AIR.ferry) continue;
    const there = g.airFleets.reduce((s, x) => s + (x.base === r.id && !afloat(x) ? x.ftr + x.bmr : 0), 0);
    const o: BaseOption = { region: r.id, name: r.name, km: d, word: fieldWord(holds).word, holds, free: Math.max(0, holds - there), why: sea ? whyNotShipAir(g, f, r) : whyNotBase(g, f, r) };
    if (sea) o.sea = true;
    out.push(o);
  }
  return out.sort((a, b) => (a.sea ? 1 : 0) - (b.sea ? 1 : 0) || a.km - b.km);
}

/** The fleets a carrier wing ashore could go back aboard: those it can board now first. Empty for any other group. */
export function groupDecks(g: Game, id: number, nat: string | null = g.human): DeckOption[] {
  const f = mine(g, id, nat);
  return f ? decksFor(g, f) : [];
}

/** What going back aboard costs, said by the order before it is given. */
const PRICE_ABOARD = 'Going aboard costs it some readiness for a month, as a move between airfields does.';

// ---- Planning a raid ------------------------------------------------------------------------------------

/** Why this group cannot go on this raid, or null if it can. */
function whyNotSeat(g: Game, f: AirFleet, kind: RaidKind, target: number): string | null {
  if (!flying(f)) return 'It has no aircraft fit to fly.';
  if (kind === 'recon') {
    const r = g.regions[target];
    if (!r) return 'Pick a province.';
    if (f.scouted === g.turn) return 'It has flown its reconnaissance this month.';
    // Weather (ruling 127): no camera flies from or over a closed sky.
    if (weatherOn(g)) { const why = whyNotSky(g, f, zoneOfRegion(g, target)); if (why) return why; }
    // (Airfields as targets: nothing takes off from a shut runway, the camera aircraft included.)
    if (g.flags.fields === 1 && fieldRules(g)) { const why = whyNotField(g, f); if (why) return why; }
    if (!atWar(g, f.nat, r.ctrl)) return 'Only ground held by a nation you are at war with is photographed.';
    const R = airRange(f, g);
    return kmTo(g, f, target) <= (f.bmr > 0 ? R.bmr : R.ftr) ? null : 'It is out of range.';
  }
  if (f.flew === g.turn) return 'It has flown its raid this month.';
  if (f.mission === 'rest') return 'It is resting and refitting.';
  if (isOffensive(f.mission)) return 'It already has orders for the month.';
  if (f.mission === 'seapatrol') return 'It is patrolling the sea against submarines this month.';
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

/** True for an enemy town worth the bombs: one this nation's side stands round, or one its armies stand beside. */
function townWanted(g: Game, nat: string, region: number): boolean {
  const r = g.regions[region];
  if (!siegeRules(g) || !atWar(g, nat, r.ctrl) || !townHeld(g, r)) return false;
  const by = besiegerOf(g, r);
  if (by) return allied(g, nat, by);
  return r.nb.some((j) => g.regions[j].units.some((u) => !u.dead && allied(g, u.nat, nat)));
}

function reachable(g: Game, nat: string, kind: RaidKind): Map<number, AirFleet[]> {
  const out = new Map<number, AirFleet[]>();
  const fleets = able(g, nat);
  const needs = kindRow(kind).needs;
  const sight = kind === 'ground' || kind === 'line' || kind === 'recon' ? sightOf(g, nat) : null;
  for (const t of targetIds(g, kind, nat)) {
    if ((kind === 'ground' || kind === 'line') && !troopsSeen(g, nat, t, sight!)) continue;
    if (kind === 'town' && !townWanted(g, nat, t)) continue;
    if (kind === 'airfield' && (!atWar(g, nat, g.regions[t].ctrl) || airfieldsHold(g, t) <= 0)) continue;
    // Ground in full view has something to show a camera only where an army there is not identified, or known from an old report.
    if (kind === 'recon' && sight![t] === SIGHT.full && !unidentified(g, nat, t)) continue;
    const can = fleets.filter((f) => whyNotSeat(g, f, kind, t) === null);
    // A raid that needs bombers is not offered where only fighters reach.
    if (!can.length || (needs === 'bmr' && !can.some((f) => f.bmr > 0))) continue;
    out.set(t, can);
  }
  return out;
}

/** Weather (ruling 127): why nothing can fly at all when every group that could is under a closed sky, or null. */
function allGrounded(g: Game, nat: string): string | null {
  if (!weatherOn(g)) return null;
  const fs = able(g, nat).filter((f) => f.flew !== g.turn);
  if (!fs.length || !fs.every((f) => grounded(g, f))) return null;
  const zs = [...new Set(fs.map((f) => zoneOfGroup(g, f)))];
  return zs.length === 1 ? `Grounded this month: the sky is closed over ${zoneName(zs[0])}.` : 'Grounded this month: the sky is closed over every airfield you fly from.';
}

/** The planner's menu: every kind of raid, with the number of targets a group could still fly against this month. */
export function raidKinds(g: Game, nat: string | null = g.human): RaidKindView[] {
  if (!airWar(g) || nat === null) return [];
  const peace = !g.nations[nat].atWar.length;
  // (Airfields as targets: the airfield raid is on the menu only with its switch on.)
  return AIRWAR.raids.kinds.filter((k) => k.key !== 'airfield' || fieldRules(g)).map((k) => {
    const n = peace ? 0 : reachable(g, nat, k.key as RaidKind).size;
    return { key: k.key as RaidKind, label: k.label, short: k.short, blurb: k.blurb, targets: n, why: peace ? 'The nation is at peace.' : n ? null : allGrounded(g, nat) ?? 'No group that can still fly this month has a target in reach.' };
  });
}

/** What the one row for a bombing raid says of itself (the design director's ruling 117, point 6). */
export const BOMB_EITHER = 'Bombers go for the factories and rail yards of one province, by day or by night: the card weighs both and asks which. Fighters in range can go with them by day.';

/**
 * The planner's menu with a bombing raid named once (ruling 117, point 6): "Bomb by night" opens the same card as
 * "Bomb factories and railways", so the list has one row for the two, its line says by day or by night, and the
 * card asks which. The row is offered where either way has a target.
 */
export function raidKindsOne(g: Game, nat: string | null = g.human): RaidKindView[] {
  const all = raidKinds(g, nat);
  const night = all.find((k) => k.key === 'night');
  return all.filter((k) => k.key !== 'night').map((k) => (k.key !== 'bomb' || !night ? k
    : { ...k, blurb: BOMB_EITHER, targets: Math.max(k.targets, night.targets), why: k.targets || night.targets ? null : k.why }));
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
  /**
   * The distance in words, as a list of targets prints it (ruling 133(g)): that of the nearest group able to fly the
   * raid itself, '1,240 km'; and never '0 km', which reads as a fault: 'carriers off this coast', 'based at the target'.
   */
  far: string;
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
  // Fog on raids (airfog.ts, its switch on): out of the side's sight its guns and the fighters in reach are as last seen.
  const k = raidFogOn(g) && g.regions[t] ? skyKnown(g, nat, t, sight) : null;
  const hide = k && !k.live ? fogBits(k) : null;
  // (The raid row, the UI director's section B of 25 Nov 2026 19:10Z: the built guns first, the standing light guns every
  // target has named apart; out of sight the built guns as last seen, and the standing ones never hidden.)
  const gunsBit = (): string => {
    const S = num(standingGuns()), built = flakGuns(gunsAt(g, t));
    // (Out of sight, her 20:08Z marks, item 6: the built guns as last seen, counted as the card counts them.)
    if (hide && k) {
      if (k.guns < 0) return `built guns not known and ${S} light`;
      const was = flakGuns(k.guns), when = seenAgo(k.ago);
      return was.all ? `about ${num(was.all)} guns built (${num(was.heavy)} heavy) when ${when}, and ${S} light` : `${S} light guns, none built when ${when}`;
    }
    return built.all ? `${num(built.all)} guns built (${num(built.heavy)} heavy) and ${S} light` : `${S} light guns`;
  };
  // City defences: the searchlights follow the guns, as they truly are even where the fog hedges the guns: the design
  // director, 25 Nov 2026 19:44Z, ruled radar and searchlights known truth reads for the joint sight form (audit 28),
  // never one true and one none. Switch off: lightsAt reads none.
  const lightsBit = (): string => (lightsAt(g, t) ? `level ${lightsAt(g, t)} searchlights` : '');
  if (kind === 'harbour') {
    const there = shipsInPort(g, t, nat);
    const seen = there.filter((f) => fleetKnown(g, f, nat) === 'exact');
    const bits = seen.map((f) => `${f.name}: ${shipList(f.ships)}`);
    if (seen.length < there.length) bits.push(seen.length ? 'more ships not identified' : 'Warships in harbour, not identified');
    if (gunsBit()) bits.push(gunsBit());
    return bits.join(', ');
  }
  const r = g.regions[t];
  if (kind === 'town') {
    const v = siegeView(g, t)!;
    const who = sight[t] === SIGHT.none && !v.by ? 'its garrison not known' : v.inside ? `${plural(v.inside, 'corps', 'corps')} inside${v.guard > 0 ? ' with its guard' : ''}` : 'held by its guard alone';
    return `${v.sizeWord}: ${who}. Troops inside fight ${Math.round((v.bonus - 1) * 100)}% stronger${townWorksPhrase(g, r)}${v.broken > 0 ? `, ${Math.round(v.broken * 100)}% of its works broken` : ''}${v.by ? ', under siege' : ''}${gunsBit() ? `, ${gunsBit()}` : ''}${lightsBit() ? `, ${lightsBit()}` : ''}`;
  }
  if (kind === 'bomb' || kind === 'night') {
    // City defences: the searchlights after the guns, with the switch on (lightsAt reads none with it off).
    const bits = [r.fac > 0 ? plural(r.fac, 'factory', 'factories') : '', onNet(t) && !railCut(g, t) ? (railsAt(t).length >= 3 ? 'a railway junction' : 'a railway') : '', gunsBit(), lightsBit(), hide ? hide.fighters : ''];
    return bits.filter(Boolean).join(', ');
  }
  if (kind === 'line') {
    const dug = r.units.filter((u) => !u.dead && atWar(g, nat, u.nat) && worksOf(u) > 0);
    const most = Math.max(0, ...dug.map(worksOf));
    return `${plural(dug.length, 'corps', 'corps')} dug in, in works of ${monthsOf(most)}${most >= DEFENCE.entrench.months ? ', finished' : ''}`;
  }
  if (kind === 'ground') {
    const foes = r.units.filter((u) => !u.dead && atWar(g, nat, u.nat));
    const known = foes.every((c) => corpsKnown(g, c, nat).level === 'exact');
    return sight[t] === SIGHT.full && known ? `${plural(foes.length, 'corps', 'corps')}, ${plural(foes.reduce((s, c) => s + c.divs, 0), 'division')}${foes.some((c) => c.dug) ? ', dug in' : ''}`
      : sight[t] === SIGHT.full ? `${plural(foes.length, 'corps', 'corps')} seen, not identified${foes.some((c) => c.dug) ? ', dug in' : ''}` : `${plural(foes.length, 'corps', 'corps')} seen`;
  }
  if (kind === 'airfield') {
    // (One fog gate, the design director's 06:21Z: the field's state only where the planner's nation knows it.)
    const v = fieldKnown(g, nat, t, sight) ? fieldView(g, t, null) : null;
    const seen = sight[t] !== SIGHT.none;
    const groups = g.airFleets.filter((f) => f.base === t && !afloat(f) && f.ftr + f.bmr > 0 && atWar(g, f.nat, nat));
    const planesThere = groups.reduce((s, f) => s + f.ftr + f.bmr, 0);
    const up = fightersOver(g, t, nat);
    const upBit = hide ? hide.fighters : up >= 1 ? `about ${num(up)} fighters can come up` : 'no fighters in reach';
    return [v ? `${v.tier === 'strip' ? 'Field strip' : 'Concrete airfield'}${v.shut ? ', shut' : ''}${v.burned ? `, ${plural(v.burned, 'level')} burned` : ''}` : 'An airfield',
      seen ? (groups.length ? `${plural(groups.length, 'air group')} based there, about ${num(planesThere)} aircraft` : 'no air group seen there') : 'what is based there is not known',
      upBit].join(', ');
  }
  if (kind === 'superiority') {
    const based = g.airFleets.filter((f) => f.base === t && !afloat(f) && flying(f) && atWar(g, f.nat, nat)).length;
    const up = fightersOver(g, t, nat);
    return [based ? `${plural(based, 'air group')} based there` : airfieldsHold(g, t) > 0 ? 'an empty airfield' : 'no airfield', hide ? hide.fighters : up >= 1 ? `about ${num(up)} fighters can come up` : 'no fighters in reach'].join(', ');
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
  const needs = kindRow(kind).needs;
  const out: RaidTarget[] = [];
  for (const [t, can] of reachable(g, nat, kind)) {
    if (lead >= 0 && !can.some((f) => f.id === lead)) continue;
    const quarry = kind === 'fleet' ? g.fleets.find((x) => x.id === t)! : null;
    const zone = quarry ? quarry.zone : kind === 'strike' ? t : -1;
    const p = sea ? g.seas[zone] : g.regions[t];
    // With a group leading, the distance is its own; otherwise that of the nearest group able to fly the raid itself
    // (ruling 133(g)): where the raid needs bombers, fighters that could only go with it do not set the distance.
    const flies = can.filter((f) => (lead < 0 ? needs !== 'bmr' || f.bmr > 0 : f.id === lead));
    const from = (flies.length ? flies : can).reduce((a, b) => (distTo(g, b, kind, t) < distTo(g, a, kind, t) ? b : a));
    const d = distTo(g, from, kind, t);
    let far = Math.round(d) > 0 ? `${num(d)} km` : sea ? (from.ship !== undefined ? 'carriers in these waters' : 'based on these shores') : from.ship !== undefined ? 'carriers off this coast' : 'based at the target';
    // Weather (ruling 127, 143): a target under cloud says so after its distance.
    if (weatherOn(g) && skyOf(g, skyZone(g, kind as AirMission, t)) === 'cloud') far += ' · cloud';
    const known = quarry && fleetKnown(g, quarry, nat) !== 'exact' ? 'report' : 'exact';
    const name = kind === 'fleet' ? placeOf(g, kind, t, nat).replace(/^the /, '') : p.name;
    out.push({ id: t, sea, zone, known, name, holder: quarry ? quarry.nat : sea ? '' : g.regions[t].ctrl, x: p.x, y: p.y, km: d, far, groups: can.length, what: whatIsThere(g, nat, kind, t, sight) });
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
export interface RaidGuns {
  level: number; loss: number; lossN: number; stop: number;
  /** Where ships' own guns fire too (a strike on ships): at sea, or in a harbour beside the guns ashore. Screen only. */
  ships?: 'sea' | 'harbour';
}

/**
 * Whose guns bring down the bombers the card counts lost to guns ("about 3 to ..."), each share by its own name: the UI
 * director's rider 3 table and rider 4 (22 November, 09:01Z and 12:53Z), for everyone, every switch. Over land, release
 * 1's words: "light flak", or "flak and level 2 guns". A strike on a fleet at sea: "the ships' guns". A harbour, where
 * the ships' guns fire beside those ashore: "the ships' guns and light flak", or "the ships' guns, flak and level 2
 * guns". (Release 1 called the ships' guns "light flak". The words are screen text only: no word of them is saved.)
 */
export function gunsTo(guns: RaidGuns): string {
  if (guns.ships === 'sea') return "the ships' guns";
  if (guns.ships === 'harbour') return guns.level ? `the ships' guns, flak and level ${guns.level} guns` : "the ships' guns and light flak";
  return guns.level ? `flak and level ${guns.level} guns` : 'light flak';
}

/**
 * The same shares in the forecast's "of them to ..." form (rider 3's table), for the 3D game's raid card, whose one line
 * under the bombers lost serves every raid, strikes on ships included: "of them to light flak" or "of them to flak and
 * guns" over land, as release 1 had it; "of them to the ships' guns" at sea; "of them to the ships' guns and light flak"
 * or "of them to the ships' guns, flak and guns" in a harbour. (The browser's day and night rows serve bombing raids
 * only, which never carry ships, and keep release 1's words: rider 4.)
 */
export function gunsOfThem(guns: RaidGuns): string {
  if (guns.ships === 'sea') return "of them to the ships' guns";
  if (guns.ships === 'harbour') return guns.level ? "of them to the ships' guns, flak and guns" : "of them to the ships' guns and light flak";
  return guns.level ? 'of them to flak and guns' : 'of them to light flak';
}

export interface RaidPlan {
  /** The fighter types at the raid's height and whose aircraft are stronger (fighters.ts): only with the switch 'Fighter types' on. */
  fighterTypes?: { types: string; edge: string | null; pilots?: string };
  kind: RaidKind;
  target: number;
  place: string;
  title: string;
  /** Why it cannot be flown as picked, or null. */
  why: string | null;
  /**
   * A fight for the sky sent with too few fighters over the target to hold any sky (release 1.5.2, airwar.json
   * ai.back.small): the card's line that says so, with the figures. Left out otherwise.
   */
  tooFew?: string;
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
  /**
   * The numbers behind those sentences, for a card that draws them rather than reads them (Version 78). Each is there
   * only where this kind of raid can do it; shares and chances are 0 to 1.
   * `output`: the share of the place's output stopped for a month, and `national`: that as a share of the holder's
   * whole production. `rail`: the chance of cutting the railway. `factory`: the chance of wrecking a factory outright.
   * `odds`: fighter odds going in (9 or more reads "overwhelming"). `readiness`: the readiness the air groups based
   * there lose. `ships`: the share of the warships at sea there that could be lost. `hits`: the bombs and torpedoes
   * that should hit, and `sunk`: the ships of each class they should sink (the whole part is sure, the rest a chance).
   * `works`: the months taken off the fieldworks, and `worksLeft`: the months of work left in them.
   * `organisation`: the organisation the troops there lose. `town`: a raid on a town, as the siege rules record it
   * (the percent of its works broken down, what the garrison's organisation loses, the percent of the guard out of action).
   */
  figures: RaidFigures;
  /**
   * The target's guns (the player's ask of 2026-11-16, "show defenses meaning anticipated flak and AA guns"): the level
   * of the anti-aircraft guns built round it (0 where there are none), the bombers the flak and those guns should
   * bring down (a part of `bomberLossN`, the rest falling to fighters), and the share of the bombing the guns stop.
   */
  guns: RaidGuns;
  /** What flying it together is worth beside sending each bomber group alone, or '' where only one goes. */
  massed: string;
  /** What the raid costs at home: fighter groups taken off patrol for the rest of the month. */
  offPatrol: string;
  /** Bombing the line: the months the raid should take off the fieldworks there. */
  wear?: number;
  /** Weather (ruling 127, 143): the card's one line for the sky over the target. Only in a campaign with weather. */
  sky?: string;
  /**
   * Fog on raids (airfog.ts; only with its switch on, and only for a target on land): what the planner's side knows of
   * the target's defences, and the forecast as a band. Out of its sight the plan's own figures are the band's high end.
   */
  fog?: RaidFog;
  /**
   * A raid on troops with the strafers' rule on (release 2, strafers.ts), on the player's own card only: the section
   * LIKELY TO GO DOWN AND STRAFE, the UI director's words of 15:04Z. Its names are the groups the card's own forecast
   * names (forecastAir's strafers, strafersExpected's ids from the same call), so its words and its figures cannot part.
   */
  strafe?: StrafeSection;
}

/**
 * Fog on raids: what one side knows of a raid's target, and its forecast as a band (the design director's ruling of
 * 24 November 22:10Z). Each pair is [the low end, the high end]: the low end counts only the defence known for sure,
 * the high end every enemy fighter last seen in reach rising. In sight the two are one figure, the truth.
 */
export interface RaidFog {
  /** In full sight now; and in sight by the side's own reconnaissance flight, this month or last. */
  live: boolean;
  scouted: boolean;
  /** Months since the place was last seen: 0 in sight, -1 never seen. */
  ago: number;
  /** The level of its guns as known, 0 to 3; -1 where never seen. */
  gunLevel: number;
  /**
   * The fighters in reach as last seen (airfog.ts SkyKnown.fighters), -1 where never seen: a reader asks the sighting
   * itself, never the words of a line (the UI director, 25 Nov 2026 21:22Z, case b, her code note).
   */
  fightersSeen: number;
  /** The card's lines: "Fighters: not known", "Guns: level 2, seen three months ago", "Damage: none, in sight" ('' where towns take no lasting damage). */
  fighters: string;
  guns: string;
  damage: string;
  /** The share lost of the bombers (of the fighters, where no bombers fly), the bombers and fighters lost, the chance of success, the enemy squadrons up. */
  loss: [number, number];
  bomberLossN: [number, number];
  fighterLossN: [number, number];
  success: [number, number];
  squadrons: [number, number];
  /** "Losses forecast: 3 to 9% (not scouted)", or one figure: "Losses forecast: 5% (scouted)". */
  band: string;
}

export interface RaidFigures {
  output?: number; national?: number; rail?: number; factory?: number;
  odds?: number; readiness?: number;
  /** Airfields as targets: what the bombs should do to the field. */
  field?: { shut: boolean; levels: number; ground: number; grounded: number; room: number; built: number };
  ships?: number; hits?: number; sunk?: Record<string, number>;
  works?: number; worksLeft?: number;
  organisation?: number;
  town?: TownHit;
  /** Damaged cities: the percent of the town the bombs should leave in ruins, until it is repaired. */
  city?: number;
}

const VERDICT: [number, string][] = [[0.8, 'Very likely to succeed'], [0.6, 'Likely to succeed'], [0.4, 'It could go either way'], [0.2, 'Likely to fail'], [0, 'Very likely to fail']];
/**
 * The same chance, worded for the fight alone, for a fight for the sky sent with too few fighters to hold any sky
 * (release 1.5.2, airwar.json ai.back.small): its chance of success is the chance the fight costs the enemy more than
 * it costs you, and the sky itself cannot be won on the numbers sent, so the words claim the fight and no more.
 */
const FIGHT_VERDICT: [number, string][] = [[0.8, 'Very likely to win the air fight'], [0.6, 'Likely to win the air fight'], [0.4, 'The air fight could go either way'], [0.2, 'Likely to lose the air fight'], [0, 'Very likely to lose the air fight']];

/** Damaged cities: the forecast's line for the lasting damage the bombs should leave on the town. */
const cityLine = (g: Game, target: number, pts: number): string =>
  `Should leave about ${pts < 1 ? 'under 1' : Math.round(pts)}% more of ${townName(g.regions[target])} in ruins: its factories and its pay fall by as much until its holder repairs it.`;

function effectsOf(g: Game, kind: RaidKind, target: number, fc: ReturnType<typeof forecastAir>, nat: string): string[] {
  const out: string[] = [];
  const place = placeOf(g, kind, target);
  // The atomic project (release 2, atom.ts): a raid on the armed city carries the bomb, and does nothing else there.
  if ((kind === 'bomb' || kind === 'night') && armedAt(g, nat) === target) {
    out.push(strikeWords(g, nat, target));
    return out;
  }
  if (kind === 'bomb' || kind === 'night') {
    const r = g.regions[target];
    out.push(r.fac > 0 ? `Should stop about ${pct(fc.cut)} of ${placeOwns(place)} output for a month: ${tenthPct(fc.nation)} of ${g.nations[r.ctrl].adj} production.` : `There is no industry at ${placeName(place)}: the bombers go for its rail yards.`);
    if (fc.rail) out.push(`About a ${pct(fc.rail)} chance of cutting the railway through ${placeName(place)}, which stays cut until engineers relay it.`);
    if (fc.ruin) out.push(`About a ${pct(fc.ruin)} chance of wrecking a factory outright.`);
    if (fc.city) out.push(cityLine(g, target, fc.city));
  } else if (kind === 'superiority') {
    out.push(`Fighter odds ${fc.odds >= 9 ? 'overwhelming' : `${fc.odds.toFixed(1)} to 1`} going in. The sky is won when your fighters end the month with ${AIR.sky.win} times what the defender can still put up.`);
    if (fc.cut > 0) out.push(`The air groups based at ${placeName(place)} should lose about ${pct(fc.cut)} of their readiness, and aircraft on the ground.`);
    if (fc.field) out.push(...fieldLines(fc.field));
  } else if (kind === 'airfield') {
    if (fc.field) out.push(...fieldLines(fc.field));
    if (!fc.field || (!fc.field.shut && !fc.field.levels && !fc.field.ground)) out.push(`Too few bombers would get through to do ${place} lasting harm: craters are filled in within the day.`);
    out.push(`Fighter odds ${fc.odds >= 9 ? 'overwhelming' : `${fc.odds.toFixed(1)} to 1`} going in: the fighters based there go up to meet you first.`);
  } else if (kind === 'strike') {
    out.push(shippingWords(g, fc.cut));
  } else if (kind === 'fleet' || kind === 'harbour') {
    const sunk = fc.ships, hits = fc.hits ?? 0;
    const word = (c: (typeof CLASSES)[number], n: number): string => (n === 1 ? NAVY.classes[c].one : NAVY.classes[c].many);
    const sure = sunk ? CLASSES.filter((c) => sunk[c] >= 1).map((c) => `${Math.floor(sunk[c])} ${word(c, Math.floor(sunk[c]))}`) : [];
    const maybe = sunk ? CLASSES.filter((c) => sunk[c] % 1 >= 0.15).map((c) => `${pct(sunk[c] % 1)} for ${sunk[c] >= 1 ? 'another' : 'a'} ${word(c, 1)}`) : [];
    out.push(`About ${hits >= 10 ? num(hits) : hits.toFixed(1)} bombs and torpedoes should hit. ${sure.length ? `That should sink ${list(sure)}` : maybe.length ? 'No ship is sure to go down' : 'Too few to sink a ship'}${maybe.length ? `, with a chance of ${list(maybe)}` : ''}. Carriers and battleships are aimed at first, and every ship hit leaves the fleet less fit for sea.`);
    if (sunk && sunk.cv > 0.15) out.push('A carrier that goes down takes most of its aircraft with it.');
  } else if (kind === 'town') {
    if (fc.town) out.push(townHitText(g, g.regions[target], fc.town, 'bombs', false));
    if (fc.town && fc.city) out.push(cityLine(g, target, fc.city));
  } else if (kind === 'line') {
    const most = Math.max(0, ...g.regions[target].units.filter((u) => !u.dead && atWar(g, nat, u.nat)).map(worksOf));
    const took = Math.min(fc.cut, most), left = Math.max(0, most - fc.cut);
    out.push(took >= 0.05
      ? `Should take about ${monthsOf(took)} off the fieldworks at ${placeName(place)}, which would leave ${left < 0.05 ? 'nothing of them' : `${monthsOf(left)} of work in them`}. An army attacking this month meets that much less of a prepared line.`
      : `Too few bombers would get through to break up the works at ${placeName(place)}.`);
    out.push('The men in the works are not hurt, and they dig the works back a month at a time: bomb in the month you attack.');
  } else if (kind === 'ground') {
    const dug = g.regions[target].units.some((u) => !u.dead && u.dug && atWar(g, nat, u.nat));
    out.push(`The troops at ${placeName(place)} should lose about ${pct(fc.cut)} of their organisation${dug ? ' (less where they are dug in)' : ''}, and some of their men and guns. They fight the weaker for it until they have rested: attack while it lasts.`);
  }
  return out;
}

/** Airfields as targets: the field lines of a raid card. */
function fieldLines(x: NonNullable<ReturnType<typeof forecastAir>['field']>): string[] {
  const out: string[] = [];
  if (x.shut) out.push(`Should shut the field until its owner repairs it${x.grounded ? `: about ${num(x.grounded)} aircraft grounded` : ''}.`);
  if (x.ground >= 1) out.push(`About ${num(x.ground)} aircraft destroyed on the ground.`);
  if (x.levels) out.push(`Should burn ${plural(x.levels, 'level')}: it would serve ${num(x.room)} of its ${num(x.built)} aircraft until rebuilt.`);
  return out;
}

/** The numbers `effectsOf` puts into words, read off the same forecast. */
function figuresOf(g: Game, kind: RaidKind, target: number, fc: ReturnType<typeof forecastAir>, nat: string): RaidFigures {
  const f: RaidFigures = {};
  // (The atomic project, release 2: a raid that carries the bomb does nothing else to the city, so it has no figures.)
  if ((kind === 'bomb' || kind === 'night') && armedAt(g, nat) === target) return f;
  if (kind === 'bomb' || kind === 'night') {
    if (g.regions[target].fac > 0) { f.output = fc.cut; f.national = fc.nation; }
    if (fc.rail) f.rail = fc.rail;
    if (fc.ruin) f.factory = fc.ruin;
    if (fc.city) f.city = fc.city;
  } else if (kind === 'superiority') {
    f.odds = fc.odds;
    if (fc.cut > 0) f.readiness = fc.cut;
  } else if (kind === 'strike') {
    f.ships = fc.cut;
  } else if (kind === 'fleet' || kind === 'harbour') {
    f.hits = fc.hits ?? 0;
    if (fc.ships) f.sunk = { ...fc.ships };
  } else if (kind === 'line') {
    const most = Math.max(0, ...g.regions[target].units.filter((u) => !u.dead && atWar(g, nat, u.nat)).map(worksOf));
    f.works = Math.min(fc.cut, most);
    f.worksLeft = Math.max(0, most - fc.cut);
  } else if (kind === 'town') {
    if (fc.town) { f.town = { ...fc.town }; if (fc.city) f.city = fc.city; }
  } else if (kind === 'ground') {
    f.organisation = fc.cut;
  } else if (kind === 'airfield') {
    f.odds = fc.odds;
  }
  if (fc.field) f.field = { ...fc.field };
  return f;
}

/**
 * A raid weighed up for the planner. `ids` are the groups asked to fly it; left out, the staff proposes every bomber
 * group that can reach (for a raid without bombers, the nearest fighter group). Fighter groups ordered to stick with
 * a bomber group that is going go too. Nothing is changed: launchRaid() flies it.
 */
export function planRaid(g: Game, kind: RaidKind, target: number, ids?: number[], nat: string | null = g.human, truth = false): RaidPlan {
  const row = kindRow(kind);
  const place = placeOf(g, kind, target, nat);
  const quarry = kind === 'fleet' ? g.fleets.find((x) => x.id === target) ?? null : null;
  const plan: RaidPlan = {
    kind, target, place, title: `${row.short}: ${place}`, why: null, seats: [], picked: [], bombers: 0, fighters: 0, meets: [], lines: [],
    success: 0, verdict: '', bomberLoss: 0, bomberLossN: 0, fighterLossN: 0, foeLossN: 0, effects: [], figures: {}, guns: { level: 0, loss: 0, lossN: 0, stop: 0 }, massed: '', offPatrol: '',
  };
  if (!airWar(g) || nat === null) { plan.why = 'Raids are planned only in the grand campaign.'; return plan; }
  if (!(kind === 'strike' ? g.seas[target] : kind === 'fleet' ? quarry : g.regions[target])) { plan.why = kind === 'strike' ? 'Pick a sea.' : kind === 'fleet' ? 'Pick a fleet.' : 'Pick a province.'; return plan; }
  if (!g.nations[nat].atWar.length) { plan.why = 'The nation is at peace.'; return plan; }
  // Weather (ruling 127): the sky over the target, known before the order; under a closed sky the card cannot open.
  if (weatherOn(g)) {
    const z = skyZone(g, kind as AirMission, target);
    // (City defences, the switch on: the place takes its article as the news gives it, "Cloud over the Ruhr", as the
    // card's Searchlights line names it; with the switch off the line prints as shipped. The UI director, 26 Nov 2026
    // 04:10Z. The line is made on the shipped place, so its room is judged as shipped and the switch never turns a
    // named place into "the target"; then the place inside it is read by the news's own rule (theWords), which
    // reaches a name inside a phrase, "Clear sky over the harbour of the Cotentin.", "Cloud over the airfield at
    // the Ruhr", where a bare name's article could not: the independent read of 26 Nov 2026.)
    const shipped = skyRaidLine(g, z, place);
    const line = shipped && cityDefences(g) ? shipped.replace(`over ${place}`, () => `over ${theWords(place, 'over ')}`) : shipped;
    if (line) plan.sky = line;
    if (skyOf(g, z) === 'closed') { plan.why = `Closed sky this month: nothing flies over ${zoneName(z)}.`; return plan; }
  }
  if (quarry && fleetKnown(g, quarry, nat) === 'none') { plan.why = 'No fleet has been found there. Search that sea first: a fleet has to be found before it can be struck.'; return plan; }
  if ((kind === 'ground' || kind === 'line') && !troopsSeen(g, nat, target, sightOf(g, nat))) { plan.why = 'No enemy troops have been seen there.'; return plan; }
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
    // (An escort's reach is the raid's: on the far side a fighter flies farther with the bombers than over a beach, ruling 187(a).)
    const d = dist(f), R = airRange(f, g, kind), k = kindOf(f);
    const effect = k === 'bmr' ? (d <= R.bmr ? 1 : 0) : kind === 'night' ? 0 : cover(d, R.ftr);
    const why = whys.get(f.id) ?? null;
    // (A group with no distance to fly is not told '0 km', which reads as a fault: its carriers lie off the target, or its field is at it.)
    const atSeaTarget = kind === 'strike' || kind === 'fleet';
    const here = d > 0 ? '' : atSeaTarget ? (f.ship !== undefined ? 'its carriers are in these waters' : 'based on these shores') : f.ship !== undefined ? 'its carriers lie off this coast' : 'based at the target';
    const note = why ? '' : kind === 'recon' ? `${Math.min(AIRWAR.recon.aircraft, f.ftr + f.bmr)} of its aircraft go` : here ? here : k === 'bmr' ? `${num(d)} km to fly` : effect >= 1 ? `${num(d)} km: well inside its range` : `${num(d)} km: at ${pct(effect)} effect over the target`;
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
    // (Fog on raids: the photographs show the ground, and its fighters, guns and damage, this month and the next.)
    plan.effects = [raidFogOn(g) ? `Every army at ${placeName(place)} and in the provinces beside it is identified, corps by corps, and the fighters, guns and damage there are seen. The sight lasts this month and next month, and is remembered after.` : `Every army at ${placeName(place)} and in the provinces beside it is identified, corps by corps. The report is exact this month and goes stale after.`, `The ${f.name} still flies its raid and keeps its patrol: this takes only ${n} aircraft.`];
    return plan;
  }

  const m = missionOf(kind);
  if (row.needs === 'bmr' && plan.bombers <= 0) { plan.why = 'Pick at least one bomber group: fighters only go as their escort.'; return plan; }
  // Fog on raids (airfog.ts, its switch on): out of the side's sight the card is weighed on what it knows, at both ends
  // of the band, and its figures are the high end's, the defence the side must allow for. (`truth`: a raid being flown
  // is weighed on what is there; it is fought on that in any case, air.ts fly.)
  const fogOn_ = !truth && raidFogOn(g) && !!g.regions[target] && kind !== 'strike' && kind !== 'fleet';
  const sight = fogOn_ ? sightOf(g, nat) : undefined;
  const hid = fogOn_ && fogged(g, nat, m, target, sight);
  const known = hid ? knownDefence(g, nat, target, 'high', sight) : undefined;
  const a = assessAir(g, going, m, target, known);
  if (a.why) { plan.why = a.why; return plan; }
  const fc = forecastAir(g, a, m, target);
  if (fogOn_) {
    const k = skyKnown(g, nat, target, sight);
    const al = hid ? assessAir(g, going, m, target, knownDefence(g, nat, target, 'low', sight)) : a;
    const lo = hid && !al.why ? forecastAir(g, al, m, target) : fc;
    const share = (x: AirForecast): number => (a.B > 0 ? x.bomberLoss : plan.fighters > 0 ? x.ftrLoss / plan.fighters : 0);
    const pair = (x: number, y: number): [number, number] => [Math.min(x, y), Math.max(x, y)];
    const loss = pair(share(lo), share(fc));
    plan.fog = {
      live: k.live, scouted: k.scouted, ago: k.ago, gunLevel: k.guns, fightersSeen: k.fighters,
      ...knownLines(g, k, damageRules(g) && (kind === 'bomb' || kind === 'night' || kind === 'town')),
      loss, bomberLossN: pair(Math.round(lo.bmrLoss), Math.round(fc.bmrLoss)), fighterLossN: pair(Math.round(lo.ftrLoss), Math.round(fc.ftrLoss)),
      success: pair(lo.success, fc.success), squadrons: pair(Math.round(al.squadrons), Math.round(a.squadrons)),
      band: `Losses forecast: ${band(loss[0], loss[1])} (${bandNote(k)})`,
    };
  }
  plan.meets = a.def.map((x) => ({ name: x.f.name, nat: x.f.nat, baseName: homeName(g, x.f), afloat: afloat(x.f), ftr: x.f.ftr, able: Math.round(x.f.ftr * x.w) }));
  plan.lines = a.lines;
  const types = raidCardLines(g, a, m);
  if (types) plan.fighterTypes = types;
  plan.success = fc.success;
  plan.verdict = VERDICT.find(([least]) => fc.success >= least)![1];
  plan.bomberLoss = fc.bomberLoss;
  plan.bomberLossN = Math.round(fc.bmrLoss);
  plan.fighterLossN = Math.round(fc.ftrLoss);
  plan.foeLossN = Math.round(fc.foeLoss);
  plan.effects = effectsOf(g, kind, target, fc, nat);
  // (Release 1.5.2: a fight for the sky sent with fewer fighters over the target than hold a sky, air.json sky.least,
  // cannot win it whatever the odds. The card says so in a line of its own, and words its chance for the fight alone.)
  // (With no fighters going it is a raid on the airfields and no fight for the sky at all: the card stands as it was.
  // A group holding less than one fighter, which the card shows as 0, is sending none.
  // With nobody to come up there is no air fight to word a chance for: the verdict says so, in the group card's words.)
  if (kind === 'superiority' && smallOn(g) && a.E < AIR.sky.least && Math.round(plan.fighters) > 0) {
    const over = Math.floor(a.E), sent = Math.round(plan.fighters);
    // (Words ruled by the UI director, 04:55Z: the short line where every fighter sent can be over the target.)
    plan.tooFew = `Too few fighters to hold this sky: it takes at least ${AIR.sky.least} fighters still over the target at the end, and you are sending ${sent > over ? `${sent}, of which only ${over} can be over it` : over}.`;
    plan.verdict = a.D <= 0 ? 'No fighters will come up' : FIGHT_VERDICT.find(([least]) => fc.success >= least)![1];
  }
  plan.figures = figuresOf(g, kind, target, fc, nat);
  // (The strafers' words, strafers.ts: only where the card's forecast priced its strafers, the switch on, and only on the
  // player's own card; a staff's plan, and so any raid on the player, carries none.)
  if (kind === 'ground' && fc.strafers && nat === g.human) {
    const ids = fc.strafers;
    // (Naming none, its line says why: the first cause that is true, strafeNone.)
    plan.strafe = strafeSection(a.att.filter((x) => ids.includes(x.f.id)).map((x) => x.f.name), ids.length ? 'fought' : strafeNone(g, a));
  }
  // (The guns stop part of the bombing of industry and of a town; over a harbour they only bring bombers down.)
  plan.guns = { level: a.aa, loss: a.gunLoss, lossN: Math.min(plan.bomberLossN, Math.round(a.B * a.gunLoss)), stop: a.aa && kind !== 'harbour' ? gunsStop(a.aa) : 0 };
  // (A strike on ships: the ships' own guns are part of that toll, and the card names them.)
  if (a.flak > 0) plan.guns.ships = kind === 'harbour' ? 'harbour' : 'sea';
  if (kind === 'line') plan.wear = fc.cut;
  // What meeting up is worth: each bomber group alone, with the fighters from its own airfield, beside all of them together.
  const bombers = going.filter((f) => f.bmr > 0);
  if (bombers.length >= 2) {
    const alone = bombers.map((b) => assessAir(g, [b, ...going.filter((x) => x.bmr <= 0 && homeOf(x) === homeOf(b))], m, target, known)).filter((x) => !x.why).map((x) => x.bomberLoss);
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
  if (off.length) plan.offPatrol = `${list(off.map((f) => `the ${f.name}`))} ${off.length === 1 ? 'leaves its patrol' : 'leave their patrols'} for the rest of the month: the sky over ${list([...new Set(off.map((f) => (afloat(f) ? `the ${homeName(g, f)}` : placeName(g.regions[f.base].name))))])} has that much less cover until next month.`;
  if (off.some(afloat)) plan.offPatrol += ` A fleet whose fighters are away is open to the enemy's strike until they are back.`;
  return plan;
}

/** One line of WHAT A HIT DOES: a number and a few words. */
/** One line of WHAT A HIT DOES. `dim`: the other way can do this and this way cannot; it is printed dim, with its true figure. */
export interface HitLine { n: string; words: string; dim?: boolean }

/**
 * One way of flying a bombing raid, by day or by night: a whole small forecast, as the raid card's two columns show
 * it (ruling 107(d), the art director's brief of 16 November). The words are ready to print, so both builds say the
 * same thing; the numbers behind them are there for a card that draws them.
 */
export interface RaidWay {
  kind: 'bomb' | 'night';
  /** BY DAY or BY NIGHT. */
  label: string;
  /** Why it cannot be flown this way with these groups, or null. */
  why: string | null;
  /** Who would go this way: by day the escorts go too, by night the bombers alone. */
  picked: number[];
  bombers: number;
  fighters: number;
  /** The chance of success and its verdict, as the raid card gives them. */
  success: number;
  verdict: string;
  /** The share and the count of the bombers likely lost, and of the fighters where fighters fly. */
  bomberLoss: number;
  bomberLossN: number;
  fighterLoss: number;
  fighterLossN: number;
  /** BOMBERS LOST and FIGHTERS LOST in print: the share and the aircraft, "2% · about 7". The second is '' where no fighters fly. */
  lost: string;
  fightersLost: string;
  /**
   * The guns' part (the player's ask of 2026-11-16): the share and the count of the bombers the flak and the
   * anti-aircraft guns should bring down, a part of the bombers lost; the count as the card prints it ("about 11");
   * and the share of the bombing the guns round the target stop.
   */
  gunLoss: number;
  gunLossN: number;
  toGuns: string;
  gunsStop: number;
  /** The line under the bombers lost: 'of them to flak and guns', or 'of them to light flak' where the target has no guns. */
  gunsWords: string;
  /** The note on the WHAT A HIT DOES line: 'after the guns stop 30% of the bombing'; empty with no guns. */
  stopNote: string;
  /** The damage expected, as the raid card's figures give it: output, national, rail, factory. */
  figures: RaidFigures;
  /** WHAT A HIT DOES: up to three lines, the output stopped, the railway cut, a factory wrecked. */
  hits: HitLine[];
  /** The line at the column's foot. */
  foot: string;
  /** The card's one line of prose for this way. */
  prose: string;
  /** Fog on raids (airfog.ts, its switch on): "Losses forecast: 3 to 9% (not scouted)". Out of sight `lost` and `fightersLost` give the band too. */
  band?: string;
}

/** A bombing raid weighed by day and by night, side by side: the same groups against the same target. */
export interface DayNight {
  /** The way the plan on the card is flown. */
  chosen: 'bomb' | 'night';
  day: RaidWay;
  night: RaidWay;
  /** What is common to both: the bombers going, and the enemy's fighters in reach. */
  bombers: number;
  foes: number;
  /**
   * The target's defences on the ground, common to both: the level of its anti-aircraft guns (0 where none are built)
   * and that in words. The rules put no fog on the guns: the forecast counts them, so the card names them.
   */
  guns: number;
  gunsLabel: string;
  /**
   * The same defences in numbers (Brian, 20 November 2026: "calculate how many flak guns they have and fighter
   * squadrons they have that will be in range to intercept them"): the guns built round the target, light and heavy
   * together (0 where none are built), and how many of them are heavy; `gunsCount` says them in words with the light
   * guns every target has named apart (the UI director's section B, 25 Nov 2026); and the enemy's fighter squadrons
   * able to meet the raid, and the groups they come from. `foes` counts every fighter of those groups; `foesAble`
   * those that can get to the raid.
   */
  gunCount: number;
  gunHeavy: number;
  gunsCount: string;
  foeSquadrons: number;
  foeGroups: number;
  foesAble: number;
  foesCount: string;
  /** After the label, in the row's small dim type: 'not scouted' where fog hides the guns (airfog.ts); empty otherwise. (It said 'light flak only' where no guns were built, until the count named the standing light guns apart: gunsCount.) */
  gunsTail: string;
  /** Fighter groups picked that fly by day and stay at home by night: "Stays home: no escort by night". */
  home: number[];
  /** Fighter groups that could join by day and cannot by night: "Cannot fly: no escort by night". */
  barred: number[];
  /**
   * Fog on raids (airfog.ts, its switch on): what the planner's side knows of the target, as the plan has it. Out of
   * sight the guns and the fighters above are as known: `guns` the level last seen (0 where never seen, with
   * `gunsKnown` false), and the fighters those known for sure.
   */
  fog?: RaidFog;
  gunsKnown?: boolean;
}

/**
 * BY DAY and BY NIGHT for one bombing raid (ruling 107(d)): the same groups against the same target, each way a whole
 * small forecast in the counts and odds the raid card gives. Fighters asked for go by day and stay at home by night,
 * so `ids` may hold both and a change of mind drops nobody. Null for any other kind of raid. Nothing is changed.
 */
export function dayOrNight(g: Game, kind: RaidKind, target: number, ids?: number[], nat: string | null = g.human): DayNight | null {
  if (kind !== 'bomb' && kind !== 'night') return null;
  const plans = { bomb: planRaid(g, 'bomb', target, ids, nat), night: planRaid(g, 'night', target, ids, nat) };
  const way = (k: 'bomb' | 'night'): RaidWay => {
    const p = plans[k];
    const f = p.figures;
    const flown = !p.why;
    // (The atomic project, release 2: a raid that carries the bomb has no hits to list and no bombing for guns to stop.)
    const bomb = !!nat && armedAt(g, nat) === target;
    const hits: HitLine[] = [];
    // A share that rounds to nothing is still something: "<1%", never "−0%".
    if (flown && f.output !== undefined) hits.push({ n: f.output <= 0 ? '0%' : f.output < 0.005 ? '<1%' : `−${pct(f.output)}`, words: 'output, for a month' });
    if (flown && f.rail) hits.push({ n: pct(f.rail), words: 'chance the railway is cut' });
    if (flown && f.factory) hits.push({ n: pct(f.factory), words: 'chance a factory is wrecked' });
    if (flown && !bomb && !hits.length) hits.push({ n: '', words: 'No industry there, and too few bombers to cut the railway.' });
    const fighterLoss = p.fighters > 0 ? p.fighterLossN / p.fighters : 0;
    // (Fog on raids: out of sight the losses are a band, low end to high.)
    const fb = flown && p.fog && !p.fog.live ? p.fog : null;
    const about = (x: [number, number]): string => band(x[0], x[1], false);
    const out: RaidWay = {
      kind: k, label: k === 'bomb' ? 'BY DAY' : 'BY NIGHT', why: p.why, picked: p.picked, bombers: p.bombers, fighters: p.fighters,
      success: p.success, verdict: p.verdict,
      bomberLoss: p.bomberLoss, bomberLossN: p.bomberLossN, fighterLoss, fighterLossN: p.fighterLossN,
      lost: flown ? `${pct(p.bomberLoss)} · about ${num(p.bomberLossN)}` : '',
      fightersLost: flown && p.fighters > 0 ? `${pct(fighterLoss)} · about ${num(p.fighterLossN)}` : '',
      gunLoss: flown ? p.guns.loss : 0, gunLossN: flown ? p.guns.lossN : 0, toGuns: flown ? `about ${num(p.guns.lossN)}` : '', gunsStop: flown ? p.guns.stop : 0,
      gunsWords: !flown ? '' : p.guns.level ? 'of them to flak and guns' : 'of them to light flak',
      stopNote: flown && !bomb && p.guns.stop > 0 ? `after the guns stop ${pct(p.guns.stop)} of the bombing` : '',
      figures: f, hits,
      foot: k === 'bomb' ? 'Escorts can fly with them.' : 'No escorts. Fewer lost, fewer hits.',
      prose: flown ? p.effects[0] ?? '' : '',
    };
    if (flown && p.fog) out.band = p.fog.band;
    if (fb) {
      out.lost = `${band(fb.loss[0], fb.loss[1])} · about ${about(fb.bomberLossN)}`;
      if (p.fighters > 0) out.fightersLost = `${band(fb.fighterLossN[0] / p.fighters, fb.fighterLossN[1] / p.fighters)} · about ${about(fb.fighterLossN)}`;
    }
    return out;
  };
  const day = way('bomb'), night = way('night');
  // A line one way has, the other prints too, dim, with its true figure (the art director's review of Version 82.1):
  // the two columns are read against each other, line for line.
  if (!day.why && !night.why) {
    const ORDER = ['output, for a month', 'chance the railway is cut', 'chance a factory is wrecked'];
    for (const [w, o] of [[day, night], [night, day]] as const) {
      const lacks = ORDER.filter((words) => o.hits.some((h) => h.words === words && !h.dim) && !w.hits.some((h) => h.words === words));
      if (!lacks.length) continue;
      const mine = [...w.hits.filter((h) => ORDER.includes(h.words)), ...lacks.map((words): HitLine => ({ n: '0%', words, dim: true }))];
      w.hits = mine.sort((a, b) => ORDER.indexOf(a.words) - ORDER.indexOf(b.words));
    }
  }
  const lead = !plans.bomb.why ? plans.bomb : plans.night;
  const fighter = (id: number): boolean => plans.bomb.seats.some((s) => s.id === id && s.kind === 'ftr');
  // (Fog on raids: out of sight the guns are as last seen, and the squadrons a band.)
  const fog = lead.fog;
  const hidden = !!fog && !fog.live;
  const level = hidden ? Math.max(0, fog.gunLevel) : g.regions[target] ? gunsAt(g, target) : 0;
  const guns = flakGuns(level);
  // (The UI director, 25 Nov 2026 19:10Z, section B: the built guns first, the standing light guns every target has
  // named apart, never hidden; a bare target in place of "No anti-aircraft guns" and "light flak only".)
  const standing = `the ${num(standingGuns())} light guns every target has.`;
  const squadrons = lead.meets.reduce((n, x) => n + squadronsOf(x.nat, x.able, turnToDate(g.turn).year), 0), able = lead.meets.reduce((n, x) => n + x.able, 0);
  const out: DayNight = {
    chosen: kind, day, night,
    bombers: lead.bombers,
    foes: lead.meets.reduce((n, x) => n + x.ftr, 0),
    guns: level, gunsLabel: level ? `Level ${level} anti-aircraft guns` : `Only ${standing}`, gunsTail: '',
    gunCount: guns.all, gunHeavy: guns.heavy,
    gunsCount: guns.all ? `About ${num(guns.all)} guns built here, ${num(guns.heavy)} of them heavy, and ${standing}` : `Only ${standing}`,
    foeSquadrons: squadrons, foeGroups: lead.meets.length, foesAble: able,
    foesCount: squadrons ? `about ${num(squadrons)} fighter ${squadrons === 1 ? 'squadron' : 'squadrons'} in range` : '',
    home: day.picked.filter((id) => fighter(id) && !night.picked.includes(id)),
    barred: plans.bomb.seats.filter((s) => s.kind === 'ftr' && !s.why && !s.picked).map((s) => s.id),
  };
  if (fog) {
    out.fog = fog;
    out.gunsKnown = fog.gunLevel >= 0;
    if (hidden) {
      // (Her 20:08Z marks, items 3 to 5: the months in words, and a none said as none built.)
      const when = seenAgo(fog.ago);
      out.gunsLabel = fog.gunLevel < 0 ? 'Anti-aircraft guns not known' : level ? `Level ${level} anti-aircraft guns, ${when}` : `No guns built, ${when}`;
      // (Her 21:22Z marks, case d: at a seen level the tail carries the month, as the 3D tail does; seen with none, the
      // count takes item 2's form, which holds its month.)
      out.gunsTail = fog.gunLevel < 0 ? 'not scouted' : level ? when : out.gunsTail;
      out.gunsCount = fog.gunLevel < 0 ? `Guns built here not known, and ${standing}` : level ? out.gunsCount : noneSeen(fog.ago);
      const [lo, hi] = fog.squadrons;
      out.foeSquadrons = hi;
      out.foesCount = hi > 0 ? `about ${band(lo, hi, false)} fighter ${hi === 1 ? 'squadron' : 'squadrons'} in range (not scouted)` : fog.fightersSeen < 0 ? 'fighters in range not known' : '';
    }
  }
  return out;
}

/**
 * Flies a raid the planner has weighed up, at once: the report is filed and its news told. Every group that flew is
 * away for the rest of the month (it flies no second raid and meets no raid on its own sky) and is back at its airfield,
 * on patrol or on escort as it was, when the nation's next month begins. Null if the raid cannot be flown as picked.
 */
export function launchRaid(g: Game, kind: RaidKind, target: number, ids: number[], nat: string | null = g.human, fought?: FoughtAir): AirReport | null {
  if (kind === 'recon' || nat === null) return null;
  const plan = planRaid(g, kind, target, ids, nat, true);
  if (plan.why) return null;
  const fleets = plan.picked.map((id) => g.airFleets.find((f) => f.id === id)!);
  const m = missionOf(kind);
  const rep = fly(g, nat, m, target, fleets, `:${(g.flags.airBattles ?? 0) + 1}`, fought, undefined, true);
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
  /** (City defences, a night raid with the switch on) The searchlights at the target as the raid's report read them. */
  lights?: { level: number; cloud: boolean; lit: number };
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
  // (The report's own result sentence, to both sides: the UI director, 21:22Z, h, "Only the raider's report card prints
  // the A5 line by itself. Every other reader of the raid's result keeps i's line ... and the 3D raid list".)
  return g.airReports.filter((r) => r.turn === turn && (mineToo(r.attNat) || mineToo(r.defNat))).map((r) => {
    const p = r.zone >= 0 ? g.seas[r.zone] : g.regions[r.region];
    return { id: r.id, turn: r.turn, kind: r.mission, region: r.region, zone: r.zone, place: r.place, x: p?.x ?? 0, y: p?.y ?? 0, attNat: r.attNat, defNat: r.defNat, attackers: side(r.att), defenders: side(r.def), won: r.won, level: r.level, result: r.result, ...(r.lights ? { lights: { ...r.lights } } : {}) };
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
  // Last month's photographs are out of date: the armies have moved. (The player's own table: a staff's flight,
  // flown only with the fog on raids, keeps to its own.)
  if (!raidFogOn(g) || f.nat === g.human) {
    const kept: Record<number, number> = {};
    for (const [k, turn] of Object.entries(g.intel.scout ?? {})) if (turn === g.turn) kept[+k] = turn;
    kept[target] = g.turn;
    g.intel.scout = kept;
  }
  // Fog on raids (airfog.ts): the photographs show the ground for the month flown and the next, to the player and to
  // every staff alike; what they show of its air defence is remembered.
  if (raidFogOn(g)) {
    const by = ((g.intel.scoutBy ??= {})[f.nat] ??= {});
    for (const k of Object.keys(by)) if (g.turn - by[+k] >= AIRWAR.fog.months) delete by[+k];
    by[target] = g.turn;
    rememberSkies(g, f.nat);
  }
  const foes = r.units.filter((u) => !u.dead && atWar(g, f.nat, u.nat));
  const seen = foes.length ? `${plural(foes.length, 'corps', 'corps')}, ${plural(foes.reduce((s, c) => s + c.divs, 0), 'division')}${foes.some((c) => c.dug) ? ', dug in' : ''}` : 'no troops there';
  const text = `Aircraft of the ${f.name} photographed ${placeName(r.name)}: ${seen}.${lost ? ` ${plural(lost, 'aircraft', 'aircraft')} did not come back.` : ''}`;
  if (f.nat === g.human) headline(g, 'info', text, f.nat, r.id);
  return { ok: true, text, lost };
}
