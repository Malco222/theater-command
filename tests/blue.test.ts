// The orders a plan can give that were added for the second year of the war in the east: gathering the fast
// troops for an offensive, keeping a force massed at a place, handing a stretch of the line to an ally,
// paying a price of its own for one prize, standing quiet, giving ground up before it is cut off, and an
// event that brings a nation into a war.
import { describe, expect, it } from 'vitest';
import { STACK, KIND, type Corps, type Game, type Region } from '../src/campaign/types';
import { addCorps, atWar, createGame, friendlyUnits, hostileUnits, removeCorps, startPhase } from '../src/campaign/game';
import { phaseFor } from '../src/campaign/ai';
import { DIPLOMACY, monthlyDiplomacy } from '../src/campaign/diplomacy';
import { loadGame, saveGame } from '../src/campaign/save';
import { current, runTurnAI, stepAI } from '../src/campaign/turn';

const JUNE_41 = 22, SEPT_41 = 25, MARCH_42 = 31, JUNE_42 = 34, SEPT_42 = 37, OCT_42 = 38, NOV_42 = 39, DEC_42 = 40, JAN_43 = 41, FEB_43 = 42;

/** One war on the historical plan, remembered as each of these months opens, with Germany about to move. */
const opening: Record<number, Game> = {};
let whole: Game;
{
  const g = createGame(3, null);
  for (let t = 1; t < FEB_43; t++) {
    runTurnAI(g);
    if ([JUNE_41, SEPT_41, MARCH_42, JUNE_42, SEPT_42, OCT_42, NOV_42, DEC_42, JAN_43, FEB_43].includes(t + 1)) opening[t + 1] = loadGame(saveGame(g))!;
  }
  whole = g;
}
/** A fresh copy of the war as that month opens, for a test to play on from. */
const at = (t: number): Game => loadGame(saveGame(opening[t]))!;

const german = (r: Region): Corps[] => r.units.filter((u) => u.nat === 'GER' && u.kind !== 'fort');
const fast = (r: Region): Corps[] => german(r).filter((u) => KIND[u.kind].mobile);
const foot = (r: Region): Corps[] => german(r).filter((u) => !KIND[u.kind].mobile);
const of = (r: Region, nat: string): Corps[] => r.units.filter((u) => u.nat === nat);

describe('gathering the fast troops for an offensive', () => {
  const SOUTH = { Kursk: 2, Kharkov: 3, Stalino: 2 } as Record<string, number>;

  it('the plan for the spring of 1942 names where the tanks are to assemble', () => {
    const g = at(MARCH_42);
    expect(current(g)).toBe('GER');
    expect(phaseFor(g, 'GER')).toMatchObject({ posture: 'defend', concentrate: SOUTH });
    // Before the order they are scattered along the whole front.
    expect(Object.keys(SOUTH).reduce((s, n) => s + fast(g.byName[n]).length, 0)).toBeLessThanOrEqual(2);
  });

  it('they come by rail in a single month, and the line they leave still stands', () => {
    const g = at(MARCH_42);
    const before = new Map(g.corps.filter((c) => c.nat === 'GER' && KIND[c.kind].mobile).map((c) => [c.id, c.reg]));
    stepAI(g);
    for (const [name, want] of Object.entries(SOUTH)) expect(fast(g.byName[name]).length, name).toBeGreaterThanOrEqual(want);
    // Nobody left a place that faces the enemy held by fewer than two corps.
    const left = new Set<number>();
    for (const c of g.corps) if (before.has(c.id) && before.get(c.id) !== c.reg && SOUTH[g.regions[c.reg].name]) left.add(before.get(c.id)!);
    expect(left.size).toBeGreaterThan(2);
    for (const id of left) {
      const r = g.regions[id];
      const faced = r.nb.some((j) => hostileUnits(g, g.regions[j], 'GER').length > 0);
      if (faced) expect(german(r).length, r.name).toBeGreaterThanOrEqual(2);
    }
  });

  it('where the assembly area is full, infantry marches out to make room', () => {
    const g = at(MARCH_42);
    const kharkov = g.byName['Kharkov'];
    while (friendlyUnits(g, kharkov, 'GER').length < STACK) addCorps(g, 'GER', `Line corps ${kharkov.units.length}`, 'inf', 2, 'Kharkov', {});
    expect(fast(kharkov).length).toBe(0);
    stepAI(g);
    expect(fast(kharkov).length).toBeGreaterThanOrEqual(3);
    expect(friendlyUnits(g, kharkov, 'GER').length).toBeLessThanOrEqual(STACK);
  });

  it('and they are still there when the offensive opens', () => {
    const g = at(JUNE_42);
    expect(phaseFor(g, 'GER')).toMatchObject({ posture: 'attack', axis: ['Voronezh'] });
    expect(Object.keys(SOUTH).reduce((s, n) => s + fast(g.byName[n]).length, 0)).toBeGreaterThanOrEqual(6);
  });
});

