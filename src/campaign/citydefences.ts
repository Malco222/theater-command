// City defences (release 2; Brian 24 November 2026 22:08Z, card 22:11Z "Both"; city defences, the design director's
// ruling of 25 Nov 2026 02:23Z). One switch, 'cityDefences' (stage.json): searchlights to build beside the
// anti-aircraft guns, guns that fire by night by what they can see, a garrison that fortifies the town it holds, and
// the town's heavy guns firing on attacking tanks. Every figure is in data/campaign/citydefences.json; the work's own
// row (label, cost, months, upkeep, blurb) is arsenal.json works.lights. One rule for every nation: nothing here reads
// a nation or a date. Off: nothing of it is read, no flag is written and every war is the war without it.

import data from '../../data/campaign/citydefences.json';
import defence from '../../data/campaign/defence.json';
import { gunsAt } from './air';
import { AC } from './aircampaigns';
import { garrisonOf } from './siege';
import { hasTech } from './tech';
import type { Game, Region } from './types';
import type { Sky } from './weather';

/**
 * A row of the history start's table: searchlights laid where the historian reads a city belt at that date
 * (historian/start-searchlights-1125.md, section 2). `region` the map's own name; `turns` the start months the reading
 * holds for; `level` 1 to 3; `mark` the historian's mark (F firm, L likely, D derived, U uncertain); `lay` whether it
 * is laid (the design director's ruling of 25 Nov 2026 02:40Z: F, L and D rows are, and a U alone is a change that is
 * not, so the last laid level holds and is written as a row of its own); `why` the source in a line.
 */
export interface LightsRow { region: string; turns: number[]; level: number; mark: 'L' | 'D' | 'F' | 'U'; lay: boolean; why: string }

/**
 * A row of the history start's guns (city defences, the design director's ruling of 25 Nov 2026 04:10Z, "Start guns
 * from the record"; historian/start-guns-1125.md, section 2), in the lights' rows' shape: `level` 0 to 3 (0 a reading
 * of none), `mark` the historian's as he wrote it ("D, U", "L, then U", or G for a stand-in). A row not laid has no
 * level and changes nothing: it writes down that ruling 124's level stands there until read.
 */
export interface GunsRow { region: string; turns: number[]; level?: number; mark: string; lay: boolean; why: string }

export const CITYDEF = data as unknown as {
  /** The share of a built gun level's day worth by night: dark, radar, lit (no radar), both. */
  night: { dark: number; radar: number; lit: number; both: number };
  /** Bombers over the target one level of searchlights can hold. */
  saturation: number;
  /** The night fighters' floor by searchlights level, 0 to 3. */
  fighters: number[];
  works: { most: number; worth: number };
  ground: { armour: number; most: number; lost: number };
  ai: { months: number };
  start: LightsRow[];
  startGuns: GunsRow[];
};

/** The entrenching share by months done (defence.json entrench.share), read here as the town works' share. */
const ENTRENCH = (defence as unknown as { entrench: { share: number[] } }).entrench;

/** The top level of a works row, the anti-aircraft guns' and the searchlights' alike (arsenal.ts TOP, guard.ts GUARD_TOP). */
export const WORKS_TOP = 3;

/** The most levels of searchlights a province can have (the works' own top). */
export const LIGHTS_TOP = WORKS_TOP;

/** True where the city defences are in play: a game carrying the flag 'cityDefences'. Every new rule reads it first. */
export const cityDefences = (g: Game): boolean => g.flags.cityDefences === 1;

/** True where a grand campaign carries the city defences (stage.json 'cityDefences'), as settledTownsOn. */
export const cityDefencesOn = (g: Game): boolean => g.flags.open === 1 && cityDefences(g);

/**
 * Turns the city defences on or off in a campaign under way. What was built stays built either way: off, the
 * searchlights' marks stay on the ground and nothing reads them, offers them or bills them.
 */
export function setCityDefences(g: Game, on: boolean): void {
  if (!on) { delete g.flags.cityDefences; return; }
  g.flags.cityDefences = 1;
  // (The record's start, once, wherever the switch comes on: the 3D game's ship list right after the start or a load,
  // the Options row, TC.cityDefences. The design director's ruling of 25 Nov 2026 04:10Z.)
  cityStartLay(g);
}

