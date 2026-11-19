// The wider world.
//
// The history campaigns are fought on the map of Europe and never see any of this. A grand campaign opens on a
// wider chart: the same sheet carried on westward, on the same scale, to the Pacific coast of North America, and
// (Version 76) south from the Rio Grande to Tierra del Fuego. The new provinces follow Europe's in number, the new
// seas follow Europe's seas, and Europe's own keep the numbers they have, so nothing that names a province or a
// sea has to change.
//
// What is attached: the provinces and seas of tools/build-geo-world.mjs, the countries that exist only out there
// (Canada, Mexico, Cuba, and the eighteen republics of Latin America), and the United States at home. Before
// Version 74 America was a country beyond the map's edge that woke in December 1941 with its industry out of
// reach. On the wider chart it is there from the first day: neutral, its capital at Washington, its factories on
// its own soil, its army a cadre.
//
// The world is attached once, as the grand campaign opens (staff.ts), before anything else is set up for it, and
// again whenever a campaign saved on it is loaded (save.ts).

import americasMap from '../generated/world-americas.json';
import wholeMap from '../generated/world-whole.json';
import { FAR_STAGE } from './stage';
import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import data from '../../data/campaign/world.json';
import { addCorps, allied, armySize, bloc, capitalDistances, computeSupply, headline, nationOf, regionOf, setWar, standsBy, theName } from './game';
import { addFleet, chartSeas, type FleetRow } from './navy';
import { clientMonth, farCountries, farGround, farSettle } from './far';
import { fieldClass, seedClass, seedFields } from './air';
import { newLab, researches } from './research';
import { rearmedAt } from './footing';
import type { Game, Region } from './types';

interface WorldMap {
  frame: { x0: number; y0: number; x1: number; y1: number };
  /** The whole world's own coordinates: world x = (x + shift) mod width; the seam is the longitude where world x is 0. */
  world: { width: number; height: number; shift: number; seam: number };
  first: number;
  land: [number, number][][][];
  lakes: { name: string; level: number; x: number; y: number; outline: number[][] }[];
  regions: any[];
  links: [number, number][];
  /** Land borders between a province of an earlier stage (Europe's or the Americas') and a far one: [older id, far id]. */
  joins?: [number, number][];
  /** How many provinces the wider world had at each stage of its drawing, today's last. */
  stages?: number[];
  seas: { zones: any[]; gates: any[]; borders: [number, number][][] };
}

/**
 * The wider world's chart: the Americas with their cities (tools/build-world-americas.mjs), or, once the far stage is
 * on (stage.ts), the whole world with the Pacific and the globe laid after them (tools/build-world-whole.mjs).
 */
export const WORLD_MAP = (FAR_STAGE ? wholeMap : americasMap) as unknown as WorldMap;

export const WORLD = data as unknown as {
  nations: Record<string, any>;
  home: Record<string, { align?: string }>;
  follow: Record<string, string>;
  dominions?: Record<string, { of: string[]; answers: number; steps?: number[]; europe?: string[]; beside?: string; willGone?: number; since?: Record<string, number> }>;
  corps: [string, string, string, number, string, { xp?: number; str?: number; hold?: number; stay?: number }][];
  fleets: FleetRow[];
  peace: Record<string, [number, number][]>;
  warWork: Record<string, number>;
  climbFrom?: Record<string, number>;
  works?: Record<string, number>;
  overseas: Record<string, OverseasRules>;
  protector: string[];
  pairs: [string, string][];
  order: string[];
  great: string[];
  oilHome: Record<string, number>;
  idle: Record<string, number>;
  labels?: Record<string, 'under' | 'west'>;
};

/** How a computer-run country an ocean away from its enemies sends its army across (world.json, 'overseas'). */
export interface OverseasRules { guard: number; ahead: number; bank: number; risk: number; hop?: number; from?: number; most?: number; air?: number; fleet?: string[]; oceans: Record<string, number> }

/** The first province beyond the map of Europe, and how many there are. */
export const WORLD_FIRST = WORLD_MAP.first;
export const WORLD_REGIONS = WORLD_MAP.regions.length;
/**
 * How many provinces the wider world had at each earlier stage of its drawing: North America and the Atlantic
 * islands alone (Versions 74 and 75), then the whole of the Americas before their cities were cut (Versions 76 to
 * 84.3; the cities version lays 339 cities after them, tools/build-world-americas.mjs). The chart only ever grows at its end, so a campaign saved on an earlier
 * stage can be laid over today's (save.ts): the ground it never knew begins as it stood on the first day.
 */
