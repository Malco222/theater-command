// The land battle: a field, two armies of blocks, a clock.
//
// Each tick: the computer generals think (once a second), every block moves or holds, every block in reach of an
// enemy wears it down, nerve follows losses, runners run, and the battle ends when an army breaks, leaves, or the
// clock runs out. Deterministic: the same ticket and the same orders at the same ticks replay the same battle.

import { Rng, hashString } from '../../core/rng';
import { ONE_SHOTS } from '../core/cues';
import { ahead, aspectOf, bearing, clamp, dist, turn } from '../core/geometry';
import { clampToDice, diceNow, fieldRatio, levelOf, RATIO } from '../core/result';
import { lineUp } from '../core/select';
import { DT, type AirFlown, type AirOnCall, type BattleSim, type BattleView, type BreakCause, type Command, type FieldEvent, type FieldResult, type FieldTicket, type Flight, type ForceTicket, type Side, type Zone } from '../core/types';
import { thinkFor, type AttackPlan } from './ai';
import { STYLE, styleFor, type Style } from './styles';
import { BEND, buildField, coverAt, diggable, FIELD, groundAt, GROUNDS, heightAt, slopeAt, streetsOf, builtShare, zonesOf, type Ground, type LandField, type LandPlace } from './field';
import { crewFor } from './crews';
import { calFor, CRATER, REACH, SHELLS_FROM } from './shells';
import { LAND, MOTION, bearerShare, gunsOf, killedShare, reachOf, smokeMeans, type Kind, type Motion, type SmokeMeans } from './rules';
import { ARMOUR, armourWeight, faceOfAspect, vehicleOf, type ArmourFace } from './armour';
import { turnToDate } from '../../core/ticket';
import { patrolSize } from './rules';
import { armyNames, bendAt, deploy, joinBlocks, makeBlocks, wetUnder, type LandOrder, type LandUnit } from './units';
import { Drill, wreckOffset } from './drill';
import { phoneFallback, phoneLines, type PhoneFallback } from './rules';

const other = (s: Side): Side => (s === 'att' ? 'def' : 'att');

/** A hedge line on the field as the 3D game lays it (release 2): its kind and its centre line, metres from the south-west corner. */
export interface Hedge { kind: string; points: [number, number][] }
/** A gate or gap in a hedge: where it is, which way the hedge runs (radians from east), and how wide. */
export interface Gate { x: number; y: number; yaw?: number; width?: number; kind?: string }
/**
 * A gap cut through a hedge (hedge cutters, release 2; ruling 242): begun at t0 by a tank block of 'side' (its front at
 * the hedge), open as a gate from t1; where, the way the hedge runs there (radians from east) and how wide. 'known':
 * the player knew the block as the cut began (known(): an enemy's cut shows in his view only then). 'open': its gate is
 * on the field.
 */
export interface Cut { t0: number; t1: number; unit: number; side: Side; x: number; y: number; yaw: number; width: number; known: boolean; open: boolean }
/**
 * The night's patrol (night patrol, release 2; LAND.patrol), as settled when the clock started: sent by block 'unit' of
 * 'side' with 'men' men, out from t0 to t1 (battle seconds, the night before), from its block's front ('from') to 'to'
 * (the post it found, else the enemy block nearest it: caught, it was caught there). 'found': the hidden post it found
 * (its block, where it stood, its kind or team), null where none; 'alerted': it was caught; 'post': the defence's block
 * that took one more hidden post then, null where none could; 'lost': the strength its block lost with its men;
 * 'flares': [t, x, y] each, the flares put up over it (looks only).
 */
export interface NightPatrol {
  side: Side; unit: number; men: number; t0: number; t1: number; from: [number, number]; to: [number, number];
  route: [number, number, number][]; found: { unit: number; x: number; y: number; kind: string } | null; alerted: boolean; post: number | null; lost: number; flares: [number, number, number][];
}
/**
 * A smoke screen (smoke on call, release 2; ruling 242): laid by block 'unit' of 'side' at battle second t along x0, y0
 * to x1, y1, standing while t < until. Numbered from 1 in the order laid.
 */
export interface Screen { id: number; side: Side; unit: number; x0: number; y0: number; x1: number; y1: number; t: number; until: number }
/**
 * A field telephone wire (field telephone lines, release 2; LAND.phones): wire 'id', laid as the clock starts from the
 * defence's headquarters 'hq' to block 'unit' of 'side', following it as it moves; 'lines' lines in it (Germany's two),
 * 'cut' of them cut now (down once cut >= lines); 'out', the lineman's mend while one is out on it.
 */
export interface Wire { id: number; side: Side; unit: number; hq: [number, number]; lines: number; cut: number; out: WireMend | null }
/** A line of wire 'wire' cut at battle second t, at x, y on it, by block 'by' (a gun block, a mortar team or a tank). */
export interface WireCut { t: number; wire: number; x: number; y: number; by: number }
/** Lineman 'man' sent out on wire 'wire': he leaves the headquarters at t0, reaches the cut at x, y at t1, and it is mended at t. */
export interface WireMend { t: number; wire: number; man: number; t0: number; t1: number; x: number; y: number }
/** An order for block 'unit' that waited for its down wire: given at t, due at 'due'; 'went', when it reached the block. */
export interface WireHeld { t: number; unit: number; due: number; went?: number }
/**
 * A block's stretcher bearers and its wounded (stretcher bearers, release 2; LAND.bearers), in strength as its str counts
 * it: its bearers in all ('all') and out carrying now ('out', the bearers of every trip still out: 'trips', each with the
 * battle second its bearers are back and how many); the litter cases lying where they fell ('waiting'); the light cases
 * at their dressing ('light', each lot with the battle second it is back in the line and how many); its aid post
 * ('aid'); whether its side has been told its bearers are out ('told'). Made at its first loss.
 */
export interface Litter { all: number; out: number; trips: { back: number; by: number }[]; waiting: number; light: { at: number; n: number }[]; aid: [number, number]; told: boolean }
/** The squares the hedge lines are mapped by, for hiding (metres). */
const HEDGE_CELL = 10;
/** Ground with nothing to hide men on the move: caught there by an ambush, they are shaken worst (LAND.hide.surprise.open). */
const OPEN_GROUND: Ground[] = ['open', 'field', 'road', 'bridge', 'marsh'];
/** The squares the hedge segments are filed by (metres), how far from where a man's way meets a hedge a gate still serves him, and how near a gate's own run ends may be to it. */
const GATE_CELL = 25, GATE_REACH = 120;
/**
 * The bocage (hedge cutters' C1-B, LandBattle.bocage): a tank block without cutters finds its way over the field's own
 * cells (FIELD.cell, 25 m), so a hedge line bars the cells whose middles lie within HEDGE_BAR cells of it (a band about
 * two cells deep, which no way walked straight or cell by cell can slip through), except those within HEDGE_GAP cells
 * of a gap in it (a gateway, or a gap cut and open). At the field's own grain: such a block's middle crosses a hedge
 * within a cell or so of a gap. Neither is a number of the fight, only how finely the hedges are laid on the cells.
 */
const HEDGE_BAR = 0.75, HEDGE_GAP = 1.25;
/** Where along a-b (0 to 1) it crosses c-d, or null. */
function segHit(ax: number, ay: number, bx: number, by: number, cx: number, cy: number, dx: number, dy: number): number | null {
  const rx = bx - ax, ry = by - ay, sx = dx - cx, sy = dy - cy, den = rx * sy - ry * sx;
  if (Math.abs(den) < 1e-12) return null;
  const t = ((cx - ax) * sy - (cy - ay) * sx) / den, u = ((cx - ax) * ry - (cy - ay) * rx) / den;
  return t >= 0 && t <= 1 && u >= 0 && u <= 1 ? t : null;
}
/** How far a point lies from a line segment. */
function segGap(px: number, py: number, ax: number, ay: number, bx: number, by: number): number {
  const dx = bx - ax, dy = by - ay, l = dx * dx + dy * dy;
  const k = l > 0 ? Math.max(0, Math.min(1, ((px - ax) * dx + (py - ay) * dy) / l)) : 0;
  return Math.hypot(px - (ax + k * dx), py - (ay + k * dy));
}

/**
 * The tank blocks the card counted as covered by their own foot (item 7, never twice: b418): of each side's tank
 * divisions (divs x str, as the card weighs them), as many as its foot divisions times LAND.alone.cover cover (one each
 * on forest and urban cards, where ground.ts reckons it; support.json's figure elsewhere; a defence one each, its works'
 * men counted as foot as the card counts them), the first blocks first, a regiment a third. None where a side has no foot.
 */
export function coveredTanks(t: FieldTicket<LandPlace>, units: LandUnit[]): Set<number> {
  const A = LAND.alone, out = new Set<number>();
  for (const s of ['att', 'def'] as Side[]) {
    const fs = t[s].forces, w = (k: (kind: string) => boolean): number => fs.filter((f) => k(f.kind)).reduce((a, f) => a + f.divs * f.str, 0);
    const foot = w((k) => A.foot.includes(k) || (s === 'def' && k === 'fort')), tanks = w((k) => k === 'arm');
    if (foot <= 0 || tanks <= 0) continue;
    const per = s === 'def' ? A.cover.def : A.cover.cards.includes(t.place.terrain) ? A.cover.close : A.cover.open;
    const share = Math.min(1, (foot * per) / tanks);
    const blocks = units.filter((u) => u.side === s && u.kind === 'arm').sort((a, c) => a.id - c.id);
    const all = blocks.reduce((a, u) => a + (u.worth ?? 1), 0);
    let got = 0;
    for (const u of blocks) { if (got + (u.worth ?? 1) > share * all + 1e-9) break; got += u.worth ?? 1; out.add(u.id); }
  }
  return out;
}
/**
 * A block's way round ground it cannot enter: how it moves, the cell it was making for, the turning points, and whether
 * the way gets there or only as near as the block can go.
 */
interface Way { m: Going; goal: number; pts: [number, number][]; whole: boolean; bars?: number }
/**
 * How a block finds its way: as its kind moves; or, foot of a corps the card brings on over dry ground, as foot that
 * will not wade (b464(5)): sent at an enemy beyond where the river bends, it goes round the bend by dry ground, as the
 * card counts it, never through it. Only the way it chooses: a block already in the water still wades out. 'hedged': a
 * tank block without cutters on a field with hedges, the bocage switch on (hedge cutters' C1-B): as tracks go, over no
 * hedge line but at a gap.
 */
type Going = Motion | 'ashore' | 'hedged';
/**
 * A strike called in (air support): who called it, which of the side's groups flies it, when, and its legs, one flight
 * each. A strike the player aims is one leg down his run, carrying all the side's bank spread along it; the staff's
 * fly a leg down the frontage of each block the bank owes most, up to a plane each, each paying what is owed there.
 */
interface Strike { side: Side; by: 'you' | 'staff'; group: number; nth: number; called: number; legs: Leg[]; bombs: boolean; ended: boolean; lost: boolean }
/**
 * A flight's leg: its run; what it carries, in open-ground terms (owed: by block, the staff's; or all of it as 'total'
 * spread down the run, the player's); how much of the run is flown; and the blocks it struck, for the event.
 */
interface Leg { flight: Flight; owed: Map<number, number>; nerve: Map<number, number>; total: number; aimed: boolean; done: number; struck: Set<number>; dropped: number; released: number; away: boolean }

/** River ground: a block may wade it, never stand in it (b464(5)). */
const inRiver = (g: Ground): boolean => g === 'water' || g === 'blown';
const live = (u: LandUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
const fighting = (u: LandUnit): boolean => live(u) && u.state !== 'routing';
/** A block's weight as its army's bar and break count it: the card's part for a corps come up from next door, else its power0. */
const weightOf = (u: LandUnit): number => u.priced ?? u.power0;

/** A field edge: the attack's own is the south, the defence's the north. */
export type Edge = 'south' | 'north' | 'west' | 'east';
/** Inward from each edge (x, y) and the facing that looks into the field. */
const INWARD: Record<Edge, [number, number, number]> = { south: [0, 1, 0], north: [0, -1, Math.PI], west: [1, 0, Math.PI / 2], east: [-1, 0, 1.5 * Math.PI] };

/**
 * The edge a corps from next door comes on by (neighbours join the fight, release 2): the one facing the province it
 * comes from ('toward', the field's frame), never the enemy's own back edge, which folds to the nearer flank (west
 * where level, as ticket.ts dryEnd). Its own back edge where 'toward' is not given. On a field whose river bends away
 * from the attack's dry corps (b464(5)), a corps of the attack keeps to its own side of the river ('own': the dry
 * flank for one that comes on over dry ground, the wet for one that crosses): never the other flank, but its own edge
 * in its own lanes.
 */
export function joinEdge(side: Side, toward: number | undefined, own?: 'west' | 'east'): Edge {
  if (toward === undefined || !Number.isFinite(toward)) return side === 'att' ? 'south' : 'north';
  const sn = Math.sin(toward), cs = Math.cos(toward);
  let e: Edge = Math.abs(cs) >= Math.abs(sn) ? (cs > 0 ? 'north' : 'south') : sn > 0 ? 'east' : 'west';
  if (e === (side === 'att' ? 'north' : 'south')) e = sn > 1e-9 ? 'east' : 'west';
  if (own && (e === 'west' || e === 'east') && e !== own) e = 'south';
  return e;
}
/** A number from 0 to under 1 fixed by a whole number: where a wreck lies, never drawn from the battle's dice. */
const mix = (n: number): number => { let h = n >>> 0; h ^= h >>> 16; h = Math.imul(h, 0x85ebca6b); h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35); h ^= h >>> 16; return (h >>> 0) / 4294967296; };
/** A die keyed by place (its seed, the block, the tick, the salt), from no stream: the sniper teams' own (salts 341-359). */
const roll = (...ns: number[]): number => { let h = 0x9e3779b9; for (const n of ns) h = Math.imul(h ^ (Math.floor(n) >>> 0), 0x01000193) + 0x7f4a7c15; return mix(h); };
/** How a block moves and is seen: its kind's, but a sniper team's as men on foot (release 2, LAND.snipers; never as towed guns). */
const motionOf = (u: LandUnit): Motion => (u.team === 'snp' ? 'foot' : MOTION[u.kind]);

/**
 * Why an army breaks, in the UI director's words (/mnt/project-files/ui-director/field-battles/land-slice-3-review.md,
 * "Why an army breaks", 14:44Z): the dispatch at the break, and in the past (past) the after card's line. 'who' is
 * "Your army" or "The Greek army". The losses form gives the share worn along the whole line, and says the army was
 * still steady where every part or most of it was (no clause where less was); the running form gives no figure.
 */
export function breakLine(why: BreakCause, who: string, past = false): string {
  if (why.by === 'running') return past ? `${who} broke: half of it was running or gone, and the rest would not stand alone.` : `${who} breaks: half of it is running or gone, and the rest will not stand alone.`;
  const pays = why.side === 'att' ? 'as much as an attack will pay before it is called off' : 'as much as a defence will pay before it gives up the ground';
  const still = why.stillSteady === 'all' ? ', though every part of it was still steady' : why.stillSteady === 'most' ? ', though most of it was still steady' : '';
  return `${who} ${past ? 'broke when it had lost' : 'breaks: it has lost'} about ${Math.round(why.worn * 100)}% of its men along the whole line, ${pays}${still}.`;
}

/**
 * The player's own warning at three quarters of his bar, by what drives it (the UI director, 14:44Z). Worn along the
 * whole line, pulling back the worn does not help, so it is not offered.
 */
export function warnLine(why: BreakCause): string {
  return why.nearer === 'running' ? 'Your army is near breaking: nearly half of it is running or gone. Pull back the worn, or hand it to the staff.'
    : `Your army is near breaking: its losses along the whole line are near what ${why.side === 'att' ? 'an attack' : 'a defence'} will pay. Withdraw, or hand it to the staff.`;
}

export class LandBattle implements BattleSim<LandUnit> {
  readonly domain = 'land' as const;
  readonly ticket: FieldTicket<LandPlace>;
  readonly field: LandField;
  t = 0;
  units: LandUnit[];
  over: FieldResult | null = null;
  ai: Record<Side, boolean>;
  /**
   * Each side's computer general's style (styles.ts; the tactics panel, item 1), read once from the ticket: null where
   * the player commands the side, which, handed to the computer, fights as the plain general always has.
   */
  readonly style: Record<Side, Style | null>;
  /** The attack's plan as its computer general set it at its first thought (ai.ts), for the fight and the 3D game. */
  plan: Partial<Record<Side, AttackPlan>> = {};
  private rng: Rng;
  private events: FieldEvent[] = [];
  /** Everything said this battle, for the after-action card. */
  readonly log: FieldEvent[] = [];
  private power: Record<Side, number>;
  /**
   * Whether a side holds at the brink by its fresh blocks: an army of LAND.fresh.upTo blocks or fewer, its works not
   * counted (b480, b484), unless it is the defence behind fortress works or in a fort, which the card prices already
   * (b488).
   */
  private freshHold: Record<Side, boolean>;
  private nextThink = 0;
  private broke: Record<Side, boolean> = { att: false, def: false };
  /** When the first blow fell between the lines (guns firing from far back do not count): null until then. */
  contactAt: number | null = null;
  /** The tanks knocked out on the field, where each stands, to the end of the fight (b477): what the view sends as zones. */
  wrecks: Zone[] = [];
  /**
   * The craters the shells have left on the ground, to the end of the fight (shells.ts): sent as zones beside the wrecks.
   * Looks only: nothing in the fight reads them. Numbered -1, -2 and on, so no crater has a wreck's id.
   */
  craters: Zone[] = [];
  /** The player has been warned that their army is near breaking (ruling 226(3)). */
  private warned: Record<Side, boolean> = { att: false, def: false };
  /** Each army's bar, how near breaking it stands, as last judged: what a withdrawal after contact pays by (b474). */
  private bar: Record<Side, number> = { att: 0, def: 0 };
  /**
   * All fall back (release 1.5; Brian, 23 November 14:47Z): the battle second each army was ordered back, null while it
   * fights on. Its blocks withdraw toward its own edge and are out once behind the zone it drew up in; when the last is
   * out, the battle ends as its withdrawal. It takes no new orders after it.
   */
  private falling: Record<Side, number | null> = { att: null, def: null };
  /** The highest each falling army's bar stood from the order until its last block was out: what it pays by (DD 14:54Z, b474). */
  private fallBar: Record<Side, number> = { att: 0, def: 0 };
  /** Seconds of battle time the fresh hold has kept each army at the brink, as judged: for the words on why it broke (b496(2)). */
  private heldFor: Record<Side, number> = { att: 0, def: 0 };
  /**
   * The attack card did not know the defence (b453): an enemy block shows the player only where it stands until one of
   * his has fought it (b464(2); known). Set by whoever opens the field. Words and pictures only: the fight never reads it.
   */
  fog = false;
  /** Under fog, the corps the attack card identified: their blocks show as the card shows them (b476(3); cardKnows). */
  seen: Set<number> = new Set();
  /**
   * The men inside the blocks, the squad brain (drill.ts): null until something that draws them asks (drillOn: the 3D
   * bridge, the browser page). Looks only: the fight never reads it, and it never draws on the battle's dice.
   */
  drill: Drill | null = null;
  /**
   * The side placing its guns and support teams before the clock starts (release 2; Brian, 21 Nov 03:49Z: "units you
   * can place at the begining of a battle on your side"): null unless something asks (setupOn), as the drill is.
   * While it is set the battle does not step; land:place moves one of that side's gun blocks, land:begin ends it.
   */
  setup: Side | null = null;
  /** Where the placing side may put a block: its zone, grown to take in every block of its own where it was drawn up. */
  private room: [number, number, number, number] | null = null;
  /** The setup step places every block of the side, not only its guns and teams (release 2, the general's controls). */
  private placeAll = false;
  /**
   * Direct command (release 2; Brian, 22 November: "I want to be able to click a unit and point where I want them to
   * go", and 03:09Z "do 1 2 and 3"). perUnit: a block the player orders is his (LandUnit.taken), and his staff thinks
   * for the rest of his side (while ai[side] is on), never for his. radio: his orders reach a block by radio, after
   * LAND.radio's delay. Both off unless whoever opens the field turns them on; off, nothing here is read.
   */
  perUnit = false;
  radio = false;
  /** True while a staff (ai.ts) is giving its orders: those are the computer's, never taken in hand nor sent by radio. */
  private staffing = false;
  /**
   * Hidden in the woods (release 2; Brian, 22 November 04:18Z: ambushes; LAND.hide): off unless whoever opens the field
   * turns it on (hideOn). Off, no block is ever hidden and nothing here is read.
   */
  hide = false;
  /** The clock has started with hide on: who is hidden is settled, and the computer's ambushes laid. */
  private hidStarted = false;
  /**
   * Night patrol (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it off if
   * someone doesnt want it"; the design director, 18:36Z, Q6 to Q8; LAND.patrol; "Night patrol" below): the night
   * before, the attack sends a patrol that may find the defence's most forward hidden post, or be caught and alert the
   * defence. Off unless whoever opens the field turns it on (the 2D page's ?patrol, the bridge's { patrol: true }), and
   * it needs hiding on: with hiding off no patrol goes out and nothing is drawn. Off, nothing below is read.
   */
  patrol = false;
  /** The night's patrol, as it was settled when the clock started (night patrol): null until then, and where none went out. */
  nightPatrol: NightPatrol | null = null;
  /** The block the player sends out on patrol, by his order while he places his army (night patrol): null where none. */
  private patrolBy: number | null = null;
  /** The field's hedges and their gates, as the 3D game handed them over (hedgesOn): empty where it has none. */
  hedges: Hedge[] = [];
  gates: Gate[] = [];
  /** Which 10 m squares lie on a hedge line (LAND.hide.hedge.near), for hiding: null with no hedges. */
  private hedgeMap: Uint8Array | null = null;
  /** The hedge lines' segments, [ax, ay, bx, by], by the GATE_CELL square they pass through: for the men's way through the gates. */
  private hedgeSegs = new Map<number, number[][]>();
  /** The sides that have been ambushed: their attack fires on the cover ahead from then (LAND.hide.search). */
  readonly ambushed = new Set<Side>();
  /**
   * Measured armour (release 2; Brian, 22 November 04:03Z to 04:16Z; armour.ts, LAND.armour): a tank's or a gun's blow on
   * a tank weighs what its gun gets through the face it strikes at that range. Off unless whoever opens the field turns
   * it on; off, nothing here is read.
   */
  measured = false;
  /**
   * Hold until relieved (release 2; Brian, 22 November 12:48Z; "Hold until relieved" below): a block on foot may be told
   * to dig in and wait for a corps coming up from next door. Off unless whoever opens the field turns it on (the 2D
   * page's ?relief, the bridge's { relief: true }); off, nothing below is read.
   */
  relief = false;
  /** The computer's tank blocks going round to a tank block's flank (ai.ts): where to, at whom, since when. */
  readonly flanking = new Map<number, { target: number; at: [number, number]; since: number }>();
  /** Each block's tank or gun model, as the rules weigh it (armour.ts vehicleOf): null for the rest. */
  private models = new Map<number, string | null>();
  /**
   * The player's orders on the air (radio): each block's, with the battle time it lands; 'smoke' (smoke on call, release
   * 2) a smoke order, which rides beside the block's other order on the air, neither replacing it nor replaced by it. An
   * order waiting for a down wire (field telephone lines) also carries the order it is (key) and its stamp (held).
   */
  private pending: { at: number; id: number; go: () => void; smoke?: true; key?: string; held?: WireHeld }[] = [];
  /**
   * Neighbours join the fight (release 2): the blocks of corps coming up from next door, not yet on the field, earliest
   * first: each with its corps' ticket, the battle second it comes on, the edge and the place there. Empty where none
   * comes, and then nothing below reads it.
   */
  private coming: { u: LandUnit; f: ForceTicket; at: number; edge: Edge; x: number; y: number; lanes: [number, number] | null }[] = [];
  /** The battle second each joining corps came on. */
  private joinedAt = new Map<number, number>();
  /** The corps that come up from next door. */
  private joinCorps = new Set<number>();
  /**
   * The rider ahead (release 2; Brian, 22 November 12:48Z: "a courier arrives first; clicking him shows the relief's
   * strength and arrival time"; the design director, 13:43Z): off unless whoever opens the field turns it on (the 2D
   * page's ?rider, the bridge's { rider: true }); off, nothing reads him. Looks and words only: the fight never does.
   */
  rider = false;
  /** Each joining corps' rider, worked out with its time (comeOn): where he sets out and stands, and when. */
  private riders: { corps: number; side: Side; f: ForceTicket; from: [number, number]; mid: [number, number]; to: [number, number]; there: number; comes: number; edge: Edge }[] = [];
  /** The joining corps a block has waited for this battle (hold until relieved): each relieves one block a battle. */
  private waitedFor = new Set<number>();
  /**
   * Hedge cutters (release 2; Brian, 22 November 14:19Z; ruling 242; LAND.cutters; "Hedge cutters" below): a tank block
   * carries cutters, may be told to cut a gap through a hedge where the player picks, and the staffs' cut where their
   * way crosses one. Off unless whoever opens the field turns it on (the 2D page's ?cutters, the bridge's { cutters:
   * true }); off, nothing below is read.
   */
  cutters = false;
  /** Every cut begun this battle (hedge cutters), in the order begun. Empty while the switch is off. */
  readonly cuts: Cut[] = [];
  /**
   * Smoke on call (release 2; Brian, 22 November 14:19Z; ruling 242; LAND.smoke; "Smoke on call" below): a block on foot
   * whose army has the means may lay a smoke screen before it, and both staffs lay one as they go in. Off unless whoever
   * opens the field turns it on (the 2D page's ?smoke, the bridge's { smoke: true }); off, nothing below is read.
   */
  smoke = false;
  /**
   * The bocage (release 2; ruling 242 C1-B, as amended 17:45Z: "unfitted tanks cross only at gaps"; "The bocage" below):
   * on a field with hedges, a tank block without hedge cutters crosses a hedge line only at a gap in it, a gateway or a
   * gap cut and open, and finds its way round to one. Its own switch, so cutters alone stays the fight it was (C1-B:
   * "under its own switch, measured against the card"). Off unless whoever opens the field turns it on (the 2D page's
   * ?bocage, the bridge's { bocage: true }); off, nothing below is read.
   */
  bocage = false;
  /** The bocage: the field's cells a hedge line bars to a tank block without cutters (barrier()); null until asked, and again whenever a gap opens. */
  private bars: Uint8Array | null = null;
  /** How many times the barred cells have been laid afresh (a gap opened): a way found before is looked for again. */
  private barsLaid = 0;
  /** Every screen laid this battle (smoke on call), in the order laid: standing while t < until. Empty while the switch is off. */
  readonly screens: Screen[] = [];
  /**
   * Field telephone lines (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it off
   * if someone doesnt want it"; the design director's rulings, 18:36Z, PHONES Q1 to Q3; LAND.phones; "Field telephone
   * lines" below): the defence's blocks are on the telephone from its headquarters; guns, mortars and tanks cut the wires,
   * and an order to a block whose wire is down waits its army's runner or wireless, the player's and every staff's alike.
   * Off unless whoever opens the field turns it on (the 2D page's ?phones, the bridge's { phones: true }); off, nothing
   * below is read.
   */
  phones = false;
  /** The defence's wires (field telephone lines), laid as the clock starts, numbered from 1. Empty while the switch is off. */
  readonly wires: Wire[] = [];
  /** Every line cut, every lineman sent, and every order that waited for a down wire, in order. Empty while the switch is off. */
  readonly wireCuts: WireCut[] = [];
  readonly wireMends: WireMend[] = [];
  readonly wireHeld: WireHeld[] = [];
  /** Each block's wire, by its id; and the wires' own dice (never the battle's), from when they are laid. */
  private wireBy = new Map<number, Wire>();
  private phoneRng: Rng | null = null;
  /** Whether any sniper team is on the field (release 2, LAND.snipers): settled at the first tick that asks. */
  private snpOn: boolean | null = null;
  /**
   * Stretcher bearers (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it off if
   * someone doesnt want it"; the design director's rulings Q4 and Q5, 18:36Z; LAND.bearers; "Stretcher bearers" below):
   * a block's wounded lie for its bearers, who carry them back to its aid post, its own blows the weaker by the men away;
   * its light cases are back in the line after their dressing. Off unless whoever opens the field turns it on (the 2D
   * page's ?bearers, the bridge's { bearers: true }); off, nothing below is read.
   */
  bearers = false;
  /**
   * Shell blasts (release 2; Brian, 23 November 05:04Z): the men a block loses to shells fall together, the men near one
   * burst, as one shell takes them (Drill.thin, DRILL.blast). Looks only: the drill reads it, the fight never. Off unless
   * whoever opens the field turns it on (the bridge's { blast: true }, the Battle rules card's Shell blasts row).
   */
  blast = false;
  /**
   * Infantry with tanks (release 2; Brian, 23 November 05:05Z): foot near a friendly tank advance behind her, no man is
   * run over by his own side's vehicles (they step out of the way), and an enemy tank can run a man over (Drill.escort,
   * clearOfVehicles, thin; DRILL.escort). Off unless turned on (the bridge's { escort: true }, the Battle rules card's
   * Infantry with tanks row). With it, tanks overrun foot in the open (LAND.overrun, overruns): harm unchanged.
   */
  escort = false;
  /**
   * Men in lorries (release 1.5, looks only; Brian, 23 November 14:10Z and 15:50Z, "soldiers in the back of a truck and
   * they then hop out"): each man aboard says his lorry and seat (MenOut.sv), and lorried men get down over the tailboard
   * one at a time (Drill.alight, hopOut), the lorries standing meanwhile. Off unless turned on (the bridge's
   * { trucks: true }, the Battle rules card's Men in lorries row): off, all get down at once where they sat.
   */
  trucks = false;
  /**
   * Armoured infantry (release 2; Brian, 23 November 07:01Z): an armoured block draws its division's riflemen riding
   * behind its tanks in their army's carriers, half-tracks or lorries, who get down near the enemy (Drill.addMech;
   * DRILL.mech). Looks only: the drill reads it, the fight never. Off unless turned on (the bridge's { mech: true }, the
   * Battle rules card's Armoured infantry row).
   */
  mech = false;
  /**
   * Self-propelled guns (release 2; Brian, 23 November 07:01Z, yes to "a tank destroyer and a self-propelled gun for each
   * major army"): some of a battery's guns are drawn as its army's self-propelled howitzer, with no tractor and no crew
   * on foot (Drill.spgOf; DRILL.spg). Looks only. Off unless turned on (the bridge's { spg: true }, the Battle rules
   * card's Self-propelled guns row); a campaign battle's only (set pieces stay as the historian drew them).
   */
  spg = false;
  /**
   * Tank destroyers (release 2, the same yes; the design director, 23 November 07:28Z, (a)): one model in an armoured
   * block's mix is drawn as its army's tank destroyer or assault gun, the block's own figures and fight unchanged
   * (Drill.tdOf; DRILL.td). Looks only. Off unless turned on (the bridge's { td: true }, the Battle rules card's Tank
   * destroyers row); a campaign battle's only.
   */
  td = false;
  /**
   * Armoured cars and AA vehicles (release 2; Brian, 23 November 07:01Z, item 3; the design director, 07:47Z): a block's
   * armoured cars out ahead on its flanks till it meets the enemy, then behind it; its AA vehicles behind it (Drill.addEscorts;
   * DRILL.cars, DRILL.aa). Looks only. Off unless turned on (the bridge's { cars: true } and { aa: true }, the Battle rules
   * card's Armoured cars and AA vehicles rows); a campaign battle's only.
   */
  cars = false;
  aa = false;
  /**
   * Jeeps, field cars and motorcycles (release 2; Brian, 23 November 07:01Z, item 3; the historian's rows, 08:18Z): a
   * block's light vehicles out ahead with its screen or parked behind it, a motorcycle there riding despatch
   * (Drill.lightOf; DRILL.light). Looks only. Off unless turned on (the bridge's { light: true }, the Battle rules card's
   * Jeeps and motorcycles row); a campaign battle's only.
   */
  light = false;
  /** Overrun (LAND.overrun): each foot block overrun in its contact now, and the tank block that drove through it. */
  readonly overrun = new Map<number, number>();
  /** Each block's bearers and wounded (Litter), from its first loss. Empty while the switch is off. */
  private readonly litters = new Map<number, Litter>();
  /**
   * The light cases back in the line (stretcher bearers): when, the block and how much strength, a block's returns in one
   * battle second taken together (at the first one's time). Empty while the switch is off.
   */
  readonly returns: { t: number; unit: number; n: number }[] = [];

  // Air support (see "Air support" below). Nothing here moves where neither side has fighters on call.
  /** Each side's fighters on call, where it has any. */
  private airOn: Record<Side, AirOnCall | null> = { att: null, def: null };
  /** Strikes each side has still to call; while it has any, its blows hold back the card's share for them. */
  private airLeft: Record<Side, number> = { att: 0, def: 0 };
  /** Battle time each side's next strike is ready: never before the armies meet. */
  private airNext: Record<Side, number> = { att: Infinity, def: Infinity };
  /** What each side's blows have held back, owed to the enemy block each fell on, as it would be on open ground. */
  private airBank: Record<Side, Map<number, number>> = { att: new Map(), def: new Map() };
  /** With it, the nerve it would have cost (held back times the nerve of the blows it was held from): paid with it. */
  private airNerve: Record<Side, Map<number, number>> = { att: new Map(), def: new Map() };
  /** The strikes called and not yet over. */
  private strikes: Strike[] = [];
  /** Strikes called so far, by side: strike n is flown by the side's group n in turn. */
  private airCalls: Record<Side, number> = { att: 0, def: 0 };
  /** Strikes not yet flown when the battle went to the dice: the staff fly them in the settlement, and the bill counts them. */
  private airSettled: Record<Side, number> = { att: 0, def: 0 };
  private flightIds = 0;
  /**
   * Each side's ledger, in open-ground terms: held back from its blows; landed on enemy blocks; wasted (on empty ground,
   * or held over its own men); and dealt, what the landed share took off them for the ground they stood on. What is
   * neither landed nor wasted is unspent: in the bank or in the air when the day ended.
   */
  readonly airLedger: Record<Side, { withheld: number; landed: number; wasted: number; dealt: number }> = { att: { withheld: 0, landed: 0, wasted: 0, dealt: 0 }, def: { withheld: 0, landed: 0, wasted: 0, dealt: 0 } };

  constructor(ticket: FieldTicket<LandPlace>) {
    this.ticket = ticket;
    this.rng = new Rng(ticket.seed);
    this.style = { att: styleFor(ticket, 'att'), def: styleFor(ticket, 'def') };
    // Each side's blocks share out exactly the power the card counted, so the odds on the field are the card's.
    const A = ticket.reckoning.A, D = ticket.reckoning.A / Math.max(1e-6, ticket.reckoning.odds);
    this.power = { att: A, def: D };
    const att = makeBlocks('att', ticket.att, A, 1, undefined, ticket.turn);
    const def = makeBlocks('def', ticket.def, D, att.length + 1, undefined, ticket.turn);
    this.units = [...att, ...def];
    // Corps from next door (release 2): built with the rest, numbered after both armies, held off the field (comeOn).
    const joining = joinBlocks(ticket, this.power, att.length + def.length + 1);
    this.covered = coveredTanks(ticket, this.units);
    const works = ticket.place.fort >= 1 || def.some((u) => u.kind === 'fort');
    // A support team (release 2) is no block of the line: a side's count is its divisions' and regiments'.
    this.freshHold = { att: att.filter((u) => u.kind !== 'fort' && !u.team).length <= LAND.fresh.upTo, def: !works && def.filter((u) => u.kind !== 'fort' && !u.team).length <= LAND.fresh.upTo };
    // Tank crews name their tanks after their unit's third battle (b477): data only, nothing in the fight reads it.
    for (const u of this.units) { const c = crewFor(u, ticket[u.side].forces.find((f) => f.corps === u.corps)?.battles ?? 0); if (c) u.crew = c; }
    // What each block's guns throw, from the model the 3D game draws it with (shells.ts): data only, as the crews'
    // marks are.
    // A mortar team throws mortar bombs; a machine-gun team throws none (release 2).
    for (const u of this.units) { const c = u.team ? (u.team === 'mor' ? 'mortar' : undefined) : calFor(u.kind, u.nat, ticket.turn); if (c) u.cal = c; }
    for (const u of joining) {
      const c = crewFor(u, ticket[u.side].joiners?.find((f) => f.corps === u.corps)?.battles ?? 0);
      if (c) u.crew = c;
      const k = u.team ? (u.team === 'mor' ? 'mortar' : undefined) : calFor(u.kind, u.nat, ticket.turn);
      if (k) u.cal = k;
    }
    // Where some of the attack's corps cross the river and some come on over dry ground, the river lies only before
    // those that cross: it bends away from the lanes of the others, at their end of the field (the design director,
    // b464(5)). Where all cross, or there is no river, the field is as it always was.
    // A ticket that has none crossing is read as one that has all cross: a river with no corps before it is no card's.
    const dry = ticket.place.dryEnd ?? 'west';
    const bend = ticket.place.river && att.some((u) => u.dry) && att.some((u) => !u.dry) ? { x: bendAt(att, zonesOf(FIELD.w, FIELD.h).att, dry), dry } : undefined;
    if (!bend) for (const u of att) delete u.dry;
    // A joiner over dry ground: the attack's keeps it where there is a river (the river's hold is not counted against
    // its blows; on a bend field it keeps to the dry lanes); a defence's never needs it.
    for (const u of joining) if (u.side === 'def' || !ticket.place.river) delete u.dry;
    // Read once the bend is settled: a river with none of the attack dry is crossed by all of it, armour too (b494).
    const armour = att.filter((u) => MOTION[u.kind] === 'tracks' || MOTION[u.kind] === 'wheels'), all = armour.reduce((a, u) => a + u.power0, 0);
    this.armourWet = all > 0 ? armour.filter((u) => !u.dry).reduce((a, u) => a + u.power0, 0) / all : 0;
    this.field = buildField(ticket.place, ticket.seed, bend);
    // A capital's or a city's day is longer as more of its field is built (the design director, b515(g)): the march in
    // through its streets and the fight in them take time the open field's does not.
    if (streetsOf(ticket.place)) this.ticket = { ...ticket, limit: Math.round(ticket.limit * (1 + LAND.city.clock * builtShare(this.field))) };
    deploy(this.units, 'att', this.field);
    deploy(this.units, 'def', this.field);
    // Corps from next door (release 2): their time is reckoned from where the armies stand as the battle opens.
    if (joining.length) this.comeOn(joining);
    // A city's own river up the field with the whole defence drawn up on one bank of it, or a second river along the
    // defence's flank to meet one across behind it (b596): its line ends on the river.
    const St = this.field.stream, held = this.units.filter((u) => u.side === 'def');
    if (St?.lie === 'up') {
      const xAt = (y: number): number => St.course.reduce((m, p) => (Math.abs(p[1] - y) < Math.abs(m[1] - y) ? p : m))[0];
      const west = held.filter((u) => u.x < xAt(u.y)).length;
      this.flankOnRiver = west === 0 || west === held.length;
      if (this.flankOnRiver) this.flankHold = LAND.hold.stream;
    } else if (St?.lie === 'across' && St.join) {
      // The second comes in from a side edge and turns up to the first: the defence all beyond where it meets it.
      const xs = St.join.course.map((p) => p[0]).filter((x) => x >= 0 && x <= this.field.w), west = St.join.course[0][0] < this.field.w / 2;
      this.flankOnRiver = xs.length > 0 && held.every((u) => (west ? u.x > Math.max(...xs) : u.x < Math.min(...xs)));
      if (this.flankOnRiver) this.flankHold = LAND.hold.join;
    }
    for (const u of this.units) u.cover = groundAt(this.field, u.x, u.y);
    this.ai = { att: !ticket.att.human, def: !ticket.def.human };
    // The day's luck, from the card's own run (ruling 226(4)): the attack's blows land that much harder or softer, and
    // the defence's the other way, so the field spreads its results as the card's chance does. Drawn apart from the
    // blow-by-blow luck, so nothing else in the fight moves with it. A second attack on the same place this month
    // (ticket.attempt) draws afresh.
    // (With the honest win chance on, the card's run is the record's wide one; the field's fortune keeps today's run, so
    // its blows and its bill are what they were, and its result is clamped into the wide run and billed at that share.)
    const [d0, d1] = ticket.reckoning.luck ?? ticket.reckoning.dice;
    const roll = d0 + (d1 - d0) * new Rng(hashString(ticket.attempt ? `fortune:${ticket.seed}:${ticket.attempt}` : `fortune:${ticket.seed}`)).next();
    this.fortune = 1 + LAND.fortune * (roll - 1);
    // Fighters on call (air support): a side with a share, strikes and a group to fly them. None draws on the dice.
    for (const s of ['att', 'def'] as Side[]) {
      const a = ticket[s].air;
      if (a && a.share > 0 && a.strikes > 0 && a.groups.length) { this.airOn[s] = a; this.airLeft[s] = Math.floor(a.strikes); }
    }
  }