describe('a force kept massed at a place', () => {
  it('the infantry on the land bridge stays there while the offensive goes on elsewhere', () => {
    const g = at(OCT_42);
    const kalach = g.byName['Kalach'];
    expect(kalach.ctrl).toBe('GER');
    expect(phaseFor(g, 'GER')!.staging).toMatchObject({ Kalach: 5 });
    const there = foot(kalach).map((c) => c.id);
    expect(there.length).toBeGreaterThanOrEqual(3);
    stepAI(g);
    for (const id of there) {
      const c = g.corps.find((x) => x.id === id);
      if (c) expect(g.regions[c.reg].name).toBe('Kalach');
    }
    // Elsewhere the army did move that month.
    expect(whole.reports.some((r) => r.turn === OCT_42 && r.attNat === 'GER' && r.regionName !== 'Stalingrad')).toBe(true);
  });

  it('a place short of its force draws the nearest infantry that can be spared, and no tanks', () => {
    const g = at(OCT_42);
    const kalach = g.byName['Kalach'];
    // Two corps of the garrison are taken away: two march in from the Don bend to replace them.
    for (const c of foot(kalach).slice(0, 2)) removeCorps(g, c);
    const tanks = fast(kalach).length;
    stepAI(g);
    expect(german(kalach).length).toBeGreaterThanOrEqual(4);
    expect(fast(kalach).length).toBeLessThanOrEqual(tanks);
  });
});

describe('a stretch of the line handed to an ally', () => {
  it('is Germany\'s own to hold until the ally has two corps there', () => {
    const g = at(SEPT_42);
    expect(phaseFor(g, 'GER')!.entrust).toMatchObject({ ROM: ['Don Bend', 'Kalmyk Steppe'], ITA: ['Millerovo'], HUN: ['Voronezh'] });
    const bend = g.byName['Don Bend'];
    expect(of(bend, 'ROM').length).toBe(0);
    stepAI(g);
    expect(german(bend).length).toBeGreaterThanOrEqual(2);
  });

  it('and once the ally is there Germany takes its own corps out, all but one', () => {
    const g = at(OCT_42);
    const bend = g.byName['Don Bend'], millerovo = g.byName['Millerovo'];
    expect(of(bend, 'ROM').length).toBe(2);
    expect(german(bend).length).toBeGreaterThanOrEqual(2);
    stepAI(g);
    expect(german(bend).length).toBeLessThanOrEqual(1);
    expect(of(bend, 'ROM').length).toBe(2);
    expect(of(millerovo, 'ITA').length).toBe(3);
    expect(german(millerovo).length).toBeLessThanOrEqual(1);
    // The enemy is still next door: it is the ally, not the danger, that lets Germany go.
    expect(bend.nb.some((j) => hostileUnits(g, g.regions[j], 'GER').length > 0)).toBe(true);
  });
});

describe('troops sent to take over a stretch of the line', () => {
  it('go in beside whoever stands there, however crowded the place is', () => {
    const g = at(SEPT_42);
    while (current(g) !== 'ROM') stepAI(g);
    const bend = g.byName['Don Bend'];
    while (friendlyUnits(g, bend, 'ROM').length < STACK) addCorps(g, 'GER', `Line corps ${bend.units.length}`, 'inf', 2, 'Don Bend', {});
    startPhase(g, 'ROM');
    expect(of(bend, 'ROM').map((c) => c.name).sort()).toEqual(['3rd Army: I and II Corps', '3rd Army: IV and V Corps']);
    expect(bend.units.length).toBe(STACK + 2);
    // They are there to hold, not to manoeuvre.
    for (const c of of(bend, 'ROM')) expect(c.hold).toBe(true);
  });

  it('any other reinforcement waits until there is room for it', () => {
    const g = at(FEB_43);
    const poltava = g.byName['Poltava'];
    expect(poltava.ctrl).toBe('GER');
    while (friendlyUnits(g, poltava, 'GER').length < STACK) addCorps(g, 'GER', `Line corps ${poltava.units.length}`, 'inf', 2, 'Poltava', {});
    startPhase(g, 'GER');
    expect(g.corps.some((c) => c.name === 'Panzer Corps from France')).toBe(false);
    removeCorps(g, poltava.units[0]);
    startPhase(g, 'GER');
    expect(g.corps.find((c) => c.name === 'Panzer Corps from France')?.reg).toBe(poltava.id);
  });
});

