// Britain's expedition to Greece in the grand campaign (playtest war 17, release 2): Creforce and W Force stood held
// in London from month 16, and W Force's 1st Armoured Brigade was counted twice, in Greece and in the 2nd Armoured
// Division in Egypt. Three mends: the guest rule in ai.ts wantsOut, a corps lifted out of its theatre loses its hold,
// and the W Force row's 'takes' leaves the 2nd Armoured the weaker by its brigade.
import { describe, expect, it } from 'vitest';
import { addCorps, aidOf, capture, createGame, declareWar, evacuate, startPhase } from '../src/campaign/game';
import { wantsOut } from '../src/campaign/ai';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import setup from '../data/campaign/setup-1939.json';
import type { Corps, Game } from '../src/campaign/types';

const W = 'W Force (New Zealand Division and 1st Armoured Brigade)';
const rows = (setup as any).expeditions as any[];
const row = (name: string): any[] => rows.find((r) => r[2] === name)!;

/** A grand campaign of late 1940: France has fallen, and Britain and Greece are at war with Italy. */
function greekWar(seed = 1): Game {
  const g = startOpen(createGame(seed, null));
  setStop(g, 1, true);
  g.turn = 16;
  declareWar(g, 'GER', 'UK');
  declareWar(g, 'ITA', 'UK');
  declareWar(g, 'ITA', 'GRE');
  g.nations.FRA.alive = false;
  return g;
}

const supplied = (c: Corps): Corps => { c.supplied = true; c.org = 1; return c; };

describe('the expedition to Greece stays to fight beside Greece', () => {
  it('the rows sent to fight beside Greece say so, and nobody else is sent to fight beside a third country', () => {
    const g = createGame(1, null);
    expect(aidOf(addCorps(g, 'UK', 'Creforce', 'inf', 1, 'Crete', {}))).toEqual(['GRE', 'ITA']);
    expect(aidOf(addCorps(g, 'UK', W, 'inf', 2, 'Thessaly', {}))).toEqual(['GRE', 'ITA']);
    expect(aidOf(addCorps(g, 'UK', '2nd Armoured Division', 'arm', 1, 'Alexandria', {}))).toEqual(['ITA']);
    const third = rows.filter((r) => Array.isArray(r[6]?.war) && !r[6].war.includes(r[1]) && !r[6].war.includes('*') && ['UK', 'USA', 'FRA'].includes(r[1]));
    expect(third.map((r) => r[2])).toEqual(['Creforce', W]);
  });

  it('with France fallen, Creforce holds Crete and W Force stays in Greece (they sailed home the month they landed)', () => {
    const g = greekWar();
    const creforce = supplied(addCorps(g, 'UK', 'Creforce', 'inf', 1, 'Crete', { hold: 1 }));
    const wforce = supplied(addCorps(g, 'UK', W, 'inf', 2, 'Thessaly', { hold: 1 }));
    expect(wantsOut(g, creforce)).toBe(false);
    expect(wantsOut(g, wforce)).toBe(false);
    // Cut off or broken, they come off as before.
    wforce.supplied = false;
    expect(wantsOut(g, wforce)).toBe(true);
    wforce.supplied = true;
    creforce.org = 0.25;
    expect(wantsOut(g, creforce)).toBe(true);
  });

  it('when Greece falls they come off; on Crete they hold on through Greece\'s collapse', () => {
    const g = greekWar();
    const creforce = supplied(addCorps(g, 'UK', 'Creforce', 'inf', 1, 'Crete', { hold: 1 }));
    const wforce = supplied(addCorps(g, 'UK', W, 'inf', 2, 'Thessaly', { hold: 1 }));
    g.nations.GRE.resilience = 0;
    expect(wantsOut(g, wforce), 'Greece collapsing: the mainland is given up').toBe(true);
    expect(wantsOut(g, creforce), 'Crete is the Balkans\' haven').toBe(false);
    g.nations.GRE.alive = false;
    expect(wantsOut(g, wforce)).toBe(true);
  });

  it('a corps no row sent to fight beside Greece reads the biggest ally as before', () => {
    const g = greekWar();
    const other = supplied(addCorps(g, 'UK', 'Some Brigade', 'inf', 1, 'Thessaly', {}));
    expect(wantsOut(g, other)).toBe(true);
  });
});

