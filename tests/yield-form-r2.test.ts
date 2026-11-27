// The yield form (release 2; the design director's 05:30Z, 06:41Z, 07:20Z and 07:28Z of 22 November 2026, on the
// historian's historian/capitulations-1939-45.md): a country yields when its field army is beaten and no ally's relief
// is in reach, beside release 1's fall on the score. At stops 1-2 a computer-run country yields on the record's row,
// keyed to the first blow of the enemy it yields to. The player's country is warned a month before. Switched by
// stage.json 'yieldRule' (TC_YIELD): off, the game is release 1's.
import { describe, expect, it } from 'vitest';
import cap from '../data/campaign/capitulations.json';
import history from '../data/campaign/history.json';
import nations from '../data/campaign/nations.json';
import stage from '../data/campaign/stage.json';
import { reliefBar } from '../src/campaign/ai';
import { fallWarning } from '../src/campaign/danger';
import { addCorps, allied, annexMonth, capitulate, createGame, declareWar, frontage, homeLost, homeOf, overseasRead, railReachFor, removeCorps, victorOf } from '../src/campaign/game';
import { cutRail } from '../src/campaign/rails';
import { crossingOf } from '../src/campaign/rivers';
import { setStop, stopOf } from '../src/campaign/history';
import { loadGame, saveGame } from '../src/campaign/save';
import { joinTown } from '../src/campaign/siege';
import { FIT_ORG, startOpen } from '../src/campaign/staff';
import { YIELD_STAGE } from '../src/campaign/stage';
import { runTurnAI } from '../src/campaign/turn';
import { YIELD_ROWS, fieldArmy, fieldBeaten, ownRelief, reliefInReach, yieldBand, yieldCause, yieldMonth, yieldRule } from '../src/campaign/yield';
import type { Corps, Game, Kind, Region } from '../src/campaign/types';

/** A 1939 grand campaign with the yield form on (or off), at a stop of the dial, the computer or a player in a seat. */
function open(on = true, seed = 1, human: string | null = null, stop = 3): Game {
  const g = createGame(seed, human);
  if (on) g.flags.yieldRule = 1;
  startOpen(g);
  expect(setStop(g, stop, true)).toBe(true);
  return g;
}
const R = (g: Game, name: string): Region => g.regions.find((r) => r.name === name && !r.overseas)!;
const add = (g: Game, nat: string, where: string, divs: number, kind: Kind = 'inf'): Corps => addCorps(g, nat, `${nat} test corps at ${where}`, kind, divs, R(g, where).id, {});
/** Takes every corps of the country and of every one of its allies off the map, wherever it stands. */
function strip(g: Game, nat: string): void {
  const side = Object.keys(g.nations).filter((id) => allied(g, id, nat));
  for (const c of g.corps.filter((c) => !c.dead && side.includes(c.nat))) removeCorps(g, c);
}
/** The enemy holds the province, with an army in its field. */
function take(g: Game, by: string, where: string, divs = 2): void {
  const r = R(g, where);
  for (const c of [...r.units]) if (!allied(g, c.nat, by)) removeCorps(g, c);
  r.ctrl = by;
  add(g, by, where, divs);
}
/** A corps of the country put into the town of a province (its garrison). */
function garrison(g: Game, nat: string, where: string, divs: number): Corps {
  const r = R(g, where);
  const c = add(g, nat, where, divs);
  if (c.town === undefined) {
    const i = r.units.indexOf(c);
    expect(i).toBeGreaterThanOrEqual(0);
    r.units.splice(i, 1);
    joinTown(g, r, c);
  }
  return c;
}
/**
 * The Netherlands as the record has it in May 1940, or worse: the Germans hold the rest of the country and stand round
 * Amsterdam, which holds out; the Dutch field army is gone and no ally has a corps anywhere.
 */
function beatenNED(g: Game, ring = 12): Region {
  declareWar(g, 'GER', 'NED');
  strip(g, 'NED');
  for (const p of ['Rotterdam', 'Gelderland', 'Friesland', 'Brabant']) take(g, 'GER', p);
  const holland = R(g, 'Holland');
  garrison(g, 'NED', 'Holland', 2);
  add(g, 'GER', 'Holland', ring, 'arm');
  return holland;
}

describe('the switch', () => {
  it('is off in release 1: nothing is read, and the month writes no flag of its own', () => {
    expect((stage as { yieldRule?: boolean }).yieldRule).toBe(false);
    if (YIELD_STAGE) return; // (a run with TC_YIELD set)
    const g = startOpen(createGame(1, null));
    expect(g.flags.yieldRule).toBeUndefined();
    expect(yieldRule(g)).toBe(false);
    for (let k = 0; k < 2; k++) runTurnAI(g);
    expect(Object.keys(g.flags).filter((k) => /^(blow|yield):/.test(k))).toEqual([]);
  });

  it('never reaches a history campaign, and a save keeps it', () => {
    const h = createGame(1, null);
    h.flags.yieldRule = 1;
    expect(yieldRule(h)).toBe(false);
    const g = open();
    expect(yieldRule(g)).toBe(true);
    expect(yieldRule(loadGame(saveGame(g))!)).toBe(true);
  });

  it('on, a month notes the first blows: Germany on Poland in its first month', () => {
    const g = open(true, 1, null, 3);
    const t = g.turn;
    runTurnAI(g);
    expect(g.flags['blow:POL:GER']).toBe(t);
  });
});

describe("the record's rows", () => {
  it('name real countries, real enemies, a lag of whole months and the theatres of the book', () => {
    const theatres = Object.keys((history as { theatres: Record<string, unknown> }).theatres);
    expect(YIELD_ROWS.length).toBe((cap as { rows: unknown[] }).rows.length);
    const seen = new Set<string>();
    for (const [nat, by, lag, theatre, mark] of YIELD_ROWS) {
      expect(nations[nat as keyof typeof nations], nat).toBeDefined();
      expect(nations[by as keyof typeof nations], by).toBeDefined();
      expect(Number.isInteger(lag) && lag >= 0, `${nat} ${lag}`).toBe(true);
      if (theatre !== null) expect(theatres, `${nat} ${theatre}`).toContain(theatre);
      expect(['', 'judgment']).toContain(mark);
      expect(seen.has(`${nat}:${by}`), `${nat} twice`).toBe(false);
      seen.add(`${nat}:${by}`);
    }
    // The record's lags as the historian corrected them (first blow to yield): the Low Countries and Denmark at once,
    // France and Poland in a month, Norway in two.
    const lag = (nat: string): number => YIELD_ROWS.find((r) => r[0] === nat)![2];
    expect([lag('NED'), lag('BEL'), lag('LUX'), lag('DEN'), lag('FRA'), lag('POL'), lag('NOR'), lag('GRE')]).toEqual([0, 0, 0, 0, 1, 1, 2, 0]);
  });
});

