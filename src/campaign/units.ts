// Units decide (release 2; Brian 23 Nov 2026 07:11Z and 07:13Z, the core theme: "It's about the units and you build
// and attack with not historical handcuffs that determine battles"; the design director's audit of 07:17Z,
// design-director/units-decide-audit-1123.md). One switch, built in steps on it: each step turns a constant keyed to a
// nation or a date into a cause any nation can build, research, train or lose. The record only sets how strong each
// cause is, and the history start's levels.
//
// Set at a grand campaign's start only (g.flags.unitsDecide), never in one under way, and never in a history
// campaign. Off, nothing behind it is read and every war is the war without it.
//
// Steps on it so far: the convoy war by cause (audit #5, navy.ts 'The convoy war by cause'); the honest win chance on
// land (audit #1, game.ts winChanceOn), on its own sub-flag 'winChance' as well (stage.json), off until the 3D field's
// spread matches the card's; in a day fight of fighters alone, height by the defenders' warning in place of the
// attacker's flat escort share (air.ts assessAir, a.height; air-battles/dogfight/sweep-height-form-1124.md); the
// quality store below, one store for soldiers, pilots and crews (the design director's ruling of 24 Nov 15:53Z on the
// rules builder's #2 form, rules-builder/release2/soviet/form.md, Part 2).

import type { Air, Arms, BattleReport, Corps, Game, Land, LandWay, Nation, QualityStore, Sea, Way } from './types';
import unitsData from '../../data/campaign/units.json';
import leanData from '../../data/battle/lean.json';
import airData from '../../data/campaign/air.json';
import carrierData from '../../data/campaign/carriers.json';
import navyData from '../../data/campaign/navy.json';
import techData from '../../data/campaign/tech.json';

/** True where this campaign was opened with Units decide: a grand campaign whose start set the flag. */
export const unitsDecide = (g: Game): boolean => g.flags.open === 1 && g.flags.unitsDecide === 1;

/**
 * Sets the switch. Only at setup, before the first month has been played; returns whether it was set. Set on a campaign
 * whose research has already opened (startOpen ran without it), it lays the switch's own start list as startOpen would
 * have (units.json startRows: Germany's Field radios, Sweden's and Norway's winter kit; DD 24 Nov 15:53Z, #2 form 'Where
 * it goes'), and seeds the quality store again from those rows, so such a campaign starts as one opened with the switch
 * and never falls back (#2 form Part 2 section 4). A team already set to one of those rows is freed, and its staff sets
 * it to work again at its next month. Set off, the store goes with it; the rows stay, as the form allows for a flag
 * cleared by hand.
 */
export function setUnitsDecide(g: Game, on: boolean): boolean {
  if (g.turn !== 1 || g.phase !== 0 || g.airReports.length || g.reports.length) return false;
  delete g.qs;
  if (!on) {
    delete g.flags.unitsDecide;
    return true;
  }
  g.flags.unitsDecide = 1;
  // The same list as tech.ts openUnitsRows lays in startOpen. Research has opened, so every nation already has its
  // tree, and there is nothing to create here.
  if (g.flags['tech:open']) {
    for (const [nat, ids] of Object.entries(UNITS.startRows)) {
      const s = g.nations[nat]?.tech;
      if (!s) continue;
      for (const id of ids) {
        if (!s.done.includes(id)) s.done.push(id);
        s.work = s.work.filter((w) => w.id !== id);
      }
    }
  }
  qualityStore(g);
  return true;
}

// ---- The quality store (DD 24 Nov 15:53Z, #2 form Part 2) --------------------------------------------------------
// The nation holds what its schools and staffs know; the unit holds what its own men have lived through. This is the
// store and its seeds only: what reads each slot, and what moves it, comes in the steps that build those rules. Every
// figure is in data/campaign/units.json, which is read only under the switch; no nation is named here.

/** A table of seeds by nation, with '_default' for every nation not listed. */
type ByNat<T> = Record<string, T>;
/** Seeds by the month a campaign opened (grand.json's starts: 1, 5, 17, 29); a later start takes the row of the latest start at or before it. */
type ByStart<T> = Record<string, ByNat<T>>;

