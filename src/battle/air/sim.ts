// The air battle: air groups as blocks on a 100 km board round the target, wearing each other down while in contact.
//
// Counted, not flown (book.ts). Each tick: the computer gives its orders; groups climb or dive between the three
// heights and move toward what their job asks; fighters burn fuel; groups that touch are in contact and lose
// aircraft at the rates the campaign's own numbers give; the guns round the target wear down whatever flies through
// them; bombers that hold their run over the target drop; morale breaks groups that bleed too much. When a group's
// count drops by one, an 'air:lost' event says how the 3D side shows that aircraft going down.

import { Rng } from '../../core/rng';
import { dist, bearing } from '../core/geometry';
import { fieldRatio } from '../core/result';
import type { BattleSim, BattleView, Command, FieldEvent, FieldResult, FieldTicket, Side, UnitState, Zone } from '../core/types';
import { DT } from '../core/types';
import { SKY, type Band } from './book';
import { aCraftOf, adjFor, oneOf, subjectOf, told, twoOf, upFirst } from './names';
import { enginesOf, escortRoles, fighterWay, passesOn, pickPass, type Pass, type Role, type When } from './doctrine';
import { collideWords, drawFall, fallWords } from './falls';
import { ShipBook, splitWings } from './ships';
import { TroopBook, TROOPS, type MarkNews, type MarkWhat } from './troops';
import { coolOf, crewOf, fateOf, fighterEngines, freshMemory, hitOf, kindOf, ramShare, seaAt, type FateMemory, type Scene } from './fates';
import { fighterGuns, type GunKind } from './guns';
import type { AirJob, AirOutcome, AirPlace, AirUnit, BomberTold, Fall, FallHow, Straggler } from './types';
import type { BomberSeat } from '../../campaign/types';
import { afterWords, bombWorth, railChanceOf, ruinChanceOf, staffHow, troopCutOf } from '../../campaign/airfought';
import type { VictoryLevel } from '../../core/ticket';
export { peopleOf } from '../../campaign/airfought';

const C = SKY.cues;

/** One of a tick's losses, before how it goes is drawn (AirSim.fated). */
interface Loss {
  u: AirUnit; fall: Fall; by: number | 'flak' | null | undefined; s: Straggler | null; alt: number; fid: number;
  /** The models this loss brings down in the picture (AR1: a model falls only for its share of the losses): 0, 1, or the rest at the last. */
  models: number;
  /** A bomber known by name: its seat in its group's list (-1: none). A fighter that fell to a named box's gunners: that box and the seat of the bomber that got it. */
  seat: number;
  gun: [box: number, seat: number] | null;
}

/** A box whose bombers are known by name (AirPlace.bombers): the seats in formation, those that limped home, those that fell, and each one's gunners' kills by nation. */
interface Seated { all: BomberSeat[]; told: number; flown: string; form: number[]; away: number[]; spare: number; fell: number[]; shot: Map<number, Record<string, number>> }

/** A named bomber as an event carries it. */
const toldOf = (B: Seated, seat: number): BomberTold => {
  const b = B.all[seat];
  return { seat, name: b.name, mark: b.mark, art: b.art, missions: b.missions, kills: b.kills };
};

const clamp01 = (v: number): number => (v < 0 ? 0 : v > 1 ? 1 : v);
const off = (u: AirUnit): boolean => u.state === 'gone' || u.state === 'withdrawn';
const flying = (u: AirUnit): boolean => !off(u) && u.job.do !== 'wait';
/** A group that has broken off or turned for home: it runs, and does not fight back. */
const away = (u: AirUnit): boolean => u.job.do === 'home' || u.state === 'routing';
/** An escort's role: close, top cover or the sweep (a loose escort). */
const roleOf = (j: { close: boolean; top?: boolean }): Role => (j.close ? 'close' : j.top ? 'top' : 'sweep');
/**
 * The share of a raider fighter that counts on target, as the campaign counts it: the staff's raid weighs each fighter
 * by how far into its range the target lies (assessAir's esc, which the ticket gave the group as its fuel).
 */
const reachOf = (u: AirUnit): number => Math.min(1, u.fuel0 / SKY.fuel.full);

/**
 * A fighter group's passes on the troops (a raid on troops, release 2): the passes it has completed, whether it is on
 * one now and how far down the lane, the lane it is flying, and the bomber box it was escorting when it went down (it
 * still counts over that box: tied).
 */
interface Strafing { done: number; run: boolean; p: number; from: [number, number]; to: [number, number]; box: number | null; over: boolean }

// The names in a dispatch (names.ts), as the tests and the battle's pages have them from here.
export { oneOf, subjectOf, told };

export class AirSim implements BattleSim<AirUnit> {
  readonly domain = 'air' as const;
  readonly ticket: FieldTicket<AirPlace>;
  readonly place: AirPlace;
  t = 0;
  units: AirUnit[] = [];
  over: FieldResult | null = null;
  ai: Record<Side, boolean>;
  private rng: Rng;
  private fallRng: Rng;
  /** Whether a share of an aircraft owed this tick falls (fall). */
  private lossRng: Rng;
  private events: FieldEvent[] = [];
  private log: FieldEvent[] = [];
  private zones: Zone[];
  private thinkAt = 0;
  private quietFor = 0;
  private scrambled = false;
  private gaveUp: Side | null = null;
  private toStaff = false;
  /** Each side's losses when the player handed the rest to the staff, for the after card. */
  private before: { att: { ftr: number; bmr: number }; def: { ftr: number; bmr: number } } | null = null;
  /** The staff's reckoning of what the raid would cost each pool, at the start (see expect), and what is handed back at the press. */
  private start = { foe: 0, ftr: 0, bmr: 0 };
  private staff: AirOutcome['staff'];
  /** Each group's morale at the start, for how near its side is to breaking off; the sides already warned of it. */
  private mor0 = new Map<number, number>();
  private warned = new Set<Side>();
  /** Whether any group has met an enemy yet (the first contact). */
  fired = false;
  /** The bombers the staff's reckoning expected through at the start (see throughNow). */
  private through0 = 0;
  /**
   * The field's throws for the railway and for a factory, drawn when the battle starts (b397): the line is cut, or a
   * factory wrecked, as soon as its chance from the bombers that have dropped comes above its throw. fly() takes the
   * same throws, so the campaign books what the field showed.
   */
  private rolls: { rail: number; ruin: number };
  /** Whether the bombs have cut the railway, and wrecked a factory outright. */
  railCut = false;
  ruined = false;
  private flakT: number;
  private flakIn: number;
  /**
   * The campaign's bomber-loss law as the raid card weighed it: the defenders' weight, the escort's weight per bomber,
   * how many times over the escort counts, the bombers' own guns, and the defenders' share of the fighting weight.
   */
  private law = { D: 0, B: 0, ePer: 0, escort: 1, guns: 0, met: 0 };
  /**
   * What the 3D game acts out beyond the counts (the design's section 1; ruling 237): who shot whom, the pass each
   * fighter flies at a bomber group, the stragglers, ammunition, the formation's state, an escort breaking off and
   * rejoining. None of it moves a count, the bombs or the verdict: its throws come from streams of their own (a fork
   * never moves its parent), and the tests run every battle with it on and off and find the same result.
   */
  readonly show: boolean;
  private when: When;
  private stragRng: Rng;
  private passRng: Rng;
  /**
   * The battle's own stream for how each loss goes down (fates.ts), drawn whether or not the battle is shown, for the
   * dispatches tell it. It never touches a count.
   */
  private lookRng: Rng;
  /** The same for a straggler's fire and its regaining the box, and for hits that do not bring an aircraft down (shown battles only). */
  private stragLook: Rng;
  private hitRng: Rng;
  /** The aircraft one model stands for, the same for every group in this battle (SKY.look.all). */
  private per = 1;
  /** The models the battle may show at the start: the 3D game's SKY.look.all, or the browser page's own (its option). */
  private look: number;
  /** A box's stragglers that are shown as a model falling out (AR1): the box's next loss is its straggler's, so one is shown only when that loss brings a model down. */
  private stragShown = new Set<number>();
  private fid = 1;
  private fates: FateMemory = freshMemory();
  /** The bombers lost out of a box's formation in the last moments, that may strike another of it (fates.ts near). */
  private recent: { box: number; fid: number; how: FallHow; t: number; used: boolean; foe?: boolean }[] = [];
  /** Battle seconds each group last showed a hit it survived; the heaviest gun each fighter group carries. */
  private hitAt = new Map<number, number>();
  private gunOf = new Map<number, GunKind | null>();
  /** The pass each fighter group is flying at each bomber group it is on ('fighter:bomber'). */
  private passes = new Map<string, Pass>();
  /** This tick, what each group was owed and by whom ('flak' for the guns): who shot down whom. */
  private owedBy = new Map<number, Map<number | 'flak', number>>();
  /** Each bomber group's gunners' kills: the enemy fighters whose loss was booked to it (ruling 227), for the campaign's record. */
  private gunKills = new Map<number, number>();
  private stragId = 1;
  /**
   * Bombers by name (AirPlace.bombers): each such box's seats, and the seat each straggler holds. Which bomber a loss
   * is, and which of a box's bombers got a fighter booked to the box, is drawn from a stream of its own in shown and
   * unshown battles alike, and is never a count: the same aircraft fall at the same moments with the names or without them.
   */
  private seatRng: Rng;
  private seated = new Map<number, Seated>();
  private stragSeat = new Map<number, number>();
  /** The escort groups that have broken away from their bombers at least once (told once each). */
  private broke = new Set<number>();
  /**
   * The beat of the passes on each box the defence's fighters are firing on (SKY.flak.pass): going in or standing off,
   * the air seconds of it left, and the group pressing the box hardest. Kept in shown and unshown battles alike, from a
   * stream of its own, and never a count.
   */
  private beatRng: Rng;
  private beats = new Map<number, { go: boolean; left: number; by: number }>();
  /** The boxes the target's guns are holding their fire on, and the fighter group each is held for (SKY.flak.hold). */
  private held = new Map<number, number>();
  /** The boxes whose first holding of fire, and first opening up again, have been told: each is told once a box. */
  private heldTold = new Set<number>();
  private openTold = new Set<number>();
  /** The enemy group each close escort has broken off to engage, if any. */
  private chasing = new Map<number, number | null>();
  /** A strike on ships (ships.ts, ruling 238): the ships under attack, and what the guns' toll is scaled by once carrier groups fly at their own heights. */
  private sea: ShipBook | null = null;
  private flakNorm = 1;
  /**
   * A raid on troops (release 2; the design director, 24 November 2026 22:28Z): the troops under the raid and what the
   * campaign's own law has earned on them so far (troops.ts). The bombers count as they drop; the raid's fighters count
   * as they complete their passes on the troops, and pay the ground fire the raid card expects of them as they fly
   * those passes. Null for any other raid, and then nothing below is ever read or written.
   */
  private troops: TroopBook | null = null;
  /** Each fighter group's passes on the troops; the fighters that have strafed so far, as the campaign counts them. */
  private strafing = new Map<number, Strafing>();
  private strafed = 0;
  /** The raid's fighter groups ordered to stay with the bombers (or given any other order): they never go down on their own. */
  private keepUp = new Set<number>();
  /** The bomber box each of the raid's fighter groups was last escorting (it goes down once that box has bombed). */
  private escorted = new Map<number, number>();
  /** The ground fire each strafing group is owed this tick, from the passes it flew (paid in wear, as the guns' toll is). */
  private fireDue = new Map<number, number>();
  /** The ground fire owed for each fighter that flies a whole pass's share: the card's 'fire' over the fighters it weighed. */
  private fireEach = 0;
  /** With the strafers' rule on: the campaign groups charged the ground fire, and what it charged them. */
  private firedOn = new Set<number>();
  private fireOwed = 0;
  /** The lane the passes run down: through the middle of the troops, along their road where they march. */
  private lane = { x: 0, y: 0, dx: 0, dy: 1 };
  /** Battle seconds of the first contact, null before it (the view's contact, as land's). */
  private metAt: number | null = null;
  /** The defending groups whose staff has weighed its climb (once each: a group that has dived is never sent back up). */
  private risen = new Set<number>();
  /** A split defence's pool (arm): each group's weight, by which it shares what the defence still in the fight has left. */
  private pool: Map<number, number> | null = null;
  /** The escorts that have fought the defence, each with the box it was escorting: one fought out still counts over it (tied). */
  private foughtFor = new Map<number, number>();
  /**
   * For the pilots the 3D game flies (Brian, 20 November 2026: "lets work on the ai for the fighter pilots. they were
   * kinda just flying around doing nothing"; "super realistic and meaningful and everyone has high AI iq"). Shown, never
   * counted: the enemy group each fighter group is making for this tick (aim), so its flights can take up their
   * places for the attack before the groups meet; and how good its pilots are, 0 to 1, from its readiness and its
   * leader, which the scene reads for how close they press, how quickly they see a fighter behind them and how well a
   * wingman holds on to his leader.
   */
  private after = new Map<number, number>();
  private skill = new Map<number, number>();

  constructor(ticket: FieldTicket<AirPlace>, opts: { show?: boolean; look?: number } = {}) {
    this.ticket = ticket;
    this.place = ticket.place;
    this.show = opts.show ?? true;
    this.look = Math.max(1, opts.look ?? SKY.look.all);
    this.when = ticket.place.when ?? { year: 1939, month: 8 };
    this.rng = new Rng(ticket.seed);
    this.fallRng = this.rng.fork(7);
    this.lossRng = this.rng.fork(11);
    this.rolls = { rail: this.rng.fork(13).next(), ruin: this.rng.fork(17).next() };
    this.stragRng = this.rng.fork(19);
    this.passRng = this.rng.fork(23);
    // 29 and 31 are the ship strikes' (the Pearl Harbor and Midway rules, ships.ts): the looks keep clear of them.
    this.lookRng = this.rng.fork(41);
    this.stragLook = this.rng.fork(43);
    this.hitRng = this.rng.fork(47);
    // 53 is the crews' (fates.ts: the men past the fifth). 59: which bomber by name a loss is.
    this.seatRng = this.rng.fork(59);
    this.beatRng = this.rng.fork(61);
    this.ai = { att: !ticket.att.human, def: !ticket.def.human };
    const T = SKY.target;
    this.zones = [
      { id: 1, kind: 'air:target', x: T.x, y: T.y, r: T.r, until: Infinity },
      { id: 2, kind: 'air:flak', x: T.x, y: T.y, r: SKY.flakR, until: Infinity, side: 'def' },
    ];
    // The time a box spends in the flak ring on the staff's run, in to where it drops (it holds the run from the edge
    // of the target ring). The guns' share of the bombers is spread over it: the raid card counts every bomber the
    // guns bring down as one that never bombed, so they fire at the boxes on the run in. Escorts are under the guns
    // in and out.
    this.flakIn = (SKY.flakR - SKY.target.r) / SKY.speed.bmr + SKY.run;
    this.flakT = (2 * (SKY.flakR - SKY.target.r)) / SKY.speed.bmr + 2 * SKY.run;
    // One scale for the whole battle (Brian, 21 November 2026: the numbers "proportionate and realistic"): every model
    // stands for the same number of aircraft, whichever group it flies in, so the picture shows the true odds.
    let all = 0;
    for (const side of ['att', 'def'] as Side[]) for (const f of ticket[side].forces) if (this.place.starts[f.corps]) all += Math.max(0, f.men);
    // How many that is depends only on who draws it (the 3D game's budget or the browser page's): drawing never moves a count.
    this.per = Math.max(1, Math.ceil(all / this.look));
    let id = 1;
    for (const side of ['att', 'def'] as Side[]) {
      for (const f of ticket[side].forces) {
        const s = this.place.starts[f.corps];
        if (!s) continue;
        const kind = f.kind === 'bmr' ? 'bmr' : 'ftr';
        const mor = clamp01(SKY.morale.base + SKY.morale.ready * f.org);
        const model = this.place.models?.[f.nat]?.[kind] ?? (kind === 'bmr' ? `Bomber:${f.nat}` : `Fighter:${f.nat}`);
        const way = kind === 'ftr' ? fighterWay(f.nat, this.when) : { form: 'box' as const, style: null };
        const u: AirUnit = {
          id: id++, side, corps: f.corps, nat: f.nat, kind, name: f.name,
          x: s.x + (kind === 'bmr' ? 0 : 1_500 * (side === 'att' ? 1 : 0)), y: s.y,
          // Defenders face the target they rise to cover; a fleet's own cover, already out over it, faces the strike (S1).
          face: side === 'att' ? 0 : s.over ? bearing(T.x, T.y, s.x, s.y) : bearing(s.x, s.y, T.x, T.y),
          str: 1, mor, state: 'steady', men0: f.men, ...this.drawn(kind, f.men), act: s.wait ? 'idle' : 'march', speed: 0,
          air: kind, n: f.men, n0: f.men, band: s.band, to: s.band, alt: s.wait ? 0 : SKY.bands.alt[s.band], climbT: 0,
          fuel: kind === 'ftr' ? s.fuel : SKY.fuel.home, fuel0: kind === 'ftr' ? s.fuel : SKY.fuel.home,
          job: s.wait ? { do: 'wait' } : side === 'att' && kind === 'bmr' ? { do: 'route' } : { do: 'patrol', x: s.x, y: s.y },
          q: kind === 'bmr' ? 1 : this.place.worth[side], bounce: 0, run: 0, dropped: 0, jettisoned: false, shot: 0, owed: 0, back: 0,
          contacts: [], vig: 1, kit: { ftr: 0, bmr: 0 }, kit0: 0, luck: 1, lane: 0, homeX: s.x, homeY: side === 'att' ? -5_000 : s.y,
          model, form: way.form, style: way.style, fire: [], strag: [], away: 0,
        };
        this.units.push(u);
        if (kind === 'ftr') this.skill.set(u.id, Math.round(clamp01(SKY.pilots.base + SKY.pilots.leader * f.xp + SKY.pilots.ready * f.org) * 100) / 100);
      }
    }
    // A strike on ships: each carrier group's bombers split by kind, and the ships under attack with the battle's throws.
    if (this.place.ships) {
      const sp = splitWings(this.units, this.place.ships, (n) => this.drawn('bmr', n), () => id++);
      this.units = sp.units;
      this.flakNorm = sp.flakNorm;
      const boxes = this.units.filter((u) => u.side === 'att' && u.air === 'bmr');
      this.sea = new ShipBook(this.place.ships, this.rng.fork(29), this.rng.fork(31), boxes.reduce((t, u) => t + u.n, 0), this.ticket.def.human ? this.ticket.def.nat : null);
      // Each wing's quarry, carriers first (the tactics panel's S2): where the 3D bends its run, and where its hits land.
      this.sea.aim(boxes);
    }
    // A raid on troops: the army under it, its looks on a stream of their own that no count reads (fork 71; 67 is the
    // ticket's, which laid the pieces), and the lane the fighters' passes run down.
    if (this.place.marks) {
      this.troops = new TroopBook(this.place.marks, this.rng.fork(71));
      this.lane = laneOf(this.place.marks.pieces, this.place.marks.road);
    }
    for (const u of this.units) {
      const B = u.air === 'bmr' ? this.place.bombers?.[u.corps] : undefined;
      // A carrier group's bombers split by kind fly as several boxes of one group: those are not known one by one.
      if (!B || !B.seats.length || this.units.some((x) => x !== u && x.air === 'bmr' && x.corps === u.corps)) continue;
      const k = Math.min(u.n0, B.seats.length);
      this.seated.set(u.id, { all: B.seats, told: B.told, flown: B.flown, form: Array.from({ length: k }, (_, i) => i), away: [], spare: u.n0 - k, fell: [], shot: new Map() });
    }
    // The raid comes on as one stream: its bomber boxes side by side on one line to the target, the escorts with them.
    const raid = this.units.filter((u) => u.side === 'att');
    if (raid.length) {
      const entry = raid.reduce((s, u) => s + u.x, 0) / raid.length;
      const boxes = raid.filter((u) => u.air === 'bmr');
      boxes.forEach((u, k) => { u.lane = (k - (boxes.length - 1) / 2) * SKY.stream; u.x = entry + u.lane; u.homeX = u.x; });
      raid.filter((u) => u.air === 'ftr').forEach((u, k) => { u.x = entry + (k % 2 ? 1 : -1) * (SKY.escort.close + Math.floor(k / 2) * 600); u.homeX = u.x; });
    }
    this.deal();
    this.arm();
    // The ground fire on the strafing fighters: the card's share of the raid's fighter losses ('fire', forecastAir: the
    // ground fire's share of every fighter it weighed), owed by each fighter for each pass it flies, so a raid whose
    // fighters all fly all their passes pays on average what the card said, and one whose fighters never go down none.
    if (this.troops) {
      const E = this.units.reduce((t, u) => t + (u.side === 'att' && u.air === 'ftr' ? u.n0 * reachOf(u) : 0), 0);
      this.fireEach = E > 0 ? (this.place.forecast.fire ?? 0) / E : 0;
      // (The strafers' rule, campaign/strafers.ts: each fighter that goes down pays the campaign's own share, as the card
      // charged each one it expected to go down; those without the reach never go down.)
      if (this.place.marks?.law.reach) this.fireEach = this.place.marks.law.fire;
    }
    // Both sides start on the staff's plan; the player changes what he likes before or after he starts the clock.
    this.think('att', true);
    this.think('def', true);
    // Each escort starts at the station its role gives it: top cover a band above its box and behind it, the sweep
    // ahead of it (the close escort is already beside it). The staff's reckoning at the start sees them there.
    for (const u of raid) {
      if (u.job.do !== 'escort' || u.job.close) continue;
      const b = this.unit(u.job.on)!, hd = bearing(b.x, b.y, SKY.target.x, SKY.target.y);
      const d = u.job.top ? -SKY.escort.behind : SKY.escort.ahead;
      u.x = b.x + Math.sin(hd) * d; u.y = Math.max(0, b.y + Math.cos(hd) * d); u.homeX = u.x;
      if (u.job.top) { u.band = u.to; u.alt = SKY.bands.alt[u.band]; }
    }
    this.start = this.expect();
    for (const u of this.units) this.mor0.set(u.id, u.mor);
    this.through0 = this.throughNow(this.place.forecast.bmr);
    // The opening view carries what the 3D game reads as the first tick gives it (the aircraft, tactics, a box's
    // strike): the scene builds every group from that view. Nothing has moved yet and no box has a straggler, so with
    // no time passed this sets only each group's 'extra', which the battle never reads, and draws no throw.
    if (this.show) for (const u of this.units) this.shown(u, 0);
  }