describe('the cause, stops 3-5', () => {
  it('the Netherlands: Amsterdam held, the field army gone, no relief. On it yields to Germany; off it stands', () => {
    const g = open();
    const holland = beatenNED(g);
    expect(fieldArmy(g, 'NED')).toEqual([]);
    expect(holland.garr?.some((c) => c.nat === 'NED' && !c.dead)).toBe(true);
    expect(reliefInReach(g, 'NED')).toBeNull();
    expect(yieldCause(g, 'NED')).toBe('GER');
    yieldMonth(g, 'NED');
    expect(g.nations.NED.alive).toBe(false);
    expect(g.nations.NED.fellOn?.to).toBe('GER');
    expect(g.headlines.some((h) => h.turn === g.turn && /Dutch field army is beaten/.test(h.text))).toBe(true);

    if (YIELD_STAGE) return; // (a run with TC_YIELD set has no off)
    const off = open(false);
    beatenNED(off);
    yieldMonth(off, 'NED');
    expect(off.nations.NED.alive).toBe(true);
    expect(Object.keys(off.flags).filter((k) => /^(blow|yield):/.test(k))).toEqual([]);
  });

  it('a corps of its own in supply in the field holds it up; cut off behind the ring, it does not', () => {
    const g = open();
    const holland = beatenNED(g);
    const fr = R(g, 'Friesland');
    for (const c of [...fr.units]) removeCorps(g, c);
    fr.ctrl = 'NED';
    const cut = add(g, 'NED', 'Friesland', 1);
    expect(fieldBeaten(g, 'NED')).toBe(true);
    expect(cut.supplied).toBe(false);
    // The ring lifted, a corps in the field of Holland is fed from the capital, and the country stands.
    for (const u of [...holland.units]) if (u.nat === 'GER') removeCorps(g, u);
    g.supplyDirty = true;
    add(g, 'NED', 'Holland', 1);
    expect(fieldBeaten(g, 'NED')).toBe(false);
    expect(yieldCause(g, 'NED')).toBeNull();
    yieldMonth(g, 'NED');
    expect(g.nations.NED.alive).toBe(true);
  });

  it('Belgium: in a pocket with the Allies beside it cut off it yields; with Allies in supply beside it, it holds', () => {
    // May 1940: the Germans through Belgium and at the sea by Calais and the Somme, and (cut) at Champagne behind Lille.
    const cut = open();
    declareWar(cut, 'GER', 'BEL');
    expect(allied(cut, 'BEL', 'FRA')).toBe(true);
    strip(cut, 'BEL');
    for (const p of ['Brussels', 'Antwerp', 'Flanders', 'Liege', 'Namur', 'Ardennes', 'Calais', 'Somme', 'Champagne']) take(cut, 'GER', p);
    const pocket = [add(cut, 'BEL', 'Lille', 3), add(cut, 'FRA', 'Lille', 4, 'arm'), add(cut, 'FRA', 'Lille', 3)];
    expect(fieldBeaten(cut, 'BEL')).toBe(true);
    expect(pocket.every((c) => !c.supplied)).toBe(true);
    expect(reliefInReach(cut, 'BEL')).toBeNull();
    yieldMonth(cut, 'BEL');
    expect(cut.nations.BEL.alive).toBe(false);
    expect(cut.nations.BEL.fellOn?.to).toBe('GER');

    // Its army shut in Antwerp, and a French army at Lille fed from Paris beside Brussels: in reach, and it holds.
    const fed = open();
    declareWar(fed, 'GER', 'BEL');
    strip(fed, 'BEL');
    for (const p of ['Brussels', 'Flanders', 'Liege', 'Namur', 'Ardennes', 'Calais', 'Somme']) take(fed, 'GER', p);
    garrison(fed, 'BEL', 'Antwerp', 3);
    add(fed, 'GER', 'Antwerp', 4);
    const french = [add(fed, 'FRA', 'Lille', 4, 'arm'), add(fed, 'FRA', 'Lille', 3)];
    expect(fieldBeaten(fed, 'BEL')).toBe(true);
    expect(french.every((c) => c.supplied)).toBe(true);
    const help = reliefInReach(fed, 'BEL');
    expect(help?.by).toBe('FRA');
    expect(help!.odds).toBeGreaterThanOrEqual(reliefBar(fed, fed.regions[help!.target]));
    yieldMonth(fed, 'BEL');
    expect(fed.nations.BEL.alive).toBe(true);
  });

  it('a capital with a relief in reach at its bar holds, a relief that cannot win does not, and it yields the month the relief goes', () => {
    // Belgium in the war beside the Allies, so a French army at Brabant is fed over Belgian roads.
    const g = open();
    declareWar(g, 'GER', 'BEL');
    const holland = beatenNED(g, 1);
    const bra = R(g, 'Brabant');
    for (const c of [...bra.units]) removeCorps(g, c);
    bra.ctrl = 'NED';
    // A relief beside the capital at the capital's bar (1.25) and short of 1.5: in reach only at a capital's bar, and
    // only with the garrison's sortie counted.
    const big = [add(g, 'FRA', 'Brabant', 2)];
    expect(reliefBar(g, holland)).toBe(1.25);
    const help = reliefInReach(g, 'NED');
    expect(big.every((c) => c.supplied)).toBe(true);
    expect(help).not.toBeNull();
    expect(help!.by).toBe('FRA');
    expect(help!.target).toBe(holland.id);
    expect(help!.odds).toBeGreaterThanOrEqual(1.25);
    expect(help!.odds).toBeLessThan(1.5);
    yieldMonth(g, 'NED');
    expect(g.nations.NED.alive).toBe(true);

    // A relief that cannot win is not in reach.
    for (const c of big) removeCorps(g, c);
    const weak = add(g, 'FRA', 'Brabant', 1);
    weak.str = 0.3;
    expect(reliefInReach(g, 'NED')).toBeNull();
    // And with the relief gone, the next opening it yields.
    removeCorps(g, weak);
    g.turn += 1;
    yieldMonth(g, 'NED');
    expect(g.nations.NED.alive).toBe(false);
  });

  it("a relief may come from two provinces off over an ally's ground in a month, never through a neutral's", () => {
    // Belgium at peace: Lille to the Dutch border runs through it. No road, no rail, no relief.
    const g = open();
    beatenNED(g, 2);
    expect(allied(g, 'FRA', 'NED')).toBe(true);
    add(g, 'FRA', 'Lille', 6, 'arm');
    expect(reliefInReach(g, 'NED')).toBeNull();
    // Belgium in the war on the Allied side: the same army comes up through Brussels to Antwerp, beside Brabant.
    const h = open();
    declareWar(h, 'GER', 'BEL');
    expect(allied(h, 'BEL', 'NED')).toBe(false);
    beatenNED(h, 2);
    expect(allied(h, 'BEL', 'NED')).toBe(true);
    add(h, 'FRA', 'Lille', 6, 'arm');
    const help = reliefInReach(h, 'NED');
    expect(help?.by).toBe('FRA');
    yieldMonth(h, 'NED');
    expect(h.nations.NED.alive).toBe(true);
  });

  it('Norway: Allied corps landed beside the enemy at odds hold it until they are gone', () => {
    const g = open();
    declareWar(g, 'GER', 'NOR');
    expect(allied(g, 'NOR', 'UK')).toBe(true);
    strip(g, 'NOR');
    for (const p of ['Oslo', 'Sorlandet', 'Bergen', 'Oppland', 'Trondheim']) take(g, 'GER', p, 1);
    expect(fieldBeaten(g, 'NOR')).toBe(true);
    const landed = [add(g, 'UK', 'Nordland', 4), add(g, 'UK', 'Nordland', 2, 'mtn')];
    const help = reliefInReach(g, 'NOR');
    expect(help?.by).toBe('UK');
    expect(g.regions[help!.target].name).toBe('Trondheim');
    for (let k = 0; k < 3; k++) { yieldMonth(g, 'NOR'); g.turn += 1; }
    expect(g.nations.NOR.alive).toBe(true);
    for (const c of landed) removeCorps(g, c);
    yieldMonth(g, 'NOR');
    expect(g.nations.NOR.alive).toBe(false);
    expect(g.nations.NOR.fellOn?.to).toBe('GER');
  });

  it('Norway: a British army in a home port across the North Sea is in reach by ship as far as the shipping carries it, at a landing\'s strength', () => {
    const g = open();
    declareWar(g, 'GER', 'NOR');
    strip(g, 'NOR');
    for (const p of ['Oslo', 'Sorlandet', 'Bergen', 'Oppland', 'Trondheim']) take(g, 'GER', p, 1);
    const bergen = R(g, 'Bergen');
    const quay = g.regions.find((r) => r.owner === 'UK' && r.ctrl === 'UK' && !r.overseas && r.port && r.seas.some((z) => bergen.seas.includes(z)))!;
    expect(quay).toBeDefined();
    for (let k = 0; k < 3; k++) addCorps(g, 'UK', `Expeditionary force ${k + 1}`, 'inf', 4, quay.id, {});
    // Three corps in the lift: in reach.
    g.nations.UK.sealift = 3;
    const help = reliefInReach(g, 'NOR');
    expect(help?.by).toBe('UK');
    expect(help!.corps).toHaveLength(3);
    // Shipping for one: one corps going ashore at a landing's strength cannot win against a division in the mountains.
    g.nations.UK.sealift = 1;
    expect(reliefInReach(g, 'NOR')).toBeNull();
    g.nations.UK.sealift = 0;
    expect(reliefInReach(g, 'NOR')).toBeNull();
  });

  it('a country that has been liberated stands as long as its capital does, as against the fall', () => {
    const g = open();
    beatenNED(g);
    g.nations.NED.freedOn = { turn: g.turn - 1, week: 1, by: 'UK' };
    yieldMonth(g, 'NED');
    expect(g.nations.NED.alive).toBe(true);
  });
});

