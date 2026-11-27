// The 'held the line' medal (release 2).
//
// Brian, 27 November 2026 13:40Z, yes to the playtester's idea: "a corps that survives a battle against long odds earns
// a medal that lifts its spirit for the rest of the war". Under this switch:
//
// 1. Long odds are the battle's own: the attack's chance of carrying the ground at least 'chance' (three in four), as
//    the battle reckoned it before the dice from the two sides' power and the run of luck the ground allows (game.ts
//    chanceOn, the attack card's own figure). No die is thrown for the medal.
// 2. Only a defence that held: the attack did not carry the ground, and the corps is still in being after it. A walk-in
//    is no battle. A fort, which cannot be anywhere else, is not a corps that chose to stand.
// 3. Once to a corps. It is felt at once on morale's own scale, and for the rest of the war its morale settles a little
//    higher (morale.ts levelOf reads 'held'). So it changes fights a little: the DD's fairness gate decides its ON.
// 4. Every army's corps alike, the player's and the computers' (the sandbox rule: from what happened on the field).
// 5. It shows as a ribbon on the corps' card (campaign-bridge.ts honours), and the player is told when his corps earns it.
//
// With the switch off nothing here is read and nothing is written.

import data from '../../data/campaign/heldline.json';
import { feel, moraleRules } from './morale';
import type { BattleReport, Corps, Game } from './types';

export const HELD = data as unknown as { chance: number; felt: number; lift: number; ribbon: [string, string] };

/** The switch: a grand campaign's own, off unless the release 2 ship list or the player turns it on. */
export const heldOn = (g: Game): boolean => g.flags.heldline === 1;

/** Turns the switch on or off at any time. Off takes every medal back (the morale already felt stays as it is). */
export function setHeld(g: Game, on: boolean): boolean {
  // (Only the grand campaign keeps morale, morale.ts moraleRules: elsewhere the medal would lift nothing.)
  if (on) {
    if (!moraleRules(g)) return false;
    g.flags.heldline = 1;
    return true;
  }
  delete g.flags.heldline;
  for (const c of g.corps) delete c.held;
  for (const v of g.atSea ?? []) delete v.corps.held;
  return true;
}

/**
 * After a land battle (resolveBattle, beside moraleBattle's own): the defenders who held against long odds earn the
 * medal. 'attack' is the attack's chance of carrying the ground, as the battle reckoned it before the dice. Returns the
 * corps that earned it now, [] where none did.
 */
export function heldTheLine(g: Game, rep: BattleReport, defenders: Corps[], attack: number): Corps[] {
  if (!heldOn(g) || rep.yielded || rep.taken || !(attack >= HELD.chance)) return [];
  const chance = Math.round((1 - Math.min(1, attack)) * 100) / 100;
  const out: Corps[] = [];
  for (const c of defenders) {
    if (c.dead || c.divs <= 0 || c.kind === 'fort' || c.held) continue;
    c.held = { at: rep.region, turn: g.turn, chance };
    if (c.mor !== undefined) feel(c, 'heldline', HELD.felt);
    out.push(c);
  }
  return out;
}

/** A defence's chance, as a soldier would put it: 'one chance in four'; 'no chance at all' where it had none. */
export const chanceWords = (p: number): string => {
  if (!(p > 0)) return 'no chance at all';
  const n = Math.max(2, Math.round(1 / p));
  const w = ['', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten'][n] ?? n.toLocaleString('en-US');
  return `one chance in ${w}`;
};

/** The news of it for the player, where his own corps earned it. '' otherwise. */
export function heldWords(g: Game, got: Corps[], place: string): string {
  const mine = got.filter((c) => c.nat === g.human);
  if (!mine.length) return '';
  const p = Math.min(...mine.map((c) => c.held!.chance));
  const who = mine.length === 1 ? mine[0].name : `${mine.length} of our corps`;
  return `${who} held the line at ${place} with ${chanceWords(p)}, and ${mine.length === 1 ? 'wears' : 'wear'} the ribbon for it. The men will not forget it.`;
}

/** The medal as a ribbon for a corps' card: its title and colours. Null with the switch off, or where it has none. */
export function heldRibbon(g: Game, c: Corps): { title: string; ribbon: [string, string] } | null {
  if (!heldOn(g) || !c.held) return null;
  return { title: `Held the line: ${g.regions[c.held.at]?.name ?? 'the line'}`, ribbon: HELD.ribbon };
}
