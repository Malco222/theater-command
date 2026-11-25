// Version 78, the design director's ruling 58, on two finds of the Playtester's in Version 76.
// (a) No declaration switches America off. A small Axis country that declared war on the United States early made
//     it an Ally at war with that country alone, and December 1941 never came: the entry asked that America stand on
//     no side. Now every entry tests the war it brings, never peace with everyone, and a computer-led America at war
//     with a member of the Axis and on no side joins the Allies that month.
// (b) A League is a pact of neighbours: a founder or a member may ask only countries of its own part of the world.
import { describe, expect, it } from 'vitest';
import worldData from '../data/campaign/world.json';
import { runAI } from '../src/campaign/ai';
import { DIPLOMACY, closeDiplomacy, holds, monthlyDiplomacy } from '../src/campaign/diplomacy';
import { addCorps, atWar, bloc, capture, createGame, declareWar, setWar } from '../src/campaign/game';
import { enrol, founders, invites, partName, partOf, pathOptions, takePath, willing } from '../src/campaign/league';
import { frontAshore, startOpen } from '../src/campaign/staff';
import { FAR_STAGE } from '../src/campaign/stage';
import type { Game } from '../src/campaign/types';
import { WORLD, armyIdle, beyondEurope, enemyAtTheDoor, overseasOf } from '../src/campaign/world';

const DEC_41 = 28;
/** A grand campaign in which Germany and Italy are at war with Britain, as they were by 1941. */
function war(human: string | null = null, seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  setWar(g, 'GER', 'UK');
  setWar(g, 'ITA', 'UK');
  return g;
}
const said = (g: Game, re: RegExp): boolean => g.headlines.some((h) => re.test(h.text));
/**
 * A month's events: its opening, and its close. With the far side on the entry of December 1941 is read as its month
 * closes and names Pearl Harbor only where Japan has struck (ruling 197); on the older chart the close reads nothing.
 */
const month = (g: Game): void => { monthlyDiplomacy(g); closeDiplomacy(g); };
const ENTRY = FAR_STAGE ? /America has entered the war/ : /Pearl Harbor/;