  /**
   * How a group of n aircraft is drawn: how many models, one for every 'per' aircraft, in a box of w by d metres.
   * 'per' is the battle's own (the constructor: all the aircraft sent up over the battle's budget), the same for every
   * group, so a group of 160 shows about seven times the models of a group of 24. No group is cut down or made up to a
   * size. The box is one box's worth at most (SKY.look.box, AR2): more models fly as more boxes in trail ('boxes' in
   * the view's extra), never as one ever wider line abreast.
   */
  private drawn(kind: 'ftr' | 'bmr', n: number): { figures0: number; figures: number; per: number; w: number; d: number } {
    const L = SKY.look, per = this.per, figures = Math.max(1, Math.ceil(n / per));
    const box = Math.min(figures, L.box[kind]);
    const rows = Math.min(L.rows[kind], box), cols = Math.ceil(box / rows);
    return { figures0: figures, figures, per, w: cols * L.gap[kind], d: rows * L.gap[kind] };
  }

  /** The boxes (a fighter group's squadrons) its models fly in now (AR2). */
  private boxesOf(u: AirUnit): number {
    return Math.max(u.figures > 0 ? 1 : 0, Math.ceil(u.figures / SKY.look.box[u.air]));
  }

  /**
   * The models a group shows after it loses one more aircraft (AR1, falls at the model's scale): its losses so far stand
   * for round(lost / per) models, so falls x per = losses over the battle. A straggler that was shown falling out is the
   * model that falls; one that was not shown brings none down. It keeps one model while it has an aircraft left, and
   * none once it has none: the last loss brings down whatever is left. At one model an aircraft it is the aircraft left.
   */
  private modelsAfter(u: AirUnit, s: Straggler | null): number {
    if (u.n <= 0) return 0;
    const fell = u.figures0 - u.figures, owed = Math.floor((u.n0 - u.n) / u.per + 0.5);
    const down = s ? (this.stragShown.delete(s.id) ? 1 : 0) : owed > fell ? 1 : 0;
    return Math.max(1, u.figures - down);
  }

  /**
   * A group's name in a dispatch: his own bare, as the tray has them; the enemy's with its country word first, as every
   * enemy named on the field (the UI director's ruling 226(7), carried to the air: "French Groupe de Chasse 3 is in a
   * fight."). With nobody's hand on either side, both bare; and never the nation word twice ("Polish Navy strike wing").
   */
  called(u: AirUnit): string {
    const adj = adjFor(u.name, this.adjOf(u));
    return adj ? `${adj} ${u.name}` : u.name;
  }

  /**
   * The enemy's nation word ('' for his own, and with nobody's hand on either side). Each line drops it where the name
   * already says it (names.ts adjFor), but it still marks the group as the enemy's ('a carrier fighter of the Brazilian Fleet').
   */
  private adjOf(u: AirUnit): string {
    const t = this.ticket;
    return t[u.side].human || !(t.att.human || t.def.human) ? '' : t[u.side].adj;
  }

  private say(what: FieldEvent['what'], u: AirUnit | null, text: string, extra?: Record<string, unknown>, cue?: string): void {
    const at = extra && typeof extra.x === 'number' ? { x: extra.x as number, y: extra.y as number } : u ? { x: u.x, y: u.y } : {};
    const e: FieldEvent = { t: this.t, what, text, ...(u ? { unit: u.id, side: u.side } : {}), ...at, ...(cue ? { cue } : {}), ...(extra ? { extra } : {}) };
    this.events.push(e);
    this.log.push(e);
  }

  unit(id: number): AirUnit | undefined { return this.units.find((u) => u.id === id); }

  // ---------------------------------------------------------------------------------------------------------------
  // The computer's orders. It plans for its own side every few seconds; for the player's side only once, at the start.

  private nearest(u: AirUnit, pred: (v: AirUnit) => boolean): AirUnit | null {
    let best: AirUnit | null = null, bd = Infinity;
    for (const v of this.units) {
      if (v.side === u.side || off(v) || !flying(v) || !pred(v) || !this.sees(u.side, v)) continue;
      const d = dist(u.x, u.y, v.x, v.y);
      if (d < bd) { bd = d; best = v; }
    }
    return best;
  }

  /**
   * The role a raid's k-th escort group flies (its fighter groups in order), by its nation's doctrine that month. Top
   * cover only from the third group (SKY.escort.topFrom): dealt to the second, its bounce moved the two-group raid's
   * fighter losses past a tenth of the card (the design director's fallback, ruling 237.2), and the record had it in
   * the large raids. The first two take the doctrine's other roles in its order.
   */
  private role(u: AirUnit): Role {
    // At stops 4 and 5 a computer raid orders its roles by its weight against the defence (the ticket's roles, styles.ts).
    const roles = this.place.roles?.[u.nat] ?? escortRoles(u.nat, this.when), from = SKY.escort.topFrom;
    const k = Math.max(0, this.units.filter((f) => f.side === 'att' && f.air === 'ftr').indexOf(u));
    const rest = roles.filter((r) => r !== 'top');
    if (k < from) return rest[k % rest.length];
    const top = roles.indexOf('top');
    return roles[(k - from + Math.max(0, top)) % roles.length];
  }

  private valid(u: AirUnit, job: AirJob): boolean {
    if (job.do === 'escort') { const b = this.unit(job.on); return !!b && !off(b) && b.side === u.side; }
    if (job.do === 'intercept') { const e = this.unit(job.on); return !!e && flying(e) && e.side !== u.side; }
    return true;
  }

  private think(side: Side, first = false): void {
    for (const u of this.units) {
      if (u.side !== side || off(u) || u.job.do === 'home' || u.job.do === 'wait' || u.state === 'routing') continue;
      if (u.air === 'bmr') { if (u.job.do !== 'route') u.job = { do: 'route' }; continue; }
      if (side === 'att') {
        // (A group on its passes on the troops keeps at them: the staff sent it down.)
        if (u.job.do === 'strafe') continue;
        if (!first && this.valid(u, u.job) && u.job.do !== 'patrol') continue;
        if (this.place.mission === 'superiority') { u.job = { do: 'hunt' }; continue; }
        // The escort, shared out over the bomber groups by their size, each group in the role its nation's doctrine gave
        // it that month: the raid's first escort group the first role, the second the next (ruling 237.2). A group whose
        // box is gone keeps its role with the next box.
        const boxes = this.units.filter((b) => b.side === 'att' && b.air === 'bmr' && !off(b));
        // With no bombers to see through, a raid on troops sends its fighters down on the troops (once each).
        if (!boxes.length) { if (this.mayStrafe(u)) this.goDown(u, null); else u.job = { do: 'home' }; continue; }
        const load = (b: AirUnit): number => this.units.filter((f) => f.job.do === 'escort' && f.job.on === b.id).reduce((s, f) => s + f.n, 0) / b.n;
        const b = [...boxes].sort((x, y) => load(x) - load(y) || x.id - y.id)[0];
        const role = u.job.do === 'escort' ? roleOf(u.job) : this.role(u);
        u.job = { do: 'escort', on: b.id, close: role === 'close', ...(role === 'top' ? { top: true } : {}) };
        u.to = role === 'top' ? Math.min(2, b.band + 1) as Band : b.band;
      } else {
        // The defenders go for the bombers that have yet to drop; failing those, any raider still coming on. A group
        // its staff sent at the escort (deal) goes for the raid's nearest fighters still coming on while it has fight
        // left for fighters, and then as the rest do.
        const loaded = (v: AirUnit): boolean => !away(v) && v.air === 'bmr' && !v.dropped && !v.jettisoned;
        const escort = (v: AirUnit): boolean => !away(v) && v.air === 'ftr';
        const atEscort = u.def === 'escort' && u.kit.ftr > 0;
        const cur = u.job.do === 'intercept' ? this.unit(u.job.on) : undefined;
        // On its staff's climb, a group keeps no more than a band above its quarry, or it could never reach it: a box
        // that goes down a band takes the climbers down with it. Once it is back at the height every staff flies at
        // (band 1), it is no longer above the raid on its staff's climb, and is priced as any other group from there
        // (here if it is there already, otherwise in fly() when it gets there): a dive on the way down is still the
        // climb's own.
        const keep = (q: AirUnit | null | undefined): void => {
          if (!u.rise || !q || u.to <= q.band + 1) return;
          u.to = (q.band + 1) as Band;
          if (u.to <= 1 && u.band <= 1) u.rise = false;
        };
        if (!first && cur && this.valid(u, u.job) && (atEscort ? escort(cur) : loaded(cur))) { keep(cur); continue; }
        const want = u.kit.bmr > 0 || u.kit0 <= 0;
        const e = (atEscort ? this.nearest(u, escort) : null) ?? (want ? this.nearest(u, loaded) : null) ?? (cur && this.valid(u, u.job) && !away(cur) ? cur : null)
          ?? this.nearest(u, (v) => !away(v) && (v.air === 'ftr' ? u.kit.ftr > 0 || u.kit0 <= 0 : want));
        if (e) u.job = { do: 'intercept', on: e.id };
        else u.job = { do: 'patrol', x: SKY.target.x, y: SKY.target.y };
        // The staff meets the raid at its own height, or, warned in time, climbs above it first (climb). Height is
        // otherwise the player's to play: a group sent above the enemy dives on it (book.ts edge.bounce); the raid
        // card's odds already weigh radar and a late scramble.
        if (first) {
          const up = this.climb(u);
          if (up !== null) {
            u.to = up; u.rise = true;
            this.say('air:climb', u, told(this.called(u), this.place.starts[u.corps]?.over ? 'Warned by the lookouts, {n} [climbs|climb] to get above the strike.' : 'Warned by radar, {n} [climbs|climb] to get above the raid.'), { to: up });
          } else u.to = Math.max(u.to, 1) as Band;
        }
        keep(e);
      }
    }
  }

  /**
   * The staff's climb (the tactics panel's 5d): warned by radar of a day raid (or, over its own fleet, by the fleet's
   * lookouts: the tactics panel's S1), a computer defence's group climbs its
   * style's bands above the raid's highest box (its highest group, in a fight for the sky) before it meets it, if the
   * raid is far enough off for the climb to be done first; if not, it meets the raid at its height, as every staff did
   * before. Weighed once for each group, so a group that has dived is never sent back up. The climb is drawn, never
   * priced (b418): the card already counts every warned fighter up in time, so place_ takes its height as level and its
   * dive knocks no nerve. Null: no climb.
   */
  private climb(u: AirUnit): Band | null {
    const D = this.place.defence;
    // Warned: by radar, or, over its own fleet, by the fleet's lookouts (the tactics panel's S1); by day only.
    if (!D || D.climb <= 0 || this.place.night || !(this.place.radar || this.place.starts[u.corps]?.over) || this.risen.has(u.id)) return null;
    this.risen.add(u.id);
    const raid = this.units.filter((v) => v.side === 'att' && flying(v));
    const boxes = raid.filter((v) => v.air === 'bmr');
    if (!raid.length) return null;
    const to = Math.min(2, Math.max(...(boxes.length ? boxes : raid).map((v) => v.band)) + D.climb) as Band;
    if (to <= Math.max(1, u.band)) return null;
    // In time: closing at the raid's speed and its own climbing speed, the raid is still further off than the climb takes.
    const need = (to - u.band) * SKY.bands.climb - u.climbT;
    const near = Math.min(...raid.map((v) => dist(u.x, u.y, v.x, v.y))) - SKY.contact.day;
    const close = (boxes.length ? SKY.speed.bmr : SKY.speed.ftr) + SKY.speed.ftr * SKY.speed.climbing;
    return near / close >= need ? to : null;
  }

  /**
   * A computer defence's roles (the tactics panel's 5c): against an escorted raid, with two groups or more and a style
   * that sends some at the escort, whole groups whose weight comes nearest the style's share go for the escort, the
   * rest for the bombers, each role with one group at least; on a tie the earlier groups in the ticket's order take
   * the escort. Otherwise no group has a role and every one goes for the bombers, as every staff did before (mass, a
   * single group, an unescorted raid). A human defender has no roles.
   */
  private deal(): void {
    const D = this.place.defence;
    if (!D) return;
    const def = this.units.filter((u) => u.side === 'def' && u.air === 'ftr');
    const raid = this.units.filter((u) => u.side === 'att');
    const escorted = raid.some((u) => u.air === 'ftr') && raid.some((u) => u.air === 'bmr');
    const n = def.length, w = def.map((u) => this.weight(u, u.n0)), all = w.reduce((t, x) => t + x, 0);
    if (D.escort <= 0 || n < 2 || !escorted || all <= 0) return;
    const take = def.map(() => false);
    if (n <= 16) {
      // Every split with a group in each role. Bit k is the ticket's k-th group; on a tie, the split whose earliest
      // differing group goes for the escort.
      let best = 0, gap = Infinity;
      for (let m = 1; m < (1 << n) - 1; m++) {
        let sh = 0;
        for (let k = 0; k < n; k++) if (m & (1 << k)) sh += w[k];
        const d = Math.abs(sh / all - D.escort), x = m ^ best;
        if (d < gap - 1e-12 || (d < gap + 1e-12 && (m & x & -x) !== 0)) { gap = d; best = m; }
      }
      for (let k = 0; k < n; k++) take[k] = (best & (1 << k)) !== 0;
    } else {
      // A great many groups: in the ticket's order, the first goes for the escort, and each after it while that brings
      // the share nearer; the last always goes for the bombers.
      let sh = 0;
      for (let k = 0; k < n - 1; k++) if (k === 0 || Math.abs((sh + w[k]) / all - D.escort) < Math.abs(sh / all - D.escort)) { sh += w[k]; take[k] = true; }
    }
    def.forEach((u, k) => { u.def = take[k] ? 'escort' : 'bombers'; });
    for (const u of def) if (u.def === 'escort') this.say('air:split', u, told(this.called(u), '{N} [is|are] sent against the escort.'), { def: 'escort' });
  }

