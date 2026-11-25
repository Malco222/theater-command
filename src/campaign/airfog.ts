// Fog and reconnaissance on air raids (release 2; Brian, 24 November 2026: "you should be able to bomb a german city but
// you dont know what your up against without doing recon. If a squadron does a recon mission it should lift the fog of
// war for that area for 2 turns"; the design director's ruling of 22:10Z). Behind the switch g.flags.rfog, off as
// shipped: with it off nothing here is read or written, and every war plays as it did.
//
// With it on, a side sees the air defence of an enemy province (the fighters in reach of its sky, its guns, the share of
// the town in ruins) only while it has the province in full sight, and remembers what it last saw there, with the month
// (g.intel.airSeen). A reconnaissance flight gives full sight of the province and the ground beside it for the month
// flown and the next (g.intel.scoutBy, read by intel.ts sightOf). Before a raid flies, the raid card and every computer
// staff read that knowledge, never the truth, and the forecast is a band: the low end counts only the defence known for
// sure, the high end every fighter last seen in reach rising and the guns last seen (or the heaviest, where none were
// ever seen). In sight, the band is one figure: the truth. A raid that is flown is always fought and booked on what is
// really there (air.ts fly), and fog never refuses a raid: it only hides what is there.
//
// The same code serves the player's card and every staff, of every nation alike.

import type { AirFleet, AirMission, Game } from './types';
import { AIRWAR, assessAir, carrierOf, fighterReach, fightersInReach, forecastAir, gunsAt, km, kmTo, type AirAssess, type AirForecast, type KnownDefence } from './air';
import { SIGHT, fogOn, sightOf } from './intel';
import { fleetKnown } from './carriers';
import { damageOf } from './citydamage';
import { characterOf } from './staff';
import { atWar } from './game';
import { scout } from './airwar';

const F = () => AIRWAR.fog;

/** True where raids are weighed on what each side knows: the switch, g.flags.rfog. */
export const raidFogOn = (g: Game): boolean => g.flags.rfog === 1;

/** True when a new grand campaign would open with the fog on raids (airwar.json fog.on; off as shipped). */
export const raidFogShipped = (): boolean => F().on === true;

/** Turns the fog on raids on or off for one war (a war runner right after startOpen, a test, or the bridge by name). */
export function setRaidFog(g: Game, on: boolean): void {
  if (on) g.flags.rfog = 1;
  else delete g.flags.rfog;
}

/** The raids whose target is a province: those the fog lies on. A strike at sea has its own search (carriers.ts). */
const LAND: ReadonlySet<AirMission> = new Set<AirMission>(['bomb', 'night', 'town', 'harbour', 'airfield', 'superiority', 'ground', 'line']);

/**
 * One side's memory of one province's air defence (types.ts Intel.airSeen): [the month, the enemy fighters in reach of
 * its sky (to the nearest 'round'), the groups they were in, the level of its guns, the percent of the town in ruins].
 */
type Seen = number[];

const roundTo = (n: number): number => { const k = Math.max(1, F().round); return Math.round(n / k) * k; };

/** True where this side has the province in full sight now: it sees what is there, and the forecast is the truth. */
function inSight(g: Game, nat: string, t: number, sight?: Uint8Array): boolean {
  return (sight ?? sightOf(g, nat))[t] === SIGHT.full;
}

/** True where the fog lies on this raid for this side: the switch on, a target on land, the land's fog on for it, and the place out of its sight. */
export function fogged(g: Game, nat: string | null, m: AirMission, t: number, sight?: Uint8Array): boolean {
  return raidFogOn(g) && nat !== null && LAND.has(m) && !!g.regions[t] && fogOn(g, nat) && !inSight(g, nat, t, sight);
}

/**
 * What this side sees of a province's air defence as it is now: the fighters in reach, their groups, the guns, the ruin.
 * The same figures the memory keeps, so a sight and a memory of it read alike.
 */
function looked(g: Game, nat: string, t: number): Seen {
  const up = fightersInReach(g, t, nat);
  return [g.turn, roundTo(up.reduce((s, f) => s + f.ftr, 0)), up.length, gunsAt(g, t), Math.round(damageOf(g, g.regions[t]) * 100)];
}