export const WORLD_STAGES = FAR_STAGE ? [104, 201, 540] : [104, 201];

/**
 * The first province of the far side: the Pacific, then the globe, then their cities, laid after the Americas' cities
 * (the rules builder's numbering, 17 November 2026: 915 to 1226, 1227 to 1593, 1594 to 2065).
 */
export const FAR_FIRST = WORLD_FIRST + (wholeMap as unknown as WorldMap).stages![1];

/** True for a campaign played on the wider world. */
export const worldOn = (g: Game): boolean => g.flags.world === 1;

/** True for a campaign whose chart reaches the far side of the world: the Pacific and the globe. */
export const farOn = (g: Game): boolean => worldOn(g) && g.regions.length > FAR_FIRST;

/** The mother country that stands by a dominion (world.json, 'dominions'), or null. */
export function patronOf(nat: string): string | null {
  for (const [p, row] of Object.entries(WORLD.dominions ?? {})) if (row.of.includes(nat)) return p;
  return null;
}

/**
 * The declare card's row for a dominion's mother country, first among its friends (the UI director's words, 17 November
 * 2026): "The mother country: Canada has been a dominion since 1867." Display only: the tie moves no opinion.
 */
export function motherLine(patron: string, nat: string, name: string): string {
  const since = WORLD.dominions?.[patron]?.since?.[nat];
  return since ? `The mother country: ${name} has been a dominion since ${since}.` : `The mother country: ${name} is a dominion.`;
}

/**
 * The share of the time a mother country answers when war is made on a dominion of hers, at the step her own war in
 * Europe stands at (ruling 222: 0.8, 0.6, 0.4 or 0.2 for Britain), or her one share where she has no steps.
 */
export function answersOf(patron: string, step?: number): number {
  const row = WORLD.dominions?.[patron];
  return (step ? row?.steps?.[step - 1] : undefined) ?? row?.answers ?? 0;
}

/** What a mother country's war in Europe is read by (ruling 222): the great powers of Europe, the ally beside her, the share of her will that puts her in danger. */
export function pressData(patron: string): { europe: string[]; beside: string | null; willGone: number } {
  const row = WORLD.dominions?.[patron];
  return { europe: row?.europe ?? [], beside: row?.beside ?? null, willGone: row?.willGone ?? 1 / 3 };
}

/** True for a province beyond the map of Europe. */
export const beyondEurope = (r: Region | number): boolean => (typeof r === 'number' ? r : r.id) >= WORLD_FIRST;

/**
 * Which side of the oceans a province stands on: 0 the old chart, 1 the Americas and the Atlantic islands, 2 the far
 * side of the world. (No province stands on the far side while its switch is off: every province beyond the old
 * chart is then on the Americas' side, as it was.)
 */
export const shoreOf = (r: Region | number): number => {
  const id = typeof r === 'number' ? r : r.id;
  return id < WORLD_FIRST ? 0 : id < FAR_FIRST ? 1 : 2;
};

/** The great lakes cut out of the land: their names and where to write them. None is a sea. */
export const lakesOf = (g: Game): WorldMap['lakes'] => (worldOn(g) ? WORLD_MAP.lakes : []);

/**
 * Attaches the wider world to a game on the map of Europe. Safe to call twice: the second call does nothing.
 * 'fresh' is false when a saved campaign is being laid over it (save.ts): the armies, fleets and wars of the first
 * day are then left out, since the save brings its own.
 */
