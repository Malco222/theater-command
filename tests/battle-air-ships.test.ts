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
import { deckMix, layShips, ShipBook, sinkLine, type ShipPiece, type ShipPlace } from '../src/battle/air/ships';
import { STYLES } from '../src/battle/air/styles';
import { Rng } from '../src/core/rng';
import { faceOnBoard, onBoard, recordMonth, stageOf, STAGES } from '../src/battle/air/stages';
import { airTicket, foughtAir, harbourOf, raidGroups } from '../src/battle/air/ticket';

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
 * The strikes: the Home Fleet with six carriers off the German Battle Fleet's harbour, a Taranto in the North Sea; the
 * two fleets together in the open Atlantic, away from every coast's fighters; and the same with the Battle Fleet given
 * two carriers and their groups, so that its own fighters stand over it (the tactics panel's carriers ground).
 */
type Ground = 'harbour' | 'fleet' | 'carriers';
const missionOf = (k: Ground): 'harbour' | 'fleet' => (k === 'harbour' ? 'harbour' : 'fleet');
const BASE: Record<Ground, string> = (() => {
  const harbour = britain(), sea = britain(), carriers = britain();
  {
    const g = harbour, home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
    home.ships.cv = 6;
    sail(g, foe, foe.home, 'port');
    const z = g.seas.find((s) => s.coast.includes(foe.base))!;
    home.zone = z.id; home.stance = 'patrol'; syncWings(g);
  }
  for (const g of [sea, carriers]) {
    const home = named(g, 'Home Fleet'), foe = named(g, 'Battle Fleet');
    home.ships.cv = 6;
    if (g === carriers) foe.ships.cv = 2;
    for (const f of [home, foe]) { f.zone = zoneOf(g, 'natl'); f.stance = 'patrol'; f.lane = undefined; }
    syncWings(g);
  }
  return { harbour: saveGame(harbour), fleet: saveGame(sea), carriers: saveGame(carriers) };
})();

/** The k-th battle of a strike: the game as set, with that battle's own luck. */
function strike(ground: Ground, k: number) {
  const g = loadGame(BASE[ground])!, kind = missionOf(ground);
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  const foe = named(g, 'Battle Fleet');
  const target = kind === 'harbour' ? foe.base : foe.id;
  const ids = raidGroups(g, kind, target, 'UK');
  const { ticket, why } = airTicket(g, kind, target, ids, 'UK', { ships: true });
  if (!ticket) throw new Error(why ?? 'no ticket');
  return { g, target, ids, ticket, foe };
}

function fight(kind: Ground, k: number, show = true, each?: (s: AirSim) => void): { s: AirSim } & ReturnType<typeof strike> {
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
      // The 3D game reads each box's kind of attack and every group's aircraft from the opening view, before the clock
      // starts (the scene builds every group from it), and the first tick says the same.
      type Shown = { model?: string; form?: string; strike?: string };
      const opening = new Map(s.view().units.map((u) => [u.id, u.extra as Shown | undefined]));
      expect(s.units.filter((u) => u.air === 'bmr' && u.side === 'att').every((u) => opening.get(u.id)?.strike === u.strike)).toBe(true);
      s.step();
      for (const u of s.view().units) {
        const x = u.extra as Shown, o = opening.get(u.id);
        expect(o?.model).toBe(x.model); expect(o?.form).toBe(x.form); expect(o?.strike).toBe(x.strike);
      }
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
      expect(told.every((e) => /^A German (battleship|cruiser|destroyer|carrier|submarine) (settles on the bottom|goes down|rolls over|blows up|is ablaze from end to end and cannot be saved|breaks in two)\.$/.test(e.text))).toBe(true);
      expect(new Set(told.map((e) => e.extra!.ship)).size).toBe(told.length);
      // The verdict is the field's and the campaign's alike.
      expect(rep.won).toBe(s.over!.level.includes('victory'));
      // The bombers line names what the bombs fell on: the fleet at sea, the port in harbour.
      expect(s.over!.lines[0]).toContain(` dropped their bombs on ${kind === 'fleet' ? 'the Battle Fleet' : g.regions[target].name}`);
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
    for (const kind of ['harbour', 'fleet', 'carriers'] as const) for (let k = 0; k < 4; k++) {
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
  const said: Record<Ground, string> = { harbour: 'raid on a harbour', fleet: 'strike on a fleet at sea', carriers: 'strike on a fleet with carriers, its own fighters over it' };
  for (const kind of ['harbour', 'fleet', 'carriers'] as const) {
    // Ruling 238: losses inside fly()'s plus or minus 15 per cent (the carrier groups' kinds at their own heights move them).
    it(`costs what the card forecast, inside 15 per cent, with the computer on both sides: the ${said[kind]}`, () => {
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

    it(`wins as often as the card's chance, within a tenth, the computer flying both sides: the ${said[kind]}`, () => {
      const RUNS = 30;
      let won = 0, chance = 0;
      for (let k = 0; k < RUNS; k++) {
        const { s, g, target, ids, ticket } = fight(kind, 200 + k, false);
        chance += ticket.place.chance;
        const rep = launchRaid(g, missionOf(kind), target, ids, 'UK', foughtAir(s.outcome()))!;
        expect(rep.won).toBe(s.over!.level.includes('victory'));
        if (rep.won) won++;
      }
      expect(Math.abs(won / RUNS - chance / RUNS), `won ${won} of ${RUNS}, the card ${(chance / RUNS).toFixed(2)}`).toBeLessThanOrEqual(0.1);
    }, 120_000);
  }
});

describe("strikes go for the carriers, and a fleet's own fighters stand over it (the tactics panel's S1 to S3, ruling 239)", () => {
  const T = SKY.target;
  const decksOf = (g: Game, forces: { corps: number }[]): number[] => forces.map((f) => f.corps).filter((c) => g.airFleets.find((x) => x.id === c)?.ship !== undefined);
  type Shown = { quarry?: number };

  it("S1: a fleet's own fighters start in the air over it, toward the strike, warned by the lookouts; a harbour and a fleet without decks as before", () => {
    const { g, ticket } = strike('carriers', 0);
    const P = ticket.place, decks = decksOf(g, ticket.def.forces);
    expect(decks.length).toBeGreaterThan(0);
    const raid = [...new Set(ticket.att.forces.map((f) => f.corps))].map((c) => P.starts[c]);
    const rx = raid.reduce((t, p) => t + p.x, 0) / raid.length - T.x, ry = raid.reduce((t, p) => t + p.y, 0) / raid.length - T.y;
    for (const c of decks) {
      const st = P.starts[c];
      expect(st).toMatchObject({ band: 1, wait: false, over: true });
      expect(Math.hypot(st.x - T.x, st.y - T.y)).toBeCloseTo(STYLES.cap.ahead, 3);
      // On the line from the fleet to the strike, on the strike's side.
      expect(((st.x - T.x) * rx + (st.y - T.y) * ry) / (STYLES.cap.ahead * Math.hypot(rx, ry))).toBeCloseTo(1, 6);
    }
    for (const kind of ['harbour', 'fleet'] as const) expect(Object.values(strike(kind, 0).ticket.place.starts).some((x) => x.over), kind).toBe(false);
    // On the board it faces the strike it is out to meet, not the fleet behind it (the 3D game turns it from there).
    const s = airBattle(ticket);
    for (const u of s.units.filter((x) => x.side === 'def' && decks.includes(x.corps))) {
      const want = Math.atan2(P.starts[u.corps].x - T.x, P.starts[u.corps].y - T.y);
      expect(Math.abs(Math.atan2(Math.sin(u.face - want), Math.cos(u.face - want)))).toBeLessThan(1e-6);
    }
  });

  it('S1: the cover meets the strike before any box is on its run, climbs above it on its staff\'s style, and no box dives on a fighter', () => {
    const RUNS = 24;
    let met = 0, climbed = 0, styled = 0;
    for (let k = 0; k < RUNS; k++) {
      const { ticket, g } = strike('carriers', k);
      const decks = new Set(decksOf(g, ticket.def.forces));
      const s = airBattle(ticket, { show: false }); s.ai.att = true; s.ai.def = true;
      const cover = s.units.filter((u) => u.side === 'def' && decks.has(u.corps));
      expect(cover.length).toBeGreaterThan(0);
      expect(cover.every((u) => u.job.do !== 'wait')).toBe(true);
      let first: number | null = null, run: number | null = null;
      while (!s.over) {
        s.step();
        if (first === null && cover.some((u) => u.contacts.length)) first = s.t;
        if (run === null && s.units.some((u) => u.side === 'att' && u.air === 'bmr' && Math.hypot(u.x - T.x, u.y - T.y) <= T.r)) run = s.t;
      }
      if (first !== null && (run === null || first <= run)) met++;
      if ((ticket.place.defence?.climb ?? 0) > 0) {
        styled++;
        if (s.history().some((e) => e.what === 'air:climb' && cover.some((u) => u.id === e.unit) && /lookouts/.test(e.text))) climbed++;
      }
      // Only fighters dive (S0): a box above a fighter is simply in contact.
      expect(s.history().filter((e) => e.what === 'air:bounce').every((e) => s.units.find((u) => u.id === e.unit)!.air === 'ftr')).toBe(true);
    }
    expect(met / RUNS, `met first in ${met} of ${RUNS}`).toBeGreaterThanOrEqual(0.8);
    expect(styled).toBeGreaterThan(0);
    expect(climbed / styled, `climbed in ${climbed} of ${styled}`).toBeGreaterThanOrEqual(0.9);
  }, 120_000);

  it("S2: every wing's first quarry is a carrier while one is afloat, and a wing's boxes go for one ship together", () => {
    for (let k = 0; k < 12; k++) {
      const { ticket } = strike('carriers', k);
      const s = airBattle(ticket);
      const pieces = s.view().ships!.pieces, boxes = s.units.filter((u) => u.side === 'att' && u.air === 'bmr');
      const opening = new Map(s.view().units.map((u) => [u.id, (u.extra as Shown).quarry]));
      expect(boxes.length).toBeGreaterThan(0);
      for (const b of boxes) {
        const q = opening.get(b.id);
        expect(pieces.find((p) => p.id === q)?.cls, `box ${b.id}`).toBe('cv');
        expect(boxes.filter((x) => x.corps === b.corps).every((x) => opening.get(x.id) === q)).toBe(true);
      }
    }
  });

  const law = (harbour: boolean, ships: Partial<Record<ShipClass, number>>) => {
    const all = { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...ships };
    return { harbour, naval: 1, spoil: 0, again: 1, found: 1, surprise: false, have: { ...all }, fleets: [{ id: 1, nat: 'JAP', name: 'Fleet 1', ships: all }] };
  };
  const fakeGame = { nations: { USA: { adj: 'American' }, JAP: { adj: 'Japanese' } } } as unknown as Game;
  const placeOf = (harbour: boolean, ships: Partial<Record<ShipClass, number>>, stage: ShipPlace['stage'] = null): ShipPlace => {
    const L = law(harbour, ships);
    return { law: L, mul: 1, stage, pieces: layShips(fakeGame, L, harbour ? 'harbour' : 'fleet', stage, T, { year: 1941, month: 11 }), wings: [], surprise: false };
  };
  const book = (P: ShipPlace, seed = 5): ShipBook => new ShipBook(P, new Rng(seed), new Rng(seed + 1), 200);

  it('S2: the wings share the carriers out, fewest first; a torpedo never takes a ship in dock; a wing whose quarry sinks takes the next', () => {
    const P = placeOf(false, { cv: 2, bb: 2, ca: 2, dd: 6 });
    const B = book(P);
    // Wing 1 is led by its dive bombers (its largest part); wing 2 is a land group's box; wing 3 torpedo bombers.
    B.aim([{ corps: 1, strike: 'torpedo', n: 10 }, { corps: 1, strike: 'dive', n: 12 }, { corps: 2, n: 20 }, { corps: 3, strike: 'torpedo', n: 9 }]);
    const cvs = P.pieces.filter((p) => p.cls === 'cv').map((p) => p.id);
    const q = [1, 2, 3].map((c) => B.quarryOf(c)!);
    expect(q.every((id) => cvs.includes(id))).toBe(true);
    // No carrier has two wings while another has none.
    for (const c of cvs) expect(q.filter((x) => x === c).length).toBeGreaterThanOrEqual(1);
    expect(B.quarryOf(9)).toBeUndefined();
    // Wing 1's carrier goes down: it takes the other.
    const was = B.quarryOf(1)!;
    B.pieces.find((p) => p.id === was)!.sunk = { t: 0, how: 'burn' };
    B.drop(1, 'dive', 1, 1);
    expect(B.quarryOf(1)).toBe(cvs.find((c) => c !== was));
    // In harbour on 7 December 1941, the battleship in dry dock is never a torpedo wing's quarry; a dive wing weighs every ship alike.
    const st = { key: 'oahu', aim: STAGES.oahu.aim.harbour!, turn: Math.PI };
    const H = placeOf(true, { bb: 8 }, st);
    expect(H.pieces.some((p) => p.dock)).toBe(true);
    const D = book(H);
    D.aim([{ corps: 1, strike: 'torpedo', n: 10 }, { corps: 2, strike: 'dive', n: 10 }]);
    const tq = H.pieces.find((p) => p.id === D.quarryOf(1))!;
    expect(tq.dock).toBeFalsy();
    expect(tq.open).toBe(Math.max(...H.pieces.filter((p) => !p.dock).map((p) => p.open)));
    // A wing led by its dive bombers, with torpedo bombers in it, takes a battleship its torpedoes can reach: outboard.
    const W = book(H);
    W.aim([{ corps: 1, strike: 'torpedo', n: 6 }, { corps: 1, strike: 'level', n: 15 }, { corps: 1, strike: 'dive', n: 19 }]);
    expect(H.pieces.find((p) => p.id === W.quarryOf(1))!.open).toBe(tq.open);
  });

  it("S3: a hit in the quarry's class lands on the quarry; the hits by class and the sinkings are as without quarries", () => {
    for (let seed = 1; seed < 40; seed++) {
      const P = placeOf(false, { cv: 3, bb: 2, ca: 2, dd: 6 });
      const A = book(P, seed), B = book(P, seed);
      A.aim([{ corps: 1, strike: 'torpedo', n: 40 }]);
      const q = A.quarryOf(1)!;
      const na: string[] = [], nb: string[] = [];
      for (const [k, n] of [[1, 30], [2, 70], [3, 120], [4, 200]] as const) {
        // (the drawn hits: a hit given to a ship that sinks without one is shown just before it goes down, K4a)
        const a = A.drop(n, 'torpedo', k, 1), b = B.drop(n, 'torpedo', k);
        for (const [i, x] of a.entries()) {
          if (x.given) { expect(a[i + 1]).toMatchObject({ what: 'air:ship-sunk', piece: { id: x.piece.id } }); continue; }
          na.push(`${x.what}:${x.piece.cls}`);
          if (x.what === 'air:ship-hit' && x.piece.cls === 'cv' && !A.pieces.find((p) => p.id === q)!.sunk) expect(x.piece.id).toBe(q);
        }
        for (const x of b) if (!x.given) nb.push(`${x.what}:${x.piece.cls}`);
      }
      expect(na).toEqual(nb);
      expect(A.sunk()).toEqual(B.sunk());
    }
  });

  it('every ship that goes down is shown taking a hit first, from the box whose bombs sank it (K4a; shown only)', () => {
    for (const kind of ['harbour', 'fleet', 'carriers'] as const) for (let k = 0; k < 8; k++) {
      const { s } = fight(kind, k, false);
      const h = s.history();
      for (const e of h.filter((x) => x.what === 'air:ship-sunk')) {
        const hit = h.find((x) => x.what === 'air:ship-hit' && x.extra!.ship === e.extra!.ship && x.t <= e.t);
        expect(hit, `${kind} ${k}: ship ${e.extra!.ship}`).toBeDefined();
        expect(h.indexOf(hit!)).toBeLessThan(h.indexOf(e));
      }
    }
  }, 120_000);

  it('S3: a carrier that goes down was a wing\'s quarry (K4)', () => {
    let sunk = 0, quarried = 0;
    for (let k = 0; k < 24; k++) {
      const seen = new Set<number>();
      const { s } = fight('carriers', k, true, (x) => { for (const u of x.view().units) { const q = (u.extra as Shown | undefined)?.quarry; if (q !== undefined) seen.add(q); } });
      for (const e of s.history().filter((x) => x.what === 'air:ship-sunk')) {
        if (s.view().ships!.pieces.find((p) => p.id === e.extra!.ship)!.cls !== 'cv') continue;
        sunk++;
        if (seen.has(e.extra!.ship as number)) quarried++;
      }
    }
    expect(sunk).toBeGreaterThan(0);
    expect(quarried / sunk, `${quarried} of ${sunk}`).toBeGreaterThanOrEqual(0.9);
  }, 120_000);
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

  it("says it in the UI director's words: the harbour by name, a ship by class and name, the player's own as 'your'", () => {
    expect(harbourOf('Pearl Harbor')).toBe('Pearl Harbor');
    expect(harbourOf('Taranto')).toBe('the harbour of Taranto');
    const p = (x: Partial<ShipPiece>): ShipPiece => ({ id: 1, fleet: 1, nat: 'ITA', adj: 'Italian', cls: 'dd', name: null, berth: null, x: 0, y: 0, face: 0, open: 1, ...x });
    expect(sinkLine(p({ nat: 'USA', adj: 'American', cls: 'bb', name: 'Arizona' }), 'explode', 'USA')).toBe('The battleship Arizona blows up.');
    expect(sinkLine(p({}), 'break', 'UK')).toBe('An Italian destroyer breaks in two.');
    expect(sinkLine(p({ nat: 'GER', adj: 'German', cls: 'ca' }), 'settle', null)).toBe('A German cruiser settles on the bottom.');
    expect(sinkLine(p({ nat: 'UK', adj: 'British', cls: 'ca' }), 'sink', 'UK')).toBe('One of your cruisers goes down.');
    expect(sinkLine(p({ nat: 'JAP', adj: 'Japanese', cls: 'cv', name: 'Akagi' }), 'burn', 'USA')).toBe('The carrier Akagi is ablaze from end to end and cannot be saved.');
  });
});