describe('the relief as the computer would send it (the review, 22 November)', () => {
  /** Belgium in the war on the Allied side, the Netherlands beaten, Germans round Amsterdam and in Brabant. */
  function lowCountries(): Game {
    const g = open();
    declareWar(g, 'GER', 'BEL');
    beatenNED(g, 2);
    return g;
  }

  it('an ally\'s corps that has moved this month still comes by train: the question is next month', () => {
    const g = lowCountries();
    const f = add(g, 'FRA', 'Lille', 6, 'arm');
    f.ap = 0;
    expect(reliefInReach(g, 'NED')?.by).toBe('FRA');
    expect(f.ap).toBe(0);
  });

  it('where no train runs, a march of a month brings it, whoever\'s month it is', () => {
    const g = lowCountries();
    const f = add(g, 'FRA', 'Lille', 6, 'arm');
    expect(cutRail(g, R(g, 'Lille').id)).toBe(true);
    f.ap = 4;
    expect(railReachFor(g, [f]).size).toBe(0);
    f.ap = 0;
    expect(g.human).toBeNull();
    expect(reliefInReach(g, 'NED')?.by).toBe('FRA');
  });

  it('it is weighed from where it would stand: corps with no room beside the place do not count', () => {
    const g = lowCountries();
    const t = R(g, 'Brabant');
    // The ground of the Allies' side beside Brabant, clear of the enemy, filled with a weak Belgian army.
    const beside = t.nb.map((j) => g.regions[j]).filter((p) => allied(g, 'FRA', p.ctrl) && !p.units.some((u) => !u.dead && !allied(g, u.nat, 'FRA')));
    expect(beside.length).toBeGreaterThan(0);
    for (const p of beside) for (let k = p.units.filter((u) => allied(g, u.nat, 'FRA')).length; k < 5; k++) { const b = add(g, 'BEL', p.name, 1); b.str = 0.05; }
    const far = ['Paris', 'Calais', 'Somme', 'Rouen'].map((n) => add(g, 'FRA', n, 6, 'arm'));
    expect(reliefInReach(g, 'NED')).toBeNull();
    // Room again: the same French army is in reach.
    for (const p of beside) for (const b of p.units.filter((u) => u.nat === 'BEL')) removeCorps(g, b);
    const help = reliefInReach(g, 'NED');
    expect(help?.by).toBe('FRA');
    expect(help!.corps.every((id) => far.some((c) => c.id === id))).toBe(true);
  });

  it('and goes in by the roads from there: an army spread over France counts no more corps than one province\'s roads carry', () => {
    const g = lowCountries();
    const t = R(g, 'Brabant');
    // Liege full of a weak Belgian army: Antwerp is the one province of the Allies' side the French could stand on.
    const lie = R(g, 'Liege');
    for (let k = lie.units.filter((u) => allied(g, u.nat, 'FRA')).length; k < 5; k++) { const b = add(g, 'BEL', 'Liege', 1); b.str = 0.05; }
    const far = ['Paris', 'Calais', 'Somme', 'Rouen', 'Normandy', 'Champagne'].map((n) => add(g, 'FRA', n, 2, 'inf'));
    const help = reliefInReach(g, 'NED');
    expect(help?.by).toBe('FRA');
    expect(help!.target).toBe(t.id);
    expect(help!.corps.length).toBeLessThanOrEqual(frontage(t));
    expect(help!.corps.length).toBeLessThan(far.length);
  });

  it('a country liberated since falls again by the cause, not by the record\'s row for its first fall', () => {
    const g = open(true, 1, null, 1);
    const holland = beatenNED(g, 1);
    g.flags['blow:NED:GER'] = g.turn - 5;
    g.nations.NED.freedOn = { turn: g.turn - 3, week: 1, by: 'UK' };
    // Its capital lost again, its army gone, and a French relief at the capital's door.
    holland.ctrl = 'GER';
    declareWar(g, 'GER', 'BEL');
    const bra = R(g, 'Brabant');
    for (const c of [...bra.units]) removeCorps(g, c);
    bra.ctrl = 'NED';
    add(g, 'FRA', 'Brabant', 6, 'arm');
    expect(stopOf(g)).toBe(1);
    yieldMonth(g, 'NED');
    expect(g.nations.NED.alive).toBe(true);
  });

  it('a first blow with no battle to date it is dated by the month\'s order: an enemy that plays later came the month before', () => {
    const g = open();
    declareWar(g, 'SOV', 'HUN');
    const hu = g.regions.find((r) => r.owner === 'HUN' && !r.overseas && r.id !== g.nations.HUN.capital)!;
    take(g, 'SOV', hu.name);
    expect(g.reports.some((rep) => rep.attNat === 'SOV')).toBe(false);
    yieldMonth(g, 'HUN');
    expect(g.flags['blow:HUN:SOV']).toBe(g.turn - 1);
    const h = open();
    declareWar(h, 'GER', 'NED');
    take(h, 'GER', 'Friesland');
    yieldMonth(h, 'NED');
    expect(h.flags['blow:NED:GER']).toBe(h.turn);
  });

  // (The second review, 22 November.)
  it('counts the corps that stay where they stand before placing those that come: the armour goes where there is room', () => {
    const g = lowCountries();
    const rot = R(g, 'Rotterdam');
    for (const c of [...rot.units]) if (!c.dead && c.nat === 'GER') removeCorps(g, c);
    rot.ctrl = 'GER';
    add(g, 'GER', 'Brabant', 4);
    for (let k = 0; k < 4; k++) add(g, 'FRA', 'Antwerp', 2);
    add(g, 'FRA', 'Brussels', 4, 'arm');
    const help = reliefInReach(g, 'NED');
    expect(help?.by).toBe('FRA');
    expect(g.regions[help!.target].name).toBe('Brabant');
    expect(help!.corps).toHaveLength(5);
  });

  it('a bank across a blown river is no place to attack from: the corps is put where it can cross', () => {
    const g = open();
    declareWar(g, 'GER', 'BEL');
    beatenNED(g, 12);
    const brab = R(g, 'Brabant'), rot = R(g, 'Rotterdam');
    for (const c of [...brab.units]) if (!c.dead && c.nat === 'GER') removeCorps(g, c);
    brab.ctrl = 'NED';
    add(g, 'GER', 'Gelderland', 12, 'arm');
    for (const c of rot.units) if (c.nat === 'GER') c.str = 0.5;
    const river = crossingOf(brab.id, rot.id);
    expect(river).not.toBeNull();
    g.flags[`br:${river!.a}:${river!.b}`] = g.turn;
    const b = add(g, 'BEL', 'Antwerp', 1); b.str = 0.2;
    add(g, 'FRA', 'Brussels', 6, 'arm');
    const help = reliefInReach(g, 'NED');
    expect(help?.by).toBe('FRA');
    expect(g.regions[help!.target].name).toBe('Rotterdam');
  });

  it('guns never go ashore alone, and never take the place of a corps that fights in the shipping', () => {
    const g = open();
    declareWar(g, 'GER', 'NOR');
    strip(g, 'NOR');
    for (const p of ['Oslo', 'Sorlandet', 'Bergen', 'Oppland', 'Trondheim']) { take(g, 'GER', p, 1); for (const c of R(g, p).units) if (c.nat === 'GER') c.str = 0.15; }
    const bergen = R(g, 'Bergen');
    const quay = g.regions.find((r) => r.owner === 'UK' && r.ctrl === 'UK' && !r.overseas && r.port && r.seas.some((z) => bergen.seas.includes(z)))!;
    addCorps(g, 'UK', 'Expeditionary force', 'inf', 4, quay.id, {});
    g.nations.UK.sealift = 1;
    expect(reliefInReach(g, 'NOR')?.by).toBe('UK');
    const guns = addCorps(g, 'UK', 'Expeditionary guns', 'art', 4, quay.id, {});
    const help = reliefInReach(g, 'NOR');
    expect(help?.by).toBe('UK');
    expect(help!.corps).not.toContain(guns.id);
  });

  it('a corps more than a month\'s march away comes only by train', () => {
    const g = lowCountries();
    const f = add(g, 'FRA', 'Lyon', 6, 'arm');
    f.ap = 0;
    expect(reliefInReach(g, 'NED')?.by).toBe('FRA');
    expect(cutRail(g, R(g, 'Lyon').id)).toBe(true);
    expect(reliefInReach(g, 'NED')).toBeNull();
  });

  it('comes by ship into a harbour of its side beside the place, no more corps than the shipping carries', () => {
    const g = lowCountries();
    const london = R(g, 'London');
    const a = addCorps(g, 'UK', 'Expeditionary force 1', 'arm', 6, london.id, {});
    const b = addCorps(g, 'UK', 'Expeditionary force 2', 'arm', 6, london.id, {});
    g.nations.UK.sealift = 0;
    expect(reliefInReach(g, 'NED')).toBeNull();
    g.nations.UK.sealift = 1;
    const one = reliefInReach(g, 'NED');
    expect(one?.by).toBe('UK');
    expect(one!.corps.filter((id) => id === a.id || id === b.id)).toHaveLength(1);
    g.nations.UK.sealift = 4;
    expect(reliefInReach(g, 'NED')!.corps.filter((id) => id === a.id || id === b.id)).toHaveLength(2);
  });

  it("by sea, one sailing from each harbour, all in the month, sharing the lift in hand (the design director's 12:20Z and 12:38Z)", () => {
    const g = lowCountries();
    g.nations.UK.sealift = 2;
    const london = R(g, 'London').id;
    const a = addCorps(g, 'UK', 'Expeditionary force 1', 'inf', 3, london, {});
    const b = addCorps(g, 'UK', 'Expeditionary force 2', 'inf', 3, london, {});
    // Two corps of one harbour, in one sailing: in reach.
    const both = reliefInReach(g, 'NED');
    expect(both?.by).toBe('UK');
    expect(both!.corps).toEqual(expect.arrayContaining([a.id, b.id]));
    // One of them alone is not.
    removeCorps(g, b);
    expect(reliefInReach(g, 'NED')).toBeNull();
    // A second sailing from a second harbour in the same month, with lift for both: in reach, as the moves would make it.
    const kent = addCorps(g, 'UK', 'Expeditionary force 2', 'inf', 3, R(g, 'Kent').id, {});
    const pair = reliefInReach(g, 'NED');
    expect(pair?.by).toBe('UK');
    expect(pair!.corps).toEqual(expect.arrayContaining([a.id, kent.id]));
    // Lift for one: the second sailing cannot go, so it is out.
    g.nations.UK.sealift = 1;
    expect(reliefInReach(g, 'NED')).toBeNull();
    // The Kent corps alone, for all the lift in hand, cannot win: out.
    g.nations.UK.sealift = 4;
    removeCorps(g, a);
    expect(reliefInReach(g, 'NED')).toBeNull();
  });
});