/** A reform whose text is told when a cause of the army it names passes a mark (units.json land.reforms.news). */
export interface ReformNews { event: string; cause: 'command' | 'xp'; above?: number; below?: number }

/** data/campaign/units.json: the store's seeds and every figure the land rules of the #2 form read. */
export interface UnitsData {
  startRows: Record<string, string[]>;
  land: {
    ladder: number[];
    trainRows: string[];
    radios: number;
    radiosRow: string;
    earned: { seed: ByNat<number>; month: number; provinces: number; corps: number };
    attack: { base: number; perC: number };
    mostCorps: { base: number; perC: number };
    gates: { renewal: number; blitz: number };
    blitzTraits: string[];
    reforms: { land: string[]; news: ReformNews[] };
    sight: { from: number; span: number };
    byOrders: { need: number; stop: number; main: number };
    card: CardWords;
    xp: { won: number; repulsed: number; held: number; beaten: number };
    label: { share: number; cap: number };
    guards: { xp: number; taken: number; elite: string[] };
    raisedBy: Record<string, string>;
    antiTank: { rows: string[]; years: number[] };
    ways: LandWay[];
    school: number;
  };
  trainIn: number;
  air: { scale: unknown; school: ByStart<number>; deck: ByStart<number>; fighter: string[]; escort: string[] };
  sea: { ways: Sea['way']['id'][]; packs: Sea['pack']['id'][]; pack: ByStart<Sea['pack']['id']>; targets: Sea['target']['id'][]; target: ByStart<Sea['target']['id']> };
}
export const UNITS = unitsData as unknown as UnitsData;

type Lean = string | { att: string; def: string };
const LEAN = leanData as unknown as { lean: Record<string, { land: Lean; sea: string; air: string }>; standIn: Record<string, string> };
const AIR_NATIONS = (airData as unknown as { nations: Record<string, { quality: number }> }).nations;
const DECK_STRIKE = (carrierData as unknown as { naval: { land: Record<string, number> } }).naval.land;
const NAVY_QUALITY = (navyData as unknown as { quality: Record<string, number> }).quality;

/** The row of a by-start table for a campaign opened in month `from`: the latest start at or before it. */
function atStart<T>(t: ByStart<T>, from: number): ByNat<T> {
  let at = -Infinity, out: ByNat<T> = {};
  for (const k of Object.keys(t)) if (Number(k) <= from && Number(k) > at) { at = Number(k); out = t[k]; }
  return out;
}
const byNat = <T>(t: ByNat<T>, nat: string): T | undefined => t[nat] ?? t._default;
const seeded = <T extends string>(id: T): Way<T> => ({ id, by: 'start' });

/**
 * A nation's army at the start (Part 2 section 2). train: its quality as the campaign opened, one rule over every
 * source of it (nations.json, the wider world's tables, world.ts's default). earned: the start's command by army, as
 * ruled at DD 24 Nov 16:06Z on the historian's reading of 1939-41 (units.json earned.seed: Germany 1, the Soviet Union
 * and France 0, Poland 0.1, Italy 0.25, Japan, Britain and the United States 0.5, and every army not yet read the range
 * middle, 0.5), less the radios row's share where the row is held at the start, so the start's c is the seed's
 * (Germany 0.85 + 0.15). school: 0. way: lean.json's land column, by side.
 */
function landSeed(n: Nation): Land {
  const L = UNITS.land;
  const radios = n.tech?.done.includes(L.radiosRow) ? L.radios : 0;
  const lean = (LEAN.lean[n.id] ?? LEAN.lean._).land;
  const [att, def] = (typeof lean === 'string' ? [lean, lean] : [lean.att, lean.def]) as [LandWay, LandWay];
  return { train: n.quality, earned: Math.max(0, (byNat(L.earned.seed, n.id) ?? 0) - radios), school: L.school, way: { att: seeded(att), def: seeded(def) } };
}