  /** Whether a side can see that group: radar shows the raid to the defender; otherwise only what is within sight. */
  sees(side: Side, v: AirUnit): boolean {
    if (v.side === side) return true;
    if (side === 'def' && this.place.radar && !this.place.night) return true;
    const r = this.place.night ? SKY.sight.night : SKY.sight.day;
    if (side === 'def' && dist(v.x, v.y, SKY.target.x, SKY.target.y) <= r) return true;
    return this.units.some((u) => u.side === side && flying(u) && dist(u.x, u.y, v.x, v.y) <= r);
  }

  // ---------------------------------------------------------------------------------------------------------------

  step(): void {
    if (this.over) return;
    const dt = DT * SKY.scale;
    this.t = Math.round((this.t + DT) * 1000) / 1000;
    for (const side of ['att', 'def'] as Side[]) if (this.ai[side] && this.t >= this.thinkAt) this.think(side);
    if (this.t >= this.thinkAt) this.thinkAt = this.t + SKY.think;
    this.scramble();
    if (this.troops) this.strafeDue();
    for (const u of this.units) if (!off(u)) this.fly(u, dt);
    this.touch();
    this.wear(dt);
    for (const u of this.units) if (!off(u)) this.settle(u, dt);
    this.warn();
    this.end(dt);
  }

  /**
   * How near a side is to breaking off, from 0 (all fresh) to 1 (broken off), and why (ruling 226, mark 3: no hidden
   * thresholds). A group is as near as the further of two marks: its morale on the way from where it started down to
   * routing ('nerve'), and, for a fighter group, its fight spent on the way down to the share at which it breaks off
   * for home ('ammo'). A group lost, routing, turned back with its bombs or fought out has broken off. Each kind is
   * weighted by the aircraft it sent up. A raid counts what turns a raid back, its losses and its escort's state
   * (the design director, b397): the further of its escort's mark and its bombers', so fresh bombers do not hold it
   * down, and either side can fill first.
   */
  nearWhy(side: Side): { at: number; why: 'ammo' | 'nerve' } {
    const sum = (us: AirUnit[]): { at: number; ammo: number; nerve: number } => {
      let all = 0, ammo = 0, nerve = 0;
      for (const u of us) {
        all += u.n0;
        if (u.state === 'gone' || u.state === 'routing' || u.jettisoned) { nerve += u.n0; continue; }
        if (u.air === 'ftr' && u.kit0 > 0 && u.vig < SKY.spent) { ammo += u.n0; continue; }
        const m0 = this.mor0.get(u.id) ?? u.mor;
        const byMor = m0 > SKY.morale.routing ? clamp01((m0 - u.mor) / (m0 - SKY.morale.routing)) : 0;
        const byFight = u.air === 'ftr' && u.kit0 > 0 ? clamp01((1 - u.vig) / (1 - SKY.spent)) : 0;
        if (byFight > byMor) ammo += u.n0 * byFight; else nerve += u.n0 * byMor;
      }
      return all > 0 ? { at: (ammo + nerve) / all, ammo, nerve } : { at: 0, ammo: 0, nerve: 0 };
    };
    const mine = this.units.filter((u) => u.side === side);
    const bmr = mine.filter((u) => u.air === 'bmr'), ftr = mine.filter((u) => u.air === 'ftr');
    const parts = bmr.length && ftr.length ? [sum(ftr), sum(bmr)] : [sum(mine)];
    const top = parts.reduce((a, b) => (b.at > a.at ? b : a));
    return { at: top.at, why: top.ammo > top.nerve ? 'ammo' : 'nerve' };
  }

  nearBreak(side: Side): number {
    return this.nearWhy(side).at;
  }

  /**
   * A side three quarters of the way to breaking off: one line, once (ruling 226, mark 3, in the UI director's words).
   * The player's own reads "Your raid is near breaking off"; the enemy's "The French fighters are near breaking off".
   */
  private warn(): void {
    for (const side of ['att', 'def'] as Side[]) {
      if (this.warned.has(side) || this.nearBreak(side) < SKY.warn) continue;
      this.warned.add(side);
      const raid = side === 'att' && this.units.some((u) => u.side === 'att' && u.air === 'bmr');
      const mine = this.ticket[side].human;
      const who = mine ? `Your ${raid ? 'raid is' : 'fighters are'}` : `The ${this.ticket[side].adj} ${raid ? 'raid is' : 'fighters are'}`;
      // The enemy's line names the cause (the UI director, 20:53Z): short of ammunition, or shaken.
      const why = mine ? '' : this.nearWhy(side).why === 'ammo' ? ': short of ammunition' : ': shaken';
      this.say('air:near', null, `${who} near breaking off${why}.`, { side });
    }
  }

  /** Defenders with no radar warning wait on the ground until the raid comes near, then climb to meet it. */
  private scramble(): void {
    if (this.scrambled) return;
    const near = this.units.some((u) => u.side === 'att' && flying(u) && dist(u.x, u.y, SKY.target.x, SKY.target.y) <= SKY.scramble);
    if (!near) return;
    this.scrambled = true;
    for (const u of this.units) {
      if (u.job.do !== 'wait') continue;
      u.job = { do: 'patrol', x: SKY.target.x, y: SKY.target.y };
      u.band = 0; u.to = 1; u.alt = SKY.bands.alt[0];
      this.say('air:scramble', u, told(this.called(u), '{N} [is|are] scrambled late and [climbs|climb] to meet the raid.'), undefined, C.scramble);
    }
    this.think('def', true);
  }

  /** Where the job sends a group this tick; 'after' keeps the enemy group it is making for, if any (shown only). */
  private aim(u: AirUnit): [number, number] {
    const [x, y, e] = this.aimAt(u);
    if (e) this.after.set(u.id, e.id); else this.after.delete(u.id);
    return [x, y];
  }

  private aimAt(u: AirUnit): [number, number, (AirUnit | null)?] {
    const T = SKY.target;
    const j = u.job;
    // In a fight, a group free to pick its enemy turns on the one it is fighting; an interceptor keeps after its quarry
    // and a close escort keeps its place by its bombers, fighting whatever comes at them.
    // (A group strafing the troops that is jumped breaks off its pass and turns on its attackers.)
    const free = j.do === 'hunt' || j.do === 'patrol' || j.do === 'strafe' || (j.do === 'escort' && !j.close);
    if (u.air === 'ftr' && u.contacts.length && free) { const e = this.unit(u.contacts[0])!; return [e.x, e.y, e]; }
    switch (j.do) {
      case 'strafe': {
        const s = this.strafing.get(u.id);
        return s ? (s.run ? s.to : s.from) : [u.x, u.y];
      }
      case 'route': return u.dropped || u.jettisoned ? [u.x, -5_000] : [T.x + u.lane, T.y];
      case 'home': return [u.homeX, u.homeY];
      case 'wait': return [u.x, u.y];
      case 'patrol': {
        const e = this.nearest(u, (v) => dist(v.x, v.y, j.x, j.y) <= SKY.escort.reach);
        return e ? [e.x, e.y, e] : [j.x, j.y];
      }
      case 'intercept': { const e = this.unit(j.on); return e && this.valid(u, j) ? [e.x, e.y, e] : [u.x, u.y]; }
      case 'hunt': {
        const e = this.nearest(u, (v) => v.air === 'ftr') ?? this.nearest(u, () => true);
        return e ? [e.x, e.y, e] : [T.x, T.y];
      }
      case 'escort': {
        const b = this.unit(j.on);
        if (!b || off(b)) return [u.x, u.y];
        if (j.close) {
          // A close escort turns on any enemy fighter that comes at its bombers, and otherwise keeps its place by them.
          const e = this.nearest(u, (v) => v.air === 'ftr' && v.job.do !== 'home' && v.state !== 'routing' && dist(v.x, v.y, b.x, b.y) <= SKY.escort.near);
          this.chasing.set(u.id, e ? e.id : null);
          if (e) return [e.x, e.y, e];
          const k = this.units.filter((f) => f.job.do === 'escort' && f.job.on === b.id).indexOf(u);
          const side = k % 2 ? 1 : -1, row = Math.floor(k / 2);
          return [b.x + side * SKY.escort.close * (1 + row * 0.5), b.y - row * 600];
        }
        if (j.top) {
          // Top cover dives on any enemy fighter that comes at its bombers (a band above, it bounces them), then climbs
          // back to its station above and behind them.
          const e = this.nearest(u, (v) => v.air === 'ftr' && v.job.do !== 'home' && v.state !== 'routing' && dist(v.x, v.y, b.x, b.y) <= SKY.escort.near);
          this.chasing.set(u.id, e ? e.id : null);
          if (e) return [e.x, e.y, e];
          if (!u.contacts.length) u.to = Math.min(2, b.band + 1) as Band;
          return [b.x - Math.sin(b.face) * SKY.escort.behind, b.y - Math.cos(b.face) * SKY.escort.behind];
        }
        // A loose escort sweeps ahead of its bombers and goes for enemy fighters that come near them.
        const e = this.nearest(u, (v) => v.air === 'ftr' && dist(v.x, v.y, b.x, b.y) <= SKY.escort.reach);
        if (e) return [e.x, e.y, e];
        const hd = bearing(b.x, b.y, T.x, T.y);
        return [b.x + Math.sin(hd) * SKY.escort.ahead, b.y + Math.cos(hd) * SKY.escort.ahead];
      }
    }
  }

  /** A close escort or top cover breaking off from its bombers to engage attackers, and coming back to them (shown, the design's 1.8). */
  private engaging(u: AirUnit, was: number | null): void {
    const j = u.job;
    if (j.do !== 'escort' || (!j.close && !j.top)) { this.chasing.delete(u.id); return; }
    const now = this.chasing.get(u.id) ?? null;
    // Told the first time each escort group breaks away in a battle; the scene shows the rest, and its return (the UI
    // director, 21:19Z).
    if (now !== null && was === null) {
      const box = this.unit(j.on)!, first = !this.broke.has(u.id);
      this.broke.add(u.id);
      this.say('air:engage', u, first ? `${told(this.called(u), '{N} [has|have] broken away from')} ${subjectOf(box.name).whom} to take on the attackers.` : '', { on: now, box: j.on });
    } else if (now === null && was !== null) this.say('air:rejoin', u, '', { box: j.on });
  }

  private fly(u: AirUnit, dt: number): void {
    if (u.job.do === 'wait') return;
    // Height: climbing takes time, diving little.
    if (u.band !== u.to) {
      u.climbT += dt;
      const up = u.to > u.band, need = up ? SKY.bands.climb : SKY.bands.dive;
      const a0 = SKY.bands.alt[u.band], a1 = SKY.bands.alt[(u.band + (up ? 1 : -1)) as Band];
      u.alt = a0 + (a1 - a0) * Math.min(1, u.climbT / need);
      if (u.climbT >= need) {
        u.band = (u.band + (up ? 1 : -1)) as Band; u.climbT = 0; u.alt = SKY.bands.alt[u.band];
        // Brought back down to every staff's height without diving (keep): its staff's climb is over.
        if (u.rise && u.band <= 1 && u.band === u.to && u.bounce <= 0) u.rise = false;
      }
    } else u.alt = SKY.bands.alt[u.band];
    const was = this.chasing.get(u.id) ?? null;
    const [tx, ty] = this.aim(u);
    if (this.show) this.engaging(u, was);
    const d = dist(u.x, u.y, tx, ty);
    const climbing = u.to > u.band;
    const v = (u.air === 'bmr' ? SKY.speed.bmr : SKY.speed.ftr) * (climbing ? SKY.speed.climbing : 1) * dt;
    u.speed = 0;
    if (d > 1) {
      u.face = bearing(u.x, u.y, tx, ty);
      const k = Math.min(1, v / d);
      u.x += (tx - u.x) * k; u.y += (ty - u.y) * k;
      u.speed = (d * k) / DT;
    }
    // Fuel: only the raid's fighters are far from home.
    if (u.air === 'ftr' && u.fuel0 < SKY.fuel.home) {
      u.fuel -= dt * (u.contacts.length ? SKY.fuel.fight : 1);
      if (u.fuel <= 0 && u.job.do !== 'home') {
        u.fuel = 0; u.job = { do: 'home' };
        this.say('air:bingo', u, told(this.called(u), '{N} [is|are] short of fuel and [turns|turn] for home.'), undefined, C.bingo);
      }
    }
    // The bombing run. A box whose lane lies outside the target ring (a stream of ten boxes or more) holds its run
    // where its lane ends, beside the ring, as the stream's edge box does at the ring's rim; its bombs count as any
    // box's (the tactics panel's air brief, item 7; the design director, 20 November 03:19Z).
    if (u.air === 'bmr' && u.side === 'att' && u.job.do === 'route' && !u.dropped && !u.jettisoned) {
      const T = SKY.target;
      if (dist(u.x, u.y, T.x, T.y) <= T.r || (Math.abs(u.lane) > T.r && dist(u.x, u.y, T.x + u.lane, T.y) < 1)) {
        u.run += dt;
        if (u.run >= SKY.run) {
          u.dropped = u.n;
          // Torpedo bombers attack; dive and level bombers bomb (the UI director, 19 November 14:26Z, ruling 6).
          this.say('air:bombs', u, `${told(this.called(u), u.strike === 'torpedo' ? '{N} [attacks|attack]' : '{N} [bombs|bomb]')} ${this.quarry() ?? this.place.name}: ${u.n} bombers over the target.`, { n: u.n }, C.bombs);
          this.bombsFall();
          if (this.sea) for (const x of this.sea.drop(this.droppedNow(), u.strike ?? 'bomb', this.t, u.corps)) this.say(x.what, u, x.text, x.extra);
          // A raid on troops: what the bombs are seen to hit, as far as the men the law now books lost allow.
          if (this.troops) this.marksHit(u, this.troops.land(this.droppedNow(), this.strafed, 'bomb', u.x, u.y));
        }
      }
    }
    if (u.job.do === 'strafe') this.pass(u);
  }

  // ---------------------------------------------------------------------------------------------------------------
  // A raid on troops (release 2): the raid's fighters go down on the troops.

  /** Whether a group of the raid may go down on the troops: a fighter of a raid on troops that has not done so already, and was given no other order. */
  private mayStrafe(u: AirUnit): boolean {
    return !!this.troops && u.side === 'att' && u.air === 'ftr' && !this.keepUp.has(u.id) && !this.strafing.get(u.id)?.over
      // (The strafers' rule: only a group with the reach to strafe goes down on the troops.)
      && (!this.troops.law.reach || this.troops.law.reach.includes(u.corps));
  }

  /**
   * By default, as the staff's own raid counts every fighter that came through: each of the raid's fighter groups goes
   * down on the troops once the bombers it escorts have bombed, turned back or gone (at once in a raid with no bombers),
   * as soon as it is out of the fight. The staff's raid counts every fighter both in the fight with the defence and on
   * the troops, so an escort that has fought itself out against the fighters still goes down on the troops (its fight
   * with the defence is over, not its guns). One short of fuel, broken, or given another order does not, and so counts
   * for less: a choice the campaign books.
   */
  private strafeDue(): void {
    for (const u of this.units) {
      if (!this.mayStrafe(u) || off(u)) continue;
      if (u.job.do === 'escort') this.escorted.set(u.id, u.job.on);
      if (!this.willStrafe(u) || u.job.do === 'strafe' || u.contacts.length) continue;
      const box = this.escorted.get(u.id), b = box === undefined ? undefined : this.unit(box);
      if (b && !off(b) && !away(b) && !b.dropped && !b.jettisoned) continue;
      if (!this.fuelFor(u)) continue;
      this.goDown(u, box ?? null);
    }
  }

  /**
   * Whether a group has the fuel left over the board to reach the troops and fly its passes: one that has not is short
   * of fuel, and goes home as it is. (The board's fuel is time over it, as for every group: at none a group turns for
   * home, bingo, and the way home is the campaign's reach, already weighed.)
   */
  private fuelFor(u: AirUnit): boolean {
    if (u.fuel0 >= SKY.fuel.home) return true;
    const L = this.lane, S = TROOPS.strafe;
    const there = dist(u.x, u.y, L.x, L.y), passes = (S.passes - (this.strafing.get(u.id)?.done ?? 0)) * 2 * S.half;
    return u.fuel >= (there + passes) / SKY.speed.ftr;
  }

  /**
   * Whether a group of the raid is still to go down on the troops, or is on its passes: an escort, or one fought out
   * and on its way home with fuel left (strafeDue); never one broken, out of fuel, on the ground, or held up.
   */
  private willStrafe(u: AirUnit): boolean {
    if (!this.mayStrafe(u) || off(u) || u.state === 'routing' || u.job.do === 'wait') return false;
    if (u.job.do === 'strafe' || u.job.do === 'escort') return true;
    return u.job.do === 'home' && u.vig < SKY.spent && u.fuel > 0;
  }

  /** Down on the troops: the passes start from the end of the lane nearer the group; each group rakes a strip of its own. */
  private goDown(u: AirUnit, box: number | null): void {
    const S = TROOPS.strafe, L = this.lane;
    const was = this.strafing.get(u.id);
    u.job = { do: 'strafe' };
    if (was) return;
    const raid = this.units.filter((f) => f.side === 'att' && f.air === 'ftr');
    const side = (raid.indexOf(u) - (raid.length - 1) / 2) * S.width * 3;
    const a: [number, number] = [L.x - L.dx * S.half + L.dy * side, L.y - L.dy * S.half - L.dx * side];
    const b: [number, number] = [L.x + L.dx * S.half + L.dy * side, L.y + L.dy * S.half - L.dx * side];
    const near = dist(u.x, u.y, a[0], a[1]) <= dist(u.x, u.y, b[0], b[1]);
    this.strafing.set(u.id, { done: 0, run: false, p: 0, from: near ? a : b, to: near ? b : a, box, over: false });
  }