/**
 * Writes what one side sees now into its memory: every enemy-held province it has in full sight. Sightings older than
 * 'forget' months are thrown away. Called as each side's month opens, and after each of its reconnaissance flights.
 */
export function rememberSkies(g: Game, nat: string | null): void {
  if (!raidFogOn(g) || nat === null || !g.nations[nat]?.alive || !fogOn(g, nat)) return;
  const sight = sightOf(g, nat);
  const book: Record<number, Seen> = ((g.intel.airSeen ??= {})[nat] ??= {});
  for (const r of g.regions) {
    if (sight[r.id] !== SIGHT.full || !atWar(g, nat, r.ctrl)) continue;
    book[r.id] = looked(g, nat, r.id);
  }
  for (const k of Object.keys(book)) if (g.turn - book[+k][0] >= F().forget) delete book[+k];
}

/** What one side knows of a province's air defence, for the card's words and for its forecast. */
export interface SkyKnown {
  /** In full sight now: every figure is the truth. */
  live: boolean;
  /** In sight by the side's own reconnaissance flight, this month or last. */
  scouted: boolean;
  /** Months since it was last seen: 0 in sight, -1 never seen (or so long ago it is forgotten). */
  ago: number;
  /** The enemy fighters in reach of its sky as last seen, and their groups; -1 where never seen. */
  fighters: number;
  groups: number;
  /** The level of its guns as last seen (0 to 3), -1 where never seen. */
  guns: number;
  /** The share of the town in ruins as last seen (0 to 1), -1 where never seen. */
  ruin: number;
  /**
   * Where the place itself was never seen: the most fighters last seen in reach of any place near it (within the reach
   * of its holder's fighters), what the high end then counts; -1 where nothing near it was seen either.
   */
  near: number;
}

/** What one side knows of one province now. */
export function skyKnown(g: Game, nat: string, t: number, sight?: Uint8Array): SkyKnown {
  const by = g.intel?.scoutBy?.[nat]?.[t];
  const scouted = by !== undefined && g.turn - by < F().months;
  if (!raidFogOn(g) || !fogOn(g, nat) || inSight(g, nat, t, sight)) {
    const [, fighters, groups, guns, ruin] = looked(g, nat, t);
    return { live: true, scouted, ago: 0, fighters, groups, guns, ruin: ruin / 100, near: fighters };
  }
  const book = g.intel?.airSeen?.[nat] ?? {};
  const s = book[t];
  if (s && g.turn - s[0] < F().forget) return { live: false, scouted: false, ago: g.turn - s[0], fighters: s[1], groups: s[2], guns: s[3], ruin: s[4] / 100, near: s[1] };
  // Never seen: the fighters seen in reach of the places near it may be in reach of it too. Their reach is the holder's
  // fighters' as a group based there has it (on the far side the far rows and the cover cap, as reach() reads them).
  const R = fighterReach(g, g.regions[t].ctrl, t);
  let near = -1;
  for (const [id, x] of Object.entries(book)) if (g.turn - x[0] < F().forget && x[1] > near && km(g, +id, t) <= R) near = x[1];
  return { live: false, scouted: false, ago: -1, fighters: -1, groups: 0, guns: -1, ruin: -1, near };
}

/**
 * The defence one end of the band counts (air.ts KnownDefence). Known for sure: the enemy fighter groups whose own fields
 * this side has in full sight now (a carrier wing, where its fleet is identified). The low end counts those alone, and
 * the guns last seen (works stand where they are built; none where never seen, the light flak any target has). The high
 * end adds every other fighter last seen in reach of the place, all of them rising (where the place was never seen, the
 * most seen in reach of a place near it: SkyKnown.near), and the guns last seen, or the heaviest that can be built
 * where none were ever seen.
 */
