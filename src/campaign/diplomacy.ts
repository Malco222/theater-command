// Diplomacy: the alignment track. Every nation stands somewhere between Allied member and
// Axis member. Neutrals have a public opinion that drifts with their own leanings and with
// whoever is winning the war; players spend political capital to court them; and history's
// own moves are events that fire only if their conditions still hold.
// The numbers and the events live in data/campaign/diplomacy.json.

import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import data from '../../data/campaign/diplomacy.json';
import relationsData from '../../data/campaign/relations.json';
import type { Align, Game, Nation, Region } from './types';
import { fits, roomLeft, roomForDivision } from './islands';
import { addCorps, allied, atWar, bloc, capture, declareWar, endVoyages, headline, keptStoresHolder, newsName, patronPress, relocate, removeCorps, standsByWhom, theName, theWords, warning, roomless } from './game';
import { leagueMonth, ownPath, playersOwn, spared, standsWith } from './league';
import { turnToDate } from '../core/ticket';
import { alertNation, traitsMonth } from './traits';
import { techMonth } from './tech';
import { fleetsOnFall, gateShutSince } from './navy';
import { airOnFall } from './air';
import { recordFall } from './liberation';
import { mayJoin, onBook, stopDef, stopOf } from './history';
import { hashString } from '../core/rng';
import worldData from '../../data/campaign/world.json';
import { pressData, warWorkMonth, worldMonth } from './world';
import { chartDist2 } from './wrap';
import { footingMonth } from './footing';
import { weatherMonth } from './weather';
import { guarantorsOf, type Guarantor } from './guarantees';
import { stationed } from './far';
import { liftMonth, pacificOpening, pacificSides, pactBinds } from './pacific';

export interface DiploEvent {
  id: string; from: number; to: number; if?: string[]; text: string;
  /** 'history': a decision history made for a nation, left to that nation in free play. 'open': only in free play. */
  mode?: 'history' | 'open';
  annex?: { by: string; nations: string[] };
  cede?: { from: string; to: string; regions: string[] };
  /**
   * Ground an occupier has left behind him: every named region 'from' still holds with none of his troops in it
   * passes to the first of 'to' still at war with him. Places he still garrisons stay his.
   */
  liberate?: { from: string; to: string[]; regions: string[] };
  spawn?: [string, string, string, number, string][];
  opinion?: Record<string, number>;
  join?: { bloc: 'allies' | 'axis'; nations: string[]; minOpinion: number };
  /** Pairs that go to war when the event fires: the first of each declares on the second. */
  war?: [string, string][];
  /**
   * 'war': the event's words are the news of the first war it brings. The declaration is told in them, under the
   * declarer's flag, in place of the plain line ("one story, one band": the UI director's words for Rashid Ali).
   */
  tell?: 'war';
  /** Its news is a band of its own, under this nation's flag, for that nation's player and its friends: the first sentence is its head. */
  band?: string;
  /**
   * Countries of the event whose computer-run corps keep to their own ground from the day it fires: a declaration,
   * not a front (Turkey, February 1945; ruling 228, b528). A player's country is never bound.
   */
  stay?: string[];
  /** Countries that have stood aside until now and enter the story here, at this place on the alignment track. */
  wake?: { nations: string[]; align: Align };
  /**
   * Countries on the map and at peace, outside both alliances, that take this place on the alignment track as the
   * event fires (the wider world, where America is there from the first day). A small country in the player's
   * hands is left to its player.
   */
  enter?: { nations: string[]; align: Align };
  /**
   * A country that ends, its ground shared out. Each share is [who takes it, the regions]: the first taker on the
   * list still in the war gets every named region the country still holds. Ground someone else already holds stays theirs.
   * 'marchIn': the takers march in as the terms are made (Iraq and Iran, 1941), so in the grand campaign a share goes
   * to its taker whether or not its army stood there before.
   */
  partition?: { nation: string; shares: [string[], string[]][]; occupied?: boolean; marchIn?: boolean };
  /** The history campaigns, by the month the seat is taken, whose card states this event as the stake: only there does it fire on the country the player leads (league.ts, spared). */
  stake?: number[];
  /** Pairs of nations that make peace: each hands back the other's ground and takes its troops home. */
  peace?: [string, string][];
  /** Countries whose armies are remade: from now on these ratings stand in place of the ones they began the war with. */
  reform?: Record<string, Partial<Pick<Nation, 'quality' | 'air' | 'slowCommand' | 'maxAttack'>>>;
  /** News of something won in the month just ended: the dispatch is dated to that month's last week, not to the new one. */
  past?: boolean;
  /**
   * 'close': read as a month closes, once every country has moved, and dated that month (closeDiplomacy). The far
   * side's entry of December 1941 is: Japan moves last in a month, and its blow came before Germany's declaration.
   * 'war': read the day any war is declared as well as when a month opens (warDiplomacy): Siam lets Japan's army
   * pass the day Japan strikes Britain, before the landings of that month sail.
   */
  when?: 'close' | 'war';
  /** Other words for the news: the first row whose conditions hold as the event fires gives them in place of 'text'. */
  says?: { if: string[]; text: string }[];
  /** Events that follow from this one: each is read the day this one fires, where its own month and conditions have come. */
  then?: string[];
}

export const DIPLOMACY = data as unknown as {
  leanAt: number; backAt: number; joinAt: number; pull: number; momentumPer: number; stepEvery: number;
  leaders: Record<'allies' | 'axis', string>; majors: string[];
  pcIncome: Record<string, number>; pcCap: number;
  court: { cost: number; opinion: number; home: number };
  americas?: { backer: string; of: string[]; join: Record<string, number>; readiness: Record<string, number>; most: number; months: number };
  tradeIncome: number; resist: Record<string, number>;
  home: Record<string, number>; bandwagon: Record<string, number>;
  events: DiploEvent[];
};

export const TRACK: Align[] = ['allies', 'allies_cobelligerent', 'leaning_allies', 'independent', 'leaning_axis', 'axis_cobelligerent', 'axis'];
export const TRACK_LABEL: Record<Align, string> = {
  allies: 'Allied member', allies_cobelligerent: 'Allied co-belligerent', leaning_allies: 'Leaning Allies', independent: 'Independent',
  leaning_axis: 'Leaning Axis', axis_cobelligerent: 'Axis co-belligerent', axis: 'Axis member',
};

/** How a nation stands to the one looking at it: its own, an ally, an enemy, or neither. */
export type Standing = 'own' | 'ally' | 'enemy' | 'neutral';

/**
 * Where a nation stands for the one looking at it. An enemy is anyone the viewer is at war with, whatever bloc it
 * belongs to. An ally is a member or co-belligerent of the viewer's own bloc. Everyone else is neutral to the viewer:
 * that includes a member of the other bloc it is not yet at war with, as Italy is to Britain in 1939.
 */
export function standing(g: Game, viewer: string, nat: string): Standing {
  if (viewer === nat) return 'own';
  if (atWar(g, viewer, nat)) return 'enemy';
  return allied(g, viewer, nat) ? 'ally' : 'neutral';
}

/** A nation's place on the track as it reads in a sentence: "Finland is fighting alongside the Axis". */
export const TRACK_PHRASE: Record<Align, string> = {
  allies: 'a member of the Allies', allies_cobelligerent: 'fighting alongside the Allies', leaning_allies: 'neutral, and leaning toward the Allies',
  independent: 'neutral', leaning_axis: 'neutral, and leaning toward the Axis', axis_cobelligerent: 'fighting alongside the Axis', axis: 'a member of the Axis',
};

/** Where a nation stands on the track: -3 Allied member ... 0 independent ... +3 Axis member. */
export const step = (n: Nation): number => TRACK.indexOf(n.align) - 3;
const sideOf = (b: 'allies' | 'axis'): number => (b === 'axis' ? 1 : -1);
const clamp = (x: number, a: number, b: number): number => Math.max(a, Math.min(b, x));

/** The opinion a nation starts with and drifts back to. */
export function homeOpinion(id: string, align: Align): number {
  const s = TRACK.indexOf(align) - 3;
  if (Math.abs(s) >= 2) return s > 0 ? 100 : -100;
  return DIPLOMACY.home[id] ?? s * 40;
}

/** The great powers. On the wider world (world.ts) those whose homeland is now on the map stand beside history's European five. */
const WIDE_MAJORS = [...DIPLOMACY.majors, ...(worldData as unknown as { great: string[] }).great];
export const majorsOf = (g: Game): string[] => (g.flags.world === 1 ? WIDE_MAJORS : DIPLOMACY.majors);

export const pcIncome = (nat: string, g?: Game): number => ((g ? majorsOf(g) : DIPLOMACY.majors).includes(nat) ? DIPLOMACY.pcIncome._major : DIPLOMACY.pcIncome._minor);

/**
 * Who is winning, from -1 (the Allies) to +1 (the Axis): the victory points each bloc has
 * taken from countries outside it.
 */
export function momentum(g: Game): number {
  let axis = 0, allies = 0;
  for (const r of g.regions) {
    if (!r.vp || r.ctrl === r.owner) continue;
    const holder = bloc(g.nations[r.ctrl]);
    if (!holder || holder === bloc(g.nations[r.owner])) continue;
    if (holder === 'axis') axis += r.vp; else if (holder === 'allies') allies += r.vp;
  }
  return clamp((axis - allies) / DIPLOMACY.momentumPer, -1, 1);
}