  /** The day's luck: the attack's blows are multiplied by it, the defence's divided. */
  readonly fortune: number;

  /**
   * What a defender standing on its own ground takes of a blow: set per ground, and for a river or fortress works,
   * so that the field counts the ground once, as the card's odds already have (ruling 226(4)). The river's only where
   * the block striking crossed it: one that came on over dry ground has no river drawn before it to make up for (b464(5)).
   * And only as far as the attack's tanks and lorries had to cross with it (armourWet): an attack on foot crosses at no
   * cost the field must make up, so a river never makes an attack better than none (b494).
   */
  private holdTake(u: LandUnit, t: LandUnit): number {
    const p = this.ticket.place, H = LAND.hold;
    const streets = streetsOf(p);
    // (A corps come up from next door holds no works: the card prices it with none.)
    return (H.take[p.terrain] ?? 0.4) * (p.river && !u.dry ? 1 + (H.river - 1) * this.armourWet : 1) * (p.fort > 0 && t.joined === undefined ? H.works[Math.min(3, p.fort)] : 1) * (streets ? H.streets[`${p.terrain} ${streets}`] ?? H.streets[streets] : 1) * this.flankHold;
  }

  /** The share of the attack's tanks and lorries, by strength, that must cross the river: 0 where it has none (b494). */
  private readonly armourWet: number;
  /** The defence's line ends on a city's own river up the field (b526), or a second river along its flank (b596), all of it on one bank: a flank not on the card. */
  private readonly flankOnRiver: boolean = false;
  /** What the held defence's take is multiplied by for that flank: LAND.hold.stream up the field, LAND.hold.join beside a second river. */
  private readonly flankHold: number = 1;

  private say(e: Omit<FieldEvent, 't'>): void {
    const ev = { t: Math.round(this.t * 10) / 10, ...e } as FieldEvent;
    if (!ev.cue && ONE_SHOTS[ev.what]) ev.cue = ONE_SHOTS[ev.what];
    // A block the player has not met is never named in a dispatch, nor its state told (b464(2)). Where it falls on one of
    // his, it is named as the hover names it; any other word of it, and its sound with it, is left out: a sound with
    // nothing to read looks like a fault (the UI director, b464). The log keeps the event, blank.
    const u = ev.unit !== undefined ? this.unit(ev.unit) : undefined;
    if (u && !this.known(u)) {
      const t = ev.what === 'contact' && u.engaged !== undefined ? this.unit(u.engaged) : undefined;
      if (t && this.ticket[t.side].human) ev.text = `${this.ticket[u.side].adj} troops are attacking your ${t.name}.`;
      else { this.log.push({ ...ev, text: '', cue: undefined }); return; }
    }
    this.events.push(ev);
    this.log.push(ev);
  }

  unit(id: number): LandUnit | undefined { return this.units.find((u) => u.id === id); }

  /** Every block of the battle: those on the field, then those of corps still coming up from next door (release 2). */
  allBlocks(): LandUnit[] { return this.coming.length ? [...this.units, ...this.coming.map((c) => c.u)] : this.units; }

  // ---- Neighbours join the fight (release 2) ----------------------------------------------------------------------
  //
  // The design director's ruling of 22 November 03:15Z: a corps next door that joins a battle is priced on the
  // campaign card, and the field only draws it coming up. It reaches its side's line at (1 - joined) of the battle's
  // own clock, worked out from its march weeks, never a fixed minute or a draw: it stands off the field until its own
  // march from the edge facing the province it comes from would bring it there then, and walks on from that edge as a
  // b407 reserve already sent, to where its side's line is struck hardest. Until then nothing on the field sees it,
  // strikes it or is sent at it; the bar and the break count it at the card's figure.

  /**
   * Holds each joiner off the field: its time, its edge and each block's place there, side by side along the edge.
   * Nothing here draws on the dice.
   */
  private comeOn(joining: LandUnit[]): void {
    const B = this.field.bend, F = this.field, J = LAND.join;
    for (const corps of [...new Set(joining.map((u) => u.corps))]) {
      const us = joining.filter((u) => u.corps === corps), u0 = us[0], side = u0.side;
      const f = this.ticket[side].joiners!.find((x) => x.corps === corps)!;
      // On a bend field each corps of the attack keeps to its own side of the river (b464(5), standWhy): one that comes
      // on over dry ground by the dry flank or the dry lanes of its own edge, one that crosses by the wet.
      const own = side === 'att' && B ? (u0.dry ? B.dry : B.dry === 'west' ? 'east' : 'west') : undefined;
      const edge = joinEdge(side, f.toward, own);
      const lanes: [number, number] | null = own && B && edge === 'south' ? (own === 'west' ? [10, B.x - BEND.clear] : [B.x + BEND.clear, F.w - 10]) : null;
      const [x, y] = this.edgeAt(side, edge, f.toward, u0.w, lanes);
      const along = edge === 'south' || edge === 'north', [lo, hi] = lanes ?? [10, F.w - 10];
      // It reaches its side's line at (1 - joined) of the battle's own day (a city's longer one too), so it comes on that
      // much earlier by its own march from its edge to the middle of its side as the battle opens: worked out from the
      // ground, never a draw. Come on at the edge at that time instead, a joiner of the defence was still on the march when
      // most fights ended, and the field missed its card by up to a fifth (the field's net is a tenth, b418; measured).
      const own0 = this.units.filter((v) => v.side === side);
      const cx = own0.length ? own0.reduce((a, v) => a + v.x, 0) / own0.length : x, cy = own0.length ? own0.reduce((a, v) => a + v.y, 0) / own0.length : y;
      const walk = Math.hypot(x - cx, y - cy) / Math.max(1, LAND.speed[u0.kind]);
      const at = Math.round(Math.max(0, (1 - u0.joined!) * this.ticket.limit - walk) * 1000) / 1000;
      // The rider ahead (release 2): a corps still off the field as the clock starts sends a rider from this same spot
      // to the middle of its side, dry and clear of every block, at the field's fastest pace (the design director,
      // 13:43Z: an existing number, no pace of his own). Pure reckoning, never a draw: worked out whether he is shown or not.
      if (at > 0) {
        this.riders.push({ corps, side, f, from: [x, y], mid: [cx, cy], to: [x, y], there: 0, comes: at, edge });
        this.riderTo(this.riders[this.riders.length - 1]);
      }
      us.forEach((u, i) => {
        const off = (i - (us.length - 1) / 2) * (u.w + J.gap);
        const px = along ? clamp(x + off, lo + u.w / 2, Math.max(lo + u.w / 2, hi - u.w / 2)) : x, py = along ? y : clamp(y + off, 40, F.h - 40);
        this.coming.push({ u, f, edge, x: px, y: py, lanes, at });
      });
      this.joinCorps.add(corps);
    }
    this.coming.sort((a, c) => a.at - c.at || a.u.id - c.u.id);
  }

  /** Where on its edge a joining corps comes on: across a back edge by the way it comes, at a flank its side's own depth. */
  private edgeAt(side: Side, edge: Edge, toward: number | undefined, w: number, lanes: [number, number] | null): [number, number] {
    const F = this.field, z = F.zones[side], J = LAND.join;
    if (edge === 'west' || edge === 'east') return [edge === 'west' ? w / 2 + 10 : F.w - w / 2 - 10, (z[1] + z[3]) / 2];
    const [lo, hi] = lanes ?? [z[0], z[2]];
    const q = toward !== undefined && Number.isFinite(toward) ? toward : 0;
    return [clamp(F.w / 2 + (F.w / 2) * Math.sin(q), lo + w / 2, Math.max(lo + w / 2, hi - w / 2)), edge === 'south' ? J.edge : F.h - J.edge];
  }

  /** The joiners whose time has come: each corps' blocks walk on together, and its coming is told. */
  private arrive(): void {
    const due: typeof this.coming = [];
    while (this.coming.length && this.t >= this.coming[0].at - 1e-9) due.push(this.coming.shift()!);
    for (const corps of [...new Set(due.map((c) => c.u.corps))]) {
      const g = due.filter((c) => c.u.corps === corps);
      for (const c of g) this.walkOn(c);
      this.joinedAt.set(corps, Math.round(this.t * 10) / 10);
      const u = g[0].u, f = g[0].f, side = u.side;
      this.say({
        what: 'land:joined', unit: u.id, side, x: u.x, y: u.y,
        text: `${this.ticket[side].human ? f.name : `The ${this.ticket[side].adj} ${f.name}`} comes up from next door${f.arrive ? `, ${f.arrive === 'early' ? 'early' : f.arrive === 'midway' ? 'midway through the fight' : 'late in the day'}` : ''}.`,
      });
    }
  }

  /**
   * Sets one joining block on its edge, clear of the river and of every block, and starts it in: a reserve already
   * sent (b407; its guns stand as guns), walking its LAND.join.walk metres onto the field, its ground where it stops.
   */
  private walkOn(c: (typeof this.coming)[number]): void {
    const u = c.u, J = LAND.join, [nx, ny, face] = INWARD[c.edge];
    [u.x, u.y] = this.clearSpot(u, c.x, c.y, nx, ny, c.lanes);
    u.face = face;
    u.cover = groundAt(this.field, u.x, u.y);
    if (u.kind !== 'art') u.reserve = 'sent';
    // After every block already on the field: no block's place in the list, or its id, moves.
    this.units.push(u);
    this.drill?.join(u);
    const o: LandOrder = { kind: 'move', x: u.x + nx * J.walk, y: u.y + ny * J.walk, face: u.side === 'att' ? 0 : Math.PI };
    // Its staff's order, never the player's: it is not taken in hand, and goes by no radio.
    this.send(u, o);
    u.home = [o.x, o.y];
  }

  /**
   * The first spot at or in from (x, y) where a block can stand: on ground it can stand on, out of the water, and clear
   * of every block on the field. Tried in a fixed order (in from the edge a cell at a time, and along it either way), so
   * the same battle always finds the same spot; where none is clear it keeps (x, y) and the march sorts it out.
   */
  private clearSpot(u: LandUnit, x: number, y: number, nx: number, ny: number, lanes: [number, number] | null): [number, number] {
    const F = this.field, J = LAND.join, m = Math.max(u.w, u.d), cell = F.cell;
    const [lo, hi] = lanes ?? [10, F.w - 10];
    // Held inside its lanes (a bend field's own side of the river), never pushed across to the field's middle.
    const fit = (px: number, py: number): [number, number] => [clamp(px, lo + m / 2, Math.max(lo + m / 2, hi - m / 2)), clamp(py, 40, F.h - 40)];
    const clear = (px: number, py: number): boolean => {
      if (this.wet(u, px, py, false) || wetUnder(F, { x: px, y: py, w: m, d: m })) return false;
      for (const o of this.units) if (live(o) && Math.abs(o.x - px) < (o.w + m) / 2 + LAND.placeClear && Math.abs(o.y - py) < (o.d + m) / 2 + LAND.placeClear) return false;
      return true;
    };
    for (let k = 0; k <= J.inward; k++) for (let j = 0; j <= 2 * J.across; j++) {
      const a = j === 0 ? 0 : (j % 2 ? 1 : -1) * Math.ceil(j / 2);
      const [px, py] = fit(x + nx * k * cell + ny * a * cell, y + ny * k * cell + nx * a * cell);
      if (clear(px, py)) return [px, py];
    }
    return fit(x, y);
  }

  /** Where a rider rides to and stands, and when he is there: worked out as the clock is set, and again once the setup step has placed the guns. */
  private riderTo(r: (typeof this.riders)[number]): void {
    r.to = this.riderSpot(r.mid[0], r.mid[1]);
    r.there = Math.round((Math.hypot(r.to[0] - r.from[0], r.to[1] - r.from[1]) / Math.max(...Object.values(LAND.speed))) * 1000) / 1000;
  }

  /** Where a rider stands: the nearest point to (x, y), outward a half cell at a time, out of the water and off a bridge, clear of every block. */
  private riderSpot(x: number, y: number): [number, number] {
    const F = this.field, c = LAND.placeClear;
    const ok = (px: number, py: number): boolean => {
      if (px < 10 || py < 10 || px > F.w - 10 || py > F.h - 10) return false;
      const g = groundAt(F, px, py);
      return !inRiver(g) && g !== 'bridge' && !this.units.some((o) => live(o) && Math.abs(o.x - px) < o.w / 2 + c && Math.abs(o.y - py) < o.d / 2 + c);
    };
    // (Rounded first, then tested: the spot he is shown at is the one that is dry and clear.)
    const r1 = (v: number): number => Math.round(v * 10) / 10;
    if (ok(r1(x), r1(y))) return [r1(x), r1(y)];
    for (let r = F.cell / 2; r <= 1000; r += F.cell / 2) for (let a = 0; a < 16; a++) {
      const px = r1(x + Math.sin((a * Math.PI) / 8) * r), py = r1(y + Math.cos((a * Math.PI) / 8) * r);
      if (ok(px, py)) return [px, py];
    }
    return [r1(x), r1(y)];
  }

  /**
   * The riders the player sees: his own side's only (a courier's news is his own army's), from the first step until his
   * corps comes up; none whose corps would beat him there (he comes on with it), none once the day is over.
   */
  riderView(): Pick<BattleView, 'riders'> {
    if (!this.rider || !this.riders.length || this.over || this.t <= 0) return {};
    const still = new Set(this.coming.map((c) => c.u.corps));
    const on = this.riders.filter((r) => this.ticket[r.side].human && still.has(r.corps) && r.there <= r.comes);
    if (!on.length) return {};
    const r3 = (v: number): number => Math.round(v * 1000) / 1000, r1 = (v: number): number => Math.round(v * 10) / 10;
    const KIND: Record<string, string> = { inf: 'infantry', mtn: 'mountain', cav: 'cavalry', eng: 'engineer', mot: 'motorized', arm: 'armoured', art: 'artillery', fort: 'fortress' };
    const mmss = (t: number): string => `${Math.floor(t / 60)}:${String(Math.floor(t % 60)).padStart(2, '0')}`;
    return {
      riders: on.map((r) => {
        const k = r.there > 0 ? Math.min(1, this.t / r.there) : 1;
        const us = this.coming.filter((c) => c.u.corps === r.corps).map((c) => c.u).sort((a, b) => a.id - b.id);
        const men = us.reduce((a, u) => a + u.men0, 0), str = men > 0 ? us.reduce((a, u) => a + u.str0 * u.men0, 0) / men : r.f.str;
        const divs = us.map((u) => u.short), kind = KIND[r.f.kind] ?? r.f.kind;
        const staff = this.ai[r.side], line = (1 - (us[0]?.joined ?? 1)) * this.ticket.limit;
        const back = r.edge === (r.side === 'att' ? 'south' : 'north'), by = back ? 'from behind your line' : `on your ${r.edge} flank`;
        const list = divs.length > 1 ? `${divs.slice(0, -1).join(', ')} and ${divs[divs.length - 1]}` : divs[0] ?? '';
        const says = `${r.f.name}: ${divs.length} ${kind} division${divs.length === 1 ? '' : 's'} (${list}), ${Math.round(men).toLocaleString('en-GB')} men, strength ${Math.round(str * 100)}. ` +
          `It comes up ${by} at ${mmss(r.comes)}${staff ? ` and reaches your line at about ${mmss(line)}` : ', and waits there for your orders'}.`;
        return {
          corps: r.corps, side: r.side, x: r1(r.from[0] + (r.to[0] - r.from[0]) * k), y: r1(r.from[1] + (r.to[1] - r.from[1]) * k),
          face: r3(bearing(r.from[0], r.from[1], r.to[0], r.to[1])), speed: k < 1 ? Math.max(...Object.values(LAND.speed)) : 0,
          there: r3(r.there), comes: r3(r.comes), edge: r.edge, ...(staff ? { line: r3(line) } : {}),
          name: r.f.name, kind: r.f.kind, divs, men: Math.round(men), str: r3(str), says,
        };
      }),
    };
  }

  /** The joiners still coming, as the view shows them: those of the player's side and those the player's staff knows. */
  private comingView(): Pick<BattleView, 'coming'> {
    if (!this.coming.length || this.over) return {};
    const shown = this.coming.filter((c) => this.known(c.u));
    return shown.length ? { coming: shown.map((c) => ({ id: c.u.id, side: c.u.side, corps: c.u.corps, kind: c.u.kind, at: c.at, edge: c.edge, x: c.x, y: c.y, ...(c.f.arrive ? { arrive: c.f.arrive } : {}) })) } : {};
  }

  // ---- Hold until relieved (release 2) -----------------------------------------------------------------------------
  //
  // Brian, 22 November 12:48Z ("Do 2 and 3"): "a unit ordered to wait for help digs in harder, but breaks faster if help
  // never comes"; the design director's rulings of 13:43Z. A block on foot told to wait for a corps coming up from next
  // door stands where it is, fires on what comes into its reach and never advances; its staff never orders it. While it
  // keeps to its spot it takes the field's light cover (LAND.cover[LAND.wreck.cover], the wrecks' own figure, within
  // LAND.dug.keep, never better than its ground gives), and what that cover spares it is kept: the field moves when harm
  // lands, never how much (b415, b418). It lands with the block if it breaks; else, once the cover ends, it is paid out at
  // the pace it was spared, shared by men with the blocks that relieved it, never as one blow on men whose friends have
  // just come; whatever is left lands at the end of the fight. Help is due when the corps' nearest block could reach it
  // at its own pace, the joiners' own reckoning (comeOn), never a draw; it is relieved once a block of a joining corps
  // stands beside it (LAND.alone.near). Past due unrelieved, or its corps out of the fight, help never came: the men are
  // let down, and break where a block starts to waver (LAND.nerve.waver), not at the rout's mark, until relieved. The
  // player's own order to move, attack, line up or withdraw ends the wait without that mark: men re-ordered are not let
  // down. Each joining corps relieves one block a battle, the player's and every staff's alike (ai.ts relieve). Nothing
  // here draws on the dice.

  /**
   * Whether this battle has corps coming up from next door, on the field yet or not; with a side, whether that side has
   * (a side's own help: the player is never told by it of an enemy's that his staff has not identified).
   */
  hasJoiners(side?: Side): boolean {
    if (side === undefined) return this.joinCorps.size > 0;
    return this.coming.some((c) => c.u.side === side) || this.units.some((u) => u.side === side && u.joined !== undefined);
  }

  /** A block of a joining corps that helps this one: its side's, of another corps, in the fight, not guns or a team. */
  private helps(r: LandUnit, u: LandUnit): boolean {
    return r.side === u.side && r.joined !== undefined && r.corps !== u.corps && fighting(r) && r.kind !== 'art';
  }

  /** Help stands beside this block: a block of a joining corps within LAND.alone.near of it, edge to edge. */
  private relieved(u: LandUnit): boolean {
    return this.units.some((r) => this.helps(r, u) && this.gap(r, u) <= LAND.alone.near);
  }

  /** This corps still has a block that is not guns coming up or in the fight: its help may yet come. */
  private canHelp(corps: number): boolean {
    return this.coming.some((c) => c.u.corps === corps && c.u.kind !== 'art') || this.units.some((r) => r.corps === corps && r.joined !== undefined && fighting(r) && r.kind !== 'art');
  }

  /**
   * When help could reach this block, and whose: of its side's joining corps not yet waited for, the one whose nearest
   * block (not guns) could be there soonest, from where it stands, or from its place on its edge at its time while it is
   * still coming, at its own pace over open ground (comeOn's own reckoning). Null where none can come.
   */
  dueAt(u: LandUnit, skip?: ReadonlySet<number>): { corps: number; due: number } | null {
    let corps = -1, due = Infinity;
    const take = (c: number, d: number): void => { if (d < due - 1e-9 || (Math.abs(d - due) <= 1e-9 && c < corps)) { corps = c; due = d; } };
    for (const c of this.coming) {
      if (c.u.side !== u.side || c.u.corps === u.corps || c.u.kind === 'art' || this.waitedFor.has(c.u.corps) || skip?.has(c.u.corps)) continue;
      take(c.u.corps, Math.max(this.t, c.at) + Math.hypot(c.x - u.x, c.y - u.y) / Math.max(1, LAND.speed[c.u.kind]));
    }
    for (const r of this.units) if (this.helps(r, u) && !this.waitedFor.has(r.corps) && !skip?.has(r.corps)) take(r.corps, this.t + Math.hypot(r.x - u.x, r.y - u.y) / Math.max(1, LAND.speed[r.kind]));
    return corps < 0 ? null : { corps, due: Math.round(due * 1000) / 1000 };
  }

  /**
   * Why this block may not wait for help now, in the player's words; null where it may (words for the UI director).
   * 'skip': corps already spoken for by blocks before it in the same order (waitWhyAll).
   */
  waitWhy(u: LandUnit, skip?: ReadonlySet<number>): string | null {
    if (!this.relief || !this.hasJoiners(u.side)) return 'No help is coming up from next door.';
    if (!fighting(u) || u.kind === 'fort') return 'It is out of the fight.';
    if (MOTION[u.kind] !== 'foot') return 'Only men on foot dig in to wait for help.';
    if (u.waited) return u.waits && !u.waits.free ? 'It is waiting for help already.' : 'It has waited for help once today.';
    if (!diggable(coverAt(this.field, u.x, u.y))) return 'There is no digging in here.';
    if (this.relieved(u)) return 'Help is beside it already.';
    const d = this.dueAt(u, skip);
    if (!d) {
      const any = this.coming.some((c) => c.u.side === u.side && c.u.corps !== u.corps && c.u.kind !== 'art') || this.units.some((r) => this.helps(r, u));
      return any ? 'Each corps coming up relieves one block, and each is spoken for.' : 'No help is coming up from next door.';
    }
    if (d.due > this.ticket.limit) return 'Help could not reach it before the day is out.';
    return null;
  }

  /**
   * Why each of these blocks, told together and in this order, may not wait: each one that may speaks for its corps
   * before the next is asked, as their orders will land (one block a joining corps).
   */
  waitWhyAll(us: LandUnit[]): (string | null)[] {
    const claimed = new Set<number>();
    return us.map((u) => {
      const why = this.waitWhy(u, claimed);
      if (why === null) claimed.add(this.dueAt(u, claimed)!.corps);
      return why;
    });
  }

  /** The joining corps' name as the dispatches give it. */
  private corpsName(side: Side, corps: number): string {
    const f = this.ticket[side].joiners?.find((x) => x.corps === corps);
    return f ? (this.ticket[side].human ? f.name : `the ${this.ticket[side].adj} ${f.name}`) : 'help';
  }

  /** The wait order landing (by radio, the player's): where it may no longer wait, it holds as the core's hold does. */
  private wait(u: LandUnit): void {
    u.order = null; u.path = []; u.keepFace = null; delete u.aim; delete u.search; delete u.searchR; delete u.cut;
    if (this.waitWhy(u) !== null) return;
    const d = this.dueAt(u)!;
    u.waits = { corps: d.corps, due: d.due, at: [u.x, u.y], since: this.t };
    u.waited = true;
    this.waitedFor.add(d.corps);
    if (this.ticket[u.side].human) this.say({ what: 'land:waits', unit: u.id, side: u.side, text: `${this.named(u)} digs in to wait for ${this.corpsName(u.side, d.corps)}.`, x: u.x, y: u.y });
  }

  /**
   * Each tick, before the staffs think: a waiting block with help beside it is relieved; one whose help is overdue, or
   * whose corps can no longer come, has waited in vain.
   */
  private waitOn(): void {
    for (const u of this.units) {
      if (!u.waits || !fighting(u)) continue;
      if (this.relieved(u)) this.endWait(u, 'relieved');
      else if (!u.waits.late && (this.t >= u.waits.due - 1e-9 || !this.canHelp(u.waits.corps))) this.endWait(u, 'late');
    }
  }

  /**
   * A wait ends, or help is late. Late, the block is still waiting: it keeps its spot, its cover and what that cover has
   * spared it, and now carries the mark of help that never came. Relieved, its wait is over and the mark lifts. The
   * player's own order, or giving ground at the brink (b496(1)), ends it, keeping the mark only where help was late
   * already (men let down stay let down until relieved or broken: the design director, 13:43Z). What its cover spared
   * lands at once if it broke, else, once the wait is over, is paid out from then at the pace it was spared (repay),
   * shared with the blocks that relieved it.
   */
  private endWait(u: LandUnit, how: 'relieved' | 'late' | 'off' | 'broke' | 'gives'): void {
    const w = u.waits;
    if (!w) return;
    if (how === 'late') w.late = true;
    else if ((how === 'off' || how === 'gives') && w.late) w.free = true;
    else delete u.waits;
    if (how === 'broke') this.landNow(u);
    else if (how !== 'late' && u.debt !== undefined) {
      const by = how === 'relieved' ? this.units.filter((r) => this.helps(r, u) && this.gap(r, u) <= LAND.alone.near).map((r) => r.id) : [];
      const d = u.debt, n = u.debtN ?? 0;
      delete u.debt; delete u.debtN;
      if (d > 0) this.repay.push({ held: u.id, by, left: d, nerve: n / d, rate: d / Math.max(DT, this.t - w.since) });
    }
    if (!this.ticket[u.side].human) return;
    if (how === 'relieved') this.say({ what: 'land:relieved', unit: u.id, side: u.side, text: `${this.corpsName(u.side, w.corps)} has reached ${this.named(u)}.`, x: u.x, y: u.y });
    if (how === 'late') this.say({ what: 'land:late', unit: u.id, side: u.side, text: `No help has reached ${this.named(u)}; its men are losing heart.`, x: u.x, y: u.y });
  }

  /**
   * What waits' cover spared, being paid out: each held block's, at the pace it was spared, shared by men with the blocks
   * that relieved it (the design director, 13:43Z). Empty with no waits, and then nothing reads it.
   */
  private repay: { held: number; by: number[]; left: number; nerve: number; rate: number }[] = [];

  /** Each tick, after the blows: a tick's worth of each repayment lands. */
  private repayNow(): void {
    for (const p of this.repay) { const d = Math.min(p.left, p.rate * DT); p.left -= d; this.repayOn(p, d); }
    this.repay = this.repay.filter((p) => p.left > 1e-12);
  }

  /**
   * The blocks a repayment lands on, each with its share by the men it has now: the held block and its relievers, on the
   * field or not (one that has left still pays its share, off its strength, as the air's dues are paid: leaving never
   * escapes it, nor hands it to the others).
   */
  private repayShares(p: (typeof this.repay)[number]): [LandUnit, number][] {
    const on = [p.held, ...p.by].map((id) => this.unit(id)).filter((x): x is LandUnit => !!x);
    const men = on.map((x) => x.men0 * (Math.max(0, x.str) / Math.max(1e-9, x.str0))), all = men.reduce((a, m) => a + m, 0);
    return on.map((x, i) => [x, all > 0 ? men[i] / all : 1 / on.length]);
  }

  /** Lands d of a repayment on its blocks: on the field as a blow, off it off their strength (harm). */
  private repayOn(p: (typeof this.repay)[number], d: number): void {
    if (d <= 0) return;
    for (const [x, k] of this.repayShares(p)) this.harm(x, d * k, p.nerve);
  }

  /** What a wait's cover spared a block, kept or still being paid out, lands on it at once (it broke). */
  private landNow(u: LandUnit): void {
    const d = u.debt, n = u.debtN ?? 0;
    delete u.debt; delete u.debtN;
    if (d !== undefined && d > 0) this.harm(u, d, n / d);
    for (const p of this.repay) if (p.held === u.id && p.left > 0) { const left = p.left; p.left = 0; this.harm(u, left, p.nerve); }
    this.repay = this.repay.filter((p) => p.left > 1e-12);
  }

  /**
   * Kept harm landing on a block: where it still stands, as a blow from whoever had struck it most (whose shells its lost
   * men go to: looks only); off the field, off its strength alone.
   */
  private harm(u: LandUnit, d: number, nerve: number): void {
    if (d <= 0) return;
    if (!live(u)) { this.offStrength(u, d); return; }
    const owed = u.owed ?? {};
    let by: LandUnit | undefined;
    for (const id of Object.keys(owed)) { const s = this.unit(Number(id)); if (s && live(s) && (!by || owed[s.id] > owed[by.id])) by = s; }
    this.suffer(u, d, nerve, by ? [[by, d]] : [], false, true);
  }

  /** Harm off a block's strength alone, as the air's dues are paid off the field (offField). */
  private offStrength(u: LandUnit, d: number): void {
    u.str = Math.max(0, u.str - (d / Math.max(1e-9, u.power0)) * u.str0);
    u.figures = u.str <= 0 ? 0 : Math.max(1, Math.ceil(u.tray * u.str));
  }

  /** The day is done: what waits' cover spared, kept or still being paid out, lands before the count. */
  private payWaits(): void {
    if (!this.relief) return;
    for (const p of this.repay) { const left = p.left; p.left = 0; this.repayOn(p, left); }
    this.repay = [];
    for (const u of this.units) if (u.debt !== undefined) { const d = u.debt, n = u.debtN ?? 0; delete u.debt; delete u.debtN; if (d > 0) this.harm(u, d, n / d); }
  }

  /** What blocks on the field are owed, were the day settled now: the fighters on call's, and every wait's (the staff's chance). */
  private dueNow(): Map<number, number> | undefined {
    let due = this.airDue();
    if (!this.relief) return due;
    const add = (id: number, d: number): void => { due ??= new Map(); due.set(id, (due.get(id) ?? 0) + d); };
    for (const u of this.units) if (u.debt && live(u)) add(u.id, u.debt);
    for (const p of this.repay) for (const [x, k] of this.repayShares(p)) if (live(x)) add(x.id, p.left * k);
    return due;
  }

  // ---- Hedge cutters (release 2) ----------------------------------------------------------------------------------
  //
  // Brian, 22 November 14:19Z ("Let's do 1 and 2": "bladed Shermans punch a gap through a hedgerow where the player
  // picks"); the design director's ruling 242, C1-A to C4-A (LAND.cutters). A tank block carries cutters from July 1944
  // on a field with hedges, whatever its army. Told to cut at a point on a hedge line, it drives for that point and on
  // through it till its rear stands LAND.cutters.near metres beyond, an order like any other (by radio, the player's,
  // and gated again as it lands). As its front reaches the hedge the cut begins (t0, a land:cut event), and LAND.cutters.secs later
  // a gap stands open there (t1): a Gate of kind 'cut' the drawn men of every block on either side walk through from
  // then. A block its staff runs cuts wherever its own way crosses a hedge line with no gap near, with no order at all
  // (C4-A); the player's own blocks only where he orders it. Looks only (C1-A): the gates are read by the drill alone
  // (drill.ts via and gateOn: no number of the fight reads this.gates, and gateOn's only caller is the drill), the
  // block's way is its move's own, and nothing here draws on the dice; so the fight is the same, cut or not. With the
  // bocage switch on as well (C1-B, "The bocage" below), the gates are read by the way a tank block without cutters
  // finds (barrier()): there a gap open is a way through for any tank, and the fight changes, measured.

  /** Whether this block carries hedge cutters: the switch on, a tank block, from LAND.cutters.from, on a field with hedges. */
  cutterOf(u: LandUnit): boolean {
    return this.cutters && u.kind === 'arm' && this.ticket.turn >= LAND.cutters.from && this.hedges.length > 0;
  }

  /**
   * Why this block may not cut through the hedge at (x, y) now, in the player's words (129(d)); null where it may. With
   * no point, whether it may cut anywhere. Whose block it is, is the caller's to ask (the bridge's words).
   */
  cutWhy(u: LandUnit, x?: number, y?: number): string | null {
    if (!this.cutters) return 'Your tanks carry no hedge cutters.';
    if (!fighting(u) || u.kind === 'fort') return 'It is out of the fight.';
    if (u.kind !== 'arm') return 'Only tanks carry hedge cutters.';
    if (this.ticket.turn < LAND.cutters.from) return 'Hedge cutters come in July 1944.';
    if (!this.hedges.length) return 'There are no hedges on this field.';
    if (x === undefined && y === undefined) return null;
    const px = Number(x), py = Number(y);
    // (What the move there would refuse: a point off the field.)
    if (!Number.isFinite(px) || !Number.isFinite(py) || px < 0 || py < 0 || px > this.field.w || py > this.field.h) return 'Off the field.';
    if (!this.hedgeNear(px, py, LAND.cutters.near)) return 'There is no hedge there.';
    return null;
  }

  /**
   * The nearest point of a hedge line to (x, y) within r metres, and the way the line runs there (radians from east, as
   * a Gate's yaw); null where none passes that near.
   */
  hedgeNear(x: number, y: number, r: number): { x: number; y: number; yaw: number } | null {
    let best: { x: number; y: number; yaw: number } | null = null, bd = r;
    const seen = new Set<number[]>();
    for (let c = Math.floor((x - r) / GATE_CELL); c <= Math.floor((x + r) / GATE_CELL); c++)
      for (let q = Math.floor((y - r) / GATE_CELL); q <= Math.floor((y + r) / GATE_CELL); q++)
        for (const s of this.hedgeSegs.get(c * 4096 + q) ?? []) {
          if (seen.has(s)) continue;
          seen.add(s);
          const dx = s[2] - s[0], dy = s[3] - s[1], l = dx * dx + dy * dy;
          if (l <= 0) continue;
          const k = Math.max(0, Math.min(1, ((x - s[0]) * dx + (y - s[1]) * dy) / l)), px = s[0] + k * dx, py = s[1] + k * dy, d = Math.hypot(x - px, y - py);
          if (d <= bd) { bd = d; best = { x: px, y: py, yaw: Math.atan2(dy, dx) }; }
        }
    return best;
  }

  /**
   * The cut order landing (by radio, the player's): where it may no longer cut there, it holds, as the core's hold does
   * (as wait does). Else it drives for the nearest point of the hedge line and on through it, the cut its order's.
   */
  private cutGo(u: LandUnit, x: number, y: number): void {
    if (this.cutWhy(u, x, y) !== null) { u.order = null; u.path = []; u.keepFace = null; delete u.aim; delete u.search; delete u.searchR; delete u.cut; return; }
    const p = this.hedgeNear(x, y, LAND.cutters.near)!;
    const k = Math.hypot(p.x - u.x, p.y - u.y), [ux, uy] = k > 1e-6 ? [(p.x - u.x) / k, (p.y - u.y) / k] : ahead(u.face);
    const on = u.d / 2 + LAND.cutters.near;
    this.send(u, { kind: 'move', x: p.x + ux * on, y: p.y + uy * on });
    u.cut = { x: p.x, y: p.y, yaw: p.yaw };
  }

  /** The point lies on this block's front or inside its ground: its front has reached it. */
  private fronted(u: LandUnit, x: number, y: number): boolean {
    const [fx, fy] = ahead(u.face), dx = x - u.x, dy = y - u.y;
    const along = dx * fx + dy * fy, across = Math.abs(dx * fy - dy * fx);
    return along <= u.d / 2 + 0.5 && along >= -u.d / 2 && across <= u.w / 2;
  }

  /** A block its staff runs: the computer's, or one of the player's his staff fights (one he has not taken in hand). */
  private staffRuns(u: LandUnit): boolean { return this.ai[u.side] && !u.taken; }

  /** A gap within LAND.cutters.width of (x, y): a gate, or a cut under way. */
  private gapNear(x: number, y: number): boolean {
    const W = LAND.cutters.width;
    return this.gates.some((g) => Math.hypot(g.x - x, g.y - y) <= W) || this.cuts.some((c) => !c.open && Math.hypot(c.x - x, c.y - y) <= W);
  }

  /**
   * Each tick, once the blocks have moved ('before': where each stood; 'moved': where its own move took it, before the
   * blocks were eased apart): a block whose front has reached the hedge it was sent to cut begins its cut, unless a gap
   * stands there already (another block's cut, or a gateway: it goes through that); one whose way there has ended short
   * of it drops it. A block its staff runs that carries cutters, whose own way this tick (its move, never a shove from a
   * neighbour) crossed a hedge line with no gap near the crossing, cuts there. Geometry only.
   */
  private cutOn(before: Map<number, [number, number]>, moved: Map<number, [number, number]>): void {
    for (const u of this.units) {
      if (u.cut) {
        const c = u.cut;
        if (!fighting(u)) delete u.cut;
        else if (this.fronted(u, c.x, c.y)) { delete u.cut; if (!this.gapNear(c.x, c.y)) this.cutBegin(u, c.x, c.y, c.yaw); }
        else if (u.order?.kind !== 'move') delete u.cut;
        continue;
      }
      if (!live(u) || !this.staffRuns(u) || !this.cutterOf(u)) continue;
      const b0 = before.get(u.id), b1 = moved.get(u.id);
      if (!b0 || !b1 || (b0[0] === b1[0] && b0[1] === b1[1])) continue;
      const p = this.hedgeCross(b0[0], b0[1], b1[0], b1[1]);
      if (!p || this.gapNear(p[0], p[1])) continue;
      const d = this.hedgeDir(p[0], p[1]);
      this.cutBegin(u, p[0], p[1], d ? Math.atan2(d[1], d[0]) : 0);
    }
  }

