// Version 67: critical fixes (the grand campaign; none of it in the historical scenarios).
//
// 1. A dug-in front that sleeps. A staff weighs a main effort against the enemy on the line and close behind it, not
//    his whole army in depth; its patience counts battles in the field, so a town stormed behind the line does not
//    start the wait again.
// 2. The word of a main effort runs ahead of the blow: the month it is named the army masses, and attacks from the
//    month after.
// 3. A war nobody can fight ends: two computer nations that cannot come at each other, and whose sides have fought
//    no battle on land for a year, sign an armistice on the lines as they stand. Never a war the player is in.
// 4. Guns were the cheap way to win: an artillery corps now costs what an armoured corps costs.
import { describe, expect, it } from 'vitest';
import { runAI } from '../src/campaign/ai';
import { specOf } from '../src/campaign/economy';
import { addCorps, createGame, removeCorps, setWar } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { RIVALS, characterOf, decide, effortOf, fronts, quietInField, quietWith, staffPhase, stalemates, startOpen, type Character } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

/** A grand campaign with the player's first month open. */
function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** A grand campaign nobody plays: every nation is the computer's. */
const watching = (seed = 3): Game => startOpen(createGame(seed, null));
const fresh = (c: Corps): Corps => { c.supplied = true; c.ap = c.apMax; c.fought = false; c.org = 1; return c; };
function line(g: Game, nat: string, me = 'GER'): { ours: Region; theirs: Region } {
  for (const r of g.regions) {
    if (r.ctrl !== nat || r.id === g.nations[nat].capital) continue;
    const j = r.nb.find((k) => g.regions[k].ctrl === me && g.regions[k].units.some((u) => u.nat === me && !u.dead && u.kind !== 'fort'));
    if (j !== undefined) return { ours: r, theirs: g.regions[j] };
  }
  throw new Error(`no line between ${nat} and ${me}`);
}
/** Runs one thing with a staff's patience and standards set aside. */
function eager<T>(who: Character, run: () => T): T {
  const C = RIVALS.characters[who];
  const old = { ...C };
  Object.assign(C, { quiet: 0, strength: 0, local: 0.05 });
  try { return run(); } finally { Object.assign(C, old); }
}
/** Forgets every battle fought so far, so that a test sets the clocks itself. */
function hush(g: Game): void {
  for (const n of Object.values(g.nations)) n.lastFight = {};
  for (const k of Object.keys(g.flags)) if (k.startsWith('ff:')) delete g.flags[k];
}

describe('a dug-in front that sleeps', () => {
  it('a front is weighed against the enemy near the line as well as against his whole army in depth', () => {
    const g = playing('GER');
    let seen = 0;
    for (const nat of ['GER', 'POL', 'FRA', 'UK']) {
      for (const f of fronts(g, nat)) {
        seen++;
        // What stands on the line and one province behind it is never more than the whole.
        expect(f.defNear).toBeLessThanOrEqual(f.def + 1e-9);
        expect(f.near).toBeGreaterThanOrEqual(f.ratio - 1e-9);
      }
    }
    expect(seen).toBeGreaterThan(0);
  });

  it('an enemy held in depth reads stronger on the whole front than on the line', () => {
    const g = playing('GER');
    const { theirs } = line(g, 'POL');
    const before = fronts(g, 'POL').find((f) => f.enemies.includes('GER'))!;
    // A reserve three provinces behind the German line: it counts in the whole, not near the line.
    const edge = new Set(before.edge);
    const deep = g.regions.find((r) => r.ctrl === 'GER' && before.depth[r.id] >= 3 && !edge.has(r.id));
    expect(deep).toBeDefined();
    for (let k = 0; k < 3; k++) fresh(addCorps(g, 'GER', `Reserve ${k}`, 'inf', 3, deep!.id, {}));
    const after = fronts(g, 'POL').find((f) => f.enemies.includes('GER'))!;
    expect(after.def).toBeGreaterThan(before.def);
    expect(after.defNear).toBeCloseTo(before.defNear, 6);
    expect(after.ratio).toBeLessThan(before.ratio);
    expect(after.near).toBeCloseTo(before.near, 6);
    expect(theirs.ctrl).toBe('GER');
  });

  it('the staff\'s patience counts battles in the field: a town stormed behind the line does not start the wait again', () => {
    const g = playing('GER');
    g.turn = 40;
    hush(g);
    setWar(g, 'GER', 'SOV');
    g.flags['war:GER:SOV'] = 22;
    // The last battle in the field was eight months ago; a town was stormed last month.
    g.flags['ff:SOV:GER'] = 32;
    g.nations.GER.lastFight.SOV = 39;
    expect(quietWith(g, 'SOV', 'GER')).toBe(1);
    expect(quietInField(g, 'SOV', 'GER')).toBe(8);
    expect(quietInField(g, 'GER', 'SOV')).toBe(8);
    // A game saved before the field was counted apart reads the older clock.
    delete g.flags['ff:SOV:GER'];
    expect(quietInField(g, 'SOV', 'GER')).toBe(1);
    // And a war younger than the last field battle on record counts from its own beginning.
    g.flags['ff:SOV:GER'] = 10;
    expect(quietInField(g, 'SOV', 'GER')).toBe(18);
  });

  it('a cautious staff no longer waits longer than an opportunist one', () => {
    expect(RIVALS.characters.cautious.quiet).toBeLessThanOrEqual(RIVALS.characters.opportunist.quiet);
    expect(RIVALS.characters.bold.quiet).toBeLessThan(RIVALS.characters.cautious.quiet);
  });
});