describe("by sea, every sailing the month allows (the review of the 12:38Z form)", () => {
  /** Germany at war with Belgium and the Netherlands, the Dutch side stripped, `ger` divisions holding `at`. */
  function sea(at: string, ger: number): Game {
    const g = open();
    declareWar(g, 'GER', 'BEL');
    declareWar(g, 'GER', 'NED');
    strip(g, 'NED');
    take(g, 'GER', at, ger);
    return g;
  }
  const weak = (g: Game, nat: string, where: string, n: number): void => {
    for (let k = 0; k < n; k++) add(g, nat, where, 1).str = 0.05;
  };

  it('two harbours put in at two harbours: London to Antwerp and Kent to Holland, where one harbour each is not enough', () => {
    const g = sea('Rotterdam', 15);
    weak(g, 'BEL', 'Antwerp', 2);
    weak(g, 'NED', 'Holland', 3);
    g.nations.UK.sealift = 4;
    const a = add(g, 'UK', 'London', 10), b = add(g, 'UK', 'London', 4), c = add(g, 'UK', 'Kent', 9), d = add(g, 'UK', 'Kent', 3);
    const r = reliefInReach(g, 'NED');
    expect(r?.by).toBe('UK');
    expect(r!.corps).toEqual(expect.arrayContaining([a.id, b.id, c.id, d.id]));
  });

  it('lift goes only to corps that have room where their sailing puts in', () => {
    const g = sea('Rotterdam', 6);
    weak(g, 'BEL', 'Antwerp', 4);
    weak(g, 'NED', 'Holland', 4);
    g.nations.UK.sealift = 2;
    const a = add(g, 'UK', 'London', 10);
    add(g, 'UK', 'London', 9);
    const c = add(g, 'UK', 'Kent', 8);
    const r = reliefInReach(g, 'NED');
    expect(r?.by).toBe('UK');
    expect(r!.corps).toEqual(expect.arrayContaining([a.id, c.id]));
  });

  it('a harbour\'s corps go where all of them can attack from, though the heaviest could put in elsewhere', () => {
    const g = sea('Brabant', 3);
    const x = crossingOf(R(g, 'Holland').id, R(g, 'Brabant').id)!;
    g.flags[`br:${x.a}:${x.b}`] = g.turn;
    weak(g, 'BEL', 'Antwerp', 3);
    weak(g, 'NED', 'Rotterdam', 3);
    g.nations.UK.sealift = 4;
    const i = add(g, 'UK', 'London', 10), t = add(g, 'UK', 'London', 4, 'arm');
    const r = reliefInReach(g, 'NED');
    expect(r?.by).toBe('UK');
    expect(r!.corps).toEqual(expect.arrayContaining([i.id, t.id]));
  });

  it('a corps past the frontage gives its lift to another harbour\'s sailing', () => {
    const g = sea('Rotterdam', 6);
    g.nations.UK.sealift = 4;
    const london = [0, 1, 2, 3].map(() => add(g, 'UK', 'London', 3));
    const kent = add(g, 'UK', 'Kent', 2);
    const r = reliefInReach(g, 'NED');
    expect(r?.by).toBe('UK');
    expect(r!.corps).toContain(kent.id);
    expect(r!.corps.filter((id) => london.some((c) => c.id === id))).toHaveLength(3);
  });

  it('shipping in hand never takes away a relief by land', () => {
    const g = open();
    declareWar(g, 'GER', 'BEL');
    beatenNED(g, 2);
    const rot = R(g, 'Rotterdam'), ant = R(g, 'Antwerp');
    for (const p of ['Holland', 'Gelderland', 'Friesland', 'Brabant']) add(g, 'GER', p, 30, 'arm');
    for (const c of [...ant.units]) removeCorps(g, c);
    for (const c of rot.units) if (c.nat === 'GER') c.str = 1.2;
    const inf = [0, 1, 2].map(() => add(g, 'UK', 'Antwerp', 3));
    add(g, 'UK', 'London', 4, 'cav');
    g.nations.UK.sealift = 0;
    const byLand = reliefInReach(g, 'NED');
    expect(byLand?.corps).toEqual(expect.arrayContaining(inf.map((c) => c.id)));
    g.nations.UK.sealift = 1;
    expect(reliefInReach(g, 'NED')).toEqual(byLand);
  });
});

