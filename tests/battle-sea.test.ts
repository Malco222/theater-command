// Fleet actions on the field (src/battle/sea): the pieces, the orders of the first slice (ruling 217: course, attack,
// speed, withdraw, carrier strike, and one press to hand the rest to the dice), sinking within what the dice allow,
// and the ratio handed back.
import { describe, expect, it } from 'vitest';
import { runOut } from '../src/battle/core/clock';
import { DT, type FieldEvent, type Zone } from '../src/battle/core/types';
import { drawUp, groupSizes, hullsOf } from '../src/battle/sea/pieces';
import { RULES_CLASSES, SEA } from '../src/battle/sea/numbers';
import { diceRange, lossShares, seaChance, seaLevel, seaRatio, shipWords, sunkCaps } from '../src/battle/sea/result';
import { SeaSim, seaWind, seaWindSpeed, type SeaLosses } from '../src/battle/sea/sim';
import { carriersOnly, fieldFights, fleetTicket, plainTicket, rowsOf, type PlainFleet } from '../src/battle/sea/ticket';
import { SEA_CLASSES, type SeaClass, type SeaShips, type SeaUnit, type ShipRow } from '../src/battle/sea/types';
import { createGame } from '../src/campaign/game';
import { fleetPower } from '../src/campaign/navy';
import { foughtOf } from '../src/battle/sea/handoff';
import { PRACTICE, practiceTicket } from '../src/battle/sea/practice';
import { headCount, hurtAfloat, ofFleet, repairLines, theFleet } from '../src/battle/sea/words';
import { SEA_CUES, SEA_ZONE_LOOPS, seaLoop } from '../src/battle/sea/cues';
import { evenLuck } from './sea-luck';
import simText from '../src/battle/sea/sim.ts?raw';
import NATIONS from '../data/campaign/nations.json';

const HOME: PlainFleet = { name: 'Home Fleet', nat: 'UK', ships: { bb: 2, cv: 1, ca: 3, dd: 8 } };
const BATTLE: PlainFleet = { name: 'Battle Fleet', nat: 'GER', ships: { bb: 2, ca: 3, dd: 12 } };

/** A battle with the player on the attacking side and the computer on the other, unless both are asked for. */
function battle(seed = 11, o: { human?: 'att' | 'def' | null; odds?: number; att?: PlainFleet[]; def?: PlainFleet[] } = {}): SeaSim {
  return new SeaSim(plainTicket({ att: o.att ?? [HOME], def: o.def ?? [BATTLE], seed, odds: o.odds ?? 1, human: o.human === undefined ? 'att' : o.human }));
}
const run = (sim: SeaSim, seconds: number): void => { for (let i = 0; i < Math.round(seconds / DT) && !sim.over; i++) sim.step(); };
const piece = (sim: SeaSim, side: 'att' | 'def', cls: string): SeaUnit => sim.units.find((u) => u.side === side && u.extra.cls === cls)!;
const events = (sim: SeaSim): FieldEvent[] => sim.view().events;
/** A destroyer piece's full salvo: its bank at the cap (torp.bank seconds of its guns' weight now). */
const salvo = (s: any, u: SeaUnit): number => SEA.fire.rate * s.output(u) * s.day(u.side) * SEA.torp.bank;

describe('the pieces', () => {
  it('b415: the gun classes have one best range, one reach and no punch of their own; carriers stand off beyond the guns', () => {
    const { bb, ca, dd, cv } = SEA.classes;
    for (const c of [ca, dd]) { expect(c.best).toBe(bb.best); expect(c.reach).toBe(bb.reach); }
    expect(cv.standoff!).toBeGreaterThan(bb.reach);
    for (const c of SEA_CLASSES) expect(SEA.classes[c]).not.toHaveProperty('punch');
  });

  it('b415: the same wear takes the same weight out of a battleship as out of a piece of destroyers', () => {
    const weightLost = (cls: 'bb' | 'dd'): number => {
      const sim = new SeaSim(plainTicket({ att: [HOME], def: [BATTLE], seed: 11, human: null }));
      const s = sim as unknown as { evenWear(u: SeaUnit, w: number): number; wear(u: SeaUnit, a: number): void; weight0: Record<string, number> };
      const foe = sim.units.find((u) => u.side === 'def' && u.extra.cls === cls)!;
      const before = sim.lostShare('def');
      s.wear(foe, s.evenWear(foe, 0.6));
      return (sim.lostShare('def') - before) * s.weight0.def;
    };
    expect(weightLost('bb')).toBeCloseTo(0.6 / SEA.fire.hull, 6);
    expect(weightLost('dd')).toBeCloseTo(0.6 / SEA.fire.hull, 6);
  });

  it('capital ships sail singly, destroyers in fours, and no piece is called a flotilla or a squadron', () => {
    const { units } = drawUp(plainTicket({ att: [HOME], def: [BATTLE], seed: 1 }).place.fleets);
    const att = units.filter((u) => u.side === 'att');
    expect(att.filter((u) => u.extra.cls === 'bb').every((u) => u.extra.ships0 === 1)).toBe(true);
    expect(att.filter((u) => u.extra.cls === 'dd').map((u) => u.extra.ships0)).toEqual([4, 4]);
    for (const u of units) expect(u.name).not.toMatch(/flotilla|squadron/i);
    expect(att.find((u) => u.extra.cls === 'dd')!.name).toBe('Destroyers 1');
    expect(att.find((u) => u.extra.cls === 'bb')!.name).toBe('Battleship 1');
    expect(att.filter((u) => u.extra.cls === 'ca').map((u) => u.name)).toEqual(['Cruiser 1', 'Cruiser 2', 'Cruiser 3']);
    // Only a carrier has the strike.
    for (const u of units) expect(!!u.abilities?.some((a) => a.key === 'strike')).toBe(u.extra.cls === 'cv');
  });

  it('a great fleet is grouped down to no more than the most pieces a side', () => {
    const big: PlainFleet = { name: 'Grand Fleet', nat: 'UK', ships: { bb: 8, cv: 4, ca: 20, dd: 60 } };
    const fleets = plainTicket({ att: [big], def: [BATTLE], seed: 1 }).place.fleets;
    const per = groupSizes(fleets.filter((f) => f.side === 'att'));
    expect(per.ca).toBeGreaterThan(1);
    expect(per.dd).toBeGreaterThan(4);
    const { units } = drawUp(fleets);
    const att = units.filter((u) => u.side === 'att');
    expect(att.length).toBeLessThanOrEqual(SEA.pieces.most);
    // Every ship is in some piece.
    for (const c of SEA_CLASSES) expect(att.filter((u) => u.extra.cls === c).reduce((n, u) => n + u.extra.ships0, 0)).toBe(big.ships[c] ?? 0);
  });

  it('fleets of the same weight and make draw up on the same front, however many pieces they come in (b438)', () => {
    // Many light pieces against fewer heavier ones of the same weight: the same ships in the same proportions, one side
    // in two fleets of plain ships, the other in one fleet with a heavier multiplier.
    const fleets = plainTicket({ seed: 1, odds: 1,
      att: [{ name: 'A', nat: 'UK', ships: { bb: 4, ca: 12, dd: 32 } }, { name: 'B', nat: 'UK', ships: { bb: 2, ca: 6, dd: 16 } }],
      def: [{ name: 'C', nat: 'GER', ships: { bb: 3, ca: 9, dd: 24 } }] }).place.fleets;
    const { units } = drawUp(fleets);
    const front = (side: 'att' | 'def', c: 'bb' | 'ca' | 'dd'): number => {
      const xs = units.filter((u) => u.side === side && u.extra.cls === c).map((u) => u.x).sort((a, b) => a - b);
      return ((xs[xs.length - 1] - xs[0]) * xs.length) / (xs.length - 1); // the row's pieces times their spacing
    };
    const pieces = (side: 'att' | 'def'): number => units.filter((u) => u.side === side).length;
    expect(pieces('att')).toBeGreaterThan(pieces('def'));
    for (const c of ['bb', 'ca', 'dd'] as const) expect(front('att', c), c).toBeCloseTo(front('def', c), 6);
    // Each row is pieces.front metres times its share of its side's weight, its pieces evenly along it (b458): on a side
    // of 3 battleships, 4 cruisers and 8 destroyers (weight 50) the battleships stand pieces.front x 10/50 apart, the
    // cruisers pieces.front x 3/50 and the fours of destroyers pieces.front x 4/50, whatever their class.
    const even = drawUp(plainTicket({ seed: 1, att: [{ name: 'A', nat: 'UK', ships: { bb: 3, ca: 4, dd: 8 } }], def: [BATTLE] }).place.fleets).units
      .filter((u) => u.side === 'att');
    expect(SEA.pieces.front).toBe(3250);
    for (const [c, w] of [['bb', 10], ['ca', 3], ['dd', 4]] as const) {
      const xs = even.filter((u) => u.extra.cls === c).map((u) => u.x).sort((a, b) => a - b);
      expect(xs[1] - xs[0], c).toBeCloseTo((SEA.pieces.front * w) / 50, 6);
    }
  });

  it("rows of different classes carrying the same share of their side's weight draw up equally wide (b458)", () => {
    const width = (units: SeaUnit[], side: 'att' | 'def', c: SeaClass): number => {
      const xs = units.filter((u) => u.side === side && u.extra.cls === c).map((u) => u.x).sort((a, b) => a - b);
      return ((xs[xs.length - 1] - xs[0]) * xs.length) / (xs.length - 1); // the row's pieces times their spacing
    };
    // A quarter of the side's weight in each class: 3 battleships, 3 carriers with decks worth 10/7 of a carrier's
    // power, 10 cruisers and 30 destroyers (seven fours and a pair), 30 of weight each. Every row is a quarter of
    // pieces.front wide, though the rows have 3, 3, 10 and 8 pieces.
    const quarters = drawUp(plainTicket({ seed: 1, def: [BATTLE],
      att: [{ name: 'A', nat: 'UK', ships: { bb: 3, ca: 10, dd: 30 } }, { name: 'B', nat: 'UK', ships: { cv: 3 }, deck: 10 / 7 }] }).place.fleets).units;
    for (const c of SEA_CLASSES) expect(width(quarters, 'att', c), c).toBeCloseTo(SEA.pieces.front / 4, 6);
    // A light line's cruisers stand on the same front as a battle line's battleships when each carries the same share
    // of its side: 10 cruisers and 10 destroyers against 3 battleships and 10 destroyers, three quarters in each.
    const lines = drawUp(plainTicket({ seed: 1, odds: 1,
      att: [{ name: 'A', nat: 'UK', ships: { ca: 10, dd: 10 } }], def: [{ name: 'C', nat: 'ITA', ships: { bb: 3, dd: 10 } }] }).place.fleets).units;
    expect(width(lines, 'att', 'ca')).toBeCloseTo(width(lines, 'def', 'bb'), 6);
    expect(width(lines, 'att', 'ca')).toBeCloseTo((SEA.pieces.front * 3) / 4, 6);
    expect(width(lines, 'att', 'dd')).toBeCloseTo(width(lines, 'def', 'dd'), 6);
  });

  it("each side's hulls come to the same multiple of its weight, so toughness picks who sinks, not who wins", () => {
    const { units, info } = drawUp(plainTicket({ att: [HOME], def: [BATTLE], seed: 1 }).place.fleets);
    for (const side of ['att', 'def'] as const) {
      let w = 0, h = 0;
      for (const u of units) if (u.side === side) { const p = info.get(u.id)!; w += p.weight * u.extra.ships0; h += p.hull * u.extra.ships0; }
      expect(h / w).toBeCloseTo(SEA.fire.hull, 9);
    }
    // A battleship takes more wear for its weight than a destroyer, as the rules sink them.
    const bb = units.find((u) => u.extra.cls === 'bb')!, dd = units.find((u) => u.extra.cls === 'dd' && u.side === bb.side)!;
    const per = (u: SeaUnit): number => info.get(u.id)!.hull / info.get(u.id)!.weight;
    expect(per(bb) / per(dd)).toBeCloseTo(RULES_CLASSES.dd.tough / RULES_CLASSES.bb.tough, 9);
  });
});

describe('the same seed gives the same action', () => {
  it('plays out identically, and another seed plays out otherwise', () => {
    const play = (seed: number): string => { const s = battle(seed, { human: null }); runOut(s); return JSON.stringify([s.over, s.units]); };
    expect(play(5)).toBe(play(5));
    expect(play(6)).not.toBe(play(5));
  });
});

describe('how the staff sails a fleet', () => {
  it('until the guns meet a fleet keeps together: no faster than its slowest gun ship, the screen on station ahead of the line (b407)', () => {
    const sim = battle(5, { human: null, att: [{ name: 'Atlantic Fleet', nat: 'USA', ships: { bb: 4, ca: 4, dd: 8 } }], def: [{ name: '2nd Fleet', nat: 'ITA', ships: { ca: 7, dd: 25 } }] });
    const ahead = (u: SeaUnit): number => (u.side === 'att' ? u.y : -u.y);
    let steps = 0;
    while (sim.contactTime === null && !sim.over && steps++ < 6000) {
      sim.step();
      // No ship lies stopped waiting for the line (the review of b407: destroyers stopped dead for twenty seconds).
      for (const u of sim.units) expect(u.extra.way, u.name).toBeGreaterThan(0.5);
      // Way comes off slowly (speeds.accel): after half a minute every gun ship sails no faster than the slowest.
      if (sim.t < 30) continue;
      for (const side of ['att', 'def'] as const) {
        const guns = sim.units.filter((u) => u.side === side && u.extra.cls !== 'cv');
        const slowest = Math.min(...guns.map((u) => SEA.classes[u.extra.cls as 'bb' | 'ca' | 'dd'].speed));
        for (const u of guns) expect(u.extra.way, u.name).toBeLessThanOrEqual(slowest * SEA.speeds.cruise + 1e-9);
      }
    }
    expect(sim.contactTime).not.toBeNull();
    // The whole fleet comes into the fight together: when the guns meet, every gun ship is on station, and the
    // destroyers are still ahead of the battle line.
    for (const side of ['att', 'def'] as const) {
      const guns = sim.units.filter((u) => u.side === side && u.extra.cls !== 'cv');
      const rear = Math.min(...guns.map(ahead));
      for (const u of guns) expect(ahead(u) - rear, u.name).toBeLessThanOrEqual(SEA.ai.station + 50);
      const bbs = guns.filter((u) => u.extra.cls === 'bb');
      for (const dd of guns.filter((u) => u.extra.cls === 'dd')) for (const bb of bbs) expect(ahead(dd), `${dd.name} ahead of ${bb.name}`).toBeGreaterThan(ahead(bb));
    }
  });

  it('a fleet that breaks off before the guns meet is run down at full speed, not at the battle line\'s', () => {
    const sim = battle(5, { human: null });
    // The fleets close until the nearest of them are about 6 km apart, then the defender's admiral breaks off.
    const gap = (): number => Math.min(...sim.units.filter((u) => u.side === 'att').flatMap((a) => sim.units.filter((b) => b.side === 'def').map((b) => Math.hypot(a.x - b.x, a.y - b.y))));
    while (gap() > 6000 && sim.contactTime === null) sim.step();
    expect(sim.contactTime).toBeNull();
    expect(sim.command({ do: 'withdrawAll', side: 'def' }, 'def')).toBe(true);
    run(sim, 30);
    for (const dd of sim.units.filter((u) => u.side === 'att' && u.extra.cls === 'dd')) expect(dd.extra.way, dd.name).toBeGreaterThan(SEA.classes.dd.speed * SEA.speeds.cruise);
  });

  it('carriers with no gun ship left are run down at full speed, not held to the battle line\'s', () => {
    const sim = battle(5, { human: null, att: [{ name: 'Battle Force', nat: 'USA', ships: { bb: 2, ca: 3, dd: 8 } }], def: [{ name: 'Carrier Division', nat: 'JAP', ships: { cv: 2 } }] });
    run(sim, 40);
    expect(sim.contactTime).toBeNull();
    for (const u of sim.units.filter((e) => e.side === 'att')) expect(u.extra.way, u.name).toBeGreaterThan(SEA.classes[u.extra.cls as 'bb' | 'ca' | 'dd'].speed * SEA.speeds.cruise);
  });

  it("a ship the player sends at a speed of its own is not held back by the fleet's", () => {
    const sim = battle();
    const dd = piece(sim, 'att', 'dd');
    expect(sim.command({ do: 'sea:speed', units: [dd.id], speed: 'flank' } as never, 'att')).toBe(true);
    run(sim, 40);
    expect(sim.contactTime).toBeNull();
    expect(dd.extra.way).toBeGreaterThan(SEA.classes.bb.speed * SEA.speeds.cruise);
  });

  it('a ship within broadside reach of the enemy turns its guns onto it, beyond it closes, and too near it opens the range (b407)', () => {
    const sim = battle(5, { human: null });
    const u = piece(sim, 'att', 'dd'), foe = piece(sim, 'def', 'dd');
    const engage = (d: number): number => {
      // The enemy dead ahead of a ship heading straight for it: how far off that bearing the ship would steer.
      u.face = 0; foe.x = u.x; foe.y = u.y + d;
      const want = (sim as unknown as { engage(a: SeaUnit, b: SeaUnit): number }).engage(u, foe);
      return Math.abs(Math.atan2(Math.sin(want), Math.cos(want)));
    };
    expect(engage(SEA.ai.broadside - 250)).toBeCloseTo(Math.PI / 2, 6);
    expect(engage(SEA.classes.dd.best)).toBeCloseTo(Math.PI / 2, 6);
    expect(engage((SEA.ai.broadside + SEA.classes.dd.reach) / 2)).toBeLessThan(1);
    expect(engage(SEA.classes.dd.best * 0.5)).toBeGreaterThan(Math.PI / 2);
  });
});

describe('each ship class to its own job (ruling 239)', () => {
  /** A field with only these pieces left on it, each placed where the test says; the rest are gone. */
  const only = (sim: SeaSim, keep: [SeaUnit, number, number][]): void => {
    for (const u of sim.units) if (!keep.some(([k]) => k === u)) u.state = 'gone';
    for (const [u, x, y] of keep) { u.x = x; u.y = y; }
  };
  const aim = (sim: SeaSim, ...us: SeaUnit[]): void => { for (const u of us) (sim as unknown as { aim(u: SeaUnit): void }).aim(u); };
  const both = (sim: SeaSim, side: 'att' | 'def', cls: string): SeaUnit[] => sim.units.filter((u) => u.side === side && u.extra.cls === cls);

  it('a battleship fires on a battleship before a nearer cruiser, a cruiser on a cruiser before a battleship', () => {
    const sim = battle(11, { human: null });
    const bb = piece(sim, 'att', 'bb'), ca = piece(sim, 'att', 'ca'), ebb = piece(sim, 'def', 'bb'), eca = piece(sim, 'def', 'ca');
    only(sim, [[bb, 10000, 10000], [ca, 10500, 10000], [eca, 10000, 12000], [ebb, 10500, 13000]]);
    aim(sim, bb, ca);
    expect([bb.extra.target, ca.extra.target]).toEqual([ebb.id, eca.id]);
  });

  it('two battleships share their fire over two enemy battleships, with no dice', () => {
    for (const seed of [3, 11, 29]) {
      const sim = battle(seed, { human: null });
      const [b1, b2] = both(sim, 'att', 'bb'), [e1, e2] = both(sim, 'def', 'bb');
      only(sim, [[b1, 9000, 10000], [b2, 11000, 10000], [e1, 9000, 13000], [e2, 11000, 13000]]);
      aim(sim, b1, b2);
      expect(new Set([b1.extra.target, b2.extra.target])).toEqual(new Set([e1.id, e2.id]));
    }
  });

  it('an order to attack given to several pieces names the main effort and each does its job; given to one, it is that piece\'s target', () => {
    const sim = battle(11, { human: 'att' });
    const bb = piece(sim, 'att', 'bb'), ca = piece(sim, 'att', 'ca'), ebb = piece(sim, 'def', 'bb'), eca = piece(sim, 'def', 'ca');
    only(sim, [[bb, 10000, 10000], [ca, 10500, 10000], [eca, 10000, 12000], [ebb, 10500, 13000]]);
    expect(sim.command({ do: 'attack', units: [bb.id, ca.id], target: eca.id }, 'att')).toBe(true);
    expect(sim.orderOf(bb.id)).toEqual({ kind: 'attack', target: eca.id, fleet: true });
    aim(sim, bb);
    expect(bb.extra.target).toBe(ebb.id);
    expect(sim.command({ do: 'attack', units: [bb.id], target: eca.id }, 'att')).toBe(true);
    expect(sim.orderOf(bb.id)).toEqual({ kind: 'attack', target: eca.id });
    aim(sim, bb);
    expect(bb.extra.target).toBe(eca.id);
  });
});

