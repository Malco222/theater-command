/**
 * Morale and rest (the grand campaign).
 *
 * Every corps carries its morale, 0 to 1, beside its strength and its rest (the cohesion it has always had). Morale
 * rises with victories, on home soil, with friends close by and with a quiet month out of the line. It falls with
 * defeats, heavy losses, being cut off, going unpaid and standing alone in front of the enemy. A corps in good heart
 * hits harder and holds together longer in a fight; a shaken one does neither.
 *
 * Each change is written down with its reason, so both interfaces can say why a corps feels as it does. What a
 * corps has felt since its nation's last monthly reckoning is kept, and what it felt in the month before that, and
 * nothing older.
 *
 * A historical scenario keeps no morale: its corps carry none, every multiplier here is exactly 1 for them, and so
 * its wars come out as they did before this rule was written. The numbers and words are in data/campaign/morale.json.
 */
import data from '../../data/campaign/morale.json';
import { clamp } from '../core/math';
import type { Corps, Game, Region } from './types';
import { allied, atWar } from './game';
import { cmdRally } from './commanders';

export const MORALE = data as unknown as {
  start: number;
  level: number;
  drift: number;
  battle: { won: number; decisive: number; repulsed: number; thrown: number; held: number; lost: number; routed: number; bloody: number; bloodyAt: number; capital: number; capitalLost: number };
  month: { home: number; beside: number; near: number; alone: number; rested: number; cutOff: number; unpaid: number; winter: number; general: number };
  fight: { span: number; steady: number };
  rested: { org: number; drafts: number };
  words: [number, string][];
  restWords: [number, string][];
  why: Record<string, string>;
};

/** True where morale is kept: the grand campaign. */
export const moraleRules = (g: Game): boolean => g.flags.open === 1;

/** The reasons that belong to the monthly reckoning: written afresh every month, never carried over. */
const MONTHLY = new Set(['home', 'friends', 'alone', 'rested', 'cutOff', 'unpaid', 'winter', 'general', 'time']);

const r3 = (x: number): number => Math.round(x * 1000) / 1000;

const named = (table: [number, string][], x: number): string => {
  let out = table[0][1];
  for (const [from, name] of table) if (x >= from) out = name;
  return out;
};

/** What a level of morale is called. */
export const moraleWord = (m: number): string => named(MORALE.words, m);
/** What a level of rest is called. */
export const restWord = (org: number): string => named(MORALE.restWords, org);

/** Gives every corps that has none its starting morale. Called when a grand campaign opens, and when an older save of one is loaded. */
export function openMorale(g: Game): void {
  if (!moraleRules(g)) return;
  for (const c of g.corps) if (c.mor === undefined) c.mor = MORALE.start;
}

/** A new corps raised in the grand campaign starts with the morale of a fresh formation. */
export function raiseMorale(g: Game, c: Corps): void {
  if (moraleRules(g)) c.mor = MORALE.start;
}

/** Moves a corps' morale and writes down why. Nothing happens to a corps that keeps no morale. */
export function feel(c: Corps, why: string, delta: number): void {
  if (c.mor === undefined || !delta) return;
  c.mor = r3(clamp(c.mor + delta, 0, 1));
  const list = (c.mw ??= []);
  const e = list.find((x) => x[0] === why && x[2] === 0);
  if (e) e[1] = r3(e[1] + delta);
  else list.push([why, r3(delta), 0]);
}

/** What morale is worth in a fight: a multiplier on the strength of the corps, 1 for a corps that keeps none. */
export const heart = (c: Corps): number => (c.mor === undefined ? 1 : 1 + (c.mor - 0.5) * MORALE.fight.span);

/** How fast a corps loses its cohesion in a fight: faster with no heart, slower in good heart, 1 for a corps that keeps none. */
export const nerve = (c: Corps): number => (c.mor === undefined ? 1 : 1 + (0.5 - c.mor) * MORALE.fight.steady);

/** True for a corps that has spent the whole month in supply without marching or fighting (the grand campaign). Asked before the month's moves are handed back. */
export const restedMonth = (g: Game, c: Corps): boolean =>
  moraleRules(g) && c.supplied && !c.fought && c.ap >= c.apMax && c.kind !== 'fort' && c.born !== g.turn;