export function attachWorld(g: Game, fresh = true): void {
  if (g.flags.world === 1 || g.regions.length !== WORLD_FIRST) return;
  g.flags.world = 1;
  // The provinces, numbered on from Europe's.
  for (const row of WORLD_MAP.regions) {
    const r = regionOf(row);
    g.regions.push(r);
    g.byName[r.name] = r;
    // A depot is a home province that can feed an army cut off from its capital.
    if (row.flags.depot) g.flags['depot:' + r.id] = 1;
  }
  // The land borders between an older province and a far one (the chart's 'joins': Kazan and Gorky, the Yukon and
  // Alaska), added to both. A province's list of neighbours is the map file's own array, shared by every game made
  // in this process, so it is copied before it grows.
  for (const [a, b] of WORLD_MAP.joins ?? []) {
    const A = g.regions[a], B = g.regions[b];
    if (!A || !B) continue;
    if (!A.nb.includes(b)) A.nb = [...A.nb, b];
    if (!B.nb.includes(a)) B.nb = [...B.nb, a];
  }
  farGround(g); // the far side's first day on the ground: who holds what, and the capitals that move (far.ts)
  // The chart itself: the coasts, the ferries, and the frame the interfaces draw in.
  g.links = [...g.links, ...WORLD_MAP.links];
  // Europe's factories as the wider world counts them (world.json, 'works'): a saved campaign is laid over a fresh
  // map, so it takes them too, with its own factories built since added on top (save.ts).
  for (const [name, fac] of Object.entries(WORLD.works ?? {})) if (g.byName[name]) g.byName[name].fac = fac;
  g.land = [...WORLD_MAP.land, ...g.land];
  g.x0 = WORLD_MAP.frame.x0;
  g.width = WORLD_MAP.frame.x1 - WORLD_MAP.frame.x0;
  g.height = Math.max(g.height, WORLD_MAP.frame.y1 - WORLD_MAP.frame.y0);
  // The seas: Europe's keep their numbers and gain the neighbours they have to the west; the oceans follow.
  chartSeas(g, WORLD_MAP.seas);
  // The countries that exist only out here take their place at the end of the order of play.
  for (const id of WORLD.order) {
    const d = WORLD.nations[id];
    if (!d || g.nations[id]) continue;
    const cap = g.regions.find((r) => r.owner === id && r.cap);
    const n = g.nations[id] = nationOf(id, d, cap ? cap.id : -1);
    n.lab = researches(id) ? newLab() : null;
    if (!g.order.includes(id)) g.order.push(id);
  }
  // And the countries whose homeland was beyond the map come home to it.
  for (const [id, h] of Object.entries(WORLD.home)) {
    const n = g.nations[id];
    const cap = g.regions.find((r) => r.owner === id && r.cap);
    if (!n || !cap) continue;
    n.capital = cap.id;
    n.offmap = null;
    // Still standing aside: it is there, and at peace. One already in the war (a campaign begun later) stays as it is.
    if (n.dormant) {
      n.dormant = false;
      n.alive = true;
      if (h.align) n.align = h.align as typeof n.align;
    }
  }
  // A campaign begun after the first day: the colonies of a country already fallen in Europe (Iceland and Greenland
  // in a 1941 start) are not left to it, frozen, but go where they would have gone on the day it fell.
  if (fresh) settleFallen(g, WORLD_FIRST);
  // Every nation's distances to its capital are counted again on the wider map.
  for (const n of Object.values(g.nations)) if (n.capital >= 0) g.capDist[n.id] = capitalDistances(g, n.capital);
  // The supply picture keeps every country's place in it, widened to the new provinces: they are unsupplied until
  // it is drawn again.
  for (const [k, v] of Object.entries(g.supply)) { const w = new Uint8Array(g.regions.length); w.set(v); g.supply[k] = w; }
  g.reach = {};
  g.railhead = {};
  g.supplyDirty = true;
  // A saved campaign's picture is laid over one drawn here, with every country in its usual place in it. (A new
  // campaign's is drawn once everything else is set up: staff.ts.)
  if (!fresh) { standInCountries(g); computeSupply(g); return; }
  // The armies standing at home on the first day: peacetime cadres.
  for (const [nat, name, kind, divs, at, o] of WORLD.corps) {
    if (!g.nations[nat]?.alive || !g.byName[at] || g.byName[at].ctrl !== nat) continue;
    const c = addCorps(g, nat, name, kind as any, divs, at, o);
    if (o.str !== undefined) c.str = o.str;
  }
  // The squadrons in home waters.
  for (const row of WORLD.fleets) if (g.nations[row.nat]?.alive && g.byName[row.base ?? '']?.ctrl === row.nat) addFleet(g, row);
  // A dominion is in its mother country's wars from the day they began.
  for (const [id, of] of Object.entries(WORLD.follow)) {
    const n = g.nations[id], M = g.nations[of];
    if (!n?.alive || !M?.alive) continue;
    for (const e of M.atWar) if (g.nations[e]?.alive) setWar(g, id, e);
  }
  for (const n of Object.values(g.nations)) if (n.alive) n.armyPeak = Math.max(n.armyPeak, armySize(g, n.id));
  farCountries(g, fresh); // the far side's countries on their first day: armies on a client's ground, the wars standing (far.ts)
  standInCountries(g);
}