/** The level of searchlights in a province, 0 to 3: always 0 with the switch off, whatever marks the ground keeps. */
export const lightsAt = (g: Game, region: number): number => (cityDefences(g) ? g.flags[`wk:lights:${region}`] ?? 0 : 0);

/**
 * The rows of a history start's table that stand at a start month: those of the latest start month the table lists
 * at or before it (a month between two listed ones takes the earlier reading), and laid. None before the first.
 * One lookup for every table of start rows read by the start month.
 */
export function startRowsAt<R extends { turns: number[]; lay: boolean }>(rows: R[], month: number): R[] {
  let at = -Infinity;
  for (const row of rows) for (const t of row.turns) if (t <= month && t > at) at = t;
  return at === -Infinity ? [] : rows.filter((row) => row.lay && row.turns.includes(at));
}

/**
 * The searchlights of the history start (city defences, the design director's ruling of 25 Nov 2026 02:23Z, 'Also',
 * and of 02:40Z on which rows are laid): with the switch on, searchlights where the historian reads a city belt at the
 * month the war began (openFrom, or the first month, the month the start guns read), province by name, at the level
 * he reads; none where he finds none. The rows say nothing of nations: whoever holds the province at that date has the
 * lights. A province keeps the higher level, as the start guns do (airversion.ts). Off: nothing. cityStartLay lays
 * them, with the guns; `rows` is for tests.
 */
export function openCityDefences(g: Game, rows: LightsRow[] = CITYDEF.start): void {
  if (!cityDefences(g)) return;
  for (const row of startRowsAt(rows, g.flags.openFrom ?? 1)) {
    const r = g.regions.find((x) => x.name === row.region);
    const lv = Math.max(0, Math.min(LIGHTS_TOP, Math.floor(row.level)));
    if (!r || lv <= 0) continue;
    g.flags[`wk:lights:${r.id}`] = Math.max(g.flags[`wk:lights:${r.id}`] ?? 0, lv);
  }
}

// The record's start laid, once: 'cds' = 1 in g.flags once cityStartLay has laid a campaign's start (written only
// with the switch on, in a grand campaign), so that turning the switch off and on again lays nothing more.
export const START_LAID = 'cds';

/**
 * The anti-aircraft guns of the history start from the record (city defences, the design director's ruling of 25 Nov
 * 2026 04:10Z, "Start guns from the record"): the laid rows of startGuns at the war's start month, by the lights' own
 * lookup. Ruling 124's flat placement (airversion.ts layStartGuns, laid first and untouched) retires where a row reads
 * the place, on a power's own ground that it holds (124's own condition: the province's owner holds it). `fresh` (the
 * war still in its first month, so nobody has built or lost a gun yet): the record decides, a reading of none removing
 * 124's guns. Otherwise only the higher level is kept: nothing is lowered or removed. A region the game in play does
 * not have is passed over. `rows` is for tests.
 */
export function layStartGunsFromRecord(g: Game, fresh: boolean, rows: GunsRow[] = CITYDEF.startGuns): void {
  if (!cityDefences(g)) return;
  for (const row of startRowsAt(rows, g.flags.openFrom ?? 1)) {
    const r = g.regions.find((x) => x.name === row.region);
    if (!r || r.owner !== r.ctrl) continue;
    const lv = Math.max(0, Math.min(WORKS_TOP, Math.floor(row.level ?? 0)));
    const key = `wk:aa:${r.id}`;
    if (fresh) {
      if (lv > 0) g.flags[key] = lv; else delete g.flags[key];
    } else if (lv > (g.flags[key] ?? 0)) g.flags[key] = lv;
  }
}

/**
 * The record's start, the searchlights and the guns together (city defences, the design director's ruling of 25 Nov
 * 2026 04:10Z): once, in a grand campaign, with the switch on; nothing otherwise. Called as a campaign opens (staff.ts
 * startOpen, after the start guns of ruling 124) and wherever the switch comes on in a campaign under way
 * (setCityDefences). The war's start month is openFrom, or the first; while the war is still in that month the
 * record sets the guns, and later it only raises them, the higher level kept. No headline, no news.
 */