  /**
   * A cut begins: kept, and told (a land:cut event, with when the gap opens, the hedge's yaw and the gap's width). Its
   * words go to the player for his own blocks; an enemy's is told with no words, for the 3D game alone (as every event
   * of a block he has not met is kept from him).
   */
  private cutBegin(u: LandUnit, x: number, y: number, yaw: number): void {
    const C = LAND.cutters, r1 = (v: number): number => Math.round(v * 10) / 10, r3 = (v: number): number => Math.round(v * 1000) / 1000;
    const c: Cut = { t0: r3(this.t), t1: r3(this.t + C.secs), unit: u.id, side: u.side, x: r1(x), y: r1(y), yaw: r3(yaw), width: C.width, known: this.known(u), open: false };
    this.cuts.push(c);
    const words = this.ticket[u.side].human || !this.ticket[other(u.side)].human;
    this.say({ what: 'land:cut', unit: u.id, side: u.side, text: words ? `${this.named(u)} cuts a gap through the hedge.` : '', x: c.x, y: c.y, extra: { t1: c.t1, yaw: c.yaw, width: c.width } });
  }

  /** Each tick, as the clock moves on: a cut whose time is up stands open as a gate, for the drawn men from now. */
  private openCuts(): void {
    for (const c of this.cuts) {
      if (c.open || this.t < c.t1 - 1e-9) continue;
      c.open = true;
      this.gates.push({ x: c.x, y: c.y, yaw: c.yaw, width: c.width, kind: 'cut' });
      // The bocage (C1-B): once a gap is open, any tank uses it.
      if (this.bocage) { this.bars = null; this.barsLaid++; }
    }
  }

  /** The cuts a side sees: its own, and those of enemy blocks it knew as they cut; every one with no side (a recording). */
  cutView(as?: Side): Pick<BattleView, 'cuts'> {
    if (!this.cutters || !this.cuts.length) return {};
    const l = this.cuts.filter((c) => as === undefined || c.side === as || c.known);
    return l.length ? { cuts: l.map((c) => ({ t0: c.t0, t1: c.t1, unit: c.unit, x: c.x, y: c.y, yaw: c.yaw, width: c.width })) } : {};
  }

  // ---- The bocage (release 2) ---------------------------------------------------------------------------------------
  //
  // The design director's ruling 242 C1-B, built as amended 17:45Z from the historian ("unfitted tanks cross only at
  // gaps"): the device's real point (the readings, C1-B: tanks without cutters "diving into the ditches nose first, to
  // be stuck there and become easy targets"; the historian's K21). On a field with hedges, with the bocage switch on, a
  // tank block that carries no cutters (before July 1944, or with the cutters switch off) crosses a hedge line only at a
  // gap in it: a gateway the 3D game laid, or a gap a cutter tank has cut and that stands open ("Once a gap is open, any
  // vehicle uses it"). It finds its way round to one, as round a river to its bridge (going 'hedged': the hedge lines
  // bar the cells about them, barrier()), and is never eased over one. A tank block carrying cutters goes on as before:
  // its cut is drawn (C1-A), and takes none of its time (the cut's LAND.cutters.secs stays the drawing's: the historian's
  // bound of 3 real minutes or less, a breach and the drive to the next bank, holds it). Lorries, guns and foot are not
  // held: the ruling holds tanks. Only the way changes: nothing here draws on the dice, and a block's way is found the
  // same each time. It changes the fight (slower tanks in the bocage): measured against off (the cutters and smoke
  // tests' (10b), LONG=1).

  /** Whether a straight way from (x0, y0) to (x1, y1) crosses a hedge line where it is barred (barrier()): not at a gap. */
  private barredCross(x0: number, y0: number, x1: number, y1: number): boolean {
    const bar = this.barrier(), len = Math.hypot(x1 - x0, y1 - y0);
    for (let skip = 0, p = this.hedgeCross(x0, y0, x1, y1); p; p = this.hedgeCross(x0, y0, x1, y1, skip)) {
      if (bar[this.cellAt(p[0], p[1])]) return true;
      skip = Math.hypot(p[0] - x0, p[1] - y0) + 0.01;
      if (skip >= len) break;
    }
    return false;
  }

  /** Held back by hedges (the bocage on): a tank block without cutters, on a field with hedges. */
  heldBack(u: LandUnit): boolean {
    return this.bocage && u.kind === 'arm' && this.hedges.length > 0 && !this.cutterOf(u);
  }

  /**
   * The field's cells a hedge line bars to a tank block without cutters: each cell whose middle lies within HEDGE_BAR
   * cells of a hedge line, but for those within HEDGE_GAP cells of a gap in it (this.gates: a gateway, or a cut open).
   * Laid when first asked and again once a gap opens.
   */
  private barrier(): Uint8Array {
    if (this.bars) return this.bars;
    const f = this.field, C = f.cols, R = f.rows, cs = f.cell, bar = new Uint8Array(C * R), r = HEDGE_BAR * cs;
    const mid = (i: number): [number, number] => [(i % C) * cs + cs / 2, Math.floor(i / C) * cs + cs / 2];
    for (const h of this.hedges) for (let k = 1; k < h.points.length; k++) {
      const [ax, ay] = h.points[k - 1], [bx, by] = h.points[k];
      const c0 = Math.max(0, Math.floor((Math.min(ax, bx) - r) / cs)), c1 = Math.min(C - 1, Math.floor((Math.max(ax, bx) + r) / cs));
      const r0 = Math.max(0, Math.floor((Math.min(ay, by) - r) / cs)), r1 = Math.min(R - 1, Math.floor((Math.max(ay, by) + r) / cs));
      for (let q = r0; q <= r1; q++) for (let c = c0; c <= c1; c++) if (segGap(c * cs + cs / 2, q * cs + cs / 2, ax, ay, bx, by) <= r) bar[q * C + c] = 1;
    }
    const gr = HEDGE_GAP * cs;
    for (const g of this.gates) {
      const c0 = Math.max(0, Math.floor((g.x - gr) / cs)), c1 = Math.min(C - 1, Math.floor((g.x + gr) / cs));
      const r0 = Math.max(0, Math.floor((g.y - gr) / cs)), r1 = Math.min(R - 1, Math.floor((g.y + gr) / cs));
      for (let q = r0; q <= r1; q++) for (let c = c0; c <= c1; c++) { const [mx, my] = mid(q * C + c); if (Math.hypot(mx - g.x, my - g.y) <= gr) bar[q * C + c] = 0; }
    }
    return (this.bars = bar);
  }

  // ---- Smoke on call (release 2) ----------------------------------------------------------------------------------
  //
  // Brian, 22 November 14:19Z ("a section lays a smoke line to cover a crossing, when and where the player orders it");
  // the design director's ruling 242, S1-A to S6, as amended 17:45Z (LAND.smoke). A block on foot, or lorried infantry,
  // whose army has the means lays a screen along a line he draws: clipped to its own front about the line's middle,
  // which must lie within its army's reach of its front (an army whose means is set down at its own front lays it there,
  // for a line drawn on its ground or within LAND.smoke.front of its front), at once as the order lands (by radio, the
  // player's, on its own place on the air beside his other order, gated again then); one at a time, its army's count a
  // battle. While it stands (t < until), a direct blow whose line from the shooter's middle to the target's middle
  // crosses it, either side's screen, strikes at LAND.smoke.share, never on top of the vague share nor of fire on cover
  // at a block still hidden (blow); and, with hiding on, no hidden block is seen through it (spotHidden). Fire from far
  // back is not touched: guns beyond LAND.guns.closeAt, mortar teams, and the air (the share a side holds back for its
  // air strikes is held back from the blow whole, as if no screen stood). It gives no cover, does no harm and shakes no
  // one. Both staffs lay one as a foot block of theirs goes in (ai.ts smokeIn). The drill draws each screen as the rounds
  // that lay it, in place of its own salvoes (S6).
  // The dice: where a screen acts it draws nothing and spares no draw. A screened blow draws its luck as it would; the
  // veil (spotHidden) draws nothing. What follows from it changes the fight, and so the draws that come after, as every
  // rule that changes the fight does: a weaker blow breaks a block later, and a block kept hidden by the veil (S1-A as the
  // build spec reads it: nobody seen through a screen) is not fired on until it is found, so the blows it would have
  // drawn are not drawn. That is the screen's effect, measured with the rest (S5-A), not a draw of its own.

  /** This block's section smoke: the switch on, a block on foot or lorried infantry (not a support team) of an army with the means. */
  smokeOf(u: LandUnit): SmokeMeans | null {
    if (!this.smoke || u.team || (MOTION[u.kind] !== 'foot' && u.kind !== 'mot')) return null;
    return smokeMeans(u.nat);
  }

  /** The screen this block laid that still stands, if any. */
  screenOf(u: LandUnit): Screen | undefined { return this.screens.find((s) => s.unit === u.id && this.t < s.until); }

  /** Metres from (x, y) to this block's front edge. */
  private frontGap(u: LandUnit, x: number, y: number): number {
    const [fx, fy] = ahead(u.face), cx = u.x + (fx * u.d) / 2, cy = u.y + (fy * u.d) / 2, hw = u.w / 2;
    return segGap(x, y, cx - fy * hw, cy + fx * hw, cx + fy * hw, cy - fx * hw);
  }

  /**
   * Why this block may not lay smoke now, in the player's words (129(d)); null where it may. With a line (x0, y0 to x1,
   * y1, or only its middle x0, y0), whether it may lay it there; with none, whether it may lay one at all. Whose block it
   * is, is the caller's to ask (the bridge's words).
   */
  smokeWhy(u: LandUnit, x0?: number, y0?: number, x1?: number, y1?: number): string | null {
    if (!this.smoke) return 'There is no smoke to lay in this battle.';
    if (!fighting(u) || u.kind === 'fort') return 'It is out of the fight.';
    if (u.team || (MOTION[u.kind] !== 'foot' && u.kind !== 'mot')) return 'Only troops on foot lay smoke.';
    const m = smokeMeans(u.nat);
    if (!m) return 'Your army has no smoke to lay.';
    if ((u.smokeLeft ?? m.count ?? LAND.smoke.count) <= 0) return 'They have no smoke left.';
    if (this.screenOf(u)) return 'Their last screen still stands.';
    if (x0 === undefined && y0 === undefined) return null;
    const a = Number(x0), b = Number(y0), c = x1 === undefined ? a : Number(x1), d = y1 === undefined ? b : Number(y1);
    if (![a, b, c, d].every(Number.isFinite)) return 'Off the field.';
    const mx = (a + c) / 2, my = (b + d) / 2;
    if (m.reach > 0 && this.frontGap(u, mx, my) > m.reach + 1e-9) return `They can lay smoke up to ${m.reach} m ahead.`;
    // An army that sets its smoke down along its own front lays it there: a line drawn far off is not where it would go.
    if (m.reach <= 0 && !this.fronted(u, mx, my) && this.frontGap(u, mx, my) > LAND.smoke.front + 1e-9) return 'They set their smoke down along their own front.';
    return null;
  }

  /**
   * Where this block lays its screen for the line drawn: along it, at most its own front long, about the line's middle
   * (a point: its front long, square to its face); set down along its own front where its army's means is (reach 0).
   */
  private smokeLine(u: LandUnit, l: number[], m: SmokeMeans): [number, number, number, number] {
    const [fx, fy] = ahead(u.face), r1 = (v: number): number => Math.round(v * 10) / 10;
    let mx: number, my: number, dx = fy, dy = -fx, half = u.w / 2;
    if (m.reach <= 0) { mx = u.x + (fx * u.d) / 2; my = u.y + (fy * u.d) / 2; }
    else {
      const [x0, y0, x1, y1] = l, len = Math.hypot(x1 - x0, y1 - y0);
      mx = (x0 + x1) / 2; my = (y0 + y1) / 2;
      if (len > 1e-6) { dx = (x1 - x0) / len; dy = (y1 - y0) / len; half = Math.min(len / 2, u.w / 2); }
    }
    return [r1(mx - dx * half), r1(my - dy * half), r1(mx + dx * half), r1(my + dy * half)];
  }

  /**
   * The smoke order landing (by radio, the player's): where it may no longer lay it there, nothing is laid and it goes
   * on as it was. Else the screen is laid at once, its count goes down, and it is told (land:smoke); the drill draws its
   * rounds. Its order is not touched: a block lays smoke as it goes.
   */
  private lay(u: LandUnit, l: number[]): void {
    if (this.smokeWhy(u, l[0], l[1], l[2], l[3]) !== null) return;
    const m = this.smokeOf(u)!, [x0, y0, x1, y1] = this.smokeLine(u, l, m);
    const s: Screen = { id: this.screens.length + 1, side: u.side, unit: u.id, x0, y0, x1, y1, t: Math.round(this.t * 1000) / 1000, until: Math.round((this.t + m.hang) * 1000) / 1000 };
    this.screens.push(s);
    u.smokeLeft = (u.smokeLeft ?? m.count ?? LAND.smoke.count) - 1;
    this.say({ what: 'land:smoke', unit: u.id, side: u.side, text: `${this.named(u)} lays a smoke screen.`, x: Math.round(((x0 + x1) / 2) * 10) / 10, y: Math.round(((y0 + y1) / 2) * 10) / 10, extra: { screen: s.id, until: s.until } });
    this.drill?.screen(s);
  }

  /** A live screen lies across the line from (ax, ay) to (bx, by). */
  screened(ax: number, ay: number, bx: number, by: number): boolean {
    for (const s of this.screens) if (this.t < s.until && segHit(ax, ay, bx, by, s.x0, s.y0, s.x1, s.y1) !== null) return true;
    return false;
  }

  /** A blow struck straight at what it sees (foot, tanks, a machine-gun team, a gun over open sights), not from far back (guns beyond their close range, a mortar team). */
  private direct(u: LandUnit, t: LandUnit): boolean {
    if (u.team === 'mor') return false;
    return !(u.kind === 'art' && !u.team && this.gap(u, t) >= gunsOf(u).closeAt);
  }

  /** The screens standing now, as a side sees them: every one, the laying block named where the side has met it. */
  screenView(as?: Side): Pick<BattleView, 'screens'> {
    if (!this.smoke || !this.screens.length) return {};
    const on = this.screens.filter((s) => this.t < s.until);
    if (!on.length) return {};
    return {
      screens: on.map((s) => {
        const u = this.unit(s.unit), named = as === undefined || s.side === as || !u || this.known(u);
        return { id: s.id, side: s.side, ...(named ? { unit: s.unit } : {}), x0: s.x0, y0: s.y0, x1: s.x1, y1: s.y1, t: s.t, until: s.until };
      }),
    };
  }

  // ---- Field telephone lines (release 2) ----------------------------------------------------------------------------
  //
  // Brian, 22 November 17:33Z ("lets do all three but make it where we can toggle it off"); the design director's
  // rulings of 18:36Z, PHONES Q1 to Q3 (LAND.phones). As the clock starts, every block of the defence on the field has a
  // wire from its headquarters (the middle of the defence zone's back edge); a corps that comes up from next door later
  // has none, and its orders go as ever. The wire runs straight to its block's middle and follows it: moving never cuts
  // it. Each blow on the block from an enemy gun block or mortar team cuts a line at LAND.phones.cut, drawn from the
  // wires' own stream ('phones:' and the seed), only for such blows and only while the wire has a line up; the cut lies
  // where the wire comes nearest the block's front. An enemy tank block whose step, its middle from where it stood to
  // where it stands, crosses the wire cuts a line outright, where it crosses. A wire is down once all its lines are cut
  // (Germany's double lines need two cuts). While it is down, every order to its block, the player's (radio switch on or
  // off) or a staff's, waits its army's fallback from when it was given: the wireless, LAND.radio's wait (longer for a
  // block whose headquarters is hit, as the radio switch reckons it), or a runner, the metres from the headquarters over
  // LAND.phones.runner. A later order takes its place and waits afresh, but a staff giving the same order again (it
  // thinks every second) leaves the one on its way be. At a wire's first cut with no lineman out on it, one leaves the
  // headquarters, walks the wire to the cut at foot pace and splices it in LAND.phones.splice seconds: the mend clears
  // every cut on the wire, and whatever was waiting for it goes at once. He is looks only: never struck, no harm. The
  // rules' own moves (the joiners walking on, a block giving ground at the brink) are no orders and never wait. Nothing
  // here draws on the battle's dice.

  /** The defence's headquarters, where every wire runs from: the middle of its zone's back edge (field.ts zonesOf). */
  phonesHq(): [number, number] {
    const z = this.field.zones.def, r1 = (v: number): number => Math.round(v * 10) / 10;
    return [r1((z[0] + z[2]) / 2), r1(z[3])];
  }

  /** This block's wire, where it has one (the switch on, a block of the defence on the field as the clock started). */
  wireOf(u: LandUnit): Wire | null { return this.phones ? this.wireBy.get(u.id) ?? null : null; }

  /** This block's wire is down: every line of it cut, and no lineman has mended it yet. */
  wireDown(u: LandUnit): boolean { const w = this.wireOf(u); return !!w && w.cut >= w.lines; }

  /** How orders go to this block while its wire is down: its army's fallback in the battle's year (LAND.phones.fallback). */
  fallbackOf(u: LandUnit): PhoneFallback { return phoneFallback(u.nat, turnToDate(this.ticket.turn).year); }

  /**
   * How long an order to this block waits while its wire is down, battle seconds: the wireless, LAND.radio's wait as the
   * radio switch reckons it (its headquarters hit: wavering or under hqAt of its strength); or a runner from the
   * headquarters to where it stands.
   */
  fallbackWait(u: LandUnit): number {
    if (this.fallbackOf(u) === 'radio') {
      const R = LAND.radio, hit = u.state === 'wavering' || u.str < R.hqAt * u.str0;
      return R.delay * (hit ? R.cut : 1) * this.radioOf(u);
    }
    const [hx, hy] = this.phonesHq();
    return Math.hypot(u.x - hx, u.y - hy) / LAND.phones.runner;
  }

  /** Each tick, before the orders land and the staffs think: the wires laid as the clock starts, and the mends whose time has come. */
  private phonesOn(): void {
    if (!this.phoneRng) this.layWires();
    for (const w of this.wires) if (w.out && this.t >= w.out.t - 1e-9) this.mend(w);
  }

  /** The clock starts: a wire to every block of the defence on the field, its own dice made (the day's luck's way). */
  private layWires(): void {
    this.phoneRng = new Rng(hashString(this.ticket.attempt ? `phones:${this.ticket.seed}:${this.ticket.attempt}` : `phones:${this.ticket.seed}`));
    const hq = this.phonesHq();
    for (const u of this.units) {
      if (u.side !== 'def' || !live(u)) continue;
      const w: Wire = { id: this.wires.length + 1, side: u.side, unit: u.id, hq, lines: phoneLines(u.nat), cut: 0, out: null };
      this.wires.push(w);
      this.wireBy.set(u.id, w);
    }
  }

  /**
   * A blow struck on a block at its wire's end: from an enemy gun block or mortar team, while the wire has a line up, it
   * cuts one at LAND.phones.cut, where the wire comes nearest the block's front. Machine guns, foot and tanks never.
   */
  private wireBlow(u: LandUnit, t: LandUnit): void {
    const w = this.wireBy.get(t.id);
    if (!w || w.cut >= w.lines || u.side === w.side || !(u.team === 'mor' || (u.kind === 'art' && !u.team))) return;
    if (this.phoneRng!.next() >= LAND.phones.cut) return;
    const [fx, fy] = ahead(t.face), px = t.x + (fx * t.d) / 2, py = t.y + (fy * t.d) / 2;
    const dx = t.x - w.hq[0], dy = t.y - w.hq[1], l = dx * dx + dy * dy;
    const k = l > 0 ? Math.max(0, Math.min(1, ((px - w.hq[0]) * dx + (py - w.hq[1]) * dy) / l)) : 1;
    this.wireCut(w, w.hq[0] + k * dx, w.hq[1] + k * dy, u);
  }

  /** Each tick, once the blocks have moved: an enemy tank block whose step crossed a wire with a line up cuts one there. */
  private wireTanks(was: [LandUnit, number, number][]): void {
    for (const [u, x0, y0] of was) {
      if (u.x === x0 && u.y === y0) continue;
      for (const w of this.wires) {
        const e = w.cut < w.lines && w.side !== u.side ? this.unit(w.unit) : undefined;
        if (!e || !live(e)) continue;
        const k = segHit(x0, y0, u.x, u.y, w.hq[0], w.hq[1], e.x, e.y);
        if (k !== null) this.wireCut(w, x0 + (u.x - x0) * k, y0 + (u.y - y0) * k, u);
      }
    }
  }

  /**
   * A line of the wire is cut: kept, and told to the defence (land:wire). At the wire's first cut with no lineman out, one
   * leaves the headquarters now; he reaches the cut at foot pace and the wire is mended LAND.phones.splice seconds later.
   */
  private wireCut(w: Wire, x: number, y: number, by: LandUnit): void {
    const r1 = (v: number): number => Math.round(v * 10) / 10, r3 = (v: number): number => Math.round(v * 1000) / 1000;
    w.cut++;
    const c: WireCut = { t: r3(this.t), wire: w.id, x: r1(x), y: r1(y), by: by.id };
    this.wireCuts.push(c);
    if (!w.out) {
      const t1 = r3(this.t + Math.hypot(c.x - w.hq[0], c.y - w.hq[1]) / LAND.speed.inf);
      w.out = { t: r3(t1 + LAND.phones.splice), wire: w.id, man: this.wireMends.length + 1, t0: c.t, t1, x: c.x, y: c.y };
      this.wireMends.push(w.out);
    }
    const u = this.unit(w.unit);
    if (!u || !this.phoneWords()) return;
    const down = w.cut >= w.lines, how = this.fallbackOf(u) === 'radio' ? 'by radio' : 'by runner';
    this.say({
      what: 'land:wire', unit: u.id, side: u.side, x: c.x, y: c.y, extra: { wire: w.id, down: down ? 1 : 0, by: this.fallbackOf(u), mend: w.out.t },
      text: down ? `The telephone line to ${this.named(u)} is cut. Orders go ${how} until it is mended.`
        : `One of the ${w.lines === 2 ? 'two telephone lines' : 'telephone lines'} to ${this.named(u)} is cut. Orders still go by the ${w.lines - w.cut === 1 ? 'other' : 'others'}.`,
    });
  }

  /** A lineman's splice is done: every cut on the wire is mended, and whatever was waiting for it goes at once. */
  private mend(w: Wire): void {
    const m = w.out!, r3 = (v: number): number => Math.round(v * 1000) / 1000;
    w.out = null; w.cut = 0;
    const u = this.unit(w.unit);
    const now = this.pending.filter((p) => p.id === w.unit && p.held);
    if (now.length) {
      this.pending = this.pending.filter((p) => !(p.id === w.unit && p.held));
      if (u && now.some((p) => !p.smoke)) delete u.orderAt;
      if (u && fighting(u)) for (const p of now) { p.go(); p.held!.went = r3(this.t); }
    }
    if (u && this.phoneWords()) this.say({ what: 'land:mend', unit: u.id, side: u.side, x: m.x, y: m.y, extra: { wire: w.id, man: m.man }, text: `The line to ${this.named(u)} is mended.` });
  }

  /** The wires' events go to the defence alone: told where it is the player's, or nobody's (a recording), never to a player attacking. */
  private phoneWords(): boolean { return this.ticket.def.human || !this.ticket.att.human; }

  /**
   * An order for a block whose wire is down: it waits its army's fallback from when it was given (from). A staff giving
   * the very same order again leaves the one on its way be; any other order takes its place and waits afresh. Stamped
   * (wireHeld) as it is given, and as it goes.
   */
  private byFallback(u: LandUnit, go: () => void, from: number, c?: Command, smoke = false): void {
    const r3 = (v: number): number => Math.round(v * 1000) / 1000, key = c ? JSON.stringify(c) : undefined;
    // A smoke order keeps its own place on the air here too (order): it replaces only a smoke order still on its way.
    const slot = (p: (typeof this.pending)[number]): boolean => p.id === u.id && !p.smoke === !smoke;
    const was = this.pending.find(slot);
    if (this.staffing && was?.held && key !== undefined && was.key === key) return;
    const held: WireHeld = { t: r3(from), unit: u.id, due: r3(from + this.fallbackWait(u)) };
    this.wireHeld.push(held);
    this.pending = this.pending.filter((p) => !slot(p));
    if (held.due <= this.t + 1e-9) { if (!smoke) delete u.orderAt; go(); held.went = r3(this.t); return; }
    this.pending.push(smoke ? { at: held.due, id: u.id, go, smoke, key, held } : { at: held.due, id: u.id, go, key, held });
    if (!smoke) u.orderAt = held.due;
  }

  /**
   * The wires as a side sees them: the defence its own, and everyone everything where the view is unfogged (no side, or
   * the card knew the defence). A cut's striker is named only where the side has met it.
   */
  phonesView(as?: Side): Pick<BattleView, 'phones'> {
    if (!this.phonesSeen(as)) return {};
    const hq = this.phonesHq(), r1 = (v: number): number => Math.round(v * 10) / 10;
    return {
      phones: {
        hq: [...hq] as [number, number],
        wires: this.wires.map((w) => {
          const u = this.unit(w.unit)!;
          return { id: w.id, side: w.side, pts: [[...w.hq], [r1(u.x), r1(u.y)]] as [number, number][], hq: [...w.hq] as [number, number], unit: w.unit, lines: w.lines, cut: w.cut };
        }),
        cuts: this.wireCuts.map((c) => { const by = this.unit(c.by); return { t: c.t, wire: c.wire, x: c.x, y: c.y, ...(as === undefined || !by || by.side === as || this.known(by) ? { by: c.by } : {}) }; }),
        mends: this.wireMends.map((m) => ({ ...m })),
        held: this.wireHeld.map((h) => ({ ...h })),
      },
    };
  }

  /** Whether a side sees the wires (phonesView): once they are laid, the defence, and everyone where the view is unfogged. */
  phonesSeen(as?: Side): boolean {
    return this.phones && this.phoneRng !== null && (as === undefined || as === 'def' || !this.fog);
  }

  /**
   * Whether the player knows this block (b464(2)): his own always; the other side's where the card knew the defence,
   * or the card identified its corps (b476(3)), once one of his has fought it or it has struck one of his in contact
   * (found), and once the battle is over.
   */
  known(u: LandUnit): boolean { return !this.fog || this.ticket[u.side].human || this.seen.has(u.corps) || !!u.found || this.over !== null; }

  /** A block's name in a dispatch: the side nobody plays carries its country, so it never reads like the player's own (ruling 226(7)). */
  named(u: LandUnit): string { return this.ticket[u.side].human ? u.name : `${this.ticket[u.side].adj} ${u.name}`; }

  /** A practice ground is no place in the campaign: its sides are the player's and theirs, not compass words. */
  private get practice(): boolean { return this.ticket.place.region < 0; }
  /** "The Polish army", or on a practice ground "Your army" or "Their army" (the UI director, after the retest). */
  private army(s: Side): string { return this.practice ? (this.ticket[s].human ? 'Your army' : 'Their army') : `The ${this.ticket[s].adj} army`; }
  /** "Your army" for the player's own, else as army() names it: the break lines' words (the UI director, 14:44Z). */
  private whose(s: Side): string { return this.ticket[s].human ? 'Your army' : this.army(s); }
  /** "the Polish defence", or on a practice ground "your defence" or "the defence". */
  private sideOf(s: Side, noun: string): string { return this.practice ? `${this.ticket[s].human ? 'your' : 'the'} ${noun}` : `the ${this.ticket[s].adj} ${noun}`; }

  // ---- Orders -----------------------------------------------------------------------------------------------------

  command(c: Command, by: Side): boolean {
    if (this.over) return false;
    // Falling back, the army takes no new orders (all fall back, release 1.5): handing it to the computer or the dice still ends it.
    if (this.falling[by] !== null && c.do !== 'auto' && c.do !== 'dice' && c.do !== 'withdrawAll') return false;
    // A block waiting for help (hold until relieved, release 2) is never its staff's to order: only the player's own
    // order calls its wait off.
    const mine = (ids: number[]): LandUnit[] => ids.map((i) => this.unit(i)).filter((u): u is LandUnit => !!u && u.side === by && fighting(u) && u.kind !== 'fort' && !(this.staffing && u.waits && !u.waits.free));
    // Direct command (release 2): the player's own orders, not his staff's. The blocks he orders are his (perUnit), and
    // his orders go by radio once the clock runs (radio); his staff's, and the computer's, as ever.
    const player = !this.staffing && this.ticket[by].human;
    const hands = (us: LandUnit[]): void => { if (this.perUnit && player) for (const u of us) this.take(u); };
    const air = this.radio && player && this.t > 0;
    // The battle time he gave it, as the 3D game shows it (it runs the rules a little ahead of the picture): the radio's
    // wait counts from there, never from before it. Absent: now.
    const said = (c as { at?: unknown }).at, from = typeof said === 'number' && Number.isFinite(said) ? Math.min(this.t, said) : this.t;
    const give = (u: LandUnit, go: () => void, smoke = false): void => this.order(u, go, air, from, c, smoke);
    switch (c.do) {
      case 'move': {
        const us = mine(c.units);
        if (!us.length) return false;
        hands(us);
        // A group keeps its shape: each block goes to the point plus its place in the group. At the double (run): the
        // player's run (release 2).
        const cx = us.reduce((s, u) => s + u.x, 0) / us.length, cy = us.reduce((s, u) => s + u.y, 0) / us.length;
        for (const u of us) {
          const o: LandOrder = { kind: 'move', x: c.x + (us.length > 1 ? u.x - cx : 0), y: c.y + (us.length > 1 ? u.y - cy : 0), face: c.face, ...(c.run ? { run: true } : {}) };
          give(u, () => this.send(u, o));
        }
        this.say({ what: 'contact', side: by, text: '' });
        this.events.pop(); this.log.pop();
        return true;
      }
      case 'line': {
        const us = mine(c.units);
        if (!us.length) return false;
        hands(us);
        for (const p of lineUp(us, by, c.x0, c.y0, c.x1, c.y1, 150)) {
          const u = this.unit(p.id)!;
          give(u, () => this.send(u, { kind: 'move', x: p.x, y: p.y, face: p.face }));
        }
        return true;
      }
      case 'attack': {
        const t = this.unit(c.target);
        const all = mine(c.units);
        // Never at a block hidden from his side (release 2): nobody there knows it is there.
        if (!t || t.side === by || !live(t) || t.hid || !all.length) return false;
        hands(all);
        // A sniper team sent in with others (the plain attack: every block at the nearest enemy) takes no lane in the
        // line abreast: it stays in its cover and takes the target as its aim, as a fire order gives it (land:fire). In
        // a lane of its own it pushed the line's real blocks out past the target's flanks and cost the plain attack 6
        // battles in 100 (the sniper gate's G7, 25 Nov; the design director's ruling of 18:55Z). Sent alone, or with
        // only other sniper teams, it goes as ordered.
        const lone = all.some((u) => u.team !== 'snp') ? all.filter((u) => u.team === 'snp') : [];
        for (const u of lone) give(u, () => { u.order = null; u.path = []; u.keepFace = null; u.aim = t.id; delete u.cut; });
        const us = lone.length ? all.filter((u) => u.team !== 'snp') : all;
        if (!us.length) return true;
        // Several blocks sent at one: they come on abreast, each in its own lane, the outer ones round the target's
        // flanks, instead of all making for its middle. One block alone goes straight for it (the computer general's way).
        const cx = us.reduce((s, u) => s + u.x, 0) / us.length, cy = us.reduce((s, u) => s + u.y, 0) / us.length;
        const axis = bearing(cx, cy, t.x, t.y);
        const [rx, ry] = ahead(axis + Math.PI / 2);
        const across = [...us].sort((a, b) => (a.x - cx) * rx + (a.y - cy) * ry - ((b.x - cx) * rx + (b.y - cy) * ry));
        const step = across.reduce((s, u) => s + u.w, 0) / across.length + 30;
        // A lane that runs past the target's flank takes the enemy block nearest it in that lane instead, so a whole
        // army sent at one block falls on their line there, block against block (the computer general's own way),
        // and never crowds one block while its neighbours take the attack in the flank (ruling 226(4)).
        const foes = this.units.filter((e) => e.side !== by && fighting(e) && !e.hid);
        // The computer general's groups go in together, its tanks keeping with their foot (ai.ts); and the player's group
        // attack too (ruling 239(5), 7b), as the card's Combined arms line assumes. A block sent alone goes alone.
        const together = c.together !== undefined ? { together: c.together } : us.length > 1 && LAND.together.player ? { together: ++this.groupIds } : {};
        across.forEach((u, i) => {
          const lane = (i - (us.length - 1) / 2) * step;
          if (us.length > 1 && Math.abs(lane) > t.w / 2 + u.w / 2) {
            const px = t.x + rx * lane, py = t.y + ry * lane;
            const e = foes.reduce((m, f) => (dist(f.x, f.y, px, py) < dist(m.x, m.y, px, py) ? f : m), t);
            // Still in its own lane, now before the block it falls on: each block its own stretch of their front (and
            // of the far bank, over a river), never every block on one enemy's middle (the art director, b439).
            if (e !== t) { const o: LandOrder = { kind: 'attack', x: e.x, y: e.y, target: e.id, press: true, lane: lane - ((e.x - t.x) * rx + (e.y - t.y) * ry), axis, ...together }; give(u, () => this.send(u, o)); return; }
          }
          const o: LandOrder = { kind: 'attack', x: t.x, y: t.y, target: t.id, press: us.length > 1, ...(us.length > 1 ? { lane, axis } : {}), ...together };
          give(u, () => this.send(u, o));
        });
        return true;
      }
      case 'hold': {
        const us = mine(c.units);
        hands(us);
        for (const u of us) give(u, () => { u.order = null; u.path = []; u.keepFace = null; delete u.aim; delete u.search; delete u.searchR; delete u.cut; });
        return us.length > 0;
      }
      case 'withdraw': {
        const us = mine(c.units);
        hands(us);
        for (const u of us) give(u, () => this.send(u, { kind: 'withdraw', x: u.x, y: by === 'att' ? 0 : this.field.h }));
        return us.length > 0;
      }
      case 'withdrawAll': {
        if (c.side !== by) return false;
        // What the fighters on call still owe lands before the army goes (air support), and what waits' cover spared
        // (hold until relieved): pulling out never escapes either.
        this.payAir();
        this.payWaits();
        // How near breaking it stood when it pulled out, before its blocks leave the count (b474).
        this.bar[by] = this.breaking(by);
        for (const u of this.units) if (u.side === by && fighting(u)) { u.state = 'withdrawn'; u.act = 'down'; }
        this.finish(other(by), 'withdrew');
        return true;
      }
      case 'auto': {
        if (c.side !== by && !this.ai[c.side]) return false;
        this.ai[c.side] = c.on;
        // Handed to the computer, the whole side is its staff's again (direct command, release 2).
        if (c.on) for (const u of this.units) if (u.side === c.side) { delete u.taken; delete u.cut; }
        return true;
      }
      case 'dice': {
        this.toDice();
        return true;
      }
      default: {
        // Air support (below): { do: 'land:air', x0, y0, x1, y1 }, the run to strike down.
        if (c.do === 'land:air') {
          const r = [c.x0, c.y0, c.x1, c.y1].map(Number);
          return r.every(Number.isFinite) && this.callAir(by, r as [number, number, number, number]);
        }
        // Fire on, stay put (the design director, ruling 239(6)): { do: 'land:fire', units, target }. The player's guns
        // fire on that block whenever it is in their reach, before whatever else is nearest, and stand where they are:
        // the computer's aim given to the player, never aimed sharper than the player (b533). Other blocks are left be.
        // The setup step (release 2): { do: 'land:place', units: [id], x, y } puts one gun block there; { do:
        // 'land:begin' } ends the step, and the clock may run.
        // { do: 'land:place', ..., face } turns it that way (release 2, radians as the core's).
        if (c.do === 'land:place') {
          const u = this.unit(Number((c.units as number[] | undefined)?.[0])), x = Number(c.x), y = Number(c.y);
          if (!u || this.placeWhy(u, x, y) !== null || by !== this.setup) return false;
          const f = c.face === undefined ? undefined : Number(c.face);
          this.place(u, x, y, f !== undefined && Number.isFinite(f) ? f : undefined);
          return true;
        }
        // Night patrol (release 2): { do: 'land:patrol', unit } while he places his army, his attack sends that block's
        // patrol out the night before (patrolWhy). Settled as the clock starts (hideStart); none unless he orders one.
        if (c.do === 'land:patrol') {
          const u = this.unit(Number(c.unit ?? (c.units as number[] | undefined)?.[0]));
          if (!this.patrol || !u || u.side !== by || by !== this.setup || this.patrolWhy(u) !== null) return false;
          this.patrolBy = u.id;
          return true;
        }
        // Hold until relieved (release 2): { do: 'land:wait', units } tells blocks on foot to dig in and wait for a corps
        // coming up from next door (waitWhy). Gated again as it lands: where one may no longer wait, it holds.
        // All fall back (release 1.5; Brian, 23 November 14:47Z): { do: 'land:fallBack' } sends every block of the side
        // back toward its own edge, by radio as any order (fallBackWhy). Corps still coming up from next door turn back.
        // Each block is out once behind the zone its side drew up in (outLine); the last one out ends the battle as the
        // side's withdrawal, billed by the highest its bar stood from now on (the design director, 14:54Z, on b474).
        if (c.do === 'land:fallBack') {
          if (this.fallBackWhy(by) !== null) return false;
          const us = this.units.filter((u) => u.side === by && fighting(u) && u.kind !== 'fort');
          this.falling[by] = this.t;
          this.fallBar[by] = this.breaking(by);
          this.coming = this.coming.filter((k) => k.u.side !== by);
          hands(us);
          for (const u of us) give(u, () => this.send(u, { kind: 'withdraw', x: u.x, y: by === 'att' ? 0 : this.field.h }));
          this.say({ what: 'land:fallBack', side: by, text: `${this.army(by)} falls back.` });
          return true;
        }
        if (c.do === 'land:wait') {
          if (!this.relief) return false;
          // Gated block by block, in order: one that may not wait (its corps spoken for by one before it) keeps its order.
          const all = mine((c.units ?? []) as number[]), whys = this.waitWhyAll(all);
          const us = all.filter((_, i) => whys[i] === null);
          if (!us.length) return false;
          hands(us);
          for (const u of us) give(u, () => this.wait(u));
          return true;
        }
        // Hedge cutters (release 2): { do: 'land:cut', units, x, y } sends tank blocks to cut a gap through the hedge at
        // that point and drive on through it (cutWhy). Gated again as it lands: where one may no longer cut there, it holds.
        if (c.do === 'land:cut') {
          if (!this.cutters) return false;
          const x = Number(c.x), y = Number(c.y);
          if (!Number.isFinite(x) || !Number.isFinite(y)) return false;
          const us = mine((c.units ?? []) as number[]).filter((u) => this.cutWhy(u, x, y) === null);
          if (!us.length) return false;
          hands(us);
          // Several blocks told to cut at one point keep the group's shape, as a move does: each cuts its own gap at its
          // place in the group along the hedge there, where the hedge runs that far; else it makes for the point, and goes
          // through the gap cut there (cutOn: never a second cut on one gap).
          const p = this.hedgeNear(x, y, LAND.cutters.near)!, ax = Math.cos(p.yaw), ay = Math.sin(p.yaw);
          const cx = us.reduce((s, u) => s + u.x, 0) / us.length, cy = us.reduce((s, u) => s + u.y, 0) / us.length;
          for (const u of us) {
            const k = (u.x - cx) * ax + (u.y - cy) * ay, q = us.length > 1 ? this.hedgeNear(p.x + ax * k, p.y + ay * k, LAND.cutters.near) : null;
            const [px, py] = q ? [q.x, q.y] : [x, y];
            give(u, () => this.cutGo(u, px, py));
          }
          return true;
        }
        // Smoke on call (release 2): { do: 'land:smoke', units, x0, y0, x1, y1 } has blocks on foot lay a screen along that
        // line (smokeWhy). Gated again as it lands: where one may no longer lay it, nothing is laid. A block laying smoke
        // keeps its order, and the one still on the air for it (the smoke has its own place on the air: order()), and is
        // not taken from the staff for it (it lays smoke as it goes).
        if (c.do === 'land:smoke') {
          if (!this.smoke) return false;
          const l = [c.x0, c.y0, c.x1 ?? c.x0, c.y1 ?? c.y0].map(Number);
          if (!l.every(Number.isFinite)) return false;
          const us = mine((c.units ?? []) as number[]).filter((u) => this.smokeWhy(u, l[0], l[1], l[2], l[3]) === null);
          if (!us.length) return false;
          for (const u of us) give(u, () => this.lay(u, l), true);
          return true;
        }
        // Direct command (release 2): { do: 'land:staff', units } hands blocks the player took back to his staff.
        if (c.do === 'land:staff') {
          const us = mine((c.units ?? []) as number[]).filter((u) => u.taken);
          for (const u of us) { delete u.taken; delete u.cut; }
          return us.length > 0;
        }
        if (c.do === 'land:begin') {
          if (by !== this.setup) return false;
          this.setup = null; this.room = null;
          // The rider ahead: his standing spot clear of the guns just placed (looks only).
          for (const r of this.riders) this.riderTo(r);
          return true;
        }
        // Reconnaissance by fire (the historian's check, release 2): { do: 'land:fire', units, x, y } with no target:
        // the blocks stand and fire on the cover there (LAND.hide.search). Any of his blocks but forts.
        if (c.do === 'land:fire' && c.target === undefined) {
          const x = Number(c.x), y = Number(c.y);
          const us = mine((c.units ?? []) as number[]).filter((u) => u.kind !== 'fort');
          if (!Number.isFinite(x) || !Number.isFinite(y) || !us.length) return false;
          hands(us);
          for (const u of us) give(u, () => { u.order = null; u.path = []; u.keepFace = null; delete u.aim; u.search = [x, y]; delete u.searchR; delete u.cut; });
          return true;
        }
        if (c.do === 'land:fire') {
          const t = this.unit(Number(c.target));
          const guns = mine((c.units ?? []) as number[]).filter((u) => u.kind === 'art');
          if (!t || t.side === by || !live(t) || t.hid || !guns.length) return false;
          hands(guns);
          for (const u of guns) give(u, () => { u.order = null; u.path = []; u.keepFace = null; u.aim = t.id; delete u.cut; });
          return true;
        }
        return false;
      }
    }
  }