/**
 * A nation's air arm at the start (Part 2 section 2). Pilot hours by start month where the historian has read them,
 * else null (not laid); held is today's air quality (air.json), strike the land bombers' training against ships
 * (carriers.json). Nothing reads these slots yet.
 */
function airSeed(nat: string, from: number): Air {
  const A = UNITS.air;
  const school = byNat(atStart(A.school, from), nat) ?? null;
  return {
    school,
    deck: byNat(atStart(A.deck, from), nat) ?? null,
    held: (AIR_NATIONS[nat] ?? AIR_NATIONS._default).quality,
    pool: school,
    poolXp: 0,
    strike: DECK_STRIKE[nat] ?? DECK_STRIKE._default,
    fighter: null,
    escort: null,
  };
}

/**
 * A nation's navy at the start (Part 2 section 2). crew: navy.json's quality stands in until the record per navy is
 * read; the way from lean.json's sea column (a way the sea has not got, night, as its stand-in); convoy doctrine by
 * start month. Nothing reads these slots yet.
 */
function seaSeed(nat: string, from: number): Sea {
  const S = UNITS.sea;
  const lean = (LEAN.lean[nat] ?? LEAN.lean._).sea;
  const way = (S.ways as string[]).includes(lean) ? lean : LEAN.standIn[lean];
  return {
    crew: NAVY_QUALITY[nat] ?? NAVY_QUALITY._default,
    night: null,
    lookout: null,
    way: seeded(way as Sea['way']['id']),
    pack: seeded(byNat(atStart(S.pack, from), nat)!),
    target: seeded(byNat(atStart(S.target, from), nat)!),
  };
}

/** One nation's three arms, by the one rule. */
const armsSeed = (n: Nation, from: number): Arms => ({ land: landSeed(n), air: airSeed(n.id, from), sea: seaSeed(n.id, from) });

/** The store for this campaign, every nation in g.nations seeded by one rule, from its start's tables. */
function seedQuality(g: Game): QualityStore {
  const from = g.flags.openFrom ?? 1;
  const nat: Record<string, Arms> = {};
  for (const n of Object.values(g.nations)) nat[n.id] = armsSeed(n, from);
  return { from, nat, news: {} };
}

/**
 * The quality store: every reader goes through this. Under Units decide only; off it is undefined and nothing is
 * written (#2 form Part 2 section 4). Under the switch a missing store is seeded on first use, from its start's tables,
 * so a flag set by hand or by setUnitsDecide never falls back to the old national constants.
 */
export function qualityStore(g: Game): QualityStore | undefined {
  if (!unitsDecide(g)) return undefined;
  return (g.qs ??= seedQuality(g));
}

/** One nation's arms in the store, under the switch only; a nation that came into the game after the store was seeded is seeded by the same rule. */
export function armsOf(g: Game, nat: string): Arms | undefined {
  const qs = qualityStore(g), n = g.nations[nat];
  if (!qs || !n) return undefined;
  return (qs.nat[nat] ??= armsSeed(n, qs.from));
}

/**
 * Seeds the store as a grand campaign opens with Units decide (staff.ts startOpen): once, after the wider world, the
 * opening research, the world's air commands and the carriers are in place, so each nation's quality and the rows it
 * holds are those of the start. Off, nothing.
 */
export function openQuality(g: Game): void {
  qualityStore(g);
}

// ---- Training and command (DD 24 Nov 15:53Z, #2 form Part 1 2a, 2b and section 1) ----------------------------------
// Each is worked out from the store and the rows the nation holds, never stored. With the switch off every reader
// below gives today's figure from the nation's own row (quality, slowCommand, maxAttack, blitz); with it on, none of
// those is read past the seed, and one rule holds for every nation.

/** Numbers built up by steps of 0.01 are read against a mark with this much room, so 0.5 reached by fifty steps is 0.5. */
const EPS = 1e-9;

const holdsRow = (n: Nation | undefined, id: string): boolean => !!n?.tech?.done.includes(id);

