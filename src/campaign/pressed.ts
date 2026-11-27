// Captured tanks pressed into service (release 2).
//
// Brian, 27 November 2026 05:23Z, yes to the playtester's idea: "after a win, repairable enemy tanks join your army
// wearing your marks". Until now a heavy win only lent the winner the LOOK of captured tanks (game.ts capturedKit, a
// fixed throw of the battle's numbers, six months, nothing in any fight read it). Under this switch the tanks are real:
//
// 1. Only what the battle left on the field. The beaten side's armour knocked out in the battle is estimated from the
//    losses the battle booked, exactly as the old wrecks are (game.ts wreckBump, AFV_PER_DIV): an armoured division
//    fields about 150 armoured vehicles and a motorised one about 40, and a corps loses them in the share of its men it
//    lost. The beaten side leaves all of its on the field; the winner holds the field.
// 2. Of those, 'repair' (a quarter) can be got running again by the winner's workshops. No die is thrown.
// 3. Only the winner's own armoured and motorised corps that fought can take them in: they have the tank crews, the
//    fitters and the recovery vehicles. Each takes them in up to the strength it is short of, shared by that gap. A
//    pressed tank is worth 'worth' (half) of one of the corps' own: no spare parts, and a crew new to it.
// 4. They wear out. Each month 'wear' (a tenth) of those still running break down for want of parts, and twice that
//    in a corps out of supply; the corps' strength falls by what they were worth, and the month's drafts make it good
//    as they would any loss. So a big win gives a quick refit that fades over a year or so, as the record has it.
// 5. Every nation, the player's and the computers', by the same rule: it is about what is on the field (the sandbox
//    rule: outcomes from forces, never the calendar or the history).
// 6. The look follows the tanks: in a battle, a side whose corps carry pressed tanks draws a few of its tank blocks as
//    the captured make in its own paint and marks (battle-bridge.ts reads kitLook; live.ts draws blocks[].captured), in
//    place of the old six months' look by nation.
//
// With the switch off nothing here is read and nothing is written: the war is the one it always was, to the byte.

import data from '../../data/campaign/pressed.json';
import { AFV_PER_DIV } from '../battle/land/wrecks';
import type { BattleReport, Corps, Game, Kind } from './types';

export const PRESSED = data as { repair: number; worth: number; wear: number; cut: number; least: number; blocks: number };

const perOf = (kind: Kind): number => AFV_PER_DIV[kind] ?? 0;

/** The switch: a grand campaign's own, off unless the release 2 ship list or the player turns it on. */
export const pressedOn = (g: Game): boolean => g.flags.pressedtanks === 1;

/** Turns the switch on or off at any time. Off takes every pressed tank off the books (their strength stays). */
export function setPressed(g: Game, on: boolean): boolean {
  if (on) { g.flags.pressedtanks = 1; return true; }
  delete g.flags.pressedtanks;
  for (const c of g.corps) delete c.kit;
  for (const v of g.atSea ?? []) delete v.corps.kit;
  return true;
}

/** What one battle pressed into service: whose tanks, how many in all, and which corps took how many. */
export interface Pressed { from: string; n: number; took: { id: number; n: number }[] }

/**
 * After a land battle: the beaten side's repairable tanks join the winner's armoured and motorised corps that fought.
 * Called from resolveBattle once the losses are booked (beside the old wrecks). Null where nothing is pressed.
 */
export function pressTanks(g: Game, rep: BattleReport, attackers: Corps[], defenders: Corps[]): Pressed | null {
  if (!pressedOn(g) || rep.yielded) return null;
  const attWon = rep.level.endsWith('victory');
  const left: Record<string, number> = {};
  for (const u of attWon ? rep.defenders : rep.attackers) {
    const per = perOf(u.kind);
    if (!per || u.guard || u.divs <= 0 || u.men <= 0 || u.lost <= 0) continue;
    left[u.nat] = (left[u.nat] ?? 0) + per * u.divs * Math.min(1, u.lost / u.men);
  }
  const fixable = Object.values(left).reduce((s, n) => s + n, 0) * PRESSED.repair;
  if (fixable < PRESSED.least) return null;
  const from = Object.entries(left).reduce((a, b) => (b[1] > a[1] ? b : a))[0];
  const takers = (attWon ? attackers : defenders).filter((c) => !c.dead && c.divs > 0 && perOf(c.kind) > 0 && c.nat !== from);
  // Each taker's gap, in its own vehicles: what it is short of its fullest.
  const gaps = takers.map((c) => Math.max(0, (c.full ?? 1) - c.str) * c.divs * perOf(c.kind));
  const room = gaps.reduce((s, n) => s + n, 0);
  if (room <= 0) return null;
  const n = Math.min(fixable, room / PRESSED.worth);
  if (n < PRESSED.least) return null;
  const took: Pressed['took'] = [];
  takers.forEach((c, i) => {
    const k = (n * gaps[i]) / room;
    if (k <= 0) return;
    c.str = Math.min(c.full ?? 1, c.str + (k * PRESSED.worth) / (perOf(c.kind) * c.divs));
    // A corps keeps one maker's tanks on its books: the most of them, where it already ran another's.
    const had = c.kit;
    c.kit = { from: had && had.from !== from && had.n > k ? had.from : from, n: (had?.n ?? 0) + k, turn: g.turn };
    took.push({ id: c.id, n: k });
  });
  return { from, n, took };
}