/** Neutrals that lean toward this nation's bloc and trade with it. Only the major powers of a bloc benefit. */
export function tradePartners(g: Game, nat: string): Nation[] {
  const b = bloc(g.nations[nat]);
  if (!b || b === 'league' || !majorsOf(g).includes(nat)) return [];
  const want = sideOf(b);
  return Object.values(g.nations).filter((n) => n.alive && !n.dormant && step(n) === want);
}

/** A neutral that can still be courted: alive, at peace and not yet a member of either bloc. */
export const courtable = (n: Nation): boolean => n.alive && !n.dormant && !n.league && !n.atWar.length && Math.abs(step(n)) <= 1;

/** How far one round of courting moves a neutral. Moves that match its public opinion go further. */
export function receptive(target: Nation, b: 'allies' | 'axis'): number {
  return clamp(1 + (target.home * sideOf(b)) / 60, 0.4, 1.6) * (DIPLOMACY.resist[target.id] ?? 1);
}

// ---- The Axis courts the Americas (ruling 223, Brian's ask of 17 November 2026) -------------------------------

const AMERICAS = DIPLOMACY.americas;

/** True for a neutral of the Americas the Axis weighs higher once the backer is drawn in (diplomacy.json 'americas'). */
export const americasNeutral = (g: Game, id: string): boolean => !!AMERICAS && g.flags.world === 1 && AMERICAS.of.includes(id) && !!g.nations[id];

/** True once the backer (the United States) is at war with the Axis, or leans to or stands with the Allies: the Axis then courts its neighbours. */
export function americasOpen(g: Game): boolean {
  if (!AMERICAS || g.flags.world !== 1) return false;
  const U = g.nations[AMERICAS.backer];
  if (!U?.alive) return false;
  return step(U) < 0 || U.atWar.some((e) => bloc(g.nations[e]) === 'axis');
}

/** The share of campaigns in which the Axis can win this country over, at this stop of the dial: the stop's figure times its readiness (b391). */
export const americasShare = (g: Game, id: string): number =>
  !AMERICAS ? 0 : (AMERICAS.join[String(stopOf(g))] ?? 0) * (AMERICAS.readiness[id] ?? AMERICAS.readiness._default ?? 1);

/** How many countries of the Americas have joined the Axis by its courting in this campaign. */
const americasIn = (g: Game): number => (AMERICAS?.of ?? []).filter((id) => g.flags[`americas:in:${id}`] !== undefined).length;

/** The one roll for this country and this campaign: whether the Axis can win it over. The same every month. */
export const americasWon = (g: Game, id: string): boolean => hashString(`americas:${g.seed}:${id}`) / 4294967296 < americasShare(g, id);

/** The countries of the Axis that have courted this country this month or last. */
export const axisCourters = (g: Game, id: string): Nation[] =>
  Object.values(g.nations).filter((n) => bloc(n) === 'axis' && (g.flags[`court:${n.id}:${id}`] ?? -9) >= g.turn - 1);
/** The Axis has courted this country this month or last. */
export const axisCourts = (g: Game, id: string): boolean => axisCourters(g, id).length > 0;
/** Whose envoys court it, as the words say them (the UI director's words for ruling 223): "German", or "Axis" for two or more. */
export const envoysWord = (g: Game, ids: string[]): string => {
  const who = new Set(ids.flatMap((id) => axisCourters(g, id).map((n) => n.adj)));
  return who.size === 1 ? [...who][0] : 'Axis';
};

/**
 * The backer courts this neutral at no cost while the Axis courts it (ruling 223(d)), whether or not it has joined
 * the Allies yet: in 1939 the United States only leans their way.
 */
export const freeCourt = (g: Game, by: string, target: string): boolean => {
  const B = g.nations[by];
  return !!AMERICAS && by === AMERICAS.backer && !!B?.alive && bloc(B) !== 'axis' && bloc(B) !== 'league' && americasNeutral(g, target) && axisCourts(g, target);
};

/**
 * The month's courting in the Americas (ruling 223): the envoys of the Axis leader, or of the first great power of
 * the Axis still standing if the player has the leader, go to the neutral of the Americas most ready to listen; and
 * a computer-run United States courts back, at no cost, every one the Axis courts. A player's Axis courts by hand.
 */
function americasMonth(g: Game): void {
  if (!americasOpen(g)) return;
  const axis = [DIPLOMACY.leaders.axis, ...majorsOf(g)].map((id) => g.nations[id]).find((n) => n?.alive && bloc(n) === 'axis' && n.id !== g.human);
  if (axis && !(g.human && g.nations[g.human] && bloc(g.nations[g.human]) === 'axis' && g.human === DIPLOMACY.leaders.axis)) {
    let best: Nation | null = null, top = 0;
    for (const id of AMERICAS!.of) {
      const T = g.nations[id];
      if (!T || !canCourt(g, axis.id, id).ok) continue;
      const v = receptive(T, 'axis') * (2 + step(T));
      if (v > top) { top = v; best = T; }
    }
    if (best) court(g, axis.id, best.id);
  }
  const U = g.nations[AMERICAS!.backer];
  if (U && U.id !== g.human) for (const id of AMERICAS!.of) if (freeCourt(g, U.id, id)) court(g, U.id, id);
  // A country won over opens the front at home: once the United States is at war with the Axis, so is it (223(e)).
  if (U?.alive && U.atWar.some((e) => bloc(g.nations[e]) === 'axis')) {
    for (const id of AMERICAS!.of) {
      const T = g.nations[id];
      if (T?.alive && g.flags[`americas:in:${id}`] !== undefined && bloc(T) === 'axis' && !T.atWar.includes(U.id) && id !== g.human) declareWar(g, id, U.id);
    }
  }
}


export function canCourt(g: Game, by: string, target: string): { ok: boolean; why: string } {
  const B = g.nations[by], T = g.nations[target];
  const b = B ? bloc(B) : null;
  if (!B || !T || !B.alive || (!b && !freeCourt(g, by, target)) || b === 'league') return { ok: false, why: 'Only a member of the Axis or the Allies can court neutrals.' };
  // A small country in the player's hands chooses its own path: nobody talks it round.
  if (by === target || !courtable(T) || ownPath(g, target)) return { ok: false, why: 'Not open to courting.' };
  if (g.flags[`court:${by}:${target}`] === g.turn) return { ok: false, why: 'Already courted this month.' };
  if (B.pc < DIPLOMACY.court.cost && !freeCourt(g, by, target)) return { ok: false, why: `Needs ${DIPLOMACY.court.cost} political capital.` };
  return { ok: true, why: '' };
}

/** Spends political capital to pull a neutral toward the courting nation's bloc. */
export function court(g: Game, by: string, target: string): boolean {
  if (!canCourt(g, by, target).ok) return false;
  const B = g.nations[by], T = g.nations[target];
  // (The United States courting back before it has joined the Allies pulls toward them: ruling 223(d).)
  const b = (bloc(B) ?? 'allies') as 'allies' | 'axis';
  const k = receptive(T, b) * sideOf(b);
  if (!freeCourt(g, by, target)) B.pc -= DIPLOMACY.court.cost;
  T.opinion = clamp(T.opinion + DIPLOMACY.court.opinion * k, -100, 100);
  T.home = clamp(T.home + DIPLOMACY.court.home * k, -100, 100);
  g.flags[`court:${by}:${target}`] = g.turn;
  // A neutral the player has courted is his affair: the history dial does not hold it back from joining.
  if (by === g.human && g.flags.hist !== undefined) g.flags[`courted:${target}`] = g.turn;
  if (b === 'axis' && by !== g.human && americasNeutral(g, target) && americasOpen(g)) courtsAmericas(g, B, T);
  settle(g, T);
  return true;
}

/**
 * A computer-run Axis country's envoys in a neutral of the Americas: the news says so the first month, and a country
 * the campaign's roll favours joins the Axis after so many months of it (ruling 223(a) and (b)).
 */
function courtsAmericas(g: Game, B: Nation, T: Nation): void {
  const key = `americas:months:${T.id}`, months = (g.flags[key] ?? 0) + 1;
  g.flags[key] = months;
  if (months === 1) {
    const seat = g.regions[T.capital];
    // (A player's America, which may court it back at no cost, has it among the news of home, where it is not crowded
    // out by the wars abroad: b434. Anyone else reads it among the news from elsewhere.)
    const mine = g.human === AMERICAS!.backer && freeCourt(g, g.human, T.id);
    headline(g, mine ? 'info' : 'event', `${B.adj} envoys are courting ${seat ? seat.name : T.name}.`, mine ? g.human! : T.id, seat?.id);
  }
  // (No more than one country of the Americas joins the Axis in a campaign: b391.)
  if (months >= AMERICAS!.months && americasWon(g, T.id) && americasIn(g) < AMERICAS!.most && joinBloc(g, T, 'axis')) g.flags[`americas:in:${T.id}`] = g.turn;
}

const blocName = (s: number): string => (s > 0 ? 'the Axis' : 'the Allies');

// ---- Friendships, surprise and wary neutrals (the grand campaign) -------------------------------------------