/** The bare countries of the far stage's guard: name, adjective, colour. */
const STAND_IN: Record<string, [string, string, string]> = {
  JAP: ['Japan', 'Japanese', '#b23a48'], CHN: ['China', 'Chinese', '#3e6fa3'], MAN: ['Manchukuo', 'Manchukuoan', '#a8894a'],
  MGL: ['Mongolia', 'Mongolian', '#8c5a3c'], THA: ['Thailand', 'Thai', '#5b8c8a'], AUS: ['Australia', 'Australian', '#2f7d5b'],
  NZL: ['New Zealand', 'New Zealand', '#40507a'], SAF: ['South Africa', 'South African', '#d98a2b'], SAU: ['Saudi Arabia', 'Saudi', '#3f8f5a'],
  YEM: ['Yemen', 'Yemeni', '#b0563f'], AFG: ['Afghanistan', 'Afghan', '#6f7f4a'], NEP: ['Nepal', 'Nepalese', '#9c3d54'], LBR: ['Liberia', 'Liberian', '#4a6fa5'],
};

/**
 * The far stage's guard: a bare country for every owner code on the chart that is still no country once the far
 * side's own rows are laid (far.ts). It stands at home, at peace and independent, with no army, so the map and every
 * page can name whoever owns a province. Where every code has its own row it does nothing; on a narrower chart it
 * is never reached.
 */
function standInCountries(g: Game): void {
  if (!farOn(g)) return;
  for (const r of g.regions) {
    const id = r.owner;
    if (g.nations[id]) continue;
    const [name, adj, color] = STAND_IN[id] ?? [id, id, '#8a8a80'];
    const own = g.regions.filter((x) => x.owner === id);
    const cap = own.find((x) => x.cap) ?? [...own].sort((a, b) => b.vp - a.vp || a.id - b.id)[0];
    g.nations[id] = nationOf(id, { name, adj, color, quality: 0.5, air: 0, resilience: 0.5, align: 'independent' }, cap.id);
    if (!g.order.includes(id)) g.order.push(id);
    g.capDist[id] = capitalDistances(g, cap.id);
  }
}

/**
 * A campaign saved on an earlier stage of the chart is being laid over today's (save.ts): the provinces from
 * 'from' on are new to it. They stand as on the first day, with the first-day garrisons and squadrons that belong
 * in them, except that ground of a country the save has already seen fall goes where a fallen country's ground
 * overseas goes: into the keeping of a sea power still fighting the victor, or failing that to the victor.
 */
export function settleNewGround(g: Game, from: number): void {
  if (!worldOn(g)) return;
  // A city cut out of a province since the campaign was saved stands as its province does: held by whoever holds
  // the province, since the month it changed hands (a city of Buffalo's held by Canada is Canada's).
  const parents = new Set<number>();
  for (let i = Math.max(from, WORLD_FIRST); i < g.regions.length; i++) {
    const r = g.regions[i], of = (WORLD_MAP.regions[i - WORLD_FIRST] as { of?: number } | undefined)?.of;
    if (of === undefined || of >= from) continue;
    parents.add(of);
    const p = g.regions[of];
    if (p && p.ctrl !== r.ctrl) { r.ctrl = p.ctrl; r.since = p.since; }
  }
  settleFallen(g, from);
  const fresh = (name: string | null | undefined): boolean => !!name && !!g.byName[name] && g.byName[name].id >= from;
  for (const [nat, name, kind, divs, at, o] of WORLD.corps) {
    if (!fresh(at) || !g.nations[nat]?.alive || g.byName[at].ctrl !== nat) continue;
    const c = addCorps(g, nat, name, kind as any, divs, at, o);
    if (o.str !== undefined) c.str = o.str;
  }
  for (const row of WORLD.fleets) if (fresh(row.base) && g.nations[row.nat]?.alive && g.byName[row.base!].ctrl === row.nat) addFleet(g, row);
  // Its first-day airfields too, where the air war is already running. The field a province held for a city cut out
  // of it since goes with the city (Houston's, out of East Texas): the province keeps the one the first day gives it
  // now, unless an air group is based there. What engineers or the treasury built there (wk:field) stays its own.
  if (g.flags.airwar === 1) {
    seedFields(g, Math.max(from, WORLD_FIRST));
    for (const p of parents) {
      const want = seedClass(g, g.regions[p]);
      if (fieldClass(g, p) <= want || g.airFleets.some((f) => f.base === p)) continue;
      if (want) g.flags[`af:${p}`] = want; else delete g.flags[`af:${p}`];
    }
  }
  for (const n of Object.values(g.nations)) if (n.alive) n.armyPeak = Math.max(n.armyPeak, armySize(g, n.id));
  farSettle(g, from); // a save from before the far side: its countries begin as on their first day (far.ts)
  g.supplyDirty = true;
}