export function knownDefence(g: Game, nat: string, t: number, end: 'low' | 'high', sight?: Uint8Array): KnownDefence {
  const S = sight ?? sightOf(g, nat);
  const k = skyKnown(g, nat, t, S);
  const sure = (f: AirFleet): boolean => {
    if (f.ship === undefined) return S[f.base] === SIGHT.full;
    const c = carrierOf(g, f);
    return !!c && fleetKnown(g, c, nat) === 'exact';
  };
  // The fighters known for sure in reach are counted by name, with their own distance: the rest of what was seen is extra.
  const known = fightersInReach(g, t, nat).filter(sure).reduce((s, f) => s + f.ftr, 0);
  const extra = end === 'high' && k.near > 0 ? Math.max(0, k.near - known) : 0;
  const aa = k.guns >= 0 ? k.guns : end === 'high' ? F().gunsUnknown : 0;
  return { sure, extra, extraNat: g.regions[t].ctrl, aa, ruined: Math.max(0, k.ruin), seen: k.guns, ago: k.ago };
}

/** A mission weighed on one side's knowledge: the forecast at both ends of the band (one and the same in sight). */
export interface FoggedForecast {
  live: boolean;
  lo: { a: AirAssess; fc: AirForecast | null };
  hi: { a: AirAssess; fc: AirForecast | null };
}

/** Weighs a mission at both ends of what this side knows of the target. In sight, or with the switch off, both ends are the truth. */
export function weighKnown(g: Game, nat: string, fleets: AirFleet[], m: AirMission, t: number, sight?: Uint8Array): FoggedForecast {
  const S = raidFogOn(g) ? sight ?? sightOf(g, nat) : undefined;
  if (!fogged(g, nat, m, t, S)) {
    const a = assessAir(g, fleets, m, t);
    const one = { a, fc: a.why ? null : forecastAir(g, a, m, t) };
    return { live: true, lo: one, hi: one };
  }
  const end = (e: 'low' | 'high') => {
    const a = assessAir(g, fleets, m, t, knownDefence(g, nat, t, e, S));
    return { a, fc: a.why ? null : forecastAir(g, a, m, t) };
  };
  return { live: false, lo: end('low'), hi: end('high') };
}

/** A point in the band: 0 the low end, 1 the high end. Each figure of the forecast is read that far from one end to the other. */
export function bandAt(lo: AirForecast, hi: AirForecast, at: number): AirForecast {
  if (at >= 1) return hi;
  if (at <= 0) return lo;
  const out = { ...hi } as Record<string, unknown>;
  const L = lo as unknown as Record<string, unknown>;
  for (const [key, v] of Object.entries(hi)) if (typeof v === 'number' && typeof L[key] === 'number') out[key] = (L[key] as number) + (v - (L[key] as number)) * at;
  return out as unknown as AirForecast;
}

/** Where in the band a nation's staff reads a forecast (airwar.json fog.reads, by its character): careful the high end, bold the middle. */
export const staffReads = (g: Game, nat: string): number => F().reads[characterOf(g, nat)] ?? 1;

/**
 * The forecast a computer staff weighs a mission by: with the switch off, or the place in its sight, the truth; out of
 * its sight, its own knowledge read at its own point in the band. The assessment returned is the high end's (whether
 * the mission can be flown is the same at both ends).
 */
export function staffWeigh(g: Game, nat: string, fleets: AirFleet[], m: AirMission, t: number, sight?: Uint8Array): { a: AirAssess; fc: AirForecast | null } {
  if (!raidFogOn(g)) { const a = assessAir(g, fleets, m, t); return { a, fc: a.why ? null : forecastAir(g, a, m, t) }; }
  const w = weighKnown(g, nat, fleets, m, t, sight);
  if (w.live || !w.lo.fc || !w.hi.fc) return w.hi;
  return { a: w.hi.a, fc: bandAt(w.lo.fc, w.hi.fc, staffReads(g, nat)) };
}

/** True where a staff should look again before it raids a place: out of its sight, and never seen or last seen 'stale' months ago or more. */
export function wantsLook(g: Game, nat: string, t: number, sight?: Uint8Array): boolean {
  if (!raidFogOn(g) || !fogOn(g, nat)) return false;
  const k = skyKnown(g, nat, t, sight);
  return !k.live && (k.ago < 0 || k.ago >= F().stale);
}

