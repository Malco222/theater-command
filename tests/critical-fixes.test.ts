// Version 63: the critical fixes from the playtest of Version 61 (the grand campaign; none of it in the historical
// scenarios).
//
// 1. A won war can be finished. A part of a country cut off from the rest of it lives on the stores of its biggest
//    town and then goes hungry, depot or no depot; a harbour feeds a town only while there is another port to send
//    the ships from; and allies standing round the same town storm it together.
// 2. The front does not fall silent. Defenders who have stood off an attack already this month meet the next one
//    tired, until their own month comes round.
// 3. The attack card says what the fight is likely to cost each side in men, if it carries and if it is thrown back.
// 4. A jab costs the defence in proportion to its weight: a corps sent at an army no longer kills two for one.
// 5. The computer brings its guns to the works and strikes a second blow where one will not do.
// 6. Italy does not give in to a power that holds only its colonies.
// 7. The card opens on the best attack to be had, a wide band of chances says what would narrow it, and a battle's
//    report names its price and reads in the past.
//
// And the three critical fixes from the playtest of Version 62:
// 8. A standing call-up that is held sends the men of the wave not raised to the corps in the field, over and above
//    what the depots send and at no cost, and the card says what holding costs.
// 9. The cabinet's small jobs (a building, an agent) are asked only at peace or after six months without a battle;
//    one missed costs a small flat price and is not counted with the directives ignored.
// 10. A class called up early brings six months of men for six months without a class.
// With them: a defence is held against a general only where the attack had less than an even chance, and the
// cabinet's reading of a town is noted the way the player's page reckons it.
import { describe, expect, it } from 'vitest';
import {
  DEFENCE, addCorps, allied, assess, battleCost, capture, chanceOn, computeSupply, createGame, evenOdds, heldDrafts, limitAttackers, relocate, removeCorps, resolveBattle, startPhase,
} from '../src/campaign/game';
import { GOV, formOf, govMonth, govOf, heldAgainst, judgeDirective, promiseTargets, quietMonths, smallJob } from '../src/campaign/government';
import { MANPOWER, classGapUntil, classNow, menLeft, poolAtStart } from '../src/campaign/manpower';
import { gunsUp, twoWaves } from '../src/campaign/ai';
import { civicAI } from '../src/campaign/buildings';
import { seat } from '../src/campaign/commanders';
import { fireEvent } from '../src/campaign/diplomacy';
import { gunsWanted } from '../src/campaign/economy';
import { setFog } from '../src/campaign/intel';
import { loadGame, saveGame } from '../src/campaign/save';
import {
  SIEGE, arrive, besieged, fedBySea, feeders, garrisonOf, inPocket, monthsShut, pocketDry, pocketMonth, pocketNat, pocketView, portElsewhere, stormForce, storesOf,
} from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import {
  attackOutlook, beginHuman, callUpOf, candidates, governmentOf, humanUp, openingForce, orderCallUp, orderEarlyClass, orderSetAside, pocketAt, stepAI,
} from '../src/campaign/turn';
import { MEN_PER_DIV, type Corps, type Directive, type Game, type GovState, type Region } from '../src/campaign/types';

/** A grand campaign with the player's first month open, as the game's own screen would have it. */
function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}

/** Hands a province to a nation for a test: whoever stood in it is taken off the map, and its town changes hands with it. */
function hand(g: Game, r: Region, to: string): void {
  for (const u of [...r.units, ...garrisonOf(r)]) removeCorps(g, u);
  capture(g, r, to);
}

/**
 * Germany at war with Poland in September 1939, and Warsaw cut off: every province round the Polish capital is in
 * German hands, with `corps` Polish corps left in the field at Warsaw.
 */
function warsawCutOff(corps = 2): { g: Game; warsaw: Region; poles: Corps[] } {
  const g = playing('GER');
  const warsaw = g.byName['Warsaw'];
  for (const j of warsaw.nb) if (g.regions[j].ctrl !== 'GER') hand(g, g.regions[j], 'GER');
  for (const u of [...warsaw.units]) removeCorps(g, u);
  const poles: Corps[] = [];
  for (let k = 0; k < corps; k++) poles.push(addCorps(g, 'POL', `Army Warsaw ${k + 1}`, 'inf', 2, warsaw.id, {}));
  g.supplyDirty = true;
  return { g, warsaw, poles };
}

/** A month goes by for the pocket: its stores are counted and the supply drawn again. */
function month(g: Game, nat: string): void {
  g.turn++;
  pocketMonth(g, nat);
  computeSupply(g);
}

