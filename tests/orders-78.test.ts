// What is on order, place by place (Version 78; the design director's rulings 46 and 57). Brian: "i want it to be
// clear that you can see what city is producing what unit and how many at a time and how many turns until they are
// ready to be deployed". One call serves both interfaces. It shows; it changes nothing.
import { describe, expect, it } from 'vitest';
import { PRODUCTION, deliver } from '../src/campaign/economy';
import { createGame } from '../src/campaign/game';
import { cityOrders, inMonths, ordersAt } from '../src/campaign/orders';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { eliteOf } from '../src/campaign/traits';
import { beginHuman, humanUp, orderArsenal, orderBuildCorps, orderCancelBuild, ordersByCity, ordersIn, stepAI } from '../src/campaign/turn';
import type { Agent, Build, Game } from '../src/campaign/types';

function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  g.nations[human].queue = [];
  return g;
}
let next = 9000;
/** Puts an order straight on a nation's books: `months` from now, placed `ago` months back. */
function put(g: Game, nat: string, at: string, b: Partial<Build> & { what: Build['what'] }, months: number, ago = 0): Build {
  const full: Build = { id: next++, name: `Order ${next}`, region: g.byName[at].id, ordered: g.turn - ago, eta: g.turn + months, cost: 10, ...b } as Build;
  g.nations[nat].queue.push(full);
  return full;
}
const corps = (kind: string, name: string, extra: Partial<Build> = {}): Partial<Build> & { what: Build['what'] } => ({ ...extra, what: 'corps', kind: kind as Build['kind'], divs: 2, name });

describe('one call for what every city has on order', () => {
  it('lists each place once, with what, how many and the months to each', () => {
    const g = playing();
    expect(ordersByCity(g)).toEqual([]);
    put(g, 'GER', 'Berlin', corps('inf', 'IV Corps'), 3, 1);
    put(g, 'GER', 'Berlin', corps('inf', 'V Corps'), 2, 2);
    put(g, 'GER', 'Berlin', { what: 'air', item: 'ftr', count: 100, name: 'Fighter wing for Berlin' }, 4);
    put(g, 'GER', 'Bavaria', corps('arm', 'XIX Corps'), 5);
    const all = ordersByCity(g);
    expect(all.map((c) => c.province)).toEqual(['Berlin', 'Bavaria']);
    // The province, and its city or town.
    expect(all[1].town).toBe('Munich');
    const b = all[0];
    expect(b.holder).toBe('GER');
    expect(b.spied).toBe(false);
    expect(b.units).toBe(3);
    expect(b.works).toBe(0);
    // Soonest first.
    expect(b.lines.map((l) => l.name)).toEqual(['V Corps', 'IV Corps', 'Fighter wing for Berlin']);
    expect(b.lines.map((l) => l.months)).toEqual([2, 3, 4]);
    expect(b.kinds.map((k) => [k.count, k.label])).toEqual([[2, 'infantry corps'], [1, 'fighter wing']]);
    expect(b.summary).toBe('3 on order: 2 infantry corps and 1 fighter wing. Next ready in 2 months.');
    expect(b.list[0]).toMatch(/^V Corps: ready .* \(in 2 months\)$/);
    expect(b.next.name).toBe('V Corps');
    expect(b.months).toBe(2);
    // Each order says where it will stand, how far along it is, and whether it can still be cancelled for its price.
    const v = b.lines[0];
    expect(v.stands).toBe(g.byName['Berlin'].id);
    expect([v.done, v.need]).toEqual([2, 4]);
    expect(v.fresh).toBe(false);
    expect(b.lines[2].fresh).toBe(true);
    expect(b.lines[2].holds).toBe(100);
    expect(b.lines[2].unit).toBe(true);
    // One place on its own.
    expect(ordersIn(g, g.byName['Bavaria'].id)!.summary).toBe('1 on order: 1 armoured corps. Ready in 5 months.');
    expect(ordersIn(g, g.byName['Hamburg'].id)).toBeNull();
    expect(inMonths(0)).toBe('this month');
    expect(inMonths(1)).toBe('in 1 month');
  });

  it('is display only: asking changes nothing in the game', () => {
    const g = playing();
    put(g, 'GER', 'Berlin', corps('inf', 'IV Corps'), 3);
    put(g, 'GER', 'Berlin', { what: 'work', item: 'factory', name: 'Factory at Berlin' }, 8);
    const before = saveGame(g);
    ordersByCity(g);
    ordersIn(g, g.byName['Berlin'].id);
    cityOrders(g, 'FRA');
    expect(saveGame(g)).toBe(before);
  });

  it('follows the orders the player really places, cancels and receives', () => {
    const g = playing();
    g.nations.GER.pp = 500;
    const berlin = g.byName['Berlin'].id;
    const b = orderBuildCorps(g, 'inf', berlin)!;
    expect(b).not.toBeNull();
    const c = ordersIn(g, b.region)!;
    expect(c.lines).toHaveLength(1);
    expect(c.lines[0].id).toBe(b.id);
    expect(c.lines[0].label).toBe(PRODUCTION.builds.inf.label.toLowerCase());
    expect(c.lines[0].need).toBe(b.eta - b.ordered);
    expect(c.lines[0].fresh).toBe(true);
    const w = orderArsenal(g, 'air:ftr', berlin);
    if (w) expect(ordersIn(g, w.region)!.lines.some((l) => l.id === w.id && l.sign === 'ftr' && l.mark === 'ftr')).toBe(true);
    expect(orderCancelBuild(g, b.id)).toBe(true);
    expect(ordersByCity(g).some((x) => x.lines.some((l) => l.id === b.id))).toBe(false);
    // Handed over, an order leaves the list.
    const again = orderBuildCorps(g, 'inf', berlin)!;
    again.eta = g.turn;
    deliver(g, 'GER');
    expect(ordersByCity(g).some((x) => x.lines.some((l) => l.id === again.id))).toBe(false);
  });
});

