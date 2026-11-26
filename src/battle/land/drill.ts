// The men inside each block: the squad brain (Brian, 20 November 2026, 20:43Z: "lets make this super realistic and
// meaningful and everyone has high AI iq"). A block is a division or a regiment, and the rules fight it as one; this
// file says what each of its men, tanks and guns is doing, second by second, so the 3D game can draw soldiers who
// fight as soldiers (the art director, 17 November 17:56Z: "no tray ... the men move, kneel and take cover as men").
//
// Looks only (ruling 217: the card decides, the field acts it out). Nothing in the fight reads anything here, and it
// draws nothing from the battle's own dice: every choice is a hash of the battle's seed, the block, the man and the
// moment, so a battle replays man for man, and a battle fought with the drill on ends exactly as one with it off
// (tests/battle-land-drill.test.ts). It is made only when something asks for it (LandBattle.drillOn: the 3D bridge,
// the browser page); the fits and the campaign's own runs never pay for it.
//
// Every pose is a true tell of the numbers: men go to ground when their block is losing men fast or is shelled, they
// work forward onto a block whose nerve is going, they run when it routs. What the player sees is what the rules hold.
//
// The principles (the same for every army; a style and experience only change the numbers, data/battle/drill.json):
// - Fire and movement: one team keeps the enemy's head down while the other rushes from cover to cover; the machine
//   guns are the base of fire and never rush with the assault.
// - Cover: every man looks round his place in the line for the best shelter within reach of it: fieldworks and
//   bunkers, the edge of a wood or of the houses facing the enemy, a fold of ground, a burnt-out tank, a shell hole.
// - Suppression: a block under heavy fire goes to ground; men caught in the open crawl to cover.
// - The assault: riflemen work forward onto the nearer flank of the block they are fighting and throw grenades from
//   close range; when it wavers they close on it.
// - Tanks: pairs leapfrog, one halting to cover the other; they fight from hull-down behind a crest where the ground
//   has one, back off to cover after firing from the open, and in woods and streets stay behind their infantry.
// - Anti-tank ambush: a holding block's anti-tank team waits hidden on the flank the enemy tanks come at, and fires
//   only when they are close.
// - Guns: a battery stands behind a crest or a wood's edge, fires by its style, and sends a spotter forward to the
//   friendly foot nearest its target, on the highest ground or the tallest house there.
// - Breaking: routing men run for their own edge; men falling back in order go by bounds, a rearguard covering them.
// - Lorries and horses: lorried foot ride in their lorries until their block slows near the enemy, then jump down;
//   cavalry ride up and get off to fight, a man in four leading the horses back. When their block goes on faster
//   than men on foot can follow, they mount again: the lorries drive up for them, the holders bring the horses.
// And they move as bodies do (the 3D pre-check, 20 November 22:35Z): no man is thrown, no one turns about in a tick, a
// tank pivots before she drives and backs into a place behind her, and a man left outside his block when it turns runs
// back to it on his own feet.

import data from '../../../data/battle/drill.json';
import { DT, type Side, type Zone } from '../core/types';
import { GROUNDS, type Ground, type LandField } from './field';
import { LAND, MOTION, bearerShare, killedShare, type Kind, type Team } from './rules';
import type { LandBattle, Screen } from './sim';
import type { LandUnit } from './units';
import { fateOf } from './armour';
import { BOOK, Suppression, boundOf, missionStep, schoolNums, turnOf, turns, type Bound, type Mission } from './tactics';
import { REACH } from './shells';
import { turnToDate } from '../../core/ticket';
import type { Calibre } from '../core/types';

/**
 * What a man, tank, gun or lorry is in his block. 'hmg' and 'mortar' (release 2: a support team's, Drill.teams) are a
 * crew-served weapon each, drawn as the weapon with its crew of three: a machine gun on its tripod, a mortar's tube on
 * its baseplate. Struck, they are men (a round at one lands near; only the rules take one).
 *
 * With the men drawn as sections (release 2, Drill sections; the 3D ground battle scene for Brian, 21 November 18:29Z:
 * "the ground battle needs a lot more soldiers in it"), each model is the key man of a section, the rest of it standing
 * round him: 'leader' a rifle section's leader (its key man; his weapon the 3D game's by nation), 'loader' an automatic
 * weapon's number two or an ammunition bearer (an anti-tank launcher's, a support team's), 'crew' a field gun's crew.
 */
export type Role = 'officer' | 'rifle' | 'mg' | 'at' | 'sapper' | 'holder' | 'tank' | 'gun' | 'lorry' | 'fo' | 'hmg' | 'mortar' | 'leader' | 'loader' | 'crew';
/**
 * What he is doing, as the 3D game plays it. The men's are the soldier's own clips (battle-models/land/soldier/README.md)
 * and four held poses: 'wait' (kneeling, rifle up, watching: fire_kneel's aim held without a shot), 'hide' (lying
 * still in cover: fire_prone's aim held, no shot), 'observe' (a spotter lying with his glasses on the target),
 * 'mounted' and 'ride' (in a lorry, on a horse), 'horses' (a horse holder), and 'down' (fallen). The vehicles' and
 * guns': 'drive', 'halt', 'fire', 'reverse', 'towed', 'deployed', 'parked'. A support team's weapon and crew: 'walk',
 * 'run' or 'flee' carried by hand, 'wait' set down on the march, 'deployed' set up, 'fire' firing, 'down' (fallen).
 */
export type Pose =
  | 'idle' | 'walk' | 'run' | 'advance' | 'creep' | 'crawl' | 'flee' | 'fire_stand' | 'fire_kneel' | 'fire_prone' | 'lmg_burst'
  | 'throw_grenade' | 'cower' | 'wait' | 'hide' | 'observe' | 'mounted' | 'ride' | 'horses' | 'down'
  | 'drive' | 'halt' | 'fire' | 'reverse' | 'towed' | 'deployed' | 'parked'
  // Field telephone lines (release 2): a lineman walking his wire out with the cable reel, and kneeling at the cut, splicing.
  | 'reel' | 'splice'
  // Stretcher bearers (release 2; LandBattle.bearers): a wounded man lying for the bearers (or for his dressing), the man
  // on the stretcher (head first, his head toward the front bearer), and the stretcher's front and rear bearers.
  | 'wounded' | 'carried' | 'stretcher_front' | 'stretcher_rear';
/** What the block as a whole is doing, as its men see it. */
export type Phase = 'column' | 'deploy' | 'bound' | 'startline' | 'fight' | 'hold' | 'rest' | 'pinned' | 'fallback' | 'flee' | 'rally' | 'gone' | 'battery' | 'limbered';
/** What shelters a man where he stands. */
export type CoverKind = 'none' | 'works' | 'building' | 'woods' | 'rock' | 'rough' | 'field' | 'wreck' | 'crater' | 'crest';

/** One man, tank, gun or lorry. */
export interface Man {
  /** His place on his block's tray (model k; the rules lift model k off when the block falls to k models). Lorries 100+. */
  k: number;
  /**
   * In a section (Drill sections), his place in model k's section from 1; the key man (the model the rules lift) has
   * none. The 3D game tells a man by k and j together.
   */
  j?: number;
  /** His own number, for the drill's dice: k for a key man, a lorry, a spotter; above 1000 for the rest of a section. */
  n: number;
  /** The section he is with now (its key man's k): his own, till its key man falls and he joins another. */
  sec?: number;
  /** He keeps by his section's weapon or officer (a loader, a gun's crew, the officer's runners): Drill.pairs places him. */
  pr?: true;
  /** One of a section's scouts, out ahead of it on the move. */
  scout?: true;
  /**
   * Drawn as platoons (Drill platoons): which of the platoon's groups he is in. None (0) for the key man's own squad;
   * then its other squads from 1, and last its headquarters.
   */
  sub?: number;
  /** A squad's leader or the platoon's (drawn as a leader): his group forms on him. */
  lead?: true;
  /**
   * A key man drawn with men of his section (hasSec: he shows as its leader where he is a rifleman) and of his platoon
   * (plat: its squads take their turns by squad). Under the battle's scale (GR3) a block's keys may differ.
   */
  hasSec?: true;
  plat?: true;
  /** Lorried foot drawn as platoons: his lorry (one to each squad and to the headquarters) and his place on its benches. */
  lor?: number;
  seat?: number;
  /**
   * The model the 3D game draws her with where it is not her block's (release 2, looks only): a carrier of armoured
   * infantry (LandBattle.mech; absent: the lorry), a battery's self-propelled gun (LandBattle.spg: no crew, no tractor),
   * an armoured block's tank destroyer (LandBattle.td), a block's armoured car, AA vehicle, jeep or motorcycle.
   */
  ml?: string;
  /** Her model's swaps as the drill picks them, where not the model's own choice (a motorcycle's sidecar, a staff jeep's gun). */
  sw?: Record<string, string>;
  /** A jeep's or motorcycle's work (LandBattle.light): riding out ahead with the screen, parked behind, or despatch. */
  duty?: 'ahead' | 'behind' | 'despatch';
  /** A tank's firing post (0 her own place, then one side and the other), the shots she has fired from it, and where she is
   * in her move to the next: backing out of sight, crossing behind the line, coming up to it. */
  post?: number;
  shots?: number;
  cyc?: 'back' | 'over' | 'up';
  role: Role;
  /** 0 the block's headquarters, 1 and 2 its two teams. */
  team: 0 | 1 | 2;
  x: number;
  y: number;
  /** Radians, 0 north, clockwise (as a block's face). */
  face: number;
  pose: Pose;
  /** Metres a battle second he is moving now. */
  v: number;
  /** Where he is going, or the place he holds. */
  spot: [number, number];
  /**
   * On a field with hedges (the Normandy bocage), his way through the first gate on his way to his spot: a stride short
   * of the gap and a stride through it, then on (Drill.via); the spot and the place he stood when it was found. None
   * where his way meets no hedge, or no gate serves it.
   */
  via?: [number, number][];
  viaFor?: [number, number];
  viaAt?: [number, number];
  /** What shelters him at his spot. */
  cover: CoverKind;
  /** Where he fires, while he fires. */
  aim?: [number, number];
  /** Battle time his last shot (a tank's or a gun's round, a grenade, an anti-tank round): the 3D plays one per stamp. */
  shot?: number;
  /** Battle time he fell, and how. */
  fell?: number;
  /** An anti-tank team lying hidden, a tank closed up (fighting alone among enemy foot). */
  hidden?: boolean;
  buttoned?: boolean;
  /** Battle time his current bit (a rush, a halt, a burst) ends. */
  until: number;
  /** Where his slot in the line was when he chose his spot: he keeps the spot until his slot moves away from it. */
  slotAt: [number, number];
  /** His own fixed lean in the line, metres across and along, so no two blocks stand alike. */
  jx: number;
  jy: number;
  /** A tank's way on (m a battle second): what she went, falling off at TANK_SHED when she checks. */
  way?: number;
  /** Who he is helping (a fallen comrade's k), while he is. */
  aid?: number;
  /** The fire-and-movement cycle of his last rush: one rush a cycle, however early he gets there. */
  rushed?: number;
  /** Metres a battle second he went over the ground in the last step (what the 3D game is sent as his speed). */
  went?: number;
  /** A trooper's horse holder (the holder's k), while he mounts again (Drill.board). */
  bus?: number;
  /** Men in lorries (hopOut): the battle time his turn comes to get down over his lorry's tailboard, and whether he is. */
  out?: number;
  off?: true;
  /**
   * Men who leave the field (Drill.exits): the battle time he began to leave with his block (lv); the time he gets up and
   * goes, his own moment (lvGo); his own pace (lvV), his way (lvDir: +1 for the field's far edge, past its h, -1 for its
   * near one, below 0), his own drift off it (lvA, radians) and the gait he goes in (lvP); and whether he is out of the
   * list, past his edge or at the cap (lvOut). A towed gun: hitched to her tractor (lvHk), once it has come for her.
   */
  lv?: number;
  lvGo?: number;
  lvV?: number;
  lvDir?: 1 | -1;
  lvA?: number;
  lvP?: Pose;
  lvOut?: true;
  lvHk?: true;
  /** Men in lorries: a lorry's way over her last metre of road (roadF), from where it was last taken (road). */
  road?: [number, number];
  roadF?: number;
  /** The shot stamp a round was last made for (Drill.fire), and when a rifleman's next shot (a machine gun's burst) goes. */
  fired?: number;
  burst?: number;
  /** A vehicle's scars, from the rounds that struck it (they stay to the end of the battle). */
  marks?: Mark[];
  /**
   * The drill book (tactics.ts): his element in fire and movement while his block bounds or fights, 1 the base of fire
   * (down and firing, covering) and 2 the men moving (rushing, the assault); none otherwise. Firing rapidly to cover his
   * mates until this battle time. A spotter's signaller: the battle time he last began a call on his handset.
   */
  el?: 1 | 2;
  rapid?: number;
  call?: number;
  /** A spotter's signaller: where his last call brought the fire, the middle of the block it was called on as he began it. */
  callAt?: [number, number];
  /** A gun's or a mortar's round of the salvo called: the battle time it fires (every gun within a second or two). */
  due?: number;
  /** A tank's machine guns on the enemy's foot (the drill book: against infantry mostly machine guns). */
  coax?: true;
  /**
   * Tanks with minds of their own (release 2, Drill.tankmind): what this tank's own commander has in hand. None while
   * the switch is off.
   * ph, fast, shaken: his block's phase as he has taken it up (rolling on fast, or shaken, with it), and `at`, the battle
   * time he takes up its new one (till then she goes on with what she was at).
   * h: the way she is going, and `turn`, the battle time she takes up her block's way once it has turned about under her.
   * rev: she is backing; keep: the battle time before which she may not change direction again (her own least time);
   * wait: the order she holds, standing, till then, [its speed, its spot].
   * stop, go: her halt to fire on the advance, the battle times she stops (cleared once she has) and drives on; from:
   * when it began.
   * fire: the battle time her next round goes (post: it counts toward her firing post's shots).
   * still: the battle time since which she has stood; ease: the battle time she stops to wait for her place to come
   * up to her, once told to (till then she rolls on slowly).
   * Looking and posts (mindLook, mindGaze, mindPost, mindSit): hull, the bearing her hull stands on while she stands
   * (toward the main threat, a little her own way; none for a gun fixed in her hull, which turns to her aim as
   * before); look, what her gun watches while it has no target, [the battle time the look ends, the far point's x and
   * y, its kind: 0 none yet, 1 her sector round the platoon, 2 her side on the march, 3 dead ahead]. sit: hull down,
   * she is backing down behind her firing place or sitting there ('down'), or coming up to it to fire ('up'); place:
   * that firing place; rise: the battle time of her next move down or up, her own moment. posted: her firing post's
   * number and since when she has been at it; shift: the battle time she leaves it for her next after a near miss or
   * a hit.
   * The other vehicles and the guns (mindOut, mindOff, mindVolley, mindTick; they have no commander's mind beyond
   * these): off, the battle time she moves off once told to, her own moment; to, the order she then takes, [its spot,
   * its speed]; slot: where her place was when she was last left be (so the plain drill gives her order again while
   * she waits); still is the battle time since which she has been left standing; fire is also a gun's own moment in
   * her battery's salvo, and lag the seconds it came after the plain drill's (her next round is due by the plain
   * drill's clock all the same).
   * rb: coming on, where she began backing toward the enemy (mindStep turns her about once she has backed far enough).
   */
  mind?: {
    ph?: Phase; fast?: boolean; shaken?: boolean; at?: number;
    h?: number; turn?: number;
    rev?: true; keep?: number; wait?: [number, number, number];
    stop?: number; go?: number; from?: number;
    fire?: number; post?: true;
    still?: number; ease?: number;
    hull?: number; look?: [number, number, number, number];
    sit?: 'down' | 'up'; place?: [number, number]; rise?: number; posted?: [number, number]; shift?: number;
    off?: number; to?: [number, number, number]; slot?: [number, number]; lag?: number;
    rb?: [number, number];
  };
  /** A tank's commander struck up in his hatch (BOOK.tankMg): the battle time; she fights buttoned up from then. */
  cmdDown?: number;
  /** Hatches (Drill.hatches): the battle time her commander drops inside (his own moment after what alarmed him), and the time he comes up again if nothing more does. */
  shutAt?: number;
  shutTo?: number;
  /**
   * Stretcher bearers (release 2; LandBattle.bearers): the battle time he fell wounded, not killed (wd), lying for the
   * bearers or, a light case (light), for his dressing; the time a light case is back in the line (back); the carry that
   * took him to his block's aid post (cy: [from, to, by1, by2], by1 the front bearer, each bearer as k x 1000 + j) and
   * the time he reached it (gone). A bearer (br), and the carry he is on now (job): off his block's drill while on it.
   */
  wd?: number;
  light?: true;
  back?: number;
  cy?: [number, number, number, number][];
  gone?: number;
  br?: true;
  job?: Carry;
  /**
   * Infantry with tanks (release 2, LandBattle.escort): the friendly tank he advances behind (her block's id x 1000 + her
   * k), and the battle time he last stepped out of a vehicle's way (Drill.clearOfVehicles).
   */
  esc?: number;
  dodged?: number;
  /**
   * Squad life (release 2, Drill.life): the battle time he takes up his block's new phase (till then he goes on with what
   * he was at: no squad changes what it does in one moment).
   */
  lagTo?: number;
  /** Squad life: battle time at which a man lying or kneeling who must get up and go gets up, his own moment (Drill.rises). */
  riseAt?: number;
  /** Squad life: one of his squad's sentries now (he kneels watching, and looks the widest way about him). */
  sentry?: true;
}

/**
 * A stretcher's carry (stretcher bearers, release 2): the man carried, his front and rear bearers; fetching him (the
 * bearers walking to him), lifting (at his side, turning to the way), carrying him to the aid post, or walking home; where
 * he was lifted, the aid post and the way between; the battle times the carry began and reaches the aid post.
 */
interface Carry { man: Man; front: Man; rear: Man; stage: 'fetch' | 'lift' | 'carry' | 'home'; from: [number, number]; to: [number, number]; way: number; t0: number; t1: number }

/**
 * What fired a round: a tank's gun, an anti-tank launcher or rifle, a field gun, a machine gun (a rifle section's or a
 * support team's on its tripod), a rifle, a grenade, a support team's mortar (release 2).
 */
/** What fired a round. 'smoke' (the drill book, while book.smoke.on): a smoke round, no harm, landing as a mortar's. */
export type Weapon = 'tank' | 'gun' | 'at' | 'mg' | 'rifle' | 'grenade' | 'mortar' | 'smoke' | 'track';
/**
 * What a round did: missed (landed near what it was fired at), glanced (struck armour or a gun's shield and bounced),
 * wore (went in and did harm short of a kill: a tank's armour holed, a lorry shot through) or killed (the rules took
 * that model: a man falls, a tank is knocked out).
 */
export type Struck = 'miss' | 'glance' | 'worn' | 'kill';
/** The face of a vehicle a round struck, as its crew would say it: its front, its left or right side, its rear, its top. */
export type Face = 'front' | 'left' | 'right' | 'rear' | 'top';
/**
 * A scar a round left on a vehicle: [face, across (-1 its left edge of that face to 1 its right), height (0 the ground
 * to 1 the top; over 0.65 the turret on a tank), kind (gouge: a bounce's bright scrape; hole: a round that went in;
 * scorch: a shell's blast on armour; pocks: bullet holes), the battle time it shows].
 */
export type Mark = [Face, number, number, 'gouge' | 'hole' | 'scorch' | 'pocks', number];
/**
 * One round (Drill.rounds): fired at battle time t by model k of block by, a weapon w, at block on (model ok where it
 * struck one), landing at x, y at battle time l; r what it did; on a vehicle, the face fc it struck and where on it
 * (u across, h height, as a Mark). A miss lands near what it was fired at.
 */
export interface Round {
  t: number; l: number; by?: number; k?: number; j?: number; w: Weapon; on: number; ok?: number; oj?: number; x: number; y: number; r: Struck;
  fc?: Face; u?: number; h?: number;
  /** A tank's machine-gun round: which gun fired it, the coaxial beside the main gun, the hull gunner's, or the commander's on the roof (BOOK.tankMg). */
  g?: 'coax' | 'hull' | 'roof';
  /** It struck the tank's commander, up in his hatch: he drops inside, and she fights buttoned up from then. */
  cm?: 1;
  /**
   * Shell blasts (release 2, LandBattle.blast): a man struck by the burst of the round before it (the same t, x and y),
   * or of the 'shell' event that burst there. No shot and no burst of its own: the 3D game draws the one burst once.
   */
  m?: 1;
  /** A tank knocked out ('kill'): how she ends, out of the fight, burning, or blown up (armour.ts fateOf: by her model and the face struck). */
  fate?: 'out' | 'burn' | 'blow';
  /**
   * Smoke on call (release 2, ruling 242 S6): a round of a screen the rules laid (LandBattle.screens), battle seconds its
   * smoke hangs once it lands (the screen's until less l); such a round is fired at no block (on -1). Absent on every
   * other round, the drill book's own smoke among them.
   */
  hang?: number;
}

/** A block's men as the 3D game reads them (LandBattle.menView). Numbers rounded; absent fields are off. */
export interface MenBlock {
  id: number;
  ph: Phase;
  men: MenOut[];
  /** The battery's spotter and his signaller, out from the first time its guns fire far to the end (an 'art' block only: a battery or a mortar team). */
  fo?: MenOut[];
  /** The foot block the spotter is with, and the block his guns fire on (absent while they are quiet). */
  foWith?: number;
  foOn?: number;
  /**
   * Field telephone lines (release 2; LandBattle.phones): the linemen out on this block's wire, each with ln: 1 (absent
   * where none is out, and while the switch is off).
   */
  ln?: MenOut[];
}
export interface MenOut {
  k: number; j?: number; r: Role; x: number; y: number; f: number; p: Pose; v: number; c: CoverKind;
  a?: [number, number]; s?: number; d?: number; h?: 1; b?: 1; mk?: Mark[];
  /** The drill book (tactics.ts): his element (1 the base of fire, 2 moving), and a signaller's call stamp (the radio
   * man's call_support clip plays once from it), with cs the spot that call brings the fire on (the middle of the block
   * called on as he began it: the ranging rounds fall about half its depth beyond, then short, then the salvos on it). */
  e?: 1 | 2; cl?: number; cs?: [number, number];
  /** Platoon cam (release 2): one of the squad leading his side's assault (Drill.leads). The camera's button drops onto these men. */
  ld?: 1;
  /** Field telephone lines (release 2): a lineman, outside the block's squads (MenBlock.ln). */
  ln?: 1;
  /**
   * Stretcher bearers (release 2; LandBattle.bearers): wd, the battle time he fell wounded (p wounded while he lies); cy,
   * the carry that took him to the aid post, [from, to, by1, by2] (p carried from 'from', his bearers by1 at the front and
   * by2 at the rear, each named as k x 1000 + j); gone, the time he reached the aid post; back, the time a light case got
   * up again; br, one of the block's stretcher bearers.
   */
  wd?: number; cy?: [number, number, number, number][]; gone?: number; back?: number; br?: 1;
  /**
   * The model the 3D game draws her with where it is not her block's (Man.ml): a carrier's (absent: the lorry), a
   * self-propelled gun's (drawn as a tank: r 'tank', her poses a tank's), a tank destroyer's, an escort's.
   */
  ml?: string;
  /** Her model's swaps the drill picks (Man.sw): {swap: choice}. */
  sw?: Record<string, string>;
  /**
   * Men in lorries (release 1.5, LandBattle.trucks): aboard (p 'mounted'), his lorry's or carrier's number in the block
   * (her k) and his seat in her bed (seatNo: two abreast, the odd ones on her right, front to back), and once the block is
   * told to get down a third, the battle time his own turn comes (alight: hopOut has him on the ground behind her
   * HOP.takes after it; the 3D game plays him up off his bench, along her floor and down off the back meanwhile, timed
   * from it, as the drill's think comes only every half second; his next change sets him on the ground behind her).
   */
  sv?: number[];
  /**
   * Men who leave the field (release 2, Drill.exits): a man of a block that has left the fight (fled off the field, or
   * withdrawn at its out line), still drawn as he runs, rides or drives for his own edge, and taken off the list once he
   * is past it (or DR.exits.cap battle seconds after his block left). He fires at nothing, nothing is fired at him, and
   * he is no longer one of his block's men: anything that counts a block's men from this list leaves him out. Absent
   * while the switch is off.
   */
  lv?: 1;
}

interface Nums { rush: number; halt: number; wave: boolean; assault: number; spacing: number; tankRush: number; tankHalt: number; scoot: number; salvo: 'battery' | 'rapid' | 'ripple'; gunEvery: number }
const DR = data as unknown as {
  exits: { past: number; cap: number; rise: number; pace: [number, number] };
  style: Record<'plain' | 'bold' | 'methodical' | 'mass', Nums>;
  xp: { raw: number; veteran: number; bunch: number; spread: number; seek: [number, number, number]; pinned: [number, number, number] };
  cover: { ground: Record<Ground, number>; edge: number; crest: number; wreck: number; crater: number };
  ranges: { column: number; bound: number; debus: number; fo: number; ambush: number; grenade: number; hullDown: [number, number] };
  pin: { harm: number; shelled: number };
  rounds: {
    odds: Record<Weapon, [number, number]>; shooterMoving: number; targetMoving: number; cover: Record<CoverKind, number>;
    xp: [number, number]; glance: Record<Face, number>; turret: number; spread: Record<Weapon, [number, number, number, number]>;
    indirect: { sigma: [number, number]; mortar: [number, number]; onVehicle: number }; speed: Record<Weapon, number>;
    smallArms: { rifle: [number, number]; mg: [number, number] }; keep: number; marks: number;
  };
  teams: { hmg: { burst: [number, number] }; mortar: { every: number }; file: number; pace: number; sep: number };
  sections: {
    rifle: Record<string, { men: number; loaders: number; scouts: number }>;
    officer: number; mg: number; at: number; sapper: number; holder: number; gun: number; hmg: number; mortar: number;
    space: number; rows: number; files: number; ahead: number;
    platoon: Record<string, { squads: number; hq: number }> & { step: number; rows: number };
  };
  tanks: { post: number; back: number; shots: [number, number] };
  life: {
    rush: { spread: number; wave: number; len: [number, number]; roll: number; knee: number; sprint: number; upright: number };
    drift: { across: number; along: number; every: [number, number] };
    rout: [number, number];
    react: { rise: number; leash: number; calm: number; ground: [number, number]; raw: number; up: [number, number]; settle: [number, number]; back: [number, number]; break: [number, number]; rally: [number, number] };
    watch: { open: number; cover: number; works: number; keep: [number, number]; sentries: number; turn: [number, number]; far: number; easy: number; glasses: number };
    scan: { own: number; every: [number, number]; arc: number; rest: number; lying: number; rate: number; back: number };
    pause: { every: [number, number]; for: [number, number]; gun: [number, number]; gunFor: [number, number] };
    ready: [number, number];
  };
  hatch: { near: [number, number, number]; own: [number, number]; drop: [number, number]; quick: [number, number, number]; quiet: [number, number, number]; burst: number; mates: [number, number, number] };
  blast: { hold: number; anchor: number; most: Record<Calibre, number>; lying: number; kneeling: number };
  escort: {
    near: number; men: number; reach: number; gap: number; file: number; side: number; moving: number; pace: [number, number];
    clear: { ahead: number; behind: number; half: number; out: number; enemyAhead: number; crush: number; crushHalf: number; dodge: number; within: number };
  };
  mech: {
    rows: Record<string, [number, number, string[]][]>; commonwealth: string[]; armoured: string[]; share: number; back: number; base: number;
    kit: { per: number; models: Record<string, string>; axis: string[]; axisStand: string; stand: string };
  };
  spg: { rows: Record<string, [number, number, string[], number, Kind[]][]>; kit: { models: Record<string, string>; axisStand: string; stand: string } };
  td: { rows: Record<string, [number, number, string[], number][]>; kit: { models: Record<string, string>; axisStand: string; stand: string } };
  cars: { rows: Record<string, [number, number, string, Record<string, number>][]>; kit: { models: Record<string, string>; axisStand: string; stand: string }; ahead: number; side: number; back: number; contact: number };
  aa: { rows: Record<string, [number, number, string, Record<string, number>][]>; kit: { models: Record<string, string>; axisStand: string; stand: string }; back: number; battery: number };
  light: {
    cars: Record<string, [number, number, string[], Record<string, number>, Record<string, 'ahead'>][]>;
    bikes: Record<string, [number, number, string[], Record<string, number>, Record<string, 'ahead'>][]>;
    kit: Record<'cars' | 'bikes', { models: Record<string, string>; axisStand: string; stand: string }>;
    sw: Record<string, Record<string, Record<string, string>>>;
    most: number; ahead: number; side: number; back: number; run: number; every: number;
  };
  tankmind: {
    react: [number, number]; quick: [number, number, number]; roll: [number, number]; straight: number; way: number; back: number;
    halt: { reach: number; rare: number; soon: number; ahead: number; len: [number, number]; lay: [number, number]; see: number; fall: number; edge: number; apart: number };
    top: number; pair: { own: number; len: [number, number]; pace: number; gather: number; rare: number }; shot: { first: [number, number]; fresh: number; pause: [number, number] }; keep: [number, number];
    hull: number; fixed: string[];
    watch: { reach: [number, number]; cone: number; first: number; hold: [number, number]; far: number; side: [number, number]; rear: number };
    post: { cover: string[]; open: number; down: number; pace: number; idle: [number, number]; see: [number, number]; rise: number; near: number; jump: number; rest: [number, number] };
    out: { spot: [number, number]; start: [number, number]; pace: [number, number]; trail: number; far: number; sight: number; arc: [number, number] };
    lean: [number, number, number]; volley: { own: number; apart: number };
    wait: { turn: number; own: number; pace: number; far: number }; weave: { side: [number, number]; len: [number, number] };
  };
};
export const DRILL = DR;

/** A fixed hash to [0, 1): the drill's own dice, never the battle's. */
const mix = (n: number): number => { let h = n >>> 0; h ^= h >>> 16; h = Math.imul(h, 0x85ebca6b); h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35); h ^= h >>> 16; return (h >>> 0) / 4294967296; };
const roll = (...ns: number[]): number => { let h = 0x9e3779b9; for (const n of ns) h = Math.imul(h ^ (Math.floor(n) >>> 0), 0x01000193) + 0x7f4a7c15; return mix(h); };
const clamp = (v: number, a: number, b: number): number => (v < a ? a : v > b ? b : v);
const live = (u: LandUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
const fighting = (u: LandUnit): boolean => live(u) && u.state !== 'routing';
const FOOT_KINDS: Kind[] = ['inf', 'mtn', 'eng', 'fort', 'cav'];
/** A vehicle's look as the header names it for the 3D game to read ahead (Drill.modelsFor): her model, and the swaps the rules pick for her ("Jeep|mg=off"). */
export const lookOf = (ml: string, sw?: Record<string, string>): string => ml + Object.keys(sw ?? {}).sort().map((k) => `|${k}=${sw![k]}`).join('');
const isFoot = (u: LandUnit): boolean => MOTION[u.kind] === 'foot' || u.kind === 'mot';
const CLOSE: Ground[] = ['woods', 'buildings', 'industry', 'suburb'];
const BUILT: Ground[] = ['buildings', 'industry', 'suburb'];
/** Poses that carry a man over the ground: the 3D plays them only while he moves (walk: one he reaches his place in ends). */
const MOVING: ReadonlySet<Pose> = new Set<Pose>(['walk', 'run', 'advance', 'creep', 'crawl', 'flee', 'drive', 'reverse', 'towed', 'stretcher_front', 'stretcher_rear']);
/** Poses that go with whatever carries him (a horse, a lorry, the horses he leads): they move at any speed. */
const CARRIED: ReadonlySet<Pose> = new Set<Pose>(['ride', 'mounted', 'horses']);
/** A man lying down (shell blasts find less of him; an enemy tank's crew may not see him in her path). */
const LYING: ReadonlySet<Pose> = new Set<Pose>(['fire_prone', 'hide', 'crawl', 'observe', 'cower', 'lmg_burst']);
/** Squad life: the poses a man watches in (he looks round his own arc: Drill.scan). */
const WATCHING: ReadonlySet<Pose> = new Set<Pose>(['wait', 'hide', 'idle', 'observe']);
/** Poses a rifleman or a machine gun fires from (each shot a round: Drill.fire). */
const SMALL_ARMS: ReadonlySet<Pose> = new Set<Pose>(['fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst']);
/** What a round can strike and scar (a man struck is a man the rules take). */
const VEHICLE: ReadonlySet<Role> = new Set<Role>(['tank', 'gun', 'lorry']);
/** A support team's weapons, each with its crew (release 2): carried by hand, set up in a line, fired from the rear. */
const CREWED: ReadonlySet<Role> = new Set<Role>(['hmg', 'mortar']);
/** A normal draw from two of the drill's dice (Box and Muller). */
const gauss = (a: number, b: number): number => Math.sqrt(-2 * Math.log(Math.max(1e-9, a))) * Math.cos(2 * Math.PI * b);
/**
 * The fastest a man goes on foot, closing up on his block: a hard run. The blocks themselves march at up to 6 m a battle
 * second (the battle runs faster than life), so a man must outpace that to catch up; the 3D game caps its run clip's rate.
 */
const CATCH_UP = 7.5;
/**
 * The fastest a tank goes, closing up on her block (m a battle second): the rules' armour goes at 10 across open ground
 * (LAND.speed, the battle faster than life, as the men's 6), and a tank catches up no faster than a fifth more. A
 * Sherman's top speed on a road was about 11 m a second (the 3D ground battle scene, 21 November 20:35Z). Where the
 * rules drive her block faster (13 on a road), she keeps its pace, no more. Lorries and towed guns keep 16.
 */
const TANK_TOP = 12;
/**
 * How fast a tank gathers way (m a battle second, each battle second): from a halt to her block's 10 in about 3 s. The
 * battle runs about twice as fast as life (the men's 6 and the tanks' 10 m a second are a run and a road's top speed), so
 * played at half the battle's clock she goes 5 m a second across a field, backs at 2 and gathers 0.75 m a second each
 * second (the 3D ground battle scene, 21 November 23:58Z, from the summer of 1944).
 */
const TANK_GAIN = 3;
/** How fast a tank's way falls off when she checks (m a battle second, each battle second): a step's pause loses little. */
const TANK_SHED = 6;
/** A tank's top speed now: TANK_TOP, or her block's pace where the rules drive it faster (as they drive it, not as it is jostled). */
const tankTop = (sq: { speed: number; u: LandUnit }): number => Math.max(TANK_TOP, Math.min(sq.speed, sq.u.speed ?? 0));
/**
 * Lorried foot and cavalry get down once their block has gone slower than 4.5 m a battle second for a second (near the
 * enemy). Once down, a short dash of their block's (most last under six seconds) they follow on foot, running hard; but
 * when it goes on faster than 5.5 for six seconds, a move no man on foot can keep up with (the rules drive it at lorry
 * or horse pace), they mount again; and if it slows below 4.5 for three seconds while they mount, they get down where
 * they are. They run to their seat in their lorry (up once there: no leap aboard), or to their horse beside its holder
 * (up within a stride of it), the lorry halted (the horse held still) and their block still going. Fast and slow are the ground the block makes over
 * the last `span` seconds (a block the rules jostle back and forth in a fight makes none, however far its box moves);
 * slow is also a block standing.
 */
const MOUNT = { down: 4.5, downAfter: 1, up: 5.5, upAfter: 6, stayAfter: 3, climb: 0.05, saddle: 1, span: 3 };
/**
 * Men in lorries (LandBattle.trucks; hopOut): a man's turn to get down comes every `hop` battle seconds in each lorry, the
 * back of her bed first (`same` after one from his own bench); he takes `takes` getting up, along her floor and down off the back (the 3D game's bench_rise,
 * a walk and out_tail, some 5.6 s of life at the battle's usual two times life); `most` seconds after the block began,
 * all left aboard get down at once. He lands `tail` behind her middle, `abreast` to his own bench's side of it (the two
 * benches take turns, so two men coming down one after the other are never on one spot), and runs clear of her; she
 * stands `clear` seconds more after the last. A lorried man's ring (ring) is `ring` wider, as his lorry stands meanwhile.
 */
const HOP = { hop: 0.65, same: 1.05, takes: 2.8, most: 15, tail: 4.1, abreast: 0.35, clear: 2.5, ring: 100 };
/** Men of a block with lorries or horses may be this much further out than foot before the ring carries them (ring). */
const RIDERS_RING = 120;
const COVER_OF: Partial<Record<Ground, CoverKind>> = { works: 'works', bunker: 'works', buildings: 'building', industry: 'building', suburb: 'building', woods: 'woods', rock: 'rock', rough: 'rough', field: 'field' };

/** The block's own frame: forward along its face, across to its right. */
const fwd = (f: number): [number, number] => [Math.sin(f), Math.cos(f)];
const right = (f: number): [number, number] => [Math.cos(f), -Math.sin(f)];
const toWorld = (u: { x: number; y: number }, f: number, a: number, l: number): [number, number] => {
  const [fx, fy] = fwd(f), [rx, ry] = right(f);
  return [u.x + a * rx + l * fx, u.y + a * ry + l * fy];
};
const bearing = (x0: number, y0: number, x1: number, y1: number): number => Math.atan2(x1 - x0, y1 - y0);
/** An angle brought into (-pi, pi]. */
const wrap = (a: number): number => { const r = (a + Math.PI) % (2 * Math.PI); return (r <= 0 ? r + 2 * Math.PI : r) - Math.PI; };
/** A facing turned toward the one wanted by at most 'max' radians, the short way round. */
const turnTo = (f: number, want: number, max: number): number => { const d = wrap(want - f); return wrap(Math.abs(d) <= max ? want : f + Math.sign(d) * max); };
/**
 * How fast each turns about, radians a battle second: a man on his feet (half a second for an about-turn), a horse, a
 * tank on her tracks (four seconds), a lorry or a gun on its wheels. No one turns about in a tick.
 */
const TURN = { foot: 8, ride: 4, tank: 0.8, wheels: 1.5 };

/** The ground under a point. */
const groundOf = (f: LandField, x: number, y: number): Ground => {
  const c = clamp(Math.floor(x / f.cell), 0, f.cols - 1), r = clamp(Math.floor(y / f.cell), 0, f.rows - 1);
  return GROUNDS[f.ground[r * f.cols + c]];
};
/** The height at a point, smoothed between the cells' centres (the cells are 25 m: a crest needs finer). */
const heightOf = (f: LandField, x: number, y: number): number => {
  const fx = clamp(x / f.cell - 0.5, 0, f.cols - 1), fy = clamp(y / f.cell - 0.5, 0, f.rows - 1);
  const c0 = Math.floor(fx), r0 = Math.floor(fy), c1 = Math.min(f.cols - 1, c0 + 1), r1 = Math.min(f.rows - 1, r0 + 1), ax = fx - c0, ay = fy - r0;
  const H = f.height, C = f.cols;
  return (H[r0 * C + c0] * (1 - ax) + H[r0 * C + c1] * ax) * (1 - ay) + (H[r1 * C + c0] * (1 - ax) + H[r1 * C + c1] * ax) * ay;
};
/** Storeys of the house at a point (its upper floors give a spotter his view; about 3 m each). */
const storeysOf = (f: LandField, x: number, y: number): number => {
  if (!f.storeys) return 0;
  const c = clamp(Math.floor(x / f.cell), 0, f.cols - 1), r = clamp(Math.floor(y / f.cell), 0, f.rows - 1);
  return f.storeys[r * f.cols + c] ?? 0;
};

interface Squad {
  u: LandUnit;
  /**
   * How its men are drawn (GR3, the true-scale plan): 2 as platoons, 1 as sections, 0 its key men only. The battle's own
   * setting (Drill.sections, Drill.platoons), or less where a scale is set and the block's men at that scale are fewer
   * than the setting would draw (Drill.tierFor).
   */
  tier: 0 | 1 | 2;
  men: Man[];
  /** Lorries (lorried infantry, a battery's tractor; armoured infantry's carriers). */
  veh: Man[];
  /** Armoured infantry (release 2, LandBattle.mech): the armoured block whose riflemen these are (Drill.mechOf); a block's armoured cars' or AA vehicles' (esc). */
  of?: number;
  /** A block's armoured cars, its AA vehicles or its jeeps and motorcycles (release 2, LandBattle.cars, aa, light; Drill.addEscorts): vehicles only, drawn with it. */
  esc?: 'cars' | 'aa' | 'light';
  /**
   * Tanks with minds of their own (Drill.tankmind; mindBlock): the pace the block is really making, for its tanks'
   * drill (its pace, less where the rules jostle it to and fro); the battle time its fight began (the think it was
   * first fighting after some seconds out of one: each tank then opens fire at her own moment), and when it was last
   * fighting.
   */
  made?: number;
  fightAt?: number;
  foughtAt?: number;
  /**
   * Tank minds (mindBlock): the shells fired at it that have not landed, or landed since its last think, [x, y, the
   * model struck or -1, the battle time it lands] each (a near miss or a hit moves a tank to her next post: mindPost);
   * and the number of the last round looked at for them.
   */
  hits?: number[];
  hitN?: number;
  fo: Man[] | null;
  foWith?: number;
  foOn?: number;
  /** Where the spotter's host block stood when he chose his place: he moves up when it has gone on 60 m. */
  foAt?: [number, number];
  phase: Phase;
  since: number;
  /** Squad life: how long the phase before this one lasted (a block whose phase flickers is not given fresh lags, Drill.lags). */
  lastFor?: number;
  nextThink: number;
  /** Strength last step, and the share of strength it is losing a second (smoothed). */
  lastStr: number;
  harm: number;
  shelledAt: number;
  /**
   * Shell blasts (release 2, LandBattle.blast): the battle time its first loss to a shell became due while held for the
   * next burst (thin), and where and when the last 'shell' event that took one of its key men burst (Drill.shelled).
   */
  held?: number;
  anchor?: { x: number; y: number; t: number };
  /** Where the block stood last step, and its heading over the ground. */
  lastX: number;
  lastY: number;
  heading: number;
  speed: number;
  /** Where it stood each step of the last MOUNT.span seconds (x, y pairs, oldest first), and the ground it made over
   * them: its way (wx, wy, a m/s vector) and pace (net). */
  track: number[];
  wx: number;
  wy: number;
  net: number;
  /** Lorried infantry got down; cavalry got off their horses. */
  down: boolean;
  /** Seconds on end the block has gone faster than men on foot can follow (MOUNT.up), and slower than MOUNT.down. */
  fast: number;
  slow: number;
  /** Battle time the block, down, began to mount again (its lorries coming up for the men, the holders with the horses). */
  boarding?: number;
  /** Men in lorries (LandBattle.trucks): each lorried man's seat in his lorry's bed, numbered once a lorry (seatNo). */
  seats?: Map<Man, number>;
  /**
   * Men in lorries: battle time the block began to get down from its lorries, a man at a time over each tailboard
   * (hopOut), and when its last man was down (its lorries stand a moment more while the men clear their tails).
   */
  hop?: number;
  left?: number;
  /**
   * Down and on the move: its lorries (its horse holders) keep close behind it, ready to take the men up again, from its
   * first second at a pace faster than men on foot keep until it has stood three seconds; else they wait back in cover.
   */
  close: boolean;
  /** It was running last think (a rally follows). */
  ran: boolean;
  /** Battle time the battery's next salvo, and the gun that fires next in a ripple. */
  salvoAt: number;
  ripple: number;
  /** Who the block's assault is working onto, and from which flank (+1 its right, -1 its left). */
  flankOf?: number;
  flank: number;
  /** Battle time its officer next moves along the line. */
  officerAt: number;
  /** Advancing by leapfrog (slow going) rather than rolling (fast): switched with a margin, so it does not flicker. */
  leap: boolean;
  /** Its men on their feet this step, worked out once (Drill.memo). */
  memo?: Memo;
  /** Its men when it was drawn up (key men and the rest of their sections): the whole its strength is a share of. */
  m0: number;
  /** Men of its sections fallen so far (the drill's dice for who goes next). */
  lost: number;
  /** Its box's axes for the face it had when they were worked out: [face, right x, y, forward x, y] (leash). */
  ax?: [number, number, number, number, number];
  /** The drill book: its groups' middles at the last think (suppression), and when the enemy first came within its
   * line's counter-attack reach (a defence). */
  mids?: Map<number, [number, number]>;
  closeAt?: number;
  /** A battery's or a mortar team's shoot at its target, its spotter calling it (tactics.ts Mission). */
  ms?: Mission;
  /** The drill book's smoke: when its platoons last laid smoke on the enemy's flank, and the rounds still to come. */
  smokeAt?: number;
  smokeLeft?: number;
  /** Smoke on call (release 2): the screen the rules laid for it, being drawn as rounds, the next i of n. */
  screenQ?: { s: Screen; i: number; n: number };
  /**
   * Stretcher bearers (release 2): its bearers, picked from its riflemen the first step the switch is on; its litter cases
   * lying for them, earliest first; its light cases lying at their dressing; and the carries under way.
   */
  brs?: Man[];
  lying?: Man[];
  dressing?: Man[];
  carries?: Carry[];
}

/** A block's men on their feet this step (Drill.memo). */
interface Memo {
  /** What it was worked out for: the block's models and whether it is on the field (a man falling clears it). */
  fig: number;
  on: boolean;
  /** All its men on their feet, and those with its lorries (while it is on the field): what a round can strike. */
  stand: Man[];
  struck: Man[];
  /** Those the block's drill moves (not those who keep by a weapon or an officer: Drill.pairs), in the block's order. */
  up: Man[];
  /** Its key men, and each by his model. */
  keys: Man[];
  keyOf: Map<number, Man>;
  /** 'up' section by section, key man first (the march's files). */
  all: Man[];
  /** The rest of each section, by its key man's k (in their order). */
  bySec: Map<number, Man[]>;
  /** Squad life (Drill.teamOf): each man's buddy team in his group, [its number, the group's teams, his place among the group's men]. */
  teams?: Map<Man, [number, number, number]>;
  /** Squad life (Drill.sentryList): each group's men who can stand sentry, in their order. */
  sentries?: Map<number, Man[]>;
}

/** Whom each kind of man who keeps by another keeps by (Drill.leadOf). */
const LEADS: Partial<Record<Role, Role[]>> = { loader: ['mg', 'at', 'hmg', 'mortar'], crew: ['gun'], rifle: ['officer'], holder: ['holder'] };
/** A field gun's crew at their places round it, metres across and along from the gun, facing as it faces (Drill.pairs). */
/**
 * Room between men (drawn as sections): no man's place within SPACE.men metres of a mate's, a gun's crew SPACE.crew of
 * each other at their gun (the 3D ground battle scene, 21 November 21:17Z: two riflemen drawn one inside the other).
 */
const SPACE = { men: 1.6, crew: 1.1, cell: 2 };
/** The places booked so far, by 2 m cell: each man where he is going, or where he is if still. */
type Room = Map<number, { m: Man; x: number; y: number }[]>;
const GUN_CREW: [number, number][] = [[-1.4, -1.6], [1.4, -1.9], [-0.9, -3.4], [1.0, -3.8], [-2.4, -4.6], [2.2, -5.3], [0, -7.5], [-3, -8]];

export class Drill {
  private readonly b: LandBattle;
  private readonly squads = new Map<number, Squad>();
  /** The rounds of the last few seconds (DR.rounds.keep), oldest first, each numbered (n) in the order fired. */
  private readonly shots: (Round & { n: number })[] = [];
  private fired = 0;
  /**
   * Models the rules took this step ('block:model') whose round is already made: a shell's (Drill.shelled), or none of
   * the field's (the air's: Drill.fromAir). And the block whose small arms took each other one (Drill.struckBy).
   */
  private readonly shellKills = new Set<string>();
  /** Infantry with tanks (release 2): each tank's followers (her block's id x 1000 + her k), and where each tank is. */
  private readonly escorts = new Map<number, Man[]>();
  private readonly byOf = new Map<string, number>();
  /** Where each wreck lies as drawn (the rules' wreck id to where her tank fell): she burns where she stood. */
  private readonly wreckAt = new Map<number, [number, number]>();
  private readonly seed: number;
  private settled = false;
  /**
   * Platoon cam (release 2; Brian, 22 November: "Do 2 and 3"): the squad leading each side's assault, as [block, group
   * (grp)], chosen every half second (Drill.pickLeads), and the battle time the next choice is made.
   */
  private readonly leads = new Map<Side, [number, number]>();
  private leadAt = 0;
  /** The battle time each squad ('block:group') was last seen on the move, for the platoon cam. */
  private readonly rushed = new Map<string, number>();
  /**
   * Each model drawn as its section (release 2; the 3D ground battle scene for Brian, 21 November 2026, 18:29Z: "the
   * ground battle needs a lot more soldiers in it"): the model's man is its key man, and the rest of his section stand
   * round him (DR.sections). Off, a model is one man, as before.
   */
  readonly sections: boolean;
  /** Drawn as platoons (release 2; Brian, 22 November: "I want a lot of troops in the ground battles"): each rifle model is
   * its platoon, its key man's squad leading, the platoon's other squads and its headquarters behind (sections too). */
  readonly platoons: boolean;
  /**
   * The drill book (tactics.ts; Brian, 22 November 00:23Z: "move like a platoon with covering fire and historical
   * tactics"): each army's platoons fight by their school, under suppression, with lift and shift and tanks and infantry
   * together. On with platoons unless asked off.
   */
  readonly byBook: boolean;
  /**
   * The battle's scale (the true-scale plan, GR1 and GR3; Brian's standing rule of 22 November 04:14Z: the numbers on
   * screen reflect the units each side brings): one drawn man stands for this many. No block draws more men than its
   * men0 over it: drawn as platoons where they fit, else as sections, else its key men only. Null: the battle's setting
   * for every block, as before.
   */
  readonly scale: number | null;
  /**
   * Squad life (release 2; Brian, 25 November 2026, 13:40Z: "i dont want all the soldiers of a squad doing the exact same
   * thing at the same time. I want them to work together and look real as individuals apart of a squad"): a squad's men
   * pair off into buddy teams that rush one after another, take up a new order each in his own time, hold in different
   * stances, each watching his own arc, and break off firing for a fresh clip. Looks only; the drill book is on with it.
   * Off unless asked (DR.life).
   */
  readonly life: boolean;
  /**
   * Heads up, hatches down (release 2; Brian, 25 November 2026, 15:11Z: "do those 3 ideas"): each tank's commander rides
   * with his head out until something sends him down (a round at his own tank, a shell bursting by her, the enemy's foot
   * within his own nerve of it), drops inside at his own moment and comes up again in his own time once it is quiet: a
   * green crew early and for long, a veteran late and briefly. Looks only (the drill's own dice; what a closed-up tank
   * sees is the rules' business and is not touched here). Off unless asked (DR.hatch); off, a block's tanks close up as
   * one with the enemy inside 120 m, as before.
   */
  readonly hatches: boolean;
  /**
   * Experience you can see (release 2; Brian, 25 November 2026, 15:11Z: "do those 3 ideas"): a squad's training shows
   * in how it moves: green squads bunch up and freeze for a moment under first fire, veterans keep wider gaps and get
   * down fast. By the block's training alone (u.xp, grade()), never its nation or the date. Looks only (the drill's
   * own dice), and only with squad life on. Off unless asked (DR.seasoned).
   */
  readonly seasoned: boolean;
  /**
   * When the leader falls (release 2; Brian, 25 November 2026, 15:11Z: "do those 3 ideas"): a squad whose leader is
   * hit goes to ground and hesitates a few seconds, then the next man takes charge. He is marked as acting leader in
   * the drill only: the man the 3D game draws is never swapped. Looks only (the drill's own dice), and only with squad
   * life on. Off unless asked (DR.leaders).
   */
  readonly leaders: boolean;
  /**
   * Tanks with minds of their own (release 2; Brian, 25 November 2026, 15:57Z: "i dont want the tanks and vehicles
   * doing the exact same thing as each other. i want everything to see like it has its own mind"; 15:56Z: "should the
   * tanks be moving or was that the real tactic?"): each tank takes up an order at her own moment, halts briefly at
   * her own place to fire and moves on while her partner covers, lays her hull to the threat and her turret to her
   * own arc; the other vehicles and the guns likewise. Looks only (the drill's own dice; the block's place and pace
   * stay the rules'). It needs neither squad life nor the drill book. Off unless asked (DR.tankmind).
   */
  readonly tankmind: boolean;
  /**
   * A tank's machine guns, seen firing (release 2; Brian, 25 November 2026, 15:55Z: "i never see the machine guns on
   * the tanks firing"; 15:56Z: "i dont want it to be all the time as thats unrealistic with ammo"): each tank her own
   * machine-gun mind: short bursts at targets worth the rounds, spells and pauses at her own pace and moment, the guns
   * her model carries, and a stock of rounds that each burst draws down, so she eases off as it runs low. Looks only
   * (the drill's own dice). It needs neither squad life nor the drill book; where the book is on as well, this takes
   * the place of the book's tank bursts. Off unless asked (DR.tankmg).
   */
  readonly tankmg: boolean;
  /**
   * Men who leave the field are seen leaving (release 2; Brian, 25 November 2026, 17:11Z: "some soldiers freeze and
   * then disapear"): the men of a routing block run on past the field's edge and do not halt and cower on it, and
   * the men, vehicles and observers of a block that has left the fight (gone off the edge, or withdrawn at its out
   * line) stay in view(), each marked `lv`, running or driving to their own edge, and are dropped one by one once
   * past it. Looks only: the rules never read it, and a battle ends the same with it on or off. Off unless asked
   * (DR.exits).
   */
  readonly exits: boolean;
  /**
   * Wounded and knocked-down men (release 2; Brian, 25 November 2026, 16:14Z: "im not seeing multiple soliders being
   * taken out by an explosion if they are in the blast radius"): a shell burst gathers the losses its block is owed
   * and is placed so that the men nearest it are the ones taken; of a block's fallen a share lie wounded and not
   * dead (view()'s wd with no cy, with or without the bearers rule); living men inside the stun ring are knocked
   * down and held a few seconds (kd). Looks only (the drill's own dice): the rules' losses stay as booked. It needs
   * the burst rule (blast) for the bursts; the wounded share works without it. Off unless asked (DR.wounded).
   */
  readonly wounded: boolean;
  /**
   * Every man his own pace (release 2; Brian, 25 November 2026, 17:30Z: "it looks like they are all jogging at the
   * same speed"; 16:24Z: "the soldiers of a squad keep zig zaging at the same time"): each man on foot moves at a
   * pace a little his own and steps off at his own moment, sprints across the open under fire and dashes the short
   * bound between cover, comes on slower under a gun or a load, tires after a long run; and a squad steers by its
   * march and its track over some seconds, not by one step of its block, so its men no longer swerve together.
   * Looks only (the drill's own dice): the rules never read it. Off unless asked (DR.paces).
   */
  readonly paces: boolean;
  /** Squad life: the men taking up their block's phase this think (their lag just over), as on a fresh phase. */
  private readonly freshNow = new Set<Man>();
  private readonly sup = new Suppression();
  /** Each side's tanks standing this step, and each block's most dangerous men (its guns), worked out once a step. */
  private tankMemo: { t: number; at: Partial<Record<Side, [number, number][]>> } = { t: -1, at: {} };
  private readonly priMemo = new Map<number, { t: number; men: Man[] }>();

  constructor(b: LandBattle, opts: { sections?: boolean; platoons?: boolean; book?: boolean; life?: boolean; hatches?: boolean; seasoned?: boolean; leaders?: boolean; tankmind?: boolean; tankmg?: boolean; exits?: boolean; wounded?: boolean; paces?: boolean; scale?: number } = {}) {
    this.b = b;
    this.seed = b.ticket.seed;
    this.sections = !!opts.sections || !!opts.platoons;
    this.platoons = !!opts.platoons;
    this.life = !!opts.life;
    this.hatches = !!opts.hatches;
    this.seasoned = this.life && !!opts.seasoned;
    this.leaders = this.life && !!opts.leaders;
    this.tankmind = !!opts.tankmind;
    this.tankmg = !!opts.tankmg;
    this.exits = !!opts.exits;
    this.wounded = !!opts.wounded;
    this.paces = !!opts.paces;
    this.byBook = opts.book ?? (this.platoons || this.life);
    this.scale = opts.scale !== undefined && Number.isFinite(opts.scale) && opts.scale > 0 ? opts.scale : null;
    for (const u of b.units) { this.squads.set(u.id, this.make(u)); this.addMech(u); this.addEscorts(u); }
  }

  /** A block placed in the setup step (release 2) before the clock starts: its men drawn up afresh where it now stands. */
  relay(u: LandUnit): void {
    if (this.b.t > 0 || !this.squads.has(u.id)) return;
    this.squads.set(u.id, this.make(u));
    this.addMech(u);
    this.addEscorts(u);
  }

  /** A block come onto the field after the drill was made (a corps from next door, release 2): its men drawn where it walks on. */
  join(u: LandUnit): void {
    if (!this.squads.has(u.id)) { this.squads.set(u.id, this.make(u)); this.addMech(u); this.addEscorts(u); }
  }

  /**
   * Armoured infantry (release 2; Brian, 23 November 07:01Z, yes to "half-tracks carrying armoured infantry"; the
   * historian's rows and the design director's rulings, 07:05Z and 07:06Z; LandBattle.mech, off unless turned on; looks
   * only). An armoured block's riflemen, drawn as lorried foot of their own: a section to each carrier of its army's row
   * (mechOf), riding DR.mech.back metres behind its tray and getting down near the enemy as lorried foot do. They are a
   * view of the block (kind 'mot'): its place (behind its tray, on its facing), its order, state, strength and harm are
   * the block's, read live, so the rules' counts and places are untouched. Their key men are numbered k from DR.mech.base
   * and fall in step with its tanks (its figures over its figures0); the rest of their sections fall as its strength goes
   * (thin). They are kept under the block's id less one, negative, so no lookup by a block's id finds them, and the 3D
   * game draws them with the block's own men (view).
   */
  private addMech(u: LandUnit): void {
    const key = -(u.id + 1);
    this.squads.delete(key);
    if (!this.b.mech || u.kind !== 'arm') return;
    const carriers = this.mechOf(u);
    if (!carriers.length) return;
    const M = DR.mech, c = carriers.length, base = M.base, back = (): number => u.d / 2 + M.back;
    const v = Object.create(u) as LandUnit;
    Object.defineProperties(v, {
      kind: { value: 'mot', enumerable: true },
      x: { get: () => u.x - Math.sin(u.face) * back(), enumerable: true },
      y: { get: () => u.y - Math.cos(u.face) * back(), enumerable: true },
      figures0: { value: base + c, enumerable: true },
      figures: { get: () => (u.figures <= 0 ? 0 : base + Math.min(c, Math.ceil((c * u.figures) / Math.max(1, u.figures0)))), enumerable: true },
      tray: { value: c, enumerable: true },
      men0: { value: Math.round(u.men0 * M.share), enumerable: true },
    });
    this.squads.set(key, this.make(v, { base, n: c, carriers, of: u.id }));
  }

  /**
   * An armoured block's carriers, its tray over four of them (one at least). What its corps carries where the campaign
   * says ("Units decide", ForceKit.halftracks; the design director, 23 November 07:19Z): DR.mech.kit.per to four tanks,
   * its share of them half-tracks (first), each its army's own model or the stand-in in its coat, the rest lorries. Else
   * the history start's mix: its army's row in the battle's month (DR.mech.rows).
   */
  private mechOf(u: LandUnit): string[] {
    const M = DR.mech, nat = M.commonwealth.includes(u.nat) ? 'UK' : u.nat;
    const share = u.kit?.halftracks;
    if (share !== undefined) {
      const n = Math.max(1, Math.round((M.kit.per * u.tray) / 4)), h = Math.round(n * clamp(share, 0, 1));
      const own = M.kit.models[nat] ?? (M.kit.axis.includes(nat) ? M.kit.axisStand : M.kit.stand);
      return Array.from({ length: n }, (_, i) => (i < h ? own : 'Truck'));
    }
    const d = turnToDate(this.b.ticket.turn), when = d.year + d.month / 12;
    const row = (M.rows[nat] ?? []).find(([from, to]) => when >= from && when < to);
    if (!row) return [];
    const list = row[2], n = Math.max(1, Math.round((list.length * u.tray) / 4));
    return Array.from({ length: n }, (_, i) => list[i % list.length]);
  }

  /**
   * The models a block's vehicles are drawn with besides its own (release 2, looks only): its armoured infantry's
   * carriers, its self-propelled guns, its tank destroyers, its armoured cars, AA vehicles, jeeps and motorcycles (with
   * the swaps the rules pick for them: lookOf), as the switches stand. For the 3D game to read them before the
   * battle opens (the header's "mls"); worked out from the block alone, so a block still coming up from next door has its
   * own too.
   */
  modelsFor(u: LandUnit): string[] {
    const n = Math.max(0, u.figures0), out = new Set<string>();
    if (this.b.mech && u.kind === 'arm') for (const m of this.mechOf(u)) out.add(m);
    for (const m of this.spgOf(u, n)) if (m) out.add(m);
    for (const m of this.tdOf(u, n)) if (m) out.add(m);
    for (const m of [...this.escortOf(u, 'cars'), ...this.escortOf(u, 'aa'), ...this.lightOf(u).map((x) => lookOf(x.ml, x.sw))]) out.add(m);
    return [...out];
  }

  /** The army whose rows a block's nation reads (the Commonwealth's as Britain's), and the battle's year with its month. */
  private armyOf(nat: string): string { return DR.mech.commonwealth.includes(nat) ? 'UK' : nat; }
  private when(): number { const d = turnToDate(this.b.ticket.turn); return d.year + d.month / 12; }
  /** A kit's own model for an army, or the stand-in in its coat (the Axis' or the rest's). */
  private kitModel(K: { models: Record<string, string>; axisStand: string; stand: string }, army: string): string {
    return K.models[army] ?? (DR.mech.kit.axis.includes(army) ? K.axisStand : K.stand);
  }

  /**
   * Self-propelled guns (release 2, LandBattle.spg; looks only): each of a battery's n guns, its model drawn
   * self-propelled or '' towed. Where the campaign says what the corps has (ForceKit.spg, "Units decide"), that share
   * of its guns, its army's own model; else the history start's (DR.spg.rows): its side's blocks of that army of the
   * row's kinds bring 'per' each, taken from that army's batteries in the order of their blocks. A battery's first guns
   * first (the rules lift the last first, so they stand longest).
   */
  private spgOf(u: LandUnit, n: number): string[] {
    const none = Array.from({ length: n }, () => '');
    if (!this.b.spg || u.kind !== 'art' || u.team || n <= 0) return none;
    const army = this.armyOf(u.nat), share = u.kit?.spg;
    if (share !== undefined) {
      const s = Math.round(n * clamp(share, 0, 1)), own = this.kitModel(DR.spg.kit, army);
      return none.map((_, k) => (k < s ? own : ''));
    }
    const when = this.when();
    const row = (DR.spg.rows[army] ?? []).find(([from, to]) => when >= from && when < to);
    if (!row) return none;
    const [, , list, per, kinds] = row;
    const mine = this.b.allBlocks().filter((x) => x.side === u.side && this.armyOf(x.nat) === army);
    let left = Math.round(mine.filter((x) => kinds.includes(x.kind)).length * per);
    // (the guns before this battery's, among its army's batteries on its side, in the order of their blocks)
    let before = 0;
    for (const x of mine) {
      if (x.id === u.id) break;
      if (x.kind === 'art' && !x.team && x.kit?.spg === undefined) before += Math.max(0, x.figures0);
    }
    left = Math.max(0, left - before);
    const s = Math.min(n, left);
    return none.map((_, k) => (k < s ? list[(before + k) % list.length] : ''));
  }

  /**
   * Tank destroyers (release 2, LandBattle.td; looks only; the design director, 23 November 07:28Z, (a)): each of an
   * armoured block's n tanks, its model drawn as a tank destroyer or '' its block's own. The corps' share where the
   * campaign says it (ForceKit.td), its army's own model; else its army's row in the battle's month (DR.td.rows). Spread
   * through the block: the k-th is one where the share's count steps up (a quarter: the fourth of each four).
   */
  private tdOf(u: LandUnit, n: number): string[] {
    const none = Array.from({ length: n }, () => '');
    if (!this.b.td || u.kind !== 'arm' || n <= 0) return none;
    const army = this.armyOf(u.nat), kit = u.kit?.td;
    let share: number, list: string[];
    if (kit !== undefined) { share = clamp(kit, 0, 1); list = [this.kitModel(DR.td.kit, army)]; }
    else {
      const when = this.when();
      const row = (DR.td.rows[army] ?? []).find(([from, to]) => when >= from && when < to);
      if (!row) return none;
      share = row[3]; list = row[2];
    }
    let c = 0;
    return none.map((_, k) => (Math.floor((k + 1) * share + 1e-9) > Math.floor(k * share + 1e-9) ? list[c++ % list.length] : ''));
  }

  /**
   * Armoured cars and AA vehicles (release 2, LandBattle.cars and aa; looks only; the historian's rows, 23 November
   * 07:47Z; the design director, 07:47Z): the models of a block's cars or AA vehicles, none where it brings none. Where
   * the campaign says what the corps has (ForceKit.cars, ForceKit.aa: so many to each of its blocks, 1 in so many below
   * one), that many, its army's own model or the stand-in in its coat; else every row of its army in the battle's month
   * that names its kind (a battery is 'art'; a support team none). Below one a block, 1 in so many in the order of the
   * blocks of its kind (its corps' by the kit, its army's on its side by the rows): every other one at a half.
   */
  private escortOf(u: LandUnit, what: 'cars' | 'aa'): string[] {
    if (!(what === 'cars' ? this.b.cars : this.b.aa) || u.team || u.kind === 'fort') return [];
    const E = DR[what], army = this.armyOf(u.nat);
    const many = (per: number, i: number): number => Math.max(0, Math.floor((i + 1) * per + 1e-9) - Math.floor(i * per + 1e-9));
    const before = (same: (x: LandUnit) => boolean): number => this.b.allBlocks().filter((x) => x.side === u.side && x.kind === u.kind && !x.team && x.id < u.id && same(x)).length;
    const kit = u.kit?.[what];
    if (kit !== undefined) return Array.from({ length: many(kit, before((x) => x.corps === u.corps)) }, () => this.kitModel(E.kit, army));
    const when = this.when(), i = before((x) => this.armyOf(x.nat) === army), out: string[] = [];
    for (const [from, to, model, per] of E.rows[army] ?? []) {
      if (when < from || when >= to || per[u.kind] === undefined) continue;
      for (let c = many(per[u.kind], i); c > 0; c--) out.push(model);
    }
    return out;
  }

  /**
   * Jeeps, field cars and motorcycles (release 2, LandBattle.light; looks only; the historian's rows, 23 November
   * 08:18Z, historian/light-cars-motorcycles-1123.md): a block's light vehicles, cars first, never more than
   * DR.light.most. Where the campaign says what the corps has (ForceKit.jeeps, ForceKit.bikes: so many to each of its
   * blocks, 1 in so many below one), its army's own models; else its army's rows in the battle's month (every other
   * army's, '_', where its army has none) that name its kind, 1 in so many as the armoured cars, a row's models taken in
   * turn. Each with her work: out ahead where the month's rows say so for the block's kind, else parked behind it (a
   * motorcycle there riding despatch); and her swaps: a car behind is a staff car without her gun, a motorcycle's
   * sidecar by nation (DR.light.sw).
   */
  lightOf(u: LandUnit): { ml: string; duty: 'ahead' | 'behind' | 'despatch'; sw?: Record<string, string> }[] {
    if (!this.b.light || u.team || u.kind === 'fort') return [];
    const L = DR.light, army = this.armyOf(u.nat), when = this.when();
    const many = (per: number, i: number): number => Math.max(0, Math.floor((i + 1) * per + 1e-9) - Math.floor(i * per + 1e-9));
    const before = (same: (x: LandUnit) => boolean): number => this.b.allBlocks().filter((x) => x.side === u.side && x.kind === u.kind && !x.team && x.id < u.id && same(x)).length;
    const out: { ml: string; duty: 'ahead' | 'behind' | 'despatch'; sw?: Record<string, string> }[] = [];
    for (const what of ['cars', 'bikes'] as const) {
      const rows = (L[what][army] ?? L[what]._ ?? []).filter(([from, to]) => when >= from && when < to);
      const ahead = rows.some((r) => r[4][u.kind] === 'ahead');
      const add = (ml: string): void => {
        const sw: Record<string, string> = {};
        for (const [swap, by] of Object.entries(L.sw[ml] ?? {})) { const c = by[u.nat] ?? by._; if (c) sw[swap] = c; }
        if (what === 'cars' && !ahead) sw.mg = 'off';
        out.push({ ml, duty: ahead ? 'ahead' : what === 'bikes' ? 'despatch' : 'behind', ...(Object.keys(sw).length ? { sw } : {}) });
      };
      const kit = u.kit?.[what === 'cars' ? 'jeeps' : 'bikes'];
      if (kit !== undefined) {
        for (let c = many(kit, before((x) => x.corps === u.corps)); c > 0; c--) add(this.kitModel(L.kit[what], army));
        continue;
      }
      const i = before((x) => this.armyOf(x.nat) === army);
      for (const [, , models, per] of rows) {
        if (per[u.kind] === undefined) continue;
        // (a row's models in turn through its army's vehicles of it, block by block of every kind: the SOV's GAZ-67s and
        // lend-lease jeeps, half and half)
        const n0 = Object.entries(per).reduce((n, [K, p]) => n + Math.floor(this.b.allBlocks().filter((x) => x.side === u.side && x.kind === K && !x.team && x.id < u.id && this.armyOf(x.nat) === army).length * p + 1e-9), 0);
        for (let c = many(per[u.kind], i), j = 0; j < c; j++) add(models[(n0 + j) % models.length]);
      }
    }
    return out.slice(0, L.most);
  }

  /**
   * A block's armoured cars, its AA vehicles and its jeeps and motorcycles (escortOf, lightOf), each a squad of vehicles only, drawn as tanks of their own
   * models (ml) with the block's men (view). Like armoured infantry they are a view of the block: its place, facing,
   * order, state and strength are its own, read live, and its box is grown to hold where they keep (outriders), so the
   * rules' counts and places are untouched. Kept under the block's id less one, less a million for the cars, two for
   * the AA and three for the light vehicles, numbered k from DR.mech.base and 100, 200 or 300 on; they fall as the block's figures go, in that share (the last
   * at half its figures), each to a round from the enemy owed most for its harm.
   */
  private addEscorts(u: LandUnit): void {
    for (const what of ['cars', 'aa', 'light'] as const) {
      const key = -(u.id + 1) - (what === 'cars' ? 1e6 : what === 'aa' ? 2e6 : 3e6);
      this.squads.delete(key);
      const light = what === 'light' ? this.lightOf(u) : [];
      const models = what === 'light' ? light.map((x) => x.ml) : this.escortOf(u, what);
      if (!models.length) continue;
      const c = models.length, base = DR.mech.base + (what === 'cars' ? 100 : what === 'aa' ? 200 : 300), L = DR.light;
      const reach = what === 'cars' ? DR.cars.ahead + DR.cars.side + 40 : what === 'aa' ? DR.aa.back + DR.aa.battery + 40 : Math.max(L.ahead + L.side, L.back + L.run) + 40;
      const v = Object.create(u) as LandUnit;
      Object.defineProperties(v, {
        // (no foot's drill nor a tank block's reads them: a battery's kind, with no guns)
        kind: { value: 'art', enumerable: true },
        team: { value: undefined, enumerable: true },
        figures0: { value: base + c, enumerable: true },
        figures: { get: () => (u.figures <= 0 ? 0 : base + Math.min(c, Math.round((c * u.figures) / Math.max(1, u.figures0)))), enumerable: true },
        tray: { value: c, enumerable: true },
        w: { get: () => u.w + 2 * reach, enumerable: true },
        d: { get: () => u.d + 2 * reach, enumerable: true },
      });
      const men: Man[] = models.map((ml, i) => ({
        k: base + i, n: (base + i) * 100, role: 'tank', team: 0, x: u.x, y: u.y, face: u.face, pose: 'halt', v: 0, spot: [u.x, u.y], cover: 'none', until: 0, slotAt: [u.x, u.y], jx: 0, jy: 0, ml,
        ...(light[i] ? { duty: light[i].duty, ...(light[i].sw ? { sw: light[i].sw } : {}) } : {}),
      }));
      const sq: Squad = {
        u: v, tier: 0, men, veh: [], fo: null, phase: 'rest', since: 0, nextThink: roll(this.seed, u.id, 5, base) * 0.5, lastStr: u.str, harm: 0, shelledAt: -99,
        lastX: u.x, lastY: u.y, heading: u.face, speed: 0, track: [], wx: 0, wy: 0, net: 0, down: true, fast: 0, slow: 0, close: false, ran: false, salvoAt: 0, ripple: 0, flank: 0, officerAt: 0, leap: true,
        m0: c, lost: 0, of: u.id, esc: what,
      };
      // Drawn up where the battle opens: in their places already.
      const ahead = what !== 'aa' && this.screening(sq, u);
      men.forEach((m, i) => { const s = this.escortSpot(sq, i, u, ahead); m.x = s[0]; m.y = s[1]; m.spot = s; m.slotAt = s; });
      this.squads.set(key, sq);
    }
  }

  /** Whether a block's armoured cars are out ahead as its screen: it has not met the enemy (DR.cars.contact), nor fights, holds or goes back. */
  private screening(sq: Squad, own: LandUnit): boolean {
    const ph = this.squads.get(own.id)?.phase ?? 'rest', e = this.threat(own);
    return own.engaged === undefined && fighting(own) && (!e || this.b.gap(own, e) > DR.cars.contact) && !['fight', 'pinned', 'hold', 'fallback', 'flee', 'battery', 'gone'].includes(ph) && sq.harm <= 0;
  }

  /** Where a block's i-th car or AA vehicle keeps: cars on its flanks, out ahead or back behind; AA in a row behind it (a battery's behind its tractors). */
  private escortSpot(sq: Squad, i: number, own: LandUnit, ahead: boolean): [number, number] {
    if (sq.esc === 'light') {
      // Jeeps and motorcycles: out on the flanks ahead with the screen; else at its rear corners, a despatch rider riding
      // back DR.light.run metres and up again each DR.light.every seconds (her own time in it).
      const L = DR.light, m = sq.men[i], s = i % 2 ? 1 : -1;
      if (m.duty === 'ahead' && ahead) return toWorld(own, own.face, s * (own.w / 2 + L.side), own.d / 2 + L.ahead);
      const p = m.duty === 'despatch' ? ((this.b.t + roll(this.seed, own.id, m.k, 7) * L.every) % L.every) / L.every : 0;
      return toWorld(own, own.face, s * (own.w / 2 + 8), -own.d / 2 - L.back - (p >= 0.35 && p < 0.7 ? L.run : 0));
    }
    if (sq.esc === 'cars') {
      const C = DR.cars, s = i % 2 ? 1 : -1, row = Math.floor(i / 2);
      return toWorld(own, own.face, s * (own.w / 2 + C.side + row * 20), ahead ? own.d / 2 + C.ahead + row * 25 : -own.d / 2 - C.back - row * 20);
    }
    const A = DR.aa, n = sq.men.length;
    return toWorld(own, own.face, (i - (n - 1) / 2) * 22, -own.d / 2 - A.back - (own.kind === 'art' ? A.battery : 0));
  }

  /**
   * A block's armoured cars or AA vehicles, each half second: each drives to her place (escortSpot) at the block's pace
   * and a little more, and halts there. A car's turret is on the enemy once he is within sight of the block; buttoned
   * up close to him. None fires (looks only: the design director, 23 November 07:47Z).
   */
  private outriders(sq: Squad): void {
    const own = Object.getPrototypeOf(sq.u) as LandUnit, t = this.b.t;
    sq.phase = this.squads.get(own.id)?.phase ?? 'rest';
    if (sq.phase === 'gone') return;
    const e = this.threat(own), near = e ? this.b.gap(own, e) : Infinity;
    const ahead = sq.esc !== 'aa' && this.screening(sq, own);
    sq.men.forEach((m, i) => {
      if (!this.stands(sq, m)) return;
      const s = this.escortSpot(sq, i, own, ahead), d = Math.hypot(s[0] - m.x, s[1] - m.y);
      m.aim = sq.esc === 'cars' && e && near < 1200 ? [e.x, e.y] : undefined;
      m.buttoned = (sq.esc === 'cars' && near < 400) || undefined;
      // (tank minds: each her own place, her own moment to move off and to halt, her own aim: mindOut)
      if (this.tankmind) { this.mindOut(sq, m, i, s, e, near); return; }
      if (d > 5) { m.spot = s; m.slotAt = s; m.pose = 'drive'; m.v = clamp(sq.speed * 1.2 + d * 0.25, 3, 12); m.until = t + 1; }
      else if (m.pose === 'drive' || m.pose === 'reverse') { m.spot = [m.x, m.y]; m.slotAt = s; m.v = 0; m.pose = 'halt'; }
    });
  }

  // ---- Who is in a block ------------------------------------------------------------------------------------------

  private make(u: LandUnit, mech?: { base: number; n: number; carriers: string[]; of: number }): Squad {
    const men: Man[] = [], veh: Man[] = [];
    const n = mech ? mech.n : Math.max(0, u.figures0);
    const roles = rolesFor(u.kind, u.tray, n, u.team);
    // (armoured infantry: as sections at most, a section to a carrier; their k and their dice from DR.mech.base up)
    const fill = this.fillFor(u, roles.slice(0, n).map((r) => r[0])), tier = mech ? (Math.min(1, fill.tier) as 0 | 1) : fill.tier;
    const B = mech?.base ?? 0;
    const man = (k: number, nn: number, role: Role, team: 0 | 1 | 2): Man => {
      nn += B * 100;
      const jx = (roll(this.seed, u.id, nn, 1) - 0.5) * 6, jy = (roll(this.seed, u.id, nn, 2) - 0.5) * 5;
      return { k: k + B, n: nn, role, team, x: u.x, y: u.y, face: u.face, pose: 'idle', v: 0, spot: [u.x, u.y], cover: 'none', until: 0, slotAt: [u.x, u.y], jx, jy };
    };
    for (let k = 0; k < n; k++) men.push(man(k, k, roles[k][0], roles[k][1]));
    // (self-propelled guns and tank destroyers, looks only: a gun or tank drawn as another model, ml; a self-propelled
    // gun has no crew on foot and no tractor)
    const sp = mech ? [] : this.spgOf(u, n), td = mech ? [] : this.tdOf(u, n);
    let selfPropelled = 0;
    for (let k = 0; k < n; k++) {
      const ml = sp[k] || td[k];
      if (!ml) continue;
      men[k].ml = ml;
      if (sp[k]) { fill.sec[k] = 0; fill.plat[k] = 0; selfPropelled++; }
    }
    // The rest of each model's section, after the key men (so a key man is always first of his k); drawn as platoons, a
    // rifle model's other squads (each on its own leader) and its headquarters after it. Lorried, each group has a lorry.
    // Under the battle's scale (GR3) a key man's section and platoon may be drawn short (fill): their first men only.
    let lor = 0;
    if (tier >= 1) for (let k = 0; k < n; k++) {
      const key = men[k], mot = u.kind === 'mot' && tier === 2;
      if (mot) { key.lor = lor; key.seat = 0; }
      const sec = sectionOf(u.nat, key.role);
      sec.slice(0, fill.sec[k]).forEach(([role, pr, scout], i) => {
        const m = man(k, 1000 + k * 32 + i + 1, role, key.team);
        m.j = i + 1; m.sec = k + B;
        if (pr) m.pr = true;
        if (scout) m.scout = true;
        if (mot) { m.lor = lor; m.seat = i + 1; }
        men.push(m);
      });
      if (fill.sec[k] > 0) key.hasSec = true;
      lor++;
      if (tier < 2 || key.role !== 'rifle' || fill.plat[k] <= 0) continue;
      key.plat = true;
      const P = platoonOf(u.nat), most = fill.plat[k];
      let j = sec.length, x = 0;
      const add = (sub: number, role: Role, seat: number, opts: { pr?: boolean; lead?: boolean } = {}): boolean => {
        if (x >= most) return false;
        const m = man(k, 20000 + k * 128 + x++, role, key.team);
        m.j = ++j; m.sec = k + B; m.sub = sub;
        if (opts.pr) m.pr = true;
        if (opts.lead) m.lead = true;
        if (mot) { m.lor = lor; m.seat = seat; }
        men.push(m);
        return true;
      };
      // The platoon's other squads, each its leader and the same section as the key man's (its scouts in the squad).
      for (let s = 1; s < P.squads && x < most; s++, lor++) {
        add(s, 'rifle', 0, { lead: true });
        sec.forEach(([role, pr], i) => add(s, role, i + 1, { pr }));
      }
      // Its headquarters: its leader and the men with him (a sergeant, the runners).
      if (x < most) {
        add(P.squads, 'rifle', 0, { lead: true });
        for (let i = 1; i < P.hq; i++) add(P.squads, 'rifle', i);
        lor++;
      }
    }
    // Lorried foot's lorries (drawn as sections, one to each section; as platoons, one to each group) and a battery's
    // tractor (one to each gun).
    // (armoured infantry: each section its carrier, of its army's row)
    const towed = n - selfPropelled;
    const lorries = mech ? n : u.kind === 'mot' ? (tier >= 1 ? lor : n >= 4 ? 2 : 1) : u.kind === 'art' && !u.team ? (tier >= 1 ? towed : towed > 0 ? 1 : 0) : 0; // a team's crews carry their weapons
    for (let i = 0; i < lorries; i++) veh.push({ k: 100 + i, n: 100 + i + B * 100, role: 'lorry', team: 0, x: u.x, y: u.y, face: u.face, pose: 'parked', v: 0, spot: [u.x, u.y], cover: 'none', until: 0, slotAt: [u.x, u.y], jx: 0, jy: 0, ...(mech ? { ml: mech.carriers[i % mech.carriers.length] } : {}) });
    const sq: Squad = {
      u, tier, men, veh, fo: null, phase: 'rest', since: 0, nextThink: roll(this.seed, u.id, 3) * 0.5, lastStr: u.str, harm: 0, shelledAt: -99,
      lastX: u.x, lastY: u.y, heading: u.face, speed: 0, track: [], wx: 0, wy: 0, net: 0, down: u.kind !== 'mot' && u.kind !== 'cav', fast: 0, slow: 0, close: false, ran: false, salvoAt: 0, ripple: 0, flank: 0, officerAt: 0, leap: true,
      m0: men.length, lost: 0, ...(mech ? { of: mech.of } : {}),
    };
    // Drawn up where the battle opens: each man at his place in the line, in the best cover near it.
    this.layStart(sq);
    return sq;
  }

  /**
   * How many of each key man's section (sec) and platoon (plat: its other squads and headquarters, in order) a block
   * draws (GR3; the production lead, 22 November 07:23Z). Unscaled, all the battle's setting draws. Under the scale its
   * key men always (the rules' models), then their sections in squad order, then the rifle models' platoons squad by
   * squad, as many men as fit its men0 over N, counted as the 3D game shows them (menOnScreen): the last group drawn
   * short, so every block draws within a man of its cap where its whole draw would be more. tier: the most any key draws.
   */
  private fillFor(u: LandUnit, keys: Role[]): { tier: 0 | 1 | 2; sec: number[]; plat: number[] } {
    const top: 0 | 1 | 2 = this.platoons ? 2 : this.sections ? 1 : 0;
    const secs = keys.map((r) => (top >= 1 ? sectionOf(u.nat, r).length : 0));
    const P = platoonOf(u.nat);
    const plats = keys.map((r, i) => (top === 2 && r === 'rifle' ? (P.squads - 1) * (1 + secs[i]) + P.hq : 0));
    if (this.scale === null || top === 0) return { tier: top, sec: secs, plat: plats };
    let room = Math.floor(u.men0 / this.scale) - keys.reduce((c, r) => c + menOnScreen(r), 0);
    const take = (most: number): number => { const n = Math.max(0, Math.min(room, most)); room -= n; return n; };
    const sec = secs.map(take), plat = plats.map(take);
    return { tier: plat.some((x) => x > 0) ? 2 : sec.some((x) => x > 0) ? 1 : 0, sec, plat };
  }

  private layStart(sq: Squad): void {
    const u = sq.u;
    sq.phase = this.phaseOf(sq);
    const tpl = u.kind === 'arm' || u.kind === 'art' ? 'line' : sq.phase === 'rest' ? 'rest' : 'line';
    for (const m of sq.men) {
      const [a, l] = this.slot(sq, m, tpl);
      const s = toWorld(u, u.face, a, l);
      const best = this.seek(sq, m, s, this.threatDir(sq), sq.men);
      m.x = best.x; m.y = best.y; m.spot = [best.x, best.y]; m.cover = best.kind; m.slotAt = s;
      m.pose = u.kind === 'arm' ? 'halt' : u.kind === 'art' ? 'deployed' : u.kind === 'mot' || u.kind === 'cav' ? (u.kind === 'mot' ? 'mounted' : 'ride') : 'wait';
    }
    sq.veh.forEach((v, i) => {
      const s = toWorld(u, u.face, (i - (sq.veh.length - 1) / 2) * 14, -u.d / 2 - 30);
      v.x = s[0]; v.y = s[1]; v.spot = s; v.slotAt = s; v.pose = 'parked';
    });
    // Lorried infantry start aboard their lorries, unless their block opens holding its ground: then they are down
    // and in their places already, the lorries parked behind.
    if (u.kind === 'mot') {
      if (sq.phase === 'hold' || sq.phase === 'startline' || sq.phase === 'fight' || sq.phase === 'pinned' || !sq.veh.length) {
        sq.down = true;
        for (const m of sq.men) m.pose = 'wait';
      } else for (const m of sq.men) { const p = this.seat(sq, m); m.x = p[0]; m.y = p[1]; m.spot = p; m.cover = 'none'; }
    }
    // Drawn as sections, no two men on one spot from the first (space): each stands clear of those placed before him.
    if (sq.tier >= 1) {
      const room: Room = new Map(), order = [...sq.men].sort((a, c) => (a.j === undefined ? 0 : 1) - (c.j === undefined ? 0 : 1) || (a.sec ?? a.k) - (c.sec ?? c.k) || (a.j ?? 0) - (c.j ?? 0));
      for (const m of order) {
        if (VEHICLE.has(m.role) || CARRIED.has(m.pose)) continue;
        const p = this.leash(sq, this.clear(room, sq, m, [m.x, m.y]), -4);
        m.x = p[0]; m.y = p[1]; m.spot = p;
        this.bookRoom(room, m, p);
      }
    }
  }

  // ---- Each step ----------------------------------------------------------------------------------------------------

  step(): void {
    const b = this.b, t = b.t;
    // The battle's last step: the models the rules took in it fall (with their rounds) before every man stands easy.
    if (b.over) { if (!this.settled) { for (const sq of this.squads.values()) if (!(this.exits && this.leftAtEnd(sq))) this.falls(sq); this.settle(); } return; }
    let old = 0;
    while (old < this.shots.length && this.shots[old].t < t - DR.rounds.keep) old++;
    if (old) this.shots.splice(0, old);
    for (const sq of this.squads.values()) {
      const u = sq.u;
      // The share of its strength it lost this step, smoothed over about two seconds.
      const lost = Math.max(0, sq.lastStr - u.str) / Math.max(1e-6, u.str0);
      sq.harm = sq.harm * 0.95 + (lost / DT) * 0.05;
      sq.lastStr = u.str;
      const moved = Math.hypot(u.x - sq.lastX, u.y - sq.lastY);
      sq.speed = sq.speed * 0.8 + (moved / DT) * 0.2;
      if (moved > 0.05) sq.heading = bearing(sq.lastX, sq.lastY, u.x, u.y);
      sq.lastX = u.x; sq.lastY = u.y;
      sq.track.push(u.x, u.y);
      if (sq.track.length > 2 * (Math.round(MOUNT.span / DT) + 1)) sq.track.splice(0, 2);
      const span = (sq.track.length / 2 - 1) * DT;
      sq.wx = span > 0 ? (u.x - sq.track[0]) / span : 0; sq.wy = span > 0 ? (u.y - sq.track[1]) / span : 0;
      sq.net = Math.hypot(sq.wx, sq.wy);
      sq.slow = sq.speed < MOUNT.down || sq.net < MOUNT.down ? sq.slow + DT : 0;
      // Fast: making ground at a lorry's or a horse's pace, and going so now. A moment's check is not a halt: the count of
      // fast seconds wears off then, and is gone once it has stood a second.
      sq.fast = sq.net > MOUNT.up && sq.speed > MOUNT.up ? sq.fast + DT : sq.slow >= 1 ? 0 : Math.max(0, sq.fast - 2 * DT);
      // (men who leave the field: a block out of the fight takes no more part in the drill; its men leave on their own)
      if (this.exits && this.leaves(sq)) continue;
      this.falls(sq);
      // Stretcher bearers (release 2): the bearers fetch, carry and come home (off the block's drill while they do).
      if (b.bearers && !sq.esc) this.litters(sq);
      // (a block's armoured cars and AA vehicles: their own drive, outriders)
      if (t >= sq.nextThink && sq.esc) { sq.nextThink = t + 0.5; this.outriders(sq); }
      else if (t >= sq.nextThink) { sq.nextThink = t + 0.5; if (this.byBook) this.feel(sq); this.think(sq); this.pairs(sq, this.space(sq)); if (b.escort) this.escort(sq); if (sq.screenQ) this.screenRound(sq); }
      if (this.hatches && u.kind === 'arm' && !sq.esc) this.hatchNow(sq);
      this.walk(sq);
      this.fire(sq);
    }
    this.shellKills.clear(); this.byOf.clear();
    if (b.escort) this.clearOfVehicles();
    if (t >= this.leadAt) { this.leadAt = t + 0.5; this.pickLeads(); }
  }

  /**
   * The squad leading each side's assault, for the platoon cam (release 2): of the side's foot squads still standing,
   * those in the assault (on the move now, or within LEAD_RUSH seconds: a squad rushes, drops and fires, and rushes
   * again on its turn), the one nearest the enemy; where none is, the foot squad nearest the enemy. The camera is not
   * thrown from squad to squad: the one leading keeps the lead while it stands and is still in the assault (or none
   * is), unless another is LEAD_BY metres nearer the enemy. Looks only: nothing in the battle reads it.
   */
  private pickLeads(): void {
    const LEAD_BY = 25, LEAD_RUSH = 15, t = this.b.t;
    const live_ = [...this.squads.values()].filter((sq) => live(sq.u));
    for (const side of ['att', 'def'] as Side[]) {
      const foes = live_.filter((sq) => sq.u.side !== side && !sq.u.hid);
      const near = (x: number, y: number): number => foes.reduce((d, f) => Math.min(d, Math.hypot(f.u.x - x, f.u.y - y)), Infinity);
      type Cand = { id: number; g: number; d: number; on: boolean };
      const cands: Cand[] = [];
      for (const sq of live_) {
        if (sq.u.side !== side || sq.u.kind === 'arm' || sq.u.kind === 'art' || sq.u.kind === 'fort') continue;
        const groups = new Map<number, { x: number; y: number; n: number; moving: boolean }>();
        for (const m of sq.men) {
          if (!this.stands(sq, m) || m.role === 'holder') continue;
          const key = grp(m), e = groups.get(key) ?? { x: 0, y: 0, n: 0, moving: false };
          e.x += m.x; e.y += m.y; e.n++;
          if (this.byBook ? m.el === 2 : MOVING.has(m.pose)) e.moving = true;
          groups.set(key, e);
        }
        for (const [g, e] of groups) {
          const key = `${sq.u.id}:${g}`;
          if (e.moving) this.rushed.set(key, t);
          const at = this.rushed.get(key);
          cands.push({ id: sq.u.id, g, d: foes.length ? near(e.x / e.n, e.y / e.n) : 0, on: at !== undefined && t - at <= LEAD_RUSH });
        }
      }
      if (!cands.length) { this.leads.delete(side); continue; }
      const pool = cands.some((c) => c.on) ? cands.filter((c) => c.on) : cands;
      const best = pool.reduce((a, c) => (c.d < a.d ? c : a));
      const was = this.leads.get(side), cur = was && pool.find((c) => c.id === was[0] && c.g === was[1]);
      this.leads.set(side, cur && best.d > cur.d - LEAD_BY ? was! : [best.id, best.g]);
    }
  }

  // ---- Men who leave the field are seen leaving (Drill.exits) ----
  //
  // Brian, 25 November 2026, 17:11Z: "some soldiers freeze and then disapear". The rules take a block out of the fight
  // once it has fled off the field ('gone') or fallen back to its out line ('withdrawn'), often far inside the field.
  // With the switch on, its men no longer leave view() all in that step: each man who stood the step before leaves on
  // his own, getting up and turning in his own moment, going at his own pace with a drift of his own (Brian, 13:40Z: "i
  // dont want all the soldiers of a squad doing the exact same thing at the same time"), and each is taken off the list
  // once DR.exits.past metres beyond his own edge, or DR.exits.cap battle seconds after his block left. A leaving man
  // fires at nothing, nothing fires at him, and no other block's drill reads him. Looks only: the drill's own dice
  // (salts 421 to 439), and none of this runs while the switch is off.

  /** Each block's figures at the drill's last step while it fought, and the battle time each block left the fight. */
  private readonly leftFig = new Map<Squad, number>();
  private readonly leftAt = new Map<Squad, number>();

  /** A man leaving the field, still on the list. */
  private leavingNow(m: Man): boolean { return this.exits && m.lv !== undefined && !m.lvOut; }

  /** Whether any man of a block (its own, its armoured infantry's, its escorts') is leaving and still on the list. */
  private leavingIn(id: number): boolean {
    for (const sq of this.squads.values()) {
      if (sq.u.id !== id && sq.of !== id) continue;
      if ([...sq.men, ...sq.veh, ...(sq.fo ?? [])].some((m) => this.leavingNow(m))) return true;
    }
    return false;
  }

  /**
   * Each step, before a block's drill: false for a block still in the fight (its figures noted); for one that has left
   * it, its men are named leaving the step it leaves, they go on a step, and true (no more drill of its own: it neither
   * thinks, walks nor fires, and its men do not fall).
   */
  private leaves(sq: Squad): boolean {
    if (live(sq.u)) { this.leftFig.set(sq, sq.u.figures); return false; }
    if (!this.leftAt.has(sq)) this.leaveNow(sq);
    // (stretcher bearers: a carry under way goes on; a bearer home from it leaves after his block)
    if (this.b.bearers && !sq.esc) {
      this.litters(sq);
      if (this.leftFig.has(sq)) for (const m of sq.men) if (m.br && m.fell === undefined && !m.job && m.lv === undefined) this.leaveOne(sq, m);
    }
    // (vehicles first: the men aboard sit in them as they go)
    for (const m of [...sq.veh, ...sq.men, ...(sq.fo ?? [])]) if (m.lv !== undefined && !m.lvOut) this.leaveStep(sq, m);
    return true;
  }

  /** The battle's last step: a block that left the fight in it has its men named leaving too, and none of them falls. */
  private leftAtEnd(sq: Squad): boolean {
    if (live(sq.u)) return false;
    if (!this.leftAt.has(sq)) this.leaveNow(sq);
    return true;
  }

  /**
   * The step a block leaves the fight: every man who stood the step before (a key man while the rules kept his model,
   * the rest of a section not fallen), its vehicles and its spotters are named leaving. A bearer on a carry goes on
   * with it. A block the drill never saw fighting has nobody to name.
   */
  private leaveNow(sq: Squad): void {
    this.leftAt.set(sq, this.b.t);
    const was = this.leftFig.get(sq);
    if (was === undefined) return;
    for (const m of sq.men) if (m.fell === undefined && !m.job && (m.j !== undefined || m.k < was)) this.leaveOne(sq, m);
    for (const m of sq.veh) if (m.fell === undefined) this.leaveOne(sq, m);
    for (const m of sq.fo ?? []) this.leaveOne(sq, m);
  }

  /**
   * One man named leaving: no aim, no cover, nobody's escort or helper, out of any firing pose; his way, his own drift,
   * his own moment to get up and go (a routing man, a vehicle or a rider already on the move goes on at once; a man on
   * foot already on the move keeps going as he was till his moment, and only then breaks into his own run: nobody is
   * stopped dead), his own pace and his gait. A man on foot runs (one routing keeps his gait, else flees); a support
   * team's weapon is carried at the walk it had; vehicles drive, a towed gun behind her tractor, at their block's pace
   * (3 m a battle second at least); riders ride.
   */
  private leaveOne(sq: Squad, m: Man): void {
    const u = sq.u, t = this.b.t, E = DR.exits, routed = u.state === 'gone', veh = VEHICLE.has(m.role), rides = CARRIED.has(m.pose);
    const moving = MOVING.has(m.pose) && m.v > 0, r = (salt: number): number => roll(this.seed, u.id, m.n, salt);
    m.lv = t; m.aim = undefined; m.hidden = undefined; m.cover = 'none'; m.esc = undefined; m.aid = undefined; m.rapid = undefined; m.burst = undefined; m.coax = undefined;
    // (his edge: his own side's, the way a routing block runs too)
    m.lvDir = u.side === 'att' ? -1 : 1;
    m.lvA = (r(424) - 0.5) * 0.5;
    m.lvGo = moving && (routed || veh || rides) ? t : t + E.rise * r(422);
    if (veh || rides) m.lvV = clamp(Math.max(3, sq.speed) * (0.9 + 0.2 * r(425)), 3, m.role === 'tank' ? tankTop(sq) : rides ? 14 : 16);
    else if (CREWED.has(m.role)) m.lvV = (moving ? m.v : DR.teams.pace) * (0.92 + 0.16 * r(426));
    else m.lvV = E.pace[0] + (E.pace[1] - E.pace[0]) * r(423) + (routed ? 1 : 0);
    m.lvP = m.role === 'tank' || m.role === 'lorry' ? 'drive' : m.role === 'gun' ? 'towed' : rides ? m.pose
      : CREWED.has(m.role) ? (moving ? m.pose : 'walk') : routed ? (moving && (m.pose === 'flee' || m.pose === 'run' || m.pose === 'advance') ? m.pose : 'flee') : 'run';
    // Till he goes: out of any firing pose, standing (a support team's weapon set down) where he was not on the move, as he
    // was otherwise (on the move, he goes on as he was).
    const still: Partial<Record<Pose, Pose>> = { fire_stand: 'idle', fire_kneel: 'wait', fire_prone: 'hide', lmg_burst: 'hide', throw_grenade: 'idle', fire: m.role === 'tank' ? 'halt' : m.role === 'gun' ? 'deployed' : 'wait' };
    if (m.lvGo > t && !moving) { m.v = 0; m.pose = still[m.pose] ?? (MOVING.has(m.pose) ? (veh ? (m.role === 'lorry' ? 'parked' : 'halt') : CREWED.has(m.role) ? 'wait' : 'idle') : m.pose); }
    else m.pose = still[m.pose] ?? m.pose;
  }

  /**
   * A leaving man's step: aboard, in his seat in his lorry wherever she goes (off the list with her); a towed gun stands
   * till her tractor has come for her, then follows on her hitch (off the list with it); else he turns toward his way
   * as fast as he can turn and, once his moment has come (or while he is still on the move as he was), goes on along it
   * at his pace, keeping a little way on while he turns (no one goes backward, crabs or turns on the spot; his x kept on
   * the field, his y not). A tractor with a gun to fetch heads for her first. Off the list once past his edge, or at the
   * cap.
   */
  private leaveStep(sq: Squad, m: Man): void {
    const f = this.b.field, t = this.b.t, x0 = m.x, y0 = m.y, veh = VEHICLE.has(m.role);
    // Off the list once he was past his edge at his last step (so every man is seen leaving at least once), or at the cap.
    if (m.lv! < t && ((m.lvDir! > 0 ? y0 - f.h : -y0) >= DR.exits.past || t - (this.leftAt.get(sq) ?? t) >= DR.exits.cap)) m.lvOut = true;
    const tow = m.role === 'gun' && !m.ml ? this.tractorOf(sq, m) : undefined;
    if (m.pose === 'mounted' && (sq.u.kind === 'mot' || m.role === 'crew') && sq.veh.length && !m.off) {
      const lorry = this.lorryOf(sq, m), p = this.seat(sq, m);
      if (lorry.lvOut) m.lvOut = true;
      m.x = p[0]; m.y = p[1]; m.face = lorry.face; m.v = lorry.v;
    } else if (tow) {
      // (one already on the move closes on her hitch behind her tractor, as the drill draws a battery on the move; one
      // standing waits for her tractor to come for her; on the hitch she is drawn on a 6 m bar behind it, never jumping)
      const [fx, fy] = fwd(tow.face), hx = tow.x - fx * 6, hy = tow.y - fy * 6, d = Math.hypot(tow.x - m.x, tow.y - m.y);
      if (!m.lvHk && m.pose === 'towed' && tow.lv !== undefined) {
        const e = Math.hypot(hx - m.x, hy - m.y), s = (Math.max(m.v, m.lvV!) + 1.5) * DT;
        if (e <= s) { m.x = hx; m.y = hy; m.lvHk = true; } else { m.face = bearing(m.x, m.y, hx, hy); m.x += ((hx - m.x) / e) * s; m.y += ((hy - m.y) / e) * s; }
      } else if (!m.lvHk && d <= 8 && tow.lv !== undefined) m.lvHk = true;
      if (m.lvHk) {
        if (tow.lvOut) m.lvOut = true;
        const e = Math.hypot(tow.x - m.x, tow.y - m.y);
        if (e > 0.5) { m.x = tow.x - ((tow.x - m.x) / e) * 6; m.y = tow.y - ((tow.y - m.y) / e) * 6; m.face = bearing(m.x, m.y, tow.x, tow.y); }
        m.v = tow.v; m.pose = 'towed';
      } else if (m.pose !== 'towed' || tow.lv === undefined) { m.v = 0; m.pose = 'deployed'; }
    } else {
      const gun = veh && m.role === 'lorry' ? this.toFetch(sq, m) : undefined;
      const want = gun ? bearing(m.x, m.y, gun.x, gun.y) : (m.lvDir! > 0 ? 0 : Math.PI) + m.lvA!;
      const rate = (m.role === 'tank' ? TURN.tank : veh ? TURN.wheels : m.pose === 'ride' ? TURN.ride : TURN.foot) * DT;
      m.face = turnTo(m.face, want, rate);
      const go = t >= m.lvGo!;
      if (go) { m.pose = m.lvP!; m.v = m.lvV!; }
      if (go || (MOVING.has(m.pose) && m.v > 0)) {
        const c = Math.cos(wrap(want - m.face)), s = m.v * DT * Math.max(c, 0.15);
        m.x = clamp(m.x + Math.sin(m.face) * s, 1, f.w - 1); m.y += Math.cos(m.face) * s;
      } else m.v = 0;
    }
    m.spot = [m.x, m.y]; m.slotAt = [m.x, m.y];
    m.went = Math.hypot(m.x - x0, m.y - y0) / DT;
  }

  /** A field gun's tractor, where every towed gun of the battery has her own (one gun to each); else none, and she drives herself. */
  private tractorOf(sq: Squad, g: Man): Man | undefined {
    const towed = sq.men.filter((x) => x.role === 'gun' && !x.ml && x.j === undefined);
    return sq.veh.length >= towed.length ? sq.veh[this.lorryIx(sq, g)] : undefined;
  }

  /** The leaving gun a battery's tractor has still to fetch (not yet hitched), if any. */
  private toFetch(sq: Squad, v: Man): Man | undefined {
    return sq.men.find((g) => g.role === 'gun' && !g.ml && g.lv !== undefined && !g.lvHk && !g.lvOut && g.pose === 'deployed' && this.tractorOf(sq, g) === v);
  }

  /** At the battle's end, a man still leaving stops in a still pose: kneeling, or crouched if his block fled; tanks (and a self-propelled gun) halt, guns stand, lorries park; riders and men aboard as they are. */
  private leftRest(sq: Squad, m: Man): Pose {
    if (m.role === 'tank' || (m.role === 'gun' && m.ml)) return 'halt';
    if (m.role === 'gun') return 'deployed';
    if (m.role === 'lorry') return 'parked';
    if (CARRIED.has(m.pose)) return m.pose;
    return sq.u.state === 'gone' ? 'cower' : 'wait';
  }

  /**
   * The battle is over: every man stops where he is. The winners stand easy, the beaten keep their heads down, and those
   * who ran crouch where they got to; tanks halt, guns stand, lorries park.
   */
  private settle(): void {
    this.settled = true;
    const won = this.b.over?.winner;
    for (const sq of this.squads.values()) {
      const u = sq.u;
      // Stretcher bearers (release 2): a stretcher on the way is set down where it is.
      for (const c of sq.carries ?? []) if (c.stage === 'carry') c.man.pose = 'wounded';
      for (const m of [...sq.men, ...sq.veh, ...(sq.fo ?? [])]) {
        if (m.fell !== undefined) continue;
        m.v = 0; m.went = 0; m.spot = [m.x, m.y]; m.aim = undefined; m.hidden = undefined;
        m.pose = m.role === 'tank' ? 'halt' : m.role === 'gun' || CREWED.has(m.role) ? 'deployed' : m.role === 'lorry' ? 'parked' : m.role === 'holder' ? (m.pose === 'ride' ? 'ride' : 'horses')
          : m.pose === 'mounted' || m.pose === 'ride' ? m.pose : sq.phase === 'flee' ? 'cower' : u.side === won ? (m.k % 3 ? 'idle' : 'wait') : 'wait';
        if (this.exits && m.lv !== undefined) m.pose = this.leftRest(sq, m);
      }
    }
  }

  /** A shell burst at a point (sim.ts shell): men of every block near it duck. */
  shelled(x: number, y: number, by?: number, on?: number, k?: number): void {
    this.duck(x, y);
    // The round that cost that model: fired by the striker's gun or tank nearest him (fire no other round for it).
    if (on === undefined || k === undefined) return;
    this.shellKills.add(on + ':' + k);
    const vq = this.squads.get(on), v = vq?.men.find((x) => x.k === k);
    if (vq && v && !(this.exits && v.lv !== undefined)) this.killRound(by !== undefined ? this.b.unit(by) : undefined, vq, v, [x, y]);
    // (shell blasts: the men of his block the rules take next fall to this burst too, if they stand near it; thin)
    if (vq && this.b.blast) vq.anchor = { x, y, t: this.b.t };
  }

  /** A model the rules took with no striker on the field (the air's bombs and guns): no round of the field's for him. */
  fromAir(on: number, k: number): void { this.shellKills.add(on + ':' + k); }
  /** A model the rules took to a block's small arms (no shell): his round comes from that block (falls). */
  struckBy(by: number, on: number, k: number): void { this.byOf.set(on + ':' + k, by); }

  /** Men of every block near a burst duck. */
  private duck(x: number, y: number): void {
    for (const sq of this.squads.values()) if (Math.hypot(sq.u.x - x, sq.u.y - y) < Math.max(sq.u.w, sq.u.d) / 2 + 40) {
      sq.shelledAt = this.b.t;
      // (hatches: a burst by a tank sends her commander down)
      if (this.hatches && sq.u.kind === 'arm' && !sq.esc) for (const m of sq.men) if (m.role === 'tank' && m.fell === undefined && Math.hypot(m.x - x, m.y - y) < DR.hatch.burst) this.alarm(sq, m, this.b.t);
    }
  }

  /**
   * Hatches: something sends this tank's commander down at battle time `at` (a round at her landing then, a burst by
   * her, the enemy's foot too near). The first of a spell, he drops inside his own moment after it ('drop', a veteran
   * quicker by 'quick'); every one keeps him down his own quiet time longer ('quiet': a green commander long, a veteran
   * briefly). With `mate`, it was at another tank of his block: he closes up for it only a share 'mates' of the time, by
   * his crew's experience (green crews when any of them is fired on, veterans at their own rounds).
   */
  private alarm(sq: Squad, m: Man, at: number, mate = false): void {
    const u = sq.u, H = DR.hatch, x = this.band(u);
    if (mate && roll(this.seed, u.id, m.n, Math.floor(at / 4), 174) >= H.mates[x]) return;
    const gone = m.shutTo === undefined || at > m.shutTo;
    if (gone) m.shutAt = at + (H.drop[0] + (H.drop[1] - H.drop[0]) * roll(this.seed, u.id, m.n, Math.floor(at), 171)) * H.quick[x];
    m.shutTo = Math.max(gone ? 0 : m.shutTo!, at + H.quiet[x] * (H.own[0] + (H.own[1] - H.own[0]) * roll(this.seed, u.id, m.n, 172)));
  }

  /** Hatches: each tank closed up or not this step, by her own commander's times (alarm); one struck in his hatch stays down. */
  private hatchNow(sq: Squad): void {
    const t = this.b.t;
    for (const m of sq.men) if (m.role === 'tank' && m.fell === undefined) m.buttoned = m.cmdDown !== undefined || (m.shutAt !== undefined && t >= m.shutAt && t < m.shutTo!) || undefined;
  }

  /** Where model k of block id stands now (alive, or the moment he falls): the shell that cost him lands there. */
  manAt(id: number, k: number): [number, number] | null {
    const sq = this.squads.get(id);
    const m = sq?.men.find((x) => x.k === k);
    return m ? [m.x, m.y] : null;
  }

  /** Where a wreck lies as drawn: where her tank fell, or the rules' own place for one that fell before the men were drawn. */
  wreckSpot(z: Zone): [number, number] {
    return this.wreckAt.get(z.id) ?? [z.x, z.y];
  }

  /**
   * Men the rules have lifted off the tray fall where they stand, a tank too: she burns where she stood (at one of her
   * posts, up to about 20 m from the place the rules lay her wreck, their own mark for its shelter; wreckSpot).
   */
  // ---- When the leader falls (Drill.leaders): its helpers stand just above falls(), below this line ----
  private falls(sq: Squad): void {
    const u = sq.u, t = this.b.t;
    for (const m of sq.men) {
      if (m.j !== undefined || m.fell !== undefined || m.k < u.figures) continue; // (the rest of a section: thin)
      if (u.state === 'withdrawn' || (u.state === 'gone' && u.figures > 0)) continue;
      // Men of a block that ran off the field did not fall: they went with it (no round, no body).
      if (u.state === 'gone' && (u.y < 0 || u.y > this.b.field.h)) continue;
      const key = u.id + ':' + m.k, by = this.byOf.get(key), shell = this.shellKills.delete(key);
      this.byOf.delete(key);
      m.fell = t; m.v = 0; m.aim = undefined; m.hidden = undefined;
      sq.memo = undefined;
      if (m.role === 'tank' && !sq.esc) {
        // The rules lay a block's wrecks in the order its tanks fall (the lower place first within a tick), as here.
        const nth = sq.men.filter((x) => x.role === 'tank' && x.fell !== undefined && x !== m).length;
        const w = this.b.wrecks.filter((z) => z.unit === u.id)[nth];
        if (w) this.wreckAt.set(w.id, [m.x, m.y]);
      }
      // The round that took him (where he stood): a shell's came with it (Drill.shelled); else small arms (or
      // a launcher, or a grenade, on a tank) from the enemy block the rules say struck him (Drill.struckBy). A model the
      // rules took before the drill was turned on falls with no round.
      if (!shell && by !== undefined) this.killRound(this.b.unit(by), sq, m);
      // (a block's armoured car or AA vehicle: to the enemy owed most for its harm, a shell from tanks or guns)
      else if (!shell && sq.esc) { const st = this.strikerOf(u); if (st) this.killRound(st, sq, m, st.kind === 'arm' || (st.kind === 'art' && st.team !== 'mg') ? [m.x, m.y] : undefined); }
      m.pose = 'down';
      m.spot = [m.x, m.y];
      if (this.b.bearers && !sq.esc) this.hurt(sq, m);
      this.aid(sq, m);
      if (sq.tier >= 1) this.regroup(sq, m);
    }
    if (sq.tier >= 1) this.thin(sq);
  }

  /** A comrade goes to a man who fell, where one is near and the block is not running (nor riding, nor mounting again). */
  private aid(sq: Squad, m: Man): void {
    const u = sq.u, t = this.b.t;
    if (!fighting(u) || m.role === 'tank' || m.role === 'gun' || !sq.down || sq.boarding !== undefined) return;
    const helper = sq.men.filter((h) => this.stands(sq, h) && h.role === 'rifle' && !h.pr && h.aid === undefined && Math.hypot(h.x - m.x, h.y - m.y) < 30)
      .sort((a, c) => Math.hypot(a.x - m.x, a.y - m.y) - Math.hypot(c.x - m.x, c.y - m.y))[0];
    if (helper && roll(this.seed, u.id, m.n, t * 10) < 0.6) {
      helper.aid = m.k; helper.spot = [m.x + 1.2, m.y - 0.8]; helper.until = t + 6; helper.aim = undefined;
      helper.pose = sq.harm > DR.pin.harm ? 'crawl' : 'creep'; helper.v = helper.pose === 'crawl' ? 1.2 : 2.5;
    }
  }

  /**
   * A key man fell (his model lifted): the rest of his section still on their feet join the nearest section left, as the
   * remnant of a shattered section does: a gun's crew another gun, a support team's bearers another weapon of its kind,
   * the rest the nearest section on foot. With none to join, a loader takes up a rifle and fights as one.
   */
  private regroup(sq: Squad, key: Man): void {
    const left = sq.men.filter((m) => m.j !== undefined && m.fell === undefined && (m.sec ?? m.k) === key.k);
    if (!left.length) return;
    const heavy = key.role === 'gun' || CREWED.has(key.role);
    const keys = sq.men.filter((x) => x.j === undefined && x !== key && this.stands(sq, x) && (heavy ? x.role === key.role : !VEHICLE.has(x.role) && !CREWED.has(x.role)));
    const to = this.nearest(keys, key)?.[0];
    for (const m of left) {
      if (to) { m.sec = to.k; continue; }
      if (m.pr && m.role !== 'crew') { m.pr = undefined; if (m.role === 'loader') m.role = 'rifle'; }
    }
    sq.memo = undefined;
  }

  /**
   * The rest of the sections fall one by one as the block loses strength: its men on their feet are kept to its men in
   * all times the share of its strength left, so a block at half strength shows half its men (the key men fall only
   * with their models). Each goes to the enemy block owed most for the harm (the one the rules give the next model to,
   * sim.ts suffer), the man nearest it first (a man in good cover less often), with the round that took him: a shell's
   * takes the men round its burst together, one round each at the one burst (the same t, x and y); small arms one by one.
   * A block that ran off the field, or left it in order, takes its men with it.
   */
  // ---- Infantry with tanks (release 2) -----------------------------------------------------------------------------
  //
  // Brian, 23 November 05:05Z: "if the troops are with tanks, have them advance behind the tank using it as cover like
  // they would in real life for the units near the tank and make sure their players cant be run over by their own
  // vechiles and they get out of the way. vechiles can run over enemy soldiers though which would kill them".
  // LandBattle.escort, looks only (the drill's, DRILL.escort): the rules' counts and places are untouched.

  /** A tank's key (her block's id x 1000 + her k), and the tanks of a side standing, each with her key. */
  private tanksStanding(side: Side, x: number, y: number, within: number): [Man, number][] {
    const out: [Man, number][] = [];
    for (const tq of this.squads.values()) {
      if (tq.u.side !== side || tq.u.kind !== 'arm' || !live(tq.u)) continue;
      if (Math.hypot(tq.u.x - x, tq.u.y - y) > within + Math.max(tq.u.w, tq.u.d) / 2) continue;
      for (const v of tq.men) if (v.role === 'tank' && this.stands(tq, v)) out.push([v, tq.u.id * 1000 + v.k]);
    }
    return out;
  }

  /**
   * A foot block on the advance with friendly tanks near: up to DRILL.escort.men of its riflemen nearest each tank file
   * in behind her, a few metres back and a stride to either side of her tracks, at her pace (she is their cover); when
   * she halts, they kneel behind her. Not while riding, mounting or carrying; the rest of the block keeps its drill.
   */
  private escort(sq: Squad): void {
    const u = sq.u, E = DR.escort, t = this.b.t;
    const on = isFoot(u) && sq.tier >= 1 && (sq.phase === 'deploy' || sq.phase === 'bound') && sq.boarding === undefined && (u.kind !== 'mot' || !!sq.down) && fighting(u);
    const tanks = on ? this.tanksStanding(u.side, u.x, u.y, E.near + Math.max(u.w, u.d) / 2) : [];
    const keys = new Set(tanks.map(([, k]) => k));
    for (const m of sq.men) if (m.esc !== undefined && (!keys.has(m.esc) || !this.stands(sq, m))) m.esc = undefined;
    for (const [k, list] of this.escorts) this.escorts.set(k, list.filter((m) => m.esc === k && m.fell === undefined));
    if (!tanks.length) return;
    const free = (m: Man): boolean => this.stands(sq, m) && m.esc === undefined && !m.pr && !m.aid && !m.scout && m.role === 'rifle' && !CARRIED.has(m.pose);
    for (const [v, k] of tanks) {
      const list = this.escorts.get(k) ?? [];
      if (list.length < E.men) {
        const [bx, by] = fwd(v.face), back: [number, number] = [v.x - bx * E.gap, v.y - by * E.gap];
        const cand = sq.men.filter((m) => free(m) && Math.hypot(m.x - back[0], m.y - back[1]) < E.reach)
          .sort((a, c) => Math.hypot(a.x - back[0], a.y - back[1]) - Math.hypot(c.x - back[0], c.y - back[1]) || a.n - c.n);
        for (const m of cand.slice(0, E.men - list.length)) { m.esc = k; list.push(m); }
        this.escorts.set(k, list);
      }
      const [fx, fy] = fwd(v.face), [rx, ry] = right(v.face), moving = (v.went ?? 0) > E.moving;
      list.forEach((m, i) => {
        if (m.esc !== k || !sq.men.includes(m)) return;
        const back = E.gap + E.file * Math.floor(i / 2), side = (i % 2 ? 1 : -1) * E.side;
        const p: [number, number] = [v.x - fx * back + rx * side, v.y - fy * back + ry * side];
        m.spot = p; m.slotAt = p; m.cover = 'wreck'; m.aim = undefined;
        if (moving || Math.hypot(m.x - p[0], m.y - p[1]) > 1.5) {
          m.pose = 'advance'; m.v = clamp((v.went ?? 0) + 0.8, E.pace[0], E.pace[1]); m.until = t + 1;
        }
      });
    }
  }

  /**
   * Every man on his feet gets out of a vehicle's way: a man of her own side in front of a tank or a lorry on the move
   * (within DRILL.escort.clear.ahead of her nose and a little to either side of her tracks) steps out to the side he is
   * nearer, at a run, and his place is moved off her way; an enemy only once she is almost on him (clear.enemyAhead), so
   * she can catch one the rules take then (Drill.thin: a tank crushes him). Fallen and wounded men are not moved.
   */
  private clearOfVehicles(): void {
    const C = DR.escort.clear, t = this.b.t;
    const movers: { v: Man; side: Side; fx: number; fy: number; rx: number; ry: number }[] = [];
    for (const vq of this.squads.values()) {
      if (!live(vq.u)) continue;
      for (const v of vq.men) {
        if ((v.role !== 'tank' && v.role !== 'lorry') || v.fell !== undefined || (v.went ?? 0) <= DR.escort.moving) continue;
        const f = v.pose === 'reverse' ? v.face + Math.PI : v.face;
        const [fx, fy] = fwd(f), [rx, ry] = right(f);
        movers.push({ v, side: vq.u.side, fx, fy, rx, ry });
      }
    }
    if (!movers.length) return;
    for (const sq of this.squads.values()) {
      const u = sq.u;
      if (!live(u) || u.kind === 'arm' || u.kind === 'art') continue;
      const near = movers.filter((o) => Math.hypot(o.v.x - u.x, o.v.y - u.y) < C.within + Math.max(u.w, u.d) / 2);
      if (!near.length) continue;
      for (const m of sq.men) {
        if (!this.stands(sq, m) || VEHICLE.has(m.role) || CARRIED.has(m.pose) || m.pose === 'wounded') continue;
        if (m.dodged !== undefined && t - m.dodged < C.dodge) continue;
        for (const o of near) {
          const dx = m.x - o.v.x, dy = m.y - o.v.y;
          const a = dx * o.fx + dy * o.fy, l = dx * o.rx + dy * o.ry;
          // (an enemy only when she is almost on him, and not while he lies pinned: her crew may not see him in time)
          if (o.side !== u.side && LYING.has(m.pose)) continue;
          const ahead = o.side === u.side ? C.ahead : C.enemyAhead;
          if (a < -C.behind || a > ahead || Math.abs(l) >= C.half) continue;
          const s = l >= 0 ? 1 : -1, go = C.out - Math.abs(l);
          const out: [number, number] = [m.x + o.rx * s * go, m.y + o.ry * s * go];
          // (his place off her way too, where it lies in it: on the side he steps to)
          const sa = (m.spot[0] - o.v.x) * o.fx + (m.spot[1] - o.v.y) * o.fy, sl = (m.spot[0] - o.v.x) * o.rx + (m.spot[1] - o.v.y) * o.ry;
          m.spot = sa > -C.behind && sa < 30 && Math.abs(sl) < C.out ? [m.spot[0] + o.rx * (s * C.out - sl), m.spot[1] + o.ry * (s * C.out - sl)] : m.spot;
          m.slotAt = m.spot;
          m.pose = 'run'; m.v = Math.max(m.v, 4.5); m.until = t + C.dodge; m.aim = undefined; m.dodged = t;
          m.spot = Math.hypot(m.spot[0] - m.x, m.spot[1] - m.y) < 0.5 ? out : m.spot;
          if (m.esc === undefined) m.spot = out;
          break;
        }
      }
    }
  }

  /**
   * A man the rules take from sq lying in the path of an enemy tank on the move, close under her nose (Drill.thin): her,
   * him and her block. Any enemy tank near the block will do: which enemy took the man is the drill's to choose.
   */
  private crushOf(sq: Squad, cand: Man[]): [Man, Man, LandUnit] | undefined {
    const C = DR.escort.clear, u = sq.u;
    for (const bq of this.squads.values()) {
      if (bq.u.side === u.side || bq.u.kind !== 'arm' || !live(bq.u)) continue;
      if (Math.hypot(bq.u.x - u.x, bq.u.y - u.y) > C.within + (Math.max(u.w, u.d) + Math.max(bq.u.w, bq.u.d)) / 2) continue;
      for (const v of bq.men) {
        if (v.role !== 'tank' || !this.stands(bq, v) || (v.went ?? 0) <= DR.escort.moving) continue;
        const f = v.pose === 'reverse' ? v.face + Math.PI : v.face;
        const [fx, fy] = fwd(f), [rx, ry] = right(f);
        for (const m of cand) {
          if (!this.stands(sq, m)) continue;
          const dx = m.x - v.x, dy = m.y - v.y, a = dx * fx + dy * fy, l = dx * rx + dy * ry;
          if (a > -1 && a < C.crush && Math.abs(l) < C.crushHalf) return [v, m, bq.u];
        }
      }
    }
    return undefined;
  }

  /** How much of him a burst finds (shell blasts): a man lying down DR.blast.lying, kneeling DR.blast.kneeling, else 1. */
  private exposed(m: Man): number {
    if (LYING.has(m.pose)) return DR.blast.lying;
    if (m.pose === 'fire_kneel' || m.pose === 'wait' || m.pose === 'creep') return DR.blast.kneeling;
    return 1;
  }

  /** Who struck a block: the enemy block owed most for its harm (sim.ts suffer), else the one it faces. */
  private strikerOf(u: LandUnit): LandUnit | undefined {
    const b = this.b, owed = u.owed ?? {};
    let by: LandUnit | undefined;
    for (const id of Object.keys(owed).map(Number)) { const e = b.unit(id); if (e && live(e) && e.side !== u.side && (!by || owed[id] > owed[by.id])) by = e; }
    return by ?? this.threat(u) ?? undefined;
  }

  private thin(sq: Squad): void {
    const u = sq.u, b = this.b;
    if (u.state === 'withdrawn' || (u.state === 'gone' && (u.figures > 0 || u.y < 0 || u.y > b.field.h))) return;
    const want = u.str <= 0 ? 0 : Math.ceil((sq.m0 * u.str) / Math.max(1e-9, u.str0) - 1e-9);
    let up = 0;
    for (const m of sq.men) if (this.stands(sq, m) || (m.j !== undefined && m.fell === undefined)) up++;
    let n = up - want;
    // Stretcher bearers (release 2): its strength back above its men on their feet, its light cases dressed get up.
    if (n < 0 && this.b.bearers) { this.rise(sq, -n); return; }
    if (n <= 0) return;
    // (Bearers on a carry go on with it: never the next to fall.)
    const cand = sq.men.filter((m) => m.j !== undefined && m.fell === undefined && !m.job);
    if (!cand.length) return;
    // Who struck: the enemy block owed most for the harm, else the one it faces.
    const by = this.strikerOf(u);
    const shell = !!by && ((by.kind === 'arm' && !this.mgKill(by, u, sq.lost)) || (by.kind === 'art' && by.team !== 'mg'));
    // Armoured infantry aboard an armoured carrier (DR.mech.armoured; the design director, 23 November 07:06Z(2): struck
    // through her armour, which a bullet does not pierce): small arms take none of them. They fall to a shell, or once
    // they are down; till then the block's count of men waits for them (it is never changed).
    if (sq.of !== undefined && !shell) {
      for (let i = cand.length - 1; i >= 0; i--) if (cand[i].pose === 'mounted' && DR.mech.armoured.includes(this.lorryOf(sq, cand[i]).ml ?? '')) cand.splice(i, 1);
      if (!cand.length) return;
    }
    const from: [number, number] = by ? [by.x, by.y] : [u.x, u.y];
    let burst: [number, number] | undefined;
    // Shell blasts (release 2; Brian, 23 November 05:04Z: "explosions on the ground should be able to take out multiple
    // people if they are close to it"; LandBattle.blast, off unless turned on): the men a block loses to shells are taken
    // together, as one burst takes them. Its losses to a gun or a tank's high explosive wait for a burst (DR.blast.hold
    // seconds at most, or until as many are due as one shell of its class takes), and a 'shell' event that took one of
    // its key men a moment ago is that burst (Drill.shelled). The men nearest the burst fall to it, within its reach
    // (shells.ts REACH, by the shell's class), a man lying down or in cover taken less often; beyond it, another shell.
    // The rules' count of the block's men is never changed: they fall up to DR.blast.hold seconds later, and together.
    // Infantry with tanks (release 2, LandBattle.escort): a man of the block in the path of an enemy tank on the move,
    // close under her nose, when the rules take one from it, is the one taken: by her tracks (a 'track' round, no shot).
    if (b.escort) {
      const crushed = this.crushOf(sq, cand);
      if (crushed) {
        const [v, m, tu] = crushed;
        cand.splice(cand.indexOf(m), 1);
        sq.lost++; n--;
        m.fell = b.t; m.v = 0; m.aim = undefined; m.hidden = undefined;
        this.killRound(tu, sq, m, undefined, false, v);
        m.pose = 'down'; m.spot = [m.x, m.y];
        this.aid(sq, m);
        if (n <= 0 || !cand.length) return;
      }
    }
    const blast = shell && b.blast && !!by;
    const cal: Calibre = by?.cal ?? 'field';
    if (blast) {
      const a = sq.anchor && b.t - sq.anchor.t <= DR.blast.anchor ? sq.anchor : undefined;
      if (a) burst = [a.x, a.y];
      else if (fighting(u) && !b.over && n < DR.blast.most[cal] && b.t - (sq.held ??= b.t) < DR.blast.hold) return;
      sq.held = undefined;
    }
    const reach = blast ? REACH[cal] : 12;
    while (n-- > 0 && cand.length) {
      let best = 0, score = Infinity;
      cand.forEach((m, i) => {
        const p = burst ?? from;
        const sc = (Math.hypot(m.x - p[0], m.y - p[1]) * (2 - (DR.rounds.cover[m.cover] ?? 1))) / (blast ? this.exposed(m) : 1) + (burst ? 6 : 40) * roll(this.seed, u.id, m.n, sq.lost, 81);
        if (sc < score) { score = sc; best = i; }
      });
      const m = cand.splice(best, 1)[0];
      sq.lost++;
      // A shell's burst: the first man's place, and the men near it after him (beyond 12 m of it, another shell; with
      // shell blasts on, beyond its class's reach). With them on, a man the burst before took too is its mate (m: 1).
      let mate = false;
      if (shell && (!burst || Math.hypot(m.x - burst[0], m.y - burst[1]) > reach)) { burst = [m.x, m.y]; this.duck(m.x, m.y); }
      else if (blast) mate = true;
      m.fell = b.t; m.v = 0; m.aim = undefined; m.hidden = undefined;
      if (by) this.killRound(by, sq, m, shell ? burst : undefined, mate);
      m.pose = 'down';
      m.spot = [m.x, m.y];
      if (b.bearers) this.hurt(sq, m);
      this.aid(sq, m);
    }
    sq.memo = undefined;
  }

  // ---- Stretcher bearers (release 2) ------------------------------------------------------------------------------
  //
  // Brian, 22 November 17:33Z; the design director's rulings Q4 and Q5 (18:36Z); LandBattle.bearers and LAND.bearers.
  // The rules count the bearers and the wounded (LandBattle.litter); the drill draws them, looks only. Of the men who
  // fall, the rules' share (1 - killed) lie wounded: a man of a section a light case at the rules' share, lying for his
  // dressing and up again once it is done and his block's strength has them back (thin, rise); the rest lie for the
  // bearers. The bearers are riflemen of the block (br), two to a stretcher, as many stretchers as the rules' bearers
  // carry cases at once (its army's share of its men over LAND.bearers.litter). A pair fetches the litter case fallen
  // earliest, lifts him and carries him head first to its aid post (LandBattle.aidOf) at LAND.bearers.pace, his place the
  // stretcher's middle, the front bearer 1.12 m ahead of him and the rear 1.35 m behind; he stays there (gone), and they
  // walk home. A block running, fled or gone sends no stretcher out; one on its way goes on. Every choice is the drill's
  // own dice: the men drawn and the fight are the same as without them.

  /** A man who falls, with the bearers on: wounded at the rules' share (else killed), a man of a section a light case at theirs. */
  private hurt(sq: Squad, m: Man): void {
    if (VEHICLE.has(m.role) || CREWED.has(m.role) || m.wd !== undefined) return;
    const B = LAND.bearers, u = sq.u;
    if (roll(this.seed, u.id, m.n, 71) >= 1 - killedShare(u.nat)) return;
    m.wd = this.b.t; m.pose = 'wounded';
    if (m.j !== undefined && roll(this.seed, u.id, m.n, 72) < B.light.share) { m.light = true; (sq.dressing ??= []).push(m); }
    else (sq.lying ??= []).push(m);
  }

  /** Its light cases whose dressing is done get up, up to n of them, the earliest hurt first, while its block fights. */
  private rise(sq: Squad, n: number): void {
    const u = sq.u, t = this.b.t, L = LAND.bearers.light, foot = LAND.speed.inf;
    if (!fighting(u) || !sq.dressing?.length) return;
    const walk = L.at > 0 ? L.at / (foot * L.out) + L.at / (foot * L.back) : 0;
    while (n > 0 && sq.dressing.length && sq.dressing[0].wd! + walk + L.dress <= t + 1e-9) {
      const m = sq.dressing.shift()!;
      m.light = undefined; m.fell = undefined; m.back = t;
      m.pose = 'wait'; m.v = 0; m.spot = [m.x, m.y]; m.slotAt = [m.x, m.y]; m.until = t + 1;
      // His section's key man fallen meanwhile, he joins the nearest section still on its feet (as regroup).
      if (!this.memo(sq).keyOf.has(m.sec ?? m.k)) {
        const to = this.nearest(this.memo(sq).keys.filter((x) => !VEHICLE.has(x.role) && !CREWED.has(x.role)), m)?.[0];
        if (to) { m.sec = to.k; m.sub = undefined; m.lead = undefined; }
      }
      sq.memo = undefined;
      n--;
    }
  }

  /** A bearer named in a carry: his model and his place in it. */
  private byId(m: Man): number { return m.k * 1000 + (m.j ?? 0); }

  /**
   * Each step with the bearers on: its bearers picked (the first time), each pair free of a carry and on its feet sent
   * for the next litter case (its block fighting, its men down), and every carry moved on a step.
   */
  private litters(sq: Squad): void {
    const u = sq.u, b = this.b, t = b.t, B = LAND.bearers, f = b.field;
    if (!sq.brs) {
      const cand = sq.men.filter((m) => m.j !== undefined && m.role === 'rifle' && !m.pr && !m.scout && !m.lead && m.fell === undefined)
        .sort((a, c) => roll(this.seed, u.id, a.n, 73) - roll(this.seed, u.id, c.n, 73) || a.n - c.n);
      const pairs = Math.min(Math.floor(cand.length / 2), Math.max(1, Math.round((bearerShare(u.nat) * sq.m0) / B.litter)));
      sq.brs = cand.slice(0, 2 * pairs);
      for (const m of sq.brs) m.br = true;
    }
    const carries = (sq.carries ??= []);
    // Free pairs out to the earliest fallen: by their places in the block's list, two by two.
    if (sq.lying?.length && fighting(u) && sq.down && sq.boarding === undefined) {
      const free = sq.brs.filter((m) => !m.job && this.stands(sq, m) && !CARRIED.has(m.pose));
      for (let i = 0; i + 1 < free.length && sq.lying.length; i += 2) {
        const man = sq.lying.shift()!, [ax, ay] = b.aidOf(u);
        const to: [number, number] = [clamp(ax, 3, f.w - 3), clamp(ay, 3, f.h - 3)], way = bearing(man.x, man.y, to[0], to[1]);
        const c: Carry = { man, front: free[i], rear: free[i + 1], stage: 'fetch', from: [man.x, man.y], to, way, t0: t, t1: t };
        for (const m of [c.front, c.rear]) { m.job = c; m.aim = undefined; m.hidden = undefined; m.pose = 'walk'; m.v = B.pace; }
        carries.push(c);
        sq.memo = undefined;
      }
    }
    for (const c of carries) this.carryStep(sq, c);
    // A carry is done once both its bearers are home.
    if (carries.some((c) => c.front.job !== c && c.rear.job !== c)) sq.carries = carries.filter((c) => c.front.job === c || c.rear.job === c);
  }

  /** A carry's step: the bearers walk to the man, lift him, carry him to the aid post and walk home. */
  private carryStep(sq: Squad, c: Carry): void {
    const u = sq.u, t = this.b.t, B = LAND.bearers, rate = TURN.foot * DT, [wx, wy] = fwd(c.way);
    const FRONT = 1.12, REAR = 1.35;
    // One step toward p at pace v, turning as he can (he turns before he goes): whether he is there.
    const stride = (m: Man, p: [number, number], v: number): boolean => {
      const ddx = p[0] - m.x, ddy = p[1] - m.y, d = Math.hypot(ddx, ddy);
      if (d < 0.05) { m.went = 0; return true; }
      const want = Math.atan2(ddx, ddy);
      m.face = turnTo(m.face, want, rate);
      const s = Math.min(d, v * DT) * Math.max(0, Math.cos(wrap(want - m.face)));
      m.x += (ddx / d) * s; m.y += (ddy / d) * s; m.went = s / DT;
      return s >= d - 1e-9;
    };
    const bearers = [c.front, c.rear];
    if (c.stage === 'fetch') {
      const at = (m: Man): [number, number] => (m === c.front ? [c.man.x + wx * FRONT, c.man.y + wy * FRONT] : [c.man.x - wx * REAR, c.man.y - wy * REAR]);
      let there = 0;
      for (const m of bearers) if (stride(m, at(m), B.pace)) there++;
      if (there === 2) { c.stage = 'lift'; for (const m of bearers) { m.pose = 'wait'; m.v = 0; m.went = 0; } }
      return;
    }
    if (c.stage === 'lift') {
      // At his side, all three turned to the way (he is lifted head first, his head toward the front bearer).
      for (const m of [...bearers, c.man]) m.face = turnTo(m.face, c.way, rate);
      if (![...bearers, c.man].every((m) => Math.abs(wrap(m.face - c.way)) < 1e-6)) return;
      c.stage = 'carry'; c.t0 = t;
      c.t1 = Math.round((t + Math.hypot(c.to[0] - c.man.x, c.to[1] - c.man.y) / B.pace) * 1000) / 1000;
      c.from = [c.man.x, c.man.y];
      (c.man.cy ??= []).push([c.t0, c.t1, this.byId(c.front), this.byId(c.rear)]);
      c.man.pose = 'carried'; c.front.pose = 'stretcher_front'; c.rear.pose = 'stretcher_rear';
      for (const m of bearers) m.v = B.pace;
      return;
    }
    if (c.stage === 'carry') {
      const left = Math.hypot(c.to[0] - c.man.x, c.to[1] - c.man.y), s = Math.min(left, B.pace * DT);
      c.man.x += wx * s; c.man.y += wy * s; c.man.spot = [c.man.x, c.man.y];
      c.front.x = c.man.x + wx * FRONT; c.front.y = c.man.y + wy * FRONT;
      c.rear.x = c.man.x - wx * REAR; c.rear.y = c.man.y - wy * REAR;
      for (const m of bearers) { m.face = c.way; m.went = s / DT; }
      if (s < left - 1e-9) return;
      // At the aid post: he is set down there, and the bearers walk home.
      c.man.gone = t; c.man.pose = 'wounded';
      c.stage = 'home';
      for (const m of bearers) { m.pose = 'walk'; m.v = B.pace; }
      return;
    }
    // Home: back into the block's ground, where its drill takes them up again. A block gone off the field takes them with it.
    for (const m of bearers) {
      if (m.job !== c) continue;
      const p = this.leash(sq, [m.x, m.y], -4);
      if (!live(u) || stride(m, p, B.pace)) {
        m.job = undefined; m.pose = 'wait'; m.v = 0; m.went = 0; m.spot = [m.x, m.y]; m.slotAt = [m.x, m.y]; m.until = t + 1;
        sq.memo = undefined;
      }
    }
  }

  // ---- What the block is doing ------------------------------------------------------------------------------------

  private phaseOf(sq: Squad): Phase {
    const b = this.b, u = sq.u;
    if (!live(u)) return 'gone';
    if (u.state === 'routing') return 'flee';
    if (u.order?.kind === 'withdraw' || u.gives || u.fellBack) return 'fallback';
    if (sq.ran && u.state === 'wavering') return 'rally';
    const moving = sq.speed > 0.6 && !!u.order;
    const near = this.nearestGap(u);
    if (u.kind === 'art') return moving ? 'limbered' : 'battery';
    const hot = sq.harm > DR.pin.harm * this.pinScale(u) || b.t - sq.shelledAt < DR.pin.shelled;
    if (u.state === 'wavering' && (u.engaged !== undefined || hot)) return 'pinned';
    // The drill book: a steady block under fire fights on, each group's heads down by the fire landing near it.
    if (hot && !moving && u.kind !== 'arm') return this.byBook && u.engaged !== undefined && u.state === 'steady' ? 'fight' : 'pinned';
    if (u.engaged !== undefined) return moving ? 'bound' : 'fight';
    if (moving) {
      if (near < DR.ranges.bound || sq.harm > 0.0005) return 'bound';
      return near < DR.ranges.column ? 'deploy' : 'column';
    }
    // The main effort lying at its start line under its guns (ai.ts: the plan's 'guns' phase).
    const P = b.plan[u.side];
    if (P && P.phase === 'guns' && P.role[u.id] === 'main' && !u.taken && near < DR.ranges.column) return 'startline';
    if (u.reserve === 'held' && near > 250) return 'rest';
    return near < DR.ranges.column || u.side === 'def' ? 'hold' : 'rest';
  }

  /** How readily its men go to ground: raw troops sooner, veterans later. */
  private pinScale(u: LandUnit): number { return 1 / DR.xp.pinned[this.band(u)]; }
  private band(u: LandUnit): 0 | 1 | 2 { return u.xp < DR.xp.raw ? 0 : u.xp >= DR.xp.veteran ? 2 : 1; }
  // ---- Experience you can see (Drill.seasoned): its helpers follow this line ----
  private nums(u: LandUnit): Nums {
    const s = this.b.style[u.side];
    return DR.style[s && this.b.ai[u.side] ? s.id : 'plain'];
  }

  /** Metres from this block's edge to the nearest enemy that is still fighting. */
  private nearestGap(u: LandUnit): number {
    let g = Infinity;
    for (const e of this.b.units) if (e.side !== u.side && fighting(e) && !e.hid) g = Math.min(g, this.b.gap(u, e));
    return g;
  }

  /** What the block faces: who it fights, else its order's target, else the nearest enemy, else nothing. */
  private threat(u: LandUnit): LandUnit | null {
    const b = this.b;
    if (u.engaged !== undefined) { const e = b.unit(u.engaged); if (e && live(e)) return e; }
    if (u.order?.target !== undefined) { const e = b.unit(u.order.target); if (e && live(e)) return e; }
    let best: LandUnit | null = null, d = 1500;
    for (const e of b.units) if (e.side !== u.side && fighting(e) && !e.hid) { const g = b.gap(u, e); if (g < d) { d = g; best = e; } }
    return best;
  }
  private threatDir(sq: Squad): [number, number] {
    const u = sq.u, e = this.threat(u);
    if (!e) return fwd(u.face);
    const d = Math.hypot(e.x - u.x, e.y - u.y) || 1;
    return [(e.x - u.x) / d, (e.y - u.y) / d];
  }

  // ---- The drill book (tactics.ts) -----------------------------------------------------------------------------------

  /** Its groups' middles now; then the rounds fired at it since its last think are felt (suppression). */
  private feel(sq: Squad): void {
    const u = sq.u, mids = new Map<number, [number, number]>(), n = new Map<number, number>();
    if (live(u)) for (const m of this.memo(sq).stand) {
      if (VEHICLE.has(m.role)) continue;
      const g = grp(m), c = mids.get(g);
      if (c) { c[0] += m.x; c[1] += m.y; } else mids.set(g, [m.x, m.y]);
      n.set(g, (n.get(g) ?? 0) + 1);
    }
    for (const [g, c] of mids) { const k = n.get(g) ?? 1; c[0] /= k; c[1] /= k; }
    sq.mids = mids;
    this.sup.feel(u.id, this.b.t, this.band(u), this.shots, mids);
  }

  /** A man's group's suppression now (none with the book off). */
  private supOf(sq: Squad, m: Man): number { return this.byBook ? this.sup.level(sq.u.id, grp(m), this.b.t) : 0; }

  /** Whether his group's heads are down (its suppression over BOOK.suppress.down). */
  private downed(sq: Squad, m: Man): boolean { return this.byBook && this.supOf(sq, m) > BOOK.suppress.down; }

  /** How a block bounds: by its army's school at its experience, or all together where its general's style is a wave. */
  private boundFor(sq: Squad, N: Nums): Bound { return boundOf(sq.u.nat, this.band(sq.u), N.wave); }

  /**
   * A block's fire-and-movement cycle by its bound: how long a man is up in a rush, his turn's slot (his rush and the
   * pause before the next turn's), and the whole cycle (its turns). A wave keeps its style's rush and halt.
   */
  private cycle(sq: Squad, N: Nums, bound: Bound): { rush: number; slot: number; cyc: number } {
    if (bound === 'wave') { const cyc = N.rush + N.halt; return { rush: N.rush, slot: cyc, cyc }; }
    const rush = BOOK.rush[this.band(sq.u)], slot = rush + BOOK.gap;
    return { rush, slot, cyc: turns(bound) * slot };
  }

  /**
   * His turn in the cycle (tactics.ts turnOf): by his squad in his platoon (its headquarters with the leading squad), or
   * by his section where the men are drawn as sections.
   */
  private turnFor(sq: Squad, m: Man, bound: Bound): number {
    const key = this.memo(sq).keyOf.get(m.sec ?? m.k);
    const plat = key ? !!key.plat : sq.tier === 2 && m.role === 'rifle';
    const squads = plat ? platoonOf(sq.u.nat).squads : 0, s = plat ? m.sub ?? 0 : m.sec ?? m.k;
    return turnOf(bound, m.role === 'mg', s, plat && s >= squads);
  }

  /** A block's most dangerous men to foot coming at it, standing now: its machine guns, anti-tank teams, guns and mortars. */
  private priority(eq: Squad): Man[] {
    const t = this.b.t, c = this.priMemo.get(eq.u.id);
    if (c && c.t === t) return c.men;
    const men = live(eq.u) ? this.memo(eq).stand.filter((x) => x.role === 'mg' || x.role === 'hmg' || x.role === 'at' || x.role === 'gun' || x.role === 'mortar') : [];
    this.priMemo.set(eq.u.id, { t, men });
    return men;
  }

  /** Where a side's tanks stand this step. */
  private tanksOf(side: Side): [number, number][] {
    const t = this.b.t;
    if (this.tankMemo.t !== t) this.tankMemo = { t, at: {} };
    let at = this.tankMemo.at[side];
    if (!at) {
      at = [];
      for (const sq of this.squads.values()) if (sq.u.side === side && sq.u.kind === 'arm' && live(sq.u)) for (const m of this.memo(sq).stand) if (m.role === 'tank') at.push([m.x, m.y]);
      this.tankMemo.at[side] = at;
    }
    return at;
  }

  /** The place of the man of 'of' nearest p (within a reach). */
  private nearestOf(of: Man[], p: [number, number], within = Infinity): [number, number] | undefined {
    let best: Man | undefined, d = within;
    for (const o of of) { const dd = Math.hypot(o.x - p[0], o.y - p[1]); if (dd < d) { d = dd; best = o; } }
    return best ? [best.x, best.y] : undefined;
  }

  /**
   * Where a man covering his mates aims (the drill book): three times in five at the enemy's weapons that would cut them
   * down as they rush (its machine guns, anti-tank teams, guns), the nearest of them; near friendly tanks, at its tank
   * hunters first; else at the nearest man of the block he fights. 'away': his fire lifted off the enemy's near edge,
   * where his own assault is closing, onto the man nearest that point.
   */
  private aimFor(sq: Squad, m: Man, e: LandUnit | null, away?: [number, number]): [number, number] | undefined {
    if (!this.byBook || !e) return this.aimAt(m, e);
    const eq = this.squads.get(e.id), t = this.b.t;
    if (!eq) return this.aimAt(m, e);
    if (away) return this.nearestOf(this.memo(eq).stand, away) ?? this.aimAt(m, e);
    const pri = this.priority(eq);
    if (pri.length && roll(this.seed, sq.u.id, m.n, Math.floor(t / 6), 91) < 0.6) {
      const near = this.tanksOf(sq.u.side).some(([x, y]) => Math.abs(x - m.x) < 120 && Math.abs(y - m.y) < 120);
      const at = near ? pri.filter((x) => x.role === 'at') : [];
      const p = this.nearestOf(at.length ? at : pri, [m.x, m.y], 700);
      if (p) return p;
    }
    return this.aimAt(m, e);
  }

  /**
   * Where a tank aims (the drill book): where her own foot is fighting the foot block she fires on, at what pins them (its
   * machine guns, anti-tank teams and guns: the targets the infantry point out to her); else the nearest of it.
   */
  private tankAim(sq: Squad, m: Man, e: LandUnit | null): [number, number] | undefined {
    if (!this.byBook || !e || e.kind === 'arm') return this.aimAt(m, e);
    const b = this.b, eq = this.squads.get(e.id);
    if (!eq || !b.units.some((f) => f.side === sq.u.side && fighting(f) && FOOT_KINDS.includes(f.kind) && b.gap(f, e) < 300)) return this.aimAt(m, e);
    return this.nearestOf(this.priority(eq), [m.x, m.y], 1000) ?? this.aimAt(m, e);
  }

  /**
   * Where a gun, a mortar or a support team's machine gun aims at block e: anywhere in it, until friendly foot comes
   * within BOOK.lift of it (the drill book's lift and shift); then the guns and mortars lift into its depth, beyond its far
   * side, and the machine guns shift to its far half, clear of their own men.
   */
  private liftAim(sq: Squad, g: Man, e: LandUnit, kind: 'gun' | 'mortar' | 'mg'): [number, number] {
    const b = this.b, u = sq.u, t = b.t;
    const r1 = roll(this.seed, u.id, g.n, t), r2 = roll(this.seed, u.id, g.n, t, 1);
    const plain: [number, number] = [e.x + (r1 - 0.5) * e.w * 0.6, e.y + (r2 - 0.5) * e.d * 0.6];
    if (!this.byBook) return plain;
    let near: LandUnit | null = null, gap = BOOK.lift[kind];
    for (const f of b.units) if (f.side === u.side && fighting(f) && FOOT_KINDS.includes(f.kind)) { const d = b.gap(f, e); if (d < gap) { gap = d; near = f; } }
    if (!near) return plain;
    const dx = e.x - near.x, dy = e.y - near.y, dd = Math.hypot(dx, dy) || 1, ax = dx / dd, ay = dy / dd;
    const deep = kind === 'mg' ? e.d * 0.3 : Math.max(e.w, e.d) / 2 + BOOK.lift.deep, across = (r1 - 0.5) * e.w * 0.6;
    return [clamp(e.x + ax * deep - ay * across, 1, b.field.w - 1), clamp(e.y + ay * deep + ax * across, 1, b.field.h - 1)];
  }

  /**
   * The assault (the drill book): the whole squads of the platoons nearest the flank it works onto, their riflemen (and
   * their guns, where the school takes them in), up to nA men.
   */
  private flankPlatoons(sq: Squad, men: Man[], nA: number, gunStays: boolean): Set<Man> {
    const keys = this.memo(sq).keys.filter((k) => k.role === 'rifle'), across = new Map<number, number>();
    for (const k of keys) across.set(k.k, this.slot(sq, k, 'line')[0] * sq.flank);
    keys.sort((a, c) => (across.get(c.k) ?? 0) - (across.get(a.k) ?? 0) || a.k - c.k);
    const out = new Set<Man>();
    for (const k of keys) {
      if (out.size >= nA) break;
      for (const m of men) if ((m.sec ?? m.k) === k.k && (m.role === 'rifle' || m.role === 'sapper' || (!gunStays && m.role === 'mg'))) out.add(m);
    }
    return out;
  }

  /** A defence's counter-attack (the drill book): the riflemen of its platoons in the second row. */
  private rearPlatoons(sq: Squad, men: Man[]): Set<Man> {
    const keys = this.memo(sq).keys.filter((k) => k.role === 'rifle'), out = new Set<Man>();
    if (!keys.length) return out;
    const ls = keys.map((k) => this.slot(sq, k, 'line')[1]), top = Math.max(...ls);
    const rear = new Set(keys.filter((_, i) => ls[i] < top - 3).map((k) => k.k));
    for (const m of men) if (rear.has(m.sec ?? m.k) && (m.role === 'rifle' || m.role === 'sapper')) out.add(m);
    return out;
  }


  // ---- Squad life (DR.life) -------------------------------------------------------------------------------------------
  // Brian, 25 November 2026, 13:40Z: "i dont want all the soldiers of a squad doing the exact same thing at the same
  // time. I want them to work together and look real as individuals apart of a squad". A squad still does one thing
  // together; each man does his own part of it in his own time. All of it the drill's own dice (looks only).

  /**
   * His buddy team in his group (a section, or a platoon's squad): [its number, the group's teams, his place among the
   * group's men]. Its leader with the rifleman at his elbow; its machine gun (its number twos keep by it: pairs); the
   * rest two by two along each arm of the wedge, neighbours together, an odd man with the pair beside him. Worked out
   * with the block's men on their feet (memo), so a team that loses a man closes up with the next.
   */
  private teamOf(sq: Squad, m: Man): [number, number, number] {
    const M = this.memo(sq);
    if (!M.teams) {
      M.teams = new Map();
      const groups = new Map<number, Man[]>();
      for (const x of M.up) {
        if (VEHICLE.has(x.role) || CREWED.has(x.role) || x.role === 'officer' || x.role === 'holder') continue;
        const g = grp(x);
        let l = groups.get(g);
        if (!l) groups.set(g, (l = []));
        l.push(x);
      }
      for (const men of groups.values()) {
        // (its leader first: the key man of his own squad, a platoon squad's own leader; the rest in their order)
        const head = (x: Man): number => (x.j === undefined || x.lead ? 0 : 1);
        men.sort((a, c) => head(a) - head(c) || (a.j ?? 0) - (c.j ?? 0));
        const guns = men.filter((x) => x.role === 'mg'), rest = men.filter((x) => x.role !== 'mg');
        const teams: Man[][] = [];
        if (rest.length) teams.push(rest.slice(0, 2));
        // (inSection: after the leader the riflemen stand alternately on either arm, so every second man is a neighbour)
        const arms: [Man[], Man[]] = [[], []];
        rest.slice(2).forEach((x, i) => arms[i % 2].push(x));
        const pairs: [Man[][], Man[][]] = [[], []];
        arms.forEach((arm, a) => {
          for (let i = 0; i < arm.length; i += 2) pairs[a].push(arm.slice(i, i + 2));
          const last = pairs[a][pairs[a].length - 1];
          if (last && last.length === 1 && pairs[a].length > 1) { pairs[a].pop(); pairs[a][pairs[a].length - 1].push(last[0]); }
        });
        for (const g of guns) teams.push([g]);
        // (the arms' pairs turn about: one from the left, one from the right)
        for (let i = 0; i < Math.max(pairs[0].length, pairs[1].length); i++) for (const a of [0, 1]) if (pairs[a][i]) teams.push(pairs[a][i]);
        teams.forEach((team, ti) => { for (const x of team) M.teams!.set(x, [ti, teams.length, men.indexOf(x)]); });
      }
    }
    return M.teams.get(m) ?? [0, 1, 0];
  }

  /**
   * Squad life: when in his squad's turn of cycle n his own buddy team rises (seconds after the turn begins) and how
   * long his own rush lasts. The squad's teams go one after another over DR.life.rush.spread turns, each cycle in a
   * fresh order (where a squad moves as one body round its gun, the gun still goes first), so some of a squad are always
   * down firing while its others run. A wave's teams rise within a second or so: it is a wave.
   */
  private rushOf(sq: Squad, m: Man, n: number, bk: { rush: number; slot: number; cyc: number }, bound: Bound): [number, number] {
    const L = DR.life.rush, u = sq.u, [ti, nT] = this.teamOf(sq, m), wave = bound === 'wave';
    const lo = wave ? 0.7 : L.len[0], hi = wave ? 1 : L.len[1];
    const len = bk.rush * (lo + (hi - lo) * roll(this.seed, u.id, m.n, n, 103));
    const W = Math.max(0, Math.min(wave ? L.wave : L.spread * bk.slot, bk.cyc - bk.rush * hi - 0.35));
    const ord = nT <= 1 || (bound === 'body' && m.role === 'mg') ? 0 : (ti + Math.floor(roll(this.seed, u.id, grp(m), n, 101) * nT)) % nT;
    return [(ord / Math.max(1, nT)) * W + 0.3 * roll(this.seed, u.id, m.n, n, 102), len];
  }

  /** Squad life: his team's share of the way round his group's teams (0 to 1), and a little of his own. */
  private teamShare(sq: Squad, m: Man): number {
    const [ti, nT] = this.teamOf(sq, m);
    return (ti + 0.5 * roll(this.seed, sq.u.id, m.n, 104)) / Math.max(1, nT);
  }

  /**
   * Squad life: no squad takes up a new order in one moment. When its block's phase changes, each man is given his own
   * moment to take it up (DR.life.react): going to ground as fire comes, each as he sees it (raw men slower); getting up
   * to move, the leader's team first and the rest by teams; taking their firing places as it halts; falling back, the gun
   * team last; a rout, a few first and then the rest; gathering to rally. Leaders act at once. Till his moment a man goes
   * on with what he was at: returned, each such man and what he was at, to be put back once his block's drill has run
   * (think). A man whose moment has just come takes the phase up as on its first think (fresh).
   */
  private lags(sq: Squad, was: Phase): [Man, Partial<Man>][] | null {
    const t = this.b.t, u = sq.u, ph = sq.phase, R = DR.life.react, up = this.memo(sq).up;
    this.freshNow.clear();
    // A block whose phase flickers (creeping forward in a firefight it flips between fight and bound each half second) is
    // given no fresh lags: a change within 'calm' seconds of the last is taken up by all at once, as without squad life,
    // and the moments already set run out. Else every flip would set each man's moment back and hold him for as long
    // as the flicker lasts.
    if (sq.since === t && was !== ph && (sq.lastFor === undefined || sq.lastFor >= R.calm)) {
      const still = (p: Phase): boolean => p === 'hold' || p === 'rest' || p === 'startline' || p === 'fight' || p === 'pinned' || p === 'rally';
      const kind: 'ground' | 'up' | 'settle' | 'back' | 'break' | 'rally' | null = ph === 'flee' ? 'break' : ph === 'fallback' ? 'back' : ph === 'rally' ? 'rally' : ph === 'pinned' ? 'ground'
        : (ph === 'bound' || ph === 'fight') && (was === 'column' || was === 'deploy') ? 'ground'
        : (ph === 'column' || ph === 'deploy' || ph === 'bound') && still(was) ? 'up'
        : still(ph) ? 'settle' : null;
      for (const m of up) {
        m.lagTo = undefined;
        if (!kind || VEHICLE.has(m.role) || CREWED.has(m.role) || m.role === 'holder' || m.role === 'officer') continue;
        const [lo, hi] = R[kind], [ti, nT] = this.teamOf(sq, m), r = roll(this.seed, u.id, m.n, Math.round(t * 2), 105);
        let lag = lo + (hi - lo) * r * (kind === 'ground' && this.band(u) === 0 ? R.raw : 1);
        if (kind === 'up') lag = lo + (hi - lo) * ((ti + 0.6 * r) / Math.max(1, nT));
        else if (kind === 'back') lag = m.role === 'mg' ? hi : lo + (hi - lo) * 0.8 * r;
        // Leaders act at once (a rout has none).
        if (kind !== 'break' && (m.lead || (m.j === undefined && m.role !== 'mg'))) lag = Math.min(lag, 0.2);
        if (lag >= 0.25) m.lagTo = t + lag;
      }
    }
    let held: [Man, Partial<Man>][] | null = null;
    for (const m of up) {
      if (m.lagTo === undefined) continue;
      if (t >= m.lagTo - 1e-6) { m.lagTo = undefined; this.freshNow.add(m); continue; }
      (held ??= []).push([m, { pose: m.pose, v: m.v, spot: m.spot, cover: m.cover, aim: m.aim, until: m.until, slotAt: m.slotAt, el: m.el, rapid: m.rapid, hidden: m.hidden, rushed: m.rushed, shot: m.shot, sentry: m.sentry }]);
    }
    return held;
  }

  /** Whether a man takes his block's phase up this think: its first, or (squad life) his own moment for it just come. */
  private fresh(sq: Squad, m: Man): boolean { return sq.since === this.b.t || this.freshNow.has(m); }

  /**
   * Squad life: the stance a man holds or waits in (DR.life.watch), where the plain drill has a whole line kneeling
   * alike. A machine gun lies at its gun. A leader kneels, his glasses up a share of the time (lying to use them). Two
   * of each squad are its sentries, kneeling and watching, the duty passing on. The rest by what shelters each: most
   * lie in the open, more kneel behind a bank or in a wood, most stand to at a parapet or a window; far from the enemy
   * some stand easy. Each keeps his stance a spell of his own, so no two change together.
   */
  /** Squad life: the men of a group who can stand sentry (on foot, not its leader, not its gun), in their order. */
  private sentryList(sq: Squad, g: number): Man[] {
    const M = this.memo(sq);
    let l = (M.sentries ??= new Map<number, Man[]>()).get(g);
    if (!l) {
      l = M.up.filter((x) => grp(x) === g && !VEHICLE.has(x.role) && !CREWED.has(x.role) && x.role !== 'holder' && x.role !== 'officer' && x.role !== 'mg' && !(x.lead || (x.j === undefined && !!x.hasSec)));
      l.sort((a, c) => (a.j ?? 0) - (c.j ?? 0));
      M.sentries.set(g, l);
    }
    return l;
  }
  private watchPose(sq: Squad, m: Man, near: number, plain: Pose): Pose {
    if (!this.life || VEHICLE.has(m.role) || CREWED.has(m.role) || m.role === 'holder' || m.role === 'officer') return plain;
    m.sentry = undefined;
    if (m.role === 'mg') return 'hide';
    const W = DR.life.watch, u = sq.u, t = this.b.t, g = grp(m);
    if (m.lead || (m.j === undefined && !!m.hasSec)) {
      const win = Math.floor((t + 8 * roll(this.seed, u.id, m.n, 111)) / 8);
      return roll(this.seed, u.id, m.n, win, 112) < W.glasses ? 'observe' : 'wait';
    }
    // The duty passes round the men who can stand it (not the leader, nor the gun), one place on each turn, so every one
    // of them serves and no turn falls on a man who cannot.
    const can = this.sentryList(sq, g), n = can.length, at = can.indexOf(m);
    const turn = W.turn[0] + (W.turn[1] - W.turn[0]) * roll(this.seed, u.id, g, 113), rota = Math.floor((t + turn * roll(this.seed, u.id, g, 114)) / turn);
    for (let k = 0; k < W.sentries && n > W.sentries && at >= 0; k++) if ((rota + Math.floor((k * n) / W.sentries)) % n === at) { m.sentry = true; return 'wait'; }
    const keep = W.keep[0] + (W.keep[1] - W.keep[0]) * roll(this.seed, u.id, m.n, 115), win = Math.floor((t + keep * roll(this.seed, u.id, m.n, 116)) / keep);
    if (near >= W.far && roll(this.seed, u.id, m.n, win, 118) < W.easy) return 'idle';
    const lying = m.cover === 'works' || m.cover === 'building' ? W.works : m.cover === 'woods' || m.cover === 'rock' || m.cover === 'wreck' || m.cover === 'rough' || m.cover === 'crest' ? W.cover : W.open;
    return roll(this.seed, u.id, m.n, win, 117) < lying ? 'hide' : 'wait';
  }

  /**
   * Squad life: how far off the way to the enemy a man watching looks now (radians, to his right; DR.life.scan): his own
   * lean, and a fresh look every few seconds of his own, a sentry sweeping the whole arc, the rest a part of it, a man
   * lying less; a leader looks round along his line to his men now and then.
   */
  private scan(sq: Squad, m: Man): number {
    const S = DR.life.scan, u = sq.u, t = this.b.t;
    const every = S.every[0] + (S.every[1] - S.every[0]) * roll(this.seed, u.id, m.n, 132), win = Math.floor((t + every * roll(this.seed, u.id, m.n, 133)) / every);
    const low = m.pose === 'hide' || m.pose === 'observe';
    if (!low && (m.lead || (m.j === undefined && !!m.hasSec)) && roll(this.seed, u.id, m.n, win, 134) < S.back) return (roll(this.seed, u.id, m.n, win, 135) < 0.5 ? -1 : 1) * (1.5 + 0.9 * roll(this.seed, u.id, m.n, win, 136));
    const share = low ? S.lying : m.sentry ? 1 : S.rest;
    return (roll(this.seed, u.id, m.n, 131) - 0.5) * 2 * S.own * (low ? 0.5 : 1) + (roll(this.seed, u.id, m.n, win, 137) - 0.5) * 2 * S.arc * share;
  }

  /**
   * Squad life: the battle time till which a man firing from the line has broken off (DR.life.pause: a fresh clip, a look
   * for his target; a machine gun's new belt or hot barrel), or none while he fires. Each man's spells are his own.
   */
  private pauseOf(sq: Squad, m: Man): number | undefined {
    if (!this.life) return undefined;
    const P = DR.life.pause, u = sq.u, t = this.b.t, gun = m.role === 'mg';
    const [e0, e1] = gun ? P.gun : P.every, [f0, f1] = gun ? P.gunFor : P.for;
    const every = e0 + (e1 - e0) * roll(this.seed, u.id, m.n, 141), at = t + every * roll(this.seed, u.id, m.n, 142), win = Math.floor(at / every);
    const len = f0 + (f1 - f0) * roll(this.seed, u.id, m.n, win, 143), ph = at - win * every;
    return ph < len ? t + (len - ph) : undefined;
  }

  /**
   * Squad life: no line on the move is dressed as on parade. Each man drifts a few paces ahead of and behind his place
   * and a little to either side of it, slowly, in his own time (DR.life.drift): [metres across, metres along] now.
   */
  private drift(sq: Squad, m: Man): [number, number] {
    const D = DR.life.drift, u = sq.u;
    const T = D.every[0] + (D.every[1] - D.every[0]) * roll(this.seed, u.id, m.n, 126), ph = (this.b.t / T + roll(this.seed, u.id, m.n, 127)) * Math.PI * 2;
    return [D.across * Math.sin(ph * 0.73 + 6.3 * roll(this.seed, u.id, m.n, 128)), D.along * Math.sin(ph)];
  }

  /**
   * Squad life: the gait of a man keeping up with his block under fire. Left further behind his place than his own
   * patience (some make it up at once, some not till they are well back) he sprints upright till he has made it up;
   * else he goes at the crouch.
   */
  private gaitOf(sq: Squad, m: Man, gain: number): Pose {
    const u = sq.u, t = this.b.t;
    // He changes his gait only at moments of his own (every second and a half to three): no squad changes step as one.
    if (m.pose === 'run' || m.pose === 'advance') {
      const T = 1.5 + 1.5 * roll(this.seed, u.id, m.n, 154), at = t + T * roll(this.seed, u.id, m.n, 155);
      if (Math.floor(at / T) === Math.floor((at - 0.5) / T)) return m.pose;
    }
    const up = this.upright(sq, m), far = DR.life.rush.sprint * (up ? 0.4 + 0.8 * roll(this.seed, u.id, m.n, 156) : 9 + 4 * roll(this.seed, u.id, m.n, 156));
    return (m.pose === 'run' ? gain > (up ? 1.5 : far * 0.5) : gain > far) ? 'run' : 'advance';
  }

  /**
   * Squad life: a man lying or kneeling who must now get up and go (left behind by his block, its line swung round, its
   * pace picked up) gets up at a moment of his own (up to DR.life.react.rise seconds after he must, his leader at once):
   * true once it has come. Never a section on its feet in one movement.
   */
  private rises(sq: Squad, m: Man): boolean {
    if (!this.life || MOVING.has(m.pose) || VEHICLE.has(m.role) || CREWED.has(m.role)) { m.riseAt = undefined; return true; }
    const t = this.b.t;
    if (m.riseAt === undefined || t > m.riseAt + 2) m.riseAt = t + (m.lead || (m.j === undefined && m.role !== 'mg') ? 0 : DR.life.react.rise * roll(this.seed, sq.u.id, m.n, Math.round(t * 2), 157));
    if (t < m.riseAt - 1e-6) return false;
    m.riseAt = undefined;
    return true;
  }

  /**
   * Squad life: how a man runs when he must cover ground, for a spell of his own: head up and sprinting (DR.life.rush.upright
   * of them at any time), or bent low at the crouch. No squad runs all one way.
   */
  private upright(sq: Squad, m: Man): boolean {
    const u = sq.u, T = 6 + 8 * roll(this.seed, u.id, m.n, 125), win = Math.floor(this.b.t / T + roll(this.seed, u.id, m.n, 129));
    return roll(this.seed, u.id, m.n, win, 130) < DR.life.rush.upright;
  }

  /** Squad life: whether a man on the approach has his rifle at the ready yet (DR.life.ready): its scouts always, the rest each from his own distance. */
  private readyNow(sq: Squad, m: Man, gap: number): boolean {
    const [lo, hi] = DR.life.ready;
    return !!m.scout || gap < DR.ranges.bound * (lo + (hi - lo) * roll(this.seed, sq.u.id, m.n, 151));
  }

  // ---- Thinking: each block twice a second --------------------------------------------------------------------------

  private think(sq: Squad): void {
    const u = sq.u, t = this.b.t;
    const was = sq.phase;
    let ph = this.phaseOf(sq);
    // Lorried infantry and cavalry get down once the enemy is near or they are hit, and their block has slowed to a pace
    // men on foot can keep (the lorries drive in close and the men jump down); they stay down while it keeps to a foot
    // pace, and mount again if it goes on faster (below).
    let got = false;
    // Armoured infantry all aboard half-tracks (sq.of, DR.mech.armoured; the design director, 23 November 07:06Z(2)): they
    // ride on, near the enemy or fighting, till an enemy weapon that can pierce a hull reaches them (the overrun's threat
    // test, sim.hullThreat; no distance of their own), and get down then once their carriers slow, as any do. With lorries
    // among their carriers they get down as lorried foot do.
    const hulls = sq.of !== undefined && sq.veh.length > 0 && sq.veh.every((v) => DR.mech.armoured.includes(v.ml ?? ''));
    const cue = hulls ? this.b.hullThreat(u) : ph === 'fight' || ph === 'pinned' || ph === 'hold' || ph === 'startline' || this.nearestGap(u) < DR.ranges.debus || sq.harm > 0;
    if (!sq.down && sq.slow >= MOUNT.downAfter && cue) {
      if (u.kind === 'mot' || (u.kind === 'cav' && this.nearestGap(u) < DR.ranges.debus * 0.85)) { got = sq.down = true; this.alight(sq); }
    }
    // Down, and the block goes on faster than they can follow on foot: up again (board). Slowed again while they mount:
    // down where they are.
    if (sq.down && ph !== 'gone' && (u.kind === 'cav' || (u.kind === 'mot' && sq.veh.length))) {
      sq.close = sq.fast >= 1 ? true : sq.slow >= 3 ? false : sq.close;
      // (Men in lorries: never up again while men are still getting down over the tailboards: hopOut sees them all down)
      if (sq.boarding === undefined && sq.fast >= MOUNT.upAfter && sq.hop === undefined) this.mountUp(sq);
      else if (sq.boarding !== undefined && sq.slow >= MOUNT.stayAfter) { got = true; this.alight(sq); }
    }
    // Just down: each man goes from his lorry or his horse to his place, as on a fresh phase.
    if (ph !== was || got) { if (this.life) sq.lastFor = sq.since > 0 ? t - sq.since : Infinity; sq.phase = ph; sq.since = t; }
    ph = sq.phase;
    sq.ran = u.state === 'routing' || (sq.ran && u.state === 'wavering' && t - sq.since < 8);
    if (ph === 'gone') return;
    if (u.kind === 'arm') { this.tanks(sq); return; }
    if (u.kind === 'art') { if (u.team) this.teams(sq); else this.guns(sq); return; }
    if (sq.boarding !== undefined) { this.board(sq); return; }
    // Men in lorries: getting down, a man at a time over each tailboard; the block's phase takes them once the last is
    // down, as on a fresh phase.
    if (sq.hop !== undefined) { if (!this.hopOut(sq)) return; sq.since = t; }
    this.lorries(sq);
    if ((u.kind === 'mot' || u.kind === 'cav') && !sq.down) { this.mounted(sq); return; }
    // Squad life: each man takes a fresh phase up in his own moment; till then he goes on with what he was at.
    const held = this.life ? this.lags(sq, was) : null;
    switch (ph) {
      case 'column': this.column(sq); break;
      case 'deploy': this.advance(sq, false); break;
      case 'bound': this.advance(sq, true); break;
      case 'startline': this.lie(sq, 'wait'); break;
      case 'fight': if (this.byBook) this.fightBook(sq); else this.fight(sq); break;
      case 'hold': this.hold(sq); break;
      case 'rest': this.rest(sq); break;
      case 'pinned': this.pinned(sq); break;
      case 'fallback': this.fallback(sq); break;
      case 'flee': this.flee(sq); break;
      case 'rally': this.rally(sq); break;
      default: break;
    }
    if (held) for (const [m, snap] of held) Object.assign(m, snap);
    this.ambush(sq);
    this.officer(sq);
    for (const m of sq.men) if (m.fell === undefined && !m.job && MOVING.has(m.pose) && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 0.3) this.arrive(sq, m);
  }

  /**
   * He has reached his place: off his feet into the pose for it (no man runs on the spot). A rifleman with a target
   * fires from it, without one he kneels watching; a routing man crouches where he got to; a tank halts, a gun towed
   * into the line unhooks, a lorry parks.
   */
  private arrive(sq: Squad, m: Man): void {
    m.v = 0;
    // (squad life: a routing man may run upright or bent low; he is known by his block's rout as well as his pose)
    const routed = m.pose === 'flee' || (this.life && sq.phase === 'flee');
    // One who keeps by a weapon or an officer waits there for it (pairs gives him his pose).
    if (m.pr) { m.pose = routed ? 'cower' : 'wait'; return; }
    switch (m.role) {
      case 'tank': m.pose = 'halt'; return;
      case 'lorry': m.pose = 'parked'; return;
      case 'gun': if (m.pose === 'towed' && sq.phase === 'battery') m.pose = 'deployed'; return;
      // A support team's crew sets its weapon up in the line, or down a moment on the march; a routing one crouches.
      case 'hmg': case 'mortar': m.pose = routed ? 'cower' : sq.phase === 'battery' ? 'deployed' : 'wait'; return;
      default: {
        m.until = Math.max(m.until, this.b.t + 1.5);
        if (routed) { m.pose = 'cower'; return; }
        if (m.aid !== undefined) { m.pose = 'wait'; m.aim = undefined; return; } // kneeling over his fallen mate
        if (!m.aim && m.role !== 'officer' && (sq.phase === 'fight' || sq.phase === 'pinned')) m.aim = this.aimAt(m, this.threat(sq.u));
        if (m.aim && m.role !== 'officer') m.pose = this.firePose(m);
        else {
          m.pose = m.role === 'mg' && sq.phase !== 'fight' ? 'hide' : 'wait';
          // (squad life: straight into his own stance for the watch, where his block holds or waits)
          if (this.life && (sq.phase === 'hold' || sq.phase === 'startline')) m.pose = this.watchPose(sq, m, this.nearestGap(sq.u), m.pose);
        }
      }
    }
  }

  /** The men still standing, not helping a fallen comrade. */
  private up(sq: Squad): Man[] { return this.memo(sq).up.filter((m) => !this.aiding(m) && !(m.role === 'holder' && sq.down)); }
  /**
   * Whether a man is on his feet: a key man while the rules keep his model; the rest of a section while their block is
   * on the field and he has not fallen (falls).
   */
  private stands(sq: Squad, m: Man): boolean { return m.fell === undefined && !m.job && (m.j === undefined ? m.k < sq.u.figures : live(sq.u)); }

  /**
   * The block's men on their feet, worked out again only when that changes: a man falls or joins another section (falls,
   * pairs: they clear it), or the rules lift a model or take the block off the field.
   */
  private memo(sq: Squad): Memo {
    const u = sq.u, on = live(u);
    if (sq.memo && sq.memo.fig === u.figures && sq.memo.on === on) return sq.memo;
    const up: Man[] = [], keys: Man[] = [], keyOf = new Map<number, Man>(), bySec = new Map<number, Man[]>(), stand: Man[] = [];
    for (const m of sq.men) {
      if (!this.stands(sq, m)) continue;
      stand.push(m);
      if (m.j === undefined) { keys.push(m); keyOf.set(m.k, m); }
      if (m.pr) continue;
      up.push(m);
      if (m.j !== undefined) { const s = grp(m); let l = bySec.get(s); if (!l) bySec.set(s, (l = [])); l.push(m); }
    }
    const all = sq.tier >= 1 ? [...up].sort((a, c) => (a.sec ?? a.k) - (c.sec ?? c.k) || (a.j ?? 0) - (c.j ?? 0)) : up;
    return (sq.memo = { fig: u.figures, on, stand, struck: on ? [...stand, ...sq.veh] : stand, up, keys, keyOf, all, bySec });
  }

  /** Whom a man who keeps by another keeps by in his section now: a loader his weapon, a gun's crew its gun, a runner his officer. */
  private leadOf(sq: Squad, m: Man): Man | undefined {
    const want = LEADS[m.role];
    if (!want) return undefined;
    const M = this.memo(sq), s = m.sec ?? m.k, key = M.keyOf.get(s);
    if (key && key !== m && !m.sub && want.includes(key.role)) return key;
    return M.bySec.get(grp(m))?.find((x) => x !== m && want.includes(x.role));
  }

  /**
   * Where a platoon's group forms, about its key man's place (across, along; outer: toward the block's flank): its other
   * squads one behind another, a little to either side, PLATOON.step metres apart; its headquarters inside the leading
   * squad's wedge, behind its point. At rest, their knots side by side.
   */
  private inPlatoon(sq: Squad, sub: number, outer: number, tpl: 'line' | 'rest'): [number, number] {
    const P = DR.sections.platoon, hq = sub >= platoonOf(sq.u.nat).squads;
    if (tpl === 'rest') return hq ? [0, -9] : [(sub % 2 ? outer : -outer) * 11 * Math.ceil(sub / 2), 0];
    return hq ? [0, -4] : [(sub % 2 ? -outer : outer) * 4, -P.step * sub];
  }

  /**
   * His place in his section, about his key man's place in the block (slot): in line, a shallow wedge with the key man
   * at its point, its automatic weapon at the outer end (toward the block's flank), the rest alternately either side of
   * him and a little back, a few metres apart; on the move its scouts out ahead; at rest, a knot round him.
   */
  private inSection(sq: Squad, m: Man, tpl: 'line' | 'rest'): [number, number] {
    const S = DR.sections, M = this.memo(sq), s = m.sec ?? m.k, key = M.keyOf.get(s);
    let [ka, kl] = key ? this.slot(sq, key, tpl) : [m.jx * 3, 0];
    // A platoon's other squads and its headquarters form about their own places behind the key man's squad, each on its
    // leader as a section on its key man.
    if (m.sub) { const [oa, ol] = this.inPlatoon(sq, m.sub, key?.team === 1 ? -1 : 1, tpl); ka += oa; kl += ol; }
    if (m.lead) return [ka + m.jx * 0.2, kl + m.jy * 0.2];
    const mates = (M.bySec.get(grp(m)) ?? [m]).filter((x) => !x.lead), i = Math.max(0, mates.indexOf(m));
    if (tpl === 'rest') {
      const n = Math.max(1, mates.length), ang = (i / n) * Math.PI * 2 + m.jx * 0.1, r = 3 + n * 0.25;
      return [ka + Math.cos(ang) * r, kl + Math.sin(ang) * r * 0.8];
    }
    const ph = sq.phase;
    if (m.scout && (ph === 'deploy' || ph === 'bound' || ph === 'column')) return [ka + (i % 2 ? 5 : -5) + m.jx * 0.3, kl + S.ahead + m.jy * 0.4];
    // Its automatic weapon at the end of the wedge's outer arm (toward the block's flank, a second gun inward); the rest
    // alternately either side of the key man, inner arm first.
    const outer = key?.team === 1 ? -1 : 1, rest = mates.filter((x) => x.role !== 'mg');
    let side: number, rank: number;
    if (m.role === 'mg') { const g = mates.filter((x) => x.role === 'mg').indexOf(m); side = g % 2 ? -outer : outer; rank = Math.ceil(rest.length / 2) + 1; }
    else { const r = Math.max(0, rest.indexOf(m)); side = r % 2 ? outer : -outer; rank = Math.floor(r / 2) + 1; }
    return [ka + side * rank * S.space + m.jx * 0.25, kl - rank * S.space * 0.35 + m.jy * 0.3];
  }

  /**
   * The men who keep by a weapon or an officer (Man.pr), each step: an automatic weapon's or a launcher's number two
   * beside it and its bearers a little behind, a gun's crew at their places round it, a support team's bearers behind
   * the weapon, the officer's runners at his elbow, a horse holder's two by him. Each goes where it goes, as it goes, and
   * takes the pose that goes with its: down beside the gun as it fires, kneeling by it set up, aboard as it rides. A
   * rifle section's gunner fallen, his number two takes the gun up; with none to keep by, he fights as a rifleman.
   */
  private pairs(sq: Squad, room?: Room): void {
    if (sq.tier < 1) return;
    const u = sq.u, t = this.b.t;
    const count = new Map<Man, number>(), booked = new Set<Man>();
    for (const m of sq.men) {
      if (!m.pr || !this.stands(sq, m)) continue;
      // Mounting again: each makes for his own lorry or horse (board), a holder's two excepted.
      if (sq.boarding !== undefined && m.role !== 'holder') continue;
      const L = this.leadOf(sq, m);
      if (!L) {
        const key = this.memo(sq).keyOf.get(m.sec ?? m.k);
        m.pr = undefined;
        if (m.role === 'loader' && key && key.role === 'rifle') m.role = 'mg'; // he takes up the gun
        else if (m.role === 'loader') m.role = 'rifle';
        sq.memo = undefined;
        continue;
      }
      const i = count.get(L) ?? 0;
      count.set(L, i + 1);
      const [oa, ol] = L.role === 'gun' ? GUN_CREW[i % GUN_CREW.length]
        : L.role === 'officer' ? ([[-2.5, -1.2], [2.2, -2.2], [0.6, -4]] as [number, number][])[i % 3]
        : L.role === 'holder' ? [i % 2 ? 3 : -3, -1]
        : CREWED.has(L.role) ? [i % 2 ? 2 : -2, -4 - 2.5 * Math.floor(i / 2)]
        : L.role === 'at' ? (i ? [-1.5, -3] : [1.3, -0.6])
        : ((o: [number, number]): [number, number] => [o[0], o[1] - 3 * Math.floor(i / 3)])(([[-1.6, -0.6], [1.5, -3.5], [-2, -5.5]] as [number, number][])[i % 3]);
      const moving = MOVING.has(L.pose) && L.v > 0;
      // On the move, about where he will be by the next look (not his spot, which runs ahead of him by more than his
      // follower's runs ahead of the follower: anchored there, a runner caught up into his officer).
      const lw = Math.hypot(L.spot[0] - L.x, L.spot[1] - L.y), la = moving && lw > 0 ? Math.min(lw, L.v * 0.75) / lw : 0;
      const anchor = moving ? { x: L.x + (L.spot[0] - L.x) * la, y: L.y + (L.spot[1] - L.y) * la } : L;
      const carried = L.pose === 'mounted' || L.pose === 'ride' || L.pose === 'horses';
      let p = this.leash(sq, toWorld(anchor, L.face, oa, ol), -4);
      // Clear of his mates' places (space): beside his weapon, never in another man. A lead on the move is booked where he
      // is and where he will be by the next look (space booked only where he is going), so none is placed on him.
      if (room && !carried) {
        if (moving && !booked.has(L)) { booked.add(L); this.bookRoom(room, L, [L.x, L.y]); this.bookRoom(room, L, [anchor.x, anchor.y]); }
        p = this.leash(sq, this.clear(room, sq, m, p), -4); this.bookRoom(room, m, p);
      }
      const d = Math.hypot(p[0] - m.x, p[1] - m.y);
      m.aim = undefined; m.hidden = undefined; m.cover = L.cover; m.until = t + 0.5; m.spot = p;
      // Aboard with it: in his seat in his lorry or the gun's tractor (walk), on his horse.
      if (carried) { m.pose = L.pose; m.v = L.pose === 'mounted' ? 0 : Math.min(14, Math.max(L.v, d)); continue; }
      // A gun limbered up: its crew run to her tractor and climb aboard (then ride in it: walk).
      if (L.role === 'gun' && L.pose === 'towed' && (sq.phase === 'limbered' || sq.phase === 'flee' || sq.phase === 'fallback') && sq.veh.length) {
        const seat = this.seat(sq, m);
        if (m.pose === 'mounted' || Math.hypot(seat[0] - m.x, seat[1] - m.y) < 0.6) { m.pose = 'mounted'; m.v = 0; }
        else { m.spot = seat; m.pose = 'run'; m.v = CATCH_UP; }
        continue;
      }
      if (moving) {
        m.pose = L.pose === 'towed' ? 'walk' : L.pose;
        m.v = Math.min(CATCH_UP, Math.max(L.v, d / 1.2));
        continue;
      }
      const still: Pose = (L.hidden && (L.role === 'gun' || CREWED.has(L.role))) || L.pose === 'cower' ? 'cower'
        : L.role === 'gun' ? (L.pose === 'fire' ? 'fire' : i === 0 && u.engaged === undefined ? 'idle' : 'wait')
        : L.pose === 'lmg_burst' || L.pose === 'fire_prone' || L.pose === 'hide' ? 'hide'
        : L.pose === 'idle' && L.role === 'officer' ? 'wait'
        : L.pose === 'idle' ? 'idle' : 'wait';
      // Off his place by more than a shuffle (half a metre: further, he would stand in a mate placed beside it), he goes.
      if (d > 0.45) {
        const far = d > 8;
        m.pose = far ? 'run' : still === 'hide' || still === 'cower' ? 'crawl' : 'creep';
        m.v = far ? 4.5 : m.pose === 'crawl' ? 1.4 : 2.5;
      } else { m.pose = still; m.v = 0; }
    }
  }
  /**
   * Room between the men of a section block (SPACE): the key men's and the rest's places (all but those who keep by a
   * weapon or an officer, whom pairs places after), each kept clear of the places of those before him (key men first,
   * then section by section). A man still is placed where he is; one on his way, where he is going. One whose place
   * moves while he is still gets up and steps across to it (creeping, or crawling from lying), and takes his pose again
   * there. Returns the places booked, for pairs.
   */
  private space(sq: Squad): Room | undefined {
    if (sq.tier < 1) return undefined;
    const M = this.memo(sq), t = this.b.t, room: Room = new Map();
    const order = [...M.up].sort((a, c) => (a.j === undefined ? 0 : 1) - (c.j === undefined ? 0 : 1) || (a.sec ?? a.k) - (c.sec ?? c.k) || (a.j ?? 0) - (c.j ?? 0));
    for (const m of order) {
      if (VEHICLE.has(m.role) || CARRIED.has(m.pose) || sq.boarding !== undefined) continue;
      const going = MOVING.has(m.pose) && m.v > 0, at: [number, number] = going ? m.spot : [m.x, m.y];
      const p = this.leash(sq, this.clear(room, sq, m, at), -4);
      if (Math.hypot(p[0] - at[0], p[1] - at[1]) > 0.05) {
        m.spot = p;
        if (!going) {
          const low = m.pose === 'fire_prone' || m.pose === 'hide' || m.pose === 'cower' || m.pose === 'lmg_burst' || m.pose === 'observe';
          m.pose = low ? 'crawl' : 'creep'; m.v = low ? 1.2 : 2; m.until = t + 1; m.aim = undefined;
        }
      }
      this.bookRoom(room, m, p);
    }
    return room;
  }

  /**
   * The nearest place to p clear of every man booked in the room (SPACE): p itself, or the first free of the points on
   * rings round it (0.8 to 4 m out, nearest first, from a turn of his own); in a crowd beyond that, p.
   */
  private clear(room: Room, sq: Squad, m: Man, p: [number, number]): [number, number] {
    const free = (x: number, y: number): boolean => {
      const cx = Math.floor(x / SPACE.cell), cy = Math.floor(y / SPACE.cell);
      for (let i = -1; i <= 1; i++) for (let j = -1; j <= 1; j++) for (const q of room.get((cx + i) * 65536 + cy + j) ?? []) {
        const min = m.role === 'crew' && q.m.role === 'crew' && (m.sec ?? m.k) === (q.m.sec ?? q.m.k) ? SPACE.crew : SPACE.men;
        if (Math.hypot(q.x - x, q.y - y) < min) return false;
      }
      return true;
    };
    if (free(p[0], p[1])) return p;
    const a0 = roll(this.seed, sq.u.id, m.n, 53) * 2 * Math.PI;
    for (let r = 0.8; r <= 4.01; r += 0.8) {
      const n = Math.max(8, Math.round(r * 6));
      for (let i = 0; i < n; i++) {
        const a = a0 + (i * 2 * Math.PI) / n, x = p[0] + Math.sin(a) * r, y = p[1] + Math.cos(a) * r;
        if (free(x, y)) return [x, y];
      }
    }
    return p;
  }

  /** Books a man's place in the room. */
  private bookRoom(room: Room, m: Man, p: [number, number]): void {
    const key = Math.floor(p[0] / SPACE.cell) * 65536 + Math.floor(p[1] / SPACE.cell);
    let l = room.get(key);
    if (!l) room.set(key, (l = []));
    l.push({ m, x: p[0], y: p[1] });
  }

  private aiding(m: Man): boolean {
    if (m.aid === undefined) return false;
    if (this.b.t >= m.until) { m.aid = undefined; return false; }
    return true;
  }

  /**
   * His place in the block's line, metres across (right of the middle) and along (forward of the middle). 'line': the
   * two teams side by side across the front, each about its half's middle at the style's spacing, the machine gun at
   * its team's outer end, the officer behind the middle, the anti-tank team beside him. 'column': two files up the
   * middle of the block. 'rest': three knots, the teams and the headquarters.
   * Drawn as sections (Drill.sections), each key man has the place a model has, the sections spread wider and in two
   * rows so each has room for its men; the rest of a section stand round their key man (inSection); on the march the
   * whole block goes in files (as many as its men need), section after section.
   */
  private slot(sq: Squad, m: Man, tpl: 'line' | 'column' | 'rest'): [number, number] {
    const u = sq.u, N = this.nums(u), band = this.band(u), M = this.memo(sq), S = DR.sections, sec = sq.tier >= 1;
    // A loader, a gun's crew, a runner: his place is his weapon's (or his officer's); Drill.pairs puts him beside it.
    if (m.pr) { const L = this.leadOf(sq, m); if (L) return this.slot(sq, L, tpl); }
    if (sec && tpl === 'column' && u.kind !== 'arm' && u.kind !== 'art') {
      const i = Math.max(0, M.all.indexOf(m)), nf = Math.max(2, Math.ceil(M.all.length / S.files));
      return [((i % nf) - (nf - 1) / 2) * 2.5 + m.jx * 0.15, u.d / 2 - 4 - Math.floor(i / nf) * 3];
    }
    if (sec && m.j !== undefined) return this.inSection(sq, m, tpl === 'rest' ? 'rest' : 'line');
    const sp = N.spacing * (band === 0 ? DR.xp.bunch : band === 2 ? DR.xp.spread : 1) * (sec ? 2 : 1);
    const all = M.keys;
    if (tpl === 'column') {
      const i = all.indexOf(m);
      return [(i % 2 ? 1 : -1) * 2.5 + m.jx * 0.2, u.d / 2 - 4 - Math.floor(i / 2) * 5];
    }
    if (u.kind === 'arm' || u.kind === 'art') {
      const i = all.indexOf(m), n = Math.max(1, all.length), gap = Math.min(u.w / n, u.kind === 'art' ? 32 : 40);
      return [(i - (n - 1) / 2) * gap + m.jx, (u.kind === 'arm' ? (i % 2 ? -6 : 6) : 0) + m.jy];
    }
    if (tpl === 'rest') {
      const c: [number, number] = m.team === 0 ? [0, -u.d / 4] : [(m.team === 1 ? -1 : 1) * u.w / (sec ? 3 : 5), 0];
      const mates = all.filter((x) => x.team === m.team), i = mates.indexOf(m), ang = (i / Math.max(1, mates.length)) * Math.PI * 2 + m.jx;
      // Sections rest each in its own knot, round the team's middle.
      const r = sec ? 6 + 3 * mates.length : 4.5;
      return [c[0] + Math.cos(ang) * r, c[1] + Math.sin(ang) * (sec ? r * 0.8 : 3.5)];
    }
    if (m.team === 0) {
      if (m.role === 'officer') return [m.jx, -u.d / 2 + 6];
      return [u.w / 8 + m.jx, -u.d / 2 + 10];
    }
    const side = m.team === 1 ? -1 : 1;
    const mates = all.filter((x) => x.team === m.team).sort((a, c) => (a.role === 'mg' ? -1 : c.role === 'mg' ? 1 : a.k - c.k));
    const i = mates.indexOf(m), n = mates.length;
    const half = u.w / 2 - 6 + (sec ? 25 : 0), span = Math.min(half - 4, sp * Math.max(0, n - 1));
    // The machine gun at the outer end; the riflemen inward from it.
    const outer = side * Math.min(half, u.w / 4 + span / 2);
    const a = outer - side * (n > 1 ? (span * i) / (n - 1) : 0);
    const l = u.d / 2 - 4 + (i % 2 ? (sec ? -(sq.tier === 2 ? S.platoon.rows : S.rows) : -3) : 0) + m.jy * 0.5;
    return [a + m.jx * 0.4, l];
  }

  /**
   * The best place near p for a man to stand: the shelter of the ground and what lies on it, a little for being near his
   * place in the line, never on top of a mate. 'dir' points at the enemy. Within his experience's reach of p.
   */
  private seek(sq: Squad, m: Man, p: [number, number], dir: [number, number], mates: Man[], reach?: number): { x: number; y: number; kind: CoverKind; v: number } {
    const f = this.b.field, u = sq.u;
    const R = reach ?? DR.xp.seek[this.band(u)] * (u.kind === 'arm' || u.kind === 'art' ? 2 : 1);
    const going = m.role === 'tank' ? 'tracks' : m.role === 'gun' ? 'guns' : m.role === 'lorry' ? 'wheels' : 'foot';
    const pace = LAND.pace[going];
    const sep = m.role === 'tank' ? 16 : m.role === 'gun' ? 18 : m.role === 'lorry' ? 10 : CREWED.has(m.role) ? DR.teams.sep : 3.5;
    const cand: [number, number][] = [p];
    const a0 = roll(this.seed, u.id, m.n, 7) * Math.PI * 2;
    // Settling into a line (not a rush): he looks further, to the wood's edge or the houses behind the line or beside it,
    // as a section leader places his men in the best cover within shouting distance of their places.
    const settle = reach === undefined && going === 'foot' && (sq.phase === 'fight' || sq.phase === 'hold' || sq.phase === 'startline' || sq.phase === 'pinned' || sq.phase === 'rest');
    const rings = settle ? [R * 0.5, R, R * 2, R * 3] : [R * 0.5, R];
    rings.forEach((r, j) => { for (let i = 0; i < 8; i++) { const a = a0 + (i * Math.PI) / 4 + (j % 2 ? Math.PI / 8 : 0); cand.push([p[0] + Math.sin(a) * r, p[1] + Math.cos(a) * r]); } });
    // A burnt-out tank (sheltering behind it, from the enemy) and a shell hole near him.
    if (going === 'foot') {
      for (const z of this.b.wrecks) { const [zx, zy] = this.wreckSpot(z); if (Math.abs(zx - p[0]) < R + 8 && Math.abs(zy - p[1]) < R + 8) cand.push([zx - dir[0] * 4, zy - dir[1] * 4]); }
      for (const z of this.b.craters) if (Math.abs(z.x - p[0]) < R && Math.abs(z.y - p[1]) < R) cand.push([z.x, z.y]);
    }
    // Only mates who could stand within a stride of one of the places looked at count (most of a big block cannot); not
    // those who keep by a weapon or an officer, who go where it goes (counted, a gunner would flee his own loaders' places
    // and they would follow him: back and forth for minutes), and are kept clear of the rest by space.
    const span = rings[rings.length - 1] + sep + 1;
    const near = mates.filter((o) => o !== m && o.fell === undefined && !o.pr && Math.abs(o.spot[0] - p[0]) < span && Math.abs(o.spot[1] - p[1]) < span);
    let best = { x: p[0], y: p[1], kind: 'none' as CoverKind, v: -Infinity };
    for (const [cx0, cy0] of cand) {
      const cx = clamp(cx0, 3, f.w - 3), cy = clamp(cy0, 3, f.h - 3);
      // Only within his block's box: a place beyond it he could not keep.
      const [qx, qy] = this.leash(sq, [cx, cy], m.role === 'lorry' ? 116 : -2);
      if (qx !== cx || qy !== cy) continue;
      const g = groundOf(f, cx, cy);
      if (pace[g] <= (going === 'foot' ? 0 : 0.12)) continue;
      const c = this.shelter(cx, cy, g, dir, going === 'foot', m.role === 'tank');
      let s = c.v - (settle ? 0.008 : 0.015) * Math.hypot(cx - p[0], cy - p[1]);
      for (const o of near) if (Math.hypot(o.spot[0] - cx, o.spot[1] - cy) < sep) s -= 0.3;
      if (s > best.v) best = { x: cx, y: cy, kind: c.kind, v: s };
    }
    if (best.v === -Infinity) best = { x: clamp(p[0], 3, f.w - 3), y: clamp(p[1], 3, f.h - 3), kind: 'none', v: 0 };
    return best;
  }

  /** How much shelter a point gives a man (or a tank: a crest is her hull-down) facing 'dir'. */
  private shelter(x: number, y: number, g: Ground, dir: [number, number], foot: boolean, tank: boolean): { v: number; kind: CoverKind } {
    const f = this.b.field, C = DR.cover;
    let v = tank ? (g === 'woods' || BUILT.includes(g) ? 0.4 : 0) : C.ground[g];
    let kind: CoverKind = COVER_OF[g] ?? 'none';
    if (v < -1) return { v, kind: 'none' };
    // The edge facing the enemy: a man lines the wood's or the houses' edge, where he can see and fire.
    if (v >= 0.3) { const ahead = groundOf(f, x + dir[0] * 15, y + dir[1] * 15); if (C.ground[ahead] < v - 0.2) v += C.edge; }
    // A fold of ground in front: hull-down for a tank, a bank to lie behind for a man.
    const rise = heightOf(f, x + dir[0] * 15, y + dir[1] * 15) - heightOf(f, x, y);
    const [lo, hi] = DR.ranges.hullDown;
    if (rise >= (tank ? lo : 0.8) && rise <= (tank ? hi : 4)) { const c = tank ? 0.8 : C.crest; if (c > v - 0.05) { v = Math.max(v, c) + 0.05; kind = 'crest'; } }
    if (foot) {
      for (const z of this.b.wrecks) {
        const [zx, zy] = this.wreckSpot(z), dx = zx - x, dy = zy - y, d = Math.hypot(dx, dy);
        if (d > 2 && d < 8 && (dx * dir[0] + dy * dir[1]) / d > 0.5 && C.wreck > v) { v = C.wreck; kind = 'wreck'; }
      }
      if (v < C.crater) for (const z of this.b.craters) if (Math.hypot(z.x - x, z.y - y) <= Math.max(1.5, z.r)) { v = C.crater; kind = 'crater'; break; }
    }
    return { v, kind };
  }

  /**
   * A point pulled inside the block's leash: its box, as the rules hold it, and a margin round it (more for men running).
   * The block is the truth: no man strays from it further than this.
   */
  /**
   * Where a man on foot makes for next, on a field with hedges: the gate on his way, found again when his spot has moved
   * more than 2 m (or he more than 5 m) since, and on through it and beyond. Only a gate inside his block's ring (he
   * never leaves his block for one). He goes through square, a stride either side of the gap, at his own place across it
   * by his number: a section goes through abreast and not in one file on one point.
   */
  private via(sq: Squad, m: Man): [number, number] | null {
    const sp = m.spot;
    if (!m.viaFor || Math.hypot(sp[0] - m.viaFor[0], sp[1] - m.viaFor[1]) > 2 || (!m.via?.length && (!m.viaAt || Math.hypot(m.x - m.viaAt[0], m.y - m.viaAt[1]) > 5))) {
      m.viaFor = [sp[0], sp[1]]; m.viaAt = [m.x, m.y]; m.via = this.gateFor(sq, m);
    }
    while (m.via?.length && Math.hypot(m.via[0][0] - m.x, m.via[0][1] - m.y) < 0.4) {
      m.via.shift();
      if (!m.via.length) { m.viaAt = [m.x, m.y]; m.via = this.gateFor(sq, m); break; }
    }
    return m.via?.[0] ?? null;
  }

  private gateFor(sq: Squad, m: Man): [number, number][] | undefined {
    const g = this.b.gateOn(m.x, m.y, m.spot[0], m.spot[1]);
    if (!g) return undefined;
    const [lx, ly] = this.leash(sq, [g.x, g.y]);
    if (Math.hypot(lx - g.x, ly - g.y) > 0.5) return undefined;
    // Along the hedge and square across it, the square pointing to his spot's side.
    let [hx, hy] = this.b.hedgeDir(g.x, g.y) ?? [0, 0];
    if (!hx && !hy) { const wx = g.x - m.x, wy = g.y - m.y, wl = Math.hypot(wx, wy) || 1; hx = -wy / wl; hy = wx / wl; }
    let nx = -hy, ny = hx;
    if ((m.spot[0] - g.x) * nx + (m.spot[1] - g.y) * ny < 0) { nx = -nx; ny = -ny; }
    const half = Math.max(0, (g.width ?? 3) / 2 - 0.5), o = (roll(this.seed, sq.u.id, m.n, 61) * 2 - 1) * half, S = 1.5;
    const cx = g.x + hx * o, cy = g.y + hy * o, at = (m.x - g.x) * nx + (m.y - g.y) * ny;
    // Already in the gap (in line with it and within a stride of it), he goes on through; anywhere else, to its mouth.
    const inGap = Math.abs((m.x - cx) * hx + (m.y - cy) * hy) <= half + 0.5;
    const way: [number, number][] = [];
    if (!inGap || at < -S + 0.3) way.push([cx - nx * S, cy - ny * S]);
    if (!inGap || at < S - 0.3) way.push([cx + nx * S, cy + ny * S]);
    return way.length ? way : undefined;
  }

  private leash(sq: Squad, p: [number, number], extra = 0): [number, number] {
    const u = sq.u, m = (sq.phase === 'flee' ? 70 : 45) + extra;
    if (!sq.ax || sq.ax[0] !== u.face) { const [rx0, ry0] = right(u.face), [fx0, fy0] = fwd(u.face); sq.ax = [u.face, rx0, ry0, fx0, fy0]; }
    const [, rx, ry, fx, fy] = sq.ax;
    const dx = p[0] - u.x, dy = p[1] - u.y, a0 = dx * rx + dy * ry, l0 = dx * fx + dy * fy;
    const a = clamp(a0, -u.w / 2 - m, u.w / 2 + m), l = clamp(l0, -u.d / 2 - m, u.d / 2 + m);
    return a === a0 && l === l0 ? p : [u.x + a * rx + l * fx, u.y + a * ry + l * fy];
  }

  /**
   * Keeps a man (a tank, a lorry) at a place that moves with his block: he heads a little ahead of it, so he keeps its
   * pace and never stops short; one left behind closes up faster. When the block halts he reaches it and settles.
   */
  private follow(sq: Squad, m: Man, s: [number, number], pose: Pose, mul = 1, way = sq.heading): void {
    const v = sq.speed, on = v > 0.4, [hx, hy] = fwd(way);
    const off = Math.hypot(s[0] - m.x, s[1] - m.y), behind = (s[0] - m.x) * hx + (s[1] - m.y) * hy;
    // He heads for a point a second and a half ahead of his place (or of himself, where he is ahead of it), so he never
    // runs onto it while the block moves; behind his place he closes up, ahead of it he eases off, and well ahead of it
    // he waits for the line to come up to him (he never turns back to it).
    const lead = on ? v * 1.5 + clamp(-behind, 0, 10) : 0;
    m.spot = [s[0] + hx * lead, s[1] + hy * lead]; m.slotAt = s; m.cover = 'none';
    m.v = on ? (behind < -8 - v * 1.5 ? 0 : v * (behind > 6 ? 1.4 * mul : behind > 2 ? 1.1 : behind < -2 ? 0.85 : 1)) : off > 1 ? 2.5 : 0;
    // The gait for the pace: no man creeps or strolls at a run.
    m.pose = pose === 'walk' && m.v > 2.5 ? 'run' : pose === 'creep' && m.v > 2.8 ? 'advance' : pose === 'crawl' && m.v > 1.6 ? 'creep' : pose;
    // A man runs no faster than a man can (a lorry or a tank as fast as hers): the block's box carries him beyond that.
    m.v = Math.min(m.v, m.role === 'tank' ? tankTop(sq) : m.role === 'lorry' || m.role === 'gun' ? 16 : pose === 'ride' ? 14 : CATCH_UP);
    if (m.v === 0) { m.spot = [m.x, m.y]; this.arrive(sq, m); }
  }

  /** Sends a man to a place near p (the best shelter there unless 'open'), at a pace, in a pose. */
  private go(sq: Squad, m: Man, p: [number, number], pose: Pose, pace: number, until: number, dir?: [number, number], reach?: number): void {
    p = this.leash(sq, p, -4);
    const best = this.seek(sq, m, p, dir ?? this.threatDir(sq), sq.men, reach);
    m.spot = [best.x, best.y]; m.cover = best.kind; m.slotAt = p;
    m.pose = pose; m.v = pace; m.until = until;
  }

  /** The firing pose for where he lies: standing in fieldworks or at a window, kneeling in woods or behind rocks or a crest, lying in the open. */
  private firePose(m: Man): Pose {
    if (m.role === 'mg') return 'lmg_burst';
    switch (m.cover) {
      case 'works': case 'building': return 'fire_stand';
      case 'woods': case 'rock': case 'wreck': case 'rough': return 'fire_kneel';
      default: return 'fire_prone';
    }
  }

  /**
   * Squad life: off his sights a moment where he lies (a fresh clip, a stoppage, a look for his target): ducked below the
   * parapet or the sill, head down behind his tree or his rock; in the open still at his rifle, not firing (a machine
   * gun silent on its bipod). The poses the 3D game already has: there is no reload clip yet (the soldier modeller's to
   * make; once there is one, this is where it goes).
   */
  private lowPose(m: Man): Pose {
    if (m.role === 'mg') return 'hide';
    switch (m.cover) {
      case 'works': case 'building': return 'wait';
      case 'woods': case 'rock': case 'wreck': case 'rough': return 'cower';
      default: return 'hide';
    }
  }

  /** The nearest standing man of the block it fights, else a point in that block: where he fires. */
  private aimAt(m: Man, e: LandUnit | null): [number, number] | undefined {
    if (!e) return undefined;
    const sq = this.squads.get(e.id);
    if (!sq) return [e.x, e.y];
    // The nearest key man, and (drawn as sections) the nearest man of his section.
    const M = this.memo(sq);
    let best: Man | undefined, d = Infinity;
    for (const o of M.keys) { const dd = Math.hypot(o.x - m.x, o.y - m.y); if (dd < d) { d = dd; best = o; } }
    if (best && sq.tier >= 1) for (let g = 0; g < 8; g++) for (const o of M.bySec.get(best.k * 8 + g) ?? []) { const dd = Math.hypot(o.x - m.x, o.y - m.y); if (dd < d) { d = dd; best = o; } }
    return best ? [best.x, best.y] : [e.x, e.y];
  }

  // ---- Foot ---------------------------------------------------------------------------------------------------------

  private column(sq: Squad): void {
    const u = sq.u, t = this.b.t, h = sq.heading;
    for (const m of this.up(sq)) {
      const [a, l] = this.slot(sq, m, 'column');
      const s = toWorld(u, h, a, l);
      m.aim = undefined; m.hidden = undefined; m.until = t + 1;
      this.follow(sq, m, s, u.kind === 'cav' && !sq.down ? 'ride' : 'walk');
    }
  }

  /**
   * On the march toward the enemy, in open order across the block's front. Within striking range or under fire it goes
   * by fire and movement: where the block itself goes slowly, the two teams leapfrog, one rushing to the next cover while
   * the other lies firing; where it goes fast, all advance at the crouch and a rolling few drop to fire a moment, then
   * sprint to catch up. A mass goes in waves, both teams together.
   */
  private advance(sq: Squad, under: boolean): void {
    const u = sq.u, t = this.b.t, N = this.nums(u), h = sq.heading, v = Math.max(0.5, sq.speed);
    const e = this.threat(u), dir = this.threatDir(sq);
    const [hx, hy] = fwd(h);
    const gapE = e ? this.b.gap(u, e) : Infinity, close = gapE < DR.ranges.bound; // within rifle and machine-gun range
    // The drill book: who rushes with whom by the army's school, in turns (one fires, one moves, one follows).
    const book = this.byBook, bound = book ? this.boundFor(sq, N) : 'wave', S = schoolNums(u.nat), bk = this.cycle(sq, N, bound);
    const life = this.life && book;
    // Leapfrog only where a man can run the block's whole cycle of ground in his rush; faster, the rolling advance.
    const vLeap = book ? (CATCH_UP * 0.85 * bk.rush) / bk.cyc : 4.7 * N.rush / (N.rush + N.halt);
    sq.leap = sq.leap ? v < vLeap * 1.1 : v < vLeap * 0.9;
    // At the double (the player's run, release 2): every man up and running with the block, no fire and movement.
    const dbl = u.order?.kind === 'move' && !!u.order.run;
    for (const m of this.up(sq)) {
      let [a, l] = this.slot(sq, m, 'line');
      if (this.life && !VEHICLE.has(m.role) && !CREWED.has(m.role)) { const [da, dl] = this.drift(sq, m); a += da; l += dl; }
      const s = toWorld(u, u.face, a, l);
      if (!under || dbl) {
        m.aim = undefined; m.until = t + 1; m.el = undefined;
        // Squad life: each man comes to the ready at his own distance from the enemy (its scouts first), not the line as one.
        const ready = this.life && e ? this.readyNow(sq, m, gapE) : close;
        this.follow(sq, m, s, dbl ? 'run' : ready ? 'advance' : 'walk');
        continue;
      }
      if (m.role === 'officer' && sq.leap) {
        // The officer goes with whichever team is moving, crouched, a few paces behind it.
        this.follow(sq, m, s, 'creep'); m.aim = undefined; m.el = undefined;
        continue;
      }
      if (sq.leap) {
        // Leapfrog: team 1 and team 2 on alternate rushes (a mass: together). The drill book: in his turn by his army's
        // school, the leaders up first and the rest by pairs after them over a second or so.
        let cyc = N.rush + N.halt, off = N.wave ? 0 : m.team === 2 ? cyc / 2 : 0, rush = N.rush;
        if (book) {
          // Where the squad moves as one body round its gun, the gun goes first, at its head.
          const first = m.lead || (m.j === undefined && m.role === 'rifle') || (bound === 'body' && m.role === 'mg');
          rush = bk.rush; cyc = bk.cyc;
          off = -(this.turnFor(sq, m, bound) * bk.slot + (first ? 0 : 0.3 + (BOOK.stagger - 0.3) * roll(this.seed, u.id, grp(m), Math.floor((m.j ?? 0) / 2), 92)));
        } else off += m.jx * 0.08;
        let at = t + off + 100 * cyc, n = Math.floor(at / cyc);
        let rushing = at - n * cyc < rush;
        if (life) {
          // Squad life: in his squad's turn its buddy teams rise one after another, in a fresh order each cycle, each man
          // his own length of rush: some of a squad are always down firing while its others run (rushOf).
          at = t - this.turnFor(sq, m, bound) * bk.slot + 100 * cyc; n = Math.floor(at / cyc);
          const [d0, len] = this.rushOf(sq, m, n, bk, bound), p0 = at - n * cyc;
          rushing = p0 >= d0 && p0 < d0 + len; rush = len;
        }
        const gain = (s[0] - m.x) * hx + (s[1] - m.y) * hy, behind = gain > 30;
        // To where his place will be halfway through the halt that follows: on average he keeps his place.
        const lead = v * (rush + (cyc - rush) / 2);
        // Already there (he got ahead of his line): he lets this rush go by, firing (never a rush back to his place).
        if (rushing && m.rushed !== n && gain + lead < 3) m.rushed = n;
        // The drill book: his group's heads down under fire, he lets this rush go by where he lies (he catches up after);
        // the gunner and a few keep firing.
        if (book && rushing && m.rushed !== n && !behind && !MOVING.has(m.pose) && this.downed(sq, m)) {
          m.rushed = n; m.v = 0; m.el = 1; m.until = t + 1.5;
          const keep = m.role === 'mg' || roll(this.seed, u.id, m.n, n, 94) < BOOK.suppress.keep;
          m.aim = keep && close && m.role !== 'officer' ? this.aimFor(sq, m, e) : undefined;
          m.pose = m.aim ? this.firePose(m) : 'cower';
          continue;
        }
        const onRush = m.pose === 'advance' || (life && m.pose === 'run');
        if (rushing && m.rushed !== n && m.role !== 'officer' || behind && !onRush && (!life || this.rises(sq, m))) {
          // Up and forward to the next cover, once a cycle (and whenever he has fallen well behind his place).
          this.go(sq, m, [s[0] + hx * lead, s[1] + hy * lead], 'advance', 3, t + rush, dir, 6);
          // Fast enough to be down again before his rush is over: a crouched run, a sprint where he has far to go.
          m.v = clamp(Math.hypot(m.spot[0] - m.x, m.spot[1] - m.y) / (rush * 0.85), 2, behind || book ? CATCH_UP : 5.5);
          // Infiltration (the drill book): where the pace allows, a share of the rushes go at a crouched creep.
          if (book && m.v <= 2.8 && !behind && roll(this.seed, u.id, m.n, n, 93) < S.crawl) m.pose = 'creep';
          // (squad life: one man sprints his rush head up, the next goes bent low: each his own way for a spell)
          else if (life && m.role !== 'officer' && this.upright(sq, m)) m.pose = 'run';
          m.rushed = n; m.aim = undefined; m.el = 2;
        } else if (onRush || m.pose === 'creep') {
          if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1.5 && t < m.until + 1) continue;
          // Down where he got to, firing if anything is in reach of his rifle (the officer watching, directing).
          m.v = 0; m.spot = [m.x, m.y]; m.cover = this.shelter(m.x, m.y, groundOf(this.b.field, m.x, m.y), dir, true, false).kind;
          // The drill book: a share crawl a metre or three aside first (the enemy is aiming where they dropped).
          const to = book && m.role !== 'officer' && roll(this.seed, u.id, m.n, n, 95) < BOOK.aside[0] ? this.aside(sq, m, n) : undefined;
          if (to) { m.spot = to; m.pose = 'crawl'; m.v = 1.2; m.until = t + 2.5; m.aim = undefined; m.el = 1; continue; }
          m.aim = close && m.role !== 'officer' ? this.aimFor(sq, m, e) : undefined;
          m.pose = m.aim ? this.firePose(m) : 'wait'; m.until = t + 1.5; m.el = m.role === 'officer' ? undefined : 1;
        } else if (book && m.pose === 'crawl' && t < m.until && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 0.3) continue;
        else if (t >= m.until && close && m.role !== 'officer') {
          // (squad life: a fresh clip, a look for his target: a moment off his sights, his own)
          const off = life ? this.pauseOf(sq, m) : undefined;
          if (off !== undefined) { m.aim = undefined; m.pose = this.lowPose(m); m.until = off; m.el = 1; }
          else { m.aim = this.aimFor(sq, m, e); m.pose = this.firePose(m); m.until = t + 1.5 + (life ? roll(this.seed, u.id, m.n, Math.round(t * 2), 106) : 0); m.el = 1; }
        }
        // Covering his mates' rushes (the drill book): rapid fire.
        if (book && m.aim && SMALL_ARMS.has(m.pose)) m.rapid = t + 1;
      } else {
        // Rolling: a few at a time drop to fire, then sprint on. The drill book: a turn at a time, so one squad (or the
        // guns) is always down firing while the rest go on at the crouched run.
        const share = 0.3, cyc = 6;
        let ph = book && bound !== 'wave' ? (t - this.turnFor(sq, m, bound) * 2 + 600) % 6 : (t + roll(this.seed, u.id, m.n, 11) * cyc) % cyc;
        let drop = book && bound !== 'wave' ? 2 : cyc * share;
        if (life && bound !== 'wave') {
          // Squad life: his squad's turn to drop comes team by team over DR.life.rush.roll seconds, each man down a spell
          // of his own: never a whole squad down and up as one.
          ph = (ph - this.teamShare(sq, m) * DR.life.rush.roll + 600) % 6;
          drop = 1.2 + roll(this.seed, u.id, m.n, Math.floor((t - this.turnFor(sq, m, bound) * 2 + 600) / 6), 107);
        }
        const gain = (s[0] - m.x) * hx + (s[1] - m.y) * hy, behind = gain > 12;
        if (ph < drop && m.role !== 'officer' && !behind) {
          if (m.pose !== 'fire_prone' && m.pose !== 'lmg_burst' && m.pose !== 'fire_kneel') { m.spot = [m.x, m.y]; m.cover = 'none'; }
          // (squad life: for so short a halt some only drop to a knee, fire and are up again)
          const knee = life && roll(this.seed, u.id, m.n, 110) < DR.life.rush.knee;
          m.v = 0; m.pose = close ? (m.role === 'mg' ? 'lmg_burst' : knee ? 'fire_kneel' : 'fire_prone') : knee ? 'wait' : life ? 'hide' : 'wait'; m.aim = close ? this.aimFor(sq, m, e) : undefined;
          m.el = 1;
          if (book && m.aim) m.rapid = t + 1;
        } else {
          // (squad life: down, he is up again at a moment of his own, not his section in one movement)
          if (life && !this.rises(sq, m)) { m.until = t + 0.5; continue; }
          m.aim = undefined; m.el = m.role === 'officer' ? undefined : 2;
          // (squad life: one left well behind his place sprints upright to make it up; the rest go on at the crouch)
          this.follow(sq, m, s, life && m.role !== 'officer' ? this.gaitOf(sq, m, gain) : 'advance', 1.1);
        }
        m.until = t + 0.5;
      }
    }
  }

  /** Lying where they are, in the best cover near their places, in one pose (the start line: waiting for the guns to lift). */
  private lie(sq: Squad, pose: Pose): void {
    const u = sq.u, t = this.b.t, dir = this.threatDir(sq), near = this.life ? this.nearestGap(u) : 0;
    for (const m of this.up(sq)) {
      if (m.role === 'officer') continue; // he walks his line (officer)
      const [a, l] = this.slot(sq, m, 'line');
      const s = toWorld(u, u.face, a, l);
      // (squad life: each looks for a better place in his own time, never the whole line at once)
      if (this.fresh(sq, m) || (Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 5 + (this.life ? 5 * roll(this.seed, u.id, m.n, 158) : 0) && this.rises(sq, m)) || t >= m.until) this.go(sq, m, s, pose, 3, t + 30 + (this.life ? 25 * roll(this.seed, u.id, m.n, Math.round(t * 2), 108) : 0), dir);
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 1) { m.pose = this.watchPose(sq, m, near, m.role === 'mg' ? 'hide' : pose); m.v = 0; }
      // Not there yet: a run where it is far (off a lorry, a horse), a crawl for the last stretch; each at his own pace,
      // so men going the same way string out and never move as one.
      else { const far = Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 15, r = roll(this.seed, u.id, m.n, 41); m.pose = far ? (this.life && !this.upright(sq, m) ? 'advance' : 'run') : 'creep'; m.v = far ? 3.8 + 1.4 * r : 2.1 + 0.6 * r; }
    }
  }

  /**
   * In a fight. The base of fire (the machine guns and the riflemen not in the assault) lines the block's front in the
   * best cover there, firing, and shifts its place now and then; the assault works forward from cover to cover onto
   * the nearer flank of the block it fights, throws grenades from close range and closes on it as it wavers. A defence
   * on its own ground keeps everyone in the line and fires from its works.
   */
  private fight(sq: Squad): void {
    const u = sq.u, b = this.b, t = b.t, N = this.nums(u), e = this.threat(u), dir = this.threatDir(sq);
    const home = u.side === 'def' && u.reserve !== 'sent' && Math.hypot(u.x - u.home[0], u.y - u.home[1]) < LAND.hold.within;
    const men = this.up(sq);
    const rifles = men.filter((m) => m.role === 'rifle' || m.role === 'sapper').sort((a, c) => (a.team === c.team ? a.k - c.k : c.team - a.team));
    const nA = home ? 0 : Math.round(rifles.length * N.assault * (e && e.state === 'wavering' ? 1.2 : 1));
    const assault = new Set(rifles.slice(0, Math.min(rifles.length, nA)));
    // The flank the assault works onto: the side of its target no friend of ours is already on.
    if (e && (sq.flankOf !== e.id || !sq.flank)) {
      sq.flankOf = e.id;
      const [rx, ry] = right(u.face);
      const friends = b.units.filter((f) => f.side === u.side && f !== u && fighting(f) && f.engaged === e.id);
      const lean = friends.reduce((s, f) => s + Math.sign((f.x - u.x) * rx + (f.y - u.y) * ry), 0);
      sq.flank = lean > 0 ? -1 : lean < 0 ? 1 : roll(this.seed, u.id, e.id) < 0.5 ? -1 : 1;
    }
    const gap = e ? b.gap(u, e) : 999, fresh = sq.since === t;
    for (const m of men) {
      if (m.role === 'officer') continue; // he walks his line (officer)
      const [a, l] = this.slot(sq, m, 'line');
      const s = toWorld(u, u.face, a, l);
      if (assault.has(m) && e) {
        // Rush, then fire, then rush again: each bound 6 to 14 m nearer, onto the flank, never closer than a grenade's throw.
        if (t >= m.until) {
          const rushNow = m.pose !== 'advance' && m.pose !== 'crawl' && m.pose !== 'creep';
          if (rushNow) {
            const [rx, ry] = right(u.face), [fx, fy] = fwd(u.face);
            const wavering = e.state !== 'steady';
            const toE = Math.max(0, gap - (wavering ? 5 : DR.ranges.grenade * 0.7));
            const reach = Math.min(u.d / 2 + Math.min(toE, 38), Math.max(0, (m.slotAt[0] - u.x) * fx + (m.slotAt[1] - u.y) * fy) + 6 + 8 * roll(this.seed, u.id, m.n, t));
            const across = clamp(a + sq.flank * (6 + 6 * roll(this.seed, u.id, m.n, t, 3)), -u.w / 2 - 20, u.w / 2 + 20);
            const p: [number, number] = [u.x + across * rx + reach * fx, u.y + across * ry + reach * fy];
            const pose: Pose = sq.harm > DR.pin.harm * 0.6 ? 'crawl' : wavering ? 'advance' : roll(this.seed, u.id, m.n, t, 5) < 0.3 ? 'creep' : 'advance';
            const pace = pose === 'crawl' ? 1.2 : pose === 'creep' ? 2.5 : 5;
            this.go(sq, m, p, pose, pace, t + N.rush + 1, dir, 8);
            m.aim = undefined;
          } else {
            // Down in cover: fire, or a grenade if their men are close.
            const aim = this.aimAt(m, e), near = aim ? Math.hypot(aim[0] - m.x, aim[1] - m.y) : 999;
            if (near < DR.ranges.grenade && roll(this.seed, u.id, m.n, t, 9) < 0.45) { m.pose = 'throw_grenade'; m.shot = t; m.aim = aim; m.until = t + 1.6; }
            else { m.pose = this.firePose(m); m.aim = aim; m.until = t + N.halt + 1.5 * roll(this.seed, u.id, m.n, t, 4); }
            m.v = 0; m.spot = [m.x, m.y];
          }
        } else if ((m.pose === 'advance' || m.pose === 'creep' || m.pose === 'crawl') && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 1) {
          m.pose = this.firePose(m); m.v = 0; m.aim = this.aimAt(m, e); m.until = Math.max(m.until, t + N.halt);
        }
        continue;
      }
      // The base of fire: its place at the front, in cover (sought afresh as the block halts to fight); a fresh place now and
      // then (a gun that has fired long moves).
      const moved = (this.life ? this.fresh(sq, m) : fresh) || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 6;
      if (moved || t >= m.until + (m.role === 'mg' ? 20 : 30) * (0.6 + 0.8 * roll(this.seed, u.id, m.n, Math.floor(t / 20)))) {
        this.go(sq, m, s, sq.harm > DR.pin.harm * 0.6 ? 'crawl' : 'creep', sq.harm > DR.pin.harm * 0.6 ? 1.2 : 2.5, t + 1, dir);
        m.aim = undefined;
        continue;
      }
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
      m.v = 0;
      if (t < m.until && m.pose !== 'creep' && m.pose !== 'crawl') continue;
      const aim = this.aimAt(m, e), near = aim ? Math.hypot(aim[0] - m.x, aim[1] - m.y) : 999;
      // Heads down a moment when the fire on them is heavy; a grenade when theirs are close; else fire, in bursts.
      const r = roll(this.seed, u.id, m.n, t, 6);
      if (sq.harm > DR.pin.harm * 0.5 && m.role !== 'mg' && r < 0.25) { m.pose = 'cower'; m.aim = undefined; m.until = t + 1.5 + r * 4; }
      else if (near < DR.ranges.grenade && m.role === 'rifle' && r < 0.4) { m.pose = 'throw_grenade'; m.shot = t; m.aim = aim; m.until = t + 1.6; }
      else if (r > 0.88 && m.role !== 'mg') { m.pose = 'wait'; m.aim = undefined; m.until = t + 1.2; }
      else { m.pose = this.firePose(m); m.aim = aim; m.until = t + 3 + 3 * r; }
      // Home in its works it holds the line: never out of the works to chase.
      if (home && m.cover === 'none' && u.dug) m.until = Math.min(m.until, t + 1);
    }
  }

  /**
   * In a fight, by the drill book (tactics.ts). The attack: the base of fire (the platoons not in the assault, and the
   * squads' guns where its army leaves them) lies in the best cover along the block's front firing, rapid while the
   * assault is out, at the enemy's weapons first, and shifts to his far side once its assault is close; the assault
   * (whole squads of the platoons on the flank it works onto) goes forward by its turns, one squad down firing while
   * another rushes, swinging out onto the enemy's flank, grenades from cover once in throwing range, and goes in at the
   * last by its army's drill as he wavers: grenades and in, walking forward firing, or the rush with a shout. Each
   * group's heads go down under the fire landing near it, the gunner and a few firing on. The defence on its own
   * ground holds its fire till the enemy is in its weapons' reach, then all fire at once; with the enemy at its line,
   * its rear platoons counter-attack to their old line.
   */
  private fightBook(sq: Squad): void {
    const u = sq.u, b = this.b, t = b.t, N = this.nums(u), e = this.threat(u), dir = this.threatDir(sq), band = this.band(u);
    const S = schoolNums(u.nat), bound = this.boundFor(sq, N), bk = this.cycle(sq, N, bound);
    const home = u.side === 'def' && u.reserve !== 'sent' && Math.hypot(u.x - u.home[0], u.y - u.home[1]) < LAND.hold.within;
    const men = this.up(sq), gap = e ? b.gap(u, e) : 999, fresh = sq.since === t, wavering = !!e && e.state !== 'steady';
    const [rx, ry] = right(u.face), [fx, fy] = fwd(u.face), G = DR.ranges.grenade;
    // The flank the assault works onto: the side of its target no friend of ours is already on.
    if (e && (sq.flankOf !== e.id || !sq.flank)) {
      sq.flankOf = e.id;
      const friends = b.units.filter((f) => f.side === u.side && f !== u && fighting(f) && f.engaged === e.id);
      const lean = friends.reduce((n, f) => n + Math.sign((f.x - u.x) * rx + (f.y - u.y) * ry), 0);
      sq.flank = lean > 0 ? -1 : lean < 0 ? 1 : roll(this.seed, u.id, e.id) < 0.5 ? -1 : 1;
    }
    // Who goes: the attack's assault; a defence's counter-attack (its rear platoons, once the enemy has been at its line
    // a while), only while it is steady.
    let assault = new Set<Man>();
    if (!home && e) {
      const rifles = men.filter((m) => m.role === 'rifle' || m.role === 'sapper').length;
      assault = this.flankPlatoons(sq, men, Math.round(rifles * N.assault * (wavering ? 1.2 : 1)), S.gunStays);
    } else if (home) {
      if (e && gap < BOOK.counter.at) sq.closeAt ??= t;
      else if (!e || gap > BOOK.counter.at * 2) sq.closeAt = undefined;
      if (S.counter >= 0 && sq.closeAt !== undefined && t - sq.closeAt >= S.counter * BOOK.counter.xp[band] && u.state === 'steady') assault = this.rearPlatoons(sq, men);
    }
    // The base shifts its fire to the enemy's far side once the assault is within reach of its own rounds.
    let near = Infinity;
    if (e && !home) for (const m of assault) near = Math.min(near, this.toBox(m, e));
    const away: [number, number] | undefined = e && near < BOOK.lift.mg + 15 ? [e.x + fx * e.d / 2, e.y + fy * e.d / 2] : undefined;
    const out = assault.size > 0;
    // Smoke on the enemy's flank as the assault goes out (its platoons' own light mortars), a round a think.
    // With smoke on call (release 2, ruling 242 S6) the screens are the rules' (screen): none of the book's own.
    if (BOOK.smoke.on && !b.smoke && !home && e && out && S.smoke > 0 && (sq.smokeAt === undefined || t - sq.smokeAt >= BOOK.smoke.every)) { sq.smokeAt = t; sq.smokeLeft = S.smoke; }
    if (sq.smokeLeft && e && !home && !b.smoke) { sq.smokeLeft--; this.smoke(sq, e, men.filter((m) => !assault.has(m)), S.smoke - sq.smokeLeft - 1, S.smoke); }
    for (const m of men) {
      if (m.role === 'officer') continue; // he walks his line (officer)
      const [a, l] = this.slot(sq, m, 'line');
      const s = toWorld(u, u.face, a, l);
      const lv = this.supOf(sq, m), down = lv > BOOK.suppress.down;
      if (assault.has(m) && e && !home) { this.assaultMan(sq, m, e, bound, bk, S.final, S.crawl, lv, wavering, dir); continue; }
      if (assault.has(m) && home) {
        // The counter-attack: up and forward to their old line, firing from it.
        const p = toWorld(u, u.face, a, l + BOOK.counter.ahead);
        if (Math.hypot(p[0] - m.slotAt[0], p[1] - m.slotAt[1]) > 3) { this.go(sq, m, p, 'advance', 4.5, t + 3, dir); m.aim = undefined; m.el = 2; continue; }
        if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
        m.v = 0; m.el = 2;
        if (t >= m.until) { m.aim = this.aimFor(sq, m, e); m.pose = m.aim ? this.firePose(m) : 'wait'; m.until = t + 2 + 2 * roll(this.seed, u.id, m.n, t, 6); }
        continue;
      }
      // The base of fire: its place at the front, in cover (sought afresh as the block halts to fight); a fresh place now
      // and then (a gun that has fired long moves); those caught in the open under heavy fire crawl for cover.
      const first = this.life ? this.fresh(sq, m) : fresh;
      // (squad life: as the block shifts, each man shifts once his own place has moved his own distance, at his own moment)
      const moved = first || (Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 6 + (this.life ? 6 * roll(this.seed, u.id, m.n, 158) : 0) && (first || this.rises(sq, m)));
      if (moved || t >= m.until + (m.role === 'mg' ? 20 : 30) * (0.6 + 0.8 * roll(this.seed, u.id, m.n, Math.floor(t / 20)))) {
        this.go(sq, m, s, down ? 'crawl' : 'creep', down ? 1.2 : 2.5, t + 1, dir);
        // (squad life: one with far to go doubles to it at the crouch, the rest creep in, each at his own pace)
        if (this.life && !down) { const far = Math.hypot(m.spot[0] - m.x, m.spot[1] - m.y) > DR.life.rush.sprint * 2; m.pose = far ? (this.upright(sq, m) ? 'run' : 'advance') : 'creep'; m.v = (far ? 4 : 2.1) + 0.8 * roll(this.seed, u.id, m.n, 119); }
        m.aim = undefined; m.el = 1;
        continue;
      }
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
      m.v = 0; m.el = home && !out ? undefined : 1;
      if (t < m.until && m.pose !== 'creep' && m.pose !== 'crawl') continue;
      const r = roll(this.seed, u.id, m.n, t, 6);
      // Home in its works: its fire held till the enemy is within its weapons' reach (then all at once).
      if (home && e && gap > (S.hold ?? (m.role === 'mg' ? BOOK.hold.mg : BOOK.hold.rifle[band]))) { m.pose = this.watchPose(sq, m, gap, m.role === 'mg' ? 'hide' : 'wait'); m.aim = undefined; m.until = t + 1; continue; }
      const aim = this.aimFor(sq, m, e, away), dn = aim ? Math.hypot(aim[0] - m.x, aim[1] - m.y) : 999;
      // Heads down while the fire on his group lasts, the gunner and a few firing on; a grenade when theirs are close;
      // else fire, rapid while his mates are out.
      if (down && m.role !== 'mg' && roll(this.seed, u.id, m.n, Math.floor(t / 2), 94) >= BOOK.suppress.keep) { m.pose = 'cower'; m.aim = undefined; m.until = t + 1 + r * 1.5; }
      else if (dn < G && m.role === 'rifle' && r < 0.4) { m.pose = 'throw_grenade'; m.shot = t; m.aim = aim; m.until = t + 1.6; }
      else if (r > 0.9 && m.role !== 'mg' && !out) { m.pose = 'wait'; m.aim = undefined; m.until = t + 1.2; }
      else {
        // (squad life: a fresh clip, a look for his target: a moment off his sights, his own)
        const off = aim ? this.pauseOf(sq, m) : undefined;
        if (off !== undefined) { m.pose = this.lowPose(m); m.aim = undefined; m.until = off; }
        else { m.pose = aim ? this.firePose(m) : 'wait'; m.aim = aim; m.until = t + 2 + 2.5 * r; if (out && aim) m.rapid = m.until; }
      }
      if (home && m.cover === 'none' && u.dug) m.until = Math.min(m.until, t + 1);
    }
  }

  /**
   * Where a man just down crawls aside to (the drill book: the enemy is aiming where he dropped): a metre or three to
   * one side, the other where a mate lies there; none where both are taken.
   */
  private aside(sq: Squad, m: Man, n: number): [number, number] | undefined {
    const u = sq.u, [rx, ry] = right(u.face), A = BOOK.aside, stand = this.memo(sq).stand;
    const d = A[1] + (A[2] - A[1]) * roll(this.seed, u.id, m.n, n, 97), s0 = roll(this.seed, u.id, m.n, n, 96) < 0.5 ? -1 : 1;
    for (const sd of [s0, -s0]) {
      const p = this.leash(sq, [m.x + rx * d * sd, m.y + ry * d * sd], -4);
      if (!stand.some((o) => o !== m && Math.hypot(o.spot[0] - p[0], o.spot[1] - p[1]) < 1.6 && Math.hypot(o.x - p[0], o.y - p[1]) < 3)) return p;
    }
    return undefined;
  }

  /**
   * One smoke round (the drill book, while book.smoke.on) from a man of the base of fire (the platoon's headquarters
   * first: its light mortar) onto the enemy's front corner on the assault's flank, the screen's rounds side by side
   * along his front (i of n). No harm: it only lands (the 3D draws the screen, drifting with the battle's wind).
   */
  private smoke(sq: Squad, e: LandUnit, base: Man[], i: number, n: number): void {
    const u = sq.u, b = this.b, t = b.t, R = DR.rounds;
    const squads = platoonOf(u.nat).squads, by = base.find((m) => (m.sub ?? 0) >= squads && m.role === 'rifle') ?? base.find((m) => m.role === 'rifle');
    if (!by) return;
    // His front corner on our flank, along his own front (he may stand askew to us).
    const [rx, ry] = right(u.face), [ex, ey] = right(e.face), [gx, gy] = fwd(e.face), z = Math.round(t * 10);
    const side = (ex * rx + ey * ry >= 0 ? 1 : -1) * sq.flank, front = (gx * (u.x - e.x) + gy * (u.y - e.y) >= 0 ? 1 : -1) * e.d / 2;
    const along = side * e.w * 0.4 + (i - (n - 1) / 2) * BOOK.smoke.spread;
    const ax = e.x + ex * along + gx * front, ay = e.y + ey * along + gy * front;
    const range = Math.hypot(ax - by.x, ay - by.y), [i0, i1] = R.indirect.mortar, sg = i0 + i1 * range;
    const x = clamp(ax + gauss(roll(this.seed, u.id, by.n, z, 87), roll(this.seed, u.id, by.n, z, 88)) * sg * 0.35, 1, b.field.w - 1);
    const y = clamp(ay + gauss(roll(this.seed, u.id, by.n, z, 89), roll(this.seed, u.id, by.n, z, 90)) * sg * 0.35, 1, b.field.h - 1);
    const r: Round = { t: Math.round(t * 10) / 10, l: Math.round((t + range / R.speed.mortar) * 10) / 10, by: u.id, k: by.k, w: 'smoke', on: e.id, x: Math.round(x * 10) / 10, y: Math.round(y * 10) / 10, r: 'miss' };
    if (by.j !== undefined) r.j = by.j;
    this.book(r);
    by.v = 0; by.spot = [by.x, by.y]; by.pose = 'wait'; by.aim = [ax, ay]; by.until = t + 1.5;
  }

  /**
   * Smoke on call (release 2, ruling 242 S6): a screen the rules just laid (LandBattle.lay), drawn as the rounds that lay
   * it, one round for each BOOK.smoke.per metres of its line (at most BOOK.smoke.most; looks only), one a think from the
   * block's base of fire, as the book's own smoke is fired (smoke). A screen laid while one is still being drawn takes its
   * place.
   */
  screen(s: Screen): void {
    const sq = this.squads.get(s.unit);
    if (!sq) return;
    const n = clamp(Math.ceil(Math.hypot(s.x1 - s.x0, s.y1 - s.y0) / BOOK.smoke.per), 1, BOOK.smoke.most);
    sq.screenQ = { s, i: 0, n };
  }

  /**
   * The next round of the screen being drawn: from the platoon's headquarters (its light mortar) or a rifleman standing,
   * else from the block's front, onto its place along the line ((i + 1/2) of n), scattered a little as a mortar's round;
   * it hangs from its landing to the screen's end (hang). No harm, at no block (on -1). The shooter's stamp is the round's
   * only change to his drill: he stands and fires it.
   */
  private screenRound(sq: Squad): void {
    const q = sq.screenQ!, u = sq.u, b = this.b, t = b.t, R = DR.rounds, s = q.s;
    if (q.i >= q.n || !live(u) || t >= s.until) { delete sq.screenQ; return; }
    const men = this.up(sq), squads = platoonOf(u.nat).squads;
    const by = men.find((m) => (m.sub ?? 0) >= squads && m.role === 'rifle') ?? men.find((m) => m.role === 'rifle');
    const k = (q.i + 0.5) / q.n, ax = s.x0 + (s.x1 - s.x0) * k, ay = s.y0 + (s.y1 - s.y0) * k, z = Math.round(t * 10);
    const [fx, fy] = fwd(u.face), ox = by ? by.x : u.x + (fx * u.d) / 2, oy = by ? by.y : u.y + (fy * u.d) / 2, n = by ? by.n : 0;
    const range = Math.hypot(ax - ox, ay - oy), [i0, i1] = R.indirect.mortar, sg = i0 + i1 * range;
    const x = clamp(ax + gauss(roll(this.seed, u.id, n, z, 81), roll(this.seed, u.id, n, z, 82)) * sg * 0.35, 1, b.field.w - 1);
    const y = clamp(ay + gauss(roll(this.seed, u.id, n, z, 83), roll(this.seed, u.id, n, z, 84)) * sg * 0.35, 1, b.field.h - 1);
    const l = Math.round((t + range / R.speed.mortar) * 10) / 10;
    const r: Round = { t: Math.round(t * 10) / 10, l, by: u.id, w: 'smoke', on: -1, x: Math.round(x * 10) / 10, y: Math.round(y * 10) / 10, r: 'miss', hang: Math.max(0, Math.round((s.until - l) * 10) / 10) };
    if (by) { r.k = by.k; if (by.j !== undefined) r.j = by.j; }
    this.book(r);
    if (by) { by.v = 0; by.spot = [by.x, by.y]; by.pose = 'wait'; by.aim = [ax, ay]; by.until = t + 1.5; }
    if (++q.i >= q.n) delete sq.screenQ;
  }

  /** Whether a mate lies within arm's length of him, down. */
  private crowded(sq: Squad, m: Man): boolean {
    return this.memo(sq).stand.some((o) => o !== m && Math.abs(o.x - m.x) < 0.9 && Math.abs(o.y - m.y) < 0.9 && o.v === 0);
  }

  /** Metres from a man to a block's ground (0 inside it). */
  private toBox(m: Man, e: LandUnit): number {
    const [rx, ry] = right(e.face), [fx, fy] = fwd(e.face), dx = m.x - e.x, dy = m.y - e.y;
    return Math.hypot(Math.max(0, Math.abs(dx * rx + dy * ry) - e.w / 2), Math.max(0, Math.abs(dx * fx + dy * fy) - e.d / 2));
  }

  /**
   * One man of the assault (fightBook): in his turn he rushes, 10 to 20 m, swinging out onto the enemy's flank, and is
   * down again firing (a share crawling a metre or three aside first); in throwing range of a steady enemy he lies
   * throwing grenades and firing; once the enemy wavers he goes in by his army's drill. Under fire his group's heads go
   * down and its rushes wait.
   */
  private assaultMan(sq: Squad, m: Man, e: LandUnit, bound: Bound, bk: { rush: number; slot: number; cyc: number }, final: 'grenade' | 'marching' | 'charge', crawl: number, lv: number, wavering: boolean, dir: [number, number]): void {
    const u = sq.u, t = this.b.t, G = DR.ranges.grenade, [rx, ry] = right(u.face), [fx, fy] = fwd(u.face);
    m.el = 2;
    if (MOVING.has(m.pose) && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1 && t < m.until + 1) return;
    const dE = this.toBox(m, e), down = lv > BOOK.suppress.down;
    // Where he is going: his own place in the line swung out onto the flank (BOOK.flank.out), each man keeping his
    // room in it; forward to within a grenade's throw of the enemy's near edge.
    const [a] = this.slot(sq, m, 'line'), mx = m.x - u.x, my = m.y - u.y;
    const da = a + sq.flank * BOOK.flank.out - (mx * rx + my * ry), edge = (e.x - m.x) * fx + (e.y - m.y) * fy - e.d / 2;
    if (wavering && !down) {
      // In: the enemy wavers. Grenades first from cover, then the rush; walking forward firing; or the rush with a shout.
      const sa = clamp(da, -8, 8), p: [number, number] = [m.x + rx * sa + fx * (edge + 4), m.y + ry * sa + fy * (edge + 4)];
      if (final === 'grenade' && dE < G && m.pose !== 'throw_grenade' && (m.shot === undefined || t - m.shot > 6) && roll(this.seed, u.id, m.n, Math.floor(t / 3), 98) < 0.5) {
        m.v = 0; m.spot = [m.x, m.y]; m.pose = 'throw_grenade'; m.shot = t; m.aim = this.aimFor(sq, m, e); m.until = t + 1.6;
        return;
      }
      if (t < m.until && m.pose === 'throw_grenade') return;
      if (final === 'marching') {
        // A shot every two or three paces, walking.
        if (m.pose === 'fire_stand' && t < m.until) return;
        if (m.pose !== 'walk' || t >= m.until) {
          if (m.pose === 'walk') { m.v = 0; m.spot = [m.x, m.y]; m.pose = 'fire_stand'; m.aim = this.aimFor(sq, m, e); m.until = t + 0.6; m.rapid = t + 1; return; }
          this.go(sq, m, p, 'walk', 1.6, t + 1.2 + 0.6 * roll(this.seed, u.id, m.n, t, 99), dir, 4); m.aim = undefined;
        }
        return;
      }
      this.go(sq, m, p, this.life && this.upright(sq, m) ? 'run' : 'advance', final === 'charge' ? CATCH_UP : 5.5, t + 4, dir, 4); m.aim = undefined;
      return;
    }
    // His turn in the cycle, the leaders first, the rest by pairs after them.
    const first = m.lead || (m.j === undefined && m.role === 'rifle') || (bound === 'body' && m.role === 'mg');
    const off = -(this.turnFor(sq, m, bound) * bk.slot + (first ? 0 : 0.3 + (BOOK.stagger - 0.3) * roll(this.seed, u.id, grp(m), Math.floor((m.j ?? 0) / 2), 92)));
    let at = t + off + 100 * bk.cyc, n = Math.floor(at / bk.cyc), rushing = at - n * bk.cyc < bk.rush, rush = bk.rush;
    if (this.life) {
      // Squad life: his squad's buddy teams rise one after another in its turn, each man his own length of rush (rushOf).
      at = t - this.turnFor(sq, m, bound) * bk.slot + 100 * bk.cyc; n = Math.floor(at / bk.cyc);
      const [d0, len] = this.rushOf(sq, m, n, bk, bound), p0 = at - n * bk.cyc;
      rushing = p0 >= d0 && p0 < d0 + len; rush = len;
    }
    const dl = edge - G * 0.7;
    if (rushing && m.rushed !== n && !down && dl > 2) {
      // Up and on: out to the flank where it is far, then in; a crouched creep where the army infiltrates, a crawl
      // under heavy fire.
      const F = BOOK.flank.rush, swing = F[0] + (F[1] - F[0]) * roll(this.seed, u.id, m.n, n, 3), step = 10 + 10 * roll(this.seed, u.id, m.n, n, 4);
      const sa = clamp(da, -swing, swing), sl = clamp(dl, 0, Math.max(3, step - Math.abs(sa) * 0.5));
      const heavy = lv > BOOK.suppress.heavy, slow = heavy || roll(this.seed, u.id, m.n, n, 93) < crawl;
      const d = Math.hypot(sa, sl), pace = heavy ? 1.2 : slow ? 2.6 : clamp(d / (rush * 0.85), 3.5, CATCH_UP);
      this.go(sq, m, [m.x + rx * sa + fx * sl, m.y + ry * sa + fy * sl], heavy ? 'crawl' : slow ? 'creep' : this.life && this.upright(sq, m) ? 'run' : 'advance', pace, t + (slow ? d / pace : rush), dir, 6);
      m.rushed = n; m.aim = undefined;
      return;
    }
    if (rushing && m.rushed !== n) m.rushed = n; // heads down (or there already): he lets this rush go by
    if (MOVING.has(m.pose)) {
      // Down where he got to; a share crawl a metre or three aside first (the enemy is aiming where they dropped).
      m.v = 0; m.spot = [m.x, m.y]; m.cover = this.shelter(m.x, m.y, groundOf(this.b.field, m.x, m.y), dir, true, false).kind;
      const to = m.pose !== 'crawl' && roll(this.seed, u.id, m.n, n, 95) < BOOK.aside[0] ? this.aside(sq, m, n) : undefined;
      if (to) { m.spot = to; m.pose = 'crawl'; m.v = 1.2; m.until = t + 2.5; m.aim = undefined; return; }
      m.until = t;
    }
    if (t < m.until) return;
    // Down on top of a mate (two men making for one fold of ground): he crawls aside.
    const to = this.crowded(sq, m) ? this.aside(sq, m, Math.round(t * 2)) : undefined;
    if (to) { m.spot = to; m.pose = 'crawl'; m.v = 1.2; m.until = t + 2; m.aim = undefined; return; }
    // Down: heads down under fire (the gunner and a few fire on); grenades in throwing range; else fire, rapid while
    // his mates rush.
    const aim = this.aimFor(sq, m, e), dn = aim ? Math.hypot(aim[0] - m.x, aim[1] - m.y) : 999, r = roll(this.seed, u.id, m.n, t, 9);
    m.v = 0; m.spot = [m.x, m.y];
    if (down && m.role !== 'mg' && roll(this.seed, u.id, m.n, Math.floor(t / 2), 94) >= BOOK.suppress.keep) { m.pose = 'cower'; m.aim = undefined; m.until = t + 1 + r * 1.5; }
    else if (dn < G && m.role !== 'mg' && r < (final === 'grenade' ? 0.45 : 0.3)) { m.pose = 'throw_grenade'; m.shot = t; m.aim = aim; m.until = t + 1.6; }
    else {
      // (squad life: a fresh clip, a look for his target: a moment off his sights, his own)
      const off2 = aim ? this.pauseOf(sq, m) : undefined;
      if (off2 !== undefined) { m.pose = this.lowPose(m); m.aim = undefined; m.until = off2; return; }
      m.pose = aim ? this.firePose(m) : 'wait'; m.aim = aim; m.until = t + 1 + 1.5 * r; if (aim) m.rapid = m.until;
    }
  }

  /** Holding its ground, watching: in the best cover along its front, a sentry up at each end, the rest ready. */
  private hold(sq: Squad): void {
    const u = sq.u, t = this.b.t, dir = this.threatDir(sq), near = this.nearestGap(u);
    for (const m of this.up(sq)) {
      if (m.role === 'officer') continue; // he walks his line (officer)
      const [a, l] = this.slot(sq, m, 'line');
      const s = toWorld(u, u.face, a, l);
      if (this.fresh(sq, m) || (Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 5 + (this.life ? 5 * roll(this.seed, u.id, m.n, 158) : 0) && this.rises(sq, m)) || (m.v > 0 && t >= m.until)) { this.go(sq, m, s, 'creep', 2.5, t + 1, dir); continue; }
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
      m.v = 0; m.aim = undefined;
      if (m.role === 'at' && m.hidden) continue;
      m.pose = this.watchPose(sq, m, near, m.role === 'mg' ? 'hide' : near < 600 ? 'wait' : roll(this.seed, u.id, m.n, Math.floor(t / 8)) < 0.3 ? 'idle' : 'wait');
    }
  }

  /** A reserve resting out of the fight: in knots, in cover, ready; the headquarters at the middle. */
  private rest(sq: Squad): void {
    const u = sq.u, t = this.b.t, dir = this.threatDir(sq);
    for (const m of this.up(sq)) {
      const [a, l] = this.slot(sq, m, 'rest');
      const s = toWorld(u, u.face, a, l);
      if (this.fresh(sq, m) || (Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 5 + (this.life ? 5 * roll(this.seed, u.id, m.n, 158) : 0) && this.rises(sq, m)) || (m.v > 0 && t >= m.until)) { this.go(sq, m, s, 'walk', 2, t + 1, dir); continue; }
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
      m.v = 0; m.aim = undefined;
      m.pose = (m.k % 5 === 1) ? 'wait' : roll(this.seed, u.id, m.n, Math.floor(t / 10)) < 0.5 ? 'idle' : 'wait';
      // (squad life: resting, each as he pleases: standing easy, on a knee, lying down, a sentry or two watching)
      if (this.life) m.pose = this.watchPose(sq, m, Infinity, m.pose);
    }
  }

  /**
   * Pinned: losing men fast, shelled, or wavering under fire. Down in cover; those caught in the open crawl to the nearest
   * shelter; about a third keep their heads down, the rest fire back, the machine guns hardest.
   */
  private pinned(sq: Squad): void {
    const u = sq.u, t = this.b.t, e = this.threat(u), dir = this.threatDir(sq);
    for (const m of this.up(sq)) {
      if (m.v > 0 && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1 && m.pose === 'crawl') continue;
      const here = this.shelter(m.x, m.y, groundOf(this.b.field, m.x, m.y), dir, true, false);
      // Caught in the open he looks for shelter once his moment's bit (a burst, a look round) is done.
      if (here.v < 0.2 && m.role !== 'officer' && (t >= m.until || m.pose === 'crawl')) {
        const best = this.seek(sq, m, [m.x, m.y], dir, sq.men, 14);
        if (best.v > here.v + 0.2) { m.spot = [best.x, best.y]; m.cover = best.kind; m.pose = 'crawl'; m.v = 1.2; m.until = t + 6; m.aim = undefined; continue; }
      }
      m.spot = [m.x, m.y]; m.cover = here.kind; m.v = 0;
      if (t < m.until && m.pose !== 'crawl') continue;
      const r = roll(this.seed, u.id, m.n, t, 8);
      if (m.role === 'mg') { m.pose = 'lmg_burst'; m.aim = this.aimAt(m, e); m.until = t + 2 + r * 2; }
      else if (m.role === 'officer') { m.pose = 'wait'; m.aim = undefined; m.until = t + 2; }
      else if (r < 0.35) { m.pose = 'cower'; m.aim = undefined; m.until = t + 2 + r * 4; }
      else { m.pose = m.cover === 'works' || m.cover === 'building' ? 'fire_kneel' : 'fire_prone'; m.aim = this.aimAt(m, e); m.until = t + 2 + r * 3; }
    }
  }

  /**
   * Falling back in order (a withdrawal, a block worn out at the brink, an army carried off by its break): by bounds,
   * one team facing the enemy and firing while the other runs back past it; at the double, a rolling rearguard.
   */
  private fallback(sq: Squad): void {
    const u = sq.u, t = this.b.t, N = this.nums(u), e = this.threat(u), dir = this.threatDir(sq), v = Math.max(1, sq.speed), h = sq.heading;
    const vLeap = 4.7 * N.rush / (N.rush + N.halt);
    sq.leap = sq.leap ? v < vLeap * 1.1 : v < vLeap * 0.9;
    for (const m of this.up(sq)) {
      const [a, l] = this.slot(sq, m, 'line');
      const s = toWorld(u, u.face, a, l);
      const cyc = N.rush + N.halt;
      // Squad life: its buddy teams turn about (one team down and facing the enemy as the next runs back past it), each
      // man his own moment; else its two halves.
      const ph = (t + (this.life ? this.teamShare(sq, m) * cyc : (m.team === 2 ? cyc / 2 : 0) + m.jx * 0.08) + 100 * cyc) % cyc;
      const cover = sq.leap ? ph >= N.rush : roll(this.seed, u.id, m.n, Math.floor(t / 2)) < 0.2;
      if (cover && m.role !== 'officer' && e && this.b.gap(u, e) < 500) { m.v = 0; m.spot = [m.x, m.y]; m.pose = m.role === 'mg' ? 'lmg_burst' : this.life && roll(this.seed, u.id, m.n, 109) < 0.35 ? 'fire_prone' : 'fire_kneel'; m.aim = this.aimAt(m, e); }
      else { m.aim = undefined; this.follow(sq, m, s, 'run', 1.15); }
      m.until = t + 0.5;
      void dir;
    }
  }

  /** Routing: every man for himself, running for his own edge, spreading as he goes. */
  private flee(sq: Squad): void {
    const u = sq.u, t = this.b.t, back = u.side === 'att' ? Math.PI : 0, v = Math.max(2, sq.speed);
    for (const m of this.up(sq)) {
      const spread = Math.min(1, (t - sq.since) / 15);
      const a = back + (roll(this.seed, u.id, m.n, 13) - 0.5) * 0.9 * spread;
      const lag = (roll(this.seed, u.id, m.n, 14) - 0.5) * 30 * spread;
      const across = (m.jx * 2 + (roll(this.seed, u.id, m.n, 15) - 0.5) * u.w) * (0.5 + spread * 0.7);
      const [rx, ry] = right(u.face);
      const s: [number, number] = [u.x + across * rx + Math.sin(a) * (10 + lag), u.y + across * ry + Math.cos(a) * (10 + lag)];
      m.aim = undefined; m.hidden = undefined; m.until = t + 0.5;
      // Squad life: every man for himself, and each his own way of it: most in headlong flight, some running upright
      // with their rifles, some bent low, each for a spell of his own.
      let gait: Pose = 'flee';
      if (this.life && !VEHICLE.has(m.role) && !CREWED.has(m.role)) {
        const T = 8 + 8 * roll(this.seed, u.id, m.n, 161), r = roll(this.seed, u.id, m.n, Math.floor(t / T + roll(this.seed, u.id, m.n, 162)), 163), F = DR.life.rout;
        gait = r < F[0] ? 'flee' : r < F[0] + F[1] ? 'run' : 'advance';
      }
      this.follow(sq, m, s, gait, 1.1);
    }
  }

  /** Rallied: the men gather on their officer, facing the enemy again. */
  private rally(sq: Squad): void {
    const u = sq.u, t = this.b.t, dir = this.threatDir(sq);
    const men = this.up(sq), lead = men.find((m) => m.role === 'officer') ?? men[0];
    if (!lead) return;
    lead.v = 0; lead.spot = [lead.x, lead.y]; lead.pose = 'idle'; lead.aim = undefined;
    const back = Math.atan2(-dir[0], -dir[1]);
    men.forEach((m, i) => {
      if (m === lead) return;
      let ang = (i / men.length) * Math.PI * 2, r = sq.tier >= 1 ? 7 + 0.25 * men.length : 7, pace = 4;
      if (this.life) {
        // Squad life: no ring on parade. They gather in a loose crescent behind their officer, between him and their own
        // rear, each at his own distance and his own pace, kneeling or lying as he gets there, facing the enemy again.
        ang = back + ((i + roll(this.seed, u.id, m.n, 121)) / men.length - 0.5) * 3.6;
        r = 3 + (r + 3) * Math.sqrt(roll(this.seed, u.id, m.n, 122));
        pace = 3 + 2.2 * roll(this.seed, u.id, m.n, 123);
      }
      const p: [number, number] = [lead.x + Math.sin(ang) * r + dir[0] * 4, lead.y + Math.cos(ang) * r + dir[1] * 4];
      if (Math.hypot(m.spot[0] - p[0], m.spot[1] - p[1]) > 4) this.go(sq, m, p, 'run', pace, t + 2, dir, 4);
      else if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 1) { m.v = 0; m.pose = this.life && m.role !== 'mg' && roll(this.seed, u.id, m.n, 124) < 0.4 ? 'hide' : 'wait'; }
    });
  }

  /** Lorried infantry still aboard, cavalry still mounted: they ride where their block goes. */
  private mounted(sq: Squad): void {
    const u = sq.u, t = this.b.t;
    if (u.kind === 'mot') {
      sq.veh.forEach((v, i) => {
        // (tank minds: each her own place in the column, and her own moment to move off: mindLean, mindOff)
        const s0 = toWorld(u, sq.heading, 0, u.d / 2 - 8 - i * 18), s = this.tankmind ? this.mindLean(sq, v, s0, sq.heading, i) : s0;
        this.follow(sq, v, s, 'drive'); v.until = t + 1;
        if (this.tankmind) this.mindOff(sq, v, i === 0);
      });
      for (const m of this.up(sq)) { m.pose = 'mounted'; m.aim = undefined; m.cover = 'none'; } // aboard: carried in his lorry (walk)
      return;
    }
    for (const m of this.up(sq)) {
      const [a, l] = this.slot(sq, m, sq.phase === 'column' ? 'column' : 'line');
      const s = toWorld(u, sq.phase === 'column' ? sq.heading : u.face, a, l);
      m.aim = undefined; m.until = t + 1;
      this.follow(sq, m, s, 'ride');
      if (m.v === 0) m.pose = 'ride'; // mounted, standing
    }
  }

  /**
   * Off the lorries and horses where they are, each on his feet: the block's phase then sends him to his place. With Men
   * in lorries on, lorried men get down over their lorry's tailboard one at a time instead, the back of her bed first and
   * the two benches turn about (hopOut), the lorries standing (Brian, 23 November 14:10Z: "soldiers in the back of a
   * truck and they then hop out and start engaging enemies").
   */
  private alight(sq: Squad): void {
    sq.boarding = undefined;
    const t = this.b.t, aboard: Man[] = [];
    for (const m of sq.men) {
      if (m.fell !== undefined || (m.pose !== 'mounted' && m.pose !== 'ride')) continue;
      if (this.b.trucks && m.pose === 'mounted' && sq.veh.length) { aboard.push(m); continue; }
      m.pose = 'wait'; m.v = 0; m.spot = [m.x, m.y]; m.aim = undefined; m.cover = 'none';
    }
    if (!aboard.length) return;
    const by = new Map<number, Man[]>();
    for (const m of aboard) { const i = this.lorryIx(sq, m), l = by.get(i); if (l) l.push(m); else by.set(i, [m]); }
    // (a man whose bench is the one the man before him came down from waits a little longer: HOP.same, so the two are
    // never on one spot behind her, the first still finding his feet; the benches take turns where both have men)
    for (const l of by.values()) {
      let at = t, was = -1;
      for (const m of l.sort((a, c) => this.seatNo(sq, c) - this.seatNo(sq, a))) {
        const j = this.seatNo(sq, m);
        if (was >= 0) at += was % 2 === j % 2 ? HOP.same : HOP.hop;
        m.out = at; m.off = undefined; was = j;
      }
    }
    sq.hop = t; sq.left = undefined;
    for (const v of sq.veh) { v.spot = [v.x, v.y]; v.slotAt = v.spot; v.pose = 'parked'; v.v = 0; }
  }

  /**
   * Men in lorries, getting down (alight): each man, his turn come, gets up and makes for the tailboard (off: the 3D game
   * plays it from here, his seat said with a 1, MenOut.sv), and HOP.takes later is on the ground behind his lorry, off at
   * a run to clear her tail (the left bench's men to her left, the right's to her right, the back benches' nearest), to
   * kneel there till the last of the block is down; the lorries stand meanwhile. True once every man is down.
   */
  private hopOut(sq: Squad): boolean {
    const t = this.b.t;
    let still = false;
    for (const v of sq.veh) { v.v = 0; v.pose = 'parked'; v.spot = [v.x, v.y]; }
    for (const m of sq.men) {
      if (m.out === undefined) continue;
      if (m.fell !== undefined || m.pose !== 'mounted' || !sq.veh.length) { m.out = undefined; m.off = undefined; continue; }
      const most = t - (sq.hop ?? t) >= HOP.most;
      if (t >= m.out && !m.off) m.off = true;
      if (t < m.out + HOP.takes && !most) { still = true; continue; }
      m.out = undefined; m.off = undefined;
      const v = this.lorryOf(sq, m), j = this.seatNo(sq, m), side = j % 2 ? 1 : -1, row = Math.floor(j / 2);
      const vf = v.roadF ?? v.face;
      [m.x, m.y] = this.landing(sq, m);
      m.face = wrap(vf + Math.PI); m.went = 0;
      m.spot = toWorld(v, vf, side * (1.6 + 0.5 * row), -HOP.tail - 1.5 - 0.4 * row); m.slotAt = m.spot;
      m.pose = 'run'; m.v = 3.5; m.aim = undefined; m.hidden = undefined; m.cover = 'none'; m.until = t + 1;
    }
    for (const m of sq.men) if (m.fell === undefined && m.out === undefined && !m.job && MOVING.has(m.pose) && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 0.3) this.arrive(sq, m);
    if (still) return false;
    sq.hop = undefined; sq.left = t;
    return true;
  }

  /**
   * Down, and the block goes on faster than men on foot can follow: they mount again. Each lorried man makes for his own
   * lorry; each trooper for the horse his holder leads (his own four's holder, else the nearest one left; with none left
   * his horse is by him).
   */
  private mountUp(sq: Squad): void {
    const u = sq.u;
    sq.boarding = this.b.t;
    sq.hop = sq.left = undefined;
    for (const m of sq.men) { m.out = undefined; m.off = undefined; }
    // Every lorry on its way to its men (a lorry halted is one waiting for them to climb in: board).
    for (const v of sq.veh) if (v.pose === 'parked') v.pose = 'drive';
    const men = sq.men.filter((m) => this.stands(sq, m));
    for (const m of men) {
      m.aid = undefined; m.hidden = undefined; m.aim = undefined;
      if (u.kind !== 'mot' && m.role !== 'holder') m.bus = this.holderOf(sq, m)?.k;
    }
  }

  /** The nearest of some men (lorries, holders) to a man, and its place in the list. */
  private nearest(of: Man[], m: Man): [Man, number] | undefined {
    let best: [Man, number] | undefined, d = Infinity;
    of.forEach((o, i) => { const dd = Math.hypot(o.x - m.x, o.y - m.y); if (dd < d) { d = dd; best = [o, i]; } });
    return best;
  }

  /**
   * The holder leading a trooper's horse now (his bus, a holder's number): that number's own man where he still stands
   * and is no one's prisoner, else a holder of his section (drawn as sections) still standing with it, as the holders
   * themselves go to every man of their number (board). None: his bus is gone with all its holders.
   * (A section's key holder down while his section's holders stood left the trooper with no man to run to: the release 2
   * drill threw at Poznan in 1939, a Polish cavalry block remounting in its rout; the design director's find, 23 Nov.)
   */
  private busOf(sq: Squad, m: Man): Man | undefined {
    if (m.bus === undefined) return undefined;
    let other: Man | undefined;
    for (const h of sq.men) {
      if (h.k !== m.bus || h.role !== 'holder' || h.pr || !this.stands(sq, h)) continue;
      if (h.j === undefined) return h;
      other ??= h;
    }
    return other;
  }

  /** A trooper's horse holder: the one at the end of his four, else the nearest still standing. */
  private holderOf(sq: Squad, m: Man): Man | undefined {
    const held = sq.men.filter((h) => h.role === 'holder' && !h.pr && this.stands(sq, h));
    return held.find((h) => Math.floor(h.k / 4) === Math.floor(m.k / 4)) ?? this.nearest(held, m)?.[0];
  }

  /** The lorry a lorried man rides in: his own, by his place on the tray. */
  private lorryOf(sq: Squad, m: Man): Man { return sq.veh[this.lorryIx(sq, m)]; }
  /** Which of the block's lorries is his: by his place on the tray, or (drawn as sections) his section's own. */
  private lorryIx(sq: Squad, m: Man): number { return (m.lor ?? (sq.tier >= 1 ? m.sec ?? m.k : m.k)) % sq.veh.length; }
  /** His seat in his lorry's bed, two benches a stride apart, front to back (so men getting down are never one knot). */
  private seat(sq: Squad, m: Man): [number, number] {
    const v = this.lorryOf(sq, m), j = this.seatNo(sq, m);
    // Drawn as sections, a section to a lorry (a gun's crew to its tractor): its key man first, close benches.
    if (sq.tier >= 1) return toWorld(v, v.face, j % 2 ? 0.55 : -0.55, 1.4 - 0.6 * Math.floor(j / 2));
    return toWorld(v, v.face, j % 2 ? 0.55 : -0.55, 0.5 - 1.1 * Math.floor(j / 2));
  }

  /**
   * Men in lorries: where he stands, down off her tailboard (hopOut): HOP.tail behind her as the 3D game faces her (her
   * way over her last metre of road: roadF), to his own bench's side.
   */
  private landing(sq: Squad, m: Man): [number, number] {
    const v = this.lorryOf(sq, m);
    return toWorld(v, v.roadF ?? v.face, (this.seatNo(sq, m) % 2 ? 1 : -1) * HOP.abreast, -HOP.tail);
  }

  /**
   * His seat's number in his lorry's bed (seat): two abreast, the odd ones on her right, front to back. Each lorry's men
   * are numbered once, in their sections' own order (a section to a lorry keeps its seats: its key man first); where
   * sections share a lorry (more sections than lorries) they sit turn about, so no two men have one seat (they sat on
   * one another: the seats' cloud test, 23 November).
   */
  private seatNo(sq: Squad, m: Man): number {
    const own = (x: Man): number => (sq.tier >= 1 ? x.seat ?? x.j ?? 0 : Math.floor((x.k - (sq.of !== undefined ? DR.mech.base : 0)) / sq.veh.length));
    if (!sq.seats) {
      sq.seats = new Map();
      const by = new Map<number, Man[]>();
      for (const x of sq.men) {
        if (x.role === 'holder') continue;
        const i = this.lorryIx(sq, x), l = by.get(i);
        if (l) l.push(x); else by.set(i, [x]);
      }
      for (const l of by.values()) l.map((x, i) => ({ x, o: own(x), i })).sort((a, c) => a.o - c.o || a.i - c.i).forEach(({ x }, n) => sq.seats!.set(x, n));
    }
    return sq.seats.get(m) ?? own(m);
  }

  /**
   * Where a man mounting runs to: his lorry, or his horse at his holder's side (the holder's string of horses spread
   * across his right and left, one per man he leads, in their order); null where he has no horse led for him.
   */
  private mountAt(sq: Squad, m: Man): [number, number] | null {
    const u = sq.u;
    if (u.kind === 'mot') return this.seat(sq, m);
    const h = this.busOf(sq, m);
    if (!h) return null;
    const led = sq.men.filter((x) => x.bus === h.k && x.role !== 'holder' && this.stands(sq, x)).sort((a, c) => a.k - c.k || a.n - c.n);
    const j = Math.max(0, led.indexOf(m)), side = j % 2 ? 1 : -1, [rx, ry] = right(h.face);
    const off = side * (1.6 + 1.6 * Math.floor(j / 2));
    return [h.x + rx * off, h.y + ry * off];
  }

  /**
   * Mounting again (mountUp). Each lorry drives up to its men and halts while they climb aboard, then goes after the
   * block; each holder brings his horses up to the men he leads them for, who run to them and mount, he last, and each
   * rides on to his place as he is up. Once every man is up the block rides on (mounted).
   */
  private board(sq: Squad): void {
    const u = sq.u, t = this.b.t;
    const men = sq.men.filter((m) => this.stands(sq, m));
    const run: Pose = sq.phase === 'flee' ? 'flee' : 'run';
    // To his lorry or his horse at a run where it is near, or has halted for him, or is well along the block's way (the
    // ground it is making, not its last step), or he is running already; else he waits for it, kneeling where he is (no
    // man runs back from the line to meet his lorry: it comes up to him). While the block itself has stopped, he waits
    // too: it may go on (he climbs in then) or stay (he gets down again); in a rout he runs on to it, and waits by it.
    const afoot = (m: Man, to: Man): void => {
      const dx = to.x - m.x, dy = to.y - m.y, at = this.mountAt(sq, m);
      m.aim = undefined; m.hidden = undefined; m.cover = 'none'; m.until = t + 1;
      const by = !!at && Math.hypot(at[0] - m.x, at[1] - m.y) < 2;
      const go = sq.slow < MOUNT.downAfter
        ? sq.phase === 'flee' || m.pose === run || Math.hypot(dx, dy) < 30 || to.v === 0 || (sq.net > 1 && (dx * sq.wx + dy * sq.wy) / sq.net > 5)
        : sq.phase === 'flee' && !by;
      if (go) { m.pose = run; m.v = 6; }
      else { m.pose = 'wait'; m.v = 0; m.spot = [m.x, m.y]; }
    };
    let all = true;
    if (u.kind === 'mot') {
      sq.veh.forEach((v, i) => {
        const off = men.filter((m) => m.pose !== 'mounted' && this.lorryIx(sq, m) === i);
        v.cover = 'none'; v.until = t + 1;
        if (!off.length) { this.follow(sq, v, toWorld(u, sq.heading, 0, u.d / 2 - 8 - i * 18), 'drive'); return; }
        all = false;
        const cx = off.reduce((a, m) => a + m.x, 0) / off.length, cy = off.reduce((a, m) => a + m.y, 0) / off.length, d = Math.hypot(cx - v.x, cy - v.y);
        // Up to its men, and halted there (once halted for them, it waits till all are aboard).
        if (d > 6 && v.pose !== 'parked') { v.spot = [cx, cy]; v.slotAt = v.spot; v.pose = 'drive'; v.v = clamp(d * 0.5, 3, 16); }
        else { v.spot = [v.x, v.y]; v.slotAt = v.spot; v.pose = 'parked'; v.v = 0; }
      });
      for (const m of men) if (m.pose !== 'mounted') afoot(m, this.lorryOf(sq, m));
    } else {
      const slotOf = (m: Man): [number, number] => {
        const [a, l] = this.slot(sq, m, sq.phase === 'column' ? 'column' : 'line');
        return toWorld(u, sq.phase === 'column' ? sq.heading : u.face, a, l);
      };
      for (const m of men) {
        if (m.role === 'holder') continue; // (a holder's two keep by him: pairs)
        if (m.pose === 'ride') { this.follow(sq, m, slotOf(m), 'ride'); if (m.v === 0) m.pose = 'ride'; m.until = t + 1; continue; }
        // His holder fell: the nearest one left takes his horse; with none, his horse is by him.
        if (!this.busOf(sq, m)) m.bus = this.holderOf(sq, m)?.k;
        const h = this.busOf(sq, m);
        if (!h) { m.pose = 'ride'; m.v = 0; m.spot = [m.x, m.y]; m.aim = undefined; m.cover = 'none'; continue; }
        all = false;
        afoot(m, h);
      }
      for (const h of men) {
        if (h.role !== 'holder' || h.pr) continue;
        const mine = men.filter((m) => m.bus === h.k && m.role !== 'holder' && m.pose !== 'ride');
        h.aim = undefined; h.cover = 'none'; h.until = t + 1;
        if (!mine.length) { if (h.pose !== 'ride') { h.pose = 'ride'; h.v = 0; h.spot = [h.x, h.y]; } this.follow(sq, h, slotOf(h), 'ride'); if (h.v === 0) h.pose = 'ride'; continue; }
        all = false;
        const cx = mine.reduce((a, m) => a + m.x, 0) / mine.length, cy = mine.reduce((a, m) => a + m.y, 0) / mine.length, d = Math.hypot(cx - h.x, cy - h.y);
        // Up to them with the horses, and halted there till all are up (before, he kept up with the block: never halted).
        const halted = h.pose === 'horses' && h.v === 0;
        h.pose = 'horses';
        if (d > 5 && !halted) { h.spot = [cx, cy]; h.v = clamp(d * 0.5, 3, 12); } else { h.spot = [h.x, h.y]; h.v = 0; }
      }
    }
    // All up, and their block going on: they ride on (slowing or stopped, they sit till it goes on, or get down: think).
    if (all && sq.slow === 0) { sq.down = false; sq.boarding = undefined; }
  }

  /** A man mounting may climb in or swing up now: his lorry halted (his horse held still) and his block still going. */
  private mayMount(sq: Squad, m: Man): boolean {
    if (sq.slow >= MOUNT.downAfter) return false;
    if (sq.u.kind === 'mot') return this.lorryOf(sq, m).pose === 'parked';
    const h = this.busOf(sq, m);
    return !!h && h.v === 0;
  }

  /**
   * Lorries: aboard, they carry the block; once it is down they pull back behind it and park in what cover there is; a
   * battery's tractor waits behind its guns, and leads them when they move. A horse holder takes his mates' horses back.
   */
  private lorries(sq: Squad): void {
    const u = sq.u, t = this.b.t;
    if (u.kind === 'cav' && sq.down) {
      // At the rear of the block's ground, leading the horses there and keeping up with it; close behind it on the move.
      for (const m of sq.men) if (m.role === 'holder' && !m.pr && this.stands(sq, m)) {
        const s = toWorld(u, u.face, m.jx * 4, -u.d / 2 - (sq.close ? 10 : 36));
        m.spot = s; m.slotAt = s; m.pose = 'horses'; m.v = Math.min(14, Math.max(3, sq.speed * 1.25)); m.aim = undefined; m.cover = 'none';
      }
    }
    if (u.kind !== 'mot' || !sq.down) return;
    // (Men in lorries: their men just down behind them, they stand a moment more while the men clear their tails)
    if (sq.left !== undefined && t < sq.left + HOP.clear && sq.phase !== 'flee') { for (const v of sq.veh) { v.v = 0; v.pose = 'parked'; v.spot = [v.x, v.y]; } return; }
    sq.veh.forEach((v, i) => {
      if (sq.phase === 'flee' || sq.phase === 'fallback' || sq.close) {
        const s = toWorld(u, u.face, (i - 0.5) * 20, -u.d / 2 - (sq.close ? 30 : 40));
        this.follow(sq, v, s, 'drive', 1.2); return;
      }
      const s0 = toWorld(u, u.face, (i - (sq.veh.length - 1) / 2) * 24, -u.d / 2 - 110), s = this.tankmind ? this.mindLean(sq, v, s0, u.face) : s0;
      if (Math.hypot(s[0] - v.slotAt[0], s[1] - v.slotAt[1]) > 25 || t < 0.2) {
        const best = this.seek(sq, v, s, this.threatDir(sq), sq.veh, 30);
        v.spot = [best.x, best.y]; v.slotAt = s; v.cover = best.kind; v.pose = 'drive'; v.v = Math.min(16, Math.max(6, sq.speed * 1.3));
      } else if (Math.hypot(v.x - v.spot[0], v.y - v.spot[1]) < 1.5) { v.pose = 'parked'; v.v = 0; }
    });
    // (tank minds: each lorry her own place in the row, above, and her own moment to move off: mindLean, mindOff)
    if (this.tankmind) for (const v of sq.veh) this.mindOff(sq, v);
  }

  /**
   * The anti-tank team's ambush: once enemy tanks come within range of a block holding or fighting where it stands, its
   * anti-tank man goes to the end of its line nearer their flank, lies hidden in the best cover there, and fires only
   * when they are close or are fighting his block; after each shot he moves. Foot in contact with tanks fighting alone
   * (rules: item 7, aloneAt) close in on them with grenades: the close-assault teams.
   */
  private ambush(sq: Squad): void {
    const u = sq.u, b = this.b, t = b.t;
    if (!fighting(u) || sq.phase === 'fallback' || sq.phase === 'flee' || sq.phase === 'column') return;
    const at = this.up(sq).find((m) => m.role === 'at');
    const tanks = b.units.filter((e) => e.side !== u.side && e.kind === 'arm' && fighting(e) && !e.hid && b.gap(u, e) < DR.ranges.ambush);
    if (at) {
      if (!tanks.length || sq.speed > 0.6) at.hidden = undefined;
      else {
        const tk = tanks.reduce((m, e) => (b.gap(u, e) < b.gap(u, m) ? e : m), tanks[0]);
        const [rx, ry] = right(u.face), side = Math.sign((tk.x - u.x) * rx + (tk.y - u.y) * ry) || 1;
        const end = toWorld(u, u.face, side * (u.w / 2 - 4), u.d / 2 - 6);
        const dir: [number, number] = [(tk.x - end[0]) / (Math.hypot(tk.x - end[0], tk.y - end[1]) || 1), (tk.y - end[1]) / (Math.hypot(tk.x - end[0], tk.y - end[1]) || 1)];
        const close = b.gap(u, tk) < 150 || tk.engaged === u.id;
        if (!at.hidden && !close) { this.go(sq, at, end, 'creep', 2.5, t + 3, dir, 14); at.hidden = true; at.aim = undefined; }
        else if (at.hidden && Math.hypot(at.x - at.spot[0], at.y - at.spot[1]) < 1 && !close) { at.pose = 'hide'; at.v = 0; }
        if (close && t >= at.until) {
          const ts = this.squads.get(tk.id), tank = ts?.men.filter((x) => x.role === 'tank' && this.stands(ts, x)).sort((p, q) => Math.hypot(p.x - at.x, p.y - at.y) - Math.hypot(q.x - at.x, q.y - at.y))[0];
          at.hidden = undefined; at.pose = 'fire_kneel'; at.v = 0; at.shot = t; at.aim = tank ? [tank.x, tank.y] : [tk.x, tk.y]; at.until = t + 6 + 3 * roll(this.seed, u.id, t);
          // Then off to a fresh place along the line.
          const next = toWorld(u, u.face, side * (u.w / 2 - 10 - 15 * roll(this.seed, u.id, t, 2)), u.d / 2 - 6);
          at.slotAt = next;
        } else if (!at.hidden && close && Math.hypot(at.x - at.slotAt[0], at.y - at.slotAt[1]) > 3 && t >= at.until - 3) this.go(sq, at, at.slotAt, 'creep', 2.5, t + 3, dir, 6);
      }
    }
    // Close assault on tanks fighting alone among our foot.
    const lone = tanks.find((e) => e.aloneAt !== undefined && t - e.aloneAt < 2 && (e.engaged === u.id || u.engaged === e.id));
    if (lone && isFoot(u)) {
      const ts = this.squads.get(lone.id);
      const tank = ts?.men.filter((x) => x.role === 'tank' && this.stands(ts, x))[0];
      if (tank) {
        const team = this.up(sq).filter((m) => m.role === 'rifle' || m.role === 'at').sort((p, q) => Math.hypot(p.x - tank.x, p.y - tank.y) - Math.hypot(q.x - tank.x, q.y - tank.y)).slice(0, 2);
        for (const m of team) {
          const d = Math.hypot(m.x - tank.x, m.y - tank.y);
          if (d > 14) { if (t >= m.until || m.pose !== 'crawl') { const side = m.k % 2 ? 1 : -1, [rx, ry] = right(tank.face); m.spot = [tank.x + rx * side * 9, tank.y + ry * side * 9]; m.pose = 'crawl'; m.v = 2; m.until = t + 8; m.hidden = undefined; } }
          else if (t >= m.until || m.pose === 'crawl') { m.pose = 'throw_grenade'; m.v = 0; m.shot = t; m.aim = [tank.x, tank.y]; m.until = t + 2.5; }
        }
      }
    }
  }

  /** The officer walks his line every so often, crouched, stopping by each team. */
  private officer(sq: Squad): void {
    const u = sq.u, t = this.b.t;
    if (sq.phase !== 'fight' && sq.phase !== 'hold' && sq.phase !== 'startline') return;
    const o = this.up(sq).find((m) => m.role === 'officer');
    if (!o || t < sq.officerAt) return;
    sq.officerAt = t + 8 + 8 * roll(this.seed, u.id, t);
    const team = (Math.floor(t / 8) % 3) as 0 | 1 | 2;
    const mates = this.up(sq).filter((m) => m.team === team && m !== o);
    const by = mates.length ? mates[Math.floor(roll(this.seed, u.id, t, 1) * mates.length)] : null;
    const p: [number, number] = by ? [by.spot[0] - Math.sin(u.face) * 4, by.spot[1] - Math.cos(u.face) * 4] : toWorld(u, u.face, 0, -u.d / 2 + 6);
    this.go(sq, o, p, 'creep', 2.5, t + 6, undefined, 5);
  }

  // ---- Tanks --------------------------------------------------------------------------------------------------------

  /**
   * Tanks. On the road, nose to tail. Coming on, in pairs: where the block goes slowly one pair halts to cover while the
   * other drives to its next firing place; fast, all drive and a tank halts to fire now and then. In a fight each takes
   * the best firing place near its own: hull-down behind a crest where there is one, else a wood's or a house's corner;
   * after firing from the open she backs off a few metres and comes up again (by style). In woods and streets with
   * their own foot near, they keep behind the foot's line. Alone among enemy foot they close up. Shaken, they back off to
   * cover; routing, they drive for their own edge. Each tank's place is where the rules would lay her wreck (sim.ts
   * wreckOffset), so she is never far from it; knocked out, she burns where she stood (falls, wreckSpot).
   */
  // ---- Tanks with minds of their own (Drill.tankmind): its helpers stand just above tanks(), below this line ----

  /**
   * Tank minds: the phase this tank's commander is acting on. When his block's phase changes (or it goes from rolling
   * on fast to leapfrogging, or is shaken) he takes it up at his own moment, DR.tankmind.react seconds later (a veteran
   * crew sooner, 'quick'); the platoon leader, its first tank, at once. Till then she goes on with what she was at:
   * her old phase's drill is still run for her each think, so a tank driving with her block keeps her moving place. A
   * change gone again before his moment comes he never takes up.
   */
  private mindPhase(sq: Squad, m: Man, lead: boolean): Phase {
    const T = DR.tankmind, u = sq.u, t = this.b.t, M = (m.mind ??= {});
    const fast = (sq.made ?? sq.speed) >= T.roll[M.fast ? 0 : 1], shaken = u.state === 'wavering';
    if (M.ph === undefined || (sq.phase === M.ph && fast === !!M.fast && shaken === !!M.shaken)) M.at = undefined;
    else if (M.at === undefined) M.at = lead ? t : t + (T.react[0] + (T.react[1] - T.react[0]) * roll(this.seed, u.id, m.n, Math.round(t * 2), 301)) * T.quick[this.band(u)];
    if (M.ph === undefined || (M.at !== undefined && t >= M.at - 1e-6)) {
      // (a new phase: whatever she had timed for the old one is dropped)
      if (M.ph !== sq.phase) M.stop = M.go = M.fire = undefined;
      M.ph = sq.phase; M.fast = fast; M.shaken = shaken; M.at = undefined;
    }
    return M.ph;
  }

  /**
   * Tank minds: the way this tank is going. Coming on with a block that goes steadily (straight over the last seconds:
   * mindBlock, and making DR.tankmind.way metres a second or more), it is the way the block has made over those
   * seconds, not its last step's: a block the rules shove a little aside each step does not swing her from side to
   * side. And she keeps her way when her block's turns about under her (the rules jostle it back in a fight, or it
   * gives a little ground): she takes the new way up at her own moment (DR.tankmind.react, her own dice), and never
   * sooner than her own least time since she last changed direction (mindStep), so a jostle shorter than that never
   * turns her and no platoon goes into reverse as one. A lesser turn she follows as it comes, and in any phase but
   * coming on her block's last step is her way, as the plain drill's.
   */
  private mindWay(sq: Squad, m: Man): number {
    const T = DR.tankmind, M = m.mind!, t = this.b.t, on = M.ph === 'deploy' || M.ph === 'bound';
    const H = on && sq.net >= T.way && (sq.made ?? 0) >= sq.speed - 1e-9 ? Math.atan2(sq.wx, sq.wy) : sq.heading;
    if (M.h === undefined || !on || Math.cos(H - M.h) > 0) { M.turn = undefined; return (M.h = H); }
    if (M.turn === undefined) M.turn = Math.max(M.keep ?? 0, t + (T.react[0] + (T.react[1] - T.react[0]) * roll(this.seed, sq.u.id, m.n, Math.round(t * 2), 302)) * T.quick[this.band(sq.u)]);
    if (t >= M.turn - 1e-6) { M.turn = undefined; return (M.h = H); }
    return M.h;
  }

  /** Tank minds: a tank's top speed: closing up after a halt she may go a little faster than the plain drill's catch-up (DR.tankmind.top). */
  private mindTop(sq: Squad): number { return Math.max(tankTop(sq), DR.tankmind.top); }

  /**
   * Tank minds, every step (walk): what her commander has timed for this step, to the step and not to her block's half
   * second. His own moment to take up his block's new phase or way comes: her drill is run for her at that step
   * (tanks, for her alone). Her halt to fire begins: she stops where she is. She keeps her direction: forward or
   * backing, she may change only once her own least time since her last change is up (DR.tankmind.keep), and stands
   * meanwhile, her order held; a reverse she is ordered into (falling back, backing out of a firing post) is real and
   * taken at once, and so is her block's call back inside its leash. Coming on she does not back toward the enemy: she
   * turns about. And her round goes, if she still stands with her gun laid and her block still fights. Returns her
   * top speed.
   */
  private mindStep(sq: Squad, m: Man): number {
    const M = m.mind, t = this.b.t;
    if (!M) return this.mindTop(sq);
    // (a block's cars, AA vehicles, jeeps and motorcycles have no commander's mind: only their own moment to move off)
    if (sq.esc) { this.mindTick(sq, m); return this.mindTop(sq); }
    // Her commander's own moment, come between two thinks of her block (he takes up its new phase, or its way turned
    // about, or stops her to wait for her place to come up; or her moment to back down behind her firing place, to
    // come up to it, or, standing, to leave her post after a near miss): her drill is run for her now, at her own step.
    const stood = M.still !== undefined;
    if ((M.at !== undefined && t >= M.at - 1e-6) || (M.turn !== undefined && t >= M.turn - 1e-6) || (M.ease !== undefined && t >= M.ease - 1e-6) || (M.rise !== undefined && t >= M.rise - 1e-6) || (stood && M.shift !== undefined && t >= M.shift - 1e-6)) {
      const ease = M.ease, rise = M.rise, shift = M.shift;
      this.tanks(sq, m);
      if (ease !== undefined && M.ease === ease) M.ease = undefined;
      if (rise !== undefined && M.rise === rise && t >= rise - 1e-6) M.rise = undefined;
      if (stood && shift !== undefined && M.shift === shift && t >= shift - 1e-6) M.shift = undefined;
    }
    if (M.stop !== undefined && t >= M.stop - 1e-6) {
      M.stop = undefined;
      // (stopped short of the cover she was making for, she is not in it)
      if (MOVING.has(m.pose)) { if (Math.hypot(m.spot[0] - m.x, m.spot[1] - m.y) > 2) m.cover = 'none'; m.pose = 'halt'; m.v = 0; m.spot = [m.x, m.y]; }
    }
    // (the order she holds while she may not yet change direction: taken up again once she may, unless another has come)
    if (M.wait) {
      if (m.pose !== 'halt' || m.v !== 0 || m.spot[0] !== M.wait[1] || m.spot[1] !== M.wait[2]) M.wait = undefined;
      else if (t >= (M.keep ?? 0) - 1e-6) { m.pose = 'drive'; m.v = M.wait[0]; M.wait = undefined; }
    }
    if (m.v > 0 && m.pose === 'drive') {
      // (as walk's own switch: a place close behind her she backs into)
      const dx = m.spot[0] - m.x, dy = m.spot[1] - m.y, d = Math.hypot(dx, dy);
      let back = d > 0.3 && d < 30 && Math.abs(wrap(Math.atan2(dx, dy) - m.face)) > 1.9;
      // Coming on, she backs no further than DR.tankmind.back metres toward the enemy, her rear to him (nor with none
      // about): her nose turned away from her way, she turns about where she stands and drives on. (walk backs a tank
      // into a place under 30 m behind her: set further off on the same line, she pivots to it.) Counted from where she
      // began to back (rb), so a place kept a few metres behind her tail as she backs does not keep her backing.
      const on = back && (M.ph === 'bound' || M.ph === 'deploy') && (!m.aim || dx * (m.aim[0] - m.x) + dy * (m.aim[1] - m.y) > 0);
      M.rb = on ? M.rb ?? [m.x, m.y] : undefined;
      if (on && (d > DR.tankmind.back || Math.hypot(m.x - M.rb![0], m.y - M.rb![1]) > DR.tankmind.back)) { m.spot = [m.x + (dx / d) * 31, m.y + (dy / d) * 31]; back = false; }
      if (back !== !!M.rev) {
        // (left outside her block's leash she may not stand: its call back in is taken at once, as an order to reverse is)
        const p: [number, number] = [m.x, m.y];
        if (t >= (M.keep ?? 0) - 1e-6 || this.leash(sq, p) !== p) { M.rev = back || undefined; M.keep = t + this.mindKeep(sq, m); }
        else { M.wait = [m.v, m.spot[0], m.spot[1]]; m.pose = 'halt'; m.v = 0; }
      }
    } else if (m.v > 0 && m.pose === 'reverse' && !M.rev) { M.rev = true; M.keep = t + this.mindKeep(sq, m); }
    if (M.rb && m.v === 0 && !M.wait) M.rb = undefined;
    const still = m.v === 0 && !MOVING.has(m.pose);
    if (!still) M.still = undefined; else if (M.still === undefined) M.still = t;
    if (M.fire !== undefined && t >= M.fire - 1e-6) {
      if (still && m.aim && sq.u.engaged !== undefined && live(sq.u)) { m.shot = t; m.pose = 'fire'; if (M.post) m.shots = (m.shots ?? 0) + 1; }
      M.fire = undefined; M.post = undefined;
    }
    return this.mindTop(sq);
  }

  /** Tank minds: this tank's own least time in one direction, forward or backing (DR.tankmind.keep). */
  private mindKeep(sq: Squad, m: Man): number { const K = DR.tankmind.keep; return K[0] + (K[1] - K[0]) * roll(this.seed, sq.u.id, m.n, 303); }

  /**
   * Tank minds, once a think for the block. The pace it is really making, for its tanks' drill: its pace, less where
   * the rules jostle it to and fro (by how straight its way over the last seconds was, the ground made against the way
   * gone, under DR.tankmind.straight: a block shoved back and forth at 4 m a second that gains 2 is coming on slowly,
   * and its tanks leapfrog). The shells fired at it since its last think. And when its fight began: the think it is
   * first fighting after DR.tankmind.shot.fresh seconds or more out of one.
   */
  private mindBlock(sq: Squad): void {
    const t = this.b.t, K = sq.track;
    let gone = 0;
    for (let i = 2; i < K.length; i += 2) gone += Math.hypot(K[i] - K[i - 2], K[i + 1] - K[i - 1]);
    sq.made = sq.speed * (gone > 1 ? Math.min(1, (sq.net * (K.length / 2 - 1) * DT) / gone / DR.tankmind.straight) : 1);
    // The shells fired at it since its last think (a tank's, a launcher's, a gun's, a mortar's), each kept till it has
    // landed and a think has looked at it (mindPost: a near miss or a hit moves a tank to her next post).
    let H: number[] | undefined;
    if (sq.hits) for (let i = 0; i < sq.hits.length; i += 4) if (sq.hits[i + 3] > t - 0.5 + 1e-6) (H ??= []).push(sq.hits[i], sq.hits[i + 1], sq.hits[i + 2], sq.hits[i + 3]);
    for (let i = this.shots.length - 1; i >= 0 && this.shots[i].n > (sq.hitN ?? 0); i--) {
      const r = this.shots[i];
      if (r.on === sq.u.id && !r.m && (r.w === 'tank' || r.w === 'at' || r.w === 'gun' || r.w === 'mortar')) (H ??= []).push(r.x, r.y, r.r === 'miss' ? -1 : r.ok ?? -1, r.l);
    }
    sq.hits = H; sq.hitN = this.fired;
    if (sq.u.engaged === undefined) return;
    if (sq.foughtAt === undefined || t - sq.foughtAt > DR.tankmind.shot.fresh) sq.fightAt = t;
    sq.foughtAt = t;
  }

  /**
   * Tank minds: the battle time this tank's next round is due. Her own period (the plain drill's: 6 to 10 s at armour,
   * BOOK.tank.every at foot with the drill book), each pause her own share of it, fresh after every round
   * (DR.tankmind.shot.pause), so her shots are never evenly spaced; and, her block's fight just begun, not before her
   * own moment to open (shot.first), so a platoon never fires its first volley as one. 'slow': her period stretched
   * by this much (rolling on fast she halts to fire less often than she fires in a fight: DR.tankmind.halt.rare).
   */
  private mindDue(sq: Squad, m: Man, e: LandUnit, slow = 1): number {
    const S = DR.tankmind.shot, u = sq.u, at = sq.fightAt ?? 0;
    const open = at + S.first[0] + (S.first[1] - S.first[0]) * roll(this.seed, u.id, m.n, Math.round(at * 2), 304);
    if (m.shot === undefined) return open;
    const r = roll(this.seed, u.id, m.n, 306), B = BOOK.tank.every, every = this.byBook && e.kind !== 'arm' ? B[0] + (B[1] - B[0]) * r : 6 + 4 * r;
    return Math.max(open, m.shot + every * slow * (S.pause[0] + (S.pause[1] - S.pause[0]) * roll(this.seed, u.id, m.n, Math.round(m.shot * 10), 305)));
  }

  /**
   * Tank minds: a halted tank's round. She fires from her halt: never before she has stood DR.tankmind.halt.lay seconds
   * with her gun laid, and at the step her round falls due, her own (mindStep fires it), not on her block's half
   * second. 'post': it counts toward the shots she fires from her firing post. Always false: nothing fires on the think.
   */
  private mindShot(sq: Squad, m: Man, e: LandUnit | null, post = false, slow = 1): boolean {
    const M = m.mind!, t = this.b.t, H = DR.tankmind.halt;
    if (!e || sq.u.engaged === undefined || M.fire !== undefined || M.still === undefined) return false;
    const laid = M.still + H.lay[0] + (H.lay[1] - H.lay[0]) * roll(this.seed, sq.u.id, m.n, Math.round(M.still * 10), 308);
    const at = Math.max(this.mindDue(sq, m, e, slow), laid, t);
    if (at < t + 0.5) { M.fire = at; M.post = post || undefined; }
    return false;
  }

  /**
   * Tank minds: whether this tank may begin a halt to fire now. Pairs cover each other (the first and second tank, the
   * third and fourth; a last odd tank goes with the pair before hers): never while her partner stands, never more than
   * half the platoon standing at once, and not within DR.tankmind.halt.apart seconds of a mate's stop.
   */
  private mindTurn(tanks: Man[], i: number): boolean {
    const H = DR.tankmind.halt, t = this.b.t, n = tanks.length;
    const pair = (j: number): number => (n > 1 && j === n - 1 && j % 2 === 0 ? (j >> 1) - 1 : j >> 1);
    let standing = 0;
    for (let j = 0; j < n; j++) {
      const O = tanks[j].mind;
      if (j === i || !O || O.go === undefined || t >= O.go - 1e-6) continue;
      if (pair(j) === pair(i) || t - (O.from ?? t) < H.apart) return false;
      standing++;
    }
    return standing < Math.max(1, n >> 1);
  }

  /**
   * Tank minds: coming on fast with her block (the rolling advance). Move, halt briefly to fire, move on: with the
   * enemy in reach and a round due she presses on a little ahead of her place, stops at her own moment, lays, fires
   * from the halt and drives on, closing up after. Her block's pace is the rules' and does not wait for her: the halt
   * is bought by letting her fall behind her place, never more than DR.tankmind.halt.fall metres and never to her
   * block's leash, so she begins only a halt she can afford, and the longest of her wish that she can. Pairs cover
   * each other (mindTurn): some of the platoon are always moving while others shoot.
   */
  private mindRoll(sq: Squad, m: Man, tanks: Man[], i: number, s: [number, number], e: LandUnit | null, bound: boolean, h: number): void {
    const u = sq.u, b = this.b, t = b.t, H = DR.tankmind.halt, M = m.mind!, V = sq.speed, [hx, hy] = fwd(h);
    const near = e ? b.gap(u, e) : Infinity, on = bound && !!e && u.engaged !== undefined && near < H.reach;
    // Her place is never outside her block's leash (kept behind its own foot on close ground it may lie far behind the
    // box): she would stand waiting for it till the leash took her, and be dragged along its edge after.
    s = this.leash(sq, s, -H.edge);
    if (M.go !== undefined) {
      // In her halt (mindStep stopped her, and fires her round at its step): standing, her gun laid, till it is time to go
      // on; cut short where her block has picked up its pace and would leave her outside its leash.
      const left = V * (M.go - t) + (V * V) / (2 * TANK_GAIN), p: [number, number] = [m.x - hx * left, m.y - hy * left];
      if (on && t < M.go - 1e-6 && !MOVING.has(m.pose) && this.leash(sq, p, -H.edge / 2) === p) { m.v = 0; m.pose = 'halt'; m.aim = this.tankAim(sq, m, e); return; }
      M.stop = M.go = M.fire = undefined;
    }
    let ahead = 0;
    if (on) {
      const due = this.mindDue(sq, m, e!, H.rare), lag = (s[0] - m.x) * hx + (s[1] - m.y) * hy, k = Math.round(t * 2);
      if (due - t < H.soon) {
        ahead = H.ahead;
        // Her own step of this half second to stop at, her own time laying, her own wish for the halt's length.
        const d0 = Math.floor(5 * roll(this.seed, u.id, m.n, k, 307)) * DT, lay = H.lay[0] + (H.lay[1] - H.lay[0]) * roll(this.seed, u.id, m.n, k, 308);
        if (due <= t + d0 + lay && this.mindTurn(tanks, i)) {
          const wish = H.len[0] + (H.len[1] - H.len[0]) * roll(this.seed, u.id, m.n, k, 309);
          // (she drives on again on a think: the halt's length is that many half seconds less her own step)
          for (let j = Math.round((d0 + wish) / 0.5); 0.5 * j - d0 >= H.len[0] - 1e-6; j--) {
            // What it costs her: the ground her block makes while she stands, and while she gathers way again.
            const len = 0.5 * j - d0, cost = V * len + (V * V) / (2 * TANK_GAIN), p: [number, number] = [m.x - hx * cost, m.y - hy * cost];
            if (lag + cost > H.fall || this.leash(sq, p, -H.edge) !== p) continue;
            M.stop = M.from = t + d0; M.fire = M.stop + Math.min(lay, len - H.see); M.go = t + 0.5 * j;
            break;
          }
        }
      }
    }
    const to: [number, number] = ahead ? [s[0] + hx * ahead, s[1] + hy * ahead] : s, was = m.v > 0 && MOVING.has(m.pose);
    this.follow(sq, m, to, 'drive', 1, h);
    if (m.v > 0 && MOVING.has(m.pose)) this.mindWeave(sq, m, h);
    // Told to stand and wait for her place to come up (she is well ahead of it: her block keeps her behind its foot, or
    // has checked), she stops at her own moment, not in her platoon's instant (its leader at once): rolling on slowly
    // meanwhile.
    if (m.v === 0 && was && V > 0.4 && i > 0) {
      const R = DR.tankmind.react;
      if (M.ease === undefined) M.ease = t + (R[0] + (R[1] - R[0]) * roll(this.seed, u.id, m.n, Math.round(t * 2), 312)) * DR.tankmind.quick[this.band(u)];
      if (t < M.ease - 1e-6) { m.pose = 'drive'; m.v = Math.max(1.5, V * 0.6); m.spot = this.leash(sq, [m.x + hx * 12, m.y + hy * 12], -H.edge); m.until = t + 1; }
    } else M.ease = undefined;
    // (waiting on for it, the pairs take turns to roll on slowly: mindWait)
    if (m.v === 0 && V > 0.4 && M.go === undefined) this.mindWait(sq, m, tanks, i, to, h);
    // Closing up (after a halt, or pressing on for her next) she may go a little faster than the plain drill's catch-up.
    if (m.v > 0 && (to[0] - m.x) * hx + (to[1] - m.y) * hy > (ahead ? 2 : 6)) m.v = Math.min(V * 1.4, this.mindTop(sq));
    // Her gun: laid on her target once her halt is set, and where she stands waiting for the line to come up to her
    // (her round may go from there too); else toward the enemy, as the plain drill.
    if (on && (M.go !== undefined || !MOVING.has(m.pose))) { m.aim = this.tankAim(sq, m, e); if (M.go === undefined) this.mindShot(sq, m, e, false, H.rare); }
    else m.aim = e && near < 900 ? [e.x, e.y] : undefined;
  }

  /**
   * Tank minds: coming on slowly, the pairs leapfrog by the style's clock (tankRush, tankHalt). Within each pair one
   * tank drives on to her next firing place (the best cover near the bound's end: go and seek) while her partner
   * stands covering, then the other way about; the second pair a quarter of the clock after the first, so the four
   * move off one after another. A last odd tank goes with the pair before hers, in its second tank's turn. Each has
   * her own lean on the clock and her own length of drive each turn (DR.tankmind.pair), and drives fast enough to be
   * in her place before her turn to cover comes; not there by then, she stands where she is (her own step of the half
   * second: mindStep), where her block's leash and her place allow. Standing, she fires from her halt at her own
   * step (mindShot). And the platoon never all stands: a tank with no mate on the move goes on, her turn or not.
   */
  private mindBound(sq: Squad, m: Man, tanks: Man[], i: number, s: [number, number], e: LandUnit | null, dir: [number, number], N: Nums, h: number): void {
    const u = sq.u, t = this.b.t, T = DR.tankmind, P = T.pair, M = m.mind!, n = tanks.length, v = Math.max(1, sq.made ?? sq.speed), cyc = N.tankRush + N.tankHalt;
    const odd = n > 1 && i === n - 1 && i % 2 === 0, pair = odd ? (i >> 1) - 1 : i >> 1, turn = odd ? 1 : i % 2;
    const at = t + 100 * cyc - (pair % 2) * cyc / 4 - turn * cyc / 2 + (roll(this.seed, u.id, m.n, 310) * 2 - 1) * P.own, c = Math.floor(at / cyc);
    const left = N.tankRush * (P.len[0] + (P.len[1] - P.len[0]) * roll(this.seed, u.id, m.n, c, 311)) - (at - c * cyc);
    const going = MOVING.has(m.pose), [hx, hy] = fwd(h);
    if (left > 0 || (n > 1 && !tanks.some((o) => o !== m && o.v > 0 && MOVING.has(o.pose)))) {
      M.stop = undefined;
      if (!going || t >= m.until) {
        const span = left > 0 ? left : N.tankRush * P.len[0];
        this.go(sq, m, [s[0] + hx * v * cyc * 0.45, s[1] + hy * v * cyc * 0.45], 'drive', v * cyc / N.tankRush, t + span, dir);
        m.v = clamp(Math.hypot(m.spot[0] - m.x, m.spot[1] - m.y) / Math.max(1, span - P.gather), m.v, Math.max(m.v, P.pace));
      }
      m.aim = e ? [e.x, e.y] : undefined;
      return;
    }
    // Her turn to cover. In her place (or standing already): her gun laid, her round at her own step.
    if (!going || Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 2) {
      m.v = 0; m.spot = [m.x, m.y]; m.pose = 'halt'; m.aim = this.tankAim(sq, m, e);
      // (she fires less often than in a fight, pair.rare; a block shoved about at a fast pace as seldom as one rolling
      // on at it, halt.rare: so about as many rounds are drawn as before)
      this.mindShot(sq, m, e, false, sq.speed >= T.roll[1] ? T.halt.rare : P.rare);
      return;
    }
    // Not there yet: she stands where she is, at her own step of this half second, unless that would leave her at her
    // block's leash or far behind her place (then she drives on to it).
    const p: [number, number] = [m.x, m.y];
    if (M.stop === undefined && this.leash(sq, p, -T.halt.edge) === p && (s[0] - m.x) * hx + (s[1] - m.y) * hy < T.halt.fall) M.stop = t + Math.floor(5 * roll(this.seed, u.id, m.n, Math.round(t * 2), 307)) * DT;
  }

  /**
   * Tank minds: rolling on with her block she steers a little to one side of her way and back again, her own amount and
   * her own time (DR.tankmind.weave), so a platoon's hulls are never dead parallel for long. The point she steers for
   * is moved across her way, by her own share of its distance from her (a few degrees whatever her pace); her place
   * and her pace stay as they were.
   */
  private mindWeave(sq: Squad, m: Man, h: number): void {
    const W = DR.tankmind.weave, id = sq.u.id, [rx, ry] = right(h);
    const side = W.side[0] + (W.side[1] - W.side[0]) * roll(this.seed, id, m.n, 314), len = W.len[0] + (W.len[1] - W.len[0]) * roll(this.seed, id, m.n, 315);
    const d = Math.hypot(m.spot[0] - m.x, m.spot[1] - m.y) * side * Math.sin(2 * Math.PI * (this.b.t / len + roll(this.seed, id, m.n, 316)));
    m.spot = [m.spot[0] + rx * d, m.spot[1] + ry * d];
  }

  /**
   * Tank minds: on the fast advance, standing to wait for her place to come up to her (her block keeps its tanks behind
   * its own foot on close ground, or has checked), the platoon does not all stand: its pairs take turns to roll on
   * slowly (DR.tankmind.wait), one tank of each pair at a time (a last odd tank with the pair before hers, in its second
   * tank's turn), each with her own lean on the turn; at no more than half her block's pace (so her place still comes
   * up to her), never more than wait.far metres beyond where she would begin to wait, nor out of her block's leash; and
   * only while her block goes steadily on (mindBlock).
   */
  private mindWait(sq: Squad, m: Man, tanks: Man[], i: number, s: [number, number], h: number): void {
    const W = DR.tankmind.wait, n = tanks.length, t = this.b.t, V = sq.speed, [hx, hy] = fwd(h);
    // (only behind a block going steadily on: one the rules shove to and fro may leap off the other way)
    if (n < 2 || (sq.made ?? 0) < V - 1e-9 || (m.x - s[0]) * hx + (m.y - s[1]) * hy > 8 + 1.5 * V + W.far) return;
    const odd = i === n - 1 && i % 2 === 0, turn = odd ? 1 : i % 2;
    const at = t + 100 * W.turn - turn * W.turn / 2 - W.own * roll(this.seed, sq.u.id, m.n, 313);
    if (at % W.turn >= W.turn / 2) return;
    const p = this.leash(sq, [m.x + hx * 12, m.y + hy * 12], -DR.tankmind.halt.edge);
    if ((p[0] - m.x) * hx + (p[1] - m.y) * hy < 2) return;
    m.pose = 'drive'; m.v = Math.min(W.pace, V / 2); m.spot = p; m.until = t + 1;
  }

  // (Tank minds, step two: looking, firing posts, the other vehicles and the guns.)

  /**
   * Tank minds: whether her gun is fixed in her hull (no turret: DR.tankmind.fixed), as far as the drill can know her
   * model: a tank destroyer's or an outrider's own, else her block's where the rules name one (a set piece's blocks). A
   * stand-in the 3D game alone picks for a campaign battle the drill is never told: she counts as turreted.
   */
  private mindFixed(sq: Squad, m: Man): boolean { return DR.tankmind.fixed.includes(m.ml ?? (sq.esc ? '' : this.b.vehicle(sq.u) ?? '')); }

  /**
   * Tank minds: what her gun watches while she has nothing to lay on. One look at a time: a far point on one bearing of
   * her arc ('mid', give or take 'half'), held her own DR.tankmind.watch.hold seconds, then another; the first only her
   * own moment after she had nothing to lay on (watch.first), so a platoon's guns never swing off together. A look she
   * holds she finishes, whatever she is asked next: every new point is a row sent to the 3D game, so not more often
   * than every few seconds. kind 1 her sector, 2 her side on the march, 3 dead ahead (kept before her nose as she
   * turns, twice as long). Returns the point, none till her watch begins.
   */
  private mindGaze(sq: Squad, m: Man, kind: 1 | 2 | 3, mid: number, half: number): [number, number] | undefined {
    const W = DR.tankmind.watch, M = m.mind!, t = this.b.t, id = sq.u.id, k = Math.round(t * 2);
    let L = M.look;
    if (!L) L = M.look = [t + W.first * roll(this.seed, id, m.n, k, 323), 0, 0, 0];
    const ahead = L[3] === 3 && (kind !== 3 || Math.abs(wrap(bearing(m.x, m.y, L[1], L[2]) - mid)) > 0.2);
    if (t < L[0] - 1e-6 && !ahead) return L[3] ? [L[1], L[2]] : undefined;
    const hold = (W.hold[0] + (W.hold[1] - W.hold[0]) * roll(this.seed, id, m.n, k, 322)) * (kind === 3 ? 2 : 1);
    const [fx, fy] = fwd(mid + (roll(this.seed, id, m.n, k, 324) * 2 - 1) * half), f = this.b.field;
    // (the point is kept on the field: nearer than watch.far where the field ends that way)
    let d = W.far;
    if (fx > 1e-6) d = Math.min(d, (f.w - 1 - m.x) / fx); else if (fx < -1e-6) d = Math.min(d, (1 - m.x) / fx);
    if (fy > 1e-6) d = Math.min(d, (f.h - 1 - m.y) / fy); else if (fy < -1e-6) d = Math.min(d, (1 - m.y) / fy);
    d = Math.max(20, d);
    L = M.look = [t + hold, Math.round((m.x + fx * d) * 10) / 10, Math.round((m.y + fy * d) * 10) / 10, kind];
    return [L[1], L[2]];
  }

  /**
   * Tank minds, once a think for each tank, after her drill: her hull and her gun apart. Her hull, standing: toward her
   * block's main threat, her thickest armour to him, a little her own way at each firing post (DR.tankmind.hull);
   * halted a moment on the fast advance, on her way. A gun fixed in her hull (mindFixed) turns with it to her aim, as
   * before (halted with nothing to lay on, her hull is her gun: on her own slice of watch.cone about the threat, as
   * assault guns are sited), and so do tanks on the road, routing or backing off. Her gun: on her target where her
   * drill laid it (her block fights; or, coming on, the enemy is within watch.reach; or, in a fight's place or pinned,
   * he is). Else she watches her own arc (mindGaze). Halted and not fighting, the platoon covers all round between
   * its tanks: the last of three or more the rear half, the others an equal share each of the front half (two tanks
   * or one, of all round), across the line as they stand; with the enemy within reach the front shares close to
   * watch.cone about him, each still her own slice. On the road and on the approach, alternate tanks left and right,
   * the last of three or more the rear. Routing or backing off, with nothing to watch, her gun comes back dead ahead
   * (a point ahead is sent: the 3D game holds the last aim it was given).
   */
  private mindLook(sq: Squad, m: Man, tanks: Man[], i: number, e: LandUnit | null): void {
    const M = m.mind;
    if (!M) return;
    const T = DR.tankmind, W = T.watch, u = sq.u, ph = M.ph ?? sq.phase, n = tanks.length;
    const coming = ph === 'deploy' || ph === 'bound', halted = !coming && ph !== 'column' && ph !== 'flee' && ph !== 'fallback';
    const fixed = this.mindFixed(sq, m);
    // (off her firing place's drill, whatever she had timed there is dropped)
    if (!halted) M.sit = M.place = M.rise = M.shift = undefined;
    const gap = e ? this.b.gap(u, e) : Infinity;
    const lay = ph === 'fallback' ? !!e : ph !== 'column' && ph !== 'flee' && !!e && (u.engaged !== undefined || (halted ? (ph === 'fight' || ph === 'pinned') && gap <= W.reach[0] : gap <= W.reach[1]));
    const lean = (roll(this.seed, u.id, m.n, m.post ?? 0, 321) * 2 - 1) * T.hull, front = e ? bearing(u.x, u.y, e.x, e.y) : u.face;
    // (her place across the line among the first f tanks: the leftmost the leftmost share)
    const [rx, ry] = right(front), at = (o: Man): number => (o.x - u.x) * rx + (o.y - u.y) * ry;
    const rank = (f: number): number => { let r = 0; for (let j = 0; j < f; j++) if (tanks[j] !== m && (at(tanks[j]) < at(m) || (at(tanks[j]) === at(m) && j < i))) r++; return r; };
    if (!(coming || halted) || (fixed && (lay || !halted))) M.hull = undefined;
    else if ((ph === 'deploy' || (ph === 'bound' && M.fast)) && M.go !== undefined) M.hull = M.h ?? sq.heading;
    // (a gun fixed in her hull, halted with nothing to lay on: her hull is her gun, on her own slice of the front; no aim)
    else if (fixed) M.hull = wrap(front + ((rank(n) + 0.5) / n - 0.5) * W.cone + lean);
    else M.hull = wrap((e ? bearing(m.x, m.y, e.x, e.y) : u.face) + lean);
    if (fixed) { M.look = undefined; if (!lay && halted) m.aim = undefined; return; }
    if (lay) { M.look = undefined; return; }
    let p: [number, number] | undefined;
    if (ph === 'flee' || ph === 'fallback') p = this.mindGaze(sq, m, 3, m.face, 0);
    else if (!halted) {
      const way = M.h ?? sq.heading;
      if (n >= 3 && i === n - 1) p = this.mindGaze(sq, m, 2, way + Math.PI, W.rear);
      else p = this.mindGaze(sq, m, 2, way + (i % 2 ? 1 : -1) * (W.side[0] + W.side[1]) / 2, (W.side[1] - W.side[0]) / 2);
    } else if (n >= 3 && i === n - 1) p = this.mindGaze(sq, m, 1, front + Math.PI, Math.PI / 2);
    else {
      const f = n >= 3 ? n - 1 : n, span = gap <= W.reach[0] ? W.cone : n >= 3 ? Math.PI : 2 * Math.PI;
      p = this.mindGaze(sq, m, 1, front + ((rank(f) + 0.5) / f - 0.5) * span, span / (2 * f));
    }
    if (p) { m.aim = p; M.fire = undefined; }
  }

  /**
   * Tank minds: hull down. A halted tank whose firing place has cover to sit behind (DR.tankmind.post.cover: a crest,
   * a wood's or a house's corner) backs down behind it, straight back post.down metres, at her own moment. In a
   * fight: post.see seconds after her round from this stand, her own each time, and only where the wait for her next
   * round is long enough for the trip down and up again (so she fires as many). From a place in the open she backs
   * down only by her own chance each round (post.open: none as shipped, for there is nothing there to sit behind and
   * the to and fro reads as a flick). Not fighting and the enemy far: her own share of post.idle seconds after she
   * came to stand. With the enemy near and no fight yet she stays up. True once she backs.
   */
  private mindSit(sq: Squad, m: Man, e: LandUnit | null): boolean {
    const M = m.mind!, T = DR.tankmind, P = T.post, u = sq.u, t = this.b.t, fight = u.engaged !== undefined && !!e;
    M.rise = undefined;
    if (M.sit || M.fire !== undefined || M.still === undefined) return false;
    let at: number;
    if (fight) {
      if (m.shot === undefined || m.shot < M.still) return false;
      if (!P.cover.includes(m.cover) && roll(this.seed, u.id, m.n, Math.round(m.shot * 10), 1, 326) >= P.open) return false;
      at = m.shot + P.see[0] + (P.see[1] - P.see[0]) * roll(this.seed, u.id, m.n, Math.round(m.shot * 10), 326);
      if (this.mindDue(sq, m, e!) - Math.max(t, at) < 2 * (P.down / P.pace + 0.7) + T.halt.lay[1] + 1) return false;
    } else if (P.cover.includes(m.cover) && (!e || this.b.gap(u, e) > T.watch.reach[0])) at = M.still + P.idle[0] + (P.idle[1] - P.idle[0]) * roll(this.seed, u.id, m.n, Math.round(M.still * 10), 325);
    else return false;
    // (her own step of the half second: mindStep runs her drill again then)
    if (t < at - 1e-6) { if (at < t + 0.5) M.rise = at; return false; }
    // Straight back along her hull where it is on the enemy, so she does not swing as she backs; inside her block's leash.
    const dir = this.threatDir(sq), [fx, fy] = fwd(m.face), back = fx * dir[0] + fy * dir[1] > 0.8 ? [fx, fy] : dir;
    const p = this.leash(sq, [m.x - back[0] * P.down, m.y - back[1] * P.down], -4);
    if (Math.hypot(p[0] - m.x, p[1] - m.y) < P.down - 0.5) return false;
    M.sit = 'down'; M.place = [m.x, m.y];
    m.spot = p; m.pose = 'reverse'; m.v = P.pace; m.until = t + 8;
    if (fight) m.aim = undefined;
    return true;
  }

  /**
   * Tank minds: a halted tank at her firing post, each think she is at her place (before the plain drill's own). A
   * shell that strikes her, or lands within DR.tankmind.post.near metres of her (her own chance to take it as her
   * cue, post.jump), moves her to her next post once she has been post.rest seconds at this one: she goes her own
   * moment after (react, quick), by the plain drill's shift (mindShift), open to every style while its block fights.
   * And hull down (mindSit): sitting behind her place she comes up to it at her own moment. In a fight, in time to
   * lay and fire her next round when it falls due; her block's fight begun with her down, her own moment after it
   * began. Not fighting, her own share of post.rise seconds after the enemy came within reach; while he is far she
   * stays down. True while she sits down behind her place or is on her way down or up: the plain drill leaves her be.
   */
  private mindPost(sq: Squad, m: Man, e: LandUnit | null, posts: boolean, calm: boolean): boolean {
    const M = m.mind!, T = DR.tankmind, P = T.post, u = sq.u, t = this.b.t, post = m.post ?? 0, k = Math.round(t * 2);
    if (!M.posted || M.posted[0] !== post) M.posted = [post, t];
    if (!posts) M.shift = undefined;
    else if (M.shift !== undefined) { if (t - M.shift > 4) M.shift = undefined; }
    else if (sq.hits && t - M.posted[1] >= P.rest[0] + (P.rest[1] - P.rest[0]) * roll(this.seed, u.id, m.n, post, 329)) {
      const H = sq.hits;
      for (let j = 0; j < H.length; j += 4) {
        if (H[j + 3] > t + 1e-6) continue; // (not landed yet)
        if (H[j + 2] !== m.k && (Math.hypot(H[j] - m.x, H[j + 1] - m.y) >= P.near || roll(this.seed, u.id, m.n, Math.round(H[j] * 10), Math.round(H[j + 3] * 10), 327) >= P.jump)) continue;
        M.shift = t + (T.react[0] + (T.react[1] - T.react[0]) * roll(this.seed, u.id, m.n, k, 328)) * T.quick[this.band(u)];
        break;
      }
    }
    // (up at her place again, or sent elsewhere meanwhile: her sitting is over)
    if (M.sit && (M.sit === 'up' || !calm || Math.abs(Math.hypot(m.spot[0] - M.place![0], m.spot[1] - M.place![1]) - P.down) > 0.6)) M.sit = M.place = M.rise = undefined;
    const fight = u.engaged !== undefined && !!e;
    if (!M.sit) return calm && !fight && this.mindSit(sq, m, e);
    // Down behind her place. A near miss there: out to her next post from where she sits.
    if (M.shift !== undefined && t >= M.shift - 1e-6) { M.sit = M.place = M.rise = undefined; return false; }
    m.v = 0; m.pose = 'halt';
    let at = Infinity;
    if (fight) {
      at = this.mindDue(sq, m, e!) - (P.down / P.pace + 0.7 + T.halt.lay[1] + 1);
      if (m.shot === undefined || m.shot < (sq.fightAt ?? 0)) at = Math.max(at, (sq.fightAt ?? t) + T.react[1] * T.quick[this.band(u)] * roll(this.seed, u.id, m.n, Math.round((sq.fightAt ?? t) * 2), 330));
    } else if (e && this.b.gap(u, e) <= T.watch.reach[0]) at = M.rise ?? t + P.rise * roll(this.seed, u.id, m.n, k, 330);
    // (coming up is her commander's order, as backing down was: taken at once, her least time in one direction or no)
    if (t >= at - 1e-6) { M.sit = 'up'; M.rise = undefined; m.spot = [M.place![0], M.place![1]]; m.pose = 'drive'; m.v = P.pace; m.until = t + 8; M.rev = undefined; M.keep = t + this.mindKeep(sq, m); }
    else M.rise = fight ? (at < t + 0.5 ? at : undefined) : at < Infinity ? at : undefined;
    m.aim = fight || !e ? undefined : [e.x, e.y];
    return true;
  }

  /** Tank minds: whether her own moment to leave her firing post for her next has come (a near miss or a hit: mindPost). */
  private mindShift(m: Man): boolean {
    const M = m.mind!;
    if (M.shift === undefined || this.b.t < M.shift - 1e-6) return false;
    M.shift = undefined;
    return true;
  }

  /**
   * Tank minds: an armoured car, an AA vehicle, a jeep or a motorcycle, each half second, in place of the plain
   * drill's drive. Her own place, a little off the plain drill's (DR.tankmind.out.spot), and while her block drives
   * her own share of out.trail seconds of its pace further back, closed up when it halts: so each stops at her own
   * moment. Standing, she drives after her place only once it has moved off her own slack from her (out.start), and
   * then at her own moment (react, quick: mindTick moves her off at her own step); the same when her place leaps (the
   * screen called in or sent out), going on as she was till then. On the move she keeps pace with her place (follow)
   * or, far from it or her block halted, drives straight for it and halts there. Standing, her nose is toward the
   * threat her own way (up to twice a tank's lean: she has no thick front to show him). A car's turret lays on her
   * own point of the enemy block within out.sight, and further off watches her own flank; an AA vehicle's gun,
   * standing, watches her own share of all round (mindGaze), and comes back dead ahead on the move. Jeeps and
   * motorcycles have no aim, as before.
   */
  private mindOut(sq: Squad, m: Man, i: number, s: [number, number], e: LandUnit | null, near: number): void {
    const T = DR.tankmind, O = T.out, own = Object.getPrototypeOf(sq.u) as LandUnit, t = this.b.t, id = own.id, M = (m.mind ??= {}), V = sq.speed;
    const r = (...ns: number[]): number => roll(this.seed, id, m.n, ...ns);
    const [rx, ry] = right(own.face), [fx, fy] = fwd(own.face), a = (r(331) * 2 - 1) * O.spot[0], l = (r(332) * 2 - 1) * O.spot[1] - (V > 0.4 ? V * O.trail * r(1, 334) : 0);
    s = [s[0] + rx * a + fx * l, s[1] + ry * a + fy * l];
    const d = Math.hypot(s[0] - m.x, s[1] - m.y), going = m.v > 0 && MOVING.has(m.pose), pace = O.pace[0] + (O.pace[1] - O.pace[0]) * r(334);
    const straight = clamp((V * 1.2 + d * 0.25) * pace, 3, 12);
    if (!going && d <= O.start[0] + (O.start[1] - O.start[0]) * r(335)) M.off = M.to = undefined;
    else if (M.off === undefined && (!going || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > O.far)) M.off = t + (T.react[0] + (T.react[1] - T.react[0]) * r(Math.round(t * 2), 333)) * T.quick[this.band(own)];
    if (M.off !== undefined) {
      // Her own moment not yet come: she goes on as she was, the order kept for her step (mindTick).
      if (t < M.off - 1e-6) M.to = [s[0], s[1], straight];
      else { M.off = M.to = undefined; m.spot = s; m.slotAt = s; m.pose = 'drive'; m.v = straight; m.until = t + 1; }
    } else if (!going) m.slotAt = s;
    else if (d > O.far || V <= 0.4) {
      if (d <= 1.5) { m.spot = [m.x, m.y]; m.v = 0; m.pose = 'halt'; } else { m.spot = s; m.pose = 'drive'; m.v = straight; m.until = t + 1; }
      m.slotAt = s;
    } else this.follow(sq, m, s, 'drive', pace);
    M.hull = fighting(own) ? wrap((e ? bearing(m.x, m.y, e.x, e.y) : own.face) + (r(321) * 2 - 1) * T.hull * 2) : undefined;
    if (sq.esc === 'cars') {
      if (e && near < O.sight) { M.look = undefined; m.aim = toWorld(e, e.face, (r(0, 339) - 0.5) * e.w * 0.7, (r(1, 339) - 0.5) * e.d * 0.7); }
      else m.aim = this.mindGaze(sq, m, 2, own.face + (i % 2 ? 1 : -1) * (O.arc[0] + O.arc[1]) / 2, (O.arc[1] - O.arc[0]) / 2);
    } else if (sq.esc === 'aa') {
      const n = sq.men.length;
      m.aim = m.v > 0 && MOVING.has(m.pose) ? this.mindGaze(sq, m, 3, m.face, 0) : this.mindGaze(sq, m, 1, own.face + ((i + 0.5) / n) * 2 * Math.PI, Math.PI / n);
    }
  }

  /**
   * Tank minds: a lorry's, a carrier's, a tractor's or a gun's own place, a little off the plain drill's
   * (DR.tankmind.lean, metres across and along). In column ('col' her number in it, none where they stand in a row)
   * each keeps lean's third number of seconds of her block's pace further behind the one ahead, and up to as much
   * again of her own: the column strings out at speed and closes up at the halt, each rolling on to her place and
   * stopping at her own moment.
   */
  private mindLean(sq: Squad, v: Man, s: [number, number], way: number, col = -1): [number, number] {
    const L = DR.tankmind.lean, id = sq.u.id, [rx, ry] = right(way), [fx, fy] = fwd(way);
    const a = (roll(this.seed, id, v.n, 336) * 2 - 1) * L[0];
    const l = (roll(this.seed, id, v.n, 337) * 2 - 1) * L[1] - (col >= 0 && sq.speed > 0.4 ? sq.speed * L[2] * (col + roll(this.seed, id, v.n, 1, 337)) : 0);
    return [s[0] + rx * a + fx * l, s[1] + ry * a + fy * l];
  }

  /**
   * Tank minds: a lorry, a carrier, a tractor or a gun told to drive from standing moves off at her own moment
   * (DR.tankmind.react, quick; 'lead', the first of a column, at once), never all in one instant. Called once the
   * plain drill has given her order: till her moment she stands as she stood, her order kept for her own step
   * (mindTick), and her place as it was when she was last left be, so the plain drill gives the order again.
   */
  private mindOff(sq: Squad, v: Man, lead = false): void {
    const T = DR.tankmind, t = this.b.t, M = (v.mind ??= {});
    // (first seen: standing where the battle opens, she is as one left standing)
    if (!M.slot) { M.slot = [v.x, v.y]; if ((v.went ?? 0) <= 0.05) M.still = t; }
    if (!(v.v > 0 && MOVING.has(v.pose))) { M.still ??= t; M.off = M.to = undefined; M.slot = [v.slotAt[0], v.slotAt[1]]; return; }
    if (M.still !== undefined) {
      if (M.off === undefined) M.off = lead ? t : t + (T.react[0] + (T.react[1] - T.react[0]) * roll(this.seed, sq.u.id, v.n, Math.round(t * 2), 333)) * T.quick[this.band(sq.u)];
      if (t < M.off - 1e-6) {
        M.to = [v.spot[0], v.spot[1], v.v];
        v.v = 0; v.spot = [v.x, v.y]; v.pose = v.role === 'gun' ? 'towed' : 'parked'; v.slotAt = [M.slot[0], M.slot[1]];
        return;
      }
    }
    M.still = M.off = M.to = undefined; M.slot = [v.slotAt[0], v.slotAt[1]];
  }

  /**
   * Tank minds: a battery limbered up. Self-propelled guns, and guns each behind her own tractor ('own'), keep each
   * her own place in the column (mindLean) and move off gun by gun (mindOff), her tractor with her. Guns in one train
   * behind the battery's one tractor go as one, as before: they are one vehicle's tow.
   */
  private mindTow(sq: Squad, guns: Man[], own: boolean): void {
    const u = sq.u, h = sq.heading;
    guns.forEach((g, i) => {
      if (!own && !g.ml) return;
      this.follow(sq, g, this.mindLean(sq, g, toWorld(u, h, 0, -10 - i * (own ? 26 : 18)), h, i), 'towed');
      if (g.v === 0) g.pose = 'towed';
      this.mindOff(sq, g, i === 0);
    });
  }

  /**
   * Tank minds: a battery's salvo as a ripple, whatever its style. A gun the plain drill would fire on this half second
   * fires at her own moment instead: within DR.tankmind.volley.own seconds, her own each time, and never within
   * volley.apart seconds of another gun of her battery (mindTick fires her at her own step). The drill book's ranging
   * gun ('solo') fires alone, as before. Her wait is not added to her next round's: that falls due by the plain drill's
   * clock, so a battery fires about as many. Wraps the plain drill's rule and gives the one to use.
   */
  private mindVolley(sq: Squad, guns: Man[], plain: (g: Man, i: number) => boolean, solo?: Man): (g: Man, i: number) => boolean {
    const V = DR.tankmind.volley, t = this.b.t, k = Math.round(t * 2);
    return (g, i) => {
      // (the ranging gun fires alone, at once: a mate's round timed within volley.apart of it waits that long)
      if (g === solo) {
        if (!plain(g, i)) return false;
        for (const o of guns) if (o !== g && o.mind?.fire !== undefined && Math.abs(o.mind.fire - t) < V.apart) o.mind.fire = t + V.apart;
        return true;
      }
      if (g.mind?.fire !== undefined || g.hidden) return false;
      // (her round is due by the plain drill's own clock, as if her last had gone when it was called for: as many rounds)
      const last = g.shot;
      if (last !== undefined && g.mind?.lag) g.shot = last - g.mind.lag;
      const due = plain(g, i);
      g.shot = last;
      if (!due) return false;
      let at = t + V.own * roll(this.seed, sq.u.id, g.n, k, 338);
      for (let n = 0, again = true; again && n < 8; n++) {
        again = false;
        for (const o of guns) {
          if (o === g) continue;
          for (const ot of [o.mind?.fire, o.shot]) if (ot !== undefined && Math.abs(at - ot) < V.apart - 1e-6) { at = ot + V.apart; again = true; }
        }
      }
      const M = (g.mind ??= {});
      M.fire = at; M.lag = at - t; g.due = undefined;
      return false;
    };
  }

  /**
   * Tank minds, every step (walk; mindStep for a block's cars and AA vehicles): what one of the other vehicles or a
   * gun has timed for this step. Her own moment to move off comes: she takes up the order kept for her (mindOff,
   * mindOut). A gun's own moment in her battery's salvo: she fires, if she still stands in her place with her gun
   * laid, her crew up and her block still fighting (mindVolley).
   */
  private mindTick(sq: Squad, m: Man): void {
    const M = m.mind!, t = this.b.t;
    if (M.to && M.off !== undefined && t >= M.off - 1e-6) {
      m.spot = [M.to[0], M.to[1]]; m.v = M.to[2]; m.pose = m.role === 'gun' ? 'towed' : 'drive'; m.until = t + 1;
      M.to = M.still = undefined;
    }
    // (a car at her place on the road: never left driving on the spot)
    if (sq.esc && m.v > 0 && m.pose === 'drive' && Math.hypot(m.spot[0] - m.x, m.spot[1] - m.y) < 0.05) { m.v = 0; m.pose = 'halt'; }
    if (m.role === 'gun' && M.fire !== undefined && t >= M.fire - 1e-6) {
      if (m.v === 0 && !m.hidden && m.aim && (m.pose === 'deployed' || m.pose === 'fire') && sq.phase === 'battery' && sq.u.engaged !== undefined && live(sq.u)) { m.shot = t; m.pose = 'fire'; }
      M.fire = undefined;
    }
  }

  private tanks(sq: Squad, only?: Man): void {
    const u = sq.u, b = this.b, t = b.t, N = this.nums(u), e = this.threat(u), dir = this.threatDir(sq), ph = sq.phase, h = sq.heading;
    const tanks = this.up(sq).filter((m) => m.role === 'tank');
    const lone = u.aloneAt !== undefined && t - u.aloneAt < 2;
    const v = Math.max(1, sq.speed);
    // With its own foot, on close ground: the foot leads, the tanks keep behind its line.
    const close = CLOSE.includes(groundOf(b.field, u.x, u.y));
    const foot = b.units.filter((f) => f.side === u.side && fighting(f) && FOOT_KINDS.includes(f.kind) && b.gap(u, f) < 150);
    const [fx, fy] = fwd(u.face);
    let behind = Infinity;
    if (close && foot.length) for (const f of foot) behind = Math.min(behind, (f.x - u.x) * fx + (f.y - u.y) * fy + f.d / 2 - 15);
    if (this.tankmind && !only) this.mindBlock(sq);
    tanks.forEach((m, i) => {
      // (tank minds: one tank's drill run at her commander's own moment, between two thinks of her block: mindStep)
      if (only && m !== only) return;
      // (tank minds: each commander acts on his block's phase and its way as he has taken them up, at his own moment)
      const ph = this.tankmind ? this.mindPhase(sq, m, i === 0) : sq.phase, h = this.tankmind ? this.mindWay(sq, m) : sq.heading;
      // (hatches: each commander by his own nerve, alarm and hatchNow; else the block's as one, the enemy inside 120 m)
      if (!this.hatches) m.buttoned = lone || (e !== null && b.gap(u, e) < 120) || m.cmdDown !== undefined || undefined;
      else if (lone || (e !== null && b.gap(u, e) < DR.hatch.near[this.band(u)] * (DR.hatch.own[0] + (DR.hatch.own[1] - DR.hatch.own[0]) * roll(this.seed, u.id, m.n, 173)))) this.alarm(sq, m, t);
      // The drill book: her machine guns on the enemy's foot while her block fights it.
      m.coax = (this.byBook && !!e && FOOT_KINDS.includes(e.kind) && u.engaged !== undefined && b.gap(u, e) < 600) || undefined;
      const n = sq.u.figures0 - 1 - m.k;
      const [wa, wl] = wreckOffset(u.id, n, u.w, u.d);
      if (ph === 'flee') { m.aim = undefined; this.follow(sq, m, toWorld(u, u.face, wa, wl), 'drive', 1.1); return; }
      if (ph === 'column') { m.aim = undefined; this.follow(sq, m, toWorld(u, h, 0, u.d / 2 - 10 - i * 22), 'drive'); return; }
      let l = wl;
      if (behind < Infinity) l = Math.min(l, behind);
      const s = toWorld(u, u.face, wa, l);
      if (ph === 'fallback') {
        // Backing off, guns still on the enemy.
        this.follow(sq, m, s, 'reverse'); m.aim = e ? [e.x, e.y] : undefined; return;
      }
      if (ph === 'deploy' || (ph === 'bound' && (this.tankmind ? !!m.mind!.fast : v >= 3.5))) {
        // (tank minds: move, halt briefly to fire, move on, each her own moment, pairs covering each other)
        if (this.tankmind) { this.mindRoll(sq, m, tanks, i, s, e, ph === 'bound', h); return; }
        // All on, a tank halting a moment to fire where one is in reach: for a shot (1.5 s of each 4, her own), and never
        // while she is behind her place, or she would fall far behind a block going on at 10 m a second.
        const [hx, hy] = fwd(h), lag = (s[0] - m.x) * hx + (s[1] - m.y) * hy, at = t + roll(this.seed, u.id, m.n, 41) * 4;
        const halt = ph === 'bound' && e && b.gap(u, e) < 600 && lag < 6 && at % 4 < 1.5 && roll(this.seed, u.id, m.n, Math.floor(at / 4)) < 0.25;
        if (halt) { m.spot = [m.x, m.y]; m.v = 0; m.pose = this.tankShot(sq, m, e, N) ? 'fire' : 'halt'; m.aim = this.tankAim(sq, m, e); }
        else { this.follow(sq, m, s, 'drive'); m.aim = e && b.gap(u, e) < 900 ? [e.x, e.y] : undefined; }
        return;
      }
      if (ph === 'bound') {
        // (tank minds: within each pair one covers while her partner moves, then the other way about)
        if (this.tankmind) { this.mindBound(sq, m, tanks, i, s, e, dir, N, h); return; }
        // Slow going: the pairs leapfrog.
        const pair = m.k < 2 ? 0 : 1, cyc = N.tankRush + N.tankHalt, off = pair ? cyc / 2 : 0;
        const moving = (t + off + 100 * cyc) % cyc < N.tankRush;
        if (moving) {
          if ((m.pose !== 'drive' && m.pose !== 'reverse') || t >= m.until) { const [hx, hy] = fwd(h); this.go(sq, m, [s[0] + hx * v * cyc * 0.45, s[1] + hy * v * cyc * 0.45], 'drive', v * cyc / N.tankRush, t + N.tankRush, dir); }
          m.aim = e ? [e.x, e.y] : undefined;
        } else if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 2 || (m.pose !== 'drive' && m.pose !== 'reverse')) {
          m.v = 0; m.spot = [m.x, m.y]; m.pose = e && this.tankShot(sq, m, e, N) ? 'fire' : 'halt'; m.aim = this.tankAim(sq, m, e);
        }
        return;
      }
      // Halted: in a fight, holding, shaken. Her firing place near her own: hull-down where the ground gives one. In a
      // fight she fights from alternate posts (every army but a mass): a few shots from one, then out of sight backward,
      // across behind the line and up again at the next, to one side of her own place and then the other.
      const shaken = ph === 'pinned' || (this.tankmind ? !!m.mind!.shaken : u.state === 'wavering');
      // (tank minds: every style's tanks may shift posts while their block fights, on a near miss or a hit: mindPost)
      const posts = (N.scoot > 0 || this.tankmind) && !shaken && !lone && u.engaged !== undefined;
      if (!posts && m.cyc) { m.cyc = undefined; m.post = 0; }
      const T = DR.tanks, [rx, ry] = right(u.face), off = [0, 1, 0, -1][(m.post ?? 0) % 4] * (m.k % 2 ? 1 : -1) * T.post;
      const want: [number, number] = shaken ? [s[0] - dir[0] * 25, s[1] - dir[1] * 25] : [s[0] + rx * off, s[1] + ry * off];
      if (Math.hypot(want[0] - m.slotAt[0], want[1] - m.slotAt[1]) > (m.cyc ? 30 : 10) || (m.pose === 'drive' && t >= m.until && !m.cyc)) {
        m.cyc = undefined;
        this.go(sq, m, want, shaken ? 'reverse' : 'drive', shaken ? 2 : 4, t + 4, dir);
        m.aim = e ? [e.x, e.y] : undefined;
        return;
      }
      if (m.cyc) {
        const there = Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 2;
        m.aim = e ? [e.x, e.y] : undefined;
        if (!there && t < m.until) return;
        if (m.cyc === 'back') {
          // Out of sight: across behind the line to the back of her next post.
          m.post = (m.post ?? 0) + 1;
          const o = [0, 1, 0, -1][m.post % 4] * (m.k % 2 ? 1 : -1) * T.post, p: [number, number] = [s[0] + rx * o, s[1] + ry * o];
          m.cyc = 'over'; m.slotAt = p; m.spot = this.leash(sq, [p[0] - dir[0] * T.back, p[1] - dir[1] * T.back], -4); m.pose = 'drive'; m.v = 3.5; m.until = t + 12;
          return;
        }
        if (m.cyc === 'over') {
          // Up to it: hull-down where the ground gives one.
          const best = this.seek(sq, m, m.slotAt, dir, sq.men);
          m.cyc = 'up'; m.spot = [best.x, best.y]; m.cover = best.kind; m.pose = 'drive'; m.v = 2.5; m.until = t + 8;
          return;
        }
        m.cyc = undefined; m.shots = 0;
      }
      if (m.pose === 'reverse' && t < m.until) return;
      if (m.pose === 'reverse') { m.spot = [m.slotAt[0], m.slotAt[1]]; const best = this.seek(sq, m, m.slotAt, dir, sq.men); m.spot = [best.x, best.y]; m.cover = best.kind; m.pose = 'drive'; m.v = 3; m.until = t + 4; return; }
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1.5) return;
      // (tank minds: her firing post's own cues, and hull down behind it: mindPost)
      if (this.tankmind && this.mindPost(sq, m, e, posts, !shaken && !lone)) return;
      m.v = 0;
      if (!e || (u.engaged === undefined && b.gap(u, e) > 600)) { m.pose = 'halt'; m.aim = e ? [e.x, e.y] : undefined; return; }
      m.aim = this.tankAim(sq, m, e);
      // Her shots from this post told, and the last of them seen go: out of sight backward, to move to her next.
      const need = T.shots[0] + Math.floor(roll(this.seed, u.id, m.n, m.post ?? 0, 23) * (T.shots[1] - T.shots[0] + 1));
      if (posts && ((N.scoot > 0 && (m.shots ?? 0) >= need && m.shot !== undefined && t - m.shot >= 1.5) || (this.tankmind && this.mindShift(m)))) {
        m.cyc = 'back'; m.spot = this.leash(sq, [m.x - dir[0] * T.back, m.y - dir[1] * T.back], -4); m.pose = 'reverse'; m.v = 2.5; m.until = t + 8;
        return;
      }
      // (tank minds: behind cover she backs down between her rounds: mindSit)
      if (this.tankmind && this.mindSit(sq, m, e)) return;
      // (tank minds: her round goes at her own step, from her halt: mindShot)
      if (u.engaged !== undefined && (this.tankmind ? this.mindShot(sq, m, e, true) : this.tankShot(sq, m, e, N))) { m.pose = 'fire'; m.shots = (m.shots ?? 0) + 1; }
      else m.pose = 'halt';
    });
    // A tank that has just fired shows it a moment (the 3D plays the round by its stamp).
    for (const m of tanks) if (m.pose === 'halt' && m.shot !== undefined && t - m.shot < 1.5) m.pose = 'fire';
    // (tank minds: her hull and her gun apart, and her own arc to watch while she has nothing to lay on: mindLook)
    if (this.tankmind) tanks.forEach((m, i) => { if (!only || m === only) this.mindLook(sq, m, tanks, i, e); });
  }

  /**
   * Whether this tank fires now (each about every 6 to 10 s while her block fights; the drill book: at foot, a round
   * every BOOK.tank.every seconds at a weapon, her machine guns doing the rest).
   */
  private tankShot(sq: Squad, m: Man, e: LandUnit | null, N: Nums): boolean {
    if (!e || sq.u.engaged === undefined) return false;
    const r = roll(this.seed, sq.u.id, m.n, 31), T = BOOK.tank.every;
    const t = this.b.t, every = this.byBook && e.kind !== 'arm' ? T[0] + (T[1] - T[0]) * r : 6 + 4 * r;
    if (m.shot !== undefined && t - m.shot < every) return false;
    m.shot = t;
    void N;
    return true;
  }

  // ---- Guns ---------------------------------------------------------------------------------------------------------

  /**
   * A battery. Moving, its guns are towed behind its tractor in column. Standing, they unlimber in a line behind the
   * block's middle, each in the best place near its own: behind a crest (firing over it at what it cannot see) or a
   * wood's edge. They fire by the style's drill: together, as fast as each can, or one after another down the line;
   * over open sights when the enemy is close or its tanks are near. Shelled, the crews go down a moment. A spotter goes
   * up with the friendly foot nearest the target while the guns fire far.
   */
  private guns(sq: Squad): void {
    const u = sq.u, b = this.b, t = b.t, N = this.nums(u), dir = this.threatDir(sq), h = sq.heading;
    const e = u.aim !== undefined ? b.unit(u.aim) ?? null : u.engaged !== undefined ? b.unit(u.engaged) ?? null : null;
    const guns = this.up(sq).filter((m) => m.role === 'gun'), tractor = sq.veh[0];
    // Drawn as sections, each gun has her own tractor (her crew aboard it: pairs), just ahead of her.
    const own = sq.tier >= 1 && sq.veh.length > 1;
    if (sq.phase === 'limbered' || sq.phase === 'flee' || sq.phase === 'fallback') {
      guns.forEach((g, i) => { g.aim = undefined; this.follow(sq, g, toWorld(u, h, 0, -10 - i * (own ? 26 : 18)), 'towed'); if (g.v === 0) g.pose = 'towed'; });
      // (tank minds: gun by gun, each her own place in the column and her own moment to move off: mindTow)
      if (this.tankmind) this.mindTow(sq, guns, own);
      if (own) guns.forEach((g) => { if (g.ml) return; const v = this.lorryOf(sq, g); this.follow(sq, v, toWorld(g, g.face, 0, 7), 'drive'); });
      else if (tractor) { this.follow(sq, tractor, toWorld(u, h, 0, 8), 'drive'); }
      this.spotter(sq, null, false); // the pair stays where it is, watching, till the guns fire again
      return;
    }
    sq.veh.forEach((tr, i) => {
      if (!own && i > 0) return;
      const s0 = toWorld(u, u.face, own ? (i - (sq.veh.length - 1) / 2) * 16 : 0, -u.d / 2 - 50), s = this.tankmind ? this.mindLean(sq, tr, s0, u.face) : s0;
      if (Math.hypot(s[0] - tr.slotAt[0], s[1] - tr.slotAt[1]) > 20) { tr.spot = s; tr.slotAt = s; tr.pose = 'drive'; tr.v = 4; } else if (Math.hypot(tr.x - tr.spot[0], tr.y - tr.spot[1]) < 2) { tr.pose = 'parked'; tr.v = 0; }
    });
    const firing = u.engaged !== undefined && !!e;
    const direct = !!e && (b.gap(u, e) < LAND.guns.closeAt || (e.kind === 'arm' && b.gap(u, e) < 800));
    const hit = t - sq.shelledAt < DR.pin.shelled || sq.harm > DR.pin.harm;
    // The salvo clock; the drill book: over the crest, its spotter's mission (ranging, the bracket, fire for effect).
    let fireNow: (g: Man, i: number) => boolean = () => false;
    const ranging = this.byBook && firing && !direct && e ? this.mission(sq, guns, e, 1, N) : undefined;
    if (this.byBook && firing && !direct) fireNow = (g) => g.due !== undefined && t >= g.due;
    else if (firing && t >= sq.salvoAt) {
      if (N.salvo === 'battery') { sq.salvoAt = t + N.gunEvery; fireNow = () => true; }
      else if (N.salvo === 'ripple') { const i0 = sq.ripple % Math.max(1, guns.length); sq.ripple++; sq.salvoAt = t + N.gunEvery / Math.max(1, guns.length); fireNow = (_g, i) => i === i0; }
      else { sq.salvoAt = t + 0.5; fireNow = (g) => g.shot === undefined || t - g.shot >= N.gunEvery * (0.7 + 0.3 * roll(this.seed, u.id, g.n, Math.floor(t / 10))); }
    }
    // (tank minds: the salvo as a ripple, each gun her own moment: mindVolley)
    if (this.tankmind) fireNow = this.mindVolley(sq, guns, fireNow, ranging?.[0]);
    guns.forEach((g, i) => {
      const [a, l] = this.slot(sq, g, 'line');
      const s = toWorld(u, u.face, a, l);
      if (Math.hypot(s[0] - g.slotAt[0], s[1] - g.slotAt[1]) > 12) { this.go(sq, g, s, 'towed', 3, t + 4, dir); g.aim = undefined; return; }
      if (Math.hypot(g.x - g.spot[0], g.y - g.spot[1]) > 1.5) return;
      g.v = 0;
      g.hidden = hit && roll(this.seed, u.id, g.n, Math.floor(t)) < 0.5 ? true : undefined; // the crew down a moment
      if (firing && fireNow(g, i) && !g.hidden) { g.shot = t; g.pose = 'fire'; g.due = undefined; }
      else if (g.pose !== 'fire' || (g.shot !== undefined && t - g.shot > 2.5)) g.pose = 'deployed';
      g.aim = !e ? undefined : ranging?.[0] === g ? ranging[1] : this.liftAim(sq, g, e, 'gun');
      g.buttoned = direct || undefined; // over open sights
    });
    // (tank minds: a tractor or a gun told to move moves off at her own moment: mindOff)
    if (this.tankmind) for (const v of [...sq.veh, ...guns]) this.mindOff(sq, v);
    this.spotter(sq, e, firing && !!e && b.gap(u, e) > DR.ranges.fo);
  }

  /**
   * A support team (release 2; Brian, 21 November 2026, 03:49Z: "attack from the back of the battle"): a machine-gun
   * team's guns on their tripods or a mortar team's tubes, each with its crew, carried by hand (no tractor). On the move
   * the crews carry them in file behind the block's middle; standing, they set up in a line, each in the best place near
   * its own. The machine guns fire on whatever their block fights, each gun its own long bursts (Drill.fire); the mortars
   * fire by the style's drill as a battery does, faster than field guns, always over the crest, and send a spotter up
   * with the foot when they fire far. Shelled, the crews go down a moment.
   */
  private teams(sq: Squad): void {
    const u = sq.u, b = this.b, t = b.t, N = this.nums(u), dir = this.threatDir(sq), h = sq.heading, T = DR.teams, mg = u.team === 'mg';
    const e = u.aim !== undefined ? b.unit(u.aim) ?? null : u.engaged !== undefined ? b.unit(u.engaged) ?? null : null;
    const crews = this.up(sq).filter((m) => CREWED.has(m.role));
    if (sq.phase === 'limbered' || sq.phase === 'flee' || sq.phase === 'fallback') {
      crews.forEach((g, i) => { g.aim = undefined; g.hidden = undefined; this.follow(sq, g, toWorld(u, h, i % 2 ? 1.5 : -1.5, -4 - i * T.file), sq.phase === 'flee' ? 'flee' : 'walk'); });
      this.spotter(sq, null, false); // a mortar team's pair stays where it is, watching, till the tubes fire again
      return;
    }
    const firing = u.engaged !== undefined && !!e;
    const hit = t - sq.shelledAt < DR.pin.shelled || sq.harm > DR.pin.harm;
    // The mortars' salvo clock: a battery's, at the mortar's quicker rate.
    let fireNow: (g: Man, i: number) => boolean = () => false;
    const ranging = this.byBook && !mg && firing && e ? this.mission(sq, crews, e, BOOK.mission.mortar, N) : undefined;
    if (this.byBook && !mg && firing) fireNow = (g) => g.due !== undefined && t >= g.due;
    else if (!mg && firing && t >= sq.salvoAt) {
      const every = N.gunEvery * T.mortar.every;
      if (N.salvo === 'battery') { sq.salvoAt = t + every; fireNow = () => true; }
      else if (N.salvo === 'ripple') { const i0 = sq.ripple % Math.max(1, crews.length); sq.ripple++; sq.salvoAt = t + every / Math.max(1, crews.length); fireNow = (_g, i) => i === i0; }
      else { sq.salvoAt = t + 0.5; fireNow = (g) => g.shot === undefined || t - g.shot >= every * (0.7 + 0.3 * roll(this.seed, u.id, g.n, Math.floor(t / 10))); }
    }
    crews.forEach((g, i) => {
      const [a, l] = this.slot(sq, g, 'line');
      const s = toWorld(u, u.face, a, l);
      if (Math.hypot(s[0] - g.slotAt[0], s[1] - g.slotAt[1]) > 12) { this.go(sq, g, s, 'run', T.pace, t + 4, dir); g.aim = undefined; return; }
      if (Math.hypot(g.x - g.spot[0], g.y - g.spot[1]) > 1.5) return;
      g.v = 0;
      g.hidden = hit && roll(this.seed, u.id, g.n, Math.floor(t)) < 0.5 ? true : undefined; // the crew down a moment
      if (mg) g.pose = firing && !g.hidden ? 'fire' : 'deployed';
      else if (firing && fireNow(g, i) && !g.hidden) { g.shot = t; g.pose = 'fire'; g.due = undefined; }
      else if (g.pose !== 'fire' || (g.shot !== undefined && t - g.shot > 2.5)) g.pose = 'deployed';
      g.aim = !e ? undefined : ranging?.[0] === g ? ranging[1] : this.liftAim(sq, g, e, mg ? 'mg' : 'mortar');
    });
    this.spotter(sq, e, !mg && firing && !!e && b.gap(u, e) > DR.ranges.fo);
  }

  /**
   * A battery's or a mortar team's mission at block e (the drill book, tactics.ts missionStep): steps it, sets each
   * gun's time for the salvo called (all within a second or two; rippled down the line where its style ripples), and
   * gives the gun to fire a ranging round and where it lands (over or short of the target along the line of fire).
   * k: the times' share (a mortar team's quicker).
   */
  private mission(sq: Squad, guns: Man[], e: LandUnit, k: number, N: Nums): [Man, [number, number]] | undefined {
    const u = sq.u, t = this.b.t, z = Math.round(t * 2);
    const [ms, shoot] = missionStep(sq.ms, e.id, t, k, (i) => roll(this.seed, u.id, z, i, 81));
    sq.ms = ms;
    const ready = guns.filter((g) => !g.hidden && Math.hypot(g.x - g.spot[0], g.y - g.spot[1]) <= 1.5);
    if (!shoot || !ready.length) return undefined;
    if (shoot === 'salvo') {
      ready.forEach((g, i) => { g.due = t + (N.salvo === 'ripple' ? i * 0.5 : 1.5 * roll(this.seed, u.id, g.n, z, 82)); });
      return undefined;
    }
    const g = ready[0], dx = e.x - g.x, dy = e.y - g.y, d = Math.hypot(dx, dy) || 1, o = (shoot === 'over' ? 1 : -1) * (e.d / 2 + BOOK.mission.range);
    g.due = t;
    return [g, [clamp(e.x + (dx / d) * o, 1, this.b.field.w - 1), clamp(e.y + (dy / d) * o, 1, this.b.field.h - 1)]];
  }

  /**
   * The battery's spotter and his signaller: with the friendly foot block nearest the guns' target, on the highest
   * ground or the tallest house within its stretch, lying with his glasses on the target. Where no friendly foot is near
   * the target, at the battery's own front, on its highest ground. Out once the guns first fire far, the pair stays out
   * to the end: when the shoot moves to another block, or its host goes on, they go up to the new place (running, the
   * last 30 m crawling) and are never made anew; while the guns are quiet they lie where they are, watching.
   */
  private spotter(sq: Squad, e: LandUnit | null, on: boolean): void {
    const u = sq.u, b = this.b, t = b.t, f = b.field;
    if (!on || !e) {
      sq.foOn = undefined;
      if (sq.fo) for (const m of sq.fo) if (m.v === 0) { m.aim = undefined; m.pose = m.k === 200 ? 'observe' : 'wait'; }
      return;
    }
    const friends = b.units.filter((x) => x.side === u.side && fighting(x) && FOOT_KINDS.includes(x.kind) && b.gap(x, e) < 900);
    let host = friends.reduce<LandUnit | null>((m, x) => (!m || b.gap(x, e) < b.gap(m, e) ? x : m), null);
    // He keeps to the block he is with unless another is clearly nearer the target: he never runs between two abreast.
    const cur = friends.find((x) => x.id === sq.foWith);
    if (cur && host && b.gap(cur, e) < b.gap(host, e) + 100) host = cur;
    const base = host ?? u;
    const gone = sq.foAt ? Math.hypot(base.x - sq.foAt[0], base.y - sq.foAt[1]) : Infinity;
    if (!sq.fo || sq.foWith !== base.id || sq.foOn !== e.id || gone > 60) {
      // His place: the best view toward the target within the host's stretch, not another battery's spotters' (two
      // batteries spotting from one block each have their own place: the 3D scene, 21 November 21:17Z).
      const taken = [...this.squads.values()].filter((o) => o !== sq && o.fo && o.u.side === u.side && (!this.exits || live(o.u))).map((o) => o.fo![0].slotAt);
      let best: [number, number] = [base.x, base.y], score = -Infinity;
      for (let i = 0; i < 25; i++) {
        const a = (i % 5 - 2) * base.w / 5, l = (Math.floor(i / 5) - 2) * base.d / 3;
        const p = toWorld(base, base.face, a, l);
        const g = groundOf(f, p[0], p[1]);
        if (LAND.pace.foot[g] <= 0 || g === 'water' || g === 'blown') continue;
        if (taken.some((q) => Math.hypot(q[0] - p[0], q[1] - p[1]) < 12)) continue;
        const sc = heightOf(f, p[0], p[1]) + storeysOf(f, p[0], p[1]) * 3 + (g === 'woods' ? 1 : 0) - Math.hypot(p[0] - e.x, p[1] - e.y) * 0.002;
        if (sc > score) { score = sc; best = p; }
      }
      const face = bearing(best[0], best[1], e.x, e.y), [rx, ry] = right(face);
      const at = (dx: number): [number, number] => [best[0] + rx * dx, best[1] + ry * dx];
      if (!sq.fo) {
        // First out: the pair crawls up to it from 30 m behind.
        const mk = (k: number, dx: number): Man => {
          const s = at(dx);
          return { k, n: k, role: 'fo', team: 0, x: s[0] - Math.sin(face) * 30, y: s[1] - Math.cos(face) * 30, face, pose: 'creep', v: 2, spot: s, cover: 'none', until: t + 1, slotAt: best, jx: 0, jy: 0 };
        };
        sq.fo = [mk(200, 0), mk(201, 2.5)];
      } else if (Math.hypot(best[0] - sq.fo[0].slotAt[0], best[1] - sq.fo[0].slotAt[1]) > 15) {
        sq.fo.forEach((m, i) => { m.spot = at(i * 2.5); m.slotAt = best; m.v = 2; });
      }
      for (const m of sq.fo) m.cover = this.shelter(m.spot[0], m.spot[1], groundOf(f, m.spot[0], m.spot[1]), [Math.sin(face), Math.cos(face)], true, false).kind;
      sq.foWith = base.id; sq.foOn = e.id; sq.foAt = [base.x, base.y];
    }
    // The drill book: his signaller on the handset for each call of the guns' mission (the radio man's call_support).
    if (this.byBook && sq.ms && sq.ms.on === e.id && sq.fo[1].call !== sq.ms.call) { sq.fo[1].call = sq.ms.call; sq.fo[1].callAt = [e.x, e.y]; }
    for (const m of sq.fo) {
      const d = Math.hypot(m.x - m.spot[0], m.y - m.spot[1]);
      if (d < 1 && m.v === 0) { m.pose = m.k === 200 ? 'observe' : 'wait'; m.aim = [e.x, e.y]; }
      else if (d > 30) { m.pose = 'run'; m.v = 6; m.aim = undefined; }
      else if (d >= 1) { m.pose = 'creep'; m.v = 2; m.aim = undefined; }
    }
  }

  // ---- Moving each man a step ---------------------------------------------------------------------------------------

  private walk(sq: Squad): void {
    const u = sq.u, f = this.b.field, t = this.b.t;
    let e: LandUnit | null | undefined;
    // Drawn as sections, the men on foot where they stand, by 2 m cell: a man on the move steps round a mate in his way.
    const crowd = sq.tier >= 1 ? new Map<number, Man[]>() : null;
    if (crowd) for (const m of sq.men) {
      if (m.fell !== undefined || m.job || VEHICLE.has(m.role) || CARRIED.has(m.pose)) continue;
      const key = Math.floor(m.x / SPACE.cell) * 65536 + Math.floor(m.y / SPACE.cell);
      let l = crowd.get(key);
      if (!l) crowd.set(key, (l = []));
      l.push(m);
    }
    const step = (m: Man): void => {
      if (m.fell !== undefined) { m.went = 0; return; }
      const x0 = m.x, y0 = m.y;
      walkOne(m);
      m.went = Math.hypot(m.x - x0, m.y - y0) / DT;
      if (m.role === 'tank') m.way = Math.max(m.went, (m.way ?? 0) - TANK_SHED * DT);
      // (Men in lorries: the way a lorry has come over her last metre of road, held while she stands: the 3D game faces
      // her so, never as the drill swings her nose, and her men come down behind her as it shows her: landing)
      if (this.b.trucks && m.role === 'lorry') {
        if (!m.road) m.road = [x0, y0];
        if (Math.hypot(m.x - m.road[0], m.y - m.road[1]) >= 1) { m.roadF = Math.atan2(m.x - m.road[0], m.y - m.road[1]); m.road = [m.x, m.y]; }
      }
    };
    const walkOne = (m: Man): void => {
      // A man firing, kneeling or lying is still: he goes nowhere until his next order gets him up.
      if (m.v > 0 && !MOVING.has(m.pose) && !CARRIED.has(m.pose)) { m.v = 0; m.spot = [m.x, m.y]; }
      // (tank minds: what her commander has timed for this step is done first, mindStep; it gives her top speed)
      if (m.role === 'tank') { const top = this.tankmind ? this.mindStep(sq, m) : tankTop(sq); m.v = Math.min(m.v, top); }
      // (tank minds: a lorry's, a tractor's or a gun's own moment to move off, a gun's own moment in her salvo: mindTick)
      else if (this.tankmind && m.mind) this.mindTick(sq, m);
      const veh = m.role === 'tank' || m.role === 'lorry' || m.role === 'gun';
      const rate = (m.role === 'tank' ? TURN.tank : veh ? TURN.wheels : m.pose === 'ride' ? TURN.ride : TURN.foot) * DT;
      // On foot across hedged fields, he makes for the gate in the hedge ahead of him first (never through the bank).
      const via = !veh && MOVING.has(m.pose) && this.b.gates.length ? this.via(sq, m) : null;
      const dx = (via ? via[0] : m.spot[0]) - m.x, dy = (via ? via[1] : m.spot[1]) - m.y, d = Math.hypot(dx, dy);
      if (m.v > 0 && (d > 0.05 || MOVING.has(m.pose))) {
        // A step toward his spot, turning toward it as fast as he can turn; a last step under 5 cm still arrives (no tank
        // drives on the spot).
        let s = Math.min(d, m.v * DT);
        if (d > 0.3) {
          const way = Math.atan2(dx, dy);
          // A tank whose next place is behind her backs into it, her front to the enemy (never her thin rear).
          if (m.role === 'tank' && m.pose === 'drive' && d < 30 && Math.abs(wrap(way - m.face)) > 1.9) { m.pose = 'reverse'; m.v = Math.min(m.v, 4); s = Math.min(d, m.v * DT); }
          const want = way + (m.pose === 'reverse' ? Math.PI : 0), f0 = m.face;
          m.face = turnTo(m.face, want, rate);
          // He turns before he goes: a man about-turns on the spot, a tank, a lorry or a gun slows while her nose is off
          // her way and pivots where it is far off: no one walks backward or crabs.
          const c = Math.cos(wrap(want - m.face));
          s = Math.min(d, m.v * DT * (c <= 0 ? 0 : veh ? Math.max(c, 0.15) : c));
          // A lorry slows in a turn, so the men seated in her bed (swung about her middle as she turns) go no further a
          // step than she can.
          if (m.role === 'lorry') s = Math.min(s, Math.max(0, 16 * DT - 0.85 * Math.abs(wrap(m.face - f0))));
        }
        // A tank gathers way as a tank does: no faster this step than TANK_GAIN past the way she has on.
        if (m.role === 'tank') s = Math.min(s, ((m.way ?? 0) + TANK_GAIN * DT) * DT);
        if (d > 0) {
          let ux = dx / d, uy = dy / d;
          // On foot and short of his place, he steps round a mate closer than SPACE.men ahead of him or beside his way: his
          // way bends (at most a third of a right angle; the room pass keeps their places apart). Only the push across his
          // way bends it; a mate dead ahead (two men running in file for far places) sends him out by a side of his own.
          // Where their ways cross (a loader whose place is on the far side of his running gunner), the lesser man (the
          // later in his section, the key man never) checks his pace and lets the other by.
          if (crowd && !veh && s > 0.02 && s < d - 1e-9 && MOVING.has(m.pose)) {
            let rx = 0, ry = 0, slow = 1;
            const cx = Math.floor(m.x / SPACE.cell), cy = Math.floor(m.y / SPACE.cell);
            for (let i = -1; i <= 1; i++) for (let j = -1; j <= 1; j++) for (const q of crowd.get((cx + i) * 65536 + cy + j) ?? []) {
              if (q === m || q.fell !== undefined) continue;
              const ox = m.x - q.x, oy = m.y - q.y, od = Math.hypot(ox, oy);
              if (od >= SPACE.men || ox * ux + oy * uy > 0.5 * od) continue; // behind him: no matter
              const w = (SPACE.men - od) / SPACE.men, along = od > 1e-6 ? (ox * ux + oy * uy) / od : -1;
              let px = od > 1e-6 ? ox / od - along * ux : 0, py = od > 1e-6 ? oy / od - along * uy : 0;
              const pl = Math.hypot(px, py);
              if (pl < 0.2) { const side = roll(this.seed, u.id, m.n, 59) < 0.5 ? 1 : -1; px = uy * side; py = -ux * side; }
              else { px /= pl; py /= pl; }
              rx += px * w; ry += py * w;
              const mj = m.j ?? 0, qj = q.j ?? 0;
              if (ox * ux + oy * uy < 0 && (mj > qj || (mj === qj && m.n > q.n))) slow = Math.min(slow, Math.max(0.35, od / SPACE.men));
            }
            s *= slow;
            if (rx || ry) {
              let bx = ux + rx * 1.5, by = uy + ry * 1.5;
              const bl = Math.hypot(bx, by) || 1; bx /= bl; by /= bl;
              const turnBy = Math.atan2(ux * by - uy * bx, ux * bx + uy * by), lim = Math.PI / 6, a = clamp(turnBy, -lim, lim);
              const ca = Math.cos(a), sa = Math.sin(a);
              [ux, uy] = [ux * ca - uy * sa, ux * sa + uy * ca];
            }
          }
          const nx = m.x + ux * s, ny = m.y + uy * s;
          // (men who leave the field: a routing block's men run on past its edge, as its middle does, and never halt on it)
          const on = this.exits && sq.phase === 'flee';
          m.x = clamp(nx, 1, f.w - 1); m.y = on ? ny : clamp(ny, 1, f.h - 1);
          // At the field's edge he can go no further that way: he halts there (no one marches on the spot against it).
          if (!on && s > 0.05 && Math.hypot(nx - m.x, ny - m.y) > 0.8 * s && MOVING.has(m.pose)) { m.spot = [m.x, m.y]; this.arrive(sq, m); }
        }
        if (s >= d - 1e-9 && !via && MOVING.has(m.pose) && m.role !== 'lorry' && sq.phase !== 'column' && sq.phase !== 'limbered' && sq.boarding === undefined) this.arrive(sq, m);
      } else {
        // Still: toward what he fires at, else toward the enemy, turning as he can.
        let want: number | undefined;
        let turn = rate;
        if (m.aim) want = bearing(m.x, m.y, m.aim[0], m.aim[1]);
        else if (m.v === 0 && fighting(u)) {
          if (e === undefined) e = this.threat(u);
          if (e) want = bearing(m.x, m.y, e.x, e.y);
          // Squad life: a man watching looks his own way round his arc, turning as a man looks, not as he snaps to a target.
          if (this.life && want !== undefined && WATCHING.has(m.pose) && !veh && !CREWED.has(m.role) && m.role !== 'holder') { want += this.scan(sq, m); turn = Math.min(rate, DR.life.scan.rate * DT); }
        }
        // (Men in lorries: a lorry standing never pivots where she stands; the 3D game faces her the way she drove, and her
        // men get down behind her as it shows her)
        // (tank minds: a standing tank's hull is toward her block's main threat, her gun free: mindLook)
        if (this.tankmind && m.mind?.hull !== undefined) want = m.mind.hull;
        if (want !== undefined && !(this.b.trucks && m.role === 'lorry')) m.face = turnTo(m.face, want, turn);
      }
      // Never far from his block: the block is the truth (the rules' box, and a margin round it). Left outside it, where
      // the block turned or went on without him, he runs back into it on his own feet (his tracks, its wheels), facing
      // his way: never thrown, never dragged. Only the block's going carries him, and only beyond the ring round its box
      // (a turn of the box never moves the ring).
      // (Men in lorries: a lorry stands where she is while her men get down over her tailboard, and HOP.clear more while
      // they clear her tail, the block's box or no)
      if (m.role === 'lorry' && (sq.hop !== undefined || (sq.left !== undefined && t < sq.left + HOP.clear && sq.phase !== 'flee'))) return;
      const extra = m.role === 'lorry' ? 120 : 0;
      // (squad life: each man on foot his own few metres of slack, so a line the block leaves behind gets up man by man)
      const slack = this.life && !veh && !CREWED.has(m.role) ? DR.life.react.leash * roll(this.seed, u.id, m.n, 153) : 0;
      const [lx, ly] = this.leash(sq, [m.x, m.y], extra + slack);
      const gd = Math.hypot(lx - m.x, ly - m.y);
      if (gd === 0) return;
      const fast = m.role === 'tank' ? tankTop(sq) : veh ? 16 : m.pose === 'ride' || m.pose === 'mounted' ? 14 : CATCH_UP, need = Math.min(fast, Math.max(3, sq.speed * 1.25));
      // Mounting again, he makes for his lorry or his horse, wherever the block has got to (board).
      const own = m.role !== 'holder' && m.pose !== 'ride' && m.pose !== 'mounted' && sq.boarding === undefined;
      // Well outside, he makes for his own place in the block as it is now (men making for the box's nearest point
      // would bunch on it); just outside, for his spot brought inside.
      if (own && gd > 2 && m.role !== 'lorry') {
        const [a, l] = this.slot(sq, m, sq.phase === 'rest' ? 'rest' : 'line');
        m.slotAt = toWorld(u, u.face, a, l); m.spot = this.leash(sq, m.slotAt, -4);
      } else if (sq.boarding === undefined) m.spot = this.leash(sq, m.spot, extra ? extra - 4 : -4);
      // A tank backs into a place close behind her (no faster than 4), as in walk's own switch.
      const back = (p: [number, number]): boolean => m.role === 'tank' && Math.hypot(p[0] - m.x, p[1] - m.y) < 30 && Math.abs(wrap(Math.atan2(p[0] - m.x, p[1] - m.y) - m.face)) > 1.9;
      if (own && (!MOVING.has(m.pose) ? gd > 0.05 : gd > 2 && m.v < (m.pose === 'reverse' ? Math.min(need, 4) : need))) {
        // Running to catch up, never sliding over the ground lying down.
        if (gd <= 2 || m.role === 'lorry') m.spot = this.leash(sq, m.slotAt, extra ? extra - 4 : -4);
        m.aim = undefined; m.hidden = undefined;
        const rev = back(m.spot);
        m.v = rev ? Math.min(need, 4) : need;
        m.pose = rev ? 'reverse' : m.role === 'tank' || m.role === 'lorry' ? 'drive' : m.role === 'gun' ? 'towed' : sq.phase === 'flee' ? 'flee' : (sq.phase === 'bound' || sq.phase === 'fight') && !(this.life && m.pose === 'run') ? 'advance' : 'run';
        m.until = t + 1;
      }
      // A lorried man or a trooper may be further out (running to his lorry or his horse) before the ring takes him.
      // (Men in lorries: a lorried man may be HOP.ring further out again, so one just down off a lorry left standing behind
      // the block, as it stands while its men get down, runs after it on his feet and is never carried off the tailboard)
      this.ring(sq, m, extra || (u.kind === 'mot' || u.kind === 'cav' ? RIDERS_RING + (this.b.trucks && u.kind === 'mot' && m.role !== 'lorry' ? HOP.ring : 0) : 0));
    };
    for (const m of sq.veh) step(m);
    for (const m of sq.men) {
      if (m.fell !== undefined) { m.went = 0; continue; }
      // A bearer on a carry goes his own way (litters).
      if (m.job) continue;
      // Aboard: he sits in his seat in his lorry, wherever it goes (in the stride he climbs in, from beside it to his seat).
      if (m.pose === 'mounted' && (u.kind === 'mot' || m.role === 'crew') && sq.veh.length) {
        // (Men in lorries: on his way down over her tailboard he is already where he will stand behind her, unseen: the 3D
        // game walks him there, and so takes him up there as he is on his feet)
        const lorry = this.lorryOf(sq, m), x0 = m.x, y0 = m.y, p = m.off ? this.landing(sq, m) : this.seat(sq, m);
        m.x = p[0]; m.y = p[1]; m.face = turnTo(m.face, lorry.face, TURN.foot * DT); m.went = m.off ? 0 : Math.hypot(m.x - x0, m.y - y0) / DT; m.spot = p; m.v = lorry.v;
        continue;
      }
      // Mounting again: to his lorry or his horse, wherever it is now; up once within a stride of it.
      if (sq.boarding !== undefined && m.role !== 'holder' && !CARRIED.has(m.pose) && this.stands(sq, m)) {
        const at = this.mountAt(sq, m);
        if (at) {
          const x0 = m.x, y0 = m.y;
          m.spot = at;
          step(m);
          if (Math.hypot(at[0] - m.x, at[1] - m.y) < (u.kind === 'mot' ? MOUNT.climb : MOUNT.saddle)) {
            if (this.mayMount(sq, m)) {
              // Up: into the lorry (he has run to his seat in her bed), or into the saddle where he stands by his horse.
              m.pose = u.kind === 'mot' ? 'mounted' : 'ride'; m.v = 0; m.aim = undefined; m.cover = 'none';
              if (u.kind === 'mot') { m.x = at[0]; m.y = at[1]; m.went = Math.hypot(m.x - x0, m.y - y0) / DT; }
              m.spot = [m.x, m.y];
            } else if ((m.went ?? 0) < 0.5 && MOVING.has(m.pose)) { m.pose = 'wait'; m.v = 0; m.spot = [m.x, m.y]; }
            // (else he runs on beside it till it halts)
          }
          continue;
        }
      }
      step(m);
    }
    // The spotter's pair, with another block: up to its place at a run, the last stretch crawling, then watching.
    if (sq.fo) for (const m of sq.fo) {
      const dx = m.spot[0] - m.x, dy = m.spot[1] - m.y, d = Math.hypot(dx, dy);
      m.went = 0;
      if (m.v > 0) {
        let s = Math.min(d, m.v * DT);
        if (d > 0.3) { const want = Math.atan2(dx, dy); m.face = turnTo(m.face, want, TURN.foot * DT); s *= Math.max(0, Math.cos(wrap(want - m.face))); }
        if (d > 0) { m.x += (dx / d) * s; m.y += (dy / d) * s; m.went = s / DT; }
        if (s >= d - 1e-9) { m.v = 0; m.pose = m.k === 200 ? 'observe' : 'wait'; m.until = t + 1; }
      } else if (m.aim) m.face = turnTo(m.face, bearing(m.x, m.y, m.aim[0], m.aim[1]), TURN.foot * DT);
    }
  }

  /** The ring round the block's box: the routing block's (its men scatter wider), so when it rallies they come in on their feet. */
  private ring(sq: Squad, m: Man, extra: number): void {
    const u = sq.u, mrg = 70 + extra, R = Math.hypot(u.w / 2 + mrg, u.d / 2 + mrg), cx = m.x - u.x, cy = m.y - u.y, r = Math.hypot(cx, cy);
    if (r > R) { m.x = u.x + (cx * R) / r; m.y = u.y + (cy * R) / r; }
  }

  // ---- What the 3D game reads ---------------------------------------------------------------------------------------

  /**
   * Every block's men, as one side sees them: an enemy block not yet identified (fog, b464(2)) sends none, so the 3D
   * game draws its plain stand. Fallen men stay to the end of the fight where they fell.
   */
  // ---- Rounds: every shot with its odds, landing where it lands ----------------------------------------------------

  /**
   * The rounds fired in the last few seconds (DR.rounds.keep), oldest first, each numbered n in the order fired (read
   * those past the last n you took). As a side sees them where one is given: rounds at an enemy block it does not know
   * are left out, and an unseen shooter is not named.
   */
  rounds(as?: Side): (Round & { n: number })[] {
    const b = this.b, out: (Round & { n: number })[] = [];
    for (const r of this.shots) {
      const on = b.unit(r.on), by = r.by !== undefined ? b.unit(r.by) : undefined;
      if (as !== undefined && on && on.side !== as && !b.known(on)) continue;
      const o = { ...r };
      if (as !== undefined && by && by.side !== as && !b.known(by)) { delete o.by; delete o.k; delete o.j; }
      out.push(o);
    }
    return out;
  }

  /**
   * Each shot of this step (a tank's, a gun's, an anti-tank round, a grenade: a new stamp) becomes a round; and each
   * rifleman and machine gun in a firing pose fires his next shot, burst by burst.
   */
  private fire(sq: Squad): void {
    const u = sq.u, t = this.b.t, R = DR.rounds;
    if (!live(u)) return;
    for (const m of sq.veh.length ? [...sq.men, ...sq.veh] : sq.men) {
      if (m.k < 100 ? !this.stands(sq, m) : m.fell !== undefined) continue;
      if (m.shot !== undefined && m.shot !== m.fired) {
        m.fired = m.shot;
        const w: Weapon | undefined = m.role === 'tank' ? 'tank' : m.role === 'gun' ? 'gun' : m.role === 'mortar' ? 'mortar' : m.pose === 'throw_grenade' ? 'grenade' : m.role === 'at' ? 'at' : undefined;
        if (w && m.aim && m.shot > t - 0.05) this.round(sq, m, w, m.aim);
      }
      // A support team's machine gun on its tripod fires long bursts, close together, all the while it is firing; a
      // tank's machine guns (the drill book) bursts at the enemy's foot between her main gun's rounds.
      const hmg = m.role === 'hmg' && m.pose === 'fire', coax = m.role === 'tank' && !!m.coax && m.pose !== 'fire';
      if (m.aim && (hmg || coax || (SMALL_ARMS.has(m.pose) && m.role !== 'at' && !VEHICLE.has(m.role)))) {
        const w: Weapon = m.role === 'mg' || hmg || coax ? 'mg' : 'rifle', [lo, hi] = hmg ? DR.teams.hmg.burst : R.smallArms[w];
        // Covering his mates' rushes (the drill book): rapid fire.
        const k = (coax ? 2 : 1) * (m.rapid !== undefined && t < m.rapid ? BOOK.rapid : 1);
        if (m.burst === undefined) m.burst = t + k * hi * roll(this.seed, u.id, m.n, t * 10, 41);
        else if (t >= m.burst) { m.burst = t + k * (lo + (hi - lo) * roll(this.seed, u.id, m.n, t * 10, 42)); this.round(sq, m, w, m.aim); }
      } else m.burst = undefined;
    }
  }

  /** The enemy block a point is in (or nearest, within 60 m of its ground). */
  private blockAt(side: Side, p: [number, number]): LandUnit | undefined {
    let on: LandUnit | undefined, gap = 60;
    for (const e of this.b.units) {
      if (e.side === side || !live(e)) continue;
      const g = Math.max(0, Math.hypot(e.x - p[0], e.y - p[1]) - Math.max(e.w, e.d) / 2);
      if (g < gap) { gap = g; on = e; }
    }
    return on;
  }

  /** A block's models that can be struck: its men and tanks and guns standing, and its lorries. */
  private standing(sq: Squad): Man[] { return this.memo(sq).struck; }

  /** Where a model will be dt battle seconds on, going on as it goes now (a round lands at Round.l). */
  private ahead(v: Man, dt: number): [number, number] {
    const go = (v.went ?? 0) * dt * (v.pose === 'reverse' ? -1 : 1);
    if (go === 0) return [v.x, v.y];
    const [fx, fy] = fwd(v.face);
    return [clamp(v.x + fx * go, 1, this.b.field.w - 1), clamp(v.y + fy * go, 1, this.b.field.h - 1)];
  }

  /** Whether a point lies within r of a vehicle standing anywhere on the field (where it will be in dt), or of a wreck. */
  private onVehicle(x: number, y: number, r: number, dt: number): boolean {
    for (const sq of this.squads.values()) {
      const u = sq.u;
      if (Math.hypot(u.x - x, u.y - y) > Math.max(u.w, u.d) / 2 + 80) continue;
      for (const q of this.standing(sq)) {
        if (!VEHICLE.has(q.role) || (this.exits && q.lv !== undefined)) continue;
        const [qx, qy] = this.ahead(q, dt);
        if (Math.hypot(qx - x, qy - y) < r) return true;
      }
    }
    for (const z of this.b.wrecks) { const [zx, zy] = this.wreckSpot(z); if (Math.hypot(zx - x, zy - y) < r) return true; }
    return false;
  }

  /** The face of a vehicle toward a point: its front, its left or right side, or its rear. */
  private faceOf(v: Man, x: number, y: number): Face {
    const rel = wrap(bearing(v.x, v.y, x, y) - v.face), a = Math.abs(rel);
    return a <= Math.PI / 4 ? 'front' : a >= (3 * Math.PI) / 4 ? 'rear' : rel > 0 ? 'right' : 'left';
  }

  /** Where on a face a round strikes: [across, height], a tank's turret DR.rounds.turret of the time. */
  private spotOn(v: Man, ...ns: number[]): [number, number] {
    const a = roll(this.seed, ...ns, 51), c = roll(this.seed, ...ns, 52), up = roll(this.seed, ...ns, 53);
    const h = v.role === 'tank' ? (up < DR.rounds.turret ? 0.7 + 0.25 * c : 0.15 + 0.45 * c) : v.role === 'lorry' ? 0.2 + 0.6 * c : 0.2 + 0.5 * c;
    return [Math.round((a - 0.5) * 160) / 100, Math.round(h * 100) / 100];
  }

  /** Puts a round in the list, and its scar on what it struck (kept to the end, the first DR.rounds.marks of them). */
  private book(r: Round, v?: Man, mark?: Mark[3]): void {
    this.shots.push({ ...r, n: ++this.fired });
    if (v && mark && r.fc && r.u !== undefined && r.h !== undefined) {
      const mk = (v.marks ??= []);
      if (mk.length < DR.rounds.marks) mk.push([r.fc, r.u, r.h, mark, Math.round(r.l * 10) / 10]);
    }
  }

  /**
   * One round from a man, a tank or a gun at a point: what it is fired at (the enemy model nearest the point; for a
   * tank's, a launcher's or a gun's, a vehicle where one is near), whether it strikes (DR.rounds: the odds by range, a
   * moving shooter or target, cover, the crew), and where it lands. It never kills: only the rules take a model
   * (killRound). A man is never struck short of that: a round at men lands near.
   */
  private round(sq: Squad, m: Man, w: Weapon, aim: [number, number]): void {
    const u = sq.u, t = this.b.t, R = DR.rounds;
    const on = this.blockAt(u.side, aim), tq = on && this.squads.get(on.id);
    if (!on || !tq) return;
    const alive = this.standing(tq);
    if (!alive.length) return;
    const heavy = w === 'tank' || w === 'at' || w === 'gun' || w === 'mortar';
    const pool = heavy && alive.some((x) => VEHICLE.has(x.role) && Math.hypot(x.x - aim[0], x.y - aim[1]) < 60) ? alive.filter((x) => VEHICLE.has(x.role)) : alive;
    let v = pool[0], dd = Infinity;
    for (const x of pool) { const d = Math.hypot(x.x - aim[0], x.y - aim[1]); if (d < dd) { dd = d; v = x; } }
    const range = Math.hypot(v.x - m.x, v.y - m.y);
    if (range > ((w === 'mg' || w === 'rifle') ? 900 : 4000)) return;
    const l = Math.round((t + range / R.speed[w]) * 10) / 10, k0 = Math.round(t * 10);
    // (hatches: a round at a tank sends her commander down as it lands, and by their experience her mates')
    if (this.hatches && v.role === 'tank' && !tq.esc) for (const x of alive) if (x.role === 'tank') this.alarm(tq, x, l, x !== v);
    const base: Round = { t: Math.round(t * 10) / 10, l, by: u.id, k: m.k, w, on: on.id, x: 0, y: 0, r: 'miss' };
    if (m.j !== undefined) base.j = m.j;
    if (w === 'mg' && m.role === 'tank') base.g = this.tankGun(u, m, aim, k0);
    const [lx, ly] = range > 0.01 ? [(v.x - m.x) / range, (v.y - m.y) / range] : fwd(m.face);
    // Everything is placed where it will be when the round lands (at l): what it is fired at, and every vehicle near.
    const fly = l - t;
    // A gun firing over the crest (not over open sights), or a mortar (always so): its round lands round its aim,
    // striking a vehicle's top where it falls on one of the block it was fired at.
    if ((w === 'gun' && !m.buttoned) || w === 'mortar') {
      const [i0, i1] = w === 'mortar' ? R.indirect.mortar : R.indirect.sigma;
      const sg = i0 + i1 * Math.hypot(aim[0] - m.x, aim[1] - m.y);
      const ea = gauss(roll(this.seed, u.id, m.n, k0, 61), roll(this.seed, u.id, m.n, k0, 62)) * sg, ec = gauss(roll(this.seed, u.id, m.n, k0, 63), roll(this.seed, u.id, m.n, k0, 64)) * sg * 0.4;
      let x = clamp(aim[0] + lx * ea + ly * ec, 1, this.b.field.w - 1), y = clamp(aim[1] + ly * ea - lx * ec, 1, this.b.field.h - 1);
      const hitV = alive.find((q) => { if (q.role !== 'tank') return false; const [qx, qy] = this.ahead(q, fly); return Math.hypot(qx - x, qy - y) < R.indirect.onVehicle; });
      if (hitV) {
        const [a, h] = this.spotOn(hitV, u.id, m.n, k0), [hx, hy] = this.ahead(hitV, fly);
        const glance = roll(this.seed, u.id, m.n, k0, 65) < R.glance.top;
        this.duck(hx, hy);
        const cm = this.commanderHit(hitV, BOOK.tankMg.splinter, u.id, m.n, k0);
        this.book({ ...base, ok: hitV.k, x: Math.round(hx * 10) / 10, y: Math.round(hy * 10) / 10, r: glance ? 'glance' : 'worn', fc: 'top', u: a, h, ...(cm ? { cm: 1 as const } : {}) }, hitV, 'scorch');
        return;
      }
      // On any other vehicle (a lorry or a gun it would wreck, which the rules never do; another block's tank or a wreck
      // it would strike with no hit booked): it falls just past them.
      for (let i = 0; i < 6 && this.onVehicle(x, y, R.indirect.onVehicle, fly); i++) { x = clamp(x + lx * 3, 1, this.b.field.w - 1); y = clamp(y + ly * 3, 1, this.b.field.h - 1); }
      this.duck(x, y);
      this.book({ ...base, x: Math.round(x * 10) / 10, y: Math.round(y * 10) / 10 });
      return;
    }
    // Aimed: the odds it strikes (a vehicle; a man is never struck short of the rules taking him).
    const isV = VEHICLE.has(v.role);
    const small = w === 'mg' || w === 'rifle';
    // A tank's, a launcher's or a gun's round at a lorry or a gun would wreck it, which the rules never do: those miss.
    const may = isV && (v.role === 'tank' || small);
    let p = 0;
    if (may) {
      const [best, half] = R.odds[w];
      p = best / (1 + (range / half) ** 2);
      if ((m.went ?? 0) > 0.5) p *= R.shooterMoving;
      if ((v.went ?? 0) > 0.5) p *= R.targetMoving;
      p *= R.cover[v.cover] ?? 1;
      p *= R.xp[0] + R.xp[1] * clamp(u.xp, 0, 1);
    }
    if (may && roll(this.seed, u.id, m.n, k0, 66) < p) {
      const fc = this.faceOf(v, m.x, m.y), [a, h] = this.spotOn(v, u.id, m.n, k0), [vx, vy] = this.ahead(v, fly);
      const at = { ...base, ok: v.k, x: Math.round(vx * 10) / 10, y: Math.round(vy * 10) / 10, fc, u: a, h };
      // Small arms only rattle a tank (no scar); they hole a lorry and pock a gun's shield. A grenade scorches.
      if (v.role === 'tank' && small) this.book({ ...at, r: 'glance', ...(this.commanderHit(v, BOOK.tankMg.commander, u.id, m.n, k0) ? { cm: 1 as const } : {}) });
      else if (small) this.book({ ...at, r: v.role === 'lorry' ? 'worn' : 'glance' }, v, 'pocks');
      else if (w === 'grenade') this.book({ ...at, r: 'glance' }, v, 'scorch');
      else { const glance = roll(this.seed, u.id, m.n, k0, 67) < R.glance[fc]; this.book({ ...at, r: glance ? 'glance' : 'worn' }, v, glance ? 'gouge' : 'hole'); }
      return;
    }
    // A miss: short or over along the line of fire, a little to one side, never on what it was fired at.
    const [s0, s1, c0, c1] = R.spread[w];
    let ea = gauss(roll(this.seed, u.id, m.n, k0, 68), roll(this.seed, u.id, m.n, k0, 69)) * (s0 + s1 * range);
    let ec = gauss(roll(this.seed, u.id, m.n, k0, 70), roll(this.seed, u.id, m.n, k0, 71)) * (c0 + c1 * range);
    const least = isV ? 2.5 : 0.6, off = Math.hypot(ea, ec);
    if (off < least) { const g = off > 1e-6 ? least / off : 1; ea = off > 1e-6 ? ea * g : least; ec *= g; }
    const [vx, vy] = this.ahead(v, fly);
    let x = clamp(vx + lx * ea + ly * ec, 1, this.b.field.w - 1), y = clamp(vy + ly * ea - lx * ec, 1, this.b.field.h - 1);
    // Nor on another vehicle (of any block) or a wreck: a round landing there would have struck it. It goes on over,
    // past them all.
    for (let i = 0; i < 6 && this.onVehicle(x, y, 2.5, fly); i++) { x = clamp(x + lx * 3, 1, this.b.field.w - 1); y = clamp(y + ly * 3, 1, this.b.field.h - 1); }
    if (heavy) this.duck(x, y);
    this.book({ ...base, x: Math.round(x * 10) / 10, y: Math.round(y * 10) / 10 });
  }

  // ---- A tank's machine guns, seen firing (Drill.tankmg): its helpers stand just above mgKill(), below this line ----
  /**
   * Whether the man (model k, or a section's man as the block's count of losses) a tank block took off the enemy's
   * foot went to her machine guns, not her main gun's shells (BOOK.tankMg.kill: more in the open). The drill book only;
   * fixed by the blocks and the count, never the battle's dice. Looks only: the rules took him either way.
   */
  mgKill(by: LandUnit, t: LandUnit, k: number): boolean {
    if (!this.byBook || by.kind !== 'arm' || !FOOT_KINDS.includes(t.kind) || t.kind === 'fort') return false;
    const close = CLOSE.includes(groundOf(this.b.field, t.x, t.y)) || t.dug;
    return roll(this.seed, by.id, t.id, k, 97) < (close ? BOOK.tankMg.kill.cover : BOOK.tankMg.kill.open);
  }

  /**
   * Which of a tank's machine guns fires this burst (BOOK.tankMg): the commander's on the roof only in the armies that
   * carried one, only with him up in his hatch, a share of the time; the hull gunner's at what is before her bow; else
   * the coaxial beside the main gun.
   */
  private tankGun(u: LandUnit, m: Man, at: [number, number], n: number): 'coax' | 'hull' | 'roof' {
    const T = BOOK.tankMg;
    if (!m.buttoned && m.cmdDown === undefined && T.roof.includes(u.nat) && roll(this.seed, u.id, m.n, n, 98) < T.roofShare) return 'roof';
    const off = Math.abs(wrap(bearing(m.x, m.y, at[0], at[1]) - m.face)) * (180 / Math.PI);
    return off <= T.hullArc ? 'hull' : 'coax';
  }

  /** A round striking a tank strikes her commander, up in his hatch, this share of the time: he drops inside, and she buttons up. */
  private commanderHit(v: Man, odds: number, ...ns: number[]): boolean {
    if (v.role !== 'tank' || v.buttoned || v.cmdDown !== undefined || roll(this.seed, ...ns, 99) >= odds) return false;
    v.cmdDown = this.b.t;
    v.buttoned = true;
    return true;
  }

  /**
   * The round that cost a model the rules took: from the striking block's tank or gun (a shell, Drill.shelled, landing
   * at p) or, on foot, its man nearest who could have fired it (on a tank: its anti-tank man, else a grenade). It is
   * the hit, landing as it is fired; on a tank it leaves the hole it went in by.
   */
  private killRound(by: LandUnit | undefined, vq: Squad, v: Man, p?: [number, number], mate = false, crush?: Man): void {
    const t = this.b.t, tq = by ? this.squads.get(by.id) : undefined;
    const at: [number, number] = p ?? [v.x, v.y];
    let s: Man | undefined, w: Weapon = p ? (by?.kind === 'arm' ? 'tank' : by?.team === 'mor' ? 'mortar' : 'gun') : 'rifle';
    if (tq) {
      const shells = (x: Man): boolean => x.role === 'tank' || x.role === 'gun' || x.role === 'mortar';
      // A tank block's man taken by no shell went to her machine guns (BOOK.tankMg).
      const mg = !p && by?.kind === 'arm';
      const can = this.standing(tq).filter((x) => x.role !== 'lorry' && x.role !== 'holder' && (mg ? x.role === 'tank' : p ? shells(x) : !shells(x)) && !(this.exits && x.lv !== undefined));
      const pref = v.role === 'tank' && !p && by?.team !== 'mg' ? can.filter((x) => x.role === 'at') : can.filter((x) => SMALL_ARMS.has(x.pose) || shells(x) || x.role === 'hmg');
      const from = pref.length ? pref : can;
      let dd = Infinity;
      for (const x of from) { const d = Math.hypot(x.x - at[0], x.y - at[1]); if (d < dd) { dd = d; s = x; } }
    }
    if (!p && s) w = s.role === 'hmg' || s.role === 'tank' ? 'mg' : v.role === 'tank' ? (s.role === 'at' ? 'at' : 'grenade') : s.role === 'mg' ? 'mg' : 'rifle';
    // (infantry with tanks: run over by an enemy tank, her tracks the round: no shot, no burst)
    if (crush) { s = crush; w = 'track'; }
    const r: Round = { t: Math.round(t * 10) / 10, l: Math.round(t * 10) / 10, w, on: vq.u.id, ok: v.k, x: Math.round(at[0] * 10) / 10, y: Math.round(at[1] * 10) / 10, r: 'kill' };
    if (v.j !== undefined) r.oj = v.j;
    if (by) r.by = by.id;
    if (mate) r.m = 1;
    if (s) {
      r.k = s.k;
      if (s.j !== undefined) r.j = s.j;
      if (w === 'mg' && s.role === 'tank' && by) r.g = this.tankGun(by, s, at, Math.round(t * 10));
      // Its shot is seen: a tank's, a gun's, a launcher's or a grenade's stamp (small arms are in the firing already).
      if (w !== 'rifle' && w !== 'mg' && w !== 'track' && !mate) { s.shot = t; s.fired = t; s.aim = [v.x, v.y]; if (s.role === 'gun' || s.role === 'mortar') s.pose = 'fire'; }
    }
    if (VEHICLE.has(v.role)) {
      const fc: Face = w === 'gun' || w === 'mortar' ? 'top' : s ? this.faceOf(v, s.x, s.y) : 'front', [a, h] = this.spotOn(v, vq.u.id, v.n, Math.round(t * 10), 9);
      // A tank's end, by her measured armour: a rear hit burns more often (Brian, 22 November 04:16Z).
      if (v.role === 'tank') r.fate = fateOf(vq.esc ? null : this.b.vehicle(vq.u), fc === 'front' ? 'front' : fc === 'rear' ? 'rear' : 'side', roll(this.seed, vq.u.id, v.n, 96), roll(this.seed, vq.u.id, v.n, 95));
      this.book({ ...r, fc, u: a, h }, v, 'hole');
    } else this.book(r);
  }

  view(as?: Side): MenBlock[] {
    const b = this.b, out: MenBlock[] = [];
    const r1 = (v: number): number => Math.round(v * 10) / 10, r2 = (v: number): number => Math.round(v * 100) / 100;
    const one = (m: Man): MenOut => {
      // A rifle section's key man is its leader (the 3D game gives him his army's leader's weapon).
      const o: MenOut = { k: m.k, r: (m.hasSec && m.role === 'rifle') || m.lead ? 'leader' : m.role, x: r1(m.x), y: r1(m.y), f: r2(wrap(m.face)), p: m.pose, v: MOVING.has(m.pose) || CARRIED.has(m.pose) ? r1(m.went ?? 0) : 0, c: m.cover };
      if (m.j !== undefined) o.j = m.j;
      if (m.aim) o.a = [r1(m.aim[0]), r1(m.aim[1])];
      if (m.shot !== undefined) o.s = r1(m.shot);
      if (m.fell !== undefined) o.d = r1(m.fell);
      if (m.hidden) o.h = 1;
      if (m.buttoned) o.b = 1;
      if (m.marks?.length) o.mk = m.marks.map((x) => [...x] as Mark);
      if (m.el && this.byBook) o.e = m.el;
      if (m.call !== undefined) o.cl = r1(m.call);
      if (m.wd !== undefined) o.wd = r1(m.wd);
      if (m.cy) o.cy = m.cy.map((c) => [r1(c[0]), r1(c[1]), c[2], c[3]]);
      if (m.gone !== undefined) o.gone = r1(m.gone);
      if (m.back !== undefined) o.back = r1(m.back);
      if (m.br) o.br = 1;
      if (m.call !== undefined && m.callAt) o.cs = [r1(m.callAt[0]), r1(m.callAt[1])];
      if (m.sw) o.sw = { ...m.sw };
      if (this.leavingNow(m)) o.lv = 1;
      if (m.ml) {
        o.ml = m.ml;
        // (a self-propelled gun is drawn as a tank: driving where the battery's guns are towed, halted where they stand)
        if (m.role === 'gun') {
          o.r = 'tank';
          o.p = m.pose === 'towed' ? (o.v > 0 ? 'drive' : 'halt') : m.pose === 'deployed' || m.pose === 'parked' || m.pose === 'idle' ? 'halt' : m.pose;
          delete o.h;
        }
      }
      return o;
    };
    // (armoured infantry, LandBattle.mech: drawn with their armoured block's own men, under its id)
    const byId = new Map<number, MenBlock>();
    for (const sq of this.squads.values()) {
      const u = sq.u;
      const lead = this.leads.get(u.side), mark = lead && lead[0] === u.id ? lead[1] : undefined;
      if (as !== undefined && u.side !== as && !b.known(u)) continue;
      if (u.state === 'withdrawn' && !sq.men.some((m) => m.fell !== undefined) && !(this.exits && this.leavingIn(u.id))) continue;
      const men = sq.men.filter((m) => m.fell !== undefined || m.job !== undefined || (this.stands(sq, m) && live(u)) || this.leavingNow(m)).map((m) => {
        const o = one(m);
        if (b.trucks && m.pose === 'mounted' && m.fell === undefined && sq.veh.length) o.sv = m.out !== undefined ? [this.lorryOf(sq, m).k, this.seatNo(sq, m), Math.round(m.out * 100) / 100] : [this.lorryOf(sq, m).k, this.seatNo(sq, m)];
        if (mark !== undefined && m.fell === undefined && grp(m) === mark && m.role !== 'lorry' && m.role !== 'holder') o.ld = 1;
        return o;
      });
      for (const v of sq.veh) if (live(u) || this.leavingNow(v)) men.push(one(v));
      if (sq.of !== undefined) { const own = byId.get(sq.of); if (own) own.men.push(...men); continue; }
      const blk: MenBlock = { id: u.id, ph: sq.phase, men };
      byId.set(u.id, blk);
      if (sq.fo && live(u)) { blk.fo = sq.fo.map(one); blk.foWith = sq.foWith; blk.foOn = sq.foOn; }
      else if (this.exits && sq.fo?.some((m) => this.leavingNow(m))) blk.fo = sq.fo.filter((m) => this.leavingNow(m)).map(one);
      if (b.phones && b.phonesSeen(as)) { const ln = this.linemen(u.id); if (ln.length) blk.ln = ln; }
      out.push(blk);
    }
    return out;
  }

  /**
   * Field telephone lines (release 2; LandBattle.phones): the linemen out on this block's wire now, one for each mend.
   * He leaves the headquarters at the mend's t0 with the cable reel ('reel'), walking the wire at foot pace to the cut,
   * kneels there splicing ('splice') from t1, as he gets there, until the mend's t, then walks back along it ('walk') and
   * is gone once he is home. Worked out from the mend alone, as the clock stands: looks only, and nothing here draws.
   * Numbered k 300 on by the mend's man (the spotter's pair are 200 and 201).
   */
  private linemen(id: number): MenOut[] {
    const b = this.b, t = b.t, out: MenOut[] = [], pace = LAND.speed.inf;
    const r1 = (v: number): number => Math.round(v * 10) / 10, r2 = (v: number): number => Math.round(v * 100) / 100;
    for (const m of b.wireMends) {
      const w = b.wires[m.wire - 1];
      if (!w || w.unit !== id || t < m.t0) continue;
      const [hx, hy] = w.hq, d = Math.hypot(m.x - hx, m.y - hy), home = m.t + d / pace;
      if (t >= home) continue;
      const out1 = bearing(hx, hy, m.x, m.y);
      // Out along the wire, at the cut, or back along it: how far out from the headquarters he stands (0 to 1).
      const k = t < m.t1 ? (m.t1 > m.t0 ? (t - m.t0) / (m.t1 - m.t0) : 1) : t < m.t ? 1 : d > 0 ? Math.max(0, 1 - ((t - m.t) * pace) / d) : 0;
      const p: Pose = t < m.t1 ? 'reel' : t < m.t ? 'splice' : 'walk';
      out.push({ k: 300 + m.man, r: 'rifle', x: r1(hx + (m.x - hx) * k), y: r1(hy + (m.y - hy) * k), f: r2(wrap(p === 'walk' ? out1 + Math.PI : out1)), p, v: p === 'splice' ? 0 : pace, c: 'none', ln: 1 });
    }
    return out;
  }

  /** The block's men, for tests and the browser page. */
  squad(id: number): { phase: Phase; men: Man[]; veh: Man[]; fo: Man[] | null } | undefined {
    const sq = this.squads.get(id);
    return sq ? { phase: sq.phase, men: sq.men, veh: sq.veh, fo: sq.fo } : undefined;
  }
}

/**
 * Who is on a block's tray, model by model (the rules lift the last first, so model 0 stands longest): a full tray of
 * foot has its officer, two machine guns (one a team), its anti-tank man and its riflemen, the two teams interleaved
 * so both thin together; engineers carry charges in their last two; a cavalry tray's every fourth man holds the horses
 * once they are off; a regiment's thin tray has a machine gun and riflemen. Tanks and guns are each their own, and so
 * is each of a support team's machine guns or mortars (release 2), with its crew.
 */
export function rolesFor(kind: Kind, tray: number, n: number, team?: Team): [Role, 0 | 1 | 2][] {
  const out: [Role, 0 | 1 | 2][] = [];
  for (let k = 0; k < n; k++) {
    if (kind === 'arm') { out.push(['tank', (k % 2 ? 2 : 1)]); continue; }
    if (kind === 'art') { out.push([team === 'mg' ? 'hmg' : team === 'mor' ? 'mortar' : 'gun', 0]); continue; }
    if (tray >= 8) {
      if (k === 0) out.push(['officer', 0]);
      else if (k === 1) out.push(['mg', 1]);
      else if (k === 2) out.push(['mg', 2]);
      else if (k === 3) out.push([kind === 'fort' ? 'mg' : 'at', kind === 'fort' ? 1 : 0]);
      else {
        const team: 1 | 2 = k % 2 ? 2 : 1;
        if (kind === 'eng' && k >= tray - 2) out.push(['sapper', team]);
        else if (kind === 'cav' && k % 4 === 3) out.push(['holder', team]);
        else out.push(['rifle', team]);
      }
    } else out.push(k === 0 ? ['mg', 1] : ['rifle', k % 2 ? 2 : 1]);
  }
  return out;
}

/**
 * The rest of a model's section, drawn as sections (Drill.sections; DR.sections): each [role, keeps by its weapon or
 * officer (Drill.pairs), a scout]. A rifle section by its army: its automatic weapon, that weapon's number twos and
 * ammunition bearers, its scouts, its riflemen (its key man is its leader). The headquarters: the officer's runners. A
 * machine-gun section: two loaders and its leader. An anti-tank team: the loader. Assault pioneers: two more with
 * charges, and riflemen. A horse holder: two more. A field gun: her crew. A support team's weapon (drawn with its three):
 * its ammunition bearers. A tank: none (her crew is inside).
 */
/** A man's group in his block: his section (its key man's k), and drawn as platoons which of its groups (Man.sub). */
const grp = (m: Man): number => (m.sec ?? m.k) * 8 + (m.sub ?? 0);

/** A rifle platoon by army (drawn as platoons): its squads, the key man's own among them, and its headquarters' men. */
export function platoonOf(nat: string): { squads: number; hq: number } {
  const P = DR.sections.platoon, p = P[nat] ?? P.default;
  return { squads: Math.max(1, Math.min(7, p.squads)), hq: Math.max(1, p.hq) };
}

/**
 * How many men a block's key men put on screen at a tier (GR3): 1, each key man and his section; 2, as well a rifle key
 * man's platoon (its other squads, each on its leader, and its headquarters). Counted as the 3D game shows them: a gun,
 * a tank and a lorry are no men (a gun's crew is its section), and a support team's weapon comes with the three the 3D
 * game draws beside it (drill.json sections: hmg 7 and mortar 8 count them).
 */
export const menOnScreen = (r: Role): number => (r === 'hmg' || r === 'mortar' ? 3 : VEHICLE.has(r) ? 0 : 1);
export function menDrawn(nat: string, keys: Role[], tier: 1 | 2): number {
  let c = 0;
  for (const r of keys) {
    c += menOnScreen(r);
    const sec = sectionOf(nat, r);
    c += sec.length;
    if (tier === 2 && r === 'rifle') { const P = platoonOf(nat); c += (P.squads - 1) * (1 + sec.length) + P.hq; }
  }
  return c;
}

export function sectionOf(nat: string, key: Role): [Role, boolean, boolean][] {
  const S = DR.sections, out: [Role, boolean, boolean][] = [];
  const add = (r: Role, c: number, pr = false, scout = false): void => { for (let i = 0; i < c; i++) out.push([r, pr, scout]); };
  switch (key) {
    case 'officer': add('rifle', S.officer - 1, true); break;
    case 'mg': add('loader', 2, true); add('rifle', S.mg - 3); break;
    case 'at': add('loader', S.at - 1, true); break;
    case 'sapper': add('sapper', 2); add('rifle', S.sapper - 3); break;
    case 'holder': add('holder', S.holder - 1, true); break;
    case 'gun': add('crew', S.gun - 1, true); break;
    case 'hmg': add('loader', S.hmg - 3, true); break;
    case 'mortar': add('loader', S.mortar - 3, true); break;
    case 'rifle': {
      const R = S.rifle[nat] ?? S.rifle.default;
      add('mg', 1); add('loader', R.loaders, true); add('rifle', R.scouts, false, true); add('rifle', R.men - 2 - R.loaders - R.scouts);
      break;
    }
    default: break;
  }
  return out;
}

/**
 * Where a block's n-th wreck lies in its box, metres across (right of its middle) and along (forward): the one formula
 * the rules use to lay a knocked-out tank's wreck (sim.ts wreck) and the drill uses to place each tank, so a tank burns
 * where she stood. Fixed by the block and the count, never by the battle's dice.
 */
export function wreckOffset(id: number, n: number, w: number, d: number): [number, number] {
  return [(mix(id * 977 + n * 131 + 7) - 0.5) * w * 0.9, (mix(id * 613 + n * 389 + 3) - 0.5) * d * 0.8];
}