  /**
   * A strafing group's tick, after it has moved: down to the low band as it nears the troops; on the lane's start, its
   * pass begins ('air:strafe'); down the lane it owes the ground fire its share of the pass it has flown; at the lane's
   * end the pass counts its part of the group as strafed and its hits follow ('air:mark-hit'). Jumped, it breaks off:
   * the pass is lost, and what the guns cost it stands.
   */
  private pass(u: AirUnit): void {
    const s = this.strafing.get(u.id)!, S = TROOPS.strafe;
    // Jumped: the pass is broken off. A group with fight left turns on its attackers (aimAt); one fought out runs for home.
    if (u.contacts.length) { s.run = false; s.p = 0; if (u.vig < SKY.spent) u.job = { do: 'home' }; return; }
    if (!s.run) {
      if (dist(u.x, u.y, s.from[0], s.from[1]) <= SKY.flakR) u.to = 0;
      if (u.band !== 0 || u.to !== 0 || dist(u.x, u.y, s.from[0], s.from[1]) > 1) return;
      s.run = true; s.p = 0;
      const len = dist(s.from[0], s.from[1], s.to[0], s.to[1]);
      const words = s.done === 0 ? told(this.called(u), `{N} [goes|go] down to strafe the troops at ${this.place.name}.`) : '';
      this.say('air:strafe', u, words, { from: [Math.round(s.from[0]), Math.round(s.from[1])], to: [Math.round(s.to[0]), Math.round(s.to[1])], width: S.width, dur: Math.round((len / SKY.speed.ftr / SKY.scale) * 10) / 10 });
      return;
    }
    const len = Math.max(1, dist(s.from[0], s.from[1], s.to[0], s.to[1]));
    const left = dist(u.x, u.y, s.to[0], s.to[1]), p = clamp01(1 - left / len);
    const part = (u.n * reachOf(u)) / S.passes;
    if (p > s.p) {
      const owe = this.fireEach * part * (p - s.p);
      this.fireDue.set(u.id, (this.fireDue.get(u.id) ?? 0) + owe); s.p = p;
      if (this.troops!.law.reach) { this.firedOn.add(u.corps); this.fireOwed += owe; }
    }
    if (left > 1) return;
    // The pass made: its part of the group counts on the troops, by the campaign's own law, and its hits follow.
    s.done++; s.run = false; s.p = 0;
    this.strafed += part;
    const mid = [(s.from[0] + s.to[0]) / 2, (s.from[1] + s.to[1]) / 2];
    this.marksHit(u, this.troops!.land(this.droppedNow(), this.strafed, 'strafe', mid[0], mid[1]));
    if (s.done >= S.passes) {
      s.over = true;
      u.job = { do: 'home' };
      this.say('air:strafed', u, told(this.called(u), '{N} [has|have] made [its|their] passes and [turns|turn] for home.'), { passes: s.done });
      return;
    }
    // The next pass back up the lane, a strip over.
    const ox = this.lane.dy * S.width, oy = -this.lane.dx * S.width;
    const k = s.done % 2 ? 1 : -1;
    [s.from, s.to] = [[s.to[0] + ox * k, s.to[1] + oy * k], [s.from[0] + ox * k, s.from[1] + oy * k]];
  }

  /**
   * What a bomb pattern or a strafing pass is seen to hit (troops.ts land): one 'air:mark-hit' each, at the piece. The
   * biggest hit of the release is told in a line; the rest the scene shows. Shown, never counted.
   */
  private marksHit(u: AirUnit, news: MarkNews[]): void {
    const own = this.ticket.def.human;
    const top = news.reduce<MarkNews | null>((m, x) => (x.big && x.men > 0 && (!m || x.men > m.men) ? x : m), null);
    for (const x of news) {
      const what = this.place.marks!.pieces.find((p) => p.id === x.piece)?.what ?? 'men';
      this.say('air:mark-hit', u, x === top ? markWords(x, what, own) : '', { piece: x.piece, strike: x.strike, how: x.how, men: x.men, big: x.big, x: x.x, y: x.y });
    }
  }

  /** Who is in contact with whom: hostile groups close together and no more than one height apart. A dive from above is a bounce. */
  private touch(): void {
    const r = this.place.night ? SKY.contact.night : SKY.contact.day;
    const live = this.units.filter(flying);
    const before = new Map(live.map((u) => [u.id, new Set(u.contacts)]));
    for (const u of live) u.contacts = [];
    for (const a of live) {
      if (a.side !== 'att') continue;
      for (const d of live) {
        if (d.side !== 'def' || d.air !== 'ftr') continue;
        // A group that has broken off or turned for home runs and does not fight back; no new fight starts with it,
        // but an enemy already on it can stay on it as it goes.
        const aw = away(a), dw = away(d);
        if (aw && dw) continue;
        if (aw || dw) {
          const [run, chase] = aw ? [a, d] : [d, a];
          // (Bombers do not give chase: their gunners fire only on what comes at them.)
          if (!before.get(a.id)?.has(d.id) || chase.air === 'bmr' || chase.kit[run.air] <= SKY.spent * chase.kit0 * 0.25) continue;
        }
        if (Math.abs(a.band - d.band) > 1 || dist(a.x, a.y, d.x, d.y) > r) continue;
        a.contacts.push(d.id); d.contacts.push(a.id);
        this.fired = true;
        if (before.get(a.id)?.has(d.id)) continue;
        // A new run at a bomber group: the fighters fly the pass their doctrine gives against that bomber (doctrine.ts).
        if (a.air === 'bmr') this.passes.set(`${d.id}:${a.id}`, pickPass(this.passRng, passesOn(d.nat, this.when, a.model, a.nat)));
        // A new contact: the higher fighter group dives on the lower one and strikes first. A bomber box above a fighter
        // is simply in contact: bombers do not dive on fighters (the tactics panel's S0).
        const hi = a.band > d.band && a.air === 'ftr' ? a : d.band > a.band ? d : null;
        if (hi) {
          const lo = hi === a ? d : a;
          // The dive that ends a staff's climb is drawn, never priced (b418): it knocks no nerve. Only that one dive, from
          // the height the climb gave (band 2). A dive from the height every staff flies at (band 1: after the climb's
          // dive, or caught short of the climb) is the group's own, priced as any other group's.
          if (hi.rise && hi.band <= 1) hi.rise = false;
          // A dive on fighters down on their passes on the troops is drawn, never priced (as b418's climb): the staff's
          // raid sends them down, and the card prices their passes in the ground fire alone.
          const low = lo.job.do === 'strafe';
          if (!low) hi.bounce = SKY.edge.bounceFor;
          hi.band = lo.band; hi.to = lo.band; hi.alt = SKY.bands.alt[lo.band];
          if (!hi.rise && !low) lo.mor = clamp01(lo.mor - SKY.morale.bounced);
          this.say('air:bounce', hi, `${told(this.called(hi), '{N} [dives|dive] on')} ${subjectOf(this.called(lo)).whom} from above.`, { on: lo.id });
        }
        for (const x of [a, d]) if (!(before.get(x.id)?.size)) {
          x.luck = this.rng.range(1 - SKY.wear.luck, 1 + SKY.wear.luck);
          // A bomber group's contact names the fighters on it, so the 3D game can play their model's own pass (SKY.cues.pass).
          const other = x === a ? d : a;
          const pass = this.show ? this.passes.get(x.air === 'bmr' ? `${other.id}:${x.id}` : `${x.id}:${other.id}`) : undefined;
          const how = this.show ? { form: x.form, style: x.style, ...(pass ? { pass } : {}) } : {};
          const extra = x.air === 'bmr' ? { with: other.id, ...how } : this.show ? { with: other.id, ...how } : undefined;
          this.say('contact', x, told(this.called(x), '{N} [is|are] in a fight.'), extra, x.air === 'bmr' ? C.onBombers : C.dogfight);
          if (this.metAt === null) this.metAt = this.t;
        }
      }
    }
  }

  /**
   * Where a group stands in the fight: diving from above, or caught climbing. Counts for its blows and against the
   * enemy's. A group on its staff's climb stands level both ways (climb: drawn, never priced).
   */
  private place_(u: AirUnit): number {
    if (u.rise) return 1;
    return (u.bounce > 0 ? SKY.edge.bounce : 1) * (u.to > u.band ? SKY.edge.climbing : 1);
  }

  /** How hard a group hits: where it stands, its nerve, and its luck. */
  private edge(u: AirUnit): number {
    return this.place_(u) * (u.state === 'wavering' ? SKY.edge.wavering : 1) * u.luck;
  }

  /**
   * What each group can do to the enemy over the whole battle, from the raid card's own forecast: the defending
   * fighters share out the escort and bomber losses the card expects, the escorts the defending fighters it expects
   * to fall to them, each by its weight in the fight (aircraft x worth; an escort by how long its fuel lets it stay),
   * and the bombers' gunners the defending fighters the card expects them to bring down (ruling 227's foeGuns), each
   * box by its size, as the card spreads the bombers' losses. A group spends this while in contact and the enemies it
   * touches pay it, so the battle's sums are the card's. A fighter group breaks off once all but SKY.spent of it is
   * gone, so it carries that much more than its share; a box's gunners fire for as long as its attackers stay, and
   * they stay until they are spent, so the gunners carry the same.
   */
  private arm(): void {
    const P = this.place, F = P.forecast;
    const att = this.units.filter((u) => u.side === 'att' && u.air === 'ftr'), def = this.units.filter((u) => u.side === 'def' && u.air === 'ftr');
    const boxes = this.units.filter((u) => u.side === 'att' && u.air === 'bmr');
    const A = att.reduce((s, u) => s + this.weight(u, u.n0), 0), D = def.reduce((s, u) => s + this.weight(u, u.n0), 0);
    const B = boxes.reduce((s, u) => s + u.n0, 0);
    const full = 1 / (1 - SKY.spent);
    // The guns take their own share of the bombers; the fighters the rest.
    const onBombers = Math.max(0, F.bmr - B * P.gunLoss);
    // The defenders' losses: the bombers' gunners' part, and the escort's the rest (ruling 227).
    const guns = B > 0 ? Math.min(F.foe, F.foeGuns ?? 0) : 0, toEscort = F.foe - guns;
    for (const u of att) { const k = A > 0 ? this.weight(u, u.n0) / A : 0; u.kit = { ftr: toEscort * k * full, bmr: 0 }; }
    for (const u of boxes) u.kit = { ftr: guns * (u.n0 / B) * full, bmr: 0 };
    // (A raid on troops: of the raid's fighters lost, the card's 'fire' falls to the ground fire on their passes, dealt as
    // they fly them (wear); the defending fighters carry the rest.)
    const onFighters = Math.max(0, F.ftr - (F.fire ?? 0));
    for (const u of def) { const k = D > 0 ? this.weight(u, u.n0) / D : 0; u.kit = { ftr: onFighters * k * full, bmr: onBombers * k * full }; }
    // A split defence (deal) fights from one pool: each group holds its weight's share of what the defence has left.
    this.pool = def.some((u) => u.def === 'escort') && D > 0 ? new Map(def.map((u) => [u.id, this.weight(u, u.n0)])) : null;
    for (const u of this.units) u.kit0 = u.kit.ftr + u.kit.bmr;
    // The card's weights (assessAir): the escort counted L.escort times over, the bombers' own defence of the month.
    const escort = P.law.escort ?? 1, own = P.law.guns ?? P.bomberDefence;
    const met = D / Math.max(1e-9, D + A * escort + B * own);
    this.law = { D, B, ePer: B > 0 ? A / B : 0, escort, guns: own, met };
  }

  /** A fighter group's weight in the fight: aircraft x worth, and for the raid's escorts, how long its fuel lets it stay. */
  private weight(u: AirUnit, n = u.n): number {
    return n * u.q * (u.side === 'att' ? Math.min(1, u.fuel0 / SKY.fuel.full) : 1);
  }

  /**
   * How much harder than the raid card expects a bomber box is hit, for the escort it has with it now: the campaign's
   * own law (bombers lost go as the defenders' share of the fighting weight, to the power L.power, the escort counted
   * L.escort times over), with this box's escort per bomber in place of the raid's. A box with its fair share of the
   * escort is hit as the card says; one left alone, harder; one with more, less. The groups by it are weighed at the
   * strength they started with, as the card weighs them: what the fight itself costs either side is already in the
   * kit, so a box that has lost bombers is not counted better escorted for it (ruling 227 parity, 22 Nov).
   */
  private cover(b: AirUnit, live: AirUnit[]): number {
    const L = this.law, P = this.place;
    if (L.met <= 0) return 1;
    const by = (f: AirUnit): boolean => f.side === 'att' && ((f.job.do !== 'home' && Math.abs(f.band - b.band) <= 1 && dist(f.x, f.y, b.x, b.y) <= SKY.escort.near) || this.tied(f, b));
    // The escort per bomber round this box: the fighters near it over the bombers near it (boxes in a stream share theirs),
    // each at its starting strength.
    const E = live.filter((f) => f.air === 'ftr' && by(f)).reduce((s, f) => s + this.weight(f, f.n0), 0);
    const per = E / Math.max(1, live.filter((f) => f.air === 'bmr' && by(f)).reduce((s, f) => s + f.n0, 0));
    const met = L.D / Math.max(1e-9, L.D + L.B * (per * L.escort + L.guns));
    return Math.max(SKY.escort.cover[0], Math.min(SKY.escort.cover[1], Math.pow(met / L.met, P.law.power)));
  }

  /**
   * An escort held off by the defenders its staff sent against it (deal) still counts over its own box: the split is
   * the staff's choice of where its weight falls, and the card has priced the escort's protection (217, b418). So does
   * an escort the defence has fought out, while it flies home, whatever the defence: its weight was spent on the
   * defenders, and the card counts the escort over its bombers for the whole raid (L.escort, 227), so the box it leaves
   * is not priced as one never escorted.
   */
  private tied(f: AirUnit, b: AirUnit): boolean {
    if (f.air !== 'ftr' || f.side !== 'att') return false;
    // An escort gone down on the troops (a raid on troops), on its passes or home from them, counts over the box it was
    // escorting: the staff's raid counts every fighter both over its bombers and on the troops, so the field does not
    // price the passes a second time in bombers.
    const s = this.strafing.get(f.id);
    if (s && s.box === b.id && (f.job.do === 'strafe' || (s.over && f.job.do === 'home'))) return true;
    if (f.job.do === 'escort') return f.job.on === b.id && f.contacts.some((i) => this.unit(i)?.def === 'escort');
    return f.job.do === 'home' && f.vig < SKY.spent && this.foughtFor.get(f.id) === b.id;
  }

  /** A fighter group's fight left, and home once all but SKY.spent of it is gone. */
  private tire(u: AirUnit): void {
    u.vig = u.kit0 > 0 ? (u.kit.ftr + u.kit.bmr) / u.kit0 : 1;
    if (u.vig < SKY.spent && u.job.do !== 'home') {
      u.job = { do: 'home' };
      this.say('air:spent', u, told(this.called(u), '{N} [has|have] fought [itself|themselves] out and [breaks|break] off for home.'));
    }
  }