/**
 * Ground from 'from' on still held by a country that has fallen goes where a fallen country's ground overseas goes
 * (game.ts, on the day of the fall): into the keeping of a sea power still fighting the victor, or failing that to the
 * victor. For ground the chart did not yet have on that day: a save laid over a wider chart, or a campaign begun late.
 */
function settleFallen(g: Game, from: number): void {
  for (let i = Math.max(from, WORLD_FIRST); i < g.regions.length; i++) {
    const r = g.regions[i], n = g.nations[r.ctrl];
    if (n?.alive || !n?.fellOn) continue;
    const by = n.fellOn.to;
    r.ctrl = (r.overseas ? protectorOf(g, by) : null) ?? (g.nations[by]?.alive ? by : r.ctrl);
    r.since = g.turn;
  }
}

/**
 * The wider world's month: a dominion follows its mother country into every war she is in, while the two stand in
 * the same alliance. One in the player's hands goes to war only when its player says so.
 */
export function worldMonth(g: Game): void {
  if (!worldOn(g) || (g.flags.dom ?? 0) > 0) return;
  clientMonth(g); // the far side: a client's tie to its patron (far.ts)
  for (const [id, of] of Object.entries(WORLD.follow)) {
    const n = g.nations[id], M = g.nations[of];
    if (!n?.alive || !M?.alive || id === g.human || !allied(g, id, of)) continue;
    for (const e of M.atWar) {
      const E = g.nations[e];
      if (!E?.alive || n.atWar.includes(e) || allied(g, id, e)) continue;
      setWar(g, id, e);
      headline(g, 'war', `${theName(n, true)} is at war with ${theName(E)}, beside ${theName(M)}.`, id);
    }
  }
  // A campaign saved before ruling 211(b): the roll for a war somebody made on a dominion is made as the month
  // ends, once (a new war has its roll the day it is declared, in declareWar, and is passed over here).
  for (const row of Object.values(WORLD.dominions ?? {})) {
    for (const d of row.of) {
      const D = g.nations[d];
      if (!D?.alive) continue;
      for (const e of [...D.atWar]) { const since = g.flags[`war:${e}:${d}`]; if (since !== undefined) standsBy(g, e, d, since); }
    }
  }
  // No declaration switches such a country off (ruling 58). A computer-led country whose industry goes to war by
  // steps (the United States), at war with a member of the Axis and standing on no side, joins the Allies that
  // month: the Axis is one enemy. Its war work is climbing already, and its staff ships its army as an Ally's.
  for (const id of Object.keys(WORLD.warWork)) {
    const n = g.nations[id];
    if (!n?.alive || n.dormant || id === g.human || bloc(n) !== null) continue;
    const foes = n.atWar.map((e) => g.nations[e]).filter((x) => x?.alive);
    const foe = foes.find((x) => bloc(x) === 'axis');
    if (!foe || foes.some((x) => bloc(x) === 'allies')) continue;
    if (!Object.values(g.nations).some((x) => x.id !== id && x.alive && !x.dormant && bloc(x) === 'allies')) continue;
    n.align = 'allies';
    n.opinion = n.home = -100;
    g.flags[`step:${id}`] = g.turn;
    g.supplyDirty = true;
    headline(g, 'event', `${theName(n, true)} joins the Allies: it is at war with ${theName(foe)}, and the Axis is one enemy.`, id);
  }
}

/** The share of a country's industry on war work while it is at peace, where the wider world gives it one of its own, or null where the ordinary rule stands. */
export function peaceFooting(g: Game, nat: string): number | null {
  const rows = worldOn(g) ? WORLD.peace[nat] : undefined;
  if (!rows) return null;
  let share = rows[0][1];
  for (const [from, s] of rows) if (g.turn >= from) share = s;
  return share;
}

/**
 * The share of a country's industry on war work once it is at war, where the wider world has it climb there by
 * steps (world.json, 'warWork'): from where the peace table stood on the day war came, so much more each month.
 * Null for a country that works at full stretch from the first day of its war, as every other does.
 */
