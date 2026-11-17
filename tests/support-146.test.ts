// What you bring and what you face (the grand campaign; rulings 146, 149, 150, 152, 156 and 158).
// Until this version only tanks stopped tanks. Now every defence has an anti-tank strength from 0 to 1, made of its
// infantry's guns of the day, tank destroyers, hand-held weapons, guns in the line, mines, fortress works and its own
// tanks, against how good the attacker's armour is for its day; the tanks' edge shrinks by it. Machine guns tell
// against men on foot, mortars against fieldworks, assault rifles in the attack, snipers in forest and streets. A
// battle's losses fall on what the defence can kill. The battle card says it before the attack (WHAT YOU FACE) and
// after it (who paid), as far as the player's reports know it; the computer's staffs do not send tanks alone at a
// gunned line. None of it is in a history campaign. The numbers are in data/campaign/support.json.
import { describe, expect, it } from 'vitest';
import { footBeside, notTanksAlone, twoWaves } from '../src/campaign/ai';
import { GUNS_AND_MINES_UNKNOWN, GUNS_UNKNOWN, briefTicket, gunsKnown, minesKnown, whatYouFace } from '../src/campaign/facing';
import { DEFENCE, addCorps, assess, createGame, limitAttackers, relocate, resolveBattle, worksShare } from '../src/campaign/game';
import { SIGHT, identify, setFog, sightOf } from '../src/campaign/intel';
import { startOpen } from '../src/campaign/staff';
import {
  SUPPORT, antiTankParts, armourOfTheDay, carried, carries, faceLines, gunsOfTheDay, shareWords, supportRules, tollFor, whoPaid,
  type AntiTank, type Faced, type Toll,
} from '../src/campaign/support';
import { TECH, facesTanks, hasTech, staffPick, techById, techRule } from '../src/campaign/tech';
import { attackOutlook, beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Agent, Corps, Game, Region } from '../src/campaign/types';

