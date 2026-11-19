// Strikes on ships fought out (ruling 238, the design director, 19 November 2026): a raid on a harbour and a strike on
// a fleet at sea, with the ships under attack on the field. Held to ruling 217's tests like every air battle: what
// each side loses averages the raid card within a tenth, the plain order wins as often as the card says, and the field
// decides what the campaign books (b397's law, here for ships: the sinkings drawn as each box drops). The carrier
// groups fly by kind (238(2)) and the set pieces are chosen by place (238, Pearl Harbor and Midway; 238(4) the names).
// The British Home Fleet's carriers against the German Battle Fleet stand in for any carrier strike: the Pacific map
// lies on another branch, and the law is the same.
import { describe, expect, it } from 'vitest';
import { CARRIERS, deckBred, shipHitsBy } from '../src/campaign/air';
import { launchRaid, type RaidKind } from '../src/campaign/airwar';
import { syncWings } from '../src/campaign/carriers';
import { createGame } from '../src/campaign/game';
import { atSea, sail, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game, ShipClass } from '../src/campaign/types';
import { SKY } from '../src/battle/air/book';
import { airBattle, type AirSim } from '../src/battle/air/sim';
import { deckMix, layShips } from '../src/battle/air/ships';
import { faceOnBoard, onBoard, recordMonth, stageOf, STAGES } from '../src/battle/air/stages';
import { airTicket, foughtAir, raidGroups } from '../src/battle/air/ticket';

const CLASSES: ShipClass[] = ['bb', 'cv', 'ca', 'dd', 'ss'];

/** A grand campaign with Britain at the table, played until Britain is at war with Germany (tests/carriers.test.ts). */
function britain(seed = 3): Game {
  const g = startOpen(createGame(seed, 'UK'));
  for (let m = 0; m < 12; m++) {
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    beginHuman(g);
    if (g.nations.UK.atWar.length && g.nations.GER.atWar.includes('UK')) return g;
    endHuman(g);
  }
  throw new Error('Britain never went to war');
}
const named = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;

/**
 * The two strikes: the Home Fleet with six carriers off the German Battle Fleet's harbour, a Taranto in the North Sea;
 * and the two fleets together in the open Atlantic, away from every coast's fighters.
 */
const BASE: Record<'harbour' | 'fleet', string> = (() => {
  const harbour = britain(), sea = britain();
  {
    const g = harbour, home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
    home.ships.cv = 6;
    sail(g, foe, foe.home, 'port');
    const z = g.seas.find((s) => s.coast.includes(foe.base))!;
    home.zone = z.id; home.stance = 'patrol'; syncWings(g);
  }
  {
    const g = sea, home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
    home.ships.cv = 6;
    for (const f of [home, foe]) { f.zone = zoneOf(g, 'natl'); f.stance = 'patrol'; f.lane = undefined; }
    syncWings(g);
  }
  return { harbour: saveGame(harbour), fleet: saveGame(sea) };
})();

/** The k-th battle of a strike: the game as set, with that battle's own luck. */
function strike(kind: 'harbour' | 'fleet', k: number) {
  const g = loadGame(BASE[kind])!;
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  const foe = named(g, 'Battle Fleet');
  const target = kind === 'harbour' ? foe.base : foe.id;
  const ids = raidGroups(g, kind, target, 'UK');
  const { ticket, why } = airTicket(g, kind, target, ids, 'UK', { ships: true });
  if (!ticket) throw new Error(why ?? 'no ticket');
  return { g, target, ids, ticket, foe };
}

function fight(kind: 'harbour' | 'fleet', k: number, show = true, each?: (s: AirSim) => void): { s: AirSim } & ReturnType<typeof strike> {
  const r = strike(kind, k);
  const s = airBattle(r.ticket, { show });
  s.ai.att = true; s.ai.def = true;
  while (!s.over) { s.step(); each?.(s); }
  return { s, ...r };
}

const total = (x: Partial<Record<ShipClass, number>> | undefined): number => CLASSES.reduce((t, c) => t + (x?.[c] ?? 0), 0);