describe("the player's seat", () => {
  it('the band first, then the yield a month on', () => {
    const g = open(true, 1, 'NED');
    beatenNED(g);
    const t = g.turn;
    yieldMonth(g, 'NED');
    expect(g.nations.NED.alive).toBe(true);
    expect(g.flags['yield:NED']).toBe(t);
    expect(yieldBand(g, 'NED')).toBe(true);
    const w = fallWarning(g, 'NED');
    expect(w?.head).toBe('The Netherlands yields next month');
    expect(w?.text).toMatch(/field army is beaten/);
    expect(w?.note).toMatch(/corps of yours in supply/);
    // (The same month again does not bring it on.)
    yieldMonth(g, 'NED');
    expect(g.nations.NED.alive).toBe(true);
    g.turn = t + 1;
    expect(yieldBand(g, 'NED')).toBe(false);
    yieldMonth(g, 'NED');
    expect(g.nations.NED.alive).toBe(false);
    expect(g.nations.NED.fellOn?.to).toBe('GER');
  });

  it('a corps in supply in the field clears the band, and the country stands', () => {
    const g = open(true, 1, 'NED');
    beatenNED(g);
    const t = g.turn;
    yieldMonth(g, 'NED');
    expect(yieldBand(g, 'NED')).toBe(true);
    // He breaks the ring and puts a corps into the field of Holland, fed from the capital.
    const holland = R(g, 'Holland');
    for (const u of [...holland.units]) if (u.nat === 'GER') removeCorps(g, u);
    g.supplyDirty = true;
    add(g, 'NED', 'Holland', 1);
    g.turn = t + 1;
    yieldMonth(g, 'NED');
    expect(g.nations.NED.alive).toBe(true);
    expect(g.flags['yield:NED']).toBeUndefined();
    expect(fallWarning(g, 'NED')?.head ?? '').not.toMatch(/yields next month/);
  });

  it('binds his country by the cause at stops 1-2 too, never by the row', () => {
    const g = open(true, 1, 'NED', 1);
    expect(stopOf(g)).toBe(1);
    declareWar(g, 'GER', 'NED');
    take(g, 'GER', 'Gelderland');
    expect(fieldBeaten(g, 'NED')).toBe(false);
    yieldMonth(g, 'NED');
    expect(g.flags['blow:NED:GER']).toBe(g.turn);
    expect(g.nations.NED.alive).toBe(true);
    // Beaten, he is warned and then yields, as at stop 3.
    strip(g, 'NED');
    for (const p of ['Rotterdam', 'Friesland', 'Brabant']) take(g, 'GER', p);
    garrison(g, 'NED', 'Holland', 2);
    add(g, 'GER', 'Holland', 12, 'arm');
    yieldMonth(g, 'NED');
    expect(yieldBand(g, 'NED')).toBe(true);
    g.turn += 1;
    yieldMonth(g, 'NED');
    expect(g.nations.NED.alive).toBe(false);
  });
});