describe('the word of a main effort runs ahead of the blow', () => {
  function poland(): Game {
    const g = playing('GER');
    setWar(g, 'GER', 'POL');
    const { ours } = line(g, 'POL');
    for (const u of [...ours.units]) if (u.kind !== 'fort') removeCorps(g, u);
    fresh(addCorps(g, 'POL', 'Test Army A', 'inf', 3, ours.id, {}));
    fresh(addCorps(g, 'POL', 'Test Army B', 'inf', 3, ours.id, {}));
    return g;
  }

  it('the month it is named the army masses and no attack is ordered; the blows fall from the month after', () => {
    const g = poland();
    eager(characterOf(g, 'POL'), () => decide(g, 'POL'));
    const e = effortOf(g, 'POL')!;
    expect(e).not.toBeNull();
    expect(e.from).toBe(g.turn);
    // A month of massing, then the months of the offensive.
    expect(e.to - e.from).toBe(RIVALS.effort.months);
    const first = staffPhase(g, 'POL')!;
    expect(first.effort).toBe(e.target);
    expect(first.minOdds).toBeGreaterThanOrEqual(50);
    expect(first.massing).toBe(true);
    expect(Object.keys(first.staging ?? {}).length).toBeGreaterThan(0);
    // The next month the same orders stand, at the odds the staff fights at.
    g.turn++;
    const next = staffPhase(g, 'POL')!;
    expect(next.effort).toBe(e.target);
    expect(next.minOdds).toBeLessThan(5);
    expect(next.massing).toBeUndefined();
    expect(next.targets).toEqual(first.targets);
  });

  it('nothing goes in during the massing month, even where the odds are overwhelming', () => {
    const g = poland();
    eager(characterOf(g, 'POL'), () => decide(g, 'POL'));
    const e = effortOf(g, 'POL')!;
    expect(e.from).toBe(g.turn);
    // One thin screen holds the place the effort is aimed at, and a whole army stands at its door.
    const at = g.regions[e.target];
    const door = at.nb.map((j) => g.regions[j]).find((r) => r.ctrl === 'POL' && r.id !== g.nations.POL.capital)!;
    for (const u of [...at.units]) if (u.kind !== 'fort') removeCorps(g, u);
    addCorps(g, e.enemy, 'Thin Screen', 'inf', 1, at.id, {});
    for (let k = 0; k < 4; k++) fresh(addCorps(g, 'POL', `Test Army ${k + 3}`, 'inf', 3, door.id, {}));
    const before = saveGame(g);
    runAI(g, 'POL');
    expect(effortOf(g, 'POL')?.from).toBe(g.turn);
    expect(g.reports.filter((r) => r.attNat === 'POL' && r.turn === g.turn)).toEqual([]);
    // The same army a month into the same effort goes in.
    const h = loadGame(before)!;
    h.flags['st:effAt:POL'] = h.turn - 1;
    runAI(h, 'POL');
    expect(h.reports.some((r) => r.attNat === 'POL' && r.turn === h.turn && r.region === at.id)).toBe(true);
  });
});

