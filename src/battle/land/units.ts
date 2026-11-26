// From the campaign's corps to the blocks on the field, and where they stand when the battle opens.
//
// One block is one division, named as the campaign's army tray names it (Brian's division cards, Version 46). A side
// with fewer than six has some divisions split into their three regiments so there is something to handle (ruling
// 217(c)), each holding its division's frontage with a third of its models (b418); no division is ever folded into
// another (ruling 226(10)). Each side's blocks share out exactly the power the attack card counted (A and D), so the
// card's odds are the field's.

import type { CrewMarks } from './crews';
import type { Calibre, ForceKit, ForceTicket, Side, SideTicket, UnitBase } from '../core/types';
import { BEND, GROUNDS, STREAM, digWorks, type Bend, type LandField } from './field';
import { LAND, TEAMS, type Kind, type Team } from './rules';
import { turnToDate } from '../../core/ticket';

/** A block's starting nerve from its corps' organisation and morale (the field's, makeBlocks; read by the field spread's level too). */
export const startNerve = (org: number, mor: number | undefined): number => Math.min(1, 0.45 + 0.35 * org + 0.2 * (mor ?? 0.6));

export interface LandOrder {
  kind: 'move' | 'attack' | 'withdraw';
  x: number;
  y: number;
  /** The facing to take on arrival, from a drawn line. */
  face?: number;
  target?: number;
  /** An attack by several blocks at one: this block's place across the attack, metres right of its middle, and the attack's bearing. */
  lane?: number;
  axis?: number;
  /** Sent with others: when its block breaks or leaves, it carries the attack on to the nearest enemy still fighting. */
  press?: boolean;
  /**
   * Going in together (the tactics panel, item 1): a tank or lorry block with this group keeps no more than STYLE.lead
   * metres ahead of the group's slowest foot block until it is in contact; the foot of the group carry it too, so their
   * tanks can find them. A group with no foot ignores it.
   */
  together?: number;
  /**
   * At the double (release 2, the player's 'move' with run): foot go LAND.double faster, upright and winded after. After
   * the break (release 2, pursue) also on a 'withdraw' the broken army's player gives: he may hurry.
   */
  run?: boolean;
  /**
   * After the break (release 2, pursue): a 'withdraw' whose way the broken army's player picked. The block makes for
   * (x, its own edge) by the field's ways, and not straight back from where it stands.
   */
  via?: boolean;
}