describe("the destroyers' torpedo run and the screen (ruling 239)", () => {
  const only = (sim: SeaSim, keep: [SeaUnit, number, number][]): void => {
    for (const u of sim.units) if (!keep.some(([k]) => k === u)) u.state = 'gone';
    for (const [u, x, y] of keep) { u.x = x; u.y = y; }
  };
  const all = (sim: SeaSim, side: 'att' | 'def', cls: string): SeaUnit[] => sim.units.filter((u) => u.side === side && u.extra.cls === cls);
  /** The guns have met: the bank and the runs count from here. */
  const met = (sim: SeaSim): any => { const s = sim as any; s.contactAt = 0; return s; };

  it("a destroyer's guns take destroyers and cruisers before battleships and carriers, and big ships only when nothing else is in reach", () => {
    const sim = battle(11, { human: null }), s = sim as any;
    const dd = piece(sim, 'att', 'dd'), ebb = piece(sim, 'def', 'bb'), eca = piece(sim, 'def', 'ca');
    only(sim, [[dd, 10000, 10000], [ebb, 10000, 11000], [eca, 10000, 14500]]);
    s.aim(dd);
    expect(dd.extra.target).toBe(eca.id);
    eca.state = 'gone';
    s.aim(dd);
    expect(dd.extra.target).toBe(ebb.id);
  });

  it('a destroyer piece banks only the weight its guns cannot fire, from the moment the guns meet, and no more than bank seconds of it', () => {
    const sim = battle(11, { human: null }), s = sim as any;
    const dd = piece(sim, 'att', 'dd'), edd = piece(sim, 'def', 'dd');
    only(sim, [[dd, 10000, 10000], [edd, 10000, 18000]]);
    // It goes in with its tubes full: a full salvo loaded, drawn in advance from its guns (the ship plan's R5, 05:01Z).
    expect(s.bank[dd.id]).toBeCloseTo(SEA.fire.rate * s.output(dd) * s.day('att') * SEA.torp.bank, 9);
    expect(s.loaded[dd.id]).toBe(s.bank[dd.id]);
    s.bank[dd.id] = 0; s.loaded[dd.id] = 0; // fired off: it fills again only by the bank rule
    s.aim(dd); s.fire(dd);
    expect(s.bank[dd.id]).toBe(0); // before the guns meet nothing is banked
    met(sim);
    const per = SEA.fire.rate * s.output(dd) * s.day('att');
    s.aim(dd); s.fire(dd);
    expect(dd.extra.target).toBe(-1);
    expect(s.bank[dd.id]).toBeCloseTo(per * DT, 12);
    for (let i = 0; i < (SEA.torp.bank + 50) / DT; i++) s.fire(dd);
    expect(s.bank[dd.id]).toBeCloseTo(per * SEA.torp.bank, 9);
    // With an enemy in reach the guns fire it, and nothing more is banked.
    edd.y = 13000;
    const banked = s.bank[dd.id], dealt = s.dealt[dd.id];
    s.aim(dd); s.fire(dd);
    expect(dd.extra.target).toBe(edd.id);
    expect(s.bank[dd.id]).toBe(banked);
    expect(s.dealt[dd.id]).toBeGreaterThan(dealt);
  });

  it('on a run the guns keep firing at what their job finds in reach, so a run takes no weight out of the fight', () => {
    const s = met(battle(11, { human: null }));
    const dd = all(s, 'att', 'dd')[0], ebb = all(s, 'def', 'bb')[0], edd = all(s, 'def', 'dd')[0];
    only(s, [[dd, 10000, 10000], [ebb, 10000, 15000], [edd, 10500, 12000]]);
    s.beginRun(dd, ebb);
    expect(dd.extra.run).toBe(ebb.id);
    const dealt = s.dealt[dd.id], bank = s.bank[dd.id];
    s.aim(dd); s.fire(dd);
    expect(dd.extra.target).toBe(edd.id);
    expect(s.dealt[dd.id]).toBeGreaterThan(dealt);
    expect(s.bank[dd.id]).toBe(bank);
  });

  it("a salvo is the bank, no more and no less: it lands 'travel' seconds after launch on its target and counts as the destroyers' weight", () => {
    const s = met(battle(11, { human: null }));
    const dd = all(s, 'att', 'dd')[0], ebb = all(s, 'def', 'bb')[0];
    only(s, [[dd, 10000, 10000], [ebb, 10000, 12400]]);
    s.bank[dd.id] = 0.05; s.loaded[dd.id] = 0; // earned by its guns: none of it owed
    const lost = s.lostShare('def'), dealt = s.dealt[dd.id];
    s.launch(dd, ebb, null);
    expect(s.bank[dd.id]).toBe(0);
    expect(s.torps).toHaveLength(1);
    expect(s.torps[0].weight).toBe(0.05);
    expect(s.torps[0].at).toBeCloseTo(s.t + SEA.torp.travel, 9);
    expect(events(s).some((e: FieldEvent) => e.what === 'sea:torpedo' && e.unit === dd.id)).toBe(true);
    s.landTorps();
    expect(s.torps).toHaveLength(1); // not yet: it is still running
    s.t += SEA.torp.travel;
    s.landTorps();
    expect(s.torps).toHaveLength(0);
    expect(s.dealt[dd.id] - dealt).toBeCloseTo(0.05, 12);
    expect(s.lostShare('def')).toBeGreaterThan(lost);
    expect(events(s).some((e: FieldEvent) => e.what === 'sea:torpedoHit' && e.unit === ebb.id)).toBe(true);
  });

  it('the launch line is told once a piece, on its first salvo; the hit line never claims every ship of a larger target (the UI director)', () => {
    const s = met(battle(11, { human: null }));
    const dd = all(s, 'att', 'dd')[0], ebb = all(s, 'def', 'bb')[0];
    only(s, [[dd, 10000, 10000], [ebb, 10000, 12400]]);
    events(s);
    const launches = (): FieldEvent[] => events(s).filter((e: FieldEvent) => e.what === 'sea:torpedo' || e.what === 'sea:torpedoHit');
    s.bank[dd.id] = 0.01; s.launch(dd, ebb, null);
    const one = launches();
    expect(one).toHaveLength(1);
    expect(one[0].text).toMatch(/^Four destroyers of .* fire torpedoes at a battleship of /);
    expect(one[0].cue).toBe('torpedo_launch');
    s.bank[dd.id] = 0.01; s.launch(dd, ebb, null);
    const two = launches();
    expect(two).toHaveLength(1);
    expect(two[0].text).toBe(''); // the event and its sound stay, for the scene's wakes; the line is told once
    expect(two[0].cue).toBe('torpedo_launch');
    // One ship: "hit a battleship of ..."; more than one: "find their mark among the battleships of ...".
    s.t += SEA.torp.travel; s.landTorps();
    const hits = launches();
    expect(hits).toHaveLength(2);
    expect(hits[0].text).toMatch(/^Torpedoes from 4 destroyers of .* hit a battleship of /);
    ebb.extra.ships = 2;
    s.bank[dd.id] = 0.01; s.launch(dd, ebb, null); events(s);
    s.t += SEA.torp.travel; s.landTorps();
    expect(launches()[0].text).toMatch(/^Torpedoes from 4 destroyers of .* find their mark among the battleships of /);
  });

  it('a salvo that sinks her is told before she goes down: the hit, then the sinking (the 3D scene\'s log)', () => {
    const s = met(battle(11, { human: null }));
    const dd = all(s, 'att', 'dd')[0], ebb = all(s, 'def', 'bb')[0];
    only(s, [[dd, 10000, 10000], [ebb, 10000, 12400]]);
    ebb.extra.ships = 1; ebb.str = 1e-6;
    events(s);
    s.bank[dd.id] = salvo(s, dd); s.launch(dd, ebb, null); events(s);
    s.t += SEA.torp.travel; s.landTorps();
    const said = events(s).map((e: FieldEvent) => e.what);
    expect(said).toContain('sea:sunk');
    expect(said).toContain('sea:torpedoHit');
    expect(said.indexOf('sea:torpedoHit')).toBeLessThan(said.indexOf('sea:sunk'));
  });

  it("a run closes at flank, launches at its style's press range, turns away for 'away' seconds and rejoins", () => {
    const s = met(battle(11, { human: null }));
    const dd = all(s, 'att', 'dd')[0], ebb = all(s, 'def', 'bb')[0];
    only(s, [[dd, 10000, 8000], [ebb, 10000, 14000]]);
    const free = { kind: 'free' } as const, flank = SEA.classes.dd.speed * SEA.speeds.flank, press = SEA.styles.methodical!.press;
    s.bank[dd.id] = 0.02;
    s.beginRun(dd, ebb);
    const inbound = s.runCourse(dd, free);
    expect(inbound.speed).toBe(flank);
    expect(Math.abs(Math.sin(inbound.want))).toBeLessThan(1e-9); // straight at the target, due north on this field
    expect(s.torps).toHaveLength(0);
    dd.y = ebb.y - press;
    const away = s.runCourse(dd, free);
    expect(s.torps).toHaveLength(1);
    expect(s.runs.get(dd.id).phase).toBe('away');
    expect(away.speed).toBe(flank);
    expect(Math.abs(Math.cos(away.want) + 1)).toBeLessThan(1e-9); // away from it
    s.t += SEA.torp.away;
    expect(s.runCourse(dd, free)).toBeNull();
    expect(s.runs.has(dd.id)).toBe(false);
    expect(dd.extra.run).toBeUndefined();
  });

  it('a destroyer piece ahead of the beam of an enemy big ship in torpedo reach goes for it at once once the guns have met and while the lines are engaged, whatever its guns have (R4; the DD, 08:10Z and 09:48Z)', () => {
    const s = battle(11, { human: null }) as any;
    expect(s.contactAt).toBeNull(); // the guns have not met
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, ebb = all(s, 'def', 'bb')[0], edd = all(s, 'def', 'dd')[0];
    const bb = all(s, 'att', 'bb')[0];
    // The battleship steams toward it from just beyond torpedo reach, a destroyer of hers nearer it; its own battleship
    // has her in gun reach (the lines engaged).
    only(s, [[dd, 10000, 10000], [ebb, 10000, 10000 + SEA.torp.start + 500], [edd, 10000, 14500], [bb, 6000, 16500]]);
    ebb.face = Math.PI;
    s.bank[dd.id] = salvo(s, dd);
    s.aim(dd);
    expect(dd.extra.target).toBe(edd.id); // its guns have the enemy destroyer
    ebb.y = 10000 + SEA.torp.start - 500;
    s.startRuns();
    // In reach and ahead of her beam, but the guns have not met: a run in the approach is the night's (Surigao), and
    // the field fights by day against an alert line. It keeps with its fleet.
    expect(s.runs.has(dd.id)).toBe(false);
    expect(s.keepsStation(dd)).toBe(true);
    s.contactAt = s.t;
    ebb.y = 10000 + SEA.torp.start + 500;
    s.startRuns();
    expect(s.runs.has(dd.id)).toBe(false); // out of torpedo reach: no run
    ebb.y = 10000 + SEA.torp.start - 500;
    s.startRuns();
    expect(s.runs.get(dd.id)?.target).toBe(ebb.id); // in reach, ahead of her beam: at once, the enemy destroyer or not
    expect(s.runs.get(dd.id)?.div).toBe(0); // alone: no division
    expect(s.keepsStation(dd)).toBe(false);
    // Its own battleship out of reach of hers (no cruiser or larger of its side engaged): by day it does not go in.
    s.runs.delete(dd.id); delete dd.extra.run;
    bb.x = 6000; bb.y = 6000;
    s.startRuns();
    expect(s.runs.has(dd.id)).toBe(false);
  });

  it('a destroyer-only side opens no run ahead of the beam by day: unsupported destroyers ran at a heavy line only to cover something (the DD, 09:48Z)', () => {
    const ITD: PlainFleet = { name: '2nd Fleet', nat: 'ITA', ships: { dd: 8 } };
    const s = met(battle(11, { human: null, att: [ITD] }));
    const dd = all(s, 'att', 'dd')[0], ebb = all(s, 'def', 'bb')[0];
    only(s, [[dd, 10000, 10000], [ebb, 10000, 14000]]);
    ebb.face = Math.PI; // steaming toward it: ahead of her beam, in torpedo reach
    s.bank[dd.id] = salvo(s, dd);
    s.startRuns();
    expect(s.runs.has(dd.id)).toBe(false);
    expect(s.engaged('att')).toBe(false);
  });

  it('the lines are engaged wherever its heavy ships have the enemy\'s in gun reach: the ship it runs at need not be the one they fight (the DD, 09:48Z)', () => {
    const s = met(battle(11, { human: null }));
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, [ebb, eb2] = all(s, 'def', 'bb'), bb = all(s, 'att', 'bb')[0];
    // Its battleship fights the second enemy battleship, out of torpedo reach; the first steams at the destroyer, out
    // of its battleship's gun reach.
    only(s, [[dd, 10000, 10000], [ebb, 10000, 14000], [eb2, 3000, 16000], [bb, 3000, 19500]]);
    ebb.face = Math.PI;
    s.bank[dd.id] = salvo(s, dd);
    expect(s.engaged('att')).toBe(true);
    s.startRuns();
    expect(s.runs.get(dd.id)?.target).toBe(ebb.id);
  });

  it('in the chase the enemy ships it runs down are engaged as they were fighting: a run ahead of a fleeing ship\'s beam opens (the DD, 09:48Z)', () => {
    const s = met(battle(11, { human: null }));
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, ebb = all(s, 'def', 'bb')[0], bb = all(s, 'att', 'bb')[0];
    // On her fighting heading she steams at the destroyer, south of her; its battleship has her in gun reach. Her home
    // is north.
    only(s, [[dd, 10000, 10000], [ebb, 10000, 14000], [bb, 6000, 14500]]);
    ebb.face = Math.PI;
    s.bank[dd.id] = salvo(s, dd);
    // Turned away alone while her side fights on, she is no line to engage and no ship to run at.
    ebb.state = 'routing';
    expect(s.engaged('att')).toBe(false);
    s.startRuns();
    expect(s.runs.has(dd.id)).toBe(false);
    // Her side broken off: she is run down, and the lines are engaged as before. But she is turning for home: the
    // destroyer on her bow now is astern of her in seconds, and a run from there is a chase that never reaches press.
    s.breakSide('def', false);
    expect(s.engaged('att')).toBe(true);
    s.startRuns();
    expect(s.runs.has(dd.id)).toBe(false);
    // One between her and home is ahead of her as she flees: it goes in.
    dd.x = 10500; dd.y = 18500;
    s.startRuns();
    expect(s.runs.get(dd.id)?.target).toBe(ebb.id);
  });

  it("on a run what its guns cannot fire bearing end-on pays its own salvo's advance first, before and after it launches; the rest banks (the DD, 08:10Z)", () => {
    const s = met(battle(11, { human: null }));
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, ebb = all(s, 'def', 'bb')[0];
    only(s, [[dd, 10000, 10000], [ebb, 10000, 14000]]);
    const full = salvo(s, dd), per = full / SEA.torp.bank;
    s.bank[dd.id] = full; s.loaded[dd.id] = full;
    // Not on a run: nothing is paid from the guns' lost bearing (fire() takes it only on a run).
    s.store(dd, per * 3);
    expect(s.loaded[dd.id]).toBeCloseTo(full - per * 3, 12); // idle weight earns the loaded salvo as ever (the bank is full)
    s.beginRun(dd, ebb);
    const before = s.loaded[dd.id];
    s.store(dd, per * 2);
    expect(s.loaded[dd.id]).toBeCloseTo(before - per * 2, 12); // before launch: the loaded salvo's unearned part
    expect(s.bank[dd.id]).toBeCloseTo(full, 12);
    // Launched: what is still unearned goes with the salvo, and the lost bearing on the turn away pays it down.
    s.launch(dd, ebb, s.runs.get(dd.id));
    const torp = s.torps.find((t: { from: number }) => t.from === dd.id);
    const drawn0 = torp.drawn;
    expect(drawn0).toBeCloseTo(before - per * 2, 12);
    s.store(dd, per * 4);
    expect(torp.drawn).toBeCloseTo(drawn0 - per * 4, 12);
    expect(s.bank[dd.id]).toBe(0); // nothing banked while the salvo's advance is unpaid
    // More than the advance: the rest banks for the next salvo.
    s.store(dd, torp.drawn + per);
    expect(torp.drawn).toBe(0);
    expect(s.bank[dd.id]).toBeCloseTo(per, 12);
    // It lands owing nothing: the side's guns pay nothing for it.
    s.t += SEA.torp.travel; s.landTorps();
    expect(s.advance.att).toBe(0);
  });

  it("a salvo landed before its guns earned it counts on neither side until its side's gunfire pays for it, as R2's debt (the DD, 09:48Z)", () => {
    const s = met(battle(11, { human: null }));
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, ebb = all(s, 'def', 'bb')[0], bb = all(s, 'att', 'bb')[0];
    only(s, [[dd, 10000, 10000], [ebb, 10000, 12400], [bb, 6000, 12400]]);
    const full = salvo(s, dd);
    s.bank[dd.id] = full; s.loaded[dd.id] = full; // all of it drawn in advance
    const lost = s.lostShare('def');
    s.launch(dd, ebb, null);
    s.t += SEA.torp.travel; s.landTorps();
    expect(s.advance.att).toBeCloseTo(full, 12);
    expect(ebb.str).toBeLessThan(1); // the blow landed
    expect(s.lostShare('def')).toBeCloseTo(lost, 12); // but counts on neither side while owed
    // The side's gunfire pays it, and what is paid then counts, part for part: this shot lands nothing of its own.
    const u0 = s.unpaid.def;
    expect(u0).toBeGreaterThan(0);
    s.aim(bb); s.fire(bb);
    const paid = full - s.advance.att;
    expect(paid).toBeGreaterThan(0);
    expect(s.unpaid.def).toBeCloseTo(u0 * (1 - paid / full), 12);
    expect((s.lostShare('def') - lost) * s.weight0.def).toBeCloseTo((u0 * paid) / full, 12);
  });

  it("what an unpaid salvo took counts on neither side as the take measured it: on a side with coasts' aircraft, on a carrier, on a piece worn through", () => {
    // A side with a carrier and the coasts' aircraft: a blow wears its aircraft at their whole weight and a carrier's
    // deck by its own share, so what the salvo took is not its weight over fire.hull (the design director, 09:48Z).
    const fresh = (): any => met(new SeaSim(plainTicket({ att: [HOME], def: [BATTLE], air: { att: 20, def: 12 }, seed: 3, human: null })));
    const land = (s: any, at: SeaUnit): number => {
      const dd = all(s, 'def', 'dd')[0], full = salvo(s, dd);
      s.bank[dd.id] = full; s.loaded[dd.id] = full; // all of it drawn in advance
      const lost = s.lostShare('att');
      s.launch(dd, at, null);
      s.t += SEA.torp.travel; s.landTorps();
      expect(s.advance.def).toBeCloseTo(full, 12);
      return lost;
    };
    // On a battleship, the side's carrier and coasts' aircraft wearing with her.
    let s = fresh(), bb = all(s, 'att', 'bb')[0];
    let lost = land(s, bb);
    expect(bb.str).toBeLessThan(1);
    expect(s.airLeft.att).toBeLessThan(20);
    expect(s.lostShare('att')).toBeCloseTo(lost, 12);
    // Paid in full by the side's guns, all of what it took counts, no more.
    const ebb = all(s, 'def', 'bb')[0];
    only(s, [[ebb, 10000, 12000], [bb, 10000, 10000], ...s.units.filter((u: SeaUnit) => u.extra.cls === 'cv' && u.side === 'att').map((u: SeaUnit) => [u, u.x, u.y])]);
    for (let i = 0; i < 2000 && s.advance.def > 0; i++) { s.aim(ebb); s.fire(ebb); }
    expect(s.advance.def).toBe(0);
    expect(s.unpaid.att).toBe(0);
    expect(s.lostShare('att')).toBeGreaterThan(lost);
    // On the carrier herself.
    s = fresh();
    const cv = all(s, 'att', 'cv')[0];
    s.deck[cv.id] = 0.5; // her air group half gone: her weight is half her hull's
    lost = land(s, cv);
    expect(cv.str).toBeLessThan(1);
    expect(s.lostShare('att')).toBeCloseTo(lost, 12);
    // On a piece with less hull left than the salvo: what it could not take out is not handed back.
    s = fresh();
    const dd = all(s, 'att', 'dd')[0];
    dd.extra.ships = 1; dd.str = 1e-4;
    lost = land(s, dd);
    expect(dd.state).toBe('gone');
    expect(s.lostShare('att')).toBeCloseTo(lost, 12);
  });

  it('on a run its guns bearing end-on feed the run\'s repayment: the weight it fires plus what pays its salvo is its full broadside', () => {
    const s = met(battle(11, { human: null }));
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, edd = all(s, 'def', 'dd')[0], ebb = all(s, 'def', 'bb')[0];
    // Steaming due north at the enemy destroyer ahead: end-on, at the fire's endOn share.
    only(s, [[dd, 10000, 10000], [edd, 10000, 12000], [ebb, 10000, 16000]]);
    dd.face = 0;
    s.bank[dd.id] = salvo(s, dd); s.loaded[dd.id] = salvo(s, dd);
    s.beginRun(dd, ebb);
    s.aim(dd);
    expect(dd.extra.target).toBe(edd.id);
    const l0 = s.loaded[dd.id], d0 = s.dealt[dd.id], str0 = edd.str;
    s.fire(dd);
    const fired = s.dealt[dd.id] - d0, repaid = l0 - s.loaded[dd.id];
    expect(edd.str).toBeLessThan(str0);
    expect(repaid).toBeGreaterThan(0);
    expect(fired / (fired + repaid)).toBeCloseTo(SEA.fire.endOn, 6);
  });

  it("one the player sends alone at a ship still waits for the guns to meet, as before R4: the cues are the staff's", () => {
    const s = battle(11, { human: null }) as any;
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, ebb = all(s, 'def', 'bb')[0];
    only(s, [[dd, 10000, 10000], [ebb, 10000, 16500]]);
    ebb.face = 0; // steaming away: abaft her beam, no cue
    s.bank[dd.id] = salvo(s, dd);
    s.orders.set(dd.id, { kind: 'attack', target: ebb.id });
    s.startRuns();
    expect(s.runs.has(dd.id)).toBe(false);
    s.contactAt = s.t;
    s.startRuns();
    expect(s.runs.get(dd.id)?.target).toBe(ebb.id);
  });

  it('abaft the beam it waits for the enemy to be slowed by gunfire (the style\'s torpWhen), then runs (R4: North Cape)', () => {
    const s = met(battle(11, { human: null }));
    s.contactAt = s.t;
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, ebb = all(s, 'def', 'bb')[0], bb = all(s, 'att', 'bb')[0];
    only(s, [[dd, 10000, 10000], [ebb, 10000, 13000], [bb, 6000, 13000]]);
    s.bank[dd.id] = salvo(s, dd);
    // Steaming due north, away from it: the destroyer lies astern, abaft her beam, and a run would be a chase.
    ebb.face = 0;
    s.aim(dd); s.startRuns();
    expect(dd.extra.target).toBe(ebb.id);
    expect(s.runs.has(dd.id)).toBe(false);
    // Just abaft the beam (a little over 90 degrees off her bow): still no run.
    ebb.face = Math.PI / 2 - 0.05;
    s.aim(dd); s.startRuns();
    expect(s.runs.has(dd.id)).toBe(false);
    // Just ahead of it: her own way carries her toward it, and the run goes in at once.
    ebb.face = Math.PI / 2 + 0.05;
    s.aim(dd); s.startRuns();
    expect(s.runs.get(dd.id)?.target).toBe(ebb.id);
    // Abaft her beam again: once the side's battleship's gunfire has shaken her to the style's torpWhen, it runs.
    s.runs.delete(dd.id); delete dd.extra.run;
    ebb.face = 0;
    ebb.mor = SEA.styles.methodical!.torpWhen + 0.05;
    s.aim(dd); s.startRuns();
    expect(s.runs.has(dd.id)).toBe(false);
    ebb.mor = SEA.styles.methodical!.torpWhen - 0.05;
    s.aim(dd); s.startRuns();
    expect(s.runs.get(dd.id)?.target).toBe(ebb.id);
  });

  it('an enemy heavy ship closing on one of its carriers faster than she goes sends it at her at once, abaft her beam or not (R4: Samar; the DD, 09:48Z)', () => {
    const CVS: PlainFleet = { name: 'Escort Force', nat: 'USA', ships: { cv: 2, dd: 6 } };
    const s = new SeaSim(plainTicket({ att: [CVS], def: [BATTLE], seed: 11, human: null })) as any;
    s.contactAt = s.t;
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, cv = all(s, 'att', 'cv')[0];
    const eca = all(s, 'def', 'ca')[0], ebb = all(s, 'def', 'bb')[0];
    // The carrier makes south at 10 m/s, far beyond the cruiser's gun reach; the cruiser comes south after her from
    // 9,000 m, the destroyer astern of the cruiser (abaft her beam) within torpedo reach; the battleship steams toward
    // the destroyer, ahead of her beam.
    only(s, [[dd, 10000, 15000], [cv, 10000, 4000], [eca, 10000, 13000], [ebb, 13000, 18000]]);
    cv.face = Math.PI; cv.extra.way = 10; eca.face = Math.PI; ebb.face = Math.PI; ebb.extra.way = 8; // she falls behind the carrier
    s.bank[dd.id] = salvo(s, dd);
    // Slower than the carrier: she keeps her distance, no threat; and with no cruiser or larger of its own engaged the
    // destroyer does not go in at the battleship ahead of it either.
    eca.extra.way = 9;
    s.aim(dd); s.startRuns();
    expect(s.runs.has(dd.id)).toBe(false);
    // Faster, but steaming away from her: no threat.
    eca.extra.way = 13; eca.face = 0;
    s.aim(dd); s.startRuns();
    expect(s.runs.has(dd.id)).toBe(false);
    // Faster and closing on her: the destroyer goes for the cruiser at once, astern of her or not.
    eca.face = Math.PI;
    s.aim(dd); s.startRuns();
    expect(s.runs.get(dd.id)?.target).toBe(eca.id);
    expect(s.runs.get(dd.id)?.guard).toBe(true);
  });

  it('a cover run launches as soon as its target is in torpedo reach; every other run presses in to its style\'s press range (Samar: Johnston; the DD, 11:25Z)', () => {
    const s = met(battle(11, { human: null }));
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, ebb = all(s, 'def', 'bb')[0];
    only(s, [[dd, 10000, 10000], [ebb, 10000, 10000 + SEA.torp.start - 500]]);
    ebb.face = Math.PI;
    const free = { kind: 'free' } as const;
    // A run ahead of her beam with the lines engaged (not a cover run), at 6,500 m: in at flank, no launch yet.
    s.bank[dd.id] = salvo(s, dd); s.beginRun(dd, ebb, 0, false);
    const before = s.marks['att:launchFar'] ?? 0;
    expect(s.runCourse(dd, free).speed).toBe(SEA.classes.dd.speed * SEA.speeds.flank);
    expect(s.runs.get(dd.id).phase).toBe('in');
    expect(s.torps.length).toBe(0);
    // The same run sent to turn her away from its carriers, she beyond torpedo reach: it closes at flank ...
    s.runs.delete(dd.id); delete dd.extra.run;
    s.beginRun(dd, ebb, 0, true);
    ebb.y = 10000 + SEA.torp.start + 500;
    expect(s.runCourse(dd, free).speed).toBe(SEA.classes.dd.speed * SEA.speeds.flank);
    expect(s.runs.get(dd.id).phase).toBe('in');
    expect(s.torps.length).toBe(0);
    // ... and fires the moment she is in reach.
    ebb.y = 10000 + SEA.torp.start - 500;
    s.runCourse(dd, free);
    expect(s.runs.get(dd.id).phase).toBe('away');
    expect(s.torps.length).toBe(1);
    expect(s.marks['att:launchFar'] ?? 0).toBe(before); // at its own launch range: not launched far
    // Fired from abeam, it turns away from her: measured from its bearing to her, that is no late turn.
    const late = s.marks['att:turnLate'] ?? 0;
    dd.face = Math.PI / 2 + 0.3; // it had been steaming across her bow, a little away from her
    s.runs.delete(dd.id); delete dd.extra.run; s.loaded[dd.id] = 0; s.bank[dd.id] = salvo(s, dd); s.torps.length = 0;
    s.beginRun(dd, ebb, 0, true); s.runCourse(dd, free);
    dd.face = s.runs.get(dd.id).away; s.runCourse(dd, free);
    s.endRun(dd, s.runs.get(dd.id), true);
    expect(s.marks['att:turnLate'] ?? 0).toBe(late);
  });

  it('a heavy ship the carrier steams toward is no threat unless she is heading at her; nor one making no more way than she does (R4: Samar; the DD, 09:48Z)', () => {
    const CVS: PlainFleet = { name: 'Escort Force', nat: 'USA', ships: { cv: 2, dd: 6 } };
    const s = new SeaSim(plainTicket({ att: [CVS], def: [BATTLE], seed: 11, human: null })) as any;
    s.contactAt = s.t;
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, cv = all(s, 'att', 'cv')[0], eca = all(s, 'def', 'ca')[0];
    only(s, [[dd, 10000, 9000], [cv, 10000, 2000], [eca, 10000, 14000]]);
    s.bank[dd.id] = salvo(s, dd);
    // The carrier steams north at the cruiser, which crosses ahead of her to the east-north-east, faster: the range
    // falls by the carrier's own way, but the cruiser is not coming at her.
    cv.face = 0; cv.extra.way = 10; eca.face = Math.PI / 2 - 0.2; eca.extra.way = 13; // edging away from her
    const rate = (cv.x - eca.x) * (Math.sin(cv.face) * cv.extra.way - Math.sin(eca.face) * eca.extra.way) + (cv.y - eca.y) * (Math.cos(cv.face) * cv.extra.way - Math.cos(eca.face) * eca.extra.way);
    expect(rate).toBeLessThan(0); // the range is falling
    expect(s.threats('att')).toEqual([]);
    // A cruiser making the carrier's own speed, straight at her: she cannot be overtaken.
    cv.face = Math.PI; cv.extra.way = 13; eca.face = Math.PI; eca.extra.way = 13;
    expect(s.threats('att')).toEqual([]);
    s.aim(dd); s.startRuns();
    expect(s.runs.has(dd.id)).toBe(false);
    // The carrier slowed to evade: now the cruiser overtakes her.
    cv.extra.way = SEA.classes.cv.speed * SEA.evade.speed;
    expect(s.threats('att')).toEqual([eca]);
  });

  it('its battleships outnumbered by the enemy\'s heavy ships in gun reach of them send it at the nearest of those ships, abaft her beam or not (R4\'s Samar cue as accepted, 06:48Z)', () => {
    const s = met(battle(11, { human: null }));
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, [ebb, eb2] = all(s, 'def', 'bb'), bb = all(s, 'att', 'bb')[0], ca = all(s, 'att', 'ca')[0];
    // Two enemy battleships in gun reach of its one; the destroyer abaft their beams, the second the nearer it.
    only(s, [[dd, 14000, 16000], [ebb, 10000, 14000], [eb2, 12000, 14000], [bb, 10000, 10000], [ca, 2000, 2000]]);
    ebb.face = Math.PI; eb2.face = Math.PI;
    s.bank[dd.id] = salvo(s, dd);
    expect(s.threats('att').map((e: SeaUnit) => e.id).sort()).toEqual([ebb.id, eb2.id].sort());
    s.startRuns();
    expect(s.runs.get(dd.id)?.target).toBe(eb2.id);
    expect(s.runs.get(dd.id)?.guard).toBe(true);
    // A cruiser of its own in reach of them too: two against two, no threat; abaft their beams it waits.
    s.runs.delete(dd.id); delete dd.extra.run;
    ca.x = 11000; ca.y = 10500;
    expect(s.threats('att')).toEqual([]);
    s.startRuns();
    expect(s.runs.has(dd.id)).toBe(false);
  });

  it('a division sent at a ship threatening its carriers runs on through its own side\'s break; the rest turn for home, and it follows once its run is over (R4: Samar; the DD, 08:10Z)', () => {
    const CVS: PlainFleet = { name: 'Escort Force', nat: 'USA', ships: { cv: 2, dd: 6 } };
    const s = new SeaSim(plainTicket({ att: [CVS], def: [BATTLE], seed: 11, human: null })) as any;
    s.contactAt = s.t;
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, dd2 = all(s, 'att', 'dd').find((u) => u !== dd)!, cv = all(s, 'att', 'cv')[0];
    const eca = all(s, 'def', 'ca')[0], ebb = all(s, 'def', 'bb')[0];
    // The destroyer 3,000 m ahead of the cruiser, beyond press; dd2 is the screen piece, given a run of its own here.
    only(s, [[dd, 10000, 9000], [dd2, 16000, 10000], [cv, 10000, 4000], [eca, 10000, 12000], [ebb, 16000, 14000]]);
    cv.face = Math.PI; cv.extra.way = 10; eca.face = Math.PI; eca.extra.way = 13; // closing on her, faster
    s.bank[dd.id] = salvo(s, dd);
    s.aim(dd); s.startRuns();
    expect(s.runs.get(dd.id)?.target).toBe(eca.id);
    expect(s.runs.get(dd.id)?.guard).toBe(true);
    s.runs.set(dd2.id, { ...s.runs.get(dd.id), target: ebb.id, div: 0, guard: false }); // a run of its own, not sent to guard
    const before = s.marks['att:guardOn'] ?? 0;
    s.breakSide('att', false);
    expect(dd.state).toBe('steady'); // on through the break
    expect(dd2.state).toBe('routing');
    expect(cv.state).toBe('routing');
    expect(s.marks['att:guardOn']).toBe(before + 1);
    // In torpedo reach of her, a cover run launches at once, beyond press (the design director, 11:25Z: Johnston).
    expect(s.runCourse(dd, { kind: 'free' })).not.toBeNull();
    expect(s.runs.get(dd.id)?.phase).toBe('away');
    // The fight goes on: it turns away, fires and holds its nerve as a steady piece does, and once its run is over it
    // turns for home with the rest.
    for (let i = 0; i < 1000 && s.runs.has(dd.id) && !s.over; i++) { s.step(); if (s.runs.has(dd.id)) expect(dd.state).toBe('steady'); }
    expect(s.runs.has(dd.id)).toBe(false);
    expect(s.runs.has(dd2.id)).toBe(false); // the other's run ended as it turned for home
    expect(dd.state).toBe('routing');
  });

  it('the pieces whose runs open together go as one division at one target and close in company (R4)', () => {
    const s = met(battle(11, { human: null }));
    s.contactAt = s.t;
    // (The enemy's destroyers gone, the screen's job is done: a screen piece runs as any other.)
    const [a, b] = all(s, 'att', 'dd'), ebb = all(s, 'def', 'bb')[0];
    const eb2 = all(s, 'def', 'bb')[1], bb = all(s, 'att', 'bb')[0];
    // Each has a battleship of its own in torpedo reach, ahead of her beam, and the other's out of it; their own
    // battleship has the enemy's in gun reach (the lines engaged).
    // (Its battleship has one enemy battleship in gun reach, not two: not outnumbered, so these are no cover runs.)
    only(s, [[a, 10000, 10000], [b, 16000, 9000], [ebb, 10000, 15000], [eb2, 16000, 15000], [bb, 11000, 18500]]);
    ebb.face = Math.PI; eb2.face = Math.PI;
    expect(s.threats('att')).toEqual([]);
    expect(s.runTarget(a)).toBe(ebb);
    expect(s.runTarget(b)).toBe(eb2);
    s.bank[a.id] = salvo(s, a); s.bank[b.id] = salvo(s, b);
    let before = s.marks['att:divisions'] ?? 0;
    // The target is the one the piece nearest its own would take (a, 5,000 m against 6,000), but b is 8,500 m from her,
    // out of torpedo reach: a goes alone, and b opens its own run in the next second, at the ship in its reach.
    s.startRuns();
    expect(s.runs.get(a.id)?.target).toBe(ebb.id);
    expect(s.runs.get(a.id)?.div).toBe(0);
    expect(s.runs.has(b.id)).toBe(false);
    s.t += 1; s.startRuns();
    expect(s.runs.get(b.id)?.target).toBe(eb2.id);
    expect(s.marks['att:divisions'] ?? 0).toBe(before);
    // b in torpedo reach of both: one division, at a's.
    s.runs.delete(a.id); delete a.extra.run; s.runs.delete(b.id); delete b.extra.run;
    b.x = 13000; b.y = 9500;
    before = s.marks['att:divisions'] ?? 0;
    s.startRuns();
    const ra = s.runs.get(a.id), rb = s.runs.get(b.id);
    expect(ra?.target).toBe(ebb.id);
    expect(rb?.target).toBe(ebb.id); // one target: b goes with a, not at its own
    expect(ra.div).toBeGreaterThan(0);
    expect(rb.div).toBe(ra.div);
    expect(s.marks['att:divisions']).toBe(before + 1);
    // In company: the leading piece, more than the fleet's station distance nearer the target, slows to half until
    // the rearmost comes up; the rearmost closes at flank.
    const free = { kind: 'free' } as const, flank = SEA.classes.dd.speed * SEA.speeds.flank, half = SEA.classes.dd.speed * SEA.speeds.slow;
    expect(s.runCourse(a, free).speed).toBe(half);
    expect(s.runCourse(b, free).speed).toBe(flank);
    b.x = 10000; b.y = a.y - SEA.ai.station + 100;
    expect(s.runCourse(a, free).speed).toBe(flank);
    // A mate the player holds, or one that has turned for home, is not waited for.
    b.y = 6000;
    expect(s.runCourse(a, free).speed).toBe(half);
    s.orders.set(b.id, { kind: 'hold' });
    expect(s.runCourse(a, free).speed).toBe(flank);
    s.orders.delete(b.id); b.state = 'routing';
    expect(s.runCourse(a, free).speed).toBe(flank);
    // One that drops out leaves the rest to go on at flank.
    b.state = 'wavering';
    expect(s.runCourse(b, free)).toBeNull();
    expect(s.runCourse(a, free).speed).toBe(flank);
  });

  it('a piece on a run that turns for home, or that the player holds or sends elsewhere, is off its run', () => {
    const s = met(battle(11, { human: null }));
    const dd = all(s, 'att', 'dd')[0], ebb = all(s, 'def', 'bb')[0], eca = all(s, 'def', 'ca')[0];
    only(s, [[dd, 10000, 8000], [ebb, 10000, 14000], [eca, 14000, 14000]]);
    s.beginRun(dd, ebb);
    s.orders.set(dd.id, { kind: 'hold' });
    s.steer(dd);
    expect(s.runs.has(dd.id)).toBe(false);
    expect(dd.extra.run).toBeUndefined();
    for (const o of [{ kind: 'move', x: 10000, y: 2000 }, { kind: 'leave' }, { kind: 'attack', target: eca.id }]) {
      s.beginRun(dd, ebb);
      s.orders.set(dd.id, o);
      s.steer(dd);
      expect(s.runs.has(dd.id)).toBe(false);
      expect(s.nextRun.get(dd.id)).toBe(s.t); // broken off on the run-in: free to run again as soon as it is fit
    }
    s.orders.delete(dd.id);
    s.beginRun(dd, ebb);
    dd.state = 'routing';
    s.steer(dd);
    expect(s.runs.has(dd.id)).toBe(false);
  });

  it('with spread, a piece runs at the ship fewest runs are closing on; a runner lost on its run-in closes on nothing', () => {
    const s = met(battle(11, { human: null }));
    const [a, b] = all(s, 'att', 'dd'), [ebb, eb2] = all(s, 'def', 'bb');
    only(s, [[a, 10000, 10000], [b, 11000, 10000], [ebb, 10000, 14000], [eb2, 13000, 14000]]);
    expect(SEA.styles.methodical!.spread).toBeGreaterThan(0);
    expect(s.runTarget(a)).toBe(ebb);
    s.beginRun(b, ebb);
    expect(s.runTarget(a)).toBe(eb2); // one run already closing on her
    while (b.extra.ships > 0) s.loseShip(b);
    expect(b.state).not.toBe('steady');
    expect(s.runs.has(b.id)).toBe(false); // its run ends as it goes (the 3D game reads extra.run)
    expect(b.extra.run).toBeUndefined();
    expect(s.runTarget(a)).toBe(ebb);
  });

  it("the screen's job is judged once the guns have met: against a fleet with no destroyers it stays on station until then", () => {
    const BIG: PlainFleet = { name: 'Battle Fleet', nat: 'GER', ships: { bb: 2, ca: 3 } };
    const s = battle(11, { human: null, def: [BIG] }) as any;
    const sc = all(s, 'att', 'dd').find((u) => s.screen.has(u.id))!;
    expect(sc.extra.screen).toBe(true);
    s.startRuns();
    expect(s.screen.has(sc.id)).toBe(true); // before contact: still the screen
    expect(sc.extra.screen).toBe(true);
    s.contactAt = s.t;
    s.startRuns();
    expect(s.screen.has(sc.id)).toBe(false); // the guns have met and no enemy destroyer is afloat: its job is done
    expect(sc.extra.screen).toBeUndefined();
  });

  it('wavering on the run-in breaks it off and keeps the bank for the next', () => {
    const s = met(battle(11, { human: null }));
    const dd = all(s, 'att', 'dd')[0], ebb = all(s, 'def', 'bb')[0];
    only(s, [[dd, 10000, 8000], [ebb, 10000, 14000]]);
    s.bank[dd.id] = 0.02;
    s.beginRun(dd, ebb);
    dd.state = 'wavering';
    expect(s.runCourse(dd, { kind: 'free' })).toBeNull();
    expect(s.runs.has(dd.id)).toBe(false);
    expect(s.bank[dd.id]).toBe(0.02);
  });

  it("the screen is the destroyer pieces nearest the big ships, the style's share of them rounded down and at least one; a fleet with no big ship keeps none", () => {
    const s = battle(11, { human: null }) as any;
    for (const side of ['att', 'def'] as const) {
      const dds = all(s, side, 'dd'), screened = dds.filter((u) => s.screen.has(u.id));
      expect(screened.length).toBe(Math.max(dds.length >= 2 ? 1 : 0, Math.floor(SEA.styles.methodical!.screen * dds.length)));
      for (const u of screened) expect(u.extra.screen).toBe(true);
    }
    const light = new SeaSim(plainTicket({ att: [{ name: '2nd Fleet', nat: 'ITA', ships: { dd: 16 } }], def: [BATTLE], seed: 3, human: null })) as any;
    expect(all(light, 'att', 'dd').some((u) => light.screen.has(u.id))).toBe(false);
  });

  it('a screen piece keeps station ahead of the ship it guards until its guns find a target, then fights where it is', () => {
    const s = met(battle(11, { human: null }));
    const scr = all(s, 'att', 'dd').find((u) => s.screen.has(u.id))!;
    const free = { kind: 'free' } as const;
    scr.extra.target = -1;
    expect(s.station(scr, free)).not.toBeNull();
    scr.extra.target = all(s, 'def', 'dd')[0].id;
    expect(s.station(scr, free)).toBeNull();
  });

  it('a screen piece fires first on an enemy destroyer making a run, before a nearer one', () => {
    const s = met(battle(11, { human: null }));
    const scr = all(s, 'att', 'dd').find((u) => s.screen.has(u.id))!, bb = all(s, 'att', 'bb')[0];
    const [near, runner] = all(s, 'def', 'dd');
    only(s, [[scr, 10000, 10000], [bb, 10000, 9000], [near, 10000, 11500], [runner, 12000, 12500]]);
    s.beginRun(runner, bb);
    s.aim(scr);
    expect(scr.extra.target).toBe(runner.id);
  });

  it('once no enemy destroyer is afloat the screen is done: its pieces leave it for good and run as any other destroyer (b)', () => {
    const s = met(battle(11, { human: null }));
    const scr = all(s, 'att', 'dd').find((u) => s.screen.has(u.id))!, ebb = all(s, 'def', 'bb')[0];
    const edds = all(s, 'def', 'dd');
    const bb = all(s, 'att', 'bb')[0]; // its own battleship engaged with hers
    only(s, [[scr, 10000, 10000], [ebb, 10000, 13000], [bb, 6000, 13000], ...edds.map((e, i): [SeaUnit, number, number] => [e, 2000 + i * 500, 18000])]);
    for (const e of edds) e.state = 'steady';
    s.bank[scr.id] = 0;
    s.startRuns();
    expect(s.screen.has(scr.id)).toBe(true); // an enemy destroyer still afloat, far off: the screen keeps to its job
    expect(s.runs.has(scr.id)).toBe(false);
    for (const e of edds) e.state = 'gone';
    ebb.face = Math.PI; // steaming toward it: the piece lies ahead of her beam
    s.bank[scr.id] = salvo(s, scr); // a full salvo banked since
    s.startRuns();
    expect(s.screen.has(scr.id)).toBe(false);
    expect(scr.extra.screen).toBeUndefined();
    expect(s.station(scr, { kind: 'free' })).toBeNull();
    expect(s.runs.get(scr.id)?.target).toBe(ebb.id);
  });

  it('the torpedoes have their sounds: the launch, the hit, and the run while a salvo is in the water', () => {
    expect([SEA_CUES['sea:torpedo'], SEA_CUES['sea:torpedoHit'], SEA_ZONE_LOOPS.torpedo]).toEqual(['torpedo_launch', 'torpedo_hit', 'torpedo_run_loop']);
  });
});

