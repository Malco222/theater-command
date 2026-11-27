// NEXT ARMY (Version 82): src/campaign/round.ts, the round of armies that still want an order.
//
// Brian's ask of 2026-11-16, playing the 3D build: "you need a way to cycle through your units that have moves left
// that you haven't used yet so you dont miss them". The design director's ruling 88:
//
// 1. In the round: every army of his that he may order and that still has a move. The untouched come first and
//    those at the front before those in the rear, then the nearest; each is visited once a round.
// 2. Left out: a kept march, an army ordered to dig in, an engineer corps with work in hand, a fortress, and an
//    army told to STAND THIS MONTH.
// 3. STAND THIS MONTH lasts until the next month opens, and a second press takes it back.
// 4. The question as the month ends counts only armies at the front that have not moved at all and have no orders.
// 5. It is the player's book-keeping: asking changes nothing, no computer country uses it, and it is saved with the game.
import { describe, expect, it } from 'vitest';
import { addCorps, atWar, createGame } from '../src/campaign/game';
import { orderMarch, planMarch } from '../src/campaign/marches';
import { armyRound, armyStands, atFront, frontIdle, nextArmy, roundCount, settleStands, standArmy, standsThisMonth, wantsOrder } from '../src/campaign/round';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, canOrder, endHuman, humanUp, orderMove, orderStance, reach, stepAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';

const ME = 'FRA';
const others = (g: Game): void => { let guard = 0; while (!humanUp(g) && guard++ < 400) stepAI(g); };
/** September 1939 on the wider world, France's player up with his first month in front of him. */
const START = ((): string => {
  const g = startOpen(createGame(25, ME));
  others(g);
  beginHuman(g);
  return saveGame(g);
})();
const game = (): Game => loadGame(START)!;
const field = (g: Game, reg: number): Corps[] => g.regions[reg].units.filter((c) => canOrder(g, c) && c.kind !== 'fort');
const stood = (g: Game): string[] => Object.keys(g.flags).filter((k) => k.startsWith('stood:'));
function nextMonth(g: Game): void {
  endHuman(g);
  others(g);
  beginHuman(g);
  settleStands(g);
}