describe('fix 1: an army cut off with its capital lives on the town\'s stores, and then goes hungry', () => {
  it('the month the ring closes, the pocket is news and its troops still eat', () => {
    const { g, warsaw, poles } = warsawCutOff(2);
    const before = g.headlines.length;
    pocketMonth(g, 'POL');
    computeSupply(g);
    expect(pocketNat(g, warsaw)).toBe('POL');
    expect(poles.every((c) => c.supplied)).toBe(true);
    expect(poles.every((c) => inPocket(g, c))).toBe(true);
    const news = g.headlines.slice(before).map((h) => h.text).join(' | ');
    expect(news).toMatch(/Warsaw is surrounded/);
    expect(news).toMatch(/2 Polish corps are cut off there, with stores for 4 months/);
    const v = pocketAt(g, warsaw.id)!;
    expect(v.nat).toBe('POL');
    expect(v.since).toBe(g.turn);
    expect(v.stores).toBe(storesOf(g, warsaw));
    expect(v.left).toBe(4);
    expect(v.spent).toBe(false);
    expect(v.text).toMatch(/no drafts reach them/);
  });

  it('when the stores are eaten the capital feeds nobody, though it is a depot still', () => {
    const { g, warsaw, poles } = warsawCutOff(2);
    pocketMonth(g, 'POL');
    computeSupply(g);
    const stores = storesOf(g, warsaw);
    for (let k = 1; k < stores; k++) { month(g, 'POL'); expect(poles.every((c) => c.supplied), `month ${k}`).toBe(true); }
    month(g, 'POL');
    expect(pocketDry(g, warsaw, 'POL')).toBe(true);
    expect(poles.every((c) => !c.supplied)).toBe(true);
    const v = pocketView(g, warsaw.id)!;
    expect(v.spent).toBe(true);
    expect(v.left).toBe(0);
    expect(v.text).toMatch(/stores of Warsaw are spent/);
  });

  it('an army bigger than the town has room for eats the stores faster', () => {
    const few = warsawCutOff(2), many = warsawCutOff(8);
    const last = (s: { g: Game; warsaw: Region; poles: Corps[] }): number => {
      pocketMonth(s.g, 'POL');
      computeSupply(s.g);
      let k = 0;
      while (s.poles.every((c) => c.supplied) && k < 12) { month(s.g, 'POL'); k++; }
      return k;
    };
    const a = last(few), b = last(many);
    expect(a).toBe(storesOf(few.g, few.warsaw));
    // Eight corps where the town has room for four: the stores go twice as fast.
    expect(b).toBe(a / 2);
  });

  it('a road opened to the rest of the country breaks the ring, and supplies come in again', () => {
    const { g, warsaw, poles } = warsawCutOff(2);
    pocketMonth(g, 'POL');
    for (let k = 0; k < 6; k++) month(g, 'POL');
    expect(poles.every((c) => !c.supplied)).toBe(true);
    // The Poles hold three provinces in a row again: that is a country, not a pocket.
    const lublin = g.byName['Lublin'];
    capture(g, lublin, 'POL');
    const next = lublin.nb.map((j) => g.regions[j]).find((x) => x.id !== warsaw.id && x.owner === 'POL' && !warsaw.nb.includes(x.id)) ?? g.byName['Kielce'];
    capture(g, next, 'POL');
    capture(g, g.byName['Kielce'], 'POL');
    const before = g.headlines.length;
    month(g, 'POL');
    expect(pocketNat(g, warsaw)).toBeNull();
    expect(pocketAt(g, warsaw.id)).toBeNull();
    expect(poles.every((c) => c.supplied)).toBe(true);
    expect(g.headlines.slice(before).map((h) => h.text).join(' | ')).toMatch(/The ring round Warsaw is broken/);
  });

  it('a neutral neighbour is no enemy: the ring is not closed', () => {
    const { g, warsaw, poles } = warsawCutOff(2);
    // One of the roads out now runs into a country at peace with Poland.
    capture(g, g.byName['Brest'], 'SOV');
    for (let k = 0; k < 6; k++) month(g, 'POL');
    expect(pocketNat(g, warsaw)).toBeNull();
    expect(poles.every((c) => c.supplied)).toBe(true);
  });

  it('a small country whose capital is nearly all it has is not cut off from itself', () => {
    const g = playing('GER');
    const lux = g.byName['Luxembourg'];
    // Germany and France at war on either side of it, and Luxembourg at war with Germany.
    g.nations.LUX.atWar.push('GER'); g.nations.GER.atWar.push('LUX');
    for (const j of lux.nb) if (g.regions[j].ctrl !== 'GER') hand(g, g.regions[j], 'GER');
    const c = addCorps(g, 'LUX', 'Corps of Volunteers', 'inf', 1, lux.id, {});
    pocketMonth(g, 'LUX');
    for (let k = 0; k < 6; k++) month(g, 'LUX');
    expect(pocketNat(g, lux)).toBeNull();
    expect(c.supplied).toBe(true);
  });

  it('the siege that follows starts with the stores the army ate', () => {
    const { g, warsaw } = warsawCutOff(2);
    pocketMonth(g, 'POL');
    month(g, 'POL'); month(g, 'POL');
    // The field army is gone and the Germans walk in: the town holds, and its siege begins two months into its stores.
    for (const u of [...warsaw.units]) removeCorps(g, u);
    const ger = addCorps(g, 'GER', 'Test Corps', 'inf', 3, warsaw.id, {});
    expect(arrive(g, warsaw, 'GER')).toBe(false);
    expect(besieged(g, warsaw)).toBe(true);
    expect(monthsShut(g, warsaw)).toBe(2);
    expect(pocketNat(g, warsaw)).toBeNull();
    expect(ger.dead).toBe(false);
  });

  it('none of it in a historical scenario', () => {
    const g = createGame(3, 'GER');
    const warsaw = g.byName['Warsaw'];
    for (const j of warsaw.nb) if (g.regions[j].ctrl !== 'GER') { for (const u of [...g.regions[j].units]) removeCorps(g, u); capture(g, g.regions[j], 'GER'); }
    for (let k = 0; k < 8; k++) month(g, 'POL');
    expect(pocketNat(g, warsaw)).toBeNull();
    expect(pocketAt(g, warsaw.id)).toBeNull();
    expect(warsaw.units.filter((u) => u.nat === 'POL').every((c) => c.supplied)).toBe(true);
  });
});

describe('fix 1: a harbour feeds a town only while there is another port to send the ships from', () => {
  it('a country with ships and a second port feeds its besieged harbour; down to its last port, it cannot', () => {
    const g = playing('GER');
    const hamburg = g.byName['Hamburg'];
    // A British army round Hamburg: the town holds.
    for (const u of [...hamburg.units]) relocate(g, u, g.byName['Berlin']);
    addCorps(g, 'UK', 'Test Corps', 'inf', 3, hamburg.id, {});
    arrive(g, hamburg, 'UK');
    expect(besieged(g, hamburg)).toBe(true);
    expect(portElsewhere(g, 'GER', [hamburg.id])).toBe(true);
    expect(feeders(g, hamburg)).toContain('GER');
    // Every other port of Germany and her friends is lost.
    for (const r of g.regions) if (r.port && r.id !== hamburg.id && allied(g, 'GER', r.ctrl)) hand(g, r, 'UK');
    expect(portElsewhere(g, 'GER', [hamburg.id])).toBe(false);
    expect(feeders(g, hamburg)).toEqual([]);
    expect(fedBySea(g, hamburg)).toBe(false);
  });

  it('a country that is fed from off the map always has a port to ship from', () => {
    const g = playing('GER');
    const off = Object.values(g.nations).find((n) => n.offmap);
    if (off) expect(portElsewhere(g, off.id, g.regions.map((r) => r.id))).toBe(true);
    expect(portElsewhere(g, 'LUX', [])).toBe(false);
  });
});

