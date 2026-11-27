// "Which has been lost" is said only of a corps on the memorial wall (arrivalwaits.ts `waitedOn`). A told row's corps
// that was at the front and is gone from the board when the row forms is called lost only where `g.stories.fallen`
// (stories.ts `storyFallen`: destroyed, surrendered or starved) holds a corps of the row's nation and that name, fallen
// in or after the month the row was told (`rowTold:<i>`, the turn of the telling). Gone for any other cause it was not
// lost, and the row forms with today's bare line ("Test Army has arrived in Magdeburg."): round 2's T2-1 (a corps taken
// by another row the same month) and T2-2 (a corps the player folded into another), and every other cause at once.
// Where it is on the wall: the UI director's section 8 point 1 (02:07Z, 26 November 2026), word for word: "{The 25th
// Army (18th Division)} has {landed at / arrived in} {Hainan}, formed without your {21st Army (18th Division)}, which
// has been lost."
//
// The board is tests/from-wait-r2's: September 1939, Grenzmark beside Poznan (Poland's Army Poznan and the Wielkopolska
// Cavalry there), the Saar beside France, Polesie Poland's; Bavaria and Magdeburg quiet. Germany is the player's, the
// switch on by hand. Rows are added at the foot of the data's list and taken off. A corps is lost here only as the rules
// lose one: in a battle (game.ts `resolveBattle`), in a town that falls (siege.ts `fall`), or starved (`startPhase`).
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { breakUp, rejoin, whyNotBreakUp, whyNotRejoin } from '../src/campaign/breakup';
import { corpsInContact } from '../src/campaign/drawn';
import { foldCorps, whyNotFold } from '../src/campaign/fold';
import { addCorps, createGame, relocate, resolveBattle, startPhase } from '../src/campaign/game';
import { fall, garrisonOf, joinTown } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } });
const addRow = (row: any[]): number => { ROWS.push(row); added++; return ROWS.length - 1; };
/**
 * (The contact rule's second form, the design director's ruling of 06:10Z, 26 November 2026: a corps beside the enemy
 * is held while it took part in the battle last resolved on its ground, or where nobody of its side keeps its province.
 * This board's corps at the front stand with others of their side, so each is set, by hand, as having fought the battle
 * last resolved where it stands, and is held as the board means. The relieved case: tests/contact-fact.test.ts.)
 */
const fought = (g: Game): void => { if (g.flags.fromWait !== 1) return; g.lastBattle = {}; for (const c of g.corps) if (!c.dead) (g.lastBattle[c.reg] ??= []).push(c.id); };
const month = (g: Game, t: number): void => { fought(g); g.turn = t; startPhase(g, 'GER'); };
const said = (g: Game, t: number): string[] => g.headlines.filter((h) => h.turn === t && h.nat === 'GER' && /Test/.test(h.text)).map((h) => h.text);
const tel = (g: Game, i: number): string[] => Object.keys(g.flags).filter((k) => /^row[A-Za-z]+:/.test(k) && k.split(':')[1] === String(i));
const wall = (g: Game): string[] => g.stories.fallen.map((f) => `${f.nat} ${f.name} ${f.turn} ${f.how}`);
const open = (): Game => { const g = startOpen(createGame(1, 'GER')); g.flags.fromWait = 1; return g; };

const BEGINS = 'The Test Army is due in Magdeburg this month, formed from your Test Corps, which is at the front in the Grenzmark. The arrival waits, and forms the month after that corps leaves the front.';
const LOST = 'The Test Army has arrived in Magdeburg, formed without your Test Corps, which has been lost.';
const BARE = 'Test Army has arrived in Magdeburg.';

/** The corps beaten in the Grenzmark by Poland's two corps from Poznan, with nobody of its side beside it: destroyed, as the rules destroy one. */
function destroy(g: Game, c: Corps): void {
  const gz = g.regions[c.reg];
  for (const u of [...gz.units]) if (u !== c && u.nat === 'GER') relocate(g, u, g.byName.Bavaria);
  c.str = 0.05;
  const att = g.byName.Poznan.units.filter((u) => u.nat === 'POL');
  for (const u of att) { u.ap = u.apMax; u.str = 1; u.org = 1; }
  resolveBattle(g, att, gz);
  expect(c.dead).toBe(true);
}