export const RELATIONS = relationsData as unknown as {
  sneak: { attack: number };
  wary: { all: { opinion: number; home: number }; friend: { opinion: number; home: number } };
  gate: { month: number; most: number };
  groups: { id: string; name: string; why: string; nations: string[] }[];
  pairs: [string, string, string][];
};

export interface Friend { nat: string; why: string }

// ---- A gate its holder has shut: who resents it (rulings 176(e) and 177(a)) -----------------------------------

const soreKey = (gate: number, nat: string): string => `gatesore:${gate}:${nat}`;

/** The side a country stands on or leans to: +1 the Axis, -1 the Allies, 0 neither (a strict neutral, or the League). */
export function sideHeld(g: Game, nat: string): number {
  const n = g.nations[nat];
  if (!n) return 0;
  const b = bloc(n);
  return b === 'axis' ? 1 : b === 'allies' ? -1 : b === 'league' ? 0 : Math.sign(step(n));
}

/** The words for that side: "the Allies", "the Axis", or '' for neither. */
export const sideName = (g: Game, nat: string): string => (sideHeld(g, nat) ? blocName(sideHeld(g, nat)) : '');

/** True for a country that would resent a gate this holder shut on its trade: at peace, outside both blocs, and the holder has a side to turn it from. */
export const wouldResent = (g: Game, holder: string, nat: string): boolean => nat !== holder && sideHeld(g, holder) !== 0 && !!g.nations[nat] && courtable(g.nations[nat]);

/** Forgets who resents a gate: it is open again, or it has changed hands. */
export function soreForget(g: Game, gate: number): void {
  for (const k of Object.keys(g.flags)) if (k.startsWith(`gatesore:${gate}:`)) delete g.flags[k];
}

/** Marks who resents a gate its holder has just shut. The list is fixed at the shutting: the countries whose trade it turned back that month. */
export function soreAtShut(g: Game, gate: number, holder: string, nats: string[]): void {
  soreForget(g, gate);
  for (const id of nats) if (wouldResent(g, holder, id)) g.flags[soreKey(gate, id)] = 0;
}

/** Who resents a gate shut now, with the points of opinion each has moved so far. */
export function soreList(g: Game, gate: number): { nat: string; so: number }[] {
  const pre = `gatesore:${gate}:`;
  return Object.keys(g.flags).filter((k) => k.startsWith(pre)).map((k) => ({ nat: k.slice(pre.length), so: g.flags[k] })).filter((x) => !!g.nations[x.nat]);
}

/**
 * The month's move for each neutral that resents a shut gate: so many points away from the side the gate's holder
 * stands on, counted against the most one shutting costs. A gate that is open again, or in other hands, is forgotten.
 */
function soreMonth(g: Game): Map<string, number> {
  const out = new Map<string, number>();
  for (const k of Object.keys(g.flags)) {
    if (!k.startsWith('gatesore:')) continue;
    const [, gi, nat] = k.split(':');
    const gate = Number(gi), gt = g.gates[gate];
    const holder = gt ? (gt.region >= 0 ? g.regions[gt.region].ctrl : gt.nation ?? '') : '';
    if (!gt || gateShutSince(g, gate) < 0) { delete g.flags[k]; continue; }
    const n = g.nations[nat], side = sideHeld(g, holder);
    if (!n || !courtable(n) || !side || g.flags[k] >= RELATIONS.gate.most) continue;
    const pts = Math.min(RELATIONS.gate.month, RELATIONS.gate.most - g.flags[k]);
    g.flags[k] += pts;
    out.set(nat, (out.get(nat) ?? 0) - side * pts);
  }
  return out;
}

/** The countries that count this one a friend, and why: its circle of neighbours first, then its single friendships. */
export function friendsOf(nat: string): Friend[] {
  const out = new Map<string, string>();
  for (const gp of RELATIONS.groups) if (gp.nations.includes(nat)) for (const id of gp.nations) if (id !== nat && !out.has(id)) out.set(id, gp.why);
  for (const [a, b, why] of RELATIONS.pairs) {
    if (a === nat && !out.has(b)) out.set(b, why);
    if (b === nat && !out.has(a)) out.set(a, why);
  }
  return [...out].map(([id, why]) => ({ nat: id, why }));
}

/** True while a nation's attacks on another's ground count as a surprise: the month it declared war on a country at peace with its side. */
export const sneakOn = (g: Game, by: string, on: string): boolean => g.flags.open === 1 && g.flags[`sneak:${by}:${on}`] === g.turn;

/** Whether a declaration of war today would catch the other country unready: it is at peace with the declarer and with all his allies. */
export function wouldSurprise(g: Game, by: string, on: string): boolean {
  const B = g.nations[on];
  // A nation whose codebreakers read the other side's signals is never caught unready.
  return g.flags.open === 1 && !!B && !alertNation(g, on) && !B.atWar.some((x) => x === by || allied(g, x, by));
}

/** One neutral's answer to an attack on a country outside both blocs: how its opinion moves and where that leaves it. */
export interface Alarm {
  nat: string;
  /** A friend of the country attacked, and the tie between them. */
  friend: boolean; why: string;
  opinion: number; home: number;
  from: Align; to: Align;
  /** True when it traded with the aggressor until now and will stop. */
  trade: boolean;
}

/** Where a neutral would stand on the track with this opinion, taking at most one step, as settle() would move it. */
function wouldStand(g: Game, n: Nation, opinion: number): Align {
  if (!courtable(n)) return n.align;
  const last = g.flags[`step:${n.id}`];
  if (last !== undefined && g.turn - last < DIPLOMACY.stepEvery) return n.align;
  const s = step(n), side = Math.sign(opinion);
  if (s === 0 && Math.abs(opinion) >= DIPLOMACY.leanAt) return TRACK[side + 3];
  if (s !== 0 && opinion * s < DIPLOMACY.backAt) return 'independent';
  if (s !== 0 && opinion * s >= DIPLOMACY.joinAt) {
    const b = s > 0 ? 'axis' : 'allies';
    const leader = g.nations[DIPLOMACY.leaders[b]];
    if (leader?.alive && bloc(leader) === b) return TRACK[3 * s + 3];
  }
  return n.align;
}

/**
 * What the neutral world would make of the player attacking a country that belongs to neither bloc. Nothing is changed.
 * Only the player pays this price: the computer's own invasions are already counted in how opinion drifts with the war.
 * Friends of the victim come first, then anyone whose place on the track would change, then the rest.
 */
export function alarmed(g: Game, by: string, on: string): Alarm[] {
  const A = g.nations[by], B = g.nations[on];
  const b = A ? bloc(A) : null;
  if (g.flags.open !== 1 || by !== g.human || !A || !B || !b || b === 'league' || bloc(B) !== null) return [];
  const friends = new Map(friendsOf(on).map((f) => [f.nat, f.why]));
  const out: Alarm[] = [];
  for (const n of Object.values(g.nations)) {
    if (n.id === by || n.id === on || !courtable(n)) continue;
    const why = friends.get(n.id);
    const w = why !== undefined ? RELATIONS.wary.friend : RELATIONS.wary.all;
    const k = -sideOf(b) * (DIPLOMACY.resist[n.id] ?? 1);
    const opinion = clamp(n.opinion + w.opinion * k, -100, 100);
    const to = wouldStand(g, n, opinion);
    out.push({
      nat: n.id, friend: why !== undefined, why: why ?? '', opinion, home: clamp(n.home + w.home * k, -100, 100), from: n.align, to,
      trade: majorsOf(g).includes(by) && step(n) === sideOf(b) && TRACK.indexOf(to) - 3 !== sideOf(b),
    });
  }
  const rank = (a: Alarm): number => (a.friend ? 0 : a.from !== a.to ? 1 : 2);
  return out.sort((x, y) => rank(x) - rank(y) || g.nations[x.nat].name.localeCompare(g.nations[y.nat].name));
}

/** Carries out what alarmed() foresaw, once the war is declared. */
export function alarm(g: Game, list: Alarm[], on: string): void {
  if (!list.length) return;
  const friends = list.filter((a) => a.friend).map((a) => g.nations[a.nat].name);
  const names = friends.length <= 1 ? friends.join('') : `${friends.slice(0, -1).join(', ')} and ${friends[friends.length - 1]}`;
  headline(g, 'event', friends.length
    ? `The attack on ${g.nations[on].name} alarms the neutral world, and its friends most of all: ${names}.`
    : `The attack on ${g.nations[on].name} alarms the neutral world.`, on);
  for (const a of list) {
    const n = g.nations[a.nat];
    n.opinion = a.opinion;
    n.home = a.home;
    settle(g, n);
  }
}