export function cityStartLay(g: Game): void {
  if (!cityDefences(g) || g.flags.open !== 1 || g.flags[START_LAID] === 1) return;
  g.flags[START_LAID] = 1;
  const began = g.flags.openFrom ?? 1;
  openCityDefences(g);
  layStartGunsFromRecord(g, g.turn === began);
}

// ---- The computer's order of searchlights (slice 5) ----------------------------------------------------------------

// A night raid's own mark (form section 11, the completeness read's point 5: the 'raided' mark also fires on a strike
// at a convoy and does not say night): 'nr:<region>' = the month a night raid's bombs last fell on the province.
// Written only with the switch on; read only by the computer's order of searchlights (arsenal.ts aiGuns).

/** Marks a province night-raided this month (air.ts fly, where the bombs of a night raid fall). Off: nothing. */
export function markNightRaid(g: Game, region: number): void {
  if (cityDefences(g)) g.flags[`nr:${region}`] = g.turn;
}

/** True where a night raid's bombs fell on the province within the last ai.months months. False with the switch off. */
export function nightRaided(g: Game, region: number): boolean {
  if (!cityDefences(g)) return false;
  const t = g.flags[`nr:${region}`];
  return t !== undefined && g.turn - t <= CITYDEF.ai.months;
}

// ---- The guns by night (slice 2) ------------------------------------------------------------------------------------

/**
 * How the built guns at a target fight a night raid (city defences, the design director's ruling of 25 Nov 2026
 * 02:23Z, choices 1, 2 and 5). `guns` the built gun levels and `lights` the searchlights' levels there; `radar` true
 * where the province's holder has Fighter control (aircampaigns.json radar.control), a research row, never a date;
 * `lit` the gun levels a searchlight holds a bomber for, under a clear sky only; `sat` what is left of the lights'
 * work against the bombers over the target (one level holds 'saturation' of them); `share` the guns' night worth as
 * a share of their day worth, every level blended; `litShare` (0 to 1) the share of the gun levels firing by
 * searchlight after the too-many-bombers cut: none without radar, for lit without radar is ruled as dark.
 */
export interface CityNight { guns: number; lights: number; radar: boolean; cloud: boolean; lit: number; sat: number; share: number; litShare: number }

/** True where a province's holder has Fighter control (aircampaigns.json radar.control), the radar that lays the guns by night. */
export function radarLays(g: Game, region: number): boolean {
  const ctrl = g.regions[region]?.ctrl;
  return !!ctrl && hasTech(g, ctrl, AC.radar.control);
}

export function cityNight(g: Game, target: number, bombers: number, sky: Sky): CityNight {
  const N = CITYDEF.night;
  const L = gunsAt(g, target), Ls = lightsAt(g, target);
  const radar = radarLays(g, target);
  const clear = sky === 'clear';
  const lit = clear ? Math.min(L, Ls) : 0;
  const sat = Math.min(1, (CITYDEF.saturation * Ls) / Math.max(1, bombers));
  const d = radar ? N.radar : N.dark, t = radar ? N.both : N.lit;
  const share = L > 0 ? ((L - lit) * d + lit * (d + (t - d) * sat)) / L : d;
  return { guns: L, lights: Ls, radar, cloud: Ls > 0 && !clear, lit, sat, share, litShare: radar && L > 0 ? (lit * sat) / L : 0 };
}

/**
 * The share of a built gun level's day worth by night at a target, under the switch (the table of the ruling, choice
 * 1): it replaces the night guns' half of ruling 107(b) rather than riding on it, one cause, one price. Cloud keeps
 * its own price beside it (weather, ruling 127).
 */
export const cityNightShare = (g: Game, target: number, bombers: number, sky: Sky): number => cityNight(g, target, bombers, sky).share;

/**
 * The night fighters' floor over a target lit by searchlights (choice 4, the floor): a share of their day worth by
 * the lights' level, under a clear sky only; 0 under cloud or with the switch off. Lights serve the fighters with no
 * guns and no radar. A floor, never a multiplier: the caller takes the larger of it and the fighters' own figure.
 */
export const lightsFloor = (g: Game, target: number, sky: Sky): number => (cityDefences(g) && sky === 'clear' ? CITYDEF.fighters[lightsAt(g, target)] ?? 0 : 0);

// ---- A garrison fortifies its town (slice 3) ------------------------------------------------------------------------

