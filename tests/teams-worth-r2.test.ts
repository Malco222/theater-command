// The support teams' value test (release 2; the design director's rulings of 25 November 2026, 19:48Z and 20:08Z): a
// staff orders a brigade only when, at its own sight, it buys more on the card for its price than the corps or refit the
// same money would, by one reckoning for every order: the battle card read with the order and without it where it would
// stand (src/campaign/orderworth.ts). The quota (one per five fighting corps) is gone and the price is untouched.
import { describe, expect, it } from 'vitest';
import { aiTeams, buildPick, corpsCost, lineFactories, PRODUCTION } from '../src/campaign/economy';
import { addCorps, createGame, setWar } from '../src/campaign/game';
import { Reckoning } from '../src/campaign/orderworth';
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

const worth = (g: Game, order: Parameters<Reckoning['worth']>[0], role: 'atk' | 'def', spend = 0): number => new Reckoning(g, 'GER').worth(order, role, spend);

describe('one reckoning for every order: the card with the order and without it', () => {
  it('no front, no worth: an order is worth what it adds to a fight the staff can see', () => {
    const g = grand();
    const { from } = sector(g);
    only(g, line(g, 'GER', from, 2));
    expect(worth(g, 'inf', 'def')).toBe(0);
    expect(worth(g, 'mg', 'def')).toBe(0);
  });

  it('a corps counts the ground and the works it would stand in, as a brigade does', () => {
    const g = grand();
    const { from, target } = sector(g);
    const own = line(g, 'GER', from, 3);
    only(g, own);
    line(g, 'POL', target, 2);
    from.terrain = 'plains';
    const open = worth(g, 'inf', 'def');
    dig(own);
    const dug = worth(g, 'inf', 'def');
    from.terrain = 'mountain';
    const hill = worth(g, 'inf', 'def');
    expect(open).toBeGreaterThan(0);
    expect(dug).toBeGreaterThan(open);
    expect(hill).toBeGreaterThan(dug);
  });

  it('machine guns are worth more behind a line dug in against men on foot than against tanks', () => {
    const g = grand();
    const { from, target } = sector(g);
    const own = line(g, 'GER', from, 4);
    only(g, own);
    dig(own);
    line(g, 'POL', target, 2);
    const foot = worth(g, 'mg', 'def');
    for (const c of target.units) c.dead = true;
    target.units = [];
    line(g, 'POL', target, 2, 'arm');
    const tanks = worth(g, 'mg', 'def');
    expect(foot).toBeGreaterThan(tanks);
    expect(tanks).toBeGreaterThan(0);
  });

  it('teams on order count as standing in the sector already: the lift is not bought twice', () => {
    const g = grand();
    const { from, target } = sector(g);
    const own = line(g, 'GER', from, 4);
    only(g, own);
    dig(own);
    line(g, 'POL', target, 2);
    const first = worth(g, 'mg', 'def');
    const full = Math.ceil(TEAMS.card.mg.most / TEAMS.card.mg.perDiv);
    for (let i = 0; i < full; i++) g.nations.GER.queue.push({ id: 900 + i, what: 'corps', kind: 'mg', divs: 1, name: `Q${i}`, cost: 0, eta: g.turn + 3, region: from.id } as never);
    expect(worth(g, 'mg', 'def')).toBeLessThan(first);
  });

  it('mortars are worth more against a line dug in than against one in the open', () => {
    const g = grand();
    const { from, target } = sector(g);
    only(g, line(g, 'GER', from, 3));
    const foes = line(g, 'POL', target, 2);
    const open = worth(g, 'mor', 'atk');
    dig(foes);
    expect(worth(g, 'mor', 'atk')).toBeGreaterThan(open);
  });

  it('the card is left as it was found: no stand-in stays on the map, every strength as it was', () => {
    const g = grand();
    const { from, target } = sector(g);
    const own = line(g, 'GER', from, 3);
    only(g, own);
    line(g, 'POL', target, 2);
    for (const c of own) c.str = 0.6;
    g.nations.GER.pp = TEAMS.mg.cost + 1;
    const units = [...from.units], strs = own.map((c) => c.str);
    const k = new Reckoning(g, 'GER');
    for (const o of ['inf', 'mg', 'mor', 'refit'] as const) for (const r of ['atk', 'def'] as const) k.worth(o, r, TEAMS.mg.cost);
    expect(from.units).toEqual(units);
    expect(own.map((c) => c.str)).toEqual(strs);
  });

  it('a refit is what the money would buy only where the purse, the order paid, would not cover the month\'s drafts', () => {
    const g = grand();
    const { from, target } = sector(g);
    const own = line(g, 'GER', from, 4);
    only(g, own);
    line(g, 'POL', target, 2);
    for (const c of own) c.str = 0.6;
    g.nations.GER.pp = 1000;
    expect(worth(g, 'refit', 'def', TEAMS.mg.cost)).toBe(0);
    g.nations.GER.pp = TEAMS.mg.cost + 1;
    const refit = worth(g, 'refit', 'def', TEAMS.mg.cost);
    expect(refit).toBeGreaterThan(0);
    // A division's worth of drafts is far cheaper than a division raised new: it buys more on the card a point.
    expect(refit).toBeGreaterThan(worth(g, 'inf', 'def'));
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
    for (const c of own) { c.xp = 0.8; c.org = 1; }
    ready(g);
    const pick = buildPick(g, 'GER', lineFactories(g, 'GER'), g.nations.GER.capital).kind;
    const k = new Reckoning(g, 'GER');
    expect(k.worth('mg', 'def')).toBeGreaterThan(k.worth(pick, 'def'));
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
