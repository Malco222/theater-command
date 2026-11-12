// Diplomacy: the alignment track. Every nation stands somewhere between Allied member and
// Axis member. Neutrals have a public opinion that drifts with their own leanings and with
// whoever is winning the war; players spend political capital to court them; and history's
// own moves are events that fire only if their conditions still hold.
// The numbers and the events live in data/campaign/diplomacy.json.

import data from '../../data/campaign/diplomacy.json';
import type { Align, Game, Nation, Region } from './types';
import { STACK } from './types';
import { addCorps, atWar, bloc, declareWar, headline, relocate, removeCorps } from './game';
import { fleetsOnFall } from './navy';
import { airOnFall } from './air';

export interface DiploEvent {
  id: string; from: number; to: number; if?: string[]; text: string;
  /** 'history': a decision history made for a nation, left to that nation in free play. 'open': only in free play. */
  mode?: 'history' | 'open';
  annex?: { by: string; nations: string[] };
  cede?: { from: string; to: string; regions: string[] };
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
  partition?: { nation: string; shares: [string[], string[]][] };
  /** Countries whose armies are remade: from now on these ratings stand in place of the ones they began the war with. */
  reform?: Record<string, Partial<Pick<Nation, 'quality' | 'air' | 'slowCommand' | 'maxAttack'>>>;
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
    if (holder === 'axis') axis += r.vp; else allies += r.vp;
  }
  return clamp((axis - allies) / DIPLOMACY.momentumPer, -1, 1);
}

/** Neutrals that lean toward this nation's bloc and trade with it. Only the major powers of a bloc benefit. */
export function tradePartners(g: Game, nat: string): Nation[] {
  const b = bloc(g.nations[nat]);
  if (!b || !DIPLOMACY.majors.includes(nat)) return [];
  const want = sideOf(b);
  return Object.values(g.nations).filter((n) => n.alive && !n.dormant && step(n) === want);
}

/** A neutral that can still be courted: alive, at peace and not yet a member of either bloc. */
export const courtable = (n: Nation): boolean => n.alive && !n.dormant && !n.atWar.length && Math.abs(step(n)) <= 1;

/** How far one round of courting moves a neutral. Moves that match its public opinion go further. */
export function receptive(target: Nation, b: 'allies' | 'axis'): number {
  return clamp(1 + (target.home * sideOf(b)) / 60, 0.4, 1.6) * (DIPLOMACY.resist[target.id] ?? 1);
}

export function canCourt(g: Game, by: string, target: string): { ok: boolean; why: string } {
  const B = g.nations[by], T = g.nations[target];
  const b = B ? bloc(B) : null;
  if (!B || !T || !B.alive || !b) return { ok: false, why: 'Only a member of a bloc can court neutrals.' };
  if (by === target || !courtable(T)) return { ok: false, why: 'Not open to courting.' };
  if (g.flags[`court:${by}:${target}`] === g.turn) return { ok: false, why: 'Already courted this month.' };
  if (B.pc < DIPLOMACY.court.cost) return { ok: false, why: `Needs ${DIPLOMACY.court.cost} political capital.` };
  return { ok: true, why: '' };
}

/** Spends political capital to pull a neutral toward the courting nation's bloc. */
export function court(g: Game, by: string, target: string): boolean {
  if (!canCourt(g, by, target).ok) return false;
  const B = g.nations[by], T = g.nations[target];
  const b = bloc(B)!;
  const k = receptive(T, b) * sideOf(b);
  B.pc -= DIPLOMACY.court.cost;
  T.opinion = clamp(T.opinion + DIPLOMACY.court.opinion * k, -100, 100);
  T.home = clamp(T.home + DIPLOMACY.court.home * k, -100, 100);
  g.flags[`court:${by}:${target}`] = g.turn;
  settle(g, T);
  return true;
}

const blocName = (s: number): string => (s > 0 ? 'the Axis' : 'the Allies');

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
      if (!n?.alive || n.atWar.length || bloc(n)) continue; // a country already in the war is no longer Moscow's to take
      for (const c of g.corps.filter((c) => c.nat === id)) removeCorps(g, c);
      for (const r of g.regions) if (r.owner === id) transfer(g, r, e.annex.by);
      n.alive = false;
      n.fellOn = { turn: g.turn, week: g.week, to: e.annex.by };
      did = true;
    }
    if (!did) return false;
  }
  if (e.cede) {
    for (const name of e.cede.regions) {
      const r = g.byName[name];
      if (!r || r.owner !== e.cede.from || r.ctrl !== e.cede.from) continue;
      transfer(g, r, e.cede.to);
      withdraw(g, e.cede.from, r);
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
    if (r && r.ctrl === row[0] && r.units.length < STACK) addCorps(g, row[0], row[1], row[2] as any, row[3], r.id, { hold: 1 });
  }
  if (!told) headline(g, 'event', e.text, e.annex?.by ?? e.cede?.to ?? e.join?.nations[0] ?? e.partition?.nation);
  for (const [id, d] of Object.entries(e.opinion ?? {})) {
    const n = g.nations[id];
    if (!n || !courtable(n)) continue;
    n.opinion = clamp(n.opinion + d, -100, 100);
    n.home = clamp(n.home + d, -100, 100);
  }
  if (e.join) {
    const s = sideOf(e.join.bloc);
    for (const id of e.join.nations) {
      const n = g.nations[id];
      if (n && courtable(n) && n.opinion * s >= e.join.minOpinion) joinBloc(g, n, e.join.bloc);
    }
  }
  return true;
}

/** Runs at the start of every month: opinion drifts, neutrals move along the track, and history's events are checked. */
export function monthlyDiplomacy(g: Game): void {
  const m = momentum(g);
  for (const n of Object.values(g.nations)) {
    if (!courtable(n)) continue;
    const bw = DIPLOMACY.bandwagon[n.id] ?? DIPLOMACY.bandwagon._default;
    n.opinion = clamp(n.opinion + (n.home - n.opinion) * DIPLOMACY.pull + m * bw, -100, 100);
    settle(g, n);
  }
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
