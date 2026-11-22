// Fog over the airfields (Brian, 22 November 2026: "fog or storms ground a base's bombers that month ... raids come in
// waves when the skies clear"; the design director's rulings of 22 November 17:44Z). The grand campaign only, behind a
// switch: data/campaign/fog.json 'on', kept with each campaign in g.flags.fog.
//
// It extends the weather's own draw (weather.ts, ruling 127), never a second one: each airfield's month is dealt from
// the campaign's seed the way each zone's sky is, apart from the game's own dice, from the odds for the field's weather
// zone and the month. Fog closes the field (R4): every aircraft based there stays on the ground that month, fighters
// too, of every nation. It works whether 127's zone skies are on or off; with them on, a closed sky grounds the whole
// zone as before and fog adds nothing there. Carriers are at sea and are never fogged in here.
//
// The waves when the skies clear are words only (R6): no raid is invented. A fogged group rests, so it comes back
// fitter, and its card and the month's news say so.
//
// The table is laid only from the historian's figures (R5): until 'sourced' is true the switch cannot ship on, and the
// odds in the data are placeholders for the tests.
//
// With the switch off nothing here runs and no field is ever fogged.

import data from '../../data/campaign/fog.json';
import { Rng, hashString } from '../core/rng';
import { turnToDate } from '../core/ticket';
import { headline } from './game';
import { WEATHER, zoneOfRegion } from './weather';
import type { AirFleet, Game } from './types';

export const FOG = data as unknown as {
  on: boolean; sourced: boolean; odds: Record<string, number[]>;
  words: { card: string; why: string; month: string; wave: string };
};

/** True in a campaign played with fog over the airfields: the grand campaign, begun with the switch on. */
export const fogOn = (g: Game): boolean => g.flags.open === 1 && g.flags.fog === 1;

/** True when a new grand campaign opens with fog (fog.json 'on', and its table sourced by the historian). */
export const fogShipped = (): boolean => FOG.on === true && FOG.sourced === true;

/** Turns fog over the airfields on or off (a new grand campaign as shipped, or the test kitchen). The bridge calls it by name. */
export function setFog(g: Game, on: boolean): void {
  if (on) g.flags.fog = 1;
  else delete g.flags.fog;
}

/** The chance in tenths that fog closes an airfield in a province in a month: its weather zone's for the month, 0 outside every zone. */
export function fogOdds(g: Game, region: number, turn = g.turn): number {
  const z = zoneOfRegion(g, region);
  if (z < 0) return 0;
  return FOG.odds[WEATHER.zones[z].key]?.[turnToDate(turn).month] ?? 0;
}

const dealt = new WeakMap<object, { turn: number; seed: number; fog: Map<number, boolean> }>();

/** True when fog closes the airfield in a province this month: dealt from the seed (as weather.ts deals the skies), never the game's dice. */
export function fogAt(g: Game, region: number, turn = g.turn): boolean {
  if (!fogOn(g) || region < 0 || region >= g.regions.length) return false;
  let m = dealt.get(g);
  if (!m || m.turn !== turn || m.seed !== g.seed) { m = { turn, seed: g.seed, fog: new Map() }; dealt.set(g, m); }
  const had = m.fog.get(region);
  if (had !== undefined) return had;
  const t = fogOdds(g, region, turn);
  const fog = t > 0 && new Rng(hashString(`fog:${g.seed}:${turn}:${region}`)).next() * 10 < t;
  m.fog.set(region, fog);
  return fog;
}

/** True when fog keeps an air group on the ground this month: its airfield is fogged in. A carrier wing aboard is at sea. */
export const fogBound = (g: Game, f: AirFleet): boolean => fogOn(g) && f.ship === undefined && fogAt(g, f.base);

const fill = (s: string, v: Record<string, string | number>): string => s.replace(/\{(\w+)\}/g, (_, k: string) => String(v[k] ?? ''));

/** Why a group cannot fly this month for fog, or null. */
export function whyNotFog(g: Game, f: AirFleet): string | null {
  return fogBound(g, f) ? fill(FOG.words.why, { base: g.regions[f.base]?.name ?? 'its base' }) : null;
}

/** The line on a fogged group's card, or null. */
export const fogCardLine = (g: Game, f: AirFleet): string | null => (fogBound(g, f) ? FOG.words.card : null);

const groupsWord = (n: number): string => (n === 1 ? 'one air group' : `${n} air groups`);

/**
 * As the player's month opens: the month's news says which of his airfields fog has closed, and, when the fog has
 * lifted from fields that were closed last month, that the groups that sat it out are fit to fly again (the waves, R6:
 * words only). Nothing is counted.
 */
export function fogMonth(g: Game): void {
  if (!fogOn(g) || !g.human) return;
  const mine = g.airFleets.filter((f) => f.nat === g.human && f.ship === undefined && f.ftr + f.bmr > 0);
  const now = mine.filter((f) => fogAt(g, f.base));
  const was = mine.filter((f) => fogAt(g, f.base, g.turn - 1) && !fogAt(g, f.base));
  if (now.length) {
    const fields = [...new Set(now.map((f) => f.base))];
    const names = fields.slice(0, 3).map((r) => g.regions[r].name);
    const where = fields.length > 3 ? `${names.join(', ')} and ${fields.length - 3} more` : names.join(fields.length === 2 ? ' and ' : ', ');
    headline(g, 'battle', fill(FOG.words.month, { n: fields.length === 1 ? 'one' : fields.length, where, groups: groupsWord(now.length) }), g.human);
  }
  if (was.length) headline(g, 'battle', fill(FOG.words.wave, { groups: groupsWord(was.length) }), g.human);
}
