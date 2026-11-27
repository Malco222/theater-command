// Beyond Europe, an army holds the ground it has taken from a country it is still at war with. Found by the Pacific
// seat builder (rider 1 to ruling 209, section 2): the computer's Japan has no ally on the continent to be its
// "host", so in September 1939 the staff that brings expeditions home lifted every corps in occupied China off the
// beaches, China walked into the empty ground, and Manchukuo fell in December. Europe keeps its host rule as it was,
// so neither the history campaigns nor the grand campaign's wars in Europe move by a single die.
import { describe, expect, it } from 'vitest';
import { wantsOut } from '../src/campaign/ai';
import { createGame, relocate, setWar } from '../src/campaign/game';
import { fieldCtrl } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { beyondEurope, worldOn } from '../src/campaign/world';
import type { Corps, Game } from '../src/campaign/types';

/** A grand campaign on the wider world as it opens. */
const wide = (seed = 1): Game => startOpen(createGame(seed, null));

/** A British corps set down, supplied and in good order, on ground Britain has taken from a country it is at war with. */
function holding(g: Game, owner: string, far: boolean): Corps {
  const r = g.regions.find((x) => x.owner === owner && beyondEurope(x) === far)!;
  const c = g.corps.find((x) => x.nat === 'UK' && !x.dead && g.regions[x.reg].owner === 'UK')!;
  setWar(g, 'UK', owner);
  c.reg = r.id;
  r.ctrl = 'UK';
  c.supplied = true;
  c.org = 1;
  return c;
}

describe('an army on ground it has taken beyond Europe', () => {
  it('holds its conquest, and comes off only when cut off or broken', () => {
    const g = wide();
    expect(worldOn(g)).toBe(true);
    const c = holding(g, 'MEX', true);
    expect(wantsOut(g, c)).toBe(false);
    // Mexico's army still stands in the field round the town Britain took: a besieged conqueror holds it too.
    expect(fieldCtrl(g, g.regions[c.reg])).toBe('MEX');
    // Shaken but not broken: it stays.
    c.org = 0.4;
    expect(wantsOut(g, c)).toBe(false);
    c.org = 0.25;
    expect(wantsOut(g, c)).toBe(true);
    c.org = 1;
    c.supplied = false;
    expect(wantsOut(g, c)).toBe(true);
  });

  it('is a guest again once the war that gave it the ground is over', () => {
    const g = wide();
    const c = holding(g, 'MEX', true);
    g.nations.UK.atWar = g.nations.UK.atWar.filter((x) => x !== 'MEX');
    g.nations.MEX.atWar = g.nations.MEX.atWar.filter((x) => x !== 'UK');
    // No land road from Mexico to Paris: the host rule sends it home, as before.
    expect(wantsOut(g, c)).toBe(true);
  });

  it('keeps its siege of a town that still holds: the field is its own though the province is not (ruling 224, b387)', () => {
    const g = wide();
    expect(g.flags.sieges).toBe(1);
    const r = g.regions.find((x) => x.owner === 'MEX' && x.ctrl === 'MEX' && beyondEurope(x) && !x.units.length)!;
    const c = g.corps.find((x) => x.nat === 'UK' && !x.dead && g.regions[x.reg].owner === 'UK')!;
    setWar(g, 'UK', 'MEX');
    relocate(g, c, r);
    c.supplied = true;
    c.org = 1;
    expect(g.regions[c.reg]).toBe(r);
    expect(r.ctrl).toBe('MEX');
    expect(fieldCtrl(g, r)).toBe('UK');
    expect(wantsOut(g, c)).toBe(false);
    // Broken, it comes off as a conqueror would.
    c.org = 0.25;
    expect(wantsOut(g, c)).toBe(true);
    // Without the siege rules the field is the province's: the host rule sends it home, as before.
    c.org = 1;
    g.flags.sieges = 0;
    expect(wantsOut(g, c)).toBe(true);
  });

  it('leaves Europe to the host rule: a shaken corps on German ground still comes off', () => {
    const g = wide();
    const c = holding(g, 'GER', false);
    c.org = 0.4;
    expect(wantsOut(g, c)).toBe(true);
  });
});

// Japan in China itself needs the Pacific's chart and Japan's rows (step 13). Until they are on the map nobody stands
// on Chinese ground on the first day, and this test waits.
const pacific = wide(7);
const inChina = (g: Game): number => g.corps.filter((c) => c.nat === 'JAP' && !c.dead && g.regions[c.reg].owner === 'CHN').length;
const FIRST_DAY = inChina(pacific);

describe('Japan in China, with nobody playing', () => {
  it.skipIf(!FIRST_DAY)('keeps its army on the ground it holds in China through the first three months', () => {
    const g = pacific;
    for (let m = 0; m < 3; m++) runTurnAI(g);
    expect(inChina(g)).toBeGreaterThanOrEqual(Math.ceil(FIRST_DAY * 0.9));
  });
});
