// Diplomacy: the alignment track. Every nation stands somewhere between Allied member and
// Axis member. Neutrals have a public opinion that drifts with their own leanings and with
// whoever is winning the war; players spend political capital to court them; and history's
// own moves are events that fire only if their conditions still hold.
// The numbers and the events live in data/campaign/diplomacy.json.

import data from '../../data/campaign/diplomacy.json';
import relationsData from '../../data/campaign/relations.json';
import type { Align, Game, Nation, Region } from './types';
import { STACK } from './types';
import { addCorps, allied, atWar, bloc, capture, declareWar, headline, relocate, removeCorps } from './game';
import { leagueMonth, ownPath, standsWith } from './league';
import { fleetsOnFall } from './navy';
import { airOnFall } from './air';

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
  /** Countries that have stood aside until now and enter the story here, at this place on the alignment track. */
  wake?: { nations: string[]; align: Align };
  /**
   * A country that ends, its ground shared out. Each share is [who takes it, the regions]: the first taker on the
   * list still in the war gets every named region the country still holds. Ground someone else already holds stays theirs.
   */
  partition?: { nation: string; shares: [string[], string[]][]; occupied?: boolean };
  /** Pairs of nations that make peace: each hands back the other's ground and takes its troops home. */
  peace?: [string, string][];
  /** Countries whose armies are remade: from now on these ratings stand in place of the ones they began the war with. */
  reform?: Record<string, Partial<Pick<Nation, 'quality' | 'air' | 'slowCommand' | 'maxAttack'>>>;
  /** News of something won in the month just ended: the dispatch is dated to that month's last week, not to the new one. */
  past?: boolean;
}

export const DIPLOMACY = data as unknown as {
  leanAt: number; backAt: number; joinAt: number; pull: number; momentumPer: number; stepEvery: number;
  leaders: Record<'allies' | 'axis', string>; majors: string[];
  pcIncome: Record<string, number>; pcCap: number;
  court: { cost: number; opinion: number; home: number };
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

export const pcIncome = (nat: string): number => (DIPLOMACY.majors.includes(nat) ? DIPLOMACY.pcIncome._major : DIPLOMACY.pcIncome._minor);

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
  if (!b || b === 'league' || !DIPLOMACY.majors.includes(nat)) return [];
  const want = sideOf(b);
  return Object.values(g.nations).filter((n) => n.alive && !n.dormant && step(n) === want);
}

/** A neutral that can still be courted: alive, at peace and not yet a member of either bloc. */
export const courtable = (n: Nation): boolean => n.alive && !n.dormant && !n.league && !n.atWar.length && Math.abs(step(n)) <= 1;

/** How far one round of courting moves a neutral. Moves that match its public opinion go further. */
export function receptive(target: Nation, b: 'allies' | 'axis'): number {
  return clamp(1 + (target.home * sideOf(b)) / 60, 0.4, 1.6) * (DIPLOMACY.resist[target.id] ?? 1);
}

export function canCourt(g: Game, by: string, target: string): { ok: boolean; why: string } {
  const B = g.nations[by], T = g.nations[target];
  const b = B ? bloc(B) : null;
  if (!B || !T || !B.alive || !b || b === 'league') return { ok: false, why: 'Only a member of the Axis or the Allies can court neutrals.' };
  // A small country in the player's hands chooses its own path: nobody talks it round.
  if (by === target || !courtable(T) || ownPath(g, target)) return { ok: false, why: 'Not open to courting.' };
  if (g.flags[`court:${by}:${target}`] === g.turn) return { ok: false, why: 'Already courted this month.' };
  if (B.pc < DIPLOMACY.court.cost) return { ok: false, why: `Needs ${DIPLOMACY.court.cost} political capital.` };
  return { ok: true, why: '' };
}

/** Spends political capital to pull a neutral toward the courting nation's bloc. */
export function court(g: Game, by: string, target: string): boolean {
  if (!canCourt(g, by, target).ok) return false;
  const B = g.nations[by], T = g.nations[target];
  const b = bloc(B) as 'allies' | 'axis';
  const k = receptive(T, b) * sideOf(b);
  B.pc -= DIPLOMACY.court.cost;
  T.opinion = clamp(T.opinion + DIPLOMACY.court.opinion * k, -100, 100);
  T.home = clamp(T.home + DIPLOMACY.court.home * k, -100, 100);
  g.flags[`court:${by}:${target}`] = g.turn;
  settle(g, T);
  return true;
}

const blocName = (s: number): string => (s > 0 ? 'the Axis' : 'the Allies');

// ---- Friendships, surprise and wary neutrals (the grand campaign) -------------------------------------------

export const RELATIONS = relationsData as unknown as {
  sneak: { attack: number };
  wary: { all: { opinion: number; home: number }; friend: { opinion: number; home: number } };
  groups: { id: string; name: string; why: string; nations: string[] }[];
  pairs: [string, string, string][];
};

