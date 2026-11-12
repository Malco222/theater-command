// The eve of war. Germany moves first each month, and on history's plan Poland is beaten inside the first one,
// so a player who took Poland used to watch it fall without giving an order. Now a country that is attacked
// before its first month comes round places its army first: marches and rail moves on its own ground, no more.
import { describe, expect, it } from 'vitest';
import { createGame, friendlyUnits, men } from '../src/campaign/game';
import {
  beginEve, current, declarable, endEve, eveAttacker, humanUp, landTargets, onEve, orderAttack, orderMove, orderRail, reach, stepAI,
} from '../src/campaign/turn';
import { loadGame, saveGame } from '../src/campaign/save';
import { STACK, type Corps, type Game } from '../src/campaign/types';

const corps = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name)!;

/** Lets the computer play until the player's first month comes round, or the player's country has fallen. */
function toFirstMonth(g: Game): void {
  for (let guard = 0; !humanUp(g) && g.nations[g.human!].alive && guard < 80; guard++) stepAI(g);
}

/** Draws the whole Polish army back by rail onto the places named, strongest corps first, five to a place. */
function drawBack(g: Game, places: string[]): number {
  const army = g.corps.filter((c) => c.nat === 'POL' && c.kind !== 'fort').sort((a, b) => men(b) - men(a));
  const want = new Map(army.map((c, k) => [c.id, g.byName[places[Math.min(places.length - 1, Math.floor(k / STACK))]]]));
  // Several passes, because a corps can only go where there is room and some have to leave first.
  for (let pass = 0; pass < 6; pass++) for (const c of army) if (c.reg !== want.get(c.id)!.id) orderRail(g, c, want.get(c.id)!.id);
  return army.filter((c) => c.reg !== want.get(c.id)!.id).length;
}

describe('the eve of war', () => {
  it('a country attacked before its first month places its army first', () => {
    const g = createGame(1, 'POL');
    const pp = g.nations.POL.pp, news = g.headlines.length, dice = g.rng.state;
    expect(eveAttacker(g)).toBe('GER');
    expect(beginEve(g)).toBe(true);
    expect(onEve(g)).toBe(true);
    expect(current(g)).toBe('POL');
    expect(humanUp(g)).toBe(true);
    // No month has passed: nothing produced, nothing reported, no dice thrown, and nobody else has moved.
    expect(g.turn).toBe(1);
    expect(g.nations.POL.pp).toBe(pp);
    expect(g.headlines.length).toBe(news);
    expect(g.rng.state).toBe(dice);
    expect(g.byName['Warsaw'].ctrl).toBe('POL');
  });

  it('nobody else gets one', () => {
    // Germany moves first; France and Britain are not attacked in September; a watcher has no army to place.
    for (const who of ['GER', 'FRA', 'UK', null]) {
      const g = createGame(1, who);
      expect(eveAttacker(g), `${who}`).toBeNull();
      expect(beginEve(g), `${who}`).toBe(false);
      expect(onEve(g)).toBe(false);
      expect(g.phase).toBe(0);
    }
  });

  it('allows marches and rail moves on friendly ground, and nothing across the frontier', () => {
    const g = createGame(1, 'POL');
    beginEve(g);
    const poznan = corps(g, 'Army Poznan'), lodz = corps(g, 'Army Lodz');
    // Army Poznan stands on the German frontier, and Germany and Poland are already at war.
    const german = g.regions[poznan.reg].nb.map((j) => g.regions[j]).filter((r) => r.ctrl === 'GER');
    expect(german.length).toBeGreaterThan(0);
    const can = reach(g, poznan);
    expect([...can.keys()].every((id) => g.regions[id].ctrl === 'POL')).toBe(true);
    expect([...can.values()].some((x) => x.attack)).toBe(false);
    expect(orderAttack(g, [poznan], german[0])).toBeNull();
    expect(g.reports.length).toBe(0);
    expect(landTargets(g, [poznan])).toEqual([]);
    expect(declarable(g)).toEqual([]);
    // It can march to the next region, or go anywhere in Poland by rail.
    expect(orderMove(g, lodz, g.byName['Warsaw'].id)).toBeGreaterThan(0);
    expect(orderRail(g, poznan, g.byName['Lublin'].id)).toBe(true);
    expect(g.regions[poznan.reg].name).toBe('Lublin');
    // A move made is a move spent: the same corps cannot entrain twice.
    expect(orderRail(g, poznan, g.byName['Warsaw'].id)).toBe(false);
  });

  it('ends with the month starting from the top, and cannot be had twice', () => {
    const g = createGame(1, 'POL');
    beginEve(g);
    endEve(g);
    expect(onEve(g)).toBe(false);
    expect(g.turn).toBe(1);
    expect(current(g)).toBe('GER');
    expect(humanUp(g)).toBe(false);
    expect(beginEve(g)).toBe(false);
  });

  it('survives a save', () => {
    const g = createGame(1, 'POL');
    beginEve(g);
    const back = loadGame(saveGame(g))!;
    expect(onEve(back)).toBe(true);
    expect(current(back)).toBe('POL');
    expect(humanUp(back)).toBe(true);
  });

  it('changes nothing if no order is given', () => {
    const plain = createGame(3, 'POL'), eve = createGame(3, 'POL');
    beginEve(eve);
    endEve(eve);
    toFirstMonth(plain);
    toFirstMonth(eve);
    expect(eve.rng.state).toBe(plain.rng.state);
    expect(eve.regions.map((r) => r.ctrl)).toEqual(plain.regions.map((r) => r.ctrl));
    expect(eve.nations.POL.alive).toBe(plain.nations.POL.alive);
    expect(eve.reports.length).toBe(plain.reports.length);
  });

  it('is worth having: the army history strung along the frontier is beaten in September, one drawn back round Warsaw is not', () => {
    for (const seed of [1, 2, 3, 4, 5, 6]) {
      const forward = createGame(seed, 'POL');
      beginEve(forward);
      endEve(forward);
      toFirstMonth(forward);
      expect(forward.nations.POL.alive, `seed ${seed}, forward`).toBe(false);

      const back = createGame(seed, 'POL');
      beginEve(back);
      expect(drawBack(back, ['Warsaw', 'Lodz', 'Kielce', 'Lublin'])).toBe(0);
      expect(friendlyUnits(back, back.byName['Warsaw'], 'POL').length).toBe(STACK);
      endEve(back);
      toFirstMonth(back);
      expect(back.nations.POL.alive, `seed ${seed}, drawn back`).toBe(true);
      expect(back.byName['Warsaw'].ctrl).toBe('POL');
      expect(back.turn).toBe(1);
      expect(current(back)).toBe('POL');
    }
  });
});
