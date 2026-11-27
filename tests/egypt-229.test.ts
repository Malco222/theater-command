// Ruling 229, part 1: the British staff answers the push on Egypt. A floor at home from the record binds every corps
// that leaves Britain by sea, and at the looser stops of the history dial the staff sails its spare field corps to
// Egypt while the desert's threat outweighs home's. 1c: the desert's Commonwealth rows of the grand campaign.
import { describe, expect, it } from 'vitest';
import { addCorps, capitulate, capture, createGame, declareWar, fieldArmy, removeCorps, startPhase, voyagesOf } from '../src/campaign/game';
import { upkeep } from '../src/campaign/economy';
import { menSpare } from '../src/campaign/manpower';
import { eliteCount } from '../src/campaign/traits';
import { closedTo } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { runAI } from '../src/campaign/ai';
import { setStop } from '../src/campaign/history';
import { MIDDLE_EAST, homeFloor, homeThreat, homeland, mayLeave, theatreFront } from '../src/campaign/middleeast';
import { startOpen } from '../src/campaign/staff';
import setup from '../data/campaign/setup-1939.json';
import theatres from '../data/campaign/theatres.json';
import type { Corps, Game, Region } from '../src/campaign/types';

const field = (g: Game, c: Corps): boolean => c.nat === 'UK' && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && !c.hold;
const atHome = (g: Game): Corps[] => g.corps.filter((c) => field(g, c) && homeland(g.regions[c.reg], 'UK'));
const inEgypt = (g: Game): Corps[] => g.corps.filter((c) => c.nat === 'UK' && !c.dead && ['Alexandria', 'Port Said'].includes(g.regions[c.reg].name));

/** A grand campaign at a stop of the history dial, at war with Italy, in the month given, with an army raised at home. */
function grand(stop: number, turn = 12, seed = 3, italy = true): Game {
  const g = startOpen(createGame(seed, null));
  setStop(g, stop, true);
  g.turn = turn;
  if (italy) declareWar(g, 'ITA', 'UK');
  ['Kent', 'Hampshire', 'London', 'Wales', 'Lancashire', 'Northumbria'].forEach((nm, i) => addCorps(g, 'UK', `Home Corps ${i + 1}`, 'inf', 2, nm, {}));
  return g;
}

/** Runs a test with the crossing's danger allowed up to this (the Mediterranean made safe for the troopships). */
function safeSea<T>(risk: number, fn: () => T): T {
  const R = MIDDLE_EAST.reinforce.UK, was = R.risk;
  R.risk = risk;
  try { return fn(); } finally { R.risk = was; }
}

/** Hands a region to someone else for a test: whoever stood there is taken off the map first. */
function hand(g: Game, r: Region, nat: string): void {
  for (const c of [...r.units]) removeCorps(g, c);
  capture(g, r, nat);
}

/** Puts an Italian army at Egypt's door: next to Mersa Matruh, the British ground nearest Libya. */
function push(g: Game, n = 4): void {
  const at = g.byName['Sidi Barrani'];
  if (at.ctrl !== 'ITA') hand(g, at, 'ITA');
  for (let i = 0; i < n; i++) addCorps(g, 'ITA', `Test Corps ${i + 1}`, 'inf', 3, at.id, {});
}

describe('the floor at home (ruling 229, 1b)', () => {
  it('is the record: a third of the field corps from June 1940 to June 1941, a quarter to the end of 1942, none outside', () => {
    expect(MIDDLE_EAST.floor.UK.map((f) => [f.from, f.to])).toEqual([[10, 22], [23, 40]]);
    const g = grand(3);
    const F = g.corps.filter((c) => field(g, c)).length;
    for (const [turn, want] of [[9, 0], [10, Math.ceil(F / 3)], [22, Math.ceil(F / 3)], [23, Math.ceil(F / 4)], [40, Math.ceil(F / 4)], [41, 0]]) {
      g.turn = turn;
      expect(homeFloor(g, 'UK'), `turn ${turn}`).toBe(want);
    }
  });

  it('counts the field corps at home against it; nothing binds a player, another country or a history campaign', () => {
    const g = grand(3, 12);
    expect(mayLeave(g, 'UK')).toBe(Math.max(0, atHome(g).length - homeFloor(g, 'UK')));
    expect(mayLeave(g, 'GER')).toBe(Infinity);
    const p = startOpen(createGame(3, 'UK'));
    p.turn = 12;
    expect(mayLeave(p, 'UK')).toBe(Infinity);
    const h = createGame(3, null);
    h.turn = 12;
    expect(mayLeave(h, 'UK')).toBe(Infinity);
  });

  it('the five home commands hold in place and are counted on neither side of it', () => {
    const g = grand(3, 12);
    const commands = g.corps.filter((c) => c.nat === 'UK' && c.hold && homeland(g.regions[c.reg], 'UK'));
    expect(commands.map((c) => c.name).sort()).toEqual(['Eastern Command', 'London District', 'Northern Command', 'Scottish Command', 'Southern Command']);
  });
});