export interface LandUnit extends UnitBase {
  kind: Kind;
  /** A support team (release 2): a gun block ('art') with its team's numbers (LAND.teams). Absent on every other block. */
  team?: Team;
  /** Its name for a card, and what it belongs to under it: "11th Infantry" over "I Corps", "1st Regiment" over "1st Infantry". */
  short: string;
  parent: string;
  /** Its share of its side's power at the start: what it strikes with and what it can take. */
  power0: number;
  /** Strength it came onto the field with (the campaign's own): its meter starts here. */
  str0: number;
  /** Morale it came onto the field with (the campaign's own): it comes back no higher than this. */
  mor0: number;
  xp: number;
  order: LandOrder | null;
  /** Where it is going next on the way to its order (a bridge, then the far bank). */
  path: [number, number][];
  /** Dug in where it began: cover while it stays put. */
  dug: boolean;
  /** How worn its uniforms and kit look, from its corps' time in the line (release 2, looks only): absent, fresh. */
  wear?: number;
  /** Its share of men new this month, in new kit (release 2, looks only): absent, none. */
  fresh?: number;
  /**
   * Hidden in the woods from the other side (release 2, LAND.hide): stood in woods when the clock started and not yet
   * found. Its enemies never strike it nor are sent at it; it holds its fire till they are close. Once found, never again.
   */
  hid?: boolean;
  /** When it sprang its ambush (opened fire while hidden): its first fire strikes harder (LAND.hide.surprise). */
  sprung?: number;
  /**
   * Overrun (release 2, LAND.overrun, LandBattle.escort): a tank block driving through the foot block 'on' to its far
   * edge ('to'), then backing out to where it stood ('back').
   */
  ovr?: { on: number; to: [number, number]; back: [number, number]; leg: 'in' | 'back' };
  /** Found by its fire but not yet located, till this battle time: fire back at it strikes softer (LAND.hide.locate). */
  unloc?: number;
  /** Closed up till this battle time (release 2, LAND.closed): a tank block whose commanders are down inside. */
  closedTo?: number;
  /**
   * Pinned (release 2, LAND.pin): slowed till this battle time to 'pinK' of its pace, a block on foot under a sniper
   * team's fire (LAND.snipers.pin); 'pinSpent' the seconds it has moved so this battle (at most LAND.pin.most).
   */
  pinTo?: number;
  pinK?: number;
  pinSpent?: number;
  /** Its leaders shot (sniper teams, release 2: LAND.snipers.lead): till this battle time it steadies on 'leadK' of its xp. */
  leadTo?: number;
  leadK?: number;
  /** A sniper team's (release 2, LAND.snipers): silent under fire till this battle time (quiet); slipping away till 'slipTo'; its slips this battle. */
  quietTo?: number;
  slipTo?: number;
  slips?: number;
  /** A machine-gun or mortar team of the computer's aimed at this found enemy sniper team (LAND.snipers; ai.ts counterSnipers). */
  hunts?: number;
  /**
   * Firing on a stretch of cover to flush out what may hide there (reconnaissance by fire, LAND.hide.search): the point,
   * and how far round it (LAND.hide.search.reach where not said; the computer's sweep of the cover ahead, wider).
   */
  search?: [number, number];
  searchR?: number;
  home: [number, number];
  /** Seconds since it was last hit, and since it last had an enemy near (for a rally). */
  still: number;
  clear: number;
  /**
   * Struck from its front this tick by someone: it holds its facing rather than turn. With the rules' faceRule on
   * (Units face the fire that hurts most, release 2): only where the enemy whose blow cost it most this tick is in front.
   */
  pinned: boolean;
  /** When it was last told it was hit on the flank, so the dispatch is not repeated every tick. */
  flankedAt: number;
  /** The facing its order asked for: kept while the enemy is in front of it. */
  keepFace: number | null;
  /** In contact this battle already (for the first-contact dispatch). */
  met: boolean;
  /**
   * The player knows it (b464(2), LandBattle.known): a block of his has fought it, his guns from far back too, or it
   * has struck one of his in contact (its guns firing from far back do not count). Nothing in the fight reads it.
   */
  found?: boolean;
  /**
   * Carried off by its army's break while it still stood: it falls back in order and reads so, never Routing, which
   * belongs to the blocks that broke (the design director, after the slice 3 review). What it leaves behind on the
   * field is counted as a runner's, as before, so the field's figures and the fairness tests do not move.
   */
  fellBack?: boolean;
  /**
   * Given ground at the brink (b496(1)): worn in a small army that its fresh blocks hold at the brink, it falls back off
   * the field rather than stand to be ground down, and while it goes, or runs, it is struck only on its rearguard. Its
   * order is a withdrawal; the player may send it in again.
   */
  gives?: boolean;
  /**
   * A defence's reserve (the design director, b407): 'held' behind the line until the line is struck, then 'sent' at
   * marching pace to where it is struck hardest. A line block from a quiet stretch that closes on the fight is 'sent' too,
   * and so is a corps come up from next door, of either side (neighbours join the fight, release 2).
   * A block that is sent no longer goes back to the ground it was drawn up on.
   */
  reserve?: 'held' | 'sent';
  /**
   * The player has taken this block in hand (release 2, per-unit command: LandBattle.perUnit): the staff thinks for the
   * rest of his side, never for it, until a 'land:staff' order hands it back.
   */
  taken?: boolean;
  /** Battle time its run at the double ends being felt: its blows are weaker till then (LAND.double.winded). */
  winded?: number;
  /** Orders by radio (release 2, LandBattle.radio): the battle time the player's last order reaches it, while it is on the way. */
  orderAt?: number;
  /** Models on its tray at full strength: a division's, or a third of them for a regiment. */
  tray: number;
  /** The divisions it carries, by their strength as the card weighs them (divs x str): a regiment a third (item 7's count). */
  worth?: number;
  /** A tank block's crews' own marks, names or kill rings, as data for the 3D game and the hover (b477); none on the rest. */
  crew?: CrewMarks;
  /** What its corps carries, as its nation built and researched it ("Units decide", ForceKit); absent, the history start's mix. */
  kit?: ForceKit;
  /**
   * What its guns throw, as the explosions sheet sizes it (shells.ts): data for the 3D game, never read by the fight.
   * None where it has only small arms.
   */
  cal?: Calibre;
  /**
   * Looks only (shells.ts): the harm each block that struck it has done it, by the striker's id, less a model's worth
   * for each model of it the striker was given. Each model it loses goes to the striker of that tick owed most, so a gun
   * firing from far back lands its shells in its share of the harm (the design director, b507). Nothing in the fight
   * reads it.
   */
  owed?: Record<number, number>;
  /** The way it stepped this tick, if it marched: at a bridge or a pass, a friend in its way makes way (LAND.queue). */
  pressing?: number;
  /** (wayround) The enemy block whose ground undid its step back this tick (spread): read and cleared by its next move. */
  barred?: number;
  /**
   * (wayround) The way round an enemy block in its line that it chose: that block, where it stood then, and the side
   * across its own way it goes round by (`side`, as its way lay then; `over`, that side as a direction on the field, so
   * the end it chose stays the same end however its way turns). Kept while that block stands there and this one is
   * falling back in order (sim.ts wayRound, move).
   */
  round?: { by: number; at: [number, number]; side: 1 | -1; over: [number, number] };
  /** The friend it made way for this tick (LAND.queue): next tick it waits rather than step back into that friend's way. */
  yieldTo?: number;
  /** Where its attack has it stand, as last worked out: its own stretch, which a friend coming on later stands beside (b439). */
  stand?: [number, number];
  /** What its stand was worked out for (its target, and its lane): the stand holds while it is still its own (b453). */
  standFor?: string;
  /** The way its stand was worked out facing: the friends beside it are reckoned the same way while it holds it. */
  standFace?: number;
  /** It came to its stand still out of reach of its target, which had stepped back: it closes straight on it. */
  closing?: boolean;
  /** (sim.standRule) The target it has stood to fight: it stands while that target is within its hold. Never set with the rule off. */
  stoodFor?: number;
  /** (sim.standRule, standKeeps.beside) The friends its stand was last worked out beside, and whether by the ground each stood on or by the stand each was making for. Never set with the rule off. */
  standBy?: { id: number; st: boolean }[];
  /** (sim.standRule, standKeeps.back) The place of the move order it last carried out, until it is sent anywhere else: sent there again while that place is under its own box, it has carried the order out. Never set with the rule off. */
  heldAt?: [number, number];
  /**
   * Guns told to fire on one block and stay put (ai.ts, the computer's main effort; ruling 239(6), the player's order
   * too): they strike it whenever it is in their reach, before whatever else is nearest.
   */
  aim?: number;
  /**
   * Battle time it last took the card's figure for tanks alone (item 7; sim.ts alone()): looks only, for the 3D game's
   * mark (a tank buttoned up, close-assault teams at it). Nothing in the fight reads it.
   */
  aloneAt?: number;
  /**
   * Its corps comes on from ground with no river between it and the place (ForceTicket.cross false): the card counts it
   * attacking over dry ground, so it draws up in the lanes the river bends away from (the design director, b464(5);
   * field.ts, Bend), and the river's hold is not counted against what it strikes (LAND.hold.river).
   */
  dry?: boolean;
  /**
   * Neighbours join the fight (release 2): set only on a block of a corps that comes up from next door, its corps'
   * ForceTicket.joined (the share of the battle it is there for). It stands off the field till its march from its edge
   * would bring it to its side's line at (1 - joined) of the battle's clock (LandBattle.comeOn), then walks on as a b407
   * reserve already sent.
   */
  joined?: number;
  /**
   * A joiner's part of its side's power as the card priced it (its weight for the part of the battle it is there for).
   * Its power0 is its corps' whole weight, what it strikes with and takes once on the field; the bar and the break
   * count it at this, from the first second, as the card did.
   */
  priced?: number;
  /**
   * Hold until relieved (release 2; Brian, 22 November 12:48Z; LandBattle.relief): told to wait for a corps coming up
   * from next door. It stands and fires, never advances, and its staff never orders it; dug in harder while it keeps
   * within LAND.dug.keep of 'at'. 'corps' is the corps whose help it waits for, 'due' the battle second that help can
   * reach it, 'since' the second it began to wait. 'late': help never came (the due passed, or its corps is out of the
   * fight): its men are let down, and break sooner until relieved. 'free': late, then given another order by the player:
   * it no longer stands, but keeps the mark. Absent on every other block, and on every block while the switch is off.
   */
  waits?: { corps: number; due: number; at: [number, number]; since: number; late?: true; free?: true };
  /** It has waited for help once this battle: never again (hold until relieved). */
  waited?: true;
  /**
   * What the cover of waiting spared it, in power, and the nerve that would have cost: kept while it waits, then landed
   * (LandBattle.endWait): the field moves when harm lands, never how much (b415, b418).
   */
  debt?: number;
  debtN?: number;
  /**
   * Hedge cutters (release 2; ruling 242; LandBattle.cutters): the cut its order sends it to make, where its front will
   * reach the hedge, at (x, y) on the hedge line, the line there running 'yaw' (radians from east, as a Gate's). Absent
   * on every other block, and on every block while the switch is off.
   */
  cut?: { x: number; y: number; yaw: number };
  /**
   * Smoke on call (release 2; ruling 242; LandBattle.smoke): the screens it may still lay this battle. Absent until it
   * first lays one, and on every block while the switch is off.
   */
  smokeLeft?: number;
}

