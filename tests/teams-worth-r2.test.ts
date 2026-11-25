// The support teams' value test (release 2; the design director's ruling of 25 November 2026, 19:48Z): a staff orders a
// brigade only when, at its own sight, it buys more on the card for its price than the corps or refit the same money
// would, by one value test for every order (src/campaign/orderworth.ts). The quota (one per five fighting corps) is gone
// and the price is untouched. These tests hold the test's parts to the card and the staff's order to the test.
import { describe, expect, it } from 'vitest';
import { aiTeams, buildPick, corpsCost, lineFactories, PRODUCTION } from '../src/campaign/economy';
import { addCorps, createGame, setWar } from '../src/campaign/game';
import { sightOf } from '../src/campaign/intel';
import { freshWorth, orderWorth } from '../src/campaign/orderworth';
import { startOpen } from '../src/campaign/staff';
import { setTeams, TEAMS } from '../src/campaign/teams';
import type { Corps, Game, Region } from '../src/campaign/types';

const grand = (on = true): Game => { const g = startOpen(createGame(3, null)); if (on) setTeams(g, true); return g; };
/** A German province on the Polish border and the Polish one beside it, both emptied, the two at war. */
const sector = (g: Game): { from: Region; target: Region } => {
  const target = g.regions.find((r) => r.ctrl === 'POL' && r.nb.some((j) => g.regions[j].ctrl === 'GER' && g.regions[j].owner === 'GER'))!;
  const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER' && g.regions[j].owner === 'GER')!];
  // Nobody else next door: the only enemy the staff sees there is the one each test sets.
  for (const r of [from, target, ...from.nb.map((j) => g.regions[j])]) {
    const gone = r === from || r === target ? r.units : r.units.filter((c) => c.nat !== 'GER');
    for (const c of gone) c.dead = true;
    r.units = r.units.filter((c) => !c.dead);
  }
  setWar(g, 'GER', 'POL');
  return { from, target };
};
const line = (g: Game, nat: string, r: Region, n: number, kind: Corps['kind'] = 'inf'): Corps[] =>
  Array.from({ length: n }, (_, i) => addCorps(g, nat, `Test ${nat} ${kind} ${i}`, kind, kind === 'inf' ? 3 : 2, r.id, {}));
const dig = (cs: Corps[], months = 3): void => { for (const c of cs) { c.dug = true; c.dugFor = months; } };
/** Every German corps but these off the map, so the only front the staff has is the one set. */
const only = (g: Game, keep: Corps[]): void => {
  const ids = new Set(keep.map((c) => c.id));
  for (const c of g.corps) if (c.nat === 'GER' && !ids.has(c.id) && !c.dead) { c.dead = true; const r = g.regions[c.reg]; r.units = r.units.filter((u) => u !== c); }
};

