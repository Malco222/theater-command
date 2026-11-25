// The land battle's numbers, in one place so they can be tuned (TODO(polish): tune against played battles and the
// fairness table; every value here is a first guess checked only by the parity test).
//
// Abstract on purpose (ruling 217): a block wears down the blocks it is in contact with at a steady rate set by its
// own power, the ground and which side it hits; nerve drains with losses. No weapons, rounds or ballistics.

import type { Ground } from './field';
import ground from '../../../data/campaign/ground.json';
import support from '../../../data/campaign/support.json';

/** The card's own figures for tanks with no infantry beside them (ground.json arms; support.json alone). */
const ARMS = (ground as unknown as { arms: { foot: string[]; alone: Record<string, number>; defence: { alone: Record<string, number> } } }).arms;
const SUPPORT_COVER = (support as unknown as { antiTank: { alone: { cover: number } } }).antiTank.alone.cover;

export type Kind = 'inf' | 'mot' | 'arm' | 'cav' | 'mtn' | 'fort' | 'art' | 'eng';
export type Motion = 'foot' | 'wheels' | 'tracks' | 'guns' | 'fixed';
/** A support team's arm (release 2): a machine-gun team or a mortar team. A ticket force of this kind makes one. */
export type Team = 'mg' | 'mor';
export const TEAMS: readonly Team[] = ['mg', 'mor'];

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
  /** Blows struck on the march, or on a block giving ground at the brink (b496(1)), and by a block that is wavering. */
  moving: 0.4,
  wavering: 0.7,
  /** Guns: their share against what they hit, the extra they take off nerve, and what they can do when the enemy is on top of them. */
  guns: { punch: 0.35, nerve: 1.5, close: 0.3, closeAt: 160 },
  /**
   * Support teams (release 2; Brian, 21 Nov 03:49Z: "units you can place at the begining of a battle on your side and
   * they can then attack from the back of the battle"; 03:57Z: built in the campaign apart from the infantry). A team
   * is a gun block ('art', so it stays back, fires on and is brought up into reach as guns are) with its own numbers:
   *   reach: metres between edges its fire carries. A machine-gun team's tripod guns reach over the heads of its own
   *     line; a mortar team's tubes lob further, short of the guns'.
   *   punch, nerve: as the guns' (above): its share against what it hits, and the extra it takes off nerve. Machine
   *     guns harm little and pin hard; mortars fall between.
   *   close, closeAt: its share when the enemy is within closeAt (a mortar cannot drop its bombs on a man beside it;
   *     a machine gun fights best close).
   *   armour: its share against tanks (bullets and light bombs do little to armour).
   *   speed (m/s, battle time, as LAND.speed), size (frontage and depth, m), figures (models on a full tray).
   * First guesses; the campaign's card prices the teams (stage 2), and the field only shares that out (b418).
   */
  /** The setup step (release 2): metres a placed gun or team keeps clear of every other block. */
  placeClear: 12,
  teams: {
    mg: { reach: 800, punch: 0.35, nerve: 2.5, close: 1, closeAt: 0, armour: 0.15, speed: 5, size: [70, 30], figures: 3 },
    mor: { reach: 1200, punch: 0.4, nerve: 1.8, close: 0.3, closeAt: 100, armour: 0.5, speed: 4.5, size: [80, 40], figures: 3 },
  } as Record<Team, { reach: number; punch: number; nerve: number; close: number; closeAt: number; armour: number; speed: number; size: [number, number]; figures: number }>,
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
  cover: { open: 1, field: 0.95, woods: 0.7, rough: 0.85, marsh: 1, water: 1.25, road: 1.05, bridge: 1.2, buildings: 0.55, rock: 0.75, bunker: 0.4, works: 1, blown: 1.25, suburb: 0.75, industry: 0.6, park: 0.9 } as Record<Ground, number>,
  pace: {
    foot: { open: 1, field: 0.9, woods: 0.6, rough: 0.7, marsh: 0.35, water: 0.35, road: 1.1, bridge: 1, buildings: 0.7, rock: 0.4, bunker: 0.5, works: 0.8, blown: 0.15, suburb: 0.85, industry: 0.75, park: 1 },
    wheels: { open: 0.9, field: 0.8, woods: 0.3, rough: 0.4, marsh: 0, water: 0, road: 1.5, bridge: 1.2, buildings: 0.5, rock: 0, bunker: 0, works: 0.5, blown: 0, suburb: 0.7, industry: 0.55, park: 0.85 },
    tracks: { open: 1, field: 0.9, woods: 0.4, rough: 0.6, marsh: 0.12, water: 0, road: 1.3, bridge: 1, buildings: 0.5, rock: 0, bunker: 0, works: 0.6, blown: 0, suburb: 0.75, industry: 0.55, park: 0.95 },
    guns: { open: 1, field: 0.9, woods: 0.4, rough: 0.5, marsh: 0.15, water: 0, road: 1.4, bridge: 1, buildings: 0.6, rock: 0, bunker: 0, works: 0.7, blown: 0, suburb: 0.75, industry: 0.6, park: 0.95 },
    fixed: { open: 0, field: 0, woods: 0, rough: 0, marsh: 0, water: 0, road: 0, bridge: 0, buildings: 0, rock: 0, bunker: 0, works: 0, blown: 0, suburb: 0, industry: 0, park: 0 },
  } as Record<Motion, Record<Ground, number>>,
  /**
   * Tanks are blind in the streets: what they strike and what they take there. Not in woods: the card prices tanks in
   * forest and on hills already (the campaign's ground rules), so the field draws them slow there but never prices them
   * again (b418). Priced twice, the panzers at Sedan carried the Meuse 21 points less often than their card said.
   */
  tanksClose: { punch: 0.6, take: 1.3, ground: ['buildings', 'industry'] as Ground[] },
  /**
   * Tanks that leave their infantry on close ground (the tactics panel, item 7; the design director, ruling 239(5)):
   * tanks see little in woods and streets, and hidden guns and close-assault teams kill those that come on alone. A
   * tank block in contact with enemy foot (the card's foot: 'foot'), with no friendly foot block within 'near' metres
   * (edge to edge), standing in woods or streets or fighting foot in fieldworks, takes that foot's blows times 'take',
   * the card's own figure for tanks alone there (1 over ground.json's arms.alone: forest for woods and fieldworks,
   * urban for streets; a defending tank the card's defence figures), on top of tanksClose. Not where that foot is
   * already in contact with friendly foot: the enemy is fixed, and a tank on its flank is the hook.
   * Never twice (b418): only the tank divisions the card counted as covered by their foot, and paid the attack for
   * fighting together, can be bitten: the card's foot divisions times 'cover' (1 on forest and urban cards, where
   * ground.ts covers a tank division with one of foot; support.json's 2 elsewhere; a defence 1), a regiment a third,
   * the first blocks first. A side with no foot has none covered: the card charged it already. On (b647): the plain
   * order lands within 0.1 point of 8df23b3 on every ground; the computer's tanks are bitten for 0.6% of their contact.
   */
  alone: {
    on: true,
    near: 200,
    foot: ARMS.foot,
    woods: ['woods'] as Ground[],
    streets: ['buildings', 'industry', 'suburb'] as Ground[],
    works: ['works', 'bunker'] as Ground[],
    take: {
      att: { woods: 1 / ARMS.alone.forest, streets: 1 / ARMS.alone.urban, works: 1 / ARMS.alone.forest },
      def: { woods: 1 / ARMS.defence.alone.forest, streets: 1 / ARMS.defence.alone.urban, works: 1 / ARMS.defence.alone.forest },
    } as Record<'att' | 'def', { woods: number; streets: number; works: number }>,
    cover: { close: 1, open: SUPPORT_COVER, def: 1, cards: ['forest', 'urban'] },
  },
  /**
   * Going in together (item 1; 7b): whether the player's group attack carries 'together', holding its tanks and lorries
   * to its foot's pace. Off (the design director, ruling 239's rider, 23:13Z, amending 239(5)): held to the slowest foot
   * of the whole order, the plain order lost 6 points on its card (15 on mountains), as LAND.hold was fitted with its
   * tanks reaching the line first. The player keeps his tanks with his foot by his own orders; the computer's groups
   * keep together by its drill (ai.ts).
   */
  together: { player: false },
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
   * telling odds behind a river land where they did (+5 and 0 against +6 and -2, the tests' seeds). 1.3 to 1.2, and
   * counted only as far as the attack's tanks and lorries cross, for b494: a river is only ever a cost, so every river
   * attack lands at or below the same attack with no river. Men on foot cross the field's rivers at no cost the field
   * needs to make up (the campaign's river attacks on foot, with no hold, land level with their no-river twins), while
   * an attack whose armour must cross pays dearly at the bridges; 1.3 lifted the telling odds 7.5 points and the
   * practice river 5 over their no-river twins, 1.2 leaves them level (+0.6) and below (-5.8).
   */
  hold: {
    take: { plains: 0.97, forest: 1.035, hills: 0.99, mountain: 1.12, marsh: 1.32, urban: 1.025, desert: 1.045 } as Record<string, number>, within: 300, river: 1.2, works: [1, 1.02, 1.06, 1.07],
    // A capital's and a city's streets (field.ts streetsOf): the card prices the place's ground, not the cover of its
    // houses, so the field takes that cover back off the held defence (b418; b511). With none, cities fell 4 points
    // under the card and capitals 17, most to the clock. A kind of ground may have its own, else the size's (b515(f)):
    // capital 1.1 is the plains capital's fit (48 attacks, captured and hand-made), and the hills' and forest's; an
    // urban capital's core is deepest; a mountain capital, a desert city and a marsh city hold on their own ground.
    // Fitted 18 November, 10 fights each, on 555 capital and city attacks captured from the campaign and hand-made on
    // every kind of ground: capitals -1.0 against the card, cities +0.2, every kind within a tenth (furthest: plains
    // cities captured -9.5, plains capitals captured +9.0 on 8); the practice city +2.0; every other place unchanged.
    // Next (b515(g)): a city's clock follows its built share, and these come back toward 1.
    streets: { city: 1.05, capital: 1.1, 'urban capital': 1.5, 'mountain capital': 1, 'desert city': 1, 'marsh city': 1 } as Record<string, number>,
    // A city's own river up the field beside the heart (b526): where the whole defence stands on one bank its line ends
    // on the river, a flank the attack cannot turn, and the card never priced the river, so the field takes that back
    // off the held defence too. An army broad enough to stand on both banks has no such flank. Fitted 19 November, 10
    // fights each on the 555 capital and city attacks with a river forced up every field the card left dry: 1 cost the
    // attack 2.2 points (mountain cities at close odds 10.7); 1.06 leaves it -0.1 against the same attacks with no river,
    // every kind within a tenth both ways (furthest: plains cities at close odds -6.7), fewer fights to the clock.
    // Re-measured with bridges by the record (b596, three): +0.25, every kind within a tenth, 58 fights to the clock
    // against 75 (1.045 would leave -0.4 but more capitals to the clock, 0 to 5 of 2,420).
    stream: 1.06,
    // A second river along the defence's flank, meeting one across the field behind it (b596(2), the Sava at Belgrade):
    // the defence drawn up beside it has its line's end on the water as up the field. Fitted 19 November on the same 555
    // attacks with the river across and a second one from the west edge on every field the card left dry, bridges 3
    // and 2: 1 left the attack where it was (+0.05) but 18 more fights of 5,550 ran to the clock (75 to 93); 1.06 (as up
    // the field) took the clock back (76) but gave the attack +2.3, two kinds at close odds over a tenth (forest capitals
    // +11.4, hills cities +13.4); 1.045 leaves +1.7, every kind within a tenth (furthest: hills cities at close odds +10.0), 81 fights to the clock.
    join: 1.045,
  },
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
   * Held there, such an army's blocks no longer fresh give ground rather than stand to be ground down (the design
   * director, b496(1): fought out, the Libyan Group at Mechili booked 1.27 of the staff's bill; now 1.06).
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
  /**
   * A capital's or a city's streets (field.ts layCity, streetsOf). inside: the metres inside the ragged edge of its core
   * that the defence's line stands, in the outer streets and never in front of the city (the design director, b529).
   * clock: a city's day is longer by this share of its field that is built (b515(g)): limit * (1 + clock * built), so
   * the march in through the streets and the fight in them do not end on the clock. Measured with the line inside the
   * edge over 555 city and capital fields x 10 seeds (b529's bars: every kind within a tenth of its card, close odds
   * too, and none on the clock more than 1 fight in 10): 0.15 holds both, the widest forest cities at close odds +8.9
   * and marsh cities at close odds 8.7% on the clock; 0.25 and 0.5 put the forest and marsh cities' close odds over
   * a tenth, 0 put marsh and mountain cities on the clock 1 fight in 8.
   */
  city: { inside: 100, clock: 0.15 },
  /** The computer general thinks once a second. */
  think: 1,
  /**
   * Air support (Brian, 2026-11-18 18:58Z; the design director, b533): a side's fighters on call strike when called,
   * the card having counted them already. While a side has strikes left, its blows hold back the card's share for them
   * (SideTicket.air.share), owed to the block each fell on; a strike pays what is owed where its run lands (sim.ts,
   * "Air support"). Seconds are battle time, metres field metres.
   *   first, every: the first strike is ready this long after the armies meet, each next this long after the last call.
   *   wait: a strike left ready this long, the staff call it themselves, for either side (b533(c): the enemy's too,
   *     never aimed sharper).
   *   eta: from the call to the run; speed: metres a second the flight flies, so the run takes its length over it;
   *     out: seconds it is seen pulling away after the run.
   *   len, minRun, wide: the run's longest and shortest, and the strip its guns rake, each side of its line half of it.
   *   near: pointed at a place, the run takes the frontage of the enemy block nearest it within this.
   *   close: no gun fires within this of the caller's own men; a run with less than 'least' of its line clear is refused.
   *   armour: tanks take this much of a strafing run (guns made for men in the open).
   *   nerve: the run's weight on nerve, as a blow from the front.
   *   alt, low: the flight's height coming in and on the run, metres (pictures only). models: planes in a flight;
   *     legs: the staff's strike splits into pairs, a pair down the frontage of each of the blocks owed most.
   *   toss: seconds from a bomb let go to its burst ('land:bomb-drop' then the 'shell'), pictures only.
   *   bombs: how many fall on the run's far third, pictures only (b533(f)): from the nation's first year of
   *     fighter-bombers, before it the run strafes alone. bombsFrom: that year by nation, '_' for every other, from the
   *     historian's list (/mnt/project-files/historian/fighter-bombers.md, 19:35Z): Germany from the Balkans and Russia,
   *     1941; Britain and America 1942; the Soviets from June 1941 (the certain date); Italy 1941; Japan 1943; France
   *     only with the Free French, 1943; Poland never in 1939; the generic fighter 1942.
   */
  air: {
    first: 40, every: 40, wait: 20, eta: 12, speed: 120, out: 6,
    len: 400, minRun: 100, wide: 60, near: 300, close: 60, least: 0.3, armour: 0.6, nerve: 1,
    alt: 150, low: 60, models: 4, legs: 2,
    bombs: 2, toss: 1.5,
    bombsFrom: { GER: 1941, UK: 1942, USA: 1942, SOV: 1941, ITA: 1941, JAP: 1943, FRA: 1943, POL: 9999, _: 1942 } as Record<string, number>,
  },
};

/** How far a block's fire carries: its team's reach, or its kind's. */
export const reachOf = (u: { kind: Kind; team?: Team }): number => (u.team ? LAND.teams[u.team].reach : LAND.reach[u.kind]);
/** Its gun numbers: its team's, or the guns' (only for a gun block). */
export const gunsOf = (u: { team?: Team }): { punch: number; nerve: number; close: number; closeAt: number } => (u.team ? LAND.teams[u.team] : LAND.guns);
