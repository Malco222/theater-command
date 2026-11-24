// Where each side last saw the other at sea (release 2, Units decide; the design director's 24 November 2026 17:49Z
// amendment to ruling 1, and his ruling of 17:54Z on naval-battles/convoy-r2/sight-store-form.md, (a) on all five
// questions): "Each staff counts the other side's units where its own side last saw them (sightings, attacks, losses),
// never where they truly are, and never from the player's plans. The computer never knows more than the player (133(a)),
// and the same sight feeds the player's own panel."
//
// The record is a memory over the one sight, never a second sight rule (his note of 17:54Z): what each side's convoys,
// escorts, hunters and patrols met at sea, and what its search found there as carriers.ts fleetFound finds a fleet. One
// rule both ways: units that act reveal themselves where they acted. A pack that attacks a convoy is seen by attacking,
// and the aircraft that keep boats down or attack them are seen by doing it.
//
// Each nation keeps one record a sea (g.intel.seaSight), in two parts, each with its own month:
// - The enemy boats it saw there, which its patrol staff reads (seapatrol.ts). They are written once a month, at the close
//   of its own month at sea (navy.ts navalUpkeep, after the hunt, its patrols' attacks and the battles on its convoy
//   routes), in every sea it had eyes on: the seas of its convoy routes while it is at war, the seas its escorts sail with
//   their convoys and its hunting groups work, the seas its patrols watch, and the seas its search has in view for boats
//   (carriers.ts boatsInView). What it saw there is the most any one source showed, so that a boat seen twice is one boat
//   (D): the boats that fired on its convoys there, those its escort met, those its patrols sighted, those it sank, and
//   the boats of fleets its side has identified there (fleetFound 'exact'). A fleet only reported ('report') is not
//   counted, since the one sight gives no strength for it (carriers.ts fleetsSeen and seaPicture); nor does its sea then
//   read as empty: where nothing else was counted there, the old record stands. (The form's Q2 (a) names fleetFound
//   without saying how a report counts; this is the builder's reading of the one sight, for the design director.) Eyes
//   and nothing seen: the record says none, with this month. No eyes: the old record stands, with its month.
// - The enemy aircraft its own boats met there, which its boats' staff reads (navy.ts quieterBeat). They are written at
//   the same close, in the seas where its raiding boats are on station and those where the enemy's patrols met its boats
//   since its last month: the watch those patrols kept over its boats on station there, as each patrolling nation's
//   month booked it (navy.ts patrolSubs, g.patrolSunk 'met': the part of a boat's time kept down by aircraft without
//   anti-submarine kit, and the part attacked by aircraft with it), and its boats lost to aircraft there. So a patrol is
//   seen once it has flown its month over the boats, never when it is ordered. No watch met: none, with this month.
//   Elsewhere the old record stands.
// A staff reads the newest record among its own nation and its allies, one rule for every alliance (seaSeen). A sea never
// seen counts as clear: no boats for the patrol staff, no aircraft for the boats' staff, so a staff learns by trying.
// There is no decay figure: each record carries its month, and the staff and the player's panel both show it.
//
// The player's panel reads the same function (boatsKnown, airKnown). With his fog of war lifted (intel.ts fogOn) his
// panel shows the truth, and only his: a staff never reads it.
//
// Written and read only while the sea patrol is live (seapatrol.ts patrolOn). Otherwise nothing here is read or written,
// and a save is what it was without it. Nothing in it reads the flag or the date.
import { NAVY, atSea, onStation, raidShare, seaRoutes, shipCount, zoneOf } from './navy';
import { patrolAircraft, patrolOn, watchAgainst, watchFor } from './seapatrol';
import { boatsInView, fleetFound } from './carriers';
import { allied, atWar } from './game';
import { fogOn } from './intel';
import { seaWhat } from './sea-words';
import { turnToDate } from '../core/ticket';
import type { Game, SeaSight, SightHow } from './types';

