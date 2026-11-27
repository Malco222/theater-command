// The marked army (release 2; Brian, 24 November 2026 18:01Z, "yes to all": the third idea of the enemy's month near
// you). The enemy corps that takes one of the player's towns is marked: it wears a red tag on the table, and its card
// says what it took, until he beats it. Beating it (throwing back its attack, or driving it off the field) clears the
// mark with a line in the month's news; so does its end, whoever brings it about. A peace with its country clears the
// mark without a word.
//
// The grand campaign's flag 'marked', set as a campaign opens where stage.json switches it on, or by the player in any
// month: a mark is only a mark, and changes no number for anyone. Off, nothing here is read or written, and every war
// is the war without it (a mark already given is kept, and not shown).

import { atWar, headline } from './game';
import type { BattleReport, Corps, Game, Region } from './types';

/** No more than this many enemy corps carry a mark at once: the first to take his towns keep theirs. */
export const MARKS_MOST = 3;

/** True where this campaign marks the armies that take the player's towns. */
export const markedOn = (g: Game): boolean => g.flags.open === 1 && g.flags.marked === 1;

/** Turns the marked army on or off. Marks already given stay on their corps either way. */
export function setMarked(g: Game, on: boolean): void {
  if (on) g.flags.marked = 1;
  else delete g.flags.marked;
}

/** "The German XIX Corps". */
const named = (g: Game, c: Corps): string => `The ${g.nations[c.nat]?.adj ?? c.nat} ${c.name}`;
const placeOf = (g: Game, c: Corps): string => g.regions[c.mark?.place ?? -1]?.name ?? 'your town';

/** The enemy corps the player sees marked: alive, and still at war with him. */
export function marksOf(g: Game): Corps[] {
  return g.corps.filter((c) => !!c.mark && !c.dead);
}

/**
 * A land battle is over (stories.ts storyBattle). A marked corps on the losing side of a battle the player's own corps
 * won is beaten: its mark comes off. A town of his taken (the place fell, not a siege) marks the corps that led the
 * attack, the biggest of the attackers, where it carries no mark yet and fewer than MARKS_MOST are marked.
 */
export function markBattle(g: Game, rep: BattleReport, target: Region, attackers: Corps[], defenders: Corps[]): void {
  const me = g.human;
  if (!me || !markedOn(g)) return;
  const win = rep.taken;
  const [winners, losers] = win ? [attackers, defenders] : [defenders, attackers];
  if (winners.some((c) => c.nat === me && !c.dead)) {
    for (const c of losers) {
      if (!c.mark || c.dead) continue; // (one destroyed is told as it is struck off: markFallen)
      headline(g, 'info', `You have beaten ${named(g, c).replace(/^The/, 'the')}, the army that took ${placeOf(g, c)}, at ${target.name}. Its red mark comes off the map.`, me, target.id);
      delete c.mark;
    }
  }
  if (!win || rep.siege || rep.attNat === me || rep.defNat !== me || !atWar(g, rep.attNat, me)) return;
  const lead = attackers.filter((c) => !c.dead && c.nat === rep.attNat && c.kind !== 'fort').sort((a, b) => b.divs - a.divs || a.id - b.id)[0];
  if (!lead || lead.mark || marksOf(g).length >= MARKS_MOST) return;
  lead.mark = { place: target.id, turn: g.turn };
  headline(g, 'info', `${named(g, lead)} took ${target.name}. Your staff has marked it in red on the map until you beat it.`, me, target.id);
}

/** A marked corps is struck off (stories.ts storyFallen): by the player's troops, or by anyone else. */
export function markFallen(g: Game, c: Corps, by: string, reg: number): void {
  const me = g.human;
  if (!c.mark) return;
  if (me && markedOn(g)) {
    const where = g.regions[reg]?.name;
    headline(g, 'info', by === me
      ? `${named(g, c)}, the army that took ${placeOf(g, c)}, has been destroyed by your troops${where ? ` at ${where}` : ''}. Its red mark comes off the map.`
      : `${named(g, c)}, the army that took ${placeOf(g, c)}, is no more. Its red mark comes off the map.`, me, reg >= 0 ? reg : undefined);
  }
  delete c.mark;
}

/** At the player's month's end (stories.ts storyMonth): a marked corps whose country is no longer at war with him loses its mark, without a word. */
export function marksMonth(g: Game): void {
  const me = g.human;
  if (!me || !markedOn(g)) return;
  for (const c of g.corps) if (c.mark && (c.dead || !atWar(g, c.nat, me))) delete c.mark;
}