describe('fix 1: allies round the same town storm it together', () => {
  it('the troops of the nation that leads go in first, and its staff sets the size of the assault', () => {
    const g = playing('GER');
    const hamburg = g.byName['Hamburg'];
    for (const u of [...hamburg.units]) relocate(g, u, g.byName['Berlin']);
    const fr = addCorps(g, 'FRA', 'French Corps', 'inf', 2, hamburg.id, {});
    const uk1 = addCorps(g, 'UK', 'British Tanks', 'arm', 3, hamburg.id, {});
    const uk2 = addCorps(g, 'UK', 'British Foot', 'inf', 3, hamburg.id, {});
    arrive(g, hamburg, 'UK');
    expect(besieged(g, hamburg)).toBe(true);
    const led = stormForce(g, [uk1, uk2, fr], hamburg, 'FRA');
    expect(led[0]).toBe(fr);
    expect(led).toHaveLength(3);
    const plain = stormForce(g, [uk1, uk2, fr], hamburg);
    expect(plain[0].nat).toBe('UK');
    expect(SIEGE.pocket.least).toBeGreaterThan(1);
  });
});

// ---- Fixes 2 to 7: a line under attack ------------------------------------------------------------------------------

interface Line { target: Region; from: Region; def: Corps[]; att: Corps[] }
/** A Polish line round Lodz with German infantry in front of it, in supply. The German offensive is an old one. */
function line(g: Game, defenders = 2, attackers = 2, divs = 3): Line {
  const target = g.byName['Lodz'];
  const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
  for (const u of [...target.units]) relocate(g, u, g.byName['Lublin']);
  for (const u of [...from.units]) if (u.nat === 'GER' && u.kind !== 'fort') relocate(g, u, g.byName['Berlin']);
  const def = Array.from({ length: defenders }, (_, i) => addCorps(g, 'POL', `Line ${i + 1}`, 'inf', 3, target.id, {}));
  const att = Array.from({ length: attackers }, (_, i) => addCorps(g, 'GER', `Storm ${i + 1}`, 'inf', divs, from.id, {}));
  for (const c of [...def, ...att]) { c.supplied = true; c.ap = c.apMax; }
  if (g.nations.GER.offStart) g.nations.GER.offStart.POL = g.turn - 6;
  if (g.nations.GER.lastFight) g.nations.GER.lastFight.POL = g.turn - 1;
  return { target, from, def, att };
}
const works = (cs: Corps[], months: number): void => { for (const c of cs) { c.dug = true; c.dugFor = months; } };
const M = DEFENCE.entrench.months;
function again(g: Game, l: Line): { g: Game; l: Line } {
  const h = loadGame(saveGame(g))!;
  const find = (c: Corps): Corps => h.corps.find((x) => x.id === c.id)!;
  return { g: h, l: { target: h.regions[l.target.id], from: h.regions[l.from.id], def: l.def.map(find), att: l.att.map(find) } };
}
/** A battle the attack loses, found by trying saves of the same game until the day's luck says so. */
function thrownBack(g0: Game, l0: Line): { g: Game; l: Line; rep: ReturnType<typeof resolveBattle> } {
  for (let k = 0; k < 40; k++) {
    const p = again(g0, l0);
    p.g.rng.next(); for (let i = 0; i < k; i++) p.g.rng.next();
    const rep = resolveBattle(p.g, p.l.att, p.l.target);
    if (!rep.taken && !rep.siege && !rep.gates) return { ...p, rep };
  }
  throw new Error('the attack was never thrown back');
}
const label = (ls: { label: string; value: string; effect?: string }[], name: string) => ls.find((x) => x.label === name);

describe('fix 2: defenders who have stood off an attack this month meet the next one tired', () => {
  it('a blow thrown back leaves the line tired, and the Battle Ticket says so', () => {
    const g = playing('GER');
    const l0 = line(g, 2, 3);
    works(l0.def, M);
    const first = assess(g, l0.att, l0.target);
    expect(first.weight).toBe(1);
    expect(first.tired).toBe(0);
    expect(label(first.lines, 'Earlier attacks')).toBeUndefined();
    const { g: h, l } = thrownBack(g, l0);
    expect(l.def.every((c) => c.blows === 1)).toBe(true);
    const second = assess(h, l.att, l.target);
    expect(second.tired).toBeCloseTo(1, 6);
    expect(label(second.lines, 'Earlier attacks')!.value).toMatch(/already stood off an attack this month/);
    // Tired men defend 15% weaker for each blow they have stood.
    const tired = second.D;
    for (const c of l.def) delete c.blows;
    const fresh = assess(h, l.att, l.target).D;
    expect(tired / fresh).toBeCloseTo(1 - DEFENCE.tired.per, 6);
  });

  it('three blows are the most that count, and the defenders\' own month clears them', () => {
    const g = playing('GER');
    const l = line(g, 2, 3);
    works(l.def, M);
    const fresh = assess(g, l.att, l.target).D;
    for (const c of l.def) c.blows = 7;
    expect(assess(g, l.att, l.target).D / fresh).toBeCloseTo(1 - DEFENCE.tired.per * DEFENCE.tired.most, 6);
    startPhase(g, 'POL');
    expect(l.def.every((c) => c.blows === undefined)).toBe(true);
  });

  it('a staff can read what a first wave would leave before it is sent', () => {
    const g = playing('GER');
    const l = line(g, 2, 3);
    works(l.def, M);
    const plain = assess(g, l.att, l.target);
    const after = assess(g, l.att, l.target, { after: { blows: 1, wear: plain.wear } });
    expect(after.odds).toBeGreaterThan(plain.odds * 1.15);
    expect(after.works).toBeCloseTo(Math.max(0, plain.works - plain.wear), 6);
    expect(label(after.lines, 'Earlier attacks')!.value).toMatch(/as the first wave will leave them/);
    // Nothing was changed by asking.
    expect(l.def.every((c) => c.blows === undefined && c.dugFor === M)).toBe(true);
  });

  it('none of it in a historical scenario', () => {
    const g = createGame(3, 'GER');
    const l0 = line(g, 2, 3);
    works(l0.def, M);
    const before = assess(g, l0.att, l0.target);
    expect(before.weight).toBe(1);
    for (const c of l0.def) c.blows = 2;
    const marked = assess(g, l0.att, l0.target);
    expect(marked.D).toBe(before.D);
    expect(marked.tired).toBe(0);
    for (const c of l0.def) delete c.blows;
    const rep = resolveBattle(g, l0.att, l0.target);
    expect(rep.lines.some((x) => x.label === 'Earlier attacks' || x.label === 'Weight of attack')).toBe(false);
    expect(l0.def.every((c) => c.blows === undefined)).toBe(true);
  });
});