// Town works (city defences, the design director's ruling of 25 Nov 2026 02:23Z, choices 6 and 7): barricades and
// strongpoints a garrison puts up in the town it holds, a month at a time, kept on the ground in flags as fieldworks
// are, never as a fort level (two nations' traits read fort levels as 'works'). `tw:<region>` the months done (1 to
// works.most, gone at 0); `twn:<region>` whose they are, as `fwn:` keeps a nation (its place among g.nations);
// `tws:<region>` the month the town was last stormed, kept until its holder's month end has read it. All three are
// written only with the switch on.

const nationIndex = (g: Game, nat: string): number => Object.keys(g.nations).indexOf(nat);

/**
 * The months of works a town has for its holder, 0 to works.most: 0 with the switch off, and 0 where the works on the
 * ground were put up by anyone but the province's holder now (a change of holder clears them; until the month end
 * does, they count for nobody).
 */
export function townWorksAt(g: Game, r: Region): number {
  if (!cityDefences(g)) return 0;
  const m = g.flags[`tw:${r.id}`] ?? 0;
  if (m <= 0 || g.flags[`twn:${r.id}`] !== nationIndex(g, r.ctrl)) return 0;
  return Math.min(CITYDEF.works.most, m);
}

/**
 * What the town works add inside the walls, above 1: works.worth times the entrenching share for the months done
 * (defence.json entrench.share, 0.5 / 0.75 / 1), so +0.15 / +0.225 / +0.3. The walls take the larger of it and the
 * forts' bonus, never both (siege.ts wallsOf). 0 with the switch off.
 */
export function townWorksWorth(g: Game, r: Region): number {
  const m = townWorksAt(g, r);
  return m > 0 ? CITYDEF.works.worth * (ENTRENCH.share[m - 1] ?? 1) : 0;
}

/** Clears a town's works: the town has changed hands (capture, and so every fall). Off: nothing. */
export function clearTownWorks(g: Game, r: Region): void {
  if (!cityDefences(g)) return;
  delete g.flags[`tw:${r.id}`];
  delete g.flags[`twn:${r.id}`];
}

/** Marks a town stormed this month, won or lost (siege.ts storm): its works gain nothing at its holder's month end. Off: nothing. */
export function markStormed(g: Game, r: Region): void {
  if (cityDefences(g)) g.flags[`tws:${r.id}`] = g.turn;
}

/**
 * The town works at the end of `nat`'s month (beside digIn, after every storm of the month has been fought): a town it
 * holds with a living corps of its own in the garrison, and not stormed since its last month end, gains a month, to
 * works.most, under siege too; a town of its with no garrison loses one. Works on ground that has changed holder are
 * cleared whoever's month it is. Off: nothing is read or written.
 */
export function townWorksMonth(g: Game, nat: string): void {
  if (!cityDefences(g)) return;
  const idx = nationIndex(g, nat);
  for (const r of g.regions) {
    const key = `tw:${r.id}`, by = `twn:${r.id}`, hit = `tws:${r.id}`;
    if (g.flags[key] !== undefined && g.flags[by] !== nationIndex(g, r.ctrl)) { delete g.flags[key]; delete g.flags[by]; }
    if (r.ctrl !== nat) continue;
    const stormed = g.flags[hit] !== undefined;
    if (stormed) delete g.flags[hit];
    const m = g.flags[key] ?? 0;
    // His own corps only (form section 5, "at least one of his corps inside"): an ally's corps in the garrison puts up
    // nothing for him. A corps in a garrison never marches joint, so its nation is its army.
    if (garrisonOf(r).some((c) => !c.dead && c.nat === nat)) {
      if (stormed || m >= CITYDEF.works.most) continue;
      g.flags[key] = m + 1;
      g.flags[by] = idx;
    } else if (m > 0) {
      if (m <= 1) { delete g.flags[key]; delete g.flags[by]; } else g.flags[key] = m - 1;
    }
  }
}

/**
 * The town works in the town line's own comma list (the UI director's words, section 4), after the strength: ", behind
 * 3 months of barricades and strongpoints", or where the forts count for more and the works add nothing, ", with 3
 * months of barricades and strongpoints, though its forts count for more". '' with none, or with the switch off.
 * `fort` is the forts' bonus inside the walls as the walls count it.
 */