describe('a prize the plan wants at a price of its own', () => {
  // A battle report gives the odds after the dice, which move them by a fifth either way.
  const fights = whole.reports.filter((r) => r.attNat === 'GER' && r.turn >= SEPT_42 && r.turn <= NOV_42 && whole.regions[r.region].owner === 'SOV');

  it('Stalingrad is assaulted at odds the plan would refuse anywhere else', () => {
    const ph = phaseFor(at(OCT_42), 'GER')!;
    expect(ph.minOdds).toBe(1.1);
    expect(ph.oddsFor).toMatchObject({ Stalingrad: 0.45 });
    const city = fights.filter((r) => r.regionName === 'Stalingrad');
    expect(city.length).toBeGreaterThan(0);
    for (const r of city) {
      expect(r.odds).toBeLessThan(1.1 * 0.8);
      expect(r.odds).toBeGreaterThanOrEqual(0.45 * 0.8);
      expect(r.taken).toBe(false);
    }
  });

  it('and nothing else is attacked below the plan\'s usual odds', () => {
    const others = fights.filter((r) => r.regionName !== 'Stalingrad');
    expect(others.length).toBeGreaterThan(2);
    for (const r of others) expect(r.odds, r.regionName).toBeGreaterThanOrEqual(1.0 * 0.8);
  });
});

describe('an army told to stand quiet', () => {
  it('makes no attack of any kind, whatever ground it loses', () => {
    const g = at(JUNE_42);
    expect(phaseFor(g, 'SOV')).toMatchObject({ posture: 'defend', quiet: true });
    const held = (x: Game): number => x.regions.filter((r) => r.owner === 'SOV' && r.ctrl !== 'SOV').length;
    expect(held(opening[NOV_42])).toBeGreaterThan(held(opening[JUNE_42]) + 5);
    expect(whole.reports.filter((r) => r.attNat === 'SOV' && r.turn >= JUNE_42 && r.turn < NOV_42)).toEqual([]);
  });

  it('and strikes when the plan lets it go', () => {
    const blows = whole.reports.filter((r) => r.attNat === 'SOV' && r.turn === NOV_42);
    expect(blows.map((r) => r.regionName).sort()).toEqual(['Don Bend', 'Kalmyk Steppe']);
    for (const r of blows) expect(r.taken).toBe(true);
  });
});

describe('ground the plan gives up', () => {
  it('the corps there march out toward home before the road is cut', () => {
    const g = at(DEC_42);
    expect(phaseFor(g, 'GER')).toMatchObject({ posture: 'defend', abandon: ['Terek'] });
    const terek = g.byName['Terek'], stavropol = g.byName['Stavropol'];
    const there = german(terek).map((c) => c.id);
    expect(there.length).toBeGreaterThanOrEqual(2);
    // The enemy is on three sides and the one road home runs through Stavropol.
    expect(terek.nb.filter((j) => g.regions[j].ctrl === 'GER').map((j) => g.regions[j].name)).toEqual(['Stavropol']);
    stepAI(g);
    expect(german(terek).length).toBe(0);
    for (const id of there) expect(g.corps.find((c) => c.id === id)!.reg).toBe(stavropol.id);
  });

  it('the places nearest home empty first, and nobody is sent back in', () => {
    const g = at(JAN_43);
    expect(phaseFor(g, 'GER')!.abandon).toEqual(['Terek', 'Stavropol', 'Salsk']);
    const stavropol = g.byName['Stavropol'];
    const before = german(stavropol).length;
    expect(before).toBeGreaterThanOrEqual(4);
    expect(stavropol.nb.some((j) => hostileUnits(g, g.regions[j], 'GER').length > 0)).toBe(true);
    stepAI(g);
    // A threatened place would otherwise draw every reserve within reach.
    expect(german(stavropol).length).toBeLessThanOrEqual(1);
    // They went back to the Kuban and the coast, not into the sea.
    const coast = ['Kuban', 'Maikop', 'Novorossiysk'].reduce((s, n) => s + german(g.byName[n]).length, 0);
    const was = ['Kuban', 'Maikop', 'Novorossiysk'].reduce((s, n) => s + german(opening[JAN_43].byName[n]).length, 0);
    expect(coast).toBeGreaterThan(was);
  });
});

