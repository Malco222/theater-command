// Uniforms wear with time at the front (release 2, Brian's approved ask; looks only, every nation). Each corps keeps how
// worn its uniforms and kit look: 0.1 when it is formed or after a quiet month out of the line, rising to 1 after six
// weeks or more in the line, pulled down by its share of fresh drafts in new kit. The 3D battle reads it on every land
// block (wear in the view), and the campaign's corps card carries it too. Nothing in a fight reads it.
import { describe, expect, it } from 'vitest';
import { unitOut } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { LandBattle, landTicket, sampleTicket, withTeams } from '../src/battle/land';
import { createGame } from '../src/campaign/game';
import { saveGame, loadGame } from '../src/campaign/save';
import { STORIES, atHome, wearMonth, wearOf } from '../src/campaign/stories';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, candidates, endHuman, humanUp, orderMove, reach, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

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
/** The player's field corps at home, full strength, with no enemy near: a quiet month leaves them alone. */
const quiet = (g: Game): Corps[] =>
  g.corps.filter((c) => c.nat === g.human && !c.dead && c.kind !== 'fort' && c.town === undefined && c.joint === undefined && atHome(g, c) && c.supplied && !c.fought && c.ap >= c.apMax);

describe('the wear numbers', () => {
  it('fresh is 0.1, a month in the line 0.7, six weeks or more 1', () => {
    expect(STORIES.wear).toEqual({ fresh: 0.1, weeks: 6, month: 4 });
    const g = playing('USA');
    for (const c of g.corps) expect(wearOf(c)).toBeCloseTo(0.1, 9);
  });
});

describe('a month in the line', () => {
  it('wears a corps that fought to 0.7, and a second month to 1; a quiet month away from the front brings it back fresh', () => {
    const g = playing('USA');
    const [c] = quiet(g);
    c.str = 1;
    c.fought = true;
    month(g);
    expect(c.wear).toBeCloseTo(0.7, 6);
    expect(c.line).toBe(4);
    c.str = 1;
    c.fought = true;
    month(g);
    expect(c.wear).toBeCloseTo(1, 6);
    expect(c.line).toBe(8);
    // It lives through a save.
    const back = loadGame(saveGame(g))!;
    expect(back.corps.find((x) => x.id === c.id)!.wear).toBeCloseTo(1, 6);
    // Rested at home: refitted, new kit.
    c.str = 1;
    month(g);
    expect(c.wear).toBeUndefined();
    expect(c.line).toBeUndefined();
    expect(wearOf(c)).toBeCloseTo(0.1, 9);
  });

  it('counts the weeks it marched where it never came near the enemy', () => {
    const g = playing('USA');
    const c = quiet(g).find((x) => [...reach(g, x).values()].some((v) => !v.attack))!;
    const to = [...reach(g, c).entries()].find(([r, v]) => r !== c.reg && !v.attack)![0];
    c.str = 1;
    const ap = c.ap;
    orderMove(g, c, to);
    const spent = ap - c.ap;
    expect(spent).toBeGreaterThan(0);
    month(g);
    expect(c.wear).toBeCloseTo(0.1 + (0.9 * spent) / 6, 6);
  });

  it('is pulled down by the fresh drafts that joined it, in new kit', () => {
    const g = playing('USA');
    const [c, d] = quiet(g);
    g.nations.USA.pp = 5000;
    c.str = 0.5; c.fought = true;
    d.str = 1; d.fought = true;
    month(g);
    expect(c.str).toBeGreaterThan(0.5);
    const fresh = (c.str - 0.5) / c.str;
    expect(c.wear!).toBeLessThan(d.wear!);
    expect(c.wear!).toBeCloseTo(0.7 * (1 - fresh) + 0.1 * fresh, 2);
    // The share of new men, for a picture that draws them clean: the rest read the old hands' wear, 0.7.
    expect(c.fresh!).toBeCloseTo(fresh, 2);
    expect(d.fresh).toBeUndefined();
    expect((c.wear! - 0.1 * c.fresh!) / (1 - c.fresh!)).toBeCloseTo(0.7, 2);
  });

  it('a whole term at the battle schools is a refit; standing at the front wears it even without a fight', () => {
    const g = playing('USA');
    const [c] = quiet(g);
    c.wear = 0.9; c.line = 6;
    c.pressed = true;
    // (Formed before this month: a corps formed this month is in new kit whatever it did.)
    c.born = g.turn - 1;
    wearMonth(g, c, c.str, true);
    expect(c.wear).toBeUndefined();
    c.wear = 0.4;
    wearMonth(g, c, c.str);
    expect(c.wear).toBeCloseTo(1, 6);
  });

  it('is kept for every nation, the computer\'s too', () => {
    const g = playing('GER');
    month(g); month(g);
    const worn = g.corps.filter((c) => !c.dead && c.wear !== undefined);
    expect(worn.some((c) => c.nat !== 'GER')).toBe(true);
    expect(worn.some((c) => c.nat === 'GER')).toBe(true);
    for (const c of g.corps) { expect(wearOf(c)).toBeGreaterThanOrEqual(0.1 - 1e-9); expect(wearOf(c)).toBeLessThanOrEqual(1 + 1e-9); }
  });
});

describe('the 3D battle reads it on every land block', () => {
  it('a worn corps\' blocks carry its wear; a practice battle\'s are fresh', () => {
    const t = withTeams(sampleTicket('plains', 1.3, { seed: 5 }));
    t.att.forces[0].wear = 0.85; t.att.forces[0].fresh = 0.2;
    const b = new LandBattle(t);
    const first = t.att.forces[0].corps;
    const mine = b.units.filter((u) => u.side === 'att' && u.corps === first);
    expect(mine.length).toBeGreaterThan(0);
    for (const u of mine) { expect(u.wear).toBe(0.85); expect(unitOut(u).wear).toBe(0.85); expect(unitOut(u).fresh).toBe(0.2); }
    for (const u of b.units.filter((x) => x.corps !== first)) { expect(unitOut(u).wear).toBeCloseTo(0.1, 9); expect(unitOut(u).fresh).toBe(0); }
  });

  it('the campaign\'s attack carries each corps\' wear onto the field, and the fight comes out the same', () => {
    const pick = (g: Game): { cs: Corps[]; r: Region } => {
      const r = g.regions.find((x) => x.name === 'Pomorze')!;
      return { r, cs: candidates(g, r).filter((c) => c.name === 'XIX Corps' || c.name === 'XXI Corps') };
    };
    const g = playing('GER'), p = pick(g);
    expect(p.cs.length).toBeGreaterThan(0);
    const plain = landTicket(g, p.cs, p.r)!;
    p.cs[0].wear = 0.7;
    const t = landTicket(g, p.cs, p.r)!;
    expect(t.att.forces.find((f) => f.corps === p.cs[0].id)!.wear).toBe(0.7);
    expect(plain.att.forces.every((f) => f.wear === undefined)).toBe(true);
    const b = new LandBattle(t), a = new LandBattle(plain);
    for (const u of b.units.filter((x) => x.corps === p.cs[0].id)) expect(unitOut(u).wear).toBe(0.7);
    // Looks only: the same battle to the last number.
    b.ai.att = b.ai.def = a.ai.att = a.ai.def = true;
    for (let k = 0; k < 2000 && !(a.over && b.over); k++) { a.step(); b.step(); }
    const sig = (x: LandBattle): string => JSON.stringify({ over: x.over, units: x.units.map((u) => [u.id, u.state, u.str, u.x, u.y]) });
    expect(sig(b)).toBe(sig(a));
  }, 120_000);
});