describe('strikes on ships are ticketed when asked for (ruling 238)', () => {
  it('a harbour raid and a fleet strike: only on request until the playtester passes them; the ships attacked as pieces', () => {
    for (const kind of ['harbour', 'fleet'] as const) {
      const { g, target, ids, ticket, foe } = strike(kind, 0);
      expect(airTicket(g, kind, target, ids, 'UK').ticket, kind).toBeNull();
      expect(atSea(foe)).toBe(kind === 'fleet');
      const P = ticket.place.ships!;
      expect(P.law.harbour).toBe(kind === 'harbour');
      expect(P.stage).toBeNull();
      // Every ship of the fleets attacked is a piece, of its own class and fleet.
      for (const f of P.law.fleets) for (const c of CLASSES) expect(P.pieces.filter((p) => p.fleet === f.id && p.cls === c).length).toBe(f.ships[c]);
      expect(new Set(P.pieces.map((p) => p.id)).size).toBe(P.pieces.length);
      expect(P.pieces.every((p) => p.name === null)).toBe(true);
      expect(ticket.title).toBe(kind === 'harbour' ? `The raid on the harbour of ${g.regions[target].name}` : 'The strike on the Battle Fleet');
      // The carriers' own groups fly: they are what carriers are for.
      expect(ticket.att.forces.some((f) => deckBred(g.airFleets.find((x) => x.id === f.corps)!))).toBe(true);
      if (kind === 'fleet') expect(ticket.place.ground!.terrain).toBe('sea');
    }
  });
});

describe("carrier groups fly by kind (ruling 238(2))", () => {
  it('splits a carrier group\'s bombers by the record\'s mix, each kind at its own height, and the parts add up to the group', () => {
    for (const kind of ['harbour', 'fleet'] as const) {
      const { g, ticket } = strike(kind, 1);
      const s = airBattle(ticket);
      const mix = deckMix('UK', ticket.place.when!, kind);
      for (const f of ticket.att.forces.filter((x) => x.kind === 'bmr')) {
        const parts = s.units.filter((u) => u.side === 'att' && u.air === 'bmr' && u.corps === f.corps);
        expect(parts.reduce((t, u) => t + u.n, 0)).toBe(f.men);
        if (!deckBred(g.airFleets.find((x) => x.id === f.corps)!)) { expect(parts.every((u) => !u.strike)).toBe(true); continue; }
        expect(parts.map((u) => u.strike).sort()).toEqual(mix.map(([k]) => k).sort());
        for (const u of parts) {
          expect(u.band).toBe(u.strike === 'dive' ? 2 : 1);
          expect(Math.abs(u.n - f.men * mix.find(([k]) => k === u.strike)![1])).toBeLessThan(1);
          expect(u.model).toBe(u.strike === 'torpedo' ? 'Swordfish' : 'Skua');
        }
      }
      // Its fighters are drawn as the carrier's fighter, and every unit has an id of its own.
      expect(s.units.filter((u) => u.side === 'att' && u.air === 'ftr').every((u) => u.model === 'Fulmar')).toBe(true);
      expect(new Set(s.units.map((u) => u.id)).size).toBe(s.units.length);
      // The 3D game reads each box's kind of attack.
      while (!s.over && !s.units.some((u) => u.extra && u.air === 'bmr')) s.step();
      expect(s.units.filter((u) => u.air === 'bmr' && u.side === 'att').every((u) => (u.extra as { strike?: string }).strike === u.strike)).toBe(true);
    }
  });
});

