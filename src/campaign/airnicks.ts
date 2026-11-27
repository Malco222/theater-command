// Earned names and colours for air groups (release 2; the design director's rulings of 25 November 2026 03:18Z (d)
// and 03:31Z, design-director/air-names-1125.md: Brian, "earned nicknames as they complete missions. i also want them
// to get unique colorings or markings").
//
// Under the corps' switch, 'nicknames'. A group completes a mission in a month when it flies and meets its task; a
// hard fight counts double; at nicknames.json air.need missions it is given a name in its arm's manner and the first
// tier of its colours, and further tiers of colours as it goes on. Every side alike, the player's included; there is
// no cap for air groups. A name and a colour change no number: no rule reads them, no war dice are thrown (the picks
// are hashes of the seed and the group's id, never of its name), and nothing here is written with the switch off.
//
// What counts, from what a flown raid already records (air.ts fly, bookCloseAir):
//   - a raid with bombers: those that bombed (`bombed`, after losses and turn-backs) were more than none;
//   - a raid of fighters alone: a fight for the sky that met the defence (it came up: D > 0) or that won or held the
//     sky; any other (a raid on troops) flown with fighters able to go (E > 0);
//   - a patrol or defence: a group of the defence the raid met (assessAir's def list) while the raid had aircraft;
//   - support over a battle: a ground-support group that flew one or more air strikes over a land battle.

import data from '../../data/campaign/nicknames.json';
import colourData from '../../data/campaign/air-colours.json';
import { Rng, hashString } from '../core/rng';
import { headline } from './game';
import { formName } from './air';
import { NICKNAMES, isBlocked, nicknamesOn, styleKey, takenNames } from './nicknames';
import { airBlocked, airBlockedAt, airBlockedWhole, nextNumber } from './airnames';
import { noteCorps } from './stories';
import type { AirFleet, AirMission, AirSide, Corps, Game } from './types';

interface GuardsRow { form: string; short: string; numbers: number[] }
/** An arm's Guards series by rung and kind (the historian's guards.SOV), and how a city honorific is set after the title. */
interface Guards { regiment: Record<'ftr' | 'bmr', GuardsRow>; division: Record<'ftr' | 'bmr', GuardsRow>; corps: Record<'ftr' | 'bmr', GuardsRow>; city: string }
interface AirStyle { show: string; any: string[]; place: string[]; guards?: Guards }

export const AIRNICKS = (data as unknown as { air: { hard: { pressed: number; held: number; costly: number; least: number }; need: number; double: number; styles: Record<string, AirStyle> } }).air;
export const AIRCOLOURS = colourData as unknown as { count: number; tiers: number[] };

/**
 * A setting outside the save, for the proof that these names change nothing (scratch probe and tests only): set, the
 * air part of the nicknames rule counts and writes nothing, as if it did not exist.
 */
export const airEarn = { skip: false };

const on = (g: Game): boolean => nicknamesOn(g) && !airEarn.skip;

/** The air style a nation names its groups in: the style its corps use, where the air lists have it, else the default. */
export const airStyleKey = (nat: string): string => (AIRNICKS.styles[styleKey(nat)] ? styleKey(nat) : '_default');
const airStyle = (nat: string): AirStyle => AIRNICKS.styles[airStyleKey(nat)];

/** A group's name with its earned name set as its arm sets it ("Jagdgeschwader 3 „Heath Hawks“"); its name alone without one. */
export function calledName(f: AirFleet): string {
  return f.nick ? airStyle(f.nat).show.replace('{name}', f.name).replace('{nick}', f.nick) : f.name;
}

/** True for a name that holds a real unit's name, on either block list. */
export const blockedAir = (name: string): boolean => isBlocked(name) || airBlocked(name);
/**
 * blockedAir for a name that holds a place of this war, the group's own hard fight (12:59Z question 6): the place's own
 * words may match a distinctive word of a real name; the rest of the name may not, and the whole name is still tested
 * against every blocked whole name. A place that is itself a blocked whole name (Moscow) is refused outright.
 */
export const blockedAirAt = (name: string, place: string): boolean => isBlocked(name) || airBlockedAt(name, place);
/** A place of this war that may not stand in a name at all: itself a blocked whole name. */
export const placeBlocked = (place: string): boolean => isBlocked(place) || airBlockedWhole(place);