describe('who is in the round', () => {
  it('is every army of his with a move left, the front before the rear', () => {
    const g = game();
    const round = armyRound(g);
    const mine = g.regions.filter((r) => field(g, r.id).length).map((r) => r.id);
    expect(round.length).toBeGreaterThan(5);
    expect(round.map((a) => a.reg).sort((a, b) => a - b)).toEqual(mine.sort((a, b) => a - b));
    // In the war's first month nobody has moved: every army is untouched, and those beside Germany come first.
    expect(round.every((a) => a.untouched)).toBe(true);
    const front = round.filter((a) => a.front), rear = round.filter((a) => !a.front);
    expect(front.length).toBeGreaterThan(0);
    expect(rear.length).toBeGreaterThan(0);
    expect(round.slice(0, front.length).every((a) => a.front)).toBe(true);
    for (const a of front) expect(g.regions[a.reg].nb.some((j) => atWar(g, ME, g.regions[j].ctrl ?? ''))).toBe(true);
    for (const a of round) expect(atFront(g, a.reg)).toBe(a.front);
    for (const a of round) expect(a.corps).toEqual(field(g, a.reg).map((c) => c.id));
  });

  it('lets go of an army as its moves are spent, and puts a touched army after the untouched', () => {
    const g = game();
    const before = armyRound(g);
    // A rear army with friendly ground beside it marches one step: it has moved, and comes after every untouched army.
    const a = before.find((x) => !x.front && x.corps.length === 1 && [...reach(g, g.corps.find((c) => c.id === x.corps[0])!)].some(([k, v]) => !v.attack && k !== x.reg))!;
    const c = g.corps.find((x) => x.id === a.corps[0])!;
    const [to] = [...reach(g, c)].filter(([k, v]) => !v.attack && k !== c.reg).sort((p, q) => p[1].cost - q[1].cost)[0];
    expect(orderMove(g, c, to)).toBeTruthy();
    const after = armyRound(g);
    expect(after.some((x) => x.reg === a.reg)).toBe(false);
    const there = after.find((x) => x.reg === to);
    if (c.ap > 0 && reach(g, c).size > 0) {
      expect(there?.corps).toContain(c.id);
      if (there!.corps.length === 1) { expect(there!.untouched).toBe(false); expect(after.indexOf(there!)).toBeGreaterThanOrEqual(after.filter((x) => x.untouched).length); }
    }
    // With no move left at all it wants no order.
    c.ap = 0;
    expect(wantsOrder(g, c)).toBe(false);
    expect(armyRound(g).some((x) => x.corps.includes(c.id))).toBe(false);
  });

  it('leaves out a kept march, an army ordered to dig in, an engineer at work and a fortress', () => {
    const g = game();
    const round = armyRound(g);
    const n = round.length;
    // A kept march: the first rear army that can be given one to another province of France.
    let marched = -1;
    for (const a of round.filter((x) => !x.front)) {
      const c = g.corps.find((x) => x.id === a.corps[0])!;
      const far = g.regions.find((r) => r.ctrl === ME && r.id !== c.reg && !reach(g, c).has(r.id) && planMarch(g, c, r.id).ok && !planMarch(g, c, r.id).now);
      if (!far) continue;
      for (const id of a.corps) orderMarch(g, g.corps.find((x) => x.id === id)!, far.id);
      marched = a.reg;
      break;
    }
    expect(marched).toBeGreaterThanOrEqual(0);
    expect(armyRound(g).some((x) => x.reg === marched)).toBe(false);
    // An order to dig in.
    const dig = armyRound(g)[0];
    for (const id of dig.corps) orderStance(g, g.corps.find((x) => x.id === id)!, 'dig');
    expect(armyRound(g).some((x) => x.reg === dig.reg)).toBe(false);
    expect(armyRound(g).length).toBe(n - 2);
    // An engineer corps with work in hand.
    const eng = armyRound(g)[0];
    const e = g.corps.find((x) => x.id === eng.corps[0])!;
    e.job = { what: 'bridge', reg: e.reg, eta: g.turn + 1 };
    expect(wantsOrder(g, e)).toBe(false);
    // A fortress is never in it.
    const fort = g.corps.find((c) => c.nat === ME && !c.dead && c.kind === 'fort');
    if (fort) { expect(wantsOrder(g, fort)).toBe(false); expect(armyRound(g).some((x) => x.corps.includes(fort.id))).toBe(false); }
  });

  it('is empty when it is not his month, and in a war nobody plays', () => {
    const g = game();
    endHuman(g);
    expect(armyRound(g)).toEqual([]);
    expect(frontIdle(g)).toEqual([]);
    const w = startOpen(createGame(25, null));
    expect(armyRound(w)).toEqual([]);
    expect(nextArmy(w, [], null).army).toBeNull();
    expect(stood(w)).toEqual([]);
  });
});

describe('the order he is shown them in', () => {
  it('visits each army once a round, the nearest first among equals, and then starts again', () => {
    const g = game();
    const all = armyRound(g);
    const eye = { x: g.byName.Paris.x, y: g.byName.Paris.y };
    let seen: number[] = [];
    const order: number[] = [];
    for (let k = 0; k < all.length; k++) {
      const nx = nextArmy(g, seen, k ? { x: g.regions[order[k - 1]].x, y: g.regions[order[k - 1]].y } : eye);
      expect(nx.again).toBe(false);
      order.push(nx.army!.reg);
      seen = nx.seen;
    }
    expect(new Set(order).size).toBe(all.length);
    // The front is gone through before the rear.
    const nFront = all.filter((a) => a.front).length;
    expect(order.slice(0, nFront).every((r) => atFront(g, r))).toBe(true);
    expect(order.slice(nFront).some((r) => atFront(g, r))).toBe(false);
    // The first shown is the front army nearest the eye.
    const d = (r: number): number => Math.hypot(g.regions[r].x - eye.x, g.regions[r].y - eye.y);
    expect(d(order[0])).toBe(Math.min(...all.filter((a) => a.front).map((a) => d(a.reg))));
    // All visited: the round starts again.
    const again = nextArmy(g, seen, eye);
    expect(again.again).toBe(true);
    expect(again.army!.reg).toBe(order[0]);
    expect(again.seen).toEqual([order[0]]);
  });

  it('counts the nearer way round where the map wraps', () => {
    const g = game();
    const all = armyRound(g).filter((a) => a.front);
    const west = all.slice().sort((a, b) => g.regions[a.reg].x - g.regions[b.reg].x)[0];
    const span = 100000;
    // An eye far off the map's east edge is next door to its west edge when the map wraps.
    const eye = { x: g.regions[west.reg].x + span - 1, y: g.regions[west.reg].y };
    expect(nextArmy(g, [], eye, span).army!.reg).toBe(west.reg);
  });
});

