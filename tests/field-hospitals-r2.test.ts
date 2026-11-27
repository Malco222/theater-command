// Field hospitals (release 2; Brian's yes of 27 November 2026 17:10Z; the design director's notes of 17:15Z): of what a
// corps loses in battle a share are wounded, kept on the corps; while it is in supply a share of them rejoin it each
// month, free, more with a Medicine building near; cut off, some are lost. Under the switch the Medicine building's
// quicker drafts (which stood in for the wounded) go and its rest stays. Every army alike. With the switch off nothing is
// written and the war is the one it always was.
import { describe, expect, it } from 'vitest';
import { HOSP, hospOn, setHosp, woundedBook, woundedLine, woundedMonth } from '../src/campaign/fieldhospitals';
import { addCorps, createGame, resolveBattle, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { loadGame, saveGame } from '../src/campaign/save';
import type { Corps, Game, Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const war = (seed = 3, human: string | null = 'GER'): Game => startOpen(createGame(seed, human));
const corps = (g: Game, nat: string, reg: number, kind: Corps['kind'] = 'inf'): Corps => addCorps(g, nat, `${nat} ${kind} hospital test corps`, kind, 2, reg, {});
const clone = (g: Game): Game => loadGame(saveGame(g))!;
const built = (g: Game, line: string, r: Region, level: number): void => { g.flags['cv:any'] = 1; g.flags[`cv:${line}:${r.id}`] = level; };
function front(g: Game): { here: Region; there: Region } {
  for (const r of g.regions) {
    if (r.ctrl !== 'GER') continue;
    const j = r.nb.find((x) => g.regions[x].ctrl === 'POL');
    if (j !== undefined) return { here: r, there: g.regions[j] };
  }
  throw new Error('no front');
}
/** A German province at home with no enemy beside it. */
const quiet = (g: Game): Region => g.regions.find((r) => r.ctrl === 'GER' && r.owner === 'GER'
  && !r.nb.some((j) => { const o = g.regions[j].ctrl; return o && o !== 'GER' && o !== 'SVK'; }))!;

describe('field hospitals (release 2)', () => {
  it('is off unless switched on: a battle books no wounded and the month gives none back', () => {
    const g = war();
    const { here, there } = front(g);
    const att = corps(g, 'GER', here.id);
    expect(hospOn(g)).toBe(false);
    woundedBook(g, att, 0.2);
    expect(att.wd).toBeUndefined();
    expect(woundedMonth(g, att, true, 0.5, 3)).toBe(0);
    const b = clone(g);
    const a2 = b.corps.find((c) => c.id === att.id)!;
    resolveBattle(b, [a2], b.regions[there.id]);
    expect(b.corps.some((c) => c.wd !== undefined)).toBe(false);
    expect(woundedLine(b, a2, true, 0)).toBe('');
  });

  it('a battle books its wounded on both sides, as a share of what the fight took, and changes no loss', () => {
    const g = war();
    const { here, there } = front(g);
    const att = corps(g, 'GER', here.id);
    const off = clone(g), on = clone(g);
    setHosp(on, true);
    const fight = (x: Game) => { const a = x.corps.find((c) => c.id === att.id)!; const before = new Map(x.corps.map((c) => [c.id, c.str])); resolveBattle(x, [a], x.regions[there.id]); return before; };
    const bOff = fight(off), bOn = fight(on);
    // The battle itself is the same battle: every corps' strength after it is the same with the switch on or off.
    expect(on.corps.map((c) => [c.id, c.str])).toEqual(off.corps.map((c) => [c.id, c.str]));
    const a = on.corps.find((c) => c.id === att.id)!;
    expect(a.wd).toBeGreaterThan(0);
    expect(a.wd).toBeCloseTo((bOn.get(a.id)! - a.str) * a.divs * HOSP.wounded, 6);
    const defs = on.corps.filter((c) => c.nat === 'POL' && c.wd);
    expect(defs.length).toBeGreaterThan(0);
    for (const d of defs) expect(d.wd).toBeLessThanOrEqual((bOn.get(d.id)! - d.str) * d.divs * HOSP.wounded + 1e-6);
    expect(bOff.size).toBe(bOn.size);
  });

  it('in supply a share comes back each month, more with a Medicine building near, never past full; cut off, some are lost', () => {
    const g = war();
    setHosp(g, true);
    const c = corps(g, 'GER', quiet(g).id);
    c.str = 0.5; c.wd = 0.4;
    expect(woundedMonth(g, c, true, 0.5, 0)).toBeCloseTo(0.4 * HOSP.back, 9);
    expect(c.str).toBeCloseTo(0.5 + (0.4 * HOSP.back) / c.divs, 9);
    expect(c.wd).toBeCloseTo(0.4 * (1 - HOSP.back), 6);
    // A Medicine building of level 2 nearby raises the share.
    c.wd = 0.4;
    expect(woundedMonth(g, c, true, 0.5, 2)).toBeCloseTo(0.4 * HOSP.back * (1 + 2 * HOSP.raise), 9);
    // Never past the corps' own full size: the rest wait.
    c.wd = 0.4;
    expect(woundedMonth(g, c, true, 0.01, 0)).toBeCloseTo(0.01 * c.divs, 9);
    expect(c.wd).toBeCloseTo(0.4 - 0.01 * c.divs, 6);
    // Cut off: none come back, and a share is lost for good.
    c.wd = 0.4;
    const s = c.str;
    expect(woundedMonth(g, c, false, 0.5, 3)).toBe(0);
    expect(c.str).toBe(s);
    expect(c.wd).toBeCloseTo(0.4 * (1 - HOSP.lost), 6);
    expect(woundedLine(g, c, false, 0)).toMatch(/^Wounded: [\d,]+ men waiting\. Cut off/);
    expect(woundedLine(g, c, true, 0)).toMatch(/^Wounded: [\d,]+ men in the hospitals; about [\d,]+ rejoin next month\.$/);
  });

  it("price once: under the switch the Medicine building no longer quickens a resting corps' drafts, and its rest stays", () => {
    const g = war();
    const r = quiet(g);
    const c = corps(g, 'GER', r.id);
    c.born = g.turn - 1; c.str = 0.5; c.org = 0.3; c.fought = false; c.ap = c.apMax; c.supplied = true;
    const plain = clone(g); // no building, switch off
    built(g, 'hospital', r, 3);
    const off = clone(g), on = clone(g);
    setHosp(on, true);
    const after = (x: Game) => { startPhase(x, 'GER'); const k = x.corps.find((y) => y.id === c.id)!; return { str: k.str, org: k.org }; };
    const P = after(plain), A = after(off), B = after(on);
    expect(A.str).toBeGreaterThan(P.str); // off: the building quickens the drafts
    expect(B.str).toBeCloseTo(P.str, 9); // on: it does not (no wounded to give back)
    expect(B.org).toBeCloseTo(A.org, 9); // its rest stays
    expect(B.org).toBeGreaterThan(P.org);
  });

  it('a computer war books and returns wounded by the same rule, keeps them through a save, and the switch off clears them', () => {
    const g = war(17, null);
    setHosp(g, true);
    for (let m = 0; m < 6; m++) runTurnAI(g);
    const hurt = g.corps.filter((c) => c.wd);
    expect(hurt.length).toBeGreaterThan(0);
    expect(new Set(hurt.map((c) => c.nat)).size).toBeGreaterThan(1);
    expect(hurt.every((c) => c.wd! > 0 && c.kind !== 'fort')).toBe(true);
    const back = loadGame(saveGame(g))!;
    expect(back.corps.filter((c) => c.wd).map((c) => [c.id, c.wd])).toEqual(hurt.map((c) => [c.id, c.wd]));
    setHosp(g, false);
    expect(g.corps.some((c) => c.wd !== undefined)).toBe(false);
  });

  it('the 3D ship list turns it on once, and a switch the player turns off stays off', () => {
    TC().start({ view: 'grand', human: 'GER', seed: 11 });
    const read = (): Game => loadGame((TC().save() as { text: string }).text)!;
    expect(hospOn(read())).toBe(false);
    expect(TC().r2Ship(['hospitalsrule'])).not.toBeNull();
    expect(hospOn(read())).toBe(true);
    TC().act('hospitalsrule:off');
    expect(TC().r2Ship(['hospitalsrule'])).toBeNull();
    expect(hospOn(read())).toBe(false);
  });
});
