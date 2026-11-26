// The drill book: how each army's platoons fight (Brian, 22 November 2026, 00:23Z: "i want the troop AI to be smart and
// move like a platoon with covering fire and historical tactics. have the historian and general make sure they move and
// play like a real world war 2 unit would and with combines arms"). The principles and the schools are written out, with
// the historian's and the general's reviews, in /mnt/project-files/battles/drill-book/.
//
// Looks only (ruling 217), like the rest of the squad brain (drill.ts): nothing in the fight reads any of this, and it
// draws nothing from the battle's dice. It is on when the men are drawn as platoons (Drill platoons: the 3D game).
//
// Every army does the same things (fire and movement, short rushes from cover to cover, suppression, fix and flank,
// lift and shift, tanks and infantry together, the guns called by radio, the counter-attack); its SCHOOL, the army's own
// manual and kit, says how: a German Gruppe's riflemen move under its machine gun's fire, a British section's rifle group
// under its Bren group's, an American platoon's squads under its base-of-fire squad's; a raw Soviet battalion goes in
// waves, a veteran one in storm groups; a Japanese approach is a crawl, then the charge. Any nation not named fights by
// the default school (the principles as written): every playable nation fights by the book (Brian, 21 November 06:54Z).
// Nothing reads the date or who the enemy is (the sandbox, 19 November): experience alone changes how well an army does
// its drill.

import data from '../../../data/battle/drill.json';
import type { Weapon } from './drill';

/** An army's school of drill (its manual and its kit). */
export type School = 'GER' | 'UK' | 'USA' | 'SOV' | 'JAP' | 'ITA' | 'FRA' | '_';
/**
 * Who rushes with whom in fire and movement. The cycle has three turns (the general, 22 November: "one squad fires,
 * one moves, one follows: a third moves, not half"; split by job, never by halves): each man rushes once a cycle, in
 * his turn, while the rest are down firing.
 * - 'body': the platoon's squads one turn each, each squad as one body round its machine gun, the gun at its head (up
 *   first, down first and firing as the riflemen come up): the squads leapfrog, not the gun and its riflemen (the
 *   historian, 22 November: "a German squad moves as one body and squads leapfrog"). The German Gruppe, and the armies
 *   built round the squad's gun.
 * - 'squad': the platoon's squads one turn each, the headquarters with the leading squad's: one fires, one moves, one
 *   follows; the automatic rifle walks with its squad. The American platoon, the Soviet chain.
 * - 'both': as 'squad', each section's gun group a turn after its rifle group: one section fixes while the others move,
 *   each gun group covering its own riflemen, then following. The British platoon, and the principles as written.
 * - 'wave': all together.
 */
export type Bound = 'body' | 'squad' | 'both' | 'wave';
/** How the assault goes in at the last: from cover with grenades, walking forward firing (marching fire), or the charge. */
export type Final = 'grenade' | 'marching' | 'charge';

export interface SchoolNums {
  bound: Bound;
  /** The bound of a raw army (under DR.xp.raw) and of a veteran one, where they differ. */
  raw?: Bound;
  veteran?: Bound;
  /** The share of rushes made at a crouched creep instead (infiltration): the pace allowing. */
  crawl: number;
  final: Final;
  /** The assault leaves its squads' guns firing with the base of fire (a section's gun group stays; where the squad
   * fights as one body round its gun, the gun goes in with it). */
  gunStays: boolean;
  /** Metres to which a defence holds its fire, where its army's drill sets it (else by weapon and experience: hold). */
  hold?: number;
  /** Smoke rounds its platoons lay on the enemy's flank as an assault goes out (none: 0), while BOOK.smoke.on. */
  smoke: number;
  /** Battle seconds after the enemy reaches a defence's line before its rear platoons counter-attack (a negative: never). */
  counter: number;
}

