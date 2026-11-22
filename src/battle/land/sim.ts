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
import { DT, type AirFlown, type AirOnCall, type BattleSim, type BattleView, type BreakCause, type Command, type FieldEvent, type FieldResult, type FieldTicket, type Flight, type Side, type Zone } from '../core/types';
import { thinkFor, type AttackPlan } from './ai';
import { STYLE, styleFor, type Style } from './styles';
import { BEND, buildField, coverAt, FIELD, groundAt, GROUNDS, heightAt, slopeAt, streetsOf, builtShare, zonesOf, type Ground, type LandField, type LandPlace } from './field';
import { crewFor } from './crews';
import { calFor, CRATER, REACH, SHELLS_FROM } from './shells';
import { LAND, MOTION, gunsOf, reachOf, type Kind, type Motion } from './rules';
import { bendAt, deploy, makeBlocks, wetUnder, type LandUnit } from './units';
import { Drill, wreckOffset } from './drill';

const other = (s: Side): Side => (s === 'att' ? 'def' : 'att');

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
interface Way { m: Going; goal: number; pts: [number, number][]; whole: boolean }
/**
 * How a block finds its way: as its kind moves; or, foot of a corps the card brings on over dry ground, as foot that
 * will not wade (b464(5)): sent at an enemy beyond where the river bends, it goes round the bend by dry ground, as the
 * card counts it, never through it. Only the way it chooses: a block already in the water still wades out.
 */