export function warFooting(g: Game, nat: string): number | null {
  if (!worldOn(g) || WORLD.warWork[nat] === undefined) return null;
  // (At war before its first month's step is taken, a climb stands where the calendar has it.)
  return Math.min(1, g.flags[`ww:${nat}`] ?? (g.nations[nat]?.atWar.length ? climbStart(g, nat) : peaceFooting(g, nat) ?? 1));
}

/**
 * Where a country's climb to war work starts in the month its war comes: the peace table, or, for a climb that waits
 * for a named month (Canada's, world.json 'climbFrom'), as far up as the calendar has it by then.
 */
function climbStart(g: Game, id: string): number {
  // (A country that rearmed at peace starts its climb from where rearmament left it: footing.ts, ruling 172.)
  const table = Math.max(peaceFooting(g, id) ?? 1, rearmedAt(g, id)), from = WORLD.climbFrom?.[id], step = WORLD.warWork[id] ?? 0;
  return from === undefined ? table : table + step * Math.max(0, g.turn - from);
}

/** The month's step in war work for the countries that climb to it: for the player's country and the computer's alike. */
export function warWorkMonth(g: Game): void {
  if (!worldOn(g)) return;
  for (const [id, step] of Object.entries(WORLD.warWork)) {
    const n = g.nations[id], key = `ww:${id}`;
    // Peace again: the factories go back to the peace table, and a new war begins from there.
    if (!n?.alive || !n.atWar.length) { delete g.flags[key]; continue; }
    // A climb that waits for a named month (Canada's, world.json 'climbFrom') stands at the peace table until then;
    // a campaign that opens after it finds the share where the calendar has it.
    const table = peaceFooting(g, id) ?? 1, was = g.flags[key], from = WORLD.climbFrom?.[id];
    const start = climbStart(g, id);
    const climbed = was === undefined ? start : from !== undefined && g.turn <= from ? table : was + step;
    const now = Math.min(1, Math.max(table, climbed >= 1 - 1e-9 ? 1 : climbed));
    g.flags[key] = now;
    if (was !== undefined && was < 1 && now >= 1 && id === g.human) headline(g, 'info', `${n.adj} industry is now wholly on war work: every factory at home earns its full worth.`, id);
  }
}

/** How many months of war are left before a country that climbs to full war work gets there: 0 when it is there, or has no climb to make. */
export function warWorkLeft(g: Game, nat: string): number {
  const step = WORLD.warWork[nat];
  if (warFooting(g, nat) === null || !step) return 0;
  // At peace, from where a war begun this month would start (warWorkMonth).
  const share = Math.min(1, g.flags[`ww:${nat}`] ?? climbStart(g, nat));
  const wait = Math.max(0, (WORLD.climbFrom?.[nat] ?? 0) - g.turn);
  return share >= 1 ? 0 : wait + Math.ceil((1 - share) / step - 1e-9);
}

/** What a country's small wells and plants at home yield, where the wider world puts its oilfields on the map; null where the ordinary figure stands. */
export const oilAtHome = (g: Game, nat: string): number | null => (worldOn(g) ? WORLD.oilHome[nat] ?? null : null);

/**
 * True for a neighbour by the map, beyond the old chart: his homeland touches the country's own homeland, so its
 * army at home can march on him. Ground the country owns across the sea makes no neighbour of him (Panama beside
 * the Canal Zone: no army of America's walks there from home).
 */
export function neighbourByLand(g: Game, nat: string, e: string): boolean {
  for (let i = WORLD_FIRST; i < g.regions.length; i++) {
    const r = g.regions[i];
    if (r.owner === e && !r.overseas && r.nb.some((j) => g.regions[j].owner === nat && !g.regions[j].overseas)) return true;
  }
  return false;
}

/**
 * True where an enemy is at the door of a country beyond the old chart: an invader from across the ocean ashore
 * anywhere on its side of the water (an enemy whose own capital stands on the old chart), or a hostile country of
 * its own side of the water across a land frontier from ground it holds. He is its army's first business. A hostile
 * country beyond the sea on its own side is not: the army has no road to him, and he does not keep it at home
 * (Version 78, ruling 58a: an Axis Brazil at war with the United States no longer holds America's army back from
 * Europe, whatever colonies Brazil has taken). Nor does he become a neighbour by land because a force was put
 * ashore on his coast (Version 81, ruling 80: the war on its own side of the ocean must not become its only
 * business): the frontier counts where the ground on the country's side of it is its own, or where he is a
 * neighbour by the map, his homeland touching the country's own homeland. Ground the country owns across the sea
 * is no part of the door: an enemy Panama beside the Canal Zone is reached by ship, and an army held at home could
 * not march to him.
 */
