// Three supply orders on top of the supply columns (release 2; Brian's yes of 23 November 2026 13:06Z, "Do all 3"):
//
//   Move the lorries ('lorryShift'): a nation's lorries are as many as its industry and fuel give it, and they go first
//     to the front it names; what is left is shared out among the rest.
//   Horse-drawn wagons ('wagons'): what the lorries cannot carry goes by horse, slower, and the ground tells on both:
//     mud on lorries, snow and desert on horses. (Without wagons, what the lorries cannot carry is not carried.)
//   Forward dumps ('dumps'): a province beyond the railhead stocked with supply, from which the corps near it are fed
//     by a shorter road; aircraft over it burn some of it, and losing its ground loses it.
//
// Each is its own switch, set as a grand campaign opens, and each works only where the supply columns run
// (columns.ts). Off, nothing here is read and the columns are the columns without it. Nothing is keyed to a nation or a
// date (the core theme, Brian 23 November 07:13Z): the numbers are in data/campaign/logistics.json.

import data from '../../data/campaign/logistics.json';
import { allied, atWar, groundWeather, headline } from './game';
import { income } from './economy';
import { fuelMet } from './resources';
import { railheads } from './rails';
import { airOver, columnsOn, columnsSettable, type ColumnRun } from './columns';
import { SIGHT, sightOf } from './intel';
import { FIRES, bookFire, dumpFiresOn, fadeFires, setDumpFires } from './dumpfires';
import type { Game } from './types';

export const LOGI = data as unknown as {
  pool: { perIncome: number };
  wagons: { perLink: number; mud: { lorry: number; horse: number }; snow: { lorry: number; horse: number }; desert: { lorry: number; horse: number } };
  dumps: { size: number; pp: number; minLinks: number; most: number; bombed: number; reach: number; aiSaving: number; aiPurse: number };
};

/** A forward dump: whose, where, the division-months of supply in it, and what is still to be brought up to it. */
export interface Dump {
  nat: string;
  region: number;
  stock: number;
  /** Division-months ordered and not yet brought up (the month after the order); 0 once stocked. */
  fill: number;
  /** When it was ordered. */
  turn: number;
}

/** The three switches, each read only where the supply columns run. */
export type LogisticsSwitch = 'lorryShift' | 'wagons' | 'dumps';
export const shiftOn = (g: Game): boolean => columnsOn(g) && g.flags.lorryShift === 1;
export const wagonsOn = (g: Game): boolean => columnsOn(g) && g.flags.wagons === 1;
export const dumpsOn = (g: Game): boolean => columnsOn(g) && g.flags.dumps === 1;
/** Whether a nation's lorries are counted: with either the lorry shift or the wagons on. */
export const poolOn = (g: Game): boolean => shiftOn(g) || wagonsOn(g);
/** Whether a switch is thrown (whether or not the columns run). */
export const logisticsSet = (g: Game, which: LogisticsSwitch): boolean => g.flags[which] === 1;

/** Sets one of the three switches: in the campaign's opening month only, as the columns are. Returns whether it was set. */
export function setLogistics(g: Game, which: LogisticsSwitch, on: boolean): boolean {
  if (!columnsSettable(g)) return false;
  if (on) g.flags[which] = 1;
  else {
    delete g.flags[which];
    // (Release 2, dumpfires.ts) The fires need the dumps: with the dumps off, they are off too.
    if (which === 'dumps') { delete g.dumps; setDumpFires(g, false); }
    if (which === 'lorryShift') delete g.lorryFirst;
  }
  return true;
}

const friendOf = (g: Game, a: string, b: string): boolean => a === b || allied(g, a, b);

// ---- Lorries ---------------------------------------------------------------------------------------------------

/** The lorries a nation can put on its roads this month: from its industry, standing idle where its fuel is short. */
export function lorryPool(g: Game, nat: string): number {
  if (!g.nations[nat]?.alive) return 0;
  return Math.round(LOGI.pool.perIncome * Math.max(0, income(g, nat)) * fuelMet(g, nat));
}

/** The enemies whose ground a province touches, for a nation: the fronts its corps there stand on. */
export function frontsAt(g: Game, nat: string, p: number): string[] {
  const out: string[] = [];
  for (const j of g.regions[p].nb) {
    const who = g.regions[j].ctrl;
    if (who && who !== nat && atWar(g, nat, who) && !out.includes(who)) out.push(who);
  }
  return out;
}