const ORD =(n: number): string => { const s = ['th', 'st', 'nd', 'rd'], v = n % 100; return n + (s[(v - 20) % 10] || s[v] || s[0]); };
const KIND_WORD: Record<Kind, string> = { inf: 'Infantry', mtn: 'Mountain', cav: 'Cavalry', eng: 'Engineers', mot: 'Motorized', arm: 'Armoured', art: 'Artillery', fort: 'Fortress' };
const TEAM_WORD: Record<Team, string> = { mg: 'Machine-Gun Battalion', mor: 'Mortar Battalion', snp: 'Sniper Detachment' };
/** The team a ticket force makes, where its kind is one (release 2). */
export const teamOf = (f: { kind: string }): Team | undefined => (TEAMS as readonly string[]).includes(f.kind) ? (f.kind as Team) : undefined;

/** What one block carries of its force: one whole division, or one of its regiments. */
export interface BlockPart { divs: number[]; regiment?: number }

/** How each force is cut into blocks: one a division, split into regiments until the side has at least 'least'. */
export function partsFor(forces: ForceTicket[], least: number = LAND.blocks.least): BlockPart[][] {
  const parts: BlockPart[][] = forces.map((f) => Array.from({ length: Math.max(1, Math.round(f.divs)) }, (_, i) => ({ divs: [i] })));
  // A support team is one block a battalion, never cut into regiments, and never counted toward the side's least: a side
  // with teams cuts its divisions as it would without them.
  const total = (): number => parts.reduce((s, p, i) => s + (teamOf(forces[i]) ? 0 : p.length), 0);
  // The force whose blocks are biggest gives up a whole division to its regiments first.
  while (total() < least) {
    let best = -1, size = 0;
    parts.forEach((p, i) => {
      if (teamOf(forces[i]) || !p.some((x) => x.regiment === undefined && x.divs.length === 1)) return;
      const each = forces[i].share / p.length;
      if (each > size) { size = each; best = i; }
    });
    if (best < 0) break;
    const p = parts[best], j = p.findIndex((x) => x.regiment === undefined && x.divs.length === 1);
    p.splice(j, 1, ...Array.from({ length: LAND.blocks.regiments }, (_, r) => ({ divs: p[j].divs, regiment: r })));
  }
  return parts;
}

/** The Commonwealth's armies cut a division into brigades as Britain's did (the historian). */
const BRIGADES = ['UK', 'CAN', 'AUS', 'NZL', 'NZ', 'SAF', 'IND'];

/**
 * The kind's word in a division's name, as its army's English wrote it (the historian, 22 November 18:54Z,
 * historian/unit-type-words-by-nation.md, from the UI director's "4th Armored"): the US Armored and Motorized, Britain
 * and the Commonwealth Armoured and Motor; Germany's Panzer, its motorized divisions Panzergrenadier from mid-1943;
 * the Soviets' and Japan's Tank, the Soviets' Motorized, Rifle and Mountain Rifle (the historian, 19:09Z); every other army Armoured and Motorised (the UI director's house
 * style, 19:03Z: "Panzer" a proper name, "1st Panzer Division"). 'ym' the battle's year times 12 plus the month from 1.
 */