export function enemyAtTheDoor(g: Game, nat: string): boolean {
  const n = g.nations[nat];
  if (!n) return false;
  const byMap = new Map<string, boolean>();
  const neighbour = (e: string): boolean => {
    let v = byMap.get(e);
    if (v === undefined) { v = neighbourByLand(g, nat, e); byMap.set(e, v); }
    return v;
  };
  // Its side of the water is the side its capital stands on. (The far side of the world is beyond the old chart too,
  // and it is not America's side: Italy's colony on the Horn of Africa, hers since 1936, is no invader at America's
  // door, and with it counted no American, Canadian or Brazilian corps ever sailed for Europe.)
  const side = n.capital >= WORLD_FIRST ? shoreOf(n.capital) : -1;
  for (let i = WORLD_FIRST; i < g.regions.length; i++) {
    const r = g.regions[i];
    if (!n.atWar.includes(r.ctrl) || (side >= 0 && shoreOf(i) !== side)) continue;
    const cap = g.nations[r.ctrl]?.capital ?? -1;
    if (cap >= 0 && !beyondEurope(cap)) return true;
    if (r.nb.some((j) => g.regions[j].ctrl === nat && ((g.regions[j].owner === nat && !g.regions[j].overseas) || neighbour(r.ctrl)))) return true;
  }
  return false;
}

/**
 * True for a computer-run staff across the ocean from every enemy that already has as many field corps at home as
 * it has any use for: it raises no more to stand on the quay. An enemy at the door ends that (`enemyAtTheDoor`).
 */
export function armyIdle(g: Game, nat: string): boolean {
  if (!worldOn(g) || nat === g.human) return false;
  const n = g.nations[nat];
  if (!n || n.capital < 0 || !beyondEurope(n.capital)) return false;
  if (enemyAtTheDoor(g, nat)) return false;
  // Corps on order count as if they had already formed.
  let home = n.queue.reduce((s, b) => s + (b.what === 'corps' && b.kind !== 'eng' ? 1 : 0), 0);
  for (let i = WORLD_FIRST; i < g.regions.length; i++) {
    const r = g.regions[i];
    // A corps in an overseas possession is that place's garrison and not the army at home (as the ferry reckons it):
    // the Canal's division, infantry since Version 84.2, counts no more than the fortress troops it replaced.
    if (r.overseas) continue;
    for (const c of r.units) if (c.nat === nat && c.kind !== 'fort' && c.kind !== 'eng') home++;
    for (const c of r.garr ?? []) if (c.nat === nat && c.kind !== 'fort' && c.kind !== 'eng') home++;
  }
  // A country that ships its army across the ocean raises corps as far ahead as its sailings will carry, and a guard besides.
  const O = overseasOf(g, nat);
  if (O) return home >= O.guard + Math.ceil(O.ahead * sailings(g, nat, O));
  return home >= (WORLD.idle[nat] ?? WORLD.idle._default);
}

/**
 * The rules a computer-run country ships its army across the ocean by, while they apply: it is at war, it has
 * shipping, its capital stands in its own hands beyond the old chart, and no enemy is at its door there
 * (`enemyAtTheDoor`: an invader ashore or a hostile neighbour is the army's first business). Null for the
 * player's country, whose army is his to ship.
 */
export function overseasOf(g: Game, nat: string): OverseasRules | null {
  const O = worldOn(g) && nat !== g.human ? WORLD.overseas[nat] : undefined;
  const n = g.nations[nat];
  if (!O || !n?.alive || !n.atWar.length || n.sealift <= 0 || n.capital < 0 || !beyondEurope(n.capital) || g.regions[n.capital].ctrl !== nat) return null;
  // (A small country's expeditionary force sails from history's month and not before: 'from'.)
  if (O.from !== undefined && g.turn < O.from) return null;
  return enemyAtTheDoor(g, nat) ? null : O;
}

