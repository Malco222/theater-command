// Version 85.1: mends from the third playtester's Italy and Soviet wars on 84.3.1 (findings-v8431.md).
import { describe, expect, it } from 'vitest';
import { airLine, type AirDelivery } from '../src/campaign/arsenal';
import { deliver } from '../src/campaign/economy';
import { createGame } from '../src/campaign/game';
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
