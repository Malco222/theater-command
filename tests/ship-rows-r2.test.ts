// Ships by class and navy, the card side (release 2; Brian's ask of 22 November 2026, 04:03Z; the design director's
// 08:49Z and 11:09Z on the naval thread's plan, naval-battles/ships-by-class-plan.md, its tests 6.2): each hull bears
// her class's row of the record for life, a fleet weighs the sum of its hulls' worths, the start fleets keep their
// total weight in each kind, and new battleships are built by route 2 at the step held when they are ordered.
// Switched by stage.json 'shipRows' (TC_SHIPROWS): off, the game is release 1's.
import { describe, expect, it } from 'vitest';
import hullData from '../data/campaign/hulls.json';
import navyData from '../data/campaign/navy.json';
import rowsData from '../data/campaign/shiprows.json';
import stage from '../data/campaign/stage.json';
import { Rng } from '../src/core/rng';
import { orderOffer } from '../src/campaign/arsenal';
import { deliver } from '../src/campaign/economy';
import { detach, merge } from '../src/campaign/fleets';
import { createGame } from '../src/campaign/game';
import { CLASSES, NAVY, fleetPower, navalUpkeep, sail, seaBattles, seaField, sinkShips, zoneOf } from '../src/campaign/navy';
import { setStop } from '../src/campaign/history';
import { grantPlans } from '../src/campaign/plans';
import { loadGame, saveGame } from '../src/campaign/save';
import * as R from '../src/campaign/shiprows';
import { startOpen } from '../src/campaign/staff';
import { SHIPROWS_STAGE } from '../src/campaign/stage';
import { techOf } from '../src/campaign/tech';
import { runTurnAI } from '../src/campaign/turn';
import type { Fleet, Game, ShipClass } from '../src/campaign/types';

/** A history campaign with ships by class on (or off): the flag set and the lists laid, as a campaign opened with the switch on has them. */
function history(on = true, seed = 1, human: string | null = null): Game {
  const g = createGame(seed, human);
  if (on) { g.flags.shipRows = 1; R.layAllRows(g); }
  return g;
}
/** A grand campaign on the wider world, with ships by class on (or off). */
const grand = (on = true, seed = 3, human: string | null = null): Game => startOpen(history(on, seed, human));
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const give = (g: Game, nat: string, ...ids: string[]): void => { const t = techOf(g, nat)!; for (const id of ids) if (!t.done.includes(id)) t.done.push(id); };
const D = rowsData as unknown as { rows: { bb: R.ShipRow[] }; unsourced: { bb: string[] } };
const rowById = (id: string): R.ShipRow => D.rows.bb.find((r) => r.id === id)!;
/** Every list in step with its count, in the form it is kept: no longer than the count, no row the record does not hold. */
function inStep(g: Game): void {
  for (const f of g.fleets) for (const c of CLASSES) {
    const list = f.rows?.[c];
    if (!list) continue;
    expect(list.length, `${f.name} ${c}`).toBeLessThanOrEqual(Math.max(0, f.ships[c]));
    expect(list[list.length - 1], `${f.name} ${c} kept with no trailing empty`).not.toBeNull();
    for (const id of list) if (id !== null) expect(R.labelOf(id), id).not.toBe('');
  }
}