/**
 * A staff's reconnaissance before its raids (the ruling's (c)): of the targets it means to weigh, in the order it would
 * weigh them, those out of its sight and never seen or seen 'stale' months ago or more are photographed first, at most
 * 'scouts' a month. Each by the nearest of its groups that can reach it and has not flown its reconnaissance this
 * month, leaving out a group the flight would bring below the readiness the staff raids at (as its searches at sea
 * do). The flight is the player's own reconnaissance (airwar.ts scout): its losses, its readiness, its sight.
 */
export function staffScouts(g: Game, nat: string, mine: AirFleet[], targets: number[]): void {
  if (!raidFogOn(g) || !fogOn(g, nat)) return;
  const ready = AIRWAR.ai.raids.ready, cost = AIRWAR.recon.ready;
  let sight = sightOf(g, nat);
  let flown = 0;
  for (const t of targets) {
    if (flown >= F().scouts) break;
    if (!wantsLook(g, nat, t, sight)) continue;
    const by = mine
      .filter((f) => f.ftr + f.bmr > 0 && f.scouted !== g.turn && !(f.ready >= ready && f.ready - cost < ready))
      .map((f) => ({ f, d: kmTo(g, f, t) }))
      .sort((a, b) => a.d - b.d || a.f.id - b.f.id);
    for (const { f } of by) {
      if (!scout(g, f.id, t, nat).ok) continue;
      flown++;
      sight = sightOf(g, nat);
      break;
    }
  }
}

// ---- The card's words --------------------------------------------------------------------------------------------

export const agoWords = (n: number): string => (n <= 0 ? 'seen this month' : n === 1 ? 'seen a month ago' : `seen ${n} months ago`);
const gunWord = (level: number): string => F().words[Math.max(0, Math.min(F().words.length - 1, level))];

/** What the raid card says one side knows of a target's defences, a line each: "Fighters: unknown", "Guns: heavy, seen 3 months ago". */
export function knownLines(g: Game, k: SkyKnown, withRuin: boolean): { fighters: string; guns: string; damage: string } {
  const when = k.live ? (k.scouted ? 'scouted' : 'in sight') : agoWords(k.ago);
  const fighters = k.fighters < 0 ? 'Fighters: unknown' : `Fighters: ${k.fighters > 0 ? `about ${k.fighters.toLocaleString('en-US')} in reach` : 'none in reach'}, ${when}`;
  const guns = k.guns < 0 ? 'Guns: unknown' : `Guns: ${gunWord(k.guns)}, ${when}`;
  const damage = !withRuin ? '' : k.ruin < 0 ? 'Damage: unknown' : `Damage: ${k.ruin > 0 ? `${Math.round(k.ruin * 100)}% of the town in ruins` : 'none'}, ${when}`;
  return { fighters, guns, damage };
}

/** "3 to 9%", or one figure where the two ends round alike. */
export function band(lo: number, hi: number, pctOf = true): string {
  const a = Math.round(Math.min(lo, hi) * (pctOf ? 100 : 1)), b = Math.round(Math.max(lo, hi) * (pctOf ? 100 : 1));
  const u = pctOf ? '%' : '';
  return a === b ? `${a}${u}` : `${a} to ${b}${u}`;
}

/**
 * The fighters as a list of targets says them where the place is out of sight: "fighters not known", "about 120
 * fighters in reach when seen a month ago". (The guns of such a row are said in airwar.ts whatIsThere, in the UI
 * director's words of 25 Nov 2026 20:08Z, item 6: the old guns text here had no reader left and is gone.)
 */
export function fogBits(k: SkyKnown): { fighters: string } {
  const when = agoWords(k.ago).replace(/^seen/, 'when seen');
  return {
    fighters: k.fighters < 0 ? 'fighters not known' : `${k.fighters > 0 ? `about ${k.fighters.toLocaleString('en-US')} fighters in reach` : 'no fighters in reach'} ${when}`,
  };
}

/** The note after a forecast: how much of it is known. */
export const bandNote = (k: SkyKnown): string => (k.live ? (k.scouted ? 'scouted' : 'in sight') : 'not scouted');