/**
 * The front a nation's lorries go to first: the player's own choice (g.lorryFirst), or for a computer nation the
 * offensive it has under way (fought in the last two months) with the most of its divisions beyond the railhead, the
 * one opened latest where two weigh the same. Null where the lorries are shared out evenly.
 */
export function firstFront(g: Game, nat: string, runs: ColumnRun[] = g.columns?.[nat] ?? []): string | null {
  if (!shiftOn(g)) return null;
  if (nat === g.human) {
    const pick = g.lorryFirst?.[nat];
    return pick && atWar(g, nat, pick) ? pick : null;
  }
  const N = g.nations[nat];
  if (!N) return null;
  const live = N.atWar.filter((foe) => N.lastFight[foe] !== undefined && g.turn - N.lastFight[foe] <= 2);
  if (!live.length) return null;
  const weight = new Map<string, number>();
  for (const r of runs) {
    if (r.fill) continue;
    for (const foe of frontsAt(g, nat, r.path[r.path.length - 1])) if (live.includes(foe)) weight.set(foe, (weight.get(foe) ?? 0) + r.divs);
  }
  let best: string | null = null, most = 0, when = -Infinity;
  for (const foe of live) {
    const w = weight.get(foe) ?? 0, t = N.offStart[foe] ?? -1;
    if (w > most || (w === most && w > 0 && t > when)) { best = foe; most = w; when = t; }
  }
  return best;
}

/** The player names the front his lorries go to first, or none ('' shares them out evenly). */
export function setLorryFirst(g: Game, nat: string, foe: string): boolean {
  if (!shiftOn(g)) return false;
  if (foe && !atWar(g, nat, foe)) return false;
  const book = (g.lorryFirst ??= {});
  if (foe) book[nat] = foe;
  else delete book[nat];
  return true;
}

/**
 * Shares a nation's lorries out among its month's columns (each run's 'lorries' is what it needs on the way in): the
 * runs on the front it names first, the biggest first, each whole while the lorries last; the rest evenly. What the
 * lorries cannot carry goes by wagon where the wagons are on ('horse'), or is not carried ('short'). Runs that stock a
 * dump are paid for apart and take nothing from the pool.
 */
export function shareOutLorries(g: Game, nat: string, runs: ColumnRun[]): void {
  if (!poolOn(g)) return;
  let pool = lorryPool(g, nat);
  const first = firstFront(g, nat, runs);
  const want = runs.filter((r) => !r.fill);
  const onFirst = (r: ColumnRun): boolean => first !== null && frontsAt(g, nat, r.path[r.path.length - 1]).includes(first);
  const motor = new Map<ColumnRun, number>();
  if (first) {
    const lead = want.filter(onFirst).sort((a, b) => b.divs - a.divs || a.path[a.path.length - 1] - b.path[b.path.length - 1]);
    for (const r of lead) { const m = Math.min(r.lorries, pool); motor.set(r, m); pool -= m; }
  }
  const rest = want.filter((r) => !motor.has(r));
  const total = rest.reduce((s, r) => s + r.lorries, 0);
  const k = total > 0 ? Math.min(1, pool / total) : 1;
  for (const r of rest) motor.set(r, r.lorries * k);
  const horses = wagonsOn(g);
  for (const r of want) {
    const need = r.lorries, m = Math.min(need, Math.round(motor.get(r) ?? need));
    r.need = need;
    r.lorries = m;
    if (need > m) { if (horses) r.horse = need - m; else r.short = need - m; }
    if (onFirst(r)) r.first = true;
  }
}

/**
 * What one province of road takes off a month's haul beyond what the enemy does, where the wagons are on: a wagon haul
 * is slow everywhere, the thaw's mud bogs lorries worse than horses, snow is hard on horses, the desert has no water
 * for them. 0 and 0 where the wagons are off.
 */
export function roadWear(g: Game, p: number): { lorry: number; horse: number } {
  if (!wagonsOn(g)) return { lorry: 0, horse: 0 };
  const W = LOGI.wagons, r = g.regions[p];
  let lorry = 0, horse = W.perLink;
  const ground = groundWeather(g, r);
  if (ground === 'mud') { lorry += W.mud.lorry; horse += W.mud.horse; } else if (ground === 'snow') { lorry += W.snow.lorry; horse += W.snow.horse; }
  if (r.terrain === 'desert') { lorry += W.desert.lorry; horse += W.desert.horse; }
  return { lorry, horse };
}