describe('fix 4: a jab costs the defence in proportion to its weight', () => {
  it('a full attack weighs 1, and a corps sent at an army weighs what its odds are of 0.6 to 1', () => {
    const g = playing('GER');
    const l = line(g, 3, 1, 1);
    works(l.def, M);
    const a = assess(g, l.att, l.target);
    expect(a.odds).toBeLessThan(DEFENCE.wear.fullAt / 2);
    expect(a.weight).toBeCloseTo(a.odds / DEFENCE.wear.fullAt, 6);
    const said = label(a.lines, 'Weight of attack')!;
    expect(said.value).toMatch(/A jab/);
    expect(said.effect).toContain(`about ${Math.round(a.weight * 100)}%`);
    const full = line(playing('GER'), 2, 3);
    expect(assess(g, l.att, l.target).weight).toBeLessThan(1);
    expect(label(assess(playing('GER'), full.att, full.target).lines, 'Weight of attack')).toBeUndefined();
  });

  it('thrown back, the jab loses more men than it costs the defence, and tires it only a little', () => {
    const g = playing('GER');
    const l0 = line(g, 3, 1, 1);
    works(l0.def, M);
    const a = assess(g, l0.att, l0.target);
    const orgBefore = l0.def.map((c) => c.org);
    const { l, rep } = thrownBack(g, l0);
    expect(rep.taken).toBe(false);
    // The playtest's jab killed two for one. Now the defence loses in proportion to how much of it was engaged.
    expect(rep.defLost).toBeLessThan(rep.attLost);
    expect(rep.defLost).toBeGreaterThan(0);
    l.def.forEach((c, i) => expect(orgBefore[i] - c.org).toBeLessThan(0.2 * a.weight + 1e-9));
    expect(l.def.every((c) => Math.abs((c.blows ?? 0) - a.weight) < 1e-9)).toBe(true);
    // The report reads in the past.
    expect(label(rep.lines, 'Weight of attack')!.effect).toMatch(/the defenders lost about/);
  });
});

describe('fix 3: the card says what the fight is likely to cost', () => {
  it('both sides\' losses in men, if it carries and if it is thrown back', () => {
    const g = playing('GER');
    setFog(g, false);
    const l = line(g, 2, 3);
    works(l.def, 1);
    const look = attackOutlook(g, l.att, l.target)!;
    expect(look.exact).toBe(true);
    expect(look.chance).toBeGreaterThan(0);
    expect(look.chance).toBeLessThan(1);
    const { won, lost } = look.cost;
    expect(won && lost).toBeTruthy();
    // A thrown-back attack costs the attackers more and the defenders less than one that carries.
    expect(lost!.ours).toBeGreaterThan(won!.ours);
    expect(lost!.theirs!).toBeLessThan(won!.theirs!);
    expect(won!.share).toBeGreaterThan(0);
  });

  it('the bill is the one the battle presents', () => {
    const g = playing('GER');
    setFog(g, false);
    const l0 = line(g, 2, 2);
    works(l0.def, M);
    const a = assess(g, l0.att, l0.target);
    const bill = battleCost(g, a, l0.att, l0.target);
    expect(bill.chance).toBe(chanceOn(a.odds, a.dice));
    const { rep } = thrownBack(g, l0);
    expect(Math.abs(rep.attLost - bill.lost!.ours) / bill.lost!.ours).toBeLessThan(0.3);
    expect(Math.abs(rep.defLost - bill.lost!.theirs) / bill.lost!.theirs).toBeLessThan(0.45);
  });

  it('an attack that cannot carry shows only what failing costs, and one that cannot fail only what winning costs', () => {
    const g = playing('GER');
    setFog(g, false);
    const jab = line(g, 3, 1, 1);
    works(jab.def, M);
    const hopeless = attackOutlook(g, jab.att, jab.target)!;
    expect(hopeless.chance).toBe(0);
    expect(hopeless.cost.won).toBeNull();
    expect(hopeless.cost.lost!.ours).toBeGreaterThan(0);
    const h = playing('GER');
    setFog(h, false);
    const sure = line(h, 1, 4);
    for (const c of sure.def) { c.str = 0.3; c.org = 0.3; }
    const certain = attackOutlook(h, sure.att, sure.target)!;
    expect(certain.chance).toBe(1);
    expect(certain.cost.lost).toBeNull();
    expect(certain.cost.won!.theirs!).toBeGreaterThan(0);
  });

  it('where the defence is partly a guess, the enemy\'s loss is given as a share of his men, never a count', () => {
    const g = playing('GER');
    setFog(g, true);
    const l = line(g, 2, 3);
    works(l.def, 1);
    const look = attackOutlook(g, l.att, l.target)!;
    expect(look.exact).toBe(false);
    for (const c of [look.cost.won, look.cost.lost]) if (c) { expect(c.theirs).toBeNull(); expect(c.share).toBeGreaterThan(0); expect(c.ours).toBeGreaterThan(0); }
    expect(look.cost.won || look.cost.lost).toBeTruthy();
    expect(look.narrow).toMatch(/Identify the two corps the staff cannot see, and it can give one figure in place of this band/);
    expect(look.wide).toBe(chanceOn(look.hi, assess(g, l.att, l.target).dice) - chanceOn(look.lo, assess(g, l.att, l.target).dice) > 0.4);
  });
});

describe('fix 7: the card opens on the best attack to be had, and a report reads in the past', () => {
  it('everyone the roads will carry, where that beats the corps picked', () => {
    const g = playing('GER');
    const l = line(g, 2, 3);
    works(l.def, M);
    const all = candidates(g, l.target);
    expect(all.length).toBeGreaterThan(1);
    const one = openingForce(g, [l.att[0]], l.target);
    expect(one.widened).toBe(true);
    expect(one.corps).toEqual(all);
    const full = openingForce(g, all, l.target);
    expect(full.widened).toBe(false);
    expect(full.corps).toEqual(all);
    // More corps than the roads carry add nothing: the pick stands.
    const most = limitAttackers(g, all, l.target);
    if (most.length < all.length) expect(openingForce(g, most, l.target).widened).toBe(false);
  });

  it('a battle\'s report says what the attack did, not what it will do, and a later battle never reads an earlier week', () => {
    const g = playing('GER');
    const l0 = line(g, 2, 3);
    works(l0.def, M);
    const before = label(assess(g, l0.att, l0.target).lines, 'Dug in')!;
    expect(before.effect).toMatch(/Won or lost, this attack takes/);
    const { g: h, l, rep } = thrownBack(g, l0);
    const told = label(rep.lines, 'Dug in')!;
    expect(told.effect).toMatch(/Won or lost, the attack took/);
    expect(told.effect).not.toMatch(/this attack takes/);
    // A second battle the same month, by corps that have marched less far.
    const late = addCorps(h, 'GER', 'Late', 'inf', 3, l.from.id, {});
    late.supplied = true; late.ap = late.apMax;
    for (const c of l.att) c.ap = 1;
    h.reports[h.reports.length - 1].week = 3;
    const next = resolveBattle(h, [late], l.target);
    expect(next.week).toBeGreaterThanOrEqual(3);
  });
});

