// Battle schools (release 2, Brian 2026-11-22: "Do 2 and 3"). The player sends a corps standing on its own country's
// soil to the battle schools for a month. It takes no orders while it is there. Left in peace, it comes back one grade
// better when his next month opens; made to fight, cut off or driven from home, it has had no term. The schools teach
// drill, so they bring a corps up to a veteran at most: only battle makes an elite. No computer nation sends any.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { saveGame, loadGame } from '../src/campaign/save';
import { STORIES, atHome, diaryText, drillTo, grade, whyNoSchool } from '../src/campaign/stories';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, canOrder, endHuman, humanUp, orderMove, orderSchool, reach, stepAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';

/** A grand campaign with the player's first month open. */
function playing(human: string, seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
}
/** The player's field corps on their own soil, fit to go to school this month. */
const atHomeCorps = (g: Game): Corps[] =>
  g.corps.filter((c) => c.nat === g.human && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && c.town === undefined && atHome(g, c) && c.supplied && !c.fought && c.ap >= c.apMax);
const lines = (g: Game): string[] => g.headlines.filter((h) => h.turn === g.turn).map((h) => h.text);

describe('the battle school numbers', () => {
  it('bring a corps up to the start of its next grade, and no further than a veteran', () => {
    expect(STORIES.drill.cap).toBe(0.6);
    expect(grade(drillTo(0.05))).toBe('Trained');
    expect(drillTo(0.05)).toBeCloseTo(0.2, 9);
    expect(drillTo(0.25)).toBeCloseTo(0.4, 9);
    expect(drillTo(0.4)).toBeCloseTo(0.6, 9);
    expect(grade(drillTo(0.45))).toBe('Veteran');
    // A veteran is taught nothing more, and an elite loses nothing.
    expect(drillTo(0.6)).toBeCloseTo(0.6, 9);
    expect(drillTo(0.7)).toBeCloseTo(0.7, 9);
    expect(drillTo(0.9)).toBeCloseTo(0.9, 9);
  });
});