describe('the book, stops 1-2', () => {
  it("a computer-run country yields in the record's month after the first blow, its army still in the field", () => {
    const g = open(true, 1, null, 1);
    declareWar(g, 'GER', 'NED');
    take(g, 'GER', 'Gelderland');
    expect(victorOf(g, 'NED')).toBe('GER');
    expect(fieldBeaten(g, 'NED')).toBe(false);
    yieldMonth(g, 'NED');
    expect(g.nations.NED.alive).toBe(false);
    expect(g.nations.NED.fellOn?.to).toBe('GER');
  });

  it("France's month: one after the blow, not before", () => {
    const g = open(true, 1, null, 2);
    take(g, 'GER', 'Lorraine');
    const t = g.turn;
    yieldMonth(g, 'FRA');
    expect(g.flags['blow:FRA:GER']).toBe(t);
    expect(g.nations.FRA.alive).toBe(true);
    g.turn = t + 1;
    yieldMonth(g, 'FRA');
    expect(g.nations.FRA.alive).toBe(false);
  });

  it('no blow yet, no yield; and a blow is kept from the month it fell', () => {
    const g = open(true, 1, null, 1);
    declareWar(g, 'GER', 'NED');
    yieldMonth(g, 'NED');
    expect(g.flags['blow:NED:GER']).toBeUndefined();
    expect(g.nations.NED.alive).toBe(true);
  });

  it("a closed book lapses the row to the cause, and the cause stands while the army does", () => {
    const g = open(true, 1, null, 1);
    g.flags['hist:off:west'] = 1;
    declareWar(g, 'GER', 'NED');
    take(g, 'GER', 'Gelderland');
    yieldMonth(g, 'NED');
    expect(g.nations.NED.alive).toBe(true);
    // Beaten with no relief, it yields by the cause, at once.
    strip(g, 'NED');
    for (const p of ['Rotterdam', 'Friesland', 'Brabant']) take(g, 'GER', p);
    garrison(g, 'NED', 'Holland', 2);
    add(g, 'GER', 'Holland', 12, 'arm');
    yieldMonth(g, 'NED');
    expect(g.nations.NED.alive).toBe(false);
  });

  it('a row whose enemy is not the one the country yields to lapses to the cause', () => {
    // Hungary's row is to the Soviet Union. Struck by Germany it has no row: the blow alone does not bring it down.
    const g = open(true, 1, null, 1);
    declareWar(g, 'GER', 'HUN');
    const bud = R(g, 'Budapest');
    const rest = g.regions.filter((r) => r.owner === 'HUN' && !r.overseas && r.id !== bud.id);
    expect(rest.length).toBeGreaterThan(0);
    take(g, 'GER', rest[0].name);
    expect(victorOf(g, 'HUN')).toBe('GER');
    expect(fieldBeaten(g, 'HUN')).toBe(false);
    yieldMonth(g, 'HUN');
    expect(g.flags['blow:HUN:GER']).toBe(g.turn);
    expect(g.nations.HUN.alive).toBe(true);
    // Beaten with no relief in reach, it yields by the cause.
    strip(g, 'HUN');
    for (const r of rest) take(g, 'GER', r.name);
    yieldMonth(g, 'HUN');
    expect(g.nations.HUN.alive).toBe(false);
    expect(g.nations.HUN.fellOn?.to).toBe('GER');
  });
});

