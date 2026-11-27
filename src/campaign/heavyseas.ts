// Heavy seas (release 2; Brian, 24 November 2026 20:38Z, "yes to all", of the idea "Today the weather at sea only
// grounds aircraft. With this, winter gales would also slow fleets, delay convoys and sometimes make a shadowing
// squadron lose its quarry in the storm"). Behind its own switch (stage.json 'heavySeas', the flag 'heavySeas'): off,
// no sea has a gale, nothing here is read, and every war is the war without it.
//
// A gale blows over a sea in a month when the sky dealt over the sea's weather zone is closed (weather.ts dealtSky: the
// same deal the weather's own switch reads, so with weather on a sea in a gale has nothing flying over it either) and the
// sea's own draw falls under HEAVY.share. The closed sky's odds are highest in winter and in the northern oceans, so the
// gales are too. Every number is in data/campaign/heavyseas.json. A gale:
//
//   slows fleets     a fleet can sail into a sea in a gale but not on through it this month (navy.ts fleetReach), like
//                    a jam on the motorway: you can drive into it, but you will not come out the far side this month;
//   delays convoys   each convoy route to a nation through a sea in a gale costs it HEAVY.convoy of the month's
//                    production, at most HEAVY.convoyMost (navy.ts seaIncomeLoss), and a shipping lane through one loses
//                    HEAVY.lane more of its cargoes (navy.ts laneLoss);
//   loses shadows    a shadow whose quarry is in a gale loses it HEAVY.contact of the time (shadow.ts shadowsFollow).
//
// The same for every country. The draws are the campaign's seed's own, never the game's dice, so a month's gales are
// the same however often it is played.

import data from '../../data/campaign/heavyseas.json';
import navyData from '../../data/campaign/navy.json';
import { Rng, hashString } from '../core/rng';
import { headline } from './game';
import { seaWhat, seaWhatList } from './sea-words';
import { dealtSky, zoneOfSea } from './weather';
import type { Fleet, Game } from './types';

export const HEAVY = data as unknown as { share: number; convoy: number; convoyMost: number; lane: number; contact: number };
const CONVOYS = (navyData as unknown as { convoys: { id: string; to: string; zones: string[] }[] }).convoys;

/** True where gales blow at sea: the grand campaign, with the switch on. */
export const heavySeasOn = (g: Game): boolean => g.flags.open === 1 && g.flags.heavySeas === 1;

/** Turns heavy seas on or off in a campaign under way. Nothing is kept: a month's gales are dealt from the seed. */
export function setHeavySeas(g: Game, on: boolean): void {
  if (on) g.flags.heavySeas = 1;
  else delete g.flags.heavySeas;
}

const NONE: ReadonlySet<number> = new Set<number>();
const dealt = new WeakMap<object, { key: string; seas: Set<number> }>();

/** The seas in a gale this month (or in `turn`). Empty while the switch is off. */
export function galeSeas(g: Game, turn = g.turn): ReadonlySet<number> {
  if (!heavySeasOn(g)) return NONE;
  const key = `${g.seed}:${turn}:${g.seas.length}`;
  const hold = dealt.get(g.seas);
  if (hold && hold.key === key) return hold.seas;
  const seas = new Set<number>();
  for (const z of g.seas) {
    if (dealtSky(g, zoneOfSea(g, z.id), turn) !== 'closed') continue;
    if (new Rng(hashString(`gale:${g.seed}:${turn}:${z.key}`)).next() < HEAVY.share) seas.add(z.id);
  }
  dealt.set(g.seas, { key, seas });
  return seas;
}

/** True where a gale blows over sea `z` this month. */
export const inGale = (g: Game, z: number): boolean => z >= 0 && galeSeas(g).has(z);

/** The share of nation `nat`'s production its convoys are late with this month: HEAVY.convoy for each route through a gale. */
export function galeConvoyLoss(g: Game, nat: string): number {
  const gales = galeSeas(g);
  if (!gales.size) return 0;
  let s = 0;
  for (const r of CONVOYS) {
    if (r.to !== nat) continue;
    if (r.zones.some((k) => gales.has(g.seas.findIndex((z) => z.key === k)))) s += HEAVY.convoy;
  }
  return Math.min(HEAVY.convoyMost, s);
}

/** What a gale adds to the share of a shipping lane's cargoes lost through seas `zones` this month. */
export const galeLaneLoss = (g: Game, zones: number[]): number => (zones.some((z) => inGale(g, z)) ? HEAVY.lane : 0);

/** The line for a fleet's panel where it lies in a sea in a gale, or ''. */
export function galeLine(g: Game, f: Fleet): string {
  const z = f.stance === 'port' ? f.home : f.zone;
  if (!inGale(g, z)) return '';
  return `A gale blows over ${seaWhat(g.seas[z].name)} this month: ships can sail into it, but not on through it.`;
}

/**
 * The player's month opens: one line of news for the gales over the seas his fleets lie in or are based on, or that
 * his convoy routes cross. Nothing where there are none.
 */
export function galeNews(g: Game, nat: string): void {
  const gales = galeSeas(g);
  if (!gales.size || nat !== g.human) return;
  const his = new Set<number>();
  for (const f of g.fleets) {
    if (f.nat !== nat || !Object.values(f.ships).some((n) => n > 0)) continue;
    for (const z of [f.zone, f.home]) if (gales.has(z)) his.add(z);
  }
  for (const r of CONVOYS) if (r.to === nat) for (const k of r.zones) { const z = g.seas.findIndex((x) => x.key === k); if (gales.has(z)) his.add(z); }
  if (!his.size) return;
  const names = [...his].sort((a, b) => a - b).map((z) => g.seas[z].name);
  const late = galeConvoyLoss(g, nat) > 0 ? ' Convoys through them are late, and some of the month\'s cargoes with them.' : '';
  headline(g, 'info', `Gales this month over ${seaWhatList(names)}: ships can sail into ${names.length > 1 ? 'them' : 'it'} but not on through ${names.length > 1 ? 'them' : 'it'}.${late}`, nat);
}