describe('an army cut off from its supply', () => {
  it('is news the month the ring closes, and is told once', () => {
    const kalach = whole.byName['Kalach'];
    const lines = whole.headlines.filter((h) => h.region === kalach.id && /^The ring closes/.test(h.text));
    expect(lines.length).toBe(1);
    expect(lines[0].turn).toBe(NOV_42);
    expect(lines[0].nat).toBe('GER');
    const m = /^The ring closes at Kalach: (\d+) German corps, ([\d,]+) men, are cut off from all supply\.$/.exec(lines[0].text);
    expect(m).not.toBeNull();
    // The corps counted are the ones standing there, unsupplied, as the next month opens.
    const trapped = german(opening[DEC_42].byName['Kalach']);
    expect(trapped.every((c) => !c.supplied)).toBe(true);
    expect(+m![1]).toBe(trapped.length);
    expect(+m![1]).toBeGreaterThanOrEqual(3);
  });

  it('a corps or two caught behind the lines is not', () => {
    const g = at(DEC_42);
    const kalach = g.byName['Kalach'];
    delete g.flags[`cut:${kalach.id}`];
    const [, , ...rest] = german(kalach);
    for (const c of rest) removeCorps(g, c);
    const before = g.headlines.length;
    startPhase(g, 'GER');
    expect(g.headlines.slice(before).filter((h) => /^The ring closes/.test(h.text))).toEqual([]);
  });
});

describe('an army starved into surrender', () => {
  it('is one headline for the place, with its corps and its men counted', () => {
    const kalach = whole.byName['Kalach'];
    const lines = whole.headlines.filter((h) => h.turn === JAN_43 && h.region === kalach.id && /lays down its arms/.test(h.text));
    expect(lines.length).toBe(1);
    const m = /^Cut off and out of supply, the German army at Kalach lays down its arms: (\d+) corps, ([\d,]+) men\.$/.exec(lines[0].text);
    expect(m).not.toBeNull();
    // The corps in the pocket as the month opened are the ones counted (a last one may fall in the final assault), and none is left on the map.
    const trapped = german(opening[JAN_43].byName['Kalach']);
    expect(+m![1]).toBeGreaterThanOrEqual(3);
    expect(+m![1]).toBeLessThanOrEqual(trapped.length);
    expect(+m![2].replace(/,/g, '')).toBeGreaterThan(60000);
    for (const c of trapped) expect(whole.corps.some((x) => x.id === c.id)).toBe(false);
  });

  it('a single corps lost that way is still named', () => {
    const g = at(JAN_43);
    const kalach = g.byName['Kalach'];
    const [keep, ...rest] = german(kalach).sort((a, b) => b.str - a.str);
    for (const c of rest) removeCorps(g, c);
    keep.str = Math.max(keep.str, 0.9);
    keep.org = 0.1;
    startPhase(g, 'GER');
    const line = g.headlines.filter((h) => h.turn === JAN_43 && h.region === kalach.id && /lays down/.test(h.text)).map((h) => h.text);
    expect(line).toEqual([`Cut off and out of supply, ${keep.name} lays down its arms at Kalach.`]);
  });
});

describe('an event that brings a nation into a war', () => {
  it('Italy follows Germany into the war in the east', () => {
    expect(atWar(opening[JUNE_41], 'ITA', 'SOV')).toBe(false);
    expect(atWar(opening[SEPT_41], 'ITA', 'SOV')).toBe(true);
    expect(opening[SEPT_41].headlines.some((h) => h.text.startsWith('Rome follows Berlin into the war in the east'))).toBe(true);
  });

  it('but not into a war Germany has not begun', () => {
    const g = createGame(1, null);
    const event = DIPLOMACY.events.find((e) => e.id === 'italy-east')!;
    expect(event.war).toEqual([['ITA', 'SOV']]);
    g.turn = event.from;
    monthlyDiplomacy(g);
    expect(atWar(g, 'ITA', 'SOV')).toBe(false);
  });
});
