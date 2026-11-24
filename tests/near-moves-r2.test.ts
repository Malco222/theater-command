// The enemy's month near you (release 2; Brian, 24 November 2026: "when the AI nations are doing their turn, show
// movements around my territories or if my units are attacked so i know whats going on"). Each computer nation's step
// of the month now says what it did near the player, for the 3D table's watch: arrows for the foreign armies that came
// up to his ground or into it, for the attacks on his troops (from where, and how each ended) and for his own corps
// that fell back; 'near' marks the steps the watch stops at; a sentence or two in his side's words. It reads the war
// and never changes it, and it tells only what his reports can see.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { saveGame } from '../src/campaign/save';
import type { BattleReport, Game } from '../src/campaign/types';
import { NEAR_ARROWS, nearMoves, type NearArrow } from '../unreal/TheaterCommand/Rules/near-moves';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Step = { nat: string; note: string; mine: boolean; near?: boolean; arrows?: NearArrow[]; nearNote?: string; moved?: boolean; focus: number[] };
const TC = (globalThis as unknown as { TC: { start: (o: object) => unknown; endMonth: () => { steps: Step[] } } }).TC;

const open = (nat: string, seed = 1939): Game => startOpen(createGame(seed, nat));
const stoodNow = (g: Game): Map<number, number> => new Map(g.corps.filter((c) => !c.dead).map((c) => [c.id, c.reg] as const));
const ctrlOf = (g: Game): string[] => g.regions.map((r) => r.ctrl);
const seeAll = (g: Game, v = 2): Uint8Array => new Uint8Array(g.regions.length).fill(v);
/** A German corps on a province next to French ground, and a neighbour of that province it can be said to have come from. */
function germanAtTheBorder(g: Game): { id: number; to: number; from: number } {
  const french = new Set(g.regions.filter((r) => r.ctrl === 'FRA').map((r) => r.id));
  for (const c of g.corps) {
    if (c.dead || c.nat !== 'GER' || french.has(c.reg)) continue;
    if (!g.regions[c.reg].nb.some((j) => french.has(j))) continue;
    const from = g.regions[c.reg].nb.find((j) => !french.has(j) && !g.regions[j].nb.some((k) => french.has(k)) && g.regions[j].ctrl === 'GER');
    if (from !== undefined) return { id: c.id, to: c.reg, from };
  }
  throw new Error('no German corps at the French border');
}

