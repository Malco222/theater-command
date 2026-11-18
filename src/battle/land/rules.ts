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
  /**
   * How far a block's blows carry, metres between edges. Guns fire over the line from well back. Tanks reach no further
   * than lorried infantry: at 380 they stood off out of the infantry's reach and struck without reply, pricing armour
   * again on top of the card, which counts it already (b418, b420: the desert villages ran 38 points over their card).
   */
  reach: { inf: 200, mtn: 200, cav: 140, eng: 140, mot: 220, arm: 220, art: 1500, fort: 300 } as Record<Kind, number>,
  /** Frontage and depth in metres. */
  size: { inf: [150, 40], mtn: [140, 40], cav: [150, 50], eng: [120, 40], mot: [140, 60], arm: [130, 60], art: [110, 50], fort: [140, 50] } as Record<Kind, [number, number]>,
  /** Models on a full-strength tray (the art director, 17 November): one fades off for each share lost. */
  figures: { inf: 12, mtn: 12, cav: 12, eng: 12, fort: 12, mot: 6, arm: 4, art: 3 } as Record<Kind, number>,
  /**
   * The share of a block's own power it takes off its target each second, at full strength and in the open. Slow
   * enough that a close fight runs two or three minutes from the first contact, time for three or four choices
   * (ruling 226(2); tests/battle-land.test.ts). A sixth slower since tanks close to their reach (b418), which had cut
   * the quickest fights under a minute and a half.
   */
  rate: 0.00125,
  /** Blows struck on the march, and by a block that is wavering. */
  moving: 0.4,
  wavering: 0.7,
  /** Guns: their share against what they hit, the extra they take off nerve, and what they can do when the enemy is on top of them. */
  guns: { punch: 0.35, nerve: 1.5, close: 0.3, closeAt: 160 },
  /** Damage and nerve by where a blow lands: [damage, nerve]. */
  aspect: { front: [1, 1], flank: [1.5, 2], rear: [2, 3] } as Record<'front' | 'flank' | 'rear', [number, number]>,
  /**
   * The damage a block takes on this ground (cover), and the speed it keeps on it, by how it moves. Fieldworks
   * ('works', dug where a block stood dug in) give no cover of their own: a blow on them takes the cover of the ground
   * they were dug into (field.dugInto; 'works' here is only for works with no record). The card counts them in the
   * defence's strength already, and the field shares out the card's own strength (ruling 217; b418: what the card
   * prices, the field draws but never prices again). Counted twice, the plain attack carried 23 points less often than
   * the card said in the campaign's dug-in battles, four in five of all it fights; counted once it lands within 3 (b407).
   */
  cover: { open: 1, field: 0.95, woods: 0.7, rough: 0.85, marsh: 1, water: 1.25, road: 1.05, bridge: 1.2, buildings: 0.55, rock: 0.75, bunker: 0.4, works: 1, blown: 1.25 } as Record<Ground, number>,
  pace: {
    foot: { open: 1, field: 0.9, woods: 0.6, rough: 0.7, marsh: 0.35, water: 0.35, road: 1.1, bridge: 1, buildings: 0.7, rock: 0.4, bunker: 0.5, works: 0.8, blown: 0.15 },
    wheels: { open: 0.9, field: 0.8, woods: 0.3, rough: 0.4, marsh: 0, water: 0, road: 1.5, bridge: 1.2, buildings: 0.5, rock: 0, bunker: 0, works: 0.5, blown: 0 },
    tracks: { open: 1, field: 0.9, woods: 0.4, rough: 0.6, marsh: 0.12, water: 0, road: 1.3, bridge: 1, buildings: 0.5, rock: 0, bunker: 0, works: 0.6, blown: 0 },
    guns: { open: 1, field: 0.9, woods: 0.4, rough: 0.5, marsh: 0.15, water: 0, road: 1.4, bridge: 1, buildings: 0.6, rock: 0, bunker: 0, works: 0.7, blown: 0 },
    fixed: { open: 0, field: 0, woods: 0, rough: 0, marsh: 0, water: 0, road: 0, bridge: 0, buildings: 0, rock: 0, bunker: 0, works: 0, blown: 0 },
  } as Record<Motion, Record<Ground, number>>,
  /**
   * Tanks are blind in the streets: what they strike and what they take there. Not in woods: the card prices tanks in
   * forest and on hills already (the campaign's ground rules), so the field draws them slow there but never prices them
   * again (b418). Priced twice, the panzers at Sedan carried the Meuse 21 points less often than their card said.
   */
  tanksClose: { punch: 0.6, take: 1.3, ground: ['buildings'] as Ground[] },
  /** Metres a second two blocks standing in each other's ground are eased apart, and the metres enemies keep between their fronts. */
  spread: 4,
  apart: 10,
  /**
   * A queue, not a pile (the art director, b418): within 'near' metres of ground a block keeps 'hard' of its pace on or
   * less (at a bridge, a pass), a friend in the way of a marching block makes way for it rather than both being eased
   * apart, so a jam reads as one block behind another, each on its own ground. In the open, blocks are eased apart as before.
   */
  queue: { hard: 0.2, near: 75 },
  /** Metres between two friends' boxes where an attack's block takes its own stretch beside a friend standing there (b439). */
  room: 10,
  /** Metres off either end of a bridge kept clear: no attack has a block stand there, so those coming over can (b439). */
  mouth: 50,
  /** Mountain troops on rough ground and rock keep this much more of their pace. */
  mountainFoot: 1.6,
  /** Higher ground: blows from above strike harder, blows from below land softer, past this many metres of rise. */
  height: { over: 8, up: 1.15, down: 0.9 },
  /**
   * Holding its own ground: a defending block within 'within' metres of where it was drawn up takes this share of the
   * damage. Set by the fairness table, ground by ground, so that the plain attack (select all, right-click the nearest,
   * Space) carries the ground about as often as the card says at the telling odds, 1.1 to 1.4 (ruling 226, mark 4), and
   * two computer generals stay fair to it too (ruling 217(3)). Above 1 where the field's own cover (woods, marsh, rock)
   * already gives the defence more than the campaign's odds, which count the ground already, mean it to have. 'river'
   * multiplies it where the defence stands behind a river, 'works' by the works' level: the card counts both already.
   * Level 2 sits between the telling-odds works (an infantry line that keeps a reserve back out of its bunkers) and the
   * practice works (motorized, no reserve): each lands within a tenth of its card (b407). Re-set for b418 (tanks no
   * longer out-reach infantry nor pay twice in woods, works give their own ground's cover, regiments hold a division's
   * frontage, blows a sixth slower, and tanks and lorries find their way round rivers, rock and marsh: mountains held
   * at 1.43 only because the tanks there stood for good at the first outcrop): over five sets of 160 battles a ground,
   * every ground lands within 4 points of its card, about 2 in the attack's favour; the test's own seeds run about 5
   * under that. 'river' 1.51 to 1.5 for b439, once blocks at a bridge made way for each other rather than shove at its
   * foot to a standstill (Sedan's panzers ran out the clock there one fight in eleven). 1.4 to 1.3 for b464(5), once foot
   * waded out of the river at the wading pace instead of crawling up its bank under fire: the practice river and the
   * telling odds behind a river land where they did (+5 and 0 against +6 and -2, the tests' seeds).
   */
  hold: { take: { plains: 0.97, forest: 1.035, hills: 0.99, mountain: 1.12, marsh: 1.32, urban: 1.025, desert: 1.045 } as Record<string, number>, within: 300, river: 1.3, works: [1, 1.02, 1.06, 1.07] },
  /** Dug in: a block is drawn in its works while it stays within this many metres of where it began. */
  dug: { keep: 30 },
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
   * it has enough to handle (ruling 217(c)). No division is ever folded into another (ruling 226(10)).
   */
  blocks: { least: 6, regiments: 3 },
  /** The army breaks when this share of its power is running or gone. */
  breaks: 0.5,
  /**
   * Or when this share of its power is worn away: what an attack will pay before it is called off, and a defence
   * before it gives up the ground. Set so that a battle fought to its end costs each side about what the campaign's
   * bill charges for the result it hands back (the design director, b356; tests/battle-land.test.ts).
   */
  spent: { att: 0.13, def: 0.18 } as Record<'att' | 'def', number>,
  /**
   * But not while more than this share of its power stands fresh or has not fought: one worn block never takes a fresh
   * army with it (ruling 226(2), b476(1)).
   */
  fought: 0.5,
  /**
   * A block at this share of its strength or more, and steady, is fresh, in contact or not; in an army of this many
   * blocks or fewer (one block a fifth of it or more), the fresh hold the army at the brink as those not yet in the
   * fight do. Larger armies hold by those not yet in the fight alone: the fresh rule there moves the field 8 points off
   * its card, and at six blocks the hills-and-works practice ground 10 (the design director, b476(1) and b480; 356
   * campaign attacks and the nine practice grounds, 18 November). Never a defence behind fortress works or in a fort,
   * whose works the card prices already (b488: Verdun's three divisions behind works ran 8.6 points under their card).
   */
  fresh: { str: 0.9, upTo: 5 },
  /**
   * Burning wrecks stay (b477, Brian's pick): every tank a block loses leaves a wreck where it stood, to the end of the
   * fight. Men on foot beside one take light cover, either army's, at the field's own figure for broken ground: a block
   * takes it for the share of its tray that wrecks within its frontage, or 'beside' metres of it, can shelter (one wreck,
   * one model's worth), never better than its ground gives already. 'r' is how far a wreck's shelter reaches, for the 3D
   * game's mark.
   */
  wreck: { cover: 'rough' as Ground, beside: 10, r: 12 },
  /**
   * What a block that leaves the field still loses on the way (stragglers, wounded, guns and lorries left behind), as a
   * share of the strength it had left. A defence driven off its own ground leaves far more behind than an attack that
   * falls back to the lines it came from: the campaign's own bill has the same shape (resolveBattle in game.ts).
   */
  count: { routing: { att: 0.05, def: 0.05 }, gone: { att: 0.1, def: 0.15 } } as { routing: Record<'att' | 'def', number>; gone: Record<'att' | 'def', number> },
  /** Randomness of each blow, so near-even fights do not always go the same way: +- this share. */
  jitter: 0.1,
  /**
   * How much of the card's own run of luck the day carries (ruling 226(4)): 1 draws the day's luck across the whole run,
   * as the card's chance does, so a plain attack at the card's odds wins about as often as the card says.
   */
  fortune: 1,
  /**
   * A stretch of the defence's line is quiet when no enemy stands within this many metres of a block's own lane, left or
   * right, anywhere on the field: that block closes on the fight once the line is struck elsewhere (b407).
   */
  quiet: 350,
  /** Metres a block pressing an attack will go to take the next enemy when its own breaks. */
  press: 600,
  /** Seconds of battle time before the defender is counted as having held. */
  limit: 360,
  /** The computer general thinks once a second. */
  think: 1,
};