describe('fix 5: the computer brings its guns to the works and strikes twice', () => {
  /** Poland, played by the computer, with three German corps dug in on its frontier. */
  function facingWorks(): { g: Game; l: Line } {
    const g = playing('GER');
    if (!g.nations.POL.atWar.includes('GER')) throw new Error('Poland is not at war with Germany');
    const l = line(g, 2, 3);
    works(l.att, M);
    return { g, l };
  }

  it('a staff facing dug-in troops with too few guns of its own wants guns, and its builders wait for them', () => {
    const { g } = facingWorks();
    for (const c of g.corps.filter((x) => x.nat === 'POL' && x.kind === 'art')) removeCorps(g, c);
    g.nations.POL.queue = g.nations.POL.queue.filter((b) => !(b.what === 'corps' && b.kind === 'art'));
    expect(gunsWanted(g, 'POL')).toBe(true);
    // With its guns bought, or nobody dug in opposite, it does not.
    const guns = addCorps(g, 'POL', 'Guns', 'art', 2, g.byName['Warsaw'].id, {});
    const rest = g.corps.filter((c) => c.nat === 'POL' && !c.dead && c.kind !== 'art' && c.kind !== 'fort' && c.kind !== 'eng').length;
    for (let k = 1; k * 6 < rest; k++) addCorps(g, 'POL', `Guns ${k}`, 'art', 2, g.byName['Warsaw'].id, {});
    expect(gunsWanted(g, 'POL')).toBe(false);
    expect(guns.dead).toBe(false);
    // The builders: money is put by, but nothing is ordered while the staff waits for its guns.
    const h = facingWorks().g;
    const orders = h.nations.POL.queue.length, purse = h.flags['cvb:POL'] ?? 0;
    civicAI(h, 'POL', 500, false, true);
    expect(h.nations.POL.queue.length).toBe(orders);
    expect(h.flags['cvb:POL'] ?? 0).toBeGreaterThan(purse);
    // And never in a historical scenario.
    expect(gunsWanted(createGame(3, 'GER'), 'POL')).toBe(false);
  });

  it('guns standing behind the front are walked up to the sector that faces works', () => {
    const { g, l } = facingWorks();
    // Lodz is held by Polish infantry and looks at German works; the guns stand two provinces back, facing nothing.
    const back = g.regions.find((r) => r.ctrl === 'POL' && !r.nb.some((j) => g.regions[j].ctrl !== 'POL') && r.nb.some((j) => g.regions[j].nb.includes(l.target.id) || j === l.target.id))!;
    expect(back).toBeTruthy();
    for (const c of g.corps.filter((x) => x.nat === 'POL' && x.kind === 'art')) removeCorps(g, c);
    const guns = addCorps(g, 'POL', 'Guns', 'art', 2, back.id, {});
    guns.supplied = true; guns.ap = guns.apMax;
    const facing = (r: Region): boolean => r.nb.some((j) => g.regions[j].units.some((u) => u.nat === 'GER' && u.dug));
    expect(facing(g.regions[guns.reg])).toBe(false);
    gunsUp(g, 'POL', new Set(['GER']), null);
    expect(guns.reg).not.toBe(back.id);
    // A second month's march at most, and they stand beside the works.
    if (!facing(g.regions[guns.reg])) { guns.ap = guns.apMax; gunsUp(g, 'POL', new Set(['GER']), null); }
    expect(facing(g.regions[guns.reg])).toBe(true);
    // Guns already beside works stay where they are.
    const at = guns.reg;
    guns.ap = guns.apMax;
    gunsUp(g, 'POL', new Set(['GER']), null);
    expect(guns.reg).toBe(at);
  });

  it('where one blow will not do and more corps stand ready than the roads carry, the staff plans two waves', () => {
    const g = playing('GER');
    const l = line(g, 3, 6);
    works(l.def, M);
    const pool = limitAttackers(g, l.att, l.target);
    expect(pool.length).toBeLessThan(l.att.length);
    const alone = assess(g, pool, l.target).odds;
    const two = twoWaves(g, l.att, pool, l.target, alone * 1.1);
    expect(two).not.toBeNull();
    expect(two!.corps.length).toBeGreaterThan(0);
    expect(two!.corps.every((c) => !pool.includes(c))).toBe(true);
    expect(two!.then).toBeGreaterThanOrEqual(alone * 1.1);
    // No reserve, no second wave; and a first wave too weak to bite is not sent.
    expect(twoWaves(g, pool, pool, l.target, alone * 1.1)).toBeNull();
    expect(twoWaves(g, l.att, pool, l.target, alone * 5)).toBeNull();
    // Never in a historical scenario.
    const h = createGame(3, 'GER');
    const hl = line(h, 3, 6);
    expect(twoWaves(h, hl.att, limitAttackers(h, hl.att, hl.target), hl.target, 0.1)).toBeNull();
  });
});

