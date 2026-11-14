// Prepared defences (the grand campaign). An army that spends a whole month where it stands, without marching or
// fighting, digs in, whoever commands it: the computer's armies as well as the player's, and with no order given.
// Its works take months to finish. Against troops who are dug in and in supply the day's luck runs against the
// attacker, so that odds which are safe in the open become a gamble; the attack costs more men and the defence
// fewer; and an attack thrown back from such a line has shot its bolt for the month. Massed guns break a line up,
// the first blows of a new offensive catch it off balance, and tanks break into one with no armour of its own.
// None of this is in the historical scenarios. The numbers are in data/campaign/defence.json.
import { describe, expect, it } from 'vitest';
import { DEFENCE, DUG_IN, FIELD_DICE, GUNS, addCorps, assess, chanceOn, createGame, endPhase, evenOdds, relocate, resolveBattle, worksShare } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { garrisonOf, joinTown } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { attackOutlook, beginHuman, humanUp, orderStance, postureOf, runTurnAI, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

const free = (seed: number, human: string | null = null): Game => startOpen(createGame(seed, human));
/** A grand campaign with the player's first month open. */
function playing(human = 'GER', seed = 3): Game {
  const g = free(seed, human);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}

interface Line { target: Region; from: Region; def: Corps[]; att: Corps[] }
/**
 * A Polish line in the country round Lodz and German infantry in front of it, all test corps, in supply and with
 * the month before them. The German offensive is an old one, so nothing catches the line off balance.
 */
function line(g: Game, defenders = 2, attackers = 2): Line {
  const target = g.byName['Lodz'];
  const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
  for (const u of [...target.units]) relocate(g, u, g.byName['Lublin']);
  const def = Array.from({ length: defenders }, (_, i) => addCorps(g, 'POL', `Line ${i + 1}`, 'inf', 3, target.id, {}));
  const att = Array.from({ length: attackers }, (_, i) => addCorps(g, 'GER', `Storm ${i + 1}`, 'inf', 3, from.id, {}));
  for (const c of [...def, ...att]) { c.supplied = true; c.ap = c.apMax; }
  g.nations.GER.offStart.POL = g.turn - 6;
  g.nations.GER.lastFight.POL = g.turn - 1;
  return { target, from, def, att };
}
/** Sets a line's works to so many months' digging. */
const works = (cs: Corps[], months: number): void => { for (const c of cs) { c.dug = true; c.dugFor = months; } };
const M = DEFENCE.entrench.months;
/** The same game again, from its save: for trying one battle two ways. */
function again(g: Game, l: Line): { g: Game; l: Line } {
  const h = loadGame(saveGame(g))!;
  const find = (c: Corps): Corps => h.corps.find((x) => x.id === c.id)!;
  return { g: h, l: { target: h.regions[l.target.id], from: h.regions[l.from.id], def: l.def.map(find), att: l.att.map(find) } };
}

describe('whoever sits a month out digs in', () => {
  it('the player\'s armies and the computer\'s alike, with no order given, and the works grow month by month', () => {
    const g = playing('GER');
    const { def, att } = line(g);
    expect(DEFENCE.entrench.auto).toBe(true);
    for (const c of [...def, ...att]) { expect(c.dug).toBeFalsy(); expect(c.dugFor).toBeUndefined(); }
    endPhase(g, 'POL');
    for (const c of def) { expect(c.dug).toBe(true); expect(c.dugFor).toBe(1); }
    for (const c of att) expect(c.dug).toBeFalsy();
    endPhase(g, 'GER');
    for (const c of att) { expect(c.dug).toBe(true); expect(c.dugFor).toBe(1); expect(c.digs).toBeUndefined(); expect(postureOf(c)).toBe('stand'); }
    for (let m = 2; m <= M + 2; m++) {
      endPhase(g, 'POL');
      for (const c of def) expect(c.dugFor).toBe(m);
    }
  });

  it('not troops inside a town, nor troops told to give ground, nor troops who marched or fought', () => {
    const g = playing('GER');
    const { def, att } = line(g, 3, 3);
    // A corps inside the walls of Warsaw: the walls are its works.
    const warsaw = g.byName['Warsaw'];
    const guard = addCorps(g, 'POL', 'Test garrison', 'inf', 2, warsaw.id, {});
    warsaw.units.splice(warsaw.units.indexOf(guard), 1);
    joinTown(g, warsaw, guard);
    guard.ap = guard.apMax;
    const inside = g.corps.filter((c) => c.nat === 'POL' && !c.dead && c.town !== undefined);
    expect(inside).toContain(guard);
    expect(garrisonOf(warsaw)).toContain(guard);
    works(def, 2);
    def[0].ap = def[0].apMax - 1;
    def[1].fought = true;
    endPhase(g, 'POL');
    // The corps that marched and the corps that fought have left their fieldworks; the third has had another month.
    for (const c of def.slice(0, 2)) { expect(c.dug).toBeUndefined(); expect(c.dugFor).toBeUndefined(); }
    expect(def[2].dugFor).toBe(3);
    for (const c of inside) expect(c.dug).toBeFalsy();
    expect(garrisonOf(g.byName['Warsaw']).every((c) => !c.dug)).toBe(true);
    // The player's order to give ground costs an army its fieldworks; an order to stand does not.
    works(att, 2);
    expect(orderStance(g, att[0], 'back')).toBe(true);
    expect(att[0].dug).toBeUndefined();
    expect(orderStance(g, att[1], 'stand')).toBe(true);
    expect(att[1].dug).toBe(true);
    endPhase(g, 'GER');
    expect(att[0].dug).toBeUndefined();
    expect(att[0].dugFor).toBeUndefined();
    expect(att[1].dugFor).toBe(3);
    expect(att[2].dugFor).toBe(3);
  });

  it('is kept in a saved game', () => {
    const g = playing('GER');
    const { def } = line(g);
    works(def, 2);
    const h = loadGame(saveGame(g))!;
    for (const c of def) expect(h.corps.find((x) => x.id === c.id)).toMatchObject({ dug: true, dugFor: 2 });
  });

  it('the historical scenarios keep the order to dig: nobody digs unasked, and luck runs as it always did', () => {
    const g = createGame(3, 'GER');
    for (const nat of ['POL', 'GER']) endPhase(g, nat);
    expect(g.corps.some((c) => c.dugFor !== undefined)).toBe(false);
    const target = g.byName['Lodz'];
    const poles = target.units.filter((c) => c.nat === 'POL');
    const germans = target.nb.flatMap((j) => g.regions[j].units.filter((c) => c.nat === 'GER' && c.kind !== 'fort')).slice(0, 3);
    expect(poles.length).toBeGreaterThan(0);
    expect(germans.length).toBeGreaterThan(0);
    for (const c of poles) { c.dug = true; c.dugFor = 9; c.supplied = true; }
    const a = assess(g, germans, target);
    expect(a.prepared).toBe(0);
    expect(a.dice).toEqual(FIELD_DICE);
    expect(a.lines.some((l) => l.label === 'Prepared defence')).toBe(false);
    expect(a.lines.some((l) => l.label === 'Dug in')).toBe(true);
  });
});

describe('a prepared line', () => {
  it('makes the day run against the attacker, by how far its works are finished', () => {
    const g = playing('GER');
    const { target, def, att } = line(g);
    const open = assess(g, att, target);
    expect(open.prepared).toBe(0);
    expect(open.dice).toEqual(FIELD_DICE);
    expect(open.lines.some((l) => l.label === 'Prepared defence')).toBe(false);
    works(def, 1);
    const young = assess(g, att, target);
    // The first month's digging does more than its share of the job (Version 61).
    const first = worksShare(1);
    expect(first).toBeGreaterThanOrEqual(1 / M);
    expect(young.prepared).toBeCloseTo(first, 6);
    expect(young.odds).toBeCloseTo(open.odds / DUG_IN, 6);
    const P = DEFENCE.prepared.dice;
    expect(young.dice[0]).toBeCloseTo(FIELD_DICE[0] + (P[0] - FIELD_DICE[0]) * first, 6);
    expect(young.dice[1]).toBeCloseTo(FIELD_DICE[1] + (P[1] - FIELD_DICE[1]) * first, 6);
    works(def, M);
    const ready = assess(g, att, target);
    expect(ready.prepared).toBeCloseTo(1, 6);
    expect(ready.dice[0]).toBeCloseTo(P[0], 6);
    expect(ready.dice[1]).toBeCloseTo(P[1], 6);
    // More months than the works need add nothing.
    works(def, M + 5);
    expect(assess(g, att, target).prepared).toBeCloseTo(1, 6);
    // One line on the Battle Ticket for the fieldworks and the luck of the day (Version 61).
    expect(ready.lines.some((l) => l.label === 'Prepared defence')).toBe(false);
    const row = ready.lines.find((l) => l.label === 'Dug in')!;
    expect(row.value).toBe('The defenders are dug in: their works are finished');
    expect(row.effect).toContain(`an even chance takes about ${evenOdds(P).toFixed(1)} to 1`);
    expect(row.effect).toContain(`nothing is sure under ${(1.2 / P[0]).toFixed(1)} to 1`);
    expect(row.effect).toContain(`The attackers lose ${Math.round((DEFENCE.prepared.losses.attacker - 1) * 100)}% more men and the defenders ${Math.round((1 - DEFENCE.prepared.losses.defender) * 100)}% fewer`);
    // The player's attack card reads the same luck.
    const look = attackOutlook(g, att, target)!;
    expect(look.even).toBeCloseTo(evenOdds(P), 6);
    if (look.exact) expect(look.chance).toBeCloseTo(chanceOn(ready.odds, P), 6);
  });

  it('must be fed, and counts for the share of the defence that is ready', () => {
    const g = playing('GER');
    const { target, def, att } = line(g);
    works(def, M);
    for (const c of def) c.supplied = false;
    const starved = assess(g, att, target);
    expect(starved.prepared).toBe(0);
    expect(starved.dice).toEqual(FIELD_DICE);
    for (const c of def) c.supplied = true;
    // One corps of the two has just come up: it has no fieldworks.
    delete def[1].dug; delete def[1].dugFor;
    const half = assess(g, att, target);
    expect(half.prepared).toBeGreaterThan(0.5);
    expect(half.prepared).toBeLessThan(0.62);
    expect(half.lines.find((l) => l.label === 'Dug in')!.value).toBe('1 of the 2 defending corps are dug in: their works are finished');
  });

  it('is broken up by massed guns, caught off balance by a new offensive, and broken into by tanks it has no armour to meet', () => {
    const g = playing('GER');
    const { target, from, def, att } = line(g);
    expect(['mountain', 'urban', 'marsh']).not.toContain(target.terrain);
    works(def, M);
    const dug = 'The defenders are dug in: their works are finished';
    const value = (cs: Corps[]): string => assess(g, cs, target).lines.find((l) => l.label === 'Dug in')!.value;
    // The first month of an offensive by an army trained to it.
    g.nations.GER.offStart.POL = g.turn;
    expect(assess(g, att, target).prepared).toBeCloseTo(1 - DEFENCE.prepared.surprise, 6);
    expect(value(att)).toBe(`${dug}, but the new offensive has caught the line off balance`);
    g.nations.GER.offStart.POL = g.turn - 6;
    expect(assess(g, att, target).prepared).toBeCloseTo(1, 6);
    // Tanks.
    const tanks = addCorps(g, 'GER', 'Test panzers', 'arm', 3, from.id, {});
    tanks.supplied = true;
    expect(assess(g, [...att, tanks], target).prepared).toBeCloseTo(1 - DEFENCE.prepared.armour, 6);
    expect(value([...att, tanks])).toBe(`${dug}, but the line has no armour to meet the tanks`);
    // With armour of its own behind the line the tanks find no opening.
    const reserve = addCorps(g, 'POL', 'Test armour', 'arm', 1, target.id, {});
    reserve.supplied = true; reserve.dug = true; reserve.dugFor = M;
    expect(assess(g, [...att, tanks], target).prepared).toBeCloseTo(1, 6);
    relocate(g, reserve, g.byName['Lublin']);
    // Guns.
    const guns = addCorps(g, 'GER', 'Test artillery', 'art', 3, from.id, {});
    guns.supplied = true;
    const soften = Math.min(GUNS.softenMost, GUNS.soften * guns.divs * guns.str);
    expect(soften).toBeGreaterThan(0);
    expect(assess(g, [...att, guns], target).prepared).toBeCloseTo(1 - soften, 6);
    expect(value([...att, guns])).toBe(`${dug}, but the guns have worked the line over`);
    // All three together.
    g.nations.GER.offStart.POL = g.turn;
    expect(assess(g, [...att, tanks, guns], target).prepared).toBeCloseTo((1 - soften) * (1 - DEFENCE.prepared.surprise) * (1 - DEFENCE.prepared.armour), 6);
    expect(value([...att, tanks, guns])).toBe(`${dug}, but the guns have worked the line over, the new offensive has caught it off balance and it has no armour to meet the tanks`);
  });

  it('turns odds that are safe in the open into a gamble', () => {
    const P = DEFENCE.prepared.dice;
    // In the open one and a half to one cannot fail. Against a finished line it carries the ground a little
    // over half the time, two to one about six times in seven, and nothing is sure under 1.2 over the worst day.
    expect(chanceOn(1.5, FIELD_DICE)).toBe(1);
    expect(chanceOn(1.19, FIELD_DICE)).toBeLessThan(0.5);
    expect(evenOdds(FIELD_DICE)).toBeCloseTo(1.2, 6);
    expect(chanceOn(1.5, P)).toBeGreaterThan(0.5);
    expect(chanceOn(1.5, P)).toBeLessThan(0.7);
    expect(chanceOn(2, P)).toBeGreaterThan(0.8);
    expect(chanceOn(2, P)).toBeLessThan(0.9);
    expect(chanceOn(1.2 / P[0], P)).toBeCloseTo(1, 6);
    expect(chanceOn(1.2 / P[0] - 0.1, P)).toBeLessThan(1);
    expect(chanceOn(evenOdds(P), P)).toBeCloseTo(0.5, 6);
    expect(evenOdds(P)).toBeGreaterThan(1.3);
    expect(1.2 / P[1]).toBeLessThanOrEqual(1);
  });

  it('holds about as often as the card says, over thirty wars', () => {
    let wins = 0, expected = 0, fought = 0;
    for (let seed = 1; seed <= 30; seed++) {
      const g = playing('GER', seed);
      const { target, def, att } = line(g, 2, 6);
      works(def, M);
      // As many corps as bring the odds nearest to one and a half to one.
      let force = att.slice(0, 1);
      for (let k = 2; k <= att.length; k++) if (Math.abs(assess(g, att.slice(0, k), target).odds - 1.5) < Math.abs(assess(g, force, target).odds - 1.5)) force = att.slice(0, k);
      const a = assess(g, force, target);
      if (a.prepared < 0.99 || a.odds < 1.2 || a.odds > 2) continue;
      fought++;
      expected += chanceOn(a.odds, a.dice);
      const rep = resolveBattle(g, force, target);
      expect(rep.prepared).toBeCloseTo(1, 2);
      // The line is beaten when the field is won, whether or not the town behind it holds.
      if (rep.taken || rep.siege || rep.gates) wins++;
    }
    expect(fought).toBeGreaterThan(20);
    expect(wins).toBeGreaterThan(0);
    expect(wins).toBeLessThan(fought);
    expect(Math.abs(wins - expected)).toBeLessThan(9);
  }, 120_000);

  it('costs the attacker more men and the defender fewer', () => {
    const g = playing('GER');
    const l0 = line(g, 2, 3);
    works(l0.def, M);
    const fight = (losses: { attacker: number; defender: number }): { att: number; def: number; prepared: number } => {
      const { g: h, l } = again(g, l0);
      const keep = { ...DEFENCE.prepared.losses };
      Object.assign(DEFENCE.prepared.losses, losses);
      try {
        const rep = resolveBattle(h, l.att, l.target);
        return { att: rep.attLost, def: rep.defLost, prepared: rep.prepared ?? 0 };
      } finally { Object.assign(DEFENCE.prepared.losses, keep); }
    };
    const plain = fight({ attacker: 1, defender: 1 });
    const real = fight(DEFENCE.prepared.losses);
    expect(real.prepared).toBeCloseTo(1, 2);
    expect(plain.att).toBeGreaterThan(0);
    expect(plain.def).toBeGreaterThan(0);
    expect(real.att / plain.att).toBeGreaterThan(DEFENCE.prepared.losses.attacker - 0.12);
    expect(real.att / plain.att).toBeLessThan(DEFENCE.prepared.losses.attacker + 0.12);
    expect(real.def / plain.def).toBeGreaterThan(DEFENCE.prepared.losses.defender - 0.12);
    expect(real.def / plain.def).toBeLessThan(DEFENCE.prepared.losses.defender + 0.12);
  });

  it('leaves an attack it throws back with no moves for the month, where a check in the open costs a day', () => {
    const g = playing('GER');
    const l0 = line(g, 3, 1);
    // In the open.
    const o = again(g, l0);
    const openRep = resolveBattle(o.g, o.l.att, o.l.target);
    expect(openRep.taken || openRep.siege || openRep.gates).toBeFalsy();
    expect(openRep.prepared).toBeUndefined();
    expect(o.l.att[0].ap).toBe(o.l.att[0].apMax - 1);
    // Against a finished line.
    works(l0.def, M);
    const p = again(g, l0);
    expect(assess(p.g, p.l.att, p.l.target).prepared).toBeGreaterThanOrEqual(DEFENCE.prepared.spentAt);
    const rep = resolveBattle(p.g, p.l.att, p.l.target);
    expect(rep.taken || rep.siege || rep.gates).toBeFalsy();
    expect(rep.prepared).toBeCloseTo(1, 2);
    expect(p.l.att[0].ap).toBe(0);
    // Works barely begun do not stop a second try.
    DEFENCE.entrench.months >= 3 && works(l0.def, 1);
    const y = again(g, l0);
    const young = assess(y.g, y.l.att, y.l.target).prepared;
    resolveBattle(y.g, y.l.att, y.l.target);
    expect(y.l.att[0].ap).toBe(young >= DEFENCE.prepared.spentAt ? 0 : y.l.att[0].apMax - 1);
  });
});

describe('a whole war with nobody at the table', () => {
  it('the armies of every nation dig in, and attacks on their lines are sometimes thrown back', () => {
    // A war in which lines are attacked early enough to see some hold: since Version 61 works wear under attack,
    // and in most wars the first two years see few attacks on finished lines.
    const g = free(11);
    let dug = 0, held = 0, met = 0, seen = 0;
    const nations = new Set<string>();
    while (g.turn <= 26) {
      runTurnAI(g);
      for (const c of g.corps) if (!c.dead && c.dug && (c.dugFor ?? 0) >= M) { dug++; nations.add(c.nat); }
      for (const rep of g.reports) {
        if (rep.id <= seen) continue;
        seen = rep.id;
        if (!rep.prepared) continue;
        met++;
        if (!rep.taken && !rep.siege && !rep.gates) held++;
      }
    }
    expect(dug).toBeGreaterThan(100);
    expect(nations.size).toBeGreaterThan(5);
    // The computer's staff attacks prepared lines, mostly with the odds to carry them, and is sometimes thrown back.
    expect(met).toBeGreaterThan(10);
    expect(held).toBeGreaterThan(0);
    expect(held).toBeLessThan(met);
  }, 180_000);
});
