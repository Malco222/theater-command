// The gate's order (Version 84.1, ruling 176(e); the grand campaign). On Brian's words of 2026-11-17: "you should be
// able to open or close the panama canal". The holder of a gate may shut it to all ships but his own and his
// allies', and open it again. The order ends when the gate changes hands. The Turkish straits keep their stricter
// rule beside it. The price is what the rules already do: trade that runs through a shut gate stops or goes the long
// way round, and the question names whose, with a figure a month, before he orders. The player's order only.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { gateBar, gateViews } from '../src/campaign/gates';
import { canPass, gateShutSince, gatesTick, setGate } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { endTrade, signTrade, tradeIncome } from '../src/campaign/trade';
import { FORT_NO_ORDERS, beginHuman, canOrder, corpsDetail, endHuman, gateQuestion, humanUp, orderGate, stepAI, whyNotGate, whyNotOrder } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

/** A grand campaign with one country at the table, in its first month to give orders. */
function seat(nat: string, seed = 3): Game {
  const g = startOpen(createGame(seed, nat));
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
  return g;
}
function nextMonth(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
}
const gateOf = (g: Game, name: RegExp): number => g.gates.findIndex((x) => name.test(x.name));
/**
 * On the cities chart no agreement of the first day sails through the canal: the Dominican Republic's trade with
 * Guatemala keeps to the Caribbean. So Peru and the Dominican Republic each give up one agreement (Chile's with Peru,
 * Haiti's with the Dominican Republic) and sign one with each other, whose goods go through it.
 */
function canalTrade(g: Game): void {
  expect(endTrade(g, 'CHL', 'PER')).toBe(true);
  expect(endTrade(g, 'HAI', 'DOM')).toBe(true);
  expect(signTrade(g, 'PER', 'DOM', new Map(), true)).not.toBeNull();
}
const passes = (g: Game, i: number, nat: string): boolean => canPass(g, g.gates[i].a, g.gates[i].b, nat);