describe("carriers' strikes and the coasts' raids (ruling 239, item 4)", () => {
  const CVS: PlainFleet = { name: 'Striking Force', nat: 'JAP', ships: { cv: 2, ca: 2, dd: 8 } };
  const MIXED: PlainFleet = { name: 'Task Force', nat: 'USA', ships: { cv: 2, bb: 1, ca: 3, dd: 6 } };
  const fleets = (): any => new SeaSim(plainTicket({ att: [CVS], def: [MIXED], seed: 7, human: null }));
  const all = (s: any, side: 'att' | 'def', cls: string): SeaUnit[] => s.units.filter((u: SeaUnit) => u.side === side && u.extra.cls === cls);

  it("a carrier left to itself strikes the enemy's carriers first, then a battleship, then cruisers and destroyers", () => {
    const s = fleets();
    const cv = all(s, 'att', 'cv')[0];
    expect(s.strikeTarget(cv).extra.cls).toBe('cv');
    for (const e of all(s, 'def', 'cv')) e.state = 'gone';
    expect(s.strikeTarget(cv).extra.cls).toBe('bb');
    for (const e of all(s, 'def', 'bb')) e.state = 'gone';
    expect(s.strikeTarget(cv).extra.cls).toBe('ca');
    for (const e of all(s, 'def', 'ca')) e.state = 'gone';
    expect(s.strikeTarget(cv).extra.cls).toBe('dd');
  });

  it('with the style sharing fire out, a second carrier takes a second enemy carrier before doubling up, with no dice', () => {
    const s = fleets();
    const [a, b] = all(s, 'att', 'cv');
    const first = s.strikeTarget(a);
    s.struck.set(a.id, first.id);
    const second = s.strikeTarget(b);
    expect(second.extra.cls).toBe('cv');
    expect(second.id).not.toBe(first.id);
    expect(fleets().strikeTarget(a).id).toBe(first.id); // the same field, the same choice
  });

  it("the coasts' raids find each class by the campaign's aim times the ships afloat, one draw a raid", () => {
    const s = new SeaSim(plainTicket({ att: [MIXED], def: [CVS], air: { def: 30 }, seed: 5, human: null })) as any;
    const n = 4000;
    for (let i = 0; i < n; i++) { s.airLeft.def = 1e-9; s.nextAir.def = s.t; s.coastAir(); }
    const t = s.tally;
    let got = 0;
    for (const c of ['cv', 'bb', 'ca', 'dd']) {
      const hit = (t[`def:raid>${c}`] ?? 0) / n, exp = (t[`def:raidExp>${c}`] ?? 0) / n;
      got += t[`def:raid>${c}`] ?? 0;
      expect(Math.abs(hit - exp)).toBeLessThan(0.03);
    }
    expect(got).toBe(n);
    // Carriers 3, battleships 2, cruisers 1, destroyers 0.5, by the ships afloat in each piece.
    const ships = (c: SeaClass): number => all(s, 'att', c).reduce((m, u) => m + u.extra.ships, 0);
    const w = { cv: 3 * ships('cv'), bb: 2 * ships('bb'), ca: ships('ca'), dd: 0.5 * ships('dd') };
    expect((t['def:raidExp>cv'] ?? 0) / n).toBeCloseTo(w.cv / (w.cv + w.bb + w.ca + w.dd), 9);
  });
});

describe('fighters and guns over the fleet (ruling 239, step 5)', () => {
  const CVS: PlainFleet = { name: 'Striking Force', nat: 'JAP', ships: { cv: 2, ca: 2, dd: 8 } };
  const CVB: PlainFleet = { name: 'Task Force', nat: 'USA', ships: { cv: 2, ca: 3, dd: 6 } };
  const LINE: PlainFleet = { name: 'Battle Force', nat: 'USA', ships: { bb: 2, ca: 3, dd: 8 } };
  const all = (s: any, side: 'att' | 'def', cls: string): SeaUnit[] => s.units.filter((u: SeaUnit) => u.side === side && u.extra.cls === cls);
  const ready = (cv: SeaUnit): void => { const ab = cv.abilities!.find((a) => a.key === 'strike')!; ab.ready = 0; ab.winding = false; };
  const cover = SEA.styles.methodical!.cover;

  it("with enemy aircraft about a carrier holds its style's cover over the fleet, and what it has not spent flies with its next strike", () => {
    const s = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 7, human: null })) as any;
    const [cv] = all(s, 'att', 'cv'), foe = all(s, 'def', 'cv')[0];
    ready(cv);
    expect(s.strike(cv, foe)).toBe(true);
    const first = s.strikes[0].weight, held = s.cover[cv.id];
    expect(held / (first + held)).toBeCloseTo(cover, 9);
    ready(cv);
    s.strike(cv, foe);
    expect(s.strikes[1].weight).toBeCloseTo(first + held, 9); // the cover it never spent flies now: nothing is lost
    expect(s.cover[cv.id]).toBeCloseTo(held, 9);
    // Against a fleet that brings no aircraft it flies the whole of it.
    const line = new SeaSim(plainTicket({ att: [CVS], def: [LINE], seed: 7, human: null })) as any;
    const lcv = all(line, 'att', 'cv')[0];
    ready(lcv);
    line.strike(lcv, all(line, 'def', 'bb')[0]);
    expect(line.cover[lcv.id]).toBe(0);
    expect(line.strikes[0].weight).toBeCloseTo(first + held, 9);
  });

  it("a strike meets the pool by the campaign's law, then the guns near its target; what each denies is paid weight for weight", () => {
    const s = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 7, human: null })) as any;
    const cvs = all(s, 'def', 'cv'), at = all(s, 'def', 'ca')[0];
    s.cover[cvs[0].id] = 0.3; s.cover[cvs[1].id] = 0.1;
    for (const u of s.units) { u.x = 10000; u.y = 10000; }
    const w = 0.5, pool = 0.4;
    const a = Math.min(pool, w * 0.8 * Math.sqrt(pool / (pool + w)));
    const guns = s.units.filter((u: SeaUnit) => u.side === 'def' && u.extra.cls !== 'cv');
    const f = Math.min(0.12, guns.reduce((n: number, u: SeaUnit) => n + u.extra.ships * ({ bb: 3, ca: 1.5, dd: 0.5 } as Record<string, number>)[u.extra.cls] * 0.0015, 0));
    const landed = s.meet(at, w, 'att', true);
    expect(landed).toBeCloseTo(w - a - f * (w - a), 12);
    expect(s.cover[cvs[0].id] + s.cover[cvs[1].id]).toBeCloseTo(pool - a, 12);
    expect(s.cover[cvs[0].id] / s.cover[cvs[1].id]).toBeCloseTo(3, 9); // each carrier pays by its share
    const owed = guns.reduce((n: number, u: SeaUnit) => n + s.owe[u.id], 0);
    expect(owed).toBeCloseTo(f * (w - a), 12);
    expect(events(s).map((e: FieldEvent) => e.what)).toEqual(expect.arrayContaining(['sea:cap', 'sea:flak']));
    // The guns pay what they owe out of their next gunfire.
    const e = all(s, 'att', 'ca')[0];
    for (const u of guns) { u.extra.target = e.id; e.x = u.x; e.y = u.y + 2000; }
    for (let i = 0; i < 400 && guns.some((u: SeaUnit) => s.owe[u.id] > 0); i++) for (const u of guns) s.fire(u);
    const t = s.tally;
    expect(t['def:denied']).toBeCloseTo(t['def:paid'], 12);
    expect(t['def:strikesIn']).toBe(1);
    expect(t['def:strikesMet']).toBe(1);
  });

  it('a gun ship out of the fight hands what it owes to the nearest of its side still fighting', () => {
    const s = new SeaSim(plainTicket({ att: [CVS], def: [LINE], seed: 7, human: null })) as any;
    const [a, b] = all(s, 'def', 'ca');
    s.owe[a.id] = 0.02;
    a.state = 'gone';
    s.handOwes();
    expect(s.owe[a.id]).toBe(0);
    const near = s.units.filter((u: SeaUnit) => u.side === 'def' && u.extra.cls !== 'cv' && u.state !== 'gone').sort((x: SeaUnit, y: SeaUnit) => Math.hypot(x.x - a.x, x.y - a.y) - Math.hypot(y.x - a.x, y.y - a.y) || x.id - y.id)[0];
    expect(s.owe[near.id]).toBeCloseTo(0.02, 12);
    void b;
  });

  it("a gun ship beyond the strike's radius of its target turns no aircraft away and owes nothing", () => {
    const s = new SeaSim(plainTicket({ att: [CVS], def: [LINE], seed: 7, human: null })) as any;
    const at = all(s, 'def', 'bb')[0], far = all(s, 'def', 'ca')[0];
    for (const u of s.units) { u.x = 10000; u.y = 10000; }
    far.x = 10000 + SEA.strike.radius + 50;
    s.meet(at, 0.5, 'att', true);
    expect(s.owe[far.id]).toBe(0);
    expect(s.owe[at.id]).toBeGreaterThan(0);
  });

  it('when the action ends, what the guns still owe lands on their side and what a carrier still holds flies: nothing is lost', () => {
    const s = new SeaSim(plainTicket({ att: [CVS], def: [LINE], air: { def: 10 }, seed: 7, human: null })) as any;
    const [cv] = all(s, 'att', 'cv'), gun = all(s, 'att', 'ca')[0];
    s.cover[cv.id] = 0.3;
    s.owe[gun.id] = 0.05;
    s.mark('att:denied', 0.05);
    const str = gun.str, target = s.strikeTarget(cv), left = target.str;
    s.finish('att', 'time');
    const t = s.tally;
    expect(s.cover[cv.id]).toBe(0);
    expect(s.owe[gun.id]).toBe(0);
    expect(t['att:coverFlown']).toBeCloseTo(0.3, 12);
    expect(t['att:settled']).toBeCloseTo(0.05, 12);
    expect(t['att:denied']).toBeCloseTo(t['att:paid'], 12);
    expect(t['att:owedLeft'] ?? 0).toBe(0);
    expect(t['att:coverLeft'] ?? 0).toBe(0);
    expect(gun.str).toBeLessThan(str);
    expect(target.str).toBeLessThan(left);
  });

  it("a strike the fleet's fighters turn back a third or more of is told so, in place of the find line (the UI director)", () => {
    const s = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 7, human: null })) as any;
    const cvs = all(s, 'def', 'cv'), at = all(s, 'def', 'cv')[0];
    for (const u of s.units) { u.x = 10000; u.y = 10000; }
    const told = (pool: number): string => {
      s.cover[cvs[0].id] = pool * 0.75; s.cover[cvs[1].id] = pool * 0.25;
      s.strikes.push({ at: s.t, side: 'att', target: at.id, weight: 0.5, from: 'Striking Force', count: 36 });
      s.landStrikes();
      return events(s).filter((e: FieldEvent) => e.what === 'sea:strike').pop()!.text;
    };
    // pool = w: the fighters turn back .8 * sqrt(1/2) = .57 of it, "half".
    expect(told(0.5)).toBe('Task Force\'s fighters turn back half of the 36 bombers from Striking Force, and the rest find a carrier.');
    // A thin pool turns back under a third: the find line stands.
    expect(told(0.01)).toBe('36 bombers from Striking Force find a carrier of Task Force.');
  });

  it("'carrierBattle' stays 'field': carriers against carriers with no battleship are still fought out, and named as such", () => {
    expect(SEA.carrierBattle).toBe('field');
    const t = plainTicket({ att: [CVS], def: [CVB], seed: 7, human: null });
    expect(carriersOnly(t)).toBe(true);
    expect(fieldFights(t)).toBe(true);
    expect(carriersOnly(plainTicket({ att: [CVS], def: [LINE], seed: 7, human: null }))).toBe(false);
  });

  it('the fighters and the guns over the fleet have their sounds', () => {
    expect([SEA_CUES['sea:cap'], SEA_CUES['sea:flak']]).toEqual(['fighter_pass', 'flak_light_fire']);
  });
});

describe("the battle line (the ship plan's R1, ruling 239's order at 23:20Z)", () => {
  const all = (s: any, side: 'att' | 'def', cls: string): SeaUnit[] => s.units.filter((u: SeaUnit) => u.side === side && u.extra.cls === cls);
  const only = (s: any, keep: [SeaUnit, number, number, number?][]): void => {
    for (const u of s.units as SeaUnit[]) if (!keep.some(([k]) => k === u)) u.state = 'gone';
    for (const [u, x, y, face] of keep) { u.x = x; u.y = y; if (face !== undefined) u.face = face; }
  };
  const square = (a: number, b: number): number => Math.abs(Math.abs(Math.atan2(Math.sin(a - b), Math.cos(a - b))) - Math.PI / 2);

  it('once the guns meet, the battleships and cruisers turn together onto one course square to the enemy, at the slowest one\'s speed', () => {
    const s = battle(5, { human: null }) as any;
    while (s.contactTime === null && !s.over) s.step();
    for (let i = 0; i < 600 && !s.over; i++) s.step();
    for (const side of ['att', 'def'] as const) {
      const L = s.lines[side], members: SeaUnit[] = s.linePlans[side].members;
      expect(L, side).not.toBeNull();
      expect(members.length).toBe(s.units.filter((u: SeaUnit) => u.side === side && s.inLine(u)).length);
      expect(members.every((u) => u.extra.cls === 'bb' || u.extra.cls === 'ca')).toBe(true);
      for (const u of members) expect(Math.abs(Math.atan2(Math.sin(u.face - L.course), Math.cos(u.face - L.course))), u.name).toBeLessThan(0.15);
      const slowest = Math.min(...members.map((u) => SEA.classes[u.extra.cls as 'bb' | 'ca'].speed)) * SEA.speeds.cruise;
      expect(L.speed).toBe(slowest);
      // Square to the enemy's gun ships' bearing, give or take the bend that holds the range.
      const [mx, my] = s.middle(members), [ex, ey] = s.middle(s.lineFoes(side));
      expect(square(L.course, Math.atan2(ex - mx, ey - my))).toBeLessThanOrEqual(SEA.line.bend + 1e-9);
      // Its place in the line is drawn: the van first.
      expect(members.map((u) => u.extra.lineAt)).toEqual(members.map((_, i) => i));
    }
    // Destroyers keep their own jobs.
    expect(s.units.some((u: SeaUnit) => u.extra.cls === 'dd' && u.extra.lineAt !== undefined)).toBe(false);
  });

  it("its hand runs with the enemy line's way across its bearing; with it coming straight on, to starboard, as the enemy's captains turn", () => {
    const hand = (foeFace: number, foeWay: number, side: 'att' | 'def'): number => {
      const s = battle(5, { human: null }) as any;
      const [bb] = all(s, side, 'bb'), [foe] = all(s, side === 'att' ? 'def' : 'att', 'bb');
      only(s, [[bb, 10000, side === 'att' ? 8000 : 12000], [foe, 10000, side === 'att' ? 12000 : 8000, foeFace]]);
      foe.extra.way = foeWay;
      s.contactAt = 0;
      s.planLines();
      return s.lines[side].hand;
    };
    expect(hand(Math.PI / 2, 10, 'att')).toBe(1); // the enemy runs east: so does the line
    expect(hand(-Math.PI / 2, 10, 'att')).toBe(-1); // west
    expect(hand(Math.PI, 10, 'att')).toBe(1); // straight on: to starboard, square to the bearing
    expect(hand(0, 10, 'def')).toBe(1); // the same for the defender: the field turned half round (b402)
  });

  it('it bends toward the enemy beyond its best range, square at best range, and away within it', () => {
    const s = battle(5, { human: null }) as any;
    const [bb] = all(s, 'att', 'bb'), [foe] = all(s, 'def', 'bb'), C = SEA.classes.bb;
    const course = (d: number): number => { bb.x = 10000; bb.y = 8000; foe.x = 10000; foe.y = 8000 + d; return s.lineCourse([bb], [foe], 1); };
    expect(course(C.reach)).toBeCloseTo(Math.PI / 2 - SEA.line.bend, 9);
    expect(course(C.best)).toBeCloseTo(Math.PI / 2, 9);
    expect(course(C.best - (C.reach - C.best))).toBeCloseTo(Math.PI / 2 + SEA.line.bend, 9);
  });

  it('a piece the player sends at a target, a point or a speed of its own leaves the line; one fighting at will or under a fleet order stays', () => {
    const s = battle(5, { human: 'att' }) as any;
    const [b1, b2] = all(s, 'att', 'bb'), [ca] = all(s, 'att', 'ca'), dd = all(s, 'att', 'dd')[0], foe = all(s, 'def', 'bb')[0];
    expect([s.inLine(b1), s.inLine(ca), s.inLine(dd)]).toEqual([true, true, false]);
    s.command({ do: 'attack', units: [b1.id, b2.id], target: foe.id }, 'att');
    expect([s.inLine(b1), s.inLine(b2)]).toEqual([true, true]);
    s.command({ do: 'attack', units: [b1.id], target: foe.id }, 'att');
    expect(s.inLine(b1)).toBe(false);
    s.command({ do: 'sea:speed', units: [ca.id], speed: 'flank' }, 'att');
    expect(s.inLine(ca)).toBe(false);
    b2.state = 'routing';
    expect(s.inLine(b2)).toBe(false);
  });

  it("it turns together off the field's edge, and away together when its T is crossed", () => {
    const s = battle(5, { human: null }) as any;
    const [a1, a2] = all(s, 'att', 'bb'), [d1, d2] = all(s, 'def', 'bb');
    // Steaming east near the east edge: it comes round onto the other hand, the rear now the van.
    only(s, [[a1, SEA.field.size - 2500, 9000, Math.PI / 2], [a2, SEA.field.size - 3000, 9000, Math.PI / 2], [d1, SEA.field.size - 2500, 12000], [d2, SEA.field.size - 3000, 12000]]);
    s.contactAt = 0;
    s.lines.att = { order: [], hand: 1, course: Math.PI / 2, speed: 11, turned: -1e9 };
    s.planLines();
    expect(s.lines.att.hand).toBe(-1);
    expect(s.linePlans.att.members[0]).toBe(a2);
    // Its van heading straight at an enemy line steaming square across its bow: it turns away together onto the
    // hand that runs with the enemy, and holds it 'settle' seconds.
    const t = battle(5, { human: null }) as any;
    const [b1, b2] = all(t, 'att', 'bb'), [e1, e2] = all(t, 'def', 'bb');
    only(t, [[b1, 10000, 8000, 0], [b2, 10000, 7600, 0], [e1, 10500, 11000, Math.PI / 2], [e2, 10000, 11000, Math.PI / 2]]);
    t.contactAt = 0;
    t.lines.att = { order: [], hand: -1, course: 0, speed: 11, turned: -1e9 };
    t.lines.def = { order: [], hand: -1, course: Math.PI / 2, speed: 11, turned: -1e9 };
    t.linePlans.att = { t: t.t, members: [b1, b2] };
    t.linePlans.def = { t: t.t, members: [e1, e2] };
    expect(t.crossed('att')).toBe(1);
    expect(t.crossed('def')).toBe(0);
    t.turnTogether('att', 1, 'crossed');
    expect(t.crossed('att')).toBe(0); // settling
    expect(t.view().events.filter((e: FieldEvent) => e.what === 'sea:line').map((e: FieldEvent) => e.extra!.why)).toEqual(['crossed']);
  });

  it('there is no line against destroyers alone, nor in a carrier group without battleships (a ring is not a line: R3)', () => {
    const light = battle(5, { human: null, def: [{ name: '2nd Fleet', nat: 'ITA', ships: { dd: 12 } }] }) as any;
    light.contactAt = 0;
    light.planLines();
    expect([light.lines.att, light.lines.def]).toEqual([null, null]);
    const cv = battle(5, { human: null, att: [{ name: 'Striking Force', nat: 'JAP', ships: { cv: 2, ca: 4, dd: 8 } }] }) as any;
    cv.contactAt = 0;
    cv.planLines();
    expect(cv.lines.att).toBeNull();
    expect(cv.lines.def).not.toBeNull();
    // A carrier group with a battleship forms its line of battleships and cruisers; its carriers stay out of it.
    const mixed = battle(5, { human: null, att: [{ name: 'Task Force', nat: 'USA', ships: { bb: 1, cv: 1, ca: 2, dd: 6 } }] }) as any;
    mixed.contactAt = 0;
    mixed.planLines();
    expect(mixed.linePlans.att.members.map((u: SeaUnit) => u.extra.cls).sort()).toEqual(['bb', 'ca', 'ca']);
  });

  it('breaking off or against an enemy with no gun ship left, there is no line: the chase runs as before', () => {
    const s = battle(5, { human: null }) as any;
    while (s.contactTime === null && !s.over) s.step();
    s.step();
    expect(s.linePlans.att.members.length).toBeGreaterThan(0);
    s.breakSide('def', false);
    s.step();
    expect([s.linePlans.att.members.length, s.linePlans.def.members.length]).toEqual([0, 0]);
  });
});