describe('the ships the field sinks are the ones the campaign books (b397\'s law)', () => {
  it('fought to the end: booked as sunk on the field, each as a box drops, never undone, and each told once', () => {
    let sunkAll = 0;
    for (const kind of ['harbour', 'fleet'] as const) for (let k = 0; k < 10; k++) {
      let last = 0, dropped = 0;
      const { s, g, target, ids } = fight(kind, k, true, (x) => {
        const n = total(x.outcome().sunk?.[Object.keys(x.outcome().sunk!)[0] as unknown as number]);
        expect(n).toBeGreaterThanOrEqual(last);
        // A ship goes down only when bombs fall.
        if (n > last) expect(x.outcome().dropped).toBeGreaterThan(dropped);
        last = n; dropped = x.outcome().dropped;
      });
      const o = s.outcome();
      const rep = launchRaid(g, kind as RaidKind, target, ids, 'UK', foughtAir(o))!;
      for (const row of rep.ships!) {
        const f = g.fleets.find((x) => x.name === row.fleet)!;
        for (const c of CLASSES) expect(row.sunk[c], `${kind} ${k} ${c}`).toBe(o.sunk![f.id][c] ?? 0);
      }
      const told = s.history().filter((e) => e.what === 'air:ship-sunk');
      expect(told.length).toBe(total(Object.values(o.sunk!)[0]));
      expect(told.every((e) => /^A German (battleship|cruiser|destroyer|carrier|submarine) (settles on the bottom|goes down|rolls over|blows up|is burning out of control|breaks in two)\.$/.test(e.text))).toBe(true);
      expect(new Set(told.map((e) => e.extra!.ship)).size).toBe(told.length);
      // The verdict is the field's and the campaign's alike.
      expect(rep.won).toBe(s.over!.level.includes('victory'));
      sunkAll += told.length;
    }
    expect(sunkAll).toBeGreaterThan(0);
  });

  it('handed to the staff part way: what had gone down stays down, and the staff\'s dice settle the rest', () => {
    for (let k = 0; k < 6; k++) {
      const r = strike('harbour', k);
      const s = airBattle(r.ticket); s.ai.att = true; s.ai.def = true;
      while (!s.over && !s.history().some((e) => e.what === 'air:ship-sunk')) s.step();
      if (s.over) continue;
      const sunk = s.outcome().sunk!;
      expect(s.command({ do: 'dice' }, 'att')).toBe(true);
      const rep = launchRaid(r.g, 'harbour', r.target, r.ids, 'UK', foughtAir(s.outcome()))!;
      const row = rep.ships!.find((x) => x.fleet === 'Battle Fleet')!;
      for (const c of CLASSES) expect(row.sunk[c]).toBeGreaterThanOrEqual(sunk[r.foe.id][c] ?? 0);
    }
  });

  it('the same with the showing on and off: the sinkings, the counts, the verdict, every dispatch', () => {
    for (const kind of ['harbour', 'fleet'] as const) for (let k = 0; k < 4; k++) {
      const on = fight(kind, k, true).s, off = fight(kind, k, false).s;
      expect(on.outcome()).toEqual(off.outcome());
      expect(on.over!.lines).toEqual(off.over!.lines);
      const told = (s: AirSim) => s.history().filter((e) => e.what.startsWith('air:ship')).map((e) => [e.t, e.what, e.text, e.extra]);
      expect(told(on)).toEqual(told(off));
    }
  });

  it('over many raids each class sinks as often as the campaign\'s own law says for the bombers that dropped', () => {
    const got: Record<string, number> = {}, want: Record<string, number> = {};
    const RUNS = 40;
    for (let k = 0; k < RUNS; k++) {
      const { s, ticket } = fight('harbour', 100 + k, false);
      const P = ticket.place.ships!, o = s.outcome();
      const exp = shipHitsBy(P.law, o.dropped * P.mul);
      for (const c of CLASSES) { got[c] = (got[c] ?? 0) + total({ [c]: o.sunk![P.law.fleets[0].id][c] ?? 0 }); want[c] = (want[c] ?? 0) + exp.sunk[c]; }
    }
    for (const c of CLASSES) {
      if (want[c] < 3) continue;
      // Within a tenth, or the luck of 40 raids (a binomial's two standard deviations).
      expect(Math.abs(got[c] - want[c]), `${c}: sank ${got[c]}, the law ${want[c].toFixed(1)}`).toBeLessThanOrEqual(Math.max(0.1 * want[c], 2 * Math.sqrt(want[c])));
    }
  });
});

describe('the plain order honours the raid card (ruling 217)', () => {
  for (const kind of ['harbour', 'fleet'] as const) {
    // Ruling 238: losses inside fly()'s plus or minus 15 per cent (the carrier groups' kinds at their own heights move them).
    it(`costs what the card forecast, inside 15 per cent, with the computer on both sides: the ${kind === 'harbour' ? 'raid on a harbour' : 'strike on a fleet at sea'}`, () => {
      const RUNS = 48;
      const f = strike(kind, 0).ticket.place.forecast;
      let foe = 0, ftr = 0, bmr = 0;
      for (let k = 0; k < RUNS; k++) {
        const { ticket } = strike(kind, 0);
        const s = airBattle({ ...ticket, seed: ticket.seed + k }, { show: false });
        s.ai.att = true; s.ai.def = true;
        while (!s.over) s.step();
        for (const gr of s.outcome().groups) if (gr.side === 'def') foe += gr.lostFtr; else { ftr += gr.lostFtr; bmr += gr.lostBmr; }
      }
      const got = { foe: foe / RUNS, ftr: ftr / RUNS, bmr: bmr / RUNS };
      for (const [k, want] of [['foe', f.foe], ['ftr', f.ftr], ['bmr', f.bmr]] as const) {
        if (want < 3) continue;
        expect(Math.abs(got[k] - want), `${k}: ${got[k].toFixed(1)} against ${want.toFixed(1)}`).toBeLessThan(Math.max(0.15 * want, 1));
      }
    }, 120_000);

    it(`wins as often as the card's chance, within a tenth, the computer flying both sides: the ${kind === 'harbour' ? 'raid on a harbour' : 'strike on a fleet at sea'}`, () => {
      const RUNS = 30;
      let won = 0, chance = 0;
      for (let k = 0; k < RUNS; k++) {
        const { s, g, target, ids, ticket } = fight(kind, 200 + k, false);
        chance += ticket.place.chance;
        const rep = launchRaid(g, kind, target, ids, 'UK', foughtAir(s.outcome()))!;
        expect(rep.won).toBe(s.over!.level.includes('victory'));
        if (rep.won) won++;
      }
      expect(Math.abs(won / RUNS - chance / RUNS), `won ${won} of ${RUNS}, the card ${(chance / RUNS).toFixed(2)}`).toBeLessThanOrEqual(0.1);
    }, 120_000);
  }
});

