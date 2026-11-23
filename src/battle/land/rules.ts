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

/** How near a hedge line counts as on it, metres: a block hiding there (LAND.hide.hedge), a hedge point picked for a cut (LAND.cutters). */
const HEDGE_NEAR = 12;
/** Fire at a block found by its own fire but not yet located: its share, and for how long (LAND.hide.locate); smoke's share too (LAND.smoke). */
const LOCATE = { secs: 25, fire: 0.5 };
/** An army's section smoke (LAND.smoke.means): how far beyond its front it lays the line, how long the screen stands, and how many it lays a battle. */
export interface SmokeMeans { reach: number; hang: number; count?: number }

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
  /**
   * Direct command (release 2; Brian, 22 November: "I want to be able to click a unit and point where I want them to
   * go"). At the double: a 'move' with run goes LAND.double.pace faster by its motion (foot run; lorries and tanks at
   * their best; guns are towed as fast as they go), is struck 'exposed' harder while on the run (upright men in the
   * open), and strikes 'blows' as hard for 'winded' seconds after (men getting their breath). Orders by radio: an order
   * of the player's reaches a block 'delay' seconds after he gives it, 'cut' times that when its headquarters is hit
   * (wavering, or under 'hqAt' of its strength: the runners go instead of the wireless). Neither is felt unless the
   * player orders a run, or the battle is opened with radio on. 'nations': each army's wait as a multiple of 'delay',
   * by the last year it held ([through year, times], the first that covers the battle's year; 1 where not listed):
   * the historian's (22 November 05:20Z, 'L as held'): Soviet 1941-42 twice (few sets below battalion: telephone and
   * runners), Japanese twice, Italian one and a half, German infantry a quarter more (telephone and runners more than
   * wireless); 1943 on, the Soviet a quarter more as the German (ours). A tank block waits as its model's share with a
   * set talks (data/battle/armour.json radio, the modeller's sheet: a radio in every German, American and British
   * tank; two T-34s in five to 1943) and the rest as its army's foot.
   */
  double: { pace: { foot: 1.5, wheels: 1.2, tracks: 1.15, guns: 1, fixed: 0 } as Record<Motion, number>, exposed: 1.25, blows: 0.75, winded: 20 },
  radio: {
    delay: 5, cut: 3.5, hqAt: 0.5,
    nations: { SOV: [[1942, 2], [9999, 1.25]], JAP: [[9999, 2]], ITA: [[9999, 1.5]], GER: [[9999, 1.25]] } as Record<string, [number, number][]>,
  },
  /**
   * Measured armour (release 2; Brian, 22 November 04:03Z to 04:16Z; armour.ts): off unless the battle is opened with it
   * (LandBattle.measured). A tank block's blow on a tank block, or a gun's over open sights, weighs the chance its
   * model's gun gets through the face it strikes at that range against the field's par for that face, held within
   * [lo, hi] (a Sherman's 75 on a Tiger's front all but nothing, a Tiger's 88 on a Sherman twice par and more). The
   * computer's tank blocks go round to the flank or rear of a tank block whose front they weigh under 'flankBelow' at
   * 'flankAt' metres, where her side or rear weighs 'flankBetter' times her front or more ('flankFor': the most battle
   * seconds they spend getting there before they go in anyway).
   */
  armour: { lo: 0.15, hi: 3, flankBelow: 0.35, flankBetter: 2, flankAt: 200, flankFor: 150 },
  /**
   * Hidden in the woods (release 2; Brian, 22 November 04:18Z: "if you put units in the forest then they aren't visible
   * to the enemy until you get close or they start shooting. this allows you to do ambushes"). Off unless the battle is
   * opened with it (LandBattle.hideOn). A block standing in woods when the clock starts is hidden from the other side:
   * its blocks never strike it nor are sent at it, and its staff never plans against it. It is found when an enemy
   * block's edge comes within 'sight' metres of its own (by its motion: men lying still under the trees are seen last,
   * engines and tracks are heard first), when it opens fire, when it is struck, or when it leaves the woods; moving
   * inside them keeps it hidden. Until then it holds its fire till an enemy is within 'ambush' metres (by its motion; a
   * machine-gun team's 'mg'; 0: it never holds, as tanks fire at their reach and guns as they always do, and so gives
   * itself away) unless it is ordered to attack or fire. Its first 'surprise.secs' seconds of fire strike 'punch' times as hard and shake 'nerve' times as much: the
   * first volley from close cover at men in the open, the shock of an ambush. Infantry ambushes in close country were
   * sprung at 50 to 150 metres (the US Army's FM 7-10, 1944; German forest-fighting notes, 1943), and troops still and
   * camouflaged under trees were seldom seen beyond 50 to 100. The computer's staff, defending, puts up to
   * 'staff.blocks' foot blocks of its line into woods on the enemy's way in before the clock starts (up to 'staff.ahead'
   * metres forward or 'staff.back' back, 'staff.across' to either side, inside its own ground as the player's are), and
   * keeps every hidden block of its line where it is, holding its fire, until it is found. An attack that sees none of
   * the enemy goes forward 'staff.ahead' metres at a time until it finds them.
   *
   * The historian's check (22 November 05:20Z; the Normandy hedgerows, 1944: "they could not see the Germans, but the
   * Germans could see them"):
   * - A block on a hedge line (within 'hedge.near' metres of one: hedgerow, lane bank, wood edge, garden hedge or wall,
   *   where the 3D game has handed the field's hedges over) hides as in woods. In woods themselves (true forest, sight
   *   25 to 75 m) hidden foot and machine guns hold their fire to 'forest' metres; on a hedge line, to 'ambush'. The men
   *   drawn on foot cross a hedge at its nearest gate (within 'hedge.gap' metres of the gap's middle; looks only).
   * - A hidden block that opens fire is found but not yet located: the fire back at it strikes 'locate.fire' as hard for
   *   'locate.secs' seconds, or until an enemy comes within its 'sight'.
   * - A tank or lorry moving inside cover is heard at 'heard' metres (by its motion; 0: as 'sight').
   * - Its ambush shakes a block caught moving in the open 'surprise.open' times (not 'nerve'), and its side's mortar
   *   teams strike what the ambush is firing on 'surprise.mortar' times as hard in those seconds (pre-sighted on the gaps).
   * - Reconnaissance by fire: a block told to fire on a stretch of cover (the player's land:fire at a point; the
   *   computer's attack once it has been ambushed, at the cover ahead in its lane) strikes a block hidden within
   *   'search.reach' metres of it at 'search.blow', and finds it about 'search.odds' times in 'search.every' seconds.
   */
  hide: {
    sight: { foot: 60, wheels: 150, tracks: 150, guns: 100, fixed: 0 } as Record<Motion, number>,
    heard: { foot: 0, wheels: 0, tracks: 350, guns: 0, fixed: 0 } as Record<Motion, number>,
    ambush: { foot: 150, wheels: 150, tracks: 0, guns: 0, fixed: 0, mg: 200 } as Record<Motion | 'mg', number>,
    forest: 100,
    surprise: { secs: 20, punch: 1.5, nerve: 2, open: 2.5, mortar: 1.25 },
    locate: LOCATE,
    search: { reach: 60, every: 10, odds: 1 / 3, blow: 0.5, ahead: 300, lane: 150 },
    hedge: { near: HEDGE_NEAR, gap: 3 },
    staff: { blocks: 2, ahead: 400, back: 100, across: 200 },
  },
  /**
   * Night patrol (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it off if
   * someone doesnt want it"; the design director, 18:36Z, Q6 to Q8; LandBattle.patrol): off unless the battle is opened
   * with it, and it needs hiding on (LAND.hide): with hiding off nothing is hidden to find, so no patrol goes out and
   * nothing is drawn. The night before the battle the attack sends one reconnaissance patrol from a block of its line
   * on foot (the computer's always, from its most forward; the player's only by his order while he places his army):
   * settled once as the clock starts, on its own stream ('patrol:' and the seed), never the battle's dice. Two draws,
   * always both, found first: with chance 'found' the defence's most forward hidden post is found before a shot is
   * fired (it springs no ambush, and the attack's first plan counts it); with chance 'caught' the patrol is caught and
   * the defence is alerted, ready but not stronger (the card prices the digging already, b418): one more of its blocks
   * takes a hidden post by the computer's own ambush rule (LAND.hide.staff), a computer defence's reserve starts
   * toward the attack at once as a bold defence's does (styles defShift), and the patrol's block loses the patrol's men
   * from its strength (its nerve untouched). Both can happen: the rear man gets the report home (Infantry Training
   * 1937, sec. 35).
   *   found, caught: the design director's knobs (18:36Z, Q6). No army's rate is sourced; the only tally is approximate,
   *     the 38th (Irish) Brigade's war diary, Italy, December 1944, about 106 patrols: about 24% saw or found something
   *     and about 42% made contact (the tactics thread's readings, 22 November 18:40Z).
   *   size: men in a reconnaissance patrol, by army. USA 4 (FM 21-75, 1944, ¶86g: "three to six men"); the UK and her
   *     kin 3 ("an officer or sergeant and two men who go out at night": a British junior officer, Intelligence Bulletin,
   *     August 1943); SOV 4 ("three- to five-man teams": Sasso, Leavenworth Paper 6, p. 12); JAP 10 ("approximately one
   *     squad (10 men)": Intelligence Bulletin, July 1944); GER 10 (patrols "not less strong than a squad": German
   *     Motorized Infantry Regiment, MIS 1942, ¶40); FRA and VIC 2 (night patrols "by riflemen alone": Passmore 2025;
   *     the count a knob); ITA 4 (unsourced: knob); every other army '_' 4 (knob). Kin follow Britain, Vichy France.
   *     Its pictures, and the men its block loses when it is caught.
   *   night: battle seconds the patrol is out, before the clock starts (negative: the night before). Looks only, a knob.
   */
  patrol: {
    found: 0.4,
    caught: 0.3,
    size: { USA: 4, UK: 3, CAN: 3, AUS: 3, NZL: 3, SAF: 3, IND: 3, SOV: 4, JAP: 10, GER: 10, FRA: 2, VIC: 2, ITA: 4, _: 4 } as Record<string, number>,
    night: { t0: -2400, t1: -600 },
  },
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
   * Overrun (release 2; Brian, 23 November 05:05Z: "vechiles can run over enemy soldiers though which would kill them";
   * the design director, 05:48Z): with LandBattle.escort on, a tank block fighting a foot block in the open, with its own
   * foot within LAND.alone.near and no enemy weapon that can hurt a tank by direct fire in its own reach of the way,
   * drives through that block's ground to its far edge and backs out to its place: once per foot block per contact
   * (until the block breaks or no tank of the enemy is in reach of it), every side, staffs and player alike. Not where the
   * foot carries its own anti-tank weapon. Harm unchanged (b418): the overrun is a picture of losses the rules already
   * take (the men taken from that block while a tank is on its ground die under her tracks, Drill.thin); the blows
   * between the two are struck as between blocks standing face to face. 'reverse': its pace backing out, a share of its
   * pace forward. 'at': from when to when a nation's foot carries its own anti-tank weapon, [from, to) as the year and
   * twelfths (the design director: looks gating only until the historian checks the dates on resumption; the
   * Commonwealth's foot carries Britain's): the US bazooka from November 1942; Britain's Boys rifle 1939-42, then the
   * PIAT; Germany's anti-tank rifles 1939-42, the Panzerschreck and Panzerfaust from late 1943; the Soviet anti-tank
   * rifles from 1941. Japan, Italy and the rest none until the historian names one.
   */
  overrun: {
    reverse: 0.5,
    at: {
      USA: [[1942 + 10 / 12, 9999]],
      UK: [[1939, 9999]],
      GER: [[1939, 1943], [1943 + 9 / 12, 9999]],
      SOV: [[1941, 9999]],
    } as Record<string, [number, number][]>,
    commonwealth: ['CAN', 'AUS', 'NZL', 'NZ', 'SAF', 'IND'],
  },
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
  /**
   * The battle's scale (the true-scale plan, GR1; Brian's standing rule of 22 November 04:14Z: on-screen numbers reflect
   * the units each side brings): one N a battle, the men both sides bring over the figure budget, rounded up to a
   * multiple of step (1 when they fit the budget whole). Looks only: the drill reads it (no block draws more men than
   * its men0 over N) and the recording states it; nothing in the fight does. Tanks and guns join the count with GR2's
   * table; lorries never do (GR6). page: the 2D field page's own budget (GR5, the production lead's option A of 22
   * November 08:03Z): every block's men as plain dots, its men0 over the page's one N, thinning with its strength, with
   * no drill; the Kursk salient's 1,050,000 men draw about 23,400 dots at 1 to 45.
   */
  scale: { budget: 8500, step: 5, page: 24000 },
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
   * Neighbours join the fight (release 2): metres a corps from next door walks onto the field from its edge before it
   * stands as a reserve; how far in from a back edge it comes on; metres between its blocks; and how far it looks for
   * clear dry ground to come on at, in field cells in from the edge and along it either way.
   */
  join: { walk: 200, edge: 40, gap: 40, inward: 24, across: 20 },
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
  /**
   * Hedge cutters (release 2; Brian, 22 November 14:19Z: "bladed Shermans punch a gap through a hedgerow where the player
   * picks"; the design director's ruling 242, C1-A to C4-A; LandBattle.cutters): off unless the battle is opened with
   * them. A tank block carries cutters from turn 'from' on a field with hedges, whatever its army (C2-A: Brian's standing
   * rule that the battle's mechanics are every playable nation's, 21 November 06:54Z). Told to cut where a hedge line
   * runs (within 'near' metres of one), it drives for that point and on through; as its front reaches the hedge the cut
   * is begun (t0), and 'secs' battle seconds later a gap 'width' metres wide stands open there (t1, a Gate of kind
   * 'cut'), which the drawn men of every block on either side walk through from then. The staffs' tank blocks cut
   * wherever their own way crosses a hedge line with no gap near (C4-A). Looks only (C1-A): nothing in the fight reads
   * a gap, so the fight is the same with the switch on or off. The bocage (C1-B, built as the ruling's 17:45Z amendment
   * asks, "unfitted tanks cross only at gaps"; its own switch, LandBattle.bocage, measured against off): a tank block
   * without cutters crosses a hedge line only at a gap, a gateway or a cut once open, and goes round to one; this is
   * where cutters (and a cut's gap) change the fight, and only with that switch on.
   *   from: turn 59, July 1944. Bradley saw the Culin cutter on 14 July and held it back until Cobra, 25 July 1944; by
   *     then three of every five First Army tanks carried one (Blumenson, Breakout and Pursuit, ch. XI).
   *   share: three tanks in five carried one at Cobra (Blumenson; Zaloga, Operation Cobra 1944): how many of a block's
   *     drawn tanks wear the blades, sent to the 3D game as the block's cutter flag (the bridge's unitOut). Looks only.
   *   secs: battle seconds a cut takes, about 25 real seconds at LAND.smoke.perMin. No source gives one (the historian,
   *     H1, 22 November: not found in ten sources; the one timed sortie bounds a breach and the drive to the next bank at
   *     about 3 real minutes): the design director's looks-only knob (ruling 242 C3), until H1 is sourced.
   *   width: the gap, metres: the 3D game's own gateway width (an M4 is about 2.6 m wide; no source gives a gap's width:
   *     ruling 242 C3).
   *   near: how near a hedge line the point picked must lie, metres: the hedge line's own (LAND.hide.hedge.near).
   */
  cutters: { from: 59, share: 0.6, secs: 6, width: 3.2, near: HEDGE_NEAR },
  /**
   * Smoke on call (release 2; Brian, 22 November 14:19Z: "a section lays a smoke line to cover a crossing, when and where
   * the player orders it"; the design director's ruling 242, S1-A to S6, amended 17:45Z from the historian's answers;
   * LandBattle.smoke): off unless the battle is opened with it. A block on foot, or lorried infantry, whose army has the
   * means lays a screen across the ground before it: a line no longer than its own front (S2), its middle within its
   * army's 'reach' of its front, standing 'hang' battle seconds; one at a time, and 'count' a battle (S3). While a screen
   * stands, any blow struck straight at its target (direct fire: foot, tanks, a machine-gun team, a gun over open
   * sights) whose line from the shooter to the target crosses it strikes at 'share' (S1-A); fire from far back (guns
   * beyond LAND.guns.closeAt, mortar teams, the air, on fire plans laid before the smoke: the German 1940 handbook) is
   * not touched. It gives no cover and does no harm. With hiding on, nobody is seen through it (the build spec's reading
   * of S1-A; it draws nothing of the dice itself, but a block it keeps hidden is not fired on till found, so the draws
   * after it change as the fight does: LandBattle's Smoke on call). Both staffs lay one as a
   * foot block of theirs goes in to the assault (S5-A); never a defence in its works.
   *   share: fire at a place, not a seen target: the rules' existing share for fire back at a block found but not
   *     located (LAND.hide.locate.fire; the German squad manual: if tanks lay smoke, "fire into the smoke"). Never twice
   *     on one blow: a blow already that vague, or fire on cover at a block still hidden (already a place, not a seen
   *     target: LAND.hide.search.blow), is not halved again. Only the ground's part of a blow: the share a side holds
   *     back for its air strikes is held back whole (the air is not touched).
   *   count: screens a block lays in a battle where its army's means gives none: the design director's knob, pending H7
   *     (ruling 242 S3; the historian found no section load for any army but Britain's).
   *   perMin: battle seconds to a real minute, turning the sources' burn times into battle time (ruling 242 S2, answering
   *     H6): foot moves at 6 m/s of battle time (LAND.speed.inf) against US quick time of 120 steps of 30 inches a minute,
   *     1.52 m/s (FM 22-5).
   *   front: an army that sets its smoke down along its own front (reach 0) lays it there for a line drawn on its own
   *     ground or within this many metres of its front, and refuses one drawn further off (129(d)): a throw, the thrown
   *     reach of the others' grenades (the historian, D: the Italian pot "is thrown or set down, so the 30 m thrown reach
   *     fits").
   *   means: by army: reach, metres beyond its front the line's middle may lie (0: it is laid along its own front);
   *     hang, battle seconds it stands (real minutes times perMin); count, where the army's own load is sourced.
   *     USA: the M19A1 WP rifle grenade to about 195 m (OP 1664); the AN-M8 HC grenade burns 105 to 150 s (FAS), about
   *       2 real minutes. (The 60 mm WP shell came only in January 1945.)
   *     UK: the platoon's 2-inch mortar, smoke to 500 yd, 457 m (canadiansoldiers.com), about 2 real minutes a bomb (the
   *       1966 pamphlet); 6 smoke bombs and 4 smoke grenades a platoon (Memorandum on Platoon Battle Drill, 1942), about
   *       3 a section (the historian, D; ruling 242 as amended: Britain and her kin 3 screens).
   *     GER: smoke hand grenades and candles, thrown (Nebelhandgranate 39, 1 to 2 minutes: Ordnance Bulletin 59,
   *       Intelligence Bulletin January 1943); the 5 cm mortar had no smoke round (MIS Special Series 14).
   *     SOV: the RDG-1 grenade, about 1.5 minutes, about 30 yd of screen, thrown (Intelligence Bulletin, June 1946).
   *     JAP: the Type 89 discharger's smoke round, 45 to 206 yd (Intelligence Bulletin, June 1943); no burn time found
   *       (the historian, H5): 22 s is the design director's knob.
   *     ITA: the 2-kg smoke pot, set down or thrown, burning 1 min 15 s (TTT 38, November 1943): laid along its own
   *       front (ruling 242 as amended).
   *     France 1940 has none (no section smoke in the 1940 platoon or weapons lists: the historian, H5), and no army
   *     not listed has any: no stand-in until a means is found (ruling 242 S2).
   *   kin: a nation that takes its senior partner's means (ruling 242 S2, "the 240 form", as plans.kin on the air
   *     branch's arsenal.json): Canada, Australia, New Zealand, South Africa and India Britain's; Vichy and Free France
   *     France's (none today; theirs too once H5 finds France a means).
   */
  smoke: {
    share: LOCATE.fire,
    count: 2,
    perMin: 15,
    front: 30,
    means: {
      USA: { reach: 195, hang: 30 },
      UK: { reach: 457, hang: 30, count: 3 },
      GER: { reach: 30, hang: 22 },
      SOV: { reach: 30, hang: 22 },
      JAP: { reach: 188, hang: 22 },
      ITA: { reach: 0, hang: 19 },
    } as Record<string, SmokeMeans>,
    kin: { CAN: 'UK', AUS: 'UK', NZL: 'UK', SAF: 'UK', IND: 'UK', VIC: 'FRA', FFR: 'FRA' } as Record<string, string>,
  },
  /**
   * Field telephone lines (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it
   * off if someone doesnt want it"; the design director's rulings, 18:36Z, PHONES Q1 to Q3; LandBattle.phones): off
   * unless the battle is opened with them. Wire was the defence's own means ("Telephones are usually installed down to
   * companies along the main line of resistance", FM 7-25 ¶52; Cherbourg 1944, "Approximately 95% of communications was
   * by wire", Signal Corps Report 63), so as the clock starts every block of the defence on the field has a wire from its
   * headquarters at the middle of its zone's back edge; a corps that comes up from next door has none. The wire head is
   * "pushed close" (FM 7-24 ¶68h): it follows its block, and moving never cuts it. A blow on the block from an enemy gun
   * block or mortar team cuts a line at 'cut' (Q1; machine guns never), and an enemy tank block whose step crosses the
   * wire cuts a line outright (FM 24-20 ¶32d: surface lines "often are broken by passing troops and vehicles"). Once its
   * lines are all cut the wire is down, and every order to that block, the player's or a staff's, waits its army's
   * fallback from when it was given: the wireless (LAND.radio's wait, as the radio switch reckons it) or a runner (the
   * metres from the headquarters over 'runner'). A lineman leaves the headquarters at the first cut, walks the wire to it
   * at foot pace (LAND.speed.inf), splices it in 'splice' seconds and the wire is up again: what was waiting goes at once.
   *   cut: the chance a line is cut by each such blow (each tick's blow of a gun block or a mortar team on the block at
   *     the wire's end): knob (DD 18:36Z). No source counts breaks: the sources say lines were "repeatedly cut by enemy
   *     artillery" (84th Division, Battle Experiences 1945) and "Heavy fire ... was constantly cutting our lines" (100th
   *     Division, 1945).
   *   splice: battle seconds a splice takes: FM 24-20 (1944) ¶15c(3) and ¶15c(5), preparing the splice "should not take
   *     more than 1 minute" and tying it "should be about 1 minute": about 2 real minutes, at LAND.smoke.perMin 15
   *     battle seconds a real minute. Finding the break is not counted (no source).
   *   runner: metres a battle second a runner goes: the rules' own foot speed (LAND.speed.inf); knob (DD 18:36Z).
   *   lines: lines to each block, by army. Germany 2 (DD 18:36Z, Q3, P6): "in the danger zone, therefore, double lines and
   *     heavy insulated wires are used" (German Tactical Doctrine, MIS): a German wire needs two cuts before orders wait.
   *     Every other army 1 ('_').
   *   fallback: by army, [through year, 'radio' or 'runner'], the first that covers the battle's year (as LAND.radio's
   *     nations): how orders go once the wire is down.
   *     USA: runners to 1942, the wireless from 1943: the SCR-536 and SCR-300 "became available in substantial quantities"
   *       only "well into 1943" (Tactical Notebook); FM 7-25 ¶49b, "radio and messengers are the primary means".
   *     UK and her kin (CAN, AUS, NZL, SAF, IND) the wireless, every year: No. 18 sets at company and platoon headquarters
   *       (the Calgary Highlanders' signals platoon, canadiansoldiers.com).
   *     GER: runners to 1942, the wireless from 1943 (the Feldfu b/c pack sets "in the forward echelon", TTT 46, 1944):
   *       "When wire connections fail or are not functioning steadily, the radio is indispensable" (German Tactical Doctrine).
   *     SOV runners: 9,586 sets made in 1941 (SPbGUT); "a broken line often meant losing touch with subordinate troops"
   *       (3DNews). JAP runners: 0 to 4 sets a battalion (TTT 46), "a pool of runners" at company (MIS Bulletin 16).
   *     ITA runners (nothing sourced but the telephone). FRA and VIC runners: "only French heavy tanks had radios" in 1940
   *       (GlobalSecurity). Every other army runners ('_'): knob (DD 18:36Z).
   */
  phones: {
    cut: 0.12,
    splice: 30,
    runner: 6,
    lines: { USA: 1, UK: 1, CAN: 1, AUS: 1, NZL: 1, SAF: 1, IND: 1, GER: 2, SOV: 1, JAP: 1, ITA: 1, FRA: 1, VIC: 1, _: 1 } as Record<string, number>,
    fallback: {
      USA: [[1942, 'runner'], [9999, 'radio']], UK: [[9999, 'radio']], CAN: [[9999, 'radio']], AUS: [[9999, 'radio']], NZL: [[9999, 'radio']], SAF: [[9999, 'radio']], IND: [[9999, 'radio']],
      GER: [[1942, 'runner'], [9999, 'radio']], SOV: [[9999, 'runner']], JAP: [[9999, 'runner']], ITA: [[9999, 'runner']], FRA: [[9999, 'runner']], VIC: [[9999, 'runner']], _: [[9999, 'runner']],
    } as Record<string, [number, PhoneFallback][]>,
  },
  /**
   * Stretcher bearers (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it off";
   * the design director's rulings Q4 and Q5, 18:36Z, and 18:38Z on the light cases; LandBattle.bearers): off unless the
   * battle is opened with them. Of each loss of strength a block suffers, 'killed' are dead and the rest wounded; of the
   * wounded, light.share are dressed and back in the line light.dress seconds later (the only same-battle return in the
   * record, Q5), and the rest are litter cases waiting for the block's bearers. Its bearers ('share' of its strength at the
   * start, counted in its strength) carry them back 'litter' to a case, out to the aid post 'aid' metres behind where it
   * started and back at 'pace': while they are out the block's own blows weaken by the men away (never its strength, its
   * nerve, its count or its size). Nothing here draws on the dice.
   *   killed: the share of each loss killed, by army (the design director, 19:14Z, from the historian's
   *     /mnt/project-files/historian/three-ideas-readings-1122.md; died of wounds counted as killed). The drill draws
   *     the rest of the men who fall as wounded. Sourced, no knob of ours:
   *     USA 0.192: 4.22 wounded to 1 killed (Dupuy).
   *     UK and her kin (CAN, AUS, NZL, SAF, IND) 0.230: 3.35 to 1 (Hansard, 27 June 1944, Italy to Rome).
   *     GER 0.279: 2.58 to 1 (OKW to January 1945, all fronts).
   *     JAP 0.271: 2.69 to 1 (China, 1937 to 1941).
   *     '_', every other army (SOV, ITA, FRA, VIC and the rest): 0.227, the middle of the laid ratios, 3.40 to 1.
   *   share: bearers as a share of a block's men, by army, sourced in kind and rounded:
   *     UK and her kin (CAN, AUS, NZL, SAF, IND) 0.025: a corporal and 19 privates a battalion, "all of these were
   *       infantrymen" (bayonetstrength.uk, The Organisation of the British Infantry Battalion 1938-45, p.10;
   *       britishmilitaryhistory.co.uk, the battalion HQ's medical list).
   *     GER 0.03: a Krankenträger in each platoon headquarters and the company's medical NCO (KStN 131n, 1 May 1944,
   *       battleorder.org): 4 of a company of 142.
   *     SOV 0.035: a medical instructor and 4 orderlies a company of 143 (shtat 04/550, 10 December 1942, battleorder.org).
   *     USA 0.02: two company aid men a company (FM 8-10, 1942, para 28d(1), 33b) and the battalion's 12-man litter
   *       squad (med-dept.com, The WW2 Aid Station).
   *     JAP 0.02: a medical orderly a platoon (TM-E 30-480, Handbook on Japanese Military Forces, evacuation).
   *     ITA, FRA and VIC 0.0275: unsourced; knob (DD 18:36Z), the middle of the sourced 2 to 3.5% (Q4).
   *     '_', every other army: 0.0275, the same knob (DD 18:36Z).
   *   litter: bearers to a stretcher (FM 8-10 para 32a: "Litter squads consist ordinarily of four bearers each").
   *   aid: metres from the block's start place to its aid post, toward its own back edge, kept on the field: the middle
   *     of FM 8-10's 300 to 800 yards (para 31b(3)), 550 yards (the design director, Q4, not the 350 proposed).
   *   pace: metres a battle second out and back: FM 8-10 para 32c, a round trip of 500 yards each way "at least twenty
   *     minutes", 914.4 m in 300 battle seconds (15 battle seconds a real minute). A round trip to 503 m: 330 s.
   *   light: the lightly wounded, back in the same battle (Q5: the only same-battle return; no bigger share for feel):
   *     share: of the wounded, the US "carded for record only", 17.1% (123,836 of 723,560 wounded: the Dupuy Institute,
   *       Carded for Record in WWII): knob (DD 18:36Z).
   *     at: metres from the block where they are dressed. The design director, 18:38Z: "Light cases are dressed at the
   *       block by the company's aid man, or by themselves, and are back in the line after the 30 s dressing knob, with
   *       no walk ... every soldier carried a field dressing." Kept as a knob (DD 18:36Z): 0, no walk.
   *     out, back: their pace going and coming back, times the foot's (LAND.speed.inf), read only where at > 0:
   *       knobs (DD 18:36Z).
   *     dress: battle seconds to dress a wound: knob (DD 18:36Z).
   */
  bearers: {
    killed: { USA: 0.192, UK: 0.23, CAN: 0.23, AUS: 0.23, NZL: 0.23, SAF: 0.23, IND: 0.23, GER: 0.279, JAP: 0.271, _: 0.227 } as Record<string, number>,
    share: { USA: 0.02, UK: 0.025, CAN: 0.025, AUS: 0.025, NZL: 0.025, SAF: 0.025, IND: 0.025, GER: 0.03, SOV: 0.035, JAP: 0.02, ITA: 0.0275, FRA: 0.0275, VIC: 0.0275, _: 0.0275 } as Record<string, number>,
    litter: 4,
    aid: 503,
    pace: 3.048,
    light: { share: 0.17, at: 0, out: 0.5, back: 1, dress: 30 },
  },
};
/** How orders go to a block whose telephone wire is down (LAND.phones.fallback): by the wireless or by a runner. */
export type PhoneFallback = 'radio' | 'runner';
/** Field telephone lines: the lines to each block of this army (LAND.phones.lines; '_' for an army not listed). */
export const phoneLines = (nat: string): number => LAND.phones.lines[nat] ?? LAND.phones.lines._;
/** Field telephone lines: how this army's orders go in this year once a wire is down (LAND.phones.fallback). */
export const phoneFallback = (nat: string, year: number): PhoneFallback => (LAND.phones.fallback[nat] ?? LAND.phones.fallback._).find(([to]) => year <= to)?.[1] ?? 'runner';

