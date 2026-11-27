// The highest award.
//
// Brian, 20 November 2026: "do the same bond tour or train new recruits for people who receive the medal of honor or
// what ever the highest award is for military service for each nation".
//
// When a unit earns an honour (stories.ts), a man of it may be given his nation's highest award for valour: the Medal
// of Honor, the Victoria Cross, Hero of the Soviet Union, the Knight's Cross and the rest (the historian's list, in
// data/campaign/stories.json under 'valour'). He is on his unit's record from that day. The player can leave him with
// his unit or send him home, as he can a bomber crew that has flown its tour (bombers.ts): on a war bond tour, which
// pays the treasury every month for five to ten months, or to train recruits, so that every later draft reaches the
// front better trained.
//
// The stories' one rule holds: every nation's holders are on the record, so the whole war has them, but only the
// player's can be sent anywhere and nothing changes a number for a computer-run nation. Whether an honour brings the
// award, and who the man is, are drawn from a stream of the honour's own, never from the dice of the war.

import budget from '../../data/campaign/budget.json';
import data from '../../data/campaign/stories.json';
import { Rng, hashString } from '../core/rng';
import { thePlace, theUnit } from './bombers';
import { headline } from './game';
import { aceSchool, schoolPilots, unitsDecide } from './units';
import type { AirFleet, Award, Cash, Corps, Fleet, Game, Hero, UnitKind } from './types';

export const VALOUR = (data as unknown as {
  valour: {
    awards: Record<string, string>;
    given: Record<string, string>;
    since: Record<string, number>;
    chance: number[];
    who: Record<string, string[]>;
    bonds: { pp: number; months: [number, number] };
    school: { steps: number };
  };
}).valour;
const VETERANS = (data as unknown as { veterans: { perSchool: number; maxSchools: number } }).veterans;
const PILOTS = (data as unknown as { cash: { pilots: number } }).cash.pilots;

/** A nation's highest award for valour as a sentence has it, in the month 'turn': 'the Medal of Honor'. */
export function awardName(nat: string, turn: number): string {
  const own = VALOUR.awards[nat];
  return own && turn >= (VALOUR.since[nat] ?? -Infinity) ? own : VALOUR.awards._default;
}

/** The same as a title: 'Medal of Honor', 'Hero of the Soviet Union'. */
export function awardTitleOf(nat: string, turn: number): string {
  const s = awardName(nat, turn).replace(/^the /, '');
  return s.charAt(0).toUpperCase() + s.slice(1);
}

/** How the news says it was given: 'awarded the Medal of Honor', 'made a Hero of the Soviet Union'. */
function given(nat: string, turn: number): string {
  const name = awardName(nat, turn);
  const own = name === VALOUR.awards[nat] ? VALOUR.given[nat] : undefined;
  return (own ?? VALOUR.given._default).replace('{award}', name);
}

const unitFor = (g: Game, kind: UnitKind, id: number): Corps | AirFleet | Fleet | undefined =>
  kind === 'corps' ? g.corps.find((c) => c.id === id && !c.dead) : kind === 'air' ? g.airFleets.find((f) => f.id === id) : g.fleets.find((f) => f.id === id);

const placeOf = (g: Game, h: Hero): string => (h.at < 0 ? '' : h.kind === 'fleet' ? `the ${g.seas[h.at]?.name ?? 'sea'}` : thePlace(g.regions[h.at]?.name ?? ''));

/**
 * What a holder did, in a line: 'A sergeant of the 9th Corps has been awarded the Medal of Honor for valour at Tobruk.'
 * A title is 'made': 'A pilot of the 3rd Fighter Division has been made a Hero of the Soviet Union for valour at Kursk.'
 */
export function heroWords(g: Game, h: Hero): string {
  const at = placeOf(g, h);
  const how = given(h.nat, h.turn);
  // A nation with no award of its own named is given 'the highest decoration for valour', which says what it was for.
  return `${h.who} of ${theUnit(h.name)} has been ${how}${how.endsWith('for valour') ? '' : ' for valour'}${at ? (h.kind === 'fleet' ? ` in ${at}` : ` at ${at}`) : ''}.`;
}

/** The holders of the award in one unit, oldest first. */
export const heroesOf = (g: Game, kind: UnitKind, unit: number): Hero[] => (g.stories.heroes ?? []).filter((h) => h.kind === kind && h.unit === unit);

/** The player's holders he has not yet decided about, whose units still stand. */
export const heroesWaiting = (g: Game): Hero[] => (g.human ? (g.stories.heroes ?? []).filter((h) => h.nat === g.human && !h.sent && !!unitFor(g, h.kind, h.unit)) : []);

/** Whose words fit a man of this unit: a tank corps' crews, a bomber group's airmen, a fleet's sailors. */
function arm(kind: UnitKind, u: Corps | AirFleet | Fleet): string {
  if (kind === 'fleet') return 'fleet';
  if (kind === 'air') return (u as AirFleet).bmr > (u as AirFleet).ftr ? 'bmr' : 'ftr';
  return (u as Corps).kind === 'arm' ? 'arm' : 'inf';
}