describe('the set pieces, by place (ruling 238)', () => {
  it('any raid on the harbour of Pearl Harbor is fought over Oahu, any strike in Midway Waters over the atoll, nowhere else a stage', () => {
    expect(stageOf('harbour', 'Pearl Harbor', null)).toBe('oahu');
    expect(stageOf('harbour', 'Oahu', null)).toBe('oahu');
    expect(stageOf('harbour', 'Midway', null)).toBe('midway');
    expect(stageOf('fleet', null, 'midway')).toBe('midway');
    expect(stageOf('fleet', null, 'hawaii')).toBeNull();
    expect(stageOf('harbour', 'Bremen', null)).toBeNull();
    expect(stageOf('fleet', null, 'natl')).toBeNull();
    // The record's months: 7 December 1941 and 4 June 1942.
    expect(recordMonth('oahu', { year: 1941, month: 11 })).toBe(true);
    expect(recordMonth('oahu', { year: 1942, month: 0 })).toBe(false);
    expect(recordMonth('midway', { year: 1942, month: 5 })).toBe(true);
  });

  it('turns the stage onto the board as the ticket turns the map: the aim on the target, the raid\'s course up the board', () => {
    for (const deg of [0, 45, 180, 231, 300]) {
      const turn = (deg * Math.PI) / 180, st = { key: 'oahu', aim: [120, -40] as [number, number], turn };
      const [x, y] = onBoard(st, st.aim, SKY.target);
      expect(x).toBeCloseTo(SKY.target.x, 6); expect(y).toBeCloseTo(SKY.target.y, 6);
      // A kilometre along the raid's course on the stage is a kilometre up the board.
      const [x1, y1] = onBoard(st, [st.aim[0] + 1000 * Math.sin(turn), st.aim[1] + 1000 * Math.cos(turn)], SKY.target);
      expect(x1 - SKY.target.x).toBeCloseTo(0, 6); expect(y1 - SKY.target.y).toBeCloseTo(1000, 6);
      expect(faceOnBoard(st, deg)).toBeCloseTo(0, 9);
    }
  });

  const fakeGame = { nations: { USA: { adj: 'American' }, JAP: { adj: 'Japanese' } } } as unknown as Game;
  const fleet = (id: number, nat: string, ships: Partial<Record<ShipClass, number>>) => ({ id, nat, name: `Fleet ${id}`, ships: { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...ships } });
  const lawOf = (harbour: boolean, fleets: ReturnType<typeof fleet>[]) => ({ harbour, naval: 1, spoil: 0, again: 1, found: 1, surprise: false, have: { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 }, fleets });

  it("Pearl Harbor: the campaign's ships fill the record's berths by class; the record's names only in its month, on its class, for its navy", () => {
    const law = lawOf(true, [fleet(1, 'USA', { bb: 9, cv: 1, ca: 3, dd: 6, ss: 2 })]);
    const st = { key: 'oahu', aim: STAGES.oahu.aim.harbour!, turn: Math.PI };
    const dec = layShips(fakeGame, law, 'harbour', st, SKY.target, { year: 1941, month: 11 });
    expect(dec.filter((p) => p.cls === 'bb').map((p) => p.name)).toEqual(['Nevada', 'Arizona', 'Tennessee', 'West Virginia', 'Maryland', 'Oklahoma', 'California', 'Pennsylvania', null]);
    expect(dec.filter((p) => p.cls === 'bb').map((p) => p.berth)).toEqual(['F-8', 'F-7', 'F-6', 'F-6o', 'F-5', 'F-5o', 'F-3', 'DD1', null]);
    // No carrier lay at the carrier berths that day: the campaign's carrier is there, by its class.
    expect(dec.find((p) => p.cls === 'cv')).toMatchObject({ berth: 'F-9', name: null });
    expect(dec.filter((p) => p.cls === 'dd').slice(0, 3).map((p) => p.name)).toEqual(['Cassin', 'Downes', 'Shaw']);
    // Berths and moorings are never shared, and only the campaign's classes take a berth (not Utah's, Vestal's or Neosho's).
    const berths = dec.map((p) => p.berth).filter(Boolean);
    expect(new Set(berths).size).toBe(berths.length);
    for (const p of dec) if (p.berth) expect(STAGES.oahu.berths.find((b) => b.id === p.berth)!.cls).toBe(p.cls);
    const spots = dec.filter((p) => !p.berth).map((p) => `${p.x.toFixed(0)},${p.y.toFixed(0)}`);
    expect(new Set(spots).size).toBe(spots.length);
    // Battleship Row lies round the aim: Arizona within a kilometre of the board's target.
    const ari = dec.find((p) => p.name === 'Arizona')!;
    expect(Math.hypot(ari.x - SKY.target.x, ari.y - SKY.target.y)).toBeLessThan(1_000);
    // Another month, the same berths by class and no names; another navy in the harbour, no names.
    const mar = layShips(fakeGame, law, 'harbour', st, SKY.target, { year: 1942, month: 2 });
    expect(mar.map((p) => p.berth)).toEqual(dec.map((p) => p.berth));
    expect(mar.every((p) => p.name === null)).toBe(true);
    const jap = layShips(fakeGame, lawOf(true, [fleet(2, 'JAP', { bb: 2 })]), 'harbour', st, SKY.target, { year: 1941, month: 11 });
    expect(jap.every((p) => p.name === null)).toBe(true);
  });

  it('Midway: both carrier forces take the record\'s names in June 1942, by class, in the record\'s order', () => {
    const st = { key: 'midway', aim: STAGES.midway.aim.fleet!, turn: 0.7 };
    const kido = layShips(fakeGame, lawOf(false, [fleet(3, 'JAP', { cv: 5, bb: 2, ca: 3, dd: 12 })]), 'fleet', st, SKY.target, { year: 1942, month: 5 });
    expect(kido.filter((p) => p.cls === 'cv').map((p) => p.name)).toEqual(['Akagi', 'Kaga', 'Hiryu', 'Soryu', null]);
    expect(kido.filter((p) => p.cls === 'bb').map((p) => p.name)).toEqual(['Haruna', 'Kirishima']);
    expect(kido.filter((p) => p.cls === 'dd').every((p) => p.name === null)).toBe(true);
    const us = layShips(fakeGame, lawOf(false, [fleet(4, 'USA', { cv: 3, ca: 2 })]), 'fleet', st, SKY.target, { year: 1942, month: 5 });
    expect(us.filter((p) => p.cls === 'cv').map((p) => p.name)).toEqual(['Yorktown', 'Enterprise', 'Hornet']);
    const later = layShips(fakeGame, lawOf(false, [fleet(3, 'JAP', { cv: 4 })]), 'fleet', st, SKY.target, { year: 1942, month: 9 });
    expect(later.every((p) => p.name === null)).toBe(true);
    // The carriers at the centre of the screen, nobody on top of anybody.
    for (const a of kido) for (const b of kido) if (a.id < b.id) expect(Math.hypot(a.x - b.x, a.y - b.y)).toBeGreaterThan(300);
  });

  it('the stage data: every berth unique, the record\'s battleships at Pearl named once each, every field and mooring inside the board\'s reach', () => {
    for (const [key, S] of Object.entries(STAGES)) {
      expect(new Set(S.berths.map((b) => b.id)).size, key).toBe(S.berths.length);
      for (const p of [...S.berths.map((b) => b.at), ...S.spots.map((s) => s.at), ...S.fields.map((f) => f.at)]) expect(Math.hypot(p[0], p[1]), key).toBeLessThan(45_000);
    }
    const bb = STAGES.oahu.berths.filter((b) => b.cls === 'bb').map((b) => b.name);
    expect(bb.sort()).toEqual(['Arizona', 'California', 'Maryland', 'Nevada', 'Oklahoma', 'Pennsylvania', 'Tennessee', 'West Virginia']);
    expect(CARRIERS.strike.aim.ss).toBe(0);
  });
});
