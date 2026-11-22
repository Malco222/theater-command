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
import { SEA_CLASSES, type SeaUnit } from '../src/battle/sea/types';

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
  it('capital ships sail singly, destroyers in fours, and no piece is called a flotilla or a squadron', () => {
    const { units } = drawUp(plainTicket({ att: [HOME], def: [BATTLE], seed: 1 }).place.fleets);
    const att = units.filter((u) => u.side === 'att');
    expect(att.filter((u) => u.extra.cls === 'bb').every((u) => u.extra.ships0 === 1)).toBe(true);
    expect(att.filter((u) => u.extra.cls === 'dd').map((u) => u.extra.ships0)).toEqual([4, 4]);
    for (const u of units) expect(u.name).not.toMatch(/flotilla|squadron/i);
    expect(att.find((u) => u.extra.cls === 'dd')!.name).toBe('4 destroyers, Home Fleet');
    expect(att.find((u) => u.extra.cls === 'bb')!.name).toBe('Battleship, Home Fleet');
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
    expect(off.text).toBe('A carrier of Home Fleet flies off 24 bombers against a battleship of Battle Fleet.');
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
    expect(r.lines).toContain('The staff settled the rest of the action in the North Sea from where it stood: what you had won stays won.');
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
    for (const c of ['ship_guns', 'ship_bell']) expect(cues.has(c), c).toBe(true);
    expect(cues.has('ship_sinking')).toBe(sim.units.some((u) => u.extra.sunk > 0));
    // A battleship's box is one hull; four destroyers in line ahead, three lengths apart.
    const bb = piece(sim, 'att', 'bb'), dd = sim.units.find((u) => u.extra.cls === 'dd' && u.extra.ships0 === 4)!;
    expect(bb.d).toBe(SEA.classes.bb.len);
    expect(dd.d).toBe(SEA.classes.dd.len * 10);
  });
});