describe('the record table', () => {
  it("each row's worth is the square root of its gunfire times its armour on the record's cells, as the plan reads them", () => {
    const want: Record<string, number> = {
      'usa-north-carolina': 1.342, 'usa-south-dakota': 1.414, 'usa-iowa': 1.406, 'uk-kgv': 1.188, 'jap-nagato': 1.046, 'jap-yamato': 1.585,
      'ger-bismarck': 1.094, 'ita-littorio': 0.938, 'fra-dunkerque': 0.684, 'fra-strasbourg': 0.724, 'fra-richelieu': 0.949, 'sov-gangut': 0.625,
      'sov-parizhskaya': 0.819, 'uk-nelson': 1.11, 'uk-qe': 1.09, 'uk-revenge': 0.96, 'ger-scharnhorst': 0.89,
    };
    expect(D.rows.bb.map((r) => r.id).sort()).toEqual(Object.keys(want).sort());
    for (const r of D.rows.bb) expect(R.rawWorth(r), r.id).toBeCloseTo(want[r.id], 3);
    // Iowa a hair below South Dakota (same shell, belt and deck 3 and 2 mm thinner), Yamato clear at about 1.6.
    expect(R.rawWorth(rowById("usa-iowa"))!).toBeLessThan(R.rawWorth(rowById("usa-south-dakota"))!);
  });

  it("the step medians are the record's periods': 1.0459, 1.0943 and 1.1412 (Nagato, Bismarck, and between Bismarck and King George V)", () => {
    expect(R.stepMedian(0)).toBeCloseTo(1.0459, 4);
    expect(R.stepMedian(1)).toBeCloseTo(1.0943, 4);
    expect(R.stepMedian(2)).toBeCloseTo(1.1412, 4);
  });

  it('every battleship the record of names holds is in one row of her own navy or listed for the historian, never both', () => {
    const navies = (hullData as unknown as { navies: Record<string, { bb?: { name: string; later?: { name?: string }[] }[] }> }).navies;
    for (const [nat, v] of Object.entries(navies)) {
      for (const h of v.bb ?? []) for (const name of [h.name, ...(h.later ?? []).map((x) => x.name).filter((x): x is string => !!x)]) {
        const rows = D.rows.bb.filter((r) => r.hulls.includes(name));
        const listed = D.unsourced.bb.includes(name);
        expect(rows.length + (listed ? 1 : 0), `${nat} ${name}`).toBe(1);
        if (rows.length) expect(rows[0].nat, name).toBe(nat);
      }
    }
  });
});

describe('the switch off', () => {
  it('ships off: a campaign carries no flag, no list and no step on an order, and its fleets weigh their counts', () => {
    expect(stage.shipRows).toBe(false);
    expect(SHIPROWS_STAGE).toBe(false);
    const g = grand(false, 3, 'GER');
    expect(g.flags.shipRows).toBeUndefined();
    expect(g.fleets.some((f) => f.rows)).toBe(false);
    g.nations.GER.pp = 5000;
    const b = orderOffer(g, 'GER', 'ship:bb');
    expect(b).not.toBeNull();
    expect(b!.step).toBeUndefined();
    b!.eta = g.turn;
    deliver(g, 'GER');
    expect(g.fleets.some((f) => f.rows)).toBe(false);
    for (const f of g.fleets) for (const c of CLASSES) expect(R.hullSum(g, f, c)).toBe(f.ships[c]);
    // A list a fleet carries (a save made with the switch on, opened with it off) is not read either.
    const battle = fleet(g, 'GER', 'Battle Fleet');
    battle.rows = { bb: ['jap-yamato', 'jap-yamato'] };
    expect(R.hullSum(g, battle, 'bb')).toBe(battle.ships.bb);
    expect(R.fleetRows(g, battle)).toBeUndefined();
  });

  it('off, a list a fleet happens to carry is never read: fleetPower is the same to the bit', () => {
    const g = history(false);
    const home = fleet(g, 'UK', 'Home Fleet');
    const before = fleetPower(g, home);
    home.rows = { bb: ['jap-yamato', 'jap-yamato'] };
    expect(fleetPower(g, home)).toBe(before);
  });
});

describe('worth 1', () => {
  it('with every hull at no row, fleetPower is release 1 in every fleet, to the bit', () => {
    for (const g of [history(true), grand(true)]) {
      const on = g.fleets.map((f) => fleetPower(g, f));
      for (const f of g.fleets) delete f.rows;
      const plain = g.fleets.map((f) => fleetPower(g, f));
      g.flags.shipRows = 0;
      expect(g.fleets.map((f) => fleetPower(g, f))).toEqual(plain);
      // (And the rows did move it: the record's hulls are not all of today's weight.)
      expect(on).not.toEqual(plain);
    }
  });
});