const S = SUPPORT.antiTank, T = SUPPORT.tech;
const free = (seed: number, human: string | null = null): Game => startOpen(createGame(seed, human));
/** A grand campaign with the player's first month open. */
function playing(human = 'GER', seed = 3): Game {
  const g = free(seed, human);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const give = (g: Game, nat: string, ...ids: string[]): void => { for (const id of ids) if (!hasTech(g, nat, id)) g.nations[nat].tech!.done.push(id); };
const take = (g: Game, nat: string, ...ids: string[]): void => { const s = g.nations[nat].tech!; s.done = s.done.filter((x) => !ids.includes(x)); };
/** The first month of a year of the war (September 1939 is month 1). */
const january = (year: number): number => 5 + 12 * (year - 1940);

interface Line { target: Region; from: Region; def: Corps[]; att: Corps[] }
/**
 * A Polish line in the country round Lodz and Germans in front of it, all test corps, in supply and with the month
 * before them. The German offensive is an old one, so nothing catches the line off balance. Poland starts with
 * Anti-tank guns; Germany does not, and neither has any of the other weapons until a test gives them.
 */
function line(g: Game, defenders: [Corps['kind'], number][] = [['inf', 3], ['inf', 3]], attackers: [Corps['kind'], number][] = [['inf', 3], ['inf', 3]]): Line {
  const target = g.byName['Lodz'];
  const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
  for (const u of [...target.units]) relocate(g, u, g.byName['Lublin']);
  const def = defenders.map(([kind, divs], i) => addCorps(g, 'POL', `Line ${i + 1}`, kind, divs, target.id, {}));
  const att = attackers.map(([kind, divs], i) => addCorps(g, 'GER', `Storm ${i + 1}`, kind, divs, from.id, {}));
  for (const c of [...def, ...att]) { c.supplied = true; c.ap = c.apMax; }
  g.nations.GER.offStart.POL = g.turn - 6;
  g.nations.GER.lastFight.POL = g.turn - 1;
  for (const id of [T.tankDestroyers, T.handHeld, T.support, ...T.armour]) { take(g, 'GER', id); take(g, 'POL', id); }
  give(g, 'POL', T.antiTank);
  take(g, 'GER', T.antiTank);
  return { target, from, def, att };
}
/** Sets a line's works to so many months' digging. */
const works = (cs: Corps[], months: number): void => { for (const c of cs) { c.dug = true; c.dugFor = months; } };
const DONE = DEFENCE.entrench.months;
const faced = (g: Game, l: Line, att: Corps[] = l.att): Faced => assess(g, att, l.target).faced!;
const at = (g: Game, l: Line, att: Corps[] = l.att): AntiTank => faced(g, l, att).antiTank!;
const row = (g: Game, l: Line, label: string, att: Corps[] = l.att): string | undefined => assess(g, att, l.target).lines.find((x) => x.label === label)?.value;

describe('the data', () => {
  it('holds the starting numbers the design director accepted', () => {
    const years = Object.keys(S.byYear).map(Number);
    expect(years).toEqual([...years].sort((a, b) => a - b));
    for (const [open, dug] of Object.values(S.byYear)) { expect(open).toBeGreaterThanOrEqual(0); expect(dug).toBeGreaterThan(open); expect(dug).toBeLessThanOrEqual(1); }
    expect(S.byYear['1939']).toEqual([0, 0.2]);
    expect(S.byYear['1944']).toEqual([0.55, 0.8]);
    expect(SUPPORT.toll.tanks).toEqual([0.7, 1.4]);
    expect(SUPPORT.staff.gunned).toBeLessThan(SUPPORT.staff.never);
    for (const id of [T.antiTank, T.tankDestroyers, T.handHeld, T.support, ...T.armour]) expect(techById(id), id).toBeTruthy();
  });

  it('gives the guns and the armour of each year, and after 1944 the last row stands', () => {
    expect(gunsOfTheDay(1939)).toEqual([0, 0.2]);
    expect(gunsOfTheDay(1942)).toEqual([0.4, 0.7]);
    expect(gunsOfTheDay(1946)).toEqual(S.byYear['1944']);
    expect(armourOfTheDay(1939)).toBe(0);
    expect(armourOfTheDay(1941)).toBe(1);
    expect(armourOfTheDay(1942)).toBe(2);
    expect(armourOfTheDay(1943)).toBe(2);
    expect(armourOfTheDay(1944)).toBe(3);
  });

  it('counts the weapons for infantry, mountain troops, lorried infantry, cavalry and fortress troops, and for nobody else', () => {
    const g = playing('GER');
    const lodz = g.byName['Lodz'];
    const kinds: [Corps['kind'], boolean][] = [['inf', true], ['mtn', true], ['mot', true], ['cav', true], ['fort', true], ['arm', false], ['art', false], ['eng', false]];
    for (const [kind, yes] of kinds) expect(carries(addCorps(g, 'POL', `Test ${kind}`, kind, 2, lodz.id, {})), kind).toBe(yes);
  });
});

describe('the anti-tank strength of a defence', () => {
  it('is the infantry\'s guns of the day: nothing in the open in 1939, a fifth behind finished fieldworks, and between the two as the works grow', () => {
    const g = playing('GER');
    const l = line(g, undefined, [['arm', 3], ['inf', 3]]);
    expect(at(g, l).strength).toBe(0);
    works(l.def, DONE);
    expect(at(g, l).strength).toBeCloseTo(0.2, 6);
    expect(at(g, l).guns).toBeCloseTo(0.2, 6);
    works(l.def, 1);
    expect(at(g, l).strength).toBeCloseTo(0.2 * worksShare(1), 6);
    // Works do nothing for a corps that is not fed.
    for (const c of l.def) c.supplied = false;
    expect(at(g, l).strength).toBe(0);
  });

  it('rises with the years of the war', () => {
    for (const [year, open, dug] of [[1941, 0.2, 0.5], [1943, 0.5, 0.75], [1945, 0.55, 0.8]] as [number, number, number][]) {
      const g = playing('GER');
      g.turn = january(year);
      const l = line(g);
      // The attacker's armour is kept at that of its day, so the year alone speaks.
      give(g, 'GER', ...T.armour.filter((id) => techById(id)!.year <= year));
      const tanks = addCorps(g, 'GER', 'Panzer', 'arm', 3, l.from.id, {});
      expect(at(g, l, [...l.att, tanks]).strength, `${year} in the open`).toBeCloseTo(open, 6);
      works(l.def, DONE);
      expect(at(g, l, [...l.att, tanks]).strength, `${year} dug in`).toBeCloseTo(dug, 6);
    }
  });

  it('counts for half where the infantry has not researched Anti-tank guns', () => {
    const g = playing('GER');
    const l = line(g, undefined, [['arm', 3]]);
    works(l.def, DONE);
    expect(at(g, l).kit).toBe(true);
    take(g, 'POL', T.antiTank);
    expect(at(g, l).kit).toBe(false);
    expect(at(g, l).strength).toBeCloseTo(0.2 * S.without, 6);
  });

  it('adds tank destroyers, and hand-held weapons that are worth more in close country', () => {
    const g = playing('GER');
    const l = line(g, undefined, [['arm', 3]]);
    give(g, 'POL', T.tankDestroyers);
    expect(at(g, l).destroyers).toBeCloseTo(S.tankDestroyers, 6);
    expect(at(g, l).strength).toBeCloseTo(S.tankDestroyers, 6);
    give(g, 'POL', T.handHeld);
    expect(at(g, l).handHeld).toBeCloseTo(S.handHeld.open, 6);
    l.target.terrain = 'hills';
    expect(at(g, l).handHeld).toBeCloseTo(S.handHeld.close, 6);
  });

  it('adds guns in the line, a division at a time and no more than the most', () => {
    const g = playing('GER');
    const l = line(g, [['inf', 3], ['art', 2]], [['arm', 3]]);
    expect(at(g, l).artillery).toBeCloseTo(2 * S.guns.perDiv, 6);
    l.def[1].divs = 9;
    expect(at(g, l).artillery).toBeCloseTo(S.guns.most, 6);
  });

  it('adds minefields, less what the attacker\'s engineers lift', () => {
    const g = playing('GER');
    const l = line(g, undefined, [['arm', 3]]);
    g.flags[`mine:${l.target.id}`] = 2;
    expect(at(g, l).mines).toBeCloseTo(2 * S.mines, 6);
    const sappers = addCorps(g, 'GER', 'Pioneers', 'eng', 1, l.from.id, {});
    sappers.supplied = true;
    expect(at(g, l, [...l.att, sappers]).mines).toBeLessThan(2 * S.mines);
  });

  it('adds the ditch and the dragon\'s teeth of fortress works that face the attack, and concrete adds no more', () => {
    const g = playing('GER');
    const l = line(g, undefined, [['arm', 3]]);
    const owner = g.regions[l.att[0].reg].owner;
    for (const [lv, share] of [[1, 0.05], [2, 0.1], [3, 0.1]] as [number, number][]) {
      l.target.fort = { lv, vs: [owner] };
      expect(at(g, l).works, `level ${lv}`).toBeCloseTo(share, 6);
      expect(at(g, l).fort).toBe(lv);
    }
    // Works that face another way are outflanked: they add nothing.
    l.target.fort = { lv: 3, vs: ['SOV'] };
    expect(at(g, l).works).toBe(0);
  });

  it('counts the defence\'s own tanks at twice their share of it: half the defence in tanks stops the edge whole', () => {
    const g = playing('GER');
    const l = line(g, [['inf', 3], ['arm', 1]], [['arm', 3]]);
    expect(at(g, l).tanks).toBeCloseTo(S.tanks * (1 / 4), 6);
    expect(at(g, l).strength).toBeCloseTo(0.5, 6);
    l.def[1].divs = 3;
    expect(at(g, l).strength).toBe(1);
  });

  it('falls before armour ahead of its day and rises against armour behind it', () => {
    const g = playing('GER');
    g.turn = january(1942);
    const l = line(g, undefined, [['arm', 3], ['inf', 3]]);
    works(l.def, DONE);
    const day = S.byYear['1942'][1];
    // 1942 expects Medium and Heavy tanks. With neither the tanks are two steps behind.
    expect(at(g, l).armour).toBeCloseTo(-2 * S.armourStep, 6);
    expect(at(g, l).strength).toBeCloseTo(day + 2 * S.armourStep, 6);
    give(g, 'GER', T.armour[0], T.armour[1]);
    expect(at(g, l).armour).toBeCloseTo(0, 6);
    expect(at(g, l).strength).toBeCloseTo(day, 6);
    give(g, 'GER', T.armour[2]);
    expect(at(g, l).strength).toBeCloseTo(day - S.armourStep, 6);
    // With no tanks in the attack there is no armour to count.
    expect(at(g, l, [l.att[1]]).armour).toBe(0);
  });

  it('is held between nothing and the whole', () => {
    const g = playing('GER');
    g.turn = january(1944);
    const l = line(g, [['inf', 3], ['art', 4], ['arm', 2]], [['arm', 3]]);
    works(l.def, DONE);
    give(g, 'POL', T.tankDestroyers, T.handHeld);
    expect(at(g, l).strength).toBe(1);
    const h = playing('GER');
    const m = line(h, undefined, [['arm', 3]]);
    give(h, 'GER', ...T.armour);
    expect(at(h, m).strength).toBe(0);
  });
});

describe('the battle', () => {
  it('leaves the tanks as much of their shock as the anti-tank strength does not take', () => {
    const g = playing('GER');
    const l = line(g, undefined, [['arm', 3], ['inf', 3]]);
    const open = assess(g, l.att, l.target);
    expect(open.faced!.shock).toBe(1);
    expect(row(g, l, 'Armor')).toBe('Tank attack up 25%');
    works(l.def, DONE);
    const dug = assess(g, l.att, l.target);
    expect(dug.faced!.shock).toBeCloseTo(0.8, 6);
    expect(row(g, l, 'Armor')).toBe('Tank attack up 20%');
    // The same tanks against the same men with the works left out of the count: the attack is smaller by the shock lost.
    const tank = l.att[0], foot = l.att[1];
    const alone = (cs: Corps[]): number => assess(g, cs, l.target).A;
    works(l.def, 0); for (const c of l.def) c.dug = false;
    const full = alone([tank]) / 1.25;
    works(l.def, DONE);
    // (Alone, they also lose to the guns for want of infantry beside them: the next test.)
    expect(alone([tank])).toBeCloseTo(full * (1 + 0.25 * 0.8) * (1 - S.alone.cut * 0.2), 4);
    expect(alone([tank, foot]) - alone([foot])).toBeGreaterThan(alone([tank]));
    expect(alone([foot])).toBeGreaterThan(0);
    // Where the defence stops the edge whole there is no Armor row at all.
    const h = playing('GER');
    const m = line(h, [['inf', 2], ['arm', 2]], [['arm', 3]]);
    expect(faced(h, m).shock).toBe(0);
    expect(row(h, m, 'Armor')).toBeUndefined();
  });

  it('cuts the attack of tanks that come on without infantry beside them, by the guns alone, and a division of infantry covers two of tanks', () => {
    const A = S.alone;
    expect(A).toEqual({ cut: 0.7, cover: 2 });
    const g = playing('GER');
    g.turn = january(1943);
    const l = line(g, undefined, [['arm', 3], ['arm', 3]]);
    give(g, 'GER', T.armour[0], T.armour[1]);
    give(g, 'POL', T.tankDestroyers);
    works(l.def, DONE);
    // The line of ruling 152(3): guns of the day behind finished works and tank destroyers, nine tenths in all.
    const alone = at(g, l);
    expect(alone.gunned).toBeCloseTo(0.9, 6);
    expect(alone.cover).toBe(0);
    expect(alone.alone).toBeCloseTo(A.cut * 0.9, 6);
    expect(faced(g, l).alone).toBeCloseTo(0.63, 6);
    expect(row(g, l, 'Tanks alone')).toBe('Tank attack cut by 63%');
    // (The line stands first on the card: it is the one thing there he can change before he clicks.)
    expect(faceLines(faced(g, l)).map((x) => x.text)).toEqual([
      'No infantry with your tanks: their guns take two thirds of the tanks\' attack.',
      'Anti-tank guns and tank destroyers: your tanks\' shock counts for a tenth.',
    ]);
    // The attack itself is smaller by that share: the same tanks against the line without its tank destroyers.
    const a1 = assess(g, l.att, l.target).A;
    take(g, 'POL', T.tankDestroyers);
    const a0 = assess(g, l.att, l.target).A;
    expect(a1 / a0).toBeCloseTo(((1 + 0.25 * 0.1) * (1 - A.cut * 0.9)) / ((1 + 0.25 * 0.25) * (1 - A.cut * 0.75)), 4);
    give(g, 'POL', T.tankDestroyers);
    // One division of infantry to two of tanks covers them all; less covers a part.
    const thin = addCorps(g, 'GER', 'Thin', 'inf', 1, l.from.id, {}), full = addCorps(g, 'GER', 'Full', 'inf', 3, l.from.id, {});
    for (const c of [thin, full]) c.supplied = true;
    expect(at(g, l, [...l.att, thin]).cover).toBeCloseTo(2 / 6, 6);
    expect(at(g, l, [...l.att, thin]).alone).toBeCloseTo(A.cut * 0.9 * (4 / 6), 6);
    expect(faceLines(faced(g, l, [...l.att, thin]))[0].text).toBe('Too little infantry with your tanks: their guns take half of the tanks\' attack.');
    expect(at(g, l, [...l.att, full]).alone).toBe(0);
    expect(row(g, l, 'Tanks alone', [...l.att, full])).toBeUndefined();
    // Better armour takes its step off the guns here too.
    give(g, 'GER', T.armour[2]);
    expect(at(g, l).gunned).toBeCloseTo(0.9 - S.armourStep, 6);
    take(g, 'GER', T.armour[2]);
    // Never twice: in a forest the old cut for tanks alone (15%) gives way where this is the larger, and stands where it is not.
    l.target.terrain = 'forest';
    expect(faced(g, l).alone).toBeCloseTo(0.63, 6);
    expect(assess(g, l.att, l.target).lines.find((x) => x.label === 'Tanks alone')).toEqual({ label: 'Tanks alone', value: 'Tank attack cut by 63%', effect: 'This takes the place of the 15% cut for tanks alone in close country' });
    const k = playing('GER');
    const n = line(k, undefined, [['arm', 3]]);
    works(n.def, DONE);
    expect(faced(k, n).alone).toBeCloseTo(0.14, 6);
    const open = assess(k, n.att, n.target).A;
    n.target.terrain = 'forest';
    expect(faced(k, n).alone).toBe(0);
    expect(row(k, n, 'Tanks alone')).toBeUndefined();
    expect(assess(k, n.att, n.target).A).toBeLessThan(open);
    // Where the ground has already stopped the tanks there is no second cut.
    l.target.terrain = 'marsh';
    expect(faced(g, l).alone).toBe(0);
    expect(row(g, l, 'Tanks alone')).toBeUndefined();
    // A defence of tanks alone has no guns for the purpose: tank against tank needs no infantry.
    const h = playing('GER');
    h.turn = january(1943);
    const m = line(h, [['arm', 3]], [['arm', 3], ['arm', 3]]);
    works(m.def, DONE);
    expect(at(h, m).strength).toBe(1);
    expect(at(h, m).gunned).toBe(0);
    expect(at(h, m).alone).toBe(0);
  });

  it('gives machine guns their worth against men on foot: most in finished fieldworks, less in the open, nothing against tanks', () => {
    const g = playing('GER');
    const l = line(g);
    const M = SUPPORT.machineGuns;
    const d = (att: Corps[] = l.att): number => assess(g, att, l.target).D;
    const tanks = addCorps(g, 'GER', 'Panzer', 'arm', 3, l.from.id, {});
    tanks.supplied = true;
    const base = d(), baseTanks = d([tanks]), baseMixed = d([l.att[0], tanks]);
    give(g, 'POL', T.support);
    expect(d() / base).toBeCloseTo(1 + M.defence * M.open, 6);
    expect(d([tanks]) / baseTanks).toBeCloseTo(1, 6);
    expect(d([l.att[0], tanks]) / baseMixed).toBeCloseTo(1 + M.defence * M.open * 0.5, 6);
    expect(row(g, l, 'Machine guns')).toBe('Defence up 5% against the men on foot');
    take(g, 'POL', T.support);
    works(l.def, DONE);
    const dug = d();
    give(g, 'POL', T.support);
    expect(d() / dug).toBeCloseTo(1 + M.defence, 6);
    expect(row(g, l, 'Machine guns')).toBe('Defence up 12% against the men on foot');
  });

  it('lets the attack\'s mortars take part of the worth out of fieldworks, and never out of fortress works', () => {
    const g = playing('GER');
    const l = line(g);
    works(l.def, DONE);
    const before = assess(g, l.att, l.target);
    expect(row(g, l, 'Mortars')).toBeUndefined();
    give(g, 'GER', T.support);
    const after = assess(g, l.att, l.target);
    expect(after.prepared).toBeCloseTo(before.prepared * (1 - SUPPORT.mortars.works), 6);
    expect(after.D).toBeLessThan(before.D);
    expect(row(g, l, 'Mortars')).toBe('The fieldworks count for 15% less');
    expect(after.faced!.worksCut).toBeCloseTo(SUPPORT.mortars.works, 6);
    // Tanks carry no mortars: with half the attack on foot they take half as much.
    const tanks = addCorps(g, 'GER', 'Panzer', 'arm', 3, l.from.id, {});
    expect(assess(g, [l.att[0], tanks], l.target).faced!.worksCut).toBeCloseTo(SUPPORT.mortars.works / 2, 6);
    // Concrete: a fortress with nobody dug in defends the same against mortars as without them.
    const h = playing('GER');
    const m = line(h);
    m.target.fort = { lv: 3, vs: [h.regions[m.att[0].reg].owner] };
    const plain = assess(h, m.att, m.target);
    give(h, 'GER', T.support);
    const shelled = assess(h, m.att, m.target);
    expect(shelled.D).toBeCloseTo(plain.D, 6);
    expect(shelled.A).toBeCloseTo(plain.A, 6);
    expect(shelled.faced!.worksCut).toBe(0);
  });

  it('makes men on foot with assault rifles attack stronger, and tanks no stronger', () => {
    const g = playing('GER');
    const l = line(g, undefined, [['inf', 3], ['arm', 3]]);
    const [foot, tanks] = l.att;
    const a = (c: Corps): number => assess(g, [c], l.target).A;
    const f0 = a(foot), t0 = a(tanks);
    give(g, 'GER', T.handHeld);
    expect(a(foot) / f0).toBeCloseTo(1 + SUPPORT.assault.attack, 6);
    expect(a(tanks) / t0).toBeCloseTo(1, 6);
    expect(row(g, l, 'Assault rifles', [foot])).toBe('Infantry attack up 8%');
    expect(row(g, l, 'Assault rifles', [tanks])).toBeUndefined();
  });

  it('charges the tanks by the anti-tank strength, from seven tenths of the usual share to nearly half as much again', () => {
    const g = playing('GER');
    const l = line(g, undefined, [['arm', 3]]);
    expect(faced(g, l).toll.tanks).toBeCloseTo(SUPPORT.toll.tanks[0], 6);
    works(l.def, DONE);
    expect(faced(g, l).toll.tanks).toBeCloseTo(0.7 + 0.7 * 0.2, 6);
    const h = playing('GER');
    const m = line(h, [['inf', 2], ['arm', 2]], [['arm', 3]]);
    expect(faced(h, m).toll.tanks).toBeCloseTo(SUPPORT.toll.tanks[1], 6);
  });

  it('charges the men on foot for machine guns, guns in the line and snipers, less the shield of tanks that can work', () => {
    const L = SUPPORT.toll;
    const g = playing('GER');
    const l = line(g, [['inf', 3], ['art', 2]]);
    expect(faced(g, l).toll.foot).toBeCloseTo(1 + 2 * L.guns, 6);
    l.def[1].divs = 9;
    expect(faced(g, l).toll.why.guns).toBeCloseTo(L.gunsMost, 6);
    // Machine guns: a defence all of machine-gunners in finished works.
    const h = playing('GER');
    const m = line(h);
    give(h, 'POL', T.support);
    expect(faced(h, m).toll.foot).toBeCloseTo(1 + L.machineGuns * SUPPORT.machineGuns.open, 6);
    works(m.def, DONE);
    expect(faced(h, m).toll.foot).toBeCloseTo(1 + L.machineGuns, 6);
    expect(faced(h, m).toll.why).toEqual({ machineGuns: L.machineGuns, guns: 0, snipers: 0, shield: 0 });
    // Tanks beside the infantry, half the attack and more: the shield, as far as the anti-tank guns let them work.
    const tanks = addCorps(h, 'GER', 'Panzer', 'arm', 6, m.from.id, {});
    const f = faced(h, m, [...m.att, tanks]);
    expect(f.toll.why.shield).toBeCloseTo(L.shield * (1 - f.antiTank!.strength), 6);
    expect(f.toll.foot).toBeCloseTo((1 + L.machineGuns) * (1 - f.toll.why.shield), 6);
    // The attackers' own guns stand behind.
    expect(f.toll.guns).toBe(L.behind);
    const guns = addCorps(h, 'GER', 'Guns', 'art', 2, m.from.id, {});
    expect(tollFor(f.toll, guns)).toBe(L.behind);
    expect(tollFor(f.toll, tanks)).toBe(f.toll.tanks);
    expect(tollFor(f.toll, m.att[0])).toBe(f.toll.foot);
    expect(tollFor(null, tanks)).toBe(1);
  });

  it('has snipers in forest, in a city and in the streets of a town, from the first month and with no research', () => {
    const g = playing('GER');
    const l = line(g);
    expect(faced(g, l).snipers).toBeNull();
    expect(faced(g, l).toll.foot).toBe(1);
    l.target.terrain = 'forest';
    expect(faced(g, l).snipers).toBe('forest');
    expect(faced(g, l).toll.why.snipers).toBe(SUPPORT.snipers.toll);
    expect(faced(g, l).toll.foot).toBeCloseTo(1 + SUPPORT.snipers.toll, 6);
    l.target.terrain = 'urban';
    expect(faced(g, l).snipers).toBe('city');
    expect(hasTech(g, 'POL', T.support)).toBe(false);
    // An attack with no men on foot is told of no snipers: there is nobody for them to shoot.
    const h = playing('GER');
    const m = line(h, [['inf', 3], ['inf', 3]], [['arm', 3]]);
    m.target.terrain = 'forest';
    expect(faced(h, m).snipers).toBeNull();
    expect(faceLines(faced(h, m)).some((x) => x.sign === 'sniper')).toBe(false);
  });

  it('makes the losses fall that way: against machine guns in finished works the infantry loses more of itself than the tanks beside it', () => {
    const g = playing('GER');
    const l = line(g, [['inf', 3], ['inf', 3], ['inf', 3]], [['inf', 3], ['arm', 3]]);
    give(g, 'POL', T.support);
    works(l.def, DONE);
    const f = faced(g, l);
    expect(f.toll.foot).toBeGreaterThan(f.toll.tanks);
    const [foot, tanks] = l.att;
    const rep = resolveBattle(g, l.att, l.target);
    const lostFoot = 1 - foot.str, lostTanks = 1 - tanks.str;
    expect(lostFoot).toBeGreaterThan(0);
    expect(lostFoot / lostTanks).toBeCloseTo(f.toll.foot / f.toll.tanks, 2);
    expect(rep.paid).toBeTruthy();
    expect(rep.paid!.att).toMatch(/^Your infantry paid most of it, to their machine guns\. The tanks' shock counted /);
    expect(rep.paid!.def).toMatch(/^Their infantry paid most of it, to your machine guns\. Their tanks' shock counted /);
    expect(rep.paid!.sign).toBe('machine-gun');
  });
});

describe('what the cards say', () => {
  const toll = (over: Partial<Toll> = {}): Toll => ({ tanks: 1, foot: 1, guns: 0.5, why: { machineGuns: 0, guns: 0, snipers: 0, shield: 0 }, ...over });
  const strength = (over: Partial<AntiTank> = {}): AntiTank => ({ strength: 0, guns: 0, kit: true, destroyers: 0, handHeld: 0, artillery: 0, mines: 0, works: 0, fort: 0, tanks: 0, armour: 0, gunned: 0, cover: 1, alone: 0, ...over });
  const face = (over: Partial<Faced> = {}): Faced => ({ antiTank: strength(), toll: toll(), shock: null, alone: 0, machineGuns: false, mgDug: false, worksCut: 0, mortars: false, gunsIn: false, snipers: null, ...over });

  it('gives a share in words', () => {
    expect([1, 0.75, 0.66, 0.5, 0.33, 0.25, 0.1, 0].map(shareWords)).toEqual(['in full', 'for three quarters', 'for two thirds', 'for half', 'for a third', 'for a quarter', 'for a tenth', 'for nothing']);
  });

  it('names what stands against the tanks, biggest first and three at most', () => {
    expect(antiTankParts(strength({ guns: 0.5, mines: 0.16, works: 0.1, fort: 2, artillery: 0.04, destroyers: 0.02 }))).toEqual(['anti-tank guns', 'mines', 'dragon\'s teeth']);
    expect(antiTankParts(strength({ works: 0.05, fort: 1 }))).toEqual(['a ditch']);
    expect(antiTankParts(strength({ guns: 0.02 }))).toEqual([]);
  });

  it('writes WHAT YOU FACE: a line for each thing the defence will do to what he brings, four at most', () => {
    expect(faceLines(face())).toEqual([]);
    expect(faceLines(face({ shock: 1 }))).toEqual([{ sign: 'anti-tank', text: 'Few anti-tank guns: your tanks\' shock counts in full.' }]);
    const all = faceLines(face({
      antiTank: strength({ strength: 0.66, guns: 0.5, mines: 0.16 }), shock: 0.34, machineGuns: true, mgDug: true, worksCut: 0.33, mortars: true, gunsIn: true,
      snipers: 'forest', toll: toll({ why: { machineGuns: 0.25, guns: 0, snipers: 0.1, shield: 0 } }),
    }));
    expect(all).toEqual([
      { sign: 'anti-tank', text: 'Anti-tank guns and mines: your tanks\' shock counts for a third.' },
      { sign: 'machine-gun', text: 'Machine guns, dug in: most of your losses will be infantry.' },
      { sign: 'mortar', text: 'Fieldworks: your mortars and guns take a third of them away.' },
      { sign: 'sniper', text: 'Snipers in the forest: more of your losses will be infantry.' },
    ]);
    expect(faceLines(face({ machineGuns: true }))[0].text).toBe('Machine guns in the open: more of your losses will be infantry.');
    expect(faceLines(face({ worksCut: 0.15, mortars: true }))[0].text).toBe('Fieldworks: your mortars take a tenth of them away.');
    expect(faceLines(face({ snipers: 'city', toll: toll({ why: { machineGuns: 0, guns: 0, snipers: 0.1, shield: 0 } }) }))[0].text).toBe('Snipers in the city: more of your losses will be infantry.');
  });

  it('writes who paid, as the attacker reads it and as the defender does', () => {
    const mg = face({ toll: toll({ foot: 1.25, why: { machineGuns: 0.25, guns: 0, snipers: 0, shield: 0 } }) });
    expect(whoPaid(mg, { foot: 9000, tanks: 1000, guns: 0 }, true)).toEqual({ text: 'Your infantry paid most of it, to their machine guns.', sign: 'machine-gun' });
    expect(whoPaid(mg, { foot: 9000, tanks: 1000, guns: 0 }, false).text).toBe('Their infantry paid most of it, to your machine guns.');
    const guns = face({ antiTank: strength({ strength: 0.66, guns: 0.5, mines: 0.16 }), shock: 0.34, toll: toll({ tanks: 1.16 }) });
    expect(whoPaid(guns, { foot: 2000, tanks: 7000, guns: 0 }, true)).toEqual({ text: 'Your tanks paid most of it, to their anti-tank guns and mines. The tanks\' shock counted for a third.', sign: 'anti-tank' });
    expect(whoPaid(guns, { foot: 2000, tanks: 7000, guns: 0 }, false).text).toBe('Their tanks paid most of it, to your anti-tank guns and mines. Their tanks\' shock counted for a third.');
    expect(whoPaid(face({ shock: 1 }), { foot: 5000, tanks: 5000, guns: 0 }, true)).toEqual({ text: 'The cost fell evenly on your corps. The tanks\' shock counted in full.', sign: null });
    expect(whoPaid(face({ shock: 0 }), { foot: 5000, tanks: 5000, guns: 0 }, true).text).toBe('The cost fell evenly on your corps. The tanks\' shock counted for nothing.');
    expect(whoPaid(face(), { foot: 0, tanks: 0, guns: 0 }, false).text).toBe('The cost fell evenly on their corps.');
    // Two sentences at most, and never a figure.
    for (const t of [whoPaid(guns, { foot: 2000, tanks: 7000, guns: 0 }, true).text, whoPaid(mg, { foot: 1, tanks: 0, guns: 0 }, true).text]) {
      expect(t.split('. ').length).toBeLessThanOrEqual(2);
      expect(t).not.toMatch(/\d/);
    }
  });

  it('shows the weapons a corps carries: anti-tank from 0 to 3, machine guns and mortars with their technology, snipers always', () => {
    const g = playing('GER');
    const l = line(g, undefined, [['inf', 3], ['arm', 3], ['art', 2]]);
    expect(carried(g, l.att[0])).toEqual({ antiTank: 0, machineGuns: false, mortars: false, snipers: true });
    expect(carried(g, l.def[0])).toEqual({ antiTank: 1, machineGuns: false, mortars: false, snipers: true });
    give(g, 'POL', T.tankDestroyers, T.handHeld, T.support);
    expect(carried(g, l.def[0])).toEqual({ antiTank: 3, machineGuns: true, mortars: true, snipers: true });
    expect(carried(g, l.att[1])).toBeNull();
    expect(carried(g, l.att[2])).toBeNull();
  });

  it('puts the figures in the ticket: who pays, as shares of the usual losses', () => {
    const g = playing('GER');
    const l = line(g, undefined, [['inf', 3], ['arm', 3], ['art', 2]]);
    give(g, 'POL', T.support);
    works(l.def, DONE);
    const f = faced(g, l);
    const pc = (x: number): string => `${Math.round(x * 100)}%`;
    expect(row(g, l, 'Who pays')).toBe(`Of the usual share of losses: tanks ${pc(f.toll.tanks)}, men on foot ${pc(f.toll.foot)} and guns ${pc(f.toll.guns)}`);
    // An attack all on foot against a defence with nothing special pays the usual share: no row.
    const h = playing('GER');
    const m = line(h);
    expect(row(h, m, 'Who pays')).toBeUndefined();
  });

  it('tells the player once, the first time a line takes a third or more out of the tanks\' shock', () => {
    const g = playing('GER');
    const l = line(g, [['inf', 3], ['arm', 1]], [['arm', 3], ['inf', 3]]);
    expect(faced(g, l).shock).toBeLessThanOrEqual(2 / 3);
    const tips = (): number => g.headlines.filter((n) => /anti-tank guns, mines, ditches and artillery stop tanks as well as tanks do/.test(n.text)).length;
    expect(tips()).toBe(0);
    resolveBattle(g, l.att, l.target);
    expect(tips()).toBe(1);
    expect(g.flags['tip:antitank']).toBe(g.turn);
    const again = line(g, [['inf', 3], ['arm', 1]], [['arm', 3], ['inf', 3]]);
    resolveBattle(g, again.att, again.target);
    expect(tips()).toBe(1);
  });
});

describe('what the player\'s reports know of it', () => {
  /** A line the player has in view and has not identified, with works, mines and machine guns in it. */
  function hidden(): { g: Game; l: Line } {
    const g = playing('GER');
    const l = line(g, undefined, [['arm', 3], ['inf', 3]]);
    give(g, 'POL', T.support);
    works(l.def, DONE);
    return { g, l };
  }

  it('tells everything of a defence that is identified, and with the fog lifted', () => {
    const { g, l } = hidden();
    const a = assess(g, l.att, l.target);
    expect(gunsKnown(g, a)).toBe(false);
    identify(g, l.def, 'GER');
    expect(gunsKnown(g, a)).toBe(true);
    expect(whatYouFace(g, a, l.target)).toEqual(faceLines(a.faced!));
    expect(whatYouFace(g, a, l.target)!.map((x) => x.sign)).toEqual(['anti-tank', 'machine-gun']);
    const { g: h, l: m } = hidden();
    setFog(h, false);
    const b = assess(h, m.att, m.target);
    expect(whatYouFace(h, b, m.target)).toEqual(faceLines(b.faced!));
  });

  it('names the works he can see across the line and says the guns are not known', () => {
    const { g, l } = hidden();
    expect(sightOf(g)[l.target.id]).toBe(SIGHT.full);
    let lines = whatYouFace(g, assess(g, l.att, l.target), l.target)!;
    // Fieldworks alone are no work against tanks: nothing of the guns, the machine guns or the shock is told.
    expect(lines).toEqual([GUNS_AND_MINES_UNKNOWN]);
    expect(GUNS_AND_MINES_UNKNOWN.text).toBe('Not scouted: their guns and mines are unknown.');
    // His own mortars against works he can see are his to know.
    give(g, 'GER', T.support);
    lines = whatYouFace(g, assess(g, l.att, l.target), l.target)!;
    expect(lines.map((x) => x.sign)).toEqual(['mortar', 'scouted']);
    // Dragon's teeth and a ditch are seen from across the line.
    l.target.fort = { lv: 2, vs: [g.regions[l.att[0].reg].owner] };
    lines = whatYouFace(g, assess(g, l.att, l.target), l.target)!;
    expect(lines[0]).toEqual({ sign: 'anti-tank', text: 'Dragon\'s teeth: they take a tenth of your tanks\' shock, before their guns are counted.' });
    expect(lines.map((x) => x.sign)).toEqual(['anti-tank', 'mortar', 'scouted']);
    l.target.fort = { lv: 1, vs: [g.regions[l.att[0].reg].owner] };
    expect(whatYouFace(g, assess(g, l.att, l.target), l.target)![0].text).toBe('A ditch: it takes a twentieth of your tanks\' shock, before their guns are counted.');
    // Snipers are the ground's own promise. (In a city the tanks' shock is the streets' to stop: no line about it.)
    l.target.terrain = 'urban';
    lines = whatYouFace(g, assess(g, l.att, l.target), l.target)!;
    expect(lines.map((x) => x.sign)).toEqual(['mortar', 'sniper', 'scouted']);
    expect(lines.every((x) => !/machine/i.test(x.text))).toBe(true);
  });

  it('hides mines with the guns: on foreign ground they are named only with a report', () => {
    const { g, l } = hidden();
    g.flags[`mine:${l.target.id}`] = 1;
    expect(minesKnown(g, l.target)).toBe(false);
    let lines = whatYouFace(g, assess(g, l.att, l.target), l.target)!;
    expect(lines).toEqual([GUNS_AND_MINES_UNKNOWN]);
    // An agent at work there is a report: the mines are named and only the guns are left unknown.
    const agent: Agent = { id: g.intel.next++, nat: 'GER', name: 'Test agent', xp: 0, reg: l.target.id, ready: g.turn, mission: 'watch', born: g.turn };
    g.intel.agents.push(agent);
    expect(minesKnown(g, l.target)).toBe(true);
    lines = whatYouFace(g, assess(g, l.att, l.target), l.target)!;
    expect(lines).toEqual([{ sign: 'anti-tank', text: 'Mines: they take a tenth of your tanks\' shock, before their guns are counted.' }, GUNS_UNKNOWN]);
    expect(GUNS_UNKNOWN.text).toBe('Not scouted: their guns are unknown.');
    // His own ground and his ally's are always known.
    expect(minesKnown(g, l.from)).toBe(true);
    // One corps of the two identified is a report of the ground, though not of all its guns.
    g.intel.agents.pop();
    identify(g, [l.def[0]], 'GER');
    expect(minesKnown(g, l.target)).toBe(true);
    expect(gunsKnown(g, assess(g, l.att, l.target))).toBe(false);
  });

  it('tells the same of ground out of view: works are on every staff\'s map, and there are two closing sentences and no third', () => {
    const g = playing('GER');
    const l = line(g, undefined, [['arm', 3], ['inf', 3]]);
    const far = g.regions.find((r) => sightOf(g)[r.id] === SIGHT.none && r.ctrl !== 'GER' && g.nations[r.ctrl]?.alive)!;
    expect(far).toBeTruthy();
    const def = addCorps(g, far.ctrl, 'Far line', 'inf', 3, far.id, {});
    def.supplied = true;
    expect(whatYouFace(g, assess(g, l.att, far), far)).toEqual([GUNS_AND_MINES_UNKNOWN]);
    far.fort = { lv: 2, vs: [g.regions[l.att[0].reg].owner] };
    g.flags[`mine:${far.id}`] = 1;
    expect(whatYouFace(g, assess(g, l.att, far), far)!.map((x) => x.text)).toEqual([
      'Dragon\'s teeth: they take a tenth of your tanks\' shock, before their guns are counted.', GUNS_AND_MINES_UNKNOWN.text,
    ]);
  });

  it('keeps out of the attack card\'s ticket what the reports do not know', () => {
    const { g, l } = hidden();
    g.flags[`mine:${l.target.id}`] = 1;
    const labels = (): string[] => attackOutlook(g, l.att, l.target)!.lines.map((x) => x.label);
    const armour = (): string | undefined => attackOutlook(g, l.att, l.target)!.lines.find((x) => x.label === 'Armor')?.value;
    expect(attackOutlook(g, l.att, l.target)!.face).toEqual([GUNS_AND_MINES_UNKNOWN]);
    expect(labels()).not.toContain('Machine guns');
    expect(labels()).not.toContain('Who pays');
    expect(labels()).not.toContain('Minefields');
    expect(armour()).toBe('Tank attack up by as much as 25%: their guns are not known');
    // Tanks sent alone at guns that are not known: the most the guns could take, on the card first and in the ticket.
    const lone = (): ReturnType<typeof attackOutlook> => attackOutlook(g, [l.att[0]], l.target);
    expect(lone()!.lines.find((x) => x.label === 'Tanks alone')?.value).toBe('Tank attack cut by as much as 70%: their guns are not known');
    expect(lone()!.face!.map((x) => x.text)).toEqual(['No infantry with your tanks: their guns take as much as two thirds of the tanks\' attack.', GUNS_AND_MINES_UNKNOWN.text]);
    expect(labels()).not.toContain('Tanks alone');
    identify(g, l.def, 'GER');
    expect(lone()!.lines.find((x) => x.label === 'Tanks alone')?.value).toBe('Tank attack cut by 14%');
    expect(lone()!.face![0].text).toBe('No infantry with your tanks: their guns take a tenth of the tanks\' attack.');
    expect(labels()).toContain('Machine guns');
    expect(labels()).toContain('Who pays');
    expect(labels()).toContain('Minefields');
    expect(armour()).toMatch(/^Tank attack up \d+%$/);
    expect(attackOutlook(g, l.att, l.target)!.face!.map((x) => x.sign)).toEqual(['anti-tank', 'machine-gun']);
  });
});

describe('one cause, one place on the attack card (ruling 166)', () => {
  it('where WHAT YOU FACE says why, the Dug in row keeps its figure alone, with what the attack takes off the works', () => {
    const g = playing('GER');
    const l = line(g);
    works(l.def, DONE);
    give(g, 'POL', T.support);
    const a = assess(g, l.att, l.target);
    const full = a.lines.find((x) => x.label === 'Dug in')!;
    expect(full.value).toContain('their works are finished');
    expect(full.effect).toContain('The day runs against the attack');
    const face = whatYouFace(g, a, l.target, null)!;
    expect(face.some((x) => !x.unknown)).toBe(true);
    const brief = briefTicket(a.lines, face);
    const row = brief.find((x) => x.label === 'Dug in')!;
    expect(row.value).toMatch(/^Troops in fieldworks defend \d+% stronger$/);
    expect(row.effect ?? 'Won or lost').toMatch(/^Won or lost/);
    expect(row.effect ?? '').not.toContain('The day runs against');
    // Every other row stands as it was, in its place.
    expect(brief.map((x) => x.label)).toEqual(a.lines.map((x) => x.label));
    for (const [i, x] of brief.entries()) if (x.label !== 'Dug in') expect(x).toBe(a.lines[i]);
    // With nothing stated above it (a history campaign's card, or only the line in italics) the row stands in full.
    expect(briefTicket(a.lines, null)).toBe(a.lines);
    expect(briefTicket(a.lines, [GUNS_AND_MINES_UNKNOWN])).toBe(a.lines);
  });
});

describe('the computer\'s staffs', () => {
  it('do not send tanks alone at a gunned line when infantry can go in with them, and never at a strongly gunned one', () => {
    const g = playing('POL');
    g.turn = january(1943);
    const l = line(g, undefined, [['arm', 3], ['arm', 3], ['inf', 3], ['inf', 2]]);
    give(g, 'GER', T.armour[0], T.armour[1]);
    const [p1, p2, i1, i2] = l.att;
    // In the open in 1943 the line is gunned (a half): the best infantry at hand goes in with the tanks.
    expect(at(g, l, [p1, p2]).strength).toBeGreaterThanOrEqual(SUPPORT.staff.never);
    expect(notTanksAlone(g, [p1, p2], l.att, l.target)).toEqual([p1, p2, i1]);
    // With no infantry to send, tanks alone do not go at a line that strong.
    expect(notTanksAlone(g, [p1, p2], [p1, p2], l.target)).toBeNull();
    // An attack that already has men on foot in it, or no tanks, is left as it was picked.
    expect(notTanksAlone(g, [p1, i2], l.att, l.target)).toEqual([p1, i2]);
    expect(notTanksAlone(g, [i1, i2], l.att, l.target)).toEqual([i1, i2]);
    // Between the two marks tanks may still go alone when there is nobody to go with them.
    take(g, 'POL', T.antiTank);
    const s = at(g, l, [p1, p2]).strength;
    expect(s).toBeLessThan(SUPPORT.staff.never);
    expect(s).toBeGreaterThanOrEqual(0.25);
    if (s >= SUPPORT.staff.gunned) expect(notTanksAlone(g, [p1, p2], [p1, p2], l.target)).toEqual([p1, p2]);
    // A line with few guns is the tanks' own business.
    const h = playing('POL');
    const m = line(h, undefined, [['arm', 3], ['inf', 3]]);
    expect(at(h, m, [m.att[0]]).strength).toBeLessThan(SUPPORT.staff.gunned);
    expect(notTanksAlone(h, [m.att[0]], m.att, m.target)).toEqual([m.att[0]]);
  });

  it('hold their marks at the printed value, whatever the machine\'s arithmetic makes of the sum', () => {
    // 1943 in the open (a half), two divisions of guns in the line (0.08), and armour one design ahead of its day
    // (0.08 off): 0.58 less 0.08 is a half on the card, and a hair under it in the machine. (The 48 wars: 17 and 22
    // attacks by tanks alone slipped through here.)
    const g = playing('POL');
    g.turn = january(1943);
    const l = line(g, [['inf', 3], ['inf', 3], ['art', 2]], [['arm', 3], ['arm', 3]]);
    give(g, 'GER', ...T.armour);
    const s = at(g, l).strength;
    expect(Math.abs(s - SUPPORT.staff.never)).toBeLessThan(1e-9);
    expect(notTanksAlone(g, l.att, l.att, l.target)).toBeNull();
  });

  it('keep the rule for a counter-stroke, a relief and a first wave: infantry next door goes in, or nobody does', () => {
    const g = playing('POL');
    g.turn = january(1943);
    const l = line(g, undefined, [['arm', 3], ['arm', 2], ['inf', 3], ['inf', 2]]);
    give(g, 'GER', T.armour[0], T.armour[1]);
    const [p1, p2, i1, i2] = l.att;
    expect(at(g, l, [p1, p2]).strength).toBeGreaterThanOrEqual(SUPPORT.staff.never);
    // A force of tanks cut to the roads, with infantry standing next door: the best of it goes in beside them.
    const sent = footBeside(g, [p1, p2], l.att, l.target)!;
    expect(sent).toContain(i1);
    expect(limitAttackers(g, sent, l.target).length).toBe(sent.length);
    // Where the staff can run only two corps in a battle, the infantry takes the weaker tank corps' place.
    const most = g.nations.GER.maxAttack;
    g.nations.GER.maxAttack = 2;
    expect(footBeside(g, [p1, p2], l.att, l.target)).toEqual([p1, i1]);
    g.nations.GER.maxAttack = most;
    // Nobody on foot to send: the stroke is not made.
    expect(footBeside(g, [p1, p2], [p1, p2], l.target)).toBeNull();
    // A force that has its infantry already is left as it was.
    expect(footBeside(g, [p1, i2], l.att, l.target)).toEqual([p1, i2]);
    // A first wave of two is picked from the corps the main body does not need. Tanks alone are no first wave here.
    expect(twoWaves(g, l.att, [i1, i2], l.target, 0.01)).toBeNull();
    const two = twoWaves(g, l.att, [i1], l.target, 0.01);
    if (two) expect(two.corps.some((c) => c.kind === 'inf')).toBe(true);
    // A history campaign is worked out as it always was.
    const tanks = [p1, p2];
    g.flags.open = 0;
    expect(footBeside(g, tanks, tanks, l.target)).toBe(tanks);
    g.flags.open = 1;
  });

  it('want Anti-tank guns sooner once they are at war with an army that has tanks', () => {
    expect(TECH.ai.guns).toBeGreaterThan(0);
    const g = free(3);
    // Germany has tanks from the first month; whoever is at war with it faces them.
    const foe = Object.keys(g.nations).find((n) => g.nations[n].alive && g.nations[n].atWar.includes('GER'))!;
    expect(foe).toBeTruthy();
    expect(facesTanks(g, foe)).toBe(true);
    const neutral = Object.keys(g.nations).find((n) => g.nations[n].alive && g.nations[n].atWar.length === 0)!;
    expect(facesTanks(g, neutral)).toBe(false);
    // The pick itself: a staff with nothing else open that is cheaper takes the guns when it faces tanks, on the same month's list.
    const picks = (nat: string): string | null => staffPick(g, nat);
    const uk = g.nations.UK;
    if (uk?.tech && !hasTech(g, 'UK', T.antiTank)) {
      const before = picks('UK');
      const was = TECH.ai.guns;
      TECH.ai.guns = 1000;
      try { expect(picks('UK')).toBe(facesTanks(g, 'UK') ? T.antiTank : before); } finally { TECH.ai.guns = was; }
    }
  });

  it('are told on the technology cards what each weapon does', () => {
    expect(techRule(techById(T.antiTank)!)).toMatch(/keeps up with the anti-tank guns of the day: without this they count at 50% of their worth/);
    for (const id of [T.tankDestroyers, T.handHeld, T.support]) expect(techRule(techById(id)!).length, id).toBeGreaterThan(20);
  });
});

describe('a history campaign', () => {
  it('is worked out as it always was: nothing faced, no rows, no line on the report, no signs on a corps', () => {
    const g = createGame(3, 'GER');
    expect(supportRules(g)).toBe(false);
    const target = g.byName['Lodz'];
    const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
    const att = [addCorps(g, 'GER', 'Storm', 'inf', 3, from.id, {}), addCorps(g, 'GER', 'Panzer', 'arm', 3, from.id, {})];
    if (!target.units.length) addCorps(g, 'POL', 'Line', 'inf', 3, target.id, {});
    for (const c of [...att, ...target.units]) { c.supplied = true; c.ap = c.apMax; }
    const a = assess(g, att, target);
    expect(a.faced).toBeNull();
    for (const label of ['Machine guns', 'Mortars', 'Assault rifles', 'Who pays']) expect(a.lines.find((x) => x.label === label)).toBeUndefined();
    expect(whatYouFace(g, a, target)).toBeNull();
    expect(carried(g, att[0])).toBeNull();
    const rep = resolveBattle(g, att, target);
    expect(rep.paid).toBeUndefined();
    expect(g.flags['tip:antitank']).toBeUndefined();
  });
});
