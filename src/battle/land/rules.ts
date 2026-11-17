// The land battle's numbers, in one place so they can be tuned (TODO(polish): tune against played battles and the
// fairness table; every value here is a first guess checked only by the parity test).
//
// Abstract on purpose (ruling 217): a block wears down the blocks it is in contact with at a steady rate set by its
// own power, the ground and which side it hits; nerve drains with losses. No weapons, rounds or ballistics.

import type { Ground } from './field';

export type Kind = 'inf' | 'mot' | 'arm' | 'cav' | 'mtn' | 'fort' | 'art' | 'eng';
export type Motion = 'foot' | 'wheels' | 'tracks' | 'guns' | 'fixed';

export const MOTION: Record<Kind, Motion> = { inf: 'foot', mtn: 'foot', cav: 'foot', eng: 'foot', mot: 'wheels', arm: 'tracks', art: 'guns', fort: 'fixed' };

export const LAND = {
  /** Metres a second over open ground, battle time (the field is 3 km by 2 km; contact comes in a few minutes at x1). */
  speed: { inf: 6, mtn: 5.5, cav: 9, eng: 5.5, mot: 11, arm: 10, art: 4, fort: 0 } as Record<Kind, number>,
  /** Radians a second a block can turn. */
  turn: { inf: 1.1, mtn: 1.1, cav: 1.4, eng: 1.1, mot: 0.9, arm: 0.8, art: 0.5, fort: 0 } as Record<Kind, number>,
  /** How far a block's blows carry, metres between edges. Guns fire over the line from well back. */
  reach: { inf: 200, mtn: 200, cav: 140, eng: 140, mot: 220, arm: 380, art: 1500, fort: 300 } as Record<Kind, number>,
  /** Frontage and depth in metres. */
  size: { inf: [150, 40], mtn: [140, 40], cav: [150, 50], eng: [120, 40], mot: [140, 60], arm: [130, 60], art: [110, 50], fort: [140, 50] } as Record<Kind, [number, number]>,
  /** Models on a full-strength tray (the art director, 17 November): one fades off for each share lost. */
  figures: { inf: 12, mtn: 12, cav: 12, eng: 12, fort: 12, mot: 6, arm: 4, art: 3 } as Record<Kind, number>,
  /** The share of a block's own power it takes off its target each second, at full strength and in the open. */
  rate: 0.011,
  /** Blows struck on the march, and by a block that is wavering. */
  moving: 0.4,
  wavering: 0.7,
  /** Guns: their share against what they hit, the extra they take off nerve, and what they can do when the enemy is on top of them. */
  guns: { punch: 0.35, nerve: 1.5, close: 0.3, closeAt: 160 },
  /** Damage and nerve by where a blow lands: [damage, nerve]. */
  aspect: { front: [1, 1], flank: [1.5, 2], rear: [2, 3] } as Record<'front' | 'flank' | 'rear', [number, number]>,
  /** The damage a block takes on this ground (cover), and the speed it keeps on it, by how it moves. */
  cover: { open: 1, field: 0.95, woods: 0.7, rough: 0.85, marsh: 1, water: 1.25, road: 1.05, bridge: 1.2, buildings: 0.55, rock: 0.75, bunker: 0.4, works: 0.6, blown: 1.25 } as Record<Ground, number>,
  pace: {
    foot: { open: 1, field: 0.9, woods: 0.6, rough: 0.7, marsh: 0.35, water: 0.15, road: 1.1, bridge: 1, buildings: 0.7, rock: 0.4, bunker: 0.5, works: 0.8, blown: 0.15 },
    wheels: { open: 0.9, field: 0.8, woods: 0.3, rough: 0.4, marsh: 0, water: 0, road: 1.5, bridge: 1.2, buildings: 0.5, rock: 0, bunker: 0, works: 0.5, blown: 0 },
    tracks: { open: 1, field: 0.9, woods: 0.4, rough: 0.6, marsh: 0.12, water: 0, road: 1.3, bridge: 1, buildings: 0.5, rock: 0, bunker: 0, works: 0.6, blown: 0 },
    guns: { open: 1, field: 0.9, woods: 0.4, rough: 0.5, marsh: 0.15, water: 0, road: 1.4, bridge: 1, buildings: 0.6, rock: 0, bunker: 0, works: 0.7, blown: 0 },
    fixed: { open: 0, field: 0, woods: 0, rough: 0, marsh: 0, water: 0, road: 0, bridge: 0, buildings: 0, rock: 0, bunker: 0, works: 0, blown: 0 },
  } as Record<Motion, Record<Ground, number>>,
  /** Tanks are blind in close country and in the streets: what they strike and what they take there. */
  tanksClose: { punch: 0.6, take: 1.3, ground: ['woods', 'buildings'] as Ground[] },
  /** Metres a second two blocks standing in each other's ground are eased apart, and the metres enemies keep between their fronts. */
  spread: 4,
  apart: 10,
  /** Mountain troops on rough ground and rock keep this much more of their pace. */
  mountainFoot: 1.6,
  /** Higher ground: blows from above strike harder, blows from below land softer, past this many metres of rise. */
  height: { over: 8, up: 1.15, down: 0.9 },
  /**
   * Holding its own ground: a defending block within 'within' metres of where it was drawn up takes this share of the
   * damage. Set by the fairness table, ground by ground, so that two computer generals carry the ground about as often
   * as the dice would (ruling 217(3)). Above 1 where the field's own cover (woods, marsh, rock) already gives the
   * defence more than the campaign's odds, which count the ground already, mean it to have.
   */
  hold: { take: { plains: 1, forest: 1.75, hills: 1.1, mountain: 1.45, marsh: 1.9, urban: 1, desert: 1 } as Record<string, number>, within: 300 },
  /** Dug in: the damage taken while it stays within this many metres of where it began. */
  dug: { take: 0.65, keep: 30 },
  nerve: {
    /** Below 'waver' it fights worse; below 'rout' it runs; a runner rallies at 'rally' when clear of the enemy for 'quiet' seconds. */
    waver: 0.4, rout: 0.2, rally: 0.35, quiet: 15, clear: 450,
    /**
     * Nerve lost for each share of strength lost, before where the blow landed. Divisions break on their nerve long
     * before they are worn away, so a battle fought to its end costs about what the campaign bills for its result
     * (the design director, b356); an attack's nerve goes faster than a defence's on its own ground.
     */
    perLoss: { att: 0.5, def: 0.5 } as Record<'att' | 'def', number>,
    /** A friend breaking within 'near' metres. */
    friendRuns: 0.03, near: 400,
    /** Coming back while not being hit for 'still' seconds, a second; faster for a runner who is clear. */
    recover: 0.01, still: 5, runnerRecover: 0.02,
    /** Shattered: below this much strength a block runs whatever its nerve. */
    shattered: 0.12,
    /** Experience and a general: experience takes this share off every loss of nerve at full; each level of the general 0.03. */
    xp: 0.35, general: 0.03,
  },
  /**
   * Blocks a side: one a division. A side with fewer than 'least' has divisions split into their three regiments until
   * it has enough to handle (ruling 217(c)); more than 'most' and divisions are folded two to a block.
   */
  blocks: { least: 6, most: 20, regiments: 3 },
  /** The army breaks when this share of its power is running or gone. */
  breaks: 0.5,
  /**
   * Or when this share of its power is worn away: what an attack will pay before it is called off, and a defence
   * before it gives up the ground. Set so that a battle fought to its end costs each side about what the campaign's
   * bill charges for the result it hands back (the design director, b356; tests/battle-land.test.ts).
   */
  spent: { att: 0.13, def: 0.18 } as Record<'att' | 'def', number>,
  /**
   * What a block that leaves the field still loses on the way (stragglers, wounded, guns and lorries left behind), as a
   * share of the strength it had left. A defence driven off its own ground leaves far more behind than an attack that
   * falls back to the lines it came from: the campaign's own bill has the same shape (resolveBattle in game.ts).
   */
  count: { routing: { att: 0.05, def: 0.05 }, gone: { att: 0.1, def: 0.15 } } as { routing: Record<'att' | 'def', number>; gone: Record<'att' | 'def', number> },
  /** Randomness of each blow, so near-even fights do not always go the same way: +- this share. */
  jitter: 0.1,
  /** Seconds of battle time before the defender is counted as having held. */
  limit: 900,
  /** The computer general thinks once a second. */
  think: 1,
};