interface BookData {
  schools: Record<School, SchoolNums>;
  nations: Record<string, School>;
  /** A rush's battle seconds by experience [raw, regular, veteran]; the pause between one turn's rush and the next's; how
   * late after his turn's first man a man rises (pairs by pairs). */
  rush: [number, number, number];
  gap: number;
  stagger: number;
  /** Suppression: by weapon [metres from where a round lands within which a group feels it, how much each adds]. */
  suppress: Record<Weapon, [number, number]> & { half: [number, number, number]; down: number; heavy: number; keep: number };
  rapid: number;
  lift: { gun: number; mortar: number; mg: number; deep: number };
  mission: { call: [number, number]; range: number; salvos: [number, number]; every: [number, number]; quiet: [number, number]; again: number; mortar: number };
  tank: { every: [number, number] };
  tankMg: { kill: { open: number; cover: number }; roof: string[]; roofShare: number; hullArc: number; commander: number; splinter: number };
  hold: { mg: number; rifle: [number, number, number] };
  counter: { at: number; ahead: number; xp: [number, number, number] };
  flank: { rush: [number, number]; out: number };
  /** per, most (smoke on call, release 2, ruling 242 S6): a screen the rules laid is drawn as a round each 'per' metres of its line, at most 'most'. */
  smoke: { on: boolean; every: number; spread: number; per: number; most: number };
  aside: [number, number, number];
}

export const BOOK = (data as unknown as { book: BookData }).book;

/** A nation's school (the default for any nation not named). */
export function schoolOf(nat: string): School {
  return BOOK.nations[nat] ?? '_';
}
export function schoolNums(nat: string): SchoolNums {
  return BOOK.schools[schoolOf(nat)] ?? BOOK.schools._;
}

/** The bound an army fights by at its experience (band 0 raw, 1, 2 veteran), or a wave where its general's style is one. */
export function boundOf(nat: string, band: 0 | 1 | 2, wave: boolean): Bound {
  if (wave) return 'wave';
  const S = schoolNums(nat);
  return (band === 0 ? S.raw : band === 2 ? S.veteran : undefined) ?? S.bound;
}

/** The turns of a cycle (three; a wave has one). */
export const turns = (bound: Bound): number => (bound === 'wave' ? 1 : 3);

/**
 * Which turn of the fire-and-movement cycle a man rushes in (0, 1, 2), by his army's bound: whether he is a gunner, his
 * squad (sq: in a platoon 0 the leading one; for a section of no platoon, its model's number) and whether he is of the
 * platoon's headquarters (it goes with the leading squad).
 */
export function turnOf(bound: Bound, gun: boolean, sq: number, hq: boolean): number {
  const s = hq ? 0 : sq;
  switch (bound) {
    case 'wave': return 0;
    case 'body': case 'squad': return s % 3;
    case 'both': return (s + (gun ? 1 : 0)) % 3;
    default: return 0;
  }
}

/** A block's groups' suppression: each group's level, when they were last brought up to date, the last round seen, and
 * the rounds seen still in the air. */
interface Felt { lv: Map<number, number>; at: number; seen: number; air: { l: number; w: Weapon; x: number; y: number }[]; band: 0 | 1 | 2 }

/**
 * Suppression (principle 4): every round that lands near a group of men (a squad, a section, a gun's crew) adds to that
 * group's suppression, the more the heavier the round; it halves every BOOK.suppress.half seconds (raw men stay down
 * longer, veterans are up sooner). Above 'down' its men keep their heads down and its rushes wait, but for its gunner and
 * a few who keep firing (keep); above 'heavy' those in the open crawl for cover.
 * Only the drill's rounds count (its own dice): looks only.
 */
export class Suppression {
  private readonly felt = new Map<number, Felt>();

  /** A group's suppression now. */
  level(block: number, g: number, t: number): number {
    const f = this.felt.get(block);
    if (!f) return 0;
    const v = f.lv.get(g);
    return v ? v * 0.5 ** ((t - f.at) / BOOK.suppress.half[f.band]) : 0;
  }