/**
 * What a nation's month at sea showed it of the enemy's boats on its convoy routes, sea by sea, as its convoy battles
 * booked them (navy.ts convoyWar): the boats that fired on its convoys there, those its escort met, and those its escorts
 * sank. Kept for the month's close only, never saved.
 */
export interface SeaMonth { attack: Record<number, number>; incident: Record<number, number>; sunk: Record<number, number> }

/** An empty month at sea, for the convoy battles to fill. */
export const seaMonth = (): SeaMonth => ({ attack: {}, incident: {}, sunk: {} });

/** The order a record's ways of seeing are written in. */
const HOW: SightHow[] = ['attack', 'incident', 'sighting', 'loss', 'search', 'keptDown'];
const r2 = (x: number): number => Math.round(x * 100) / 100;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;

/**
 * The boats of a nation's enemies on station in a sea now: each fleet's boats x its share of the month on station. The
 * truth: only the lifted fog's panel reads it, never a staff.
 */
function boatsThere(g: Game, nat: string, z: number): number {
  let b = 0;
  for (const f of g.fleets) if (f.zone === z && atSea(f) && f.ships.ss > 0 && atWar(g, f.nat, nat)) b += f.ships.ss * onStation(g, f, z);
  return b;
}

/** What a nation's boats met of the enemy's aircraft on patrol in one sea: the parts of a boat's time watched with kit and without, and its boats they sank. */
interface AirMet { kit: number; none: number; lost: number }

/**
 * What a nation's boats met of the enemy's aircraft on patrol since its own last month at sea, sea by sea: each enemy's
 * month of patrols (g.patrolSunk, booked in its own month by navy.ts patrolSubs) that came after it, this month's before
 * its own in the order of play and last month's after it. The watch those patrols kept over its boats on station ('met',
 * each patrolling nation's part, added up) and its boats they sank ('at'). Only a patrol that has flown its month over
 * the boats is here: an order not yet flown is not (the design director's ruling of 24 November 2026 17:54Z).
 */
function airMet(g: Game, nat: string): Map<number, AirMet> {
  const me = g.order.indexOf(nat);
  const out = new Map<number, AirMet>();
  const sea = (z: number): AirMet => { let r = out.get(z); if (!r) out.set(z, (r = { kit: 0, none: 0, lost: 0 })); return r; };
  for (const by of Object.keys(g.patrolSunk ?? {}).sort()) {
    const rec = g.patrolSunk![by];
    if (!(rec.turn === g.turn || (rec.turn === g.turn - 1 && g.order.indexOf(by) > me))) continue;
    for (const z of Object.keys(rec.met ?? {})) { const m = rec.met![+z][nat]; if (m) { const r = sea(+z); r.kit += m.kit; r.none += m.none; } }
    for (const z of Object.keys(rec.at ?? {})) { const k = rec.at![+z][nat]; if (k) sea(+z).lost += k; }
  }
  return out;
}

/**
 * The close of a nation's month at sea (navy.ts navalUpkeep, after the hunt, its patrols' attacks and the battles on its
 * convoy routes): what it saw of the enemy's boats in every sea it had eyes on, and what its own boats met of the
 * enemy's aircraft where they are on station or were met by the enemy's patrols since its last month, written into its
 * record (the design director's ruling of 24 November 2026 17:54Z, Q2 and Q3 (a)). Only what happened is read: what its
 * convoys, escorts, hunters and patrols met, what its side's search has identified, and the watch the enemy's patrols
 * kept over its boats as their own months booked it (airMet). Never an order or a plan, the enemy's patrol orders not
 * yet flown included. 'sunk' is the month's hunt by sea, its patrols' kills included (navy.ts huntSubs and patrolSubs).
 * Nothing while the patrol is not live.
 */