describe('fix 6: Italy does not give in to a power that holds only its colonies', () => {
  const free = (seed: number): Game => startOpen(createGame(seed, null));
  function italy(g: Game): { rome: Region; naples: Region; colony: Region } {
    for (const [a, b] of [['GER', 'ITA'], ['ITA', 'GER']]) g.nations[a].atWar = g.nations[a].atWar.filter((x) => x !== b);
    if (!g.nations.ITA.atWar.includes('SOV')) { g.nations.ITA.atWar.push('SOV'); g.nations.SOV.atWar.push('ITA'); }
    if (!g.nations.ITA.atWar.includes('UK')) { g.nations.ITA.atWar.push('UK'); g.nations.UK.atWar.push('ITA'); }
    const rome = g.byName['Rome'], naples = g.byName['Naples'];
    const away = g.regions.find((r) => r.ctrl === 'ITA' && !r.overseas && r !== rome && r !== naples)!;
    for (const r of [rome, naples]) for (const u of [...r.units]) if (u.kind !== 'fort') relocate(g, u, away);
    const colony = g.regions.find((r) => r.owner === 'ITA' && r.overseas)!;
    return { rome, naples, colony };
  }

  it('a British army in an Italian colony gives Britain no claim on Rome', () => {
    const g = free(5);
    const { rome, colony } = italy(g);
    expect(colony).toBeTruthy();
    for (const u of [...colony.units, ...garrisonOf(colony)]) removeCorps(g, u);
    capture(g, colony, 'UK');
    addCorps(g, 'UK', 'Desert Corps', 'inf', 3, colony.id, {});
    // Nobody stands on Italy's home ground: the armistice finds no one to give in to, and Italy fights on.
    expect(fireEvent(g, 'italian-armistice')).toBe(false);
    expect(g.nations.ITA.alive).toBe(true);
    expect(rome.ctrl).toBe('ITA');
  });

  it('with an army on Italian soil the terms go to it, colonies or no colonies', () => {
    const g = free(5);
    const { rome, naples, colony } = italy(g);
    for (const u of [...colony.units, ...garrisonOf(colony)]) removeCorps(g, u);
    capture(g, colony, 'UK');
    addCorps(g, 'UK', 'Desert Corps', 'inf', 3, colony.id, {});
    const c = addCorps(g, 'SOV', 'Front', 'inf', 3, naples.id, {});
    if (c.town !== undefined) throw new Error('set down inside the town');
    expect(fireEvent(g, 'italian-armistice')).toBe(true);
    expect(g.nations.ITA.fellOn).toMatchObject({ to: 'SOV', how: 'terms' });
    expect(rome.ctrl).toBe('SOV');
    // The colony stays with whoever took it.
    expect(colony.ctrl).toBe('UK');
  });
});

// ---- The three critical fixes from the playtest of Version 62 ------------------------------------------------

const copy = (g: Game): Game => loadGame(saveGame(g))!;

describe('fix 8: a held call-up sends its men to the corps in the field', () => {
  /** The Soviet player at war with Germany, his field corps worn to `str` of their strength. */
  function sovietAtWar(str: number): { g: Game; ids: number[] } {
    const g = playing('SOV', 5);
    g.nations.SOV.atWar.push('GER'); g.nations.GER.atWar.push('SOV');
    const mine = g.corps.filter((c) => c.nat === 'SOV' && !c.dead && c.kind !== 'fort' && !c.town);
    for (const c of mine) c.str = str;
    return { g, ids: mine.map((c) => c.id) };
  }
  const filled = (g: Game, ids: number[]): number => ids.reduce((s, id) => { const c = g.corps.find((x) => x.id === id)!; return s + (c.dead ? 0 : c.divs * c.str); }, 0);
  const corps = (g: Game): number => g.corps.filter((c) => c.nat === 'SOV' && !c.dead).length;
  /** The player's next month opens, with nobody having moved in between. */
  const next = (g: Game): void => { g.turn++; startPhase(g, 'SOV'); };

  it('the wave that is not raised goes out as drafts over and above the depots\' cap, and costs the treasury nothing', () => {
    const { g, ids } = sovietAtWar(0.6);
    const held = copy(g), ran = copy(g);
    expect(orderCallUp(held, true)).toBe(true);
    const h0 = filled(held, ids), r0 = filled(ran, ids), hc = corps(held), rc = corps(ran), hp = held.nations.SOV.pp, rp = ran.nations.SOV.pp;
    next(held); next(ran);
    // Let run, the wave is raised; held, nobody is raised and its men are counted off for the field.
    expect(corps(ran)).toBeGreaterThan(rc);
    expect(corps(held)).toBe(hc);
    const wave = heldDrafts(held, 'SOV');
    expect(wave).toBeGreaterThan(0);
    expect(heldDrafts(ran, 'SOV')).toBe(0);
    // The corps in the field took in what the depots send and the wave's men besides: most of a wave more than the side that let it run.
    const more = (filled(held, ids) - h0) - (filled(ran, ids) - r0);
    expect(more).toBeGreaterThan(wave * 0.8);
    expect(more).toBeLessThanOrEqual(wave + 0.5);
    // And the treasury paid for no more men than it did where the call-up ran.
    expect(hp - held.nations.SOV.pp).toBeLessThanOrEqual(rp - ran.nations.SOV.pp + 1e-6);
  });

  it('the card says what holding costs, and with the corps full the men wait in the reserve', () => {
    const { g, ids } = sovietAtWar(1);
    const running = callUpOf(g)!.callUp!;
    expect(running.held).toBe(false);
    expect(running.drafts).toBe(Math.round(running.corps * running.divs * MEN_PER_DIV));
    expect(running.text).toMatch(new RegExp(`Hold it, and ${running.corps} new corps a month are not raised: their men go as drafts to the corps in the field instead`));
    expect(running.text).toContain('An army kept small that way can be outnumbered.');
    expect(orderCallUp(g, true)).toBe(true);
    const held = callUpOf(g)!.callUp!;
    expect(held.text).toMatch(/^The standing call-up is held: \d+ new corps a month are not raised, and the waves wait until you let it run\. Their men go to the corps in the field instead: up to .+ a month over and above what the depots send, at no cost to the treasury\./);
    expect(held.text).toContain('Once the corps are full the men wait in the reserve, and the army is no bigger for it.');
    // Full corps take nobody: the reserve keeps the wave's men, less only the month's ordinary business.
    const before = menLeft(g, 'SOV'), f0 = filled(g, ids);
    next(g);
    expect(filled(g, ids)).toBeCloseTo(f0, 6);
    expect(menLeft(g, 'SOV')).toBeGreaterThanOrEqual(before - 1e-6);
    // Let run again, the count is wiped and the next wave is raised.
    expect(orderCallUp(g, false)).toBe(true);
    expect(g.flags['mphw:SOV']).toBeUndefined();
    expect(heldDrafts(g, 'SOV')).toBe(0);
    const c0 = corps(g);
    next(g);
    expect(corps(g)).toBeGreaterThan(c0);
  });

  it('nothing of it for a nation the computer plays, or in a historical scenario', () => {
    const g = playing('GER', 5);
    g.flags['mph:SOV'] = 1; g.flags['mphw:SOV'] = 24;
    expect(heldDrafts(g, 'SOV')).toBe(0);
    const h = createGame(1, null);
    h.flags['mph:GER'] = 1; h.flags['mphw:GER'] = 24;
    expect(heldDrafts(h, 'GER')).toBe(0);
  });
});