function kindWord(nat: string, k: Kind, ym = 0): string {
  if (k === 'arm') return nat === 'USA' ? 'Armored' : nat === 'GER' ? 'Panzer' : nat === 'SOV' || nat === 'JAP' ? 'Tank' : 'Armoured';
  if (k === 'mot') {
    if (nat === 'USA' || nat === 'SOV') return 'Motorized';
    if (BRIGADES.includes(nat)) return 'Motor';
    if (nat === 'GER') return ym >= 1943 * 12 + 6 ? 'Panzergrenadier' : 'Motorized';
    return 'Motorised';
  }
  // (the Soviets' foot: the Rifle Division, as its Rifle Regiments under it)
  if (k === 'inf' && nat === 'SOV') return 'Rifle';
  if (k === 'mtn' && nat === 'SOV') return 'Mountain Rifle';
  return KIND_WORD[k];
}

/**
 * A regiment block's own word in its army, in the battle's month (the historian's ruling, 22 November; release 2, the
 * 'namesOn' switch): what a third of a division was called. 'n' is its number, 'i' which third (0 to 2), 'ym' the year
 * times 12 plus the month from 1.
 * - Foot: a regiment ("33rd Infantry Regiment", the Soviets' "Rifle Regiment"); Britain and the Commonwealth's armies a
 *   brigade (three battalions), never a regiment; Germany's Infanterie-Regiment until September 1942, Grenadier-Regiment
 *   from October 1942. Italy's divisions were binary from 1938: two regiments, and a Blackshirt legion as the third.
 * - Armour: the American Combat Commands A, B and R; Germany's Panzer-Regiment; the Soviet tank brigade (1942 to 1945,
 *   a tank regiment before); the British armoured brigade; a tank regiment elsewhere.
 * The numbers are ours: a division's thirds numbered on from its own (the 11th's are the 31st to 33rd).
 */
export function regimentWord(nat: string, kind: Kind, n: number, i: number, ym: number): string {
  const o = ORD(n);
  if (kind === 'arm') {
    if (nat === 'USA') return `Combat Command ${'ABR'[i] ?? 'R'}`;
    if (nat === 'GER') return `Panzer-Regiment ${n}`;
    if (nat === 'SOV') return ym >= 1942 * 12 + 1 ? `${o} Tank Brigade` : `${o} Tank Regiment`;
    if (BRIGADES.includes(nat)) return `${o} Armoured Brigade`;
    return `${o} Tank Regiment`;
  }
  const arm = kind === 'mtn' ? 'Mountain' : kind === 'cav' ? 'Cavalry' : kind === 'mot' ? kindWord(nat, 'mot', ym) : kind === 'eng' ? 'Engineer' : 'Infantry';
  if (BRIGADES.includes(nat)) return `${o} ${arm} Brigade`;
  if (nat === 'GER') {
    const late = ym >= 1942 * 12 + 10;
    const word = kind === 'mtn' ? 'Gebirgsjäger' : kind === 'cav' ? 'Reiter' : kind === 'mot' ? (late ? 'Panzergrenadier' : 'Infanterie') : kind === 'eng' ? 'Pionier' : late ? 'Grenadier' : 'Infanterie';
    return `${word}-Regiment ${n}${kind === 'mot' && !late ? ' (mot.)' : ''}`;
  }
  if (nat === 'SOV') return `${o} ${kind === 'mtn' ? 'Mountain Rifle' : kind === 'cav' ? 'Cavalry' : kind === 'mot' ? 'Motor Rifle' : kind === 'eng' ? 'Engineer' : 'Rifle'} Regiment`;
  if (nat === 'ITA' && i === 2 && (kind === 'inf' || kind === 'mtn')) return `${o} Blackshirt Legion`;
  return `${o} ${arm} Regiment`;
}

/**
 * Names a side's regiment blocks as their armies did (regimentWord): its tag the block's own word, its parent its
 * division's full name, its card name the two together. The blocks of whole divisions keep their names. In the order
 * makeBlocks made them.
 */
export function armyNames(t: SideTicket, units: LandUnit[], turn: number): void {
  const d = turnToDate(turn), ym = d.year * 12 + d.month + 1;
  const cut = partsFor(t.forces);
  let j = 0;
  t.forces.forEach((f, i) => {
    const team = teamOf(f);
    const k = (team ? 'art' : Object.keys(LAND.speed).includes(f.kind) ? f.kind : 'inf') as Kind;
    for (const p of cut[i]) {
      const u = units[j++];
      if (!u || p.regiment === undefined || team) continue;
      const div = f.divisions?.[p.divs[0]], no = (f.first ?? 1) + p.divs[0];
      const full = div?.name ?? `${ORD(no)} ${kindWord(f.nat, k, ym)} Division`, short = div?.short ?? `${ORD(no)} ${kindWord(f.nat, k, ym)}`;
      const lead = /^(\d+)/.exec(full)?.[1];
      const n = lead ? (Number(lead) - 1) * LAND.blocks.regiments + p.regiment + 1 : p.regiment + 1;
      u.short = regimentWord(u.nat, k, n, p.regiment, ym);
      u.parent = full;
      u.name = `${u.short}, ${short}`;
    }
  });
}

/** How many blocks each force gives. */
export function blocksFor(forces: ForceTicket[]): number[] {
  return partsFor(forces).map((p) => p.length);
}

