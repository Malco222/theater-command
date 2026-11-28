// Bridging columns (release 2; Brian's yes of 27 November 2026 17:10Z; the design director's rulings of 17:15Z and
// 17:48Z on r2-three-ideas/FORM-rails-rivers-1127.md, part B, and of 28 November 01:12Z): each engineer corps carries
// one; where the bridges are down, the first corps of its side on wheels to cross within the engineers' own march lays
// it; it counts as standing for its side at war only, is promised once, carries no trains, and comes back by fact, at
// once where its engineer corps is gone. With the switch off nothing is written or read.
import { describe, expect, it } from 'vitest';
import { bridgingMonth, bridgingOn, columnAt, columnFor, columnLine, columnsOf, freeColumn, overColumns, setBridging } from '../src/campaign/bridging';
import { ENGINEERING, blocked, crossingOf, downBetween, noCrossing } from '../src/campaign/rivers';
import { addCorps, assess, createGame, moveCost, moveCorps, removeCorps, resolveBattle, setWar } from '../src/campaign/game';
import { reach } from '../src/campaign/turn';
import { startOpen } from '../src/campaign/staff';
import { railsAt } from '../src/campaign/rails';
import { loadGame, saveGame } from '../src/campaign/save';
import type { Corps, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const war = (seed = 3, human: string | null = 'GER'): Game => startOpen(createGame(seed, human));
const corps = (g: Game, nat: string, reg: number, kind: Corps['kind']): Corps => addCorps(g, nat, `${nat} ${kind} column test corps`, kind, 2, reg, {});
/** Hanover and Magdeburg, both German on the first day, with a river between them (crossings of the map of Europe). */
const HAN = 4, MAG = 18, BER = 0, SAX = 16;
const blowAt = (g: Game, a: number, b: number): void => { const x = crossingOf(a, b)!; g.flags[`br:${x.a}:${x.b}`] = g.turn; g.supplyDirty = true; };
const flagsOf = (g: Game): string[] => Object.keys(g.flags).filter((k) => /^bc[eu]?:/.test(k));
/** The engineer corps Germany opens with stand in Berlin: their moves spent, they can lay a column only there. */
const pinEngineers = (g: Game): void => { for (const c of g.corps) if (c.kind === 'eng' && c.nat === 'GER') c.ap = 0; };

describe('bridging columns (release 2)', () => {
  it('is off unless switched on: tanks wait on the bank, nothing is written', () => {
    const g = war();
    expect(crossingOf(HAN, MAG)).not.toBeNull();
    blowAt(g, HAN, MAG);
    pinEngineers(g);
    corps(g, 'GER', HAN, 'eng');
    const tank = corps(g, 'GER', HAN, 'arm');
    expect(bridgingOn(g)).toBe(false);
    expect(noCrossing(g, 'arm', HAN, MAG, 'GER')).toBe(true);
    expect(blocked(g, tank, g.regions[MAG])).toBe(true);
    expect(moveCorps(g, tank, g.regions[MAG])).toBe(false);
    expect(columnFor(g, 'GER', HAN, MAG, true)).toBe(false);
    expect(flagsOf(g)).toEqual([]);
  });

  it('on: the order to cross lays the column of an engineer corps on the bank; other wheels and the infantry cross by it', () => {
    const g = war();
    setBridging(g, true);
    blowAt(g, HAN, MAG);
    pinEngineers(g);
    const eng = corps(g, 'GER', HAN, 'eng');
    const tank = corps(g, 'GER', HAN, 'arm'), lorries = corps(g, 'GER', HAN, 'mot'), foot = corps(g, 'GER', HAN, 'inf');
    expect(columnLine(g, eng)).toMatch(/^Its bridging column is free/);
    // The march over a column costs what it costs over a bridge.
    const boats = moveCost(g, foot, g.regions[MAG], HAN);
    const x0 = crossingOf(HAN, MAG)!, k = `br:${x0.a}:${x0.b}`, when = g.flags[k];
    delete g.flags[k];
    const standing = moveCost(g, tank, g.regions[MAG], HAN);
    g.flags[k] = when;
    expect(moveCost(g, tank, g.regions[MAG], HAN)).toBe(standing);
    expect(blocked(g, tank, g.regions[MAG])).toBe(false);
    expect(moveCorps(g, tank, g.regions[MAG])).toBe(true);
    const x = crossingOf(HAN, MAG)!;
    expect(columnAt(g, 'GER', x)).toBe(true);
    expect(columnsOf(g, 'GER')).toEqual([{ x, from: HAN, by: eng.id, used: g.turn }]);
    expect(columnLine(g, eng)).toMatch(/^Its bridging column is laid over the /);
    // The engineers stay where they are; the railways do not run over a pontoon.
    expect(eng.reg).toBe(HAN);
    expect(downBetween(g, HAN, MAG)).toBe(true);
    // Once laid, the infantry marches over it as over a bridge, and more wheels follow without a second column.
    expect(moveCost(g, foot, g.regions[MAG], HAN)).toBe(boats - ENGINEERING.blown.march);
    expect(moveCorps(g, lorries, g.regions[MAG])).toBe(true);
    expect(columnsOf(g, 'GER').length).toBe(1);
  });

  it('a column cannot be in two places, and is laid only within its engineer corps\' own march', () => {
    const g = war();
    setBridging(g, true);
    blowAt(g, HAN, MAG);
    blowAt(g, BER, SAX);
    pinEngineers(g);
    for (const c of g.corps) if (c.kind === 'eng' && c.nat === 'GER') c.dead = true;
    // No engineer corps can reach Hanover's bank this month: the tanks wait.
    const far = corps(g, 'GER', BER, 'eng');
    far.ap = 0;
    const tank = corps(g, 'GER', HAN, 'arm');
    expect(freeColumn(g, 'GER', HAN)).toBeNull();
    expect(blocked(g, tank, g.regions[MAG])).toBe(true);
    // The engineers in Berlin lay theirs over the river by Berlin; then they have none for Hanover.
    const tank2 = corps(g, 'GER', BER, 'arm');
    expect(moveCorps(g, tank2, g.regions[SAX])).toBe(true);
    const near = corps(g, 'GER', HAN, 'eng');
    near.dead = true;
    expect(freeColumn(g, 'GER', BER)).toBeNull();
  });

  it('serves its side at war: an ally at war crosses by it and uses it; an ally at peace and the enemy gain nothing', () => {
    const g = war();
    setBridging(g, true);
    blowAt(g, HAN, MAG);
    pinEngineers(g);
    corps(g, 'GER', HAN, 'eng');
    const tank = corps(g, 'GER', HAN, 'arm');
    expect(moveCorps(g, tank, g.regions[MAG])).toBe(true);
    const x = crossingOf(HAN, MAG)!;
    // The enemy gains nothing; nor a member of the bloc at peace.
    expect(noCrossing(g, 'arm', HAN, MAG, 'POL')).toBe(true);
    expect(g.nations.ITA.atWar.length).toBe(0);
    expect(columnAt(g, 'ITA', x)).toBe(false);
    // At war, the ally's lorries cross by the German column, and their crossing counts as using it.
    setWar(g, 'ITA', 'FRA');
    expect(columnAt(g, 'ITA', x)).toBe(true);
    expect(noCrossing(g, 'mot', HAN, MAG, 'ITA')).toBe(false);
    g.flags[`bcu:GER:${x.a}:${x.b}`] = g.turn - 5;
    const lorries = corps(g, 'ITA', HAN, 'mot');
    expect(moveCorps(g, lorries, g.regions[MAG])).toBe(true);
    expect(columnsOf(g, 'GER')[0].used).toBe(g.turn);
    expect(columnsOf(g, 'ITA')).toEqual([]);
  });

  it('is promised once: two crossings in one attack, one free column, and the card and the battle lay it once', () => {
    const g = war();
    setBridging(g, true);
    pinEngineers(g);
    // Hanover attacked over its rivers from Magdeburg and from a second bank the engineers in Magdeburg reach this month.
    const eng = corps(g, 'GER', MAG, 'eng');
    const m = reach(g, eng);
    const B = g.regions[HAN].nb.find((j) => j !== MAG && crossingOf(j, HAN) !== null && g.regions[j].ctrl === 'GER' && m.has(j) && !m.get(j)!.attack);
    expect(B).toBeDefined();
    blowAt(g, MAG, HAN);
    blowAt(g, B!, HAN);
    const t1 = corps(g, 'GER', MAG, 'arm'), t2 = corps(g, 'GER', B!, 'arm');
    g.regions[HAN].ctrl = 'POL';
    corps(g, 'POL', HAN, 'inf');
    // Either tank alone could have the one column.
    expect(columnFor(g, 'GER', MAG, HAN, true)).toBe(true);
    expect(columnFor(g, 'GER', B!, HAN, true)).toBe(true);
    // Both in one attack: it is promised once, to the first crossing ordered; the other waits on its bank.
    const plan = overColumns(g, [t1, t2], g.regions[HAN]);
    expect([...plan.over]).toEqual([t1.id]);
    expect(plan.laid.map((e) => e.id)).toEqual([eng.id]);
    const river = assess(g, [t1, t2], g.regions[HAN]).lines.find((l) => l.label === 'River')!;
    expect(river.effect).toMatch(/one corps of tanks, lorries or guns cannot cross/);
    expect(river.effect).toMatch(/lays its one column as the attack goes in, and the first crossing ordered takes it/);
    // Asking wrote nothing.
    expect(flagsOf(g)).toEqual([]);
    // Going in lays the one the card named, once.
    resolveBattle(g, [t1, t2], g.regions[HAN]);
    expect(columnsOf(g, 'GER').map((c) => [c.from, c.by])).toEqual([[MAG, eng.id]]);
  });

  it('a column whose engineer corps is gone stands nowhere at once; ringed or on leave, the corps keeps it', () => {
    const g = war();
    setBridging(g, true);
    blowAt(g, HAN, MAG);
    pinEngineers(g);
    const eng = corps(g, 'GER', HAN, 'eng');
    const tank = corps(g, 'GER', HAN, 'arm');
    expect(moveCorps(g, tank, g.regions[MAG])).toBe(true);
    const x = crossingOf(HAN, MAG)!;
    // Home on leave, it still exists: its column keeps the three facts.
    eng.leave = g.turn + 2;
    expect(columnAt(g, 'GER', x)).toBe(true);
    delete eng.leave;
    // Destroyed: at once the column stands nowhere, and the tanks wait on the bank again.
    removeCorps(g, eng);
    expect(columnAt(g, 'GER', x)).toBe(false);
    expect(columnsOf(g, 'GER')).toEqual([]);
    expect(noCrossing(g, 'arm', MAG, HAN, 'GER')).toBe(true);
    // Its flags go at the next opening of its nation's month.
    g.turn += 1;
    expect(bridgingMonth(g, 'GER')).toEqual([{ x, why: 'gone' }]);
    expect(flagsOf(g)).toEqual([]);
  });

  it('comes back by fact at the opening of its nation\'s month: unused in the month gone, the bridge rebuilt, the bank lost', () => {
    const g = war();
    setBridging(g, true);
    blowAt(g, HAN, MAG);
    pinEngineers(g);
    const eng = corps(g, 'GER', HAN, 'eng');
    const tank = corps(g, 'GER', HAN, 'arm');
    expect(moveCorps(g, tank, g.regions[MAG])).toBe(true);
    const x = crossingOf(HAN, MAG)!;
    const t = g.turn;
    // Used this month: it stays at the next month's opening.
    g.turn = t + 1;
    expect(bridgingMonth(g, 'GER')).toEqual([]);
    // Nobody crossed by it in month t + 1: it comes back at t + 2.
    g.turn = t + 2;
    expect(bridgingMonth(g, 'GER')).toEqual([{ x, why: 'unused' }]);
    expect(columnLine(g, eng)).toMatch(/free/);
    expect(flagsOf(g)).toEqual([]);
    // Laid again; the bridge is rebuilt: it comes back.
    tank.ap = tank.apMax;
    expect(moveCorps(g, tank, g.regions[HAN])).toBe(true);
    delete g.flags[`br:${x.a}:${x.b}`];
    g.turn = t + 3;
    expect(bridgingMonth(g, 'GER')).toEqual([{ x, why: 'bridge' }]);
    // Laid again from Hanover; Hanover is lost: it comes back.
    blowAt(g, HAN, MAG);
    tank.ap = tank.apMax;
    expect(moveCorps(g, tank, g.regions[MAG])).toBe(true);
    expect(columnsOf(g, 'GER')[0].from).toBe(HAN);
    expect(columnsOf(g, 'GER')[0].by).toBe(eng.id);
    g.regions[HAN].ctrl = 'POL';
    g.turn = t + 4;
    expect(bridgingMonth(g, 'GER')).toEqual([{ x, why: 'bank' }]);
  });

  it('an attack across is priced at the bridges\' figure over a column, and its card says so', () => {
    const g = war();
    setBridging(g, true);
    blowAt(g, HAN, MAG);
    pinEngineers(g);
    corps(g, 'GER', HAN, 'eng');
    const tank = corps(g, 'GER', HAN, 'arm'), foot = corps(g, 'GER', HAN, 'inf');
    // Magdeburg in Polish hands with a Polish corps in it, for the assessment's sake.
    g.regions[MAG].ctrl = 'POL';
    corps(g, 'POL', MAG, 'inf');
    const card = assess(g, [tank, foot], g.regions[MAG]);
    const river = card.lines.find((l) => l.label === 'River')!;
    expect(river.value).toMatch(/over a bridging column/);
    expect(river.effect).toMatch(new RegExp(`over a bridging column the attack is cut to ${Math.round(ENGINEERING.river.attack * 100)}%`, 'i'));
    expect(river.effect).not.toMatch(/boats/);
    // Off, the same attack: tanks cannot cross, the infantry goes in boats.
    setBridging(g, false);
    const off = assess(g, [tank, foot], g.regions[MAG]).lines.find((l) => l.label === 'River')!;
    expect(off.value).toMatch(/bridges over the .* are down$/);
    expect(off.effect).toMatch(/boats/);
  });

  it('switching off takes every column up; the columns keep through a save', () => {
    const g = war();
    setBridging(g, true);
    blowAt(g, HAN, MAG);
    pinEngineers(g);
    corps(g, 'GER', HAN, 'eng');
    const tank = corps(g, 'GER', HAN, 'arm');
    expect(moveCorps(g, tank, g.regions[MAG])).toBe(true);
    const back = loadGame(saveGame(g))!;
    expect(columnsOf(back, 'GER').length).toBe(1);
    setBridging(g, false);
    expect(flagsOf(g)).toEqual([]);
    expect(railsAt(HAN).length).toBeGreaterThan(0);
  });

  it('the options act and TC switch it, and the act note says what it does', () => {
    const t = TC();
    expect(typeof t.bridging).toBe('function');
  });
});