describe('STAND THIS MONTH', () => {
  it('takes the army out of the round until the next month opens, and a second press takes it back', () => {
    const g = game();
    const a = armyRound(g)[0];
    const n = armyRound(g).length;
    expect(armyStands(g, a.reg)).toBe('none');
    expect(standArmy(g, a.reg, true)).toBe(a.corps.length);
    expect(armyStands(g, a.reg)).toBe('all');
    expect(armyRound(g).length).toBe(n - 1);
    expect(armyRound(g).some((x) => x.reg === a.reg)).toBe(false);
    for (const id of a.corps) expect(standsThisMonth(g, g.corps.find((c) => c.id === id)!)).toBe(true);
    // A second press.
    expect(standArmy(g, a.reg, false)).toBe(a.corps.length);
    expect(armyStands(g, a.reg)).toBe('none');
    expect(armyRound(g).length).toBe(n);
    expect(stood(g)).toEqual([]);
    // Stood again, it is back in the round as the next month opens, and nothing is left written down.
    const state = JSON.stringify([g.corps.map((c) => [c.id, c.reg, c.ap, c.digs, c.dug, c.yields]), g.headlines.length]);
    standArmy(g, a.reg, true);
    // The order is book-keeping: no corps is changed by it and it makes no news.
    expect(JSON.stringify([g.corps.map((c) => [c.id, c.reg, c.ap, c.digs, c.dug, c.yields]), g.headlines.length])).toBe(state);
    nextMonth(g);
    expect(stood(g)).toEqual([]);
    if (field(g, a.reg).length) expect(armyRound(g).some((x) => x.reg === a.reg)).toBe(true);
  });

  it('is kept with the saved game, and a corps that marches away is no longer standing', () => {
    const g = game();
    const a = armyRound(g).find((x) => !x.front)!;
    standArmy(g, a.reg, true);
    const h = loadGame(saveGame(g))!;
    expect(armyStands(h, a.reg)).toBe('all');
    expect(armyRound(h).some((x) => x.reg === a.reg)).toBe(false);
    const c = h.corps.find((x) => x.id === a.corps[0])!;
    const step = [...reach(h, c)].filter(([k, v]) => !v.attack && k !== c.reg).sort((p, q) => p[1].cost - q[1].cost)[0];
    if (step && orderMove(h, c, step[0])) expect(standsThisMonth(h, c)).toBe(false);
  });

  it('says nothing where there is nothing to stand', () => {
    const g = game();
    const empty = g.regions.find((r) => r.ctrl === ME && !field(g, r.id).length)!;
    expect(armyStands(g, empty.id)).toBeNull();
    expect(standArmy(g, empty.id, true)).toBe(0);
  });
});

describe('the question as the month ends', () => {
  it('counts only armies at the front that have not moved and have no orders', () => {
    const g = game();
    const idle = frontIdle(g);
    expect(idle).toEqual(armyRound(g).filter((a) => a.front).map((a) => a.reg));
    // The rear never raises it: with every front army standing there is nothing to ask.
    for (const r of idle) standArmy(g, r, true);
    expect(frontIdle(g)).toEqual([]);
    expect(armyRound(g).length).toBeGreaterThan(0);
  });

  it('does not count an army he has marched a corps into', () => {
    const g = game();
    const idle = frontIdle(g);
    // A rear corps that can reach a front army this month joins it: that army has had his eye.
    let joined = -1;
    for (const a of armyRound(g).filter((x) => !x.front)) {
      const c = g.corps.find((x) => x.id === a.corps[0])!;
      const to = idle.find((r) => { const v = reach(g, c).get(r); return !!v && !v.attack; });
      if (to === undefined) continue;
      if (orderMove(g, c, to) && c.reg === to && c.ap < c.apMax) { joined = to; break; }
    }
    if (joined >= 0) expect(frontIdle(g)).not.toContain(joined);
    else expect(idle.length).toBeGreaterThan(0);
  });
});