/**
 * The training a nation's troops fight with, read at corpsBase, the card's Troops line and the town guard. Off, its
 * quality, as today. Under Units decide, training is the ladder (DD 24 Nov 15:53Z (4)): the higher of the army's
 * start seed (land.train) and the ladder's rung for one, two or three of the training rows held (0.85, 1.05, 1.25),
 * the same ladder for every nation. A start never fixes a ceiling, and an army that began better trained keeps its
 * own level.
 */
export function armyTraining(g: Game, nat: string): number {
  if (!unitsDecide(g)) return g.nations[nat].quality;
  const L = UNITS.land, n = g.nations[nat], land = armsOf(g, nat)!.land;
  let rows = 0;
  for (const id of L.trainRows) if (holdsRow(n, id)) rows++;
  return rows ? Math.max(land.train, L.ladder[Math.min(rows, L.ladder.length) - 1]) : land.train;
}

/**
 * An army's command c, 0 to 1, under Units decide only (#2 form Part 1 2b): Field radios' share if the row is held, plus
 * the command it has earned in war, at most 1. Off there is none: 1, and nothing reads it there.
 */
export function commandOf(g: Game, nat: string): number {
  const land = armsOf(g, nat)?.land;
  if (!land) return 1;
  const L = UNITS.land;
  return Math.min(1, (holdsRow(g.nations[nat], L.radiosRow) ? L.radios : 0) + land.earned);
}

/**
 * What its command does to an army's attack. Off, a slow command's x0.85 (nations.json slowCommand), as today. Under
 * Units decide, x(0.85 + 0.15c) for every nation (#2 form Part 1 2b): today's slow command at c 0, nothing at c 1.
 */
export function attackCommand(g: Game, nat: string): number {
  if (!unitsDecide(g)) return g.nations[nat].slowCommand ? 0.85 : 1;
  const A = UNITS.land.attack;
  return A.base + A.perC * commandOf(g, nat);
}

/**
 * The most corps a nation's staffs bring into one battle, or 0 for no limit. Off, nations.json maxAttack, as today.
 * Under Units decide, 8 + 12c rounded down for every nation (#2 form Part 1 2b; choice 11): 8 at c 0, 14 at c 0.5, 20
 * at c 1.
 */
export function mostCorps(g: Game, nat: string): number {
  if (!unitsDecide(g)) return g.nations[nat].maxAttack;
  const M = UNITS.land.mostCorps;
  return Math.floor(M.base + M.perC * commandOf(g, nat) + EPS);
}

/**
 * True where a nation's staffs keep a slow command's habits: no failed attack renewed the same month, a month to answer
 * ground just lost, no gap in the line seen in time, no troops sent by rail. Off, nations.json slowCommand, as today.
 * Under Units decide, while command c is under 0.3 (#2 form Part 1 2b), for every nation.
 */
export function slowHabits(g: Game, nat: string): boolean {
  if (!unitsDecide(g)) return g.nations[nat].slowCommand;
  return commandOf(g, nat) < UNITS.land.gates.renewal - EPS;
}

/**
 * The blitz gate: the fresh offensive's tempo (x1.2 for fast troops and one more move), the deeper opening thrust, the
 * surprise of a new offensive and the armoured spearheads' attack. Off, nations.json blitz (Germany's), as today.
 * Under Units decide, a nation that holds Field radios at command c 0.6 or more (#2 form Part 1 section 1), whoever it
 * is: no nation's own tempo or spearheads row is read (Part 2 section 4 'Inert'). Surprise reads this gate until the
 * field spread form replaces it.
 */
export function blitzGate(g: Game, nat: string): boolean {
  const n = g.nations[nat];
  if (!unitsDecide(g)) return !!n?.blitz;
  if (!n || !holdsRow(n, UNITS.land.radiosRow)) return false;
  return commandOf(g, nat) >= UNITS.land.gates.blitz - EPS;
}

/**
 * Moves an army's earned command by `by`, kept between 0 and 1 and on a clean step of a millionth, under Units decide
 * only (#2 form Part 1 2b). Returns the new figure, or undefined off.
 */