/** The blocks of one side, not yet placed. least: the fewest blocks the side is cut into (regiments make up the rest). */
export function makeBlocks(side: Side, t: SideTicket, power: number, firstId: number, least: number = LAND.blocks.least, turn?: number): LandUnit[] {
  const out: LandUnit[] = [];
  // (the battle's month, for the words that changed with the war: Germany's Panzergrenadier from mid-1943)
  const ym = turn === undefined ? 0 : ((d) => d.year * 12 + d.month + 1)(turnToDate(turn));
  const cut = partsFor(t.forces, least);
  t.forces.forEach((f, i) => {
    const team = teamOf(f);
    const k = (team ? 'art' : Object.keys(LAND.speed).includes(f.kind) ? f.kind : 'inf') as Kind;
    const n = Math.max(1, Math.round(f.divs));
    // Each division as the campaign has it; where the ticket carries none, numbered as the corps' own and alike.
    const divs = Array.from({ length: n }, (_, d) => {
      const own = f.divisions?.[d];
      const no = (f.first ?? 1) + d;
      return {
        name: own?.name ?? (team ? `${ORD(no)} ${TEAM_WORD[team]}` : `${ORD(no)} ${kindWord(f.nat, k, ym)} Division`), short: own?.short ?? (team ? `${ORD(no)} ${TEAM_WORD[team].split(' ')[0]}` : `${ORD(no)} ${kindWord(f.nat, k, ym)}`),
        str: own?.str ?? f.str, xp: own?.xp ?? f.xp, men: own?.men ?? f.men / n,
      };
    });
    const weightOf = (p: BlockPart): number => p.divs.reduce((s, d) => s + Math.max(0.01, divs[d].str), 0) / (p.regiment !== undefined ? LAND.blocks.regiments : 1);
    const all = cut[i].reduce((s, p) => s + weightOf(p), 0);
    for (const p of cut[i]) {
      const ds = p.divs.map((d) => divs[d]);
      const name = p.regiment !== undefined ? `${ORD(p.regiment + 1)} Regiment, ${ds[0].short}`
        : ds.length === 1 ? ds[0].name : ds.map((d) => d.short).join(' and ');
      const short = p.regiment !== undefined ? `${ORD(p.regiment + 1)} Regiment` : ds.length > 1 ? name : ds[0].short;
      const parent = p.regiment !== undefined ? ds[0].short : f.name;
      // A regiment holds its division's frontage, thinner: a third of the models on the same tray. Cut to a third of the
      // frontage, three regiments of a small attack crowded onto one enemy block, power packed three times as thick as the
      // card counts it, and the desert's coast villages ran 18 points over their card (b418, b420).
      const [w, d] = team ? LAND.teams[team].size : LAND.size[k];
      const per = team ? LAND.teams[team].figures : LAND.figures[k];
      const full = p.regiment !== undefined ? Math.max(1, Math.round(per / LAND.blocks.regiments)) : per;
      const str0 = Math.max(0.05, Math.min(1, ds.reduce((s, x) => s + x.str, 0) / ds.length));
      const mor0 = startNerve(f.org, f.mor);
      const share = weightOf(p) / all;
      out.push({
        id: firstId + out.length, side, corps: f.corps, nat: f.nat, kind: k, ...(team ? { team } : {}), name, short, parent,
        x: 0, y: 0, face: side === 'att' ? 0 : Math.PI,
        str: str0, str0, mor: mor0, mor0, xp: ds.reduce((s, x) => s + x.xp, 0) / ds.length,
        state: 'steady', men0: Math.round(ds.reduce((s, x) => s + x.men, 0) / (p.regiment !== undefined ? LAND.blocks.regiments : 1)),
        figures0: Math.max(1, Math.ceil(full * str0)), figures: Math.max(1, Math.ceil(full * str0)), tray: full,
        w, d, act: 'idle', speed: 0,
        power0: power * f.share * share,
        worth: weightOf(p),
        order: null, path: [], dug: !!f.dug, home: [0, 0], still: 99, clear: 99, pinned: false, flankedAt: -99, keepFace: null, met: false,
        ...(f.cross === false ? { dry: true } : {}),
        ...(f.wear !== undefined ? { wear: f.wear } : {}),
        ...(f.fresh ? { fresh: f.fresh } : {}),
        ...(f.kit ? { kit: f.kit } : {}),
      });
    }
  });
  return out;
}

/**
 * Neighbours join the fight (release 2): both sides' blocks of the corps coming up from next door (SideTicket.joiners),
 * the attack's first, numbered on from firstId after both armies, one a division (never cut into regiments), not yet
 * placed and never dug in. Each carries its corps' joined, its part as the card priced it (priced: what makeBlocks
 * gives, the side's power times its share), and its corps' whole weight as its power0 (priced over joined). None where
 * there are no joiners.
 */
export function joinBlocks(t: { att: SideTicket; def: SideTicket; turn?: number }, power: Record<Side, number>, firstId: number): LandUnit[] {
  const out: LandUnit[] = [];
  for (const side of ['att', 'def'] as Side[]) {
    const js = (t[side].joiners ?? []).filter((f) => Number.isFinite(f.joined) && f.joined! > 0 && f.share > 0);
    if (!js.length) continue;
    for (const u of makeBlocks(side, { ...t[side], forces: js }, power[side], firstId + out.length, 0, t.turn)) {
      const j = Math.min(1, js.find((f) => f.corps === u.corps)!.joined!);
      u.joined = j;
      u.priced = u.power0;
      u.power0 = u.priced / j;
      u.dug = false;
      out.push(u);
    }
  }
  return out;
}