describe("what counts, and what home is (the design director's 09:05Z)", () => {
  it("counts only corps that fight, at or above the staffs' own fit line: Norway's 6th Division in Finnmark counts", () => {
    const g = open();
    declareWar(g, 'GER', 'NOR');
    const sixth = g.corps.find((c) => c.nat === 'NOR' && !c.dead && c.name === '6th Division')!;
    expect(g.regions[sixth.reg].name).toBe('Finnmark');
    expect(fieldArmy(g, 'NOR')).toContain(sixth);
    for (const c of g.corps.filter((c) => c.nat === 'NOR' && !c.dead && c !== sixth)) removeCorps(g, c);
    expect(fieldArmy(g, 'NOR')).toEqual([sixth]);
    expect(fieldBeaten(g, 'NOR')).toBe(false);
    sixth.org = FIT_ORG - 0.01;
    expect(fieldArmy(g, 'NOR')).toEqual([]);
    expect(fieldBeaten(g, 'NOR')).toBe(true);
    sixth.org = FIT_ORG;
    expect(fieldArmy(g, 'NOR')).toEqual([sixth]);
  });

  it('guns and engineers alone are no field army (ruling 146)', () => {
    const g = open();
    declareWar(g, 'GER', 'NOR');
    strip(g, 'NOR');
    const guns = add(g, 'NOR', 'Finnmark', 1, 'art');
    const sappers = add(g, 'NOR', 'Finnmark', 1, 'eng');
    expect(guns.supplied && sappers.supplied).toBe(true);
    expect(fieldArmy(g, 'NOR')).toEqual([]);
    expect(fieldBeaten(g, 'NOR')).toBe(true);
    const foot = add(g, 'NOR', 'Finnmark', 1);
    g.supplyDirty = true;
    expect(fieldArmy(g, 'NOR')).toEqual([foot]);
  });

  it("Syria and Albania read overseas with the form on, and only then; every other mark is the map's but the 11:50Z rows", () => {
    const all = (cap as unknown as { overseas: string[]; home: string[] });
    const names = all.overseas;
    expect(names.slice(0, 2)).toEqual(['Syria', 'Albania']);
    const g = open();
    for (const n of names) {
      const rs = g.regions.filter((r) => r.name === n);
      expect(rs).toHaveLength(1);
      expect(!!rs[0].overseas).toBe(false);
      expect(overseasRead(g, rs[0])).toBe(true);
    }
    expect(overseasRead(g, g.regions.find((r) => r.name === 'Lebanon')!)).toBe(true);
    expect(overseasRead(g, R(g, 'Corsica'))).toBe(false);
    for (const r of g.regions) if (!names.includes(r.name) && !all.home.includes(r.name)) expect(overseasRead(g, r)).toBe(!!r.overseas);

    if (YIELD_STAGE) return; // (a run with TC_YIELD set has no off)
    const off = open(false);
    for (const r of off.regions) expect(overseasRead(off, r)).toBe(!!r.overseas);
  });

  it('the Army of the Levant holds nothing up: France with no army at home has no field army, the corps in Syria notwithstanding', () => {
    const g = open();
    strip(g, 'FRA');
    const levant = add(g, 'FRA', 'Syria', 4);
    expect(levant.supplied).toBe(true);
    expect(fieldArmy(g, 'FRA')).toEqual([]);
    const home = add(g, 'FRA', 'Lorraine', 4);
    g.supplyDirty = true;
    expect(fieldArmy(g, 'FRA')).toEqual([home]);
  });

  it("the score reads the same map: a Greek army in Albania takes nothing of Italy's home with the form on", () => {
    const g = open();
    declareWar(g, 'ITA', 'GRE');
    take(g, 'GRE', 'Albania', 4);
    R(g, 'Albania').vp = 2; // (worth none on the map: given some, to show the score's share)
    expect(homeLost(g, 'ITA')).toBe(0);
    expect(victorOf(g, 'ITA')).not.toBe('GRE');

    if (YIELD_STAGE) return; // (a run with TC_YIELD set has no off)
    const off = open(false);
    declareWar(off, 'ITA', 'GRE');
    take(off, 'GRE', 'Albania', 4);
    R(off, 'Albania').vp = 2;
    expect(homeLost(off, 'ITA')).toBeGreaterThan(0);
    expect(victorOf(off, 'ITA')).toBe('GRE');
  });

  it('off its home, its own corps count only as a relief that could win at the bar, by the same ways as an ally\'s', () => {
    // France, its home army gone, the Germans at Sedan, its own corps over the border in Belgium.
    const g = open();
    declareWar(g, 'GER', 'BEL');
    strip(g, 'FRA');
    take(g, 'GER', 'Sedan', 2);
    expect(R(g, 'Sedan').owner).toBe('FRA');
    expect(R(g, 'Namur').owner).toBe('BEL');
    expect(fieldArmy(g, 'FRA')).toEqual([]);
    expect(ownRelief(g, 'FRA')).toBeNull();
    const weak = add(g, 'FRA', 'Namur', 1);
    weak.str = 0.3;
    g.supplyDirty = true;
    expect(weak.supplied).toBe(true);
    expect(ownRelief(g, 'FRA')).toBeNull();
    expect(fieldBeaten(g, 'FRA')).toBe(true);
    removeCorps(g, weak);
    const strong = add(g, 'FRA', 'Namur', 6, 'arm');
    g.supplyDirty = true;
    const help = ownRelief(g, 'FRA');
    expect(help?.by).toBe('FRA');
    expect(help!.corps).toContain(strong.id);
    expect(g.regions[help!.target].name).toBe('Sedan');
    expect(help!.odds).toBeGreaterThanOrEqual(reliefBar(g, g.regions[help!.target]));
    expect(fieldArmy(g, 'FRA')).toEqual([]);
    expect(fieldBeaten(g, 'FRA')).toBe(false);
  });

  it('a corps of its own off its home and cut off from supply counts for nothing, however strong', () => {
    const g = open();
    declareWar(g, 'GER', 'BEL');
    beatenNED(g, 2);
    const exiles = add(g, 'NED', 'Antwerp', 6, 'arm');
    g.supplyDirty = true;
    expect(fieldArmy(g, 'NED')).toEqual([]);
    expect(exiles.supplied).toBe(false);
    expect(ownRelief(g, 'NED')).toBeNull();
    expect(fieldBeaten(g, 'NED')).toBe(true);
  });
});