describe('the start', () => {
  it("each fleet opens with its named hulls' rows, the rest with none", () => {
    const g = history(true);
    expect(R.listOf(fleet(g, 'UK', 'Home Fleet'), 'bb')).toEqual(['uk-nelson', 'uk-nelson', null, null, 'uk-revenge', 'uk-revenge', 'uk-revenge']);
    expect(R.listOf(fleet(g, 'UK', 'Mediterranean Fleet'), 'bb')).toEqual(['uk-qe', null, null]);
    expect(R.listOf(fleet(g, 'GER', 'Battle Fleet'), 'bb')).toEqual(['ger-scharnhorst', 'ger-scharnhorst']);
    expect(R.listOf(fleet(g, 'FRA', 'Atlantic Fleet'), 'bb')).toEqual(['fra-dunkerque', 'fra-strasbourg']);
    expect(R.listOf(fleet(g, 'SOV', 'Baltic Fleet'), 'bb')).toEqual(['sov-gangut', 'sov-gangut']);
    expect(fleet(g, 'ITA', '1st Fleet').rows).toBeUndefined();
    inStep(g);
  });

  it('the start fleets keep their total weight in each kind (the rebase), in a history campaign and on the wider world', () => {
    for (const g of [history(true), grand(true)]) {
      const start = g.fleets.filter((f) => f.ships.bb > 0);
      const count = start.reduce((s, f) => s + f.ships.bb, 0), weight = start.reduce((s, f) => s + R.hullSum(g, f, 'bb'), 0);
      expect(weight).toBeCloseTo(count, 9);
    }
    // The Soviet Baltic line reads about 0.7 of today's weight (plan 7.4: 0.705-0.715 on the rough check).
    const g = history(true);
    const gangut = R.hullWorth(g, 'bb', 'sov-gangut');
    expect(gangut).toBeGreaterThan(0.68);
    expect(gangut).toBeLessThan(0.74);
    // A hull with no row reads today's weight (Q5), never the yardstick.
    expect(R.hullWorth(g, 'bb', null)).toBe(1);
  });
});