  /** The wear: what contact and the guns cost each group this tick, owed until a whole aircraft is lost. */
  private wear(dt: number): void {
    const P = this.place;
    const live = this.units.filter(flying);
    const byId = new Map(live.map((u) => [u.id, u]));
    const foes = (u: AirUnit, k: 'ftr' | 'bmr'): AirUnit[] => u.contacts.map((i) => byId.get(i)!).filter((v) => v && v.air === k);
    const sum = (xs: AirUnit[]): number => xs.reduce((s, v) => s + v.n, 0);
    const burn = 1 - Math.exp(-dt / SKY.tire);
    // A split defence's pool (arm): what its groups still in the fight have left, drawn at their whole rate and shared by
    // weight among those in contact. A group that is gone, has broken off or routs takes its own share with it, as any
    // group's fight goes with it; one on the ground still counts, since it would come up.
    const pool = this.pool, kept = pool ? this.units.filter((u) => pool.has(u.id) && !off(u) && !away(u)) : [];
    const W = kept.reduce((s, u) => s + pool!.get(u.id)!, 0);
    const on = { ftr: 0, bmr: 0 };
    if (pool && W > 0) for (const u of live) {
      if (!pool.has(u.id) || !u.contacts.length || away(u)) continue;
      const k = pool.get(u.id)! / W;
      if (foes(u, 'ftr').length) on.ftr += k;
      if (foes(u, 'bmr').length) on.bmr += k;
    }
    const pace = (u: AirUnit, k: 'ftr' | 'bmr'): number => (on[k] > 0 && pool?.has(u.id) ? 1 / on[k] : 1);
    // The escorts in a fight with the defence now, each with its box (tied): one fought out still counts over it.
    for (const f of live) if (f.side === 'att' && f.air === 'ftr' && f.job.do === 'escort' && f.contacts.some((i) => byId.get(i)?.side === 'def')) this.foughtFor.set(f.id, f.job.on);
    const covers = new Map<number, number>();
    // Who owed what this tick: each group's debts by creditor, and each fighter's fire. Kept whether or not the battle
    // is shown, for the way each loss goes (fates.ts) is told in the dispatches either way; none of it moves a count.
    this.owedBy.clear(); for (const u of live) u.fire = [];
    const owe = (v: AirUnit, by: number | 'flak', k: number): void => {
      v.owed += k;
      if (k <= 0) return;
      const m = this.owedBy.get(v.id) ?? new Map<number | 'flak', number>();
      m.set(by, (m.get(by) ?? 0) + k);
      this.owedBy.set(v.id, m);
    };
    for (const u of live) {
      // (A raid on troops: a group fought out against the fighters that has gone down on the troops has no fight left to give.)
      if (u.air !== 'ftr' || !u.contacts.length || u.kit0 <= 0 || away(u) || (u.job.do === 'strafe' && u.vig < SKY.spent)) continue;
      const F = foes(u, 'ftr'), Bs = foes(u, 'bmr');
      // On fighters: the more for coming down from above, the less for climbing or wavering.
      if (F.length && u.kit.ftr > 0) {
        const spend = u.kit.ftr * burn * pace(u, 'ftr');
        u.kit.ftr -= spend;
        const nF = sum(F);
        for (const v of F) {
          const k = spend * (v.n / Math.max(1, nF)) * Math.min(2, this.edge(u) / this.place_(v));
          owe(v, u.id, k);
          u.fire.push({ on: v.id, rate: k / dt });
        }
      }
      // On bombers: the same, and harder on a box with less than its share of the escort by it.
      if (Bs.length && u.kit.bmr > 0) {
        const spend = u.kit.bmr * burn * pace(u, 'bmr');
        u.kit.bmr -= spend;
        const nB = sum(Bs);
        for (const b of Bs) {
          const most = P.law.most * b.n0 - (b.n0 - b.n) - b.owed;
          if (most <= 0) continue;
          const cover = covers.get(b.id) ?? this.cover(b, live);
          covers.set(b.id, cover);
          const k = Math.min(most, spend * (b.n / Math.max(1, nB)) * cover * Math.min(2, this.edge(u)));
          owe(b, u.id, k);
          u.fire.push({ on: b.id, rate: k / dt, ...(this.passes.has(`${u.id}:${b.id}`) ? { pass: this.passes.get(`${u.id}:${b.id}`) } : {}) });
          if (cover > 1.2) b.mor = clamp01(b.mor - SKY.morale.alone * dt);
        }
      }
      if (!pool?.has(u.id)) this.tire(u);
    }
    // A split defence's pool: what its groups spent this tick comes off what those still in the fight hold together,
    // each keeping its weight's share.
    if (pool && W > 0) {
      const left = { ftr: kept.reduce((s, u) => s + u.kit.ftr, 0), bmr: kept.reduce((s, u) => s + u.kit.bmr, 0) };
      for (const u of kept) {
        const k = pool.get(u.id)! / W;
        u.kit = { ftr: left.ftr * k, bmr: left.bmr * k };
        if (flying(u)) this.tire(u);
      }
    }
    // The bombers' gunners (ruling 227): a box spends its share on the enemy fighters in contact with it, as a fighter
    // group spends its own, for as long as they stay on it; homeward or breaking, it still fires on what comes at it.
    for (const u of live) {
      if (u.air !== 'bmr' || !u.contacts.length || u.kit.ftr <= 0) continue;
      const F = foes(u, 'ftr');
      if (!F.length) continue;
      const spend = u.kit.ftr * burn;
      u.kit.ftr -= spend;
      const nF = sum(F);
      for (const v of F) {
        const k = spend * (v.n / Math.max(1, nF)) * Math.min(2, this.edge(u) / this.place_(v));
        owe(v, u.id, k);
        u.fire.push({ on: v.id, rate: k / dt });
      }
    }
    // The passes on each box: the fighters on it go in, stand off to form up again, and go in again.
    this.beat(live, dt);
    // The guns round the target. They do not fire on a box while their own fighters are going in on it (SKY.flak.hold):
    // what they would have brought down in that time is the fighters' instead, so the box is owed the same either way.
    // Between the passes, and once the fighters have gone, they fire.
    const holding = new Map<number, number>();
    if (P.gunLoss > 0) for (const u of live) {
      if (u.side !== 'att' || dist(u.x, u.y, SKY.target.x, SKY.target.y) > SKY.flakR) continue;
      if (u.air === 'bmr') {
        if (u.dropped || u.jettisoned) continue;
        const pass = SKY.flak.hold ? this.beats.get(u.id) : undefined;
        const own = pass?.go ? pass.by : null;
        owe(u, own ?? 'flak', (u.n * P.gunLoss * SKY.flak.band[u.band] * this.flakNorm * dt) / this.flakIn);
        u.shot = Math.min(1, u.shot + dt / this.flakIn);
        if (own !== null) holding.set(u.id, own);
      } else if (SKY.flak.fighters > 0) owe(u, 'flak', (u.n * P.gunLoss * SKY.flak.fighters * SKY.flak.band[u.band] * dt) / this.flakT);
    }
    // A raid on troops: the ground fire on the fighters strafing them, for the share of their passes flown this tick.
    if (this.fireDue.size) {
      for (const u of live) { const k = this.fireDue.get(u.id); if (k) owe(u, 'flak', k); }
      this.fireDue.clear();
    }
    this.holdFire(holding, live);
    for (const b of live) if (b.air === 'bmr' && b.owed > 0) this.straggle(b);
    // Whole aircraft fall.
    this.fall(live);
  }

  /**
   * Whole aircraft fall from what each group was owed this tick, each with how it goes down. A share of an aircraft
   * falls by chance, so a fight's losses scatter round what it owed as a run of raids scatters (ruling 226, mark 4:
   * the obvious order wins about as often as the raid card says, not every time the expected count tips its way).
   */
  private fall(live: AirUnit[]): void {
    const P = this.place;
    const lost: Loss[] = [];
    const hit: [AirUnit, number][] = [];
    for (const u of live) {
      let k = u.owed;
      const owed = k;
      u.owed = 0;
      for (; k > 0 && u.n > 0; k -= 1) {
        if (k < 1 && this.lossRng.next() >= k) break;
        // Which group brought it down, and whether it was a straggler (the box's next loss is its straggler's): never a count.
        const by = this.creditor(u);
        const s = u.air === 'bmr' ? this.nextDown(u) : null;
        // Which bomber by name it is, or which bomber's gunners got this fighter: never a count either.
        const seat = u.air === 'bmr' ? this.seatDown(u, s) : -1;
        const gun = u.air === 'ftr' ? this.gunnerOf(u, by) : null;
        // A fighter booked to a box is its gunners' kill (ruling 227): kept for the campaign's record, named or not.
        if (u.air === 'ftr' && typeof by === 'number' && this.unit(by)?.air === 'bmr') this.gunKills.set(by, (this.gunKills.get(by) ?? 0) + 1);
        u.n--;
        u.mor = clamp01(u.mor - SKY.morale.perLoss / u.n0);
        const over = u.side === 'att' ? 'enemy' : 'own';
        const fall = drawFall(this.fallRng, u.air, over, P.saved[u.side]);
        if (fall.back) u.back++;
        const alt = s ? Math.max(0, this.shownAlt(u) - s.down) : this.shownAlt(u);
        const before = u.figures;
        u.figures = this.modelsAfter(u, s);
        lost.push({ u, fall, by, s, alt, fid: this.fid++, seat, gun, models: before - u.figures });
      }
      if (this.show && owed > 0 && u.n > 0) hit.push([u, owed]);
    }
    if (lost.length) this.fated(lost);
    for (const [u, owed] of hit) this.hitShown(u, owed);
  }

  /**
   * How each of this tick's losses goes (fates.ts), from its situation; then each is told. A fighter lost in the same
   * moment as the bomber box it was firing on may have collided with one of its bombers (head-on most of all), or rammed
   * it on purpose where and when pilots did (its pilot may get out and fly again), and two fighters locked in a fight
   * may have collided, as may two of one group lost in the same moment (two bombers of a tight box by day, two fighters
   * pressing one attack); a bomber of a tight box by day lost moments after another of the same box, close by, may be
   * struck by it or by its wreckage, or by a fighter shot down while it attacked the box. Never a count: the losses, the
   * pilots back and their order are what the rules drew (tests/battle-air-fates.test.ts).
   */
  private fated(lost: Loss[]): void {
    const F = SKY.fate;
    const pair = new Map<Loss, Loss>(), meant = new Set<Loss>();
    for (const a of lost) {
      if (pair.has(a) || a.u.air !== 'ftr') continue;
      for (const b of lost) {
        if (b === a || pair.has(b) || b.u.side === a.u.side || b.fall.back) continue;
        const fire = a.u.fire.find((f) => f.on === b.u.id);
        // On purpose: the bomber it was firing on, by its nation and the month.
        const m = b.u.air === 'bmr' && fire ? this.ramMeant(a.u, b.u, fire.pass) : 0;
        // By accident: never one whose pilot the rules bring back.
        const p = a.fall.back ? 0 : b.u.air === 'bmr' ? (fire ? (fire.pass === 'headon' ? F.ram.headon : F.ram.other) : 0) : a.u.contacts.includes(b.u.id) ? F.ram.tangle : 0;
        if (m + p <= 0) continue;
        const x = this.lookRng.next();
        if (x < m + p) { pair.set(a, b); pair.set(b, a); if (x < m) { meant.add(a); meant.add(b); } break; }
      }
    }
    // Two of one group lost in the same moment: bombers of a tight box by day, one swerving from its hit into the next,
    // or two fighters pressing the same attack (the UI director, 15:38Z). A draw only when there are two.
    lost.forEach((a, i) => {
      if (pair.has(a) || a.s || a.fall.back) return;
      const p = a.u.air === 'bmr' ? (a.u.state === 'steady' && !this.place.night ? F.ram.same.bmr : 0) : a.u.fire.length || a.u.contacts.length ? F.ram.same.ftr : 0;
      if (p <= 0) return;
      const b = lost.find((x, j) => j > i && x.u === a.u && !pair.has(x) && !x.s && !x.fall.back);
      if (b && this.lookRng.next() < p) { pair.set(a, b); pair.set(b, a); }
    });
    this.recent = this.recent.filter((r) => this.t - r.t <= F.near.t);
    const falls = new Map<Loss, Fall>();
    for (const L of lost) {
      const { u, s } = L;
      const other = pair.get(L);
      // A loss of its own tight box moments before, close by, that has not yet struck another, or a fighter that was
      // attacking the box falling through it; never a straggler, which is clear of the box, and never in the night
      // stream (the historian). Seen falling a moment first, so the eye can follow it down onto this one (the art director).
      const near = u.air === 'bmr' && !s && u.state === 'steady' && !this.place.night
        ? this.recent.find((r) => r.box === u.id && !r.used && this.t - r.t >= F.near.min) : undefined;
      const fate = fateOf(this.lookRng, this.sceneOf(L, near), this.fates, other ? { fid: other.fid, cause: 'ram' } : undefined);
      // A loss strikes at most one other (a fighter falling through a box it fired on is listed once for each box).
      if (fate.how === 'struck' && near) for (const r of this.recent) if (r.fid === near.fid) r.used = true;
      if (u.air === 'bmr' && !s) this.recent.push({ box: u.id, fid: L.fid, how: fate.how, t: this.t, used: false });
      // A fighter going down while it attacks a box may fall through it (never one that collided: it is already locked to its bomber).
      if (u.air === 'ftr' && fate.how !== 'collide') for (const f of u.fire) this.recent.push({ box: f.on, fid: L.fid, how: fate.how, t: this.t, used: false, foe: true });
      falls.set(L, {
        ...L.fall, ...fate, kind: kindOf(fate.how), fid: L.fid, ...(meant.has(L) ? { meant: true as const } : {}),
        captured: L.fall.back || L.fall.over !== 'enemy' ? 0 : fate.chutes + (fate.out ?? 0),
      });
    }
    // A collision is told once, by the first of the two to be told, in one line for both (the UI director, 15:02Z): the
    // one who rams first; otherwise the bomber first, as the ruled line has it.
    const told = new Set<Loss>();
    for (const L of lost) {
      const { u, s } = L;
      const fall = falls.get(L)!, other = pair.get(L);
      let text = '';
      if (!other) text = fallWords(u.name, this.adjOf(u), fall);
      else if (!told.has(other)) {
        const o = falls.get(other)!;
        const [x, y] = meant.has(L) ? (u.air === 'ftr' ? [L, other] : [other, L]) : u.air === 'bmr' || other.u.air !== 'bmr' ? [L, other] : [other, L];
        // Two of the same group are named once ('Two bombers of the 8th Bomb Group collide ...'; the UI director, 15:38Z).
        const same = x.u === y.u;
        const a = same ? twoOf(x.u.name, x.u.air === 'bmr' ? 'bomber' : 'fighter', this.adjOf(x.u)) : this.nameOf(x.u);
        text = collideWords(a, same ? '' : this.nameOf(y.u), fall.chutes + o.chutes, (fall.out ?? 0) + (o.out ?? 0), meant.has(L));
      }
      told.add(L);
      const who = L.by !== undefined ? { by: L.by, ...(s ? { strag: s.id, eng: s.eng, ...this.stragAt(u, s) } : {}) } : {};
      const loud = fall.how === 'explode' || fall.how === 'breakup' || fall.how === 'collide';
      // By name: the bomber this is ('bomber'), or the bomber whose gunners brought this fighter down ('gunner').
      const B = L.seat >= 0 ? this.seated.get(u.id) : undefined;
      const named = { ...(B ? { bomber: toldOf(B, L.seat) } : {}), ...(L.gun ? { gunner: { box: L.gun[0], ...toldOf(this.seated.get(L.gun[0])!, L.gun[1]) } } : {}) };
      // 'models': how many models fall in the picture for it (AR1). Every aircraft lost is still told.
      this.say('air:lost', u, text, { ...fall, air: u.air, alt: L.alt, models: L.models, ...(fall.chutes ? { chuteCue: C.chutes } : {}), ...who, ...named }, loud ? C.explode : C.down);
      // One of the group's longest-serving bombers is named as it goes (its seats come old hands first; the campaign's
      // news names the oldest again after the raid, stories.ts bombersFlew). Shown only; the words are the UI director's to rule.
      if (B && L.seat < SKY.named.lines && B.all[L.seat].name && B.all[L.seat].missions >= B.told) {
        const b = B.all[L.seat];
        this.shownSay('air:named', u, `That was ${b.name}, with ${b.missions} ${B.flown}s flown.`, { bomber: toldOf(B, L.seat), fid: L.fid });
      }
    }
  }

  /** The chance a fighter lost in the same moment as the bomber it was firing on rammed it on purpose (fates.ts ramShare). */
  private ramMeant(a: AirUnit, b: AirUnit, pass?: Pass): number {
    return ramShare(a.nat, this.when, enginesOf(b.model), pass);
  }

  /** One loss's situation (fates.ts Scene). */
  private sceneOf(L: Loss, near: { fid: number; how: FallHow; foe?: boolean } | undefined): Scene {
    const { u, s, by } = L;
    const engines = u.air === 'bmr' ? enginesOf(u.model) : fighterEngines(u.model);
    const at = s ? this.stragAt(u, s) : { x: u.x, y: u.y };
    const v = typeof by === 'number' ? this.unit(by) : undefined;
    const band = this.shownBand(u), kinds = SKY.flak.kinds[band];
    return {
      air: u.air, engines, nat: u.nat, model: u.model, when: this.when,
      gun: v && v.air === 'ftr' ? this.heaviest(v) : null,
      flak: by === 'flak' ? (kinds[kinds.length - 1] as Scene['flak']) : null,
      // (Ground fire on a fighter strafing the troops is the column's own light guns and small arms.)
      ...(by === 'flak' && u.job.do === 'strafe' ? { flak: 'light' as const } : {}),
      pass: u.air === 'bmr' && typeof by === 'number' ? this.passes.get(`${by}:${u.id}`) ?? null : null,
      band, alt: L.alt,
      loaded: u.air === 'bmr' && u.dropped <= 0 && !u.jettisoned,
      strag: s ? s.eng : null,
      // A strike on a fleet is fought over open sea; a harbour over its stage's own ground.
      sea: this.place.ships && !this.place.ships.law.harbour ? true : seaAt(this.place.ground, at.x, at.y),
      night: !!this.place.night,
      back: L.fall.back, over: L.fall.over,
      crew: u.air === 'bmr' ? crewOf(u.model, this.when) : 1,
      near: near ? { fid: near.fid, how: near.how, ...(near.foe ? { foe: true } : {}) } : null,
    };
  }

  /** The heaviest gun a fighter group carries that month (guns.ts), for the way what it shoots down goes. */
  private heaviest(v: AirUnit): GunKind | null {
    if (!this.gunOf.has(v.id)) {
      const L = this.place.guns?.[v.nat]?.ftr ?? fighterGuns(v.model, v.nat, this.when);
      const order: GunKind[] = ['rifle', 'hmg', 'cannon20'];
      const top = L.guns.reduce<GunKind | null>((m, g) => (m === null || order.indexOf(g.kind) > order.indexOf(m) ? g.kind : m), null);
      this.gunOf.set(v.id, top);
    }
    return this.gunOf.get(v.id) ?? null;
  }

  /** A group as another's dispatch names it: 'a bomber of the 8th Bomb Group', 'a German fighter of JG 26'. */
  private nameOf(v: AirUnit): string {
    return v.air === 'bmr' ? this.aBomberOf(v, false) : aCraftOf(v.name, 'fighter', this.adjOf(v));
  }

  /**
   * A hit one of its aircraft survives (Brian, 20 November: "not all planes that get hit go down"; fates.ts hitOf):
   * while a group is being hit, a chance of SKY.hit.k times what it was owed this tick, at most once each SKY.hit.gap
   * battle seconds. Shown only, and never told: holes, smoke, pieces torn off, or a bomber's engine fire put out.
   */
  private hitShown(u: AirUnit, owed: number): void {
    const H = SKY.hit;
    if (this.t - (this.hitAt.get(u.id) ?? -Infinity) < H.gap) return;
    if (!this.hitRng.chance(Math.min(0.5, owed * H.k))) return;
    this.hitAt.set(u.id, this.t);
    const h = hitOf(this.hitRng, u.air);
    // A fire put out leaves its propeller feathered on a bomber with more than one engine (the art director).
    this.say('air:hit', u, '', { ...h, air: u.air, ...(h.mark === 'fireout' && enginesOf(u.model) > 1 ? { prop: 'feather' } : {}), slot: this.hitRng.int(Math.max(1, u.figures)), alt: u.alt });
  }

  /**
   * The defending fighter group attacking a box: one in contact with it, still fighting and firing on it this tick,
   * the one pressing it hardest first. Null when no fighter of the defence is firing on the box.
   */
  private onBox(b: AirUnit): AirUnit | null {
    const m = this.owedBy.get(b.id);
    let best: AirUnit | null = null, most = -Infinity;
    for (const id of b.contacts) {
      const d = this.unit(id);
      if (!d || d.side !== 'def' || d.air !== 'ftr' || !flying(d) || away(d) || !d.fire.some((f) => f.on === b.id)) continue;
      const k = m?.get(d.id) ?? 0;
      if (k > most) { most = k; best = d; }
    }
    return best;
  }

