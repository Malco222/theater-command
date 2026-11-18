// Version 85.1: mends from the third playtester's Italy and Soviet wars on 84.3.1 (findings-v8431.md).
import { describe, expect, it } from 'vitest';
import { ARSENAL, airLine, canStation, stationSite, type AirDelivery } from '../src/campaign/arsenal';
import { airFleetsOf, airWar, airfieldsHold } from '../src/campaign/air';
import { deliver } from '../src/campaign/economy';
import { atWar, createGame, theWords } from '../src/campaign/game';
import { bridgesOnCapture, crossingsAt, farBank, riverName } from '../src/campaign/rivers';
import { runTurnAI } from '../src/campaign/turn';
import setup from '../data/campaign/setup-1939.json';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

function playing(human: string, seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}

describe("a month's aircraft from the factories in one line (finding 7)", () => {
  const d = (o: Partial<AirDelivery>): AirDelivery => ({ text: 'x.', region: 1, wing: 'ftr', count: 100, fleet: '12th Fighter Aviation Division', base: 'Moscow', new: false, war: true, ...o });

  it('keeps one wing\'s own line, and folds two or more into one, counting wings that join the same group together', () => {
    expect(airLine([])).toBeNull();
    expect(airLine([d({ text: '100 fighters from the factories join 12th Fighter Aviation Division at Moscow.' })])!.text).toBe('100 fighters from the factories join 12th Fighter Aviation Division at Moscow.');
    expect(airLine([d({}), d({}), d({ new: true, fleet: '13th Fighter Aviation Regiment' }), d({ wing: 'bmr', fleet: '2nd Bomber Corps', base: 'Ryazan', region: 7 })])).toEqual({
      text: "The month's aircraft from the factories: 200 fighters join 12th Fighter Aviation Division at Moscow; a new air group, the 13th Fighter Aviation Regiment, forms on the airfield at Moscow with 100 fighters; 100 bombers join 2nd Bomber Corps at Ryazan.",
      region: 1,
    });
  });

  it("gives the player one line for a month's wings, where each had its own", () => {
    const g = playing('SOV');
    const n = g.nations.SOV;
    const before = g.headlines.length;
    const at = g.airFleets.find((f) => f.nat === 'SOV' && f.ship === undefined)!.base;
    for (const item of ['ftr', 'ftr', 'bmr']) n.queue.push({ id: g.nextBuild++, what: 'air', item, name: 'test', region: at, ordered: g.turn - 3, eta: g.turn, cost: 0, count: 100 });
    deliver(g, 'SOV');
    const lines = g.headlines.slice(before).filter((h) => /from the factories|forms on the airfield/.test(h.text));
    expect(lines.length, lines.map((h) => h.text).join(' | ')).toBe(1);
    expect(lines[0].text).toMatch(/^The month's aircraft from the factories: /);
  });
});

describe('new aircraft go to a field with room (finding 7)', () => {
  it('stay at the main base while it has room, and go to the nearest field with room once it is full', () => {
    const g = playing('SOV');
    expect(airWar(g)).toBe(true);
    const own = airFleetsOf(g, 'SOV').filter((f) => f.ship === undefined);
    const main = own.reduce((a, b) => (b.ftr + b.bmr > a.ftr + a.bmr ? b : a));
    const based = (id: number): number => g.airFleets.reduce((s, f) => s + (f.base === id && f.ship === undefined ? f.ftr + f.bmr : 0), 0);
    const wing = Math.max(ARSENAL.air.ftr.count, ARSENAL.air.bmr.count);
    if (airfieldsHold(g, main.base) - based(main.base) < wing) main.ftr -= wing;
    expect(stationSite(g, 'SOV')).toBe(main.base);
    // The main base fills up: the next wing goes to the nearest of the nation's fields with room for it.
    main.ftr += airfieldsHold(g, main.base);
    const at = stationSite(g, 'SOV');
    expect(at).not.toBe(main.base);
    expect(canStation(g, 'SOV', g.regions[at])).toBe(true);
    expect(airfieldsHold(g, at) - based(at)).toBeGreaterThanOrEqual(wing);
    const B = g.regions[main.base], d = (id: number): number => Math.hypot(g.regions[id].x - B.x, g.regions[id].y - B.y);
    for (const r of g.regions) if (r.id !== main.base && canStation(g, 'SOV', r) && airfieldsHold(g, r.id) - based(r.id) >= wing) expect(d(r.id), r.name).toBeGreaterThanOrEqual(d(at));
    // A place the player names is kept, full or not.
    expect(stationSite(g, 'SOV', main.base)).toBe(main.base);
  });
});

describe('the words (finding 8)', () => {
  it('gives a plural place the apostrophe alone', () => {
    expect(theWords("The bombing stopped 40% of Midlands's output for a month.")).toBe("The bombing stopped 40% of the Midlands' output for a month.");
    expect(theWords("Ardennes's roads are narrow.")).toBe("The Ardennes' roads are narrow.");
    expect(theWords("Netherlands's fleet sails.")).toBe("The Netherlands' fleet sails.");
    // A single name ending in s keeps its 's.
    expect(theWords("Paris's factories.")).toBe("Paris's factories.");
  });

  it('has the American destroyers join, not joins', () => {
    const g = createGame(3, null);
    while (g.turn < 16) runTurnAI(g);
    const lines = g.headlines.filter((h) => /American destroyers/.test(h.text)).map((h) => h.text);
    expect(lines).toContain('More of the American destroyers join the Western Approaches Fleet.');
    expect(lines).toContain('The first of fifty old American destroyers join the Western Approaches Fleet.');
    expect(lines.some((t) => /destroyers joins/.test(t))).toBe(false);
  });

  it('names the two Soviet tank corps of 1939 as the record does', () => {
    const names = (setup as unknown as { corps: unknown[][] }).corps.filter((r) => r[0] === 'SOV').map((r) => r[1]);
    expect(names).toContain('15th Tank Corps');
    expect(names).toContain('25th Tank Corps');
    expect(names.some((n) => /Front tanks/.test(String(n)))).toBe(false);
  });

  it('tells the bridges over one river at one place once in a month, whatever the bridges and the captures', () => {
    const g = playing('GER');
    // A Polish province on a river with more than one bridge to Polish ground across it.
    const pick = g.regions.find((r) => r.ctrl === 'POL' && crossingsAt(r.id).filter((c) => g.regions[farBank(c, r.id)].ctrl === 'POL').length >= 2)!;
    expect(pick).toBeDefined();
    expect(atWar(g, 'GER', 'POL')).toBe(true);
    const before = g.headlines.length;
    pick.ctrl = 'GER';
    bridgesOnCapture(g, pick, 'GER');
    bridgesOnCapture(g, pick, 'GER');
    const told = g.headlines.slice(before).filter((h) => /bridges over/.test(h.text)).map((h) => h.text);
    const rivers = new Set(crossingsAt(pick.id).filter((c) => g.regions[farBank(c, pick.id)].ctrl === 'POL').map(riverName));
    expect(told.length, told.join(' | ')).toBeLessThanOrEqual(rivers.size * 2);
    expect(new Set(told).size).toBe(told.length);
  });
});