describe('lifted out of its theatre, a corps holds nothing in place', () => {
  it('off Crete to London the hold is gone; off the mainland to Crete it is kept', () => {
    const g = greekWar();
    declareWar(g, 'GER', 'GRE');
    const creforce = addCorps(g, 'UK', 'Creforce', 'inf', 1, 'Crete', { hold: 1 });
    const wforce = addCorps(g, 'UK', W, 'inf', 2, 'Peloponnese', { hold: 1 });
    for (const nm of ['Thessaly', 'Athens']) capture(g, g.byName[nm], 'GER');
    expect(evacuate(g, wforce)).toBe(true);
    expect(g.regions[wforce.reg].name).toBe('Crete');
    expect(wforce.hold).toBe(true);
    expect(evacuate(g, creforce)).toBe(true);
    expect(creforce.reg).toBe(g.nations.UK.capital);
    expect(creforce.hold).toBe(false);
  });
});

describe('W Force takes its 1st Armoured Brigade out of the 2nd Armoured Division (grand campaign)', () => {
  it('is the row\'s: half of the 2nd Armoured, with a line for the player', () => {
    expect(row(W)[6].takes).toMatchObject({ name: '2nd Armoured Division', share: 0.5 });
    expect(row('2nd Armoured Division').slice(0, 6)).toEqual([17, 'UK', '2nd Armoured Division', 'arm', 1, 'Alexandria']);
  });

  /** The grand campaign at month 17, Britain at war with Italy: the 2nd Armoured lands in Egypt. */
  function landed(human: string | null): { g: Game; arm: Corps } {
    const g = startOpen(createGame(3, human));
    setStop(g, 3, true);
    g.turn = 17;
    declareWar(g, 'ITA', 'UK');
    startPhase(g, 'UK');
    const arm = g.corps.find((c) => c.nat === 'UK' && c.name === '2nd Armoured Division' && !c.dead)!;
    return { g, arm };
  }

  it('when W Force lands the 2nd Armoured is left with half, and a player is told where the brigade went', () => {
    for (const human of [null, 'UK']) {
      const { g, arm } = landed(human);
      expect(arm, 'the 2nd Armoured lands in Egypt first').toBeDefined();
      arm.str = 1;
      const from = g.regions[arm.reg].name;
      g.turn = 19;
      declareWar(g, 'ITA', 'GRE');
      const seen = g.headlines.length;
      startPhase(g, 'UK');
      expect(g.corps.some((c) => c.name === W && !c.dead), 'W Force lands').toBe(true);
      // (Half goes, and the month's replacements then refill a little of it.)
      expect(arm.str).toBeLessThan(0.7);
      const told = g.headlines.slice(seen).filter((h) => /1st Armoured Brigade sails/.test(h.text)).map((h) => h.text);
      expect(told).toEqual(human ? [`The 1st Armoured Brigade sails from ${from} for Greece with W Force. The 2nd Armoured Division stays in the desert with its other brigade.`] : []);
    }
  });

  it('without W Force the 2nd Armoured keeps both brigades', () => {
    const { g, arm } = landed(null);
    arm.str = 1;
    g.turn = 19;
    startPhase(g, 'UK');
    expect(g.corps.some((c) => c.name === W)).toBe(false);
    expect(arm.str).toBeGreaterThan(0.95);
  });

  it('a 2nd Armoured that lands after W Force comes the weaker by the brigade', () => {
    const g = startOpen(createGame(3, null));
    setStop(g, 3, true);
    g.turn = 19;
    // (Greece's war brings Britain in against Italy at once, and the rows land in their order: the 2nd Armoured's row is
    // held back a month here to see the other order.)
    const late = row('2nd Armoured Division');
    const due = late[0];
    late[0] = 20;
    let arm: Corps | undefined;
    try {
      declareWar(g, 'ITA', 'GRE');
      startPhase(g, 'UK');
      expect(g.corps.some((c) => c.name === W && !c.dead), 'W Force lands').toBe(true);
      expect(g.corps.some((c) => c.name === '2nd Armoured Division')).toBe(false);
      g.turn = 20;
      startPhase(g, 'UK');
      arm = g.corps.find((c) => c.nat === 'UK' && c.name === '2nd Armoured Division' && !c.dead);
    } finally { late[0] = due; }
    expect(arm, 'the 2nd Armoured lands').toBeDefined();
    expect(arm!.str).toBeLessThan(0.7);
  });

  it('history\'s campaigns keep the order of battle they were measured with', () => {
    const g = createGame(3, null);
    g.turn = 17;
    declareWar(g, 'ITA', 'UK');
    startPhase(g, 'UK');
    const arm = g.corps.find((c) => c.nat === 'UK' && c.name === '2nd Armoured Division' && !c.dead);
    if (!arm) return;
    const before = arm.str;
    g.turn = 19;
    declareWar(g, 'ITA', 'GRE');
    startPhase(g, 'UK');
    expect(arm.str).toBe(before);
  });
});