const FRONT: Kind[] = ['inf', 'mtn', 'eng', 'fort', 'cav'];
const MOBILE: Kind[] = ['arm', 'mot'];
/** Metres between blocks drawn up side by side in a row. */
export const GAP = 40;

/** A side's rows: foot in the front line, tanks and lorries next (half of them in front where there is no foot), guns. */
function rowsOf(mine: LandUnit[]): LandUnit[][] {
  const lines: LandUnit[][] = [mine.filter((u) => FRONT.includes(u.kind)), mine.filter((u) => MOBILE.includes(u.kind)), mine.filter((u) => u.kind === 'art')];
  if (!lines[0].length && lines[1].length) lines[0] = lines[1].splice(0, Math.ceil(lines[1].length / 2));
  return lines;
}
const rowWidth = (row: LandUnit[]): number => row.reduce((s, u) => s + u.w + GAP, -GAP);

/**
 * Where the river bends (b464(5)): the x of the line between the attack's lanes that do not cross ('dry' at that end
 * of the field) and those that do. Each group of corps has as much of the attacker's band as its widest row needs, the
 * room to spare shared out the same way, and BEND.clear either side of the line, where the river runs down to the
 * attacker's edge, kept clear.
 */
export function bendAt(units: LandUnit[], z: [number, number, number, number], dry: 'west' | 'east'): number {
  const att = units.filter((u) => u.side === 'att');
  const need = (us: LandUnit[]): number => Math.max(0, ...rowsOf(us).map(rowWidth));
  const a = need(att.filter((u) => u.dry)), b = need(att.filter((u) => !u.dry)), room = z[2] - z[0] - 2 * BEND.clear;
  const span = a + b > 0 ? (room >= a + b ? a + ((room - a - b) * a) / (a + b) : (room * a) / (a + b)) : room / 2;
  return dry === 'west' ? z[0] + span + BEND.clear : z[2] - span - BEND.clear;
}

/**
 * Draws a side up in its zone: foot in the front line across the middle, tanks and lorries on the wings behind it,
 * guns at the back. The defender forms in front of its town where it has one, and its dug-in blocks get their works.
 * Where the river bends away from the lanes of the corps that do not cross (field.bend, b464(5)), the attack draws up
 * as two: those that do not cross at their end of the band, those that cross at the other, the river between them.
 */
export function deploy(units: LandUnit[], side: Side, f: LandField): void {
  const mine = units.filter((u) => u.side === side);
  const z = f.zones[side];
  if (side === 'att' && f.bend) {
    const B: Bend = f.bend, west = B.dry === 'west', inner = (s: -1 | 1): number => B.x + s * BEND.clear;
    const lanes: [LandUnit[], number, number][] = [
      [mine.filter((u) => u.dry), west ? 10 : inner(1), west ? inner(-1) : f.w - 10],
      [mine.filter((u) => !u.dry), west ? inner(1) : 10, west ? f.w - 10 : inner(-1)],
    ];
    for (const [us, lo, hi] of lanes) if (us.length) drawUp(us, side, f, (Math.max(lo, z[0]) + Math.min(hi, z[2])) / 2, lo, hi);
  } else if (f.stream && (f.stream.lie === 'up' || (f.stream.lie === 'shore' && f.stream.side !== 'back'))) streamLanes(mine, side, f);
  else if (side === 'def' && f.stream?.lie === 'across' && f.stream.join && joinLane(mine, f)) { /* drawn up beside the second river */ }
  else drawUp(mine, side, f, side === 'def' && f.town ? Math.min(z[2] - 300, Math.max(z[0] + 300, f.town.x)) : (z[0] + z[2]) / 2);
  for (const u of mine) {
    u.x = Math.min(f.w - u.w / 2 - 10, Math.max(u.w / 2 + 10, u.x));
    u.y = Math.min(f.h - 40, Math.max(40, u.y));
    // Never drawn up in a city's own river (b526): it lies behind the defence's rows and clear of the lanes either side
    // of it; should a row still reach it (a deep row in a narrow lane, a second river coming in behind), the block steps
    // forward out of it, toward the enemy, and a few steps more to stand a stone's throw off its water and bridges where
    // those steps take it clear and keep it in its zone (beside a river up the field they would only take it out of its
    // line, so it stays where its row put it).
    if (f.stream) {
      for (let k = 0; k < 40 && wetUnder(f, u); k++) u.y += side === 'att' ? 10 : -10;
      const y0 = u.y;
      for (let k = 0; k < 8 && wetUnder(f, u, STREAM.off); k++) u.y += side === 'att' ? 10 : -10;
      // Nor onto a block already drawn up there (a deep row in a narrow lane stepped onto the row in front of it).
      const onTop = mine.some((o) => o !== u && Math.abs(o.x - u.x) < (o.w + u.w) / 2 && Math.abs(o.y - u.y) < (o.d + u.d) / 2);
      if (wetUnder(f, u, STREAM.off) || (side === 'att' ? u.y > z[3] : u.y < z[1]) || onTop) u.y = y0;
    }
    u.home = [u.x, u.y];
    if (u.dug) digWorks(f, u.x, u.y, u.w, u.d);
  }
}