describe('the gate its holder can shut', () => {
  it('America holds the Panama Canal and may shut it; nobody else may', () => {
    const g = seat('USA');
    const p = gateOf(g, /Panama/), gib = gateOf(g, /Gibraltar/);
    expect(p).toBeGreaterThanOrEqual(0);
    const v = gateViews(g)[p];
    expect(v.holder).toBe('USA');
    expect(v.order).toBe('shut');
    expect(v.orderLine).toBe('Shut the gate');
    expect(v.shutSince).toBe(-1);
    expect(v.shutLine).toBe('');
    expect(v.line).toBe('You hold it. Open to your ships.');
    expect(whyNotGate(g, p)).toBeNull();
    // A gate another country holds: no order, and the line says whose it is to give.
    const other = gateViews(g)[gib];
    expect(other.order).toBeNull();
    expect(other.orderLine).toBe('Only its holder can shut it.');
    expect(whyNotGate(g, gib)).toBe('Only its holder can shut it.');
    expect(gateQuestion(g, gib)).toBeNull();
    expect(orderGate(g, gib, true)).toBe(false);
    expect(gateShutSince(g, gib)).toBe(-1);
    // Britain, looking at Panama, has no order either.
    expect(gateViews(g, 'UK')[p].order).toBeNull();
  });

  it('the question names whose trade goes the long way round, with a figure, and changes nothing', () => {
    const g = seat('USA');
    canalTrade(g);
    const p = gateOf(g, /Panama/);
    gateQuestion(g, p); // any forecast refreshes the supply cache once
    const before = saveGame(g);
    const q = gateQuestion(g, p)!;
    expect(saveGame(g)).toBe(before);
    expect(q.order).toBe('shut');
    expect(q.question).toBe('Shut the Panama Canal?');
    expect(q.label).toBe('Sea gate · you hold it');
    expect(q.yes).toBe('Shut the gate');
    expect(q.no).toBe('Leave it open');
    expect(q.heading).toBe('What it costs each month');
    expect(q.lead).toContain("only your own ships and your allies'");
    // Every agreement whose goods go through the canal today is on the tally, both its countries, each with a loss.
    const gt = g.gates[p];
    const through = g.trade.filter((d) => d.way === 'sea' && d.seas.some((z, k) => k > 0 && ((d.seas[k - 1] === gt.a && z === gt.b) || (d.seas[k - 1] === gt.b && z === gt.a))));
    expect(through.length).toBeGreaterThan(0);
    for (const d of through) for (const nat of [d.a, d.b]) {
      const row = q.rows.find((r) => r.nat === nat)!;
      expect(row, `${nat} is on the tally`).toBeDefined();
      expect(row.loss).toBeGreaterThan(0);
      expect(row.what).toMatch(/^Its trade with .+ (stops|goes the long way round)\.$/);
      expect(row.own).toBe(false);
    }
    expect(q.total).toBeCloseTo(q.rows.reduce((s, r) => s + r.loss, 0), 9);
  });

  it('shut, only the holder and his allies pass; the view, the plate and the refusal say who shut it', () => {
    const g = seat('USA');
    const p = gateOf(g, /Panama/);
    expect(passes(g, p, 'UK')).toBe(true);
    expect(orderGate(g, p, true)).toBe(true);
    expect(orderGate(g, p, true)).toBe(false); // it is shut already
    expect(gateShutSince(g, p)).toBe(g.turn);
    expect(passes(g, p, 'USA')).toBe(true);
    expect(passes(g, p, 'UK')).toBe(false);
    expect(passes(g, p, 'CHL')).toBe(false);
    const mine = gateViews(g)[p];
    expect(mine.open).toBe(true);
    expect(mine.order).toBe('open');
    expect(mine.orderLine).toBe('Open the gate');
    expect(mine.line).toBe("You hold it, and you have shut it: only your own ships and your allies' pass.");
    expect(mine.shutLine).toBe("You shut it in September 1939: only your own ships and your allies' pass.");
    const uk = gateViews(g, 'UK')[p];
    expect(uk.open).toBe(false);
    expect(uk.why).toBe("the United States has shut it to all ships but its own and its allies'");
    expect(uk.line).toBe(`Shut to your ships: ${uk.why}.`);
    expect(uk.shutLine).toBe("The United States shut it in September 1939: only its own ships and its allies' pass.");
    expect(uk.orderLine).toBe('Only its holder can open it.');
    // A British fleet that could reach the Gulf of Panama through the canal is told why it cannot.
    const f = g.fleets.find((x) => x.nat === 'UK')!;
    const was = { base: f.base, home: f.home };
    const carib = g.regions.find((r) => r.ctrl === 'UK' && r.port && r.seas.includes(g.gates[p].a));
    if (carib) {
      f.base = carib.id;
      const far = g.gates[p].b;
      const bar = gateBar(g, f, far);
      if (bar) expect(bar.line).toBe(`The Panama Canal is closed to your ships: ${uk.why}.`);
      f.base = was.base;
    }
    // The question now offers to open it, and says since when.
    const q = gateQuestion(g, p)!;
    expect(q.order).toBe('open');
    expect(q.question).toBe('Open the Panama Canal again?');
    expect(q.heading).toBe('What it is costing each month');
    expect(q.lead).toContain('You shut it in September 1939.');
    expect(q.yes).toBe('Open the gate');
    expect(q.no).toBe('Leave it shut');
    expect(q.rows.length).toBeGreaterThan(0);
  });

  it('a save keeps the gate shut; opened again at once, everything stands as it did', () => {
    const g = seat('USA');
    const p = gateOf(g, /Panama/);
    gateQuestion(g, p);
    const before = saveGame(g);
    expect(orderGate(g, p, true)).toBe(true);
    const h = loadGame(saveGame(g))!;
    expect(gateShutSince(h, p)).toBe(g.turn);
    expect(passes(h, p, 'UK')).toBe(false);
    expect(orderGate(g, p, false)).toBe(true);
    expect(orderGate(g, p, false)).toBe(false); // it is open already
    expect(gateShutSince(g, p)).toBe(-1);
    expect(passes(g, p, 'UK')).toBe(true);
    // The order marks the supply net to be looked at again, as any change at sea does: nothing else differs.
    expect(saveGame(g).replace('"dirty":true', '"dirty":false')).toBe(before.replace('"dirty":true', '"dirty":false'));
  });

  it('trade through a shut gate goes the long way round or stops, from the month the gate is shut', () => {
    const g = seat('USA');
    canalTrade(g);
    const p = gateOf(g, /Panama/), gt = g.gates[p];
    const crossing = (): typeof g.trade => g.trade.filter((d) => d.way === 'sea' && d.state !== 'cut' && d.seas.some((z, k) => k > 0 && ((d.seas[k - 1] === gt.a && z === gt.b) || (d.seas[k - 1] === gt.b && z === gt.a))));
    const was = crossing().map((d) => ({ a: d.a, b: d.b, pays: d.worth * (1 - d.lost), income: tradeIncome(g, d.a) }));
    expect(was.length).toBeGreaterThan(0);
    expect(orderGate(g, p, true)).toBe(true);
    nextMonth(g);
    // No goods cross the canal any longer, but for the holder's own and his allies'.
    expect(crossing().filter((d) => d.a !== 'USA' && d.b !== 'USA').length).toBe(0);
    for (const w of was) {
      const d = g.trade.find((x) => x.a === w.a && x.b === w.b);
      // The agreement goes the long way round and pays less, or nothing got through and the two countries gave it up.
      if (d) expect(d.state === 'cut' ? 0 : d.worth * (1 - d.lost)).toBeLessThan(w.pays);
    }
    // Open again, the canal carries trade as before from the next month.
    expect(orderGate(g, p, false)).toBe(true);
    expect(passes(g, p, 'UK')).toBe(true);
  });

  it('the order ends when the gate changes hands, and does not come back with it', () => {
    const g = seat('USA');
    const p = gateOf(g, /Panama/), zone = g.regions[g.gates[p].region];
    expect(orderGate(g, p, true)).toBe(true);
    zone.ctrl = 'UK';
    // At once: the new holder has given no such order.
    expect(gateShutSince(g, p)).toBe(-1);
    expect(passes(g, p, 'CHL')).toBe(true);
    expect(gateViews(g)[p].order).toBeNull();
    expect(whyNotGate(g, p)).toBe('Only its holder can shut it.');
    // As the month turns the old order is forgotten: the gate retaken is open.
    gatesTick(g);
    expect(Object.keys(g.flags).some((k) => k.startsWith('gateshut:'))).toBe(false);
    zone.ctrl = 'USA';
    expect(gateShutSince(g, p)).toBe(-1);
    expect(passes(g, p, 'UK')).toBe(true);
    expect(gateViews(g)[p].order).toBe('shut');
  });

  it('the Turkish straits keep their stricter rule beside it', () => {
    const g = seat('TUR');
    const t = gateOf(g, /Turkish/);
    expect(g.gates[t].closed).toBe(true);
    const atPeace = Object.values(g.nations).find((n) => n.alive && !n.dormant && n.id !== 'TUR' && !n.atWar.length)!;
    const atWar = Object.values(g.nations).find((n) => n.alive && !n.dormant && n.atWar.length > 0)!;
    // Their own rule: in wartime only the holder's allies pass, and a country at war with nobody.
    expect(passes(g, t, atPeace.id)).toBe(true);
    expect(passes(g, t, atWar.id)).toBe(false);
    const q = gateQuestion(g, t)!;
    expect(q.question).toBe('Shut the Turkish straits?');
    expect(q.lead).toMatch(/^Shut, they let through only your own ships/);
    expect(orderGate(g, t, true)).toBe(true);
    // Shut by Turkey's order, a country at peace is turned back too; Turkey's own ships pass.
    expect(passes(g, t, atPeace.id)).toBe(false);
    expect(passes(g, t, atWar.id)).toBe(false);
    expect(passes(g, t, 'TUR')).toBe(true);
    // A country its own rule turns back is told that reason; a country at peace is told of the order.
    expect(gateViews(g, atWar.id)[t].why).toMatch(/^in wartime only /);
    expect(gateViews(g, atPeace.id)[t].why).toBe("Turkey has shut it to all ships but its own and its allies'");
    expect(gateQuestion(g, t)!.lead).toContain('in wartime only your allies may pass, and a country at war with nobody');
    expect(orderGate(g, t, false)).toBe(true);
    expect(passes(g, t, atPeace.id)).toBe(true);
  });

  it('no staff shuts a gate: a war nobody plays never has one shut, and the rule can still be set for one', () => {
    const g = startOpen(createGame(20, null));
    for (let k = 0; k < 60; k++) stepAI(g);
    expect(Object.keys(g.flags).some((k) => k.startsWith('gateshut:'))).toBe(false);
    expect(whyNotGate(g, 0)).toBe('Only its holder can shut it.');
    expect(orderGate(g, 0, true)).toBe(false);
    // The rule itself, as a later version's staff would use it.
    setGate(g, 0, true);
    expect(gateShutSince(g, 0)).toBe(g.turn);
    setGate(g, 0, false);
    expect(gateShutSince(g, 0)).toBe(-1);
  });
});