describe('the lists keep in step with the ships', () => {
  it('hulls lost go by the stars\' fixed rotation (b487, Q10); sinkShips draws nothing more for it', () => {
    const g = history(true);
    const home = fleet(g, 'UK', 'Home Fleet');
    home.lost.bb = 0;
    home.ships.bb -= 2; home.lost.bb += 2;
    R.loseRows(home, 'bb', 2);
    // [N, N, -, -, R, R, R]: place 0 goes, then place 1 of the six left.
    expect(R.listOf(home, 'bb')).toEqual(['uk-nelson', null, 'uk-revenge', 'uk-revenge', 'uk-revenge']);
    // Through sinkShips the same rotation, from the fleet's earlier losses (three before: the first to go is place 3).
    const h = history(true);
    const hf = fleet(h, 'UK', 'Home Fleet');
    hf.lost.bb = 3;
    const start = R.listOf(hf, 'bb');
    let checked = 0;
    for (let seed = 1; hf.ships.bb === 7 && seed < 80; seed++) {
      const k = sinkShips(hf, 0.4, new Rng(seed), ['bb']).bb;
      if (k > 0) {
        const want = [...start];
        for (let j = 0; j < k; j++) want.splice((3 + j) % want.length, 1);
        expect(R.listOf(hf, 'bb')).toEqual(want);
        checked++;
      }
    }
    expect(checked).toBe(1);
    // The same draws, call for call, with the rows or without them.
    const a = history(true, 4), b = history(false, 4);
    for (let seed = 1; seed < 30; seed++) {
      const fa = fleet(a, 'UK', 'Home Fleet'), fb = fleet(b, 'UK', 'Home Fleet');
      const ra = new Rng(seed), rb = new Rng(seed);
      expect(sinkShips(fa, 0.3, ra)).toEqual(sinkShips(fb, 0.3, rb));
      expect(ra.state).toBe(rb.state);
      inStep(a);
    }
  });

  it('the field says which rows went down where it reports them (plan 5.1 c3); the rest go by the rotation', () => {
    const g = history(true);
    const home = fleet(g, 'UK', 'Home Fleet');
    home.ships.bb -= 1; home.lost.bb += 1;
    R.loseRows(home, 'bb', 1, ['uk-revenge']);
    expect(R.listOf(home, 'bb')).toEqual(['uk-nelson', 'uk-nelson', null, null, 'uk-revenge', 'uk-revenge']);
  });

  it('a split takes the last hulls and their rows; a join brings them in at the back', () => {
    const g = grand(true, 3, 'UK');
    const home = fleet(g, 'UK', 'Home Fleet');
    const a = detach(g, home, { bb: 2 })!;
    expect(R.listOf(a, 'bb')).toEqual(['uk-revenge', 'uk-revenge']);
    expect(R.listOf(home, 'bb')).toEqual(['uk-nelson', 'uk-nelson', null, null, 'uk-revenge']);
    inStep(g);
    expect(merge(g, a, home)).toBe(true);
    expect(R.listOf(home, 'bb')).toEqual(['uk-nelson', 'uk-nelson', null, null, 'uk-revenge', 'uk-revenge', 'uk-revenge']);
    expect(a.rows).toBeUndefined();
    inStep(g);
  });

  it("an arrival brings the hulls its words name (King George V in December 1940), a move its hulls' own rows", () => {
    const g = history(true);
    const home = fleet(g, 'UK', 'Home Fleet');
    g.turn = 16;
    const had = home.ships.bb;
    navalUpkeep(g, 'UK');
    expect(home.ships.bb).toBe(had + 1);
    expect(R.listOf(home, 'bb').at(-1)).toBe('uk-kgv');
    // A move: the hulls named, by their rows, from the fleet they sailed with; the rest from its back.
    expect(R.rowsNamed('UK', 'bb', 'the battlecruiser Hood and the battleship Valiant', 2)).toEqual([null, 'uk-qe']);
    expect(R.namedIn('USA', 'bb', 'the battleships Idaho, Mississippi and New Mexico and the carrier Yorktown leave for the Atlantic')).toEqual(['Idaho', 'Mississippi', 'New Mexico']);
    const med = fleet(g, 'UK', 'Mediterranean Fleet');
    const went = R.takeRows(med, 'bb', 1, ['uk-qe']);
    expect(went).toEqual(['uk-qe']);
    med.ships.bb -= 1;
    expect(R.listOf(med, 'bb')).toEqual([null, null]);
    inStep(g);
  });

  it('through two years of a grand campaign every list stays in step, and a save keeps them', () => {
    const g = grand(true, 5);
    for (let m = 0; m < 24; m++) { runTurnAI(g); inStep(g); }
    const h = loadGame(saveGame(g))!;
    expect(h.flags.shipRows).toBe(1);
    expect(h.fleets.map((f) => f.rows ?? null)).toEqual(g.fleets.map((f) => f.rows ?? null));
    expect(h.fleets.map((f) => fleetPower(h, f))).toEqual(g.fleets.map((f) => fleetPower(g, f)));
  }, 120_000);
});