export function earnCommand(g: Game, nat: string, by: number): number | undefined {
  const land = armsOf(g, nat)?.land;
  if (!land) return undefined;
  land.earned = Math.min(1, Math.max(0, Math.round((land.earned + by) * 1e6) / 1e6));
  return land.earned;
}

/**
 * Under Units decide, what a reform on history's calendar (diplomacy.json 'reform') still does to a nation: every part
 * but the army's own (units.json land.reforms.land: training, a slow command, the most corps), or null where nothing
 * is left (#2 form Part 1 section 1; choice 8). The air parts stay as they are, for the air thread.
 */
export function reformLeft<T extends object>(to: T): Partial<T> | null {
  const out: Partial<T> = {};
  let any = false;
  for (const [k, v] of Object.entries(to)) {
    if (UNITS.land.reforms.land.includes(k)) continue;
    (out as Record<string, unknown>)[k] = v;
    any = true;
  }
  return any ? out : null;
}

/** True where a reform's text is told by a cause passing its mark under Units decide, never by the calendar (units.json land.reforms.news). */
export const toldByCause = (id: string): boolean => UNITS.land.reforms.news.some((r) => r.event === id);

// ---- Experience, schools and titles (DD 24 Nov 15:53Z, #2 form Part 1 2d and section 1; Part 2 sections 2 and 4) ----
// What a corps' own men have lived through is the corps' xp; what its nation's schools know is land.school, the level
// new men join at. Each is priced once: xp in corpsBase's 0.85 + 0.4 xp, the schools only through the xp new men bring.

/**
 * A corps' experience after a battle under Units decide, by its result (#2 form Part 1 2d; units.json land.xp), in
 * place of today's flat +0.05 attacking and +0.04 defending, win or lose: the attacker +0.05 (1 - xp) if it carried the
 * day and +0.02 (1 - xp) if it was repulsed; the defender +0.04 (1 - xp) if it held and +0.02 (1 - xp) if it was beaten.
 * A corps learns most from a fight that goes its way, and less the more it knows already. `carried`: this side won.
 */
export function xpByResult(xp: number, side: 'att' | 'def', carried: boolean): number {
  const X = UNITS.land.xp;
  const gain = side === 'att' ? (carried ? X.won : X.repulsed) : carried ? X.held : X.beaten;
  return xp + gain * (1 - xp);
}

/**
 * The veteran cadres at a nation's drafts' schools: the count its drafts' training and its new corps' experience read
 * (stories.ts training, raisedXp). Under Units decide the store's land.school, for every nation (#2 form Part 2
 * section 2); off g.stories.schools, as today.
 */
export function schoolsOf(g: Game, nat: string): number {
  const land = armsOf(g, nat)?.land;
  return land ? land.school : g.stories.schools[nat] ?? 0;
}

/**
 * Cadres sent home to a nation's schools as instructors, at most `most` in all. Under Units decide they go into the
 * store's land.school (#2 form Part 2 section 2: a schools order raises land.school); off into g.stories.schools, as
 * today.
 */
export function addSchools(g: Game, nat: string, by: number, most: number): void {
  const land = armsOf(g, nat)?.land;
  if (land) land.school = Math.min(most, land.school + by);
  else g.stories.schools[nat] = Math.min(most, (g.stories.schools[nat] ?? 0) + by);
}

/**
 * True where the land's drafts' schools take a schools order from a unit of this arm. Off, every arm's, as today. Under
 * Units decide, never an airman's: his school step raises the air arm's school (air.school), never the land's (#2 form
 * Part 2 section 6, confirmed by the air thread at 15:55Z), and that step is not laid yet (U), so an airman sent to
 * the schools brings his trained pilots and nothing to the army's drafts.
 */
export const landSchools = (g: Game, kind: 'corps' | 'air' | 'fleet'): boolean => !unitsDecide(g) || kind !== 'air';