describe('T2-1: two rows on one corps, the corps taken by the first', () => {
  it('the first row says section 4, the second today\'s bare line, and nothing in the war says "lost"', () => {
    const g = open();
    const c = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    const a = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]);
    const b = addRow([2, 'GER', 'Second Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]);
    month(g, 2);
    expect(said(g, 2)).toEqual([BEGINS, 'The Second Test Army is due in Magdeburg this month, formed from your Test Corps, which is at the front in the Grenzmark. The arrival waits, and forms the month after that corps leaves the front.']);
    // (`rowTold` is the turn of the telling.)
    expect([g.flags[`rowTold:${a}`], g.flags[`rowTold:${b}`]]).toEqual([2, 2]);
    relocate(g, c, g.byName.Bavaria);
    month(g, 3);
    expect([g.flags[`sent:${a}`], g.flags[`sent:${b}`]]).toEqual([1, 1]);
    expect(said(g, 3)).toEqual(['The Test Army has arrived in Magdeburg, formed from your Test Corps, now that it has left the front.', 'Second Test Army has arrived in Magdeburg.']);
    expect(g.headlines.filter((h) => /lost/.test(h.text)).map((h) => h.text)).toEqual([]);
    expect(wall(g)).toEqual([]);
    expect([...tel(g, a), ...tel(g, b)]).toEqual([]);
  });
});

describe('T2-2: the corps folded by the player while the row waits', () => {
  it('folded into another of his corps after it left the front: today\'s bare line', () => {
    const g = open();
    const c = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    const k = addCorps(g, 'GER', 'Keep Test Corps', 'inf', 2, g.byName.Bavaria.id, {});
    const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]);
    month(g, 2);
    expect(said(g, 2)).toEqual([BEGINS]);
    relocate(g, c, g.byName.Bavaria);
    c.str = 0.3; k.str = 0.3; c.supplied = true; k.supplied = true;
    expect(whyNotFold(g, k, c)).toBe(null);
    expect(foldCorps(g, k, c)).toBe(true);
    expect(c.dead).toBe(true);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 3)).toEqual([BARE]);
    expect(wall(g)).toEqual([]);
    expect(tel(g, i)).toEqual([]);
  });
});

describe('a corps of the mask broken up while the row waits (breakup.ts)', () => {
  /** The Test Corps of three divisions at the front, told in month 2; then broken up in his own month, one division split off. */
  const split = (): { g: Game; c: Corps; p: Corps; i: number } => {
    const g = open();
    g.flags.breakUp = 1;
    const c = addCorps(g, 'GER', 'Test Corps', 'inf', 3, g.byName.Grenzmark.id, {});
    const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]);
    month(g, 2);
    expect(said(g, 2)).toEqual([BEGINS]);
    g.phase = g.order.indexOf('GER');
    expect(whyNotBreakUp(g, c, [0])).toBe(null);
    const p = breakUp(g, c, [0])!;
    // (The part takes its division's own name; the corps keeps its name and its id.)
    expect(p.name).not.toBe('Test Corps');
    expect(c.name).toBe('Test Corps');
    relocate(g, c, g.byName.Bavaria); relocate(g, p, g.byName.Bavaria);
    return { g, c, p, i };
  };

  it('the corps keeps its name, and the row takes it: section 4, which is true', () => {
    const { g, i } = split();
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 3)).toEqual(['The Test Army has arrived in Magdeburg, formed from your Test Corps, now that it has left the front.']);
  });

  it('the corps put back into its part (it leaves the map, its men in the part): today\'s bare line', () => {
    const { g, c, p, i } = split();
    expect(whyNotRejoin(g, c, p)).toBe(null);
    expect(rejoin(g, c, p)).toBe(true);
    expect(c.dead).toBe(true);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 3)).toEqual([BARE]);
    expect(wall(g)).toEqual([]);
  });
});

