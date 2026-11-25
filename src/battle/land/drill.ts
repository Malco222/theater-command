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
import { LAND, MOTION, type Kind, type Team } from './rules';
import type { LandBattle } from './sim';
import type { LandUnit } from './units';

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
  | 'drive' | 'halt' | 'fire' | 'reverse' | 'towed' | 'deployed' | 'parked';
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
  /** Lorried foot drawn as platoons: his lorry (one to each squad and to the headquarters) and his place on its benches. */
  lor?: number;
  seat?: number;
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
  /** The shot stamp a round was last made for (Drill.fire), and when a rifleman's next shot (a machine gun's burst) goes. */
  fired?: number;
  burst?: number;
  /** A vehicle's scars, from the rounds that struck it (they stay to the end of the battle). */
  marks?: Mark[];
}

/**
 * What fired a round: a tank's gun, an anti-tank launcher or rifle, a field gun, a machine gun (a rifle section's or a
 * support team's on its tripod), a rifle, a grenade, a support team's mortar (release 2).
 */
export type Weapon = 'tank' | 'gun' | 'at' | 'mg' | 'rifle' | 'grenade' | 'mortar';
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
}
export interface MenOut {
  k: number; j?: number; r: Role; x: number; y: number; f: number; p: Pose; v: number; c: CoverKind;
  a?: [number, number]; s?: number; d?: number; h?: 1; b?: 1; mk?: Mark[];
}

interface Nums { rush: number; halt: number; wave: boolean; assault: number; spacing: number; tankRush: number; tankHalt: number; scoot: number; salvo: 'battery' | 'rapid' | 'ripple'; gunEvery: number }
const DR = data as unknown as {
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
};
export const DRILL = DR;