/** True for a computer-run country that ships its own army overseas, at war or not: history's dated formations are not sent to it as well. */
export const shipsItsOwn = (g: Game, nat: string): boolean => {
  const n = g.nations[nat];
  return worldOn(g) && nat !== g.human && !!WORLD.overseas[nat] && !!n && n.capital >= 0 && beyondEurope(n.capital) && g.regions[n.capital].ctrl === nat;
};

/**
 * The great powers a country that ships its army across the ocean may go to war with from the first month (the
 * design director's ruling 31): those of the bloc opposed to the one it stands with or leans toward. It reaches them
 * through its friends' ports, so no enemy coast need lie within range of its own. Empty for every other country.
 */
export function acrossTheWater(g: Game, nat: string, majors: string[]): string[] {
  const n = g.nations[nat];
  if (!worldOn(g) || !WORLD.overseas[nat] || !n?.alive) return [];
  const side = bloc(n) ?? (n.align === 'leaning_allies' ? 'allies' : n.align === 'leaning_axis' ? 'axis' : null);
  if (side !== 'allies' && side !== 'axis') return [];
  return majors.filter((id) => id !== nat && g.nations[id]?.alive && !g.nations[id].dormant && bloc(g.nations[id]) === (side === 'allies' ? 'axis' : 'allies'));
}

/** Whether going to war with this country would bring a country across the ocean to stand with the Allies (ruling 32), for the card that asks. */
export function wouldStandWithAllies(g: Game, nat: string, on: string): boolean {
  const n = g.nations[nat], B = g.nations[on];
  if (!worldOn(g) || !WORLD.overseas[nat] || !n?.alive || !B?.alive || bloc(n) !== null || bloc(B) !== 'axis') return false;
  if (n.atWar.some((id) => g.nations[id]?.alive && bloc(g.nations[id]) === 'allies')) return false;
  return Object.values(g.nations).some((x) => x.id !== nat && x.alive && !x.dormant && bloc(x) === 'allies');
}

/**
 * Ruling 32: a country that ships its army across the ocean, at war with a power of the Axis and with no Allied
 * country, stands with the Allies from that day: their ports and supply lines are open to it. It is asked when a
 * player declares war (a computer's country comes in by history's events, which do the same), with 'none' for `was`
 * when it stood in no bloc before the declaration. True when it has come to stand with them.
 */
export function standWithAllies(g: Game, nat: string, was: string | null = null): boolean {
  const n = g.nations[nat];
  if (!worldOn(g) || !WORLD.overseas[nat] || !n?.alive) return false;
  const say = (): void => headline(g, 'event', `${theName(n, true)} stands with the Allies: their ports and their supply lines are open to its armies.`, nat);
  // The declaration itself may have done it (a country leaning to the Allies that attacks their enemy): it is told all the same.
  if (bloc(n) === 'allies' && was === 'none') { say(); return true; }
  if (bloc(n) !== null) return false;
  const foes = n.atWar.map((id) => g.nations[id]).filter((x) => x?.alive);
  if (!foes.some((x) => bloc(x) === 'axis') || foes.some((x) => bloc(x) === 'allies')) return false;
  if (!Object.values(g.nations).some((x) => x.id !== nat && x.alive && !x.dormant && bloc(x) === 'allies')) return false;
  n.align = 'allies_cobelligerent';
  n.opinion = n.home = -100;
  g.flags[`step:${nat}`] = g.turn;
  g.supplyDirty = true;
  say();
  return true;
}

/** How many corps a month the country's shipping carries across its oceans, all runs together. */
export const sailings = (g: Game, nat: string, O: OverseasRules): number => Object.values(O.oceans).reduce((s, x) => s + x, 0) * g.nations[nat].sealift;

/** Whether a province lies across the named ocean from the Americas: where that run's ships can put an army down. */
export function acrossOcean(ocean: string, r: Region): boolean {
  // The Atlantic run ends on the old chart: Europe, Africa's northern shore and the Near East. (The Pacific's entry comes with its own slice.)
  return ocean === 'atlantic' ? !beyondEurope(r) : false;
}

/**
 * Who takes over a fallen country's ground beyond the old chart: islands and colonies an ocean away from the
 * victor's armies. The first power on the list that is alive, lives by its ships and is at war with the victor;
 * null where there is none, and the ground goes with the rest.
 */
export function protectorOf(g: Game, by: string): string | null {
  if (!worldOn(g)) return null;
  for (const id of WORLD.protector) {
    const n = g.nations[id];
    if (n?.alive && n.seaSupply && n.atWar.includes(by)) return id;
  }
  return null;
}