/** What declaring war would mean, for the prompt that asks the player whether he means it. Nothing is changed. */
export interface WarForecast {
  on: string;
  /** The bloc the country will join once it is attacked, if it joins one. */
  joins: 'allies' | 'axis' | null;
  /** What this month's attacks on its ground are worth if the blow is a surprise: 1 when it is not. */
  sneak: number;
  /**
   * Why the blow is no surprise, or null when it is one: 'war' (the country is already at war with the declarer's
   * side), 'alert' (its codebreakers read the other side's signals: a national ability) or 'watch' (it is at peace,
   * but this game has no rule of surprise: a historical scenario, where every army watches its frontier).
   */
  ready: 'war' | 'alert' | 'watch' | null;
  friends: Friend[];
  /**
   * Who stands by it (ruling 229, part 3): its guarantors, as each stands toward this declarer this month, with the
   * card's line for each. The grand campaign's; none in a historical scenario, which keeps the war it had.
   */
  guarantors: Guarantor[];
  alarm: Alarm[];
  /** True when the country trades with the declarer today: that income ends with the peace. */
  trade: boolean;
  /** The other members of the League, when the country belongs to it: each goes to war with the declarer the same day. */
  league: string[];
  /**
   * The mother country that may stand by it, when it is a dominion of hers (ruling 211(b)), or null. `answers` is
   * the share of the time she goes to war with the declarer the same month as her war in Europe stands today
   * (ruling 222: 0.8, 0.6, 0.4 or 0.2; one roll when the war is declared), and `patronSays` the card's two
   * sentences, the first set bold as the League's is.
   */
  patron: string | null;
  answers: number;
  patronSays: [string, string] | null;
}

const TIMES = ['never', 'once', 'twice', 'three times', 'four times', 'five times', 'six times', 'seven times', 'eight times', 'nine times'];
/**
 * The declare card's two sentences where a mother country may answer for the country declared on (the UI
 * director's words for ruling 222, 17 November 2026): the odds as her war in Europe stands today, in plain words,
 * and the reason in one short clause after them; never a promise.
 */
export function patronSays(g: Game, patron: string, on: string): [string, string] {
  const P = newsName(g.nations[patron]), press = patronPress(g, patron);
  const k = Math.min(9, Math.max(1, Math.round(press.answers * 10)));
  const ally = pressData(patron).beside, F = ally && g.nations[ally] ? newsName(g.nations[ally]) : 'France';
  const why = press.step === 4 ? `${P} must look to its own shores`
    : press.step === 3 ? (press.fallen ? `${F} has fallen, and ${P} fights on in Europe` : ally && atWar(g, ally, patron) ? `${P} is at war in Europe, with ${F} among its enemies` : `${P} is at war in Europe, and ${F} stays out of it`)
    : press.step === 2 ? `${P} is at war in Europe, with ${F} still fighting beside it`
    : `${P} has no war in Europe to hold it back`;
  const times = TIMES[k][0].toUpperCase() + TIMES[k].slice(1);
  return [`${P} may stand by it.`, `${times} in ten ${P} declares war on you the same month, and ${TIMES[10 - k]} in ten it stays out of this war: ${why}.`];
}

export function warForecast(g: Game, by: string, on: string): WarForecast {
  const A = g.nations[by], B = g.nations[on];
  const ba = A ? bloc(A) : null;
  const sneak = wouldSurprise(g, by, on);
  const patron = standsByWhom(g, by, on);
  const fighting = !!B && B.atWar.some((x) => x === by || allied(g, x, by));
  return {
    on,
    ready: sneak ? null : fighting ? 'war' : alertNation(g, on) ? 'alert' : 'watch',
    joins: B && bloc(B) === null && ba && ba !== 'league' && !ownPath(g, on) ? (ba === 'axis' ? 'allies' : 'axis') : null,
    sneak: sneak ? RELATIONS.sneak.attack : 1,
    friends: friendsOf(on).filter((f) => g.nations[f.nat]?.alive),
    guarantors: guarantorsOf(g, by, on),
    alarm: alarmed(g, by, on),
    trade: g.flags.open === 1 && tradePartners(g, by).some((n) => n.id === on),
    league: standsWith(g, by, on),
    patron,
    answers: patron ? patronPress(g, patron).answers : 0,
    patronSays: patron ? patronSays(g, patron, on) : null,
  };
}

function setAlign(g: Game, n: Nation, s: number): void {
  n.align = TRACK[s + 3];
  g.flags[`step:${n.id}`] = g.turn;
  g.supplyDirty = true;
}

/** Moves a neutral along the track when its opinion has crossed a threshold. One step at a time. */
function settle(g: Game, n: Nation): void {
  if (!courtable(n)) return;
  const last = g.flags[`step:${n.id}`];
  if (last !== undefined && g.turn - last < DIPLOMACY.stepEvery) return;
  const s = step(n);
  const side = Math.sign(n.opinion);
  if (s === 0 && Math.abs(n.opinion) >= DIPLOMACY.leanAt) {
    setAlign(g, n, side);
    headline(g, 'event', `${n.name} leans toward ${blocName(side)}: trade and treaties, but not at war.`, n.id);
  } else if (s !== 0 && n.opinion * s < DIPLOMACY.backAt) {
    setAlign(g, n, 0);
    headline(g, 'event', `${n.name} steps back from ${blocName(s)} to strict neutrality.`, n.id);
  } else if (s !== 0 && n.opinion * s >= DIPLOMACY.joinAt) {
    // (A neutral of the Americas joins the Axis only as ruling 223 has it, or as a player's own envoys bring it in.)
    if (s > 0 && americasNeutral(g, n.id) && !(g.human && bloc(g.nations[g.human]) === 'axis' && g.flags[`court:${g.human}:${n.id}`] !== undefined)) return;
    // The history dial: by the book only the countries that really joined a bloc join it of their own accord.
    if (mayJoin(g, n.id, s > 0 ? 'axis' : 'allies')) joinBloc(g, n, s > 0 ? 'axis' : 'allies');
  }
}

/** Full membership needs the bloc's leader to still be standing. */
function joinBloc(g: Game, n: Nation, b: 'allies' | 'axis'): boolean {
  const leader = g.nations[DIPLOMACY.leaders[b]];
  if (!leader?.alive || bloc(leader) !== b || bloc(n) || n.atWar.length) return false;
  setAlign(g, n, 3 * sideOf(b));
  n.opinion = n.home = 100 * sideOf(b);
  headline(g, 'war', `${n.name} joins ${blocName(sideOf(b))}.`, n.id);
  return true;
}

// ---- Events: history as data ------------------------------------------------------------

/**
 * Ruling 233, rider b612: the leavings in play. For every country an event can still take out of the war by terms
 * (a partition) on grounds the event itself names ('anyHeld' or 'allHeld'), those grounds, read from the event. Not an
 * event that cannot fire in this game: one of the other mode, one that has fired or is past its last month, or one
 * that spares the country the player leads (rulings 99 and 103).
 */
export function leavings(g: Game): { nation: string; grounds: string[] }[] {
  const open = g.flags.open === 1;
  const out: { nation: string; grounds: string[] }[] = [];
  for (const e of DIPLOMACY.events) {
    if (!e.partition || (e.mode && (e.mode === 'open') !== open) || g.flags[`event:${e.id}`] !== undefined || g.turn > (e.to ?? 999) || spared(g, e.partition.nation, e.stake)) continue;
    const test = (e.if ?? []).find((c) => c.startsWith('anyHeld:') || c.startsWith('allHeld:'));
    if (test) out.push({ nation: e.partition.nation, grounds: test.slice(test.indexOf(':') + 1).split(',').slice(1) });
  }
  return out;
}

/** Ruling 233, rider b612: true while this country's leaving is in play and an enemy of its holds or stands on a ground its leaving event names. */
export function leavingThreat(g: Game, nation: string): boolean {
  return leavings(g).some((L) => L.nation === nation && L.grounds.some((nm) => {
    const r = g.byName[nm];
    return !!r && (atWar(g, nation, r.ctrl) || r.units.some((u) => !u.dead && u.kind !== 'fort' && atWar(g, nation, u.nat)));
  }));
}

