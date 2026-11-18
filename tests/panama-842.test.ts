// Panama's garrison, the gate left to its guard, and who resents a shut gate (Version 84.2; rulings 176(d) and
// 177(a)). On Brian's words of 2026-11-17, playing America: "there are troops you start with in panama but you cant
// move them. you should be able to open or close the panama canal". The Panama Canal Department is now one infantry
// division he can move and the computer's America holds in place; ordering the last corps out of a gate's province
// says the gate is left to its town guard; and a gate shut on the trade of a country at peace costs its holder that
// country's opinion, a point a month to eight at most, named in the gate's box before he orders.
import { describe, expect, it } from 'vitest';
import { RELATIONS, sideHeld, soreList, wouldResent } from '../src/campaign/diplomacy';
import { addCorps, createGame, relocate } from '../src/campaign/game';
import { endTrade, signTrade } from '../src/campaign/trade';
import { loadGame, saveGame } from '../src/campaign/save';
import { guardMen } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, canOrder, endHuman, gateLeft, gateQuestion, humanUp, orderGate, runTurnAI, stepAI, whyNotOrder } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

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
const panama = (g: Game) => g.corps.find((c) => c.name === 'Panama Canal Department')!;

describe("Panama's garrison is a division he can move", () => {
  it('one infantry division in the Canal Zone that takes orders; Gibraltar and the Maginot stay fortress troops', () => {
    const g = seat('USA');
    const c = panama(g);
    expect([c.kind, c.divs, g.regions[c.reg].name, c.hold]).toEqual(['inf', 1, 'Canal Zone', true]);
    expect(canOrder(g, c)).toBe(true);
    expect(whyNotOrder(g, c)).toBeNull();
    expect(g.corps.filter((x) => x.kind === 'fort' && !x.dead).map((x) => x.nat).sort()).toEqual(['FRA', 'FRA', 'FRA', 'UK']);
  });

  it("the computer's America holds it in place", () => {
    const g = startOpen(createGame(20, null));
    for (let t = 1; t < 14; t++) runTurnAI(g);
    const c = panama(g);
    expect(c.dead).toBe(false);
    expect(g.regions[c.reg].name).toBe('Canal Zone');
  });
});

describe('the last corps ordered out of a gate\'s province: the gate is left to its guard', () => {
  it('says so before the order, in the rules\' sentence, and only for the last corps of the gate\'s holder', () => {
    const g = seat('USA');
    const c = panama(g), zone = g.regions[c.reg];
    const left = gateLeft(g, [c])!;
    expect(left.gate).toBe(gateOf(g, /Panama/));
    expect(left.region).toBe(zone.id);
    // (The figure is the one the province card gives for a place held by its town guard only.)
    const men = guardMen(g, zone).toLocaleString('en-US');
    expect(left.men).toBe(guardMen(g, zone));
    expect(left.men).toBeGreaterThan(4000);
    expect(left.ifLine).toBe(`If they go, the Panama Canal is left to its town guard: about ${men} men, and no army.`);
    expect(left.line).toBe(`The Panama Canal is left to its town guard: about ${men} men, and no army.`);
    // Nothing is changed by asking.
    const before = saveGame(g);
    gateLeft(g, [c]);
    expect(saveGame(g)).toBe(before);
    // A second corps stays behind: nothing to say. A corps anywhere else: nothing to say.
    const more = addCorps(g, 'USA', 'II Corps', 'inf', 1, zone.id, {});
    expect(gateLeft(g, [c])).toBeNull();
    expect(gateLeft(g, [c, more])!.line).toBe(left.line);
    const army = g.corps.find((x) => x.nat === 'USA' && !x.dead && g.regions[x.reg].name === 'New York')!;
    expect(gateLeft(g, [army])).toBeNull();
    expect(gateLeft(g, [])).toBeNull();
  });

  it('straits are left to their guard, in the plural; another country\'s gate is not his to leave', () => {
    const d = seat('DEN');
    const zealand = d.regions[d.gates[gateOf(d, /Danish/)].region];
    const all = zealand.units.filter((c) => c.nat === 'DEN' && !c.dead);
    expect(all.length).toBeGreaterThan(0);
    expect(gateLeft(d, all)!.line).toMatch(/^The Danish straits are left to their town guard: about [\d,]+ men, and no army\.$/);
    // America's player picks up nothing in Zealand; and a British corps in Gibraltar is Britain's affair.
    const g = seat('USA');
    const rock = g.regions[g.gates[gateOf(g, /Gibraltar/)].region];
    expect(gateLeft(g, rock.units.filter((c) => !c.dead))).toBeNull();
  });
});