// ---- Forward dumps ---------------------------------------------------------------------------------------------

export const dumpsOf = (g: Game, nat: string): Dump[] => (g.dumps ?? []).filter((d) => d.nat === nat);
export const dumpPrice = (): number => LOGI.dumps.size * LOGI.dumps.pp;

/** Why a nation cannot stock a dump in a province now, or null if it can. */
export function whyNoDump(g: Game, nat: string, region: number): string | null {
  const D = LOGI.dumps, r = g.regions[region], N = g.nations[nat];
  if (!dumpsOn(g)) return 'Forward dumps are off in this campaign.';
  if (!r || !N) return 'There is no such place.';
  if (r.ctrl !== nat) return `${r.name} is not held by your own troops.`;
  if ((g.dumps ?? []).some((d) => d.region === region)) return `There is a dump at ${r.name} already.`;
  if (dumpsOf(g, nat).length >= D.most) return `You keep ${D.most} dumps at most: let one run down first.`;
  const road = railheads(g, nat);
  if (!road || !(road[region] >= D.minLinks)) return `${r.name} is too close to the railhead: a dump pays only ${D.minLinks} provinces of road or more beyond it.`;
  if (N.pp < dumpPrice()) return 'The treasury cannot pay for it now.';
  return null;
}

/** Orders a dump stocked: paid for now, brought up by a column from the railhead next month. */
export function orderDump(g: Game, nat: string, region: number): boolean {
  if (whyNoDump(g, nat, region) !== null) return false;
  g.nations[nat].pp -= dumpPrice();
  (g.dumps ??= []).push({ nat, region, stock: 0, fill: LOGI.dumps.size, turn: g.turn });
  return true;
}

/** Takes back a dump ordered this month and not yet brought up: the money is paid back. */
export function cancelDump(g: Game, nat: string, region: number): boolean {
  const i = (g.dumps ?? []).findIndex((d) => d.nat === nat && d.region === region && d.turn === g.turn && d.stock === 0);
  if (i < 0) return false;
  g.nations[nat].pp += dumpPrice();
  g.dumps!.splice(i, 1);
  if (!g.dumps!.length) delete g.dumps;
  return true;
}

/** The road from a dump over ground its nation and its allies hold, a province at a time (0 at the dump, -1 beyond reach). */
export function dumpRoads(g: Game, nat: string, region: number): Int16Array {
  const d = new Int16Array(g.regions.length).fill(-1);
  d[region] = 0;
  let edge = [region];
  for (let k = 1; k <= LOGI.dumps.reach && edge.length; k++) {
    const next: number[] = [];
    for (const p of edge) for (const j of g.regions[p].nb) {
      if (d[j] >= 0) continue;
      const who = g.regions[j].ctrl;
      if (!who || !friendOf(g, who, nat)) continue;
      d[j] = k;
      next.push(j);
    }
    edge = next;
  }
  return d;
}

/**
 * A nation's dumps at the start of its month: one whose ground is lost is burned; aircraft over one burn part of
 * what is in it. Returns the dumps stocked and ready, each with its roads.
 */
export function tendDumps(g: Game, nat: string): { dump: Dump; road: Int16Array }[] {
  if (!dumpsOn(g) || !g.dumps?.length) return [];
  const me = g.human;
  const out: { dump: Dump; road: Int16Array }[] = [];
  // (Release 2, dumpfires.ts) A nation's dumps set burning before last month are out.
  const fires = dumpFiresOn(g);
  if (fires) fadeFires(g, nat);
  g.dumps = g.dumps.filter((d) => {
    if (d.nat !== nat) return true;
    const r = g.regions[d.region], who = r.ctrl;
    if (!who || !friendOf(g, who, nat)) {
      if (me === nat) headline(g, 'info', `Our forward dump at ${r.name} was burned before the enemy could take it: ${Math.round(d.stock + d.fill)} division-months of supply lost.`, me, d.region, { arm: 'land', lost: me });
      else if (me && who === me) headline(g, 'info', `The ${g.nations[nat].adj} forward dump at ${r.name} went up in flames as our troops came in.`, me, d.region, { arm: 'land' });
      return false;
    }
    if (d.fill <= 0 && d.stock > 0) {
      const over = airOver(g, nat, d.region);
      const burnt = d.stock * Math.min(1, LOGI.dumps.bombed * over.air);
      if (burnt >= 1) {
        d.stock -= burnt;
        // (Release 2, dumpfires.ts) Enough of it went up to set it burning: its side's corps near it are slower this month.
        const fire = fires && burnt >= FIRES.least;
        if (fire) bookFire(g, { nat, region: d.region, turn: g.turn, burnt: Math.round(burnt * 10) / 10, ...(over.mine > 0 ? { mine: 1 as const } : {}) });
        if (me === nat && burnt >= 3) headline(g, 'info', `Enemy aircraft bombed our forward dump at ${r.name}: ${Math.round(burnt)} division-months of supply burned, ${Math.round(d.stock)} left.${fire ? ' It is still burning, and the corps it feeds will move slower this month for want of fuel and shells.' : ''}`, me, d.region, { arm: 'air', lost: me });
        else if (fire && me && over.mine > 0 && atWar(g, me, nat)) headline(g, 'info', `Our aircraft have set the ${g.nations[nat].adj} forward dump at ${r.name} burning: ${Math.round(burnt)} division-months of their supply went up, and their troops near it will be slow this month.`, me, d.region, { arm: 'air' });
      }
      out.push({ dump: d, road: dumpRoads(g, nat, d.region) });
    }
    return true;
  });
  return out;
}