/** Whether any of a block's box, grown by off metres each way, stands in water or on a bridge. */
export function wetUnder(f: LandField, u: Pick<LandUnit, 'x' | 'y' | 'w' | 'd'>, off = 0): boolean {
  const hw = u.w / 2 + off, hd = u.d / 2 + off, nx = Math.ceil((2 * hw) / (f.cell / 2)), ny = Math.ceil((2 * hd) / (f.cell / 2));
  for (let i = 0; i <= nx; i++) for (let j = 0; j <= ny; j++) {
    const c = Math.floor((u.x - hw + (2 * hw * i) / nx) / f.cell), r = Math.floor((u.y - hd + (2 * hd * j) / ny) / f.cell);
    if (c < 0 || r < 0 || c >= f.cols || r >= f.rows) continue;
    const k = GROUNDS[f.ground[r * f.cols + c]];
    if (k === 'water' || k === 'bridge' || k === 'blown') return true;
  }
  return false;
}

/**
 * The defence beside a second river that comes in from its side edge to meet one across the field behind it (b596, the
 * Sava at Belgrade): drawn up on the heart's side of where it meets the first, a stone's throw off its water, as near
 * the heart as it fits. False where no part of it lies inside the field (it stands as it would with no second river).
 */
function joinLane(mine: LandUnit[], f: LandField): boolean {
  const J = f.stream!.join!, z = f.zones.def, y0 = z[1] - 120;
  const xs = J.course.filter(([x, y]) => y >= y0 && x >= 0 && x <= f.w).map(([x]) => x);
  if (!xs.length) return false;
  const clear = J.half * (1 + STREAM.wobble) + f.cell + STREAM.off, west = J.course[0][0] < f.w / 2;
  const [a, b] = west ? [Math.max(...xs) + clear, f.w - 10] : [10, Math.min(...xs) - clear];
  const centre = f.town ? Math.min(z[2] - 300, Math.max(z[0] + 300, f.town.x)) : (z[0] + z[2]) / 2;
  drawUp(mine, 'def', f, Math.min(b, Math.max(a, centre)), a, b);
  return true;
}

/**
 * A side drawn up beside a city's own river (field.stream, b526). One that runs up the field leaves two lanes, either
 * side of it and a stone's throw beyond its water: each army stands in the heart's where its front line fits, else across
 * both, its blocks shared out by the room each has. A shore leaves one lane, off its water.
 */
function streamLanes(mine: LandUnit[], side: Side, f: LandField): void {
  // Clear of its water at its widest, by a cell (the water is drawn by the cell) and the stone's throw.
  const S = f.stream!, z = f.zones[side], clear = S.half * (1 + STREAM.wobble) + f.cell + STREAM.off;
  // Over all the depth its rows may take, the stone's throw in front of them too: where the lanes are narrow, they run
  // back toward the side's own edge.
  const y0 = side === 'att' ? 0 : z[1] - 120, y1 = side === 'att' ? z[3] + 120 : f.h;
  const xs = S.course.filter(([, y]) => y >= y0 && y <= y1).map(([x]) => x);
  const lo = Math.min(...xs), hi = Math.max(...xs);
  const lane = (a: number, b: number): [number, number, number] => [a, b, Math.max(0, Math.min(b, z[2]) - Math.max(a, z[0]))];
  const lanes = S.lie === 'shore' ? [lo < f.w / 2 ? lane(hi + STREAM.clear, f.w - 10) : lane(10, lo - STREAM.clear)] : [lane(10, lo - clear), lane(hi + clear, f.w - 10)];
  const open = lanes.filter((l) => l[2] >= 200);
  const groups: LandUnit[][] = open.map(() => []), fill = open.map(() => 0);
  // Both armies stand on the heart's bank where their front line fits there, the river beside them and never between
  // them: the plain order's target is the defender nearest the attack, and a quarter of the attack wading over to it
  // cost the attack up to 15 points against the card in the hills and mountains. Only an army too broad for that bank
  // spreads over both, its blocks shared out by the room each has, block by block in their order.
  const home = f.town ? open.findIndex(([a, b]) => f.town!.x >= a && f.town!.x <= b) : -1;
  if (home >= 0 && rowWidth(rowsOf(mine)[0]) <= open[home][2]) groups[home].push(...mine);
  else for (const u of mine) {
    let k = 0;
    for (let j = 1; j < open.length; j++) if ((fill[j] + u.w) / open[j][2] < (fill[k] + u.w) / open[k][2]) k = j;
    groups[k].push(u); fill[k] += u.w;
  }
  // Each lane's rows as near as they fit to where the side stands with no river (the defence about the heart, the attack
  // at the middle of its zone), not spread to the far ends of their lanes.
  const centre = side === 'def' && f.town ? Math.min(z[2] - 300, Math.max(z[0] + 300, f.town.x)) : (z[0] + z[2]) / 2;
  open.forEach(([a, b], j) => {
    if (groups[j].length) drawUp(groups[j], side, f, Math.min(b, Math.max(a, centre)), a, b);
  });
}

/**
 * Draws blocks up in rows about cx. Between lo and hi (metres east) where they are given, the attack's lanes either side
 * of the river's bend: a row that would run past either is moved in, and a row wider than the room goes on in a further
 * row behind it, GAP kept between its blocks, so no two are drawn up on top of each other (b492(3)).
 */