export function noteSeaSight(g: Game, nat: string, month: SeaMonth = seaMonth(), sunk: Record<number, number> = {}): void {
  const n = g.nations[nat];
  if (!patrolOn(g) || !n?.alive) return;
  const t = g.turn;
  // (Where it had eyes on the enemy's boats this month.)
  const eyes = new Set<number>();
  const eye = (z: number): void => { if (g.seas[z]) eyes.add(z); };
  if (n.atWar.length) {
    // (Its convoys, on every sea of their routes while it is at war.)
    for (const r of NAVY.convoys) if (r.to === nat) for (const k of r.zones) eye(zoneOf(g, k));
    // (Its escorts, on every sea of the route or lane they sail with, and its hunting groups where they hunt.)
    for (const f of g.fleets) {
      if (f.nat !== nat || !atSea(f) || shipCount(f.ships) <= 0) continue;
      if (f.stance === 'escort') { eye(f.zone); for (const z of seaRoutes(g).find((r) => r.id === f.lane)?.zones ?? []) eye(z); }
      else if (f.stance === 'hunt') eye(f.zone);
    }
    // (Its patrols, over each sea they watched this month.)
    for (const a of g.airFleets) if (a.nat === nat && a.mission === 'seapatrol' && g.seas[a.target] && patrolAircraft(g, a, a.target) > 0) eye(a.target);
    // (Its search, where it has a sea in view for boats as fleetFound judges it.)
    for (const z of g.seas) if (boatsInView(g, nat, z.id)) eye(z.id);
  }
  // (What each source showed, sea by sea. HF/DF and codebreaking, once ruled, are more sources here, never a second
  // store: the design director's note of 17:54Z.)
  const shown = new Map<number, Map<SightHow, number>>();
  const show = (z: number, how: SightHow, x: number): void => {
    if (!(x > 0) || !g.seas[z]) return;
    let m = shown.get(z);
    if (!m) shown.set(z, (m = new Map()));
    m.set(how, (m.get(how) ?? 0) + x);
  };
  // (The boats that fired on its convoys, and those its escort met: navy.ts convoyWar.)
  for (const z of Object.keys(month.attack)) show(+z, 'attack', month.attack[+z]);
  for (const z of Object.keys(month.incident)) show(+z, 'incident', month.incident[+z]);
  // (The boats it sank: the hunt, its patrols and its convoys' escorts.)
  for (const z of Object.keys(sunk)) show(+z, 'loss', sunk[+z]);
  for (const z of Object.keys(month.sunk)) show(+z, 'loss', month.sunk[+z]);
  // (The boats its own patrols sighted: the boats on station x the share of the sea its own aircraft watch.)
  const watched = new Map<number, number>();
  for (const a of g.airFleets) {
    if (a.nat !== nat || a.mission !== 'seapatrol' || !g.seas[a.target] || watched.has(a.target)) continue;
    watched.set(a.target, watchFor(g, a.target, (x) => x === nat)?.s ?? 0);
  }
  // (And the boats of fleets its side has identified there, as fleetFound finds a fleet: the one sight. A fleet only
  // reported is not counted, as the map counts no strength for it (carriers.ts fleetsSeen, seaPicture), and its sea is
  // not read as empty either.)
  const reported = new Set<number>();
  for (const f of g.fleets) {
    if (!atSea(f) || f.ships.ss <= 0 || !g.seas[f.zone] || !atWar(g, f.nat, nat)) continue;
    const b = f.ships.ss * onStation(g, f, f.zone);
    show(f.zone, 'sighting', b * (watched.get(f.zone) ?? 0));
    const found = fleetFound(g, f, nat);
    if (found === 'exact') show(f.zone, 'search', b);
    else if (found === 'report') reported.add(f.zone);
  }
  const book = (z: number): SeaSight => (((g.intel.seaSight ??= {})[nat] ??= {})[z] ??= {});
  for (const z of [...new Set([...eyes, ...shown.keys()])].sort((a, b) => a - b)) {
    const m = shown.get(z);
    if (!m && reported.has(z)) continue;
    const most = m ? Math.max(...m.values()) : 0;
    book(z).boats = { t, n: r2(most), how: HOW.filter((h) => (m?.get(h) ?? 0) > 0) };
  }
  // (The aircraft its own boats met, where they are on station raiding and where the enemy's patrols met them since its
  // last month: the watch those patrols kept over them, as each patrol's own month booked it, and its boats lost to
  // aircraft there. Never the orders standing now.)
  const met = airMet(g, nat);
  const on = new Set<number>(met.keys());
  for (const f of g.fleets) if (f.nat === nat && atSea(f) && f.ships.ss > 0 && raidShare(f) > 0 && g.seas[f.zone]) on.add(f.zone);
  for (const z of [...on].sort((a, b) => a - b)) {
    if (!g.seas[z]) continue;
    const a = met.get(z);
    const none = r3(Math.min(1, a?.none ?? 0)), kit = r3(Math.min(1 - none, a?.kit ?? 0)), lost = a?.lost ?? 0;
    const how: SightHow[] = [];
    if (kit > 0) how.push('attack');
    if (lost > 0) how.push('loss');
    if (none > 0) how.push('keptDown');
    book(z).air = { t, kit, none, lost, how };
  }
}