/** Tests one condition of an event, written as "key:arguments". */
export function holds(g: Game, cond: string): boolean {
  const i = cond.indexOf(':');
  const key = i < 0 ? cond : cond.slice(0, i);
  const rest = i < 0 ? '' : cond.slice(i + 1);
  const a = rest.split(',');
  const N = (id: string): Nation | undefined => g.nations[id];
  switch (key) {
    case 'alive': return !!N(a[0])?.alive;
    // 'computer:ITA': nobody leads this country in the grand campaign. What history does to a government is done to
    // the computer's; the player's own stands or falls by the cabinet's rules (ruling 99).
    // 'computer:ITA,47': the same, and in a history campaign the Italy of a player who took his seat in any month
    // but the 47th (July 1943, whose card states Rome's fall as the stake) is spared too (ruling 103).
    case 'computer': return !spared(g, a[0], a.slice(1).map(Number));
    // 'stop:1,2': the campaign is played at one of these stops of the history dial (a history campaign is at stop 3).
    // (What the world was on the day belongs to more stops than the book's calendar does: rider b591 to ruling 234.
    // Siam let Japan pass within hours on 8 December 1941.)
    case 'stop': return a.map(Number).includes(stopOf(g));
    // 'by:21': no later than this month. In the grand campaign an event's calendar otherwise only says when it may
    // first happen; an event that keeps history's calendar at the book's stops says here when its moment has passed.
    case 'by': return g.turn <= Number(a[0]);
    case 'fallen': return !N(a[0])?.alive;
    case 'peace': return !N(a[0])?.atWar.includes(a[1]);
    case 'atPeace': return !N(a[0])?.atWar.length;
    case 'notInBloc': return !!N(a[0]) && bloc(N(a[0])!) === null;
    case 'inBloc': return !!N(a[0]) && bloc(N(a[0])!) === a[1];
    // 'openTo:CUB,allies': in no alliance, or on that side already. (A country in the other camp or in the League is not history's to move.)
    case 'openTo': return !!N(a[0]) && (bloc(N(a[0])!) === null || bloc(N(a[0])!) === a[1]);
    // 'anyPeace:USA,GER,ITA': the first is at peace with at least one of the others still standing, so a war the entry brings has not begun.
    case 'anyPeace': return !!N(a[0]) && a.slice(1).some((id) => !!N(id)?.alive && !N(a[0])!.atWar.includes(id));
    // 'binds:USA': history's entry still binds this country. At the dial's looser stops (neutrals with ambitions) a
    // player who has already taken a side keeps the war he chose: history does not widen it for him.
    case 'binds': return !(a[0] === g.human && !!stopDef(g).ambitions && !!N(a[0]) && bloc(N(a[0])!) !== null);
    case 'step>=': return !!N(a[0]) && step(N(a[0])!) >= Number(a[1]);
    case 'step<=': return !!N(a[0]) && step(N(a[0])!) <= Number(a[1]);
    case 'opinion>=': return !!N(a[0]) && N(a[0])!.opinion >= Number(a[1]);
    case 'opinion<=': return !!N(a[0]) && N(a[0])!.opinion <= Number(a[1]);
    case 'momentum>=': return momentum(g) >= Number(a[0]);
    case 'momentum<=': return momentum(g) <= Number(a[0]);
    // 'holds:ROM,Bessarabia': the place is that country's own and in its hands. (Or in its hands by agreement with
    // the owner: Japan in Saigon under the demand, far.ts `stationed`. Nothing is so held with the far side off.)
    case 'holds': { const r = g.byName[a[1]]; return !!r && r.ctrl === a[0] && (r.owner === a[0] || stationed(g, r)); }
    case 'flag': return !!g.flags[rest];
    // 'book': the dial stands at a stop that keeps history's calendar. 'book:pacific': and the book is still open in that theatre.
    case 'book': return !!stopDef(g).book && (rest === '' || onBook(g, rest));
    // 'since:event:x,2': that flag was raised at least two months ago.
    case 'since': return !!g.flags[a[0]] && g.turn - g.flags[a[0]] >= Number(a[1] ?? 1);
    case 'war': return !!N(a[0]) && !!N(a[1]) && atWar(g, a[0], a[1]);
    // No army of that alliance on the country's ground or on ground bordering it (Turkey, 1945: b528).
    case 'noArmyNear': {
      const near = new Set<number>();
      for (const r of g.regions) if (r.owner === a[1]) { near.add(r.id); for (const j of r.nb) near.add(j); }
      return !g.corps.some((c) => !c.dead && c.kind !== 'fort' && near.has(c.reg) && !!N(c.nat) && bloc(N(c.nat)!) === a[0]);
    }
    // An army of that alliance stands on the country's home ground (b528: the Allies on German soil).
    case 'armyOn': return g.corps.some((c) => !c.dead && c.kind !== 'fort' && g.regions[c.reg].owner === a[1] && !g.regions[c.reg].overseas && !!N(c.nat) && bloc(N(c.nat)!) === a[0]);
    case 'dormant': return !!N(a[0])?.dormant;
    // "anyHeld:allies,Oran,Algiers": some place on the list is in the hands of that bloc.
    case 'allHeld': return a.slice(1).every((nm) => { const r = g.byName[nm]; return !!r && !!N(r.ctrl) && bloc(N(r.ctrl)!) === a[0]; });
    case 'anyHeld': return a.slice(1).some((nm) => { const r = g.byName[nm]; return !!r && !!N(r.ctrl) && bloc(N(r.ctrl)!) === a[0]; });
    // 'taken:allies,Cassino,event:italian-armistice': the place is in that bloc's hands, and since that flag was
    // raised it has stood in other hands at a month's opening. News of a fight is told only where there was one:
    // ground that came over by a treaty's terms and was never fought for has not been taken (ruling 100).
    // 'pact:JAP,USA': a pact binds Germany and Italy to the war between the two, begun once it bound (ruling 197; pacific.ts).
    case 'pact': return pactBinds(g, a[0], a[1]);
    case 'taken': { const r = g.byName[a[1]], at = g.flags[a[2]]; return !!r && !!at && !!N(r.ctrl) && bloc(N(r.ctrl)!) === a[0] && (g.flags[`others:${a[1]}`] ?? 0) >= at; }
    default: return false; // an unknown condition never holds, so a typo cannot fire an event
  }
}

/** Sends a corps standing on ground its country has just given up back to the nearest home region with room. */
export function withdraw(g: Game, nat: string, from: Region): void {
  for (const c of [...from.units]) {
    if (c.nat !== nat) continue;
    let best: Region | null = null, bd = 1e12;
    for (const r of g.regions) {
      if (r.owner !== nat || r.ctrl !== nat || !fits(g, r, nat, [c], { rows: r.units.filter((u) => !roomless(g, u)).length })) continue;
      const d = chartDist2(g, r, from);
      if (d < bd) { bd = d; best = r; }
    }
    if (best) relocate(g, c, best); else removeCorps(g, c);
  }
}

/**
 * Two nations at war make peace. Each goes home: ground of the one that the other still occupies is handed back,
 * and nobody stays on the other's soil. False if they were not at war. It says nothing: the news is the caller's.
 */
export function signPeace(g: Game, a: string, b: string): boolean {
  const A = g.nations[a], B = g.nations[b];
  if (!A?.alive || !B?.alive || !A.atWar.includes(b)) return false;
  A.atWar = A.atWar.filter((x) => x !== b);
  B.atWar = B.atWar.filter((x) => x !== a);
  for (const r of g.regions) {
    if ((r.owner === a && r.ctrl === b) || (r.owner === b && r.ctrl === a)) { r.ctrl = r.owner; r.since = g.turn; }
    if (r.ctrl === a && r.units.some((u) => u.nat === b)) withdraw(g, b, r);
    if (r.ctrl === b && r.units.some((u) => u.nat === a)) withdraw(g, a, r);
  }
  return true;
}

export function transfer(g: Game, r: Region, to: string): void {
  // (Under true reach the holder's kept stores stay his where the port was already his: game.ts keptStoresHolder.)
  keptStoresHolder(g, r, to);
  r.owner = to; r.ctrl = to; r.cap = false; r.since = g.turn;
  g.supplyDirty = true;
}