describe('the sailing to Egypt (ruling 229, 1a)', () => {
  it('weighs the desert by the enemy at its edge, and home by what can cross to it', () => {
    const g = grand(3, 12);
    const ground = new Set<number>((theatres as any).desert.regions.map((nm: string) => g.byName[nm].id));
    const ports = ['Alexandria', 'Port Said'].map((nm) => g.byName[nm]);
    const before = theatreFront(g, 'UK', ground, ports)!;
    expect(before.at.name).toBe('Alexandria');
    expect(before.on.has(g.byName['Mersa Matruh'].id)).toBe(true);
    push(g, 2);
    const after = theatreFront(g, 'UK', ground, ports)!;
    // (Sidi Barrani is Italian now: the two corps there are at the edge of the British ground, and count.)
    expect(after.on.has(g.byName['Sidi Barrani'].id)).toBe(false);
    expect(after.threat).toBeGreaterThanOrEqual(6);
    expect(after.held).toBeCloseTo(before.held - 0, 6);
    // Home: the five commands and the army at home stand against what Germany's ships could carry from his harbours
    // in reach, two months of them.
    const h = homeThreat(g, 'UK', 2);
    expect(h.held).toBeCloseTo(7 + 12, 6);
    expect(h.threat).toBeGreaterThanOrEqual(0);
  });

  it('at stop 3, with Italy at Egypt\'s door and the sea safe, spare corps sail from home to Egypt, and never below the floor', () => {
    const g = grand(3, 12);
    push(g, 5);
    const home = atHome(g).map((c) => c.id), there = inEgypt(g).length;
    safeSea(1, () => runAI(g, 'UK'));
    const came = inEgypt(g).filter((c) => home.includes(c.id));
    expect(came.length).toBeGreaterThan(0);
    expect(inEgypt(g).length - there).toBe(came.length);
    expect(atHome(g).length).toBeGreaterThanOrEqual(homeFloor(g, 'UK'));
    expect(came.length).toBeLessThanOrEqual(g.nations.UK.sealift);
  });

  it('no corps sails over a crossing too dangerous for the staff: it goes round the Cape instead, two months at sea (b460)', () => {
    const g = grand(3, 12);
    push(g, 5);
    const home = atHome(g).map((c) => c.id), floor = homeFloor(g, 'UK');
    runAI(g, 'UK');
    expect(inEgypt(g).filter((c) => home.includes(c.id))).toEqual([]);
    const sea = voyagesOf(g, 'UK');
    expect(sea.length).toBeGreaterThan(0);
    expect(sea.length).toBeLessThanOrEqual(g.nations.UK.sealift);
    for (const v of sea) {
      expect(home).toContain(v.corps.id);
      expect(v.due).toBe(14);
      expect(g.regions[v.to].name).toBe('Alexandria');
      // Off the map: on no ground, in no count but the field army's.
      expect(g.corps.includes(v.corps)).toBe(false);
      expect(g.regions.some((r) => r.units.includes(v.corps))).toBe(false);
    }
    // The floor binds the sailing, and the corps at sea still belong to the army it is measured against.
    expect(homeFloor(g, 'UK')).toBe(floor);
    expect(atHome(g).length).toBeGreaterThanOrEqual(floor);
    // (The way by the Cape ends in the Gulf of Aden, which has no coast: Suez is no harbour, and no sailing could price it.)
    expect(g.byName['Suez'].port).toBe(false);
  });

  it('a corps round the Cape lands at Alexandria two months later as it was, by the arrival rows\' code, and is kept in a save', () => {
    const g = grand(3, 12);
    push(g, 5);
    runAI(g, 'UK');
    const sea = voyagesOf(g, 'UK').map((v) => ({ id: v.corps.id, str: v.corps.str, divs: v.corps.divs }));
    expect(sea.length).toBeGreaterThan(0);
    // Saved and loaded at sea.
    const h = loadGame(saveGame(g))!;
    expect(voyagesOf(h, 'UK').map((v) => v.corps.id)).toEqual(sea.map((s) => s.id));
    for (const x of [g, h]) {
      x.turn = 13;
      startPhase(x, 'UK');
      expect(voyagesOf(x, 'UK').length, 'not before its month').toBe(sea.length);
      x.turn = 14;
      startPhase(x, 'UK');
      expect(voyagesOf(x, 'UK')).toEqual([]);
      expect(x.atSea).toBeUndefined();
      for (const s of sea) {
        const c = x.corps.find((u) => u.id === s.id)!;
        expect(c, `corps ${s.id} landed`).toBeDefined();
        expect(x.regions[c.reg].units).toContain(c);
        expect(['Alexandria', 'Port Said', 'Mersa Matruh', 'El Alamein', 'Cairo', 'Suez', 'Sinai', 'Faiyum']).toContain(x.regions[c.reg].name);
        expect(c.str).toBe(s.str);
        expect(c.divs).toBe(s.divs);
      }
    }
  });

  it('with Alexandria lost to the enemy, the corps at sea land at the nearest harbour their side holds on that front', () => {
    const g = grand(3, 12);
    push(g, 5);
    runAI(g, 'UK');
    expect(voyagesOf(g, 'UK').length).toBeGreaterThan(0);
    hand(g, g.byName['Alexandria'], 'ITA');
    g.turn = 14;
    startPhase(g, 'UK');
    expect(voyagesOf(g, 'UK')).toEqual([]);
    const landed = g.corps.filter((c) => c.nat === 'UK' && g.regions[c.reg].name === 'Port Said');
    expect(landed.length).toBeGreaterThan(0);
  });

  it('corps at sea stay on the army\'s books: its field army, its pay, its men and its cap on special formations', () => {
    const g = grand(3, 12);
    push(g, 5);
    runAI(g, 'UK');
    const sea = voyagesOf(g, 'UK');
    expect(sea.length).toBeGreaterThan(0);
    const all = [...g.corps.filter((c) => c.nat === 'UK' && !c.dead), ...sea.map((v) => v.corps)];
    expect(fieldArmy(g, 'UK')).toBeCloseTo(all.reduce((s, c) => s + c.divs * c.str, 0), 9);
    const paid = upkeep(g, 'UK').filter((l) => l.key.startsWith('corps:')).reduce((s, l) => s + (l.count ?? 0), 0);
    expect(paid).toBe(all.reduce((s, c) => s + c.divs, 0));
    const v = sea[0].corps, elite = eliteCount(g, 'UK'), spare = menSpare(g, 'UK');
    expect(Number.isFinite(spare)).toBe(true);
    v.elite = 'dominion';
    v.str -= 0.25;
    expect(eliteCount(g, 'UK')).toBe(elite + 1);
    expect(menSpare(g, 'UK')).toBeCloseTo(spare - 0.25 * v.divs, 6);
  });

  it('a Britain that falls loses its corps at sea with the rest of its army, and a freed one lands none of them', () => {
    const g = grand(3, 12);
    push(g, 5);
    runAI(g, 'UK');
    expect(voyagesOf(g, 'UK').length).toBeGreaterThan(0);
    const ids = voyagesOf(g, 'UK').map((v) => v.corps.id);
    capitulate(g, 'UK', 'GER');
    expect(voyagesOf(g, 'UK')).toEqual([]);
    expect(g.atSea).toBeUndefined();
    g.nations.UK.alive = true;
    g.turn = 14;
    startPhase(g, 'UK');
    expect(g.corps.some((c) => ids.includes(c.id))).toBe(false);
  });

  it('a Britain whose own waters are shut sends nobody round the Cape', () => {
    const run = (shut: boolean): number => {
      const g = grand(3, 12);
      push(g, 5);
      if (shut) {
        const seas = new Set(g.regions.filter((r) => homeland(r, 'UK') && r.port).flatMap((r) => r.seas));
        const model = g.fleets.find((f) => f.nat === 'ITA')!;
        for (const z of seas) g.fleets.push({ ...model, id: 9000 + z, ships: { ...model.ships, bb: 60 }, base: -1, home: z, zone: z, stance: 'patrol' as const });
        for (const z of seas) expect(closedTo(g, z, 'UK')).toBe(true);
      }
      runAI(g, 'UK');
      return voyagesOf(g, 'UK').length;
    };
    expect(run(false)).toBeGreaterThan(0);
    expect(run(true)).toBe(0);
  });

  it('nobody goes round the Cape at stops 1 and 2, nor for a player\'s Britain', () => {
    for (const stop of [1, 2]) {
      const g = grand(stop, 12);
      push(g, 5);
      runAI(g, 'UK');
      expect(voyagesOf(g, 'UK'), `stop ${stop}`).toEqual([]);
    }
    const p = grand(3, 12);
    p.human = 'UK';
    push(p, 5);
    runAI(p, 'UK');
    expect(voyagesOf(p, 'UK')).toEqual([]);
  });

  it('nobody sails at stops 1 and 2, nor while the desert is quiet (Italy at peace)', () => {
    for (const stop of [1, 2]) {
      const g = grand(stop, 12);
      push(g, 5);
      const home = atHome(g).map((c) => c.id);
      safeSea(1, () => runAI(g, 'UK'));
      expect(inEgypt(g).filter((c) => home.includes(c.id)), `stop ${stop}`).toEqual([]);
    }
    const g = grand(3, 12, 3, false);
    expect(g.nations.UK.atWar).toContain('GER');
    const home = atHome(g).map((c) => c.id);
    safeSea(1, () => runAI(g, 'UK'));
    expect(inEgypt(g).filter((c) => home.includes(c.id))).toEqual([]);
  });

  it('a floor that leaves nobody spare sends nobody', () => {
    const g = grand(3, 12);
    push(g, 5);
    // Every field corps at home but the floor's share is struck off: none may leave.
    const spare = atHome(g).length - homeFloor(g, 'UK');
    for (const c of atHome(g).slice(0, spare)) removeCorps(g, c);
    while (mayLeave(g, 'UK') > 0) removeCorps(g, atHome(g)[0]);
    const home = atHome(g).map((c) => c.id);
    expect(home.length).toBeGreaterThan(0);
    safeSea(1, () => runAI(g, 'UK'));
    expect(inEgypt(g).filter((c) => home.includes(c.id))).toEqual([]);
  });
});