/**
 * What the viewer's side last saw in a sea: of each part, the newest record among its own nation's and its allies' (one
 * rule for every alliance; on a month's tie, the one that saw the most), or null where none of them ever saw it. Read by
 * the staffs and, through boatsKnown and airKnown, by the player's panel. Null while the patrol is not live.
 */
export function seaSeen(g: Game, viewer: string | null, z: number): SeaSight | null {
  const all = viewer && patrolOn(g) && g.nations[viewer] ? g.intel?.seaSight : undefined;
  if (!all || !viewer) return null;
  let boats: SeaSight['boats'], air: SeaSight['air'];
  for (const k of Object.keys(all).sort()) {
    if (k !== viewer && !(g.nations[k] && allied(g, k, viewer))) continue;
    const r = all[k][z];
    if (!r) continue;
    if (r.boats && (!boats || r.boats.t > boats.t || (r.boats.t === boats.t && r.boats.n > boats.n))) boats = r.boats;
    if (r.air && (!air || r.air.t > air.t || (r.air.t === air.t && r.air.kit + r.air.none > air.kit + air.none))) air = r.air;
  }
  return boats || air ? { ...(boats ? { boats } : {}), ...(air ? { air } : {}) } : null;
}

/** The enemy boats the viewer's side last saw in a sea, and the month (seaSeen), or null where it never saw any there: for the patrol staff, no boats. */
export const seenBoats = (g: Game, viewer: string | null, z: number): NonNullable<SeaSight['boats']> | null => seaSeen(g, viewer, z)?.boats ?? null;

/** The enemy aircraft the viewer's side's boats last met in a sea, and the month (seaSeen), or null where they never met any there: for the boats' staff, a clear sea. */
export const seenAir = (g: Game, viewer: string | null, z: number): NonNullable<SeaSight['air']> | null => seaSeen(g, viewer, z)?.air ?? null;

/** True where the viewer is the player and has lifted his own fog of war: his panel then shows the truth, and only his. */
const lifted = (g: Game, viewer: string | null): boolean => viewer !== null && viewer === g.human && !fogOn(g, viewer);

/**
 * The enemy boats the viewer's panel shows in a sea, and the month: his side's sight (seenBoats), or, with the player's
 * own fog of war lifted, the boats on station there now. Null where none were ever seen, and while the patrol is not
 * live. The panel's only: a staff reads seenBoats.
 */
export function boatsKnown(g: Game, viewer: string | null, z: number): { n: number; t: number } | null {
  if (!viewer || !patrolOn(g) || !g.seas[z]) return null;
  if (lifted(g, viewer)) return { n: boatsThere(g, viewer, z), t: g.turn };
  const b = seenBoats(g, viewer, z);
  return b ? { n: b.n, t: b.t } : null;
}

/**
 * The enemy aircraft the viewer's panel shows over a sea, as his boats met them, and the month: his side's sight
 * (seenAir), or, with the player's own fog of war lifted, the watch kept against his boats there now. Null where his
 * side's boats never met any there, and while the patrol is not live. The panel's only: a staff reads seenAir.
 */