describe('new battleships (the grand campaign: route 2, Q4)', () => {
  it('each navy builds its own best row until it holds a step; a step lifts a navy to its median, and never lowers a row', () => {
    const g = grand(true);
    const at = (nat: string): R.RowId[] => [0, 1, 2].map((k) => R.buildRow(g, nat, 'bb', k));
    expect(at('USA')).toEqual(['usa-north-carolina', 'usa-south-dakota', 'usa-south-dakota']);
    expect(at('UK')).toEqual(['uk-kgv', 'uk-kgv', 'uk-kgv']);
    expect(at('JAP')).toEqual(['jap-nagato', 'jap-yamato', 'jap-yamato']);
    expect(at('GER')).toEqual(['ger-bismarck', 'ger-bismarck', 'step:2']);
    for (const nat of ['ITA', 'FRA', 'SOV']) expect(at(nat).slice(1), nat).toEqual(['step:1', 'step:2']);
    expect(at('ITA')[0]).toBe('ita-littorio');
    expect(at('FRA')[0]).toBe('fra-strasbourg');
    expect(at('SOV')[0]).toBe('sov-parizhskaya');
  });

  it("kin take the senior partner's path and the better step; a smaller navy builds to the plans it holds, or with no row (ruling 235)", () => {
    const g = grand(true);
    expect(R.buildRow(g, 'CAN', 'bb', 0)).toBe('uk-kgv');
    give(g, 'UK', 'earlywarning', 'firecontrol');
    expect(R.stepOf(g, 'UK')).toBe(1);
    expect(R.stepOf(g, 'CAN')).toBe(1);
    expect(R.buildRow(g, 'BRA', 'bb', 0)).toBeNull();
    expect(grantPlans(g, 'BRA', 'bb', 'USA', 'taken')).toBe(true);
    expect(R.buildRow(g, 'BRA', 'bb', 0)).toBe('usa-north-carolina');
    // Never a date: a history campaign keeps no research tree, so its navies hold step 0 in every month.
    const h = history(true);
    expect(techOf(h, 'GER')).toBeNull();
    for (const turn of [1, 40, 70]) { h.turn = turn; expect(R.stepOf(h, 'GER')).toBe(0); }
  });

  it('her row is the step held when she was ordered (plan 4.3); the same research on two turns gives the same row', () => {
    const g = grand(true, 3, 'GER');
    g.nations.GER.pp = 5000;
    const battle = fleet(g, 'GER', 'Battle Fleet');
    const b = orderOffer(g, 'GER', 'ship:bb')!;
    expect(b.step).toBe(0);
    give(g, 'GER', 'earlywarning', 'firecontrol', 'carriers', 'taskforces');
    expect(R.stepOf(g, 'GER')).toBe(2);
    const was = battle.ships.bb;
    b.eta = g.turn;
    deliver(g, 'GER');
    const joined = g.fleets.filter((f) => f.nat === 'GER' && f.ships.bb > 0).flatMap((f) => R.listOf(f, 'bb'));
    expect(joined.filter((x) => x === 'ger-bismarck')).toHaveLength(1);
    expect(battle.ships.bb + g.fleets.filter((f) => f.nat === 'GER' && f !== battle).reduce((s, f) => s + f.ships.bb, 0)).toBe(was + 1);
    // Ordered now, at step 2: Germany's own best (Bismarck, 1.09) is below the step's median, so she is a new design.
    const c = orderOffer(g, 'GER', 'ship:bb')!;
    expect(c.step).toBe(2);
    c.eta = g.turn;
    deliver(g, 'GER');
    expect(g.fleets.filter((f) => f.nat === 'GER').flatMap((f) => R.listOf(f, 'bb'))).toContain('step:2');
    inStep(g);
  });
});

