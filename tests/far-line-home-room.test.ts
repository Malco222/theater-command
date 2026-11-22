// A corps going back inside the record's line goes to the nearest ground inside it that has room (the design
// director's 13:08Z of 22 November 2026 to ruling 234; src/campaign/ai.ts, `lineHome`).
//
// As built the goal was the nearest ground inside the line, whoever stood on it. In war 11 at the book's first stop the
// reserve rule sent the 23rd Army's brigades from Kukong to Chuanhsien in month 71, the last of Chuanhsien's span;
// later that month the rule that clears the ground the line leaves at 72 took two rows back from Chuanhsien and the
// 11th Army's three from Kweilin into Kukong, which then held five corps and was the brigades' goal: they could not
// step, and stood off the line in August 1945 (test 2 of the six) with Lingling next door open. One map: the goal is a
// place the move can take it. The boards are set by hand.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { lineHome } from '../src/campaign/ai';
import { addCorps, atWar, createGame, declareWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { lineLeaves } from '../src/campaign/line';
import { startOpen } from '../src/campaign/staff';
import { STACK, type Corps, type Game } from '../src/campaign/types';

/** A war at stop 1 in month 71: Japan at war with China, the choices of 1944 and 1945 gone, the three places Japan's and empty. */
const war = (): Game => {
  const g = startOpen(createGame(3, null));
  setStop(g, 1, true);
  if (!atWar(g, 'JAP', 'CHN')) declareWar(g, 'JAP', 'CHN');
  g.turn = 71;
  g.flags['st:done:JAP:ichigo'] = 57;
  g.flags['st:done:JAP:kukong45'] = 64;
  for (const name of ['Kukong', 'Lingling', 'Chuanhsien']) {
    const r = g.byName[name];
    r.ctrl = 'JAP';
    for (const c of r.units) c.dead = true;
    r.units = [];
  }
  g.supplyDirty = true;
  return g;
};
const put = (g: Game, place: string, n: number): Corps[] => Array.from({ length: n }, (_, k) => addCorps(g, 'JAP', `${place} row ${k + 1}`, 'inf', 1, place, { xp: 0.3 }));
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;

describe('a corps going back inside the line goes to the nearest ground inside it that has room (the design director, 13:08Z of 22 November 2026)', () => {
  it('the board is the case: in month 72 the line leaves Chuanhsien and keeps Kukong and Lingling, both next door', () => {
    const g = war();
    expect(STACK).toBe(5);
    expect(lineLeaves(g, 'JAP', g.byName['Chuanhsien'], 72)).toBe(true);
    expect(lineLeaves(g, 'JAP', g.byName['Kukong'], 72)).toBe(false);
    expect(lineLeaves(g, 'JAP', g.byName['Lingling'], 72)).toBe(false);
    expect(g.byName['Chuanhsien'].nb).toContain(g.byName['Kukong'].id);
    expect(g.byName['Chuanhsien'].nb).toContain(g.byName['Lingling'].id);
    // (Kukong is the first of the two in province order: the goal as built.)
    expect(g.byName['Kukong'].id).toBeLessThan(g.byName['Lingling'].id);
  });

  it('five in Kukong: the row at Chuanhsien goes to Lingling', () => {
    const g = war();
    put(g, 'Kukong', 5);
    put(g, 'Lingling', 1);
    const [c] = put(g, 'Chuanhsien', 1);
    lineHome(g, 'JAP', 72);
    expect(where(g, c)).toBe('Lingling');
    expect(g.byName['Kukong'].units.filter((u) => !u.dead).length).toBe(5);
  });

  it('four in Kukong: it goes to Kukong, as it did', () => {
    const g = war();
    put(g, 'Kukong', 4);
    put(g, 'Lingling', 1);
    const [c] = put(g, 'Chuanhsien', 1);
    lineHome(g, 'JAP', 72);
    expect(where(g, c)).toBe('Kukong');
  });

  it('two rows at Chuanhsien and one place left in Kukong: one fills Kukong, and the other goes to Lingling', () => {
    const g = war();
    put(g, 'Kukong', 4);
    const rows = put(g, 'Chuanhsien', 2);
    lineHome(g, 'JAP', 72);
    expect(rows.map((c) => where(g, c)).sort()).toEqual(['Kukong', 'Lingling']);
  });
});