  /**
   * The beat of the passes (SKY.flak.pass). The fighters firing on a box go in on it together, break away to form up
   * again, and go in again: a new attack opens with a pass, and each pass and each gap lasts as the pass the group
   * pressing hardest is flying has it. When the last of them stops firing on the box the beat ends. Each fighter's
   * fire on the box says where it is in the beat ('go' or 'wait', the air seconds left), for the scene to fly the
   * pass by. Nothing here is counted: the fire each side takes is the same every tick, going in or standing off.
   */
  private beat(live: AirUnit[], dt: number): void {
    const B = SKY.flak.pass;
    const length = (go: boolean, d: AirUnit, b: AirUnit): number => {
      const pass = this.passes.get(`${d.id}:${b.id}`) ?? 'quarter';
      return ((go ? B.go : B.off)[pass] ?? (go ? B.go : B.off).quarter) * this.beatRng.range(1 - B.vary, 1 + B.vary);
    };
    const on = new Set<number>();
    for (const b of live) {
      if (b.side !== 'att' || b.air !== 'bmr') continue;
      const d = this.onBox(b);
      if (!d) continue;
      on.add(b.id);
      let c = this.beats.get(b.id);
      if (!c) this.beats.set(b.id, (c = { go: true, left: length(true, d, b), by: d.id }));
      else {
        c.by = d.id;
        c.left -= dt;
        while (c.left <= 0) {
          // A gap of no length is no gap: the next pass follows straight on the last.
          const gap = c.go ? length(false, d, b) : 0;
          if (c.go && gap > 0) { c.go = false; c.left += gap; } else { c.go = true; c.left += length(true, d, b); }
        }
      }
      const left = Math.ceil(c.left * 10) / 10;
      for (const id of b.contacts) {
        const f = this.unit(id)?.fire.find((x) => x.on === b.id);
        if (f && this.unit(id)!.side === 'def') Object.assign(f, c.go ? { go: left } : { wait: left });
      }
    }
    for (const id of [...this.beats.keys()]) if (!on.has(id)) this.beats.delete(id);
  }

  /**
   * Which boxes the guns are holding their fire on this tick. The first time the guns fall silent for a box, and the
   * first time they open up on it again, is told when the battle is shown; after that the scene shows it (each
   * group's 'flak' and 'hold', every tick). They open up again between two passes, as the fighters pull away to form
   * up, or when the fighters break off for good.
   */
  private holdFire(now: Map<number, number>, live: AirUnit[]): void {
    for (const [box, by] of now) {
      if (this.held.has(box) || this.heldTold.has(box)) continue;
      this.heldTold.add(box);
      const b = this.unit(box)!, d = this.unit(by)!;
      this.shownSay('air:hold', b, `The guns of ${this.place.name} hold their fire as ${told(this.called(d), '{n} [goes|go] in on')} ${subjectOf(this.called(b)).whom}.`, { box, by, hold: true });
    }
    for (const [box, by] of this.held) {
      if (now.has(box) || this.openTold.has(box)) continue;
      const b = live.find((u) => u.id === box);
      // Only where the guns do open up again: the box still on its way in, inside the ring.
      if (!b || b.dropped || b.jettisoned || away(b) || dist(b.x, b.y, SKY.target.x, SKY.target.y) > SKY.flakR) continue;
      this.openTold.add(box);
      const d = this.unit(by);
      const how = this.beats.has(box) ? '{N} [pulls|pull] away, and the' : '{N} [breaks|break] off, and the';
      this.shownSay('air:hold', b, `${d ? told(this.called(d), how) : 'The'} guns of ${this.place.name} open up again on ${subjectOf(this.called(b)).whom}.`, { box, by, hold: false, ...(this.beats.has(box) ? { between: true } : {}) });
    }
    this.held = now;
  }

  /** What the scene reads of the guns' hold on a box: the group they are silent for and how long, or how soon the next pass is. */
  private heldLook(u: AirUnit, done: boolean): { hold?: number; holdLeft?: number; holdIn?: number } {
    const c = this.beats.get(u.id);
    const left = c ? Math.ceil(c.left * 10) / 10 : 0;
    if (this.held.has(u.id)) return { hold: this.held.get(u.id), holdLeft: left };
    const firing = SKY.flak.hold && c && !c.go && u.side === 'att' && this.place.gunLoss > 0 && !done && !away(u) && dist(u.x, u.y, SKY.target.x, SKY.target.y) <= SKY.flakR;
    return firing ? { holdIn: left } : {};
  }

  /** The group that owed a group the most this tick takes the next aircraft it loses ('flak' for the guns). */
  private creditor(u: AirUnit): number | 'flak' | null {
    const m = this.owedBy.get(u.id);
    if (!m || !m.size) return null;
    let best: number | 'flak' = 'flak', most = -Infinity;
    for (const [by, k] of m) if (k > most) { most = k; best = by; }
    m.set(best, most - 1);
    return best;
  }

  /**
   * A box hit this tick may have one of its bombers lose an engine and fall out of formation (the design's 1.6, ruling
   * 237.1): a chance of strag.k times what the box was owed, never more than strag.most at a time, never its last
   * aircraft in formation. The engine, and whether its airscrew is feathered or windmills, by the battle's own stream
   * for stragglers. A single-engined bomber (a carrier or dive bomber) cannot lose its one engine and stay up: its
   * engine is hit and runs rough, its airscrew still turning ('windmill', engine 1), and it falls back as a twin does.
   */
  private straggle(b: AirUnit): void {
    const G = SKY.strag;
    if (b.n <= 0 || b.strag.length >= G.most || b.n - b.strag.length <= 1) return;
    if (this.stragRng.next() >= G.k * b.owed) return;
    const m = this.owedBy.get(b.id);
    let by: number | 'flak' = 'flak', most = -Infinity;
    if (m) for (const [k, v] of m) if (v > most) { most = v; by = k; }
    const engines = enginesOf(b.model), four = engines === 4, single = engines === 1;
    // The same draws whatever the engines, so a box's stragglers fall out at the same moments whatever it flies.
    const one = 1 + this.stragRng.int(four ? 4 : single ? 1 : 2);
    // One engine: hit, still turning. Two engines: one out, feathered. Four: one dead and windmilling, or two out and
    // feathered (B38, B39); and before its bombs have gone, one cleanly feathered too, for a loaded heavy on three
    // engines seldom held the box at height (the historian's review, question 6: after the drop most could keep up, so
    // it does not fall out for that).
    const loaded = b.dropped <= 0 && !b.jettisoned;
    const x = four ? this.stragRng.next() * (loaded ? 3 : 2) : 0;
    const two = four && x >= 1 && x < 2, wind = four && x < 1;
    const eng = two ? [one, one % 4 + 1].sort((p, q) => p - q) : [one];
    const s: Straggler = {
      id: this.stragId++, eng, prop: wind || single ? 'windmill' : 'feather', why: by === 'flak' ? 'flak' : 'ftr', by, age: 0, back: 0, down: 0,
      rate: four ? this.stragRng.range(G.four.back[0], G.four.back[1]) : G.twin.back * SKY.speed.bmr, sink: four ? G.four.sink : G.twin.sink, cover: null,
    };
    // From its own stream, never a count: its dead engine may be burning as it falls out, and the crew may put it out.
    if (this.stragLook.chance(G.fire.k)) s.fire = this.stragLook.chance(G.fire.stays) ? Infinity : G.fire.out[0] + this.stragLook.next() * (G.fire.out[1] - G.fire.out[0]);
    // Shown as a model falling out of the box only when the loss it stands for brings a model down (AR1): the box's
    // losses so far, the stragglers ahead of it and itself, at the model's scale. Never the box's last model.
    const ahead = b.strag.filter((x) => this.stragShown.has(x.id)).length;
    const owed = Math.floor((b.n0 - b.n + b.strag.length + 1) / b.per + 0.5);
    if (b.per <= 1 || (owed > b.figures0 - b.figures + ahead && b.figures - ahead > 1)) this.stragShown.add(s.id);
    b.strag.push(s);
    // By name: the bomber that falls out keeps its seat, and the box's next loss, the straggler's, is that bomber.
    const B = this.seated.get(b.id);
    const seat = B && B.form.length ? this.seatFrom(B, B.form) : -1;
    if (seat >= 0) this.stragSeat.set(s.id, seat);
    // The UI director's words (ui-director/words-air-battle-dispatches.md, 21:19Z): both forms end alike. A
    // single-engined bomber has no engine to lose and fly on: its one engine is hit (the Mac's first Pearl Harbor run).
    const text = single
      ? s.why === 'flak' ? `Flak has hit the engine of ${this.aBomberOf(b, false)}, and it is falling out of formation.` : `${this.aBomberOf(b, true)} has taken a hit in the engine and is falling out of formation.`
      : s.why === 'flak' ? `Flak has knocked out an engine on ${this.aBomberOf(b, false)}, and it is falling out of formation.` : `${this.aBomberOf(b, true)} has lost an engine and is falling out of formation.`;
    this.shownSay('air:straggle', b, text, { id: s.id, box: b.id, model: this.stragShown.has(s.id) ? 1 : 0, eng: s.eng, prop: s.prop, why: s.why, by: s.by, ...(s.fire ? { fire: true } : {}), ...(B && seat >= 0 ? { bomber: toldOf(B, seat) } : {}) });
  }

  /** The aircraft a box loses next: its oldest straggler that has been seen to leave and has no escort over it; failing that, one in formation; failing that, any straggler. */
  private nextDown(b: AirUnit): Straggler | null {
    const G = SKY.strag;
    const k = b.strag.findIndex((s) => s.age >= G.seen && s.cover === null);
    const at = k >= 0 ? k : b.n > b.strag.length ? -1 : 0;
    return at >= 0 ? b.strag.splice(at, 1)[0] : null;
  }

  /**
   * One of a list of a box's seats, the likelier the greater its risk (a green crew's, as the campaign's own draw has
   * it: bombers.ts unlucky), taken out of the list; with 'spare' aircraft the box has beyond its seats, maybe one of
   * those (-1).
   */
  private seatFrom(B: Seated, pool: number[], spare = 0): number {
    let all = spare;
    for (const k of pool) all += B.all[k].risk;
    if (all <= 0) return -1;
    let x = this.seatRng.next() * all;
    for (let i = 0; i < pool.length; i++) { x -= B.all[pool[i]].risk; if (x < 0) return pool.splice(i, 1)[0]; }
    return spare > 0 || !pool.length ? -1 : pool.pop()!;
  }

  /**
   * Which bomber by name a box's loss is: its straggler's seat when the loss is a straggler's; otherwise one in
   * formation; failing that, one that had limped clear. -1 for a box not known by name.
   */
  private seatDown(u: AirUnit, s: Straggler | null): number {
    const B = this.seated.get(u.id);
    if (!B) return -1;
    let k = -1;
    if (s && this.stragSeat.has(s.id)) { k = this.stragSeat.get(s.id)!; this.stragSeat.delete(s.id); }
    else if (B.form.length || B.spare > 0) { k = this.seatFrom(B, B.form, B.spare); if (k < 0) B.spare--; }
    else if (B.away.length) k = this.seatFrom(B, B.away);
    if (k >= 0) B.fell.push(k);
    return k;
  }

  /** A straggler's seat goes back to its box's formation (it caught up), or among those that limped clear. */
  private seatBack(b: AirUnit, s: Straggler, to: 'form' | 'away'): void {
    const B = this.seated.get(b.id), k = this.stragSeat.get(s.id);
    if (!B || k === undefined) return;
    this.stragSeat.delete(s.id);
    B[to].push(k);
  }

  /**
   * Which bomber's gunners brought down a fighter whose loss is booked to a box known by name. The gunners' kills are
   * the box's that owed them (ruling 227): the loss's creditor ('by', the group that owed it the most) is the box, and
   * the box is credited, one of its bombers by the battle's own stream. A loss booked to an escort or to the guns is
   * theirs, and no lot moves it. Returns the box and the seat, or null.
   */
  private gunnerOf(u: AirUnit, by: number | 'flak' | null | undefined): [number, number] | null {
    if (typeof by !== 'number' || !this.seated.has(by)) return null;
    const box = this.unit(by)!, B = this.seated.get(by)!;
    // Any bomber still in formation; failing that, one of the box's stragglers.
    const pool = B.form.length ? B.form : box.strag.map((s) => this.stragSeat.get(s.id)).filter((k): k is number => k !== undefined);
    if (!pool.length) return null;
    const k = pool[this.seatRng.int(pool.length)];
    const got = B.shot.get(k) ?? {};
    got[u.nat] = (got[u.nat] ?? 0) + 1;
    B.shot.set(k, got);
    return [box.id, k];
  }

  /** Where a straggler is: behind its box along its heading, and below it. */
  private stragAt(b: AirUnit, s: Straggler): { x: number; y: number } {
    return { x: b.x - Math.sin(b.face) * s.back, y: b.y - Math.cos(b.face) * s.back };
  }

  /**
   * 'A bomber of KG 2' for his own box, 'a German bomber of KG 2' for the enemy's (the UI director, 21:19Z), 'a bomber
   * of the 8th Bomb Group'; a box named for its aircraft 'one of the Shokaku and Zuikaku dive bombers', the enemy's 'one
   * of the Japanese ...', and 'how' after the name ('one of the ..., damaged,') (names.ts aCraftOf).
   */
  private aBomberOf(b: AirUnit, first: boolean, how = ''): string {
    const said = aCraftOf(b.name, 'bomber', this.adjOf(b), how);
    return first ? upFirst(said) : said;
  }

  /** A straggler that got clear of the fight and is on its way home. */
  private limped(b: AirUnit, s: Straggler, seen = false): void {
    b.away++;
    this.stragShown.delete(s.id);
    this.seatBack(b, s, 'away');
    // Told only when one is seen to get clear in the fight; those that go home with their box, or at the end, are not.
    this.shownSay('air:limped', b, seen ? `${this.aBomberOf(b, true, 'damaged')} has turned for home on its own.` : '', { id: s.id, box: b.id, ...this.stragAt(b, s) });
  }

  /**
   * Each tick, what the 3D game reads beyond the counts (the design's 1.5 to 1.8; the bridge passes 'extra' on): the
   * aircraft and tactics, a fighter's ammunition (1 full, 0 spent: its fight left above the share at which it breaks
   * off), fuel for the raid's fighters and its fire; a box's state (tight while steady, loose while wavering, broken
   * while routing) and its stragglers, each drifting back and down, one covered by a pair of its close escort.
   */
  private shown(u: AirUnit, dt: number): void {
    if (u.air === 'ftr') {
      const ammo = u.kit0 > 0 ? clamp01((u.vig - SKY.spent) / (1 - SKY.spent)) : 1;
      // 'cool': how its engines are cooled, for the shade of what a hit one trails (the art director; fates.ts coolOf).
      // For the pilots (shown only): the bombers an escort guards ('with'), the enemy group it is making for this tick
      // ('quarry': an interceptor's from before the groups meet, an escort's only while it has turned on an attacker),
      // and how good its pilots are ('skill').
      const q = flying(u) && !away(u) ? this.after.get(u.id) : undefined;
      u.extra = {
        model: u.model, cool: coolOf(u.model), form: u.form, style: u.style, boxes: this.boxesOf(u), ammo: Math.round(ammo * 1000) / 1000,
        ...(u.job.do === 'escort' ? { role: roleOf(u.job), with: u.job.on } : {}),
        // A raid on troops: a group on its passes flies them low ('strafe'), and 'run' is how far down this pass it is.
        ...(u.job.do === 'strafe' ? { strike: 'strafe', run: Math.round((this.strafing.get(u.id)?.p ?? 0) * 1000) / 1000, passes: this.strafing.get(u.id)?.done ?? 0 } : {}),
        ...(q !== undefined ? { quarry: q } : {}),
        skill: this.skill.get(u.id) ?? SKY.pilots.base,
        ...(u.def ? { def: u.def } : {}),
        ...(u.side === 'att' && u.fuel0 < SKY.fuel.home ? { fuel: Math.round(clamp01(u.fuel / Math.max(1, u.fuel0)) * 1000) / 1000 } : {}),
        fire: u.fire.map((f) => ({ ...f, rate: Math.round(f.rate * 10_000) / 10_000 })),
      };
      return;
    }
    const done = u.dropped > 0 || u.jettisoned;
    u.extra = {
      model: u.model, cool: coolOf(u.model), form: u.form, ...(u.strike ? { strike: u.strike, run: Math.round(Math.min(1, u.run / SKY.run) * 1000) / 1000, drawAlt: Math.round(this.shownAlt(u)) } : {}),
      // The ship its wing goes for (S2): the 3D bends the last of its run onto it. Absent with none afloat to go for.
      ...(this.sea && u.side === 'att' && this.sea.quarryOf(u.corps) != null ? { quarry: this.sea.quarryOf(u.corps) } : {}), box: u.state === 'routing' ? 'broken' : u.state === 'wavering' ? 'loose' : 'tight', boxes: this.boxesOf(u), away: u.away,
      strag: u.strag.map((s) => ({ id: s.id, model: this.stragShown.has(s.id) ? 1 : 0, eng: s.eng, prop: s.prop, why: s.why, cover: s.cover ?? -1, back: Math.round(s.back), down: Math.round(s.down), ...(s.fire ? { fire: true } : {}), ...(s.join ? { join: true } : {}) })),
      // The guns firing at it: only over a defended target, on its way in, never once it turns for home (the brief),
      // and never while their own fighters are going in on it ('hold': the guns are silent for that pass, with the air
      // seconds of it left in 'holdLeft'; between passes 'holdIn' is the air seconds until the next, SKY.flak.hold).
      flak: u.side === 'att' && this.place.gunLoss > 0 && !done && !away(u) && !this.held.has(u.id) && dist(u.x, u.y, SKY.target.x, SKY.target.y) <= SKY.flakR ? SKY.flak.kinds[this.shownBand(u)] : [],
      ...this.heldLook(u, done),
    };
  }

  /** A strike on a fleet at sea: the bombs fall on the fleet, as the ticket's title names it ("the Mobile Force"), not
   * on the place; undefined for any other raid, a harbour's included. */
  private quarry(): string | undefined {
    const name = this.place.ships && !this.place.ships.law.harbour ? this.place.ships.law.fleets[0]?.name : undefined;
    return name ? `the ${name}` : undefined;
  }

  /** The band a box is drawn at: a torpedo box flies its run in low, though it is counted in the middle (ships.ts BAND). */
  private shownBand(u: AirUnit): Band { return u.strike === 'torpedo' && !u.dropped && dist(u.x, u.y, SKY.target.x, SKY.target.y) <= SKY.flakR ? 0 : u.band; }
  private shownAlt(u: AirUnit): number { return this.shownBand(u) === u.band ? u.alt : SKY.bands.alt[this.shownBand(u)]; }

