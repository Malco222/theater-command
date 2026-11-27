// The contact rule, round 4, the mender's boards: the rule sceptic's S4-1 and S4-2 and the telling sceptic's T4-1 and
// T4-2 (reports /home/user/runs/contact2/r4/reports/r3-rule-sceptic4.json and r4-telling-sceptic4.json). The far side
// on, seed 3, the first day.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { answering, frontAnswering } from '../src/campaign/ai';
import { arrivalWaitOf, setFromWait } from '../src/campaign/arrivalwaits';
import { divisionsOf } from '../src/campaign/armies';
import { signPeace } from '../src/campaign/diplomacy';
import { contactRead, corpsInContact, recordBattle } from '../src/campaign/drawn';
import { addCorps, atWar, createGame, declareWar, leavingWatch, relocate, rowLeaving, ruleHolds, startPhase } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { farOn } from '../src/campaign/world';
import type { Corps, Game, Region } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
let added = 0;
const addRow = (row: any[]): number => { ROWS.push(row); added++; return ROWS.length - 1; };
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } leavingWatch.tell = null; });
const month = (g: Game, t: number, nat = 'GER'): void => { g.turn = t; startPhase(g, nat); };

// ---- S4-1: a held corps whose general takes its hold off this month (ai.ts `answering`, `frontAnswering`) is free --

describe("S4-1: an enemy corps under orders to hold that answers this month (the general's own answer rule) is not bound", () => {
  // (Hainan, Japan's on the first day, has one neighbour, Pakhoi, China's, where the Southern Kwangtung garrison stands
  // held; on the far side the war itself frees the holds on the front facing Japan, so it answers: ai.ts
  // `frontAnswering`. A lone Japanese test corps on Hainan, Japan's own garrison moved off.)
  function board(): { g: Game; c: Corps; pk: Region; gar: Corps } {
    const g = startOpen(createGame(3, null));
    expect(farOn(g)).toBe(true);
    expect(setFromWait(g, true)).toBe(true);
    const hn = g.byName.Hainan, pk = g.byName.Pakhoi;
    expect(hn.nb.map((j) => g.regions[j].name)).toEqual(['Pakhoi']);
    expect(atWar(g, 'JAP', 'CHN')).toBe(true);
    for (const u of [...hn.units]) relocate(g, u, g.byName.Canton);
    const gar = pk.units.find((u) => u.name === 'Southern Kwangtung garrison')!;
    expect([pk.ctrl, pk.units.filter((u) => !u.dead).length, gar.hold, !!gar.stay]).toEqual(['CHN', 1, true, false]);
    const c = addCorps(g, 'JAP', 'Lone Test Corps', 'inf', 1, hn.id, {});
    return { g, c, pk, gar };
  }

  it('the held garrison answers this month: it is free to come on, and the lone corps beside it is held', () => {
    const { g, c, gar } = board();
    expect(frontAnswering(g, 'CHN').includes(gar) || answering(g, 'CHN').includes(gar)).toBe(true);
    expect(contactRead(g, 'JAP', c)).toEqual({ beside: true, battle: false, kept: false });
    expect(corpsInContact(g, 'JAP', c)).toBe(true);
  });

  it('the mirror: the same garrison marked to stay answers nothing: bound, and the lone corps is not held (R4-1 as built)', () => {
    const { g, c, gar } = board();
    gar.stay = true;
    expect(frontAnswering(g, 'CHN').includes(gar) || answering(g, 'CHN').includes(gar)).toBe(false);
    expect(contactRead(g, 'JAP', c)).toEqual({ beside: true, battle: false, kept: true });
    expect(corpsInContact(g, 'JAP', c)).toBe(false);
  });
});

// ---- The Grenzmark boards (Germany's lent corps beside Poland), as tests/contact-r4-marks.test.ts lays them -----

function grenz(o: { human?: boolean } = {}): { g: Game; gz: Region } {
  const g = startOpen(createGame(3, null));
  expect(farOn(g)).toBe(true);
  if (o.human) { g.human = 'GER'; g.flags.offers = 1; }
  expect(setFromWait(g, true)).toBe(true);
  const gz = g.byName.Grenzmark;
  for (const u of [...gz.units]) relocate(g, u, g.byName.Bavaria);
  expect(atWar(g, 'GER', 'POL')).toBe(true);
  return { g, gz };
}
const lend = (g: Game, name: string, gz: Region, words: any = { until: 2 }): { c: Corps; j: number } => {
  const c = addCorps(g, 'GER', name, 'inf', 1, gz.id, {});
  const j = addRow([1, 'GER', name, 'inf', 1, 'Grenzmark', words]);
  g.flags[`sent:${j}`] = 1;
  g.flags[`lent:${j}`] = c.id;
  return { c, j };
};

// ---- S4-2: a lent regiment whose loan's end is the player's offer stays, so it is no leaver ------------------------

