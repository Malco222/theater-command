// Fleet actions on the field (src/battle/sea): the pieces, the orders of the first slice (ruling 217: course, attack,
// speed, withdraw, carrier strike, and one press to hand the rest to the dice), sinking within what the dice allow,
// and the ratio handed back.
import { describe, expect, it } from 'vitest';
import { runOut } from '../src/battle/core/clock';
import { DT, type FieldEvent } from '../src/battle/core/types';
import { drawUp, groupSizes } from '../src/battle/sea/pieces';
import { RULES_CLASSES, SEA } from '../src/battle/sea/numbers';
import { diceRange, lossShares, seaChance, seaLevel, seaRatio, shipWords, sunkCaps } from '../src/battle/sea/result';
import { SeaSim, type SeaLosses } from '../src/battle/sea/sim';
import { plainTicket, type PlainFleet } from '../src/battle/sea/ticket';
import { SEA_CLASSES, type SeaClass, type SeaShips, type SeaUnit } from '../src/battle/sea/types';
import { foughtOf } from '../src/battle/sea/handoff';
import { PRACTICE, practiceTicket } from '../src/battle/sea/practice';
import { headCount, hurtAfloat, ofFleet, repairLines, theFleet } from '../src/battle/sea/words';
import { SEA_CUES, seaLoop } from '../src/battle/sea/cues';

const HOME: PlainFleet = { name: 'Home Fleet', nat: 'UK', ships: { bb: 2, cv: 1, ca: 3, dd: 8 } };
const BATTLE: PlainFleet = { name: 'Battle Fleet', nat: 'GER', ships: { bb: 2, ca: 3, dd: 12 } };

/** A battle with the player on the attacking side and the computer on the other, unless both are asked for. */
function battle(seed = 11, o: { human?: 'att' | 'def' | null; odds?: number; att?: PlainFleet[]; def?: PlainFleet[] } = {}): SeaSim {
  return new SeaSim(plainTicket({ att: o.att ?? [HOME], def: o.def ?? [BATTLE], seed, odds: o.odds ?? 1, human: o.human === undefined ? 'att' : o.human }));
}
const run = (sim: SeaSim, seconds: number): void => { for (let i = 0; i < Math.round(seconds / DT) && !sim.over; i++) sim.step(); };
const piece = (sim: SeaSim, side: 'att' | 'def', cls: string): SeaUnit => sim.units.find((u) => u.side === side && u.extra.cls === cls)!;
const events = (sim: SeaSim): FieldEvent[] => sim.view().events;

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
    const sim = battle(5, { human: null, att: [{ name: 'Atlantic Fleet', nat: 'US', ships: { bb: 4, ca: 4, dd: 8 } }], def: [{ name: '2nd Fleet', nat: 'ITA', ships: { ca: 7, dd: 25 } }] });
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
    const sim = battle(5, { human: null, att: [{ name: 'Battle Force', nat: 'US', ships: { bb: 2, ca: 3, dd: 8 } }], def: [{ name: 'Carrier Division', nat: 'JAP', ships: { cv: 2 } }] });
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
    // The raid's weight, shared over what the defender brought: its ships wear as hulls, its coasts' aircraft (12)
    // whole (to within the raid's falling on several ships one after another).
    const W = SEA.fire.rate * 5 * SEA.air.share * SEA.air.every * s.day('att'), air = 12 / T;
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
      const theirs = sim.units.filter((u) => u.side === 'def' && u.state !== 'gone' && u.extra.ships > 0 && u.str <= 0.5 && u.extra.ships0 === 1);
      if (!theirs.length) continue;
      const lines = repairLines(hurt, 'att', 'Italian');
      const line = lines.find((l) => l.includes('Italian'))!;
      expect(line, lines.join(' | ')).toMatch(/came home for repair/);
      for (const u of theirs) expect(line).toContain(u.extra.cls === 'bb' ? 'battleship' : u.extra.cls === 'ca' ? 'cruiser' : 'destroyer');
      return;
    }
    throw new Error('no close victory with an Italian ship afloat at half its hull in 60 seeds');
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

