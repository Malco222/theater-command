// The standing order for the day the enemy comes: stand and fight, or give ground. It is the player's alone to
// give. Troops under it slip away whole when a blow that would carry the position is about to land, once a
// month, and a war nobody plays never sees it.
import { describe, expect, it } from 'vitest';
import { DUG_IN, YIELD_ODDS, assess, createGame, limitAttackers, resolveBattle } from '../src/campaign/game';
import { beginEve, beginHuman, endEve, endHuman, humanUp, orderEvePlan, orderMove, orderRail, orderStance, postureOf, runTurnAI, stepAI } from '../src/campaign/turn';
import { loadGame, saveGame } from '../src/campaign/save';
import type { Corps, Game, Region } from '../src/campaign/types';

/** Poland on the eve of war, with the player about to place the army. */
function eve(seed = 1): Game {
  const g = createGame(seed, 'POL');
  beginEve(g);
  return g;
}

const field = (g: Game, r: Region, nat: string): Corps[] => r.units.filter((c) => c.nat === nat && c.kind !== 'fort');

/** The German corps next to a region, as many as its roads will take. */
function germansRound(g: Game, r: Region): Corps[] {
  return limitAttackers(g, r.nb.flatMap((j) => field(g, g.regions[j], 'GER')), r);
}

describe('the order to fall back', () => {
  it('is the player\'s to give, and only to the player\'s own field troops', () => {
    const g = eve();
    const pole = field(g, g.byName['Lodz'], 'POL')[0];
    const german = g.corps.find((c) => c.nat === 'GER' && c.kind !== 'fort')!;
    expect(orderStance(g, german, 'back')).toBe(false);
    expect(german.yields).toBeUndefined();
    expect(orderStance(g, pole, 'back')).toBe(true);
    expect(pole.yields).toBe(true);
    expect(orderStance(g, pole, 'stand')).toBe(true);
    expect(pole.yields).toBeUndefined();
  });

  it('lets troops slip away whole when the blow would carry the position', () => {
    const g = eve();
    const lodz = g.byName['Lodz'];
    const poles = field(g, lodz, 'POL');
    for (const c of poles) orderStance(g, c, 'back');
    endEve(g);
    const before = poles.map((c) => ({ str: c.str, org: c.org }));
    const germans = germansRound(g, lodz);
    const germanStr = germans.map((c) => c.str);
    expect(assess(g, germans, lodz).odds).toBeGreaterThanOrEqual(YIELD_ODDS);
    const battles = g.reports.length;
    const rep = resolveBattle(g, germans, lodz);
    // The ground changes hands and it is filed with the battles, but nobody fought.
    expect(rep).toMatchObject({ yielded: true, taken: true, attLost: 0, defLost: 0, prisoners: 0, regionName: 'Lodz' });
    expect(g.reports.length).toBe(battles + 1);
    expect(lodz.ctrl).toBe('GER');
    expect(rep.defenders.map((u) => u.fate)).toEqual(poles.map(() => 'yielded'));
    expect(rep.attackers.some((u) => u.fate === 'advanced')).toBe(true);
    poles.forEach((c, i) => {
      expect(c.dead).toBeFalsy();
      expect(c.reg).not.toBe(lodz.id);
      expect(g.regions[c.reg].ctrl).toBe('POL');
      expect(c.str).toBe(before[i].str);
      expect(c.org).toBeCloseTo(Math.max(0, before[i].org - 0.15), 6);
      expect(c.fought).toBeFalsy();
      expect(c.gaveWay).toBe(true);
    });
    germans.forEach((c, i) => {
      expect(c.str).toBe(germanStr[i]);
      expect(c.fought).toBeFalsy();
    });
  });

  it('is spent for the month: caught again, the same troops stand and fight', () => {
    const g = eve();
    const lodz = g.byName['Lodz'];
    const pole = field(g, lodz, 'POL')[0];
    orderStance(g, pole, 'back');
    endEve(g);
    resolveBattle(g, germansRound(g, lodz), lodz);
    const refuge = g.regions[pole.reg];
    const again = resolveBattle(g, germansRound(g, refuge), refuge);
    expect(again.yielded).toBeUndefined();
    expect(pole.fought).toBe(true);
    expect(again.defenders.some((u) => u.name === pole.name && u.fate !== 'yielded')).toBe(true);
    // The order itself stands, and is good again when the corps' own month comes round.
    expect(pole.yields).toBe(true);
    if (!pole.dead) {
      for (let guard = 0; !humanUp(g) && g.nations.POL.alive && guard < 80; guard++) stepAI(g);
      if (humanUp(g)) {
        beginHuman(g);
        expect(pole.gaveWay).toBeUndefined();
        expect(pole.yields).toBe(true);
      }
    }
  });

  it('is not obeyed before an attack too weak to carry the ground', () => {
    const g = eve();
    const lodz = g.byName['Lodz'];
    const pole = field(g, lodz, 'POL')[0];
    orderStance(g, pole, 'back');
    endEve(g);
    // The weakest corps in reach, sent in alone.
    const one = [germansRound(g, lodz).sort((x, y) => assess(g, [x], lodz).odds - assess(g, [y], lodz).odds)[0]];
    expect(assess(g, one, lodz).odds).toBeLessThan(YIELD_ODDS);
    const rep = resolveBattle(g, one, lodz);
    expect(rep.yielded).toBeUndefined();
    expect(pole.fought).toBe(true);
    expect(pole.gaveWay).toBeUndefined();
  });

  it('leaves whoever has other orders to fight alone', () => {
    const g = eve();
    const poznan = g.byName['Poznan'];
    const [stays, goes] = field(g, poznan, 'POL');
    expect(goes).toBeDefined();
    orderStance(g, goes, 'back');
    endEve(g);
    const str = goes.str;
    const rep = resolveBattle(g, germansRound(g, poznan), poznan);
    expect(rep.yielded).toBeUndefined();
    expect(goes.str).toBe(str);
    expect(goes.fought).toBeFalsy();
    expect(goes.reg).not.toBe(poznan.id);
    expect(stays.fought || stays.dead).toBeTruthy();
    // Both are on the report: one fought, the other is marked as having given ground.
    expect(rep.defenders.find((u) => u.name === goes.name)?.fate).toBe('yielded');
    expect(rep.defenders.find((u) => u.name === stays.name)?.fate).not.toBe('yielded');
  });

  it('is never obeyed in the nation\'s own capital', () => {
    const g = eve();
    const warsaw = g.byName['Warsaw'];
    const poles = field(g, warsaw, 'POL');
    for (const c of poles) orderStance(g, c, 'back');
    endEve(g);
    // Everything Germany has, brought up to the gates for the purpose of the test.
    const germans = g.corps.filter((c) => c.nat === 'GER' && c.kind !== 'fort').slice(0, 12);
    const gate = g.regions[warsaw.nb.find((j) => g.regions[j].units.some((c) => c.nat === 'GER'))!];
    for (const c of germans) { const from = g.regions[c.reg]; from.units.splice(from.units.indexOf(c), 1); gate.units.push(c); c.reg = gate.id; }
    const force = limitAttackers(g, germans, warsaw);
    expect(assess(g, force, warsaw).odds).toBeGreaterThanOrEqual(YIELD_ODDS);
    const rep = resolveBattle(g, force, warsaw);
    expect(rep.yielded).toBeUndefined();
    expect(rep.defenders.some((u) => u.fate === 'yielded')).toBe(false);
    expect(poles.every((c) => c.fought || c.dead)).toBe(true);
  });

  it('is kept in a saved game', () => {
    const g = eve();
    const pole = field(g, g.byName['Lodz'], 'POL')[0];
    orderStance(g, pole, 'back');
    const back = loadGame(saveGame(g))!;
    expect(back.corps.find((c) => c.id === pole.id)!.yields).toBe(true);
  });

  it('never appears in a war nobody plays', () => {
    const g = createGame(2, null);
    while (g.turn < 5) runTurnAI(g);
    expect(g.corps.some((c) => c.yields || c.gaveWay || c.digs || c.dug)).toBe(false);
    expect(g.reports.some((r) => r.yielded || r.defenders.some((u) => u.fate === 'yielded'))).toBe(false);
    expect(g.reports.some((r) => r.lines.some((l) => l.label === 'Dug in'))).toBe(false);
  });
});

