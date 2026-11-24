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

import type { Air, Arms, Game, Land, LandWay, Nation, QualityStore, Sea, Way } from './types';
import unitsData from '../../data/campaign/units.json';
import leanData from '../../data/battle/lean.json';
import airData from '../../data/campaign/air.json';
import carrierData from '../../data/campaign/carriers.json';
import navyData from '../../data/campaign/navy.json';

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
    sight: { from: number; span: number };
    byOrders: { need: number; stop: number; main: number };
    xp: { won: number; repulsed: number; held: number; beaten: number };
    label: { share: number; cap: number };
    guards: { xp: number; won: number };
    ways: LandWay[];
    school: number;
  };
  trainIn: number;
  air: { scale: unknown; school: ByStart<number>; deck: ByStart<number>; fighter: string[]; escort: string[]; aceStep: number | null; head: number | null };
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

// ---- The air arm's school and reserve (the air thread's readers, #2 form Part 2 sections 2 and 6) -----------------

/**
 * Takes `n` pilots into a nation's reserve in the store: its mean hours (null while the school is) and mean experience
 * blend by count with the `was` already there. `hours` null brings no hours (a pilot from a school not laid).
 */
export function intoPool(A: Air, was: number, n: number, hours: number | null, fxp: number): void {
  if (n <= 0) return;
  const all = Math.max(0, was) + n, w = Math.max(0, was);
  A.pool = A.school === null ? null : w <= 0 || A.pool === null ? hours : hours === null ? null : (A.pool * w + hours * n) / all;
  A.poolXp = (A.poolXp * w + fxp * n) / all;
}

/**
 * New pilots straight from school into a nation's reserve, by count (the count is AirPool.pilots, which exists once
 * the nation has an air force). Under the switch they come at the school's hours with no experience.
 */
export function schoolPilots(g: Game, nat: string, n: number): void {
  const p = g.airPool[nat];
  if (!p || n <= 0) return;
  const A = armsOf(g, nat)?.air;
  if (A) intoPool(A, p.pilots, n, A.school, 0);
  p.pilots += n;
}

/**
 * An air ace home to train pilots (stories.ts cashHonour, valour.ts sendHero): under the switch it raises the nation's
 * flying school by one step, units.json air.aceStep, and not the land drafts' schools (#2 form Part 2 section 6). The
 * step is U: until it is read, the school stands. A school not laid stays not laid.
 */
export function aceSchool(g: Game, nat: string): void {
  const A = armsOf(g, nat)?.air, step = UNITS.air.aceStep;
  if (A && A.school !== null && step) A.school += step;
}

/** The way an arm fights now: a change ordered trains in until its month, then is the way (#2 form Part 2 section 3). */
export const wayNow = <T extends string>(g: Game, w: Way<T>): T => (w.next !== undefined && w.ready !== undefined && g.turn >= w.ready ? w.next : w.id);