describe('a month at the battle schools', () => {
  it('brings a quiet corps at home back one grade better, and it takes no orders while it is away', () => {
    const g = playing('USA');
    const [c, d] = atHomeCorps(g);
    expect(c && d).toBeTruthy();
    c.xp = 0.25; d.xp = 0.25;
    c.str = d.str = 1;
    expect(whyNoSchool(g, c)).toBeNull();
    expect(orderSchool(g, c, true)).toBe(true);
    expect(c.school).toBe(true);
    // At school it cannot be marched: the term is a whole quiet month.
    expect(canOrder(g, c)).toBe(false);
    const to = [...reach(g, d).keys()].find((r) => r !== d.reg && !reach(g, d).get(r)!.attack);
    if (to !== undefined) expect(orderMove(g, c, to)).toBe(0);
    expect(c.ap).toBe(c.apMax);
    expect(whyNoSchool(g, c)).toBe('It is at the battle schools already.');
    // The order lives through a save.
    const back = loadGame(saveGame(g))!;
    expect(back.corps.find((x) => x.id === c.id)!.school).toBe(true);
    month(g);
    expect(c.school).toBeUndefined();
    expect(c.xp).toBeCloseTo(0.4, 6);
    expect(grade(c.xp)).toBe('Seasoned');
    // The corps left at the front grade is unchanged by the month (a quiet month teaches nothing by itself).
    expect(d.xp).toBeLessThan(0.4);
    expect(canOrder(g, c)).toBe(true);
    expect(lines(g).some((t) => t.startsWith(`${c.name} is back from the battle schools`) && t.includes('seasoned now'))).toBe(true);
    const diary = (c.log ?? []).filter((l) => l[1] === 'drill');
    expect(diary.length).toBe(1);
    expect(diaryText(g, c, diary[0])).toMatch(/^A month at the battle schools at .+: came back seasoned\.$/);
  });

  it('trains the drafts too: a worn corps comes back a grade better than it went, not watered down', () => {
    const g = playing('USA');
    const [c] = atHomeCorps(g);
    c.xp = 0.45; c.str = 0.5;
    g.nations.USA.pp = 5000;
    expect(orderSchool(g, c, true)).toBe(true);
    month(g);
    expect(c.str).toBeGreaterThan(0.5);
    expect(c.xp).toBeCloseTo(0.6, 6);
    expect(grade(c.xp)).toBe('Veteran');
  });

  it('can be called back before the month is out, and then it has had no term', () => {
    const g = playing('USA');
    const [c] = atHomeCorps(g);
    c.xp = 0.25;
    expect(orderSchool(g, c, true)).toBe(true);
    expect(orderSchool(g, c, false)).toBe(true);
    expect(c.school).toBeUndefined();
    expect(canOrder(g, c)).toBe(true);
    expect(orderSchool(g, c, false)).toBe(false);
  });

  it('is cut short if the corps has to fight, and the order ends with the month', () => {
    const g = playing('USA');
    const [c] = atHomeCorps(g);
    c.xp = 0.25; c.str = 1;
    expect(orderSchool(g, c, true)).toBe(true);
    // An enemy attack while it is at school: it fought this month.
    c.fought = true;
    month(g);
    expect(c.school).toBeUndefined();
    expect(c.xp).toBeLessThan(0.4);
    expect(lines(g).some((t) => t.startsWith(`${c.name}'s month at the battle schools`) && t.includes('it had to fight'))).toBe(true);
  });

  it('says why a corps cannot go', () => {
    const g = playing('USA');
    const [c, d, e] = atHomeCorps(g);
    c.xp = 0.65;
    expect(whyNoSchool(g, c)).toBe('It is veteran already, and the schools can teach it no more. Only battle makes an elite.');
    expect(orderSchool(g, c, true)).toBe(false);
    d.xp = 0.1; d.fought = true;
    expect(whyNoSchool(g, d)).toMatch(/^It has fought this month already/);
    e.xp = 0.1; e.ap = e.apMax - 1;
    expect(whyNoSchool(g, e)).toMatch(/^It has marched this month already/);
    // A garrison: its month opens with no marches, so it is told why, not that it has marched.
    const town = g.corps.find((x) => x.nat === g.human && !x.dead && x.kind !== 'fort' && x.town !== undefined);
    if (town) expect(whyNoSchool(g, town)).toBe('A garrison keeps to its town: order it out to take the field first.');
    const fake = { ...c, town: c.reg, fought: false, ap: 0 } as Corps;
    expect(whyNoSchool(g, fake)).toBe('A garrison keeps to its town: order it out to take the field first.');
    // Abroad: the schools are at home.
    const abroad = g.corps.find((x) => x.nat === g.human && !x.dead && x.kind !== 'fort' && !atHome(g, x));
    if (abroad) expect(whyNoSchool(g, abroad)).toBe("The battle schools are at home: the corps must stand on its own country's soil.");
    // Another nation's corps never goes to the player's schools.
    const theirs = g.corps.find((x) => x.nat !== g.human && !x.dead && x.kind !== 'fort')!;
    expect(whyNoSchool(g, theirs)).toBe('Only your own corps go to your battle schools.');
    expect(orderSchool(g, theirs, true)).toBe(false);
  });

  it('is open to the player whatever nation he leads, and to no computer nation', () => {
    for (const nat of ['GER', 'UK', 'SOV', 'ITA', 'BRA']) {
      const g = playing(nat, 5);
      const c = atHomeCorps(g).find((x) => drillTo(x.xp) > x.xp);
      if (!c) continue;
      expect(whyNoSchool(g, c)).toBeNull();
    }
    // A computer nation's corps with the flag set by hand is not taught: the reckoning clears it and changes nothing.
    const g = playing('USA');
    const theirs = g.corps.find((x) => x.nat !== g.human && !x.dead && x.kind !== 'fort' && x.str >= 0.99)!;
    const xp = theirs.xp;
    theirs.school = true;
    month(g);
    expect(theirs.school).toBeUndefined();
    expect(theirs.xp).toBeLessThanOrEqual(xp + 1e-9);
  });

  it('is only in the grand campaign', () => {
    const g = createGame(7, 'GER');
    for (let k = 0; !humanUp(g) && k < 60; k++) stepAI(g);
    beginHuman(g);
    const c = g.corps.find((x) => x.nat === 'GER' && !x.dead && x.kind !== 'fort')!;
    expect(whyNoSchool(g, c)).toBe('The battle schools are open only in the grand campaign.');
  });
});