/** Takes away the dumps a nation has used up this month, and tells the player of his. */
export function clearDumps(g: Game, nat: string): void {
  if (!g.dumps?.length) return;
  g.dumps = g.dumps.filter((d) => {
    if (d.nat !== nat || d.fill > 0 || d.stock >= 1) return true;
    if (nat === g.human) headline(g, 'info', `Our forward dump at ${g.regions[d.region].name} is used up.`, nat, d.region, { arm: 'land' });
    return false;
  });
  if (!g.dumps.length) delete g.dumps;
}

/**
 * Where a dump would save a nation most road this month: for every province on its columns' roads far enough beyond
 * the railhead, the division-provinces of road it would cut, best first.
 */
export function dumpPlaces(g: Game, nat: string, runs: ColumnRun[] = g.columns?.[nat] ?? []): { region: number; saving: number }[] {
  const D = LOGI.dumps;
  const road = railheads(g, nat);
  if (!road) return [];
  const saving = new Map<number, number>();
  for (const r of runs) {
    if (r.fill || r.dump !== undefined) continue;
    for (let i = D.minLinks; i < r.path.length; i++) {
      const p = r.path[i];
      if (r.path.length - 1 - i > D.reach) continue;
      if (g.regions[p].ctrl !== nat || !(road[p] >= D.minLinks)) continue;
      saving.set(p, (saving.get(p) ?? 0) + r.divs * i);
    }
  }
  const taken = new Set((g.dumps ?? []).map((d) => d.region));
  return [...saving].filter(([p]) => !taken.has(p)).map(([region, s]) => ({ region, saving: Math.round(s) })).sort((a, b) => b.saving - a.saving || a.region - b.region);
}

/** A computer nation stocks a dump where it saves the most road, if that is worth it and it can well afford it. */
export function aiDumps(g: Game, nat: string, runs: ColumnRun[]): void {
  if (!dumpsOn(g) || nat === g.human) return;
  const D = LOGI.dumps, N = g.nations[nat];
  if (dumpsOf(g, nat).length >= D.most || N.pp < dumpPrice() * D.aiPurse) return;
  const best = dumpPlaces(g, nat, runs)[0];
  if (best && best.saving >= D.aiSaving) orderDump(g, nat, best.region);
}

/** One dump as a map draws it. */
export interface DumpView { nat: string; region: number; x: number; y: number; stock: number; filling: boolean; side: 'ours' | 'friend' | 'theirs' }

/** The dumps a viewer can see: his own and his allies', and an enemy's where its province is in sight. */
export function dumpsView(g: Game, viewer: string | null = g.human): DumpView[] {
  if (!dumpsOn(g) || !g.dumps?.length) return [];
  const sight = viewer ? sightOf(g, viewer) : null;
  return g.dumps.filter((d) => !viewer || friendOf(g, d.nat, viewer) || (sight !== null && sight[d.region] >= SIGHT.glimpse)).map((d) => ({
    nat: d.nat, region: d.region, x: g.regions[d.region].x, y: g.regions[d.region].y, stock: Math.round(d.stock), filling: d.fill > 0,
    side: !viewer ? 'friend' : d.nat === viewer ? 'ours' : friendOf(g, d.nat, viewer) ? 'friend' : 'theirs',
  }));
}