describe('what a fleet weighs', () => {
  it("a kind weighs the sum of its hulls' worths, and the field's ticket rows carry the same worth (plan 5.1 c1, test 7)", () => {
    const on = history(true), off = history(false);
    const baltic = fleet(on, 'SOV', 'Baltic Fleet'), plain = fleet(off, 'SOV', 'Baltic Fleet');
    const w = R.hullWorth(on, 'bb', 'sov-gangut');
    const mul = fleetPower(off, plain) / CLASSES.reduce((s, c) => s + plain.ships[c] * NAVY.classes[c].power, 0);
    expect(fleetPower(on, baltic)).toBeCloseTo(fleetPower(off, plain) + (2 * w - 2) * NAVY.classes.bb.power * mul, 9);
    expect(fleetPower(on, baltic)).toBeLessThan(fleetPower(off, plain));
    const rows = R.fleetRows(on, baltic)!;
    expect(rows.bb).toEqual([{ id: 'sov-gangut', label: 'Gangut class', worth: w, n: 2 }]);
    for (const f of on.fleets) for (const c of R.ROW_KINDS) {
      const sum = (R.fleetRows(on, f)?.[c] ?? []).reduce((s, r) => s + r.worth * r.n, 0);
      expect(sum, `${f.name} ${c}`).toBeCloseTo(f.ships[c] > 0 ? R.hullSum(on, f, c) : 0, 9);
    }
    expect(R.fleetRows(off, plain)).toBeUndefined();
  });

  it('only the kinds with rows move: carriers keep kind weight (Q7), cruisers and destroyers wait for the historian', () => {
    expect(R.ROW_KINDS).toEqual(['bb'] as ShipClass[]);
    const g = history(true);
    for (const f of g.fleets) for (const c of ['cv', 'ca', 'dd', 'ss'] as ShipClass[]) expect(R.hullSum(g, f, c)).toBe(f.ships[c]);
    // (navy.json's first fleets are the history campaigns' start: Force H, from June 1940, is not among them.)
    expect((navyData as unknown as { fleets: { name: string; from?: number }[] }).fleets.find((f) => f.name === 'Force H')!.from).toBeGreaterThan(1);
  });
});