/** The running loss rate of an arm on one kind of mission (aircraft lost over aircraft flown), or null before it means anything. */
export function rateOf(g: Game, nat: string, kind: string): number | null {
  const t = g.airRates?.[`${nat}:${kind}`];
  return t && t[0] >= AIRNICKS.hard.least ? t[1] / t[0] : null;
}

/** One group's part in a raid, for the count. */
export interface FlownPart { f: AirFleet; side: AirSide; done: boolean; hard: boolean }

/**
 * A raid has been flown (air.ts fly): each group that took part, and whether it completed its mission and fought a
 * hard fight in it. The arm's running loss rates take the raid in after every group has been judged on the rates before it.
 */
export function airRaidFlown(g: Game, m: AirMission, r: { odds: number; bombers: number; bombed: number; E: number; D: number; sky: boolean }, att: { f: AirFleet; side: AirSide }[], def: { f: AirFleet; side: AirSide }[], place: number): void {
  if (!on(g)) return;
  const H = AIRNICKS.hard;
  const met = r.D > 0;
  const reached = r.bombers > 0 ? r.bombed > 0 : m === 'superiority' ? met || r.sky : r.E > 0;
  const flew = (s: AirSide): number => s.ftr + s.bmr;
  const lost = (s: AirSide): number => s.lostFtr + s.lostBmr;
  const costly = (nat: string, kind: string, s: AirSide): boolean => {
    const rate = rateOf(g, nat, kind);
    return rate !== null && rate > 0 && lost(s) >= 1 && flew(s) > 0 && lost(s) / flew(s) >= H.costly * rate;
  };
  const parts: { f: AirFleet; done: boolean; hard: boolean }[] = [];
  for (const x of att) {
    if (flew(x.side) <= 0) continue;
    const pressed = reached && met && r.odds <= H.pressed;
    parts.push({ f: x.f, done: reached, hard: reached && (pressed || costly(x.f.nat, m, x.side)) });
  }
  const raidUp = r.bombers + r.E > 0;
  for (const x of def) {
    if (flew(x.side) <= 0 || !raidUp) continue;
    parts.push({ f: x.f, done: true, hard: r.odds >= H.held || costly(x.f.nat, 'defend', x.side) });
  }
  // The arm's running tally takes this raid in only now.
  const tally = (g.airRates ??= {});
  const add = (nat: string, kind: string, s: AirSide): void => { const t = (tally[`${nat}:${kind}`] ??= [0, 0]); t[0] += flew(s); t[1] += lost(s); };
  for (const x of att) if (flew(x.side) > 0) add(x.f.nat, m, x.side);
  for (const x of def) if (flew(x.side) > 0 && raidUp) add(x.f.nat, 'defend', x.side);
  for (const p of parts) if (p.done) completed(g, p.f, p.hard, place);
}

/** Ground-support groups that flew air strikes over a land battle (air.ts bookCloseAir): each completes its mission. */
export function airSupportFlown(g: Game, groups: AirFleet[], place: number): void {
  if (!on(g)) return;
  for (const f of groups) completed(g, f, false, place);
}

/** A mission completed: one a month, double for a hard fight; then the name and the colours where the count has come. */
function completed(g: Game, f: AirFleet, hard: boolean, place: number): void {
  const d = (f.done ??= { n: 0, at: 0, x: 0, h: 0 });
  const twice = AIRNICKS.double;
  if (d.at !== g.turn) { d.at = g.turn; d.x = hard ? 1 : 0; d.n += hard ? twice : 1; }
  else if (hard && !d.x) { d.x = 1; d.n += twice - 1; }
  // (12:59Z question 5: the place of its latest hard fight in this war, for the city of a Guards title.)
  if (hard) { d.h++; f.hardAt = place; }
  if (d.n >= AIRNICKS.need && !f.nick && f.guards === undefined) nameGroup(g, f, place);
  colour(g, f);
}

/**
 * The name a group's own arm gives it: one in its style that none of its nation's units carries and no block list
 * holds, sometimes after the place of the fight. In an arm that gives the Guards title, the Guards title and a new
 * number from the Guards series, with the name of the city of its latest hard fight (place; none without one). `over`
 * is the place of the mission that brought it to the mark, for a place name only where it has no hard fight on record.
 */