describe('what one enemy month did near the player', () => {
  it('draws a foreign army that came up to his border, red for a country at war with him', () => {
    const g = open('FRA');
    const c = germanAtTheBorder(g);
    const stood = stoodNow(g);
    stood.set(c.id, c.from);
    const out = nearMoves(g, 'GER', stood, ctrlOf(g), [], seeAll(g));
    expect(out.near).toBe(true);
    const a = out.arrows.find((x) => x.to === c.to && x.from === c.from)!;
    expect(a).toMatchObject({ kind: 'move', nat: 'GER', war: true });
    expect(a.away).toBeUndefined();
    expect(a.corps).toBeGreaterThanOrEqual(1);
    expect(a.divs).toBeGreaterThanOrEqual(1);
    expect(out.nearNote).toMatch(new RegExp(`moves? up to ${g.regions[c.to].name}, next to your land`));
  });

  it('tells nothing his reports cannot see, and no division count where the place is only glimpsed', () => {
    const g = open('FRA');
    const c = germanAtTheBorder(g);
    const stood = stoodNow(g);
    stood.set(c.id, c.from);
    expect(nearMoves(g, 'GER', stood, ctrlOf(g), [], seeAll(g, 0))).toEqual({ near: false, arrows: [], nearNote: '' });
    const glimpsed = nearMoves(g, 'GER', stood, ctrlOf(g), [], seeAll(g, 1));
    expect(glimpsed.arrows.find((x) => x.to === c.to)!.divs).toBeNull();
  });

  it('marks an army going away from him, which is drawn but never stops the watch on its own', () => {
    const g = open('FRA');
    const c = germanAtTheBorder(g);
    const stood = stoodNow(g);
    // the same corps, the other way: from the border back to where it came from
    stood.set(c.id, c.to);
    g.corps.find((x) => x.id === c.id)!.reg = c.from;
    const out = nearMoves(g, 'GER', stood, ctrlOf(g), [], seeAll(g));
    expect(out.arrows).toEqual([expect.objectContaining({ kind: 'move', from: c.to, to: c.from, away: true })]);
    expect(out.near).toBe(false);
    expect(out.nearNote).toMatch(new RegExp(`pulls? back from ${g.regions[c.to].name}`));
  });

  it('draws a long road one province long, at the end that touches him, and counts only the corps the sentence speaks of', () => {
    const g = open('FRA');
    const c = germanAtTheBorder(g);
    const near = new Set([c.to, ...g.regions[c.to].nb]);
    const far = g.regions.find((r) => r.ctrl === 'GER' && !near.has(r.id) && Math.hypot(r.x - g.regions[c.to].x, r.y - g.regions[c.to].y) > 150)!;
    const stood = stoodNow(g);
    stood.set(c.id, far.id); // (came by rail from far behind)
    // and another German corps on the border going back the other way, far
    const other = g.corps.find((x) => !x.dead && x.nat === 'GER' && x.id !== c.id && x.reg !== c.to && g.regions[x.reg].nb.some((j) => g.regions[j].ctrl === 'FRA'))!;
    stood.set(other.id, other.reg);
    other.reg = far.id;
    const out = nearMoves(g, 'GER', stood, ctrlOf(g), [], seeAll(g));
    const up = out.arrows.find((a) => a.to === c.to)!;
    expect(g.regions[c.to].nb).toContain(up.from);
    expect(Math.hypot(g.regions[up.from].x - far.x, g.regions[up.from].y - far.y)).toBeLessThan(Math.hypot(g.regions[c.to].x - far.x, g.regions[c.to].y - far.y));
    const back = out.arrows.find((a) => a.away)!;
    expect(back.from).toBe(stood.get(other.id));
    expect(g.regions[back.from].nb).toContain(back.to);
    const corpsUp = out.arrows.filter((a) => !a.away && a.kind === 'move').reduce((t, a) => t + a.corps, 0);
    expect(out.nearNote).toContain(corpsUp === 1 ? `A German corps moves up to ${g.regions[c.to].name}` : `German armies (${corpsUp} corps) move up to`);
  });

  it('says nothing of his own month, nor of his allies\'', () => {
    const g = open('FRA');
    const c = germanAtTheBorder(g);
    const stood = stoodNow(g);
    stood.set(c.id, c.from);
    expect(nearMoves(g, 'FRA', stood, ctrlOf(g), [], seeAll(g)).near).toBe(false);
    expect(nearMoves(g, 'UK', stood, ctrlOf(g), [], seeAll(g)).near).toBe(false);
  });

  it('draws an attack on his troops from where the attackers stood, one arrow for the field and the town together, and his corps that fell back', () => {
    const g = open('FRA');
    const c = germanAtTheBorder(g);
    const corps = g.corps.find((x) => x.id === c.id)!;
    const target = g.regions[c.to].nb.find((j) => g.regions[j].ctrl === 'FRA')!;
    const mine = g.corps.find((x) => !x.dead && x.nat === 'FRA')!;
    const back = g.regions[target].nb.find((j) => g.regions[j].ctrl === 'FRA' && j !== target)!;
    const stood = stoodNow(g);
    stood.set(mine.id, target);
    mine.reg = back; // (pushed back from the place attacked)
    const ctrl0 = ctrlOf(g);
    const report = (over: Partial<BattleReport>): BattleReport => ({
      id: 1, turn: g.turn, week: 1, region: target, regionName: g.regions[target].name, attNat: 'GER', defNat: 'FRA',
      attackers: [{ name: corps.name, nat: 'GER', kind: corps.kind, divs: corps.divs, men: 1, lost: 0, fate: '' }],
      defenders: [{ name: mine.name, nat: 'FRA', kind: mine.kind, divs: mine.divs, men: 1, lost: 0, fate: 'retreated' }],
      lines: [], aPower: 1, dPower: 1, odds: 0.5, level: 'marginal' as BattleReport['level'], taken: false, attLost: 0, defLost: 0, prisoners: 0, ...over,
    }) as BattleReport;
    const reps = [report({ siege: true, taken: true, attLost: 1500, defLost: 3000 }), report({ id: 2, town: true, taken: true, attLost: 500, defLost: 1000 })];
    const out = nearMoves(g, 'GER', stood, ctrl0, reps, seeAll(g));
    const attacks = out.arrows.filter((a) => a.kind === 'attack');
    expect(attacks).toEqual([expect.objectContaining({ from: c.to, to: target, nat: 'GER', taken: true, lost: [2000, 4000] })]);
    expect(attacks[0].siege).toBeUndefined();
    expect(out.arrows).toContainEqual(expect.objectContaining({ kind: 'fell', from: target, to: back, nat: 'FRA' }));
    expect(out.nearNote).toBe(`The German army attacks ${g.regions[target].name} and takes it. They lost 2,000 men, you 4,000.`);
    // Thrown back, it says so; under siege, it says so.
    expect(nearMoves(g, 'GER', stood, ctrl0, [report({ attLost: 900, defLost: 400 })], seeAll(g)).nearNote).toBe(`A German attack on ${g.regions[target].name} is thrown back. They lost 900 men, you 400.`);
    expect(nearMoves(g, 'GER', stood, ctrl0, [report({ siege: true, taken: true, attLost: 900, defLost: 400 })], seeAll(g)).nearNote).toContain('the town holds out under siege');
  });

  it('tells his allies\' battles near him apart, in their own sentence (Brian 18:01Z, "yes to all")', () => {
    const g = open('FRA');
    const c = germanAtTheBorder(g);
    const corps = g.corps.find((x) => x.id === c.id)!;
    const stood = stoodNow(g);
    // British troops attacked by the Germans in a province next to French ground (c.to: the German corps' own, for the test)
    const report = (over: Partial<BattleReport>): BattleReport => ({
      id: 1, turn: g.turn, week: 1, region: c.to, regionName: g.regions[c.to].name, attNat: 'GER', defNat: 'UK',
      attackers: [{ name: corps.name, nat: 'GER', kind: corps.kind, divs: corps.divs, men: 1, lost: 0, fate: '' }],
      defenders: [{ name: 'I Corps', nat: 'UK', kind: 'inf', divs: 2, men: 1, lost: 0, fate: '' }],
      lines: [], aPower: 1, dPower: 1, odds: 0.5, level: 'marginal' as BattleReport['level'], taken: false, attLost: 900, defLost: 400, prisoners: 0, ...over,
    }) as BattleReport;
    stood.set(c.id, c.from);
    const held = nearMoves(g, 'GER', stood, ctrlOf(g), [report({})], seeAll(g));
    const ally = held.arrows.find((a) => a.ally)!;
    expect(ally).toMatchObject({ kind: 'attack', to: c.to, nat: 'GER', def: 'UK', taken: false });
    expect(ally.ours).toBeUndefined();
    expect(held.nearAllies).toBe(true);
    expect(held.allyNote).toBe(`British troops at ${g.regions[c.to].name} throw back a German attack.`);
    expect(held.nearNote).not.toContain('British');
    // the British attacking, in their own month: told, but the watch stops for it only as an allies' battle
    const theirs = nearMoves(g, 'UK', stood, ctrlOf(g), [report({ attNat: 'UK', defNat: 'GER', attackers: [{ name: 'I Corps', nat: 'UK', kind: 'inf', divs: 2, men: 1, lost: 0, fate: '' }], defenders: [{ name: corps.name, nat: 'GER', kind: corps.kind, divs: corps.divs, men: 1, lost: 0, fate: '' }], taken: true })], seeAll(g));
    expect(theirs.near).toBe(false);
    expect(theirs.nearAllies).toBe(true);
    expect(theirs.arrows).toEqual([expect.objectContaining({ ally: true, ours: true, nat: 'UK', taken: true })]);
    expect(theirs.allyNote).toBe(`British troops attack ${g.regions[c.to].name} and take it.`);
    // nothing his reports cannot see
    expect(nearMoves(g, 'UK', stood, ctrlOf(g), [report({ attNat: 'UK', defNat: 'GER' })], seeAll(g, 0)).nearAllies).toBeUndefined();
  });

  it('keeps no more than eight arrows, and reads the war without changing it', () => {
    const g = open('FRA');
    const before = saveGame(g);
    const stood = new Map<number, number>();
    // every German corps "came" from somewhere else
    for (const c of g.corps) if (!c.dead) stood.set(c.id, c.nat === 'GER' ? g.regions[c.reg].nb[0] : c.reg);
    const out = nearMoves(g, 'GER', stood, ctrlOf(g), [], seeAll(g));
    expect(out.arrows.length).toBeLessThanOrEqual(NEAR_ARROWS);
    expect(saveGame(g)).toBe(before);
  });
});