type Going = Motion | 'ashore';
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
/** A number from 0 to under 1 fixed by a whole number: where a wreck lies, never drawn from the battle's dice. */
const mix = (n: number): number => { let h = n >>> 0; h ^= h >>> 16; h = Math.imul(h, 0x85ebca6b); h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35); h ^= h >>> 16; return (h >>> 0) / 4294967296; };

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
    const att = makeBlocks('att', ticket.att, A, 1);
    const def = makeBlocks('def', ticket.def, D, att.length + 1);
    this.units = [...att, ...def];
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
    // Where some of the attack's corps cross the river and some come on over dry ground, the river lies only before
    // those that cross: it bends away from the lanes of the others, at their end of the field (the design director,
    // b464(5)). Where all cross, or there is no river, the field is as it always was.
    // A ticket that has none crossing is read as one that has all cross: a river with no corps before it is no card's.
    const dry = ticket.place.dryEnd ?? 'west';
    const bend = ticket.place.river && att.some((u) => u.dry) && att.some((u) => !u.dry) ? { x: bendAt(att, zonesOf(FIELD.w, FIELD.h).att, dry), dry } : undefined;
    if (!bend) for (const u of att) delete u.dry;
    // Read once the bend is settled: a river with none of the attack dry is crossed by all of it, armour too (b494).
    const armour = att.filter((u) => MOTION[u.kind] === 'tracks' || MOTION[u.kind] === 'wheels'), all = armour.reduce((a, u) => a + u.power0, 0);
    this.armourWet = all > 0 ? armour.filter((u) => !u.dry).reduce((a, u) => a + u.power0, 0) / all : 0;
    this.field = buildField(ticket.place, ticket.seed, bend);
    // A capital's or a city's day is longer as more of its field is built (the design director, b515(g)): the march in
    // through its streets and the fight in them take time the open field's does not.
    if (streetsOf(ticket.place)) this.ticket = { ...ticket, limit: Math.round(ticket.limit * (1 + LAND.city.clock * builtShare(this.field))) };
    deploy(this.units, 'att', this.field);
    deploy(this.units, 'def', this.field);
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
    const [d0, d1] = ticket.reckoning.dice;
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
  private holdTake(u: LandUnit): number {
    const p = this.ticket.place, H = LAND.hold;
    const streets = streetsOf(p);
    return (H.take[p.terrain] ?? 0.4) * (p.river && !u.dry ? 1 + (H.river - 1) * this.armourWet : 1) * (p.fort > 0 ? H.works[Math.min(3, p.fort)] : 1) * (streets ? H.streets[`${p.terrain} ${streets}`] ?? H.streets[streets] : 1) * this.flankHold;
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
    const mine = (ids: number[]): LandUnit[] => ids.map((i) => this.unit(i)).filter((u): u is LandUnit => !!u && u.side === by && fighting(u) && u.kind !== 'fort');
    switch (c.do) {
      case 'move': {
        const us = mine(c.units);
        if (!us.length) return false;
        // A group keeps its shape: each block goes to the point plus its place in the group.
        const cx = us.reduce((s, u) => s + u.x, 0) / us.length, cy = us.reduce((s, u) => s + u.y, 0) / us.length;
        for (const u of us) this.send(u, { kind: 'move', x: c.x + (us.length > 1 ? u.x - cx : 0), y: c.y + (us.length > 1 ? u.y - cy : 0), face: c.face });
        this.say({ what: 'contact', side: by, text: '' });
        this.events.pop(); this.log.pop();
        return true;
      }
      case 'line': {
        const us = mine(c.units);
        if (!us.length) return false;
        for (const p of lineUp(us, by, c.x0, c.y0, c.x1, c.y1, 150)) {
          const u = this.unit(p.id)!;
          this.send(u, { kind: 'move', x: p.x, y: p.y, face: p.face });
        }
        return true;
      }
      case 'attack': {
        const t = this.unit(c.target);
        const us = mine(c.units);
        if (!t || t.side === by || !live(t) || !us.length) return false;
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
        const foes = this.units.filter((e) => e.side !== by && fighting(e));
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
            if (e !== t) { this.send(u, { kind: 'attack', x: e.x, y: e.y, target: e.id, press: true, lane: lane - ((e.x - t.x) * rx + (e.y - t.y) * ry), axis, ...together }); return; }
          }
          this.send(u, { kind: 'attack', x: t.x, y: t.y, target: t.id, press: us.length > 1, ...(us.length > 1 ? { lane, axis } : {}), ...together });
        });
        return true;
      }
      case 'hold': {
        const us = mine(c.units);
        for (const u of us) { u.order = null; u.path = []; u.keepFace = null; delete u.aim; }
        return us.length > 0;
      }
      case 'withdraw': {
        const us = mine(c.units);
        for (const u of us) this.send(u, { kind: 'withdraw', x: u.x, y: by === 'att' ? 0 : this.field.h });
        return us.length > 0;
      }
      case 'withdrawAll': {
        if (c.side !== by) return false;
        // What the fighters on call still owe lands before the army goes (air support): pulling out never escapes it.
        this.payAir();
        // How near breaking it stood when it pulled out, before its blocks leave the count (b474).
        this.bar[by] = this.breaking(by);
        for (const u of this.units) if (u.side === by && fighting(u)) { u.state = 'withdrawn'; u.act = 'down'; }
        this.finish(other(by), 'withdrew');
        return true;
      }
      case 'auto': {
        if (c.side !== by && !this.ai[c.side]) return false;
        this.ai[c.side] = c.on;
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
        if (c.do === 'land:place') {
          const u = this.unit(Number((c.units as number[] | undefined)?.[0])), x = Number(c.x), y = Number(c.y);
          if (!u || this.placeWhy(u, x, y) !== null || by !== this.setup) return false;
          this.place(u, x, y);
          return true;
        }
        if (c.do === 'land:begin') {
          if (by !== this.setup) return false;
          this.setup = null; this.room = null;
          return true;
        }
        if (c.do === 'land:fire') {
          const t = this.unit(Number(c.target));
          const guns = mine((c.units ?? []) as number[]).filter((u) => u.kind === 'art');
          if (!t || t.side === by || !live(t) || !guns.length) return false;
          for (const u of guns) { u.order = null; u.path = []; u.keepFace = null; u.aim = t.id; }
          return true;
        }
        return false;
      }
    }
  }

  private send(u: LandUnit, o: LandUnit['order'] & object): void {
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
    // Any other order lifts a gun's aim (fire on, stay put).
    delete u.aim;
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
  private going(u: LandUnit): Going { return u.dry && MOTION[u.kind] === 'foot' ? 'ashore' : MOTION[u.kind]; }
  private ashore: Record<Ground, number> | null = null;
  private paceOf(m: Going): Record<Ground, number> { return m === 'ashore' ? (this.ashore ??= { ...LAND.pace.foot, water: 0, blown: 0 }) : LAND.pace[m]; }
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
      if (!w || w.m !== m || !near(w.goal, goal)) {
        w = { m, goal, ...this.findWay(m, u.x, u.y, x, y) };
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
    const f = this.field, C = f.cols, pace = this.paceOf(m), s = this.cellAt(x, y);
    const go = (i: number): boolean => pace[GROUNDS[f.ground[i]]] > 0;
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
        if (go(j)) {
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
    const f = this.field, C = f.cols, n = f.ground.length, pace = this.paceOf(m);
    const top = Math.max(...Object.values(pace)), s = this.cellAt(sx, sy), g = this.cellAt(x, y), gc = g % C, gr = Math.floor(g / C);
    const go = (i: number): number => pace[GROUNDS[f.ground[i]]];
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
    const pace = this.paceOf(m);
    for (let n = Math.abs(c1 - c) + Math.abs(r1 - r); n > 0 && (c !== c1 || r !== r1); n--) {
      if (Math.abs(tx - ty) < 1e-9) { tx += stepX; ty += stepY; c += sx; r += sy; n--; }
      else if (tx < ty) { tx += stepX; c += sx; }
      else { ty += stepY; r += sy; }
      if (c < 0 || r < 0 || c >= f.cols || r >= f.rows) return true;
      if (pace[GROUNDS[f.ground[r * f.cols + c]]] <= 0) return false;
    }
    return true;
  }

  // ---- The tick -----------------------------------------------------------------------------------------------------

  /** The squad brain, made the first time it is asked for (drill.ts). */
  drillOn(opts?: { sections?: boolean; platoons?: boolean }): Drill { return (this.drill ??= new Drill(this, opts)); }

  /**
   * Opens the setup step for one side (release 2): only before the first step. Its guns and support teams may then be
   * placed anywhere in its own zone (or where its own blocks were drawn up), on dry ground they can stand on, clear of
   * every other block; the computer's stand where they were drawn up. Every other order is taken as before the clock
   * starts. False if the battle has begun.
   */
  setupOn(side: Side): boolean {
    if (this.t > 0 || this.over) return false;
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
    if (u.kind !== 'art' || !fighting(u)) return 'Only guns and support teams are placed.';
    if (u.dug) return 'It is dug in where it stands.';
    if (!Number.isFinite(x) || !Number.isFinite(y)) return 'Off the field.';
    const r = this.room!, hw = u.w / 2, hd = u.d / 2;
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

  /** Puts a block where the setup step placed it: drawn up there, facing the enemy, with no order. */
  private place(u: LandUnit, x: number, y: number): void {
    u.x = x; u.y = y; u.face = u.side === 'att' ? 0 : Math.PI;
    u.order = null; u.path = []; u.keepFace = null;
    delete u.aim; delete u.stand; delete u.standFor; delete u.standFace; delete u.closing;
    this.ways.delete(u.id);
    u.home = [x, y];
    u.cover = groundAt(this.field, x, y);
    this.drill?.relay(u);
  }

  step(): void {
    if (this.setup) return;
    if (this.over) { this.drill?.step(); return; }
    if (this.t >= this.nextThink) {
      this.nextThink = this.t + LAND.think;
      for (const s of ['att', 'def'] as Side[]) if (this.ai[s]) thinkFor(this, s);
    }
    for (const u of this.units) { u.pinned = false; if (live(u)) { u.still += DT; u.clear += DT; } }
    // Blows first, from where everyone stands, then nerve, then movement: no block gets to strike first by its place in the list.
    const hurt = new Map<number, { dmg: number; nerve: number; from: [LandUnit, number][] }>();
    // What blows held back this tick for air strikes, by the block they fell on (air support).
    const kept = this.airOn.att || this.airOn.def ? new Map<number, number>() : null;
    for (const u of this.units) {
      if (!fighting(u)) continue;
      const t = this.targetOf(u);
      u.engaged = t?.id;
      if (!t) continue;
      if (this.contactAt === null && u.kind !== 'art') this.contactAt = this.t;
      // He knows a block one of his has fought, his guns' target too, and one of theirs that has struck his in contact;
      // their guns firing from far back are not contact (b464(2)).
      if (this.ticket[u.side].human || u.kind !== 'art' || this.gap(u, t) < LAND.guns.closeAt) { u.found = true; t.found = true; }
      if (!u.met) { u.met = true; if (u.side === 'att' || !t.met) this.say({ what: 'contact', unit: u.id, side: u.side, text: `${this.named(u)} is in action.`, x: u.x, y: u.y }); }
      const blow = this.blow(u, t);
      // A side with air strikes still to call holds back the card's share for them, owed to the block it fell on, in
      // what it would be on open ground (air support, below).
      if (this.airLeft[u.side] > 0) {
        const held = blow.dmg * this.airOn[u.side]!.share, raw = held / blow.ground;
        blow.dmg -= held;
        this.airBank[u.side].set(t.id, (this.airBank[u.side].get(t.id) ?? 0) + raw);
        this.airLedger[u.side].withheld += raw;
        kept!.set(t.id, (kept!.get(t.id) ?? 0) + raw);
      }
      const h = hurt.get(t.id) ?? { dmg: 0, nerve: 0, from: [] };
      h.dmg += blow.dmg; h.nerve += blow.nerve;
      // Who struck it this tick, and how hard: whose blow a lost model goes to (suffer; shells.ts). Never read by the fight.
      h.from.push([u, blow.dmg]);
      hurt.set(t.id, h);
      if (blow.front) t.pinned = true;
      if (blow.aspect !== 'front' && this.t - t.flankedAt > 20) {
        t.flankedAt = this.t;
        if (t.side === 'att' ? this.ticket.att.human : this.ticket.def.human) this.say({ what: 'flanked', unit: t.id, side: t.side, text: `${this.named(t)} is hit from the ${blow.aspect === 'rear' ? 'rear' : 'flank'}.`, x: t.x, y: t.y });
      }
    }
    // The nerve what was held back would have cost, as the tick's blows on that block cost it, owed with it.
    if (kept) for (const [id, raw] of kept) { const h = hurt.get(id)!, side = other(this.unit(id)!.side); this.airNerve[side].set(id, (this.airNerve[side].get(id) ?? 0) + raw * h.nerve); }
    for (const [id, h] of hurt) this.suffer(this.unit(id)!, h.dmg, h.nerve, h.from);
    if (this.airOn.att || this.airOn.def) this.flyAir();
    for (const u of this.units) if (live(u)) this.nerve(u);
    for (const u of this.units) if (live(u)) this.move(u);
    this.spread();
    this.t = Math.round((this.t + DT) * 1000) / 1000;
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
      const x = clamp(u.x + ux * push, 5, this.field.w - 5), y = clamp(u.y + uy * push, 5, this.field.h - 5), pace = LAND.pace[MOTION[u.kind]];
      const there = groundAt(this.field, x, y), here = groundAt(this.field, u.x, u.y);
      if (inRiver(there) && !inRiver(here)) return false;
      const to = pace[there];
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
    const reach = reachOf(u);
    // Guns told to fire on one block strike it whenever it is in their reach (ai.ts; ruling 239(6)).
    if (u.aim !== undefined) { const a = this.unit(u.aim); if (a && live(a) && a.side !== u.side && this.gap(u, a) <= reach) return a; }
    let best: LandUnit | null = null, score = Infinity;
    for (const e of this.units) {
      if (e.side === u.side || !live(e)) continue;
      const d = this.gap(u, e);
      if (d > reach) continue;
      if (u.order?.kind === 'attack' && u.order.target === e.id) return e;
      const off = Math.abs(turn(u.face, bearing(u.x, u.y, e.x, e.y)));
      const s = d * (1 + off) * (e.state === 'routing' ? 2 : 1);
      if (s < score) { score = s; best = e; }
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

  /** Metres between two blocks' edges, roughly. */
  gap(a: LandUnit, b: LandUnit): number {
    return Math.max(0, dist(a.x, a.y, b.x, b.y) - (Math.max(a.w, a.d) + Math.max(b.w, b.d)) / 4);
  }

  /**
   * What a block takes of a blow for the ground it stands on (its cover, the shelter of wrecks beside men on foot, tanks
   * blind in the streets): the part of what it takes that is where it stands, not whose ground it is.
   */
  private groundTake(t: LandUnit): number {
    const gt = coverAt(this.field, t.x, t.y);
    let take = LAND.cover[gt];
    // Men on foot beside a wreck take light cover, either army's (b477).
    const ws = this.wreckShare(t);
    if (ws > 0) take = take * (1 - ws) + Math.min(take, LAND.cover[LAND.wreck.cover]) * ws;
    if (t.kind === 'arm' && LAND.tanksClose.ground.includes(gt)) take *= LAND.tanksClose.take;
    return take;
  }

  private blow(u: LandUnit, t: LandUnit): { dmg: number; nerve: number; front: boolean; aspect: 'front' | 'flank' | 'rear'; ground: number } {
    const gu = coverAt(this.field, u.x, u.y);
    let p = u.power0 * (0.35 + 0.65 * (u.str / u.str0)) * LAND.rate;
    if (u.state === 'wavering') p *= LAND.wavering;
    // A block giving ground at the brink (giveGround) leaves a rearguard facing the enemy: a blow on it lands at the
    // weight of one struck by an enemy on the move (LAND.moving), as the enemy must come on after it, and on its front,
    // never its turned back (b496(1)).
    const rearguard = this.givingGround(t);
    if ((u.order && u.path.length && u.speed > 0.5) || rearguard) p *= LAND.moving;
    let nerveMul = 1;
    if (u.kind === 'art') {
      const G = gunsOf(u), close = this.gap(u, t) < G.closeAt;
      p *= close ? G.close : G.punch;
      if (!close) nerveMul *= G.nerve;
      // A support team's bullets and light bombs do little to tanks (release 2).
      if (u.team && t.kind === 'arm') p *= LAND.teams[u.team].armour;
    }
    if (u.kind === 'arm' && LAND.tanksClose.ground.includes(gu)) p *= LAND.tanksClose.punch;
    const rise = heightAt(this.field, u.x, u.y) - heightAt(this.field, t.x, t.y);
    if (rise > LAND.height.over) p *= LAND.height.up; else if (rise < -LAND.height.over) p *= LAND.height.down;
    const aspect = rearguard ? 'front' : aspectOf(t.face, t.x, t.y, u.x, u.y);
    const [am, nm] = LAND.aspect[aspect];
    const ground = this.groundTake(t);
    let take = ground;
    // Tanks that have left their infantry on close ground take the card's own figure for tanks alone there (item 7).
    if (t.kind === 'arm' && LAND.alone.on) take *= this.alone(t, u);
    if (t.side === 'def' && dist(t.x, t.y, t.home[0], t.home[1]) < LAND.hold.within) take *= this.holdTake(u);
    const luck = 1 + LAND.jitter * (this.rng.next() * 2 - 1);
    const dmg = p * am * take * luck * (u.side === 'att' ? this.fortune : 1 / this.fortune) * DT;
    return { dmg, nerve: nm * nerveMul, front: aspect === 'front', aspect, ground };
  }

  /**
   * 'from' is who struck it this tick and how hard. Each model it loses goes to the one of them owed most (LandUnit.owed:
   * the harm each striker has done it, less a model's worth for each model given), the first of equals; a gun or a tank
   * given a model lands a shell there. So guns are seen firing in their share of the harm, not only where they did the
   * most in a tick (the design director, b507). Looks only: nothing in the fight reads it.
   */
  private suffer(t: LandUnit, dmg: number, nerveMul: number, from: [LandUnit, number][] = []): void {
    // Damage is in power: as a share of what the block came with, it comes off its strength meter.
    const share = dmg / Math.max(1e-9, t.power0);
    const before = t.str, had = t.figures;
    t.str = Math.max(0, t.str - share * t.str0);
    const lost = (before - t.str) / t.str0;
    const steady = 1 - LAND.nerve.xp * t.xp - LAND.nerve.general * this.generalOf(t.side);
    t.mor = Math.max(0, t.mor - lost * LAND.nerve.perLoss[t.side] * nerveMul * Math.max(0.3, steady));
    t.still = 0;
    t.figures = t.str <= 0 ? 0 : Math.max(1, Math.ceil(t.tray * t.str));
    const owed = (t.owed ??= {}), model = t.power0 / Math.max(1e-9, t.tray * t.str0);
    for (const [u, d] of from) owed[u.id] = (owed[u.id] ?? 0) + d;
    for (let k = t.figures; k < had; k++) {
      const w = t.kind === 'arm' ? this.wreck(t) : null;
      let by: LandUnit | undefined;
      for (const [u] of from) if (!by || owed[u.id] > owed[by.id]) by = u;
      if (!by) { this.drill?.fromAir(t.id, k); continue; }
      owed[by.id] -= model;
      if (SHELLS_FROM.includes(by.kind) && by.team !== 'mg') this.shell(by, t, k, w);
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
    if (u.str <= N.shattered * u.str0 || u.mor < N.rout) { this.rout(u); return; }
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
  }

  private move(u: LandUnit): void {
    const k = u.kind;
    u.pressing = undefined;
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
      if (u.order.press && t && !fighting(t) && !this.ai[u.side]) {
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
    const m = MOTION[k];
    const g = groundAt(this.field, u.x, u.y);
    let pace = LAND.pace[m][g];
    if (k === 'mtn' && (g === 'rough' || g === 'rock')) pace = Math.min(1, pace * LAND.mountainFoot);
    // Up a slope it slows; out of the river's bed it wades at the wading pace, which counts the bank already (b464(5):
    // every bank read as a cliff above the bed, and foot crawled their last 20 m of the river at 0.8 m a second).
    const up = inRiver(g) ? 0 : heightAt(this.field, u.x + Math.sin(head) * 20, u.y + Math.cos(head) * 20) - heightAt(this.field, u.x, u.y);
    pace *= up > 0 ? Math.max(0.4, 1 - (up / 20) * 1.5) : 1;
    const top = u.team ? LAND.teams[u.team].speed : LAND.speed[k];
    let v = top * Math.max(0.12, pace) * (run ? 1.15 : 1) * (u.state === 'wavering' ? 0.85 : 1);
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
    } else if (u.order?.kind === 'withdraw' && (u.side === 'att' ? u.y <= 6 : u.y >= this.field.h - 6)) {
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
    if (MOTION[u.kind] === 'fixed') return false;
    const g = groundAt(this.field, clamp(x, 5, this.field.w - 5), clamp(y, 5, this.field.h - 5));
    if (g === 'bridge' || g === 'water' || g === 'blown' || LAND.pace[MOTION[u.kind]][g] <= LAND.queue.hard) return true;
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
    const pace = LAND.pace[MOTION[u.kind]], f = this.field;
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
      s[u.side] += (u.power0 * (str / u.str0) * k) / tot;
    }
    return s;
  }

  private brokenShare(s: Side): number {
    let gone = 0;
    for (const u of this.units) if (u.side === s && (u.state === 'routing' || u.state === 'gone')) gone += u.power0;
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
    const on = mine.filter((u) => u.state !== 'withdrawn'), p0 = on.reduce((a, u) => a + u.power0, 0);
    const steady = p0 > 0 ? on.filter((u) => u.state === 'steady').reduce((a, u) => a + u.power0, 0) / p0 : 0;
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
    for (const u of this.units) if (u.side === s && fighting(u) && this.fresh(u)) n += u.power0;
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
      const left = this.units.filter((u) => u.side === s && fighting(u));
      if (!left.length) {
        const how = this.units.some((u) => u.side === s && u.state === 'withdrawn') && !this.units.some((u) => u.side === s && u.state === 'routing') ? 'withdrew' : 'destroyed';
        // Driven from the field, it broke: why, as it stood when the last block left the fight (b496(2)).
        this.finish(other(s), how, undefined, how === 'destroyed' ? this.breakCause(s) : undefined);
        return;
      }
      const b = this.brink(s), near = b.bar;
      this.bar[s] = near;
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
    if (this.t >= this.ticket.limit) this.finish('def', 'time');
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
    const loser = other(winner);
    // "about 4%", or under one in a hundred "less than 1%", never "about 0%" (the UI director's words, b358).
    // Where nothing was lost at all, it says "none" (b408).
    const pct = (p: number): string => (p < 1e-4 ? 'none' : p < 0.005 ? 'less than 1%' : `about ${Math.round(p * 100)}%`);
    const cap = (t: string): string => t[0].toUpperCase() + t.slice(1);
    const line = this.practice && this.ticket.def.human ? 'Your line' : this.practice ? 'Their line' : `The ${this.ticket.def.adj} line`;
    const lines = [
      how === 'rout' ? this.brokeAfter(loser)
        : how === 'withdrew' ? `${this.army(loser)} withdrew from the field.`
          : how === 'time' ? `${line} held until the day was done.`
            : how === 'destroyed' ? `${this.army(loser)} was driven from the field.`
              : this.staffLine(),
    ];
    // Broken, why, in the past: straight after the line that says when (the UI director, 14:44Z). So after a rout the
    // strength line is the third, and the pursuit's line comes after it. Its figure is at the break, the strength line's
    // at the end, so the strength line opens "In all," under it (the UI director, 16:37Z).
    const told = how === 'rout' && why && why.by !== 'none';
    if (told) lines.push(breakLine(why, this.whose(loser), true));
    // Handed over, the field's figure is only the part before the staff took it: it says so, as it reads beside the
    // campaign's bill for the whole day (the design director, b356). Handed over before a shot, the staff's line says so
    // already, and there is no figure to give (the UI director, after the retest).
    if (how !== 'objective') lines.push(lost.att < 1e-4 && lost.def < 1e-4 ? 'Neither side lost any of its strength.' : `${told ? `In all, ${this.sideOf('att', 'attack')}` : cap(this.sideOf('att', 'attack'))} lost ${pct(lost.att)} of its strength, ${this.sideOf('def', 'defence')} ${pct(lost.def)}.`);
    else if (this.contactAt !== null) lines.push(`Before you handed the battle to the staff, ${this.sideOf('att', 'attack')} had lost ${pct(lost.att)} of its strength on the field, and ${this.sideOf('def', 'defence')} ${pct(lost.def)}.`);
    // The battle is over: nothing marches on (the blocks' cards and the 3D models alike), and none is left standing in the
    // river: one still wading it is set on the nearer bank (b464(5)). The figures above are already counted.
    for (const u of this.units) if (live(u) && u.state !== 'routing') { u.act = 'idle'; u.speed = 0; u.engaged = undefined; }
    for (const u of this.units) if (live(u) && inRiver(groundAt(this.field, u.x, u.y))) { [u.x, u.y] = this.shore(u, 0); u.cover = groundAt(this.field, u.x, u.y); }
    this.over = {
      winner: verdict, how, seconds: Math.round(this.t), lost, men, R: Math.round(r * 1000) / 1000,
      level: levelOf(r, men.att > men.def), lines,
      forces: [...forces.values()].map((f) => ({ corps: f.corps, side: f.side, lost: f.p > 0 ? Math.round((f.lost / f.p) * 1000) / 1000 : 0 })),
      ...(how === 'rout' || how === 'destroyed' ? { broke: loser, ...(why ? { brokeBy: why } : {}) } : {}),
      // Pulled out after the armies met: the rearguard and the stragglers are the rules' to bill, by its bar (b474).
      ...(how === 'withdrew' && this.contactAt !== null ? { pulled: { side: loser, bar: Math.round(Math.min(1, this.bar[loser]) * 1000) / 1000 } } : {}),
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
  private brokeAfter(s: Side): string {
    const sec = this.t - (this.contactAt ?? this.t), who = this.army(s);
    // "Of fighting" on every form (the UI director, b464).
    const halves = Math.round(sec / 30), m = Math.floor(halves / 2), half = halves % 2 === 1;
    if (halves === 0) return `${who} broke within half a minute of fighting.`;
    const n = ['', 'a', 'two', 'three', 'four', 'five', 'six'][m] ?? `${m}`;
    const span = m === 0 ? 'half a minute' : m === 1 ? (half ? 'a minute and a half' : 'a minute') : `${n}${half ? ' and a half' : ''} minutes`;
    return `${who} broke after ${span} of fighting.`;
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
    // As toDice would roll it: what the fighters on call still owe paid first.
    const k = this.ticket.reckoning, sh = this.standingShares(this.airDue());
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
    const k = this.ticket.reckoning, bal = this.standing(this.airDue()), r = bal.att / Math.max(1e-6, bal.def) / Math.max(1e-6, k.odds);
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
    const foes = this.units.filter((e) => e.side !== s && live(e) && e.kind !== 'fort' && this.known(e));
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
    void as; // Every block on the field is in sight: what the player knows of each is known() (b464(2)).
    return { t: this.t, limit: this.ticket.limit, units: this.units, balance: this.standing(), events: ev, zones: this.wrecks.concat(this.craters).map((z) => ({ ...z })), ai: { ...this.ai }, over: this.over, breaking: { att: this.breaking('att'), def: this.breaking('def') }, breakingBy: { att: this.breakCause('att'), def: this.breakCause('def') }, contact: this.contactAt, ...(this.setup ? { setup: this.setup } : {}), ...this.airView(), ...this.planView() };
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