/** How far a block's fire carries: its team's reach, or its kind's. */
export const reachOf = (u: { kind: Kind; team?: Team }): number => (u.team ? LAND.teams[u.team].reach : LAND.reach[u.kind]);
/** Night patrol: the men in an army's reconnaissance patrol (LAND.patrol.size; every army not listed, '_'). */
export const patrolSize = (nat: string): number => LAND.patrol.size[nat] ?? LAND.patrol.size._;
/** Its gun numbers: its team's, or the guns' (only for a gun block). */
export const gunsOf = (u: { team?: Team }): { punch: number; nerve: number; close: number; closeAt: number } => (u.team ? LAND.teams[u.team] : LAND.guns);
/** An army's section smoke (LAND.smoke): its own, or its senior partner's (kin); null where it has none. */
export const smokeMeans = (nat: string): SmokeMeans | null => LAND.smoke.means[LAND.smoke.kin[nat] ?? nat] ?? null;
/** An army's stretcher bearers, as a share of a block's men (LAND.bearers.share): its own, or every other army's. */
export const bearerShare = (nat: string): number => LAND.bearers.share[nat] ?? LAND.bearers.share._;
/** An army's share of each loss killed, the rest wounded (LAND.bearers.killed): its own, or every other army's. */
export const killedShare = (nat: string): number => LAND.bearers.killed[nat] ?? LAND.bearers.killed._;