describe('the month\'s steps the 3D table watches', () => {
  it('in the spring of 1940 played as France, marks the German months that came near, with arrows and a sentence', () => {
    TC.start({ view: 'grand', human: 'FRA', seed: 1939, from: 5 });
    const near: Step[] = [];
    for (let m = 0; m < 7; m++) near.push(...TC.endMonth().steps.filter((s) => s.near));
    expect(near.length).toBeGreaterThan(2);
    for (const s of near) {
      expect(s.nat).not.toBe('FRA');
      for (const a of s.arrows ?? []) {
        expect(['move', 'attack', 'fell']).toContain(a.kind);
        expect(a.to).toBeGreaterThanOrEqual(0);
        expect(a.from).toBeGreaterThanOrEqual(-1);
        expect(a.corps).toBeGreaterThan(0);
      }
    }
    // Somewhere in that spring a French position is attacked and lost, and the sentence says what it cost.
    const lost = near.flatMap((s) => s.arrows ?? []).filter((a) => a.kind === 'attack' && a.taken);
    expect(lost.length).toBeGreaterThan(0);
    expect(near.some((s) => /attacks .+ and takes it\. They lost [\d,]+ men, you [\d,]+\./.test(s.nearNote ?? ''))).toBe(true);
    // Steps with arrows and nothing else to show still look at where the arrows end.
    for (const s of near) if (s.moved) expect(s.focus.length).toBeGreaterThan(0);
  }, 120000);
});