describe('a corps of the mask on the memorial wall: section 8 point 1', () => {
  it('destroyed in battle while the row waits: "formed without your Test Corps, which has been lost"', () => {
    const g = open();
    const c = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]);
    month(g, 2);
    expect(said(g, 2)).toEqual([BEGINS]);
    destroy(g, c);
    expect(wall(g)).toEqual(['GER Test Corps 2 destroyed']);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 3)).toEqual([LOST]);
    expect(tel(g, i)).toEqual([]);
  });

  it('surrendered with the town it held (siege.ts `fall`): the same sentence', () => {
    const g = open();
    const gz = g.byName.Grenzmark;
    const c = addCorps(g, 'GER', 'Test Corps', 'inf', 2, gz.id, {});
    const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]);
    month(g, 2);
    expect(said(g, 2)).toEqual([BEGINS]);
    gz.units.splice(gz.units.indexOf(c), 1);
    joinTown(g, gz, c);
    expect(garrisonOf(gz)).toEqual([c]);
    fall(g, gz, 'POL', 'storm');
    expect(c.dead).toBe(true);
    expect(wall(g)).toEqual(['GER Test Corps 2 surrendered']);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 3)).toEqual([LOST]);
  });

  it('starved, cut off in Polesie: the same sentence', () => {
    const g = open();
    const c = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Polesie.id, {});
    c.org = 0.1;
    expect(corpsInContact(g, 'GER', c)).toBe(true);
    const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]);
    // (Told in the month's arrivals, then starved in the same month's supply, which comes after them.)
    month(g, 2);
    expect(said(g, 2)).toEqual([
      'The Test Army is due in Magdeburg this month, formed from your Test Corps, which is at the front in Polesie. The arrival waits, and forms the month after that corps leaves the front.',
      "Test Corps is struck off the army's list and written on the memorial wall.",
    ]);
    expect(c.dead).toBe(true);
    expect(wall(g)).toEqual(['GER Test Corps 2 starved']);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 3)).toEqual([LOST]);
  });
});

describe('the wall read from the month the row was told', () => {
  it('a corps of the same name fallen before the telling, the masked corps then taken by another row: today\'s bare line', () => {
    const g = open();
    // (An older Test Corps, destroyed in the Grenzmark in month 1: on the wall before any row is told.)
    g.turn = 1;
    destroy(g, addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {}));
    expect(wall(g)).toEqual(['GER Test Corps 1 destroyed']);
    // (A new Test Corps at the front, and two rows on it.)
    const c = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    const a = addRow([2, 'GER', 'Second Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]);
    const b = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]);
    month(g, 2);
    expect(said(g, 2)).toContain(BEGINS);
    expect(g.flags[`rowTold:${b}`]).toBe(2);
    relocate(g, c, g.byName.Bavaria);
    month(g, 3);
    expect([g.flags[`sent:${a}`], g.flags[`sent:${b}`]]).toEqual([1, 1]);
    expect(said(g, 3)).toEqual(['The Second Test Army has arrived in Magdeburg, formed from your Test Corps, now that it has left the front.', BARE]);
  });

  it('one masked corps on the wall and another gone for another cause (folded): today\'s bare line', () => {
    const g = open();
    const c = addCorps(g, 'GER', 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
    const s = addCorps(g, 'GER', 'Second Test Corps', 'inf', 2, g.byName.Saar.id, {});
    const k = addCorps(g, 'GER', 'Keep Test Corps', 'inf', 2, g.byName.Bavaria.id, {});
    fought(g);
    expect(corpsInContact(g, 'GER', s)).toBe(true);
    const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps', 'Second Test Corps'] }]);
    month(g, 2);
    expect(said(g, 2)).toEqual(['The Test Army is due in Magdeburg this month, formed from two of your corps. The Test Corps in the Grenzmark and the Second Test Corps in the Saar are at the front, so the arrival waits, and forms the month after both leave the front.']);
    destroy(g, c);
    relocate(g, s, g.byName.Bavaria);
    s.str = 0.3; k.str = 0.3; s.supplied = true; k.supplied = true;
    expect(foldCorps(g, k, s)).toBe(true);
    expect(wall(g)).toEqual(['GER Test Corps 2 destroyed']);
    month(g, 3);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(said(g, 3)).toEqual([BARE]);
  });
});