/**
 * The Guards title under Units decide (#2 form Part 1 section 1, Part 2 section 4; choice 13): every corps in `cs`, of
 * any nation, at experience 0.5 or more with three provinces taken (its record's 'taken': attacks won and ground walked
 * into after the defenders gave way, never defences held; the form's 'attack wins only' is not what that counter counts)
 * takes it, the month it is earned, in its own field. It is a name only: its card and its name read it, and
 * nothing else does. Never c.elite, which edge(), eliteCount, fold and the cadres read, since corpsBase's 0.85 + 0.4 xp
 * already prices that experience. Off, nothing is written.
 */
export function earnGuards(g: Game, cs: readonly Corps[]): void {
  if (!unitsDecide(g)) return;
  const G = UNITS.land.guards;
  for (const c of cs) if (!c.dead && c.guards === undefined && c.xp >= G.xp - EPS && (c.rec?.taken ?? 0) >= G.taken) c.guards = g.turn;
}

/**
 * True for a special formation (a traits.json unit id) whose title is earned in battle under Units decide and never
 * bought: no nation orders it and it adds nothing in battle (units.json land.guards.elite: the Guards corps). Off,
 * none.
 */
export const titleEarned = (g: Game, id: string | undefined): boolean => !!id && unitsDecide(g) && UNITS.land.guards.elite.includes(id);

/**
 * The research row that raises a special formation under Units decide, in place of the first year it could be raised
 * (units.json land.raisedBy: the heavy tank corps needs Heavy tanks; #2 form Part 1 section 1), or undefined. Off,
 * none: the year, as today.
 */
export const raisedBy = (g: Game, id: string): string | undefined => (unitsDecide(g) ? UNITS.land.raisedBy[id] : undefined);

// ---- The capture label (DD 24 Nov 15:53Z, #2 form Part 1 section 7; choice 7) --------------------------------------
// Of the defence's men lost in a fight, a share is booked as prisoners by the worth of the two sides' men: 0.148 x q,
// q the attack's per-man attack worth over the defence's per-man defence worth, at most 0.9. One number, fitted to the
// record's fail-or-advance pair at Kursk; the worth ratio, a cause, carries the difference between the sides. It is a
// split of men already lost: it adds none, and the men taken in the pursuit, cut off or in the town stay in the
// report's prisoners. When the field spread's capture ladder lands, that form decides whether the label becomes its
// first rung or goes; it is never a second price on top of the ladder.

/**
 * The defence's men lost in a fight (`lost`, the report's defLost) that the capture label books as prisoners:
 * round(lost x min(0.9, 0.148 x q)) (units.json land.label), never more than were lost. Read once from the final men
 * lost, after the field's bill where the battle was fought on the field, so a battle the card settles and one the field
 * hands back are booked by this one rule (#2 form Part 1 section 6). Under Units decide only; off, or with no q, none.
 */
export function capturedOf(g: Game, q: number | undefined, lost: number): number | undefined {
  if (!unitsDecide(g) || q === undefined || !(q >= 0)) return undefined;
  const C = UNITS.land.label;
  return Math.max(0, Math.min(lost, Math.round(lost * Math.min(C.cap, C.share * q))));
}

/**
 * Every prisoner a battle took, as its cards say it: those taken in the pursuit, with no way out or in the town
 * (prisoners), and those of the fight's own losses the capture label booked (captured, Units decide only). The cards'
 * count of men lost never reads this: the label's prisoners are already in defLost, so the loss stays defLost +
 * prisoners (monthcard.ts). Off, the report's prisoners, as today. One helper for the campaign's report card, the month
 * card and the field's after card.
 */
export const prisonersOf = (r: Pick<BattleReport, 'prisoners' | 'captured'>): number => r.prisoners + (r.captured ?? 0);