describe('the desert rows of the grand campaign (ruling 229, 1c)', () => {
  const rows = (setup as any).expeditions as any[];
  const row = (name: string): any[] => rows.find((r) => r[2] === name)!;

  it('are the historian\'s: the South Africans, the New Zealanders unless W Force sailed, the Indians of 1942', () => {
    expect(row('1st South African Division').slice(0, 6)).toEqual([21, 'UK', '1st South African Division', 'inf', 1, 'Alexandria']);
    expect(row('2nd South African Division').slice(0, 6)).toEqual([22, 'UK', '2nd South African Division', 'inf', 1, 'Alexandria']);
    expect(row('2nd New Zealand Division')[6].unless).toBe('W Force (New Zealand Division and 1st Armoured Brigade)');
    expect(row('5th Indian Division')[0]).toBe(32);
    expect(row('10th Indian Division').slice(4, 6)).toEqual([1, 'Mersa Matruh']);
    for (const nm of ['1st South African Division', '2nd South African Division', '2nd New Zealand Division', '5th Indian Division', '10th Indian Division']) expect(row(nm)[6]).toMatchObject({ open: 1, war: ['UK', 'ITA'] });
    // The 9th Australian comes once: by the Tobruk row, which falls back to a harbour of Egypt when Tobruk is lost.
    expect(rows.filter((r) => r[1] === 'UK' && /9th Australian/.test(r[2]) && r[0] < 30)).toHaveLength(1);
    // XIII and XXX Corps bring 1 division each in the grand campaign, 2 in history's.
    expect(row('XIII Corps, Eighth Army')[6].openDivs).toBe(1);
    expect(row('XXX Corps, Eighth Army')[6].openDivs).toBe(1);
  });

  it('the New Zealanders stood in for leave Egypt when W Force lands, and a player is told where his division has gone', () => {
    for (const human of [null, 'UK']) {
      const g = startOpen(createGame(3, human));
      setStop(g, 3, true);
      g.turn = 19;
      declareWar(g, 'ITA', 'UK');
      startPhase(g, 'UK');
      const nz = g.corps.find((c) => c.nat === 'UK' && c.name === '2nd New Zealand Division' && !c.dead);
      expect(nz, 'the New Zealanders land in Egypt first').toBeDefined();
      const from = g.regions[nz!.reg].name;
      g.turn = 20;
      declareWar(g, 'ITA', 'GRE');
      const seen = g.headlines.length;
      startPhase(g, 'UK');
      expect(g.corps.some((c) => c.name.startsWith('W Force') && !c.dead), 'W Force lands').toBe(true);
      expect(g.corps.some((c) => c.id === nz!.id)).toBe(false);
      const told = g.headlines.slice(seen).filter((h) => /New Zealand Division sails/.test(h.text));
      expect(told.map((h) => h.text)).toEqual(human ? [`The 2nd New Zealand Division sails from ${from} for Greece. It goes as part of W Force, which has landed there with the 1st Armoured Brigade.`] : []);
    }
  });

  it('the Tobruk garrison lands at the nearest harbour its side holds when Tobruk is lost, and holds nothing in place there', () => {
    const g = grand(1, 18);
    expect(g.byName['Tobruk'].ctrl).toBe('ITA');
    startPhase(g, 'UK');
    const c = g.corps.find((x) => x.name === 'Tobruk garrison (9th Australian Division)' && !x.dead);
    expect(c).toBeDefined();
    expect(g.regions[c!.reg].port).toBe(true);
    expect(g.regions[c!.reg].ctrl).toBe('UK');
    expect(c!.hold).toBe(false);
  });
});