describe('one value test for every order', () => {
  it('a corps with no lift is its own weight on the card per point of its price', () => {
    const g = grand();
    const sight = sightOf(g, 'GER');
    expect(orderWorth(g, 'GER', 'inf', 'def', sight)).toBeCloseTo(freshWorth(g, 'GER', 'inf', 3, 'def') / corpsCost(g, 'GER', 'inf'), 9);
    expect(orderWorth(g, 'GER', 'inf', 'atk', sight)).toBeCloseTo(freshWorth(g, 'GER', 'inf', 3, 'atk') / corpsCost(g, 'GER', 'inf'), 9);
  });

  it('machine guns are worth most behind a line dug in against men on foot, and nothing more against no one', () => {
    const g = grand();
    const { from, target } = sector(g);
    const own = line(g, 'GER', from, 4);
    only(g, own);
    const bare = freshWorth(g, 'GER', 'mg', TEAMS.mg.divs, 'def') / corpsCost(g, 'GER', 'mg');
    expect(orderWorth(g, 'GER', 'mg', 'def', sightOf(g, 'GER'))).toBeCloseTo(bare, 9);
    line(g, 'POL', target, 2);
    const open = orderWorth(g, 'GER', 'mg', 'def', sightOf(g, 'GER'));
    dig(own);
    const dug = orderWorth(g, 'GER', 'mg', 'def', sightOf(g, 'GER'));
    expect(open).toBeGreaterThan(bare);
    expect(dug).toBeGreaterThan(open);
    // Against tanks the lift is nothing: the men on foot are who it tells against.
    for (const c of target.units) c.dead = true;
    target.units = [];
    line(g, 'POL', target, 2, 'arm');
    expect(orderWorth(g, 'GER', 'mg', 'def', sightOf(g, 'GER'))).toBeCloseTo(bare, 9);
  });

  it('teams on order count as standing in the sector already: the lift is not bought twice', () => {
    const g = grand();
    const { from, target } = sector(g);
    const own = line(g, 'GER', from, 4);
    only(g, own);
    dig(own);
    line(g, 'POL', target, 2);
    const first = orderWorth(g, 'GER', 'mg', 'def', sightOf(g, 'GER'));
    g.nations.GER.pp = 500;
    const full = Math.ceil(TEAMS.card.mg.most / TEAMS.card.mg.perDiv);
    for (let i = 0; i < full; i++) g.nations.GER.queue.push({ id: 900 + i, what: 'corps', kind: 'mg', divs: 1, name: `Q${i}`, cost: 0, eta: g.turn + 3, region: from.id } as never);
    const after = orderWorth(g, 'GER', 'mg', 'def', sightOf(g, 'GER'));
    expect(after).toBeLessThan(first);
    expect(after).toBeCloseTo(freshWorth(g, 'GER', 'mg', TEAMS.mg.divs, 'def') / corpsCost(g, 'GER', 'mg'), 9);
  });

  it('mortars are worth more only against works: their own weight against a line not dug in', () => {
    const g = grand();
    const { from, target } = sector(g);
    const own = line(g, 'GER', from, 3);
    only(g, own);
    const foes = line(g, 'POL', target, 2);
    const bare = freshWorth(g, 'GER', 'mor', TEAMS.mor.divs, 'atk') / corpsCost(g, 'GER', 'mor');
    expect(orderWorth(g, 'GER', 'mor', 'atk', sightOf(g, 'GER'))).toBeCloseTo(bare, 9);
    dig(foes);
    expect(orderWorth(g, 'GER', 'mor', 'atk', sightOf(g, 'GER'))).toBeGreaterThan(bare);
  });

  it('a refit is what the money would buy only where the purse, the order paid, would not cover the month\'s drafts', () => {
    const g = grand();
    const { from } = sector(g);
    const own = line(g, 'GER', from, 4);
    only(g, own);
    for (const c of own) c.str = 0.6;
    const sight = sightOf(g, 'GER');
    g.nations.GER.pp = 1000;
    expect(orderWorth(g, 'GER', 'refit', 'def', sight, TEAMS.mg.cost)).toBe(0);
    g.nations.GER.pp = TEAMS.mg.cost + 1;
    const refit = orderWorth(g, 'GER', 'refit', 'def', sight, TEAMS.mg.cost);
    expect(refit).toBeGreaterThan(0);
    // A division's worth of drafts is far cheaper than a division raised new: it buys more on the card a point.
    expect(refit).toBeGreaterThan(orderWorth(g, 'GER', 'inf', 'def', sight));
    expect(PRODUCTION.replacementCost).toBeLessThan(corpsCost(g, 'GER', 'inf') / 3);
  });
});

describe('the staff orders by the test, not by a quota', () => {
  const ready = (g: Game): void => { const n = g.nations.GER; n.pp = 400; n.queue = n.queue.filter((b) => b.what !== 'corps'); };
  const ordered = (g: Game): string[] => g.nations.GER.queue.filter((b) => b.what === 'corps').map((b) => b.kind!);

  it('a big army with no front where a brigade beats a corps orders none', () => {
    const g = grand();
    const { from, target } = sector(g);
    const own = line(g, 'GER', from, 1);
    // Thirty corps far from any enemy: the old quota would have ordered six brigades for them.
    const home = g.regions[g.nations.GER.capital];
    const rear = Array.from({ length: 30 }, (_, i) => addCorps(g, 'GER', `Rear ${i}`, 'inf', 3, home.id, {}));
    only(g, [...own, ...rear]);
    line(g, 'POL', target, 1, 'arm');
    ready(g);
    aiTeams(g, 'GER');
    expect(ordered(g)).toEqual([]);
  });

  it('a line dug in on strong ground against men on foot gets its machine guns, where they buy more than the corps', () => {
    const g = grand();
    const { from, target } = sector(g);
    from.terrain = 'mountain';
    const own = line(g, 'GER', from, 4);
    only(g, own);
    dig(own);
    line(g, 'POL', target, 3);
    ready(g);
    const sight = sightOf(g, 'GER');
    const pick = buildPick(g, 'GER', lineFactories(g, 'GER'), g.nations.GER.capital).kind;
    expect(orderWorth(g, 'GER', 'mg', 'def', sight)).toBeGreaterThan(orderWorth(g, 'GER', pick, 'def', sight));
    aiTeams(g, 'GER');
    expect(ordered(g)).toEqual(['mg']);
  });

  it('switched off, nothing is ordered, whatever the test would say', () => {
    const g = grand(false);
    const { from, target } = sector(g);
    from.terrain = 'mountain';
    const own = line(g, 'GER', from, 4);
    only(g, own);
    dig(own);
    line(g, 'POL', target, 3);
    ready(g);
    aiTeams(g, 'GER');
    expect(ordered(g)).toEqual([]);
  });
});