function drawUp(mine: LandUnit[], side: Side, f: LandField, cx: number, lo?: number, hi?: number): void {
  const z = f.zones[side];
  const dir = side === 'att' ? 1 : -1;
  let front = side === 'att' ? z[3] - 40 : z[1] + 40;
  const lines = rowsOf(mine);
  // In a capital's or a city's streets the defence stands a block inside the ragged edge of its core, in the outer
  // streets, never in front of the city (the design director, b529): the core's edge along its front line, at the middle.
  if (side === 'def' && f.streetEdge) {
    const half = rowWidth(lines[0]) / 2;
    const ys = f.streetEdge.filter((y, c) => y >= 0 && Math.abs(c * f.cell + f.cell / 2 - cx) <= half).sort((a, b) => a - b);
    if (ys.length) front = Math.max(front, ys[Math.floor(ys.length / 2)] + LAND.city.inside);
    // A city's own river across the field keeps behind the front line it read there (b526): never deeper than that.
    if (f.stream?.front !== undefined) front = Math.min(front, f.stream.front);
  }
  const x0 = lo ?? 10, x1 = hi ?? f.w - 10;
  /**
   * Where a row stands on the wings: half on each, beyond the ends of the front line. None where a wing would run off
   * the field or past its lane (a big army's long front): the row stands centred behind the line instead, so no two blocks are drawn up
   * on top of each other.
   */
  const wingsAt = (row: LandUnit[]): number[] | null => {
    const span = (lines[0].length * (150 + GAP)) / 2;
    const at = row.map((u, i) => { const left = i % 2 === 0, k = Math.floor(i / 2); return cx + (left ? -1 : 1) * (span + 60 + k * (u.w + GAP)); });
    return row.every((u, i) => at[i] - u.w / 2 >= x0 && at[i] + u.w / 2 <= x1) ? at : null;
  };
  /** Draws a row up at depth y; says whether it stood on the wings (false: centred). */
  const place = (row: LandUnit[], y: number, wings: boolean): boolean => {
    const at = wings ? wingsAt(row) : null;
    if (at) {
      row.forEach((u, i) => { u.x = at[i]; u.y = y; });
      return true;
    }
    const width = rowWidth(row);
    let x = cx - width / 2;
    for (const u of row) { u.x = x + u.w / 2; u.y = y; x += u.w + GAP; }
    if (lo !== undefined && hi !== undefined && row.length > 0) {
      // Kept to its own lanes: moved in off the river's bend.
      const a = row[0].x - row[0].w / 2, b = row[row.length - 1].x + row[row.length - 1].w / 2;
      const shift = a < x0 ? x0 - a : b > x1 ? x1 - b : 0;
      for (const u of row) u.x += shift;
    }
    return false;
  };
  // A defence of four blocks or more in its line keeps a quarter of them back as a reserve, centred between its tanks
  // on the wings (b407). Each is the last division of whichever corps has most in the line, so every corps' first
  // divisions stand in the line. Where the tanks could not stand on the wings and are centred behind the line, the
  // reserve stands a row further back. Every defence, whatever its general's style: the fit's reserve is part of what
  // the card priced (the design director, ruling 239's rider, 21:57Z).
  const held: LandUnit[] = [];
  if (side === 'def' && lines[0].length >= 4) {
    for (let n = Math.max(1, Math.floor(lines[0].length / 4)); n > 0; n--) {
      const count = (c: number): number => lines[0].filter((u) => u.corps === c).length;
      let at = -1;
      for (let i = lines[0].length - 1; i >= 0; i--) if (at < 0 || count(lines[0][i].corps) > count(lines[0][at].corps)) at = i;
      held.unshift(...lines[0].splice(at, 1));
    }
  }
  if (lo === undefined || hi === undefined) {
    place(lines[0], front, false);
    const wide = place(lines[1], front - dir * 110, true);
    if (held.length) { place(held, front - dir * (wide || !lines[1].length ? 110 : 170), false); for (const u of held) u.reserve = 'held'; }
    place(lines[2], front - dir * 230, false);
    return;
  }
  // In its lanes (b492(3)): each line cut into rows no wider than the room, the front ones full and the rest behind.
  // A line's first row stands where it would with room enough (the tanks 110 m behind the front line, the guns 230 m),
  // or further back to keep GAP behind the row before it, as each row after it does. Where the last row would stand
  // past the back of the zone, the gaps between rows close up, 5 m at a time to no less than 10 m; past that the rows
  // run on back toward the attacker's edge of the field.
  const rows: { us: LandUnit[]; at: number; first: boolean; wings: boolean }[] = [];
  const cut = (line: LandUnit[], at: number): void => line.forEach((u, i) => {
    const r = rows[rows.length - 1];
    if (i > 0 && rowWidth([...r.us, u]) <= x1 - x0) r.us.push(u);
    else rows.push({ us: [u], at, first: i === 0, wings: false });
  });
  cut(lines[0], 0);
  // The tanks stand on the wings only beside a front line that stands in one row.
  const wide = rows.length <= 1 && lines[1].length > 0 && wingsAt(lines[1]) !== null;
  if (wide) rows.push({ us: lines[1], at: 110, first: true, wings: true });
  else cut(lines[1], 110);
  cut(held, wide || !lines[1].length ? 110 : 170);
  for (const u of held) u.reserve = 'held';
  cut(lines[2], 230);
  const deep = (us: LandUnit[]): number => Math.max(...us.map((u) => u.d));
  const steps = (gap: number): number[] => {
    const ys: number[] = [];
    rows.forEach((r, i) => {
      const behind = i ? ys[i - 1] - dir * (deep(rows[i - 1].us) / 2 + gap + deep(r.us) / 2) : front;
      ys.push(!r.first ? behind : dir > 0 ? Math.min(front - r.at, behind) : Math.max(front + r.at, behind));
    });
    return ys;
  };
  const back = dir > 0 ? z[1] : z[3], n = rows.length - 1;
  const past = (ys: number[]): boolean => n >= 0 && dir * (ys[n] - back) < deep(rows[n].us) / 2;
  let ys = steps(GAP);
  for (let gap = GAP - 5; gap >= 10 && past(ys); gap -= 5) ys = steps(gap);
  rows.forEach((r, i) => place(r.us, ys[i], r.wings));
}