/** A fixed hash to [0, 1): the drill's own dice, never the battle's. */
const mix = (n: number): number => { let h = n >>> 0; h ^= h >>> 16; h = Math.imul(h, 0x85ebca6b); h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35); h ^= h >>> 16; return (h >>> 0) / 4294967296; };
const roll = (...ns: number[]): number => { let h = 0x9e3779b9; for (const n of ns) h = Math.imul(h ^ (Math.floor(n) >>> 0), 0x01000193) + 0x7f4a7c15; return mix(h); };
const clamp = (v: number, a: number, b: number): number => (v < a ? a : v > b ? b : v);
const live = (u: LandUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
const fighting = (u: LandUnit): boolean => live(u) && u.state !== 'routing';
const FOOT_KINDS: Kind[] = ['inf', 'mtn', 'eng', 'fort', 'cav'];
const isFoot = (u: LandUnit): boolean => MOTION[u.kind] === 'foot' || u.kind === 'mot';
const CLOSE: Ground[] = ['woods', 'buildings', 'industry', 'suburb'];
const BUILT: Ground[] = ['buildings', 'industry', 'suburb'];
/** Poses that carry a man over the ground: the 3D plays them only while he moves (walk: one he reaches his place in ends). */
const MOVING: ReadonlySet<Pose> = new Set<Pose>(['walk', 'run', 'advance', 'creep', 'crawl', 'flee', 'drive', 'reverse', 'towed']);
/** Poses that go with whatever carries him (a horse, a lorry, the horses he leads): they move at any speed. */
const CARRIED: ReadonlySet<Pose> = new Set<Pose>(['ride', 'mounted', 'horses']);
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
  men: Man[];
  /** Lorries (lorried infantry, a battery's tractor). */
  veh: Man[];
  fo: Man[] | null;
  foWith?: number;
  foOn?: number;
  /** Where the spotter's host block stood when he chose his place: he moves up when it has gone on 60 m. */
  foAt?: [number, number];
  phase: Phase;
  since: number;
  nextThink: number;
  /** Strength last step, and the share of strength it is losing a second (smoothed). */
  lastStr: number;
  harm: number;
  shelledAt: number;
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
  private readonly byOf = new Map<string, number>();
  /** Where each wreck lies as drawn (the rules' wreck id to where her tank fell): she burns where she stood. */
  private readonly wreckAt = new Map<number, [number, number]>();
  private readonly seed: number;
  private settled = false;
  /**
   * Each model drawn as its section (release 2; the 3D ground battle scene for Brian, 21 November 2026, 18:29Z: "the
   * ground battle needs a lot more soldiers in it"): the model's man is its key man, and the rest of his section stand
   * round him (DR.sections). Off, a model is one man, as before.
   */
  readonly sections: boolean;
  /** Drawn as platoons (release 2; Brian, 22 November: "I want a lot of troops in the ground battles"): each rifle model is
   * its platoon, its key man's squad leading, the platoon's other squads and its headquarters behind (sections too). */
  readonly platoons: boolean;

  constructor(b: LandBattle, opts: { sections?: boolean; platoons?: boolean } = {}) {
    this.b = b;
    this.seed = b.ticket.seed;
    this.sections = !!opts.sections || !!opts.platoons;
    this.platoons = !!opts.platoons;
    for (const u of b.units) this.squads.set(u.id, this.make(u));
  }

  /** A block placed in the setup step (release 2) before the clock starts: its men drawn up afresh where it now stands. */
  relay(u: LandUnit): void {
    if (this.b.t > 0 || !this.squads.has(u.id)) return;
    this.squads.set(u.id, this.make(u));
  }

  // ---- Who is in a block ------------------------------------------------------------------------------------------

  private make(u: LandUnit): Squad {
    const men: Man[] = [], veh: Man[] = [];
    const n = Math.max(0, u.figures0);
    const roles = rolesFor(u.kind, u.tray, n, u.team);
    const man = (k: number, nn: number, role: Role, team: 0 | 1 | 2): Man => {
      const jx = (roll(this.seed, u.id, nn, 1) - 0.5) * 6, jy = (roll(this.seed, u.id, nn, 2) - 0.5) * 5;
      return { k, n: nn, role, team, x: u.x, y: u.y, face: u.face, pose: 'idle', v: 0, spot: [u.x, u.y], cover: 'none', until: 0, slotAt: [u.x, u.y], jx, jy };
    };
    for (let k = 0; k < n; k++) men.push(man(k, k, roles[k][0], roles[k][1]));
    // The rest of each model's section, after the key men (so a key man is always first of his k); drawn as platoons, a
    // rifle model's other squads (each on its own leader) and its headquarters after it. Lorried, each group has a lorry.
    let lor = 0;
    if (this.sections) for (let k = 0; k < n; k++) {
      const key = men[k], mot = u.kind === 'mot' && this.platoons;
      if (mot) { key.lor = lor; key.seat = 0; }
      const sec = sectionOf(u.nat, key.role);
      sec.forEach(([role, pr, scout], i) => {
        const m = man(k, 1000 + k * 32 + i + 1, role, key.team);
        m.j = i + 1; m.sec = k;
        if (pr) m.pr = true;
        if (scout) m.scout = true;
        if (mot) { m.lor = lor; m.seat = i + 1; }
        men.push(m);
      });
      lor++;
      if (!this.platoons || key.role !== 'rifle') continue;
      const P = platoonOf(u.nat);
      let j = sec.length, x = 0;
      const add = (sub: number, role: Role, seat: number, opts: { pr?: boolean; lead?: boolean } = {}): void => {
        const m = man(k, 20000 + k * 128 + x++, role, key.team);
        m.j = ++j; m.sec = k; m.sub = sub;
        if (opts.pr) m.pr = true;
        if (opts.lead) m.lead = true;
        if (mot) { m.lor = lor; m.seat = seat; }
        men.push(m);
      };
      // The platoon's other squads, each its leader and the same section as the key man's (its scouts in the squad).
      for (let s = 1; s < P.squads; s++, lor++) {
        add(s, 'rifle', 0, { lead: true });
        sec.forEach(([role, pr], i) => add(s, role, i + 1, { pr }));
      }
      // Its headquarters: its leader and the men with him (a sergeant, the runners).
      add(P.squads, 'rifle', 0, { lead: true });
      for (let i = 1; i < P.hq; i++) add(P.squads, 'rifle', i);
      lor++;
    }
    // Lorried foot's lorries (drawn as sections, one to each section; as platoons, one to each group) and a battery's
    // tractor (one to each gun).
    const lorries = u.kind === 'mot' ? (this.sections ? lor : n >= 4 ? 2 : 1) : u.kind === 'art' && !u.team ? (this.sections ? n : 1) : 0; // a team's crews carry their weapons
    for (let i = 0; i < lorries; i++) veh.push({ k: 100 + i, n: 100 + i, role: 'lorry', team: 0, x: u.x, y: u.y, face: u.face, pose: 'parked', v: 0, spot: [u.x, u.y], cover: 'none', until: 0, slotAt: [u.x, u.y], jx: 0, jy: 0 });
    const sq: Squad = {
      u, men, veh, fo: null, phase: 'rest', since: 0, nextThink: roll(this.seed, u.id, 3) * 0.5, lastStr: u.str, harm: 0, shelledAt: -99,
      lastX: u.x, lastY: u.y, heading: u.face, speed: 0, track: [], wx: 0, wy: 0, net: 0, down: u.kind !== 'mot' && u.kind !== 'cav', fast: 0, slow: 0, close: false, ran: false, salvoAt: 0, ripple: 0, flank: 0, officerAt: 0, leap: true,
      m0: men.length, lost: 0,
    };
    // Drawn up where the battle opens: each man at his place in the line, in the best cover near it.
    this.layStart(sq);
    return sq;
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
    if (this.sections) {
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
    if (b.over) { if (!this.settled) { for (const sq of this.squads.values()) this.falls(sq); this.settle(); } return; }
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
      this.falls(sq);
      if (t >= sq.nextThink) { sq.nextThink = t + 0.5; this.think(sq); this.pairs(sq, this.space(sq)); }
      this.walk(sq);
      this.fire(sq);
    }
    this.shellKills.clear(); this.byOf.clear();
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
      for (const m of [...sq.men, ...sq.veh, ...(sq.fo ?? [])]) {
        if (m.fell !== undefined) continue;
        m.v = 0; m.went = 0; m.spot = [m.x, m.y]; m.aim = undefined; m.hidden = undefined;
        m.pose = m.role === 'tank' ? 'halt' : m.role === 'gun' || CREWED.has(m.role) ? 'deployed' : m.role === 'lorry' ? 'parked' : m.role === 'holder' ? (m.pose === 'ride' ? 'ride' : 'horses')
          : m.pose === 'mounted' || m.pose === 'ride' ? m.pose : sq.phase === 'flee' ? 'cower' : u.side === won ? (m.k % 3 ? 'idle' : 'wait') : 'wait';
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
    if (vq && v) this.killRound(by !== undefined ? this.b.unit(by) : undefined, vq, v, [x, y]);
  }

  /** A model the rules took with no striker on the field (the air's bombs and guns): no round of the field's for him. */
  fromAir(on: number, k: number): void { this.shellKills.add(on + ':' + k); }
  /** A model the rules took to a block's small arms (no shell): his round comes from that block (falls). */
  struckBy(by: number, on: number, k: number): void { this.byOf.set(on + ':' + k, by); }

  /** Men of every block near a burst duck. */
  private duck(x: number, y: number): void {
    for (const sq of this.squads.values()) if (Math.hypot(sq.u.x - x, sq.u.y - y) < Math.max(sq.u.w, sq.u.d) / 2 + 40) sq.shelledAt = this.b.t;
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
      if (m.role === 'tank') {
        // The rules lay a block's wrecks in the order its tanks fall (the lower place first within a tick), as here.
        const nth = sq.men.filter((x) => x.role === 'tank' && x.fell !== undefined && x !== m).length;
        const w = this.b.wrecks.filter((z) => z.unit === u.id)[nth];
        if (w) this.wreckAt.set(w.id, [m.x, m.y]);
      }
      // The round that took him (where he stood): a shell's came with it (Drill.shelled); else small arms (or
      // a launcher, or a grenade, on a tank) from the enemy block the rules say struck him (Drill.struckBy). A model the
      // rules took before the drill was turned on falls with no round.
      if (!shell && by !== undefined) this.killRound(this.b.unit(by), sq, m);
      m.pose = 'down';
      m.spot = [m.x, m.y];
      this.aid(sq, m);
      if (this.sections) this.regroup(sq, m);
    }
    if (this.sections) this.thin(sq);
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
  private thin(sq: Squad): void {
    const u = sq.u, b = this.b;
    if (u.state === 'withdrawn' || (u.state === 'gone' && (u.figures > 0 || u.y < 0 || u.y > b.field.h))) return;
    const want = u.str <= 0 ? 0 : Math.ceil((sq.m0 * u.str) / Math.max(1e-9, u.str0) - 1e-9);
    let up = 0;
    for (const m of sq.men) if (this.stands(sq, m) || (m.j !== undefined && m.fell === undefined)) up++;
    let n = up - want;
    if (n <= 0) return;
    const cand = sq.men.filter((m) => m.j !== undefined && m.fell === undefined);
    if (!cand.length) return;
    // Who struck: the enemy block owed most for the harm, else the one it faces.
    let by: LandUnit | undefined;
    const owed = u.owed ?? {};
    for (const id of Object.keys(owed).map(Number)) { const e = b.unit(id); if (e && live(e) && e.side !== u.side && (!by || owed[id] > owed[by.id])) by = e; }
    by ??= this.threat(u) ?? undefined;
    const shell = !!by && (by.kind === 'arm' || (by.kind === 'art' && by.team !== 'mg'));
    const from: [number, number] = by ? [by.x, by.y] : [u.x, u.y];
    let burst: [number, number] | undefined;
    while (n-- > 0 && cand.length) {
      let best = 0, score = Infinity;
      cand.forEach((m, i) => {
        const p = burst ?? from;
        const sc = Math.hypot(m.x - p[0], m.y - p[1]) * (2 - (DR.rounds.cover[m.cover] ?? 1)) + (burst ? 6 : 40) * roll(this.seed, u.id, m.n, sq.lost, 81);
        if (sc < score) { score = sc; best = i; }
      });
      const m = cand.splice(best, 1)[0];
      sq.lost++;
      // A shell's burst: the first man's place, and the men near it after him (beyond 12 m of it, another shell).
      if (shell && (!burst || Math.hypot(m.x - burst[0], m.y - burst[1]) > 12)) { burst = [m.x, m.y]; this.duck(m.x, m.y); }
      m.fell = b.t; m.v = 0; m.aim = undefined; m.hidden = undefined;
      if (by) this.killRound(by, sq, m, shell ? burst : undefined);
      m.pose = 'down';
      m.spot = [m.x, m.y];
      this.aid(sq, m);
    }
    sq.memo = undefined;
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
    if (hot && !moving && u.kind !== 'arm') return 'pinned';
    if (u.engaged !== undefined) return moving ? 'bound' : 'fight';
    if (moving) {
      if (near < DR.ranges.bound || sq.harm > 0.0005) return 'bound';
      return near < DR.ranges.column ? 'deploy' : 'column';
    }
    // The main effort lying at its start line under its guns (ai.ts: the plan's 'guns' phase).
    const P = b.plan[u.side];
    if (P && P.phase === 'guns' && P.role[u.id] === 'main' && near < DR.ranges.column) return 'startline';
    if (u.reserve === 'held' && near > 250) return 'rest';
    return near < DR.ranges.column || u.side === 'def' ? 'hold' : 'rest';
  }

  /** How readily its men go to ground: raw troops sooner, veterans later. */
  private pinScale(u: LandUnit): number { return 1 / DR.xp.pinned[this.band(u)]; }
  private band(u: LandUnit): 0 | 1 | 2 { return u.xp < DR.xp.raw ? 0 : u.xp >= DR.xp.veteran ? 2 : 1; }
  private nums(u: LandUnit): Nums {
    const s = this.b.style[u.side];
    return DR.style[s && this.b.ai[u.side] ? s.id : 'plain'];
  }

  /** Metres from this block's edge to the nearest enemy that is still fighting. */
  private nearestGap(u: LandUnit): number {
    let g = Infinity;
    for (const e of this.b.units) if (e.side !== u.side && fighting(e)) g = Math.min(g, this.b.gap(u, e));
    return g;
  }

  /** What the block faces: who it fights, else its order's target, else the nearest enemy, else nothing. */
  private threat(u: LandUnit): LandUnit | null {
    const b = this.b;
    if (u.engaged !== undefined) { const e = b.unit(u.engaged); if (e && live(e)) return e; }
    if (u.order?.target !== undefined) { const e = b.unit(u.order.target); if (e && live(e)) return e; }
    let best: LandUnit | null = null, d = 1500;
    for (const e of b.units) if (e.side !== u.side && fighting(e)) { const g = b.gap(u, e); if (g < d) { d = g; best = e; } }
    return best;
  }
  private threatDir(sq: Squad): [number, number] {
    const u = sq.u, e = this.threat(u);
    if (!e) return fwd(u.face);
    const d = Math.hypot(e.x - u.x, e.y - u.y) || 1;
    return [(e.x - u.x) / d, (e.y - u.y) / d];
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
    if (!sq.down && sq.slow >= MOUNT.downAfter && (ph === 'fight' || ph === 'pinned' || ph === 'hold' || ph === 'startline' || this.nearestGap(u) < DR.ranges.debus || sq.harm > 0)) {
      if (u.kind === 'mot' || (u.kind === 'cav' && this.nearestGap(u) < DR.ranges.debus * 0.85)) { got = sq.down = true; this.alight(sq); }
    }
    // Down, and the block goes on faster than they can follow on foot: up again (board). Slowed again while they mount:
    // down where they are.
    if (sq.down && ph !== 'gone' && (u.kind === 'cav' || (u.kind === 'mot' && sq.veh.length))) {
      sq.close = sq.fast >= 1 ? true : sq.slow >= 3 ? false : sq.close;
      if (sq.boarding === undefined && sq.fast >= MOUNT.upAfter) this.mountUp(sq);
      else if (sq.boarding !== undefined && sq.slow >= MOUNT.stayAfter) { got = true; this.alight(sq); }
    }
    // Just down: each man goes from his lorry or his horse to his place, as on a fresh phase.
    if (ph !== was || got) { sq.phase = ph; sq.since = t; }
    ph = sq.phase;
    sq.ran = u.state === 'routing' || (sq.ran && u.state === 'wavering' && t - sq.since < 8);
    if (ph === 'gone') return;
    if (u.kind === 'arm') { this.tanks(sq); return; }
    if (u.kind === 'art') { if (u.team) this.teams(sq); else this.guns(sq); return; }
    if (sq.boarding !== undefined) { this.board(sq); return; }
    this.lorries(sq);
    if ((u.kind === 'mot' || u.kind === 'cav') && !sq.down) { this.mounted(sq); return; }
    switch (ph) {
      case 'column': this.column(sq); break;
      case 'deploy': this.advance(sq, false); break;
      case 'bound': this.advance(sq, true); break;
      case 'startline': this.lie(sq, 'wait'); break;
      case 'fight': this.fight(sq); break;
      case 'hold': this.hold(sq); break;
      case 'rest': this.rest(sq); break;
      case 'pinned': this.pinned(sq); break;
      case 'fallback': this.fallback(sq); break;
      case 'flee': this.flee(sq); break;
      case 'rally': this.rally(sq); break;
      default: break;
    }
    this.ambush(sq);
    this.officer(sq);
    for (const m of sq.men) if (m.fell === undefined && MOVING.has(m.pose) && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 0.3) this.arrive(sq, m);
  }

  /**
   * He has reached his place: off his feet into the pose for it (no man runs on the spot). A rifleman with a target
   * fires from it, without one he kneels watching; a routing man crouches where he got to; a tank halts, a gun towed
   * into the line unhooks, a lorry parks.
   */
  private arrive(sq: Squad, m: Man): void {
    m.v = 0;
    // One who keeps by a weapon or an officer waits there for it (pairs gives him his pose).
    if (m.pr) { m.pose = m.pose === 'flee' ? 'cower' : 'wait'; return; }
    switch (m.role) {
      case 'tank': m.pose = 'halt'; return;
      case 'lorry': m.pose = 'parked'; return;
      case 'gun': if (m.pose === 'towed' && sq.phase === 'battery') m.pose = 'deployed'; return;
      // A support team's crew sets its weapon up in the line, or down a moment on the march; a routing one crouches.
      case 'hmg': case 'mortar': m.pose = m.pose === 'flee' ? 'cower' : sq.phase === 'battery' ? 'deployed' : 'wait'; return;
      default: {
        m.until = Math.max(m.until, this.b.t + 1.5);
        if (m.pose === 'flee') { m.pose = 'cower'; return; }
        if (m.aid !== undefined) { m.pose = 'wait'; m.aim = undefined; return; } // kneeling over his fallen mate
        if (!m.aim && m.role !== 'officer' && (sq.phase === 'fight' || sq.phase === 'pinned')) m.aim = this.aimAt(m, this.threat(sq.u));
        if (m.aim && m.role !== 'officer') m.pose = this.firePose(m);
        else m.pose = m.role === 'mg' && sq.phase !== 'fight' ? 'hide' : 'wait';
      }
    }
  }

  /** The men still standing, not helping a fallen comrade. */
  private up(sq: Squad): Man[] { return this.memo(sq).up.filter((m) => !this.aiding(m) && !(m.role === 'holder' && sq.down)); }
  /**
   * Whether a man is on his feet: a key man while the rules keep his model; the rest of a section while their block is
   * on the field and he has not fallen (falls).
   */
  private stands(sq: Squad, m: Man): boolean { return m.fell === undefined && (m.j === undefined ? m.k < sq.u.figures : live(sq.u)); }

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
    const all = this.sections ? [...up].sort((a, c) => (a.sec ?? a.k) - (c.sec ?? c.k) || (a.j ?? 0) - (c.j ?? 0)) : up;
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
    if (!this.sections) return;
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
    if (!this.sections) return undefined;
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
    const u = sq.u, N = this.nums(u), band = this.band(u), M = this.memo(sq), S = DR.sections, sec = this.sections;
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
    const l = u.d / 2 - 4 + (i % 2 ? (sec ? -(this.platoons ? S.platoon.rows : S.rows) : -3) : 0) + m.jy * 0.5;
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
  private follow(sq: Squad, m: Man, s: [number, number], pose: Pose, mul = 1): void {
    const v = sq.speed, on = v > 0.4, [hx, hy] = fwd(sq.heading);
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

  /** The nearest standing man of the block it fights, else a point in that block: where he fires. */
  private aimAt(m: Man, e: LandUnit | null): [number, number] | undefined {
    if (!e) return undefined;
    const sq = this.squads.get(e.id);
    if (!sq) return [e.x, e.y];
    // The nearest key man, and (drawn as sections) the nearest man of his section.
    const M = this.memo(sq);
    let best: Man | undefined, d = Infinity;
    for (const o of M.keys) { const dd = Math.hypot(o.x - m.x, o.y - m.y); if (dd < d) { d = dd; best = o; } }
    if (best && this.sections) for (let g = 0; g < 8; g++) for (const o of M.bySec.get(best.k * 8 + g) ?? []) { const dd = Math.hypot(o.x - m.x, o.y - m.y); if (dd < d) { d = dd; best = o; } }
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
    const close = e ? this.b.gap(u, e) < DR.ranges.bound : false; // within rifle and machine-gun range
    // Leapfrog only where a man can run the block's whole cycle of ground in his rush; faster, the rolling advance.
    const vLeap = 4.7 * N.rush / (N.rush + N.halt);
    sq.leap = sq.leap ? v < vLeap * 1.1 : v < vLeap * 0.9;
    for (const m of this.up(sq)) {
      const [a, l] = this.slot(sq, m, 'line');
      const s = toWorld(u, u.face, a, l);
      if (!under) {
        m.aim = undefined; m.until = t + 1;
        this.follow(sq, m, s, close ? 'advance' : 'walk');
        continue;
      }
      if (m.role === 'officer' && sq.leap) {
        // The officer goes with whichever team is moving, crouched, a few paces behind it.
        this.follow(sq, m, s, 'creep'); m.aim = undefined;
        continue;
      }
      if (sq.leap) {
        // Leapfrog: team 1 and team 2 on alternate rushes (a mass: together).
        const cyc = N.rush + N.halt, off = N.wave ? 0 : m.team === 2 ? cyc / 2 : 0;
        const at = t + off + m.jx * 0.08 + 100 * cyc, n = Math.floor(at / cyc);
        const rushing = at - n * cyc < N.rush;
        const gain = (s[0] - m.x) * hx + (s[1] - m.y) * hy, behind = gain > 30;
        // To where his place will be halfway through the halt that follows: on average he keeps his place.
        const lead = v * (N.rush + N.halt / 2);
        // Already there (he got ahead of his line): he lets this rush go by, firing (never a rush back to his place).
        if (rushing && m.rushed !== n && gain + lead < 3) m.rushed = n;
        if (rushing && m.rushed !== n && m.role !== 'officer' || behind && m.pose !== 'advance') {
          // Up and forward to the next cover, once a cycle (and whenever he has fallen well behind his place).
          this.go(sq, m, [s[0] + hx * lead, s[1] + hy * lead], 'advance', 3, t + N.rush, dir, 6);
          // Fast enough to be down again before his rush is over: a crouched run, a sprint where he has far to go.
          m.v = clamp(Math.hypot(m.spot[0] - m.x, m.spot[1] - m.y) / (N.rush * 0.85), 2, behind ? CATCH_UP : 5.5);
          m.rushed = n; m.aim = undefined;
        } else if (m.pose === 'advance' || m.pose === 'creep') {
          if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1.5 && t < m.until + 1) continue;
          // Down where he got to, firing if anything is in reach of his rifle (the officer watching, directing).
          m.v = 0; m.spot = [m.x, m.y]; m.cover = this.shelter(m.x, m.y, groundOf(this.b.field, m.x, m.y), dir, true, false).kind;
          m.aim = close && m.role !== 'officer' ? this.aimAt(m, e) : undefined;
          m.pose = m.aim ? this.firePose(m) : 'wait'; m.until = t + 1.5;
        } else if (t >= m.until && close && m.role !== 'officer') { m.aim = this.aimAt(m, e); m.pose = this.firePose(m); m.until = t + 1.5; }
      } else {
        // Rolling: a few at a time drop to fire, then sprint on.
        const share = 0.3, cyc = 6;
        const ph = (t + roll(this.seed, u.id, m.n, 11) * cyc) % cyc;
        const behind = ((s[0] - m.x) * hx + (s[1] - m.y) * hy) > 12;
        if (ph < cyc * share && m.role !== 'officer' && !behind) {
          if (m.pose !== 'fire_prone' && m.pose !== 'lmg_burst' && m.pose !== 'fire_kneel') { m.spot = [m.x, m.y]; m.cover = 'none'; }
          m.v = 0; m.pose = close ? (m.role === 'mg' ? 'lmg_burst' : 'fire_prone') : 'wait'; m.aim = close ? this.aimAt(m, e) : undefined;
        } else {
          m.aim = undefined;
          this.follow(sq, m, s, 'advance', 1.1);
        }
        m.until = t + 0.5;
      }
    }
  }

  /** Lying where they are, in the best cover near their places, in one pose (the start line: waiting for the guns to lift). */
  private lie(sq: Squad, pose: Pose): void {
    const u = sq.u, t = this.b.t, dir = this.threatDir(sq);
    for (const m of this.up(sq)) {
      if (m.role === 'officer') continue; // he walks his line (officer)
      const [a, l] = this.slot(sq, m, 'line');
      const s = toWorld(u, u.face, a, l);
      if (sq.since === t || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 5 || t >= m.until) this.go(sq, m, s, pose, 3, t + 30, dir);
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 1) { m.pose = m.role === 'mg' ? 'hide' : pose; m.v = 0; }
      // Not there yet: a run where it is far (off a lorry, a horse), a crawl for the last stretch; each at his own pace,
      // so men going the same way string out and never move as one.
      else { const far = Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 15, r = roll(this.seed, u.id, m.n, 41); m.pose = far ? 'run' : 'creep'; m.v = far ? 3.8 + 1.4 * r : 2.1 + 0.6 * r; }
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
      const moved = fresh || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 6;
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

  /** Holding its ground, watching: in the best cover along its front, a sentry up at each end, the rest ready. */
  private hold(sq: Squad): void {
    const u = sq.u, t = this.b.t, dir = this.threatDir(sq), near = this.nearestGap(u);
    for (const m of this.up(sq)) {
      if (m.role === 'officer') continue; // he walks his line (officer)
      const [a, l] = this.slot(sq, m, 'line');
      const s = toWorld(u, u.face, a, l);
      if (sq.since === t || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 5 || (m.v > 0 && t >= m.until)) { this.go(sq, m, s, 'creep', 2.5, t + 1, dir); continue; }
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
      m.v = 0; m.aim = undefined;
      if (m.role === 'at' && m.hidden) continue;
      m.pose = m.role === 'mg' ? 'hide' : near < 600 ? 'wait' : roll(this.seed, u.id, m.n, Math.floor(t / 8)) < 0.3 ? 'idle' : 'wait';
    }
  }

  /** A reserve resting out of the fight: in knots, in cover, ready; the headquarters at the middle. */
  private rest(sq: Squad): void {
    const u = sq.u, t = this.b.t, dir = this.threatDir(sq);
    for (const m of this.up(sq)) {
      const [a, l] = this.slot(sq, m, 'rest');
      const s = toWorld(u, u.face, a, l);
      if (sq.since === t || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 5 || (m.v > 0 && t >= m.until)) { this.go(sq, m, s, 'walk', 2, t + 1, dir); continue; }
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
      m.v = 0; m.aim = undefined;
      m.pose = (m.k % 5 === 1) ? 'wait' : roll(this.seed, u.id, m.n, Math.floor(t / 10)) < 0.5 ? 'idle' : 'wait';
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
      const cyc = N.rush + N.halt, ph = (t + (m.team === 2 ? cyc / 2 : 0) + m.jx * 0.08 + 100 * cyc) % cyc;
      const cover = sq.leap ? ph >= N.rush : roll(this.seed, u.id, m.n, Math.floor(t / 2)) < 0.2;
      if (cover && m.role !== 'officer' && e && this.b.gap(u, e) < 500) { m.v = 0; m.spot = [m.x, m.y]; m.pose = m.role === 'mg' ? 'lmg_burst' : 'fire_kneel'; m.aim = this.aimAt(m, e); }
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
      this.follow(sq, m, s, 'flee', 1.1);
    }
  }

  /** Rallied: the men gather on their officer, facing the enemy again. */
  private rally(sq: Squad): void {
    const u = sq.u, t = this.b.t, dir = this.threatDir(sq);
    const men = this.up(sq), lead = men.find((m) => m.role === 'officer') ?? men[0];
    if (!lead) return;
    lead.v = 0; lead.spot = [lead.x, lead.y]; lead.pose = 'idle'; lead.aim = undefined;
    men.forEach((m, i) => {
      if (m === lead) return;
      const ang = (i / men.length) * Math.PI * 2, r = this.sections ? 7 + 0.25 * men.length : 7;
      const p: [number, number] = [lead.x + Math.sin(ang) * r + dir[0] * 4, lead.y + Math.cos(ang) * r + dir[1] * 4];
      if (Math.hypot(m.spot[0] - p[0], m.spot[1] - p[1]) > 4) this.go(sq, m, p, 'run', 4, t + 2, dir, 4);
      else if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 1) { m.v = 0; m.pose = 'wait'; }
    });
    void u;
  }

  /** Lorried infantry still aboard, cavalry still mounted: they ride where their block goes. */
  private mounted(sq: Squad): void {
    const u = sq.u, t = this.b.t;
    if (u.kind === 'mot') {
      sq.veh.forEach((v, i) => {
        const s = toWorld(u, sq.heading, 0, u.d / 2 - 8 - i * 18);
        this.follow(sq, v, s, 'drive'); v.until = t + 1;
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

  /** Off the lorries and horses where they are, each on his feet: the block's phase then sends him to his place. */
  private alight(sq: Squad): void {
    sq.boarding = undefined;
    for (const m of sq.men) if (m.fell === undefined && (m.pose === 'mounted' || m.pose === 'ride')) { m.pose = 'wait'; m.v = 0; m.spot = [m.x, m.y]; m.aim = undefined; m.cover = 'none'; }
  }

  /**
   * Down, and the block goes on faster than men on foot can follow: they mount again. Each lorried man makes for his own
   * lorry; each trooper for the horse his holder leads (his own four's holder, else the nearest one left; with none left
   * his horse is by him).
   */
  private mountUp(sq: Squad): void {
    const u = sq.u;
    sq.boarding = this.b.t;
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

  /** A trooper's horse holder: the one at the end of his four, else the nearest still standing. */
  private holderOf(sq: Squad, m: Man): Man | undefined {
    const held = sq.men.filter((h) => h.role === 'holder' && !h.pr && this.stands(sq, h));
    return held.find((h) => Math.floor(h.k / 4) === Math.floor(m.k / 4)) ?? this.nearest(held, m)?.[0];
  }

  /** The lorry a lorried man rides in: his own, by his place on the tray. */
  private lorryOf(sq: Squad, m: Man): Man { return sq.veh[this.lorryIx(sq, m)]; }
  /** Which of the block's lorries is his: by his place on the tray, or (drawn as sections) his section's own. */
  private lorryIx(sq: Squad, m: Man): number { return (m.lor ?? (this.sections ? m.sec ?? m.k : m.k)) % sq.veh.length; }
  /** His seat in his lorry's bed, two benches a stride apart, front to back (so men getting down are never one knot). */
  private seat(sq: Squad, m: Man): [number, number] {
    const v = this.lorryOf(sq, m);
    // Drawn as sections, a section to a lorry (a gun's crew to its tractor): its key man first, close benches.
    if (this.sections) { const j = m.seat ?? m.j ?? 0; return toWorld(v, v.face, j % 2 ? 0.55 : -0.55, 1.4 - 0.6 * Math.floor(j / 2)); }
    const j = Math.floor(m.k / sq.veh.length);
    return toWorld(v, v.face, j % 2 ? 0.55 : -0.55, 0.5 - 1.1 * Math.floor(j / 2));
  }

  /**
   * Where a man mounting runs to: his lorry, or his horse at his holder's side (the holder's string of horses spread
   * across his right and left, one per man he leads, in their order); null where he has no horse led for him.
   */
  private mountAt(sq: Squad, m: Man): [number, number] | null {
    const u = sq.u;
    if (u.kind === 'mot') return this.seat(sq, m);
    const h = m.bus === undefined ? undefined : sq.men.find((x) => x.k === m.bus && x.j === undefined && this.stands(sq, x));
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
        if (m.bus === undefined || !men.some((h) => h.k === m.bus && h.j === undefined && h.role === 'holder')) m.bus = this.holderOf(sq, m)?.k;
        if (m.bus === undefined) { m.pose = 'ride'; m.v = 0; m.spot = [m.x, m.y]; m.aim = undefined; m.cover = 'none'; continue; }
        all = false;
        afoot(m, men.find((h) => h.k === m.bus && h.j === undefined)!);
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
    const h = sq.men.find((x) => x.k === m.bus && x.j === undefined && x.fell === undefined);
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
    sq.veh.forEach((v, i) => {
      if (sq.phase === 'flee' || sq.phase === 'fallback' || sq.close) {
        const s = toWorld(u, u.face, (i - 0.5) * 20, -u.d / 2 - (sq.close ? 30 : 40));
        this.follow(sq, v, s, 'drive', 1.2); return;
      }
      const s = toWorld(u, u.face, (i - (sq.veh.length - 1) / 2) * 24, -u.d / 2 - 110);
      if (Math.hypot(s[0] - v.slotAt[0], s[1] - v.slotAt[1]) > 25 || t < 0.2) {
        const best = this.seek(sq, v, s, this.threatDir(sq), sq.veh, 30);
        v.spot = [best.x, best.y]; v.slotAt = s; v.cover = best.kind; v.pose = 'drive'; v.v = Math.min(16, Math.max(6, sq.speed * 1.3));
      } else if (Math.hypot(v.x - v.spot[0], v.y - v.spot[1]) < 1.5) { v.pose = 'parked'; v.v = 0; }
    });
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
    const tanks = b.units.filter((e) => e.side !== u.side && e.kind === 'arm' && fighting(e) && b.gap(u, e) < DR.ranges.ambush);
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
  private tanks(sq: Squad): void {
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
    tanks.forEach((m, i) => {
      m.buttoned = lone || (e !== null && b.gap(u, e) < 120) || undefined;
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
      if (ph === 'deploy' || (ph === 'bound' && v >= 3.5)) {
        // All on, a tank halting a moment to fire where one is in reach: for a shot (1.5 s of each 4, her own), and never
        // while she is behind her place, or she would fall far behind a block going on at 10 m a second.
        const [hx, hy] = fwd(h), lag = (s[0] - m.x) * hx + (s[1] - m.y) * hy, at = t + roll(this.seed, u.id, m.n, 41) * 4;
        const halt = ph === 'bound' && e && b.gap(u, e) < 600 && lag < 6 && at % 4 < 1.5 && roll(this.seed, u.id, m.n, Math.floor(at / 4)) < 0.25;
        if (halt) { m.spot = [m.x, m.y]; m.v = 0; m.pose = this.tankShot(sq, m, e, N) ? 'fire' : 'halt'; m.aim = this.aimAt(m, e); }
        else { this.follow(sq, m, s, 'drive'); m.aim = e && b.gap(u, e) < 900 ? [e.x, e.y] : undefined; }
        return;
      }
      if (ph === 'bound') {
        // Slow going: the pairs leapfrog.
        const pair = m.k < 2 ? 0 : 1, cyc = N.tankRush + N.tankHalt, off = pair ? cyc / 2 : 0;
        const moving = (t + off + 100 * cyc) % cyc < N.tankRush;
        if (moving) {
          if ((m.pose !== 'drive' && m.pose !== 'reverse') || t >= m.until) { const [hx, hy] = fwd(h); this.go(sq, m, [s[0] + hx * v * cyc * 0.45, s[1] + hy * v * cyc * 0.45], 'drive', v * cyc / N.tankRush, t + N.tankRush, dir); }
          m.aim = e ? [e.x, e.y] : undefined;
        } else if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 2 || (m.pose !== 'drive' && m.pose !== 'reverse')) {
          m.v = 0; m.spot = [m.x, m.y]; m.pose = e && this.tankShot(sq, m, e, N) ? 'fire' : 'halt'; m.aim = this.aimAt(m, e);
        }
        return;
      }
      // Halted: in a fight, holding, shaken. Her firing place near her own: hull-down where the ground gives one. In a
      // fight she fights from alternate posts (every army but a mass): a few shots from one, then out of sight backward,
      // across behind the line and up again at the next, to one side of her own place and then the other.
      const shaken = ph === 'pinned' || u.state === 'wavering';
      const posts = N.scoot > 0 && !shaken && !lone && u.engaged !== undefined;
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
      m.v = 0;
      if (!e || (u.engaged === undefined && b.gap(u, e) > 600)) { m.pose = 'halt'; m.aim = e ? [e.x, e.y] : undefined; return; }
      m.aim = this.aimAt(m, e);
      // Her shots from this post told, and the last of them seen go: out of sight backward, to move to her next.
      const need = T.shots[0] + Math.floor(roll(this.seed, u.id, m.n, m.post ?? 0, 23) * (T.shots[1] - T.shots[0] + 1));
      if (posts && (m.shots ?? 0) >= need && m.shot !== undefined && t - m.shot >= 1.5) {
        m.cyc = 'back'; m.spot = this.leash(sq, [m.x - dir[0] * T.back, m.y - dir[1] * T.back], -4); m.pose = 'reverse'; m.v = 2.5; m.until = t + 8;
        return;
      }
      if (u.engaged !== undefined && this.tankShot(sq, m, e, N)) { m.pose = 'fire'; m.shots = (m.shots ?? 0) + 1; }
      else m.pose = 'halt';
    });
    // A tank that has just fired shows it a moment (the 3D plays the round by its stamp).
    for (const m of tanks) if (m.pose === 'halt' && m.shot !== undefined && t - m.shot < 1.5) m.pose = 'fire';
  }

  /** Whether this tank fires now (each about every 6 to 10 s while her block fights). */
  private tankShot(sq: Squad, m: Man, e: LandUnit | null, N: Nums): boolean {
    if (!e || sq.u.engaged === undefined) return false;
    const t = this.b.t, every = 6 + 4 * roll(this.seed, sq.u.id, m.n, 31);
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
    const own = this.sections && sq.veh.length > 1;
    if (sq.phase === 'limbered' || sq.phase === 'flee' || sq.phase === 'fallback') {
      guns.forEach((g, i) => { g.aim = undefined; this.follow(sq, g, toWorld(u, h, 0, -10 - i * (own ? 26 : 18)), 'towed'); if (g.v === 0) g.pose = 'towed'; });
      if (own) guns.forEach((g) => { const v = this.lorryOf(sq, g); this.follow(sq, v, toWorld(g, g.face, 0, 7), 'drive'); });
      else if (tractor) { this.follow(sq, tractor, toWorld(u, h, 0, 8), 'drive'); }
      this.spotter(sq, null, false); // the pair stays where it is, watching, till the guns fire again
      return;
    }
    sq.veh.forEach((tr, i) => {
      if (!own && i > 0) return;
      const s = toWorld(u, u.face, own ? (i - (sq.veh.length - 1) / 2) * 16 : 0, -u.d / 2 - 50);
      if (Math.hypot(s[0] - tr.slotAt[0], s[1] - tr.slotAt[1]) > 20) { tr.spot = s; tr.slotAt = s; tr.pose = 'drive'; tr.v = 4; } else if (Math.hypot(tr.x - tr.spot[0], tr.y - tr.spot[1]) < 2) { tr.pose = 'parked'; tr.v = 0; }
    });
    const firing = u.engaged !== undefined && !!e;
    const direct = !!e && (b.gap(u, e) < LAND.guns.closeAt || (e.kind === 'arm' && b.gap(u, e) < 800));
    const hit = t - sq.shelledAt < DR.pin.shelled || sq.harm > DR.pin.harm;
    // The salvo clock.
    let fireNow: (g: Man, i: number) => boolean = () => false;
    if (firing && t >= sq.salvoAt) {
      if (N.salvo === 'battery') { sq.salvoAt = t + N.gunEvery; fireNow = () => true; }
      else if (N.salvo === 'ripple') { const i0 = sq.ripple % Math.max(1, guns.length); sq.ripple++; sq.salvoAt = t + N.gunEvery / Math.max(1, guns.length); fireNow = (_g, i) => i === i0; }
      else { sq.salvoAt = t + 0.5; fireNow = (g) => g.shot === undefined || t - g.shot >= N.gunEvery * (0.7 + 0.3 * roll(this.seed, u.id, g.n, Math.floor(t / 10))); }
    }
    guns.forEach((g, i) => {
      const [a, l] = this.slot(sq, g, 'line');
      const s = toWorld(u, u.face, a, l);
      if (Math.hypot(s[0] - g.slotAt[0], s[1] - g.slotAt[1]) > 12) { this.go(sq, g, s, 'towed', 3, t + 4, dir); g.aim = undefined; return; }
      if (Math.hypot(g.x - g.spot[0], g.y - g.spot[1]) > 1.5) return;
      g.v = 0;
      g.hidden = hit && roll(this.seed, u.id, g.n, Math.floor(t)) < 0.5 ? true : undefined; // the crew down a moment
      if (firing && fireNow(g, i) && !g.hidden) { g.shot = t; g.pose = 'fire'; }
      else if (g.pose !== 'fire' || (g.shot !== undefined && t - g.shot > 2.5)) g.pose = 'deployed';
      g.aim = e ? [e.x + (roll(this.seed, u.id, g.n, t) - 0.5) * e.w * 0.6, e.y + (roll(this.seed, u.id, g.n, t, 1) - 0.5) * e.d * 0.6] : undefined;
      g.buttoned = direct || undefined; // over open sights
    });
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
    if (!mg && firing && t >= sq.salvoAt) {
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
      else if (firing && fireNow(g, i) && !g.hidden) { g.shot = t; g.pose = 'fire'; }
      else if (g.pose !== 'fire' || (g.shot !== undefined && t - g.shot > 2.5)) g.pose = 'deployed';
      g.aim = e ? [e.x + (roll(this.seed, u.id, g.n, t) - 0.5) * e.w * 0.6, e.y + (roll(this.seed, u.id, g.n, t, 1) - 0.5) * e.d * 0.6] : undefined;
    });
    this.spotter(sq, e, !mg && firing && !!e && b.gap(u, e) > DR.ranges.fo);
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
      const taken = [...this.squads.values()].filter((o) => o !== sq && o.fo && o.u.side === u.side).map((o) => o.fo![0].slotAt);
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
    const crowd = this.sections ? new Map<number, Man[]>() : null;
    if (crowd) for (const m of sq.men) {
      if (m.fell !== undefined || VEHICLE.has(m.role) || CARRIED.has(m.pose)) continue;
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
    };
    const walkOne = (m: Man): void => {
      // A man firing, kneeling or lying is still: he goes nowhere until his next order gets him up.
      if (m.v > 0 && !MOVING.has(m.pose) && !CARRIED.has(m.pose)) { m.v = 0; m.spot = [m.x, m.y]; }
      if (m.role === 'tank') m.v = Math.min(m.v, tankTop(sq));
      const veh = m.role === 'tank' || m.role === 'lorry' || m.role === 'gun';
      const rate = (m.role === 'tank' ? TURN.tank : veh ? TURN.wheels : m.pose === 'ride' ? TURN.ride : TURN.foot) * DT;
      const dx = m.spot[0] - m.x, dy = m.spot[1] - m.y, d = Math.hypot(dx, dy);
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
          m.x = clamp(nx, 1, f.w - 1); m.y = clamp(ny, 1, f.h - 1);
          // At the field's edge he can go no further that way: he halts there (no one marches on the spot against it).
          if (s > 0.05 && Math.hypot(nx - m.x, ny - m.y) > 0.8 * s && MOVING.has(m.pose)) { m.spot = [m.x, m.y]; this.arrive(sq, m); }
        }
        if (s >= d - 1e-9 && MOVING.has(m.pose) && m.role !== 'lorry' && sq.phase !== 'column' && sq.phase !== 'limbered' && sq.boarding === undefined) this.arrive(sq, m);
      } else {
        // Still: toward what he fires at, else toward the enemy, turning as he can.
        let want: number | undefined;
        if (m.aim) want = bearing(m.x, m.y, m.aim[0], m.aim[1]);
        else if (m.v === 0 && fighting(u)) { if (e === undefined) e = this.threat(u); if (e) want = bearing(m.x, m.y, e.x, e.y); }
        if (want !== undefined) m.face = turnTo(m.face, want, rate);
      }
      // Never far from his block: the block is the truth (the rules' box, and a margin round it). Left outside it, where
      // the block turned or went on without him, he runs back into it on his own feet (his tracks, its wheels), facing
      // his way: never thrown, never dragged. Only the block's going carries him, and only beyond the ring round its box
      // (a turn of the box never moves the ring).
      const extra = m.role === 'lorry' ? 120 : 0;
      const [lx, ly] = this.leash(sq, [m.x, m.y], extra);
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
        m.pose = rev ? 'reverse' : m.role === 'tank' || m.role === 'lorry' ? 'drive' : m.role === 'gun' ? 'towed' : sq.phase === 'flee' ? 'flee' : sq.phase === 'bound' || sq.phase === 'fight' ? 'advance' : 'run';
        m.until = t + 1;
      }
      // A lorried man or a trooper may be further out (running to his lorry or his horse) before the ring takes him.
      this.ring(sq, m, extra || (u.kind === 'mot' || u.kind === 'cav' ? RIDERS_RING : 0));
    };
    for (const m of sq.veh) step(m);
    for (const m of sq.men) {
      if (m.fell !== undefined) { m.went = 0; continue; }
      // Aboard: he sits in his seat in his lorry, wherever it goes (in the stride he climbs in, from beside it to his seat).
      if (m.pose === 'mounted' && (u.kind === 'mot' || m.role === 'crew') && sq.veh.length) {
        const lorry = this.lorryOf(sq, m), x0 = m.x, y0 = m.y, p = this.seat(sq, m);
        m.x = p[0]; m.y = p[1]; m.face = turnTo(m.face, lorry.face, TURN.foot * DT); m.went = Math.hypot(m.x - x0, m.y - y0) / DT; m.spot = p; m.v = lorry.v;
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
      // A support team's machine gun on its tripod fires long bursts, close together, all the while it is firing.
      const hmg = m.role === 'hmg' && m.pose === 'fire';
      if (m.aim && (hmg || (SMALL_ARMS.has(m.pose) && m.role !== 'at' && !VEHICLE.has(m.role)))) {
        const w: Weapon = m.role === 'mg' || hmg ? 'mg' : 'rifle', [lo, hi] = hmg ? DR.teams.hmg.burst : R.smallArms[w];
        if (m.burst === undefined) m.burst = t + hi * roll(this.seed, u.id, m.n, t * 10, 41);
        else if (t >= m.burst) { m.burst = t + lo + (hi - lo) * roll(this.seed, u.id, m.n, t * 10, 42); this.round(sq, m, w, m.aim); }
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
        if (!VEHICLE.has(q.role)) continue;
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
    const base: Round = { t: Math.round(t * 10) / 10, l, by: u.id, k: m.k, w, on: on.id, x: 0, y: 0, r: 'miss' };
    if (m.j !== undefined) base.j = m.j;
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
        this.book({ ...base, ok: hitV.k, x: Math.round(hx * 10) / 10, y: Math.round(hy * 10) / 10, r: glance ? 'glance' : 'worn', fc: 'top', u: a, h }, hitV, 'scorch');
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
      if (v.role === 'tank' && small) this.book({ ...at, r: 'glance' });
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

  /**
   * The round that cost a model the rules took: from the striking block's tank or gun (a shell, Drill.shelled, landing
   * at p) or, on foot, its man nearest who could have fired it (on a tank: its anti-tank man, else a grenade). It is
   * the hit, landing as it is fired; on a tank it leaves the hole it went in by.
   */
  private killRound(by: LandUnit | undefined, vq: Squad, v: Man, p?: [number, number]): void {
    const t = this.b.t, tq = by ? this.squads.get(by.id) : undefined;
    const at: [number, number] = p ?? [v.x, v.y];
    let s: Man | undefined, w: Weapon = p ? (by?.kind === 'arm' ? 'tank' : by?.team === 'mor' ? 'mortar' : 'gun') : 'rifle';
    if (tq) {
      const shells = (x: Man): boolean => x.role === 'tank' || x.role === 'gun' || x.role === 'mortar';
      const can = this.standing(tq).filter((x) => x.role !== 'lorry' && x.role !== 'holder' && (p ? shells(x) : !shells(x)));
      const pref = v.role === 'tank' && !p && by?.team !== 'mg' ? can.filter((x) => x.role === 'at') : can.filter((x) => SMALL_ARMS.has(x.pose) || shells(x) || x.role === 'hmg');
      const from = pref.length ? pref : can;
      let dd = Infinity;
      for (const x of from) { const d = Math.hypot(x.x - at[0], x.y - at[1]); if (d < dd) { dd = d; s = x; } }
    }
    if (!p && s) w = s.role === 'hmg' ? 'mg' : v.role === 'tank' ? (s.role === 'at' ? 'at' : 'grenade') : s.role === 'mg' ? 'mg' : 'rifle';
    const r: Round = { t: Math.round(t * 10) / 10, l: Math.round(t * 10) / 10, w, on: vq.u.id, ok: v.k, x: Math.round(at[0] * 10) / 10, y: Math.round(at[1] * 10) / 10, r: 'kill' };
    if (v.j !== undefined) r.oj = v.j;
    if (by) r.by = by.id;
    if (s) {
      r.k = s.k;
      if (s.j !== undefined) r.j = s.j;
      // Its shot is seen: a tank's, a gun's, a launcher's or a grenade's stamp (small arms are in the firing already).
      if (w !== 'rifle' && w !== 'mg') { s.shot = t; s.fired = t; s.aim = [v.x, v.y]; if (s.role === 'gun' || s.role === 'mortar') s.pose = 'fire'; }
    }
    if (VEHICLE.has(v.role)) {
      const fc: Face = w === 'gun' || w === 'mortar' ? 'top' : s ? this.faceOf(v, s.x, s.y) : 'front', [a, h] = this.spotOn(v, vq.u.id, v.n, Math.round(t * 10), 9);
      this.book({ ...r, fc, u: a, h }, v, 'hole');
    } else this.book(r);
  }

  view(as?: Side): MenBlock[] {
    const b = this.b, out: MenBlock[] = [];
    const r1 = (v: number): number => Math.round(v * 10) / 10, r2 = (v: number): number => Math.round(v * 100) / 100;
    const one = (m: Man): MenOut => {
      // A rifle section's key man is its leader (the 3D game gives him his army's leader's weapon).
      const o: MenOut = { k: m.k, r: (this.sections && m.j === undefined && m.role === 'rifle') || m.lead ? 'leader' : m.role, x: r1(m.x), y: r1(m.y), f: r2(wrap(m.face)), p: m.pose, v: MOVING.has(m.pose) || CARRIED.has(m.pose) ? r1(m.went ?? 0) : 0, c: m.cover };
      if (m.j !== undefined) o.j = m.j;
      if (m.aim) o.a = [r1(m.aim[0]), r1(m.aim[1])];
      if (m.shot !== undefined) o.s = r1(m.shot);
      if (m.fell !== undefined) o.d = r1(m.fell);
      if (m.hidden) o.h = 1;
      if (m.buttoned) o.b = 1;
      if (m.marks?.length) o.mk = m.marks.map((x) => [...x] as Mark);
      return o;
    };
    for (const sq of this.squads.values()) {
      const u = sq.u;
      if (as !== undefined && u.side !== as && !b.known(u)) continue;
      if (u.state === 'withdrawn' && !sq.men.some((m) => m.fell !== undefined)) continue;
      const men = sq.men.filter((m) => m.fell !== undefined || (this.stands(sq, m) && live(u))).map(one);
      for (const v of sq.veh) if (live(u)) men.push(one(v));
      const blk: MenBlock = { id: u.id, ph: sq.phase, men };
      if (sq.fo && live(u)) { blk.fo = sq.fo.map(one); blk.foWith = sq.foWith; blk.foOn = sq.foOn; }
      out.push(blk);
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