export function earnedAirName(g: Game, f: AirFleet, place: string, over = ''): { nick?: string; name?: string; guards?: number; short?: string } {
  const s = airStyle(f.nat);
  const taken = takenNames(g, f.nat);
  const ok = (n: string): boolean => !taken.has(n) && !blockedAir(n);
  const rng = new Rng(hashString(`airnick:${g.seed}:${f.nat}:${f.id}`));
  // (12:59Z question 6: the place of the group's own hard fight may match a distinctive word; nothing else in the name
  // may. With no hard fight on record, a place name may still take the place it flew over, checked like any other name.)
  const at = place && !placeBlocked(place) ? place : '';
  const placed = at ? s.place.map((t) => t.replace('{place}', at)).filter((n) => !taken.has(n) && !blockedAirAt(n, at))
    : over ? s.place.map((t) => t.replace('{place}', over)).filter(ok) : [];
  const any = s.any.filter(ok);
  if (s.guards) {
    const G = s.guards, kind = f.kind ?? (f.bmr > f.ftr ? 'bmr' : 'ftr');
    // The group's rung, by its name (a regiment where the name cannot tell), and the next number of that Guards series.
    const rung = /Corps$/.test(f.name) ? 'corps' : /Division$/.test(f.name) ? 'division' : 'regiment';
    const row = G[rung][kind];
    const city = at;
    const form = city ? row.form.replace('Guards', G.city.replace('{city}', city)) : row.form;
    const got = nextNumber(g, `guards:${f.nat}:${rung}:${kind}`, row.numbers, (n) => formName(form, n), (x) => g.airFleets.some((y) => y.nat === f.nat && y.name === x) || blockedAirAt(x, city));
    return { name: got.name, guards: got.n as number, short: formName(row.short, got.n) };
  }
  if (placed.length && (rng.next() < 0.25 || !any.length)) return { nick: rng.pick(placed) };
  return any.length ? { nick: rng.pick(any) } : {};
}

function nameGroup(g: Game, f: AirFleet, place: number): void {
  // The place in its name, a Guards city or a place name, is that of its latest hard fight in this war (12:59Z question
  // 5), taken now and kept; with no hard fight on record, or from a save that predates the field, no place at all.
  const hp = f.hardAt;
  const at = hp !== undefined && hp >= 0 ? g.regions[hp]?.name ?? '' : '';
  const got = earnedAirName(g, f, at, place >= 0 ? g.regions[place]?.name ?? '' : '');
  if (!got.nick && !got.name) return;
  if (got.name) { f.was = f.name; f.name = got.name; f.guards = got.guards; if (got.short) f.short = got.short; f.roleNamed = true; }
  if (got.nick) f.nick = got.nick;
  noteCorps(g, f as unknown as Corps, 'airnamed', place, f.done!.n);
  if (f.nat === g.human) {
    const what = got.name ? `${f.was} is made a Guards unit for its ${f.done!.n} missions: it is now ${calledName(f)}.` : `After ${f.done!.n} missions, the crews of ${f.name} are being called ${f.nick}.`;
    headline(g, 'honour', what, f.nat, place >= 0 ? place : undefined);
  }
}

/** The colours a group has earned: its scheme once, and the tier its missions have reached. */
function colour(g: Game, f: AirFleet): void {
  const T = AIRCOLOURS.tiers, n = f.done?.n ?? 0;
  if (n < T[0]) return;
  const tier = T.filter((t) => n >= t).length;
  if (f.colours) { if (tier > f.colours.tier) f.colours.tier = tier; return; }
  const N = AIRCOLOURS.count;
  const worn = new Array(N).fill(0) as number[];
  for (const x of g.airFleets) if (x.nat === f.nat && x.colours && x !== f) worn[x.colours.scheme]++;
  const start = hashString(`aircolour:${g.seed}:${f.nat}:${f.id}`) % N;
  let best = start;
  for (let k = 0; k < N; k++) { const i = (start + k) % N; if (worn[i] < worn[best]) best = i; }
  f.colours = { scheme: best, tier, since: g.turn };
}
