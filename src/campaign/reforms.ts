/**
 * Earned reforms, for the sandbox stops (4 and 5) of the history dial in the open grand campaign: the design director's
 * one rule (20 November 2026, 05:33Z), a reform fires only from its own cause in that nation's war. Any nation may take
 * one; none comes by date. The record's four dated reform events are not read at these stops (diplomacy.ts): these
 * stand in their place. Causes, backstops and effects: reforms.json (the historian's table). Stops 1 to 3 never read this.
 */
import data from '../../data/campaign/reforms.json';
import air from '../../data/campaign/air.json';
import { majorsOf } from './diplomacy';
import { atWar, headline } from './game';
import { stopDef } from './history';
import { hasTech } from './tech';
import type { Game, Nation, Region } from './types';

interface Reforms {
  learns: { land: number; trade: number; factories: number; backstop: number; quality: number; maxAttack: number; air: number };
  ofAge: { land: number; backstop: number; quality: number; heart: Record<string, string[]> };
  ofAgeAir: { combat: number; backstop: number; air: number; airMost: number };
  bleeds: { raids: number; tech: string; backstop: number; backstopTech: string; air: number };
  cadre: { share: number; quality: number };
  text: Record<Kind, string>;
}
export type Kind = 'learns' | 'ofAge' | 'ofAgeAir' | 'bleeds' | 'cadre';
export const REFORMS = data as unknown as Reforms;
const PRODUCTION = (air as unknown as { production: Record<string, [number, number, number][]> }).production;

/** True where the earned reforms are kept: the sandbox stops of the open grand campaign. */
export const reformsOn = (g: Game): boolean => g.flags.open === 1 && !!stopDef(g).ambitions;

const pair = (a: string, b: string): string => (a < b ? `${a}:${b}` : `${b}:${a}`);
/** Counts a month once: flag 'NAME:KEY' counts the months, 'NAMEt:KEY' holds the last month counted. */
function bump(g: Game, name: string, key: string): void {
  if (g.flags[`${name}t:${key}`] === g.turn) return;
  g.flags[`${name}t:${key}`] = g.turn;
  g.flags[`${name}:${key}`] = (g.flags[`${name}:${key}`] ?? 0) + 1;
}
const count = (g: Game, name: string, key: string): number => g.flags[`${name}:${key}`] ?? 0;

/** Months with a land battle against a great power (L). */
export const landMonths = (g: Game, nat: string): number => count(g, 'cm', nat);
/** Months two nations fought each other: a land battle, or bombs on either's home ground. */
export const foughtMonths = (g: Game, a: string, b: string): number => count(g, 'cv', pair(a, b));
/** Months of enemy day raids on the nation's home ground by an enemy holding long-range fighters. */
export const escortedRaids = (g: Game, nat: string): number => count(g, 'dr', nat);
/** Months of enemy raids on the nation's home ground, by day or by night, while an enemy holds four-engined bombers. */
export const heavyRaids = (g: Game, nat: string): number => count(g, 'rh', nat);
/** Months the nation bombed an enemy's home ground. */
export const bombingMonths = (g: Game, nat: string): number => count(g, 'bm', nat);
/** Months the nation bombed an enemy's home ground or was bombed on its own. */
export const airWarMonths = (g: Game, nat: string): number => count(g, 'bb', nat);

/**
 * A land battle was fought: a month in combat for each pair that fought, and a month of L for every army in it that
 * faced a great power (lesser wars are already in a nation's starting rating).
 */
export function noteBattle(g: Game, att: string[], def: string[]): void {
  if (!reformsOn(g)) return;
  const great = majorsOf(g), A = new Set(att), D = new Set(def);
  const facesGreat = (foes: Set<string>): boolean => [...foes].some((f) => great.includes(f));
  for (const n of A) if (facesGreat(D)) bump(g, 'cm', n);
  for (const n of D) if (facesGreat(A)) bump(g, 'cm', n);
  for (const a of A) for (const d of D) if (a !== d) bump(g, 'cv', pair(a, d));
}

/** Bombs fell on a province's factories, by day or by night. Only a nation's home ground counts. */
export function noteRaid(g: Game, by: string, r: Region, night: boolean): void {
  if (!reformsOn(g)) return;
  const victim = r.ctrl;
  if ((r.first ?? r.owner) !== victim || by === victim) return;
  bump(g, 'bm', by);
  bump(g, 'bb', by);
  bump(g, 'bb', victim);
  bump(g, 'cv', pair(by, victim));
  const B = REFORMS.bleeds.backstopTech;
  if (Object.values(g.nations).some((e) => e.alive && atWar(g, victim, e.id) && hasTech(g, e.id, B))) bump(g, 'rh', victim);
  if (!night && hasTech(g, by, REFORMS.bleeds.tech)) bump(g, 'dr', victim);
}

/** The share of the factories on its own first-day ground that the nation holds now (1 where it had none). */
export function factoriesHeld(g: Game, nat: string): number {
  let all = 0, held = 0;
  for (const r of g.regions) if ((r.first ?? r.owner) === nat) { all += r.fac; if (r.ctrl === nat) held += r.fac; }
  return all > 0 ? held / all : 1;
}