/** True for a corps that will count as rested if it is left alone for the remainder of this month. */
export const resting = (g: Game, c: Corps): boolean => moraleRules(g) && c.supplied && !c.fought && c.ap >= c.apMax && c.kind !== 'fort';

/** How much faster the drafts reach a corps that has been out of the line all month. */
export const restedDrafts = (g: Game, c: Corps): number => (restedMonth(g, c) ? MORALE.rested.drafts : 1);

/** The extra rest a corps gets back for a whole month out of the line. */
export const restedOrg = (g: Game, c: Corps): number => (restedMonth(g, c) ? MORALE.rested.org : 0);

// ---- Battle ---------------------------------------------------------------------------------

/**
 * What a battle does to the heart of the corps that fought it. 'odds' is the result the dice gave, 'win' whether the
 * attack carried the ground, and 'lost' the share of its men each corps lost in the fight, by corps id.
 */
export function moraleBattle(g: Game, attackers: Corps[], defenders: Corps[], odds: number, win: boolean, lost: Map<number, number>, capital: boolean): void {
  if (!moraleRules(g)) return;
  const B = MORALE.battle;
  for (const c of attackers) {
    if (win) feel(c, odds >= 2 ? 'decisive' : 'won', odds >= 2 ? B.decisive : B.won);
    else feel(c, odds < 0.8 ? 'thrown' : 'repulsed', odds < 0.8 ? B.thrown : B.repulsed);
    if ((lost.get(c.id) ?? 0) >= B.bloodyAt) feel(c, 'bloody', B.bloody);
    if (win && capital) feel(c, 'capital', B.capital);
  }
  for (const c of defenders) {
    if (win) feel(c, odds >= 2 ? 'routed' : 'lost', odds >= 2 ? B.routed : B.lost);
    else feel(c, 'held', B.held);
    if ((lost.get(c.id) ?? 0) >= B.bloodyAt) feel(c, 'bloody', B.bloody);
  }
}

/** The day a nation's own capital changes hands, every corps it has feels it. */
export function moraleCapital(g: Game, r: Region, old: string): void {
  if (!moraleRules(g) || !r.cap || r.owner !== old || r.ctrl === old || !g.nations[old]?.alive) return;
  if (g.nations[old].capital !== r.id) return;
  for (const c of g.corps) if (c.nat === old && !c.dead) feel(c, 'capitalLost', MORALE.battle.capitalLost);
}

// ---- The monthly reckoning ------------------------------------------------------------------

/**
 * A corps' month, at the start of its nation's turn: where it stands, who stands with it, and whether it has had any
 * rest. Called before the month's moves are handed back, so that 'rested' still knows what the corps did.
 */
export function moraleMonth(g: Game, c: Corps, facts: { owed: number; cold: boolean }): void {
  if (c.mor === undefined) return;
  // What was felt before the last reckoning is forgotten. Battles since then are carried one month more.
  c.mw = (c.mw ?? []).filter((e) => e[2] === 0 && !MONTHLY.has(e[0])).map((e) => [e[0], e[1], 1]);
  const M = MORALE.month;
  const here = g.regions[c.reg];
  feel(c, 'time', (MORALE.level - c.mor) * MORALE.drift);
  if (here.owner === c.nat && !here.overseas) feel(c, 'home', M.home);
  const friend = (u: Corps): boolean => u !== c && !u.dead && allied(g, u.nat, c.nat);
  const beside = here.units.some(friend);
  let near = false, foe = false;
  for (const j of here.nb) {
    for (const u of g.regions[j].units) {
      if (friend(u)) near = true;
      else if (atWar(g, u.nat, c.nat)) foe = true;
    }
  }
  if (beside || near) feel(c, 'friends', (beside ? M.beside : 0) + (near ? M.near : 0));
  else if (foe) feel(c, 'alone', M.alone);
  if (restedMonth(g, c)) feel(c, 'rested', M.rested);
  if (!c.supplied) feel(c, 'cutOff', M.cutOff);
  if (facts.owed > 0) feel(c, 'unpaid', M.unpaid * facts.owed);
  if (facts.cold) feel(c, 'winter', M.winter);
  if (cmdRally(g, c) > 1) feel(c, 'general', M.general);
  if (!c.mw.length) delete c.mw;
}