  /**
   * A box's stragglers each tick, shown or not (the box's next loss is its straggler's, so the way each loss goes, which
   * the dispatches tell, follows them): each drifts back and down, its fire may go out, it may catch its box up after
   * the drop, or get clear and limp home; one is covered by a pair of its close escort. Told only when shown.
   */
  private drift(u: AirUnit, dt: number): void {
    const G = SKY.strag, done = u.dropped > 0 || u.jettisoned;
    for (const s of [...u.strag]) {
      s.age += dt;
      // The crew put the fire out (Brian: "after they put out the fire"), feathering the dead engine's propeller as they
      // fired its bottle (the historian; the art director: it stops feathered, and keeps a soot streak); a carrier or
      // dive bomber's one engine runs on, rough. One that never goes out burns on.
      if (s.fire !== undefined && s.fire > 0 && s.fire !== Infinity && (s.fire -= dt) <= 0) {
        s.fire = 0;
        if (enginesOf(u.model) > 1) s.prop = 'feather';
        this.shownSay('air:fireout', u, `On ${this.aBomberOf(u, false)}, the crew put out the engine fire.`, { id: s.id, box: u.id, prop: s.prop, ...this.stragAt(u, s) });
      }
      // Once its box has dropped and its fire is out, a heavy on three engines may catch the box up: most with the dead
      // engine feathered, few with it windmilling (the historian). A twin on one engine, or a carrier or dive bomber with
      // its one engine hit, never does.
      if (done && s.join === undefined && !(s.fire !== undefined && s.fire > 0)) {
        const p = enginesOf(u.model) === 4 && s.eng.length === 1 ? (s.prop === 'feather' ? G.join.feather : G.join.windmill) : 0;
        s.join = p > 0 && this.stragLook.chance(p);
        if (s.join) { s.rate = -G.join.close; s.sink = -G.join.climb; }
      }
      s.back += s.rate * dt;
      s.down += s.sink * dt;
      if (!done) s.back = Math.min(s.back, G.held);
      else if (s.join && s.back <= 0) {
        u.strag.splice(u.strag.indexOf(s), 1);
        this.stragShown.delete(s.id);
        this.seatBack(u, s, 'form');
        this.shownSay('air:regain', u, this.regainWords(u, s), { id: s.id, box: u.id, prop: s.prop });
        continue;
      } else if (s.back > G.clear) { u.strag.splice(u.strag.indexOf(s), 1); this.limped(u, s, true); }
      if (s.join) s.down = Math.max(0, s.down);
    }
    const guard = this.units.find((f) => f.job.do === 'escort' && f.job.on === u.id && f.job.close && flying(f) && !away(f) && !f.contacts.length);
    u.strag.forEach((s, k) => { s.cover = k === 0 && guard ? guard.id : null; });
  }

  /**
   * A straggler back with its box, naming the engines it still has from its type, in words, as an idiom (the UI
   * director, 20 November 15:02Z): 'on three engines' for a four-engined bomber, 'on one engine' for a twin, nothing
   * for a single-engined one. At night there is no formation: it stays with the stream (the historian; the words a
   * placeholder for the UI director).
   */
  private regainWords(u: AirUnit, s: Straggler): string {
    const left = enginesOf(u.model) - s.eng.length;
    const on = enginesOf(u.model) <= 1 || left < 1 ? '' : `, on ${['', 'one', 'two', 'three'][left] ?? String(left)} engine${left === 1 ? '' : 's'},`;
    return this.place.night
      ? `${this.aBomberOf(u, true)}${on} stays with the stream.`
      : `${this.aBomberOf(u, true)}${on} has caught up and is back in formation.`;
  }

  /** An event only a shown battle tells: the straggler's, a hit it survives (tests/battle-air-realism.test.ts). */
  private shownSay(what: FieldEvent['what'], u: AirUnit | null, text: string, extra?: Record<string, unknown>, cue?: string): void {
    if (this.show) this.say(what, u, text, extra, cue);
  }

  /** Morale, leaving the board, and the end of a group. */
  private settle(u: AirUnit, dt: number): void {
    if (u.bounce > 0) {
      u.bounce = Math.max(0, u.bounce - dt);
      // The dive that ended its staff's climb has run its course: from here the group is priced as any other.
      if (u.bounce <= 0 && u.rise) u.rise = false;
    }
    if (u.n <= 0) { this.state(u, 'gone', told(this.called(u), '{N} [is|are] wiped out.')); return; }
    u.str = u.n / u.n0;
    if (!u.contacts.length) u.mor = Math.min(SKY.morale.base + SKY.morale.ready, u.mor + SKY.morale.mend * dt);
    const was = u.state;
    if (u.mor < SKY.morale.routing && was !== 'routing') {
      // One notice for one event (the UI director, 18:28Z): the break says the bombs went too.
      const dump = u.air === 'bmr' && !u.dropped && !u.jettisoned;
      if (dump) u.jettisoned = true;
      this.state(u, 'routing', dump ? told(this.called(u), '{N} [breaks|break]: [it jettisons its|they jettison their] bombs and [turns|turn] for home.') : told(this.called(u), '{N} [breaks|break] off and [heads|head] for home.'));
      u.job = { do: 'home' };
    } else if (was !== 'routing' && u.mor < SKY.morale.wavering && was !== 'wavering') this.state(u, 'wavering', told(this.called(u), '{N} [is|are] wavering.'));
    else if (was === 'wavering' && u.mor >= SKY.morale.wavering + 0.05) this.state(u, 'steady', told(this.called(u), '{N} [steadies|steady].'), 'rallied');
    // For the models: what it is doing, and whom it is fighting.
    u.engaged = u.contacts[0];
    u.act = u.job.do === 'wait' ? 'idle' : u.state === 'routing' ? 'rout' : u.contacts.length ? 'fight' : 'march';
    if (u.air === 'bmr') this.drift(u, dt);
    if (this.show) this.shown(u, dt);
    // Off the board: a raider past its own edge, a defender down at its airfield.
    if (u.job.do === 'home' || (u.job.do === 'route' && (u.dropped || u.jettisoned))) {
      const gone = u.side === 'att' ? u.y <= 0 : dist(u.x, u.y, u.homeX, u.homeY) <= 1_500;
      if (gone) this.state(u, 'withdrawn', u.side === 'att' ? told(this.called(u), '{N} [is|are] clear of the fight and [heads|head] for home.') : told(this.called(u), '{N} [lands|land].'));
    }
  }

  private state(u: AirUnit, s: UnitState, text: string, what?: FieldEvent['what']): void {
    if (u.state === s) return;
    u.state = s;
    if (s === 'gone' || s === 'withdrawn') { u.act = 'down'; u.speed = 0; u.engaged = undefined; u.contacts = []; if (s === 'gone') u.figures = 0; }
    if (s === 'withdrawn') for (const st of u.strag.splice(0)) this.limped(u, st);
    this.say(what ?? (s === 'steady' ? 'rallied' : s), u, text);
  }

  /** Every bomber has bombed or turned back (with no bombers, every raider is on its way home), and nobody is in a fight. */
  winding(): boolean {
    const att = this.units.filter((u) => u.side === 'att');
    const fighting = this.units.some((u) => flying(u) && u.contacts.length > 0);
    const done = (u: AirUnit): boolean => off(u) || u.job.do === 'home' || u.state === 'routing' || (u.air === 'bmr' && (u.dropped > 0 || u.jettisoned));
    const bombers = att.filter((u) => u.air === 'bmr');
    // (A raid on troops is not over while its fighters are still going down on them.)
    if (this.troops && att.some((u) => !off(u) && u.job.do === 'strafe')) return false;
    return !fighting && (bombers.length ? bombers.every(done) : att.every(done));
  }

  private end(dt: number): void {
    const att = this.units.filter((u) => u.side === 'att');
    if (att.every(off)) return this.finish('rout');
    if (this.gaveUp) return this.finish('withdrew');
    if (this.t >= this.ticket.limit) return this.finish('time');
    this.quietFor = this.winding() ? this.quietFor + dt : 0;
    if (this.quietFor >= SKY.quiet) this.finish('objective');
  }

  // ---------------------------------------------------------------------------------------------------------------

  command(c: Command, by: Side): boolean {
    if (this.over) return false;
    const mine = (ids?: number[]): AirUnit[] => (ids ?? []).map((i) => this.unit(i)!).filter((u) => u && u.side === by && !off(u) && u.state !== 'routing');
    const fighters = (ids?: number[]): AirUnit[] => mine(ids).filter((u) => u.air === 'ftr' && u.job.do !== 'wait');
    switch (c.do) {
      case 'auto': this.ai[c.side] = c.on; if (c.on) this.think(c.side, true); return true;
      // Hand it to the staff (ruling 217(4), the design director 18:27Z): the rest is settled at once from where each
      // side stands, and what has been fought stays fought. The rules then hold it inside the dice's run as always.
      case 'dice': this.settleNow(); return true;
      case 'withdrawAll': {
        if (c.side !== by) return false;
        this.gaveUp = by;
        for (const u of this.units) if (u.side === by && !off(u)) this.goHome(u);
        this.keptUp(this.units.filter((u) => u.side === by));
        this.end(0);
        return true;
      }
      case 'withdraw': { const us = mine(c.units); us.forEach((u) => this.goHome(u)); this.keptUp(us); return us.length > 0; }
      case 'move': case 'hold': {
        const us = fighters(c.units);
        for (const u of us) u.job = c.do === 'move' ? { do: 'patrol', x: c.x, y: c.y } : { do: 'patrol', x: u.x, y: u.y };
        this.keptUp(us);
        return us.length > 0;
      }
      case 'attack': {
        const e = this.unit(c.target);
        if (!e || e.side === by || !flying(e)) return false;
        const us = fighters(c.units);
        for (const u of us) u.job = { do: 'intercept', on: e.id };
        this.keptUp(us);
        return us.length > 0;
      }
      case 'air:escort': {
        const b = this.unit(Number(c.target));
        if (!b || b.side !== by || b.air !== 'bmr' || off(b)) return false;
        const us = fighters(c.units);
        for (const u of us) { u.job = { do: 'escort', on: b.id, close: c.close !== false }; u.to = c.close !== false ? b.band : u.to; }
        this.keptUp(us);
        return us.length > 0;
      }
      case 'air:hunt': { const us = fighters(c.units); us.forEach((u) => { u.job = { do: 'hunt' }; }); this.keptUp(us); return us.length > 0; }
      // A raid on troops (release 2): send fighters down on the troops now, or ({ hold: true }) keep them up with the
      // bombers, so that they never go down on their own. Nothing else is offered: the campaign books the troops by
      // the bombers that drop and the fighters that strafe, not by which part of the column is hit.
      case 'air:strafe': {
        if (!this.troops) return false;
        const us = fighters(c.units).filter((u) => u.side === 'att' && u.job.do !== 'home' && !this.strafing.get(u.id)?.over);
        if (c.hold === true) {
          for (const u of us) {
            this.keepUp.add(u.id);
            if (u.job.do !== 'strafe') continue;
            const box = this.units.filter((b) => b.side === 'att' && b.air === 'bmr' && !off(b) && !away(b)).sort((a, b) => dist(u.x, u.y, a.x, a.y) - dist(u.x, u.y, b.x, b.y) || a.id - b.id)[0];
            u.job = box ? { do: 'escort', on: box.id, close: true } : { do: 'patrol', x: SKY.target.x, y: SKY.target.y };
            u.to = box ? box.band : 1;
          }
          return us.length > 0;
        }
        for (const u of us) { this.keepUp.delete(u.id); this.goDown(u, u.job.do === 'escort' ? u.job.on : this.strafing.get(u.id)?.box ?? null); }
        return us.length > 0;
      }
      case 'air:band': {
        const b = Number(c.band);
        if (b !== 0 && b !== 1 && b !== 2) return false;
        const us = mine(c.units).filter((u) => u.job.do !== 'wait');
        // The player's own height order is priced as always, so it ends a staff's climb. Above the low band it takes a
        // group off its passes on the troops.
        us.forEach((u) => {
          if (u.job.do === 'strafe' && b > 0) { u.job = { do: 'patrol', x: u.x, y: u.y }; this.keptUp([u]); }
          u.to = b as Band; u.climbT = 0; u.rise = false;
        });
        return us.length > 0;
      }
      case 'air:jettison': {
        const us = mine(c.units).filter((u) => u.air === 'bmr' && !u.dropped && !u.jettisoned);
        for (const u of us) { u.jettisoned = true; u.job = { do: 'home' }; this.say('air:jettison', u, told(this.called(u), '{N} [jettisons its|jettison their] bombs and [turns|turn] for home.'), { n: u.n }); }
        return us.length > 0;
      }
      default: return false;
    }
  }

  /** A raid on troops: the raid's fighters given another order no longer go down on their own. */
  private keptUp(us: AirUnit[]): void {
    if (this.troops) for (const u of us) if (u.side === 'att') this.keepUp.add(u.id);
  }

  private goHome(u: AirUnit): void {
    if (u.air === 'bmr' && !u.dropped && !u.jettisoned && u.side === 'att') { u.jettisoned = true; this.say('air:jettison', u, told(this.called(u), '{N} [jettisons its|jettison their] bombs and [turns|turn] for home.'), { n: u.n }); }
    u.job = { do: 'home' };
  }

  /** Each side's fighting strength still in the air, as a share of what both sent up (the view's balance). */
  balance(): Record<Side, number> {
    const power = (s: Side): number => this.units.filter((u) => u.side === s).reduce((t, u) => t + u.n * u.q, 0);
    const p0 = (s: Side): number => this.units.filter((u) => u.side === s).reduce((t, u) => t + u.n0 * u.q, 0);
    const all = p0('att') + p0('def') || 1;
    return { att: power('att') / all, def: power('def') / all };
  }

  /**
   * The view, with land's two army bars in land's shape: breaking (how near each side is to breaking off, 0 to 1; it
   * breaks at 1) and contact (battle seconds of the first contact, null before it), so the 3D top bar reads both alike.
   */
  view(as?: Side): BattleView<AirUnit> & { breaking: Record<Side, number>; contact: number | null; ships?: ReturnType<ShipBook['view']>; marks?: ReturnType<TroopBook['view']> } {
    const units = this.units.map((u) => ({ ...u, spotted: { att: this.sees('att', u), def: this.sees('def', u) } }))
      .filter((u) => !as || u.side === as || u.spotted[as]);
    const events = this.events;
    this.events = [];
    return { t: this.t, limit: this.ticket.limit, units, balance: this.balance(), events, zones: this.zones, ai: { ...this.ai }, over: this.over, breaking: { att: this.nearBreak('att'), def: this.nearBreak('def') }, contact: this.metAt, ...(this.sea ? { ships: this.sea.view() } : {}), ...(this.troops ? { marks: this.troops.view() } : {}) };
  }

  /** Every event so far, for the after-action card and replays. */
  history(): FieldEvent[] { return [...this.log]; }

  // ---------------------------------------------------------------------------------------------------------------

  /**
   * The rest of the raid, settled at once from where each side stands (ruling 217(4)). Each fighter group still in the
   * fight deals what it has left before it would break off, at the strength it has now, to the enemies still in play:
   * a group the player drove off deals nothing more, a battered one less, and a box left without its escort pays for
   * it, as the board would have made it pay. Bombers still coming on get through; those that turned back stay turned.
   */
  /**
   * What the rest of the raid would still cost each pool from where each side stands, at even luck: the defending
   * fighters ('foe'), the raid's fighters ('ftr') and its bombers ('bmr'). Each fighter group's fight left, at its
   * strength now, against the enemy still in play, and the guns' run each box has still to fly. Groups on the ground
   * count: they would come up.
   */
  private expect(): { foe: number; ftr: number; bmr: number } {
    const live = this.units.filter((u) => !off(u));
    const inPlay = (v: AirUnit): boolean => !away(v);
    const out = { foe: 0, ftr: 0, bmr: 0 };
    const took = new Map<number, number>();
    // (A box's gunners count as a fighter group's fight does: against the enemy fighters still in play.)
    for (const u of live) {
      if (away(u) || u.kit0 <= 0) continue;
      const vig = (u.kit.ftr + u.kit.bmr) / u.kit0;
      if (vig <= SKY.spent) continue;
      const k = ((vig - SKY.spent) / vig) * (u.n / u.n0) * (u.state === 'wavering' ? SKY.edge.wavering : 1);
      const F = live.filter((v) => v.side !== u.side && v.air === 'ftr' && inPlay(v)), Bs = live.filter((v) => v.side !== u.side && v.air === 'bmr' && inPlay(v));
      if (F.length) out[u.side === 'att' ? 'foe' : 'ftr'] += u.kit.ftr * k;
      const nB = Bs.reduce((s, v) => s + v.n, 0);
      for (const b of Bs) {
        const most = this.place.law.most * b.n0 - (b.n0 - b.n) - (took.get(b.id) ?? 0);
        const hit = most > 0 ? Math.min(most, u.kit.bmr * k * (b.n / Math.max(1, nB)) * this.cover(b, live)) : 0;
        took.set(b.id, (took.get(b.id) ?? 0) + hit);
        out.bmr += hit;
      }
    }
    for (const b of live) if (b.air === 'bmr' && b.side === 'att' && inPlay(b)) out.bmr += b.n * this.place.gunLoss * SKY.flak.band[b.band] * this.flakNorm * (1 - b.shot);
    // A raid on troops: the ground fire still to come on the passes the raid's fighters have yet to fly.
    if (this.troops) for (const u of live) out.ftr += this.fireEach * this.passesAhead(u);
    return out;
  }

  /**
   * A raid on troops: the fighters a group is still expected to put on the troops, as passes' shares of it (its
   * fighters now, as the campaign weighs them, times the passes left over the passes a group flies): those of a group on
   * its passes, or of an escort that will go down once its bombers have bombed; none for one held up, gone home or done.
   */
  private passesAhead(u: AirUnit): number {
    if (!this.willStrafe(u)) return 0;
    const s = this.strafing.get(u.id), S = TROOPS.strafe;
    return (u.n * reachOf(u) * (S.passes - (s?.done ?? 0) - (s?.run ? s.p : 0))) / S.passes;
  }