describe("carriers' station and the AA ring (the ship plan's R3; the design director, 14:36Z)", () => {
  const CVS: PlainFleet = { name: 'Striking Force', nat: 'JAP', ships: { cv: 2, ca: 2, dd: 8 } };
  const CVB: PlainFleet = { name: 'Task Force', nat: 'USA', ships: { cv: 2, ca: 3, dd: 6 } };
  const LINE: PlainFleet = { name: 'Battle Force', nat: 'USA', ships: { bb: 2, ca: 3, dd: 8 } };
  const MIX: PlainFleet = { name: 'Mixed', nat: 'USA', ships: { bb: 2, cv: 2, ca: 3, dd: 8 } };
  const all = (s: any, side: 'att' | 'def', cls: string): SeaUnit[] => s.units.filter((u: SeaUnit) => u.side === side && u.extra.cls === cls);
  const gap = (a: SeaUnit, b: SeaUnit): number => Math.hypot(a.x - b.x, a.y - b.y);
  const only = (s: any, keep: [SeaUnit, number, number][]): void => {
    for (const u of s.units as SeaUnit[]) if (!keep.some(([k]) => k === u)) u.state = 'gone';
    for (const [u, x, y] of keep) { u.x = x; u.y = y; }
  };

  it("until the guns meet, where the enemy brings aircraft, a carrier group keeps its whole escort round its carriers at the screen's distance, shared in turn and spaced from the enemy's bearing; at contact it is let go", () => {
    const s = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 5, human: null })) as any;
    run(s, 240);
    expect(s.contactAt).toBeNull();
    for (const side of ['att', 'def'] as const) {
      const escort = s.units.filter((u: SeaUnit) => u.side === side && (u.extra.cls === 'ca' || u.extra.cls === 'dd')).map((u: SeaUnit) => u.id);
      expect([...s.rings[side].keys()].sort((a: number, b: number) => a - b), side).toEqual(escort);
      // Shared in turn by id: the pieces in id order go to the carriers in id order, one each, round again.
      const cvs = all(s, side, 'cv').map((c) => c.id).sort((a, b) => a - b);
      expect(escort.map((id: number) => s.rings[side].get(id).cv), side).toEqual(escort.map((_: number, n: number) => cvs[n % cvs.length]));
      const ds: number[] = [], [ex, ey] = s.centre(side === 'att' ? 'def' : 'att');
      for (const c of cvs) {
        const mine = [...s.rings[side]].filter(([, r]: [number, { cv: number }]) => r.cv === c);
        // Each its own place, 0 to k-1, the first toward the enemy's middle: the places spaced evenly round from its bearing.
        expect(mine.map(([, r]: [number, { i: number }]) => r.i).sort(), `${side} ${c}`).toEqual(mine.map((_: unknown, i: number) => i));
        expect(mine.every(([, r]: [number, { k: number }]) => r.k === mine.length)).toBe(true);
        const cv = s.byId.get(c), first = s.byId.get(mine.find(([, r]: [number, { i: number }]) => r.i === 0)![0]);
        expect(Math.hypot(first.x - ex, first.y - ey), `${side} ${c}`).toBeLessThan(Math.hypot(cv.x - ex, cv.y - ey) - SEA.torp.station / 2);
        for (const [id] of mine) {
          const d = gap(s.byId.get(id), cv);
          ds.push(d);
          // Round its carrier, inside the strike's radius, so its guns count against aircraft and it shares what lands.
          expect(d, `${side} piece ${id}`).toBeGreaterThan(SEA.torp.station / 2);
          expect(d, `${side} piece ${id}`).toBeLessThan(SEA.strike.radius);
        }
        // A carrier whose escort is all in its ring has no gun ship to take station astern of: its standoff as before.
        expect(s.carrierHelm(cv), `${side} ${c}`).toBeNull();
      }
      // The ring's places are torp.station out (a piece whose place lies astern of a carrier under way lags inside it).
      expect(Math.abs(ds.reduce((a, b) => a + b, 0) / ds.length - SEA.torp.station), side).toBeLessThan(150);
    }
    while (s.contactAt === null && !s.over) s.step();
    s.step();
    // From contact the escort fights as any other ship does: its weight stays in the fight (whole strength).
    expect([s.rings.att.size, s.rings.def.size]).toEqual([0, 0]);
  });

  it("no ring where the enemy brings no aircraft, nor in a fleet with a battleship of its own, nor once a side breaks off or the enemy has no gun ship left; the coasts' aircraft count", () => {
    const line = new SeaSim(plainTicket({ att: [CVS], def: [LINE], seed: 5, human: null })) as any;
    line.step();
    expect(line.rings.att.size).toBe(0); // a battle line with no aircraft: the escort goes for it as before
    const coast = new SeaSim(plainTicket({ att: [CVS], def: [LINE], seed: 5, human: null, air: { def: 10 } })) as any;
    coast.step();
    expect(coast.rings.att.size).toBe(all(coast, 'att', 'ca').length + all(coast, 'att', 'dd').length);
    const mixed = new SeaSim(plainTicket({ att: [HOME], def: [CVB], seed: 5, human: null })) as any;
    mixed.step();
    expect(mixed.rings.att.size).toBe(0); // its battleships keep their line, the carrier astern of it
    expect(mixed.rings.def.size).toBeGreaterThan(0);
    const broke = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 5, human: null })) as any;
    run(broke, 30);
    expect(broke.rings.att.size).toBeGreaterThan(0);
    broke.breakSide('def', true);
    broke.step();
    expect([broke.rings.att.size, broke.rings.def.size]).toEqual([0, 0]); // the chase runs as before
    const bare = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 5, human: null })) as any;
    run(bare, 30);
    for (const u of bare.units as SeaUnit[]) if (u.side === 'def' && u.extra.cls !== 'cv') u.state = 'gone';
    bare.step();
    expect(bare.rings.att.size).toBe(0); // nothing for it to stand off: the screen runs the carriers down, as before
  });

  it('the ring is let go for good once a carrier sights a battle line, and the screen meets it: the guns meet as they did', () => {
    const s = new SeaSim(plainTicket({ att: [CVS], def: [LINE], seed: 5, human: null, air: { def: 10 } })) as any;
    let formed = false, letGo = -1;
    while (!s.over && s.contactAt === null) {
      s.step();
      if (s.rings.att.size) formed = true;
      if (letGo < 0 && formed && !s.rings.att.size) {
        letGo = s.t;
        // Let go as the carriers turn away from an enemy battleship within sight (b389), and not before.
        expect(all(s, 'att', 'cv').some((c) => s.evasion(c) !== null)).toBe(true);
      }
      if (letGo >= 0) expect(s.rings.att.size).toBe(0);
    }
    expect(formed).toBe(true);
    expect(letGo).toBeGreaterThan(0);
    // The ring that ran with its carriers kept the guns from meeting all action (b402's screen stays and fights).
    expect(s.contactAt).not.toBeNull();
    expect(s.contactAt).toBeLessThan(letGo + 120);
  });

  it("a ring's carrier sunk, sent on a course, held or withdrawn hands its pieces to the others", () => {
    const s = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 5, human: 'att' })) as any;
    run(s, 30);
    const [c1, c2] = all(s, 'att', 'cv'), n = s.rings.att.size;
    c1.state = 'gone';
    s.step();
    expect(s.rings.att.size).toBe(n);
    expect([...s.rings.att.values()].every((r: { cv: number; k: number }) => r.cv === c2.id && r.k === n)).toBe(true);
    for (const order of [{ do: 'move', units: [c2.id], x: c2.x, y: c2.y - 3000 }, { do: 'hold', units: [c2.id] }, { do: 'withdraw', units: [c2.id] }]) {
      const t = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 5, human: 'att' })) as any;
      run(t, 30);
      const [d1, d2] = all(t, 'att', 'cv');
      expect(t.command({ ...order, units: [d2.id] }, 'att'), order.do).toBe(true);
      t.step();
      expect([...t.rings.att.values()].every((r: { cv: number }) => r.cv === d1.id), order.do).toBe(true);
      expect(t.rings.att.size, order.do).toBe(n);
    }
  });

  it("an order to attack one ship, a speed of the player's own, or a course, hold or withdrawal takes a piece out of the ring; the plain order does not (b402, mark 4)", () => {
    const s = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 5, human: 'att' })) as any;
    const [ca1, ca2] = all(s, 'att', 'ca'), [d1, d2] = all(s, 'att', 'dd'), foe = piece(s, 'def', 'cv');
    expect(s.command({ do: 'attack', units: [ca1.id, ca2.id], target: foe.id }, 'att')).toBe(true);
    expect(s.command({ do: 'attack', units: [d1.id], target: foe.id }, 'att')).toBe(true);
    expect(s.command({ do: 'sea:speed', units: [d2.id], speed: 'flank' } as never, 'att')).toBe(true);
    s.step();
    expect([ca1, ca2, d1, d2].map((u) => s.rings.att.has(u.id))).toEqual([true, true, false, false]);
    for (const order of [{ do: 'move', x: 10000, y: 5000 }, { do: 'hold' }, { do: 'withdraw' }]) {
      const t = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 5, human: 'def' })) as any;
      const e = all(t, 'def', 'dd')[0];
      expect(t.command({ ...order, units: [e.id] }, 'def'), order.do).toBe(true);
      t.step();
      expect(t.rings.def.has(e.id), order.do).toBe(false);
      expect(t.rings.def.size, order.do).toBe(all(t, 'def', 'ca').length + all(t, 'def', 'dd').length - 1);
    }
  });

  it('while the enemy brings aircraft a carrier keeps station astern of its own gun ships, away from the enemy and outside its standoff, all action beside a battle line; a carrier group lets it go with its ring when the guns meet; with no aircraft about, its standoff as before', () => {
    for (const [att, def, side] of [[HOME, CVB, 'att'], [CVB, HOME, 'def']] as const) {
      const s = new SeaSim(plainTicket({ att: [att], def: [def], seed: 5, human: null })) as any;
      while (s.contactAt === null && !s.over) s.step();
      expect(s.contactAt, side).not.toBeNull();
      const cv = piece(s, side, 'cv');
      // The guns have met: beside its battleships it keeps its station through the gun fight, sampled each second for
      // two minutes after contact (on this seed neither side breaks before 440 s).
      let samples = 0;
      for (let k = 0; k < 120 && !s.over && s.broken === null; k++) {
        run(s, 1);
        const guns = s.units.filter((u: SeaUnit) => u.side === side && u.extra.cls !== 'cv' && u.state !== 'gone');
        const [gx, gy] = s.middle(guns), [ex, ey] = s.centre(side === 'att' ? 'def' : 'att');
        const along = ((cv.x - gx) * (ex - gx) + (cv.y - gy) * (ey - gy)) / Math.hypot(ex - gx, ey - gy);
        expect(along, `${side} ${s.t}`).toBeLessThan(-0.9 * SEA.ai.keep); // astern of them, on the far side from the enemy
        expect(Math.abs(Math.hypot(cv.x - gx, cv.y - gy) - SEA.ai.keep), `${side} ${s.t}`).toBeLessThan(500);
        expect(s.onStation.has(cv.id), `${side} ${s.t}`).toBe(true); // as it steered this tick (read before the helm is asked)
        expect(s.carrierHelm(cv), `${side} ${s.t}`).not.toBeNull();
        samples++;
      }
      expect(samples, side).toBe(120);
      // With no battleship of its own afloat and fighting, the line it kept astern of is gone: its standoff as before.
      for (const state of ['gone', 'routing'] as const) {
        const keep = all(s, side, 'bb').map((b) => [b, b.state] as const);
        for (const [b] of keep) b.state = state;
        expect(s.carrierHelm(cv), `${side} ${state}`).toBeNull();
        for (const [b, was] of keep) b.state = was;
      }
      expect(s.carrierHelm(cv), side).not.toBeNull();
    }
    // A carrier group: its escort, let go from the ring at contact, fights at will at cruising speed (gun ships it could
    // stand astern of), but the station goes with the ring.
    const g = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 5, human: null })) as any;
    while (g.contactAt === null && !g.over) g.step();
    run(g, 30);
    expect(g.over).toBeFalsy();
    for (const side of ['att', 'def'] as const) {
      expect(g.rings[side].size, side).toBe(0);
      expect(g.units.some((u: SeaUnit) => u.side === side && u.extra.cls !== 'cv' && u.state === 'steady' && u.extra.speed === 'cruise' && !g.runs.has(u.id)), side).toBe(true);
      for (const c of all(g, side, 'cv')) if (c.state !== 'gone') expect(g.carrierHelm(c), `${side} ${c.id}`).toBeNull();
    }
    const t = new SeaSim(plainTicket({ att: [HOME], def: [BATTLE], seed: 5, human: null })) as any;
    t.step();
    expect(t.carrierHelm(piece(t, 'att', 'cv'))).toBeNull();
    // A carrier the player sends at a speed of their own goes as told.
    const u = new SeaSim(plainTicket({ att: [HOME], def: [CVB], seed: 5, human: 'att' })) as any;
    const ucv = piece(u, 'att', 'cv');
    expect(u.command({ do: 'sea:speed', units: [ucv.id], speed: 'slow' } as never, 'att')).toBe(true);
    run(u, 60);
    expect(u.carrierHelm(ucv)).toBeNull();
    expect(ucv.extra.way).toBeCloseTo(SEA.classes.cv.speed * SEA.speeds.slow, 6);
  });

  it("two carriers keep station abreast, their navy's spacing apart, never on top of each other (the design director, 21:00Z: the record's one figure for carriers in one group, whatever they stand behind)", () => {
    const JMIX: PlainFleet = { name: 'Mobile Force', nat: 'JAP', ships: { bb: 2, cv: 2, ca: 3, dd: 8 } };
    const cases = [[MIX, CVB, 'att', undefined], [CVB, MIX, 'def', undefined], [JMIX, CVB, 'att', undefined], [MIX, CVB, 'att', ['taskforces']]] as const;
    for (const [att, def, side, held] of cases) {
      const s = new SeaSim(plainTicket({ att: [att], def: [def], seed: 5, human: null, research: held && { [side]: [...held] } })) as any;
      const R = s.carrierWay(side).spacing, label = `${side} ${s.ticket[side].nat} ${held ?? ''} ${R}`;
      expect(R, label).toBe(SEA.carriers.path[s.ticket[side].nat][held ? 1 : 0].spacing);
      let least = Infinity, chase = Infinity;
      for (let i = 0; i < 20000 && !s.over; i++) {
        s.step();
        const [a, b] = all(s, side, 'cv');
        if (s.t > 300 && a.state !== 'gone' && b.state !== 'gone' && s.onStation.has(a.id) && s.onStation.has(b.id)) {
          if (s.broken === null) least = Math.min(least, gap(a, b)); else chase = Math.min(chase, gap(a, b));
        }
      }
      // Through the gun fight (contact is at 295 s) they hold their places, the spacing apart. In the chase after the break
      // they run at flank behind places they cannot reach, each lagging its own by a different distance, so the gap closes;
      // they still never come near each other.
      expect(least, label).toBeGreaterThan(0.75 * R);
      expect(least, label).toBeLessThan(1.1 * R);
      expect(chase, label).toBeGreaterThan(R / 2);
      expect(chase, label).toBeLessThan(Infinity);
    }
  });

  it("a navy's carriers keep company by its path in the data, a step for the research it holds (Carrier task forces), never by a date: each for herself or on a guide, at the navy's spacing", () => {
    const P = SEA.carriers.path;
    const way = (nat: string, research?: string[]): unknown => {
      const s = new SeaSim(plainTicket({ att: [{ ...CVS, nat }], def: [CVB], seed: 5, human: null, research: research && { att: research } })) as any;
      return s.carrierWay('att');
    };
    // The table as ruled (the design director, 20:56Z; the historian's carrier-spacing-record.md).
    expect(way('JAP')).toEqual({ form: 'herself', spacing: 7000 });
    expect(way('JAP', ['taskforces'])).toEqual({ form: 'herself', spacing: 2000 });
    expect(way('USA')).toEqual({ form: 'herself', spacing: 2740 });
    expect(way('USA', ['taskforces'])).toEqual({ form: 'guide', spacing: 2300 });
    expect(way('UK')).toEqual(P.UK[0]);
    expect(way('UK', ['taskforces'])).toEqual(P.UK[1]);
    // A navy not named takes '_', the American path: the guide comes with the research (the sandbox's navies).
    expect(P._).toEqual(P.USA);
    for (const nat of ['GER', 'ITA']) {
      expect(way(nat), nat).toEqual(P._[0]);
      expect(way(nat, ['taskforces']), nat).toEqual(P._[1]);
    }
    // Research that is not the carriers' leaves a navy where it stands; each side stands on its own research.
    expect(way('USA', ['radar', 'carriers'])).toEqual(P.USA[0]);
    const s = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 5, human: null, research: { def: ['taskforces'] } })) as any;
    expect(s.carrierWay('att')).toEqual(P.JAP[0]);
    expect(s.carrierWay('def')).toEqual(P.USA[1]);
    // Every step is a way and a spacing and nothing else, and every spacing is wider than the screen's 1,000 m, which went
    // (the record has none so close: the design director, 20:50Z).
    expect(SEA.carriers.grow).toEqual(['taskforces']);
    for (const [nat, path] of Object.entries(P)) {
      expect(path.length, nat).toBe(SEA.carriers.grow.length + 1);
      for (const st of path) {
        expect(Object.keys(st).sort(), nat).toEqual(['form', 'spacing']);
        expect(['herself', 'guide'], nat).toContain(st.form);
        expect(st.spacing, nat).toBeGreaterThan(SEA.torp.station);
      }
    }
  });

  it('with Carrier task forces an American carrier group steams on its guide wherever its carriers keep no station, before the guns meet as after: the first by id keeps her standoff, the other her place abreast, the navy\'s spacing off; a carrier under air attack leaves the formation', () => {
    const R = SEA.carriers.path.USA[1].spacing;
    const s = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 5, human: null, research: { def: ['taskforces'] } })) as any;
    const [guide, other] = all(s, 'def', 'cv').sort((a, b) => a.id - b.id), jap = all(s, 'att', 'cv');
    let before = 0, after = 0, out = 0, least = Infinity, first: number | null = null, closest = Infinity;
    while (!s.over) {
      const p = s.guidePlace(other), m0 = s.marks['def:cvGuide:s'] ?? 0, struck = s.underAir(guide) || s.underAir(other);
      expect(s.guideHelm(guide), `${s.t}`).toBeNull(); // the guide keeps her standoff (engage)
      for (const c of jap) expect(s.guideHelm(c), `${s.t}`).toBeNull(); // Japan's way is each for herself
      expect(!!p, `${s.t}`).toBe(!struck); // out of formation exactly while an enemy strike is aloft at one of them
      const wasContact = s.contactAt !== null;
      s.step();
      if (s.broken !== null || guide.state === 'gone' || other.state === 'gone') break;
      // Steered on her guide each tick both keep formation (one follower: DT a tick on the mark), and never else.
      expect((s.marks['def:cvGuide:s'] ?? 0) - m0, `${s.t}`).toBeCloseTo(p ? DT : 0, 9);
      if (!p) { out++; continue; }
      if (!wasContact) { before++; continue; }
      after++;
      least = Math.min(least, gap(guide, other));
      const d = Math.hypot(other.x - p.sx, other.y - p.sy);
      if (first === null) first = d;
      closest = Math.min(closest, d);
    }
    expect(before).toBeGreaterThan(0);
    expect(after).toBeGreaterThan(0);
    expect(out).toBeGreaterThan(0);
    // Through the gun fight they never come near each other (each to her own standoff, R3 let them pass 132-214 m apart),
    // and the follower works up to her place (here from 1,446 m off to under 250 m).
    expect(least).toBeGreaterThan(0.75 * R);
    expect(first).not.toBeNull();
    expect(closest).toBeLessThan(first! / 2);
    // Without the research the same carriers steam each for herself: no guide.
    const t = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 5, human: null })) as any;
    while (!t.over) t.step();
    expect(t.marks['def:cvGuide:s'] ?? 0).toBe(0);
    expect(s.marks['att:cvGuide:s'] ?? 0).toBe(0);
  });

  it('the guide sunk, breaking off, sent at a speed of her own or under air attack, the next carrier by id in formation is the guide; the place is across the line from the enemy\'s middle through the guide, one place a carrier on the hand she lies, the navy\'s spacing apart, inside the field\'s edge, closed at flank and kept at the guide\'s course and way, never below slow; the guide paces a follower far from her place', () => {
    const CV3: PlainFleet = { name: 'Task Group', nat: 'USA', ships: { cv: 3, ca: 3, dd: 6 } };
    const s = new SeaSim(plainTicket({ att: [CV3], def: [CVS], seed: 5, human: 'att', research: { att: ['taskforces'] } })) as any;
    s.step();
    expect(s.contactAt).toBeNull();
    s.strikes = []; // no strike aloft: every carrier in formation
    const [c1, c2, c3] = all(s, 'att', 'cv').sort((a, b) => a.id - b.id);
    const steered = (): boolean[] => [c1, c2, c3].map((c) => s.guideHelm(c) !== null);
    expect(steered()).toEqual([false, true, true]); // before the guns meet: a carrier group keeps no station
    // Formation is keepsCarrierStation's, as for the station: afloat, not breaking off, at cruising speed.
    for (const state of ['gone', 'routing'] as const) {
      c1.state = state;
      expect([c2, c3].map((c) => s.guideHelm(c) !== null), state).toEqual([false, true]);
      c1.state = 'steady';
    }
    const speed = (c: SeaUnit, v: string): void => { expect(s.command({ do: 'sea:speed', units: [c.id], speed: v } as never, 'att')).toBe(true); };
    speed(c3, 'slow');
    expect(steered()).toEqual([false, true, false]); // a carrier sent at a speed of her own goes as told
    speed(c3, 'cruise'); speed(c1, 'flank');
    expect(steered()).toEqual([false, false, true]); // the guide sent at her own speed: the next by id leads
    speed(c1, 'cruise');
    expect(steered()).toEqual([false, true, true]);
    // An enemy strike aloft at a carrier takes her out of formation: at the guide, the next by id leads; at a follower,
    // she goes. A strike of her own side's is nothing to her.
    const strike = (side: string, target: SeaUnit): void => { s.strikes = [{ at: s.t + SEA.strike.windup, side, target: target.id, weight: 0, from: '', count: 0, cv: 0 }]; };
    strike('def', c1);
    expect(steered()).toEqual([false, false, true]);
    strike('def', c3);
    expect(steered()).toEqual([false, true, false]);
    strike('att', c2);
    expect(steered()).toEqual([false, true, true]);
    s.strikes = [];
    // The place: the guide at (10,000, 10,000), the enemy due north of her; across the line is east-west. Carriers lying
    // east of her take the places one and two spacings east of her, in the order they lie, and close on them at flank.
    const [e1, e2] = all(s, 'def', 'ca');
    only(s, [[c1, 10000, 10000], [c2, 12000, 6000], [c3, 14000, 6000], [e1, 10000, 20000]]);
    s.centres.t = -1; // the sides' middles are kept for the tick: read them afresh with the ships moved
    const R = SEA.carriers.path.USA[1].spacing, slow = SEA.classes.cv.speed * SEA.speeds.slow, cruise = SEA.classes.cv.speed * SEA.speeds.cruise;
    c1.face = 0.3; c1.extra.way = (slow + cruise) / 2;
    let h = s.guideHelm(c2);
    expect(h.want).toBeCloseTo(Math.atan2(10000 + R - c2.x, 10000 - c2.y), 9);
    expect(h.speed).toBeCloseTo(SEA.classes.cv.speed * SEA.speeds.flank, 9);
    expect(s.guideHelm(c3).want).toBeCloseTo(Math.atan2(10000 + 2 * R - c3.x, 10000 - c3.y), 9);
    // Both more than the spacing from their places (4,011 and 4,045 m): the guide keeps her standoff at slow, the fleet's
    // keep-together way, and the pace is marked.
    expect(s.guideWaits(c1)).toBe(true);
    const pace = (): number => s.marks['att:cvPace:s'] ?? 0;
    let p0 = pace();
    s.steer(c1);
    expect(pace() - p0).toBeCloseTo(DT, 9);
    c1.x = 10000; c1.y = 10000; c1.face = 0.3; c1.extra.way = (slow + cruise) / 2;
    s.centres.t = -1;
    // One within the spacing of her place is still far enough off for her guide to wait; both within it, she goes on.
    c2.x = 10000 + R; c2.y = 9000;
    expect(s.guideWaits(c1)).toBe(true);
    c3.x = 10000 + 2 * R; c3.y = 10000 - R + 10;
    expect(s.guideWaits(c1)).toBe(false);
    p0 = pace();
    s.steer(c1);
    expect(pace() - p0).toBe(0);
    c1.x = 10000; c1.y = 10000; c1.face = 0.3; c1.extra.way = (slow + cruise) / 2;
    s.centres.t = -1;
    c3.y = 10000 - R - 10;
    expect(s.guideWaits(c1)).toBe(true);
    expect(s.guideWaits(c2)).toBe(false); // only the guide paces
    strike('def', c3); // a follower out of formation is none of her guide's
    expect(s.guideWaits(c1)).toBe(false);
    s.strikes = [];
    // One lying west of her, the place west; the other, alone on the east, the first place east.
    c2.x = 8000; c2.y = 6000; c3.x = 14000; c3.y = 6000;
    expect(s.guideHelm(c2).want).toBeCloseTo(Math.atan2(10000 - R - c2.x, 10000 - c2.y), 9);
    expect(s.guideHelm(c3).want).toBeCloseTo(Math.atan2(10000 + R - c3.x, 10000 - c3.y), 9);
    // On her place, the guide's course and way.
    c2.x = 10000 + R + 50; c2.y = 10000 - 100;
    h = s.guideHelm(c2);
    expect(h.want).toBeCloseTo(0.3, 9);
    expect(h.speed).toBeCloseTo(c1.extra.way, 9);
    // The guide slower than slow: the follower keeps way on at slow, steering round her place.
    c1.extra.way = slow / 2;
    h = s.guideHelm(c2);
    expect(h.speed).toBeCloseTo(slow, 9);
    expect(h.want).toBeCloseTo(Math.atan2(10000 + R - c2.x, 10000 - c2.y), 9);
    c1.extra.way = (slow + cruise) / 2;
    // The place is kept inside the field's edge (the turn-away's margin, 1,200 m): the guide 1,500 m from the east edge,
    // the place a spacing east of her is brought in to 1,200 m from it.
    const F = SEA.field.size;
    c1.x = F - 1500; c2.x = F - 1300; c2.y = 6000; c3.x = F - 1250; c3.y = 4000; e1.x = F - 1500; e1.y = 19000;
    s.centres.t = -1;
    expect(s.guideHelm(c2).want).toBeCloseTo(Math.atan2(F - 1200 - c2.x, 10000 - c2.y), 9);
    c1.x = 10000; c3.x = 14000; c3.y = 6000; e1.x = 10000; e1.y = 20000;
    // The line runs from the enemy's middle, not from the enemy nearest the guide.
    c3.state = 'gone'; c2.x = 12000; c2.y = 6000; e1.x = 7000; e1.y = 15000; e2.state = 'steady'; e2.x = 13000; e2.y = 19000;
    s.centres.t = -1;
    const [mx, my] = s.centre('def'), md = Math.hypot(c1.x - mx, c1.y - my), ux = (c1.x - mx) / md, uy = (c1.y - my) / md;
    const across = (c: SeaUnit): number => c.x * uy - c.y * ux, off = (across(c2) > across(c1) ? 1 : -1) * R;
    expect(s.guideHelm(c2).want).toBeCloseTo(Math.atan2(c1.x + uy * off - c2.x, c1.y - ux * off - c2.y), 9);
    // A lone carrier has no guide.
    c1.state = 'gone';
    expect(s.guideHelm(c2)).toBeNull();
    expect(s.guideWaits(c2)).toBe(false);
    // A carrier the player slows goes as told, at her own speed.
    const u = new SeaSim(plainTicket({ att: [CV3], def: [CVS], seed: 5, human: 'att', research: { att: ['taskforces'] } })) as any;
    while (u.contactAt === null && !u.over) u.step();
    const u3 = all(u, 'att', 'cv').sort((a, b) => a.id - b.id)[2];
    expect(u.command({ do: 'sea:speed', units: [u3.id], speed: 'slow' } as never, 'att')).toBe(true);
    run(u, 60);
    expect(u.guideHelm(u3)).toBeNull();
    expect(u3.extra.way).toBeCloseTo(slow, 6);
  });

  it("each for herself, a carrier never closes inside her navy's spacing of a sister: a course closing on the nearest within it turns square to the line between them, on the side nearer it; under air attack too, in either way; one sent at a speed of her own keeps no line, nor does a carrier in her guide's formation", () => {
    const J3: PlainFleet = { name: 'Striking Force', nat: 'JAP', ships: { cv: 3, ca: 2, dd: 8 } };
    const s = new SeaSim(plainTicket({ att: [J3], def: [CVB], seed: 5, human: 'att' })) as any;
    s.step();
    s.strikes = [];
    const [a, b, c] = all(s, 'att', 'cv').sort((x, y) => x.id - y.id), ecv = piece(s, 'def', 'cv');
    const R = SEA.carriers.path.JAP[0].spacing; // 7,000 m: Kido Butai's box
    const N = 0, E = Math.PI / 2, S = Math.PI, W = 3 * Math.PI / 2;
    only(s, [[a, 6000, 10000], [b, 6000 + R - 100, 10000], [c, 1000, 10000], [ecv, 6000 + 500, 10500]]);
    c.state = 'gone';
    const norm = (x: number): number => ((x % (2 * Math.PI)) + 2 * Math.PI) % (2 * Math.PI);
    const apart = (u: SeaUnit, want: number): number => norm(s.apart(u, want));
    const marked = (): number => s.marks['att:cvApart:s'] ?? 0;
    // b due east, inside the spacing. A course closing on her turns to the square course on its own side of the line;
    // one square to it or opening holds; straight at her, the square course clockwise.
    let m0 = marked();
    expect(apart(a, E / 2)).toBeCloseTo(N, 9);
    expect(marked() - m0).toBeCloseTo(DT, 9);
    expect(apart(a, E + E / 2)).toBeCloseTo(S, 9);
    expect(apart(a, E)).toBeCloseTo(S, 9);
    m0 = marked();
    for (const w of [N, S, S + E / 2, W, W + E / 2]) expect(apart(a, w), `${w}`).toBeCloseTo(w, 9);
    expect(marked() - m0).toBe(0);
    // Outside the spacing she steers as she would.
    b.x = 6000 + R + 100;
    expect(apart(a, E / 2)).toBeCloseTo(E / 2, 9);
    // The line is to the nearest sister afloat inside the spacing: c 2,000 m west of her, b inside it to the east.
    b.x = 6000 + R - 100; c.state = 'steady'; c.x = 4000;
    expect(apart(a, W + E / 2)).toBeCloseTo(N, 9); // closing on c (north-west) turns north
    c.state = 'gone';
    expect(apart(a, W + E / 2)).toBeCloseTo(W + E / 2, 9); // c sunk: b's line, and north-west opens from b
    // An enemy carrier is no sister (ecv lies 707 m off).
    b.x = 6000 + R + 100;
    expect(apart(a, E / 2)).toBeCloseTo(E / 2, 9);
    b.x = 6000 + R - 100;
    // Under direct air attack she swings on her own, but not into a sister (the design director, 21:00Z).
    s.strikes = [{ at: s.t + SEA.strike.windup, side: 'def', target: a.id, weight: 0, from: '', count: 0, cv: 0 }];
    expect(s.underAir(a)).toBe(true);
    expect(apart(a, E / 2)).toBeCloseTo(N, 9);
    s.strikes = [];
    // Sent at a speed of her own, or breaking off, she goes as told or runs: no line.
    expect(s.command({ do: 'sea:speed', units: [a.id], speed: 'flank' } as never, 'att')).toBe(true);
    expect(apart(a, E / 2)).toBeCloseTo(E / 2, 9);
    expect(s.command({ do: 'sea:speed', units: [a.id], speed: 'cruise' } as never, 'att')).toBe(true);
    expect(apart(a, E / 2)).toBeCloseTo(N, 9);
    a.state = 'routing';
    expect(apart(a, E / 2)).toBeCloseTo(E / 2, 9);
    a.state = 'steady';
    // On the guide (American, Carrier task forces): a carrier in formation keeps her place and no line; under air attack,
    // out of it, the line binds her at her navy's spacing.
    const g = new SeaSim(plainTicket({ att: [CVB], def: [J3], seed: 5, human: null, research: { att: ['taskforces'] } })) as any;
    g.step();
    g.strikes = [];
    const [g1, g2] = all(g, 'att', 'cv').sort((x, y) => x.id - y.id), RG = SEA.carriers.path.USA[1].spacing;
    only(g, [[g1, 6000, 10000], [g2, 6000 + RG - 100, 10000], [piece(g, 'def', 'cv'), 6000, 19000]]);
    expect(norm(g.apart(g1, E / 2))).toBeCloseTo(E / 2, 9);
    expect(norm(g.apart(g2, W + E / 2))).toBeCloseTo(W + E / 2, 9);
    g.strikes = [{ at: g.t + SEA.strike.windup, side: 'def', target: g1.id, weight: 0, from: '', count: 0, cv: 0 }];
    expect(norm(g.apart(g1, E / 2))).toBeCloseTo(N, 9);
    expect(norm(g.apart(g2, W + E / 2))).toBeCloseTo(W + E / 2, 9); // g2 is in formation still (the lone one: no guide)
    g2.x = 6000 + RG + 100;
    expect(norm(g.apart(g1, E / 2))).toBeCloseTo(E / 2, 9);
  });

  it("each for herself in a fight, a navy's carriers draw apart by their standoffs and never close by will once inside its spacing, through air attacks too", () => {
    // Japan, no research: Kido Butai's 7,000 m. Drawn up 813 m apart, they open by their own standoffs (the line never
    // opens by will) and once inside the spacing each holds off the other, struck or not.
    const s = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 5, human: null })) as any;
    const [a, b] = all(s, 'att', 'cv');
    let c0: number | null = null, least = Infinity, struck = 0;
    while (!s.over) {
      s.step();
      if (s.broken !== null || a.state === 'gone' || b.state === 'gone') break;
      if (s.contactAt === null) continue;
      if (c0 === null) c0 = gap(a, b);
      least = Math.min(least, gap(a, b));
      if (s.underAir(a) || s.underAir(b)) struck++;
    }
    expect(c0).not.toBeNull();
    expect(c0!).toBeLessThan(SEA.carriers.path.JAP[0].spacing);
    expect(struck).toBeGreaterThan(0);
    expect(s.marks['att:cvApart:s'] ?? 0).toBeGreaterThan(0);
    // Through the gun fight the gap falls only by the helm's lag in turning (6,889 m at contact, 6,581 m at the least).
    // With no line the two came 132-214 m apart (R3); with none under air attack, 3,680-4,646 m.
    expect(least).toBeGreaterThan(0.9 * c0!);
  });

  it('a carrier closes on its station at flank, nearer an enemy than its standoff it opens the range along the line from the enemy through its own ships, and its station stands back out of every enemy\'s standoff', () => {
    const s = new SeaSim(plainTicket({ att: [HOME], def: [CVB], seed: 5, human: null })) as any;
    const cv = piece(s, 'att', 'cv'), bb = piece(s, 'att', 'bb'), eca = piece(s, 'def', 'ca'), ecv = piece(s, 'def', 'cv');
    // Its gun ship at y 8,000 and the enemy due north: its station is ai.keep due south of the battleship.
    only(s, [[cv, 10000, 2000], [bb, 10000, 8000], [eca, 10000, 16000], [ecv, 10000, 18000]]);
    let h = s.carrierHelm(cv);
    expect(Math.abs(Math.sin(h.want))).toBeLessThan(1e-9); // due north for (10000, 8000 - keep)
    expect(Math.cos(h.want)).toBeGreaterThan(0);
    expect(h.speed).toBeCloseTo(SEA.classes.cv.speed * SEA.speeds.flank, 9);
    // The enemy cruiser 3,000 m off the battleship's beam, so within its standoff of the place ai.keep astern: the station
    // stands further back, out to the standoff (the carrier, at y 2,000, still due south of it and closing).
    eca.x = 13000; eca.y = 8000;
    const so = SEA.classes.cv.standoff!, back = Math.sqrt(so * so - 3000 * 3000);
    cv.y = 8000 - back - 150; cv.extra.way = 0;
    h = s.carrierHelm(cv);
    expect(Math.abs(Math.sin(h.want))).toBeLessThan(1e-6); // due north: the station is ahead of it, 150 m
    expect(Math.cos(h.want)).toBeGreaterThan(0);
    cv.y = 8000 - back + 150;
    h = s.carrierHelm(cv);
    // 150 m past it: inside the standoff, so it opens the range, south.
    expect(Math.cos(h.want)).toBeLessThan(0);
    // The enemy cruiser within its standoff of the carrier: she turns away, south, at her cruising speed.
    eca.x = 10000; eca.y = 2000 + so - 1000; cv.y = 2000;
    h = s.carrierHelm(cv);
    expect(Math.abs(Math.sin(h.want))).toBeLessThan(1e-9);
    expect(Math.cos(h.want)).toBeLessThan(0);
    expect(h.speed).toBeCloseTo(SEA.classes.cv.speed * SEA.speeds.cruise, 9);
  });

  it("R4's overtaking cue reads a carrier on station at the speed she could make: her own gun ships' way does not make an enemy at cruising speed overtake her", () => {
    const s = new SeaSim(plainTicket({ att: [HOME], def: [CVB], seed: 5, human: null })) as any;
    const cv = piece(s, 'att', 'cv'), bb = piece(s, 'att', 'bb'), eca = piece(s, 'def', 'ca'), ecv = piece(s, 'def', 'cv');
    only(s, [[cv, 10000, 5400], [bb, 10000, 8000], [eca, 10000, 16000], [ecv, 10000, 19000]]);
    s.step();
    expect(s.onStation.has(cv.id)).toBe(true);
    cv.face = 0; cv.extra.way = SEA.classes.bb.speed * SEA.speeds.cruise; // held back to her battleship's way
    eca.face = Math.PI; // coming straight at her
    eca.extra.way = SEA.classes.ca.speed * SEA.speeds.cruise;
    expect(s.threats('att')).toEqual([]);
    eca.extra.way = SEA.classes.ca.speed * SEA.speeds.flank;
    expect(s.threats('att')).toEqual([eca]);
  });

  it("nothing in the rules reads the wind: a carrier's turn into it is the 3D game's to show", () => {
    const play = (watch: boolean): string => {
      const s = new SeaSim(plainTicket({ att: [HOME, CVS], def: [BATTLE, CVB], seed: 5, human: null }));
      if (watch) for (const k of ['wind', 'windSpeed']) Object.defineProperty(s, k, { get: () => { throw new Error(`a rule read the ${k}`); } });
      run(s, 600);
      return JSON.stringify([s.over, s.units]);
    };
    // (The hashes behind them, seaWind and seaWindSpeed, are called by those two getters alone.)
    expect(play(true)).toBe(play(false));
  });
});