function fire(g: Game, e0: DiploEvent): boolean {
  // (The words that fit the day, where the event has more than one telling.)
  const words = e0.says?.find((row) => row.if.every((c) => holds(g, c)))?.text;
  const e = words === undefined ? e0 : { ...e0, text: words };
  let did = false, told = false;
  // Ruling 99: no event takes the player's own country out of the war or deals out its ground, at any stop of the
  // dial, at any level and whatever the office setting. An event that carries a partition never fires on it.
  if (e.partition && spared(g, e.partition.nation, e.stake)) return false;
  if (e.annex) {
    for (const id of e.annex.nations) {
      const n = g.nations[id];
      // A country already in the war is no longer Moscow's to take, nor one that stands in an alliance, nor the player's own.
      if (!n?.alive || n.atWar.length || bloc(n) || playersOwn(g, id)) continue;
      for (const c of g.corps.filter((c) => c.nat === id)) removeCorps(g, c);
      endVoyages(g, id);
      recordFall(g, id, e.annex.by, 'annexed');
      for (const r of g.regions) if (r.owner === id) transfer(g, r, e.annex.by);
      n.alive = false;
      n.fellOn = { turn: g.turn, week: g.week, to: e.annex.by, how: 'annexed' };
      did = true;
    }
    if (!did) return false;
  }
  if (e.cede) {
    // No treaty signs away the ground of a country in the player's hands: whoever wants it must come and take it.
    if (playersOwn(g, e.cede.from)) {
      const to = g.nations[e.cede.to], lost = e.cede.regions.filter((nm) => g.byName[nm]?.owner === e.cede!.from);
      if (!to?.alive || !lost.length) return false;
      // (In a war the two are already fighting the demand is peace terms refused: the UI director's words, ruling 228.)
      const asked = lost.length === 1 ? lost[0] : `${lost.slice(0, -1).join(', ')} and ${lost[lost.length - 1]}`, from = g.nations[e.cede.from];
      headline(g, 'event', atWar(g, e.cede.from, e.cede.to)
        ? `${theName(to, true)} offers peace for ${asked}. ${theName(from, true)} will not sign its ground away, and the war goes on.`
        : `${theName(to, true)} demands ${asked}. ${theName(from, true)} gives up nothing without a fight.`, e.cede.from);
      return true;
    }
    for (const name of e.cede.regions) {
      const r = g.byName[name];
      // Ground the taker already holds by force of arms is signed over like the rest.
      if (!r || r.owner !== e.cede.from || (r.ctrl !== e.cede.from && r.ctrl !== e.cede.to)) continue;
      transfer(g, r, e.cede.to);
      withdraw(g, e.cede.from, r);
      did = true;
    }
    if (!did) return false;
  }
  if (e.peace) {
    let made = false;
    for (const [a, b] of e.peace) if (signPeace(g, a, b)) made = true;
    if (!made && !did) return false;
    g.supplyDirty = true;
    did = true;
  }
  if (e.liberate) {
    const L = e.liberate;
    const to = L.to.find((id) => g.nations[id]?.alive && atWar(g, id, L.from));
    for (const name of L.regions) {
      const r = g.byName[name];
      if (!to || !r || r.ctrl !== L.from || r.units.some((u) => !allied(g, u.nat, to))) continue;
      capture(g, r, to);
      did = true;
    }
    if (!did) return false;
  }
  if (e.wake) {
    for (const id of e.wake.nations) {
      const n = g.nations[id];
      if (!n || !n.dormant) continue;
      n.alive = true; n.dormant = false;
      n.align = e.wake.align;
      n.opinion = n.home = homeOpinion(id, n.align);
      did = true;
    }
    if (!did) return false;
    g.supplyDirty = true;
    // The news of who has come in is told before the declarations that follow from it.
    headline(g, 'event', e.text, e.wake.nations[0]);
    told = true;
  }
  // The countries of an entry that are not history's to move: fallen, asleep, a small country in the player's hands,
  // or one that stands in another camp. The war the entry brings is not theirs either.
  const barred = new Set<string>();
  if (e.enter) {
    const side = e.enter.align.startsWith('allies') ? 'allies' : e.enter.align.startsWith('axis') ? 'axis' : null;
    let any = false, there = false;
    for (const id of e.enter.nations) {
      const n = g.nations[id];
      const b = n ? bloc(n) : null;
      if (!n?.alive || n.dormant || ownPath(g, id) || (b !== null && b !== side)) { barred.add(id); continue; }
      // Already a member on that side (an early declaration brought it in): it has nothing to join, but the war
      // the entry brings may still be its to fight. No declaration switches a country off (ruling 58).
      if (n.align === e.enter.align) { there = true; continue; }
      n.align = e.enter.align;
      n.opinion = n.home = homeOpinion(id, n.align);
      g.flags[`step:${id}`] = g.turn;
      any = true;
    }
    if (!any && !(there && e.war)) return false;
    if (any) {
      did = true;
      g.supplyDirty = true;
      // (An event whose words are its war's own news tells them with the declaration, below.)
      if (e.tell !== 'war') { headline(g, 'event', e.text, e.enter.nations[0]); told = true; }
    }
  }
  if (e.war) {
    for (const [a, b] of e.war) {
      const A = g.nations[a], B = g.nations[b];
      if (barred.has(a) || !A?.alive || !B?.alive || A.atWar.includes(b)) continue;
      // In free play no event drags a country at peace into the war: that decision is its own.
      if (g.flags.open === 1 && !B.atWar.length) continue;
      // An entry whose countries were on their side already tells its news with the first war it brings.
      if (e.enter && !told && !allied(g, a, b) && e.tell !== 'war') { headline(g, 'event', e.text, e.enter.nations[0]); told = true; }
      const at = g.headlines.length;
      declareWar(g, a, b);
      did = did || A.atWar.includes(b);
      // The declaration's own line is told in the event's words.
      if (e.tell === 'war' && !told && A.atWar.includes(b)) {
        const plain = theWords(`${theName(A, true)} declares war on ${theName(B)}.`);
        const h = g.headlines.slice(at).find((x) => x.kind === 'war' && x.nat === a && x.text === plain);
        if (h) { h.text = theWords(e.text); told = true; }
      }
    }
    if (!did) return false;
  }
  for (const id of e.stay ?? []) if (!barred.has(id) && g.nations[id]?.alive) g.flags[`stay:${id}`] = g.turn;
  if (e.partition) {
    const X = g.nations[e.partition.nation];
    if (!X?.alive) return false;
    // The grand campaign: terms go to whoever is on the ground. A power with no man in the country and none of its
    // provinces inherits nothing, whatever history's treaty said, and an army standing on a province keeps it.
    const open = g.flags.open === 1;
    // (Its home ground, that is: a colony taken in Africa gives nobody a claim on the mother country.)
    const home = g.regions.filter((r) => r.owner === X.id && (!open || !r.overseas));
    const live = (id: string): boolean => !!g.nations[id]?.alive && g.nations[id].atWar.length > 0;
    const stake = (id: string): number => home.reduce((s, r) => s + (r.ctrl === id ? 1 : 0) + (r.units.some((u) => u.nat === id && u.kind !== 'fort') ? 1 : 0), 0);
    const first = (list: string[]): string | undefined => list.find((id) => live(id) && (!open || !!e.partition!.marchIn || stake(id) > 0));
    const held = new Set(g.regions.filter((r) => r.ctrl === X.id).map((r) => r.id));
    for (const c of g.corps.filter((c) => c.nat === X.id)) removeCorps(g, c);
    endVoyages(g, X.id);
    fleetsOnFall(g, X.id);
    airOnFall(g, X.id);
    // Where another army stands on the country's ground, that ground is its own to hold: the terms divide the rest.
    if (e.partition.occupied || open) for (const r of g.regions) {
      if (r.ctrl !== X.id) continue;
      const guest = r.units.find((u) => u.kind !== 'fort' && g.nations[u.nat]?.alive && g.nations[u.nat].atWar.length > 0);
      if (guest) { r.ctrl = guest.nat; r.cap = false; r.since = g.turn; }
    }
    let heir: string | undefined;
    for (const [takers, names] of e.partition.shares) {
      const to = first(takers);
      if (!to) continue;
      heir ??= to;
      for (const nm of names) {
        const r = g.byName[nm];
        if (r && r.ctrl === X.id) { r.ctrl = to; r.cap = false; r.since = g.turn; }
      }
    }
    // Nobody the treaty names is there: the country gives in to whichever power at war has most of its home ground under its boots.
    if (!heir && open) heir = Object.keys(g.nations).filter((id) => id !== X.id && live(id) && stake(id) > 0).sort((a, b) => stake(b) - stake(a) || (a < b ? -1 : 1))[0];
    if (!heir) return false;
    // Anything the terms forgot goes with the rest.
    for (const r of g.regions) if (r.ctrl === X.id) { r.ctrl = heir; r.cap = false; r.since = g.turn; }
    // (For the news: what passed to somebody other than the heir, by the terms or because his army stood on it.)
    const others = new Map<string, string[]>();
    for (const r of g.regions) if (held.has(r.id) && r.ctrl !== heir) others.set(r.ctrl, [...(others.get(r.ctrl) ?? []), r.name]);
    X.alive = false;
    X.align = 'independent'; // its ground is nobody's ally's now: whoever holds a piece holds it as a conquest
    X.fellOn = { turn: g.turn, week: g.week, to: heir, how: 'terms' };
    // The grand campaign: history's own words are kept only where the treaty ran as history wrote it, to its first
    // heir and with every place it names in the hands it names (or their ally's). Anywhere else the news is told as
    // it happened here and says what truly changed hands (ruling 134(e)): history's words name ground that an army
    // standing on it has kept, or that went to somebody else.
    const asWritten = heir === e.partition.shares[0]?.[0]?.[0] && e.partition.shares.every(([takers, names]) => names.every((nm) => {
      const r = g.byName[nm];
      return !r || r.ctrl === takers[0] || allied(g, r.ctrl, takers[0]);
    }));
    if (open && !asWritten) {
      const H = g.nations[heir];
      const also = [...others.entries()].sort((a, b) => b[1].length - a[1].length || (a[0] < b[0] ? -1 : 1))
        .map(([id, names]) => ` ${theName(g.nations[id], true)} takes ${names.length <= 3 ? listed(names) : `${names.length} of its provinces`}.`).join('');
      headline(g, 'event', `${X.name} has signed an armistice and is out of the war. Its armies lay down their arms, and ${theName(H)} takes over what is left of the country.${also}`, X.id, undefined, { arm: 'land', won: H.id, lost: X.id });
      told = true;
    }
    recordFall(g, X.id, heir, 'terms');
    for (const n of Object.values(g.nations)) n.atWar = n.atWar.filter((x) => x !== X.id);
    X.atWar = [];
    g.supplyDirty = true;
    did = true;
  }
  if (e.reform) {
    let any = false;
    for (const [id, to] of Object.entries(e.reform)) {
      const n = g.nations[id];
      if (!n?.alive) continue;
      Object.assign(n, to);
      any = true;
    }
    if (!any) return false;
  }
  for (const row of e.spawn ?? []) {
    const r = g.byName[row[4]];
    if (!r || r.ctrl !== row[0]) continue;
    // A place packed with troops has no room for an army to form in: it forms at the nearest place behind it that has.
    let at: Region | undefined;
    const seen = new Set([r.id]);
    for (let ring = [r], step = 0; ring.length && !at && step <= 3; step++) {
      at = ring.find((x) => roomForDivision(g, x, row[0], x.units.filter((u) => !roomless(g, u)).length));
      const next: Region[] = [];
      for (const x of ring) for (const j of x.nb) if (!seen.has(j) && g.regions[j].ctrl === row[0]) { seen.add(j); next.push(g.regions[j]); }
      ring = next;
    }
    if (at) addCorps(g, row[0], row[1], row[2] as any, row[3], at.id, { hold: 1 });
  }
  if (!told) {
    headline(g, 'event', e.text, e.band ?? e.annex?.by ?? e.cede?.to ?? e.liberate?.to[0] ?? e.join?.nations[0] ?? e.partition?.nation);
    if (e.band) g.headlines[g.headlines.length - 1].head = e.text.match(/^[^.!?]+/)?.[0];
    if (e.past && g.turn > 1) Object.assign(g.headlines[g.headlines.length - 1], { turn: g.turn - 1, week: 4 });
  }
  for (const [id, d] of Object.entries(e.opinion ?? {})) {
    const n = g.nations[id];
    if (!n || !courtable(n) || ownPath(g, id)) continue;
    n.opinion = clamp(n.opinion + d, -100, 100);
    n.home = clamp(n.home + d, -100, 100);
  }
  if (e.join) {
    const s = sideOf(e.join.bloc);
    for (const id of e.join.nations) {
      const n = g.nations[id];
      if (n && courtable(n) && !ownPath(g, id) && n.opinion * s >= e.join.minOpinion) joinBloc(g, n, e.join.bloc);
    }
  }
  return true;
}