describe('the two numbers beside NEXT ARMY (ruling 101(e))', () => {
  it('the bright number is the untouched armies at the front; the rear is the quieter one and never holds the light', () => {
    const g = game();
    const round = armyRound(g), n = roundCount(g);
    const front = round.filter((a) => a.front), rear = round.filter((a) => !a.front);
    expect(n).toEqual({ bright: front.length, quiet: rear.length, front: front.length });
    expect(round.filter((a) => a.bright)).toEqual(front);
    expect(round.some((a) => a.raised)).toBe(false);
    // Every army at the front told to stand: the bright number is nought with the whole rear still in the round.
    for (const a of front) standArmy(g, a.reg, true);
    expect(roundCount(g)).toEqual({ bright: 0, quiet: rear.length, front: 0 });
    expect(armyRound(g).map((a) => a.reg).sort((a, b) => a - b)).toEqual(rear.map((a) => a.reg).sort((a, b) => a - b));
    // Tab still visits the rear.
    expect(nextArmy(g, [], null).army?.front).toBe(false);
    // An army in the rear that has marched part of its month has had an order: it leaves the rear's number, and Tab
    // still finds it after all the untouched while it can go somewhere.
    const moved = rear[0];
    for (const c of field(g, moved.reg)) c.ap = c.apMax - 1;
    expect(roundCount(g)).toEqual({ bright: 0, quiet: rear.length - 1, front: 0 });
    const after = armyRound(g);
    expect(after[after.length - 1].reg).toBe(moved.reg);
    // The whole rear told to stand as well: both numbers are nought.
    for (const a of rear) standArmy(g, a.reg, true);
    expect(roundCount(g)).toEqual({ bright: 0, quiet: 0, front: 0 });
    expect(armyRound(g)).toEqual([]);
  });

  it('an army at the front that he has touched leaves the bright number while it still has a move', () => {
    const g = game();
    const before = roundCount(g);
    const a = armyRound(g).find((x) => x.front)!;
    // Every corps of it has marched a little of its month (a short step), and can march again.
    for (const c of field(g, a.reg)) c.ap = c.apMax - 1;
    const at = armyRound(g).find((x) => x.reg === a.reg)!;
    expect(at).toMatchObject({ front: true, untouched: false, bright: false });
    // It joins neither number: the rear's is the untouched too.
    expect(roundCount(g)).toEqual({ bright: before.bright - 1, quiet: before.quiet, front: before.front - 1 });
    expect(armyRound(g).some((x) => x.reg === a.reg)).toBe(true);
    // The question as the month ends does not count it either: it has had his eye.
    expect(frontIdle(g)).not.toContain(a.reg);
  });

  it('a country at peace opens with nothing bright: every army is the quieter number', () => {
    const g = startOpen(createGame(25, 'BRA'));
    let guard = 0;
    while (!humanUp(g) && guard++ < 400) stepAI(g);
    beginHuman(g);
    const round = armyRound(g);
    expect(round.length).toBeGreaterThan(0);
    expect(roundCount(g)).toEqual({ bright: 0, quiet: round.length, front: 0 });
    expect(frontIdle(g)).toEqual([]);
  });

  it('an army raised this month is bright until it has an order, wherever it stands; the armies a campaign opens with are not', () => {
    const g = game();
    expect(armyRound(g).some((a) => a.raised)).toBe(false);
    nextMonth(g);
    const rear = armyRound(g).find((a) => !a.front && !a.raised)!;
    const before = roundCount(g);
    expect(rear.bright).toBe(false);
    // A corps raised in a quiet province behind the front, this month.
    const quietPlace = g.regions.find((r) => r.ctrl === ME && r.owner === ME && !atFront(g, r.id) && !field(g, r.id).length && r.units.length === 0)!;
    const c = addCorps(g, ME, 'XXX Corps', 'inf', 2, quietPlace.id, {});
    c.ap = c.apMax;
    if (!wantsOrder(g, c)) throw new Error('the new corps is not in the round');
    const at = armyRound(g).find((a) => a.reg === quietPlace.id)!;
    expect(at).toMatchObject({ front: false, raised: true, bright: true });
    // (It is bright and not at the front: the page says "newly raised" when none of the bright is at the front.)
    expect(roundCount(g)).toEqual({ bright: before.bright + 1, quiet: before.quiet, front: before.front });
    // It is shown before the rest of the rear.
    const order = armyRound(g).map((a) => a.reg);
    expect(order.indexOf(quietPlace.id)).toBeLessThan(order.indexOf(rear.reg));
    // STAND THIS MONTH is an order: it is bright no longer.
    standArmy(g, quietPlace.id, true);
    expect(roundCount(g)).toEqual(before);
    // And next month it is an army like any other.
    nextMonth(g);
    expect(armyRound(g).find((a) => a.reg === quietPlace.id)?.raised ?? false).toBe(false);
  });
});