// A unit that takes no orders says why (ruling 176(c)), on Brian's words of 2026-11-17: "there are troops you start
// with in panama but you cant move them". The sentence is the rules' own, on the corps card for both games.
// (Since Version 84.2 Panama's garrison is an infantry division he can move, ruling 176(d): tests/panama-842.test.ts.
// Gibraltar's garrison and the Maginot sectors stay fortress troops.)
describe('a unit that takes no orders says why', () => {
  it("Gibraltar's garrison is fortress troops, and says so", () => {
    const g = seat('UK');
    const rock = g.regions[g.gates[gateOf(g, /Gibraltar/)].region];
    const fort = rock.units.find((c) => c.nat === 'UK' && c.kind === 'fort')!;
    expect(fort, 'Gibraltar has its fortress troops').toBeDefined();
    expect(canOrder(g, fort)).toBe(false);
    expect(whyNotOrder(g, fort)).toBe('Fortress troops man their works and do not move.');
    expect(whyNotOrder(g, fort)).toBe(FORT_NO_ORDERS);
    expect(corpsDetail(g, fort).noOrders).toBe(FORT_NO_ORDERS);
  });

  it('a corps that takes orders says nothing; one sent home says until when; another country its own business', () => {
    const g = seat('USA');
    const mine = g.corps.find((c) => c.nat === 'USA' && !c.dead && c.kind !== 'fort')!;
    expect(canOrder(g, mine)).toBe(true);
    expect(whyNotOrder(g, mine)).toBeNull();
    expect(corpsDetail(g, mine).noOrders).toBeNull();
    mine.leave = g.turn + 2;
    expect(canOrder(g, mine)).toBe(false);
    expect(whyNotOrder(g, mine)).toBe('It is at home, out of the line, and takes no orders until November 1939.');
    expect(corpsDetail(g, mine).noOrders).toBe(whyNotOrder(g, mine));
    delete mine.leave;
    // Another country's fortress troops are not his to order, and nothing is said of it on their card.
    const theirs = g.corps.find((c) => c.nat !== 'USA' && !c.dead && c.kind === 'fort')!;
    expect(theirs).toBeDefined();
    expect(whyNotOrder(g, theirs)).toBeNull();
    expect(corpsDetail(g, theirs).noOrders).toBeNull();
  });
});
