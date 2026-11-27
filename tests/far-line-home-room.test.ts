// A corps going back inside the record's line goes to the nearest ground inside it that has room (the design
// director's 13:08Z of 22 November 2026 to ruling 234; src/campaign/ai.ts, `lineHome`).
//
// As built the goal was the nearest ground inside the line, whoever stood on it. In war 11 at the book's first stop the
// reserve rule sent the 23rd Army's brigades from Kukong to Chuanhsien in month 71, the last of Chuanhsien's span;
// later that month the rule that clears the ground the line leaves at 72 took two rows back from Chuanhsien and the
// 11th Army's three from Kweilin into Kukong, which then held five corps and was the brigades' goal: they could not
// step, and stood off the line in August 1945 (test 2 of the six) with Lingling next door open. One map: the goal is a
// place the move can take it. The boards are set by hand.
//
// And it counts its march by roads that have room (his 14:55Z of the same day). The goal had room, the road to it did
// not: in the same war three of the 11th Army's rows stood at Kweilin in month 71, the last of Kweilin's span. Five
// places inside the line stood two marches off and the goal was Swatow, by Kukong; Kukong took its fifth corps that
// month, and the 34th, 58th and 68th and the 3rd and 13th had no step, with Chuanhsien next door empty and Lingling
// beyond it open. In 72 the line gave Chuanhsien back and they stood at Kweilin beyond the line for good.
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

describe('a corps going back inside the line counts its march by roads that have room (the design director, 14:55Z of 22 November 2026)', () => {
  /** The case's board: Kweilin Japan's too and empty, five in Kukong, one in Lingling. */
  const kweilin = (): Game => {
    const g = war();
    const r = g.byName['Kweilin'];
    r.ctrl = 'JAP';
    for (const c of r.units) c.dead = true;
    r.units = [];
    g.supplyDirty = true;
    put(g, 'Kukong', 5);
    put(g, 'Lingling', 1);
    return g;
  };
  const row = (g: Game, name: string, place: string): Corps => { const c = addCorps(g, 'JAP', name, 'inf', 1, place, { xp: 0.3 }); c.ap = 4; return c; };
  const beyond = (g: Game, t: number): string[] => g.corps.filter((c) => c.nat === 'JAP' && !c.dead && c.kind !== 'fort' && lineLeaves(g, 'JAP', g.regions[c.reg], t)).map((c) => `${c.name} at ${where(g, c)}`);

  it('the board is the case: the line leaves Kweilin in 72; Kukong and Chuanhsien are next door, Lingling is not', () => {
    const g = kweilin();
    expect(lineLeaves(g, 'JAP', g.byName['Kweilin'], 71)).toBe(false);
    expect(lineLeaves(g, 'JAP', g.byName['Kweilin'], 72)).toBe(true);
    expect(g.byName['Kweilin'].nb).toContain(g.byName['Kukong'].id);
    expect(g.byName['Kweilin'].nb).toContain(g.byName['Chuanhsien'].id);
    expect(g.byName['Kweilin'].nb).not.toContain(g.byName['Lingling'].id);
  });

  it('five in Kukong: the 34th, 58th and 68th go by Chuanhsien to Lingling in 71, the 3rd and 13th with them, and nobody stands beyond the line in 72', () => {
    const g = kweilin();
    const a = row(g, '11th Army (34th, 58th and 68th Divisions)', 'Kweilin');
    const b = row(g, '11th Army (3rd and 13th Divisions)', 'Kweilin');
    lineHome(g, 'JAP', 72);
    expect([where(g, a), where(g, b)]).toEqual(['Lingling', 'Lingling']);
    // (Each made two marches of one, Kweilin to Chuanhsien and Chuanhsien to Lingling: Lingling is not next to Kweilin.)
    expect([a.ap, b.ap]).toEqual([2, 2]);
    expect(g.byName['Chuanhsien'].units.filter((u) => !u.dead).length).toBe(0);
    expect(g.byName['Kukong'].units.filter((u) => !u.dead).length).toBe(5);
    expect(beyond(g, 72)).toEqual([]);
  });

  it('a place left in Kukong: the first goes to Kukong, as it did, and the next by Chuanhsien to Lingling', () => {
    const g = kweilin();
    g.byName['Kukong'].units[0].dead = true;
    g.byName['Kukong'].units = g.byName['Kukong'].units.filter((u) => !u.dead);
    const a = row(g, 'first', 'Kweilin');
    const b = row(g, 'second', 'Kweilin');
    lineHome(g, 'JAP', 72);
    expect([where(g, a), where(g, b)]).toEqual(['Kukong', 'Lingling']);
    expect(beyond(g, 72)).toEqual([]);
  });

  it('no road with room (five in Kukong and five at Chuanhsien): it stands, as it did', () => {
    const g = kweilin();
    const held = put(g, 'Chuanhsien', 5);
    for (const c of held) c.ap = 0;
    const a = row(g, 'first', 'Kweilin');
    lineHome(g, 'JAP', 72);
    expect(where(g, a)).toBe('Kweilin');
    expect(a.ap).toBe(4);
  });
});