describe('a war nobody can fight', () => {
  /** Two countries at war that have never met and have no ground in touch. */
  function apart(g: Game): void {
    hush(g);
    setWar(g, 'SWE', 'TUR');
    g.flags['war:SWE:TUR'] = g.turn;
  }

  it('ends in an armistice after a year: each keeps what it holds, and it is in the news', () => {
    const g = watching();
    apart(g);
    const A = RIVALS.stalemate!.after;
    g.turn += A - 1;
    expect(stalemates(g)).toEqual([]);
    expect(g.nations.SWE.atWar).toContain('TUR');
    g.turn += 1;
    const h = g.headlines.length;
    const held = g.regions.map((r) => r.ctrl).join(',');
    expect(stalemates(g)).toEqual([['SWE', 'TUR']]);
    expect(g.nations.SWE.atWar).not.toContain('TUR');
    expect(g.nations.TUR.atWar).not.toContain('SWE');
    expect(g.flags['arm:SWE:TUR']).toBe(g.turn);
    expect(g.regions.map((r) => r.ctrl).join(',')).toBe(held);
    const text = g.headlines.slice(h).map((x) => x.text).join('\n');
    expect(text).toMatch(/Sweden and Turkey sign an armistice/);
    // Nothing is signed twice, and the peace is kept with the campaign.
    expect(stalemates(g)).toEqual([]);
    expect(loadGame(saveGame(g))!.nations.SWE.atWar).not.toContain('TUR');
  });

  it('not while their sides have fought on land within the year', () => {
    const g = watching();
    apart(g);
    g.turn += RIVALS.stalemate!.after + 6;
    g.nations.TUR.lastFight.SWE = g.turn - 3;
    expect(stalemates(g)).toEqual([]);
    g.nations.TUR.lastFight.SWE = g.turn - RIVALS.stalemate!.after;
    expect(stalemates(g)).toEqual([['SWE', 'TUR']]);
  });

  it('not where their ground touches, however long the front has slept', () => {
    const g = watching();
    hush(g);
    g.turn += 40;
    expect(g.nations.GER.atWar).toContain('FRA');
    const signed = stalemates(g);
    expect(signed.some(([a, b]) => (a === 'FRA' && b === 'GER') || (a === 'GER' && b === 'FRA'))).toBe(false);
    expect(g.nations.GER.atWar).toContain('FRA');
  });

  it('not for a country whose ally still faces the enemy: Britain does not sign while France holds the line', () => {
    const g = watching();
    hush(g);
    g.turn += 40;
    stalemates(g);
    expect(g.nations.UK.atWar).toContain('GER');
    expect(g.nations.GER.atWar).toContain('UK');
  });

  it('never a war the player is fighting in, and never in a historical scenario', () => {
    const g = playing('GER');
    apart(g);
    setWar(g, 'GER', 'SWE');
    g.turn += 40;
    // The player is at war with Sweden: Sweden's other wars are part of his war.
    expect(stalemates(g)).toEqual([]);
    expect(g.nations.SWE.atWar).toContain('TUR');
    expect(g.nations.GER.atWar).toContain('SWE');
    const h = createGame(3, null);
    setWar(h, 'SWE', 'TUR');
    h.turn += 40;
    expect(stalemates(h)).toEqual([]);
    expect(h.nations.SWE.atWar).toContain('TUR');
  });
});

describe('guns are no longer the cheap way to win', () => {
  it('an artillery corps costs what an armoured corps costs, and division for division twice the infantry', () => {
    const g = playing('GER');
    const art = specOf(g, 'art')!, arm = specOf(g, 'arm')!, inf = specOf(g, 'inf')!;
    expect(art.cost).toBeGreaterThanOrEqual(arm.cost);
    expect(art.cost / art.divs).toBeGreaterThanOrEqual(2 * (inf.cost / inf.divs));
  });

  it('the historical scenarios have no artillery to buy', () => {
    const h = createGame(3, 'GER');
    expect(specOf(h, 'art')).toBeUndefined();
  });
});