export interface Friend { nat: string; why: string }

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
  return g.flags.open === 1 && !!B && !B.atWar.some((x) => x === by || allied(g, x, by));
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
      trade: DIPLOMACY.majors.includes(by) && step(n) === sideOf(b) && TRACK.indexOf(to) - 3 !== sideOf(b),
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
  friends: Friend[];
  alarm: Alarm[];
  /** True when the country trades with the declarer today: that income ends with the peace. */
  trade: boolean;
  /** The other members of the League, when the country belongs to it: each goes to war with the declarer the same day. */
  league: string[];
}

export function warForecast(g: Game, by: string, on: string): WarForecast {
  const A = g.nations[by], B = g.nations[on];
  const ba = A ? bloc(A) : null;
  return {
    on,
    joins: B && bloc(B) === null && ba && ba !== 'league' && !ownPath(g, on) ? (ba === 'axis' ? 'allies' : 'axis') : null,
    sneak: wouldSurprise(g, by, on) ? RELATIONS.sneak.attack : 1,
    friends: friendsOf(on).filter((f) => g.nations[f.nat]?.alive),
    alarm: alarmed(g, by, on),
    trade: g.flags.open === 1 && tradePartners(g, by).some((n) => n.id === on),
    league: standsWith(g, by, on),
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
    joinBloc(g, n, s > 0 ? 'axis' : 'allies');
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

/** Tests one condition of an event, written as "key:arguments". */
export function holds(g: Game, cond: string): boolean {
  const i = cond.indexOf(':');
  const key = i < 0 ? cond : cond.slice(0, i);
  const rest = i < 0 ? '' : cond.slice(i + 1);
  const a = rest.split(',');
  const N = (id: string): Nation | undefined => g.nations[id];
  switch (key) {
    case 'alive': return !!N(a[0])?.alive;
    case 'fallen': return !N(a[0])?.alive;
    case 'peace': return !N(a[0])?.atWar.includes(a[1]);
    case 'atPeace': return !N(a[0])?.atWar.length;
    case 'notInBloc': return !!N(a[0]) && bloc(N(a[0])!) === null;
    case 'inBloc': return !!N(a[0]) && bloc(N(a[0])!) === a[1];
    case 'step>=': return !!N(a[0]) && step(N(a[0])!) >= Number(a[1]);
    case 'step<=': return !!N(a[0]) && step(N(a[0])!) <= Number(a[1]);
    case 'opinion>=': return !!N(a[0]) && N(a[0])!.opinion >= Number(a[1]);
    case 'opinion<=': return !!N(a[0]) && N(a[0])!.opinion <= Number(a[1]);
    case 'momentum>=': return momentum(g) >= Number(a[0]);
    case 'momentum<=': return momentum(g) <= Number(a[0]);
    case 'holds': return g.byName[a[1]]?.ctrl === a[0] && g.byName[a[1]]?.owner === a[0];
    case 'flag': return !!g.flags[rest];
    // 'since:event:x,2': that flag was raised at least two months ago.
    case 'since': return !!g.flags[a[0]] && g.turn - g.flags[a[0]] >= Number(a[1] ?? 1);
    case 'war': return !!N(a[0]) && !!N(a[1]) && atWar(g, a[0], a[1]);
    case 'dormant': return !!N(a[0])?.dormant;
    // "anyHeld:allies,Oran,Algiers": some place on the list is in the hands of that bloc.
    case 'allHeld': return a.slice(1).every((nm) => { const r = g.byName[nm]; return !!r && !!N(r.ctrl) && bloc(N(r.ctrl)!) === a[0]; });
    case 'anyHeld': return a.slice(1).some((nm) => { const r = g.byName[nm]; return !!r && !!N(r.ctrl) && bloc(N(r.ctrl)!) === a[0]; });
    default: return false; // an unknown condition never holds, so a typo cannot fire an event
  }
}

/** Sends a corps standing on ground its country has just given up back to the nearest home region with room. */
function withdraw(g: Game, nat: string, from: Region): void {
  for (const c of [...from.units]) {
    if (c.nat !== nat) continue;
    let best: Region | null = null, bd = 1e12;
    for (const r of g.regions) {
      if (r.owner !== nat || r.ctrl !== nat || r.units.length >= STACK) continue;
      const d = (r.x - from.x) ** 2 + (r.y - from.y) ** 2;
      if (d < bd) { bd = d; best = r; }
    }
    if (best) relocate(g, c, best); else removeCorps(g, c);
  }
}

function transfer(g: Game, r: Region, to: string): void {
  r.owner = to; r.ctrl = to; r.cap = false; r.since = g.turn;
  g.supplyDirty = true;
}

function fire(g: Game, e: DiploEvent): boolean {
  let did = false, told = false;
  if (e.annex) {
    for (const id of e.annex.nations) {
      const n = g.nations[id];
      // A country already in the war is no longer Moscow's to take, nor one that stands in an alliance, nor the player's own.
      if (!n?.alive || n.atWar.length || bloc(n) || ownPath(g, id)) continue;
      for (const c of g.corps.filter((c) => c.nat === id)) removeCorps(g, c);
      for (const r of g.regions) if (r.owner === id) transfer(g, r, e.annex.by);
      n.alive = false;
      n.fellOn = { turn: g.turn, week: g.week, to: e.annex.by };
      did = true;
    }
    if (!did) return false;
  }
  if (e.cede) {
    // No treaty signs away the ground of a small country in the player's hands: whoever wants it must come and take it.
    if (ownPath(g, e.cede.from)) {
      const to = g.nations[e.cede.to], lost = e.cede.regions.filter((nm) => g.byName[nm]?.owner === e.cede!.from);
      if (!to?.alive || !lost.length) return false;
      headline(g, 'event', `${to.name} demands ${lost.length === 1 ? lost[0] : `${lost.slice(0, -1).join(', ')} and ${lost[lost.length - 1]}`}. ${g.nations[e.cede.from].name} gives up nothing without a fight.`, e.cede.from);
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
    for (const [a, b] of e.peace) {
      const A = g.nations[a], B = g.nations[b];
      if (!A?.alive || !B?.alive || !A.atWar.includes(b)) continue;
      A.atWar = A.atWar.filter((x) => x !== b);
      B.atWar = B.atWar.filter((x) => x !== a);
      // Each goes home: ground of the one that the other still occupies is handed back, and nobody stays on the other's soil.
      for (const r of g.regions) {
        if ((r.owner === a && r.ctrl === b) || (r.owner === b && r.ctrl === a)) { r.ctrl = r.owner; r.since = g.turn; }
        if (r.ctrl === a && r.units.some((u) => u.nat === b)) withdraw(g, b, r);
        if (r.ctrl === b && r.units.some((u) => u.nat === a)) withdraw(g, a, r);
      }
      made = true;
    }
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
  if (e.war) {
    for (const [a, b] of e.war) {
      const A = g.nations[a], B = g.nations[b];
      if (!A?.alive || !B?.alive || A.atWar.includes(b)) continue;
      // In free play no event drags a country at peace into the war: that decision is its own.
      if (g.flags.open === 1 && !B.atWar.length) continue;
      declareWar(g, a, b);
      did = did || A.atWar.includes(b);
    }
    if (!did) return false;
  }
  if (e.partition) {
    const X = g.nations[e.partition.nation];
    if (!X?.alive) return false;
    const first = (list: string[]): string | undefined => list.find((id) => g.nations[id]?.alive && g.nations[id].atWar.length > 0);
    for (const c of g.corps.filter((c) => c.nat === X.id)) removeCorps(g, c);
    fleetsOnFall(g, X.id);
    airOnFall(g, X.id);
    // Where another army stands on the country's ground, that ground is its own to hold: the terms divide the rest.
    if (e.partition.occupied) for (const r of g.regions) {
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
    if (!heir) return false;
    // Anything the terms forgot goes with the rest.
    for (const r of g.regions) if (r.ctrl === X.id) { r.ctrl = heir; r.cap = false; r.since = g.turn; }
    X.alive = false;
    X.align = 'independent'; // its ground is nobody's ally's now: whoever holds a piece holds it as a conquest
    X.fellOn = { turn: g.turn, week: g.week, to: heir };
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
      at = ring.find((x) => x.units.length < STACK);
      const next: Region[] = [];
      for (const x of ring) for (const j of x.nb) if (!seen.has(j) && g.regions[j].ctrl === row[0]) { seen.add(j); next.push(g.regions[j]); }
      ring = next;
    }
    if (at) addCorps(g, row[0], row[1], row[2] as any, row[3], at.id, { hold: 1 });
  }
  if (!told) {
    headline(g, 'event', e.text, e.annex?.by ?? e.cede?.to ?? e.liberate?.to[0] ?? e.join?.nations[0] ?? e.partition?.nation);
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
export function monthlyDiplomacy(g: Game): void {
  const m = momentum(g);
  for (const n of Object.values(g.nations)) {
    // A small country in the player's hands stays where its player puts it.
    if (!courtable(n) || ownPath(g, n.id)) continue;
    const bw = DIPLOMACY.bandwagon[n.id] ?? DIPLOMACY.bandwagon._default;
    n.opinion = clamp(n.opinion + (n.home - n.opinion) * DIPLOMACY.pull + m * bw, -100, 100);
    settle(g, n);
  }
  // The grand campaign: now and then a circle of small countries founds the League, or another joins it.
  leagueMonth(g);
  // In free play history's calendar only says when an event may first happen: if its conditions come true later
  // than they did, it happens later.
  const open = g.flags.open === 1;
  for (const e of DIPLOMACY.events) {
    if (e.mode && (e.mode === 'open') !== open) continue;
    if (g.flags[`event:${e.id}`] || g.turn < e.from || (g.turn > e.to && !open)) continue;
    if (!(e.if ?? []).every((c) => holds(g, c))) continue;
    if (fire(g, e)) g.flags[`event:${e.id}`] = g.turn;
  }
}