  /** The player takes a block in hand (direct command): his staff leaves it be; a held reserve he orders is committed. */
  private take(u: LandUnit): void {
    u.taken = true;
    if (u.reserve === 'held') u.reserve = 'sent';
  }

  /**
   * An order for one block: at once, or by radio (direct command, release 2), landing LAND.radio's delay later, longer
   * when its headquarters is hit. A later order on the air replaces the one still on the way. A smoke order (smoke on
   * call, release 2: 'smoke') has a place of its own on the air: it replaces only a smoke order of that block's still on
   * the way, never its move or attack, nor is replaced by one, so "move, then smoke to cover it" lands both; and it
   * leaves the block's orderAt (its move's or attack's) alone.
   */
  private order(u: LandUnit, go: () => void, air: boolean, from = this.t, c?: Command, smoke = false): void {
    // Field telephone lines (release 2): to a block whose wire is down, whoever gives it, by its army's fallback.
    if (this.phones && this.t > 0 && this.wireDown(u)) { this.byFallback(u, go, from, c, smoke); return; }
    if (!air) { go(); return; }
    const R = LAND.radio, hit = u.state === 'wavering' || u.str < R.hqAt * u.str0;
    const at = Math.round((from + R.delay * (hit ? R.cut : 1) * this.radioOf(u)) * 1000) / 1000;
    this.pending = this.pending.filter((p) => p.id !== u.id || !p.smoke !== !smoke);
    // Given long enough ago (as shown) that it is due already: carried out now.
    if (at <= this.t + 1e-9) { if (!smoke) delete u.orderAt; go(); return; }
    this.pending.push(smoke ? { at, id: u.id, go, smoke } : { at, id: u.id, go });
    if (!smoke) u.orderAt = at;
  }

  /** The player's orders on the air that have reached their blocks (radio): carried out, if the block still fights. */
  private landOrders(): void {
    if (!this.pending.length) return;
    const due = this.pending.filter((p) => p.at <= this.t + 1e-9);
    if (!due.length) return;
    this.pending = this.pending.filter((p) => p.at > this.t + 1e-9);
    for (const p of due) {
      const u = this.unit(p.id);
      if (!u) continue;
      if (!p.smoke) delete u.orderAt;
      if (fighting(u)) { p.go(); if (p.held) p.held.went = Math.round(this.t * 1000) / 1000; }
    }
  }

  private send(u: LandUnit, o: LandUnit['order'] & object): void {
    // A block waiting for help given any order but hold calls its wait off: from where it stands, help never came.
    if (u.waits && !u.waits.free) this.endWait(u, 'off');
    o.x = clamp(o.x, u.w / 2, this.field.w - u.w / 2);
    // A block falling back makes for its own edge of the field itself, where it leaves (b418: it stopped 10 m short).
    if (o.kind !== 'withdraw') o.y = clamp(o.y, 10, this.field.h - 10);
    // Sent to stand in the river: it stands on the nearest bank instead (a bridge it may hold).
    if (o.kind === 'move' && this.wet(u, o.x, o.y, false) && groundAt(this.field, o.x, o.y) !== 'bridge') {
      search: for (let r = 8; r <= 240; r += 8) for (let a = 0; a < 16; a++) {
        const x = clamp(o.x + Math.sin((a * Math.PI) / 8) * r, u.w / 2, this.field.w - u.w / 2), y = clamp(o.y + Math.cos((a * Math.PI) / 8) * r, 10, this.field.h - 10);
        if (!this.wet(u, x, y, false)) { o.x = x; o.y = y; break search; }
      }
    }
    u.order = o;
    u.keepFace = o.face ?? null;
    // Any other order lifts a gun's aim (fire on, stay put), and fire on cover; and a cut it was sent to make (cutGo sets
    // it again after).
    delete u.aim; delete u.search; delete u.searchR; delete u.cut;
    // An attack's way is the one it will walk: to its place in the lane before its target, as the march works it out.
    const t = o.kind === 'attack' && o.target !== undefined ? this.unit(o.target) : undefined;
    const at = t ? this.lanePoint(u, t, this.closeTo(u)) : [o.x, o.y];
    u.path = at ? this.route(u, at[0], at[1]) : [];
  }

  /** How near a block closes on what it attacks before it stands and fights: well inside its reach. */
  private closeTo(u: LandUnit): number {
    return reachOf(u) * (u.kind === 'art' ? 0.85 : 0.6);
  }

  /**
   * The way there: straight, unless ground it cannot enter lies across it (a river away from its bridges, rock, marsh
   * for a lorry, a bunker). Then round it, the quickest way over the ground (a road is quicker than a field), cut to as
   * few turns as can be walked straight between: over a bridge, round a bend of the river, round an outcrop. Where it
   * cannot get there at all it goes as near as it can, and stands. A block set down in ground it cannot enter (a tank
   * among rock) first walks out of it the shortest way. An attack's way is asked for afresh every tick, so the way found
   * is kept while the block makes for much the same place, and the block takes the furthest turning point it can walk
   * straight to (b418: tanks and lorries never crossed rivers, and stood for good at rock and marsh).
   */
  private ways = new Map<number, Way>();
  /** By how it moves: the cells within a few of ground it can hardly cross or not at all (a river, rock, marsh). */
  private hardNear: Partial<Record<Going, Uint8Array>> = {};
  // (A joiner over dry ground keeps off the water only where the river bends away from the dry lanes; with no bend it wades.)
  private going(u: LandUnit): Going {
    if (this.bocage && this.heldBack(u)) return 'hedged';
    return u.dry && motionOf(u) === 'foot' && (u.joined === undefined || this.field.bend) ? 'ashore' : motionOf(u);
  }
  private ashore: Record<Ground, number> | null = null;
  private paceOf(m: Going): Record<Ground, number> { return m === 'ashore' ? (this.ashore ??= { ...LAND.pace.foot, water: 0, blown: 0 }) : m === 'hedged' ? LAND.pace.tracks : LAND.pace[m]; }
  private route(u: LandUnit, x: number, y: number): [number, number][] {
    const m = this.going(u), f = this.field;
    if (m === 'fixed') return [[x, y]];
    const out = this.exit(m, u.x, u.y);
    if (out) return [...out, [x, y]];
    if (this.dry(m, u.x, u.y, x, y)) { this.ways.delete(u.id); return [[x, y]]; }
    const goal = this.cellAt(x, y);
    const near = (a: number, b: number): boolean => Math.abs((a % f.cols) - (b % f.cols)) <= 2 && Math.abs(Math.floor(a / f.cols) - Math.floor(b / f.cols)) <= 2;
    let w = this.ways.get(u.id), fresh = false;
    for (;;) {
      if (!w || w.m !== m || !near(w.goal, goal) || (m === 'hedged' && w.bars !== this.barsLaid)) {
        w = { m, goal, ...this.findWay(m, u.x, u.y, x, y), ...(m === 'hedged' ? { bars: this.barsLaid } : {}) };
        this.ways.set(u.id, w);
        fresh = true;
      }
      const pts = w.pts, end = pts[pts.length - 1];
      let k = pts.length - 1;
      while (k >= 0 && !this.dry(m, u.x, u.y, pts[k][0], pts[k][1])) k--;
      // Pushed off its way, or at the end of a way kept from a little earlier with the place now out of sight: look again.
      const lost = k < 0 || (w.whole && k === pts.length - 1 && dist(u.x, u.y, end[0], end[1]) < 10 && !this.dry(m, end[0], end[1], x, y));
      if (lost && !fresh) { w = undefined; continue; }
      // Nowhere nearer to go: it stays where it is.
      if (k < 0) return [[u.x, u.y]];
      const way = pts.slice(k);
      if (w.whole && this.dry(m, end[0], end[1], x, y) && (end[0] !== x || end[1] !== y)) way.push([x, y]);
      return way;
    }
  }

  private cellAt(x: number, y: number): number {
    const f = this.field;
    return clamp(Math.floor(y / f.cell), 0, f.rows - 1) * f.cols + clamp(Math.floor(x / f.cell), 0, f.cols - 1);
  }