/**
 * An honour has been given to a unit (stories.ts award): a man of it may be given his nation's highest award for it,
 * the likelier the greater the honour. Told to the player when the man is one of his own. Returns the holder, or null.
 */
export function decorate(g: Game, a: Award, fame: number, u: Corps | AirFleet | Fleet): Hero | null {
  const rng = new Rng(hashString(`valour:${g.seed}:${a.id}`));
  if (!rng.chance(VALOUR.chance[Math.min(VALOUR.chance.length - 1, Math.max(0, fame))] ?? 0)) return null;
  const all = (g.stories.heroes ??= []);
  const h: Hero = { id: all.length + 1, turn: g.turn, nat: a.nat, kind: a.kind, unit: a.unit, name: a.name, who: rng.pick(VALOUR.who[arm(a.kind, u)] ?? VALOUR.who.inf), at: a.at, award: a.id };
  all.push(h);
  if (a.nat === g.human) headline(g, 'honour', `${heroWords(g, h)} You can keep them with their unit, or send them home from its record, on a war bond tour or to train recruits.`, a.nat, a.kind === 'fleet' || a.at < 0 ? undefined : a.at);
  return h;
}

/** Why a holder cannot be sent home this way, or '' when he can. */
export function whyNotSend(g: Game, h: Hero, how: Cash): string {
  if (h.nat !== g.human) return 'This is not your unit.';
  if (h.sent) return h.sent === 'bonds' ? 'Already home on a war bond tour.' : 'Already home training recruits.';
  const u = unitFor(g, h.kind, h.unit);
  if (!u) return 'The unit no longer exists.';
  if (h.kind === 'corps' && !(u as Corps).supplied) return 'The corps is cut off. There is no way home.';
  if (how === 'school' && (g.stories.schools[h.nat] ?? 0) >= VETERANS.maxSchools) return 'The schools have all the instructors they can use.';
  return '';
}

/** What sending a holder home would bring, in words, for the card. */
export function sendWorth(h: Hero, how: Cash): string {
  const V = VALOUR;
  if (how === 'bonds') return `+${(V.bonds.pp * budget.scale).toLocaleString('en-US')} a month for the treasury, for ${V.bonds.months[0]} to ${V.bonds.months[1]} months`;
  const better = `every later draft and new corps is trained ${Math.round(VETERANS.perSchool * V.school.steps * 100 * 10) / 10} points better`;
  return h.kind === 'air' ? `${PILOTS} more trained pilots at once, and ${better}` : better;
}

/**
 * The player sends a holder of the award home: on a war bond tour, where the crowds he draws pay the treasury every
 * month until the tour is over, or to train recruits, so that every later draft is better trained.
 */
export function sendHero(g: Game, id: number, how: Cash): boolean {
  const h = (g.stories.heroes ?? []).find((x) => x.id === id);
  if (!h || whyNotSend(g, h, how)) return false;
  const V = VALOUR, title = awardName(h.nat, h.turn);
  h.sent = how;
  h.sentOn = g.turn;
  // A decoration has a holder; a title is what its holder is ('a Hero of the Soviet Union').
  const who = `${h.who} of ${theUnit(h.name)}, ${/^Hero /.test(title) ? `a ${title}` : `holder of ${title}`},`;
  if (how === 'bonds') {
    h.left = V.bonds.months[0] + new Rng(hashString(`valour:tour:${g.seed}:${h.id}`)).int(V.bonds.months[1] - V.bonds.months[0] + 1);
    headline(g, 'info', `${who} goes home on a war bond tour, which will bring in ${(V.bonds.pp * budget.scale).toLocaleString('en-US')} a month for ${h.left} months.`, h.nat);
  } else {
    // Under Units decide an air hero trains pilots: the flying school takes his step, not the land drafts' schools (#2 form Part 2 section 6).
    if (h.kind === 'air' && unitsDecide(g)) { aceSchool(g, h.nat); schoolPilots(g, h.nat, PILOTS); }
    else {
      g.stories.schools[h.nat] = Math.min(VETERANS.maxSchools, (g.stories.schools[h.nat] ?? 0) + V.school.steps);
      if (h.kind === 'air') { const p = g.airPool[h.nat]; if (p) p.pilots += PILOTS; }
    }
    headline(g, 'info', `${who} goes home to train recruits. From now on every draft reaches the front better trained.`, h.nat);
  }
  return true;
}

/** The player's holders on a war bond tour pay the treasury for the month, and each tour is a month nearer its end. Returns what they paid. */
export function valourMonth(g: Game, nat: string): number {
  if (nat !== g.human) return 0;
  let pay = 0;
  for (const h of g.stories.heroes ?? []) {
    if (h.nat !== nat || h.sent !== 'bonds' || !h.left) continue;
    pay += VALOUR.bonds.pp;
    h.left--;
  }
  g.nations[nat].pp += pay;
  return pay;
}