/** The news of it for the player, where his corps took tanks or the enemy took his. '' otherwise. */
export function pressedWords(g: Game, p: Pressed, place: string): string {
  const me = g.human;
  if (!me) return '';
  const count = (k: number): string => Math.max(1, Math.round(k)).toLocaleString('en-US');
  const adj = g.nations[p.from]?.adj ?? p.from;
  const mine = p.took.map((t) => ({ c: g.corps.find((c) => c.id === t.id), n: t.n })).filter((t) => t.c?.nat === me);
  if (mine.length) {
    const n = mine.reduce((s, t) => s + t.n, 0);
    const who = mine.length === 1 ? mine[0].c!.name : `${mine.length} of our corps`;
    return `${count(n)} ${adj} tanks knocked out at ${place} are got running again and join ${who}, in our colours. Without spare parts they will not last for ever.`;
  }
  if (p.from === me) return `The enemy has got ${count(p.n)} of our tanks left on the field at ${place} running again, and drives them in his own colours.`;
  return '';
}

/** A nation's month: some of the pressed tanks break down for want of parts, and the corps is the weaker for it. */
export function pressedMonth(g: Game, nat: string): void {
  if (!pressedOn(g)) return;
  for (const c of g.corps) {
    if (c.nat !== nat || !c.kit) continue;
    const per = perOf(c.kind);
    if (c.dead || !per || c.divs <= 0) { delete c.kit; continue; }
    // (None can be more than the corps now holds: what it lost since was lost among its own.)
    const n = Math.min(c.kit.n, (c.str * c.divs * per) / PRESSED.worth);
    const broke = n * Math.min(1, PRESSED.wear * (c.supplied ? 1 : PRESSED.cut));
    c.str = Math.max(0, c.str - (broke * PRESSED.worth) / (per * c.divs));
    const still = n - broke;
    if (still < 1) delete c.kit;
    else c.kit.n = still;
  }
}

/**
 * The look of one side of a battle (battle-bridge.ts): whose tanks its corps have pressed into service and how many of
 * its tank blocks to draw as that make, 1 to 'blocks' by their share of the side's armour. Null with the switch off,
 * or where none of them carries any.
 */
export function kitLook(g: Game, cs: Corps[]): { from: string; n: number } | null {
  if (!pressedOn(g)) return null;
  let kit = 0, all = 0;
  const by: Record<string, number> = {};
  for (const c of cs) {
    const per = perOf(c.kind);
    if (!per || c.dead) continue;
    all += per * c.divs * c.str;
    if (c.kit) { kit += c.kit.n; by[c.kit.from] = (by[c.kit.from] ?? 0) + c.kit.n; }
  }
  if (kit < 1 || all <= 0) return null;
  const from = Object.entries(by).reduce((a, b) => (b[1] > a[1] ? b : a))[0];
  return { from, n: Math.max(1, Math.min(PRESSED.blocks, Math.ceil((10 * kit) / all))) };
}

/** A corps' line on its card: '14 captured Soviet tanks in its own colours'. '' where it has none or the switch is off. */
export function kitLine(g: Game, c: Corps): string {
  if (!pressedOn(g) || !c.kit || c.kit.n < 1) return '';
  const n = Math.round(c.kit.n);
  return `${n.toLocaleString('en-US')} captured ${g.nations[c.kit.from]?.adj ?? c.kit.from} tank${n === 1 ? '' : 's'} in its own colours.`;
}
