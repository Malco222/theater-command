/**
 * Joint armies (the grand campaign).
 *
 * Allies already share a province and defend it together. This lets them attack together too, as one army under one
 * hand. The player may ask an ally's field corps that stand in a province with his own to march with his army. Such
 * a corps keeps its own flag, its own pay, drafts, supply line and general, but it takes the player's orders in the
 * player's month: it moves with his corps, attacks beside them, and the ground they take together is his. Its own
 * staff leaves it alone while it is away.
 *
 * An ally lends only what it can spare: a share of its field corps, and nothing while it is at war with none of the
 * player's enemies. A corps goes back to its own command when the player sends it
 * back, when it is left for a month with none of his troops beside it, or when the two countries are allies no more.
 *
 * Nothing here is used by a computer nation or in a historical scenario: no corps joins anybody there, so those wars
 * are fought exactly as before. The numbers are in data/campaign/joint.json.
 */
import data from '../../data/campaign/joint.json';
import type { Corps, Game } from './types';
import { allied, atWar, headline } from './game';

export const JOINT = data as unknown as { share: number; least: number; most: number; capital: number };

/** Joint armies are formed only in the grand campaign. */
export const jointRules = (g: Game): boolean => g.flags.open === 1;

/** The nation whose army a corps marches in: its own, or the ally's it has joined. */
export const armyOf = (c: Corps): string => c.joint ?? c.nat;

const field = (c: Corps): boolean => !c.dead && c.kind !== 'fort';
const nameOf = (g: Game, nat: string): string => g.nations[nat].name;

/** A nation's field corps that march with an ally's army. */
export const lentBy = (g: Game, nat: string): Corps[] => g.corps.filter((c) => c.nat === nat && !c.dead && c.joint !== undefined);

/** Every corps of another nation that marches with this nation's armies. */
export const lentTo = (g: Game, nat: string): Corps[] => g.corps.filter((c) => !c.dead && c.joint === nat);

/** The most field corps a nation will lend at one time: a share of what it has. */
export function lendLimit(g: Game, nat: string): number {
  const n = g.corps.reduce((s, c) => s + (c.nat === nat && field(c) ? 1 : 0), 0);
  return Math.max(0, Math.min(n, JOINT.most, Math.max(JOINT.least, Math.floor(n * JOINT.share))));
}

/** Why a corps cannot join this nation's army, or null if it can. */
export function whyNotJoin(g: Game, c: Corps, to: string | null = g.human): string | null {
  if (!jointRules(g) || to === null || to !== g.human || !g.nations[to]?.alive) return 'Joint armies are formed only by the player, in the grand campaign.';
  if (c.dead) return 'That corps is no more.';
  if (c.nat === to) return 'It is your own.';
  if (!allied(g, c.nat, to)) return `${nameOf(g, c.nat)} is not your ally.`;
  if (c.joint === to) return 'It already marches with your army.';
  if (c.kind === 'fort') return 'Fortress troops do not leave their works.';
  const here = g.regions[c.reg];
  if (!here.units.some((u) => u.nat === to && field(u))) return 'An ally\'s corps joins an army of yours that stands in the same province.';
  const foes = g.nations[to].atWar;
  if (foes.length && !foes.some((e) => atWar(g, c.nat, e))) return `${nameOf(g, c.nat)} is at war with none of your enemies: its corps stay under its own command.`;
  if (JOINT.capital > 0 && g.nations[c.nat].capital === c.reg && here.units.filter((u) => u !== c && u.nat === c.nat && field(u) && u.joint === undefined).length < JOINT.capital) {
    return `${nameOf(g, c.nat)} keeps a corps of its own in its capital.`;
  }
  const limit = lendLimit(g, c.nat);
  if (lentBy(g, c.nat).length >= limit) return limit ? `${nameOf(g, c.nat)} has lent all the corps it can spare (${limit}).` : `${nameOf(g, c.nat)} has no corps to spare.`;
  return null;
}

/** The ally's corps in a province that could join the nation's army there now. */
export function couldJoin(g: Game, region: number, to: string | null = g.human): Corps[] {
  const r = g.regions[region];
  return r ? r.units.filter((c) => whyNotJoin(g, c, to) === null) : [];
}

/** Puts an ally's corps into the nation's army. It keeps what marching it has left this month. */
export function joinArmy(g: Game, c: Corps, to: string | null = g.human): boolean {
  if (whyNotJoin(g, c, to) !== null) return false;
  c.joint = to as string;
  // Its own staff's standing orders do not follow it.
  delete c.yields; delete c.digs;
  return true;
}

/** Sends a corps back to its own command. It does not march again for its own staff this month. */
export function leaveArmy(g: Game, c: Corps): boolean {
  if (c.joint === undefined) return false;
  delete c.joint;
  delete c.yields; delete c.digs;
  if (!c.dead) g.flags[`back:${c.id}`] = g.turn;
  return true;
}

/**
 * What a corps has to march with as its own nation's month opens: nothing if it is with an ally's army (it marches
 * in that army's month), and nothing if it came back from one this month.
 */
export function jointHold(g: Game, c: Corps): boolean {
  if (c.joint !== undefined) {
    if (c.joint === g.human && g.nations[c.joint].alive && allied(g, c.nat, c.joint)) return true;
    // The army it marched with is no more, or is its ally no longer: it is its own country's again, from this month.
    delete c.joint;
    return false;
  }
  const key = `back:${c.id}`;
  if (g.flags[key] === undefined) return false;
  const now = g.flags[key] === g.turn;
  delete g.flags[key];
  return now;
}

/**
 * The month opens for a nation with allied corps in its armies: those left with none of its troops beside them, and
 * those of a country that is its ally no more, go back to their own command; the rest are ready to march.
 */
export function jointMonth(g: Game, nat: string): void {
  if (!jointRules(g)) return;
  for (const c of g.corps) {
    if (c.dead || c.joint === undefined) continue;
    // A commander who has left the war, or the table, commands nobody.
    if (c.joint !== g.human || !g.nations[c.joint].alive || !allied(g, c.nat, c.joint)) { leaveArmy(g, c); continue; }
    if (c.joint !== nat) continue;
    const r = g.regions[c.reg];
    if (!r.units.some((u) => u.nat === nat && field(u))) {
      leaveArmy(g, c);
      delete g.flags[`back:${c.id}`];
      headline(g, 'info', `Left with none of your troops beside it, ${c.name} at ${r.name} has gone back to ${g.nations[c.nat].adj} command.`, nat, r.id);
      continue;
    }
    c.ap = c.apMax;
  }
}