// ---- The staff's sight and way, and the card (DD 24 Nov 15:53Z, #2 form Part 1 section 5 and 'On the card'; rulings
// (1) and (14) amended) -------------------------------------------------------------------------------------------------
// A staff reads the card's odds through its sight, which comes from its command: at sight 1 it reads the card, as
// today's staff does; at 0 it counts men, kinds and ground, and leaves out the troops' worth and the enemy's works,
// since reading the enemy takes reconnaissance and signals, the same cause as command. The way 'mass', by orders, sets
// the odds a staff asks: ordinary attacks go in at seen 0.9 and stop adding corps at seen 1.3, the main effort's at 2.6.
// The player's card shows the odds as the player's own staff reads them, with the cause named; a computer staff's
// misread is never shown. Off, every staff reads the card and asks today's odds, and the card is today's.

/**
 * A nation's staff's sight, 0 to 1, from its command c: clamp((c - 0.6) / 0.4, 0, 1) (units.json land.sight; #2 form
 * Part 1 section 5), for every nation. Off, 1: every staff reads the card.
 */
export function staffSight(g: Game, nat: string): number {
  if (!unitsDecide(g)) return 1;
  const S = UNITS.land.sight, s = (commandOf(g, nat) - S.from) / S.span;
  return s >= 1 - EPS ? 1 : s <= EPS ? 0 : s;
}

/**
 * The odds a staff believes an attack has (#2 form Part 1 section 5): the card's odds x (q / wk)^(s - 1), s the
 * attacking staff's sight, q the attack's per-man attack worth over the defence's per-man defence worth and wk the
 * works' multiplier on the defence (game.ts assess, Assessment.read). At sight 1 they are the card's odds exactly; at
 * 0 the staff counts men, kinds and ground, not worth or works. The one helper every reading of the card's odds by a
 * computer staff goes through (ai.ts offensive, twoWaves and localAttacks; staff.ts sector), and the player's card
 * (turn.ts attackOutlook). Off, or for guns overrun, the card's odds.
 */
export function seenOdds(g: Game, a: { odds: number; read?: { nat: string; q: number; wk: number } }): number {
  const r = a.read;
  if (!r || !unitsDecide(g)) return a.odds;
  const s = staffSight(g, r.nat);
  return s >= 1 ? a.odds : a.odds * Math.pow(r.q / r.wk, s - 1);
}

/** A staff of the way 'mass', by orders (units.json land.byOrders), as its attacking way: the rest keep today's odds. */
const byOrders = (g: Game, nat: string): boolean => armsOf(g, nat)?.land.way.att.id === 'mass';

/**
 * Where a computer staff stops adding corps to an attack, in the odds it sees. Off, and for the ways methodical, bold
 * and night, `today` (the staff's 2.6). By orders (#2 form Part 1 section 5; ruling (1)): 1.3 for an ordinary attack,
 * blending to 2.6 as its sight rises; 2.6 in the main effort's sector (`main`), whatever its sight.
 */
export function stopAt(g: Game, nat: string, today: number, main: boolean): number {
  if (!unitsDecide(g) || !byOrders(g, nat)) return today;
  const B = UNITS.land.byOrders, s = staffSight(g, nat);
  return main ? B.main : s >= 1 ? today : B.stop + (B.main - B.stop) * s;
}

/**
 * The odds a computer staff asks before an attack goes in, in the odds it sees. Off, and for the ways methodical, bold
 * and night, `today` (the temper's minOdds as the plan gives it, with what today's staff adds for a line dug in and
 * fed). By orders (#2 form Part 1 section 5): 0.9, blending to `today` as its sight rises, so at sight 1 it is today's.
 */
export function needAt(g: Game, nat: string, today: number): number {
  if (!unitsDecide(g) || !byOrders(g, nat)) return today;
  const B = UNITS.land.byOrders, s = staffSight(g, nat);
  return s >= 1 ? today : B.need + (today - B.need) * s;
}

// The attack card's lines on the causes (#2 form 'On the card'; Part 2 section 5): words from units.json land.card.