describe('who resents a shut gate', () => {
  it('his own row stands first, the rows add up, and the two neutrals whose trade it turns back are named', () => {
    const g = seat('USA');
    canalTrade(g);
    const p = gateOf(g, /Panama/);
    const q = gateQuestion(g, p)!;
    expect(q.rows[0].own).toBe(true);
    expect(q.rows[0].what).toBe('None of your own trade runs through it.');
    expect(q.rows[0].loss).toBe(0);
    expect(q.rows.slice(1).map((r) => r.nat).sort()).toEqual(['DOM', 'PER']);
    // Each figure is in the money the page shows, and the totals are the rows added up.
    const sumOf = (rs: { loss: number }[]): number => Math.round(rs.reduce((s, r) => s + r.loss, 0) * 100);
    for (const r of q.rows) expect(Math.abs(r.loss * 100 - Math.round(r.loss * 100))).toBeLessThan(1e-9);
    expect(Math.round(q.total * 100)).toBe(sumOf(q.rows));
    expect(Math.round(q.others * 100)).toBe(sumOf(q.rows.filter((r) => !r.own)));
    expect(q.sumLabel).toBe('All told, to others');
    // America leans to the Allies: both neutrals would turn from them.
    expect(sideHeld(g, 'USA')).toBe(-1);
    expect(q.resentHeading).toBe('Who will resent it');
    expect(q.resentNote).toBe('Each moves away from the Allies in public opinion for as long as it stays shut.');
    expect(q.resent.map((r) => [r.nat, r.so, r.most, r.line])).toEqual([
      ['DOM', 0, 8, '1 point of opinion a month, 8 at most'],
      ['PER', 0, 8, '1 point of opinion a month, 8 at most'],
    ]);
    // Asking changes nothing.
    const before = saveGame(g);
    gateQuestion(g, p);
    expect(saveGame(g).replace('"dirty":true', '"dirty":false')).toBe(before.replace('"dirty":true', '"dirty":false'));
  });

  it('a point a month away from his side while it stays shut, eight at most, and no further once it is open', () => {
    const g = seat('USA');
    canalTrade(g);
    const still = loadGame(saveGame(g))!;
    const p = gateOf(g, /Panama/);
    expect(orderGate(g, p, true)).toBe(true);
    expect(soreList(g, p).map((x) => [x.nat, x.so]).sort()).toEqual([['DOM', 0], ['PER', 0]]);
    // Month by month against the same game with the gate left open: the gap is the points moved, less what drifts home.
    const pull = 0.08;
    let want = 0;
    for (let m = 1; m <= 10; m++) {
      nextMonth(g);
      nextMonth(still);
      want = want * (1 - pull) + (m <= RELATIONS.gate.most ? RELATIONS.gate.month : 0);
      for (const id of ['DOM', 'PER']) expect(g.nations[id].opinion - still.nations[id].opinion, `${id} after ${m} months`).toBeCloseTo(want, 6);
      // Nobody else is moved by it.
      expect(g.nations.MEX.opinion).toBeCloseTo(still.nations.MEX.opinion, 9);
      expect(soreList(g, p).map((x) => x.so)).toEqual([Math.min(m, 8), Math.min(m, 8)]);
      if (m === 3) {
        const q = gateQuestion(g, p)!;
        expect(q.resentHeading).toBe('Who resents it');
        expect(q.resentNote).toBe('Each has moved away from the Allies in public opinion since you shut it. Open, they move no further.');
        expect(q.resent.map((r) => r.line)).toEqual(['3 of 8 points so far', '3 of 8 points so far']);
        // The save keeps it.
        expect(soreList(loadGame(saveGame(g))!, p)).toEqual(soreList(g, p));
      }
    }
    // Open again: nobody resents it any longer, and a second shutting starts its own count.
    expect(orderGate(g, p, false)).toBe(true);
    expect(soreList(g, p)).toEqual([]);
    expect(Object.keys(g.flags).some((k) => k.startsWith('gatesore:'))).toBe(false);
  });

  it('a holder on neither side has no side to turn them from; a member of a bloc, or a country at war, does not resent', () => {
    const d = seat('DEN');
    expect(sideHeld(d, 'DEN')).toBe(0);
    expect(wouldResent(d, 'DEN', 'SWE')).toBe(false);
    const q = gateQuestion(d, gateOf(d, /Danish/))!;
    expect(q.resent).toEqual([]);
    expect(q.resentNote).toBe('');
    const g = seat('UK');
    expect(sideHeld(g, 'UK')).toBe(-1);
    expect(wouldResent(g, 'UK', 'IRL')).toBe(true);
    // France is Britain's ally and Germany its enemy: neither is a neutral to be turned.
    expect(wouldResent(g, 'UK', 'FRA')).toBe(false);
    expect(wouldResent(g, 'UK', 'GER')).toBe(false);
    expect(wouldResent(g, 'UK', 'UK')).toBe(false);
    // Gibraltar shut: Ireland and Greece, whose agreement it stops, move toward the Axis. (Their agreement by sea is
    // made here: since Iran's oil came on the map (the realism audit, item 14) Greece's first deal is with Iran,
    // overland, and Ireland's with Latvia.)
    endTrade(g, 'IRN', 'GRE');
    expect(signTrade(g, 'IRL', 'GRE', new Map(), true)?.seas).toContain(13);
    const gib = gateOf(g, /Gibraltar/);
    expect(gateQuestion(g, gib)!.resent.map((r) => r.nat).sort()).toEqual(['GRE', 'IRL']);
    orderGate(g, gib, true);
    const was = { IRL: g.nations.IRL.opinion, GRE: g.nations.GRE.opinion };
    const still = loadGame(saveGame(g))!;
    orderGate(still, gib, false);
    nextMonth(g);
    nextMonth(still);
    expect(g.nations.IRL.opinion - still.nations.IRL.opinion).toBeCloseTo(1, 6);
    expect(g.nations.GRE.opinion - still.nations.GRE.opinion).toBeCloseTo(1, 6);
    expect(was.IRL).toBeDefined();
    // The gate changes hands: the order ends, and with it the resentment.
    const rock = g.regions[g.gates[gib].region];
    for (const c of [...rock.units]) relocate(g, c, g.byName.London);
    rock.ctrl = 'GER';
    nextMonth(g);
    expect(soreList(g, gib)).toEqual([]);
  });
});