describe('fix 9: the cabinet\'s small jobs wait for a quiet time, and cost little', () => {
  /** The cabinet's next directive, drawn now, with a balanced table that has just asked for a town. */
  function ask(g: Game, st: GovState): Directive | undefined {
    delete st.dir;
    st.next = g.turn; st.headNext = g.turn + 99; st.lastKind = 'take';
    govMonth(g, g.human!);
    return st.dir;
  }
  const rich = (g: Game): void => { g.nations[g.human!].pp = 5000; };

  it('a country in the thick of a war is asked for neither a building nor an agent; six months without a battle, and it is', () => {
    const g = playing('GER'), st = govOf(g, 'GER')!, n = g.nations.GER;
    expect(n.atWar.length).toBeGreaterThan(0);
    const kinds: string[] = [];
    for (let k = 0; k < 30; k++) {
      rich(g);
      n.lastFight.POL = g.turn - (k % 5);          // a battle within the last five months, every month
      expect(quietMonths(g, 'GER')).toBeLessThan(GOV.directives.small.quiet);
      const d = ask(g, st);
      if (d) kinds.push(d.kind);
      g.turn++;
    }
    expect(kinds.length).toBeGreaterThan(0);
    expect(kinds.some((k) => k === 'build' || k === 'agent')).toBe(false);
    // The front falls quiet: no battle of Germany's own, and no new war, for six months.
    const q = playing('GER'), qs = govOf(q, 'GER')!;
    for (const o of Object.values(q.nations)) { if (o.id === 'GER') o.lastFight = {}; else delete o.lastFight.GER; }
    expect(quietMonths(q, 'GER')).toBeLessThan(GOV.directives.small.quiet);
    q.turn += GOV.directives.small.quiet;
    expect(quietMonths(q, 'GER')).toBeGreaterThanOrEqual(GOV.directives.small.quiet);
    const quiet: string[] = [];
    for (let k = 0; k < 40 && !quiet.some((x) => x === 'build' || x === 'agent'); k++) { rich(q); const d = ask(q, qs); if (d) quiet.push(d.kind); q.turn++; }
    expect(quiet.some((x) => x === 'build' || x === 'agent')).toBe(true);
  });

  it('a war just begun is no quiet time, and a country at peace is asked as before', () => {
    const g = playing('GER');
    for (const o of Object.values(g.nations)) { if (o.id === 'GER') o.lastFight = {}; else delete o.lastFight.GER; }
    expect(quietMonths(g, 'GER')).toBeLessThan(GOV.directives.small.quiet);
    const p = playing('ITA'), ps = govOf(p, 'ITA')!;
    expect(p.nations.ITA.atWar).toEqual([]);
    expect(quietMonths(p, 'ITA')).toBe(Infinity);
    const kinds: string[] = [];
    for (let k = 0; k < 40 && !kinds.some((x) => x === 'build' || x === 'agent'); k++) { rich(p); const d = ask(p, ps); if (d) kinds.push(d.kind); p.turn++; }
    expect(kinds.some((x) => x === 'build' || x === 'agent')).toBe(true);
  });

  it('a building is asked for only of a treasury that could pay for it today', () => {
    const p = playing('ITA'), ps = govOf(p, 'ITA')!;
    const kinds: string[] = [];
    for (let k = 0; k < 30; k++) { p.nations.ITA.pp = 1; const d = ask(p, ps); if (d) kinds.push(d.kind); p.turn++; }
    expect(kinds.some((x) => x === 'build' || x === 'agent')).toBe(false);
  });

  it('a small job missed costs a small flat price, and is not counted with the directives ignored', () => {
    const M = GOV.directives.missed, S = GOV.directives.small;
    const job = (g: Game, kind: 'build' | 'agent' | 'trade'): Directive => ({ id: g.turn, nat: 'ITA', kind, from: 'ITA', issued: g.turn, due: g.turn, capital: 4, money: 0, target: 999,
      title: kind === 'build' ? 'Build up a town' : kind === 'agent' ? 'Recruit an agent' : 'Sign a trade agreement', text: 'A job.' });
    expect(smallJob({ kind: 'build' })).toBe(true);
    expect(smallJob({ kind: 'agent' })).toBe(true);
    expect(smallJob({ kind: 'trade' })).toBe(false);
    expect(smallJob({ kind: 'take' })).toBe(false);
    const g = playing('ITA'), st = govOf(g, 'ITA')!, swing = GOV.forms[formOf('ITA')].swing;
    st.seats = {};
    // Two directives already ignored: an ordinary one would now cost base and two steps.
    st.missed = 2; st.trust = 60;
    st.dir = job(g, 'build');
    const v = governmentOf(g)!.directive!;
    expect(v.small).toBe(true);
    expect(v.price).toBeCloseTo(Math.round(S.missed * swing * 10) / 10, 9);
    expect(v.asidePrice).toBeCloseTo(Math.round(S.missed * M.aside * swing * 10) / 10, 9);
    // It runs out undone.
    const h = g.headlines.length;
    g.turn++;
    judgeDirective(g, 'ITA');
    expect(st.dir).toBeUndefined();
    expect(st.trust).toBeCloseTo(60 - S.missed * swing, 9);
    expect(st.missed).toBe(2);
    expect(st.blame).toBeUndefined();
    expect(g.headlines.slice(h).some((x) => /It was a small matter: the war cabinet's confidence in you slips a little, and it is not counted with the directives you have ignored\./.test(x.text))).toBe(true);
    // Set aside, it costs half that, and is not counted either.
    st.dir = job(g, 'agent'); st.dir.due = g.turn + 2; st.trust = 60;
    expect(orderSetAside(g)).toBe(true);
    expect(st.trust).toBeCloseTo(60 - S.missed * M.aside * swing, 9);
    expect(st.missed).toBe(2);
    // An ordinary job missed still costs the climbing price, and is counted.
    st.dir = job(g, 'trade'); st.trust = 60;
    const ordinary = governmentOf(g)!.directive!;
    expect(ordinary.small).toBe(false);
    expect(ordinary.price).toBeCloseTo(Math.round(Math.min(M.most, M.base + M.step * 2) * swing * 10) / 10, 9);
    g.turn++;
    judgeDirective(g, 'ITA');
    expect(st.missed).toBe(3);
    expect(st.trust).toBeCloseTo(60 - Math.min(M.most, M.base + M.step * 2) * swing, 9);
  });

  it('a town asked for gives its odds in the past once the month it was set in is over, beside what the card reads today', () => {
    const g = playing('GER'), st = govOf(g, 'GER')!;
    const r = g.regions.find((x) => x.ctrl === 'POL' && !x.cap)!;
    st.dir = { id: g.turn, nat: 'GER', kind: 'take', from: 'GER', region: r.id, issued: g.turn, due: g.turn + 4, capital: 4, money: 0, title: `Take ${r.name}`,
      text: `The war cabinet wants ${r.name} taken from Poland, by you or an ally, by the end of the year. With your corps in reach brought up, the attack card reads about 2 to 1.` };
    expect(governmentOf(g)!.directive!.text).toContain('With your corps in reach brought up, the attack card reads about 2 to 1.');
    g.turn++;
    const v = governmentOf(g)!.directive!;
    expect(v.text).toContain('When the order was written the attack card, with your corps in reach brought up, read about 2 to 1.');
    expect(v.text).not.toContain('the attack card reads');
    expect(v.oddsLine).toMatch(/today\.$/);
  });
});

describe('fix 10: a class called up early brings six months of men for six months without a class', () => {
  it('six for six, and the card says they are the same men sooner', () => {
    const E = MANPOWER.early;
    expect(E.months).toBe(6);
    expect(E.gap).toBe(6);
    const g = playing('GER');
    g.flags['mp:GER'] = poolAtStart(g, 'GER') * 0.3;
    const v = callUpOf(g)!.early;
    expect(v).toMatchObject({ months: 6, gap: 6, why: null });
    expect(v.men).toBeCloseTo(Math.round(classNow(g, 'GER') * 6 * MEN_PER_DIV), -2);
    expect(v.text).toMatch(/^Call up the next class early: about .+ men join the reserve at once, 6 months of classes together\. No class then comes of age for 6 months, because those boys are with the colours already: the same men, sooner\.$/);
    expect(orderEarlyClass(g)).toBe(true);
    expect(classGapUntil(g, 'GER')).toBe(g.turn + 6);
  });

  it('with room in the reserve for less, fewer months come and fewer months go without a class', () => {
    const g = playing('GER');
    // Room for about two months of classes, and no more.
    const one = classNow(g, 'GER');
    g.flags['mp:GER'] = poolAtStart(g, 'GER') - one * 2;
    const v = callUpOf(g)!.early;
    expect(v.why).toBeNull();
    expect(v.months).toBe(2);
    expect(v.gap).toBe(2);
    expect(v.text).toContain('2 months of classes together. No class then comes of age for 2 months');
    const before = menLeft(g, 'GER');
    expect(orderEarlyClass(g)).toBe(true);
    expect(menLeft(g, 'GER') - before).toBeCloseTo(one * 2, 3);
    expect(classGapUntil(g, 'GER')).toBe(g.turn + 2);
  });
});

describe('the lesser findings of the Version 62 playtest', () => {
  it('a defence lost is held against a general only where the attack had less than an even chance on the day', () => {
    const W = GOV.head.winnable;
    const at = (odds: number, att: boolean, even?: number) => ({ t: 1, reg: 0, odds, att, ...(even === undefined ? {} : { even }) });
    const held = (b: ReturnType<typeof at>): boolean => heldAgainst({ rec: { lost: [b] } } as never).length === 1;
    // In the open an even chance is about 1.2 to 1: at 1.3 and 1.45 the attacker was likelier than not to carry it.
    expect(evenOdds([0.75, 1.25])).toBeCloseTo(1.2, 9);
    expect(held(at(1.1, false, 1.2))).toBe(true);
    expect(held(at(1.3, false, 1.2))).toBe(false);
    expect(held(at(1.45, false, 1.2))).toBe(false);
    // Against prepared positions an even chance takes more, and a line lost at 1.45 could have been held.
    expect(held(at(1.45, false, 1.7))).toBe(true);
    // Battles written down before Version 63 carry no mark: they are read as fought in the open.
    expect(held(at(1.1, false))).toBe(true);
    expect(held(at(1.3, false))).toBe(false);
    // His own attack thrown back counts at good odds that were also better than even.
    expect(held(at(W, true, 1.2))).toBe(true);
    expect(held(at(W + 0.1, true, 1.7))).toBe(false);
    expect(held(at(W - 0.1, true, 1.2))).toBe(false);
  });

  it('a lost battle is written down with the odds an even chance took that day', () => {
    const g = playing('GER');
    const l = line(g, 2, 2);
    works(l.def, M);
    seat(g, l.att[0], 'Ernst Probe');
    // Against finished works the day runs against the attack: an even chance takes more than it would in the open.
    const even = evenOdds(assess(g, l.att, l.target).dice);
    expect(even).toBeGreaterThan(1.2);
    const t = thrownBack(g, l);
    const gen = t.g.generals.find((x) => x.name === 'Ernst Probe')!;
    expect(gen.rec.lost).toHaveLength(1);
    expect(gen.rec.lost![0]).toMatchObject({ att: true, reg: l.target.id, even: Math.round(even * 100) / 100 });
  });

  it('the cabinet notes its reading of a town once the month has opened, the way the page reckons it', () => {
    const g = playing('GER'), st = govOf(g, 'GER')!;
    const noted = st.reach!.find((m) => m.t === g.turn)!;
    expect(noted).toBeDefined();
    const targets = promiseTargets(g);
    expect(targets.length).toBeGreaterThan(0);
    // What the cabinet has noted for this month is what the page shows today, town for town.
    let same = 0;
    for (const t of targets) { const was = noted.odds[t.region]; if (was !== undefined && Math.abs(was - t.odds) < 0.006) same++; }
    expect(same).toBe(targets.filter((t) => noted.odds[t.region] !== undefined).length);
    expect(same).toBeGreaterThan(0);
  });
});