/** The words of the card's cause lines (units.json land.card). */
export interface CardWords {
  training: { below: [number, string][]; top: string; seed: string };
  command: { below: [number, string][]; top: string; radios: [string, string]; carried: [string, string, string] };
  commandEffect: string; attackDown: string; attackWhole: string;
  ways: Record<LandWay, string>;
  sight: { none: string; part: string };
}
const CARD = UNITS.land.card;
const TREE = (techData as unknown as { techs: { id: string; name: string }[] }).techs;
/** A research row's name as running text: 'Battle drill' is 'battle drill', 'NCO schools' keeps its capitals. */
const rowName = (id: string): string => {
  const n = TREE.find((t) => t.id === id)?.name ?? id;
  return n.length > 1 && n[1] === n[1].toLowerCase() ? n[0].toLowerCase() + n.slice(1) : n;
};
const wordAt = (x: number, w: { below: [number, string][]; top: string }): string => w.below.find(([m]) => x < m - EPS)?.[1] ?? w.top;
const fill = (t: string, by: Record<string, string | number>): string => t.replace(/\{(\w+)\}/g, (_, k: string) => String(by[k] ?? ''));
const upFirst = (t: string): string => t[0].toUpperCase() + t.slice(1);

/**
 * An army's training as the card says it: its word, then its cause: the army as it went to war where its own start
 * sets the level, and the training rows it holds. Under Units decide only.
 */
export function trainingWords(g: Game, nat: string): string {
  const L = UNITS.land, land = armsOf(g, nat)!.land, n = g.nations[nat];
  const rows = L.trainRows.filter((id) => holdsRow(n, id));
  const rung = rows.length ? L.ladder[Math.min(rows.length, L.ladder.length) - 1] : -Infinity;
  const why = [...(land.train >= rung ? [CARD.training.seed] : []), ...rows.map(rowName)];
  return `${wordAt(armyTraining(g, nat), CARD.training)} (${why.join(', ')})`;
}

/** An army's command as the card says it: its word, then Field radios held or not and the offensives carried through. Under Units decide only. */
export function commandWords(g: Game, nat: string): string {
  const C = CARD.command, land = armsOf(g, nat)!.land, k = land.carried ?? 0;
  const radios = C.radios[holdsRow(g.nations[nat], UNITS.land.radiosRow) ? 0 : 1];
  return `${wordAt(commandOf(g, nat), C)} (${radios}; ${fill(C.carried[Math.min(k, 2)], { n: k })})`;
}

/** A staff's way, attacking or defending, as the card names it. Under Units decide only. */
export const wayWords = (g: Game, nat: string, side: 'att' | 'def'): string => CARD.ways[armsOf(g, nat)!.land.way[side].id];

/**
 * The attack card's lines on the causes each side's staff reads (#2 form 'On the card'; Part 2 section 5): training,
 * each side's; command, the attacker's, which prices the attack alone, with what it does to this attack and what moves
 * it; and each side's staff doctrine, the attacker's attacking way against the defender's defending one. Never a flag
 * or a date, and never a computer staff's misread. Under Units decide only; off, none.
 */
export function causeLines(g: Game, att: string, def: string): { label: string; value: string; effect?: string }[] {
  if (!armsOf(g, att) || !armsOf(g, def)) return [];
  const pct = Math.round((1 - attackCommand(g, att)) * 100);
  return [
    { label: 'Training', value: `${upFirst(trainingWords(g, att))} against ${trainingWords(g, def)}` },
    { label: 'Command', value: upFirst(commandWords(g, att)), effect: fill(CARD.commandEffect, { attack: pct > 0 ? fill(CARD.attackDown, { pct }) : CARD.attackWhole, most: mostCorps(g, att) }) },
    { label: 'Staff doctrine', value: `${upFirst(wayWords(g, att, 'att'))} against ${wayWords(g, def, 'def')}` },
  ];
}

/**
 * What the player's card says where the player's own staff reads the odds at a sight under 1, naming the cause, its
 * command (ruling (14) amended); null at sight 1, where the card is today's, and off.
 */
export function sightWords(g: Game, nat: string): string | null {
  if (!armsOf(g, nat)) return null;
  const s = staffSight(g, nat);
  return s >= 1 ? null : fill(s > 0 ? CARD.sight.part : CARD.sight.none, { command: commandWords(g, nat) });
}