describe('the badge: a sign and its number always describe the same thing', () => {
  it('shows the unit ready soonest, counts the orders of exactly that sign, and adds a plus for other kinds', () => {
    const g = playing();
    put(g, 'GER', 'Berlin', corps('arm', 'XIX Corps'), 2);
    put(g, 'GER', 'Berlin', corps('inf', 'IV Corps'), 3);
    put(g, 'GER', 'Berlin', corps('inf', 'V Corps'), 4);
    const c = ordersIn(g, g.byName['Berlin'].id)!;
    expect(c.badge).toEqual({ mark: 'arm', starred: false, count: 1, plus: true, months: 2 });
    expect(c.summary).toBe('3 on order: 1 armoured corps and 2 infantry corps. Next ready in 2 months.');
    // A single order: no count to show and no plus.
    put(g, 'GER', 'Bavaria', corps('mtn', 'XVIII Corps'), 1);
    expect(ordersIn(g, g.byName['Bavaria'].id)!.badge).toEqual({ mark: 'mtn', starred: false, count: 1, plus: false, months: 1 });
    expect(ordersIn(g, g.byName['Bavaria'].id)!.summary).toBe('1 on order: 1 mountain corps. Ready in 1 month.');
    // Several of one kind: the number, and no plus.
    put(g, 'GER', 'Hamburg', corps('inf', 'X Corps'), 3);
    put(g, 'GER', 'Hamburg', corps('inf', 'XI Corps'), 3);
    put(g, 'GER', 'Hamburg', corps('inf', 'XII Corps'), 6);
    expect(ordersIn(g, g.byName['Hamburg'].id)!.badge).toEqual({ mark: 'inf', starred: false, count: 3, plus: false, months: 3 });
  });

  it('on a tie takes the kind with more on order there, then the one ordered first', () => {
    const g = playing();
    put(g, 'GER', 'Berlin', corps('arm', 'XIX Corps'), 3);
    put(g, 'GER', 'Berlin', corps('inf', 'IV Corps'), 3);
    put(g, 'GER', 'Berlin', corps('inf', 'V Corps'), 5);
    expect(ordersIn(g, g.byName['Berlin'].id)!.badge).toEqual({ mark: 'inf', starred: false, count: 2, plus: true, months: 3 });
    const first = put(g, 'GER', 'Bavaria', corps('mot', 'XIV Corps'), 2);
    put(g, 'GER', 'Bavaria', corps('cav', 'I Cavalry Corps'), 2);
    const m = ordersIn(g, g.byName['Bavaria'].id)!;
    expect(m.next.id).toBe(first.id);
    expect(m.badge).toEqual({ mark: 'mot', starred: false, count: 1, plus: true, months: 2 });
  });

  it('counts destroyers, cruisers and battleships together under the hull, and names the classes in the list', () => {
    const g = playing();
    put(g, 'GER', 'Hamburg', { what: 'ship', item: 'dd', count: 4, name: 'Destroyer flotilla at Hamburg' }, 3);
    put(g, 'GER', 'Hamburg', { what: 'ship', item: 'ca', count: 1, name: 'Cruiser at Hamburg' }, 9);
    put(g, 'GER', 'Hamburg', { what: 'ship', item: 'bb', count: 1, name: 'Battleship at Hamburg' }, 20);
    put(g, 'GER', 'Hamburg', { what: 'ship', item: 'ss', count: 4, name: 'Submarine flotilla at Hamburg' }, 5);
    const c = ordersIn(g, g.byName['Hamburg'].id)!;
    expect(c.badge).toEqual({ mark: 'hull', starred: false, count: 3, plus: true, months: 3 });
    expect(c.lines.map((l) => l.mark)).toEqual(['hull', 'ss', 'hull', 'hull']);
    expect(c.kinds.map((k) => [k.count, k.label, k.mark])).toEqual([[1, 'destroyer flotilla', 'hull'], [1, 'submarine flotilla', 'ss'], [1, 'cruiser', 'hull'], [1, 'battleship', 'hull']]);
    expect(c.summary).toBe('4 on order: 1 destroyer flotilla, 1 submarine flotilla, 1 cruiser and 1 battleship. Next ready in 3 months.');
    expect(c.lines[0].holds).toBe(4);
    // A carrier and a troopship keep signs of their own.
    put(g, 'GER', 'Schleswig-Holstein', { what: 'ship', item: 'cv', count: 1, name: 'Aircraft carrier at Schleswig-Holstein' }, 22);
    expect(ordersIn(g, g.byName['Schleswig-Holstein'].id)!.badge!.mark).toBe('cv');
  });

  it('counts a special formation apart from plain corps of its kind, under a starred sign', () => {
    const g = playing();
    const own = eliteOf('GER')!;
    put(g, 'GER', 'Berlin', corps(own.kind, 'Heavy Corps', { elite: own.id }), 2);
    put(g, 'GER', 'Berlin', corps(own.kind, 'XIX Corps'), 4);
    const c = ordersIn(g, g.byName['Berlin'].id)!;
    expect(c.badge).toEqual({ mark: own.kind, starred: true, count: 1, plus: true, months: 2 });
    expect(c.lines[0].elite).toBe(true);
    expect(c.lines[0].label).toBe(own.label.charAt(0).toLowerCase() + own.label.slice(1));
    expect(c.kinds).toHaveLength(2);
    expect(c.kinds[0].starred).toBe(true);
    expect(c.kinds[1].starred).toBe(false);
    // Airborne troops take the parachute.
    put(g, 'GER', 'Bavaria', corps('inf', '1st Airborne Corps', { airborne: true }), 6);
    const m = ordersIn(g, g.byName['Bavaria'].id)!;
    expect(m.badge!.mark).toBe('para');
    expect(m.lines[0].label).toBe('airborne corps');
  });

  it('gives works no badge and no plus: they are listed after the units', () => {
    const g = playing();
    put(g, 'GER', 'Berlin', { what: 'work', item: 'factory', name: 'Factory at Berlin' }, 1);
    put(g, 'GER', 'Berlin', { what: 'fort', name: 'Fortifications at Berlin' }, 2);
    const only = ordersIn(g, g.byName['Berlin'].id)!;
    expect(only.badge).toBeNull();
    expect(only.units).toBe(0);
    expect(only.works).toBe(2);
    expect(only.summary).toBe('2 on order: 1 factory and 1 fortifications. Next ready in 1 month.');
    expect(only.lines.every((l) => !l.unit && l.mark === '')).toBe(true);
    // A unit ordered beside them leads, though it is ready later; the works do not bring the plus.
    put(g, 'GER', 'Berlin', corps('inf', 'IV Corps'), 3);
    const c = ordersIn(g, g.byName['Berlin'].id)!;
    expect(c.badge).toEqual({ mark: 'inf', starred: false, count: 1, plus: false, months: 3 });
    expect(c.lines.map((l) => l.unit)).toEqual([true, false, false]);
    expect(c.next.name).toBe('IV Corps');
    expect(c.summary).toBe('3 on order: 1 infantry corps, 1 factory and 1 fortifications. Next ready in 3 months.');
    // Places with a unit forming come before places with only works under way.
    put(g, 'GER', 'Bavaria', { what: 'work', item: 'aa', name: 'Anti-aircraft guns at Bavaria' }, 1);
    expect(ordersByCity(g).map((x) => x.province)).toEqual(['Berlin', 'Bavaria']);
  });
});