export function airKnown(g: Game, viewer: string | null, z: number): { kit: number; none: number; lost: number; t: number } | null {
  if (!viewer || !patrolOn(g) || !g.seas[z]) return null;
  if (lifted(g, viewer)) { const w = watchAgainst(g, z, viewer); return { kit: w?.kit ?? 0, none: w?.none ?? 0, lost: airMet(g, viewer).get(z)?.lost ?? 0, t: g.turn }; }
  const a = seenAir(g, viewer, z);
  return a ? { kit: a.kit, none: a.none, lost: a.lost, t: a.t } : null;
}

/** About so many boats: to one place under ten, as the convoy card counts the pack. */
const about = (n: number): string => (n < 10 ? n.toFixed(1) : String(Math.round(n)));
/** A share of a boat's time, in whole per cent. */
const pct = (x: number): string => `${Math.round(x * 100)}%`;
/** The months records were made in: "October 1940", "March to October 1940", or "December 1940 to March 1941". */
function months(from: number, to: number): string {
  const a = turnToDate(from), b = turnToDate(to);
  if (from === to) return a.label;
  return a.year === b.year ? `${a.label.split(' ')[0]} to ${b.label}` : `${a.label} to ${b.label}`;
}

/**
 * The player's panel on a convoy route (the design director's ruling of 17:54Z, Q4 (a)): "Boats last seen on its seas:
 * about N (October 1940)", the boats his side last saw on each of the route's seas added up, with the months they were
 * seen, or "No boats seen on its seas". Empty while the patrol is not live.
 */
export function routeSeenWords(g: Game, viewer: string | null, zones: number[]): string {
  if (!viewer || !patrolOn(g)) return '';
  let n = 0, from = Infinity, to = -Infinity;
  for (const z of [...new Set(zones)]) {
    const b = boatsKnown(g, viewer, z);
    if (!b || !(b.n > 0)) continue;
    n += b.n; from = Math.min(from, b.t); to = Math.max(to, b.t);
  }
  return n >= 0.05 ? `Boats last seen on its seas: about ${about(n)} (${months(from, to)})` : 'No boats seen on its seas';
}

/** The same for one sea, on a sea patrol's order: "Boats last seen there: about N (October 1940)", or "No boats seen there". Empty while the patrol is not live. */
export function seaSeenWords(g: Game, viewer: string | null, z: number): string {
  if (!viewer || !patrolOn(g)) return '';
  const b = boatsKnown(g, viewer, z);
  return b && b.n >= 0.05 ? `Boats last seen there: about ${about(b.n)} (${turnToDate(b.t).label})` : 'No boats seen there';
}

/**
 * The player's panel where he holds boats (Q4 (a)): "Aircraft last met over the Western Approaches: keeping the boats
 * down about 20% of the time, attacking them with radar and depth charges about 5% (October 1940)", as his side's boats
 * last met them there; "...: none (October 1940)" where they met none; "No aircraft met over ..." where his boats were
 * never there. Empty while the patrol is not live.
 */
export function airMetWords(g: Game, viewer: string | null, z: number): string {
  if (!viewer || !patrolOn(g) || !g.seas[z]) return '';
  const sea = seaWhat(g.seas[z].name);
  const a = airKnown(g, viewer, z);
  if (!a) return `No aircraft met over ${sea}`;
  const parts: string[] = [];
  if (a.none >= 0.005) parts.push(`keeping the boats down about ${pct(a.none)} of the time`);
  if (a.kit >= 0.005) parts.push(`attacking them with radar and depth charges about ${pct(a.kit)} of the time`);
  if (a.lost > 0) parts.push(`${a.lost} boat${a.lost === 1 ? '' : 's'} lost to them`);
  return `Aircraft last met over ${sea}: ${parts.length ? parts.join(', ') : 'none'} (${turnToDate(a.t).label})`;
}