describe("the historian's marks and the dated annexations (the design director's 11:50Z)", () => {
  const capm = cap as unknown as { overseas: string[]; home: string[]; annexed: { by: string; from: number; regions: string[] }[] };
  const P = (g: Game, n: string): Region => g.byName[n];
  const WEST = ['Poznan', 'Pomorze', 'Danzig Corridor', 'Polish Silesia', 'Mlawa'];
  const EAST = ['Lwow', 'Brest', 'Bialystok', 'Polesie', 'Volhynia'];
  /** Poland fallen to Germany in the first month, the Red Army in its share (game.ts capitulate, the pact standing). */
  function fallen(on = true): Game {
    const g = open(on);
    capitulate(g, 'POL', 'GER');
    return g;
  }

  it('Bohemia, Moravia and Spanish Morocco read overseas, the Canaries and the Azores home; Libya stays overseas; Iceland and Egypt as the map has them', () => {
    expect(capm.overseas).toEqual(['Syria', 'Albania', 'Bohemia', 'Moravia', 'Spanish Morocco']);
    expect(capm.home).toEqual(['Canary Islands', 'Azores']);
    const g = open();
    for (const n of ['Bohemia', 'Moravia', 'Spanish Morocco']) {
      const r = P(g, n);
      expect(!!r.overseas).toBe(false);
      expect(overseasRead(g, r)).toBe(true);
      expect(homeOf(g, r)).toBeNull();
    }
    for (const n of ['Canary Islands', 'Azores']) {
      const r = P(g, n);
      expect(!!r.overseas).toBe(true);
      expect(overseasRead(g, r)).toBe(false);
      expect(homeOf(g, r)).toBe(r.owner);
    }
    const libya = g.regions.filter((r) => r.owner === 'ITA' && r.overseas && r.name !== 'Rhodes');
    expect(libya).toHaveLength(15);
    for (const r of libya) expect(homeOf(g, r)).toBeNull();
    for (const n of ['Iceland', 'Cairo', 'Alexandria']) expect(overseasRead(g, P(g, n))).toBe(true);
    // The score reads the same map: Germany's home by worth is the Reich's own ground without the Protectorate.
    P(g, 'Bohemia').ctrl = 'SOV';
    expect(homeLost(g, 'GER')).toBe(0);

    if (YIELD_STAGE) return; // (a run with TC_YIELD set has no off)
    const off = open(false);
    for (const r of off.regions) expect(homeOf(off, r)).toBe(r.overseas ? null : r.owner);
  });

  it('Western Poland is German home ground from October 1939 once Poland has fallen in German hands, Lodz from November, the east Soviet from November; the General Government stays Poland\'s', () => {
    const g = fallen();
    for (const n of [...WEST, 'Lodz', 'Warsaw', 'Krakow']) expect(P(g, n).ctrl).toBe('GER');
    for (const n of EAST) expect(P(g, n).ctrl).toBe('SOV');
    annexMonth(g);
    for (const n of [...WEST, ...EAST, 'Lodz']) expect(homeOf(g, P(g, n))).toBe('POL');
    g.turn = 2;
    annexMonth(g);
    for (const n of WEST) expect(homeOf(g, P(g, n))).toBe('GER');
    for (const n of [...EAST, 'Lodz', 'Warsaw', 'Krakow']) expect(homeOf(g, P(g, n))).toBe('POL');
    g.turn = 3;
    annexMonth(g);
    expect(homeOf(g, P(g, 'Lodz'))).toBe('GER');
    for (const n of EAST) expect(homeOf(g, P(g, n))).toBe('SOV');
    for (const n of ['Warsaw', 'Krakow', 'Kielce', 'Lublin']) expect(homeOf(g, P(g, n))).toBe('POL');
    // Wilno is the Red Army's in the game, and Lithuania never holds it: it stays Poland's.
    expect(P(g, 'Wilno').ctrl).toBe('SOV');
    expect(homeOf(g, P(g, 'Wilno'))).toBe('POL');
    // The score and the cause read it: the Red Army in Poznan is on Germany's home ground, and a German corps there is at home.
    expect(homeLost(g, 'GER')).toBe(0);
    declareWar(g, 'SOV', 'GER');
    P(g, 'Poznan').ctrl = 'SOV';
    expect(homeLost(g, 'GER')).toBeGreaterThan(0);
    P(g, 'Poznan').ctrl = 'GER';
    strip(g, 'GER');
    const at = add(g, 'GER', 'Poznan', 2);
    g.supplyDirty = true;
    expect(fieldArmy(g, 'GER')).toEqual([at]);
    // A Poland restored takes its own ground back.
    g.nations.POL.alive = true;
    expect(homeOf(g, P(g, 'Poznan'))).toBe('POL');
  });

  it('no annexation while Poland still fights, nor of ground the annexer does not hold; a de facto one (Alsace) moves nothing; off, nothing is written', () => {
    const fights = open();
    fights.turn = 3;
    P(fights, 'Poznan').ctrl = 'GER';
    annexMonth(fights);
    expect(homeOf(fights, P(fights, 'Poznan'))).toBe('POL');
    const g = fallen();
    P(g, 'Mlawa').ctrl = 'SOV';
    g.turn = 2;
    annexMonth(g);
    expect(homeOf(g, P(g, 'Mlawa'))).toBe('POL');
    expect(homeOf(g, P(g, 'Poznan'))).toBe('GER');
    capitulate(g, 'FRA', 'GER');
    g.turn = 12;
    annexMonth(g);
    expect(P(g, 'Alsace').ctrl).toBe('GER');
    expect(homeOf(g, P(g, 'Alsace'))).toBe('FRA');

    if (YIELD_STAGE) return; // (a run with TC_YIELD set has no off)
    const off = fallen(false);
    off.turn = 3;
    annexMonth(off);
    expect(Object.keys(off.flags).some((k) => k.startsWith('annex:'))).toBe(false);
    for (const n of [...WEST, ...EAST]) expect(homeOf(off, P(off, n))).toBe('POL');
  });
});
