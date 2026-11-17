// Dead buttons (2026-11-17): a press either works or says its true reason. The page's buttons are checked on the
// built page (ui-dead.cjs); here are the rules' own sentences that the page shows, each held to the rule it speaks for.
import { describe, expect, it } from 'vitest';
import { offers } from '../src/campaign/arsenal';
import { orderCorps } from '../src/campaign/economy';
import { createGame, evacuate, whyNotEvacuate } from '../src/campaign/game';
import { closedTo } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

/** A grand campaign with the player's first month open, as the game's own screen would have it. */
function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** Sets a corps down in a province, for a test: no march, no cost. */
function place(g: Game, c: Corps, to: Region): void {
  const r = g.regions[c.reg];
  r.units.splice(r.units.indexOf(c), 1);
  c.reg = to.id;
  to.units.push(c);
}
/** The same game again, so that an order can be tried without spending it. */
const copy = (g: Game): Game => loadGame(saveGame(g))!;

describe('Evacuate by sea says why not', () => {
  /** Britain in September 1939 with a British corps set down in a French port. */
  function atFrenchPort(): { g: Game; c: Corps; port: Region } {
    const g = playing('UK', 3);
    const c = g.corps.find((x) => x.nat === 'UK' && x.kind === 'inf' && !x.dead)!;
    const port = g.regions.find((r) => r.port && r.owner === 'FRA' && r.ctrl === 'FRA' && r.seas.length > 0 && !r.units.some((u) => u.nat !== 'FRA'))!;
    place(g, c, port);
    return { g, c, port };
  }

  it('has no reason where the corps can be lifted, and the lift goes', () => {
    const { g, c } = atFrenchPort();
    expect(whyNotEvacuate(g, c)).toBeNull();
    const h = copy(g);
    expect(evacuate(h, h.corps.find((x) => x.id === c.id)!)).toBe(true);
  });

  it('names the closed sea and both strengths when the enemy holds every sea at the port', () => {
    const { g, c, port } = atFrenchPort();
    const ger = g.fleets.filter((f) => f.nat === 'GER');
    for (const z of port.seas) { const f = ger.shift()!; f.zone = z; f.stance = 'patrol'; f.ships.bb += 20; f.ships.dd += 40; }
    expect(port.seas.every((z) => closedTo(g, z, 'UK'))).toBe(true);
    const why = whyNotEvacuate(g, c)!;
    expect(why).toMatch(/is closed to your troopships: the enemy has \d+ there to your \d+, and \d+ would open it\./);
    expect(port.seas.some((z) => why.includes(g.seas[z].name))).toBe(true);
    const h = copy(g);
    expect(evacuate(h, h.corps.find((x) => x.id === c.id)!)).toBe(false);
  });

  it('says home ground is home ground', () => {
    const g = playing('UK', 3);
    const c = g.corps.find((x) => x.nat === 'UK' && x.kind === 'inf' && !x.dead)!;
    const home = g.regions.find((r) => r.port && r.owner === 'UK' && r.ctrl === 'UK')!;
    place(g, c, home);
    expect(whyNotEvacuate(g, c)).toBe(`${home.name} is home ground: there is nowhere to be lifted off to.`);
    expect(evacuate(g, c)).toBe(false);
  });

  it('agrees with the rule at every port of the map, for a great power and for one with no ships', () => {
    for (const nat of ['UK', 'GER', 'POL']) {
      const g = playing(nat, 3);
      const c = g.corps.find((x) => x.nat === nat && x.kind === 'inf' && !x.dead)!;
      const ports = g.regions.filter((r) => r.port).filter((_, i) => i % 6 === 0).concat(g.regions.filter((r) => !r.port).slice(0, 2));
      for (const r of ports) {
        place(g, c, r);
        const h = copy(g);
        expect(evacuate(h, h.corps.find((x) => x.id === c.id)!), `${nat} at ${r.name}`).toBe(whyNotEvacuate(g, c) === null);
      }
    }
  });
});

describe('Raise a lost corps again says why not', () => {
  /** Germany in September 1939 with one infantry corps on the memorial wall and a full treasury. */
  function withFallen(): { g: Game; id: number; cap: number } {
    const g = playing('GER', 7);
    const cap = g.nations.GER.capital;
    const id = g.stories.fallen.length + 1;
    g.stories.fallen.push({ id, nat: 'GER', name: 'Test Korps', kind: 'inf', divs: 3, turn: 1, reg: cap, how: 'destroyed', by: 'POL', born: 1, home: -1, xp: 0 } as Game['stories']['fallen'][number]);
    g.nations.GER.pp = 9999;
    return { g, id, cap };
  }
  const offer = (g: Game, cap: number) => offers(g, 'GER', cap).find((o) => o.key === 'corps:inf')!;

  it('the arsenal gives no reason and the order is taken while the reserve has the men', () => {
    const { g, id, cap } = withFallen();
    expect(offer(g, cap).why).toBeNull();
    expect(orderCorps(g, 'GER', 'inf', cap, id)?.revive).toBe(id);
  });

  it('the arsenal names the reserve of men when it is spent, and the order is refused', () => {
    const { g, id, cap } = withFallen();
    g.flags['mp:GER'] = 0;
    expect(offer(g, cap).why).toMatch(/^The reserve has not the men/);
    expect(orderCorps(g, 'GER', 'inf', cap, id)).toBeNull();
  });

  it('the arsenal names the treasury when the money is short', () => {
    const { g, id, cap } = withFallen();
    g.nations.GER.pp = 0;
    expect(offer(g, cap).poor).toBe(true);
    expect(orderCorps(g, 'GER', 'inf', cap, id)).toBeNull();
  });
});