/** Runs at the start of every month: opinion drifts, neutrals move along the track, and history's events are checked. */
/** Carries out one of history's events now, whatever the calendar says: for the tests of what an event does. */
export const fireEvent = (g: Game, id: string): boolean => { const e = DIPLOMACY.events.find((x) => x.id === id); return !!e && fire(g, e); };

export function monthlyDiplomacy(g: Game): void {
  // The last act of the grand campaign: no alliances, no courting, no treaties, and history's calendar is closed.
  if ((g.flags.dom ?? 0) > 0) { liftMonth(g); warWorkMonth(g); footingMonth(g); weatherMonth(g); traitsMonth(g); techMonth(g); return; }
  const m = momentum(g);
  // A gate its holder has shut: the neutrals whose trade it turned back move a point a month away from his side (ruling 177(a)).
  const sore = soreMonth(g);
  for (const n of Object.values(g.nations)) {
    // A small country in the player's hands stays where its player puts it.
    if (!courtable(n) || ownPath(g, n.id)) continue;
    const bw = DIPLOMACY.bandwagon[n.id] ?? DIPLOMACY.bandwagon._default;
    n.opinion = clamp(n.opinion + (n.home - n.opinion) * DIPLOMACY.pull + m * bw + (sore.get(n.id) ?? 0), -100, 100);
    settle(g, n);
  }
  // The Axis courts the Americas, and the United States courts back (ruling 223).
  americasMonth(g);
  // The far side: the Pacific war's ties, read from the state as the month opens (ruling 197; pacific.ts).
  pacificOpening(g);
  // The wider world: the dominions follow their mother country, and a great industry that has gone to war turns over a little more of itself.
  worldMonth(g);
  warWorkMonth(g);
  // The nation's footing: each climb takes its step, staffs decide, all-out war wears the will (ruling 172; switched off, nothing).
  footingMonth(g);
  weatherMonth(g);
  // The grand campaign: now and then a circle of small countries founds the League, or another joins it.
  leagueMonth(g);
  traitsMonth(g);
  techMonth(g);
  // In free play history's calendar only says when an event may first happen: if its conditions come true later
  // than they did, it happens later.
  const open = g.flags.open === 1;
  for (const e of DIPLOMACY.events) {
    if (e.when === 'close') continue;
    if (e.mode && (e.mode === 'open') !== open) continue;
    if (g.flags[`event:${e.id}`] || g.turn < e.from || (g.turn > e.to && !open)) continue;
    if (!(e.if ?? []).every((c) => holds(g, c))) continue;
    if (fire(g, e)) { g.flags[`event:${e.id}`] = g.turn; if (e.then) thenRead(g, e, open); }
  }
  // The far side: who stands where once the month's events are in (Japan beside the Axis, China with the Allies).
  pacificSides(g);
  // (For 'taken': each watched place that stands in other hands, now that its flag is raised, is noted.)
  for (const [side, place, flag] of WATCHED) {
    const r = g.byName[place], h = r ? g.nations[r.ctrl] : undefined;
    if (r && g.flags[flag] && !(h && bloc(h) === side)) g.flags[`others:${place}`] = g.turn;
  }
  romeWavers(g);
  romeWaversOwn(g);
}

/** The events read as a month closes. None while the far side's switch is off. */
const CLOSING: DiploEvent[] = DIPLOMACY.events.filter((e) => e.when === 'close');

/**
 * The month's close, once every country has moved and before the calendar turns: the events marked for it are read
 * as the month's opening reads the others, and are dated the month that ends. Nothing while no event is so marked.
 */
export function closeDiplomacy(g: Game): void {
  if (!CLOSING.length || (g.flags.dom ?? 0) > 0) return;
  const open = g.flags.open === 1;
  for (const e of CLOSING) {
    if (e.mode && (e.mode === 'open') !== open) continue;
    if (g.flags[`event:${e.id}`] || g.turn < e.from || (g.turn > e.to && !open)) continue;
    if (!(e.if ?? []).every((c) => holds(g, c))) continue;
    if (fire(g, e)) { g.flags[`event:${e.id}`] = g.turn; if (e.then) thenRead(g, e, open); }
  }
  pacificSides(g);
}

/** The events read the day a war is declared. None while the far side's switch is off. */
const AT_WAR: DiploEvent[] = DIPLOMACY.events.filter((e) => e.when === 'war');
let readingWar = false;

/**
 * The day a war is declared (game.ts declareWar, once the war and its ties are set): the events marked for it are
 * read as the month's opening reads them. A war such an event brings does not read them again from within.
 */
export function warDiplomacy(g: Game): void {
  if (!AT_WAR.length || readingWar || (g.flags.dom ?? 0) > 0) return;
  const open = g.flags.open === 1;
  readingWar = true;
  try {
    for (const e of AT_WAR) {
      if (e.mode && (e.mode === 'open') !== open) continue;
      if (g.flags[`event:${e.id}`] || g.turn < e.from || (g.turn > e.to && !open)) continue;
      if (!(e.if ?? []).every((c) => holds(g, c))) continue;
      if (fire(g, e)) { g.flags[`event:${e.id}`] = g.turn; if (e.then) thenRead(g, e, open); }
    }
  } finally { readingWar = false; }
}

/** The events an event names under 'then' are read the day it fires: the republics follow America the month it enters, whenever in the month that is. */
function thenRead(g: Game, e: DiploEvent, open: boolean): void {
  for (const id of e.then ?? []) {
    const f = DIPLOMACY.events.find((x) => x.id === id);
    if (!f || (f.mode && (f.mode === 'open') !== open) || g.flags[`event:${f.id}`] || g.turn < f.from || (g.turn > f.to && !open)) continue;
    if ((f.if ?? []).every((c) => holds(g, c)) && fire(g, f)) g.flags[`event:${f.id}`] = g.turn;
  }
}

/** The places some event's 'taken' condition watches: [the bloc, the place, the flag]. */
const WATCHED: string[][] = DIPLOMACY.events.flatMap((e) => (e.if ?? []).filter((c) => c.startsWith('taken:')).map((c) => c.slice('taken:'.length).split(',')));

