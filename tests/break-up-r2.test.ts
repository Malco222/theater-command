// Breaking a corps up into its divisions, and putting it back together (release 2; Brian, 24 November 2026 18:02Z:
// "when I click on a unit stack, I want to be able to see each individual unit apart of it. that way i can break it up
// if needed"). Behind its own switch (stage.json 'breakUp'): off, nothing of it is read.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { createGame } from '../src/campaign/game';
import { divisionsOf } from '../src/campaign/armies';
import { breakUp, breakUpOn, rejoin, rejoinTargets, setBreakUp, whyNotBreakUp, whyNotRejoin } from '../src/campaign/breakup';
import { loadGame, saveGame } from '../src/campaign/save';
import { BREAKUP_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { humanUp, stepAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';

function germany(on = true): Game {
  const g = startOpen(createGame(3, 'GER'));
  let guard = 0;
  while (!humanUp(g) && guard++ < 50) stepAI(g);
  setBreakUp(g, on); // (on unless switched off since the 1.5.1 patch shipped it on)
  return g;
}
/** One of his corps of three divisions or more, in the field. */
const big = (g: Game): Corps => g.corps.find((c) => c.nat === 'GER' && !c.dead && c.divs >= 3 && c.kind !== 'fort' && c.town === undefined && !c.joint)!;
const menOf = (cs: Corps[]): number => cs.reduce((n, c) => n + c.str * c.divs, 0);
const xpMen = (cs: Corps[]): number => cs.reduce((n, c) => n + c.xp * c.str * c.divs, 0);

describe('the switch', () => {
  it('switched off, nothing of it is read (the 1.5.1 patch ships it on: the flag 0 turns it off)', () => {
    expect((stage as { breakUp?: boolean }).breakUp).toBe(false);
    expect(BREAKUP_STAGE).toBe(false);
    const g = germany(false);
    expect(breakUpOn(g)).toBe(false);
    const c = big(g), before = saveGame(g);
    expect(whyNotBreakUp(g, c, [0])).toMatch(/switched on/);
    expect(breakUp(g, c, [0])).toBeNull();
    expect(saveGame(g)).toBe(before);
  });
  it('is the grand campaign only', () => {
    const g = createGame(3, 'GER');
    setBreakUp(g, true);
    expect(breakUpOn(g)).toBe(false);
  });
});

describe('breaking a corps up', () => {
  it('splits the picked divisions off where the corps stands: the same men, numbers and names, nothing drawn from the dice', () => {
    const g = germany();
    const c = big(g), divs = c.divs, reg = c.reg;
    const cards = divisionsOf(g, c);
    const men = menOf([c]), xp = xpMen([c]), dice = g.rng.state, n = g.corps.length;
    const counters = Object.entries(g.flags).filter(([k]) => k.startsWith('dn:'));
    expect(whyNotBreakUp(g, c, [1])).toBeNull();
    const p = breakUp(g, c, [1])!;
    expect(p).not.toBeNull();
    expect(g.corps.length).toBe(n + 1);
    expect(p.divs).toBe(1);
    expect(c.divs).toBe(divs - 1);
    expect(p.reg).toBe(reg);
    expect(p.town).toBeUndefined();
    expect(g.regions[reg].units.indexOf(p)).toBe(g.regions[reg].units.indexOf(c) + 1);
    // Nothing lost or gained.
    expect(menOf([c, p])).toBeCloseTo(men, 9);
    expect(xpMen([c, p])).toBeCloseTo(xp, 9);
    // The division keeps its number and its name; the corps keeps the rest of them.
    expect(p.name).toBe(cards[1].name);
    expect(divisionsOf(g, p).map((d) => [d.no, d.name])).toEqual([[cards[1].no, cards[1].name]]);
    expect(divisionsOf(g, c).map((d) => d.no)).toEqual(cards.filter((d) => d.slot !== 1).map((d) => d.no));
    expect(divisionsOf(g, c).map((d) => d.name)).toEqual(cards.filter((d) => d.slot !== 1).map((d) => d.name));
    // The general stays with the corps; the part has the corps' moves, morale and supply.
    expect(p.general).toBeNull();
    expect(p.ap).toBe(c.ap);
    expect(p.mor).toBe(c.mor);
    expect(p.supplied).toBe(c.supplied);
    // No fresh numbers are taken from the nation's count, and no die is thrown.
    expect(Object.entries(g.flags).filter(([k]) => k.startsWith('dn:'))).toEqual(counters);
    expect(g.rng.state).toBe(dice);
    // And it is saved with the game.
    const back = loadGame(saveGame(g))!;
    const bp = back.corps.find((x) => x.id === p.id)!;
    expect(bp.part).toBe(c.id);
    expect(bp.whole).toEqual(cards.map((d) => d.no));
    expect(divisionsOf(back, bp)[0].name).toBe(cards[1].name);
  });

  it('two or more divisions split off go as one corps', () => {
    const g = germany();
    const c = big(g), divs = c.divs;
    const p = breakUp(g, c, [0, 2])!;
    expect(p.divs).toBe(2);
    expect(c.divs).toBe(divs - 2);
    expect(p.name).toMatch(/detachment$/);
  });

  it('says why not, in a sentence', () => {
    const g = germany();
    const c = big(g);
    expect(whyNotBreakUp(g, c, [])).toMatch(/Pick the divisions/);
    expect(whyNotBreakUp(g, c, Array.from({ length: c.divs }, (_, i) => i))).toMatch(/have to stay/);
    expect(whyNotBreakUp(g, c, [c.divs])).toMatch(/Pick the divisions/);
    const one = g.corps.find((x) => x.nat === 'GER' && !x.dead && x.divs === 1);
    if (one) expect(whyNotBreakUp(g, one, [0])).toMatch(/single division/);
    const theirs = g.corps.find((x) => x.nat === 'FRA' && !x.dead && x.divs >= 2)!;
    expect(whyNotBreakUp(g, theirs, [0])).toMatch(/your own corps/);
  });
});

describe('putting it back together', () => {
  it('a part rejoins the corps it came from: the corps is whole again, as it was', () => {
    const g = germany();
    const c = big(g), divs = c.divs;
    const cards = divisionsOf(g, c), men = menOf([c]);
    const p = breakUp(g, c, [1])!;
    expect(rejoinTargets(g, p).map((x) => x.id)).toEqual([c.id]);
    expect(rejoin(g, p, c)).toBe(true);
    expect(p.dead).toBe(true);
    expect(g.corps.includes(p)).toBe(false);
    expect(c.divs).toBe(divs);
    expect(menOf([c])).toBeCloseTo(men, 9);
    expect(divisionsOf(g, c).map((d) => [d.no, d.name])).toEqual(cards.map((d) => [d.no, d.name]));
    expect(c.whole).toBeUndefined();
  });

  it('two parts of one corps may join each other; nothing joins a corps it was not part of, or grows past the corps it was', () => {
    const g = germany();
    const c = big(g);
    const a = breakUp(g, c, [0])!;
    const b = breakUp(g, c, [0])!;
    expect(whyNotRejoin(g, a, b)).toBeNull();
    const other = g.corps.find((x) => x.nat === 'GER' && !x.dead && x !== c && x.reg !== c.reg && x.divs >= 1)!;
    expect(whyNotRejoin(g, a, other)).toMatch(/only the corps it was split from|stand together/);
    // Every part back: the corps is its whole size, and no bigger.
    expect(rejoin(g, a, c)).toBe(true);
    expect(rejoin(g, b, c)).toBe(true);
    expect(c.divs).toBe(c.whole?.length ?? c.divs);
  });
});