/** Corps lost and enemy corps destroyed (the memorial wall): over the whole war, and from month 'since' on. */
export function trade(g: Game, nat: string, since: number): { lost: number; destroyed: number; lostSince: number; destroyedSince: number } {
  const t = { lost: 0, destroyed: 0, lostSince: 0, destroyedSince: 0 };
  for (const f of g.stories?.fallen ?? []) {
    if (f.nat === nat) { t.lost++; if (f.turn >= since) t.lostSince++; }
    else if (f.by === nat) { t.destroyed++; if (f.turn >= since) t.destroyedSince++; }
  }
  return t;
}
/** The most field corps the nation has had at once, counted each month ('peak:NAT'). */
export const peakCorps = (g: Game, nat: string): number => g.flags[`peak:${nat}`] ?? 0;

/** The nation's aircraft output this month (air.json production: fighters and bombers), 0 where none is written. */
export function airOutput(g: Game, nat: string): number {
  let out = 0;
  for (const [t, f, b] of PRODUCTION[nat] ?? []) if (t <= g.turn) out = f + b;
  return out;
}

const taken = (g: Game, k: Kind, nat: string): boolean => g.flags[`event:reform:${k}:${nat}`] !== undefined;
const round2 = (x: number): number => Math.round(x * 100) / 100;
/** One nation takes a reform: its ratings move, the world hears of it, and it is never taken twice. */
function take(g: Game, k: Kind, n: Nation, to: Partial<Pick<Nation, 'quality' | 'air' | 'slowCommand' | 'maxAttack'>>): void {
  Object.assign(n, to);
  g.flags[`event:reform:${k}:${n.id}`] = g.turn;
  headline(g, 'event', REFORMS.text[k].replace('{adj}', n.adj), n.id);
}
/** Whether the nation's army has lost heart (the events 'heart' names, fired, that name it). */
const lostHeart = (g: Game, nat: string): boolean =>
  Object.entries(REFORMS.ofAge.heart).some(([e, nats]) => nats.includes(nat) && g.flags[`event:${e}`] !== undefined);
/** Whether this nation is one that learns (slow command, or a top attack below 14; 0 is no limit), or has learned. */
const learner = (g: Game, n: Nation): boolean => taken(g, 'learns', n.id) || !!n.slowCommand || (n.maxAttack > 0 && n.maxAttack < REFORMS.learns.maxAttack);

/**
 * Each month at the sandbox stops: every nation at war is weighed for each reform it has not taken. The air forces
 * that bleed are weighed first, since an enemy's bleeding is what brings another's air force of age.
 */
export function reformsMonth(g: Game): void {
  if (!reformsOn(g)) return;
  const field: Record<string, number> = {};
  for (const c of g.corps) if (!c.dead && c.kind !== 'fort') field[c.nat] = (field[c.nat] ?? 0) + 1;
  for (const [nat, k] of Object.entries(field)) if (k > peakCorps(g, nat)) g.flags[`peak:${nat}`] = k;
  const nats = Object.values(g.nations).filter((n) => n.alive && n.atWar.some((e) => g.nations[e]?.alive));
  const L = REFORMS.learns, A = REFORMS.ofAge, S = REFORMS.ofAgeAir, B = REFORMS.bleeds, C = REFORMS.cadre;
  for (const n of nats) {
    if (taken(g, 'bleeds', n.id)) continue;
    if (escortedRaids(g, n.id) >= B.raids || heavyRaids(g, n.id) >= B.backstop) take(g, 'bleeds', n, { air: Math.round(n.air * B.air) });
  }
  for (const n of nats) {
    const land = landMonths(g, n.id);
    const t = trade(g, n.id, g.turn - L.trade);
    if (!taken(g, 'learns', n.id) && learner(g, n)) {
      if ((land >= L.land && t.destroyedSince > t.lostSince && factoriesHeld(g, n.id) >= L.factories) || land >= L.backstop)
        take(g, 'learns', n, { quality: round2(n.quality + L.quality), slowCommand: false, maxAttack: n.maxAttack > 0 ? Math.max(n.maxAttack, L.maxAttack) : 0, air: Math.round(n.air * L.air) });
    } else if (!learner(g, n) && !taken(g, 'ofAge', n.id) && n.quality < 1 && !lostHeart(g, n.id)
      && ((land >= A.land && t.destroyed >= t.lost) || land >= A.backstop)) {
      // (One reform of the two to a nation: one that learns never comes of age. One that has lost heart does not either.)
      take(g, 'ofAge', n, { quality: round2(n.quality + A.quality) });
    }
    if (!taken(g, 'ofAgeAir', n.id)) {
      const bled = n.atWar.find((e) => g.nations[e]?.alive && taken(g, 'bleeds', e) && foughtMonths(g, n.id, e) >= S.combat);
      if (bled !== undefined || airWarMonths(g, n.id) >= S.backstop) {
        const rivals = Object.values(g.nations).filter((x) => x.alive && (bled !== undefined ? atWar(g, x.id, bled) : x.atWar.length > 0));
        const most = rivals.every((x) => x.id === n.id || airOutput(g, x.id) < airOutput(g, n.id)) && airOutput(g, n.id) > 0;
        take(g, 'ofAgeAir', n, { air: Math.round(n.air * (most ? S.airMost : S.air)) });
      }
    }
    if (!taken(g, 'cadre', n.id) && n.quality >= 1 && peakCorps(g, n.id) > 0 && t.lost >= C.share * peakCorps(g, n.id))
      take(g, 'cadre', n, { quality: round2(n.quality + C.quality) });
  }
}