describe('the order to dig in', () => {
  it('is one of three an army can be under, and they exclude each other', () => {
    const g = eve();
    const pole = field(g, g.byName['Lodz'], 'POL')[0];
    expect(postureOf(pole)).toBe('stand');
    orderStance(g, pole, 'dig');
    expect(postureOf(pole)).toBe('dig');
    orderStance(g, pole, 'back');
    expect(postureOf(pole)).toBe('back');
    expect(pole.digs).toBeUndefined();
    orderStance(g, pole, 'dig');
    expect(pole.yields).toBeUndefined();
    orderStance(g, pole, 'stand');
    expect(postureOf(pole)).toBe('stand');
    expect(pole.digs).toBeUndefined();
  });

  it('makes an army that stayed where it stood a quarter stronger in defence, and says so on the ticket', () => {
    const g = eve();
    const lodz = g.byName['Lodz'];
    const pole = field(g, lodz, 'POL')[0];
    orderStance(g, pole, 'dig');
    // The order alone digs nothing: it is the month spent on the spot that does.
    expect(pole.dug).toBeUndefined();
    endEve(g);
    expect(pole.dug).toBe(true);
    const germans = germansRound(g, lodz);
    const dug = assess(g, germans, lodz);
    expect(dug.lines.find((l) => l.label === 'Dug in')?.effect).toContain('25%');
    delete pole.dug;
    const plain = assess(g, germans, lodz);
    expect(plain.lines.some((l) => l.label === 'Dug in')).toBe(false);
    expect(plain.odds / dug.odds).toBeCloseTo(DUG_IN, 6);
    expect(plain.A).toBeCloseTo(dug.A, 6);
  });

  it('takes a month on the spot: troops who came by rail have no fieldworks until they have sat one out', () => {
    const g = eve();
    orderEvePlan(g);
    const garrison = field(g, g.byName['Warsaw'], 'POL');
    expect(garrison.length).toBe(5);
    for (const c of garrison) orderStance(g, c, 'dig');
    endEve(g);
    // All five came in by rail on the eve: Germany's September finds them above ground.
    expect(garrison.filter((c) => c.dug).length).toBe(0);
    for (let guard = 0; !humanUp(g) && g.nations.POL.alive && guard < 80; guard++) stepAI(g);
    expect(humanUp(g)).toBe(true);
    beginHuman(g);
    endHuman(g);
    for (const c of garrison) expect(c.dug, c.name).toBe(true);
  });

  it('leaves the fieldworks behind when the army moves, retreats or is given other orders', () => {
    const g = eve();
    const [a, b, c] = ['Lodz', 'Kielce', 'Lublin'].map((n) => field(g, g.byName[n], 'POL')[0]);
    for (const x of [a, b, c]) orderStance(g, x, 'dig');
    endEve(g);
    expect([a, b, c].map((x) => x.dug)).toEqual([true, true, true]);
    // Beaten out of its position, an army loses the fieldworks it dug there.
    const lodz = g.byName['Lodz'];
    const rep = resolveBattle(g, germansRound(g, lodz), lodz);
    expect(rep.taken).toBe(true);
    expect(a.dug).toBeUndefined();
    // So does one that marches away, or goes by rail, or is told to do something else.
    g.phase = g.order.indexOf('POL');
    beginHuman(g);
    expect(b.dug).toBe(true);
    orderStance(g, b, 'stand');
    expect(b.dug).toBeUndefined();
    expect(orderRail(g, c, g.byName['Przemysl'].id) || orderMove(g, c, g.byName['Przemysl'].id) > 0).toBe(true);
    expect(c.dug).toBeUndefined();
    // Still under orders to dig, it has to sit a month before the new fieldworks are ready.
    expect(c.digs).toBe(true);
    endHuman(g);
    expect(c.dug).toBeUndefined();
  });

  it('is kept in a saved game', () => {
    const g = eve();
    const pole = field(g, g.byName['Lodz'], 'POL')[0];
    orderStance(g, pole, 'dig');
    endEve(g);
    const back = loadGame(saveGame(g))!.corps.find((c) => c.id === pole.id)!;
    expect(back.digs).toBe(true);
    expect(back.dug).toBe(true);
  });
});
