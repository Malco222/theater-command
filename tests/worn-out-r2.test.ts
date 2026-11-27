// Worn out in the line (release 2; Brian's yes of 27 November 2026 17:10Z; the design director's ruling of 17:15Z):
// each month a corps spends in the line takes a little of its fighting power, to a cap, and a month at rest brings it
// all back. The battle, the attack card and every staff read it through one function. Every army alike. With the
// switch off nothing is written and the war is the one it always was.
import { describe, expect, it } from 'vitest';
import { WORN, blunted, edge, setWorn, wornLine, wornMonth, wornOn } from '../src/campaign/wornout';
import { addCorps, assess, createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { loadGame, saveGame } from '../src/campaign/save';
import type { Corps, Game, Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const war = (seed = 3, human: string | null = 'GER'): Game => startOpen(createGame(seed, human));
const corps = (g: Game, nat: string, reg: number, kind: Corps['kind'] = 'inf'): Corps => addCorps(g, nat, `${nat} ${kind} worn test corps`, kind, 2, reg, {});
/** A German province with a Polish one next door, at war on the first day. */
function front(g: Game): { here: Region; there: Region } {
  for (const r of g.regions) {
    if (r.ctrl !== 'GER') continue;
    const j = r.nb.find((x) => g.regions[x].ctrl === 'POL');
    if (j !== undefined) return { here: r, there: g.regions[j] };
  }
  throw new Error('no front');
}
/** A month in the line: it fought, and its moves are spent. */
const fought = (c: Corps): void => { c.fought = true; c.ap = 0; };
/** A month at rest: in supply, no march, no fight. */
const rested = (c: Corps): void => { c.fought = false; c.ap = c.apMax; c.supplied = true; };

describe('worn out in the line (release 2)', () => {
  it('is off unless switched on: nothing is written and edge() is exactly 1', () => {
    const g = war();
    const { here } = front(g);
    const c = corps(g, 'GER', here.id);
    expect(wornOn(g)).toBe(false);
    for (let m = 0; m < 8; m++) { fought(c); wornMonth(g, c); }
    expect(c.spent).toBeUndefined();
    expect(c.runIn).toBeUndefined();
    expect(edge(c)).toBe(1);
    expect(blunted(c)).toBe(false);
    expect(wornLine(c)).toBe('');
  });

  it('every month in the line takes a step more, from the first, to the cap; a month at rest brings it all back', () => {
    const g = war();
    setWorn(g, true);
    const { here } = front(g);
    const c = corps(g, 'GER', here.id);
    c.born = g.turn - 1;
    const steps = Math.round(WORN.most / WORN.step);
    for (let m = 1; m <= steps + 3; m++) {
      fought(c);
      wornMonth(g, c);
      expect(c.spent).toBeCloseTo(Math.min(WORN.most, WORN.step * m), 12);
      expect(c.runIn).toBe(m);
    }
    expect(edge(c)).toBeCloseTo(1 - WORN.most, 12);
    expect(blunted(c)).toBe(true);
    expect(wornLine(c)).toBe(`Worn out: ${Math.round(WORN.most * 100)}% weaker after ${steps + 3} months in the line. A month at rest brings it back.`);
    rested(c);
    wornMonth(g, c);
    expect(c.spent).toBeUndefined();
    expect(edge(c)).toBe(1);
  });

  it('in the line without a fight counts (an enemy next door); a march behind the front keeps the loss but adds none', () => {
    const g = war();
    setWorn(g, true);
    const { here } = front(g);
    const c = corps(g, 'GER', here.id);
    c.born = g.turn - 1;
    c.fought = false; c.ap = 0; // it marched, and the enemy holds the ground next door
    wornMonth(g, c);
    expect(c.spent).toBeCloseTo(WORN.step, 12);
    // Behind the front: a German province with no enemy beside it.
    const back = g.regions.find((r) => r.ctrl === 'GER' && !r.nb.some((j) => { const o = g.regions[j].ctrl; return o && o !== 'GER' && o !== 'SVK'; }) && !r.units.some((u) => u.nat !== 'GER'))!;
    c.reg = back.id;
    c.fought = false; c.ap = 0;
    wornMonth(g, c);
    expect(c.spent).toBeCloseTo(WORN.step, 12);
    expect(c.runIn).toBeUndefined();
  });

  it('the battle reads it inside its own assessment, the card says it, and both sides alike', () => {
    const g = war();
    const { here, there } = front(g);
    const att = corps(g, 'GER', here.id, 'inf');
    const base = assess(g, [att], there);
    setWorn(g, true);
    att.spent = WORN.most;
    const worn = assess(g, [att], there);
    expect(worn.odds).toBeCloseTo(base.odds * (1 - WORN.most), 9);
    const line = worn.lines.find((l) => l.label === 'Worn out');
    expect(line?.value).toBe('Attackers worn by months in the line');
    expect(line?.effect).toBe(`Attack cut by up to ${Math.round(WORN.most * 100)}%`);
    // The defenders' wear is read the same way.
    att.spent = undefined;
    for (const d of there.units) d.spent = 0.04;
    const def = assess(g, [att], there);
    expect(def.odds).toBeGreaterThan(base.odds);
    expect(def.lines.find((l) => l.label === 'Worn out')?.effect).toBe('Defence cut by up to 4%');
  });

  it('a computer war wears and rests by the same rule, keeps it through a save, and the switch off gives every edge back', () => {
    const g = war(17, null);
    setWorn(g, true);
    for (let m = 0; m < 6; m++) runTurnAI(g);
    const worn = g.corps.filter((c) => c.spent);
    expect(worn.length).toBeGreaterThan(0);
    expect(new Set(worn.map((c) => c.nat)).size).toBeGreaterThan(1);
    expect(worn.every((c) => c.spent! > 0 && c.spent! <= WORN.most + 1e-12 && c.kind !== 'fort')).toBe(true);
    const back = loadGame(saveGame(g))!;
    expect(back.corps.filter((c) => c.spent).map((c) => [c.id, c.spent])).toEqual(worn.map((c) => [c.id, c.spent]));
    setWorn(g, false);
    expect(g.corps.some((c) => c.spent !== undefined || c.runIn !== undefined)).toBe(false);
  });

  it('the 3D ship list turns it on once, and a switch the player turns off stays off', () => {
    TC().start({ view: 'grand', human: 'GER', seed: 11 });
    const read = (): Game => loadGame((TC().save() as { text: string }).text)!;
    expect(wornOn(read())).toBe(false);
    expect(TC().r2Ship(['wornoutrule'])).not.toBeNull();
    expect(wornOn(read())).toBe(true);
    TC().act('wornoutrule:off');
    expect(TC().r2Ship(['wornoutrule'])).toBeNull();
    expect(wornOn(read())).toBe(false);
  });
});