export function townWorksWords(g: Game, r: Region, fort: number): string {
  const m = townWorksAt(g, r);
  if (m <= 0) return '';
  const months = `${m} month${m === 1 ? '' : 's'} of barricades and strongpoints`;
  return fort > townWorksWorth(g, r) ? `, with ${months}, though its forts count for more` : `, behind ${months}`;
}

// ---- The town's guns in the ground role (slice 4) -------------------------------------------------------------------

// The heavy anti-aircraft guns of a town that a garrison holds fire on the tanks that storm it (city defences, the
// design director's ruling of 25 Nov 2026 02:23Z, choice 8, form section 6 as corrected in section 11: nothing is
// added to air raids). Against tanks only, only in a storm of the town, only with a living garrison inside; and when
// such a town falls by storm a level of the guns is lost with it. The storm's one assessment (game.ts assess) reads it,
// so the battle card, the dice, the computer's storm decision and every forecast of the storm agree.

/**
 * The gun levels at a town that fire on its stormers' tanks: the built guns (air.ts gunsAt) where a living corps
 * stands in its garrison. 0 with no garrison, no guns, or the switch off.
 */
export function townFlak(g: Game, r: Region): number {
  if (!cityDefences(g)) return 0;
  if (!garrisonOf(r).some((c) => !c.dead)) return 0;
  return gunsAt(g, r.id);
}

/** The further cut to the tanks' attack from `level` gun levels dug in with the garrison: ground.armour a level, to ground.most. */
export const flakCut = (level: number): number => (level > 0 ? Math.min(CITYDEF.ground.most, CITYDEF.ground.armour * level) : 0);

/** The further cut to the tanks storming a town (on top of the streets' own): 0 with no garrison, no guns, or the switch off. */
export const townFlakCut = (g: Game, r: Region): number => flakCut(townFlak(g, r));

/** The most the town's guns can take from the tanks, where the stormer does not know them: the guns' top level's cut. */
export const flakCutMost = (): number => flakCut(WORKS_TOP);

/**
 * The battle card's line for the guns against the tanks (the UI director's words, section 5): a second 'Armor' line
 * after the streets line. The value matches neither of the fog patterns turn.ts keeps for the 'Armor' label; its own
 * fog form is flakFog.
 */
export const flakLine = (cut: number): { label: string; value: string; effect: string } =>
  ({ label: 'Armor', value: 'Heavy anti-aircraft guns dug in with the garrison', effect: `Tank attack cut by a further ${Math.round(cut * 100)}%` });

/** Whether a ticket line is the guns' line against the tanks (flakLine), for the fog. */
export const isFlakLine = (l: { label: string; value: string }): boolean => l.label === 'Armor' && /^Heavy anti-aircraft guns dug in with the garrison$/.test(l.value);

/** The guns' line where the stormer has not identified the defence (the UI director's words, section 5). */
export const flakFog = (): { label: string; value: string; effect: string } =>
  ({ label: 'Armor', value: 'The garrison may have heavy anti-aircraft guns dug in', effect: `Tank attack cut by as much as ${Math.round(flakCutMost() * 100)}% if it has` });

/**
 * The price (form section 6): a town that had a garrison and guns at the storm's start and falls by storm loses
 * ground.lost levels of its guns ('wk:aa:<region>' goes down, deleted at nought), and the ground keeps the mark
 * 'aal:<region>' = the month it was lost (the art director's scorched pits). Surrender, starvation and handover keep the
 * guns. Returns the result line for the battle's report (the UI director's words, section 6), or null. `level` is
 * townFlak as the storm began; 0 (no garrison, no guns, the switch off) does nothing.
 */
export function flakLostWithTown(g: Game, r: Region, level: number, town: string): { label: string; value: string } | null {
  if (!cityDefences(g) || level <= 0) return null;
  const key = `wk:aa:${r.id}`;
  const left = Math.max(0, (g.flags[key] ?? 0) - CITYDEF.ground.lost);
  if (left > 0) g.flags[key] = left; else delete g.flags[key];
  g.flags[`aal:${r.id}`] = g.turn;
  return { label: 'Anti-aircraft guns', value: `The anti-aircraft guns of ${town} fought the tanks over open sights, and one level of them was lost with the town.` };
}