  /**
   * The way a block standing in ground it cannot enter walks out: cell by cell, side to side, over that same kind of
   * ground (never into water it cannot cross unless it stands in water already) to the nearest cell it can enter.
   * Nothing where it stands on ground it can enter.
   */
  private exit(m: Going, x: number, y: number): [number, number][] | null {
    const f = this.field, C = f.cols, pace = this.paceOf(m), s = this.cellAt(x, y), bar = m === 'hedged' ? this.barrier() : null;
    const go = (i: number): boolean => pace[GROUNDS[f.ground[i]]] > 0 && !(bar && bar[i]);
    // A tank block without cutters standing at a hedge (the bocage) walks out on its own side of it, never over it.
    const mine = (i: number): boolean => !bar || !this.barredCross(x, y, (i % C) * f.cell + f.cell / 2, Math.floor(i / C) * f.cell + f.cell / 2);
    if (go(s)) return null;
    const wet = (i: number): boolean => GROUNDS[f.ground[i]] === 'water' || GROUNDS[f.ground[i]] === 'blown';
    const inWet = wet(s), back = new Map<number, number>([[s, -1]]);
    const mid = (i: number): [number, number] => [(i % C) * f.cell + f.cell / 2, Math.floor(i / C) * f.cell + f.cell / 2];
    let ring = [s];
    while (ring.length) {
      const next: number[] = [];
      let best = -1, bd = Infinity;
      for (const i of ring) for (const [dc, dr] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {
        const c = (i % C) + dc, r = Math.floor(i / C) + dr, j = r * C + c;
        if (c < 0 || c >= C || r < 0 || r >= f.rows || back.has(j)) continue;
        back.set(j, i);
        if (go(j) && mine(j)) {
          const d = dist(x, y, ...mid(j));
          if (d < bd || (d === bd && j < best)) { bd = d; best = j; }
        } else if (inWet || !wet(j)) next.push(j);
      }
      if (best >= 0) {
        const way: [number, number][] = [];
        for (let i = best; i !== s; i = back.get(i)!) way.unshift(mid(i));
        return way;
      }
      ring = next;
    }
    return null;
  }

  /**
   * The quickest way over the field's cells from one point to another for a block that moves this way, side to side or
   * corner to corner (never across the corner of ground it cannot enter), then cut to the fewest turning points it can
   * walk straight between. Where the place cannot be reached, the way ends at the nearest cell that can, and says so.
   */
  private findWay(m: Going, sx: number, sy: number, x: number, y: number): { pts: [number, number][]; whole: boolean } {
    const f = this.field, C = f.cols, n = f.ground.length, pace = this.paceOf(m), bar = m === 'hedged' ? this.barrier() : null;
    const top = Math.max(...Object.values(pace)), s = this.cellAt(sx, sy), g = this.cellAt(x, y), gc = g % C, gr = Math.floor(g / C);
    const go = (i: number): number => (bar && bar[i] ? 0 : pace[GROUNDS[f.ground[i]]]);
    const h = (i: number): number => {
      const dc = Math.abs((i % C) - gc), dr = Math.abs(Math.floor(i / C) - gr);
      return (Math.max(dc, dr) + (Math.SQRT2 - 1) * Math.min(dc, dr)) / top;
    };
    const cost = new Float64Array(n).fill(Infinity), back = new Int32Array(n).fill(-1), done = new Uint8Array(n);
    // A small heap of cells by cost so far plus the least cost left, each entry keeping the key it went in with; ties
    // go to the lower cell, so the way is always the same.
    const hk: number[] = [], hv: number[] = [];
    const less = (a: number, b: number): boolean => hk[a] < hk[b] || (hk[a] === hk[b] && hv[a] < hv[b]);
    const swap = (a: number, b: number): void => { [hk[a], hk[b]] = [hk[b], hk[a]]; [hv[a], hv[b]] = [hv[b], hv[a]]; };
    const push = (i: number, key: number): void => {
      hk.push(key); hv.push(i);
      for (let k = hk.length - 1; k > 0;) { const p = (k - 1) >> 1; if (!less(k, p)) break; swap(k, p); k = p; }
    };
    const pop = (): number => {
      const top0 = hv[0], lk = hk.pop()!, lv = hv.pop()!;
      if (hk.length) {
        hk[0] = lk; hv[0] = lv;
        for (let k = 0; ;) {
          const l = 2 * k + 1, r = l + 1;
          let b = k;
          if (l < hk.length && less(l, b)) b = l;
          if (r < hk.length && less(r, b)) b = r;
          if (b === k) break;
          swap(k, b); k = b;
        }
      }
      return top0;
    };
    cost[s] = 0; push(s, h(s));
    let best = s, bh = h(s);
    while (hk.length) {
      const i = pop();
      if (done[i]) continue;
      done[i] = 1;
      if (h(i) < bh) { bh = h(i); best = i; }
      if (i === g) break;
      const c = i % C, r = Math.floor(i / C);
      for (let dr = -1; dr <= 1; dr++) for (let dc = -1; dc <= 1; dc++) {
        if ((!dr && !dc) || c + dc < 0 || c + dc >= C || r + dr < 0 || r + dr >= f.rows) continue;
        const j = i + dr * C + dc;
        if (done[j] || go(j) <= 0) continue;
        if (dr && dc && (go(i + dc) <= 0 || go(i + dr * C) <= 0)) continue;
        const c2 = cost[i] + (dr && dc ? Math.SQRT2 : 1) / Math.max(0.12, go(j));
        if (c2 < cost[j]) { cost[j] = c2; back[j] = i; push(j, c2 + h(j)); }
      }
    }
    const cs = f.cell, mid = (i: number): [number, number] => [(i % C) * cs + cs / 2, Math.floor(i / C) * cs + cs / 2];
    const raw: [number, number][] = [];
    for (let i = best; i !== -1 && i !== s; i = back[i]) raw.unshift(mid(i));
    if (best === g && raw.length) raw[raw.length - 1] = [x, y];
    // Already in its own cell: the place itself, where that is the cell; else nowhere nearer to go.
    if (!raw.length) return best === g ? { pts: [[x, y]], whole: true } : { pts: [[sx, sy]], whole: false };
    // Cut the corners: from each turning point, straight on to the last cell it can still walk to.
    const pts: [number, number][] = [];
    let ax = sx, ay = sy;
    for (let j = 1; j < raw.length; j++) {
      if (!this.dry(m, ax, ay, raw[j][0], raw[j][1])) { [ax, ay] = raw[j - 1]; pts.push(raw[j - 1]); }
    }
    pts.push(raw[raw.length - 1]);
    return { pts, whole: best === g };
  }

  /**
   * Whether a block that moves this way can go straight from one point to the other over nothing it cannot enter:
   * every cell the line passes through after the one it starts in, not samples along it, so the line never clips the
   * corner of a water or rock cell. A line through the very corner of two cells goes from one to the other across it.
   */
  private dry(m: Going, x0: number, y0: number, x1: number, y1: number): boolean {
    const f = this.field, cs = f.cell;
    const cOf = (v: number): number => clamp(Math.floor(v / cs), 0, f.cols - 1), rOf = (v: number): number => clamp(Math.floor(v / cs), 0, f.rows - 1);
    let c = cOf(x0), r = rOf(y0);
    const c1 = cOf(x1), r1 = rOf(y1), dx = x1 - x0, dy = y1 - y0, sx = Math.sign(dx), sy = Math.sign(dy);
    const stepX = dx ? cs / Math.abs(dx) : Infinity, stepY = dy ? cs / Math.abs(dy) : Infinity;
    let tx = dx ? (sx > 0 ? (c + 1) * cs - x0 : x0 - c * cs) / Math.abs(dx) : Infinity;
    let ty = dy ? (sy > 0 ? (r + 1) * cs - y0 : y0 - r * cs) / Math.abs(dy) : Infinity;
    const pace = this.paceOf(m), bar = m === 'hedged' ? this.barrier() : null;
    for (let n = Math.abs(c1 - c) + Math.abs(r1 - r); n > 0 && (c !== c1 || r !== r1); n--) {
      if (Math.abs(tx - ty) < 1e-9) { tx += stepX; ty += stepY; c += sx; r += sy; n--; }
      else if (tx < ty) { tx += stepX; c += sx; }
      else { ty += stepY; r += sy; }
      if (c < 0 || r < 0 || c >= f.cols || r >= f.rows) return true;
      if (pace[GROUNDS[f.ground[r * f.cols + c]]] <= 0) return false;
      if (bar && bar[r * f.cols + c]) return false;
    }
    return true;
  }

  // ---- The tick -----------------------------------------------------------------------------------------------------

  /** The squad brain, made the first time it is asked for (drill.ts). */
  drillOn(opts?: { sections?: boolean; platoons?: boolean; book?: boolean; life?: boolean; hatches?: boolean; scale?: boolean | number }): Drill {
    if (this.drill) return this.drill;
    const { scale, ...rest } = opts ?? {};
    // True: the battle's own N; a number: that N (a recording or a test that sets its own).
    const n = scale === true ? this.scale() : typeof scale === 'number' ? scale : undefined;
    return (this.drill = new Drill(this, n !== undefined ? { ...rest, scale: n } : rest));
  }

  /**
   * The battle's one N (GR1, LAND.scale): every man both sides bring (their blocks' men0, as the battle opens) over the
   * figure budget, rounded up to a multiple of the step; 1 when they fit the budget whole. Sedan, Normandy, Prokhorovka
   * and Kursk give 20, 20, 35 and 125. Looks only. menOf: a block's men as counted (a fogged field counts what the
   * player can know: the bridge's battleStart); a figure that is no number counts none. budget: the figures it is set
   * for (the 2D page's dots: LAND.scale.page). The blocks of corps still coming up from next door count too.
   */
  scale(menOf: (u: LandUnit) => number = (u) => u.men0, budget: number = LAND.scale.budget): number {
    // (Corps from next door count from the start: they are men the side brings, and their coming must not overflow it.)
    const S = LAND.scale, men = this.allBlocks().reduce((s, u) => { const m = menOf(u); return s + (Number.isFinite(m) ? Math.max(0, m) : 0); }, 0);
    const raw = men / budget;
    return raw <= 1 ? 1 : Math.ceil(raw / S.step) * S.step;
  }

  /**
   * Opens the setup step for one side (release 2): only before the first step. Its guns and support teams may then be
   * placed anywhere in its own zone (or where its own blocks were drawn up), on dry ground they can stand on, clear of
   * every other block; the computer's stand where they were drawn up. Every other order is taken as before the clock
   * starts. False if the battle has begun.
   */
  setupOn(side: Side, all = false): boolean {
    if (this.t > 0 || this.over) return false;
    // Every block of the side, not only its guns and teams (release 2: the player as the general places his army).
    this.placeAll = all;
    const z = this.field.zones[side];
    const mine = this.units.filter((u) => u.side === side);
    this.room = [
      Math.min(z[0], ...mine.map((u) => u.x - u.w / 2)), Math.min(z[1], ...mine.map((u) => u.y - u.d / 2)),
      Math.max(z[2], ...mine.map((u) => u.x + u.w / 2)), Math.max(z[3], ...mine.map((u) => u.y + u.d / 2)),
    ];
    this.setup = side;
    return true;
  }

  /** Where the placing side may put its guns and teams, [x0, y0, x1, y1] metres; null outside the setup step. */
  setupRoom(): [number, number, number, number] | null { return this.room ? [...this.room] as [number, number, number, number] : null; }

  /** Why a block may not stand there in the setup step, or null where it may (release 2). */
  placeWhy(u: LandUnit, x: number, y: number): string | null {
    if (!this.setup || u.side !== this.setup) return 'The battle has begun.';
    if (this.placeAll ? u.kind === 'fort' || !fighting(u) : u.kind !== 'art' || !fighting(u)) return this.placeAll ? 'Fortifications stay where they are built.' : 'Only guns and support teams are placed.';
    // (A block waiting for help is dug in where it was told to wait.)
    if (u.dug || u.waits) return 'It is dug in where it stands.';
    return this.standWhy(u, x, y, this.room!);
  }

  /** Why a block may not stand there inside its side's ground 'r' (the setup step's own checks), or null where it may. */
  private standWhy(u: LandUnit, x: number, y: number, r: [number, number, number, number]): string | null {
    if (!Number.isFinite(x) || !Number.isFinite(y)) return 'Off the field.';
    const hw = u.w / 2, hd = u.d / 2;
    if (x - hw < r[0] || x + hw > r[2] || y - hd < r[1] || y + hd > r[3]) return 'Outside your ground.';
    if (this.wet(u, x, y, false) || wetUnder(this.field, { ...u, x, y })) return 'Not on water, a bridge or ground it cannot stand on.';
    // A river bend: the attack's lanes either side of it are each the card's own (b464(5)); a block keeps to its lane.
    const B = this.field.bend;
    if (B && u.side === 'att' && (Math.sign(x - B.x) !== Math.sign(u.x - B.x) || Math.abs(x - B.x) < BEND.clear + hw)) return 'Its corps keeps to its own side of the river.';
    // A city's river up the field: the defence's line ends on it, so a block stays on the bank it was drawn up on.
    const St = this.field.stream;
    if (St?.lie === 'up') {
      const xAt = (yy: number): number => St.course.reduce((m, p) => (Math.abs(p[1] - yy) < Math.abs(m[1] - yy) ? p : m))[0];
      if (Math.sign(x - xAt(y)) !== Math.sign(u.x - xAt(u.y))) return 'It stays on its own bank of the river.';
    }
    // Clear of every block by a few metres, so neither is shoved off its place as they turn to fire (spread).
    for (const o of this.units) {
      if (o === u || !live(o)) continue;
      if (Math.abs(o.x - x) < (o.w + u.w) / 2 + LAND.placeClear && Math.abs(o.y - y) < (o.d + u.d) / 2 + LAND.placeClear) return 'Another block stands there.';
    }
    return null;
  }

  /** Puts a block where the setup step placed it: drawn up there, facing the enemy (or the way it was turned), with no order. */
  private place(u: LandUnit, x: number, y: number, face?: number): void {
    u.x = x; u.y = y; u.face = face ?? (u.side === 'att' ? 0 : Math.PI);
    u.order = null; u.path = []; u.keepFace = face ?? null;
    delete u.aim; delete u.search; delete u.searchR; delete u.stand; delete u.standFor; delete u.standFace; delete u.closing; delete u.cut;
    this.ways.delete(u.id);
    u.home = [x, y];
    u.cover = groundAt(this.field, x, y);
    // Placed in the woods, it will start hidden (release 2): shown so while he places.
    if (this.hide && !this.hidStarted) this.markHid(u);
    this.drill?.relay(u);
  }

  // ---- Measured armour (release 2; armour.ts) ----------------------------------------------------------------------

  /** The tank or gun model a block fights with in this battle's month, where the sheet knows it: null for the rest. */
  vehicle(u: LandUnit): string | null {
    if (!this.models.has(u.id)) this.models.set(u.id, u.team ? null : vehicleOf(u.kind, u.nat, this.ticket.turn));
    return this.models.get(u.id)!;
  }

  /**
   * Names the regiment blocks as their armies did (release 2; the historian's ruling, 22 November: units.ts
   * regimentWord): "Infanterie-Regiment 33" of the "11th Infantry Division", a British brigade, an American Combat
   * Command. Before the clock starts only; false once it has begun. Looks only: no number in the fight reads a name.
   */
  namesOn(): boolean {
    if (this.t > 0 || this.over) return false;
    for (const side of ['att', 'def'] as const) armyNames(this.ticket[side], this.units.filter((u) => u.side === side), this.ticket.turn);
    return true;
  }

  /**
   * The model a block is drawn with, where the 3D set piece picks its own (Normandy's first German tank block is a
   * Tiger): the rules weigh it as that model from then. False where the block or the model is unknown to the sheet (a
   * tank block needs a model the sheet has duels for); null hands it back to vehicleOf.
   */
  vehicleIs(id: number, model: string | null): boolean {
    const u = this.unit(id) ?? this.coming.find((c) => c.u.id === id)?.u;
    if (!u) return false;
    if (model === null) { this.models.delete(id); return true; }
    if (!ARMOUR.models[model] || (u.kind === 'arm' && !ARMOUR.duels[model])) return false;
    this.models.set(id, model);
    return true;
  }

  /** What a blow from u on tank block t weighs for t's armour on that face at that range (1 at par); null where unknown. */
  armourAt(u: LandUnit, t: LandUnit, face: ArmourFace, range: number): number | null {
    const s = this.vehicle(u), v = this.vehicle(t);
    return s && v ? armourWeight(s, v, face, range, this.ticket.turn, LAND.armour.lo, LAND.armour.hi) : null;
  }

  // ---- Hidden in the woods (release 2; Brian, 22 November 04:18Z; LAND.hide) -------------------------------------

  /**
   * Turns concealment on: before the clock starts only. Every block standing in woods shows as hidden at once (the setup
   * step shows his so as he places them); who is hidden is settled when the clock starts. False once it has begun.
   */
  hideOn(): boolean {
    if (this.t > 0 || this.over) return false;
    this.hide = true;
    // Sniper teams (release 2, Q6(2)): the player's start where his staff would put them, and he may move them before
    // the clock starts; the computer's take their cover as the clock starts (hideStart).
    for (const s of ['att', 'def'] as Side[]) if (this.ticket[s].human && this.units.some((u) => u.team === 'snp' && u.side === s)) this.laySnipers(s);
    for (const u of this.units) this.markHid(u);
    return true;
  }

  /** Whether a side can see a block: its own always; the enemy's unless it is hidden in the woods. */
  sees(side: Side, e: LandUnit): boolean { return e.side === side || !e.hid; }

  /**
   * The field's hedges (the historian's check, 22 November: the Normandy bocage): the hedge lines and their gates as the
   * 3D game lays them on this field, metres from its south-west corner. Before the clock starts only; a block on a hedge
   * line then hides as in woods (LAND.hide). False once it has begun.
   */
  hedgesOn(hedges: Hedge[], gates: Gate[] = []): boolean {
    if (this.t > 0 || this.over) return false;
    const W = this.field.w, H = this.field.h, C = HEDGE_CELL, near = LAND.hide.hedge.near;
    const cols = Math.ceil(W / C), rows = Math.ceil(H / C), map = new Uint8Array(cols * rows);
    const keep: Hedge[] = [];
    for (const h of hedges) {
      const pts = (h.points ?? []).filter((p) => Array.isArray(p) && Number.isFinite(p[0]) && Number.isFinite(p[1]));
      if (pts.length < 2 || !pts.some(([x, y]) => x >= 0 && x <= W && y >= 0 && y <= H)) continue;
      keep.push({ kind: h.kind, points: pts });
      for (let i = 1; i < pts.length; i++) {
        const [ax, ay] = pts[i - 1], [bx, by] = pts[i];
        const c0 = Math.max(0, Math.floor((Math.min(ax, bx) - near) / C)), c1 = Math.min(cols - 1, Math.floor((Math.max(ax, bx) + near) / C));
        const r0 = Math.max(0, Math.floor((Math.min(ay, by) - near) / C)), r1 = Math.min(rows - 1, Math.floor((Math.max(ay, by) + near) / C));
        for (let r = r0; r <= r1; r++) for (let c = c0; c <= c1; c++) if (segGap((c + 0.5) * C, (r + 0.5) * C, ax, ay, bx, by) <= near) map[r * cols + c] = 1;
      }
    }
    this.hedges = keep;
    this.gates = gates.filter((g) => Number.isFinite(g.x) && Number.isFinite(g.y) && g.x >= 0 && g.x <= W && g.y >= 0 && g.y <= H);
    this.hedgeMap = keep.length ? map : null;
    this.bars = null;
    this.hedgeSegs.clear();
    for (const h of keep) for (let i = 1; i < h.points.length; i++) {
      const [ax, ay] = h.points[i - 1], [bx, by] = h.points[i], seg = [ax, ay, bx, by];
      for (let c = Math.floor(Math.min(ax, bx) / GATE_CELL); c <= Math.floor(Math.max(ax, bx) / GATE_CELL); c++)
        for (let r = Math.floor(Math.min(ay, by) / GATE_CELL); r <= Math.floor(Math.max(ay, by) / GATE_CELL); r++) {
          const k = c * 4096 + r, l = this.hedgeSegs.get(k);
          if (l) l.push(seg); else this.hedgeSegs.set(k, [seg]);
        }
    }
    if (this.hide) for (const u of this.units) this.markHid(u);
    return true;
  }

  /**
   * Where a straight way from (x0, y0) to (x1, y1) first meets a hedge line, more than 'skip' metres out: null where it
   * meets none (the 3D game's men go through the gates: drill.ts).
   */
  hedgeCross(x0: number, y0: number, x1: number, y1: number, skip = 0): [number, number] | null {
    if (!this.hedgeSegs.size) return null;
    const len = Math.hypot(x1 - x0, y1 - y0);
    if (len <= skip) return null;
    let best: [number, number] | null = null, bestK = Infinity;
    const seen = new Set<number[]>();
    for (let c = Math.floor(Math.min(x0, x1) / GATE_CELL); c <= Math.floor(Math.max(x0, x1) / GATE_CELL); c++)
      for (let r = Math.floor(Math.min(y0, y1) / GATE_CELL); r <= Math.floor(Math.max(y0, y1) / GATE_CELL); r++)
        for (const s of this.hedgeSegs.get(c * 4096 + r) ?? []) {
          if (seen.has(s)) continue;
          seen.add(s);
          const k = segHit(x0, y0, x1, y1, s[0], s[1], s[2], s[3]);
          if (k !== null && k * len > skip && k < bestK) { bestK = k; best = [x0 + (x1 - x0) * k, y0 + (y1 - y0) * k]; }
        }
    return best;
  }

  /**
   * The gate a man going from (x0, y0) to (x1, y1) goes through, where his straight way meets a hedge (more than 'skip'
   * metres out): the gate nearest where it meets it, within GATE_REACH, that he can walk to straight. Null where his way
   * meets no hedge, or no gate serves: he climbs the bank there.
   */
  gateOn(x0: number, y0: number, x1: number, y1: number, skip = 0): Gate | null {
    const at = this.hedgeCross(x0, y0, x1, y1, skip);
    // A place on the hedge itself (a man lining it) is reached from either side: no gate.
    if (!at || Math.hypot(at[0] - x1, at[1] - y1) < 0.75) return null;
    const near = this.gates.map((g) => ({ g, d: Math.hypot(g.x - at[0], g.y - at[1]) })).filter((x) => x.d <= GATE_REACH).sort((a, b) => a.d - b.d);
    for (const { g } of near) {
      // Walkable straight to it: nothing in his way short of the gap itself.
      const to = this.hedgeCross(x0, y0, g.x, g.y, skip);
      if (!to || Math.hypot(to[0] - g.x, to[1] - g.y) <= LAND.hide.hedge.gap) return g;
    }
    return null;
  }

  /** The way the hedge line nearest (x, y) runs there, a unit vector: null where none passes within LAND.hide.hedge.near. */
  hedgeDir(x: number, y: number): [number, number] | null {
    let best = Infinity, dir: [number, number] | null = null;
    const c = Math.floor(x / GATE_CELL), r = Math.floor(y / GATE_CELL);
    for (let i = -1; i <= 1; i++) for (let j = -1; j <= 1; j++) for (const s of this.hedgeSegs.get((c + i) * 4096 + r + j) ?? []) {
      const l = Math.hypot(s[2] - s[0], s[3] - s[1]), d = segGap(x, y, s[0], s[1], s[2], s[3]);
      if (l > 0 && d < best) { best = d; dir = [(s[2] - s[0]) / l, (s[3] - s[1]) / l]; }
    }
    return best <= LAND.hide.hedge.near ? dir : null;
  }

  /** On a hedge line (within LAND.hide.hedge.near of one), where the field has hedges. */
  onHedge(x: number, y: number): boolean {
    if (!this.hedgeMap || x < 0 || y < 0 || x >= this.field.w || y >= this.field.h) return false;
    return this.hedgeMap[Math.floor(y / HEDGE_CELL) * Math.ceil(this.field.w / HEDGE_CELL) + Math.floor(x / HEDGE_CELL)] === 1;
  }

  /** Standing in woods (fieldworks dug into woods too): where a block can hide. */
  private inWoods(u: LandUnit): boolean { return coverAt(this.field, u.x, u.y) === 'woods'; }

  /** Cover a block can hide in: woods, or a hedge line; a sniper team also buildings, rubble and rough (LAND.snipers.cover). */
  private inCover(u: LandUnit): boolean { return this.inWoods(u) || this.onHedge(u.x, u.y) || (u.team === 'snp' && this.sniperCover(u.x, u.y)); }

  // ---- Sniper teams (release 2; Brian, 25 November 16:07Z; LAND.snipers, withSnipers; the design director's ruling of
  // 16:30Z, sniper teams Q1 to Q8) --------------------------------------------------------------------------------------
  //
  // Only where the switch put sniper teams on the field: nothing here runs, and no die is drawn, for any other block. Its
  // dice are its own, keyed by place (roll, salts 341-359), never the battle's, so on and off keep the battle's in step.

  /** Ground a sniper team hides on beyond woods and a hedge line (LAND.snipers.cover). */
  sniperCover(x: number, y: number): boolean { return LAND.snipers.cover.includes(coverAt(this.field, x, y)); }

  /** Hiding ground for a sniper team: its own cover, woods, or a hedge line. */
  sniperGround(x: number, y: number): boolean { return this.sniperCover(x, y) || coverAt(this.field, x, y) === 'woods' || this.onHedge(x, y); }

  /** Closed up now (LAND.closed): a tank block whose commanders are down inside, set by a rule whose switch is on. */
  closed(u: LandUnit): boolean { return u.closedTo !== undefined && this.t < u.closedTo; }

  /** A sniper team's die, keyed by its seed, the team, the tick and the salt (341-359). */
  private snpRoll(u: LandUnit, salt: number): number { return roll(this.ticket.seed, this.ticket.attempt ?? 0, u.id, Math.round(this.t / DT), salt); }

  /** The weight of a sniper team's blows on t (LAND.snipers.bite), 0 to 1: its striking weight now over t's power. */
  private bite(u: LandUnit, t: LandUnit): number {
    const r = (u.power0 * (0.35 + 0.65 * (u.str / u.str0))) / Math.max(1e-9, t.power0);
    return Math.min(1, r / LAND.snipers.bite);
  }

  /** A sniper team's choice of target, as a multiplier on targetOf's score (LAND.snipers.pick: lower first). */
  private sniperPick(e: LandUnit): number {
    const P = LAND.snipers.pick;
    if (e.state === 'routing') return P.routing;
    if (e.team === 'snp') return P.snp;
    if (e.team === 'mg') return P.mg;
    if (e.kind === 'arm') return this.closed(e) ? P.closed : P.open;
    return motionOf(e) === 'foot' && e.speed > 0.5 && e.order?.kind !== 'withdraw' && (e.pinSpent ?? 0) < LAND.pin.most ? P.moving : P.still;
  }

  /**
   * In woods, off a hedge line, a sniper team reaches beyond LAND.hide.forest only where its line to 'e' leaves the woods
   * within LAND.snipers.edge metres: it shoots from the edge.
   */
  private sniperSees(u: LandUnit, e: LandUnit, d: number): boolean {
    if (d <= LAND.hide.forest || !this.inWoods(u) || this.onHedge(u.x, u.y)) return true;
    const k = Math.hypot(e.x - u.x, e.y - u.y), step = Math.min(10, this.field.cell);
    if (k < 1e-6) return true;
    for (let r = step; r <= LAND.snipers.edge; r += step) if (coverAt(this.field, u.x + ((e.x - u.x) * r) / k, u.y + ((e.y - u.y) * r) / k) !== 'woods') return true;
    return false;
  }

  /**
   * Where a sniper team of the computer's goes next (ai.ts snipersUp): hiding ground within 'step' metres of it toward
   * 'to', never ahead of 'front' (its own leading foot, measured along its side's way), and at least 'keep' metres from
   * every found enemy block; the furthest forward first, then the nearest. Null where there is none.
   */
  sniperSpot(u: LandUnit, to: [number, number], front: number, step = 400, keep: number = LAND.snipers.keep): [number, number] | null {
    const dir = u.side === 'att' ? 1 : -1, cell = this.field.cell, k = Math.max(1e-6, Math.hypot(to[0] - u.x, to[1] - u.y));
    const foes = this.units.filter((e) => e.side !== u.side && live(e) && !e.hid);
    let best: [number, number] | null = null, score = -Infinity;
    for (let a = -step; a <= step; a += cell) for (let c = -step / 2; c <= step / 2; c += cell) {
      // Along the way to 'to', and across it.
      const x = Math.round(clamp(u.x + ((to[0] - u.x) * a) / k - ((to[1] - u.y) * c) / k, 5, this.field.w - 5));
      const y = Math.round(clamp(u.y + ((to[1] - u.y) * a) / k + ((to[0] - u.x) * c) / k, 5, this.field.h - 5));
      if (Math.hypot(x - u.x, y - u.y) > step || dir * y > dir * front || !this.sniperGround(x, y)) continue;
      if (foes.some((e) => dist(e.x, e.y, x, y) < keep)) continue;
      const sc = dir * y * 1000 - Math.hypot(x - u.x, y - u.y);
      if (sc <= score || this.wet(u, x, y, false)) continue;
      score = sc; best = [x, y];
    }
    return best;
  }

  /**
   * Where a sniper team slipping away goes (ai.ts snipersUp; LAND.snipers.slip): hiding ground 'least' to 'most' metres
   * from where it is, no nearer its own side's front than it is (never forward), no nearer any found enemy block than it
   * is now; the nearest first. Null where there is none: it crawls to another window where it is.
   */
  slipSpot(u: LandUnit, least = 30, most = 120): [number, number] | null {
    const dir = u.side === 'att' ? 1 : -1, cell = Math.min(10, this.field.cell);
    const foes = this.units.filter((e) => e.side !== u.side && live(e) && !e.hid);
    const now = foes.reduce((m, e) => Math.min(m, dist(e.x, e.y, u.x, u.y)), Infinity);
    let best: [number, number] | null = null, gap = Infinity;
    for (let dy = -most; dy <= 0; dy += cell) for (let dx = -most; dx <= most; dx += cell) {
      const r = Math.hypot(dx, dy);
      if (r < least || r > most || r >= gap) continue;
      const x = Math.round(clamp(u.x + dx, 5, this.field.w - 5)), y = Math.round(clamp(u.y + dir * dy, 5, this.field.h - 5));
      if (!this.sniperGround(x, y) || this.wet(u, x, y, false) || foes.some((e) => dist(e.x, e.y, x, y) < now)) continue;
      gap = r; best = [x, y];
    }
    return best;
  }

  /**
   * A sniper team fires from hiding this tick (LAND.snipers.find): no ambush sprung, no 'fired' at once. Found by odds
   * by its flash and its crack, less often the better its men; found, it is not yet located for find.locate seconds. Its
   * fire counts as an ambush for the struck side's staff, which searches the cover ahead.
   */
  private snipeFromHiding(u: LandUnit, t: LandUnit): void {
    const F = LAND.snipers.find;
    this.ambushed.add(t.side);
    let f = (this.gap(u, t) > F.near ? F.far : F.close) * (F.xp - u.xp);
    if (this.units.some((e) => e.team === 'snp' && e.side !== u.side && fighting(e) && this.gap(u, e) <= reachOf(u))) f *= F.hunt;
    f = clamp(f, 0, 1);
    if (this.snpRoll(u, 341) >= 1 - (1 - f) ** (DT / F.every)) return;
    this.reveal(u, 'fired', t);
    u.unloc = Math.round((this.t + F.locate) * 1000) / 1000;
  }

  /**
   * What a sniper team's blow does beyond its harm, by its bite b (LAND.snipers): its leaders shot, the block steadies on
   * xp x (1 - lead.xp b) for lead.secs; a block on foot goes to ground and crawls at pace x (1 - pin.pace b) for
   * pin.secs, until it has spent LAND.pin.most seconds so (never routing or falling back).
   */
  private sniped(u: LandUnit, t: LandUnit): void {
    const S = LAND.snipers, b = this.bite(u, t), at = (secs: number): number => Math.round((this.t + secs) * 1000) / 1000;
    const lead = 1 - S.lead.xp * b;
    t.leadK = t.leadTo !== undefined && this.t < t.leadTo ? Math.min(t.leadK ?? 1, lead) : lead;
    t.leadTo = at(S.lead.secs);
    if (motionOf(t) !== 'foot' || t.state === 'routing' || t.order?.kind === 'withdraw' || (t.pinSpent ?? 0) >= LAND.pin.most) return;
    const pin = 1 - S.pin.pace * b;
    t.pinK = t.pinTo !== undefined && this.t < t.pinTo ? Math.min(t.pinK ?? 1, pin) : pin;
    t.pinTo = at(S.pin.secs);
  }

  /** A sniper team at half strength or the rout nerve (LAND.snipers.back): it withdraws to its own edge, never routs. */
  private sniperBack(u: LandUnit): void {
    if (u.order?.kind === 'withdraw') return;
    this.send(u, { kind: 'withdraw', x: u.x, y: u.side === 'att' ? 0 : this.field.h });
    u.engaged = undefined;
    delete u.slipTo;
    delete u.search; delete u.searchR;
    if (this.ticket[u.side].human) this.say({ what: 'land:snipersBack', unit: u.id, side: u.side, text: `${this.named(u)} has lost too many men and pulls back.`, x: u.x, y: u.y });
  }

  /**
   * Each tick (LAND.snipers.slip): a sniper team found, on its hiding ground, unstruck for slip.after seconds, with no
   * enemy within slip.clear metres, slips away: it holds its fire slip.secs while it moves, then is hidden again if all
   * of that still holds (struck meanwhile, suffer calls it off and it stays found). At most slip.most a battle, none
   * below slip.str of its strength.
   */
  private snipersSlip(): void {
    const S = LAND.snipers.slip;
    for (const u of this.units) {
      if (u.team !== 'snp' || u.hid || !fighting(u) || u.order?.kind === 'withdraw') continue;
      const clear = (): boolean => this.sniperGround(u.x, u.y) && !this.units.some((e) => e.side !== u.side && fighting(e) && this.gap(u, e) <= S.clear);
      if (u.slipTo !== undefined) {
        if (this.t < u.slipTo - 1e-9) continue;
        delete u.slipTo;
        if (clear()) { u.hid = true; delete u.unloc; }
        continue;
      }
      if ((u.slips ?? 0) >= S.most || u.str < S.str * u.str0 || u.still < S.after || !clear()) continue;
      u.slipTo = Math.round((this.t + S.secs) * 1000) / 1000;
      u.slips = (u.slips ?? 0) + 1;
    }
  }

  /**
   * The clock starts (Q6(2)): a computer side's sniper teams take hiding ground, each to its own stretch of the line,
   * buildings, hedges and wood edges first. The defence's: the most forward spot about its leading foot (layAmbush's
   * reach of it, within its room) from which the attack's way in is within reach. The attack's: within LAND.snipers.behind metres behind its leading foot, where
   * the enemy's line is within reach; otherwise it stays. The player's stay where he placed them.
   */
  private laySnipers(side: Side): void {
    const S = LAND.snipers, H = LAND.hide.staff, dir = side === 'att' ? 1 : -1, z = this.field.zones[side];
    const own = this.units.filter((u) => u.side === side && live(u));
    const snp = own.filter((u) => u.team === 'snp' && !u.taken);
    const foot = own.filter((u) => !u.team && u.kind !== 'art' && u.kind !== 'fort');
    const foes = this.units.filter((u) => u.side !== side && live(u) && u.kind !== 'fort');
    if (!snp.length || !foot.length || !foes.length) return;
    const room: [number, number, number, number] = [
      Math.min(z[0], ...own.map((u) => u.x - u.w / 2)), Math.min(z[1], ...own.map((u) => u.y - u.d / 2)),
      Math.max(z[2], ...own.map((u) => u.x + u.w / 2)), Math.max(z[3], ...own.map((u) => u.y + u.d / 2)),
    ];
    // Its leading foot's front, along its side's way.
    const front = dir > 0 ? Math.max(...foot.map((u) => u.y + u.d / 2)) : Math.min(...foot.map((u) => u.y - u.d / 2));
    const x0 = room[0], span = Math.max(1, room[2] - room[0]), reach = LAND.teams.snp.reach;
    const cell = this.hedgeMap ? Math.min(this.field.cell, HEDGE_CELL) : this.field.cell;
    // In reach of what: the defence's, the attack's way in (its blocks where they stand and its march to the line); the
    // attack's, the enemy's line.
    const inReach = (x: number, y: number): boolean => foes.some((e) => Math.hypot(e.x - x, e.y - y) - (e.w + e.d) / 4 <= reach || (side === 'def' && Math.abs(e.x - x) <= reach && dir * (y - e.y) < 0 && Math.abs(y - e.y) - (e.d / 2) <= reach + 400));
    snp.sort((a, b) => a.x - b.x || a.id - b.id);
    snp.forEach((u, i) => {
      const cx = x0 + (span * (i + 0.5)) / snp.length, half = span / snp.length / 2;
      const [lo, hi] = side === 'def' ? [-H.back, H.ahead] : [-S.behind, 0];
      let best: { x: number; y: number; score: number } | null = null;
      for (let dy = lo; dy <= hi; dy += cell) for (let dx = -half; dx <= half; dx += cell) {
        const x = Math.round(cx + dx), y = Math.round(front + dir * dy);
        if (!this.sniperGround(x, y)) continue;
        if (!inReach(x, y)) continue;
        // The furthest forward; buildings, hedges and wood edges before plain cover; then the nearest its stretch's middle.
        const edge = this.sniperCover(x, y) || this.onHedge(x, y) || !this.inWoodsAt(x, y + dir * 30) ? 1 : 0;
        const score = edge * 1e6 + dir * y * 10 - Math.abs(dx) * 0.1;
        if (best && score <= best.score) continue;
        if (this.standWhy(u, x, y, room) !== null) continue;
        best = { x, y, score };
      }
      if (best) this.place(u, best.x, best.y);
    });
  }

  /** Where a sniper team hides, in words: the hedges, the houses, the ruins, the rocks. */
  private sniperWhere(u: LandUnit): string {
    if (this.onHedge(u.x, u.y)) return 'the hedges';
    const c = coverAt(this.field, u.x, u.y);
    return c === 'buildings' || c === 'suburb' || c === 'industry' ? 'the houses' : c === 'blown' ? 'the ruins' : c === 'rough' || c === 'rock' ? 'the rocks' : 'cover';
  }

  /** Woods at this point (a wood's edge is where the woods end a little ahead). */
  private inWoodsAt(x: number, y: number): boolean { return coverAt(this.field, clamp(x, 0, this.field.w - 1), clamp(y, 0, this.field.h - 1)) === 'woods'; }

  /** Hidden if it stands in woods or on a hedge line, before the clock starts. Fieldworks are known where they are built. */
  private markHid(u: LandUnit): void {
    if (live(u) && u.kind !== 'fort' && this.inCover(u)) u.hid = true;
    else delete u.hid;
  }

  /**
   * How close the enemy must come before a hidden block opens fire unbidden: Infinity where it never holds (guns). In
   * true forest, foot and machine guns hold to LAND.hide.forest: they could see no further.
   */
  private ambushAt(u: LandUnit): number {
    const H = LAND.hide, a = u.team === 'mg' ? H.ambush.mg : H.ambush[MOTION[u.kind]];
    if (a <= 0) return Infinity;
    return !this.onHedge(u.x, u.y) && this.inWoods(u) ? Math.min(a, H.forest) : a;
  }

  /**
   * An army's wait for an order by radio, as a multiple of LAND.radio.delay (the historian's, 22 November): its foot's by
   * nation and year; a tank block's as its model's share with a set, the rest as its foot.
   */
  radioOf(u: LandUnit): number {
    const year = turnToDate(this.ticket.turn).year;
    const foot = (LAND.radio.nations[u.nat] ?? []).find(([to]) => year <= to)?.[1] ?? 1;
    if (u.kind !== 'arm') return foot;
    const m = this.vehicle(u), r = m ? ARMOUR.models[m]?.radio : undefined;
    return r === undefined ? foot : r + (1 - r) * foot;
  }

  /** The clock starts: the computer's defence lays its ambushes, then every block in woods is hidden. */
  private hideStart(): void {
    this.hidStarted = true;
    // Only the computer's: the player's own staff never moves the blocks he drew up.
    if (this.ai.def && !this.ticket.def.human) this.layAmbush('def');
    // Sniper teams (release 2): the computer's take their cover first, then they hide with the rest.
    for (const s of ['att', 'def'] as Side[]) if (this.ai[s] && !this.ticket[s].human && this.units.some((u) => u.team === 'snp' && u.side === s)) this.laySnipers(s);
    for (const u of this.units) this.markHid(u);
    // Night patrol (release 2): settled now, with the posts laid and hidden, before either staff first thinks.
    if (this.patrol) this.patrolOut();
  }

  /**
   * The computer's defence puts a block or two of its line into woods on the enemy's way in, before the clock starts
   * (LAND.hide.staff): foot of its line (not its held reserve, nor a block dug in), each to the furthest-forward wood
   * within 'ahead' metres before it or 'back' behind it and 'across' to either side, inside its own ground as the
   * player's setup step allows his, clear of every other block. Where no woods lie near, none goes. 'most' blocks at
   * most, of those 'may' allows (a caught patrol's one more post: night patrol); the blocks it put there.
   */
  private layAmbush(side: Side, most: number = LAND.hide.staff.blocks, may: (u: LandUnit) => boolean = () => true): LandUnit[] {
    const H = LAND.hide.staff, dir = side === 'att' ? 1 : -1;
    const z = this.field.zones[side];
    const own = this.units.filter((u) => u.side === side && live(u));
    const room: [number, number, number, number] = [
      Math.min(z[0], ...own.map((u) => u.x - u.w / 2)), Math.min(z[1], ...own.map((u) => u.y - u.d / 2)),
      Math.max(z[2], ...own.map((u) => u.x + u.w / 2)), Math.max(z[3], ...own.map((u) => u.y + u.d / 2)),
    ];
    const foot = own.filter((u) => MOTION[u.kind] === 'foot' && !u.team && !u.dug && u.reserve !== 'held' && !u.taken && !this.inCover(u) && may(u));
    const picks: { u: LandUnit; x: number; y: number; ahead: number }[] = [];
    // Hedge lines are narrow: looked for a hedge square at a time.
    const cell = this.hedgeMap ? Math.min(this.field.cell, HEDGE_CELL) : this.field.cell;
    for (const u of foot) {
      let best: { x: number; y: number; ahead: number } | null = null;
      for (let dy = -H.back; dy <= H.ahead; dy += cell) for (let dx = -H.across; dx <= H.across; dx += cell) {
        const x = u.x + dx, y = u.y + dir * dy;
        if (coverAt(this.field, x, y) !== 'woods' && !this.onHedge(x, y)) continue;
        // The furthest forward, then the nearest across.
        if (best && (dy < best.ahead || (dy === best.ahead && Math.abs(dx) >= Math.abs(best.x - u.x)))) continue;
        if (this.standWhy(u, x, y, room) !== null) continue;
        best = { x, y, ahead: dy };
      }
      if (best) picks.push({ u, ...best });
    }
    // The furthest forward first, one to a stretch of the line.
    picks.sort((a, b) => b.ahead - a.ahead || a.u.id - b.u.id);
    const laid: LandUnit[] = [];
    for (const p of picks) {
      if (laid.length >= most) break;
      if (this.standWhy(p.u, p.x, p.y, room) !== null) continue;
      this.place(p.u, p.x, p.y);
      laid.push(p.u);
    }
    return laid;
  }

  /**
   * Found (release 2): no longer hidden from the other side, and known to the player if it is the enemy's (b464(2)).
   * 'fired': it opened fire on 'by'; 'seen': 'by' came close enough to see it; 'out': it left the woods; 'struck': hit
   * from the air; 'patrol': the night before, by the patrol 'by' sent out (night patrol, release 2), before any shot.
   * Told to the player as his blocks would report it.
   */
  private reveal(u: LandUnit, how: 'fired' | 'seen' | 'heard' | 'out' | 'struck' | 'searched' | 'patrol', by?: LandUnit): void {
    if (!u.hid) return;
    delete u.hid;
    // Found by its own fire, it is not yet located: the fire back strikes softer for a while (LAND.hide.locate).
    if (how === 'fired') u.unloc = Math.round((this.t + LAND.hide.locate.secs) * 1000) / 1000;
    const mineFound = this.ticket[u.side].human, theirs = this.ticket[other(u.side)].human;
    if (theirs) u.found = true;
    const where = this.inWoods(u) ? 'the woods' : u.team === 'snp' ? this.sniperWhere(u) : 'the hedges';
    // A sniper team found by its fire (release 2): its flash or its crack gave it away; it springs no ambush.
    const text = u.team === 'snp' && how === 'fired'
      ? mineFound ? `${this.named(u)} has been spotted by its fire in ${where}.` : theirs ? `${by ? `Your ${by.name} has` : 'We have'} spotted an enemy sniper in ${where}.` : ''
      : mineFound
      ? how === 'fired' ? `${this.named(u)} springs its ambush from ${where}.` : how === 'seen' ? `${this.named(u)} has been seen in ${where}.` : how === 'heard' ? `${this.named(u)} has been heard moving in ${where}.` : how === 'out' ? `${this.named(u)} has left cover and can be seen.` : how === 'searched' ? `${this.named(u)} is flushed out of ${where} by fire.` : how === 'patrol' ? `An enemy patrol found ${this.named(u)} in ${where} in the night.` : `${this.named(u)} is struck in ${where}: the enemy knows it is there.`
      : theirs
        ? how === 'fired' && by ? `Enemy troops hidden in ${where} open fire on your ${by.name}.` : (how === 'seen' || how === 'heard') && by ? `Your ${by.name} has found enemy troops in ${where}.` : how === 'searched' && by ? `Your ${by.name} has flushed enemy troops out of ${where}.` : how === 'patrol' ? 'Our night patrol found a hidden post.' : how === 'out' ? 'Enemy troops come out of cover.' : `Enemy troops found in ${where}.`
        : '';
    this.say({ what: 'land:found', unit: u.id, side: u.side, text, x: u.x, y: u.y, extra: { how, ...(by ? { by: by.id } : {}) } });
  }

  /**
   * Each tick: a hidden block that has left cover, or that an enemy has come close enough to see (or to hear: a tank or
   * lorry on the move), is found; one found by its fire is located once that long has passed or an enemy is that close.
   */
  private spotHidden(): void {
    const H = LAND.hide;
    for (const u of this.units) {
      const sight = H.sight[motionOf(u)];
      if (u.unloc !== undefined && (this.t >= u.unloc || !live(u) || this.units.some((e) => e.side !== u.side && fighting(e) && this.gap(u, e) <= sight))) delete u.unloc;
      if (!u.hid) continue;
      if (!live(u)) { delete u.hid; continue; }
      if (!this.inCover(u)) { this.reveal(u, 'out'); continue; }
      const heard = u.speed > 0.5 ? H.heard[motionOf(u)] : 0;
      // Smoke on call (release 2): nobody sees it through a screen standing between them (it may still be heard).
      const veiled = (e: LandUnit): boolean => this.smoke && this.screens.length > 0 && this.screened(e.x, e.y, u.x, u.y);
      // Closed up (LAND.closed), a tank block sees it only from nearer.
      const sees = (e: LandUnit): number => (e.closedTo !== undefined && this.closed(e) ? sight * LAND.closed.sight : sight);
      const by = this.units.find((e) => e.side !== u.side && fighting(e) && this.gap(u, e) <= Math.max(sees(e), heard) && (this.gap(u, e) <= heard || !veiled(e)));
      if (by) this.reveal(u, this.gap(u, by) <= sees(by) && !veiled(by) ? 'seen' : 'heard', by);
    }
  }

  // ---- Night patrol (release 2) -----------------------------------------------------------------------------------
  //
  // Brian, 22 November 17:33Z ("lets do all three but make it where we can toggle it off if someone doesnt want it");
  // the design director, 18:36Z, Q6 to Q8 (LAND.patrol). The night before, the attack sends one patrol out from a
  // block of its line on foot: the computer's always, from its most forward (doctrine: Q8); the player's only by his
  // order while he places his army (land:patrol). The defence never patrols. It needs hiding on (only hidden posts can
  // be found): it is settled once in hideStart, after the computer's ambushes are laid and every block in cover is
  // hidden and before either staff first thinks, so the attack's first plan counts what it found. Two draws on its own
  // stream ('patrol:' and the seed, and the attempt), always both and found first, never on the battle's dice: found,
  // the defence's most forward hidden post is found before any shot (it never springs its ambush); caught, the defence
  // is alerted, ready but not stronger (Q7, b418): one more of its blocks takes a hidden post by the computer's own
  // ambush rule (the player's defence too, laid for him and named in his warning), a computer defence's reserve starts
  // toward the attack at once as a bold one does (ai.ts defend; a player's moves his own), and the patrol's block loses
  // the patrol's men from its strength (counted as lost; its nerve untouched). The flares over it are looks only.

  /** Why this block may not go out on patrol tonight, in the player's words (129(d)); null where it may. */
  patrolWhy(u: LandUnit): string | null {
    if (!this.patrol) return 'There are no night patrols in this battle.';
    if (!this.hide) return 'Nothing hides in this battle, so there is nothing for a patrol to find.';
    if (u.side !== 'att') return 'Only the attacker sends out a patrol.';
    if (this.t > 0 || this.over || this.hidStarted) return 'The battle has begun: a patrol goes out the night before.';
    if (this.setup !== u.side) return 'A patrol is sent while you place your army, before the battle begins.';
    if (this.patrolBy !== null) return 'One patrol a night: one is already going out.';
    if (!fighting(u)) return 'It is out of the fight.';
    if (u.team || MOTION[u.kind] !== 'foot') return 'Only men on foot go out on patrol.';
    return null;
  }

  /** A side's computer defence alerted by the patrol it caught (night patrol): its reserve starts toward the attack at once (ai.ts). */
  alertedStaff(side: Side): boolean {
    return this.patrol && side === 'def' && !this.ticket.def.human && this.nightPatrol?.alerted === true;
  }

  /** The night's patrol goes out, and what it found and whether it was caught are settled (hideStart). */
  private patrolOut(): void {
    const P = LAND.patrol, side: Side = 'att', foe: Side = 'def';
    const foot = (u: LandUnit | undefined): u is LandUnit => !!u && u.side === side && fighting(u) && !u.team && MOTION[u.kind] === 'foot';
    // The computer's from its most forward block of the line on foot (ties by id); the player's only where he sent one.
    const by = this.ticket[side].human
      ? (this.patrolBy === null ? undefined : this.unit(this.patrolBy))
      : this.units.filter((u) => foot(u) && u.reserve !== 'held').sort((a, b) => b.y - a.y || a.id - b.id)[0];
    if (!foot(by)) return;
    const rng = new Rng(hashString(this.ticket.attempt ? `patrol:${this.ticket.seed}:${this.ticket.attempt}` : `patrol:${this.ticket.seed}`));
    const found = rng.next() < P.found, caught = rng.next() < P.caught;
    // The defence's most forward hidden post: the one nearest the attack's start, ties by id.
    const post = this.units.filter((u) => u.side === foe && live(u) && u.hid).sort((a, b) => a.y - b.y || a.id - b.id)[0];
    const men = patrolSize(by.nat);
    const r1 = (v: number): number => Math.round(v * 10) / 10;
    // Where it went: to the post it found, else to the enemy block nearest it; caught, it was caught there.
    // Never toward a post still hidden: the drawn walk and its flares would show where it lies (only blocks the attack can see).
    const near = found && post ? post : this.units.filter((u) => u.side === foe && live(u) && !u.hid).sort((a, b) => dist(a.x, a.y, by.x, by.y) - dist(b.x, b.y, by.x, by.y) || a.id - b.id)[0];
    const to: [number, number] = near ? [r1(near.x), r1(Math.max(0, near.y - near.d / 2 - 40))] : [r1(by.x), r1(by.y + by.d / 2 + 200)];
    // Its way in the night (looks only, for the 3D game's prelude): out from its block to 'to' by the middle of the night and
    // back by t1, through two points a little to one side (fixed by the seed, never a rule's stream).
    const from: [number, number] = [r1(by.x), r1(by.y + by.d / 2)];
    const w = (j: number): number => (mix(Math.imul(this.ticket.seed >>> 0, 0x85ebca6b) + j) - 0.5) * 60;
    const tm = Math.round((P.night.t0 + P.night.t1) / 2), at = (f: number, j: number): [number, number] => [r1(from[0] + (to[0] - from[0]) * f + w(j)), r1(from[1] + (to[1] - from[1]) * f)];
    const route: [number, number, number][] = [
      [P.night.t0, from[0], from[1]], [Math.round((P.night.t0 + tm) / 2), ...at(0.5, 1)], [tm, to[0], to[1]], [Math.round((tm + P.night.t1) / 2), ...at(0.5, 2)], [P.night.t1, from[0], from[1]],
    ];
    const night: NightPatrol = {
      side, unit: by.id, men, t0: P.night.t0, t1: P.night.t1, from, to, route,
      found: null, alerted: caught, post: null, lost: 0, flares: [],
    };
    this.nightPatrol = night;
    if (found && post) {
      night.found = { unit: post.id, x: r1(post.x), y: r1(post.y), kind: post.team ?? post.kind };
      // Found before any shot: it never springs its ambush.
      this.reveal(post, 'patrol', by);
    }
    if (caught) {
      // (a) One more hidden post, by the computer's own ambush rule: a block the player has ordered stays where he put it.
      const laid = this.layAmbush(foe, 1, (u) => !u.order);
      for (const u of laid) this.markHid(u);
      const extra = laid.find((u) => u.hid);
      if (extra) night.post = extra.id;
      // (c) The patrol's men are lost to its block: its strength, never its nerve.
      const lost = Math.min(by.str, (men * by.str0) / Math.max(1, by.men0));
      by.str -= lost;
      by.figures = by.str <= 0 ? 0 : Math.max(1, Math.ceil(by.tray * by.str));
      night.lost = lost;
      // The flares the defence put up over it: three, between t0 and t1, near where it was caught (looks only).
      const k = (i: number, j: number): number => mix(Math.imul(this.ticket.seed >>> 0, 0x9e3779b1) + i * 8 + j);
      for (let i = 0; i < 3; i++) {
        const t = Math.round(P.night.t0 + (P.night.t1 - P.night.t0) * ((i + k(i, 1)) / 3));
        night.flares.push([t, r1(Math.min(this.field.w, Math.max(0, to[0] + (k(i, 2) - 0.5) * 240))), r1(Math.min(this.field.h, Math.max(0, to[1] + (k(i, 3) - 0.5) * 120)))]);
      }
      // (b) is the computer's staff's (alertedStaff). The defence is told, the player's own with where his new post went.
      if (this.ticket[foe].human) {
        const where = extra ? ` ${extra.name} has taken a hidden post in ${this.inWoods(extra) ? 'the woods' : 'the hedges'}.` : '';
        this.say({ what: 'land:patrol', side: foe, ...(extra ? { unit: extra.id } : {}), text: `We caught an enemy patrol in the night. An attack is coming.${where}`, x: extra ? r1(extra.x) : to[0], y: extra ? r1(extra.y) : to[1], extra: { caught: true, ...(extra ? { post: extra.id } : {}) } });
      }
    }
    // The attack is told what came of it (found: the post's own land:found, above).
    if (this.ticket[side].human && (caught || !found || !post)) {
      const text = caught ? 'Our patrol was caught in the night. They know we\'re coming.' : 'Our patrol came back with nothing to report.';
      this.say({ what: 'land:patrol', side, unit: by.id, text, x: to[0], y: to[1], extra: { caught, found: !!night.found } });
    }
  }

  /** The night's patrol as a side sees it: the attack all of it; the defence only once alerted, where it caught it and the flares. */
  patrolView(as?: Side): Pick<BattleView, 'patrol'> {
    const p = this.nightPatrol;
    if (!this.patrol || !p) return {};
    const flares = p.flares.map((f): [number, number, number] => [f[0], f[1], f[2]]);
    if (as === undefined || as === p.side) {
      return { patrol: { side: p.side, unit: p.unit, t0: p.t0, t1: p.t1, men: p.men, from: [p.from[0], p.from[1]], to: [p.to[0], p.to[1]], route: p.route.map((r): [number, number, number] => [r[0], r[1], r[2]]), found: p.found ? { x: p.found.x, y: p.found.y, kind: p.found.kind } : null, alerted: p.alerted, flares } };
    }
    return p.alerted ? { patrol: { side: p.side, t0: p.t0, t1: p.t1, men: p.men, to: [p.to[0], p.to[1]], found: null, alerted: true, flares } } : {};
  }

  step(): void {
    if (this.setup) return;
    if (this.over) { this.drill?.step(); return; }
    // Hidden in the woods (release 2): as the clock starts, the computer's defence lays its ambushes and every block
    // then standing in woods is hidden from the other side.
    if (this.hide && !this.hidStarted) this.hideStart();
    // Field telephone lines (release 2): the wires laid as the clock starts, before any joiner comes on; mends due.
    if (this.phones) this.phonesOn();
    // Corps from next door whose time has come walk on (release 2): seen by both staffs this same tick.
    if (this.coming.length && this.t >= this.coming[0].at - 1e-9) this.arrive();
    // The player's orders on the air that have reached their blocks (direct command, release 2).
    this.landOrders();
    // Hold until relieved (release 2): who is relieved, and whose help is overdue, before the staffs think.
    if (this.relief && this.joinCorps.size) this.waitOn();
    if (this.t >= this.nextThink) {
      this.nextThink = this.t + LAND.think;
      this.staffing = true;
      for (const s of ['att', 'def'] as Side[]) if (this.ai[s]) thinkFor(this, s);
      this.staffing = false;
    }
    for (const u of this.units) { u.pinned = false; if (live(u)) { u.still += DT; u.clear += DT; } }
    // Blows first, from where everyone stands, then nerve, then movement: no block gets to strike first by its place in the list.
    const hurt = new Map<number, { dmg: number; nerve: number; from: [LandUnit, number][]; blind: boolean }>();
    // What blows held back this tick for air strikes, by the block they fell on (air support).
    const kept = this.airOn.att || this.airOn.def ? new Map<number, number>() : null;
    for (const u of this.units) {
      if (!fighting(u)) continue;
      const t = this.targetOf(u);
      u.engaged = t?.id;
      if (!t) {
        // Firing on cover where nobody is (reconnaissance by fire) still gives a hidden shooter away.
        if (u.hid && u.search && this.hide) this.reveal(u, 'fired');
        continue;
      }
      // Only fire on cover strikes a block still hidden (targetOf): it lands blind (LAND.hide.search).
      const blind = !!t.hid;
      if (this.contactAt === null && u.kind !== 'art') this.contactAt = this.t;
      // A hidden block that opens fire gives itself away, and its first fire is the ambush's (release 2, LAND.hide).
      if (u.hid && u.team === 'snp') this.snipeFromHiding(u, t);
      else if (u.hid) { u.sprung = this.t; this.reveal(u, 'fired', t); }
      // An ambushed side shoots at the cover ahead from then on (the historian: "shooting constantly at the hedgerows").
      if (u.sprung === this.t) this.ambushed.add(t.side);
      // He knows a block one of his has fought, his guns' target too, and one of theirs that has struck his in contact;
      // their guns firing from far back are not contact (b464(2)). Fire on cover finds nothing yet.
      if (this.ticket[u.side].human || u.kind !== 'art' || this.gap(u, t) < LAND.guns.closeAt) { u.found = true; if (!blind) t.found = true; }
      if (!u.met) {
        u.met = true;
        // A hidden sniper team's first shot (the design director's sniper ruling Q7, guard 3): the player it falls on
        // hears of it where his block is struck, never where the shooter lies, and the dispatch names nothing of him,
        // with the fog of war on or off (hidden is hidden).
        const unseen = u.team === 'snp' && u.hid && !this.ticket[u.side].human && this.ticket[t.side].human;
        if (u.side === 'att' || !t.met) {
          if (unseen) this.say({ what: 'contact', side: u.side, text: `${this.ticket[u.side].adj} troops are attacking your ${t.name}.`, x: t.x, y: t.y });
          else this.say({ what: 'contact', unit: u.id, side: u.side, text: `${this.named(u)} is in action.`, x: u.x, y: u.y });
        }
      }
      // A sniper team's shot at a tank block is at her commanders' heads: no blow, no harm, no hold on her (Q3); she
      // closes up (LAND.closed). It draws no die.
      if (u.team === 'snp' && t.kind === 'arm') { if (!blind) t.closedTo = Math.round((this.t + LAND.closed.secs) * 1000) / 1000; continue; }
      const blow = this.blow(u, t);
      // Field telephone lines (release 2): a gun's or a mortar team's blow may cut the wire to the block it falls on.
      if (this.phones && this.wires.length) this.wireBlow(u, t);
      if (blind) {
        const F = LAND.hide.search;
        blow.dmg *= F.blow;
        if (this.rng.next() < 1 - (1 - F.odds) ** (DT / F.every)) this.reveal(t, 'searched', u);
      }
      // A side with air strikes still to call holds back the card's share for them, owed to the block it fell on, in
      // what it would be on open ground (air support, below). Held back from the blow as it would be with no smoke
      // screen across it: a screen spoils the ground's aim, not the air's (smoke on call, ruling 242 S1-A), so only the
      // part fired from the ground now is struck at the screen's share.
      if (this.airLeft[u.side] > 0) {
        const full = blow.dmg / blow.screen, held = full * this.airOn[u.side]!.share, raw = held / blow.ground;
        blow.dmg = (full - held) * blow.screen;
        this.airBank[u.side].set(t.id, (this.airBank[u.side].get(t.id) ?? 0) + raw);
        this.airLedger[u.side].withheld += raw;
        kept!.set(t.id, (kept!.get(t.id) ?? 0) + raw);
      }
      const h = hurt.get(t.id) ?? { dmg: 0, nerve: 0, from: [], blind: true };
      h.dmg += blow.dmg; h.nerve += blow.nerve;
      if (!blind) h.blind = false;
      // Who struck it this tick, and how hard: whose blow a lost model goes to (suffer; shells.ts). Never read by the fight.
      h.from.push([u, blow.dmg]);
      hurt.set(t.id, h);
      if (blow.front) t.pinned = true;
      if (u.team === 'snp' && !blind) this.sniped(u, t);
      if (blow.aspect !== 'front' && this.t - t.flankedAt > 20) {
        t.flankedAt = this.t;
        if (t.side === 'att' ? this.ticket.att.human : this.ticket.def.human) this.say({ what: 'flanked', unit: t.id, side: t.side, text: `${this.named(t)} is hit from the ${blow.aspect === 'rear' ? 'rear' : 'flank'}.`, x: t.x, y: t.y });
      }
    }
    // The nerve what was held back would have cost, as the tick's blows on that block cost it, owed with it.
    if (kept) for (const [id, raw] of kept) { const h = hurt.get(id)!, side = other(this.unit(id)!.side); this.airNerve[side].set(id, (this.airNerve[side].get(id) ?? 0) + raw * h.nerve); }
    for (const [id, h] of hurt) this.suffer(this.unit(id)!, h.dmg, h.nerve, h.from, h.blind);
    // What waits' cover spared, being paid out at the pace it was spared (hold until relieved, release 2).
    if (this.repay.length) this.repayNow();
    if (this.airOn.att || this.airOn.def) this.flyAir();
    // Stretcher bearers (release 2): bearers home and light cases back in the line, then bearers out to the wounded.
    if (this.bearers) this.carry();
    for (const u of this.units) if (live(u)) this.nerve(u);
    // Hedge cutters (release 2): where each block stood before it moved, to find where its way crossed a hedge.
    const at = (): Map<number, [number, number]> => new Map(this.units.map((u): [number, [number, number]] => [u.id, [u.x, u.y]]));
    const before = this.cutters && this.hedges.length ? at() : null;
    // Field telephone lines (release 2): where each enemy tank block stood before it moved, to find a wire its way crossed.
    const tanks = this.phones && this.wires.length ? this.units.filter((u) => u.side === 'att' && u.kind === 'arm' && live(u)).map((u): [LandUnit, number, number] => [u, u.x, u.y]) : null;
    if (this.escort) this.overruns();
    for (const u of this.units) if (live(u)) this.move(u);
    // And where its own move took it, before the blocks are eased apart: a shove is no way of its own.
    const moved = before ? at() : null;
    this.spread();
    if (before) this.cutOn(before, moved!);
    if (tanks) this.wireTanks(tanks);
    if (this.hide) this.spotHidden();
    if (this.hide && (this.snpOn ??= this.allBlocks().some((u) => u.team === 'snp'))) this.snipersSlip();
    this.t = Math.round((this.t + DT) * 1000) / 1000;
    // Cuts whose time is up stand open as gaps, before the drill's men walk this tick (hedge cutters, release 2).
    if (this.cuts.length) this.openCuts();
    this.judge();
    this.drill?.step();
  }

  /**
   * Nobody stands in anybody else: where two blocks' ground overlaps they are eased apart along the shallower way, a
   * few metres a second, onto ground they can stand on. Blocks side by side in a line, or one behind the other, are
   * left alone; enemies keep a few metres between their fronts. For the screens and the models as much as for the
   * fight: two divisions never share one patch of ground.
   */
  private spread(): void {
    const us = this.units.filter((u) => fighting(u) && u.kind !== 'fort');
    // Never eased onto ground it can hardly cross (into the river off a bridge, tanks into a marsh), unless it stands on such
    // already; nor into the river off its bank, where foot could wade but would be left standing (b464(5)).
    const can = (u: LandUnit, ux: number, uy: number, push: number): boolean => {
      const x = clamp(u.x + ux * push, 5, this.field.w - 5), y = clamp(u.y + uy * push, 5, this.field.h - 5), pace = LAND.pace[motionOf(u)];
      const there = groundAt(this.field, x, y), here = groundAt(this.field, u.x, u.y);
      if (inRiver(there) && !inRiver(here)) return false;
      const to = pace[there];
      // The bocage (C1-B): a tank block without cutters is never eased over a hedge line but at a gap.
      if (this.bocage && this.going(u) === 'hedged' && this.barredCross(u.x, u.y, x, y)) return false;
      return to > LAND.queue.hard || (to > 0 && to >= pace[here]);
    };
    const go = (u: LandUnit, ux: number, uy: number, push: number): void => {
      if (can(u, ux, uy, push)) { u.x = clamp(u.x + ux * push, 5, this.field.w - 5); u.y = clamp(u.y + uy * push, 5, this.field.h - 5); }
    };
    for (let i = 0; i < us.length; i++) for (let j = i + 1; j < us.length; j++) {
      const a = us[i], b = us[j];
      const dx = b.x - a.x, dy = b.y - a.y;
      if (Math.abs(dx) > 300 || Math.abs(dy) > 300) continue;
      // In a's own frame: across its front, and along its depth. b's box as it lies in that frame.
      const [fx, fy] = ahead(a.face), [rx, ry] = ahead(a.face + Math.PI / 2);
      const lat = dx * rx + dy * ry, dep = dx * fx + dy * fy;
      const c = Math.abs(Math.cos(b.face - a.face)), s = Math.abs(Math.sin(b.face - a.face));
      const pl = a.w / 2 + (b.w / 2) * c + (b.d / 2) * s - Math.abs(lat);
      const pd = a.d / 2 + (b.w / 2) * s + (b.d / 2) * c + (a.side === b.side ? 0 : LAND.apart) - Math.abs(dep);
      if (pl <= 0 || pd <= 0) continue;
      // A tank block driving through a foot block's ground (overrun) is not eased off it.
      if (a.side !== b.side && this.inOverrun(a, b)) continue;
      // A queue, not a pile (the art director, b418, b439): a block marching into a friend's ground has it make way,
      // away from it by whichever way the ground lets it, so the one goes on and the other gives ground and comes on
      // behind, each on its own ground; at a bridge or a pass as everywhere. Two marching into each other: the one with
      // less to go goes on. A friend that cannot give way at all, the marching block steps round.
      if (a.side === b.side && (this.narrows(a, a.x, a.y) || this.narrows(b, b.x, b.y))) {
        const into = (u: LandUnit, ox: number, oy: number): boolean => u.pressing !== undefined && ox * Math.sin(u.pressing) + oy * Math.cos(u.pressing) > 0;
        const pa = into(a, dx, dy), pb = into(b, -dx, -dy);
        if (pa || pb) {
          const goA = pa && (!pb || this.toGo(a) < this.toGo(b) || (this.toGo(a) === this.toGo(b) && a.id < b.id));
          const mover = goA ? a : b, y = goA ? b : a, sy = goA ? 1 : -1;
          const p = Math.min(Math.min(pl, pd), (mover.speed + LAND.spread) * DT);
          const sl = Math.sign(lat) || 1, sd = Math.sign(dep) || 1, d1 = Math.hypot(dx, dy) || 1;
          const ways: [number, number][] = pl < pd ? [[rx * sl, ry * sl], [fx * sd, fy * sd]] : [[fx * sd, fy * sd], [rx * sl, ry * sl]];
          ways.push([dx / d1, dy / d1]);
          const w = ways.find(([wx, wy]) => can(y, wx * sy, wy * sy, p));
          if (w) { go(y, w[0] * sy, w[1] * sy, p); y.yieldTo = mover.id; continue; }
          if (can(mover, -rx * sl * sy, -ry * sl * sy, p)) { go(mover, -rx * sl * sy, -ry * sl * sy, p); continue; }
        }
      }
      // In the open, a block on the march slides a friend in its way aside as fast as it comes on, and slides aside
      // itself, so the two pass side by side and no box is drawn on another (the art director, b439).
      if (a.side === b.side && (a.pressing !== undefined || b.pressing !== undefined) && !this.narrows(a, a.x, a.y) && !this.narrows(b, b.x, b.y)) {
        const sl = Math.sign(lat) || (a.id < b.id ? 1 : -1), p = Math.min(pl / 2, (Math.max(a.speed, b.speed) + LAND.spread) * DT);
        if (can(a, -rx * sl, -ry * sl, p) && can(b, rx * sl, ry * sl, p)) {
          go(a, -rx * sl, -ry * sl, p); go(b, rx * sl, ry * sl, p);
          // Two on the march into each other: the one with further to go waits a moment for the other to pass.
          if (a.pressing !== undefined && b.pressing !== undefined) { const wa = this.toGo(a) > this.toGo(b) || (this.toGo(a) === this.toGo(b) && a.id > b.id); (wa ? a : b).yieldTo = (wa ? b : a).id; }
          continue;
        }
      }
      // Side by side where neither can step aside (on a bridge, in a gap): one behind the other instead.
      let along = pl < pd;
      if (along) { const sl = Math.sign(lat) || (a.id < b.id ? 1 : -1), p = Math.min(pl / 2, LAND.spread * DT); if (!can(a, -rx * sl, -ry * sl, p) && !can(b, rx * sl, ry * sl, p)) along = false; }
      const sg = Math.sign(along ? lat : dep) || (a.id < b.id ? 1 : -1);
      const ux = (along ? rx : fx) * sg, uy = (along ? ry : fy) * sg;
      const push = Math.min((along ? pl : pd) / 2, LAND.spread * DT);
      // A block wading the river is never put back into it by an enemy on the bank: the enemy gives the room (b464(5)).
      const wa = a.side !== b.side && inRiver(groundAt(this.field, a.x, a.y)), wb = a.side !== b.side && inRiver(groundAt(this.field, b.x, b.y));
      if (!wa) go(a, -ux, -uy, wb ? 2 * push : push);
      if (!wb) go(b, ux, uy, wa ? 2 * push : push);
    }
  }

  /** Who this block strikes: its ordered target if in reach, else the enemy in reach most in front of it. */
  targetOf(u: LandUnit): LandUnit | null {
    const marching = u.order && u.order.kind !== 'attack' && u.path.length > 0;
    if (u.kind === 'art' && marching) return null;
    if (u.order?.kind === 'withdraw') return null;
    let reach = reachOf(u);
    // A sniper team under fire, or slipping away, holds its fire (LAND.snipers.quiet, slip).
    const snp = u.team === 'snp';
    if (snp && ((u.quietTo !== undefined && this.t < u.quietTo) || u.slipTo !== undefined)) return null;
    // Guns told to fire on one block strike it whenever it is in their reach (ai.ts; ruling 239(6)).
    if (u.aim !== undefined) { const a = this.unit(u.aim); if (a && live(a) && a.side !== u.side && !a.hid && this.gap(u, a) <= reach) return a; }
    // Hidden in the woods (release 2): it holds its fire till the enemy is close, unless it is sent in. A sniper team
    // fires from hiding at its whole reach, and may be found for it (snipeFromHiding).
    if (u.hid && u.order?.kind !== 'attack' && !snp) reach = Math.min(reach, this.ambushAt(u));
    let best: LandUnit | null = null, score = Infinity;
    for (const e of this.units) {
      // A block hidden from its side is never struck: nobody there knows it is there.
      if (e.side === u.side || !live(e) || e.hid) continue;
      const d = this.gap(u, e);
      if (d > reach || (snp && !this.sniperSees(u, e, d))) continue;
      if (u.order?.kind === 'attack' && u.order.target === e.id) return e;
      const off = Math.abs(turn(u.face, bearing(u.x, u.y, e.x, e.y)));
      const s = d * (1 + off) * (snp ? this.sniperPick(e) : e.state === 'routing' ? 2 : 1);
      if (s < score) { score = s; best = e; }
    }
    // Firing on cover (reconnaissance by fire): nothing else in reach, it strikes blind at what hides near that point.
    if (!best && u.search && this.hide) {
      const [sx, sy] = u.search, R = u.searchR ?? LAND.hide.search.reach;
      for (const e of this.units) {
        if (e.side === u.side || !live(e) || !e.hid || dist(e.x, e.y, sx, sy) > R + Math.max(e.w, e.d) / 2) continue;
        const d = this.gap(u, e);
        if (d <= reach && d < score) { score = d; best = e; }
      }
    }
    return best;
  }

  /**
   * How far a tank or lorry block stands ahead of its group's slowest foot block, toward the enemy (going in together);
   * null where the group has no foot left fighting.
   */
  private aheadOfFoot(u: LandUnit, group: number): number | null {
    const dir = u.side === 'att' ? 1 : -1;
    let slow: number | null = null;
    for (const f of this.units) {
      if (f.side !== u.side || !fighting(f) || MOTION[f.kind] !== 'foot' || f.order?.together !== group) continue;
      const p = dir * f.y;
      if (slow === null || p < slow) slow = p;
    }
    return slow === null ? null : dir * u.y - slow;
  }

  /**
   * Tanks alone (the tactics panel, item 7; LAND.alone): what a tank block takes of this foot block's blow for having
   * left its own infantry on close ground, or against foot in fieldworks: the card's own figure for tanks alone there,
   * or 1. Only a tank block the card counted as covered by its foot (covered), never one it already charged as alone.
   */
  alone(t: LandUnit, u: LandUnit): number {
    const A = LAND.alone;
    if (!this.covered.has(t.id) || !A.foot.includes(u.kind)) return 1;
    const g = coverAt(this.field, t.x, t.y), gu = groundAt(this.field, u.x, u.y);
    const works = A.works.includes(gu) || (u.dug && dist(u.x, u.y, u.home[0], u.home[1]) <= LAND.dug.keep);
    const kind = A.streets.includes(g) ? 'streets' : A.woods.includes(g) ? 'woods' : works ? 'works' : null;
    if (!kind) return 1;
    const foot = (f: LandUnit): boolean => fighting(f) && A.foot.includes(f.kind);
    // The foot it fights is already held by friendly foot: fixed, and the tank is on its flank.
    if (this.units.some((f) => f.side === t.side && foot(f) && (f.engaged === u.id || u.engaged === f.id))) return 1;
    // Friendly foot beside it.
    if (this.units.some((f) => f.side === t.side && foot(f) && this.gap(t, f) <= A.near)) return 1;
    this.bitten.add(t.id);
    t.aloneAt = this.t;
    return A.take[t.side][kind];
  }

  /** The player's group attacks' 'together' groups, numbered above the computer's (ai.ts uses block ids and 100000). */
  private groupIds = 200000;
  /** Tank blocks the card counted as covered by their foot (item 7): set when the armies are made. */
  private covered = new Set<number>();
  /** Tank blocks bitten for being alone this battle, for the tests and the 3D game's mark. */
  readonly bitten = new Set<number>();
  /** A bold defence's held blocks walking behind their line (ai.ts walk()): the x each was last sent to. */
  readonly shifted = new Map<number, number>();
  /** Whether the player has been told of the enemy reserve's walk (once a battle). */
  private toldShift = false;

  /**
   * The enemy's reserve walking behind its line toward the stretch one of the player's blocks is coming at (ai.ts; ruling
   * 239's rider, 03:20Z): told once, as that block of his reports what it sees, never naming the enemy's (b464's fog
   * rule). Words: the UI director's (land-slice-3-review.md, 20 Nov 05:02).
   */
  reserveSeen(side: Side, toward: LandUnit): void {
    if (this.toldShift || toward.side === side || !this.ticket[toward.side].human) return;
    this.toldShift = true;
    this.say({ what: 'land:reserve', unit: toward.id, side: toward.side, text: `Your ${this.named(toward)} reports movement behind the enemy line, shifting toward it.`, x: toward.x, y: toward.y });
  }

  // ---- Overrun (release 2) -------------------------------------------------------------------------------------------
  //
  // Brian, 23 November 05:05Z: "vechiles can run over enemy soldiers though which would kill them"; the design director,
  // 05:48Z: through the block's ground to its far edge and back, once per foot block per contact, open ground only, no
  // enemy weapon that can hurt a tank in its own direct-fire reach of the way, the foot's own anti-tank weapon counted,
  // harm unchanged. LAND.overrun.

  /** Whether one of the two is driving through the other's ground. */
  inOverrun(a: LandUnit, b: LandUnit): boolean {
    return a.ovr?.on === b.id || b.ovr?.on === a.id;
  }

  /** Whether a block's foot carries its own anti-tank weapon in the battle's month (LAND.overrun.at, by its army). */
  private ownAntiTank(u: LandUnit): boolean {
    // ("Units decide": what its corps carries, where the campaign says; else the history start's, by its army and month)
    if (u.kit?.at !== undefined) return u.kit.at;
    const O = LAND.overrun, d = turnToDate(this.ticket.turn), when = d.year + d.month / 12;
    const nat = O.commonwealth.includes(u.nat) ? 'UK' : u.nat;
    return (O.at[nat] ?? []).some(([from, to]) => when >= from && when < to);
  }

  /**
   * How far an enemy block's weapons that can hurt a tank reach by direct fire (null: none): a tank block's reach, a
   * gun's over open sights, a fort's, foot with its own anti-tank weapon its reach. Support teams' bullets and bombs: none.
   */
  private tankReach(e: LandUnit): number | null {
    if (e.team) return null;
    if (e.kind === 'arm' || e.kind === 'fort') return reachOf(e);
    if (e.kind === 'art') return gunsOf(e).closeAt;
    if (LAND.alone.foot.includes(e.kind) && this.ownAntiTank(e)) return reachOf(e);
    return null;
  }

  /**
   * Whether an enemy weapon that can pierce a hull reaches a block now (tankReach: a tank's, a gun's over open sights, a
   * fort's, foot with its own anti-tank weapon), the overrun's threat test. Looks only for the drill: armoured infantry
   * aboard their half-tracks ride on till one does (the design director, 23 November 07:06Z(2); Drill.think).
   */
  hullThreat(u: LandUnit): boolean {
    for (const e of this.units) {
      if (e.side === u.side || !fighting(e)) continue;
      const r = this.tankReach(e);
      if (r !== null && this.gap(u, e) <= r) return true;
    }
    return false;
  }

  /** The tank blocks that may, begin their overruns (each tick, before the blocks move). */
  private overruns(): void {
    const foot = (f: LandUnit): boolean => fighting(f) && LAND.alone.foot.includes(f.kind);
    // A foot block overrun in this contact may be overrun again once the contact is over: broken, or no enemy tank in reach.
    for (const [id] of this.overrun) {
      const f = this.unit(id);
      if (!f || !fighting(f) || !this.units.some((e) => e.side !== f.side && e.kind === 'arm' && fighting(e) && this.gap(e, f) <= reachOf(e))) this.overrun.delete(id);
    }
    for (const u of this.units) {
      if (u.ovr || u.kind !== 'arm' || u.team || !fighting(u) || u.state === 'wavering' || u.gives || u.fellBack || u.engaged === undefined) continue;
      if (u.order && u.order.kind !== 'attack') continue;
      const f = this.unit(u.engaged);
      if (!f || f.side === u.side || !foot(f) || f.hid || this.overrun.has(f.id) || this.gap(u, f) > reachOf(u)) continue;
      // The foot in the open: not dug in where it began (fieldworks), nor on a hedge; its own anti-tank weapon none.
      if ((f.dug && dist(f.x, f.y, f.home[0], f.home[1]) <= LAND.dug.keep) || this.ownAntiTank(f)) continue;
      // Its own foot beside it (LAND.alone.near): tanks never go in alone.
      if (!this.units.some((g) => g.side === u.side && foot(g) && this.gap(u, g) <= LAND.alone.near)) continue;
      // The way: from where it stands through to the foot's far edge, every step of it open ground its tracks can take.
      const dx = f.x - u.x, dy = f.y - u.y, len = Math.hypot(dx, dy) || 1;
      const to: [number, number] = [clamp(f.x + (dx / len) * (f.d / 2), 5, this.field.w - 5), clamp(f.y + (dy / len) * (f.d / 2), 5, this.field.h - 5)];
      const run = dist(u.x, u.y, to[0], to[1]);
      let open = true;
      for (let s = 0; s <= run && open; s += 10) {
        const x = u.x + ((to[0] - u.x) * s) / Math.max(1, run), y = u.y + ((to[1] - u.y) * s) / Math.max(1, run), g = groundAt(this.field, x, y);
        if (!OPEN_GROUND.includes(coverAt(this.field, x, y)) || inRiver(g) || this.onHedge(x, y) || LAND.pace.tracks[g] <= LAND.queue.hard) open = false;
      }
      if (!open || (this.bocage && this.barredCross(u.x, u.y, to[0], to[1]))) continue;
      // No enemy weapon that can hurt a tank by direct fire with the way in its own reach (edge to edge).
      const threat = this.units.some((e) => {
        if (e.side === u.side || e.id === f.id || !fighting(e)) return false;
        const r = this.tankReach(e);
        if (r === null) return false;
        const ex = e.x - u.x, ey = e.y - u.y, k = clamp((ex * (to[0] - u.x) + ey * (to[1] - u.y)) / Math.max(1, run * run), 0, 1);
        const near = dist(e.x, e.y, u.x + (to[0] - u.x) * k, u.y + (to[1] - u.y) * k) - (Math.max(e.w, e.d) + Math.max(u.w, u.d)) / 4;
        return near <= r;
      });
      if (threat) continue;
      u.ovr = { on: f.id, to, back: [u.x, u.y], leg: 'in' };
      u.path = [];
      this.overrun.set(f.id, u.id);
      const mine = this.ticket[u.side].human, theirs = this.ticket[f.side].human;
      if (mine || theirs) this.say({ what: 'land:overrun', unit: mine ? u.id : f.id, side: mine ? u.side : f.side, text: mine ? `${this.named(u)} drives through the enemy's line.` : `Enemy tanks drive through ${this.named(f)}'s line.`, x: f.x, y: f.y });
    }
  }

  /**
   * A tank block on its overrun this tick: on through the foot's ground to its far edge, then backing out, facing the
   * enemy, to where it stood. False when the overrun is over or called off (it broke, wavered, or was ordered away).
   */
  private drive(u: LandUnit): boolean {
    const O = u.ovr!;
    if (!fighting(u) || u.state === 'wavering' || u.gives || u.fellBack || (u.order && u.order.kind !== 'attack')) { u.ovr = undefined; return false; }
    const f = this.unit(O.on);
    if (O.leg === 'in' && (!f || !fighting(f))) O.leg = 'back';
    const to = O.leg === 'in' ? O.to : O.back;
    const d = dist(u.x, u.y, to[0], to[1]);
    if (d < 1) {
      if (O.leg === 'back') { u.ovr = undefined; return false; }
      O.leg = 'back';
      u.speed = 0; u.act = 'fight';
      return true;
    }
    const head = bearing(u.x, u.y, to[0], to[1]), on = O.leg === 'in';
    if (on) {
      if (Math.abs(turn(u.face, head)) > 0.6) { this.turnTo(u, head); u.speed = 0; u.act = 'turn'; return true; }
      this.turnTo(u, head);
    }
    const g = groundAt(this.field, u.x, u.y);
    const v = LAND.speed.arm * Math.max(0.12, LAND.pace.tracks[g]) * (on ? 1 : LAND.overrun.reverse);
    const step = Math.min(d, v * DT), [ax, ay] = ahead(head);
    u.x = clamp(u.x + ax * step, 5, this.field.w - 5);
    u.y = clamp(u.y + ay * step, 5, this.field.h - 5);
    u.speed = step / DT;
    if (on) u.pressing = head;
    u.act = 'march';
    u.cover = groundAt(this.field, u.x, u.y);
    return true;
  }

  /** Metres between two blocks' edges, roughly. */
  gap(a: LandUnit, b: LandUnit): number {
    return Math.max(0, dist(a.x, a.y, b.x, b.y) - (Math.max(a.w, a.d) + Math.max(b.w, b.d)) / 4);
  }

  /**
   * What a block takes of a blow for the ground it stands on (its cover, the shelter of wrecks beside men on foot, tanks
   * blind in the streets): the part of what it takes that is where it stands, not whose ground it is.
   */
  private groundTake(t: LandUnit, at: [number, number] = [t.x, t.y]): number {
    const gt = coverAt(this.field, at[0], at[1]);
    let take = LAND.cover[gt];
    // Men on foot beside a wreck take light cover, either army's (b477).
    const ws = this.wreckShare(t);
    if (ws > 0) take = take * (1 - ws) + Math.min(take, LAND.cover[LAND.wreck.cover]) * ws;
    if (t.kind === 'arm' && LAND.tanksClose.ground.includes(gt)) take *= LAND.tanksClose.take;
    return take;
  }

  /**
   * One block's blow on another this tick: its weight (dmg), its nerve's multiplier, whether it lands on the front and
   * from which aspect, the part of what the target takes that is its ground (ground), and the share a smoke screen across
   * it left of it (screen: 1 where none; smoke on call, release 2), so the air's part of it can be held back whole.
   */
  private blow(u: LandUnit, t: LandUnit): { dmg: number; nerve: number; front: boolean; aspect: 'front' | 'flank' | 'rear'; ground: number; screen: number } {
    // An overrun's tank strikes and is struck as from where it stood before the drive (its height and its own ground
    // there), face to face: harm unchanged (b418; the design director, 06:02Z).
    const pair = this.inOverrun(u, t);
    const uAt: [number, number] = pair && u.ovr ? u.ovr.back : [u.x, u.y], tAt: [number, number] = pair && t.ovr ? t.ovr.back : [t.x, t.y];
    const gu = coverAt(this.field, uAt[0], uAt[1]);
    let p = u.power0 * (0.35 + 0.65 * (u.str / u.str0)) * LAND.rate;
    // Stretcher bearers (release 2): the men out carrying the wounded back strike nothing.
    if (this.bearers) p *= this.atHand(u);
    if (u.state === 'wavering') p *= LAND.wavering;
    // A block giving ground at the brink (giveGround) leaves a rearguard facing the enemy: a blow on it lands at the
    // weight of one struck by an enemy on the move (LAND.moving), as the enemy must come on after it, and on its front,
    // never its turned back (b496(1)).
    const rearguard = this.givingGround(t);
    if ((u.order && u.path.length && u.speed > 0.5) || rearguard) p *= LAND.moving;
    // Men getting their breath after a run at the double strike softer (direct command, release 2).
    if (u.winded !== undefined && this.t < u.winded) p *= LAND.double.blows;
    let nerveMul = 1;
    // An ambush's first fire, from close cover (release 2, LAND.hide.surprise): it shakes men caught moving in the open
    // worst; their mortars, pre-sighted on the gaps, strike what it is firing on harder in those seconds.
    const SP = LAND.hide.surprise;
    if (u.sprung !== undefined && this.t - u.sprung < SP.secs) {
      p *= SP.punch;
      nerveMul *= t.speed > 0.2 && OPEN_GROUND.includes(coverAt(this.field, t.x, t.y)) && !this.onHedge(t.x, t.y) ? SP.open : SP.nerve;
    }
    if (u.team === 'mor' && this.hide && this.units.some((f) => f.side === u.side && f.sprung !== undefined && this.t - f.sprung < SP.secs && f.engaged === t.id)) p *= SP.mortar;
    // Found by its fire but not yet located: the fire back at it is half aimed (LAND.hide.locate).
    const vague = t.unloc !== undefined && this.t < t.unloc;
    if (vague) p *= LAND.hide.locate.fire;
    // Smoke on call (release 2, ruling 242 S1-A): a direct blow across a screen standing between the shooter's middle and
    // the target's is fired at a place, not a seen target, as the vague share is, and as fire on cover at a block still
    // hidden is (blind: LAND.hide.search.blow, in step): never both. Fire from far back is not touched (direct). Strength
    // only: no draw of the dice is added or taken away.
    const screen = this.smoke && !vague && !t.hid && this.screens.length && this.direct(u, t) && this.screened(u.x, u.y, t.x, t.y) ? LAND.smoke.share : 1;
    p *= screen;
    if (u.kind === 'art') {
      const G = gunsOf(u), close = this.gap(u, t) < G.closeAt;
      p *= close ? G.close : G.punch;
      if (!close) nerveMul *= G.nerve;
      // A support team's bullets and light bombs do little to tanks (release 2).
      if (u.team && t.kind === 'arm') p *= LAND.teams[u.team].armour;
    }
    if (u.kind === 'arm' && LAND.tanksClose.ground.includes(gu)) p *= LAND.tanksClose.punch;
    // Closed up (LAND.closed): her commanders down inside, she finds less of the men on foot to fire at.
    if (u.kind === 'arm' && motionOf(t) === 'foot' && this.closed(u)) p *= LAND.closed.foot;
    const rise = heightAt(this.field, uAt[0], uAt[1]) - heightAt(this.field, tAt[0], tAt[1]);
    if (rise > LAND.height.over) p *= LAND.height.up; else if (rise < -LAND.height.over) p *= LAND.height.down;
    const aspect = rearguard || pair ? 'front' : aspectOf(t.face, t.x, t.y, u.x, u.y);
    const [am, nm] = LAND.aspect[aspect];
    // Measured armour (release 2): a tank's blow on a tank, or a gun's over open sights, by the face and the range.
    let amx = 1;
    if (this.measured && t.kind === 'arm' && (u.kind === 'arm' || (u.kind === 'art' && !u.team && this.gap(u, t) < gunsOf(u).closeAt))) amx = this.armourAt(u, t, faceOfAspect(aspect), this.gap(u, t)) ?? 1;
    const ground = this.groundTake(t, tAt);
    let take = ground;
    // Tanks that have left their infantry on close ground take the card's own figure for tanks alone there (item 7).
    if (t.kind === 'arm' && LAND.alone.on) take *= this.alone(t, u);
    if (t.side === 'def' && dist(t.x, t.y, t.home[0], t.home[1]) < LAND.hold.within) take *= this.holdTake(u, t);
    // Upright and running in the open, at the double: struck harder (direct command, release 2).
    if (t.order?.kind === 'move' && t.order.run && t.speed > 0.5) take *= LAND.double.exposed;
    // A sniper team found and located: two men and no depth; the window it fires from blown in by bombs and shells.
    if (t.team === 'snp' && !t.hid && !vague) {
      take *= LAND.snipers.take.fragile;
      if (u.team === 'mor' || (u.kind === 'art' && !u.team) || u.kind === 'arm') take *= LAND.snipers.take.blast;
    }
    const luck = 1 + LAND.jitter * ((u.team === 'snp' ? this.snpRoll(u, 342) : this.rng.next()) * 2 - 1);
    const dmg = p * am * amx * take * luck * (u.side === 'att' ? this.fortune : 1 / this.fortune) * DT;
    return { dmg, nerve: nm * nerveMul, front: aspect === 'front', aspect, ground, screen };
  }

  /**
   * 'from' is who struck it this tick and how hard. Each model it loses goes to the one of them owed most (LandUnit.owed:
   * the harm each striker has done it, less a model's worth for each model given), the first of equals; a gun or a tank
   * given a model lands a shell there. So guns are seen firing in their share of the harm, not only where they did the
   * most in a tick (the design director, b507). Looks only: nothing in the fight reads it.
   */
  private suffer(t: LandUnit, dmg: number, nerveMul: number, from: [LandUnit, number][] = [], blind = false, bare = false): void {
    // A hidden block struck (planes over the woods) is found (release 2); fire on cover alone finds it by its own odds.
    // Kept harm landing ('bare', hold until relieved) is no blow of anyone's that tick, and finds no one.
    if (t.hid && dmg > 0 && !blind && !bare) this.reveal(t, 'struck');
    // Waiting for help, dug in where it was told to wait (hold until relieved, release 2), whether help is late or not:
    // the field's light cover, never better than its ground gives; what it spares is kept, with the nerve it would have
    // cost, and lands when the wait ends (endWait). 'bare': that harm landing.
    if (t.waits && !t.waits.free && !bare && dmg > 0 && dist(t.x, t.y, t.waits.at[0], t.waits.at[1]) <= LAND.dug.keep) {
      const take = this.groundTake(t), f = Math.min(take, LAND.cover[LAND.wreck.cover]) / Math.max(1e-9, take);
      if (f < 1) { const spared = dmg * (1 - f); t.debt = (t.debt ?? 0) + spared; t.debtN = (t.debtN ?? 0) + spared * nerveMul; dmg *= f; }
    }
    // Damage is in power: as a share of what the block came with, it comes off its strength meter.
    const share = dmg / Math.max(1e-9, t.power0);
    const before = t.str, had = t.figures;
    t.str = Math.max(0, t.str - share * t.str0);
    const lost = (before - t.str) / t.str0;
    // Sniper teams (release 2): its leaders shot (LAND.snipers.lead), it steadies on part of its experience.
    const xpNow = t.leadTo !== undefined && this.t < t.leadTo ? t.xp * (t.leadK ?? 1) : t.xp;
    const steady = 1 - LAND.nerve.xp * xpNow - LAND.nerve.general * this.generalOf(t.side);
    t.mor = Math.max(0, t.mor - lost * LAND.nerve.perLoss[t.side] * nerveMul * Math.max(0.3, steady));
    // A sniper team under fire, seen, vague or blind, holds its fire till LAND.snipers.quiet after the last blow; struck
    // while it slips away, it stays found.
    if (t.team === 'snp') { t.quietTo = Math.round((this.t + LAND.snipers.quiet) * 1000) / 1000; delete t.slipTo; }
    t.still = 0;
    t.figures = t.str <= 0 ? 0 : Math.max(1, Math.ceil(t.tray * t.str));
    // Stretcher bearers (release 2): its wounded lie for its bearers, its light cases go to their dressing. A model lost
    // stays lost (its wreck laid once): the light cases come back as strength, never as a model stood up again.
    if (this.bearers) { this.wounded(t, before - t.str); t.figures = Math.min(had, t.figures); }
    const owed = (t.owed ??= {}), model = t.power0 / Math.max(1e-9, t.tray * t.str0);
    for (const [u, d] of from) owed[u.id] = (owed[u.id] ?? 0) + d;
    for (let k = t.figures; k < had; k++) {
      const w = t.kind === 'arm' ? this.wreck(t) : null;
      let by: LandUnit | undefined;
      for (const [u] of from) if (!by || owed[u.id] > owed[by.id]) by = u;
      if (!by) { this.drill?.fromAir(t.id, k); continue; }
      owed[by.id] -= model;
      // A tank block's man off the enemy's foot may go to her machine guns, not a shell (the drill book; looks only).
      // (a sniper team's man is shot, as a machine-gun team's is: no shell)
      if (SHELLS_FROM.includes(by.kind) && by.team !== 'mg' && by.team !== 'snp' && !(by.kind === 'arm' && this.drill?.mgKill(by, t, k))) this.shell(by, t, k, w);
      else this.drill?.struckBy(by.id, t.id, k);
    }
  }

  /**
   * A shell that cost a block its model k (counted from 0 in its models' order): it lands at that model's place within
   * the block's frontage and depth, or on the tank it knocked out, at her wreck (shells.ts). Fixed by the block and the
   * model, never drawn from the battle's random stream. A 'shell' event for the 3D game, sent once and never logged: it
   * says nothing, and names the block that fired only where the player knows it (b464(2); a block he does not know that
   * lands a shell on his is always guns firing from far back, since any other striker is known by the blow).
   */
  private shell(by: LandUnit, t: LandUnit, k: number, w: Zone | null): void {
    let x: number, y: number;
    // With the men drawn (drill.ts), the shell lands on the man it costs, where he stands, and on the tank it knocks out
    // where she stands (at a post, she burns there: drill.ts wreckSpot); else at his place on the tray, or at her wreck.
    const man = this.drill?.manAt(t.id, k);
    if (man) { x = Math.round(clamp(man[0], 2, this.field.w - 2) * 10) / 10; y = Math.round(clamp(man[1], 2, this.field.h - 2) * 10) / 10; }
    else if (w) { x = w.x; y = w.y; }
    else {
      const across = (mix(t.id * 787 + k * 271 + 11) - 0.5) * t.w * 0.9, along = (mix(t.id * 449 + k * 523 + 5) - 0.5) * t.d * 0.8;
      const c = Math.cos(t.face), s = Math.sin(t.face);
      x = Math.round(clamp(t.x + across * c + along * s, 2, this.field.w - 2) * 10) / 10;
      y = Math.round(clamp(t.y - across * s + along * c, 2, this.field.h - 2) * 10) / 10;
    }
    const cal = by.cal ?? 'field', at = Math.round(this.t * 10) / 10, armour = t.kind === 'arm';
    const ground = groundAt(this.field, x, y);
    const ev: FieldEvent = { t: at, what: 'shell', text: '', x, y, cal, on: t.id, armour, ground, reach: REACH[cal] };
    if (this.known(by)) ev.by = by.id;
    this.events.push(ev);
    this.drill?.shelled(x, y, by.id, t.id, k);
    // On the ground it leaves its crater to the end of the fight; on armour, or in the river, none.
    if (!armour && !inRiver(ground)) {
      this.craters.push({ id: -(this.craters.length + 1), kind: 'crater', x, y, r: CRATER[cal] / 2, until: this.ticket.limit, at, cal });
    }
  }

  /**
   * A tank knocked out: her wreck stays where she stood, within the block's frontage, to the end of the fight (b477). Where
   * on the tray is fixed by the block and the count, never by the battle's dice, so the fight's own luck does not move.
   */
  private wreck(t: LandUnit): Zone {
    const n = this.wrecks.filter((z) => z.unit === t.id).length;
    // The drill stands each tank at this same place (drill.ts wreckOffset), or at a post near it in a fight, and draws her
    // wreck where she fell (drill.ts wreckSpot): this place is the rules' own, for the shelter it gives men beside it.
    const [across, along] = wreckOffset(t.id, n, t.w, t.d);
    const c = Math.cos(t.face), s = Math.sin(t.face);
    const x = clamp(t.x + across * c + along * s, 2, this.field.w - 2), y = clamp(t.y - across * s + along * c, 2, this.field.h - 2);
    const z: Zone = { id: this.wrecks.length + 1, kind: 'wreck', x: Math.round(x * 10) / 10, y: Math.round(y * 10) / 10, r: LAND.wreck.r, until: this.ticket.limit, side: t.side, at: Math.round(this.t * 10) / 10, unit: t.id };
    this.wrecks.push(z);
    return z;
  }

  /** The share of a block on foot that wrecks shelter: one model's worth for each within its frontage or beside it (b477). */
  private wreckShare(u: LandUnit): number {
    if (!this.wrecks.length || MOTION[u.kind] !== 'foot') return 0;
    const c = Math.cos(u.face), s = Math.sin(u.face), hw = u.w / 2 + LAND.wreck.beside, hd = u.d / 2 + LAND.wreck.beside;
    let n = 0;
    for (const z of this.wrecks) { const dx = z.x - u.x, dy = z.y - u.y; if (Math.abs(dx * c - dy * s) <= hw && Math.abs(dx * s + dy * c) <= hd) n++; }
    return Math.min(1, n / Math.max(1, u.tray));
  }

  private generalOf(s: Side): number { return this.ticket[s].general?.level ?? 0; }

  private nerve(u: LandUnit): void {
    const N = LAND.nerve;
    const enemyNear = this.units.some((e) => e.side !== u.side && fighting(e) && dist(e.x, e.y, u.x, u.y) < N.clear);
    if (enemyNear) u.clear = 0;
    if (u.state === 'routing') {
      if (u.clear > N.quiet) u.mor = Math.min(u.mor0, u.mor + N.runnerRecover * DT);
      if (!this.broke[u.side] && u.clear > N.quiet && u.mor >= N.rally && u.str > N.shattered) {
        u.state = 'wavering'; u.order = null; u.path = []; u.act = 'idle'; u.gives = false;
        this.say({ what: 'rallied', unit: u.id, side: u.side, text: `${this.named(u)} has rallied.`, x: u.x, y: u.y });
      }
      return;
    }
    if (u.still > N.still) u.mor = Math.min(u.mor0, u.mor + N.recover * DT);
    // Help that never came (hold until relieved, release 2): it breaks where a block starts to waver, until relieved.
    // A sniper team never routs (LAND.snipers.back): at half strength or the rout nerve it withdraws, as a block ordered back.
    if (u.team === 'snp' && (u.str <= LAND.snipers.back * u.str0 || u.mor < N.rout)) this.sniperBack(u);
    else if (u.str <= N.shattered * u.str0 || u.mor < (u.waits?.late ? N.waver : N.rout)) { this.rout(u); return; }
    const was = u.state;
    u.state = u.mor < N.waver ? 'wavering' : 'steady';
    if (was === 'steady' && u.state === 'wavering') this.say({ what: 'wavering', unit: u.id, side: u.side, text: `${this.named(u)} is wavering.`, x: u.x, y: u.y });
  }

  private rout(u: LandUnit): void {
    if (u.state === 'routing') return;
    u.state = 'routing'; u.order = null; u.path = []; u.keepFace = null; u.engaged = undefined;
    this.say({ what: 'routing', unit: u.id, side: u.side, text: `${this.named(u)} breaks and runs.`, x: u.x, y: u.y });
    // Friends nearby see it go.
    for (const f of this.units) if (f !== u && f.side === u.side && fighting(f) && dist(f.x, f.y, u.x, u.y) < LAND.nerve.near) f.mor = Math.max(0, f.mor - LAND.nerve.friendRuns);
    // Waiting for help, it waits no longer (hold until relieved): what its cover spared lands with the break.
    if (u.waits) this.endWait(u, 'broke');
    else if (this.repay.length) this.landNow(u);
  }

  private move(u: LandUnit): void {
    const k = u.kind;
    u.pressing = undefined;
    if (u.ovr && this.drive(u)) return;
    const gave = u.yieldTo !== undefined ? this.unit(u.yieldTo) : undefined;
    u.yieldTo = undefined;
    let to: [number, number] | null = null, run = false;
    if (u.state === 'routing') { to = [u.x, u.side === 'att' ? -50 : this.field.h + 50]; run = true; }
    else if (u.order?.kind === 'withdraw') {
      // Straight back to its own edge, or back round by its bridge where a river or rock lies behind it.
      u.path = this.dry(this.going(u), u.x, u.y, u.x, u.order.y) && !this.exit(this.going(u), u.x, u.y) ? [[u.x, u.order.y]] : this.route(u, u.order.x, u.order.y);
      to = u.path[0];
    }
    else if (u.order?.kind === 'attack') {
      let t = this.unit(u.order.target!);
      // An attack pressed by several blocks carries on: when its block breaks or leaves, it takes the nearest enemy
      // still fighting within reach of the march, as the computer general would (ruling 226(4)).
      if (u.order.press && t && !fighting(t) && (!this.ai[u.side] || u.taken)) {
        const next = this.units.filter((e) => e.side !== u.side && fighting(e) && dist(e.x, e.y, u.x, u.y) < LAND.press)
          .reduce<LandUnit | null>((m, e) => (!m || dist(e.x, e.y, u.x, u.y) < dist(m.x, m.y, u.x, u.y) ? e : m), null);
        if (next) {
          // Where it stands across the attack's front, it stays: the next block is taken in front of it, not in its middle.
          const ax = u.order.axis, lane = ax === undefined ? undefined : (u.x - next.x) * Math.sin(ax + Math.PI / 2) + (u.y - next.y) * Math.cos(ax + Math.PI / 2);
          this.send(u, { kind: 'attack', x: next.x, y: next.y, target: next.id, press: true, ...(ax === undefined ? {} : { lane, axis: ax }) }); t = next;
        }
      }
      if (!t || !live(t)) { u.order = null; u.path = []; }
      else {
        // Close until it is well in reach, then stand and fight.
        // Never stands in the water to fight, or on the bridge: it presses on to its own ground on the far bank.
        const want = this.closeTo(u);
        const at = this.gap(u, t) > want || this.wet(u, u.x, u.y) ? this.lanePoint(u, t, want) : null;
        if (at) { u.path = this.route(u, at[0], at[1]); to = u.path[0]; }
        else { u.path = []; to = null; }
      }
    } else if (u.order && u.path.length) {
      // A block on the march stops when an enemy is on top of it, unless it is running or pulling out. One that can no
      // longer walk straight to its next turning point (it came at it a little to one side) looks again for its way.
      to = u.path[0];
      if (u.order.kind === 'move' && !this.dry(this.going(u), u.x, u.y, to[0], to[1])) { u.path = this.route(u, u.order.x, u.order.y); to = u.path[0]; }
    }
    // Already at a turning point of its way: on to the next, from the point itself. At the end of its way (as near as
    // it can get): nothing to walk, so it stands, and faces as it stands.
    if (to && dist(u.x, u.y, to[0], to[1]) < 0.5 && !run) {
      if (u.path.length > 1 && u.path[0] === to) { u.x = to[0]; u.y = to[1]; u.path.shift(); to = u.path[0]; }
      else if (u.order?.kind === 'move') { u.path.shift(); to = u.path[0] ?? null; }
      else to = null;
    }
    if (u.engaged !== undefined && u.order?.kind === 'move' && u.kind !== 'art') {
      const e = this.unit(u.engaged);
      if (e && this.gap(u, e) < 60) to = null;
    }
    // A block may wade the river, never stand in it (b464(5)): with nothing left to walk there (told to hold, its target
    // gone, rallied in the water, an enemy on top of its march) it wades on to the nearer bank, the one before it first.
    const wading = !run && inRiver(groundAt(this.field, u.x, u.y));
    if (wading && !to) to = this.shore(u);
    if (!to) {
      u.speed = 0;
      // Face what it is fighting, unless held by an enemy in front or by the facing its line was given.
      const e = u.engaged !== undefined ? this.unit(u.engaged) : undefined;
      let want = u.keepFace;
      if (e && !u.pinned) {
        const off = Math.abs(turn(u.keepFace ?? u.face, bearing(u.x, u.y, e.x, e.y)));
        if (u.keepFace === null || off > Math.PI / 3) want = bearing(u.x, u.y, e.x, e.y);
      }
      if (u.order?.kind === 'move' && !u.path.length) { u.order = null; }
      if (want !== null && Math.abs(turn(u.face, want)) > 0.02) { this.turnTo(u, want); u.act = 'turn'; }
      else u.act = e ? 'fight' : 'idle';
      if (u.state === 'routing') u.act = 'rout';
      return;
    }
    const head = bearing(u.x, u.y, to[0], to[1]);
    // It made way for a friend with the road last tick: it waits, rather than step straight back into that friend's way
    // (never in the river: there it presses on to the bank). At a bridge's end it waits too: that is where the queue
    // forms, and two blocks that shoved each other there without waiting stood locked till the battle ended (b453).
    if (gave && !run && !this.wet(u, u.x, u.y, false) && Math.sin(head) * (gave.x - u.x) + Math.cos(head) * (gave.y - u.y) > 0) { u.speed = 0; u.act = 'idle'; return; }
    // Going in together (ai.ts; the tactics panel, item 1): a tank or lorry block keeps no more than STYLE.lead ahead of
    // its group's slowest foot block until it has been in contact. It waits for them rather than step further ahead,
    // never in the river or on a bridge.
    if (!run && u.order?.together !== undefined && (k === 'arm' || k === 'mot') && !u.met && Math.cos(head) * (u.side === 'att' ? 1 : -1) > 0 && !this.wet(u, u.x, u.y, false)) {
      const ahead0 = this.aheadOfFoot(u, u.order.together);
      if (ahead0 !== null && ahead0 >= STYLE.lead) { this.turnTo(u, head); u.speed = 0; u.act = 'idle'; return; }
    }
    const off = turn(u.face, head);
    // Blocks wheel before they march; a runner just runs, and one wading the river turns as it wades.
    if (!run && !wading && Math.abs(off) > 0.6) { this.turnTo(u, head); u.speed = 0; u.act = 'turn'; return; }
    if (run) u.face = head; else this.turnTo(u, head);
    const m = motionOf(u);
    const g = groundAt(this.field, u.x, u.y);
    let pace = LAND.pace[m][g];
    if (k === 'mtn' && (g === 'rough' || g === 'rock')) pace = Math.min(1, pace * LAND.mountainFoot);
    // Up a slope it slows; out of the river's bed it wades at the wading pace, which counts the bank already (b464(5):
    // every bank read as a cliff above the bed, and foot crawled their last 20 m of the river at 0.8 m a second).
    const up = inRiver(g) ? 0 : heightAt(this.field, u.x + Math.sin(head) * 20, u.y + Math.cos(head) * 20) - heightAt(this.field, u.x, u.y);
    pace *= up > 0 ? Math.max(0.4, 1 - (up / 20) * 1.5) : 1;
    const top = u.team ? LAND.teams[u.team].speed : LAND.speed[k];
    // At the double (direct command, release 2): faster, and winded for a while after.
    const double = !run && u.order?.kind === 'move' && !!u.order.run;
    if (double) u.winded = this.t + LAND.double.winded;
    let v = top * Math.max(0.12, pace) * (run ? 1.15 : 1) * (double ? LAND.double.pace[m] : 1) * (u.state === 'wavering' ? 0.85 : 1);
    // Pinned (LAND.pin; a sniper team's fire, release 2): men on foot go to ground and crawl, at most LAND.pin.most
    // seconds a battle, counted while it moves; never routing or falling back.
    if (u.pinTo !== undefined && this.t < u.pinTo && m === 'foot' && !run && u.order?.kind !== 'withdraw' && (u.pinSpent ?? 0) < LAND.pin.most) {
      v *= u.pinK ?? 1;
      u.pinSpent = Math.round(((u.pinSpent ?? 0) + DT) * 1000) / 1000;
    }
    const d = dist(u.x, u.y, to[0], to[1]);
    const stepLen = Math.min(d, v * DT);
    const [ax, ay] = ahead(head);
    let nx = u.x + ax * stepLen, ny = u.y + ay * stepLen;
    // Ground it cannot enter: try a little to either side, else stop. Not while it is still walking out of such ground.
    if (LAND.pace[m][groundAt(this.field, nx, ny)] <= 0 && LAND.pace[m][g] > 0 && !run) {
      let moved = false;
      for (const sw of [0.5, -0.5, 1, -1]) {
        const [bx, by] = ahead(head + sw);
        const tx = u.x + bx * stepLen, ty = u.y + by * stepLen;
        if (LAND.pace[m][groundAt(this.field, tx, ty)] > 0) { nx = tx; ny = ty; moved = true; break; }
      }
      if (!moved) { u.speed = 0; u.act = 'idle'; return; }
    }
    u.x = clamp(nx, 5, this.field.w - 5);
    u.y = run ? ny : clamp(ny, 5, this.field.h - 5);
    u.speed = stepLen / DT;
    if (!run) u.pressing = head;
    u.act = run ? 'rout' : v > top * 1.05 ? 'run' : 'march';
    u.cover = groundAt(this.field, u.x, u.y);
    if (d <= Math.max(6, stepLen + 0.01)) {
      u.path.shift();
      if (!u.path.length && u.order?.kind === 'move') { u.order = null; u.speed = 0; }
    }
    // Off the field.
    if (u.y < 0 || u.y > this.field.h) {
      if (u.state === 'routing') {
        u.state = 'gone'; u.act = 'down'; u.figures = 0;
        this.say({ what: 'gone', unit: u.id, side: u.side, text: `${this.named(u)} has fled the field.` });
      } else if (u.order?.kind === 'withdraw') {
        u.state = 'withdrawn'; u.act = 'down';
        this.say({ what: 'withdrawn', unit: u.id, side: u.side, text: `${this.named(u)} has fallen back off the field.` });
      }
    } else if (u.order?.kind === 'withdraw' && (u.side === 'att' ? u.y <= this.outLine('att') : u.y >= this.outLine('def'))) {
      u.state = 'withdrawn'; u.act = 'down';
      this.say({ what: 'withdrawn', unit: u.id, side: u.side, text: `${this.named(u)} has fallen back off the field.` });
    }
  }

  /** Within LAND.queue.near metres of ground this block can hardly cross or not at all: at a bridge, a pass, a gap. */
  private narrows(u: LandUnit, x: number, y: number): boolean {
    const m = this.going(u), f = this.field, C = f.cols, R = f.rows;
    let near = this.hardNear[m];
    if (!near) {
      near = this.hardNear[m] = new Uint8Array(C * R);
      const n = Math.round(LAND.queue.near / f.cell), pace = this.paceOf(m);
      for (let i = 0; i < C * R; i++) {
        if (pace[GROUNDS[f.ground[i]]] > LAND.queue.hard) continue;
        const c0 = i % C, r0 = Math.floor(i / C);
        for (let r = Math.max(0, r0 - n); r <= Math.min(R - 1, r0 + n); r++) for (let c = Math.max(0, c0 - n); c <= Math.min(C - 1, c0 + n); c++) near[r * C + c] = 1;
      }
    }
    return near[this.cellAt(x, y)] === 1;
  }

  /** How far a block has still to go on its way. */
  private toGo(u: LandUnit): number {
    let d = 0, x = u.x, y = u.y;
    for (const [px, py] of u.path) { d += dist(x, y, px, py); x = px; y = py; }
    return d;
  }

  /**
   * Where a block in a lane makes for: in front of its target as the attack came on, out to the side by its lane, and
   * for a lane wider than the target's front, forward round its flank. With no lane, or once it stands at that point and
   * is still out of reach, straight at the target. Null once it stands at that point within reach: it stands and fights.
   */
  private lanePoint(u: LandUnit, t: LandUnit, want: number): [number, number] | null {
    const o = u.order!;
    const lone = o.lane === undefined || o.axis === undefined;
    // A block with no lane makes straight for its target, to stand before it on the way it comes.
    const axis = lone ? bearing(u.x, u.y, t.x, t.y) : o.axis!, lane = lone ? 0 : o.lane!;
    const key = lone ? `${t.id}` : `${t.id}/${lane}/${axis}`;
    // How far off its target a block may stand, at its own stretch, and still fight: half way from where it closes to
    // the end of its reach. A stand pushed further out by its friends, it never fires from (the review, b453).
    const hold = want + (reachOf(u) - want) / 2;
    const off = (x: number, y: number): number => dist(x, y, t.x, t.y) - (Math.max(u.w, u.d) + Math.max(t.w, t.d)) / 4;
    const there = (s: [number, number]): [number, number] | null => {
      if (dist(u.x, u.y, s[0], s[1]) >= 12) return s;
      // At its stand: in reach, it stands; still out of reach (its target has stepped back), it closes on the target from
      // here on, and does not turn back for the stand a step later.
      if (this.gap(u, t) <= hold && !this.wet(u, u.x, u.y)) return null;
      u.closing = true;
      return [t.x, t.y];
    };
    // The stand it worked out holds while it is still its own, dry and in reach: a step to one side, or a friend first in
    // the list, does not send it off to another stretch (it flipped between two, and wheeled in place: the review, b453).
    const old = u.stand && u.standFor === key ? u.stand : null;
    if (old && u.closing) return [t.x, t.y];
    const kept = !!old && off(old[0], old[1]) <= hold && !this.wet(u, old[0], old[1]);
    if (old && kept && !this.claimed(u, old[0], old[1], u.standFace ?? bearing(old[0], old[1], t.x, t.y))) return there(old);
    const [fx, fy] = ahead(axis), [rx, ry] = ahead(axis + Math.PI / 2);
    const lim = t.w / 2 + u.w / 2;
    const side0 = Math.min(Math.abs(lane), lim) * Math.sign(lane);
    const back = Math.max(0, want * 0.8 + t.d / 2 + u.d / 2 - Math.max(0, Math.abs(lane) - lim));
    let p: [number, number] = [t.x + rx * side0 - fx * back, t.y + ry * side0 - fy * back];
    // Its own ground to stand on, never the river or the bridge: on into the far bank, short of the block it attacks;
    // else back to the nearer bank.
    if (this.wet(u, p[0], p[1])) {
      const room = Math.max(0, back - (t.d / 2 + u.d / 2 + LAND.apart));
      let dry: [number, number] | null = null;
      for (let k = 8; k <= Math.max(room, 8) && !dry; k += 8) { const q: [number, number] = [p[0] + fx * Math.min(k, room), p[1] + fy * Math.min(k, room)]; if (!this.wet(u, q[0], q[1])) dry = q; }
      for (let k = 8; k <= 240 && !dry; k += 8) { const q: [number, number] = [p[0] - fx * k, p[1] - fy * k]; if (!this.wet(u, q[0], q[1])) dry = q; }
      if (dry) p = dry;
    }
    // Its own stretch: where a friend already stands there, it stands beside it, on the side it comes from, a little
    // room between the two (the art director, b439: no box drawn on another), never in the river, and never so far out
    // that it cannot fight from there: then the friend's other side, else as near as it can.
    const face = bearing(p[0], p[1], t.x, t.y), [sx, sy] = ahead(face + Math.PI / 2);
    const fit = (q: [number, number]): boolean => off(q[0], q[1]) <= hold && !this.wet(u, q[0], q[1])
      && !this.units.some((e) => e.side !== u.side && live(e) && dist(e.x, e.y, q[0], q[1]) < (Math.max(e.w, e.d) + Math.max(u.w, u.d)) / 2);
    for (let k = 0; k < 4; k++) {
      const f = this.claimed(u, p[0], p[1], face);
      if (!f) break;
      const lat = (p[0] - f.x) * sx + (p[1] - f.y) * sy, mine = (u.x - f.x) * sx + (u.y - f.y) * sy;
      const side = Math.abs(lat) >= 1 ? Math.sign(lat) : Math.sign(mine) || Math.sign(lane) || (u.id < f.id ? -1 : 1);
      const by = (u.w + f.w) / 2 + LAND.room;
      const near: [number, number] = [p[0] + sx * side * (by - Math.abs(lat)), p[1] + sy * side * (by - Math.abs(lat))];
      const far: [number, number] = [p[0] - sx * side * (by + Math.abs(lat)), p[1] - sy * side * (by + Math.abs(lat))];
      const q = fit(near) ? near : fit(far) ? far : null;
      if (!q) break;
      p = q;
    }
    // Nowhere free in reach: it keeps the stand it had, rather than hop each tick between two that are taken.
    if (old && kept && this.claimed(u, p[0], p[1], face)) return there(old);
    u.stand = p; u.standFor = key; u.standFace = face; u.closing = false;
    return there(p);
  }

  /**
   * The friend whose ground this block's box would stand on at x, y facing so (with LAND.room between), or null: one
   * standing there, or one whose attack has it stand there and is nearer to it than this block is to x, y.
   */
  private claimed(u: LandUnit, x: number, y: number, face: number): { id: number; x: number; y: number; w: number } | null {
    const [fx, fy] = ahead(face), [rx, ry] = ahead(face + Math.PI / 2), mine = dist(u.x, u.y, x, y);
    for (const f of this.units) {
      if (f === u || f.side !== u.side || !fighting(f)) continue;
      let at: [number, number] | null = f.speed <= 0.5 ? [f.x, f.y] : null;
      if (!at && f.order?.kind === 'attack' && f.stand) {
        const theirs = dist(f.x, f.y, f.stand[0], f.stand[1]);
        if (theirs < mine || (theirs === mine && f.id < u.id)) at = f.stand;
      }
      if (!at) continue;
      const dx = at[0] - x, dy = at[1] - y;
      if (Math.abs(dx) > 300 || Math.abs(dy) > 300) continue;
      const c = Math.abs(Math.cos(f.face - face)), s = Math.abs(Math.sin(f.face - face));
      const pl = u.w / 2 + (f.w / 2) * c + (f.d / 2) * s + LAND.room / 2 - Math.abs(dx * rx + dy * ry);
      const pd = u.d / 2 + (f.w / 2) * s + (f.d / 2) * c - Math.abs(dx * fx + dy * fy);
      if (pl > 0 && pd > 0) return { id: f.id, x: at[0], y: at[1], w: f.w };
    }
    return null;
  }

  /**
   * Ground a block may not stand on: the river, a bridge and the road off either end of it (kept clear for those
   * coming over), or ground it can hardly cross (LAND.queue.hard). A block sent to stand there by its own order
   * (mouth false) may stand at a bridge's end.
   */
  private wet(u: LandUnit, x: number, y: number, mouth = true): boolean {
    if (motionOf(u) === 'fixed') return false;
    const g = groundAt(this.field, clamp(x, 5, this.field.w - 5), clamp(y, 5, this.field.h - 5));
    if (g === 'bridge' || g === 'water' || g === 'blown' || LAND.pace[motionOf(u)][g] <= LAND.queue.hard) return true;
    if (!mouth) return false;
    if (!this.mouths) {
      const f = this.field, C = f.cols, R = f.rows, n = Math.round(LAND.mouth / f.cell);
      const m = this.mouths = new Uint8Array(C * R);
      for (let i = 0; i < C * R; i++) {
        const k = GROUNDS[f.ground[i]];
        if (k !== 'bridge' && k !== 'blown') continue;
        const c0 = i % C, r0 = Math.floor(i / C);
        for (let r = Math.max(0, r0 - n); r <= Math.min(R - 1, r0 + n); r++) for (let c = Math.max(0, c0 - n); c <= Math.min(C - 1, c0 + n); c++) m[r * C + c] = 1;
      }
    }
    return this.mouths[this.cellAt(x, y)] === 1;
  }
  /** The cells within LAND.mouth of a bridge, worked out once. */
  private mouths: Uint8Array | null = null;

  /**
   * Out of the river (b464(5)): the nearest point off river ground the block can stand on, the bank before it rather
   * than the one behind unless that is much the nearer (a turn away counts as perTurn metres a radian: 30 while it
   * wades, none once the battle is over, so it is set down on the nearest bank). Where it stands already, that point.
   */
  private shore(u: LandUnit, perTurn = 30): [number, number] {
    const pace = LAND.pace[motionOf(u)], f = this.field;
    const ok = (x: number, y: number): boolean => { const g = groundAt(f, x, y); return !inRiver(g) && pace[g] > 0; };
    if (ok(u.x, u.y)) return [u.x, u.y];
    let best: [number, number] = [u.x, u.y], score = Infinity;
    for (let r = 8; r <= 240 && r < score; r += 8) for (let a = 0; a < 16; a++) {
      const b = u.face + (a * Math.PI) / 8, x = clamp(u.x + Math.sin(b) * r, 5, f.w - 5), y = clamp(u.y + Math.cos(b) * r, 5, f.h - 5);
      const sc = r + perTurn * Math.abs(turn(u.face, b));
      if (sc < score && ok(x, y)) { score = sc; best = [x, y]; }
    }
    return best;
  }

  private turnTo(u: LandUnit, want: number): void {
    const r = LAND.turn[u.kind] * DT;
    const d = turn(u.face, want);
    u.face = Math.abs(d) <= r ? want : u.face + Math.sign(d) * r;
  }

  // ---- Stretcher bearers (release 2) --------------------------------------------------------------------------------
  //
  // Brian, 22 November 17:33Z ("lets do all three but make it where we can toggle it off if someone doesnt want it"); the
  // design director's rulings Q4 and Q5 (18:36Z), and the light cases (18:38Z); LAND.bearers. Of each loss of strength a
  // block suffers (suffer: the blows, the air, kept harm landing), LAND.bearers.killed are dead and the rest wounded. Of
  // the wounded, light.share are dressed at the block and back in the line light.dress seconds later (with no walk while
  // light.at is 0), if the block is still fighting then: running, fled or gone, it takes none back. They come back as
  // strength, up to what it started with, never raising its nerve: the only upward write to strength in the fight. The
  // rest are litter cases: while they lie waiting and its bearers (share of its strength at the start, by army) are home,
  // a block still fighting sends a trip, litter bearers a case, out to its aid post (LAND.bearers.aid behind where it
  // started, toward its own edge) and back at LAND.bearers.pace: 2 x aid / pace, 330 s. Bearers out are still counted in
  // its strength, its nerve, the day's count and its size: they only weaken its own blows, by the share of its strength
  // away (atHand). One land:bearers event a block, to the player's own side, as its bearers first go out. Nothing here
  // draws on the dice. The drill draws the men (drill.ts): the wounded, the bearers and the carries, looks only.

  /** Where a block's aid post stands: LAND.bearers.aid metres behind its start place, toward its own back edge, on the field. */
  aidOf(u: LandUnit): [number, number] {
    const back = u.side === 'att' ? -1 : 1;
    return [clamp(u.home[0], 0, this.field.w), clamp(u.home[1] + back * LAND.bearers.aid, 0, this.field.h)];
  }

  /** A block's bearers and wounded, as the rules count them (strength): none before its first loss, or with the switch off. */
  litter(u: LandUnit): Readonly<Litter> | undefined { return this.bearers ? this.litters.get(u.id) : undefined; }

  /** A loss of strength d on a block: its share of wounded, the light cases to their dressing, the rest lying for the bearers. */
  private wounded(u: LandUnit, d: number): void {
    if (!(d > 0)) return;
    const B = LAND.bearers, L = B.light;
    let l = this.litters.get(u.id);
    if (!l) { l = { all: bearerShare(u.nat) * u.str0, out: 0, trips: [], waiting: 0, light: [], aid: this.aidOf(u), told: false }; this.litters.set(u.id, l); }
    const hurt = d * (1 - killedShare(u.nat)), light = hurt * L.share, foot = LAND.speed.inf;
    const at = Math.round((this.t + (L.at > 0 ? L.at / (foot * L.out) : 0) + L.dress + (L.at > 0 ? L.at / (foot * L.back) : 0)) * 1000) / 1000;
    const last = l.light[l.light.length - 1];
    if (last && last.at === at) last.n += light; else if (light > 0) l.light.push({ at, n: light });
    l.waiting += hurt - light;
  }

  /**
   * Each tick, after the harm: the bearers whose trip is done are home; the light cases whose dressing is done are back
   * in the line, where their block still fights (up to its strength at the start); then, while litter cases wait and
   * bearers are home, a block still fighting sends them out: min(waiting, home / litter) cases, litter bearers each.
   */
  private carry(): void {
    const B = LAND.bearers, trip = (2 * B.aid) / B.pace, t = this.t;
    for (const u of this.units) {
      const l = this.litters.get(u.id);
      if (!l) continue;
      if (l.trips.length && l.trips[0].back <= t + 1e-9) {
        while (l.trips.length && l.trips[0].back <= t + 1e-9) l.trips.shift();
        l.out = l.trips.reduce((a, x) => a + x.by, 0);
      }
      while (l.light.length && l.light[0].at <= t + 1e-9) {
        const n = l.light.shift()!.n, was = u.str;
        if (!fighting(u)) continue;
        u.str = Math.min(u.str0, u.str + n);
        if (u.str <= was) continue;
        let r: (typeof this.returns)[number] | undefined;
        for (let i = this.returns.length - 1; i >= 0 && !r; i--) if (this.returns[i].unit === u.id) r = this.returns[i];
        if (r && Math.floor(r.t + 1e-9) === Math.floor(t + 1e-9)) r.n += u.str - was;
        else this.returns.push({ t: Math.round(t * 1000) / 1000, unit: u.id, n: u.str - was });
      }
      if (!fighting(u) || l.waiting <= 1e-12) continue;
      const home = l.all - l.out;
      if (home <= 1e-12) continue;
      const cases = Math.min(l.waiting, home / B.litter), by = cases * B.litter;
      l.waiting -= cases; l.out += by;
      l.trips.push({ back: Math.round((t + trip) * 1000) / 1000, by });
      if (!l.told) {
        l.told = true;
        if (this.ticket[u.side].human) this.say({ what: 'land:bearers', unit: u.id, side: u.side, text: `${this.named(u)}'s stretcher bearers are carrying the wounded back.`, x: u.x, y: u.y });
      }
    }
  }

  /** The share of a block's strength at hand to strike with: all of it, less its bearers out carrying (none below nothing). */
  private atHand(u: LandUnit): number {
    const l = this.litters.get(u.id);
    if (!l || l.out <= 0 || u.str <= 0) return 1;
    return Math.max(0, u.str - l.out) / u.str;
  }

  /**
   * The bearers' part of the view, a side's own blocks (every side's with no side given, as a recording is): each block's
   * bearers out and litter cases waiting, in men (strength times its men at full strength), and the battle second the
   * next of its bearers are back (null with none out); the aid post of each block that has had wounded; and the light
   * cases back in the line in the two battle seconds before this one (when, the block, how many men), each told once by
   * its t and block: a battle second is shown only once it is over, so an entry never changes after it is shown (a
   * block's returns in one second are summed), and one of no whole man is left out. Every side's where the view is
   * unfogged (the card knew the defence), as the telephone lines are. Left out with the switch off, and before anyone
   * is hurt.
   */
  bearerView(as?: Side): Pick<BattleView, 'bearers' | 'aid' | 'returns'> {
    if (!this.bearers || !this.litters.size) return {};
    const r1 = (v: number): number => Math.round(v * 10) / 10;
    const ours = (u: LandUnit | undefined): u is LandUnit => !!u && (as === undefined || u.side === as || !this.fog);
    const men = (u: LandUnit, s: number): number => Math.round((s * u.men0) / Math.max(1e-9, u.str0));
    const bearers: NonNullable<BattleView['bearers']> = [], aid: NonNullable<BattleView['aid']> = [], returns: NonNullable<BattleView['returns']> = [];
    for (const [id, l] of [...this.litters].sort((a, c) => a[0] - c[0])) {
      const u = this.unit(id);
      if (!ours(u)) continue;
      aid.push({ side: u.side, unit: id, x: r1(l.aid[0]), y: r1(l.aid[1]) });
      const out = men(u, l.out), waiting = men(u, l.waiting);
      if (out > 0 || waiting > 0) bearers.push({ unit: id, out, waiting, back: l.trips.length ? l.trips[0].back : null });
    }
    const now = Math.floor(this.t + 1e-9);
    for (const r of this.returns) {
      const u = this.unit(r.unit), sec = Math.floor(r.t + 1e-9);
      if (sec >= now || sec < now - 2 || !ours(u)) continue;
      const n = men(u, r.n);
      if (n > 0) returns.push({ t: r.t, unit: r.unit, n });
    }
    return { ...(bearers.length ? { bearers } : {}), ...(aid.length ? { aid } : {}), ...(returns.length ? { returns } : {}) };
  }

  // ---- The end --------------------------------------------------------------------------------------------------

  /** Each side's standing power as a share of both sides' at the start: the balance bar. */
  standing(due?: Map<number, number>): Record<Side, number> {
    const tot = this.power.att + this.power.def;
    const s = { att: 0, def: 0 };
    for (const u of this.units) {
      if (!live(u)) continue;
      const k = u.state === 'routing' ? 0.3 : 1;
      // 'due': damage owed it by the enemy's fighters on call, as it would stand once paid (airDue, for the staff's chance).
      const d = due?.get(u.id), str = d === undefined ? u.str : Math.max(0, u.str - (d / Math.max(1e-9, u.power0)) * u.str0);
      // A corps come up from next door stands at the card's part for it, less the harm it has taken.
      s[u.side] += u.priced === undefined ? (u.power0 * (str / u.str0) * k) / tot : (Math.max(0, u.priced - u.power0 * (1 - str / u.str0)) * k) / tot;
    }
    for (const c of this.coming) s[c.u.side] += c.u.priced! / tot;
    return s;
  }

  private brokenShare(s: Side): number {
    let gone = 0;
    for (const u of this.units) if (u.side === s && (u.state === 'routing' || u.state === 'gone')) gone += weightOf(u);
    return gone / this.power[s];
  }

  /** The share of a side's power worn away on the field (strength lost, before anyone runs). */
  wornShare(s: Side): number {
    let worn = 0;
    for (const u of this.units) if (u.side === s) worn += u.power0 * (1 - u.str / u.str0);
    return worn / this.power[s];
  }

  /**
   * How near an army is to breaking, 0 to 1: its whole worn state weighted by strength against what an army of its
   * side will stand (LAND.spent), or half its power running, whichever is nearer. One block running counts only its
   * own share, so it never takes the army with it while the rest is fresh (ruling 226(2)). The army bar shows this.
   */
  breaking(s: Side): number {
    return this.brink(s).bar;
  }

  /** The bar, with the two shares it is reckoned from and whether the fresh hold keeps it at the brink. */
  private brink(s: Side): { bar: number; worn: number; running: number; holding: boolean } {
    const worn = this.wornShare(s), running = this.brokenShare(s), over = worn / LAND.spent[s];
    // Worn past what it will stand, it still holds while most of it is fresh or has not been in the fight: the bar waits
    // at the brink (b476(1)). A small army held there gives ground with its worn (giveGround).
    const holding = over >= 1 && this.freshShare(s) > LAND.fought;
    return { bar: Math.min(holding ? 0.99 : 1, Math.max(running / LAND.breaks, over)), worn, running, holding };
  }

  /**
   * Why an army's bar stands where it does (b496(2)): the two shares breaking() takes the nearer of, each against its
   * mark, its blocks as their cards read them, and the fresh hold. Worn across the whole line, a large army breaks at
   * LAND.spent of its power lost (13 in 100 for an attack) while every block's card may still read Steady: these are the
   * numbers for the words that say so. Read at the break for the result and the 'broken' event, and as it stands for the
   * view; nothing in the fight reads it.
   */
  breakCause(s: Side): BreakCause {
    const r3 = (x: number): number => Math.round(x * 1000) / 1000;
    const b = this.brink(s), w = b.worn / LAND.spent[s], r = b.running / LAND.breaks;
    const mine = this.units.filter((u) => u.side === s), nearer = r >= w && r > 0 ? 'running' : 'worn';
    const on = mine.filter((u) => u.state !== 'withdrawn'), p0 = on.reduce((a, u) => a + weightOf(u), 0);
    const steady = p0 > 0 ? on.filter((u) => u.state === 'steady').reduce((a, u) => a + weightOf(u), 0) / p0 : 0;
    return {
      side: s, bar: r3(b.bar), by: Math.max(w, r) < 1 ? 'none' : nearer, nearer,
      worn: r3(b.worn), spent: LAND.spent[s], running: r3(b.running), breaks: LAND.breaks,
      fresh: r3(this.freshShare(s)), fought: LAND.fought, small: this.freshHold[s],
      holding: b.holding, held: this.heldFor[s] > 0, heldFor: Math.round(this.heldFor[s] * 10) / 10,
      blocks: mine.length, blocksWorn: mine.filter((u) => u.str < u.str0 * LAND.fresh.str).length, wornAt: LAND.fresh.str,
      routing: mine.filter((u) => u.state === 'routing').length, fled: mine.filter((u) => u.state === 'gone').length,
      steady: r3(steady), stillSteady: steady >= 0.9995 ? 'all' : steady > 0.5 ? 'most' : 'less',
    };
  }

  /**
   * The share of a side's power standing in blocks that have not been in the fight (never met the enemy, and not worn
   * by a blow from afar) or, in an army of LAND.fresh.upTo blocks or fewer, that are fresh: steady, at nine tenths of
   * their strength or more, in contact or not. While more than half the army stands so, one worn block never takes it
   * with it (ruling 226(2); the design director, b476(1) and b480). Not for a defence behind works or in a fort: the
   * works keep its men fresh and the card prices the works already, so the field never prices them twice (b488).
   */
  private freshShare(s: Side): number {
    let n = 0;
    for (const u of this.units) if (u.side === s && fighting(u) && this.fresh(u)) n += weightOf(u);
    // A corps still coming up from next door has not been in the fight: it holds its army at the brink.
    for (const c of this.coming) if (c.u.side === s) n += c.u.priced!;
    return n / this.power[s];
  }

  /** A block that holds its army at the brink (freshShare): fresh, in a small army, or not yet in the fight. */
  private fresh(u: LandUnit): boolean {
    return (this.freshHold[u.side] && u.state === 'steady' && u.str >= u.str0 * LAND.fresh.str) || (!u.met && u.str >= u.str0 * 0.97);
  }

  /**
   * A small army held at the brink by its fresh blocks does not stand there to be ground down (the design director,
   * b496(1)): each block of it no longer fresh gives ground, falling back off the field while the fresh hold, and is
   * struck only on its rearguard as it goes (blow); so is one already running, behind the fresh. The fresh still hold
   * the army until most of it has fought. A block already falling back is not sent again, and says nothing; one the
   * player sends in again is left to the player (gives stays set until it rallies).
   */
  private giveGround(s: Side, left: LandUnit[]): void {
    for (const u of this.units) if (u.side === s && u.state === 'routing') u.gives = true;
    for (const u of left) {
      if (u.gives || this.fresh(u)) continue;
      u.gives = true;
      if (u.order?.kind === 'withdraw') continue;
      // Waiting for help, it ends its wait as it goes; men already let down stay so (hold until relieved).
      if (u.waits) this.endWait(u, 'gives');
      this.send(u, { kind: 'withdraw', x: u.x, y: s === 'att' ? 0 : this.field.h });
      u.engaged = undefined;
      // The UI director's words (16:37Z): "gives ground" read as ground lost.
      this.say({ what: 'land:gives', unit: u.id, side: s, text: `${this.named(u)} is worn out and falls back; the rest of its army holds.`, x: u.x, y: u.y });
    }
  }

  /** Giving ground at the brink, and still on its way back or running (an order of the player's may send it in again). */
  private givingGround(u: LandUnit): boolean {
    return !!u.gives && (u.order?.kind === 'withdraw' || u.state === 'routing');
  }

  private judge(): void {
    for (const s of ['att', 'def'] as Side[]) {
      // Falling back (all fall back), a fort's garrison cannot go with it: the battle ends when the rest are out.
      const fall = this.falling[s] !== null;
      const left = this.units.filter((u) => u.side === s && fighting(u) && !(fall && u.kind === 'fort'));
      if (!left.length) {
        const out = this.units.some((u) => u.side === s && u.state === 'withdrawn');
        // Ordered back, it withdrew while any block of it got out: those caught running on the way are its losses (b474).
        const how = out && (fall || !this.units.some((u) => u.side === s && u.state === 'routing')) ? 'withdrew' : 'destroyed';
        // Driven from the field, it broke: why, as it stood when the last block left the fight (b496(2)).
        this.finish(other(s), how, undefined, how === 'destroyed' ? this.breakCause(s) : undefined);
        return;
      }
      const b = this.brink(s), near = b.bar;
      this.bar[s] = near;
      if (fall) this.fallBar[s] = Math.max(this.fallBar[s], near);
      if (b.holding) this.heldFor[s] += DT;
      // Held at the brink, a small army gives ground with its worn (b496(1)).
      if (b.holding && this.freshHold[s]) this.giveGround(s, left);
      // At three quarters, a line each army's way (the UI director's words for ruling 226(3)): the player's own in amber
      // on the page, the other's one dispatch. With nobody playing, nothing is said.
      if (!this.warned[s] && near >= 0.75 && (this.ticket.att.human || this.ticket.def.human)) {
        this.warned[s] = true;
        // By what drives the bar (the UI director, 14:44Z); the enemy's stays one plain dispatch, with no fields.
        const own = !!this.ticket[s].human, why = own ? this.breakCause(s) : undefined;
        this.say({ what: 'nearBreak', side: s, text: why ? warnLine(why) : `${this.army(s)} is near breaking.`, ...(why ? { brokeBy: why } : {}) });
      }
      // An army breaks when half its power is running or gone, or when it has bled as much as an army of its side
      // will stand (LAND.spent): so a battle fought out costs about what the campaign bills for its result (b356).
      if (near >= 1) {
        this.broke[s] = true;
        // Why it broke, as it stood at the break, for the result, the event and the 3D game (b496(2)).
        const why = this.breakCause(s);
        // The blocks that broke are already running; the rest fall back in order with their army.
        for (const u of left) { u.fellBack = true; u.order = { kind: 'withdraw', x: u.x, y: s === 'att' ? 0 : this.field.h }; u.path = []; u.keepFace = null; u.engaged = undefined; }
        this.say({ what: 'broken', side: s, text: breakLine(why, this.whose(s)), brokeBy: { ...why } });
        this.finish(other(s), 'rout', undefined, why);
        return;
      }
    }
    // The day done with an army still falling back: it withdrew, the rest of it caught on the way (all fall back).
    if (this.t >= this.ticket.limit) {
      if (this.falling.def !== null) this.finish('att', 'withdrew');
      else if (this.falling.att !== null) this.finish('def', 'withdrew');
      else this.finish('def', 'time');
    }
  }

  /**
   * Why this side may not fall back now, in the player's words; null where it may (all fall back, release 1.5). Only once
   * the battle has begun, once, and with a block of it that can go.
   */
  fallBackWhy(s: Side): string | null {
    if (this.over) return 'The battle is over.';
    if (this.setup !== null) return 'The battle has not begun.';
    if (this.falling[s] !== null) return 'Your army is already falling back.';
    if (!this.units.some((u) => u.side === s && fighting(u) && u.kind !== 'fort')) return 'No unit of yours can fall back.';
    return null;
  }

  /** Whether that army has been ordered back (all fall back), for the view and the 3D game. */
  fallingBack(s: Side): boolean { return this.falling[s] !== null; }

  /**
   * Where a withdrawing block of that side is out: a field's breadth from its own edge (6 m), or, with its army falling
   * back, behind the zone it drew up in (Brian: "behind them where they started").
   */
  private outLine(s: Side): number {
    const z = this.field.zones[s];
    if (s === 'att') return this.falling.att !== null ? Math.max(6, z[1]) : 6;
    return this.falling.def !== null ? Math.min(this.field.h - 6, z[3]) : this.field.h - 6;
  }

  /**
   * What a block keeps of its strength in the day's count: the fled leave LAND.count.gone behind them, runners and those
   * that fell back in order LAND.count.routing. A defence's runner caught in the water at the end is booked as fled (the
   * design director, b608(3)): it has not got back in order, and a stand with the water at its back carries its price at
   * the break. An attack's runner back in a river it came over counts as before: the card's crossing prices that.
   */
  private kept(u: LandUnit): number {
    const caught = u.side === 'def' && u.state === 'routing' && inRiver(groundAt(this.field, u.x, u.y));
    return u.state === 'gone' || caught ? u.str * (1 - LAND.count.gone[u.side]) : u.state === 'routing' || u.fellBack ? u.str * (1 - LAND.count.routing[u.side]) : u.str;
  }

  /** Each side's share of its starting power lost: strength worn away, and what runners and the fled leave behind them. */
  lostShares(): Record<Side, number> {
    const lost = { att: 0, def: 0 };
    for (const u of this.units) {
      const kept = this.kept(u);
      lost[u.side] += u.power0 * (1 - kept / u.str0);
    }
    return { att: lost.att / this.power.att, def: lost.def / this.power.def };
  }

  private finish(winner: Side, how: FieldResult['how'], R?: number, why?: BreakCause): void {
    if (this.over) return;
    // What the fighters on call still owe is paid where it is owed before the day's count (air support): the card counted
    // their share, and the field delivers all of it, however soon the day ends.
    this.payAir();
    // So does what every wait's cover spared (hold until relieved, release 2).
    this.payWaits();
    const lost = this.lostShares();
    // Men counted as the shares are, with what the runners and the fled leave behind, so the two never tell two stories.
    const men = { att: 0, def: 0 };
    for (const u of this.units) {
      const kept = this.kept(u);
      men[u.side] += Math.round(u.men0 * (1 - kept / u.str0));
    }
    const k = this.ticket.reckoning;
    const r = R ?? clampToDice(fieldRatio(winner, lost), k.odds, k.dice, winner);
    // The campaign's verdict follows R: under the dice's clamp it can differ from who held the field.
    const verdict: Side = r >= RATIO.carry ? 'att' : 'def';
    const forces = new Map<number, { corps: number; side: Side; lost: number; p: number }>();
    for (const u of this.units) {
      const f = forces.get(u.corps) ?? { corps: u.corps, side: u.side, lost: 0, p: 0 };
      f.lost += u.power0 * (1 - u.str / u.str0); f.p += u.power0;
      forces.set(u.corps, f);
    }
    // A corps from next door that never came up: nothing lost on the field.
    for (const c of this.coming) if (!forces.has(c.u.corps)) forces.set(c.u.corps, { corps: c.u.corps, side: c.u.side, lost: 0, p: 0 });
    const loser = other(winner);
    // "about 4%", or under one in a hundred "less than 1%", never "about 0%" (the UI director's words, b358).
    // Where nothing was lost at all, it says "none" (b408).
    const pct = (p: number): string => (p < 1e-4 ? 'none' : p < 0.005 ? 'less than 1%' : `about ${Math.round(p * 100)}%`);
    const cap = (t: string): string => t[0].toUpperCase() + t.slice(1);
    const line = this.practice && this.ticket.def.human ? 'Your line' : this.practice ? 'Their line' : `The ${this.ticket.def.adj} line`;
    // Broken, when and why in one line (the UI director, 18:26Z: "broke" said twice read as two events): the break's
    // figure is its men along the line at the break; the strength line under it is the whole day's, the rout counted.
    const told = how === 'rout' && why && why.by !== 'none';
    const lines = [
      how === 'rout' ? this.brokeAfter(loser, told ? why : undefined)
        : how === 'withdrew' && this.falling[loser] !== null ? `${this.army(loser)} fell back behind the ground it started from, to fight another day.`
        : how === 'withdrew' ? `${this.army(loser)} withdrew from the field.`
          : how === 'time' ? `${line} held until the day was done.`
            : how === 'destroyed' ? `${this.army(loser)} was driven from the field.`
              : this.staffLine(),
    ];
    // Handed over, the field's figure is only the part before the staff took it: it says so, as it reads beside the
    // campaign's bill for the whole day (the design director, b356). Handed over before a shot, the staff's line says so
    // already, and there is no figure to give (the UI director, after the retest).
    if (how !== 'objective') lines.push(lost.att < 1e-4 && lost.def < 1e-4 ? 'Neither side lost any of its strength.' : (told ? `By the day's end, counting the rout, ${this.sideOf('att', 'attack')} had lost ${pct(lost.att)} of its strength in all, ${this.sideOf('def', 'defence')} ${pct(lost.def)}.` : `${cap(this.sideOf('att', 'attack'))} lost ${pct(lost.att)} of its strength, ${this.sideOf('def', 'defence')} ${pct(lost.def)}.`));
    else if (this.contactAt !== null) lines.push(`Before you handed the battle to the staff, ${this.sideOf('att', 'attack')} had lost ${pct(lost.att)} of its strength on the field, and ${this.sideOf('def', 'defence')} ${pct(lost.def)}.`);
    // The battle is over: nothing marches on (the blocks' cards and the 3D models alike), and none is left standing in the
    // river: one still wading it is set on the nearer bank (b464(5)). The figures above are already counted.
    for (const u of this.units) if (live(u) && u.state !== 'routing') { u.act = 'idle'; u.speed = 0; u.engaged = undefined; }
    for (const u of this.units) if (live(u) && inRiver(groundAt(this.field, u.x, u.y))) { [u.x, u.y] = this.shore(u, 0); u.cover = groundAt(this.field, u.x, u.y); }
    this.over = {
      winner: verdict, how, seconds: Math.round(this.t), lost, men, R: Math.round(r * 1000) / 1000,
      level: levelOf(r, men.att > men.def), lines,
      forces: [...forces.values()].map((f) => ({ corps: f.corps, side: f.side, lost: f.p > 0 ? Math.round((f.lost / f.p) * 1000) / 1000 : 0, ...(this.joinCorps.has(f.corps) ? { arrived: this.joinedAt.get(f.corps) ?? null } : {}) })),
      ...(how === 'rout' || how === 'destroyed' ? { broke: loser, ...(why ? { brokeBy: why } : {}) } : {}),
      // Pulled out after the armies met: the rearguard and the stragglers are the rules' to bill, by its bar (b474).
      // Ordered back (all fall back), the highest its bar stood from the order until its last block was out (DD 14:54Z).
      ...(how === 'withdrew' && this.contactAt !== null ? { pulled: { side: loser, bar: Math.round(Math.min(1, this.falling[loser] !== null ? Math.max(this.fallBar[loser], this.bar[loser]) : this.bar[loser]) * 1000) / 1000 } } : {}),
      ...(how === 'withdrew' && this.falling[loser] !== null ? { fellBack: loser } : {}),
      // What the fighters on call flew and lost, by group, for the campaign's bill (air support).
      ...(this.airOn.att || this.airOn.def ? { air: this.airBill() } : {}),
    };
    this.strikes = [];
    if (verdict !== winner) this.over.lines.push('The day went their way, but the campaign counts it as close as the odds allowed.');
    this.say({ what: 'end', text: lines[0] });
  }

  /**
   * The staff's line, in its three forms (the UI director's words for ruling 226(5)): before any contact, the whole
   * attack; after it, the rest, and "what you had won stays won" only where the field had moved the player's way from
   * where the card put it, by five points of the balance or more.
   */
  private staffLine(): string {
    // A practice ground is not a place: the staff settle the battle there, not the attack on it (b408).
    const what = this.practice ? 'battle' : `attack on ${this.ticket.place.name}`;
    // Before the armies met: true whether or not the guns had opened (b409, the UI director's words).
    if (this.contactAt === null) return `The staff settled the whole ${what}: the armies had not met.`;
    const me: Side = this.ticket.def.human && !this.ticket.att.human ? 'def' : 'att';
    const bal = this.standing(), tot = this.power.att + this.power.def;
    // Moved his way by five points of the balance's share or more (the design director, 20:51): a point's drift is not a gain.
    const moved = (bal[me] / Math.max(1e-9, bal.att + bal.def)) >= this.power[me] / tot + 0.05;
    return `The staff settled the rest of the ${what} from where it stood${moved ? ': what you had won stays won' : ''}.`;
  }

  /**
   * How long it broke after: the field's own time from the first contact to the break (ruling 226(6)), to the half
   * minute (b464(6)). Whole minutes made everything from half a minute to a minute and a half "a minute", the span most
   * routs at odds the card calls sure or hopeless fall in.
   */
  private brokeAfter(s: Side, why?: BreakCause): string {
    const sec = this.t - (this.contactAt ?? this.t), who = this.army(s);
    // "Of fighting" on every form (the UI director, b464).
    const halves = Math.round(sec / 30), m = Math.floor(halves / 2), half = halves % 2 === 1;
    const n = ['', 'a', 'two', 'three', 'four', 'five', 'six'][m] ?? `${m}`;
    const span = halves === 0 ? 'within half a minute' : `after ${m === 0 ? 'half a minute' : m === 1 ? (half ? 'a minute and a half' : 'a minute') : `${n}${half ? ' and a half' : ''} minutes`}`;
    const when = `${who} broke ${span} of fighting`;
    // (and why, where told, in the same line: breakLine's past words after its "broke")
    if (!why) return `${when}.`;
    const said = breakLine(why, who, true);
    const after = said.startsWith(`${who} broke`) ? said.slice(`${who} broke`.length) : `: ${said}`;
    return `${when}${after.startsWith(':') ? after : `,${after}`}`;
  }

  /** Each side's standing power against what it brought, as the staff reckons the day now. */
  private standingShares(due?: Map<number, number>): Record<Side, number> {
    const st = this.standing(due), tot = this.power.att + this.power.def;
    return { att: st.att / (this.power.att / tot), def: st.def / (this.power.def / tot) };
  }

  /**
   * The staff's chance now that the attack carries the ground, were the battle handed to them this moment: the dice's
   * own chance at the ratio standing, held to their run at the card's odds (what toDice would roll). For HAND IT TO THE
   * STAFF's question (ruling 226(5)).
   */
  chanceNow(): number {
    // As toDice would roll it: what the fighters on call still owe paid first, and what waits' cover spared.
    const k = this.ticket.reckoning, sh = this.standingShares(this.dueNow());
    if (k.odds * k.dice[0] >= RATIO.carry) return 1;
    if (k.odds * k.dice[1] < RATIO.carry) return 0;
    const now = k.odds * (Math.max(1e-6, sh.att) / Math.max(1e-6, sh.def));
    return Math.max(0, Math.min(1, (k.dice[1] - RATIO.carry / Math.max(1e-6, now)) / (k.dice[1] - k.dice[0])));
  }

  /**
   * The staff's chance at a card's own odds, the fight having moved the balance since the card: for the staff's band
   * where the card did not know the defence (b453), so HAND IT TO THE STAFF gives a range and never the true chance.
   */
  chanceAt(card: number): number {
    const k = this.ticket.reckoning, bal = this.standing(this.dueNow()), r = bal.att / Math.max(1e-6, bal.def) / Math.max(1e-6, k.odds);
    if (card * k.dice[0] >= RATIO.carry) return 1;
    if (card * k.dice[1] < RATIO.carry) return 0;
    const now = card * r;
    return Math.max(0, Math.min(1, (k.dice[1] - RATIO.carry / Math.max(1e-6, now)) / (k.dice[1] - k.dice[0])));
  }

  /** Hand it to the staff (ruling 217(4), the button named in b348): the rest is rolled on the dice, at the ratio then standing. */
  toDice(): void {
    if (this.over) return;
    const k = this.ticket.reckoning;
    // The strikes not yet flown are the staff's to fly in the settlement: the bill counts them (the card priced them).
    for (const s of ['att', 'def'] as Side[]) if (this.airOn[s]) this.airSettled[s] = this.airLeft[s];
    this.payAir();
    this.payWaits();
    const R = diceNow(k.odds, k.dice, this.standingShares(), this.rng.next());
    this.finish(R >= RATIO.carry ? 'att' : 'def', 'objective', R);
  }

  // ---- Air support ------------------------------------------------------------------------------------------------
  //
  // Brian, 2026-11-18 18:58Z: "during ground battles where fighters are in range, you should be able to call in air
  // support where the fighters will strafe and drop bombs. the fighter squadron has to be on ground support mode where
  // they can be called in for missions like this that turn." The design director, b533: counted on the card, placed by
  // the player, shown as pictures. The campaign counts a side's fighters on call in its sky term (SideTicket.air.share:
  // the part of its strength they made); on the field, while the side has strikes left to call, its blows hold that
  // share back, owed to the block each fell on, and each strike pays it out where its run lands. So the field never
  // adds to the card (b418): the strikes carry part of the strength the card counted, and only where they land is the
  // player's. The first strike is ready LAND.air.first after the armies meet, each next LAND.air.every after the last
  // call; one left ready LAND.air.wait the staff call themselves, for either side, at the blocks owed most, paying
  // each what is owed (b533(c): the enemy's strikes are the staff's, never aimed sharper). A run is a strip of ground
  // its guns rake, never within LAND.air.close of the caller's own men (no friendly fire, b533(d)). Planes are pictures:
  // no flight model, no aiming, no gunnery (air-battle-paused); a plane lost is the campaign's price for a strike
  // (AirOnCall.groups[].lose), shown as it falls. Nothing here draws on the battle's dice: where neither side has
  // fighters on call, the battle is exactly as it was.

  /** Whether this side has fighters on call over this battle. */
  hasAir(s: Side): boolean { return this.airOn[s] !== null; }

  /** This side's strike in the air, if any: one flight's strike at a time. */
  private flying(s: Side): boolean { return this.strikes.some((k) => k.side === s && !k.ended); }

  /** Whether this side has a strike ready to call now. */
  airReady(s: Side): boolean { return this.airLeft[s] > 0 && this.t >= this.airNext[s] && !this.flying(s) && !this.over; }

  /**
   * Why this side cannot call a strike down this run now, in the UI director's words to be (b533(d)), or null where it
   * can: none on call; all flown; before the armies meet; the next not ready yet, or one already in the air; or the run
   * too close to its own men to fire on enough of it.
   */
  airWhyNot(s: Side, run?: [number, number, number, number]): string | null {
    if (!this.airOn[s]) return 'No fighters on call';
    if (this.airLeft[s] <= 0) return 'All strikes flown';
    if (this.contactAt === null) return 'After the armies meet';
    if (this.flying(s)) return 'A strike is on its way';
    if (this.t < this.airNext[s]) { const w = Math.ceil(this.airNext[s] - this.t); return `Ready in ${Math.floor(w / 60)}:${String(w % 60).padStart(2, '0')}`; }
    if (run && this.clearShare(s, run) < LAND.air.least) return 'Too close to your own men';
    return null;
  }

  /**
   * The run a strike flies when the player points at a place rather than drawing one: down the frontage of the enemy
   * block nearest the point, from one end to the other (at least LAND.air.minRun long); with none within LAND.air.near,
   * a run of that length across the field through the point.
   */
  airRunAt(s: Side, x: number, y: number): [number, number, number, number] {
    // Only a block the player knows of (b464(2)): a run that found one he has not would tell him where it is.
    const foes = this.units.filter((e) => e.side !== s && live(e) && e.kind !== 'fort' && this.known(e) && !e.hid);
    const near = foes.reduce<LandUnit | null>((m, e) => (dist(e.x, e.y, x, y) < (m ? dist(m.x, m.y, x, y) : LAND.air.near) ? e : m), null);
    return near ? this.frontageRun(near) : this.clipRun([x - LAND.air.minRun / 2, y, x + LAND.air.minRun / 2, y]);
  }

  /** A run drawn by hand, as it will be flown: kept on the field and to its longest (what callAir flies). */
  airRun(r: [number, number, number, number]): [number, number, number, number] { return this.clipRun(r); }

  /** A run down a block's frontage, its whole width, at least LAND.air.minRun. */
  private frontageRun(b: LandUnit): [number, number, number, number] {
    const [cx, cy] = ahead(b.face + Math.PI / 2), half = Math.max(b.w, LAND.air.minRun) / 2;
    return this.clipRun([b.x - cx * half, b.y - cy * half, b.x + cx * half, b.y + cy * half]);
  }

  /** A run kept on the field and to its longest, about its middle. */
  private clipRun(r: [number, number, number, number]): [number, number, number, number] {
    let [x0, y0, x1, y1] = r;
    const len = dist(x0, y0, x1, y1);
    if (len > LAND.air.len) { const mx = (x0 + x1) / 2, my = (y0 + y1) / 2, k = LAND.air.len / len; x0 = mx + (x0 - mx) * k; y0 = my + (y0 - my) * k; x1 = mx + (x1 - mx) * k; y1 = my + (y1 - my) * k; }
    const w = this.field.w, h = this.field.h;
    return [clamp(x0, 0, w), clamp(y0, 0, h), clamp(x1, 0, w), clamp(y1, 0, h)].map((v) => Math.round(v * 10) / 10) as [number, number, number, number];
  }

  /** The points a run's guns rake, one a metre-step apart along it, at least two. */
  private runPoints(r: [number, number, number, number], n: number): [number, number][] {
    const out: [number, number][] = [];
    for (let i = 0; i < n; i++) { const f = (i + 0.5) / n; out.push([r[0] + (r[2] - r[0]) * f, r[1] + (r[3] - r[1]) * f]); }
    return out;
  }

  /** Whether a point is within LAND.air.close of this side's own men: its guns hold fire there. */
  private overOwn(s: Side, x: number, y: number): boolean {
    return this.units.some((u) => u.side === s && live(u) && dist(u.x, u.y, x, y) - Math.max(u.w, u.d) / 2 < LAND.air.close);
  }

  /** The share of a run its guns may fire on, clear of the side's own men. */
  private clearShare(s: Side, r: [number, number, number, number]): number {
    const pts = this.runPoints(r, 20);
    return pts.filter(([x, y]) => !this.overOwn(s, x, y)).length / pts.length;
  }

  /** The enemy blocks a run's guns rake at a point: those whose ground the strip LAND.air.wide across covers there. */
  private underRun(s: Side, x: number, y: number): LandUnit[] {
    const half = LAND.air.wide / 2;
    return this.units.filter((e) => {
      if (e.side === s || !live(e) || e.kind === 'fort') return false;
      const dx = x - e.x, dy = y - e.y, c = Math.cos(e.face), sn = Math.sin(e.face);
      return Math.abs(dx * c - dy * sn) <= e.w / 2 + half && Math.abs(dx * sn + dy * c) <= e.d / 2 + half;
    });
  }

  /** Call a strike down this run (the player's), or the staff's where run is left out. False where it cannot be called. */
  callAir(s: Side, run?: [number, number, number, number]): boolean {
    const r = run ? this.clipRun(run) : undefined;
    if (this.airWhyNot(s, r) !== null) return false;
    const on = this.airOn[s]!, bank = this.airBank[s];
    const legs: Leg[] = [];
    // A flight starts where its run's line puts it LAND.air.eta out, as flyStrike flies it in.
    const flight = (r: [number, number, number, number], models: number): Flight => {
      const len = Math.max(1, dist(r[0], r[1], r[2], r[3])), back = LAND.air.eta * LAND.air.speed;
      return { id: ++this.flightIds, side: s, nat: on.groups[this.airCalls[s] % on.groups.length].nat, year: this.year(), models, x: r[0] - ((r[2] - r[0]) / len) * back, y: r[1] - ((r[3] - r[1]) / len) * back, alt: LAND.air.alt, face: bearing(r[0], r[1], r[2], r[3]), phase: 'in', run: r };
    };
    const leg = (f: Flight, aimed: boolean): Leg => ({ flight: f, owed: new Map(), nerve: new Map(), total: 0, aimed, done: 0, struck: new Set(), dropped: 0, released: 0, away: false });
    if (r) {
      // The player's: all the bank, spread down his run.
      let total = 0;
      for (const v of bank.values()) total += v;
      const l = leg(flight(r, LAND.air.models), true);
      l.owed = new Map(bank); l.nerve = new Map(this.airNerve[s]); l.total = total;
      legs.push(l);
    } else {
      // The staff's: down the frontage of each block owed most, a plane or more each; what is owed a block past the
      // last goes with the leg nearest it.
      const live1 = (id: number): LandUnit | undefined => { const u = this.unit(id); return u && live(u) ? u : undefined; };
      const owed = [...bank].filter(([id, v]) => v > 0 && live1(id)).sort((a, b) => b[1] - a[1] || a[0] - b[0]);
      if (!owed.length) return false;
      const top = owed.slice(0, LAND.air.legs), n = top.length;
      top.forEach(([id], i) => {
        const b = this.unit(id)!;
        legs.push(leg(flight(this.frontageRun(b), Math.floor(LAND.air.models / n) + (i < LAND.air.models % n ? 1 : 0)), false));
      });
      for (const [id, v] of owed) {
        const b = this.unit(id)!;
        let at = 0;
        top.forEach(([tid], i) => { const t = this.unit(tid)!, a = this.unit(top[at][0])!; if (dist(t.x, t.y, b.x, b.y) < dist(a.x, a.y, b.x, b.y)) at = i; });
        legs[at].owed.set(id, v); legs[at].nerve.set(id, this.airNerve[s].get(id) ?? v); legs[at].total += v;
      }
      // Blocks owed that have left the field still pay it, as the count of the day still counts them.
      for (const [id, v] of bank) if (v > 0 && !live1(id)) this.offField(s, id, v);
    }
    bank.clear();
    this.airNerve[s].clear();
    const bombs = this.year() >= (LAND.air.bombsFrom[on.groups[this.airCalls[s] % on.groups.length].nat] ?? LAND.air.bombsFrom._);
    this.strikes.push({ side: s, by: r ? 'you' : 'staff', group: this.airCalls[s] % on.groups.length, nth: this.airCalls[s] + 1, called: this.t, legs, bombs, ended: false, lost: false });
    this.airCalls[s]++;
    this.airLeft[s]--;
    this.airNext[s] = this.airLeft[s] > 0 ? this.t + LAND.air.every : Infinity;
    const human = this.ticket[s].human;
    // The staff's call on the player's side says so; his own needs no word, nor does the enemy's (he sees its planes).
    this.say({ what: 'land:air-call', side: s, text: human && !r ? 'The staff called in the fighters.' : '', x: legs[0].flight.run[0], y: legs[0].flight.run[1], extra: { flights: legs.map((l) => l.flight.id), runs: legs.map((l) => l.flight.run), eta: LAND.air.eta, by: r ? 'you' : 'staff' } });
    return true;
  }

  /** The year the ticket's date falls in: which fighters fly, and whether they carry bombs. */
  private year(): number { const m = /(\d{4})/.exec(this.ticket.date); return m ? Number(m[1]) : 1939; }

  /**
   * Each tick: the next strike comes ready once the armies have met; one left ready the staff call; and each strike in
   * the air comes in, makes its run, paying what it carries where it lands, and pulls away.
   */
  private flyAir(): void {
    for (const s of ['att', 'def'] as Side[]) {
      const on = this.airOn[s];
      if (!on) continue;
      if (this.contactAt !== null && this.airNext[s] === Infinity && this.airLeft[s] > 0 && this.airCalls[s] === 0) this.airNext[s] = this.contactAt + LAND.air.first;
      if (this.airReady(s) && this.t >= this.airNext[s] + LAND.air.wait) this.callAir(s);
    }
    const hurt = new Map<number, [number, number]>();
    for (const k of this.strikes) if (!k.ended) this.flyStrike(k, hurt);
    for (const [id, [d, n]] of hurt) this.suffer(this.unit(id)!, d, d > 0 ? (n / d) * LAND.air.nerve : LAND.air.nerve, []);
    this.strikes = this.strikes.filter((k) => !k.ended);
  }

  /** One strike's tick, its legs flown together. What each leg lands this tick is added to 'hurt'. */
  private flyStrike(k: Strike, hurt: Map<number, [number, number]>): void {
    const A = LAND.air, at = k.called + A.eta, led = this.airLedger[k.side];
    let over = true;
    for (const l of k.legs) {
      const r = l.flight.run, len = Math.max(1, dist(r[0], r[1], r[2], r[3])), secs = Math.max(DT, len / A.speed), [dx, dy] = [(r[2] - r[0]) / len, (r[3] - r[1]) / len];
      const f = l.flight;
      if (this.t < at) {
        // Coming in down the line of its run, from its own side, coming down to the height of the run.
        const back = (at - this.t) * A.speed, k0 = (at - this.t) / A.eta;
        f.phase = 'in'; f.x = r[0] - dx * back; f.y = r[1] - dy * back; f.alt = A.low + (A.alt - A.low) * k0;
        over = false;
        continue;
      }
      if (l.done < 1) {
        if (f.phase === 'in') {
          // Over the start of its run: down onto it, and its guns open down the strip for the run's length.
          f.phase = 'run';
          this.say({ what: 'land:air-in', side: k.side, text: '', x: r[0], y: r[1], extra: { flight: f.id, run: r, models: f.models, alt: A.low, nat: f.nat, year: f.year, heading: f.face }, cue: 'fighter_pass' });
          this.say({ what: 'land:strafe', side: k.side, text: '', x: (r[0] + r[2]) / 2, y: (r[1] + r[3]) / 2, extra: { flight: f.id, from: [r[0], r[1]], to: [r[2], r[3]], width: A.wide, secs: Math.round(secs * 10) / 10 }, cue: 'machine_gun' });
        }
        const step = Math.min(1 - l.done, DT / secs), from = l.done;
        l.done = Math.min(1, l.done + step);
        f.x = r[0] + (r[2] - r[0]) * l.done; f.y = r[1] + (r[3] - r[1]) * l.done; f.alt = A.low;
        if (l.aimed) {
          // The player's run: this tick's share of all it carries, on the blocks under the strip here; where none, or
          // over his own men, it is lost.
          const share = l.total * step, mx = r[0] + (r[2] - r[0]) * (from + step / 2), my = r[1] + (r[3] - r[1]) * (from + step / 2);
          const on = this.overOwn(k.side, mx, my) ? [] : this.underRun(k.side, mx, my);
          if (!on.length) led.wasted += share;
          else for (const e of on) this.land(k.side, e, share / on.length, hurt, l, l.total > 0 ? this.sumOf(l.nerve) / l.total : 1);
        } else {
          // The staff's: each block it carries for, its share of this tick, wherever it stands; one gone, lost.
          for (const [id, v] of l.owed) { const e = this.unit(id)!; if (live(e)) this.land(k.side, e, v * step, hurt, l, v > 0 ? (l.nerve.get(id) ?? v) / v : 1); else this.offField(k.side, id, v * step); }
        }
        // Bombs on the far third, pictures only (b533(f)): each let go LAND.air.toss before it lands ('land:bomb-drop'),
        // bursting where it lands (a 'shell' of class 'bomb').
        if (k.bombs) {
          const fbOf = (i: number): number => 2 / 3 + ((i + 0.5) / A.bombs) / 3;
          for (let i = l.released; i < A.bombs; i++) {
            const fb = fbOf(i), fr = Math.max(0, fb - (A.toss * A.speed) / len);
            if (l.done < fr) break;
            this.say({ what: 'land:bomb-drop', side: k.side, text: '', x: Math.round(f.x * 10) / 10, y: Math.round(f.y * 10) / 10, extra: { flight: f.id, from: { x: Math.round(f.x * 10) / 10, y: Math.round(f.y * 10) / 10, alt: A.low }, to: { x: Math.round((r[0] + (r[2] - r[0]) * fb) * 10) / 10, y: Math.round((r[1] + (r[3] - r[1]) * fb) * 10) / 10 }, at: Math.round((this.t + Math.max(0, fb - l.done) * secs) * 10) / 10 } });
            l.released = i + 1;
          }
          for (let i = l.dropped; i < l.released; i++) { const fb = fbOf(i); if (l.done >= fb) { this.bomb(k.side, r[0] + (r[2] - r[0]) * fb, r[1] + (r[3] - r[1]) * fb, f); l.dropped = i + 1; } }
        }
        if (l.done >= 1) {
          // The run made: what it struck, in the dispatches (blocks the player knows only).
          this.say({ what: 'land:strafed', side: k.side, text: this.strafeWords(k.side, l), x: (r[0] + r[2]) / 2, y: (r[1] + r[3]) / 2, extra: { flight: f.id, on: [...l.struck].filter((id) => this.known(this.unit(id)!)) } });
        }
        over = false;
        continue;
      }
      // Pulling away: on along the run and up.
      const gone = this.t - (at + secs);
      if (gone < A.out) { f.phase = 'out'; f.x = r[2] + dx * gone * A.speed; f.y = r[3] + dy * gone * A.speed; f.alt = A.low + (A.alt - A.low) * (gone / A.out); over = false; }
      else l.away = true;
    }
    // The planes the strike cost fall as the run ends, over the field (a picture of the price, below).
    if (!k.lost && k.legs.every((l) => l.done >= 1)) { k.lost = true; this.planesLost(k); }
    if (over) k.ended = true;
  }

  /**
   * A share of a strike, in open-ground terms, landing on an enemy block: what it takes off it for its ground, and the
   * nerve it costs as the blows it was held from would have (nerve: their weight on nerve, a unit of what was held).
   */
  private land(s: Side, e: LandUnit, raw: number, hurt: Map<number, [number, number]>, l: Leg, nerve: number): void {
    const led = this.airLedger[s];
    const d = raw * this.groundTake(e) * (e.kind === 'arm' && l.aimed ? LAND.air.armour : 1);
    led.landed += raw; led.dealt += d;
    // His fighters have seen what they struck (b464(2)): it is known from now on, as a block met is.
    if (this.ticket[s].human) e.found = true;
    const h = hurt.get(e.id) ?? [0, 0];
    h[0] += d; h[1] += d * nerve;
    hurt.set(e.id, h);
    l.struck.add(e.id);
  }

  private sumOf(m: Map<number, number>): number { let n = 0; for (const v of m.values()) n += v; return n; }

  /** A bomb, pictures only: a 'shell' of class 'bomb' where it lands, and its crater (shells.ts). */
  private bomb(s: Side, x: number, y: number, f: Flight): void {
    x = Math.round(clamp(x, 2, this.field.w - 2) * 10) / 10; y = Math.round(clamp(y, 2, this.field.h - 2) * 10) / 10;
    // Read only from a block the player knows (b464(2)): over one he does not, a bomb bursts as on its ground.
    const hit = this.underRun(s, x, y)[0], on = hit && this.known(hit) ? hit : undefined, ground = groundAt(this.field, x, y), armour = on?.kind === 'arm', at = Math.round(this.t * 10) / 10;
    const ev: FieldEvent = { t: at, what: 'shell', text: '', x, y, cal: 'bomb', armour: !!armour, ground, reach: REACH.bomb, extra: { flight: f.id } };
    if (on) ev.on = on.id;
    this.events.push(ev);
    this.drill?.shelled(x, y);
    if (!armour && !inRiver(ground)) this.craters.push({ id: -(this.craters.length + 1), kind: 'crater', x, y, r: CRATER.bomb / 2, until: this.ticket.limit, at, cal: 'bomb' });
  }

  /**
   * The words a strafing run leaves in the dispatches, for the player's side only and never naming a block he does not
   * know (b464(2)): his own run, the blocks it struck; theirs, his blocks it struck. The UI director's words (21:18Z):
   * "strafed and bombed" where the run let bombs go.
   */
  private strafeWords(s: Side, l: Leg): string {
    const me: Side | null = this.ticket.att.human ? 'att' : this.ticket.def.human ? 'def' : null;
    if (!me) return '';
    const hit = [...l.struck].map((id) => this.unit(id)!), did = l.dropped > 0 ? 'strafed and bombed' : 'strafed';
    if (s === me) {
      const seen = hit.filter((u) => this.known(u));
      return !hit.length ? 'Your fighters found nothing under their run.' : seen.length ? `Your fighters ${did} ${seen.map((u) => this.named(u)).join(' and ')}.` : `Your fighters ${did} the enemy line.`;
    }
    const mine = hit.filter((u) => u.side === me);
    return mine.length ? `${this.ticket[s].adj} fighters ${did} your ${mine.map((u) => u.name).join(' and your ')}.` : '';
  }

  /**
   * The planes a strike cost its group, as the campaign prices a strike (AirOnCall.groups[].lose: no dice, fought or
   * settled by the staff, the same): each whole plane that price comes to falls, a picture of the loss ('air:lost').
   */
  private planesLost(k: Strike): void {
    // Every group on call flies every strike, as the campaign books it (bookCloseAir: each group the card's strikes).
    let n = 0;
    for (const g of this.airOn[k.side]!.groups) {
      const falls = Math.floor(g.lose * k.nth + 1e-9) - Math.floor(g.lose * (k.nth - 1) + 1e-9);
      for (let i = 0; i < falls; i++, n++) {
        const f = k.legs[n % k.legs.length].flight;
        const m = mix(f.id * 7919 + n * 131 + 17), kind = m < 0.3 ? 'explode' : m < 0.7 ? 'burn' : 'spin';
        this.say({ what: 'air:lost', side: k.side, text: this.lostWords(k.side), x: Math.round(f.x * 10) / 10, y: Math.round(f.y * 10) / 10, extra: { flight: f.id, kind, chutes: kind !== 'explode' && mix(f.id * 613 + n * 7) < 0.5 ? 1 : 0, over: k.side === 'att' ? 'enemy' : 'own', air: 'ftr', alt: Math.round(f.alt) }, cue: 'plane_down' });
      }
    }
  }

  /** A fighter down, in the player's dispatches (the UI director, 21:18Z): his own to ground fire, or theirs to his men. */
  private lostWords(s: Side): string {
    const me: Side | null = this.ticket.att.human ? 'att' : this.ticket.def.human ? 'def' : null;
    if (!me) return '';
    if (s === me) return 'Ground fire brought down one of your fighters.';
    const adj = this.ticket[s].adj, word = adj === 'Enemy' ? 'enemy' : adj;
    return `Your men brought down ${/^[AEIOUaeiou]/.test(word) ? 'an' : 'a'} ${word} fighter.`;
  }

  /** What each side's fighters flew and lost, by group, for the campaign's bill (FieldResult.air). */
  private airBill(): FieldResult['air'] {
    const out: FieldResult['air'] = {};
    for (const s of ['att', 'def'] as Side[]) {
      const on = this.airOn[s];
      if (!on) continue;
      // Each group flies every strike (bookCloseAir's rule); handed to the dice, the strikes left are the staff's to fly.
      const n = this.airCalls[s] + this.airSettled[s];
      out[s] = on.groups.map((g): AirFlown => ({ group: g.id, sorties: n, lost: Math.round(g.lose * n * 1000) / 1000 }));
    }
    return out;
  }

  /**
   * Handed to the staff, or the day done: what the bank still owes and what strikes in the air still carry is paid
   * where it is owed, at once (the staff settle the rest from where the field stands, the air's share in it).
   */
  private payAir(): void {
    if (!this.airOn.att && !this.airOn.def) return;
    const hurt = new Map<number, [number, number]>();
    for (const { s, id, raw, nerve } of this.airDues()) {
      const e = this.unit(id), led = this.airLedger[s];
      if (!e) { led.wasted += raw; continue; }
      if (!live(e)) { this.offField(s, id, raw); continue; }
      const d = raw * this.groundTake(e), h = hurt.get(id) ?? [0, 0];
      led.landed += raw; led.dealt += d; h[0] += d; h[1] += d * nerve; hurt.set(id, h);
    }
    for (const s of ['att', 'def'] as Side[]) { this.airBank[s].clear(); this.airNerve[s].clear(); this.airLeft[s] = 0; }
    for (const k of this.strikes) { for (const l of k.legs) l.done = Math.max(l.done, 1); k.ended = true; }
    for (const [id, [d, n]] of hurt) this.suffer(this.unit(id)!, d, d > 0 ? n / d : 1, []);
  }

  /**
   * What the fighters on call still owe, each side's, block by block: the bank, and what strikes in the air carry for
   * the rest of their runs (an aimed run's, shared as the bank it took was). Read only: payAir pays it, airDue projects it.
   */
  private airDues(): { s: Side; id: number; raw: number; nerve: number }[] {
    const out: { s: Side; id: number; raw: number; nerve: number }[] = [];
    for (const s of ['att', 'def'] as Side[]) {
      if (!this.airOn[s]) continue;
      for (const [id, v] of this.airBank[s]) out.push({ s, id, raw: v, nerve: v > 0 ? (this.airNerve[s].get(id) ?? v) / v : 1 });
      for (const k of this.strikes) if (k.side === s && !k.ended) for (const l of k.legs) {
        const left = 1 - l.done;
        if (left <= 0) continue;
        const sum = this.sumOf(l.owed);
        for (const [id, v] of l.owed) out.push({ s, id, raw: (l.aimed ? l.total * (sum > 0 ? v / sum : 0) : v) * left, nerve: v > 0 ? (l.nerve.get(id) ?? v) / v : 1 });
      }
    }
    return out;
  }

  /** The damage each block on the field would take were what is owed paid now (for the staff's chance), or nothing. */
  private airDue(): Map<number, number> | undefined {
    if (!this.airOn.att && !this.airOn.def) return undefined;
    const due = new Map<number, number>();
    for (const { id, raw } of this.airDues()) {
      const e = this.unit(id);
      if (e && live(e)) due.set(id, (due.get(id) ?? 0) + raw * this.groundTake(e));
    }
    return due;
  }

  /**
   * What is owed a block that has left the field (withdrawn, or fled off it): it still pays, in open-ground terms, as
   * the day's count still counts what it took away (lostShares). Never wasted: only an aimed run's empty ground is.
   */
  private offField(s: Side, id: number, raw: number): void {
    const e = this.unit(id), led = this.airLedger[s];
    if (!e) { led.wasted += raw; return; }
    led.landed += raw; led.dealt += raw;
    e.str = Math.max(0, e.str - (raw / Math.max(1e-9, e.power0)) * e.str0);
    e.figures = e.str <= 0 ? 0 : Math.max(1, Math.ceil(e.tray * e.str));
  }

  /** The flights over the field now, for the screens. */
  flightsNow(): Flight[] {
    const out: Flight[] = [];
    const r1 = (v: number): number => Math.round(v * 10) / 10;
    for (const k of this.strikes) for (const l of k.legs) if (!k.ended && !l.away) out.push({ ...l.flight, x: r1(l.flight.x), y: r1(l.flight.y), alt: r1(l.flight.alt), face: Math.round(l.flight.face * 1000) / 1000, run: [...l.flight.run] as Flight['run'] });
    return out;
  }

  /** Each computer general's style and its attack's plan, for the 3D game; nothing where neither side has a style. */
  private planView(): Pick<BattleView<LandUnit>, 'plan'> {
    const out: NonNullable<BattleView<LandUnit>['plan']> = {};
    for (const s of ['att', 'def'] as Side[]) {
      const st = this.style[s];
      if (!st || !this.ai[s]) continue;
      const P = this.plan[s];
      // A defence's held blocks walking behind its line toward the attack (bold): the 'shift' phase, those blocks.
      const walking = P ? [] : this.units.filter((u) => u.side === s && u.reserve === 'held' && this.shifted.has(u.id) && live(u)).map((u) => u.id);
      out[s] = { style: st.id, ...(P ? { phase: P.phase, main: [...P.main], startY: P.startY } : walking.length ? { phase: 'shift' as const, reserve: walking } : {}) };
    }
    return Object.keys(out).length ? { plan: out } : {};
  }

  view(as?: Side): BattleView<LandUnit> {
    const ev = this.events;
    this.events = [];
    // Every block on the field is in sight: what the player knows of each is known() (b464(2)). 'as' only sorts the cuts
    // and screens a side sees (hedge cutters, smoke on call; release 2), and the night's patrol (night patrol).
    return { t: this.t, limit: this.ticket.limit, units: this.units, balance: this.standing(), events: ev, zones: this.wrecks.concat(this.craters).map((z) => ({ ...z })), ai: { ...this.ai }, over: this.over, breaking: { att: this.breaking('att'), def: this.breaking('def') }, breakingBy: { att: this.breakCause('att'), def: this.breakCause('def') }, contact: this.contactAt, ...(this.setup ? { setup: this.setup } : {}), ...(this.falling.att !== null || this.falling.def !== null ? { fallBack: { att: this.falling.att !== null, def: this.falling.def !== null } } : {}), ...this.airView(), ...this.planView(), ...this.comingView(), ...this.riderView(), ...this.patrolView(as), ...this.cutView(as), ...this.screenView(as), ...this.phonesView(as), ...this.bearerView(as) };
  }

  /** Air support's part of the view, only where a side has fighters on call. */
  airView(): Pick<BattleView, 'flights' | 'air'> {
    if (!this.airOn.att && !this.airOn.def) return {};
    const air: NonNullable<BattleView['air']> = {};
    for (const s of ['att', 'def'] as Side[]) {
      if (!this.airOn[s]) continue;
      const k = this.strikes.find((x) => x.side === s && !x.ended);
      air[s] = { ready: this.airReady(s) ? 1 : 0, left: this.airLeft[s], nextIn: this.airLeft[s] > 0 && this.airNext[s] !== Infinity && this.t < this.airNext[s] ? Math.round((this.airNext[s] - this.t) * 10) / 10 : null, eta: k && this.t < k.called + LAND.air.eta ? Math.round((k.called + LAND.air.eta - this.t) * 10) / 10 : null };
    }
    return { flights: this.flightsNow(), air };
  }
}

/** Kinds the sim knows, for anything that builds a ticket by hand. */
export const KINDS: Kind[] = ['inf', 'mot', 'arm', 'cav', 'mtn', 'fort', 'art', 'eng'];
export { slopeAt };