describe('the entries of the wider world test the war they bring (ruling 58a)', () => {
  it('is written that way in the data: no entry asks for peace with everyone, or that its country stand on no side', () => {
    for (const id of ['pearl-harbor-world', 'cuba-war', 'mexico-war', 'brazil-war']) {
      const e = DIPLOMACY.events.find((x) => x.id === id)!;
      expect(e, id).toBeDefined();
      expect((e.if ?? []).some((c) => c.startsWith('atPeace:') || c.startsWith('notInBloc:')), id).toBe(false);
      expect((e.if ?? []).some((c) => c.startsWith('openTo:')), id).toBe(true);
      expect((e.if ?? []).some((c) => c.startsWith('peace:') || c.startsWith('anyPeace:')), id).toBe(true);
    }
  });

  it('brings December 1941 as it always did to an America that stayed out', () => {
    const g = war();
    expect(bloc(g.nations.USA)).toBeNull();
    g.turn = DEC_41 - 1;
    month(g);
    expect(atWar(g, 'USA', 'GER')).toBe(false);
    g.turn = DEC_41;
    month(g);
    expect(g.nations.USA.align).toBe('allies');
    expect(atWar(g, 'USA', 'GER')).toBe(true);
    expect(atWar(g, 'USA', 'ITA')).toBe(true);
    expect(g.flags['event:pearl-harbor-world']).toBe(DEC_41);
    expect(g.headlines.filter((h) => ENTRY.test(h.text))).toHaveLength(1);
  });

  it('does not let an early Axis declaration switch America off: December 1941 still brings the wars not begun', () => {
    // The Playtester's case: a player leads Brazil into the Axis and declares war on the United States in 1940.
    const g = war('BRA');
    g.nations.BRA.align = 'axis';
    g.turn = 13;
    declareWar(g, 'BRA', 'USA');
    // Struck by the Axis, America is an Ally from that day, at war with Brazil alone.
    expect(g.nations.USA.align).toBe('allies');
    expect(g.nations.USA.atWar).toEqual(['BRA']);
    g.turn = DEC_41;
    month(g);
    expect(atWar(g, 'USA', 'GER')).toBe(true);
    expect(atWar(g, 'USA', 'ITA')).toBe(true);
    expect(g.nations.USA.align).toBe('allies');
    expect(g.flags['event:pearl-harbor-world']).toBe(DEC_41);
    // The news is told once, with the first war it brings.
    expect(g.headlines.filter((h) => ENTRY.test(h.text))).toHaveLength(1);
    // And the republics that follow America follow it still.
    expect(atWar(g, 'CUB', 'GER')).toBe(true);
    expect(atWar(g, 'PAN', 'GER')).toBe(true);
  });

  it('does not let a hostile country beyond the sea keep America\'s army at home: only an invader or a hostile neighbour does', () => {
    const O = WORLD.overseas.USA;
    // The Playtester's case again, with America run by the computer: an Axis Brazil at war with it since 1940.
    const g = war();
    g.nations.BRA.align = 'axis';
    g.turn = 13;
    declareWar(g, 'BRA', 'USA');
    expect(g.regions.some((r) => r.ctrl === 'BRA' && r.nb.some((j) => g.regions[j].ctrl === 'USA'))).toBe(false);
    expect(enemyAtTheDoor(g, 'USA')).toBe(false);
    // At war, with shipping, and no enemy it can march at: the army is for the far shore, and no more is raised than the ships will carry.
    expect(overseasOf(g, 'USA')).toBe(O);
    for (let k = 0; k < 40; k++) addCorps(g, 'USA', `Idle Corps ${k}`, 'inf', 1, 'Chicago', {});
    expect(armyIdle(g, 'USA')).toBe(true);
    // Brazil's conquests beyond the water change nothing: an island or a colony lost is not a road for the army.
    const guiana = g.byName['French Guiana'];
    expect(guiana).toBeDefined();
    capture(g, guiana, 'BRA');
    expect(overseasOf(g, 'USA')).toBe(O);
    // A hostile neighbour across a land frontier is the army's first business.
    const m = war();
    setWar(m, 'USA', 'MEX');
    expect(enemyAtTheDoor(m, 'USA')).toBe(true);
    expect(overseasOf(m, 'USA')).toBeNull();
    expect(armyIdle(m, 'USA')).toBe(false);
    // So is an invader from across the ocean ashore anywhere on its side of the water, an island included.
    const inv = war();
    setWar(inv, 'USA', 'GER');
    expect(overseasOf(inv, 'USA')).toBe(O);
    const cuba = inv.regions.find((r) => r.owner === 'CUB')!;
    expect(cuba.nb.some((j) => inv.regions[j].ctrl === 'USA')).toBe(false);
    capture(inv, cuba, 'GER');
    expect(enemyAtTheDoor(inv, 'USA')).toBe(true);
    expect(overseasOf(inv, 'USA')).toBeNull();
    // A country with no war and a player's own country are as they were.
    expect(overseasOf(war(), 'USA')).toBeNull();
    expect(enemyAtTheDoor(war(), 'USA')).toBe(false);
  });

  it('sends no corps back across the ocean from the far shore: the army gathered in Britain is not fed into Brazil a corps at a time', () => {
    const g = war();
    g.turn = 30;
    declareWar(g, 'GER', 'USA');
    g.nations.BRA.align = 'axis';
    declareWar(g, 'BRA', 'USA');
    expect(g.nations.USA.atWar).toEqual(expect.arrayContaining(['GER', 'BRA']));
    g.nations.USA.sealift = Math.max(g.nations.USA.sealift, 4);
    // A harbour Britain holds beside Brazil is, by the rule for fronts, a front its side holds near the enemy.
    const guiana = g.byName['British Guiana'];
    expect(guiana.port && guiana.ctrl === 'UK' && guiana.nb.some((j) => g.regions[j].ctrl === 'BRA')).toBe(true);
    expect(frontAshore(g, 'USA')?.ports ?? []).toContain(guiana);
    // Corps across the ocean, standing idle in Britain where the ocean run put them down.
    const there = [0, 1, 2].map((k) => addCorps(g, 'USA', `Corps ${k}`, 'inf', 2, 'Cornwall', {}));
    for (const c of there) g.flags[`sf:${c.id}`] = 1;
    runAI(g, 'USA');
    for (const c of there) expect(beyondEurope(g.regions[c.reg]), `${c.name} at ${g.regions[c.reg].name}`).toBe(false);
    expect(g.corps.filter((c) => c.nat === 'USA' && !c.dead && g.regions[c.reg].owner === 'BRA')).toEqual([]);
  });

  it('gives an Axis that declares on America early the same America, early: at war with Germany, then with Italy in its turn', () => {
    const g = war();
    g.turn = 1;
    declareWar(g, 'GER', 'USA');
    expect(g.nations.USA.align).toBe('allies');
    expect(atWar(g, 'USA', 'ITA')).toBe(false);
    g.turn = DEC_41;
    month(g);
    expect(atWar(g, 'USA', 'ITA')).toBe(true);
    expect(atWar(g, 'USA', 'GER')).toBe(true);
  });

  it('makes a computer-led America at war with a member of the Axis and on no side an Ally that month', () => {
    const g = war();
    // A quarrel that began before its enemy took a side: no declaration moved America then.
    setWar(g, 'USA', 'BRA');
    expect(bloc(g.nations.USA)).toBeNull();
    g.turn = 6;
    month(g);
    expect(bloc(g.nations.USA)).toBeNull();
    g.nations.BRA.align = 'axis';
    month(g);
    expect(g.nations.USA.align).toBe('allies');
    expect(said(g, /United States joins the Allies: it is at war with Brazil, and the Axis is one enemy/)).toBe(true);
    // A player who leads America keeps that choice for himself.
    const p = war('USA');
    setWar(p, 'USA', 'BRA');
    p.nations.BRA.align = 'axis';
    p.turn = 6;
    month(p);
    expect(bloc(p.nations.USA)).toBeNull();
  });

  it('brings the war with Germany to a player whose America fights Italy alone, except at the looser stops of the dial', () => {
    const usual = war('USA');
    setWar(usual, 'USA', 'ITA');
    usual.nations.USA.align = 'allies_cobelligerent';
    usual.turn = DEC_41;
    expect(holds(usual, 'binds:USA')).toBe(true);
    month(usual);
    expect(atWar(usual, 'USA', 'GER')).toBe(true);
    expect(usual.nations.USA.align).toBe('allies');
    // Neutrals with ambitions (stops 4 and 5): the war he chose stays the war he has.
    for (const stop of [4, 5]) {
      const loose = war('USA');
      loose.flags.hist = stop;
      setWar(loose, 'USA', 'ITA');
      loose.nations.USA.align = 'allies_cobelligerent';
      loose.turn = DEC_41;
      expect(holds(loose, 'binds:USA')).toBe(false);
      month(loose);
      expect(atWar(loose, 'USA', 'GER')).toBe(false);
      expect(loose.nations.USA.align).toBe('allies_cobelligerent');
      // An America that has taken no side is still brought in there, as before.
      const out = war('USA');
      out.flags.hist = stop;
      out.turn = DEC_41;
      expect(holds(out, 'binds:USA')).toBe(true);
    }
  });

  it('lets a country already fighting somebody else follow America into the war all the same', () => {
    const g = war();
    setWar(g, 'CUB', 'HAI');
    setWar(g, 'MEX', 'GUA');
    setWar(g, 'BRA', 'PAR');
    g.turn = 36;
    setWar(g, 'USA', 'GER');
    g.nations.USA.align = 'allies';
    month(g);
    for (const id of ['CUB', 'MEX', 'BRA']) {
      expect(atWar(g, id, 'GER'), id).toBe(true);
      expect(g.nations[id].align, id).toBe('allies');
    }
  });

  it('moves nobody who stands in the other camp, in the League, or in a player\'s own hands', () => {
    const g = war('BRA');
    g.nations.CUB.align = 'axis';
    enrol(g, g.nations.MEX);
    g.turn = 36;
    setWar(g, 'USA', 'GER');
    g.nations.USA.align = 'allies';
    month(g);
    expect(atWar(g, 'CUB', 'GER')).toBe(false);
    expect(g.nations.CUB.align).toBe('axis');
    expect(atWar(g, 'MEX', 'GER')).toBe(false);
    expect(g.nations.MEX.league).toBe(true);
    // The player's Brazil chooses for itself.
    expect(atWar(g, 'BRA', 'GER')).toBe(false);
    expect(bloc(g.nations.BRA)).toBeNull();
  });
});

