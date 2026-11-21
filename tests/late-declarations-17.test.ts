// The historian's realism audit, item 17 (ruling 228's small rows): the late declarations of war in the Americas.
// Bolivia in April 1943, Colombia in November 1943, Ecuador, Paraguay, Peru, Uruguay and Venezuela in February 1945,
// and Argentina in March 1945, each in the form of the Central American entry: only on the wider world, only while
// Germany stands at war with the United States, and never a country that another camp holds.
import { describe, expect, it } from 'vitest';
import { DIPLOMACY, monthlyDiplomacy } from '../src/campaign/diplomacy';
import { atWar, createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const FIVE = ['ECU', 'PAR', 'PER', 'URU', 'VEN'];
function war(): Game {
  const g = startOpen(createGame(3, null));
  setWar(g, 'GER', 'UK');
  g.nations.USA.align = 'allies';
  setWar(g, 'USA', 'GER');
  return g;
}
function at(g: Game, turn: number): void {
  g.turn = turn;
  monthlyDiplomacy(g);
}

describe('the late declarations of war in the Americas', () => {
  it('are written in the form of the other entries of the wider world', () => {
    for (const id of ['bolivia-war', 'colombia-war', 'americas-war-1945', 'argentina-war']) {
      const e = DIPLOMACY.events.find((x) => x.id === id)!;
      expect(e, id).toBeDefined();
      expect(e.mode, id).toBe('open');
      expect(e.if, id).toEqual(expect.arrayContaining(['flag:world', 'alive:GER', 'war:USA,GER']));
    }
    expect(DIPLOMACY.events.find((x) => x.id === 'bolivia-war')!.from).toBe(44);
    expect(DIPLOMACY.events.find((x) => x.id === 'colombia-war')!.from).toBe(51);
    expect(DIPLOMACY.events.find((x) => x.id === 'americas-war-1945')!.from).toBe(66);
    expect(DIPLOMACY.events.find((x) => x.id === 'argentina-war')!.from).toBe(67);
  });

  it('come each in its month, and not before', () => {
    const g = war();
    at(g, 43);
    expect(atWar(g, 'BOL', 'GER')).toBe(false);
    at(g, 44);
    expect(g.nations.BOL.align).toBe('allies');
    expect(atWar(g, 'BOL', 'GER')).toBe(true);
    expect(atWar(g, 'COL', 'GER')).toBe(false);
    at(g, 51);
    expect(atWar(g, 'COL', 'GER')).toBe(true);
    for (const n of FIVE) expect(atWar(g, n, 'GER'), n).toBe(false);
    at(g, 66);
    for (const n of FIVE) {
      expect(g.nations[n].align, n).toBe('allies');
      expect(atWar(g, n, 'GER'), n).toBe(true);
    }
    expect(atWar(g, 'ARG', 'GER')).toBe(false);
    at(g, 67);
    expect(g.nations.ARG.align).toBe('allies');
    expect(atWar(g, 'ARG', 'GER')).toBe(true);
    expect(g.headlines.filter((h) => /Argentina, the last of the American republics/.test(h.text))).toHaveLength(1);
  });

  it('leave a country in the other camp where it stands, and wait on America at war with Germany', () => {
    const g = war();
    g.nations.PER.align = 'axis';
    at(g, 66);
    expect(atWar(g, 'PER', 'GER')).toBe(false);
    expect(g.nations.PER.align).toBe('axis');
    for (const n of FIVE.filter((x) => x !== 'PER')) expect(atWar(g, n, 'GER'), n).toBe(true);

    // An America that stayed out (December 1941 spent without it): Bolivia waits on it.
    const h = startOpen(createGame(3, null));
    setWar(h, 'GER', 'UK');
    h.flags['event:pearl-harbor-world'] = 28;
    at(h, 44);
    expect(atWar(h, 'USA', 'GER')).toBe(false);
    expect(atWar(h, 'BOL', 'GER')).toBe(false);
  });
});