const listed = (xs: string[], last = 'and'): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} ${last} ${xs[xs.length - 1]}`);

/**
 * Rulings 99(f) and 100(a): any player at war on the side of the computer's Italy is warned what Rome will do. From the month an
 * enemy first holds one of the grounds the fall of Mussolini waits on, a warning names the ground, says when Rome
 * changes its government and when it signs, and what then becomes of Italy's ground. It is told the month a ground
 * is lost, and every month from two months before the date. The grounds and the date are read from the two events
 * themselves. News only: no rule reads it, and nothing is said where nobody plays.
 */
function romeWavers(g: Game): void {
  const me = g.human, I = g.nations.ITA;
  if (g.flags.open !== 1 || !me || me === 'ITA' || !I?.alive || !g.nations[me]?.alive || !g.nations[me].atWar.length || !allied(g, me, 'ITA')) return;
  const falls = DIPLOMACY.events.find((e) => e.id === 'mussolini-falls');
  const test = falls?.if?.find((c) => c.startsWith('anyHeld:'));
  if (!falls || !test || g.flags['event:italian-armistice']) return;
  const [side, ...grounds] = test.slice('anyHeld:'.length).split(',');
  if (bloc(g.nations[me]) === side) return;
  const lost = grounds.filter((nm) => { const h = g.nations[g.byName[nm]?.ctrl ?? '']; return !!h && h.id !== 'ITA' && bloc(h) === side; });
  let now = 0;
  for (const nm of lost) now |= 1 << grounds.indexOf(nm);
  // (A ground won back and lost again is news again.)
  const fresh = now & ~(g.flags['told:rome'] ?? 0);
  if (now) g.flags['told:rome'] = now; else delete g.flags['told:rome'];
  const changed = !!g.flags['event:mussolini-falls'];
  if (!now || !(fresh || changed || g.turn >= falls.from - 2)) return;
  const who = side === 'allies' ? 'the Allies' : 'the Axis';
  const held = lost.length <= 2 ? listed(lost) : `${lost[0]} and ${lost.length - 1} more of its grounds`;
  const six = listed(grounds, 'or');
  // (The art director's words of 2026-11-16, the condition first. "Or later" and "if one is still held" are the
  // rule: in a grand campaign both events stay armed from their month on.)
  const when = g.turn < falls.from ? `in ${turnToDate(falls.from).label}` : 'next month';
  const ground = 'Italy\'s ground then goes to whoever has an army on Italian soil, your own corps included.';
  const text = changed
    ? `Italy signs an armistice next month unless ${listed(lost)} ${lost.length === 1 ? 'is' : 'are'} taken back. Rome has changed its government, and it signs while an enemy holds any of ${six}. ${ground}`
    : `Italy makes peace if ${who} still hold ${held} ${when}. If an enemy holds any of ${six} ${when} or later, Rome changes its government that month and signs an armistice the month after, if one is still held. ${ground}`;
  warning(g, 'Rome wavers', text, me, g.byName[lost[0]]?.id, 'ITA');
}

/** What Rome's fall holds over the player's own Italy, where history can end his campaign with it. */
export interface RomeStake {
  /** The places Rome watches, and those of them an enemy holds now. */
  grounds: string[];
  lost: string[];
  /** 'the Allies'. */
  who: string;
  /** The first month Mussolini can fall, and the month he fell, if he has. */
  falls: number;
  fell?: number;
  /** The month whose opening ends the campaign as things stand: set only where he has fallen and a place is still held. */
  due?: number;
}

/**
 * The stake of the one history campaign an event can end on the player: his own Italy, from the seat of July 1943
 * (rulings 103 and 117). Null everywhere else: in the grand campaign, from any other seat, for any other country,
 * once the armistice is signed, and where nobody plays. The places and the months are read from the two events.
 */
export function romeStake(g: Game, viewer: string | null = g.human): RomeStake | null {
  const I = g.nations.ITA;
  if (viewer !== 'ITA' || g.human !== 'ITA' || g.flags.open === 1 || !I?.alive || g.flags['event:italian-armistice']) return null;
  const falls = DIPLOMACY.events.find((e) => e.id === 'mussolini-falls'), signs = DIPLOMACY.events.find((e) => e.id === 'italian-armistice');
  const test = falls?.if?.find((c) => c.startsWith('anyHeld:'));
  // (Only where the armistice can take the player's Italy out: every other seat is spared, by rulings 99 and 103.)
  if (!falls || !signs || !test || spared(g, 'ITA', signs.stake) || g.turn > Math.max(falls.to, signs.to)) return null;
  const [side, ...grounds] = test.slice('anyHeld:'.length).split(',');
  const lost = grounds.filter((nm) => { const h = g.nations[g.byName[nm]?.ctrl ?? '']; return !!h && h.id !== 'ITA' && bloc(h) === side; });
  const fell: number | undefined = g.flags['event:mussolini-falls'];
  const due = fell !== undefined && lost.length > 0 && g.turn + 1 >= signs.from && g.turn + 1 <= signs.to;
  return { grounds, lost, who: side === 'allies' ? 'the Allies' : 'the Axis', falls: falls.from, ...(fell !== undefined ? { fell } : {}), ...(due ? { due: g.turn + 1 } : {}) };
}

/**
 * The month a history campaign's stake falls due on the player (ruling 118(f)): that month the warning is no band
 * but the lead story of his card, in both builds, and this is all of it. The month named is the rules' own: the one
 * whose opening he will not see as that country's commander if he fails.
 */
export interface StakeDue {
  /** The warning whose place it takes ('Rome wavers'), and the country whose fall ends the campaign. */
  title: string;
  nat: string;
  /** The month it falls due, as a turn and by name ('September'). */
  due: number;
  month: string;
  /** What happened, in the event's own words, and one short line under it. */
  head: string;
  sub: string;
  /** What ends the campaign, when, and what would stop it, naming every place held. */
  sentence: string;
  /** The months left; the places an enemy holds, and of how many. */
  left: number;
  lost: string[];
  of: number;
  /** The two figures as the card prints them: '1' and 'month left'; '1' and 'of the six places lost'. */
  figures: { n: string; label: string }[];
  /** Where SHOW ON MAP goes: the first place held against him. */
  region?: number;
}

/** The title of the warning to the player's own Italy once Mussolini has fallen and every place is held again (ruling 120). */
export const ROME_HOLDS = 'Rome holds, for now';
/** The opening of the sentence in that band's words that says the month of grace is spent; the month card prints the sentence on the band's face. */
export const NO_LAST_MONTH = 'There will be no last month';
const COUNTED = ['no', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten'];

/**
 * The stake that falls due as next month opens, or null: in every other month, and from every seat no event can
 * end. Today one campaign has such a stake (Italy from the seat of July 1943: see romeStake); a second is added here
 * and nowhere else.
 */
export function stakeDue(g: Game, viewer: string | null = g.human): StakeDue | null {
  const st = romeStake(g, viewer);
  if (!st || st.due === undefined) return null;
  const month = turnToDate(st.due).label.split(' ')[0];
  const n = st.lost.length, of = st.grounds.length;
  const num = (k: number): string => COUNTED[k] ?? String(k);
  // (Ruling 118(f): it names every place held, and when more than three are held it counts them and then names them.)
  const unless = n > 3 ? `unless all ${num(n)} places ${st.who} hold are retaken first: ${listed(st.lost)}` : `unless ${listed(st.lost)} ${n === 1 ? 'is' : 'are'} retaken first`;
  return {
    title: 'Rome wavers', nat: 'ITA', due: st.due, month,
    head: 'Mussolini has fallen', sub: 'Rome seeks an armistice',
    sentence: `An armistice ends your campaign as ${month} opens, ${unless}.`,
    left: st.due - g.turn, lost: st.lost, of,
    figures: [{ n: String(st.due - g.turn), label: st.due - g.turn === 1 ? 'month left' : 'months left' }, { n: String(n), label: `of the ${num(of)} places lost` }],
    region: g.byName[st.lost[0]]?.id,
  };
}

/**
 * Ruling 117, point 4: no campaign ends on the player without a red band at least a month before, saying what ends
 * it and what would stop it. The player's own Italy is put out of the war by history only in the campaign whose card
 * states Rome's fall as its stake (the seat of July 1943), and there he is told every month: while no ground is
 * lost, which six places Rome watches; when one is lost, which, and the month Mussolini falls; and from the month
 * he falls, that an armistice ends the campaign next month while an Ally holds one of the six, naming those held.
 * News only: no rule reads it. The grounds and the dates are read from the two events themselves.
 */
function romeWaversOwn(g: Game): void {
  const w = stakeWarning(g);
  if (w) warning(g, w.title, w.text, w.nat, w.region, w.about);
}

/** A stake's standing warning: its title, its words as things stand, the country it is about and a place to show. */
export interface StakeWarning {
  title: string;
  text: string;
  nat: string;
  about: string;
  region?: number;
  /** The next moment the test is made, as the month that will open then ('September'): the stake is tried as each month opens. */
  tested: string;
}

/**
 * The warning of a seat's stake as things stand this moment, or null where no stake is live. The month's news is
 * made from it (romeWaversOwn), and a month card takes its band from it, so that the card which opens a campaign
 * carries the warning before any month's events have run (ruling 123(a)) and every card says the stake as it is
 * when he reads it.
 */
export function stakeWarning(g: Game, viewer: string | null = g.human): StakeWarning | null {
  const st = romeStake(g, viewer);
  if (!st) return null;
  const { grounds, lost, who, falls } = st;
  const six = listed(grounds, 'or');
  const held = listed(lost);
  const are = lost.length === 1 ? 'is' : 'are';
  const changed = st.fell !== undefined;
  const date = turnToDate(falls).label;
  const all = COUNTED[grounds.length] ?? String(grounds.length);
  // (The month a ground is lost and every month after: the band is on every card of this campaign, by the ruling.)
  const text = changed
    ? lost.length
      ? `Italy signs an armistice next month unless ${held} ${are} taken back. Mussolini has fallen, and Rome signs while ${who} hold any of ${six}: that ends your campaign. Take back what they hold this month and Italy fights on.`
      // (Ruling 120: once the month of grace is spent the band must not read like the months before the fall.)
      : `Mussolini has fallen. If ${who} hold any of the ${all} as a month opens, Rome signs at once and your campaign ends. ${NO_LAST_MONTH}: hold all ${all}. They are ${listed(grounds)}.`
    : lost.length
      ? `Mussolini falls ${g.turn < falls ? `in ${date}` : 'next month'} unless ${held} ${are} taken back. The month after, Italy signs an armistice while ${who} hold any of ${six}, and that ends your campaign.`
      : `Mussolini falls if ${who} take one of six places. They are ${listed(grounds)}${g.turn < falls ? `, and they count from ${date}` : ''}. The month after he falls, Italy signs an armistice if one is still held, and that ends your campaign.`;
  return { title: changed && !lost.length ? ROME_HOLDS : 'Rome wavers', text, nat: 'ITA', about: 'ITA', region: g.byName[lost[0] ?? grounds[grounds.length - 1]]?.id, tested: turnToDate(g.turn + 1).label.split(' ')[0] };
}