describe('another country', () => {
  it('shows only what an agent in the very province reports', () => {
    const g = playing();
    put(g, 'FRA', 'Paris', corps('inf', 'Test Corps'), 3);
    put(g, 'FRA', 'Paris', corps('arm', 'Test Armour'), 5);
    put(g, 'FRA', 'Bordeaux', corps('inf', 'Elsewhere Corps'), 2);
    expect(ordersByCity(g)).toEqual([]);
    expect(ordersIn(g, g.byName['Paris'].id)).toBeNull();
    const a: Agent = { id: g.intel.next++, nat: 'GER', name: 'Test agent', xp: 0, reg: g.byName['Paris'].id, ready: g.turn, mission: 'watch', born: g.turn };
    g.intel.agents.push(a);
    const all = ordersByCity(g);
    expect(all.map((c) => c.province)).toEqual(['Paris']);
    expect(all[0].spied).toBe(true);
    expect(all[0].holder).toBe('FRA');
    expect(all[0].badge).toEqual({ mark: 'inf', starred: false, count: 1, plus: true, months: 3 });
    // An agent still on the road reports nothing yet.
    a.ready = g.turn + 1;
    expect(ordersByCity(g)).toEqual([]);
    a.ready = g.turn;
    // The player's own places come before what his agents report.
    put(g, 'GER', 'Berlin', corps('inf', 'IV Corps'), 6);
    expect(ordersByCity(g).map((c) => [c.province, c.spied])).toEqual([['Berlin', false], ['Paris', true]]);
    // The French see their own orders in full, and nothing of Germany's.
    expect(cityOrders(g, 'FRA').map((c) => c.province).sort()).toEqual(['Bordeaux', 'Paris']);
  });
});