  /**
   * HAND IT TO THE STAFF: what was fought stays fought, and the rest goes to the rules' own dice (the design director,
   * log b368). The battle ends here and hands back what each group had lost, with each pool's share of the raid's cost
   * still to come from where each side stands (the staff's reckoning now against the same reckoning at the start).
   * The campaign rolls the raid exactly as it does for any raid and books that share of its roll on top. Pressed at
   * once, the share is 1 and nothing is lost yet: it books exactly what the campaign would have rolled.
   */
  private settleNow(): void {
    if (this.over) return;
    const lost = (s: Side, k: 'ftr' | 'bmr'): number => this.units.filter((u) => u.side === s && u.air === k).reduce((t, u) => t + u.n0 - u.n, 0);
    this.before = { att: { ftr: lost('att', 'ftr'), bmr: lost('att', 'bmr') }, def: { ftr: lost('def', 'ftr'), bmr: 0 } };
    this.staff = { ...this.restNow(), chance: this.staffChance(), fired: this.fired };
    this.toStaff = true;
    this.finish('objective');
  }

  /**
   * What the staff would settle from where the battle stands: each pool's share of the raid's cost still to come (the
   * staff's reckoning now against the same reckoning at the start), and the bombers in the air that had turned back
   * without bombing, which do not bomb when the staff settle it.
   */
  restNow(): NonNullable<AirOutcome['staff']> {
    const now = this.expect(), S = this.start;
    // A pool the reckoning never weighed (no escort, no bombers) is left to the dice whole: they give it what they give it.
    const share = (k: 'foe' | 'ftr' | 'bmr'): number => (S[k] > 0 ? Math.min(2, now[k] / S[k]) : 1);
    const turned = this.units.filter((b) => b.air === 'bmr' && b.side === 'att' && b.n > 0 && !b.dropped && (b.jettisoned || away(b)));
    return { rest: { foe: share('foe'), ftr: share('ftr'), bmr: share('bmr') }, away: turned.reduce((t, b) => t + b.n, 0) };
  }

  /** The bombers the staff would expect over the target from here: those that have bombed, and those still on their way less the bombers still to fall. */
  private throughNow(bmrToCome: number): number {
    const bs = this.units.filter((u) => u.side === 'att' && u.air === 'bmr');
    const done = bs.reduce((t, b) => t + b.dropped, 0);
    const coming = bs.filter((b) => !off(b) && !b.dropped && !b.jettisoned && !away(b)).reduce((t, b) => t + b.n, 0);
    return done + Math.max(0, coming - bmrToCome);
  }

  /** A raid on troops: the fighters that have strafed, and those still expected to on the passes not yet completed. */
  private strafedExpected(): number {
    let n = this.strafed;
    for (const u of this.units) {
      const ahead = this.passesAhead(u), s = this.strafing.get(u.id);
      n += ahead + (s?.run ? (u.n * reachOf(u) * s.p) / TROOPS.strafe.passes : 0) * (ahead > 0 ? 1 : 0);
    }
    return n;
  }

  /** The bombers that have dropped on the target so far. */
  droppedNow(): number {
    return this.units.filter((u) => u.side === 'att' && u.air === 'bmr').reduce((t, u) => t + u.dropped, 0);
  }

  /**
   * As each box bombs: whether the bombs dropped so far have cut the railway, or wrecked a factory outright, by the
   * campaign's own law against the field's throws (b397). Told once each, as it happens.
   */
  private bombsFall(): void {
    const L = this.place.bombs;
    if (!L) return;
    const n = this.droppedNow();
    if (!this.railCut && railChanceOf(L, n) > this.rolls.rail) {
      this.railCut = true;
      this.say('air:rail', null, `The railway through ${this.place.name} is cut.`, { x: SKY.target.x, y: SKY.target.y });
    }
    if (!this.ruined && ruinChanceOf(L, n) > this.rolls.ruin) {
      this.ruined = true;
      this.say('air:ruin', null, `A factory at ${this.place.name} is wrecked outright.`, { x: SKY.target.x, y: SKY.target.y });
    }
  }

  /**
   * The score the stamp will judge, as it stands (the design director, b397; the UI director's aim slip, 20:53Z): the
   * aircraft each side has lost, what the bombs have done on the campaign's scale, the railway and the factory, and
   * the fighters each side still has in the air. 'ratio' is fly()'s own measure on these counts; 1 or more is a win.
   */
  score(): { lost: Record<Side, number>; worth: number; cut: boolean; ruined: boolean; ratio: number; up: Record<Side, number> } {
    const R = this.place.reckon, L = this.place.bombs;
    const lost = (s: Side): number => this.units.filter((u) => u.side === s).reduce((t, u) => t + u.n0 - u.n, 0);
    const dropped = this.droppedNow();
    const worth = this.place.mission === 'superiority' ? 0
      : this.sea ? this.sea.value()
      : this.troops ? this.troops.value(dropped, this.strafed)
      : L ? bombWorth(L, dropped) + 300 * ((this.railCut ? 1 : 0) + (this.ruined ? 1 : 0))
      : this.through0 > 0 ? R.value * Math.min(1.5, dropped / this.through0) : 0;
    const up = (s: Side): number => this.units.filter((u) => u.side === s && u.air === 'ftr' && flying(u)).reduce((t, u) => t + u.n, 0);
    const l = { att: lost('att'), def: lost('def') };
    return { lost: l, worth, cut: this.railCut, ruined: this.ruined, ratio: (l.def + worth + 10) / (l.att + 10), up: { att: up('att'), def: up('def') } };
  }

  /**
   * The raider's chance of success if the staff settled the rest now (ruling 226, mark 5: HAND IT TO THE STAFF says
   * the staff's chance now), weighed as forecastAir weighs the raid card's: the aircraft each side has lost and is
   * still expected to lose, and what the bombers still getting through should achieve. Before anything has happened
   * it is the raid card's own chance.
   */
  staffChance(): number {
    const P = this.place, R = P.reckon, F = P.forecast, { rest } = this.restNow();
    const lost = (s: Side, k: 'ftr' | 'bmr'): number => this.units.filter((u) => u.side === s && u.air === k).reduce((t, u) => t + u.n0 - u.n, 0);
    const foe = lost('def', 'ftr') + rest.foe * F.foe, ftr = lost('att', 'ftr') + rest.ftr * F.ftr, bmrToCome = rest.bmr * F.bmr;
    const bmr = lost('att', 'bmr') + bmrToCome;
    const L = this.place.bombs;
    let pr: number, pf: number, value: number;
    if (L) {
      // By the campaign's own law (b397): what the bombers still getting through should do, and, for the railway and a
      // factory, what has happened already, or the chance the staff's own throw gives it from all the bombs.
      const t = this.droppedNow(), T = Math.max(t, this.throughNow(bmrToCome));
      // Handed over, a railway already cut stays cut, and the staff throw again for one not yet cut, from all the bombs.
      const rest = (done: boolean, p: (n: number) => number): number => (done ? 1 : p(T));
      pr = rest(this.railCut, (n) => railChanceOf(L, n));
      pf = rest(this.ruined, (n) => ruinChanceOf(L, n));
      value = bombWorth(L, T);
    } else {
      const share = this.through0 > 0 ? Math.min(1.5, this.throughNow(bmrToCome) / this.through0) : 1;
      pr = Math.min(1, R.rail * share); pf = Math.min(1, R.ruin * share); value = R.value * share;
      // A strike on ships: what has gone down already, and the card's worth for the bombers still to drop.
      if (this.sea) value = this.sea.value() + (this.through0 > 0 ? R.value * Math.min(1.5, Math.max(0, this.throughNow(bmrToCome) - this.droppedNow()) / this.through0) : 0);
      // A raid on troops, by the campaign's own law: the bombers that have dropped and are still expected to, and the
      // fighters that have strafed and are still expected to.
      if (this.troops) value = this.troops.value(Math.max(this.droppedNow(), this.throughNow(bmrToCome)), this.strafedExpected());
    }
    const beats = (worth: number): number => { const x = Math.pow((foe + worth + 10) / (ftr + bmr + 10), R.k); return x / (1 + x); };
    return (1 - pr) * (1 - pf) * beats(value) + pr * (1 - pf) * beats(value + 300) + (1 - pr) * pf * beats(value + 300) + pr * pf * beats(value + 600);
  }

  outcome(): AirOutcome {
    const groups = new Map<number, AirOutcome['groups'][number]>();
    for (const u of this.units) {
      const row = groups.get(u.corps) ?? { id: u.corps, side: u.side, lostFtr: 0, lostBmr: 0, back: 0, flewFtr: 0, flewBmr: 0 };
      if (u.air === 'ftr') { row.lostFtr += u.n0 - u.n; row.flewFtr += u.n0; } else { row.lostBmr += u.n0 - u.n; row.flewBmr += u.n0; }
      row.back += u.back;
      // The bombers known by name: the rows of those that fell, and what each one's gunners shot down.
      // A bomber group's gunners' kills: every fighter whose loss was booked to it (ruling 227), named or not.
      if (this.gunKills.get(u.id)) row.guns = (row.guns ?? 0) + this.gunKills.get(u.id)!;
      const B = this.seated.get(u.id);
      if (B) {
        row.fell = [...(row.fell ?? []), ...B.fell.map((k) => B.all[k].row)];
        row.shot = [...(row.shot ?? []), ...[...B.shot].map(([k, by]) => ({ row: B.all[k].row, by: { ...by } }))];
      }
      groups.set(u.corps, row);
    }
    const att = this.units.filter((u) => u.side === 'att' && u.air === 'bmr');
    // A box that bombed lost u.n0 - u.dropped on the way in and u.dropped - u.n after; one that never bombed lost all its losses on the way.
    const after = att.reduce((s, u) => s + (u.dropped ? u.dropped - u.n : 0), 0);
    return {
      groups: [...groups.values()], chance: this.place.chance,
      dropped: att.reduce((s, u) => s + u.dropped, 0), jettisoned: att.reduce((s, u) => s + (u.jettisoned ? u.n : 0), 0),
      lostBefore: att.reduce((s, u) => s + u.n0 - u.n, 0) - after, lostAfter: after,
      ...(this.gaveUp ? { gaveUp: this.gaveUp } : {}),
      ...(this.staff ? { staff: this.staff } : {}),
      // Fought to the end, the field's throws decide the railway and the factory in the campaign too. Handed to the staff,
      // what the bombs had done stands (a throw of 0 cuts it) and the staff's own dice decide the rest (b368).
      // Fought to the end, the ships the field sank are the ones the campaign books; handed over, they stay sunk (ships.ts).
      ...(this.sea ? { sunk: this.sea.sunk() } : {}),
      // Ruling 240: with damage control in play, the ships it kept afloat on the same throws (ships.ts kept()).
      ...(this.sea?.kept() ? { kept: this.sea.kept()! } : {}),
      // A raid on troops: the fighters that strafed, and the organisation cut the field earned by the campaign's own law
      // from them and the bombers that dropped. Fought to the end, the campaign books the cut from these same counts.
      ...(this.troops ? { strafed: this.strafed, troops: { cut: troopCutOf(this.troops.law, att.reduce((s, u) => s + u.dropped, 0), this.strafed) } } : {}),
      ...(this.troops?.law.reach ? { strafers: [...this.firedOn].sort((x, y) => x - y), strafeFire: this.fireOwed } : {}),
      ...(this.place.bombs ? { rolls: this.staff ? { ...(this.railCut ? { rail: 0 } : {}), ...(this.ruined ? { ruin: 0 } : {}) } : { ...this.rolls }, railCut: this.railCut, ruined: this.ruined } : {}),
    };
  }

  /**
   * The field's verdict by the campaign's own measure (fly(): the aircraft the defender lost, and what the bombs were
   * worth, against the aircraft the raider lost, each with 10 added), on the score the field shows (score()): the
   * bombs weighed by the campaign's law, the railway and the factory as the field's throws decided them (b397). fly()
   * judges it again on the same counts, and the after card says that (b384(a); ruling 226 mark 6). A side that broke
   * off everything lost; where its losses say otherwise, it is a close defeat. As the raider reads it.
   */
  private judge(): { won: boolean; level: VictoryLevel; ratio: number } {
    const { ratio } = this.score();
    const won = this.gaveUp ? this.gaveUp === 'def' : ratio >= 1;
    const level: VictoryLevel = won ? (ratio >= 2 ? 'Decisive victory' : 'Close victory') : ratio < 0.5 ? 'Decisive defeat' : 'Close defeat';
    return { won, level, ratio };
  }

  private finish(how: FieldResult['how']): void {
    if (this.over) return;
    const out = this.outcome();
    const p0 = (s: Side): number => this.units.filter((u) => u.side === s).reduce((t, u) => t + u.n0 * u.q, 0) || 1;
    const pl = (s: Side): number => this.units.filter((u) => u.side === s).reduce((t, u) => t + (u.n0 - u.n) * u.q, 0);
    const lost = { att: pl('att') / p0('att'), def: pl('def') / p0('def') };
    const bombers0 = this.units.filter((u) => u.side === 'att' && u.air === 'bmr').reduce((s, u) => s + u.n0, 0);
    // The verdict by the campaign's own measure. Handed to the staff, the field has none of its own: the staff's dice
    // settle it, and the after card says what they made of it.
    const v = this.judge();
    const winner: Side = v.won ? 'att' : 'def';
    const R = fieldRatio(winner, lost);
    const count = (s: Side, k: 'ftr' | 'bmr'): number => this.units.filter((u) => u.side === s && u.air === k).reduce((t, u) => t + u.n0 - u.n, 0);
    // The after card in the UI director's words (airfought.ts afterWords). Handed to the staff, the battle does not
    // know the bill (the campaign's dice settle the rest), so its own card leaves the bill and the bombers line out;
    // the campaign's raid order writes the whole card from what it books (airpages.ts airAct).
    const me: Side = this.ticket.def.human && !this.ticket.att.human ? 'def' : 'att';
    const bill = (s: Side) => ({ ftr: count(s, 'ftr'), bmr: count(s, 'bmr') });
    const quarry = this.quarry();
    const lines = afterWords({
      place: this.place.name, ...(quarry ? { target: quarry } : {}), sky: this.place.mission === 'superiority', me, adj: { att: this.ticket.att.adj, def: this.ticket.def.adj },
      bombers: this.toStaff ? 0 : bombers0, dropped: out.dropped, jettisoned: Math.max(0, bombers0 - out.dropped - out.lostBefore), lostBefore: out.lostBefore, staff: this.toStaff,
      bill: this.toStaff ? null : { att: bill('att'), def: bill('def') }, before: this.before,
      ...(this.staff ? { how: staffHow(me, this.staff, this.place.chance) } : {}),
    });
    this.over = {
      winner, how, seconds: this.t, lost,
      men: { att: count('att', 'ftr') + count('att', 'bmr'), def: count('def', 'ftr') },
      R, level: v.level, lines,
      forces: out.groups.map((gr) => {
        const us = this.units.filter((u) => u.corps === gr.id);
        const n0 = us.reduce((s, u) => s + u.n0, 0) || 1;
        return { corps: gr.id, side: gr.side, lost: us.reduce((s, u) => s + u.n0 - u.n, 0) / n0 };
      }),
      extra: { air: out },
    };
    // Stragglers still out when it ends are on their way home.
    for (const u of this.units) for (const st of u.strag.splice(0)) this.limped(u, st);
    // The end is told without the bill: the campaign books it (the after card, airfought.ts), and a dispatch with the
    // battle's own count beside the card's would read as a second bill (the design director, 18:39Z).
    this.say('end', null, `${this.ticket.title} is over.`);
  }
}

/** A battle from a ticket. */
export const airBattle = (t: FieldTicket<AirPlace>, opts?: { show?: boolean; look?: number }): AirSim => new AirSim(t, opts);

/**
 * The lane a raid's fighters strafe down (a raid on troops): through the middle of the pieces on the ground, along the
 * column's road where it marches, otherwise along the way the pieces lie longest (a camp's spread, a line of works).
 * Laid from the pieces alone: no throw.
 */
function laneOf(pieces: { x: number; y: number }[], road: [number, number][] | null): { x: number; y: number; dx: number; dy: number } {
  const T = SKY.target;
  const n = pieces.length;
  const x = n ? pieces.reduce((s, p) => s + p.x, 0) / n : T.x, y = n ? pieces.reduce((s, p) => s + p.y, 0) / n : T.y;
  let dx = 0, dy = 1;
  if (road && road.length > 1) { dx = road[road.length - 1][0] - road[0][0]; dy = road[road.length - 1][1] - road[0][1]; }
  else if (n > 1) {
    // The long axis of the pieces' spread.
    let sxx = 0, syy = 0, sxy = 0;
    for (const p of pieces) { sxx += (p.x - x) ** 2; syy += (p.y - y) ** 2; sxy += (p.x - x) * (p.y - y); }
    const th = 0.5 * Math.atan2(2 * sxy, sxx - syy);
    if (sxx + syy > 0) { dx = Math.cos(th); dy = Math.sin(th); }
  }
  const d = Math.hypot(dx, dy) || 1;
  return { x, y, dx: dx / d, dy: dy / d };
}

/** What each kind of piece is called in a dispatch. */
const MARK_WORDS: Record<string, string> = {
  men: 'infantry', wagon: 'horse-drawn wagons', lorry: 'lorries', halftrack: 'half-tracks', car: 'staff cars', gun: 'field guns', aa: 'anti-aircraft guns', tank: 'tanks',
};

/**
 * The line for the biggest hit of a bomb pattern or a strafing pass, in plain words ("Bombs burst among the lorries:
 * several are burning."); 'your' where the troops are the player's own.
 */
function markWords(x: MarkNews, what: MarkWhat, own: boolean): string {
  const them = `${own ? 'your' : 'the'} ${MARK_WORDS[what] ?? 'troops'}`;
  const first = x.strike === 'strafe' ? `Fighters rake ${them} with gunfire` : `Bombs burst among ${them}`;
  const how = what === 'men'
    ? (x.how === 'blast' ? 'many are hit' : 'the men scatter for cover')
    : `${Math.round(x.men / (TROOPS.worth[what] ?? 1)) <= 1 ? 'one is' : 'several are'} ${x.how === 'burn' ? 'burning' : x.how === 'blast' ? 'thrown over' : 'knocked out'}`;
  return `${first}: ${how}.`;
}