describe("S4-2: the player's lent regiment whose loan's end is his offer stays on the map, and is no leaver for a lent corps beside it", () => {
  it("the Lent Test Corps' loan ends while the regiment beside it stays; the regiment, then alone, is held, told", () => {
    const { g, gz } = grenz({ human: true });
    const a = lend(g, 'Lent Test Corps', gz);
    const b = lend(g, 'Lent Regiment Test', gz);
    addRow([9, 'GER', 'Test Tank Division', 'arm', 1, 'Magdeburg', { awaits: ['Lent Regiment Test'], str: 1 }]);
    expect(a.j).toBeLessThan(b.j);
    month(g, 2);
    // (The regiment stays whatever is asked of it: its end is his to answer. So the lent corps read first is not alone,
    // and its loan ends; the regiment, read after it, is then alone beside the enemy, and the one test holds its offer
    // back, told: 'loanHome'.)
    expect([a.c.dead, g.flags[`lent:${a.j}`], g.flags[`wait:${a.j}`], g.flags[`rowTold:${a.j}`]]).toEqual([true, -1, undefined, undefined]);
    expect([b.c.dead, g.flags[`lent:${b.j}`], g.flags[`wait:${b.j}`], g.flags[`rowTold:${b.j}`]]).toEqual([false, b.c.id, 2, 2]);
    expect(ruleHolds(g).filter((h) => h.path.startsWith('loan')).map((h) => [h.i, h.path, h.corps])).toEqual([[b.j, 'loanHome', ['Lent Regiment Test']]]);
    // (And once he keeps it, it stays for good beside nobody else's loan: no hold of the rule's stands.)
    g.flags[`kept:${b.j}`] = 1;
    month(g, 3);
    expect([b.c.dead, g.flags[`lent:${b.j}`]]).toEqual([false, b.c.id]);
    expect(ruleHolds(g).filter((h) => h.path.startsWith('loan'))).toEqual([]);
  });

  it('outside the pass too, the telling counts the regiment no leaver', () => {
    const { g, gz } = grenz({ human: true });
    const b = lend(g, 'Lent Regiment Test', gz);
    addRow([9, 'GER', 'Test Tank Division', 'arm', 1, 'Magdeburg', { awaits: ['Lent Regiment Test'], str: 1 }]);
    const late = addRow([3, 'GER', 'Late Test Row', 'inf', 1, 'Magdeburg', {}]);
    month(g, 2);
    g.flags[`kept:${b.j}`] = 1;
    expect([...rowLeaving(g, 'GER', late)].map((x) => x.name)).not.toContain('Lent Regiment Test');
  });
});

// ---- T4-1: a war row's loan held, let go by its war beginning again, then held again ------------------------------

describe("T4-1: a war row's loan held a second time is told again, dated by its second hold", () => {
  it('held in month 2, its war begun in month 3 (no telling flags left), its war over in month 4: a new headline and card', () => {
    const { g, gz } = grenz({ human: true });
    const { c, j } = lend(g, 'Lent Test Corps', gz, { war: ['GER', 'USA'] });
    const p = addCorps(g, 'GER', 'Parent Test Corps', 'inf', 1, gz.id, {});
    c.drew = divisionsOf(g, c).map((d) => ({ name: [d.name, d.short] as [string, string], no: d.no, from: p.id }));
    recordBattle(g, gz, [c, ...g.byName.Poznan.units.filter((u) => u.nat === 'POL')]);
    const heads = (t: number): string[] => g.headlines.filter((h: any) => h.turn === t && /Lent Test Corps/.test(h.text)).map((h: any) => h.text);
    month(g, 2);
    expect([g.flags[`wait:${j}`], g.flags[`rowTold:${j}`]]).toEqual([2, 2]);
    expect(heads(2)).toHaveLength(1);
    declareWar(g, 'GER', 'USA');
    month(g, 3);
    expect([c.dead, g.flags[`lent:${j}`], g.flags[`rowTold:${j}`], g.flags[`rowAt:${j}`]]).toEqual([false, c.id, undefined, undefined]);
    expect(arrivalWaitOf(g, c)).toBeNull();
    expect(signPeace(g, 'GER', 'USA')).toBe(true);
    month(g, 4);
    expect([g.flags[`wait:${j}`], g.flags[`rowTold:${j}`]]).toEqual([4, 4]);
    expect(heads(4)).toEqual(['The Lent Test Corps, lent to the campaign, is due this month to go back into the Parent Test Corps, but it is at the front in the Grenzmark. It stays, and goes back the month after it leaves the front.']);
    expect(arrivalWaitOf(g, c)?.whole).toBe('This corps was due to go back into the Parent Test Corps in December 1939. It goes back the month after it leaves the front.');
    expect(ruleHolds(g).map((h) => [h.path, h.told, h.untold])).toEqual([['loanBack', true, false]]);
  });
});

// ---- T4-2: outside the pass, a loan held this month is no leaver for the rows after it, as in the pass -------------

describe("T4-2: the telling's leaving set read between passes is the rule's own in-pass set", () => {
  it('two lent corps alone together, loans over in month 2; a late row read after both: the same set before and in the pass', () => {
    const { g, gz } = grenz();
    const a = lend(g, 'Lent Test Corps A', gz);
    const b = lend(g, 'Lent Test Corps B', gz);
    const late = addRow([3, 'GER', 'Late Test Row', 'inf', 1, 'Magdeburg', {}]);
    const names = (s: ReadonlySet<Corps>): string[] => [...s].map((x) => x.name).filter((n) => n.startsWith('Lent Test')).sort();
    const rule: Record<number, string[]> = {};
    leavingWatch.tell = (g2, nat, i, leaving) => { if (nat === 'GER' && i === late) rule[g2.turn] = names(leaving); };
    month(g, 2);
    expect([a.c.dead, g.flags[`wait:${a.j}`], b.c.dead]).toEqual([false, 2, true]);
    // (After month 2's pass, as the player reads his cards: A is held this month, B is gone.)
    const between = names(rowLeaving(g, 'GER', late));
    month(g, 3);
    expect([a.c.dead, g.flags[`wait:${a.j}`]]).toEqual([false, 3]);
    expect(rule[3]).toEqual([]);
    expect(between).toEqual(rule[3]);
    expect(names(rowLeaving(g, 'GER', late))).toEqual(rule[3]);
  });
});