describe("badly hurt ships leave the line (the ship plan's R2, the design director at 01:36Z, 02:15Z and 03:49Z)", () => {
  const ddPiece = (s: any, side: 'att' | 'def'): SeaUnit => s.units.find((u: SeaUnit) => u.side === side && u.extra.cls === 'dd' && u.extra.ships0 > 1);
  /** What is left of the piece's damaged ship, in ships (1: none damaged). */
  const own = (u: SeaUnit): number => u.str * u.extra.ships0 - (u.extra.ships - 1);
  const live = (u: SeaUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';

  it('a ship worn to half her hull leaves the line, sunk or burning as the dice allow; the rest of her piece fights on', () => {
    const s = battle(5, { human: null }) as any;
    const u = ddPiece(s, 'def'), n0 = u.extra.ships0, H = s.hullOf(u);
    // Worn to 0.6 of a ship: she keeps her place.
    s.wear(u, (0.4 / n0) * H);
    expect(u.extra.ships).toBe(n0);
    expect(own(u)).toBeCloseTo(0.6, 9);
    expect(s.debt.att).toBe(0);
    // Worn on to 0.4: she leaves, the piece one ship fewer and whole; her side has lost what the blow dealt.
    const before = s.lostShare('def');
    s.wear(u, (0.2 / n0) * H);
    expect(u.extra.ships).toBe(n0 - 1);
    expect(u.str).toBeCloseTo((n0 - 1) / n0, 12);
    expect(u.extra.sunk + ((u.extra.burnt as number | undefined) ?? 0)).toBe(1);
    expect(live(u)).toBe(true);
    const w = s.info.get(u.id).weight;
    expect(s.lostShare('def') - before).toBeCloseTo((w * 0.2) / s.weight0.def, 9);
    // What she took with her is owed by the side whose blow put her out, in the blows' own measure, and counts toward
    // her side's loss as that side's next blows pay it, landing nothing.
    const owed = w * 0.4 * SEA.fire.hull;
    expect(s.debt.att).toBeCloseTo(owed, 9);
    expect(s.debt.def).toBe(0);
    const foe = piece(s, 'def', 'bb'), str0 = foe.str;
    expect(s.take(foe, owed / 2)).toBe(0);
    expect(s.lostShare('def') - before).toBeCloseTo((w * (0.2 + 0.2)) / s.weight0.def, 9);
    expect(s.take(foe, owed / 2)).toBeCloseTo(0, 12);
    expect(s.lostShare('def') - before).toBeCloseTo((w * (0.2 + 0.4)) / s.weight0.def, 9);
    expect(foe.str).toBeCloseTo(str0, 12);
  });

  it('her event tells the 3D game the hull she went with (look only): what she had left on falling out, 0 worn through', () => {
    const s = battle(5, { human: null }) as any;
    const u = ddPiece(s, 'def'), n0 = u.extra.ships0, H = s.hullOf(u);
    const went = (): FieldEvent[] => s.view().events.filter((e: FieldEvent) => (e.what === 'sea:sunk' || e.what === 'sea:burning') && e.unit === u.id);
    s.view();
    s.wear(u, (0.6 / n0) * H);
    const out = went();
    expect(out.length).toBe(1);
    expect(out[0].extra!.hull as number).toBeCloseTo(0.4, 3);
    // A whole ship worn through by one blow goes with nothing left.
    s.wear(u, (1 / n0) * H);
    const through = went();
    expect(through.length).toBe(1);
    expect(through[0].extra!.hull).toBe(0);
  });

  it('the debt is paid out of the side\'s next blows, weight for weight: what lands is what is left', () => {
    const s = battle(5, { human: null }) as any;
    const foe = piece(s, 'def', 'bb'), str0 = foe.str;
    s.debt.att = 10;
    expect(s.take(foe, 4)).toBe(0);
    expect(foe.str).toBe(str0);
    expect(s.debt.att).toBe(6);
    expect(s.take(foe, 9)).toBe(3);
    expect(s.debt.att).toBe(0);
    expect(foe.str).toBeLessThan(str0);
    // The other side's debt is its own: it pays nothing of it.
    s.debt.def = 5;
    expect(s.take(foe, 2)).toBe(2);
    expect(s.debt.def).toBe(5);
  });

  it('through a whole action no fighting piece keeps a ship at half her hull, and the debts are paid in the fight', () => {
    for (const seed of [3, 5, 8]) {
      const s = battle(seed, { human: null }) as any;
      while (!s.over) {
        s.step();
        for (const u of s.units as SeaUnit[]) if (live(u) && u.extra.ships > 0) expect(own(u), `${seed} ${u.name} at ${s.t}`).toBeGreaterThan(SEA.side.hurt);
      }
      // Ships did leave the line, and their debts were paid in the fight.
      expect((s.marks['att:debtPaid'] ?? 0) + (s.marks['def:debtPaid'] ?? 0), `${seed}`).toBeGreaterThan(0);
    }
  });

  it('a debt still unpaid when the action ends counts on neither side: she is a damaged ship home, not a loss', () => {
    const s = battle(5, { human: null }) as any;
    const u = ddPiece(s, 'def'), n0 = u.extra.ships0;
    s.wear(u, (0.6 / n0) * s.hullOf(u));
    expect(s.debt.att).toBeGreaterThan(0);
    const lost = [s.lostShare('att'), s.lostShare('def')], strs = s.units.map((f: SeaUnit) => f.str), owed = s.debt.att;
    s.settle();
    expect([s.lostShare('att'), s.lostShare('def')]).toEqual(lost);
    expect(s.units.map((f: SeaUnit) => f.str)).toEqual(strs);
    expect(s.debt.att).toBe(owed);
  });
});

describe("breaking off and the torpedo runs (the ship plan's R5, the design director at 01:34Z and 04:58Z)", () => {
  const all = (s: any, side: 'att' | 'def', cls: string): SeaUnit[] => s.units.filter((u: SeaUnit) => u.side === side && u.extra.cls === cls);
  const only = (s: any, keep: [SeaUnit, number, number, number?][]): void => {
    for (const u of s.units as SeaUnit[]) if (!keep.some(([k]) => k === u)) u.state = 'gone';
    for (const [u, x, y, face] of keep) { u.x = x; u.y = y; if (face !== undefined) u.face = face; }
  };
  const heading = (s: any, u: SeaUnit): number => { const f0 = u.face; s.steer(u); const c = u.face; u.face = f0; return c; };

  it('a ship breaking off turns away from the enemy nearest her still fighting and bends for home, not due home', () => {
    const s = battle(5, { human: null }) as any;
    const [ca] = all(s, 'att', 'ca'), [foe] = all(s, 'def', 'bb');
    // The attacker's home is the south edge (course pi). The enemy to her east: she makes off south-west.
    only(s, [[ca, 10000, 10000, Math.PI], [foe, 13000, 10000]]);
    ca.state = 'routing';
    const want = s.breakOffCourse(ca, Math.PI);
    expect(want).toBeCloseTo(Math.atan2(-1, -1), 9);
    // The enemy dead astern of her course home: straight home.
    foe.x = 10000; foe.y = 13000;
    expect(Math.abs(Math.atan2(Math.sin(s.breakOffCourse(ca, Math.PI) - Math.PI), Math.cos(s.breakOffCourse(ca, Math.PI) - Math.PI)))).toBeLessThan(1e-9);
    // By the west edge with the enemy to her east, she holds for home rather than leave the field by its side.
    ca.x = 500; foe.x = 3500; foe.y = 10000;
    expect(Math.abs(Math.sin(s.breakOffCourse(ca, Math.PI)))).toBeLessThan(1e-9);
    // An enemy that is breaking off too is not one she turns from: with none still fighting, home.
    foe.state = 'routing';
    expect(s.breakOffCourse(ca, Math.PI)).toBe(Math.PI);
    // And the helm takes it: her course comes round toward it.
    foe.state = 'steady'; ca.x = 10000; foe.x = 13000;
    const before = Math.abs(Math.atan2(Math.sin(ca.face - Math.atan2(-1, -1)), Math.cos(ca.face - Math.atan2(-1, -1))));
    const after = heading(s, ca);
    expect(Math.abs(Math.atan2(Math.sin(after - Math.atan2(-1, -1)), Math.cos(after - Math.atan2(-1, -1))))).toBeLessThan(before);
  });

  it('a destroyer piece runs only with a full salvo banked; short of one it fights with its guns', () => {
    const s = battle(11, { human: null }) as any;
    while (s.contactTime === null && !s.over) s.step();
    s.contactAt = s.t;
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, ebb = all(s, 'def', 'bb')[0];
    const bb = all(s, 'att', 'bb')[0]; // its own battleship engaged with hers
    only(s, [[dd, 10000, 10000], [ebb, 10000, 13000, Math.PI], [bb, 6000, 13000]]);
    s.bank[dd.id] = salvo(s, dd) * 0.99;
    s.aim(dd); s.startRuns();
    expect(s.runs.has(dd.id)).toBe(false);
    s.bank[dd.id] = salvo(s, dd);
    s.aim(dd); s.startRuns();
    expect(s.runs.get(dd.id)?.target).toBe(ebb.id);
  });

  it("a destroyer piece goes in with a full salvo loaded; what its guns have not earned of it is owed by its side when the salvo lands, paid out of the side's gunfire", () => {
    const s = battle(11, { human: null }) as any;
    for (const u of s.units as SeaUnit[]) {
      if (u.extra.cls !== 'dd') { expect(s.bank[u.id]).toBe(0); continue; }
      expect(s.bank[u.id]).toBeCloseTo(salvo(s, u), 12);
      expect(s.loaded[u.id]).toBe(s.bank[u.id]);
    }
    while (s.contactAt === null && !s.over) s.step();
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, ebb = all(s, 'def', 'bb')[0], edd = all(s, 'def', 'dd')[0];
    only(s, [[dd, 10000, 10000], [ebb, 10000, 18000], [edd, 2000, 2000]]);
    // Its guns with nothing in reach for ten seconds earn ten seconds of it.
    s.aim(dd);
    const full = salvo(s, dd), per = full / SEA.torp.bank;
    for (let i = 0; i < 10 / DT; i++) s.fire(dd);
    expect(s.bank[dd.id]).toBeCloseTo(full, 9);
    expect(s.loaded[dd.id]).toBeCloseTo(full - per * 10, 9);
    // Launched: nothing is owed while the salvo runs (one still running when the action ends has dealt nothing).
    s.launch(dd, ebb, null);
    expect(s.loaded[dd.id]).toBe(0);
    expect(s.advance.att).toBe(0);
    // It lands: the unearned part is owed by its side now that it has dealt its weight (the design director, 06:14Z).
    s.t += SEA.torp.travel;
    s.landTorps();
    expect(s.torps.length).toBe(0);
    expect(s.advance.att).toBeCloseTo(full - per * 10, 9);
    expect(s.advance.def).toBe(0);
    // The side's next gunfire pays it before anything lands.
    edd.x = 10000; edd.y = 12000;
    s.aim(dd);
    const owed = s.advance.att, dealt = s.dealt[dd.id];
    s.fire(dd);
    expect(s.advance.att).toBeLessThan(owed);
    expect(s.dealt[dd.id]).toBe(dealt);
  });

  it('runs are made again: in whole actions the staff\'s destroyers make torpedo runs, each with a full salvo', () => {
    let runs = 0;
    for (const seed of [3, 5, 8]) {
      const s = battle(seed, { human: null }) as any;
      const launch0 = s.launch.bind(s);
      s.launch = (u: SeaUnit, foe: SeaUnit, run: unknown): void => { if (run) { runs++; expect(s.bank[u.id] / salvo(s, u), `${seed}`).toBeGreaterThanOrEqual(1 - 1e-6); } launch0(u, foe, run); };
      runOut(s);
    }
    expect(runs).toBeGreaterThan(0);
  });

  it('through whole actions, no run the staff makes launches short of a full salvo', () => {
    for (const seed of [3, 5, 8]) {
      const s = battle(seed, { human: null, def: [{ name: '2nd Fleet', nat: 'ITA', ships: { ca: 7, dd: 25 } }] }) as any;
      const launch0 = s.launch.bind(s);
      const shares: number[] = [];
      s.launch = (u: SeaUnit, foe: SeaUnit, run: unknown): void => { if (run) shares.push(s.bank[u.id] / salvo(s, u)); launch0(u, foe, run); };
      runOut(s);
      for (const x of shares) expect(x, `${seed}`).toBeGreaterThanOrEqual(1 - 1e-6);
    }
  });
});

describe("what the 3D game draws of how ships are handled (the ship plan's B1, ruling 239 at 23:20Z)", () => {
  const CVS: PlainFleet = { name: 'Striking Force', nat: 'JAP', ships: { cv: 2, ca: 2, dd: 8 } };
  const CVB: PlainFleet = { name: 'Task Force', nat: 'USA', ships: { cv: 2, ca: 3, dd: 6 } };
  const LINE: PlainFleet = { name: 'Battle Force', nat: 'USA', ships: { bb: 2, ca: 3, dd: 8 } };
  const all = (s: any, side: 'att' | 'def', cls: string): SeaUnit[] => s.units.filter((u: SeaUnit) => u.side === side && u.extra.cls === cls);
  const only = (s: any, keep: [SeaUnit, number, number][]): void => {
    for (const u of s.units as SeaUnit[]) if (!keep.some(([k]) => k === u)) u.state = 'gone';
    for (const [u, x, y] of keep) { u.x = x; u.y = y; }
  };

  it("no rule reads a fleet's flag: the same ships under any two nations' flags fight the same action (Brian, 21 November: every battle mechanic holds for every playable nation)", () => {
    const play = (a: string, d: string): string => {
      const sim = new SeaSim(plainTicket({ att: [{ ...HOME, nat: a }], def: [{ ...BATTLE, nat: d }], seed: 11, human: null }));
      run(sim, 1800);
      return JSON.stringify(sim.units.map((u) => [u.id, u.state, Math.round(u.x), Math.round(u.y), u.extra.ships, u.extra.run ?? null]));
    };
    expect(play('ROM', 'YUG')).toBe(play('UK', 'GER'));
  });

  it('nothing in the rules reads them: scrambled before every tick, the same action plays out to the same end', () => {
    const play = (scramble: boolean): string => {
      const s = new SeaSim(plainTicket({ att: [HOME, CVS], def: [BATTLE, CVB], seed: 5, human: null }));
      const told: FieldEvent[] = [];
      for (let i = 0; i < Math.round(1500 / DT) && !s.over; i++) {
        if (scramble) for (const u of s.units) Object.assign(u.extra, { foe: 999, helm: 7, runPhase: 'in', runUntil: -1, smoke: true, lineAt: 5, combFrom: 999, combUntil: -1 }, u.extra.cls === 'cv' ? { flight: 'out', cover: 9 } : {});
        s.step();
        if (i % 50 === 0) told.push(...s.view().events);
      }
      told.push(...s.view().events);
      // Keys in order: a scrambled field is set before the sim's own, so only the order of the keys could differ.
      return JSON.stringify([s.over, s.units, told], (_, v) => (v && typeof v === 'object' && !Array.isArray(v) ? Object.fromEntries(Object.entries(v).sort(([a], [b]) => (a < b ? -1 : 1))) : v));
    };
    expect(play(true)).toBe(play(false));
  });

  it('every piece says the enemy its course was set against and how hard it turns, + to starboard', () => {
    const s = new SeaSim(plainTicket({ att: [HOME], def: [BATTLE], seed: 11, human: null }));
    let bad = 0, foes = 0, turning = 0;
    for (let i = 0; i < Math.round(900 / DT) && !s.over; i++) {
      s.step();
      for (const u of s.units) {
        const f = u.extra.foe as number, h = u.extra.helm as number;
        const e = s.units.find((p) => p.id === f);
        if (!(f === -1 || (e && e.side !== u.side)) || !(Math.abs(h) <= 1)) bad++;
        if (f >= 0) foes++;
        if (h !== 0) turning++;
      }
    }
    expect(bad).toBe(0);
    expect(foes).toBeGreaterThan(0);
    expect(turning).toBeGreaterThan(0);
    // Sent to a point on its starboard bow, a ship puts its helm to starboard; on its port bow, to port.
    for (const [dx, sign] of [[3000, 1], [-3000, -1]] as const) {
      const sim = battle();
      const bb = piece(sim, 'att', 'bb');
      bb.face = 0;
      sim.command({ do: 'move', units: [bb.id], x: bb.x + dx, y: bb.y + 1000 }, 'att');
      sim.step();
      expect(Math.sign(bb.extra.helm as number)).toBe(sign);
      expect(bb.extra.foe).toBe(-1); // its order set its course, not an enemy
    }
  });

  it("a carrier's air group goes out, comes back and lands on 'windup' seconds after it found its target; its held fighters fall as they fight", () => {
    const s = new SeaSim(plainTicket({ att: [CVS], def: [CVB], seed: 7, human: null })) as any;
    const cv = all(s, 'att', 'cv')[0], foe = all(s, 'def', 'cv')[0];
    expect([cv.extra.flight, cv.extra.cover]).toEqual(['deck', 0]);
    const ab = cv.abilities!.find((a) => a.key === 'strike')!;
    ab.ready = 0; ab.winding = false;
    expect(s.strike(cv, foe)).toBe(true);
    s.showLooks();
    expect([cv.extra.flight, cv.extra.cover]).toEqual(['out', 1]);
    s.t += SEA.strike.windup;
    s.landStrikes();
    s.showLooks();
    expect(cv.extra.flight).toBe('back');
    s.view();
    s.t += SEA.strike.windup - 1;
    s.recover();
    expect(s.view().events.some((e: FieldEvent) => e.what === 'sea:recover')).toBe(false);
    s.t += 1;
    s.recover();
    s.showLooks();
    const back = s.view().events.find((e: FieldEvent) => e.what === 'sea:recover')!;
    expect([back.unit, back.text, back.extra!.ditch, back.cue]).toEqual([cv.id, '', false, undefined]);
    expect(cv.extra.flight).toBe('deck');
    // The fighters it held meet a strike on its fleet and come down: the share still up falls.
    s.meet(cv, s.cover[cv.id] / 2, 'def', true);
    s.showLooks();
    expect(cv.extra.cover as number).toBeGreaterThan(0);
    expect(cv.extra.cover as number).toBeLessThan(1);
    // A carrier gone down before its aircraft are back: they come down in the sea.
    ab.ready = 0; ab.winding = false;
    s.strike(cv, foe);
    s.t += SEA.strike.windup;
    s.landStrikes();
    cv.state = 'gone';
    s.t += SEA.strike.windup;
    s.view();
    s.recover();
    expect(s.view().events.find((e: FieldEvent) => e.what === 'sea:recover')!.extra!.ditch).toBe(true);
  });

  it('a torpedo salvo says how full the bank was; a run says its phase and when it turns back, and lays smoke as it turns away', () => {
    const s = battle(11, { human: null }) as any;
    s.contactAt = 0;
    const [dd, dd2] = all(s, 'att', 'dd'), ebb = all(s, 'def', 'bb')[0];
    only(s, [[dd, 10000, 8000], [dd2, 12000, 8000], [ebb, 10000, 14000]]);
    const free = { kind: 'free' } as const, press = SEA.styles.methodical!.press;
    s.bank[dd.id] = (SEA.fire.rate * s.output(dd) * s.day('att') * SEA.torp.bank) / 2;
    s.beginRun(dd, ebb);
    s.showLooks();
    expect([dd.extra.runPhase, dd.extra.runUntil, dd.extra.smoke]).toEqual(['in', undefined, false]);
    dd.y = ebb.y - press;
    s.runCourse(dd, free);
    s.showLooks();
    expect([dd.extra.runPhase, dd.extra.runUntil, dd.extra.smoke]).toEqual(['away', Math.round((s.t + SEA.torp.away) * 10) / 10, true]);
    expect(s.view().events.find((e: FieldEvent) => e.what === 'sea:torpedo')!.extra!.share).toBe(0.5);
    s.t += SEA.torp.away;
    s.runCourse(dd, free);
    s.showLooks();
    expect([dd.extra.runPhase, dd.extra.runUntil, dd.extra.smoke]).toEqual([undefined, undefined, false]);
    // An empty bank launches no tracks.
    s.bank[dd2.id] = 0;
    s.launch(dd2, ebb, null);
    expect(s.view().events.find((e: FieldEvent) => e.what === 'sea:torpedo')!.extra!.share).toBe(0);
  });

  it("light ships breaking off lay smoke, and so does a destroyer piece near its carrier turning away from a battle line; big ships do not", () => {
    const s = new SeaSim(plainTicket({ att: [CVS], def: [LINE], seed: 7, human: null })) as any;
    const cv = all(s, 'att', 'cv')[0], [dd, far] = all(s, 'att', 'dd'), ca = all(s, 'att', 'ca')[0], bb = all(s, 'def', 'bb')[0];
    only(s, [[cv, 10000, 9000], [dd, 10000 + SEA.torp.station - 100, 9000], [far, 13000, 9000], [ca, 7000, 9000], [bb, 10000, 9000 + SEA.evade.see / 2]]);
    for (const u of [cv, dd, far, ca, bb]) s.steer(u);
    s.showLooks();
    expect([dd.extra.smoke, far.extra.smoke, ca.extra.smoke, cv.extra.smoke]).toEqual([true, false, false, false]);
    for (const u of [far, ca, bb]) u.state = 'routing';
    s.showLooks();
    expect([far.extra.smoke, ca.extra.smoke, bb.extra.smoke]).toEqual([true, true, false]);
  });

  it("a ship out of the fight burning limps for her own side's edge, her course a hash within the spread, at a fraction of cruising", () => {
    const s = battle(11, { human: null }) as any;
    for (const side of ['att', 'def'] as const) {
      const bb = piece(s, side, 'bb');
      s.lostShips[side].bb = s.caps[side].bb;
      s.view();
      s.loseShip(bb);
      const limp = s.view().events.find((e: FieldEvent) => e.what === 'sea:burning')!.extra!.limp as { course: number; speed: number };
      const home = side === 'att' ? Math.PI : 0;
      expect(Math.abs(Math.atan2(Math.sin(limp.course - home), Math.cos(limp.course - home)))).toBeLessThanOrEqual(SEA.look.limpSpread / 2 + 0.01);
      expect(limp.speed).toBe(Math.round(SEA.classes.bb.speed * SEA.speeds.cruise * SEA.look.limpSpeed * 10) / 10);
    }
  });

  it('the guns that turn aircraft away are named on the flak event', () => {
    const s = new SeaSim(plainTicket({ att: [CVS], def: [LINE], seed: 7, human: null })) as any;
    const bb = all(s, 'def', 'bb')[0];
    s.view();
    s.meet(bb, 1, 'att', true);
    const ships = s.view().events.find((e: FieldEvent) => e.what === 'sea:flak')!.extra!.ships as number[];
    const near = (s.units as SeaUnit[]).filter((e) => e.side === 'def' && e.state !== 'gone' && e.extra.cls !== 'cv' && Math.hypot(e.x - bb.x, e.y - bb.y) <= SEA.strike.radius).map((e) => e.id);
    expect(ships).toContain(bb.id);
    expect([...ships].sort()).toEqual([...near].sort());
  });

  it('one wind a battle, from its seed, its bearing and its strength: a hash, the same every time, and not the same in every battle', () => {
    const winds = Array.from({ length: 40 }, (_, i) => seaWind(i + 1));
    expect(winds.every((w) => w >= 0 && w < 2 * Math.PI)).toBe(true);
    expect(seaWind(5)).toBe(seaWind(5));
    expect(Math.min(...winds)).toBeLessThan(1);
    expect(Math.max(...winds)).toBeGreaterThan(5);
    const sim = battle(5);
    expect(sim.wind).toBe(seaWind(sim.ticket.seed));
    const blows = Array.from({ length: 40 }, (_, i) => seaWindSpeed(i + 1));
    expect(blows.every((w) => w >= 4 && w <= 11)).toBe(true);
    expect(Math.max(...blows) - Math.min(...blows)).toBeGreaterThan(4);
    expect(sim.windSpeed).toBe(seaWindSpeed(sim.ticket.seed));
  });
});

describe('what a blow takes', () => {
  // b415: the card counts each side's carriers and its coasts' aircraft in its strength; the field wears them as it
  // wears the ships, so a side breaks off on the whole of what it brought and the aircraft are worth what the card says.
  const withAir = (): SeaSim => new SeaSim(plainTicket({ att: [HOME], def: [BATTLE], air: { att: 20, def: 12 }, seed: 3, human: null }));

  it("the field's strength is the card's: the ships and the coasts' aircraft", () => {
    const sim = withAir(), s = sim as any;
    expect(s.weight0.att).toBeCloseTo(sim.ticket.reckoning.A, 6);
    expect(s.weight0.def).toBeCloseTo(sim.ticket.reckoning.D, 6);
    expect(sim.lostShare('att')).toBe(0);
    expect(sim.balance().att + sim.balance().def).toBeCloseTo(1, 9);
  });

  it("a blow on a ship wears the side's ships, its carriers' aircraft and its coasts' aircraft by their weight, and no carrier's hull", () => {
    const sim = withAir(), s = sim as any;
    const dd = piece(sim, 'att', 'dd'), cv = piece(sim, 'att', 'cv');
    const T = s.weight0.att, w = 0.2, nerve = cv.mor;
    s.take(dd, w);
    expect(dd.extra.sunk).toBe(0); // (a ship going down nearby shakes the carrier's nerve as any other's)
    // Each part of what the side brought takes its share of the blow by weight: the ships and the carrier wear as hulls
    // do ('fire.hull' turns weight into wear), the coasts' aircraft have no hull and lose the whole of theirs ...
    const air = 20 / T, out = w / T;
    expect(1 - (cv.extra.planes as number)).toBeCloseTo(out / SEA.fire.hull, 9);
    expect(1 - s.airLeft.att / 20).toBeCloseTo(out, 9);
    expect(sim.lostShare('att') * T).toBeCloseTo((w * (1 - air)) / SEA.fire.hull + w * air, 9);
    // ... and the carrier stays afloat and unshaken: it is its aircraft that are lost.
    expect(cv.str).toBe(1);
    expect(cv.mor).toBe(nerve);
    expect(s.airLeft.def).toBe(12);
  });

  it("a ship going down shakes the pieces near it by the weight it carried, against its side's (b438)", () => {
    const shakeOf = (cls: 'dd' | 'bb'): number => {
      const sim = withAir(), s = sim as any;
      const lost = piece(sim, 'att', cls), near = sim.units.find((u) => u.side === 'att' && u.id !== lost.id && u.extra.cls === 'ca')!;
      near.x = lost.x; near.y = lost.y;
      const before = near.mor;
      s.loseShip(lost);
      expect(before - near.mor).toBeCloseTo((SEA.nerve.near * s.info.get(lost.id).weight) / s.weight0.att, 9);
      return before - near.mor;
    };
    expect(shakeOf('dd')).toBeLessThan(shakeOf('bb'));
  });

  it("a carrier's strikes and the coasts' raids are as strong as the aircraft left", () => {
    const sim = withAir(), s = sim as any;
    const cv = piece(sim, 'att', 'cv');
    const full = s.output(cv);
    s.deck[cv.id] = 0.5;
    expect(s.output(cv)).toBeCloseTo(full / 2, 9);
    s.airLeft.att = 5;
    s.nextAir = { att: 0, def: Infinity };
    const before = sim.lostShare('def');
    s.coastAir();
    const T = s.weight0.def;
    // The raid's weight, less what the defender's guns near the ship it found denied it (step 5; a trade they pay for out
    // of their own next gunfire), shared over what the defender brought: its ships wear as hulls, its coasts' aircraft
    // (12) whole (to within the raid's falling on several ships one after another).
    const flak = s.tally['def:denied'] ?? 0;
    expect(flak).toBeGreaterThan(0);
    expect(flak).toBeLessThanOrEqual(0.12 * SEA.fire.rate * 5 * SEA.air.share * SEA.air.every * s.day('att') + 1e-12);
    const W = SEA.fire.rate * 5 * SEA.air.share * SEA.air.every * s.day('att') - flak, air = 12 / T;
    expect((sim.lostShare('def') - before) * T).toBeCloseTo((W * (1 - air)) / SEA.fire.hull + W * air, 3);
  });
});

describe('the orders of the first slice', () => {
  it('a course: a ship steers for the point it is sent to and heaves to there', () => {
    const sim = battle();
    const bb = piece(sim, 'att', 'bb');
    const to = { x: bb.x - 3000, y: bb.y + 500 };
    expect(sim.command({ do: 'move', units: [bb.id], x: to.x, y: to.y }, 'att')).toBe(true);
    const d0 = Math.hypot(bb.x - to.x, bb.y - to.y);
    run(sim, 60);
    expect(Math.hypot(bb.x - to.x, bb.y - to.y)).toBeLessThan(d0 - 300);
    run(sim, 400);
    if (!sim.over && bb.state === 'steady') expect(Math.hypot(bb.x - to.x, bb.y - to.y)).toBeLessThan(400);
  });

  it('several ships sent to one point form a line there instead of piling up', () => {
    const sim = battle();
    const ca = sim.units.filter((u) => u.side === 'att' && u.extra.cls === 'ca').map((u) => u.id);
    expect(sim.command({ do: 'move', units: ca, x: 10000, y: 4000 }, 'att')).toBe(true);
    run(sim, 300);
    const xs = sim.units.filter((u) => ca.includes(u.id)).map((u) => u.x).sort((a, b) => a - b);
    for (let i = 1; i < xs.length; i++) expect(xs[i] - xs[i - 1]).toBeGreaterThan(300);
  });

  it('speed: flank gains way past cruising, and holding takes the way off', () => {
    const sim = battle();
    const [a, b] = sim.units.filter((u) => u.side === 'att' && u.extra.cls === 'dd');
    expect(sim.command({ do: 'sea:speed', units: [a.id], speed: 'flank' } as never, 'att')).toBe(true);
    expect(sim.command({ do: 'hold', units: [b.id] }, 'att')).toBe(true);
    run(sim, 40);
    expect(a.extra.way).toBeGreaterThan(SEA.classes.dd.speed * SEA.speeds.cruise);
    expect(b.extra.way).toBe(0);
    // An unknown speed is refused.
    expect(sim.command({ do: 'sea:speed', units: [a.id], speed: 'warp' } as never, 'att')).toBe(false);
  });

  it('attack: a ship fires on the enemy it is told to as soon as it has it in reach', () => {
    const sim = battle();
    const bb = piece(sim, 'att', 'bb'), foe = piece(sim, 'def', 'bb');
    expect(sim.command({ do: 'attack', units: [bb.id], target: foe.id }, 'att')).toBe(true);
    let fired = false;
    for (let i = 0; i < 6000 && !sim.over && !fired; i++) {
      sim.step();
      if (Math.hypot(bb.x - foe.x, bb.y - foe.y) <= SEA.classes.bb.reach && bb.extra.target === foe.id) fired = true;
    }
    expect(fired).toBe(true);
    // An order to the enemy's ships, or at one's own, is refused.
    expect(sim.command({ do: 'attack', units: [foe.id], target: bb.id }, 'att')).toBe(false);
    expect(sim.command({ do: 'attack', units: [bb.id], target: piece(sim, 'att', 'ca').id }, 'att')).toBe(false);
  });

  it('carrier strike: counted, not flown: it flies off, lands after its wind-up, and must wait to strike again', () => {
    const sim = battle();
    const cv = piece(sim, 'att', 'cv'), foe = piece(sim, 'def', 'bb');
    expect(sim.command({ do: 'ability', units: [cv.id], key: 'strike', target: foe.id }, 'att')).toBe(true);
    const off = events(sim).find((e) => e.what === 'ability')!;
    expect(off.text).toBe('A carrier of the Home Fleet flies off 24 bombers against a battleship of the Battle Fleet.');
    // Not ready again until its aircraft are back.
    expect(sim.command({ do: 'ability', units: [cv.id], key: 'strike', target: foe.id }, 'att')).toBe(false);
    const str0 = foe.str;
    run(sim, SEA.strike.windup - 1);
    expect(events(sim).some((e) => e.what === 'sea:strike')).toBe(false);
    run(sim, 2);
    const hit = events(sim).find((e) => e.what === 'sea:strike')!;
    expect(hit.unit).toBe(foe.id);
    expect(foe.str).toBeLessThan(str0);
    // No aircraft are on the field at any time: the strike is a count and a weight.
    expect(sim.units.every((u) => SEA_CLASSES.includes(u.extra.cls as never))).toBe(true);
    // A strike ordered of a ship that is not a carrier does nothing.
    expect(sim.command({ do: 'ability', units: [piece(sim, 'att', 'bb').id], key: 'strike', target: foe.id }, 'att')).toBe(false);
  });

  it('withdraw: a ship makes for its own edge at full speed and leaves the field, not lost', () => {
    const sim = battle();
    const ca = piece(sim, 'att', 'ca');
    expect(sim.command({ do: 'withdraw', units: [ca.id] }, 'att')).toBe(true);
    run(sim, 600);
    expect(ca.state).toBe('withdrawn');
    expect(ca.extra.sunk).toBe(0);
    // A ship that has left takes no more orders.
    expect(sim.command({ do: 'hold', units: [ca.id] }, 'att')).toBe(false);
  });

  it('the whole fleet breaks off on its admiral\'s order, and the action ends as a defeat for it', () => {
    const sim = battle();
    run(sim, 30);
    expect(sim.command({ do: 'withdrawAll', side: 'def' }, 'att')).toBe(false); // not his to order
    expect(sim.command({ do: 'withdrawAll', side: 'att' }, 'att')).toBe(true);
    runOut(sim);
    expect(sim.over!.winner).toBe('def');
    expect(sim.over!.R).toBeLessThan(1);
  });

  it('"what you had won stays won" only once the field has moved the hander\'s way by 5 points or more', () => {
    for (const odds of [1.6, 2, 2.5]) {
      const sim = battle(11, { odds });
      const k = sim.ticket.reckoning.odds, share = (o: number): number => o / (1 + o);
      const gain = (): number => share((k * (1 - sim.lostShare('att'))) / (1 - sim.lostShare('def'))) - share(k);
      while (!sim.over && gain() < 0.05) sim.step();
      if (sim.over) continue;
      expect(sim.command({ do: 'dice' }, 'att')).toBe(true);
      expect(sim.over!.lines).toContain('The staff settled the rest of the action in the North Sea from where it stood: what you had won stays won.');
      return;
    }
    throw new Error('no action moved 5 points before it ended');
  });

  it('one press hands the rest of the action to the dice, at the ratio then standing', () => {
    const sim = battle(11, { odds: 1.3 });
    run(sim, 300);
    expect(sim.command({ do: 'dice' }, 'att')).toBe(true);
    const r = sim.over!;
    expect(r).not.toBeNull();
    expect((r.extra as unknown as SeaLosses).toDice).toBe(true);
    const [lo, hi] = diceRange(1.3);
    expect(r.R).toBeGreaterThanOrEqual(Math.min(lo, 1));
    expect(r.R).toBeLessThanOrEqual(hi);
    // At 5:00 the field has moved the attacker's way by about 1 point, under the 5 the clause asks (20:51 UTC).
    expect(r.lines).toContain('The staff settled the rest of the action in the North Sea from where it stood.');
    // The player never reads "dice" (log b348).
    expect(r.lines.join(' ')).not.toMatch(/dice/i);
    // Nothing is taken after the end.
    expect(sim.command({ do: 'hold', units: [piece(sim, 'att', 'bb').id] }, 'att')).toBe(false);
  });

  it('handed to the staff before a shot is fired, it books exactly what the campaign would have rolled', () => {
    for (const seed of [3, 11, 29]) for (const odds of [0.85, 1.3, 2.14]) {
      const sim = battle(seed, { odds });
      expect(sim.command({ do: 'dice' }, 'att')).toBe(true);
      const r = sim.over!;
      expect(r.R).toBeCloseTo(odds * sim.luck, 9);
      expect(r.winner).toBe(r.R >= 1 ? 'att' : 'def');
      expect(r.lines).toContain('No ship had gone down on either side when the staff took over.');
    }
  });

  it('the computer takes over a side when asked, and gives it back', () => {
    const sim = battle();
    expect(sim.ai).toEqual({ att: false, def: true });
    expect(sim.command({ do: 'auto', side: 'att', on: true } as never, 'att')).toBe(true);
    expect(sim.ai.att).toBe(true);
  });
});

describe('what an action hands back', () => {
  it('never sinks more of a class than the rules would at that side\'s worst luck; the rest burn and come home', () => {
    for (let seed = 1; seed <= 12; seed++) {
      const sim = battle(seed, { human: null, odds: seed % 2 ? 0.9 : 1.4 });
      runOut(sim);
      const x = sim.over!.extra as unknown as SeaLosses;
      const caps = sunkCaps(sim.ticket.place.fleets as never, sim.ticket.reckoning.odds);
      for (const side of ['att', 'def'] as const) for (const c of SEA_CLASSES) {
        const sunk = sim.ticket.place.fleets.filter((f) => f.side === side).reduce((n, f) => n + x.sunk[f.id][c], 0);
        expect(sunk, `seed ${seed} ${side} ${c}`).toBeLessThanOrEqual(caps[side][c]);
      }
      // The pieces agree with the fleets' books.
      const onField = sim.units.reduce((n, u) => n + u.extra.sunk, 0);
      const booked = Object.values(x.sunk).reduce((n, s) => n + s.bb + s.cv + s.ca + s.dd, 0);
      expect(onField).toBe(booked);
    }
  });

  it("the ratio is the field's verdict, never outside what the dice could roll, and the level is the rules' word for it", () => {
    for (let seed = 1; seed <= 12; seed++) {
      const odds = [0.85, 1, 1.2, 2][seed % 4];
      const sim = battle(seed, { human: null, odds });
      runOut(sim);
      const r = sim.over!;
      const [lo, hi] = diceRange(odds);
      expect(r.winner === 'att').toBe(r.R >= 1);
      if (r.winner === 'att') expect(r.R).toBeLessThanOrEqual(Math.max(1, hi));
      else expect(r.R).toBeGreaterThanOrEqual(Math.min(0.999, lo));
      expect(r.level).toBe(seaLevel(r.R));
    }
  });

  it('the action is over within the time the field allows', () => {
    const sim = battle(3, { human: null });
    runOut(sim);
    expect(sim.over!.seconds).toBeLessThanOrEqual(SEA.field.limit + DT);
  });
});

describe("the rules' own scale", () => {
  it("losses at a given ratio are the rules' word for word", () => {
    // navy.ts seaBattles: hard = clamp(0.14 + 0.08 edge, .18, .5), light = clamp(0.16 / edge, .03, .14).
    expect(lossShares(1).att).toBeCloseTo(0.14, 9);
    expect(lossShares(1).def).toBeCloseTo(0.22, 9);
    expect(lossShares(0.5).att).toBeCloseTo(0.3, 9);
    expect(lossShares(0.5).def).toBeCloseTo(0.08, 9);
    expect(lossShares(10)).toEqual({ att: 0.03, def: 0.5 });
  });

  it("the dice's chance: none below 1/1.2, all above 1/0.8", () => {
    expect(seaChance(0.8)).toBe(0);
    expect(seaChance(1)).toBeCloseTo(0.5, 9);
    expect(seaChance(1.25)).toBe(1);
  });

  it('a won fight books at least the narrowest win and a lost one at most the narrowest loss (log b331)', () => {
    // A rout of the defender at odds the dice could never carry: the narrowest win.
    expect(seaRatio('att', { att: 0, def: 0.5 }, 0.5)).toBe(1);
    // A lost fight at long odds in the attacker's favour: the narrowest loss.
    expect(seaRatio('def', { att: 0.5, def: 0 }, 3)).toBe(0.999);
    // Within the dice's run the exchange decides.
    const r = seaRatio('att', { att: 0.1, def: 0.2 }, 1);
    expect(r).toBeGreaterThan(1);
    expect(r).toBeLessThanOrEqual(1.2);
  });

  it('ships are listed as the rules list them', () => {
    expect(shipWords({ bb: 1, cv: 0, ca: 2, dd: 4 })).toBe('1 battleship, 2 cruisers, 4 destroyers');
    expect(shipWords({ bb: 0, cv: 0, ca: 0, dd: 0 })).toBe('no ships');
  });
});

describe('what the 3D game and the sound read', () => {
  it('every ship lost leaves her smoke where she went, to the end of the fight; drawn only, the same every time', () => {
    const run = (seed: number) => {
      const sim = battle(seed, { human: null });
      const lost: FieldEvent[] = [];
      let zones: Zone[] = [];
      while (!sim.over) {
        sim.step();
        const v = sim.view();
        lost.push(...v.events.filter((e) => e.what === 'sea:sunk' || e.what === 'sea:burning'));
        // (a torpedo salvo's track is a zone of its own kind, from its launch to where it falls: not smoke)
        zones = v.zones.filter((z) => z.kind !== 'torpedo');
      }
      return { sim, lost, zones };
    };
    let some = 0;
    for (const seed of [4, 7, 11]) {
      const { sim, lost, zones } = run(seed);
      // One zone a ship lost, of its kind, its piece and side, from the second she went; none other.
      expect(zones.length).toBe(lost.length);
      some += zones.length;
      const F = SEA.field.size;
      lost.forEach((e, i) => {
        const z = zones[i], u = sim.units.find((x) => x.id === e.unit)!;
        expect(z).toMatchObject({ kind: e.what === 'sea:sunk' ? 'sunk' : 'burning', unit: e.unit, side: e.side, until: sim.ticket.limit, at: Math.round(e.t * 10) / 10 });
        // laid in the order the ships went (the zones' ids count the torpedo tracks laid between them too)
        if (i > 0) expect(z.id).toBeGreaterThan(zones[i - 1].id);
        expect(z.r).toBe(Math.round(SEA.classes[u.extra.cls as SeaClass].len * 0.6));
        expect(z.x >= 2 && z.x <= F - 2 && z.y >= 2 && z.y <= F - 2).toBe(true);
      });
      // The same seed lays the same smoke, and the fight comes out the same.
      const again = run(seed);
      expect(again.zones).toEqual(zones);
      expect(again.sim.over).toEqual(sim.over);
    }
    expect(some).toBeGreaterThan(0);
  });

  it('each piece shows one figure a ship afloat, its way, and what it is doing; events say where and what sounds', () => {
    const sim = battle(4, { human: null });
    const seen = new Set<string>(), cues = new Set<string>();
    let placed = true;
    while (!sim.over) {
      sim.step();
      for (const u of sim.units) {
        seen.add(u.act);
        expect(u.figures).toBe(u.extra.ships);
        if (u.act === 'fight') expect(u.engaged).toBeDefined();
      }
      for (const e of sim.view().events) {
        if (e.cue) cues.add(e.cue);
        if (e.unit !== undefined && (e.x === undefined || e.y === undefined)) placed = false;
      }
    }
    expect(placed).toBe(true);
    for (const a of ['march', 'fight']) expect(seen.has(a), a).toBe(true);
    expect(seen.has('down')).toBe(sim.units.some((u) => u.state === 'gone'));
    for (const c of ['ship_guns', 'ship_bells_two']) expect(cues.has(c), c).toBe(true);
    expect(cues.has('ship_sinking')).toBe(sim.units.some((u) => u.extra.sunk > 0));
    // Brian cut the horn (14:49Z): a fleet breaking off names no sound until a new one passes his ear.
    expect(cues.has('ship_horn')).toBe(false);
    expect(Object.values(SEA_CUES)).not.toContain('ship_horn');
    // Under way a destroyer is its own wash, every other warship its engines (the sound director, 14:59Z).
    expect([seaLoop('dd', 'march'), seaLoop('dd', 'run'), seaLoop('bb', 'march'), seaLoop('cv', 'turn'), seaLoop('dd', 'fight'), seaLoop('ca', 'idle')])
      .toEqual(['destroyer_wash_loop', 'destroyer_wash_loop', 'ship_engine_loop', 'ship_engine_loop', 'sea_battle_loop', 'sea_day_loop']);
    // A battleship's box is one hull; four destroyers in line ahead, three lengths apart.
    const bb = piece(sim, 'att', 'bb'), dd = sim.units.find((u) => u.extra.cls === 'dd' && u.extra.ships0 === 4)!;
    expect(bb.d).toBe(SEA.classes.bb.len);
    expect(dd.d).toBe(SEA.classes.dd.len * 10);
  });
});

// ---- The design director's ruling b458 -------------------------------------------------------------------------------

const ships = (o: Partial<SeaShips>): SeaShips => ({ bb: 0, cv: 0, ca: 0, dd: 0, ...o });
const live = (u: SeaUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
/** A battle fought a while, then the enemy (the defender) broken off on its admiral's order: the chase is on. */
function chase(seed = 11, o: { human?: 'att' | 'def' | null; fight?: number } = {}): SeaSim {
  const sim = battle(seed, { human: o.human === undefined ? 'att' : o.human });
  run(sim, o.fight ?? 400);
  expect(sim.over).toBeNull();
  expect(sim.command({ do: 'withdrawAll', side: 'def' }, 'def')).toBe(true);
  expect(sim.brokenSide).toBe('def');
  return sim;
}
const mineAfloat = (sim: SeaSim, side: 'att' | 'def' = 'att'): number[] => sim.units.filter((u) => u.side === side && live(u) && u.state !== 'routing').map((u) => u.id);

describe('b458, 1: stopping the chase ends the action', () => {
  const orders: [string, (sim: SeaSim) => Parameters<SeaSim['command']>[0]][] = [
    ['hold (H)', (sim) => ({ do: 'hold', units: mineAfloat(sim) })],
    ['break off (R)', (sim) => ({ do: 'withdraw', units: mineAfloat(sim) })],
    ['steer for a point (a right-click on the sea)', (sim) => ({ do: 'move', units: mineAfloat(sim), x: 10000, y: 6000 })],
    ['form a line (a right-drag on the sea)', (sim) => ({ do: 'line', units: mineAfloat(sim), x0: 8000, y0: 6000, x1: 12000, y1: 6000 })],
  ];
  for (const [name, order] of orders) {
    it(`an order to ${name} lets the beaten enemy go: the action ends at once, the side that broke off beaten`, () => {
      const sim = chase();
      run(sim, 6);
      const t = sim.t, lost = { att: sim.lostShare('att'), def: sim.lostShare('def') };
      expect(sim.command(order(sim), 'att')).toBe(true);
      const r = sim.over!;
      expect(r).not.toBeNull();
      expect(r.winner).toBe('att');
      expect(r.how).toBe('withdrew');
      expect(r.seconds).toBe(t);
      // The losses as they stand, and booked as any ending is: a win for the side that chased, within the dice.
      expect(r.lost).toEqual(lost);
      expect(r.R).toBeGreaterThanOrEqual(1);
      expect(r.level).toBe(seaLevel(r.R));
      const f = foughtOf(r);
      expect(f.R).toBe(r.R);
      expect(f.toDice).toBeUndefined();
      expect(r.lines[0]).toBe('UK ships hold the North Sea.');
      // Nothing more happens: no order is taken and a step does nothing.
      sim.step();
      expect(sim.t).toBe(t);
    });
  }

  it('WITHDRAW in the chase lets the beaten enemy go as R does: the action ends at once, the side that broke off beaten', () => {
    const sim = chase();
    run(sim, 6);
    const t = sim.t, lost = { att: sim.lostShare('att'), def: sim.lostShare('def') };
    expect(sim.command({ do: 'withdrawAll', side: 'att' }, 'att')).toBe(true);
    const r = sim.over!;
    expect(r.winner).toBe('att');
    expect(r.how).toBe('withdrew');
    expect(r.seconds).toBe(t);
    expect(r.lost).toEqual(lost);
    expect(r.lines[0]).toBe('UK ships hold the North Sea.');
    // Before any break it is the fleet breaking off, as it always was; and the side that broke off has nothing to withdraw.
    const early = battle();
    run(early, 200);
    expect(early.command({ do: 'withdrawAll', side: 'att' }, 'att')).toBe(true);
    expect(early.brokenSide).toBe('att');
    expect(early.over).toBeNull();
    expect(early.command({ do: 'withdrawAll', side: 'att' }, 'att')).toBe(false);
    expect(early.over).toBeNull();
  });

  it('the same orders before any side has broken off end nothing', () => {
    const sim = battle();
    run(sim, 200);
    for (const [, order] of orders) expect(sim.command(order(sim), 'att')).toBe(true);
    expect(sim.over).toBeNull();
    expect(sim.letGo('att')).toBe(false);
  });

  it('an order to attack the beaten enemy is a pursuit: the chase runs on until its time is up', () => {
    const sim = chase();
    const foe = sim.units.find((u) => u.side === 'def' && live(u))!;
    expect(sim.command({ do: 'attack', units: mineAfloat(sim), target: foe.id }, 'att')).toBe(true);
    expect(sim.over).toBeNull();
    run(sim, SEA.side.pursuit - 10);
    expect(sim.over).toBeNull();
    runOut(sim);
    expect(sim.over!.winner).toBe('att');
    expect(sim.over!.seconds).toBeGreaterThanOrEqual(400 + SEA.side.pursuit - 1);
  });

  it("a course among or past the beaten fleet runs it down; only a course on the chaser's own sea lets it go", () => {
    // The review of b458: a right-click on the water just past the fleeing enemy, or one that missed a fleeing hull by a
    // few pixels, ended the action at once. The ruling's let-go is a right-click on the player's own sea.
    for (const chaser of ['att', 'def'] as const) {
      const broke = chaser === 'att' ? 'def' : 'att';
      const sim = battle(11, { human: chaser });
      run(sim, 400);
      expect(sim.command({ do: 'withdrawAll', side: broke }, broke)).toBe(true);
      run(sim, 6);
      const mid = (side: 'att' | 'def'): [number, number] => {
        const us = sim.units.filter((u) => u.side === side && live(u));
        return [us.reduce((n, u) => n + u.x, 0) / us.length, us.reduce((n, u) => n + u.y, 0) / us.length];
      };
      // Points along the line from the chaser's middle through the beaten fleet's: beyond it, just past its nearest
      // ship (a missed pick), and behind the chaser.
      const points = (): Record<'past' | 'miss' | 'home', [number, number]> => {
        const [mx, my] = mid(chaser), [tx, ty] = mid(broke), k = 1 / Math.hypot(tx - mx, ty - my), ux = (tx - mx) * k, uy = (ty - my) * k;
        const near = sim.units.filter((u) => u.side === broke && live(u)).sort((a, b) => Math.hypot(a.x - mx, a.y - my) - Math.hypot(b.x - mx, b.y - my))[0];
        return { past: [tx + ux * 1500, ty + uy * 1500], miss: [near.x + ux * 150, near.y + uy * 150], home: [mx - ux * 1500, my - uy * 1500] };
      };
      const { past, miss, home } = points();
      expect(sim.ownSea(chaser, ...past), chaser).toBe(false);
      expect(sim.ownSea(chaser, ...miss), chaser).toBe(false);
      expect(sim.ownSea(chaser, ...home), chaser).toBe(true);
      const mine = (): number[] => mineAfloat(sim, chaser);
      expect(sim.command({ do: 'move', units: mine(), x: past[0], y: past[1] }, chaser)).toBe(true);
      expect(sim.over, `${chaser}: a course past the beaten fleet`).toBeNull();
      expect(sim.command({ do: 'move', units: mine(), x: miss[0], y: miss[1] }, chaser)).toBe(true);
      expect(sim.over, `${chaser}: a course just past a fleeing ship`).toBeNull();
      // A line with one end among the beaten fleet runs it down too: every point of it must be the chaser's own sea.
      expect(sim.command({ do: 'line', units: mine(), x0: home[0], y0: home[1], x1: miss[0], y1: miss[1] }, chaser)).toBe(true);
      expect(sim.over, `${chaser}: a line reaching among the beaten fleet`).toBeNull();
      // The chase runs on, the chaser's ships on the course they were given ...
      run(sim, 20);
      expect(sim.over).toBeNull();
      // ... until a course for the chaser's own sea lets the beaten fleet go, there and then.
      const back = points().home, t = sim.t;
      expect(sim.command({ do: 'move', units: mine(), x: back[0], y: back[1] }, chaser)).toBe(true);
      const r = sim.over!;
      expect(r, `${chaser}: a course for its own sea`).not.toBeNull();
      expect(r.winner).toBe(chaser);
      expect(r.how).toBe('withdrew');
      expect(r.seconds).toBe(t);
    }
  });

  it("the side that broke off cannot let anyone go, and the player's own fleet broken changes nothing", () => {
    const sim = battle(11);
    run(sim, 400);
    expect(sim.command({ do: 'withdrawAll', side: 'att' }, 'att')).toBe(true);
    expect(sim.letGo('att')).toBe(false);
    // The player's ships are all breaking off: there is no one to order, and the action runs to its end.
    expect(sim.command({ do: 'hold', units: sim.units.filter((u) => u.side === 'att').map((u) => u.id) }, 'att')).toBe(false);
    expect(sim.over).toBeNull();
    runOut(sim);
    expect(sim.over!.winner).toBe('def');
  });

  it('after a break no carrier flies a new strike unless the player sent it after the beaten enemy', () => {
    // The Home Fleet's carrier, its aircraft ready the moment the enemy breaks off.
    const flown = (o: { human: 'att' | null; pursue: boolean }): number => {
      const sim = chase(11, { human: o.human });
      events(sim);
      const cv = piece(sim, 'att', 'cv');
      for (const a of cv.abilities!) { a.ready = 0; a.winding = false; }
      if (o.pursue) {
        const foe = sim.units.find((u) => u.side === 'def' && live(u))!;
        expect(sim.command({ do: 'attack', units: mineAfloat(sim), target: foe.id }, 'att')).toBe(true);
      }
      let n = 0;
      for (let i = 0; i < 100 && !sim.over; i++) { sim.step(); n += events(sim).filter((e) => e.what === 'ability' && e.unit === cv.id).length; }
      return n;
    };
    expect(flown({ human: 'att', pursue: false }), "the player's carrier left to itself").toBe(0);
    expect(flown({ human: null, pursue: false }), "the staff's carrier").toBe(0);
    expect(flown({ human: 'att', pursue: true }), 'sent after the beaten enemy').toBe(1);
    // Before any break a ready carrier strikes as it always has.
    const sim = battle(11);
    run(sim, 400);
    const cv = piece(sim, 'att', 'cv');
    for (const a of cv.abilities!) { a.ready = 0; a.winding = false; }
    events(sim);
    run(sim, 2);
    expect(events(sim).some((e) => e.what === 'ability' && e.unit === cv.id)).toBe(true);
  });
});

describe('b458, 1 (review): a chase handed to the staff', () => {
  it('the staff give the sea to the side that chases, whoever hands it over: the dice set only by how much', () => {
    // At odds where the dice alone would give the action to the fleet that broke off: the attacker at 0.7 to 1 chasing
    // a defender that broke off at once, and the defender chasing an attacker at 1.5 to 1 that did.
    for (const [odds, broke] of [[0.7, 'def'], [1.5, 'att']] as const) for (const by of ['att', 'def'] as const) for (const seed of [3, 11, 29]) {
      const chaser = broke === 'att' ? 'def' : 'att';
      const sim = battle(seed, { odds, human: by });
      run(sim, 30);
      expect(sim.command({ do: 'withdrawAll', side: broke }, broke)).toBe(true);
      run(sim, 5);
      expect(sim.fired()).toBe(false);
      // The dice's own word at this point, before the rule: the side that broke off.
      expect(odds * sim.luck >= 1, `odds ${odds} seed ${seed}`).toBe(broke === 'att');
      // The staff's question says what they will do: they are sure of the chaser's win.
      expect(sim.handChance()).toBe(chaser === 'att' ? 1 : 0);
      expect(sim.command({ do: 'dice' }, by)).toBe(true);
      const r = sim.over!;
      expect(r.winner, `odds ${odds} handed by ${by} seed ${seed}`).toBe(chaser);
      expect(r.R >= 1).toBe(chaser === 'att');
      expect(r.level).toBe(seaLevel(r.R));
      expect(r.how).toBe('time');
      expect((r.extra as unknown as SeaLosses).toDice).toBe(true);
      expect(r.lines[0]).toBe(`${sim.ticket[chaser].adj} ships hold the North Sea.`);
      expect(foughtOf(r).R).toBe(r.R);
    }
  });

  it('before any side has broken off the staff settle it as they always have', () => {
    for (const seed of [3, 11, 29]) {
      const sim = battle(seed, { odds: 1 });
      run(sim, 30);
      expect(sim.handChance()).toBeGreaterThan(0);
      expect(sim.handChance()).toBeLessThan(1);
      expect(sim.command({ do: 'dice' }, 'att')).toBe(true);
      expect(sim.over!.R).toBeCloseTo(sim.luck, 9);
      expect(sim.over!.winner).toBe(sim.luck >= 1 ? 'att' : 'def');
    }
  });
});

describe('b458, 2a: a strike or a raid in the air does not end the fast approach', () => {
  it('quiet holds while strikes fly and raids come in, and ends the moment the guns meet, for good', () => {
    const sim = new SeaSim(plainTicket({ att: [HOME], def: [BATTLE], air: { def: 15 }, seed: 11, human: 'att' }));
    let flown = 0, landed = 0, raids = 0, met = false;
    while (!sim.over) {
      sim.step();
      for (const e of events(sim)) {
        if (sim.contactTime !== null) continue;
        if (e.what === 'ability') flown++;
        if (e.what === 'sea:strike') landed++;
        if (e.what === 'sea:air') raids++;
      }
      if (sim.contactTime !== null) met = true;
      expect(sim.quiet, `t ${sim.t}`).toBe(!met);
    }
    // Before the guns met, a strike had flown and landed and the coast's aircraft had raided: quiet held through them.
    expect(met).toBe(true);
    expect(flown).toBeGreaterThan(0);
    expect(landed).toBeGreaterThan(0);
    expect(raids).toBeGreaterThan(0);
  });

  it('a side breaking off before the guns meet ends it too: what follows is the chase, not the approach', () => {
    const sim = battle(11);
    run(sim, 20);
    expect(sim.contactTime).toBeNull();
    expect(sim.quiet).toBe(true);
    expect(sim.command({ do: 'withdrawAll', side: 'att' }, 'att')).toBe(true);
    expect(sim.quiet).toBe(false);
    run(sim, 5);
    expect(sim.quiet).toBe(false);
  });
});

describe('b458, 3: the ships each side took home badly hurt', () => {
  it('a ship afloat at half its hull or less is badly hurt; in a piece of several, the one the wear has reached', () => {
    expect(hurtAfloat(0.5, 1, 1)).toBe(1);
    expect(hurtAfloat(0.06, 1, 1)).toBe(1);
    expect(hurtAfloat(0.51, 1, 1)).toBe(0);
    expect(hurtAfloat(1, 1, 1)).toBe(0);
    expect(hurtAfloat(0.6, 4, 3)).toBe(1); // 2.4 hulls: two whole ships and one at 40%
    expect(hurtAfloat(0.7, 4, 3)).toBe(0); // 2.8 hulls: the third at 80%
    expect(hurtAfloat(0.1, 4, 1)).toBe(1);
    expect(hurtAfloat(0, 4, 0)).toBe(0);
  });

  it("the after card names them by kind, in the repair line's form, yours first", () => {
    const none = ships({});
    expect(repairLines({ att: none, def: ships({ bb: 1, ca: 1 }) }, 'att', 'Italian')).toEqual(['Two Italian ships came home for repair: 1 battleship and 1 cruiser.']);
    expect(repairLines({ att: none, def: ships({ ca: 1 }) }, 'att', 'Italian')).toEqual(['One Italian cruiser came home for repair.']);
    expect(repairLines({ att: ships({ dd: 2 }), def: none }, 'att', 'German')).toEqual(['Two of your destroyers came home for repair.']);
    expect(repairLines({ att: ships({ dd: 1 }), def: ships({ dd: 3 }) }, 'att', 'German')).toEqual(['One of your destroyers came home for repair.', 'Three German destroyers came home for repair.']);
    expect(repairLines({ att: ships({ bb: 1 }), def: ships({ ca: 1, dd: 2 }) }, 'def', 'British')).toEqual(['Three of your ships came home for repair: 1 cruiser and 2 destroyers.', 'One British battleship came home for repair.']);
    expect(repairLines({ att: none, def: none }, 'att', 'Italian')).toEqual([]);
    // One to nine open a sentence as words; from 10 the figure stands (the UI director, b476 3).
    expect([1, 2, 9, 10, 12].map(headCount)).toEqual(['One', 'Two', 'Nine', '10', '12']);
    expect(repairLines({ att: none, def: ships({ dd: 10, ca: 1 }) }, 'att', 'Italian')).toEqual(['11 Italian ships came home for repair: 1 cruiser and 10 destroyers.']);
  });

  it('every action books each fleet\'s badly hurt ships: those burning, and those afloat at half their hull or less', () => {
    for (let seed = 1; seed <= 6; seed++) {
      const sim = battle(seed, { human: null, odds: seed % 2 ? 0.9 : 1.3 });
      runOut(sim);
      const x = sim.over!.extra as unknown as SeaLosses;
      for (const f of sim.ticket.place.fleets) {
        const want = ships({ ...x.burnt[f.id] });
        for (const u of sim.units) if (u.corps === f.id && u.state !== 'gone' && u.extra.ships > 0 && u.str * u.extra.ships0 - (u.extra.ships - 1) <= 0.5 + 1e-9) want[u.extra.cls as 'bb']++;
        expect(x.hurt[f.id], `seed ${seed} fleet ${f.name}`).toEqual(want);
      }
    }
  });

  it('Light ships against battleships: a close victory over a beaten fleet names the ships it took home hurt', () => {
    // The playtester's case: the Italians broke off with a cruiser and a battleship badly hurt, and the card said only
    // what sank. Played as the staff plays it until a close victory over a fleet that took a ship home hurt.
    const p = PRACTICE.find((x) => x.name === 'Light ships against battleships')!;
    for (let seed = 1; seed <= 60; seed++) {
      const sim = new SeaSim(practiceTicket(p, seed, null));
      runOut(sim);
      const r = sim.over!, x = r.extra as unknown as SeaLosses;
      if (r.winner !== 'att' || r.level !== 'Close victory') continue;
      const hurt = { att: ships({}), def: ships({}) };
      for (const f of sim.ticket.place.fleets) for (const c of SEA_CLASSES) hurt[f.side][c] += x.hurt[f.id][c];
      // Since R2 a ship worn to half her hull leaves the line burning, so the hurt ships are those gone out of the fight
      // burning, and any still afloat at half (none, on the field, but the rule stands).
      const theirs = sim.units.filter((u) => u.side === 'def' && u.state !== 'gone' && u.extra.ships0 === 1 && (((u.extra.burnt as number | undefined) ?? 0) > 0 || (u.extra.ships > 0 && u.str <= 0.5)));
      if (!theirs.length) continue;
      const lines = repairLines(hurt, 'att', 'Italian');
      const line = lines.find((l) => l.includes('Italian'))!;
      expect(line, lines.join(' | ')).toMatch(/came home for repair/);
      for (const u of theirs) expect(line).toContain(u.extra.cls === 'bb' ? 'battleship' : u.extra.cls === 'ca' ? 'cruiser' : 'destroyer');
      return;
    }
    throw new Error('no close victory with an Italian ship taken home hurt in 60 seeds');
  });
});

describe('b458, 5: a practice field is headed by its card', () => {
  it("each practice action's ticket is titled by the card's name and dated Practice; a sea's own action keeps its sea", () => {
    for (const p of PRACTICE) {
      const t = practiceTicket(p, 7);
      expect(t.title).toBe(p.name);
      expect(t.date).toBe('Practice');
      expect(t.place.zoneName).toBe(p.sea);
      expect(t.att.human).toBe(true);
      // The same fleets and odds as the chooser's card reckons them.
      expect(t.reckoning.odds).toBeCloseTo(plainTicket({ att: p.att, def: p.def, air: p.air, seed: 1 }).reckoning.odds, 12);
    }
    expect(PRACTICE[0].name).toBe('Two battle lines');
    expect(plainTicket({ att: [HOME], def: [BATTLE], seed: 1, zoneName: 'Ionian Sea' }).title).toBe('The action in the Ionian Sea');
  });
});

describe('b458, 6: a fleet in a sentence', () => {
  it('a fleet whose name ends in "Fleet" or "Navy" takes "the"; any other stands bare (b467)', () => {
    expect(theFleet('Home Fleet')).toBe('the Home Fleet');
    expect(theFleet('1st Fleet')).toBe('the 1st Fleet');
    expect(theFleet('Mediterranean Fleet')).toBe('the Mediterranean Fleet');
    expect(theFleet('Force H')).toBe('Force H');
    expect(theFleet('Striking Force')).toBe('Striking Force');
    expect(theFleet('Polish Navy')).toBe('the Polish Navy');
    expect(theFleet('Royal Norwegian Navy')).toBe('the Royal Norwegian Navy');
    expect(ofFleet('Polish Navy')).toBe('of the Polish Navy');
    expect(ofFleet('Force H')).toBe('of Force H');
    expect(ofFleet('Battle Fleet')).toBe('of the Battle Fleet');
  });

  it('every dispatch names Force H bare and the 1st Fleet with "the", and never a flotilla or a squadron', () => {
    const p = PRACTICE.find((x) => x.name === 'Light ships against battleships')!;
    const said: string[] = [];
    for (const seed of [1, 2, 3]) {
      const sim = new SeaSim(practiceTicket(p, seed, null));
      while (!sim.over) { sim.step(); for (const e of sim.view().events) if (e.text) said.push(e.text); }
    }
    const all = said.join('\n');
    expect(all).not.toMatch(/the Force H/);
    expect(all).toMatch(/of Force H/);
    expect(all).toMatch(/of the 1st Fleet/);
    expect(all).not.toMatch(/flotilla|squadron/i);
  });
});


// ---- The ship plan's R6: ships comb the tracks (the design director, 05:17Z) -----------------------------------------

describe("ships comb the tracks (the ship plan's R6; the design director, 05:17Z)", () => {
  const CVS: PlainFleet = { name: 'Striking Force', nat: 'JAP', ships: { cv: 2, ca: 2, dd: 8 } };
  const all = (s: any, side: 'att' | 'def', cls: string): SeaUnit[] => s.units.filter((u: SeaUnit) => u.side === side && u.extra.cls === cls);
  const only = (s: any, keep: [SeaUnit, number, number, number?][]): void => {
    for (const u of s.units as SeaUnit[]) if (!keep.some(([k]) => k === u)) u.state = 'gone';
    for (const [u, x, y, face] of keep) { u.x = x; u.y = y; if (face !== undefined) u.face = face; }
  };
  const TAU = 2 * Math.PI;
  const norm = (a: number): number => ((a % TAU) + TAU) % TAU;
  /** The signed turn from a to b, + to starboard. */
  const off = (a: number, b: number): number => Math.atan2(Math.sin(b - a), Math.cos(b - a));
  /** Where a salvo launched now at a ship steaming due north (face 0) at cruising speed aims: torp.travel seconds ahead. */
  const aimY = (u: SeaUnit, y: number): number => y + Math.round(u.extra.way * SEA.torp.travel);
  const combsOf = (s: any): FieldEvent[] => s.view().events.filter((e: FieldEvent) => e.what === 'sea:comb');
  /** Samar on the record ticket (tools/sea-record/record.ts:59-67, as its ticketOf builds it), at a build step; with each
   *  cruiser's longest hold of a salvo fired at her, in seconds. */
  const samar = (seed: number, step: number): { s: any; told: FieldEvent[]; T: Record<string, number>; caOwn: number } => {
    const t = plainTicket({ att: [{ name: 'Centre Force', nat: 'JAP', ships: { bb: 4, ca: 8, dd: 11 }, admiral: 2 }], def: [{ name: 'Taffy 3', nat: 'USA', ships: { cv: 6, dd: 7 }, deck: 0.35, bombers: 6 * 12, admiral: 3 }], seed, human: null, zoneName: 'Philippine Sea', title: 'The action off Samar' });
    t.att = { ...t.att, adj: 'Japanese' }; t.def = { ...t.def, adj: 'American' }; t.date = 'October 1944';
    const s = new SeaSim(t) as any;
    s.r6Step = step;
    const told: FieldEvent[] = [], held = new Map<string, number>();
    while (!s.over) {
      s.step();
      told.push(...s.view().events.filter((e: FieldEvent) => e.what === 'sea:comb'));
      for (const [id, tp] of s.combs as Map<number, any>) if (tp.target === id && s.byId.get(id).extra.cls === 'ca') held.set(`${id}:${tp.t0}`, (held.get(`${id}:${tp.t0}`) ?? 0) + DT);
    }
    return { s, told, T: s.tally as Record<string, number>, caOwn: Math.max(0, ...held.values()) };
  };

  it('1. she turns away, her stern to the tracks on their bearing, the shorter way round; no enemy sets her course; forward of her beam away is neither toward nor the nearer end', () => {
    // Abeam to port; then forward of her port beam, where toward and the nearer end both turn her to port.
    for (const [lx, ly] of [[6000, 10000], [7000, 13000]]) {
      const s = battle(11, { human: null }) as any;
      const bb = all(s, 'att', 'bb')[0], dd = all(s, 'def', 'dd')[0];
      only(s, [[bb, 10000, 10000, 0], [dd, lx, ly]]);
      s.contactAt = 0; // the guns have met: left to herself she would steer by the destroyers (engage)
      s.launch(dd, bb, null);
      const tp = s.torps[0];
      expect(tp.dir).toBe(Math.atan2(tp.x - lx, tp.y - ly)); // the tracks' bearing, launcher to aim point
      const away = off(0, tp.dir), toward = off(0, tp.dir + Math.PI);
      expect(away).toBeGreaterThan(0);
      expect(toward).toBeLessThan(0);
      if (ly > 10000) expect(Math.abs(toward)).toBeLessThan(Math.abs(away)); // the nearer end would be toward
      s.step();
      expect(s.combs.get(bb.id)).toBe(tp);
      expect(bb.extra.foe).toBe(-1);
      expect(bb.extra.helm as number).toBeGreaterThan(0); // away, to starboard: the shorter way onto it
      run(s, 55);
      expect(Math.abs(off(bb.face, tp.dir))).toBeLessThan(0.05);
      expect(s.tally['att:combs']).toBe(1);
      expect(s.tally['att:combs>bb']).toBe(1);
      expect(s.tally['att:comb:s']).toBeGreaterThan(50);
      // The combLost mark (measured only, for splitting a miss): her fire at her comb's angle against her fire at the
      // angle of the course her helm would have steered without it, every other term the same. Her stern to the tracks
      // and the launcher in her reach, she loses fire to the comb, and the mark counts it as lost, not gained.
      const bears = (h: number): number => SEA.fire.endOn + (1 - SEA.fire.endOn) * Math.abs(Math.sin(off(h, Math.atan2(dd.x - bb.x, dd.y - bb.y))));
      const lost0 = s.tally['att:combLost'];
      s.step();
      const lost = SEA.fire.rate * s.output(bb) * s.reachFactor(SEA.classes.bb, Math.hypot(dd.x - bb.x, dd.y - bb.y)) * (bears(s.combBare[bb.id]) - bears(bb.face)) * s.day('att') * DT;
      expect(bb.engaged).toBe(dd.id);
      expect(lost).toBeGreaterThan(0);
      expect(Math.abs(s.tally['att:combLost'] - lost0 - lost)).toBeLessThan(0.01 * lost);
    }
  });

  it('2. the tracks dead at her bow, she turns to starboard; the field turned half round, the same turn tick for tick', () => {
    const play = (half: boolean): { faces: number[]; helm: number; dir: number } => {
      const s = battle(11, { human: null }) as any;
      const bb = all(s, 'att', 'bb')[0], dd = all(s, 'def', 'dd')[0];
      only(s, [[bb, 10000, 10000, half ? Math.PI : 0], [dd, 10000, half ? 6000 : 14000]]);
      s.launch(dd, bb, null);
      const dir = s.torps[0].dir, faces: number[] = [];
      s.step();
      const helm = bb.extra.helm as number;
      faces.push(bb.face);
      for (let i = 0; i < 60; i++) { s.step(); faces.push(bb.face); }
      return { faces, helm, dir };
    };
    const a = play(false), b = play(true);
    // Dead at her bow: the comb course lies exactly a half turn off, +pi one way round and -pi the other.
    expect(off(0, a.dir)).toBe(Math.PI);
    expect(b.dir - Math.PI).toBeCloseTo(-Math.PI, 12);
    expect(a.helm).toBeGreaterThan(0);
    expect(b.helm).toBeGreaterThan(0);
    a.faces.forEach((f, i) => expect(Math.abs(off(f + Math.PI, b.faces[i])), `${i}`).toBeLessThan(1e-9));
  });

  it('3. not in the tick a salvo is launched, but from the next: ships listed before the launcher and after it start together', () => {
    const s = battle(11, { human: null }) as any;
    const [b1, b2] = all(s, 'att', 'bb'), [d1, d2] = all(s, 'def', 'dd');
    only(s, [[b1, 7000, 10000, 0], [b2, 13000, 10000, 0], [d1, 7000, 12000], [d2, 13000, 12000]]);
    // b1 steers before its launcher, b2 after its own.
    s.units = [b1, d1, d2, b2, ...s.units.filter((u: SeaUnit) => ![b1, b2, d1, d2].includes(u))];
    for (const [d, b] of [[d1, b1], [d2, b2]] as const) { s.bank[d.id] = salvo(s, d); s.beginRun(d, b); }
    s.step(); // each launches on its run, in its own steer
    expect(s.torps.map((t: any) => [t.target, t.t0])).toEqual([[b1.id, s.t], [b2.id, s.t]]);
    expect([s.combs.has(b1.id), s.combs.has(b2.id)]).toEqual([false, false]);
    expect(combsOf(s)).toEqual([]);
    s.step();
    expect([s.combs.has(b1.id), s.combs.has(b2.id)]).toEqual([true, true]);
    expect(combsOf(s).map((e) => e.unit).sort()).toEqual([b1.id, b2.id].sort());
  });

  it("4. she holds her comb until the salvo lands, then works back to her place in her line (R6 item 2; the design director, 19:03Z) and keeps the line's course from there", () => {
    const s = battle(5, { human: null }) as any;
    const [a1, a2] = all(s, 'att', 'bb'), [e1] = all(s, 'def', 'bb'), dd = all(s, 'def', 'dd')[0];
    // Two lines out of each other's reach, the guns having met.
    only(s, [[a1, 9800, 5000, 0], [a2, 10200, 5000, 0], [e1, 10000, 16000, Math.PI], [dd, 9800, 8000]]);
    s.contactAt = 0;
    s.step();
    expect(s.linePlans.att.members).toContain(a1);
    s.bank[dd.id] = 0.02;
    s.launch(dd, a1, null);
    dd.state = 'gone'; // the salvo runs on without its launcher
    const tp = s.torps[0];
    while (s.t + DT < tp.at - 1e-9) { s.step(); expect(s.combs.get(a1.id), `${s.t}`).toBe(tp); }
    s.step();
    expect(s.t).toBeGreaterThanOrEqual(tp.at - 1e-9);
    expect(s.combs.has(a1.id)).toBe(false);
    expect(Math.abs(off(a1.face, s.lines.att.course))).toBeGreaterThan(0.5); // still on the tracks' bearing as it lands
    expect(s.regaining.has(a1.id)).toBe(true);
    let back = -1;
    while (!s.over && s.t < tp.at + 400 && back < 0) { s.step(); if (!s.place.has(a1.id)) back = s.t; }
    expect(back).toBeGreaterThan(tp.at);
    expect(s.tally['att:regained']).toBe(1);
    run(s, 30);
    expect(s.linePlans.att.members).toContain(a1);
    expect(Math.abs(off(a1.face, s.lines.att.course))).toBeLessThan(0.15);
  });

  it("5. the course is the tracks' bearing from the launcher's place at the launch, not from where it is now", () => {
    const s = battle(11, { human: null }) as any;
    const bb = all(s, 'att', 'bb')[0], dd = all(s, 'def', 'dd')[0];
    only(s, [[bb, 10000, 10000, 0], [dd, 7000, 10000]]);
    s.launch(dd, bb, null);
    const tp = s.torps[0], then = Math.atan2(tp.x - 7000, tp.y - 10000);
    dd.x = 10000; dd.y = 14000; // turned away and gone on since
    s.step();
    const [e] = combsOf(s);
    expect(Math.abs(e.extra!.course as number - Math.round(norm(then) * 100) / 100)).toBeLessThan(1e-9);
    expect(Math.abs(off(Math.atan2(tp.x - dd.x, tp.y - dd.y), then))).toBeGreaterThan(1);
  });

  it('6. two salvoes: the first until it lands, then the second, then none; two launched in the same tick: the smaller turn, then the one further to starboard, never by id or launch order', () => {
    const s = battle(11, { human: null }) as any;
    const bb = all(s, 'att', 'bb')[0], [d1, d2] = all(s, 'def', 'dd');
    only(s, [[bb, 10000, 10000, 0], [d1, 6000, 10000], [d2, 14000, 10000]]);
    s.bank[d1.id] = 0.01; s.launch(d1, bb, null);
    run(s, 30);
    s.bank[d2.id] = 0.01; s.launch(d2, bb, null);
    const [t1, t2] = s.torps;
    expect(Math.abs(off(t1.dir, t2.dir))).toBeGreaterThan(1); // from two bearings
    let n1 = 0, n2 = 0;
    while (s.t < t2.at + 2) {
      s.step();
      const c = s.combs.get(bb.id) ?? null;
      expect(c, `${s.t}`).toBe(s.t < t1.at - 1e-9 ? t1 : s.t < t2.at - 1e-9 ? t2 : null);
      if (c === t1) n1++; else if (c === t2) n2++;
    }
    expect(n1).toBeGreaterThan(0);
    expect(n2).toBeGreaterThan(0);
    // Two running at her as she comes free to comb (held, then let go): the one launched first, though the later needs
    // the smaller turn.
    const f = battle(11) as any;
    const fb = all(f, 'att', 'bb')[0], [f1, f2] = all(f, 'def', 'dd'), fy = aimY(fb, 10000);
    only(f, [[fb, 10000, 10000, 0], [f1, 13000, fy + 1000], [f2, 7000, fy - 2000]]);
    f.command({ do: 'hold', units: [fb.id] }, 'att');
    f.bank[f1.id] = 0.01; f.launch(f1, fb, null);
    f.step();
    f.bank[f2.id] = 0.01; f.launch(f2, fb, null);
    f.step();
    expect(f.combs.has(fb.id)).toBe(false);
    f.orders.delete(fb.id); // the hold let go: she fights at will
    f.step();
    expect(f.combs.get(fb.id).from).toBe(f1.id);
    // Launched in the same tick.
    const pick = (a: [number, number], b: [number, number], order: 0 | 1): { pick: number; dirs: number[]; tie: number } => {
      const t = battle(11, { human: null }) as any;
      const b0 = all(t, 'att', 'bb')[0], [e1, e2] = all(t, 'def', 'dd'), y = aimY(b0, 10000);
      only(t, [[b0, 10000, 10000, 0], [e1, a[0], y + a[1]], [e2, b[0], y + b[1]]]);
      for (const e of order ? [e2, e1] : [e1, e2]) { t.bank[e.id] = 0.01; t.launch(e, b0, null); }
      t.step();
      const c = t.combs.get(b0.id);
      return { pick: c.from === e1.id ? 0 : 1, dirs: t.torps.map((x: any) => x.dir), tie: t.tally['att:combTie'] ?? 0 };
    };
    for (const order of [0, 1] as const) {
      // The smaller turn: about 56 degrees to starboard before about 108 to port.
      const small = pick([7000, -2000], [13000, 1000], order);
      expect(small.pick).toBe(0);
      expect(small.tie).toBe(1);
      // The same turn each way: to starboard.
      const even = pick([7000, -2000], [13000, -2000], order);
      expect(even.dirs[0]).toBe(-even.dirs[1]);
      expect(even.pick).toBe(0);
      // The same two launchers the other way round, so the starboard one has the other id.
      const swap = pick([13000, -2000], [7000, -2000], order);
      expect(swap.pick).toBe(1);
    }
    // Two on the same hand whose turns differ by less than the tolerance: the one further to starboard, whichever was
    // launched first (the launchers' steer order).
    const near = (first: 0 | 1): { from: number; ids: number[]; dirs: number[] } => {
      const t = battle(11, { human: null }) as any;
      const b0 = all(t, 'att', 'bb')[0], [e1, e2] = all(t, 'def', 'dd');
      only(t, [[b0, 10000, 10000, 0], [e1, 7000, 12000], [e2, 7000.002, 12000]]);
      for (const e of first ? [e2, e1] : [e1, e2]) { t.orders.set(e.id, { kind: 'hold' }); t.bank[e.id] = 0.01; t.launch(e, b0, null); }
      t.step();
      return { from: t.combs.get(b0.id).from, ids: [e1.id, e2.id], dirs: t.torps.map((x: any) => x.dir) };
    };
    const k0 = near(0), k1 = near(1);
    expect(Math.abs(k0.dirs[0] - k0.dirs[1])).toBeGreaterThan(0);
    expect(Math.abs(k0.dirs[0] - k0.dirs[1])).toBeLessThan(1e-6);
    expect(k0.from).toBe(k0.ids[1]); // e2's tracks lie the further to starboard (her heading is 0)
    expect(k1.from).toBe(k0.from);
  });

  it('7. a salvo with no weight in it turns no ship', () => {
    const s = battle(11, { human: null }) as any;
    const bb = all(s, 'att', 'bb')[0], dd = all(s, 'def', 'dd')[0];
    only(s, [[bb, 10000, 10000, 0], [dd, 6000, 10000]]);
    s.bank[dd.id] = 0;
    s.launch(dd, bb, null);
    expect(s.torps[0].weight).toBe(0);
    for (let i = 0; i < 20; i++) { s.step(); expect(s.combs.has(bb.id)).toBe(false); }
    expect(combsOf(s)).toEqual([]);
  });

  it('8. the hit is unchanged: the salvo lands on the ship it was fired at, torp.travel after the launch, with its whole weight, though she has turned off the aim point', () => {
    const s = battle(11, { human: null }) as any;
    const bb = all(s, 'att', 'bb')[0], dd = all(s, 'def', 'dd')[0];
    only(s, [[bb, 10000, 10000, 0], [dd, 4000, 10000]]); // beyond the guns' reach of each other
    expect(s.command({ do: 'hold', units: [dd.id] }, 'def')).toBe(true);
    s.bank[dd.id] = 0.05; s.loaded[dd.id] = 0;
    s.launch(dd, bb, null);
    const tp = s.torps[0], lost = s.lostShare('att'), dealt = s.dealt[dd.id];
    let hit: FieldEvent | undefined;
    while (!hit && s.t < tp.at + 1) { s.step(); hit = s.view().events.find((e: FieldEvent) => e.what === 'sea:torpedoHit'); if (!hit) expect(s.combs.get(bb.id)).toBe(tp); }
    expect(hit!.unit).toBe(bb.id);
    expect(hit!.t).toBeGreaterThanOrEqual(tp.at - 1e-9);
    expect(hit!.t).toBeLessThan(tp.at + DT);
    expect(s.dealt[dd.id] - dealt).toBeCloseTo(0.05, 12);
    expect(s.tally['def:torp>bb']).toBeCloseTo(0.05, 12);
    expect(s.lostShare('att')).toBeGreaterThan(lost);
    expect(Math.hypot(bb.x - tp.x, bb.y - tp.y)).toBeGreaterThan(500); // she is well off where it was aimed
  });

  it('9. an order to steer, hold or withdraw, or breaking off, and she combs nothing; at will, under a fleet order or her own attack order, or at a speed of her own she combs; an order given and then superseded while it runs, and she combs it again', () => {
    const set = (give: (s: any, bb: SeaUnit, ca: SeaUnit, dd: SeaUnit) => void): boolean => {
      const s = battle(11) as any; // the player's side is the attacker
      const bb = all(s, 'att', 'bb')[0], ca = all(s, 'att', 'ca')[0], dd = all(s, 'def', 'dd')[0];
      only(s, [[bb, 10000, 10000, 0], [ca, 10500, 10000, 0], [dd, 6000, 10000]]);
      s.bank[dd.id] = 0.01;
      s.launch(dd, bb, null);
      give(s, bb, ca, dd);
      s.step();
      return s.combs.has(bb.id);
    };
    expect(set((s, bb) => s.command({ do: 'move', units: [bb.id], x: 10000, y: 15000 }, 'att'))).toBe(false);
    expect(set((s, bb) => s.command({ do: 'hold', units: [bb.id] }, 'att'))).toBe(false);
    expect(set((s, bb) => s.command({ do: 'withdraw', units: [bb.id] }, 'att'))).toBe(false);
    expect(set((_, bb) => { bb.state = 'routing'; })).toBe(false);
    expect(set(() => undefined)).toBe(true);
    expect(set((s, bb, ca, dd) => s.command({ do: 'attack', units: [bb.id, ca.id], target: dd.id }, 'att'))).toBe(true);
    expect(set((s, bb, _, dd) => s.command({ do: 'attack', units: [bb.id], target: dd.id }, 'att'))).toBe(true);
    expect(set((s, bb) => s.command({ do: 'sea:speed', units: [bb.id], speed: 'flank' }, 'att'))).toBe(true);
    // Held, then sent at the enemy with her consort while the salvo still runs: she combs it again.
    const s = battle(11) as any;
    const bb = all(s, 'att', 'bb')[0], ca = all(s, 'att', 'ca')[0], dd = all(s, 'def', 'dd')[0];
    only(s, [[bb, 10000, 10000, 0], [ca, 10500, 10000, 0], [dd, 6000, 10000]]);
    s.bank[dd.id] = 0.01;
    s.launch(dd, bb, null);
    s.step();
    expect(s.combs.has(bb.id)).toBe(true);
    s.command({ do: 'hold', units: [bb.id] }, 'att');
    run(s, 5);
    expect(s.combs.has(bb.id)).toBe(false);
    s.view();
    s.command({ do: 'attack', units: [bb.id, ca.id], target: dd.id }, 'att');
    s.step();
    expect(s.combs.get(bb.id)).toBe(s.torps[0]);
    expect(combsOf(s).map((e) => e.unit)).toEqual([bb.id]);
    // The same salvo taken up again is not a new comb (the marks count salvoes combed): it is counted as a resume.
    expect(s.tally['att:combs']).toBe(1);
    expect(s.tally['att:combResume']).toBe(1);
  });

  it("10. the torpedo water (step 2): a ship of hers within torp.radius of the aim point combs the salvo, one just outside does not, one that steams in starts then, and one that steams out holds it until it lands", () => {
    const s = battle(11, { human: null }) as any;
    s.r6Step = 2; // the build's step 2 (Q2): the water
    const [b1, b2] = all(s, 'att', 'bb'), [c1, c2, c3] = all(s, 'att', 'ca'), dd = all(s, 'def', 'dd')[0];
    const R = SEA.torp.radius, ay = aimY(b1, 10000);
    only(s, [[b1, 10000, 10000, 0], [b2, 10000 + R - 100, ay, 0], [c1, 10000 + R + 100, ay, 0], [c2, 10000, ay + R + 5, Math.PI], [c3, 10000 - R + 5, ay, 1.5 * Math.PI], [dd, 10000, 7000]]);
    s.bank[dd.id] = 0.01;
    s.launch(dd, b1, null);
    const tp = s.torps[0];
    expect([tp.x, tp.y]).toEqual([10000, ay]);
    s.step();
    const now = (): boolean[] => [b1, b2, c1, c2, c3].map((u) => s.combs.get(u.id) === tp);
    expect(now()).toEqual([true, true, false, false, true]);
    const ev = combsOf(s);
    expect(ev.find((e) => e.unit === b2.id)!.extra!.fired).toBe(b1.id); // fired at her line mate
    expect(s.tally['att:combWater']).toBe(2);
    // The cruiser steaming at the aim point comes into the water and starts then.
    let inAt = -1;
    while (s.t < tp.at - 1 && inAt < 0) { s.step(); if (s.combs.get(c2.id) === tp) inAt = s.t; }
    expect(inAt).toBeGreaterThan(0);
    expect(inAt).toBeLessThan(3);
    // The one steaming out of it holds her comb to the end, out of the water.
    while (s.t + DT < tp.at - 1e-9) { s.step(); expect(s.combs.get(c3.id)).toBe(tp); }
    expect(Math.hypot(c3.x - tp.x, c3.y - tp.y)).toBeGreaterThan(R);
  });

  it("11. a combing carrier keeps her navy's spacing from a sister (apart) in every navy, at a speed of her own too, as under air attack (Q3)", () => {
    const TF = (nat: string): PlainFleet => ({ name: 'Task Force', nat, ships: { bb: 1, cv: 2, ca: 1, dd: 4 } });
    const set = (nat: string, research?: string[]): { s: any; a: SeaUnit; b: SeaUnit; tp: any } => {
      const s = new SeaSim(plainTicket({ att: [TF(nat)], def: [BATTLE], seed: 5, human: 'att', research: research && { att: research } })) as any;
      const [a, b] = all(s, 'att', 'cv').sort((x, y) => x.id - y.id), bb = all(s, 'att', 'bb')[0], dd = all(s, 'def', 'dd')[0];
      const R = s.carrierWay('att').spacing, y = aimY(a, 10000);
      // Her sister due east inside the spacing; the tracks' bearing north-east, closing on her.
      only(s, [[a, 6000, 10000, 0], [b, 6000 + R - 100, 10000, 0], [bb, 3000, 4000, 0], [dd, 3000, y - 3000]]);
      s.bank[dd.id] = 0.01;
      s.launch(dd, a, null);
      return { s, a, b, tp: s.torps[0] };
    };
    const N = 0, NE = Math.PI / 4;
    // Each for herself (Japan): bound as ever, and at a speed of her own too.
    const j = set('JAP');
    expect(j.tp.dir).toBeCloseTo(NE, 12);
    expect(norm(j.s.combCourse(j.a, j.tp))).toBeCloseTo(N, 9);
    j.s.command({ do: 'sea:speed', units: [j.a.id], speed: 'flank' }, 'att');
    expect(norm(j.s.apart(j.a, NE))).toBeCloseTo(NE, 9); // not combing, she keeps no line at a speed of her own
    expect(norm(j.s.combCourse(j.a, j.tp))).toBeCloseTo(N, 9); // combing, she does
    // On a guide (American, Carrier task forces), not under air attack: bound while she combs; and under air attack.
    const g = set('USA', ['taskforces']);
    expect(g.s.carrierWay('att').form).toBe('guide');
    expect(norm(g.s.apart(g.a, NE))).toBeCloseTo(NE, 9);
    expect(norm(g.s.combCourse(g.a, g.tp))).toBeCloseTo(N, 9);
    g.s.strikes = [{ at: g.s.t + SEA.strike.windup, side: 'def', target: g.a.id, weight: 0, from: '', count: 0, cv: 0 }];
    expect(norm(g.s.combCourse(g.a, g.tp))).toBeCloseTo(N, 9);
    // Her sister beyond the spacing: the tracks' bearing.
    g.b.x = 6000 + g.s.carrierWay('att').spacing + 100;
    expect(norm(g.s.combCourse(g.a, g.tp))).toBeCloseTo(NE, 9);
    // In the field: she takes the salvo the next tick on the course square to her sister.
    j.s.step();
    const [e] = combsOf(j.s);
    expect(e.unit).toBe(j.a.id);
    expect(e.extra!.course).toBe(0);
    // R3's mark (cvApart:s) counts each carrier's own helm once a tick, as with no comb; the comb's spacing is R6's own
    // mark (combApart:s), so the knockouts' battle reads R3's mark as with no comb.
    const marks = (w: number): [number, number] => {
      const k = set('JAP');
      k.s.torps[0].weight = w;
      for (let i = 0; i < 10; i++) k.s.step();
      return [k.s.tally['att:cvApart:s'] ?? 0, k.s.tally['att:combApart:s'] ?? 0];
    };
    const [withComb, combApart] = marks(0.01), [without, none] = marks(0);
    expect(withComb).toBeCloseTo(without, 9);
    expect(withComb).toBeCloseTo(20 * DT, 9); // two carriers, ten ticks
    expect(combApart).toBeCloseTo(10 * DT, 9);
    expect(none).toBe(0);
  });

  it("12. within 800 m of the field's edge, the edge wins: she holds the salvo, but the event tells the course she steers, and back inside it the comb steers her again", () => {
    const s = battle(11, { human: null }) as any;
    const bb = all(s, 'att', 'bb')[0], dd = all(s, 'def', 'dd')[0];
    const y = aimY(bb, 10000);
    only(s, [[bb, 500, 10000, 0], [dd, 3500, y]]); // the tracks run due west, off the field
    s.launch(dd, bb, null);
    expect(off(0, s.torps[0].dir)).toBeCloseTo(-Math.PI / 2, 9);
    s.step();
    expect(s.combs.has(bb.id)).toBe(true);
    expect(bb.extra.helm as number).toBeGreaterThan(0); // back toward the middle of the field, to starboard
    expect(bb.extra.foe).toBe(-1);
    // Held, not steering her: the event tells the course she steers (the edge's, toward the middle of the field), not
    // the tracks' bearing off the field, and the crossed-T guard does not take her (test 13).
    const F = SEA.field.size, [e] = combsOf(s);
    expect(e.extra!.course).toBe(Math.round(norm(Math.atan2(F / 2 - 500, F / 2 - 10000)) * 100) / 100);
    expect(s.combSteered.has(bb.id)).toBe(false);
    // Back inside the edge's 800 m, the comb steers her again (no new event: the same salvo).
    bb.x = 1500;
    s.step();
    expect(s.combSteered.has(bb.id)).toBe(true);
    expect(combsOf(s)).toEqual([]);
    expect(Math.abs(off(norm(s.combTo[bb.id]), norm(s.torps[0].dir)))).toBeLessThan(1e-9);
  });

  it("13. a combing guide neither trips nor clears her line's crossed-T test: it reads the line's course while her comb steers her, and her own heading where the field's edge does", () => {
    const t = battle(5, { human: null }) as any;
    const [b1, b2] = all(t, 'att', 'bb'), [e1, e2] = all(t, 'def', 'bb'), dd = all(t, 'def', 'dd')[0];
    only(t, [[b1, 10000, 8000, 0], [b2, 10000, 7600, 0], [e1, 10500, 11000, Math.PI / 2], [e2, 10000, 11000, Math.PI / 2], [dd, 7000, 8000]]);
    t.contactAt = 0;
    const lines = (): void => {
      t.lines.att = { order: [], hand: -1, course: 0, speed: 11, turned: -1e9 };
      t.lines.def = { order: [], hand: -1, course: Math.PI / 2, speed: 11, turned: -1e9 };
      t.linePlans.att = { t: t.t, members: [b1, b2] };
      t.linePlans.def = { t: t.t, members: [e1, e2] };
    };
    lines();
    expect(t.crossed('att')).toBe(1);
    // A salvo at the guide, taken up the second after.
    t.launch(dd, b1, null);
    t.t = Math.round((t.t + DT) * 1000) / 1000;
    t.steer(b1);
    expect(t.combs.has(b1.id)).toBe(true);
    Object.assign(b1, { x: 10000, y: 8000 });
    lines();
    // Turned off the enemy by her comb, the line's course still runs at it: crossed still.
    b1.face = Math.PI / 2;
    expect(t.crossed('att')).toBe(1);
    // The line's course square off the enemy, her comb heading at it: not crossed.
    t.lines.att.course = Math.PI / 2; b1.face = 0;
    expect(t.crossed('att')).toBe(0);
    // With no comb her own heading is read, as before R6.
    t.combs.delete(b1.id); t.combSteered.delete(b1.id);
    expect(t.crossed('att')).toBe(1);
    // Inside 800 m of the field's edge the edge steers her though she holds the salvo: her own heading is read.
    const u = battle(5, { human: null }) as any;
    const [g1, g2] = all(u, 'att', 'bb'), [f1, f2] = all(u, 'def', 'bb'), ud = all(u, 'def', 'dd')[0], X = 400;
    only(u, [[g1, X, 8000, 0], [g2, X, 7600, 0], [f1, X + 500, 11000, Math.PI / 2], [f2, X, 11000, Math.PI / 2], [ud, X + 3000, 8000]]);
    u.contactAt = 0;
    u.launch(ud, g1, null);
    u.t = Math.round((u.t + DT) * 1000) / 1000;
    u.steer(g1);
    expect(u.combs.has(g1.id)).toBe(true);
    expect(u.combSteered.has(g1.id)).toBe(false);
    Object.assign(g1, { x: X, y: 8000, face: Math.PI / 2 }); // on the edge's course, east
    u.lines.att = { order: [], hand: -1, course: 0, speed: 11, turned: -1e9 };
    u.lines.def = { order: [], hand: -1, course: Math.PI / 2, speed: 11, turned: -1e9 };
    u.linePlans.att = { t: u.t, members: [g1, g2] };
    u.linePlans.def = { t: u.t, members: [f1, f2] };
    expect(u.crossed('att')).toBe(0);
  });

  it("14. the build's steps (Q2, a build stage only): a line mate in the water holds her course, and a carrier group's cruiser with a salvo at her holds hers (step 2 too), taken once a tick whatever the steer order; Samar at steps 1 and 2; the rule is whole behind the switch", () => {
    // A build stage only (05:17Z, Q2(c)): the switch is a private readonly 1 in the code and set by nothing, so no datum,
    // ticket, flag or look field can move it; the two holds only compare it with a step (tests set it on an instance).
    const code = simText.split('\n').filter((l) => /r6Step/.test(l) && !/^\s*(\*|\/\*\*|\/\/)/.test(l)).map((l) => l.trim());
    expect(code.filter((l) => /\br6Step\b\s*:/.test(l))).toEqual(['private readonly r6Step: number = 1;']);
    const uses = [...code.join('\n').matchAll(/\br6Step\b(.{0,5})/g)].map((m) => m[1]);
    expect(uses).toHaveLength(3);
    expect(uses.filter((m) => !/^ (<|>=) \d\b/.test(m))).toEqual([': num']);
    const s = battle(11, { human: null }) as any;
    expect(s.r6Step).toBe(1);
    const [b1, b2] = all(s, 'att', 'bb'), dd = all(s, 'def', 'dd')[0];
    only(s, [[b1, 10000, 10000, 0], [b2, 10000 + SEA.torp.radius - 100, aimY(b1, 10000), 0], [dd, 10000, 7000]]);
    s.bank[dd.id] = 0.01;
    s.launch(dd, b1, null);
    s.step();
    expect([s.combs.has(b1.id), s.combs.has(b2.id)]).toEqual([true, false]);
    for (const step of [1, 2, 3]) {
      const g = new SeaSim(plainTicket({ att: [CVS], def: [BATTLE], seed: 5, human: null })) as any;
      g.r6Step = step;
      const ca = all(g, 'att', 'ca')[0], cv = all(g, 'att', 'cv')[0], e = all(g, 'def', 'dd')[0];
      only(g, [[ca, 10000, 10000, 0], [cv, 10000, 6000, 0], [e, 6000, 10000]]);
      g.bank[e.id] = 0.01;
      g.launch(e, ca, null);
      g.step();
      expect(g.combs.has(ca.id), `step ${step}`).toBe(step >= 3);
    }
    // The hold reads R1's carrier-group test as R1 does, once at the start of the tick: the last battleship leaving the
    // field by her own edge in the middle of the steer holds every cruiser's comb from the next tick, whether the cruiser
    // steers before her or after her.
    const MIX: PlainFleet = { name: 'Mixed', nat: 'USA', ships: { bb: 1, cv: 1, ca: 2, dd: 4 } };
    const RAID: PlainFleet = { name: 'Raiders', nat: 'JAP', ships: { ca: 1, dd: 8 } };
    const leaving = (caFirst: boolean): { faces: number[]; combs: boolean[][]; out: number } => {
      const w = new SeaSim(plainTicket({ att: [MIX], def: [RAID], seed: 11, human: null })) as any;
      const bb = all(w, 'att', 'bb')[0], cv = all(w, 'att', 'cv')[0], [c1, c2] = all(w, 'att', 'ca'), [d1, d2] = all(w, 'def', 'dd');
      only(w, [[bb, 4000, 320, Math.PI], [cv, 15000, 6000, 0], [c1, 8000, 8000, 0], [c2, 12000, 8000, 0], [d1, 11000, 8000], [d2, 15000, 8000]]);
      w.orders.set(bb.id, { kind: 'leave' });
      const rest = w.units.filter((u: SeaUnit) => ![bb, cv, c1, c2].includes(u));
      w.units = caFirst ? [c1, bb, cv, c2, ...rest] : [bb, c1, cv, c2, ...rest];
      w.bank[d1.id] = 0.01; w.launch(d1, c1, null);
      w.bank[d2.id] = 0.01; w.launch(d2, c2, null);
      w.step();
      const faces: number[] = [], combs: boolean[][] = [];
      let out = -1;
      for (let i = 0; i < 40; i++) {
        w.step();
        if (out < 0 && bb.state === 'withdrawn') out = i;
        faces.push(c1.face); combs.push([w.combs.has(c1.id), w.combs.has(c2.id)]);
      }
      return { faces, combs, out };
    };
    const first = leaving(true), after = leaving(false);
    expect(first.out).toBeGreaterThan(0);
    expect(after.out).toBe(first.out);
    expect(first.combs[first.out]).toEqual([true, true]); // her own tick: the test was taken before she left
    expect(first.combs[first.out + 1]).toEqual([false, false]);
    expect(after.combs).toEqual(first.combs);
    expect(after.faces).toEqual(first.faces);
    // Samar at the build's steps 1 and 2 (the record ticket, a few seeds): at step 1 the cover run's target cruiser combs
    // for about its run, no battleship combs, every comb is on a salvo fired at the comber, and Taffy 3 (a carrier group)
    // combs nothing; at step 2 a battleship combs a salvo fired at another ship, and Taffy still combs nothing. Taffy
    // breaks at both.
    for (const seed of evenLuck(3)) {
      const one = samar(seed, 1);
      expect(one.T['att:combs>ca'], `${seed}`).toBeGreaterThanOrEqual(1);
      expect(one.T['att:combs>bb'] ?? 0, `${seed}`).toBe(0);
      expect(one.told.every((e: FieldEvent) => e.extra!.fired === e.unit)).toBe(true); // fired at her: no water at step 1
      expect(one.T['att:comb>ca:s'] / one.T['att:combs>ca']).toBeGreaterThan(0.9 * SEA.torp.travel); // for its run
      expect(one.T['def:combs'] ?? 0).toBe(0); // Taffy is a carrier group: held at steps 1 and 2
      expect(one.s.brokenSide).toBe('def');
      const two = samar(seed, 2);
      expect(two.T['att:combs>bb'], `${seed}`).toBeGreaterThanOrEqual(1);
      expect(two.told.some((e: FieldEvent) => e.extra!.fired !== e.unit && two.s.byId.get(e.unit!).extra.cls === 'bb')).toBe(true);
      expect(two.T['def:combs'] ?? 0).toBe(0);
      expect(two.s.brokenSide).toBe('def');
    }
  });

  it("15. the look: a 'sea:comb' event, with no words, as she takes each salvo, the first and each switch; 'combFrom' and 'combUntil' while she combs, gone after", () => {
    const s = battle(11, { human: null }) as any;
    const bb = all(s, 'att', 'bb')[0], [d1, d2] = all(s, 'def', 'dd');
    only(s, [[bb, 10000, 10000, 0], [d1, 6000, 10000], [d2, 14000, 10000]]);
    s.bank[d1.id] = 0.01; s.launch(d1, bb, null);
    s.view();
    s.step();
    const t1 = s.torps[0];
    const want = (tp: any, from: number): unknown => ({ from, course: Math.round(norm(s.combTo[bb.id]) * 100) / 100, until: Math.round(tp.at * 10) / 10, fired: bb.id });
    let ev = combsOf(s);
    expect(ev.map((e) => [e.unit, e.side, e.text])).toEqual([[bb.id, 'att', '']]);
    expect(ev[0].extra).toEqual(want(t1, d1.id));
    expect(ev[0].extra!.course).toBeCloseTo(norm(t1.dir), 2);
    expect([bb.extra.combFrom, bb.extra.combUntil]).toEqual([d1.id, Math.round(t1.at * 10) / 10]);
    run(s, 30);
    s.bank[d2.id] = 0.01; s.launch(d2, bb, null);
    const t2 = s.torps[1];
    expect(combsOf(s)).toEqual([]); // no new event while she holds the first
    while (s.t < t1.at - 1e-9) s.step();
    ev = combsOf(s);
    expect(ev.map((e) => e.unit)).toEqual([bb.id]);
    expect(ev[0].extra).toEqual(want(t2, d2.id));
    expect([bb.extra.combFrom, bb.extra.combUntil]).toEqual([d2.id, Math.round(t2.at * 10) / 10]);
    while (s.t < t2.at - 1e-9) s.step();
    expect(combsOf(s)).toEqual([]);
    expect([bb.extra.combFrom, bb.extra.combUntil]).toEqual([undefined, undefined]);
    expect('combFrom' in bb.extra).toBe(false);
    expect(s.tally['att:combSwitch']).toBe(1);
  });

  it("16. Samar on the record ticket, the whole rule: the cover run's target cruiser combs for its run, Kurita's battleships in its water comb, and Taffy 3 still breaks", () => {
    for (const seed of evenLuck(3)) {
      const w = samar(seed, 3); // the whole rule, nothing held (once the build's stage goes at step 3, this is the rule)
      expect(w.caOwn, `${seed}`).toBeGreaterThanOrEqual(0.9 * SEA.torp.travel); // a cruiser fired at, for about its run
      expect(w.told.some((e: FieldEvent) => e.extra!.fired !== e.unit && w.s.byId.get(e.unit!).extra.cls === 'bb'), `${seed}`).toBe(true); // Yamato's turn, from torpedoes fired at another
      expect(w.s.brokenSide).toBe('def');
    }
  });

  it("17. the comb never changes her speed: a helm that keeps her on a place gives the speed it keeps there, not its speed closing on it as the comb carries her off", () => {
    // A battleship astern of her line, beyond ai.keep of its middle, working back to it: she combs at the line's speed.
    const s = battle(5, { human: null }) as any;
    const [a1, a2] = all(s, 'att', 'bb'), [e1] = all(s, 'def', 'bb'), dd = all(s, 'def', 'dd')[0];
    only(s, [[a1, 10000, 2000, 0], [a2, 10000, 8000, 0], [e1, 4000, 14000, Math.PI], [dd, 13000, 3000]]);
    s.contactAt = 0;
    s.step();
    expect(s.linePlans.att.members).toContain(a1);
    s.bank[dd.id] = 0.02; s.launch(dd, a1, null);
    dd.state = 'gone';
    run(s, 30);
    expect(s.combs.has(a1.id)).toBe(true);
    expect(Math.abs(a1.extra.way - s.lines.att.speed)).toBeLessThan(0.01);
    // A carrier keeping her station astern of her battle line, the enemy bringing aircraft: she combs at about her
    // station's way (the gun ships' 11 m/s here), never run up toward flank as the comb carries her off her station.
    const c = battle(11, { human: null, att: [HOME], def: [BATTLE, { name: 'Task Force', nat: 'USA', ships: { cv: 2, ca: 3, dd: 6 } }] }) as any;
    const cv = all(c, 'att', 'cv')[0], cd = all(c, 'def', 'dd')[0], V = SEA.classes.cv.speed;
    const helm = c.carrierHelm.bind(c);
    const last: { h: { speed: number } | null } = { h: null };
    let settled = 0;
    c.carrierHelm = (u: SeaUnit) => { const h = helm(u); if (u === cv) last.h = h; return h; };
    while (!c.over && c.t < 900 && settled < 300) {
      last.h = null;
      c.step();
      const h = last.h as { speed: number } | null;
      settled = h && h.speed < V * SEA.speeds.flank - 0.01 && Math.abs(cv.extra.way - h.speed) < 0.01 ? settled + 1 : 0;
    }
    expect(settled).toBe(300);
    const [sx, sy] = [cd.x, cd.y];
    Object.assign(cd, { x: cv.x + 4000, y: cv.y }); c.bank[cd.id] = 0.01; c.launch(cd, cv, null); Object.assign(cd, { x: sx, y: sy });
    let most = 0, ticks = 0;
    while (c.t < c.torps.at(-1).at - 1) { c.step(); if (c.combs.has(cv.id)) { ticks++; most = Math.max(most, cv.extra.way); } }
    expect(ticks).toBeGreaterThan(600);
    expect(most).toBeLessThan(V * SEA.speeds.cruise);
  });

  it('18. a ship that goes down or burns out of the fight while she combs leaves no comb behind', () => {
    const s = battle(11, { human: null }) as any;
    const bb = all(s, 'att', 'bb')[0], dd = all(s, 'def', 'dd')[0];
    only(s, [[bb, 10000, 10000, 0], [dd, 6000, 10000]]);
    s.bank[dd.id] = 0.01; s.launch(dd, bb, null);
    s.step();
    expect(s.combs.has(bb.id)).toBe(true);
    while (bb.extra.ships > 0) s.loseShip(bb);
    expect(live(bb)).toBe(false);
    expect(s.combs.has(bb.id)).toBe(false);
    expect(s.combSteered.has(bb.id)).toBe(false);
    run(s, 120);
    expect([...s.combs.keys()]).toEqual([]);
  });

  it('19. the comb marks that read where the enemy lies are taken once every piece has steered: the same whatever the order the pieces steer in', () => {
    const read = (enemyFirst: boolean): [number, boolean, number] => {
      const s = battle(11, { human: null }) as any;
      const bb = all(s, 'att', 'bb')[0], dd = all(s, 'def', 'dd')[0], ca = all(s, 'def', 'ca')[0];
      const R = SEA.classes.bb.reach;
      // An enemy cruiser just beyond her reach, steaming straight at her: within it once it has moved this tick.
      only(s, [[bb, 10000, 10000, 0], [dd, 3000, 10000], [ca, 10000 + R + 0.5, 10000, 1.5 * Math.PI]]);
      s.bank[dd.id] = 0.01; s.launch(dd, bb, null);
      dd.state = 'gone';
      s.command({ do: 'move', units: [ca.id], x: 0, y: 10000 }, 'def');
      const rest = s.units.filter((u: SeaUnit) => u !== bb && u !== ca);
      s.units = enemyFirst ? [ca, bb, ...rest] : [bb, ca, ...rest];
      s.step();
      expect(s.combs.has(bb.id)).toBe(true);
      return [s.tally['att:combSilent:s'] ?? 0, s.combIn[bb.id], Math.hypot(ca.x - bb.x, ca.y - bb.y) - R];
    };
    const a = read(true), b = read(false);
    expect(a[2]).toBeLessThan(0); // within her reach once every piece has steered
    expect(a).toEqual(b);
    expect(a.slice(0, 2)).toEqual([0, true]);
  });

  it("20. one rule for every navy (Q6): under every nation's flag in the game the same ships comb the same tracks on the same course, tick for tick", () => {
    const play = (nat: string): string => {
      const s = battle(11, { human: null, att: [{ ...HOME, nat }], def: [{ ...BATTLE, nat }] }) as any;
      const bb = all(s, 'att', 'bb')[0], ca = all(s, 'att', 'ca')[0], dd = all(s, 'def', 'dd')[0], ed = all(s, 'def', 'dd')[1];
      // Forward of the battleship's port beam, and dead at the cruiser's bow (the tie).
      only(s, [[bb, 10000, 10000, 0], [ca, 14000, 10000, 0], [dd, 7000, 13000], [ed, 14000, 14000]]);
      s.contactAt = 0;
      s.launch(dd, bb, null);
      s.launch(ed, ca, null);
      const seen: number[][] = [];
      for (let i = 0; i < 600; i++) { s.step(); seen.push([bb.face, ca.face, s.combs.has(bb.id) ? 1 : 0, s.combs.has(ca.id) ? 1 : 0]); }
      return JSON.stringify(seen);
    };
    const nats = [...new Set([...Object.keys(NATIONS).filter((k) => k !== '_about'), ...Object.keys(SEA.carriers.path).filter((k) => k !== '_')])];
    expect(nats).toContain('JAP');
    const uk = play('UK');
    expect(uk).toContain(',1,1]');
    for (const nat of nats) expect(play(nat) === uk, nat).toBe(true);
  });

  it("21. what her guns could not bring to bear while she combs is banked, as a destroyer's idle guns' weight is, and goes with her first blow after the comb, and no more (the design director, 11:58Z); with nothing in her reach she banks nothing", () => {
    const s = battle(11, { human: null }) as any;
    const bb = all(s, 'att', 'bb')[0], dd = all(s, 'def', 'dd')[0];
    only(s, [[bb, 10000, 10000, 0], [dd, 6000, 10000]]); // abeam to port, in her reach
    expect(s.command({ do: 'hold', units: [dd.id] }, 'def')).toBe(true);
    s.contactAt = 0;
    s.launch(dd, bb, null);
    const tp = s.torps[0];
    expect(s.bank[bb.id]).toBe(0);
    let lost = 0, combed = 0, paid = -1;
    while (s.t < tp.at + 120 && paid < 0) {
      const l0 = s.tally['att:combLost'] ?? 0, b0 = s.bank[bb.id], d0 = s.dealt[bb.id], p0 = s.tally['att:combPaid'] ?? 0;
      s.step();
      const l = (s.tally['att:combLost'] ?? 0) - l0;
      if (s.combs.has(bb.id)) {
        combed++;
        lost += Math.max(0, l);
        expect(s.bank[bb.id]).toBeCloseTo(b0 + Math.max(0, l), 12); // what she could not fire, and only that
        continue;
      }
      if ((s.tally['att:combPaid'] ?? 0) > p0) {
        paid = s.tally['att:combPaid'] - p0;
        expect(paid).toBeCloseTo(b0, 12);
        expect(s.bank[bb.id]).toBe(0);
        expect(s.dealt[bb.id] - d0).toBeGreaterThan(b0); // the bank and that tick's own fire, on the ship her guns bear on
      }
    }
    expect(combed).toBeGreaterThan(50);
    expect(lost).toBeGreaterThan(0);
    expect(paid).toBeCloseTo(lost, 9);
    expect(s.tally['att:combLeft'] ?? 0).toBe(0);

    // The launcher beyond her reach: she combs, her guns have nothing to bear on, and the bank stays empty.
    const t = battle(11, { human: null }) as any;
    const b2 = all(t, 'att', 'bb')[0], d2 = all(t, 'def', 'dd')[0];
    only(t, [[b2, 10000, 10000, 0], [d2, 4000, 10000]]);
    expect(t.command({ do: 'hold', units: [d2.id] }, 'def')).toBe(true);
    t.contactAt = 0;
    t.bank[d2.id] = 0.01; t.loaded[d2.id] = 0;
    t.launch(d2, b2, null);
    const lands = t.torps[0].at;
    let held = 0;
    while (t.t < lands) { t.step(); if (t.combs.has(b2.id)) held++; expect(t.bank[b2.id]).toBe(0); }
    expect(held).toBeGreaterThan(50);
  });

  // R6 item (2), how a ship that combed regains her place (the design director, 19:03Z; naval-battles/r6/regain-form.md).
  const LINE3: PlainFleet = { name: 'Battle Force', nat: 'USA', ships: { bb: 3, dd: 4 } };

  it("22. her place (R6 item 2): up to her comb's end the battle is the same to the bit with the rule on and off; off, no place is kept and no mark taken; on, she works back after it", () => {
    const play = (back: boolean): { ticks: string[]; first: number; s: any } => {
      const s = battle(10, { human: null }) as any;
      s.r6Back = back;
      const ticks: string[] = [];
      let first = -1;
      while (!s.over) {
        s.step();
        ticks.push(JSON.stringify([s.t, s.units.map((u: SeaUnit) => [u.x, u.y, u.face, u.str, u.mor, u.state, u.extra.way, u.extra.ships]), s.view().events]));
        if (first < 0 && s.regaining.size) first = ticks.length - 1;
        if (!back) { expect(s.place.size).toBe(0); expect(s.regaining.size).toBe(0); }
      }
      return { ticks, first, s };
    };
    const on = play(true), off6 = play(false);
    expect(on.first).toBeGreaterThan(0);
    expect(on.ticks.slice(0, on.first + 1)).toEqual(off6.ticks.slice(0, on.first + 1));
    expect(on.ticks.slice(on.first + 1).some((t, i) => t !== off6.ticks[on.first + 1 + i])).toBe(true);
    expect((on.s.tally['att:regain:s'] ?? 0) + (on.s.tally['def:regain:s'] ?? 0)).toBeGreaterThan(0);
    for (const k of Object.keys(off6.s.tally)) expect(k).not.toMatch(/regain/);
  });

  it("23. she works back to her own place, not the line's middle, at flank while it lies ahead of her beam, and is back within ai.station (300 m) of it; the line does not wait", () => {
    const s = new SeaSim(plainTicket({ att: [LINE3], def: [BATTLE], seed: 5, human: null })) as any;
    const [a1, a2, a3] = all(s, 'att', 'bb'), [e1] = all(s, 'def', 'bb'), dd = all(s, 'def', 'dd')[0];
    // A line of three abreast, she on its port wing 1,200 m from its middle; the enemy out of reach, the guns having met.
    only(s, [[a1, 8800, 5000, 0], [a2, 10000, 5000, 0], [a3, 11200, 5000, 0], [e1, 10000, 16000, Math.PI], [dd, 8800, 8000]]);
    s.contactAt = 0;
    s.step();
    expect(s.linePlans.att.members).toContain(a1);
    s.bank[dd.id] = 0.02; s.launch(dd, a1, null);
    dd.state = 'gone';
    const tp = s.torps[0], speed = s.lines.att.speed, flank = SEA.classes.bb.speed * SEA.speeds.flank;
    while (s.combs.has(a1.id) || s.t < tp.t0 + 1) s.step();
    expect(s.regaining.has(a1.id)).toBe(true);
    const at = s.place.get(a1.id) as [number, number];
    expect(Math.hypot(at[0], at[1])).toBeGreaterThan(1000); // her place: the wing, not the middle
    const gapTo = (): number => { const [cx, cy] = s.middle(s.linePlans.att.members); return Math.hypot(a1.x - cx - at[0], a1.y - cy - at[1]); };
    let gap = gapTo(), most = 0, ticks = 0;
    expect(gap).toBeGreaterThan(SEA.ai.station);
    while (s.place.has(a1.id) && s.t < tp.at + 600) {
      gap = gapTo();
      s.step();
      ticks++;
      most = Math.max(most, a1.extra.way);
      expect(s.lines.att.speed).toBe(speed); // the line does not wait for her
    }
    expect(s.place.has(a1.id)).toBe(false);
    expect(gap).toBeLessThan(SEA.ai.station + 10); // back once within ai.station of her place, as measured before the tick
    expect(most).toBeGreaterThan(flank - 0.05);
    expect(s.tally['att:regained']).toBe(1);
    expect(s.tally['att:regain:s']).toBeCloseTo((ticks - 1) * DT, 6); // every tick out of her place, the tick she is back not
    run(s, 60);
    expect(gapTo()).toBeLessThan(SEA.ai.station + 50);
    expect(Math.abs(off(a1.face, s.lines.att.course))).toBeLessThan(0.05);
  });

  it("24. a line ship holding no place to regain is steered exactly as before: the line's course and speed inside the keep, and back toward the line's middle beyond it", () => {
    const s = battle(10, { human: null }) as any;
    const orig = s.lineHelm.bind(s);
    let checked = 0, regaining = 0;
    const before = (u: SeaUnit): { want: number; speed: number; hold?: number } | null => {
      const P = s.linePlans[u.side], L = s.lines[u.side];
      if ((u.extra.cls !== 'bb' && u.extra.cls !== 'ca') || !L || P.t !== s.t || !P.members.includes(u)) return null;
      const [cx, cy] = s.middle(P.members), gap = Math.hypot(u.x - cx, u.y - cy);
      if (gap <= SEA.ai.keep) return { want: L.course, speed: L.speed };
      const pull = Math.min(0.6, (gap - SEA.ai.keep) / SEA.ai.keep), back = off(L.course, Math.atan2(cx - u.x, cy - u.y));
      return { want: L.course + pull * back, speed: Math.cos(back) > 0 ? SEA.classes[u.extra.cls as SeaClass].speed * SEA.speeds.flank : L.speed, hold: L.speed };
    };
    s.lineHelm = (u: SeaUnit) => {
      const held = s.place.has(u.id) && s.regaining.has(u.id);
      const was = held ? null : before(u);
      const got = orig(u);
      if (held) { regaining++; return got; }
      checked++;
      if (was === null) expect(got).toBeNull();
      else {
        expect(Math.abs(off(got.want, was.want))).toBeLessThan(1e-12);
        expect([got.speed, got.hold]).toEqual([was.speed, was.hold]);
      }
      return got;
    };
    while (!s.over) s.step();
    expect(checked).toBeGreaterThan(1000);
    expect(regaining).toBeGreaterThan(100);
  });
});

describe('ships by class on the field (naval-battles/ships-by-class-plan.md 5.1 c1 and c3, tests 6.1)', () => {
  const row = (id: string | null, worth: number, n: number, label = id ?? ''): ShipRow => ({ id, label, worth, n });
  /** Two lines whose battleships and destroyers come by row: the attacker's two classes of battleship and three of
   *  destroyer, the defender's one of each with hulls past its rows. */
  const RA: PlainFleet = { name: 'Atlantic Fleet', nat: 'USA', ships: { bb: 4, ca: 3, dd: 12 }, rows: { bb: [row('nc', 1.3, 2), row('ny', 0.8, 2)], dd: [row('fl', 1.1, 4), row('bn', 0.9, 4), row('sm', 1.0, 4)] } };
  const RB: PlainFleet = { name: 'Battle Fleet', nat: 'GER', ships: { bb: 3, ca: 3, dd: 12 }, rows: { bb: [row('bi', 1.2, 2)], dd: [row('z', 1.05, 6)] } };
  const plain = (f: PlainFleet): PlainFleet => { const { rows: _, ...rest } = f; return rest; };
  const sideW = (units: SeaUnit[], info: Map<number, { weight: number }>, side: 'att' | 'def'): number => units.filter((u) => u.side === side).reduce((n, u) => n + info.get(u.id)!.weight * u.extra.ships0, 0);

  it('1. no rows, no change: a ticket with none carries none and draws up as ever; rows of worth 1 in one row fight the same battle to the bit', () => {
    const t = plainTicket({ att: [HOME], def: [BATTLE], seed: 3 });
    for (const f of t.place.fleets) expect('rows' in f).toBe(false);
    expect(rowsOf(undefined, { bb: 2, cv: 0, ca: 0, dd: 0 })).toBeUndefined();
    const { info } = drawUp(t.place.fleets);
    for (const p of info.values()) expect('rows' in p).toBe(false);
    const flat = (f: PlainFleet): PlainFleet => ({ ...f, rows: Object.fromEntries(SEA_CLASSES.filter((c) => (f.ships[c] ?? 0) > 0).map((c) => [c, [row(null, 1, f.ships[c]!)]])) });
    const play = (att: PlainFleet, def: PlainFleet): { s: SeaSim; ev: string } => {
      const s = new SeaSim(plainTicket({ att: [att], def: [def], seed: 7, human: null }));
      runOut(s);
      return { s, ev: JSON.stringify(s.view().events) };
    };
    const a = play(HOME, BATTLE), b = play(flat(HOME), flat(BATTLE));
    expect(b.ev).toBe(a.ev);
    const ra = a.s.over!, rb = b.s.over!;
    expect(rb.R).toBe(ra.R);
    const { rows, ...rest } = rb.extra as unknown as SeaLosses;
    expect(rest).toEqual(ra.extra);
    expect(rows).toBeDefined();
    expect((ra.extra as unknown as SeaLosses).rows).toBeUndefined();
  });

  it("2. weight kept: each side's pieces weigh what its ticket does, rows or none; a fleet's ticket weighs its fleet power whatever its rows, and rows of worth 1 leave its multiplier as it was", () => {
    for (const odds of [undefined, 1, 1.15]) {
      const t = plainTicket({ att: [RA], def: [RB], seed: 3, odds, air: { def: 5 } });
      const { units, info } = drawUp(t.place.fleets);
      expect(Math.abs(sideW(units, info, 'att') - t.reckoning.A)).toBeLessThan(1e-9);
      expect(Math.abs(sideW(units, info, 'def') - (t.reckoning.D - t.place.air.def))).toBeLessThan(1e-9);
      // Each kind's weight is its hulls' worths times the kind's weight.
      for (const f of t.place.fleets) for (const c of SEA_CLASSES) {
        const w = units.filter((u) => u.corps === f.id && u.extra.cls === c).reduce((n, u) => n + info.get(u.id)!.weight * u.extra.ships0, 0);
        expect(Math.abs(w - hullsOf(f, c) * RULES_CLASSES[c].power * (c === 'cv' ? f.deck : 1) * f.mul)).toBeLessThan(1e-9);
      }
    }
    const g = createGame(3, null);
    const f = g.fleets.find((x) => x.ships.bb >= 2 && x.ships.dd >= 2)!;
    const was = fleetTicket(g, f, 'att');
    const ones = fleetTicket(g, f, 'att', { bb: [row(null, 1, f.ships.bb)] });
    expect(ones.mul).toBe(was.mul);
    const two = fleetTicket(g, f, 'att', { bb: [row('a', 1.25, 1), row('b', 0.9, f.ships.bb - 1)], ss: [row('x', 3, 2)] });
    const weigh = (x: typeof was): number => SEA_CLASSES.reduce((n, c) => n + hullsOf(x, c) * RULES_CLASSES[c].power * (c === 'cv' ? x.deck : 1) * x.mul, 0);
    expect(Math.abs(weigh(two) - fleetPower(g, f)) / fleetPower(g, f)).toBeLessThan(1e-12);
    expect(Math.abs(weigh(was) - fleetPower(g, f)) / fleetPower(g, f)).toBeLessThan(1e-12);
    expect(Object.keys(two.rows!)).toEqual(['bb']); // submarines fight no fleet action
  });

  it("3. the same rule both sides: a piece's rows come only from its own fleet's list; rows swapped between the sides draw up the same pieces turned half round", () => {
    const t = plainTicket({ att: [RA], def: [RB], seed: 3 });
    const { units, info } = drawUp(t.place.fleets);
    for (const u of units) {
      const p = info.get(u.id)!, own = (p.fleet.rows?.[u.extra.cls as SeaClass] ?? []).map((r) => r.id);
      for (const id of p.rows ?? []) expect(id === null || own.includes(id), `${u.name} ${id}`).toBe(true);
      expect(p.fleet.id).toBe(u.corps);
    }
    // The same fleets the other way round: the same pieces, each side's from its own list.
    const r = plainTicket({ att: [{ ...RB, nat: 'USA' }], def: [{ ...RA, nat: 'GER' }], seed: 3 });
    const back = drawUp(r.place.fleets);
    const shape = (us: SeaUnit[], inf: Map<number, { weight: number; rows?: (string | null)[] }>, side: 'att' | 'def') => us.filter((u) => u.side === side).map((u) => [u.extra.cls, u.extra.ships0, inf.get(u.id)!.weight, (inf.get(u.id)!.rows ?? []).join()]);
    expect(shape(back.units, back.info, 'def')).toEqual(shape(units, info, 'att'));
    expect(shape(back.units, back.info, 'att')).toEqual(shape(units, info, 'def'));
  });

  it("5. hand-back: the rows of the ships each fleet lost add up to its ships lost of the kind, sunk and abandoned early, merged pieces too; foughtOf's sunk is unchanged", () => {
    let seen = 0;
    for (const seed of [3, 5, 8, 13]) {
      for (const sailors of [undefined, { water: 'cold' as const, early: [1] }]) {
        const s = new SeaSim(plainTicket({ att: [RA], def: [RB], seed, human: null, sailors }));
        runOut(s);
        const x = s.over!.extra as unknown as SeaLosses;
        for (const f of s.ticket.place.fleets) for (const c of SEA_CLASSES) {
          const given = f.rows?.[c];
          const lost = x.sunk[f.id][c] + (x.abandoned?.[f.id]?.[c] ?? 0);
          if (!given) { expect(x.rows![f.id]?.[c]).toBeUndefined(); continue; }
          const ids = x.rows![f.id][c]!;
          expect(ids.length, `${seed} ${f.name} ${c}`).toBe(lost);
          seen += ids.length;
          // No row lost more often than the fleet had hulls of it.
          for (const r of given) expect(ids.filter((i) => i === r.id).length).toBeLessThanOrEqual(r.n + (r.id === null ? f.ships[c] : 0));
        }
        expect(foughtOf(s.over!).sunk).toEqual(sailors ? Object.fromEntries(Object.keys(x.sunk).map((id) => [id, Object.fromEntries(SEA_CLASSES.map((c) => [c, x.sunk[+id][c] + (x.abandoned?.[+id]?.[c] ?? 0)]))])) : x.sunk);
      }
    }
    expect(seen).toBeGreaterThan(10);
    // Past the field's limit, with the fleet's call to abandon early: ships abandoned are lost to the campaign, rows and all.
    const a = new SeaSim(plainTicket({ att: [RA], def: [RB], seed: 3, human: null, sailors: { water: 'cold', early: [1] } })) as any;
    const dd = a.units.find((v: SeaUnit) => v.side === 'att' && v.extra.cls === 'dd')!;
    a.caps.att.dd = 1;
    a.wear(dd, a.hullOf(dd) * 1.01);
    const xa = a.fleetLoss;
    expect([xa.sunk[1].dd, xa.abandoned[1].dd]).toEqual([1, 3]);
    expect(a.lostRows[1].dd).toEqual(['fl', 'fl', 'fl', 'fl']);
    // A merged piece gives up its hulls by a fixed rotation: one of each row before a second of either.
    const s = new SeaSim(plainTicket({ att: [RA], def: [RB], seed: 3, human: null })) as any;
    const u = s.units.find((v: SeaUnit) => v.side === 'att' && v.extra.cls === 'dd')!;
    s.hullsLeft.set(u.id, ['fl', 'fl', 'bn', 'bn']); s.info.get(u.id).rows = ['fl', 'fl', 'bn', 'bn'];
    const lostTo = (): (string | null)[] => s.lostRows[1].dd;
    for (let k = 0; k < 3; k++) s.loseShip(u);
    expect(lostTo()).toEqual(['fl', 'bn', 'fl']);
  });

  it('6. a row on the ticket carries only its id, label, worth and n; the field\'s kinds only; never more hulls than the kind has', () => {
    const given = { bb: [{ id: 'a', label: 'A class', worth: 1.2, n: 2, reach: 30000 } as ShipRow, row('b', 0.9, 5)], ss: [row('s', 2, 3)] } as Partial<Record<string, ShipRow[]>>;
    const rows = rowsOf(given, { bb: 4, cv: 0, ca: 0, dd: 0 })!;
    expect(Object.keys(rows)).toEqual(['bb']);
    expect(rows.bb!.map((r) => Object.keys(r).sort())).toEqual([['id', 'label', 'n', 'worth'], ['id', 'label', 'n', 'worth']]);
    expect(rows.bb!.map((r) => r.n)).toEqual([2, 2]);
    const t = plainTicket({ att: [{ ...RA, rows: given as PlainFleet['rows'] }], def: [RB], seed: 1 });
    for (const f of t.place.fleets) for (const c of SEA_CLASSES) for (const r of f.rows?.[c] ?? []) expect(Object.keys(r).sort()).toEqual(['id', 'label', 'n', 'worth']);
  });

  it("8. pieces by row: below the limit every piece holds one row; past it rows of one kind within one fleet merge, closest worths first, and no side passes the limit", () => {
    const t = plainTicket({ att: [RA], def: [RB], seed: 3 });
    const { units, info } = drawUp(t.place.fleets);
    for (const u of units) { const r = info.get(u.id)!.rows; if (r) expect(new Set(r).size, u.name).toBe(1); }
    // The attacker's destroyers sail in fours, a piece to each of its three rows; the defender's six of one row and six with none.
    const dds = (side: 'att' | 'def'): string[] => units.filter((u) => u.side === side && u.extra.cls === 'dd').map((u) => info.get(u.id)!.rows!.join());
    expect(dds('att')).toEqual(['fl,fl,fl,fl', 'bn,bn,bn,bn', 'sm,sm,sm,sm']);
    expect(dds('def')).toEqual(['z,z,z,z', 'z,z', ',,,', ',']);
    // Many rows of destroyers past the limit: they merge within the fleet and kind, the closest worths first.
    const many: PlainFleet = { name: 'Many', nat: 'USA', ships: { bb: 6, ca: 6, dd: 30 }, rows: { dd: [row('a', 1.0, 3), row('b', 1.3, 3), row('c', 1.01, 3), row('d', 0.7, 3), row('e', 1.31, 3), row('f', 0.71, 3), row('g', 1.0, 3), row('h', 1.2, 3), row('i', 0.9, 3), row('j', 1.1, 3)] } };
    const m = plainTicket({ att: [many, { ...many, name: 'More', rows: undefined }], def: [RB], seed: 3 });
    const per = groupSizes(m.place.fleets.filter((f) => f.side === 'att'));
    const d = drawUp(m.place.fleets);
    const att = d.units.filter((u) => u.side === 'att');
    expect(att.length).toBeLessThanOrEqual(SEA.pieces.most);
    const mixed = att.filter((u) => new Set(d.info.get(u.id)!.rows ?? []).size > 1);
    expect(mixed.length).toBeGreaterThan(0);
    for (const u of mixed) {
      expect(u.extra.cls).toBe('dd');
      expect(d.info.get(u.id)!.fleet.name).toBe('Many');
      expect(u.extra.ships0).toBeLessThanOrEqual(per.dd);
    }
    // Closest worths first: 'a' and 'g' (both 1.0) come together, as do 'd' and 'f' (0.7 and 0.71); the merged rows then
    // sail ten hulls a piece, in the list's order.
    const holding = (id: string): string[] => mixed.filter((u) => d.info.get(u.id)!.rows!.includes(id)).map((u) => [...new Set(d.info.get(u.id)!.rows)].join());
    expect(holding('a')).toEqual(['a,g,c,h']);
    expect(holding('d')).toEqual(['e,d,f,i']);
    expect(att.filter((u) => u.extra.cls === 'dd' && d.info.get(u.id)!.fleet.name === 'Many').map((u) => u.extra.ships0)).toEqual([10, 10, 10]);
    // Weight still kept.
    expect(Math.abs(sideW(d.units, d.info, 'att') - m.reckoning.A)).toBeLessThan(1e-9);
  });
});