describe('a League is a pact of neighbours (ruling 58b)', () => {
  it('knows which part of the world every country lies in', () => {
    for (const id of Object.keys(worldData.nations)) expect(partOf(id), id).toBe('americas');
    expect(partOf('USA')).toBe('americas');
    for (const id of ['BEL', 'NED', 'SWE', 'TUR', 'EGY', 'GER', 'UK']) expect(partOf(id), id).toBe('europe');
    expect(partName('BRA')).toBe('the Americas');
    expect(partName('BEL')).toBe('Europe and the Mediterranean');
  });

  it('lets Brazil ask only the Americas, and Belgium only Europe', () => {
    const g = startOpen(createGame(3, 'BRA'));
    const mine = founders(g, 'BRA');
    expect(mine.length).toBeGreaterThan(0);
    expect(mine.every((x) => partOf(x.nat) === 'americas')).toBe(true);
    expect(mine.some((x) => x.nat === 'ARG')).toBe(true);
    const no = willing(g, 'BEL', ['BRA']);
    expect(no.ok).toBe(false);
    expect(no.why).toBe('A pact of neighbours: Brazil lies in the Americas, another part of the world.');
    expect(takePath(g, 'BRA', 'found', 'BEL')).toBe(false);
    expect(g.flags['league:born']).toBeUndefined();
    // With a neighbour it is founded as before.
    const partner = mine.find((x) => x.ok);
    if (partner) {
      expect(takePath(g, 'BRA', 'found', partner.nat)).toBe(true);
      expect(invites(g, 'BRA').every((x) => partOf(x.nat) === 'americas')).toBe(true);
      expect(willing(g, 'NED', ['BRA', partner.nat]).ok).toBe(false);
    }
    const e = startOpen(createGame(3, 'BEL'));
    const theirs = founders(e, 'BEL');
    expect(theirs.length).toBeGreaterThan(0);
    expect(theirs.every((x) => partOf(x.nat) === 'europe')).toBe(true);
    expect(willing(e, 'ARG', ['BEL']).why).toBe('A pact of neighbours: Belgium lies in Europe and the Mediterranean, another part of the world.');
  });

  it('offers a player no place in a League of another part of the world', () => {
    const g = startOpen(createGame(3, 'BRA'));
    g.flags['league:born'] = g.turn;
    enrol(g, g.nations.BEL);
    enrol(g, g.nations.NED);
    const join = pathOptions(g, 'BRA').find((o) => o.id === 'join')!;
    expect(join.ok).toBe(false);
    expect(join.why).toBe('The League is a pact of neighbours, and its members lie in Europe and the Mediterranean.');
    expect(takePath(g, 'BRA', 'join')).toBe(false);
    const d = startOpen(createGame(3, 'DEN'));
    d.flags['league:born'] = d.turn;
    enrol(d, d.nations.BEL);
    enrol(d, d.nations.NED);
    expect(pathOptions(d, 'DEN').find((o) => o.id === 'join')!.ok).toBe(true);
  });
});