describe("the review's mends (22 November, 18:40Z)", () => {
  it('Force H at stops 1 and 2: the book lays the Resolution, the Hood and the Valiant bring their own rows, and the Valiant takes hers east', () => {
    const g = grand(true, 5);
    setStop(g, 1, true);
    const weight = (): number => g.fleets.filter((f) => f.nat === 'UK').reduce((s, f) => s + R.hullSum(g, f, 'bb'), 0);
    for (const t of [10, 11, 12, 13]) {
      g.turn = t;
      navalUpkeep(g, 'UK');
      inStep(g);
      if (t === 10) {
        // The book's Resolution (Revenge class), then the move's Hood (no row) and Valiant (Queen Elizabeth, rebuilt).
        expect(R.listOf(fleet(g, 'UK', 'Force H'), 'bb')).toEqual(['uk-revenge', null, 'uk-qe']);
        // The Home Fleet keeps its Nelsons and its three Revenges: the Valiant went as a hull with no row (plan 3.2).
        expect(R.listOf(fleet(g, 'UK', 'Home Fleet'), 'bb')).toEqual(['uk-nelson', 'uk-nelson', 'uk-revenge', 'uk-revenge', 'uk-revenge']);
      }
    }
    expect(R.listOf(fleet(g, 'UK', 'Mediterranean Fleet'), 'bb').at(-1)).toBe('uk-qe');
    // The Resolution, and the Renown in from off the chart with no row.
    expect(R.listOf(fleet(g, 'UK', 'Force H'), 'bb')).toEqual(['uk-revenge', null]);
    // Nothing sank, and no hull lost or changed her row on the way: the Valiant is a Queen Elizabeth in the East beside the Warspite.
    const all = g.fleets.filter((f) => f.nat === 'UK').flatMap((f) => R.listOf(f, 'bb'));
    const n = (id: string): number => all.filter((x) => x === id).length;
    expect([n('uk-nelson'), n('uk-revenge'), n('uk-qe')]).toEqual([2, 4, 2]);
    expect(weight()).toBeCloseTo(all.reduce((s, id) => s + R.hullWorth(g, 'bb', id), 0), 12);
  });

  it('a move whose named row the fleet does not show takes a hull with no row as her; with none, the last goes as she stands', () => {
    const g = history(true);
    const home = fleet(g, 'UK', 'Home Fleet');
    expect(R.takeRows(home, 'bb', 1, ['uk-qe'])).toEqual(['uk-qe']);
    home.ships.bb -= 1;
    expect(R.listOf(home, 'bb')).toEqual(['uk-nelson', 'uk-nelson', null, 'uk-revenge', 'uk-revenge', 'uk-revenge']);
    const f = { ...home, ships: { ...home.ships, bb: 2 }, rows: { bb: ['uk-nelson', 'uk-revenge'] } } as Fleet;
    expect(R.takeRows(f, 'bb', 1, ['uk-qe'])).toEqual(['uk-revenge']);
  });

  it('the rebase is fixed in the campaign as it opens, kept by a save, and read from there', () => {
    const g = grand(true);
    const m = g.flags['rebase:bb'];
    expect(m).toBeGreaterThan(1);
    expect(R.rebase(g, 'bb')).toBe(m);
    const h = loadGame(saveGame(g))!;
    expect(h.flags['rebase:bb']).toBe(m);
    // A build holding other data reads the campaign's own factor, not its own.
    h.flags['rebase:bb'] = 1.25;
    expect(R.hullWorth(h, 'bb', 'uk-kgv')).toBeCloseTo(R.rawWorth(rowById('uk-kgv'))! * 1.25, 12);
    // A history campaign is fixed as its fleets form; off, nothing is written.
    const k = history(true);
    R.pinRebase(k);
    expect(k.flags['rebase:bb']).toBe(R.rebase(k, 'bb'));
    for (const off of [history(false), grand(false)]) {
      R.pinRebase(off);
      expect(Object.keys(off.flags).filter((x) => x.startsWith('rebase:'))).toEqual([]);
    }
  });

  it('an action handed back to the dice sinks the hulls the field named first (plan 5.2 item 5)', () => {
    let checked = 0;
    for (let seed = 1; seed <= 12; seed++) {
      const g = history(true, seed);
      const home = fleet(g, 'UK', 'Home Fleet'), battle = fleet(g, 'GER', 'Battle Fleet');
      battle.rows = { bb: ['ger-scharnhorst', 'ger-bismarck'] };
      const norw = zoneOf(g, 'norw');
      sail(g, home, norw, 'patrol'); sail(g, battle, norw, 'patrol');
      seaField.fought = () => ({ R: 3, sunk: { [battle.id]: { bb: 1 }, [home.id]: {} }, toDice: true, rows: { [battle.id]: { bb: ['ger-bismarck'] } } });
      try { seaBattles(g, 'UK'); } finally { seaField.fought = undefined; }
      if (battle.ships.bb !== 1) continue;
      expect(R.listOf(battle, 'bb'), `seed ${seed}`).toEqual(['ger-scharnhorst']);
      checked++;
    }
    expect(checked).toBeGreaterThan(3);
  });

  it("her row is fixed as she is laid down: plans gained before she is delivered do not change her design (plan 4.3)", () => {
    const g = grand(true, 3, 'BRA');
    expect(grantPlans(g, 'BRA', 'bb', 'USA', 'taken')).toBe(true);
    g.nations.BRA.pp = 50000;
    const yard = g.regions.find((r) => r.owner === 'BRA' && r.port && r.ctrl === 'BRA')!;
    g.flags[`wk:yard:${yard.id}`] = 3;
    const b = orderOffer(g, 'BRA', 'ship:bb')!;
    expect(b.row).toBe('usa-north-carolina');
    expect(grantPlans(g, 'BRA', 'bb', 'ITA', 'taken')).toBe(true);
    expect(R.buildRow(g, 'BRA', 'bb', 0)).toBe('ita-littorio');
    b.eta = g.turn;
    deliver(g, 'BRA');
    expect(g.fleets.filter((f) => f.nat === 'BRA').flatMap((f) => R.listOf(f, 'bb'))).toContain('usa-north-carolina');
    expect(g.fleets.filter((f) => f.nat === 'BRA').flatMap((f) => R.listOf(f, 'bb'))).not.toContain('ita-littorio');
  });

  it("a row whose cells are not all in and that states no worth is no row: its hulls read today's weight (Q5)", () => {
    const part = { ...rowById('usa-north-carolina'), id: 'x', deck: undefined } as unknown as R.ShipRow;
    expect(R.rawWorth(part)).toBeNull();
    expect(R.rawWorth({ ...part, worth: 0.9 } as R.ShipRow)).toBe(0.9);
  });
});