// ---- What both interfaces read --------------------------------------------------------------

export interface MoodReason {
  /** The reason's key in morale.json. */
  key: string;
  text: string;
  /** What it moved morale by, in points of 0 to 1. */
  delta: number;
  /** True if it comes from the month before the last reckoning. */
  earlier: boolean;
}

export interface Mood {
  /** Morale, 0 to 1, or null where no morale is kept (a historical scenario). */
  morale: number | null;
  moraleWord: string;
  trend: 'rising' | 'falling' | 'steady';
  /** Rest, 0 to 1: the cohesion a corps has always had, under the name the player reads. */
  rest: number;
  restWord: string;
  /** One sentence on how the corps feels and why. */
  line: string;
  /** One sentence on its rest. */
  restLine: string;
  why: MoodReason[];
  /** True if the corps will count as rested, should it neither march nor fight this month. */
  resting: boolean;
}

const lower = (s: string): string => s[0].toLowerCase() + s.slice(1);
const listed = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

/** How a corps feels, and why: morale and rest on a scale of 0 to 1, each with its word and a sentence. */
export function corpsMood(g: Game, c: Corps): Mood {
  const rest = clamp(c.org, 0, 1);
  const quiet = resting(g, c);
  const restLine = !c.supplied ? 'Cut off from supply: it gets no rest at all.'
    : c.fought ? `${restWord(rest)}: it has fought this month, and will get back less of its rest for it.`
    : quiet ? `${restWord(rest)}: left where it stands for the month, it rests, takes in drafts half as fast again and recovers its spirits.`
    : `${restWord(rest)}: it has marched this month. A month where it neither marches nor fights would rest it properly.`;
  if (c.mor === undefined) {
    return { morale: null, moraleWord: '', trend: 'steady', rest, restWord: restWord(rest), line: '', restLine: !c.supplied ? restLine : `${restWord(rest)}.`, why: [], resting: false };
  }
  const why: MoodReason[] = (c.mw ?? [])
    .filter((e) => Math.abs(e[1]) >= 0.005)
    .map((e) => ({ key: e[0], text: MORALE.why[e[0]] ?? e[0], delta: e[1], earlier: e[2] === 1 }))
    .sort((a, b) => Number(a.earlier) - Number(b.earlier) || Number(a.key === 'time') - Number(b.key === 'time') || Math.abs(b.delta) - Math.abs(a.delta));
  // The trend is what has happened since the last reckoning, the reckoning included. Morale that has nowhere left to go is steady.
  const net = why.filter((w) => !w.earlier).reduce((s, w) => s + w.delta, 0);
  const trend: Mood['trend'] = net >= 0.02 && c.mor < 0.995 ? 'rising' : net <= -0.02 && c.mor > 0.005 ? 'falling' : 'steady';
  const word = moraleWord(c.mor);
  const told = why.filter((w) => !w.earlier && w.key !== 'time' && (trend === 'falling' ? w.delta < 0 : trend === 'rising' ? w.delta > 0 : true)).slice(0, 3).map((w) => lower(w.text));
  // With nothing but time at work, high spirits are settling back or low ones are mending.
  const line = trend === 'steady' ? `${word}, and holding steady${told.length ? `: ${listed(told)}` : ''}.`
    : told.length ? `${word} and ${trend}: ${listed(told)}.`
    : trend === 'falling' ? `${word}, and settling back towards its usual temper.`
    : `${word}, and recovering with time.`;
  return { morale: c.mor, moraleWord: word, trend, rest, restWord: restWord(rest), line, restLine, why, resting: quiet };
}

/** The morale of a force as one number: its corps weighed by their divisions. Null where no morale is kept. */
export function forceMorale(cs: Corps[]): number | null {
  let sum = 0, n = 0;
  for (const c of cs) if (c.mor !== undefined) { sum += c.mor * c.divs; n += c.divs; }
  return n > 0 ? sum / n : null;
}