  /**
   * Brings a block's groups up to date at time t: decays every level, then adds each round fired at the block since
   * the last look (numbered n, landed by t) to every group whose middle lies within the round's reach of where it fell.
   */
  feel(block: number, t: number, band: 0 | 1 | 2, rounds: readonly { n: number; on: number; l: number; w: Weapon; x: number; y: number }[], groups: Map<number, [number, number]>): void {
    let f = this.felt.get(block);
    if (!f) this.felt.set(block, (f = { lv: new Map(), at: t, seen: 0, air: [], band }));
    const S = BOOK.suppress, k = 0.5 ** ((t - f.at) / S.half[band]);
    f.band = band;
    for (const [g, v] of f.lv) { const w = v * k; if (w < 0.01) f.lv.delete(g); else f.lv.set(g, w); }
    f.at = t;
    // The rounds fired at it since the last look join those still in the air; each counts once it has landed.
    for (const r of rounds) if (r.n > f.seen) { f.seen = r.n; if (r.on === block) f.air.push({ l: r.l, w: r.w, x: r.x, y: r.y }); }
    if (!f.air.length) return;
    const left: Felt['air'] = [];
    for (const r of f.air) {
      if (r.l > t) { left.push(r); continue; }
      const by = S[r.w];
      if (!by) continue; // smoke: no harm
      const [reach, add] = by;
      for (const [g, [x, y]] of groups) {
        const d = Math.hypot(x - r.x, y - r.y);
        if (d < reach) f.lv.set(g, (f.lv.get(g) ?? 0) + add * (1 - (0.5 * d) / reach));
      }
    }
    f.air = left;
  }
}

/**
 * A battery's or a mortar team's shoot at one target, its spotter calling it on the radio (the general's section 5): the
 * call; one ranging round over the target; a call; a second short of it (the bracket); a call; fire for effect, every
 * gun a salvo, 3 or 4 of them a few seconds apart; quiet; then a short call and fire for effect again, no ranging. A new
 * target starts it afresh. stage: 0 calling for the first ranging round, 1 for the second, 2 for fire for effect, 3 in
 * fire for effect (left: its salvos to come), 4 quiet. call: when the spotter's signaller began his latest call.
 */
export interface Mission { on: number; stage: 0 | 1 | 2 | 3 | 4; next: number; left: number; call: number }
/** What a mission fires at a moment: nothing, one ranging round (1 over the target, -1 short of it), or a salvo. */
export type Shoot = 0 | 'over' | 'short' | 'salvo';

const span = (a: [number, number], r: number): number => a[0] + (a[1] - a[0]) * r;

/**
 * Steps a mission at time t against target 'on': what fires now. k is the share of the times (a mortar team's are
 * quicker: BOOK.mission.mortar); r(i) a roll of the battery's own dice for this moment (looks only).
 */
export function missionStep(ms: Mission | undefined, on: number, t: number, k: number, r: (i: number) => number): [Mission, Shoot] {
  const M = BOOK.mission;
  if (!ms || ms.on !== on) return [{ on, stage: 0, next: t + k * span(M.call, r(1)), left: 0, call: t }, 0];
  if (t < ms.next) return [ms, 0];
  switch (ms.stage) {
    case 0: return [{ ...ms, stage: 1, next: t + k * span(M.call, r(2)), call: t }, 'over'];
    case 1: return [{ ...ms, stage: 2, next: t + k * span(M.call, r(3)), call: t }, 'short'];
    case 2: return [{ ...ms, stage: 3, left: Math.round(span(M.salvos, r(4))) - 1, next: t + span(M.every, r(5)) }, 'salvo'];
    case 3:
      if (ms.left > 0) return [{ ...ms, left: ms.left - 1, next: t + span(M.every, r(6)) }, 'salvo'];
      return [{ ...ms, stage: 4, next: t + k * span(M.quiet, r(7)) }, 0];
    default: return [{ ...ms, stage: 2, next: t + k * M.again, call: t }, 0];
  }
}
