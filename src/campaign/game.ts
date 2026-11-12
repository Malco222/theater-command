// The campaign rules: the map, who is at war, how armies move and are supplied, how a
// battle is auto-resolved, and how a nation falls. The same battle maths will sit under
// the playable 3D battles later, so that skipping a battle and fighting it are the same fight.

import mapData from '../generated/campaign-map.json';
import nationsData from '../../data/campaign/nations.json';
import setup from '../../data/campaign/setup-1939.json';
import { Rng, hashString } from '../core/rng';
import { clamp } from '../core/math';
import { turnToDate, type TicketLine, type VictoryLevel } from '../core/ticket';
import { BUDGET, PRODUCTION, budgeted, canBuildYet, deliver, firstCorpsNumber, income, upkeepTotal } from './economy';
import { DIPLOMACY, homeOpinion, pcIncome } from './diplomacy';
import { afterBattle, cmdAp, cmdAttack, cmdCutOff, cmdDefence, cmdLine, cmdLoss, cmdRally, cmdSteady, cmdWinter, commandersIn, generalOf, monthlyCommand, seat } from './commanders';
import { AIR, airOnFall, airUpkeep, setupAir, skyOver } from './air';
import { researchTick, setupResearch, techMul, techNotes } from './research';
import { fleetsOnFall, navalUpkeep, openSeas, openingStations, seaOpenAt, setupNavy, shipCorps } from './navy';
import { drafted, newStories, storyAttack, storyBattle, storyDefence, storyFallen, storyMonth, storyWalk } from './stories';
import eastData from '../../data/campaign/east.json';
import arsenalData from '../../data/campaign/arsenal.json';
import { barred, combinedArms, groundAttack, groundDefence, groundLine, groundMarch, groundRules, groundSays, isFoot } from './ground';
import { ENGINEERING, blocked, bridgeDown, bridgesOnCapture, downBetween, noCrossing, riverBetween, riverName } from './rivers';
import { engineersMonth, liftMines, minesAt, sappers } from './engineers';
import desertData from '../../data/campaign/desert.json';
import theatreData from '../../data/campaign/theatres.json';

/** Rules for campaigning deep inside the Soviet Union. See data/campaign/east.json. */
export const EASTERN = eastData as unknown as {
  deepOwners: string[]; mud: number[]; lineDefence: number; stepFresh: number; stepConverted: number; convertAfter: number; badWeather: number;
  bands: [number, number][]; drag: [number, number][]; roads: number; winter: { months: number[]; attack: number; defence: number; attrition: number; rest: number; hardy: string[] };
  mobilisation: Record<string, { perMonth: number[]; after: number; divs: number; kind: Kind; xp: number; name: string; works?: [number, string, number][] }>;
  works?: [number, string, number, string[], string, { taken?: boolean; port?: boolean; vs?: string[] }?][];
};
/** Rules for campaigning in the desert. See data/campaign/desert.json. */
export const DESERT = desertData as unknown as {
  terrain: string; step: number; port: number; ports: Record<string, number>; bands: [number, number][]; lineDefence: number; open: number; stores: number; retreat: [number, number]; replace: number; drag: [number, number][];
  route: { id: string; feeds: string[]; perTenth: number };
  works?: [number, string, number, string[], string][];
};
import {
  FORT_MUL, KIND, MEN_PER_DIV, STACK, TERRAIN_DEF,
  type BattleReport, type Corps, type Field, type Game, type Headline, type Kind, type Nation, type Region, type ReportUnit,
} from './types';

const ORDER = ['GER', 'ITA', 'SVK', 'HUN', 'SOV', 'POL', 'FRA', 'UK', 'USA', 'BEL', 'NED', 'LUX', 'DEN', 'NOR'];
const EAST = new Set(['POL', 'SOV', 'LIT', 'LAT', 'EST', 'ROM', 'FIN']);

export function createGame(seed: number, human: string | null): Game {
  const md = mapData as any;
  // Which side theatre each region belongs to. A theatre with an opening month is ordinary ground until then (see openTheatres).
  const theatreAt = (name: string): number => { const k = THEATRE_OF.get(name) ?? -1; return k >= 0 && THEATRE_LIST[k].opens === undefined ? k : -1; };
  const regions: Region[] = md.regions.map((r: any): Region => ({
    id: r.id, name: r.name, owner: r.owner, ctrl: r.owner, terrain: r.terrain, vp: r.vp, fac: r.fac,
    // A depot on the coast is a harbour too. One inland feeds an army but takes no ships.
    cap: !!r.flags.cap, port: !!r.flags.port || (!!r.flags.depot && (r.seas ?? []).length > 0),
    fort: r.flags.fort ? { lv: r.flags.fort[0], vs: r.flags.fort.slice(1) } : null,
    vichy: !!r.flags.vichy, sov: !!r.flags.sov, oil: !!r.flags.oil, overseas: !!r.flags.col, theatre: theatreAt(r.name),
    x: r.x, y: r.y, nb: r.nb, polys: r.polys, units: [], seas: r.seas ?? [], since: 0, prev: null,
  }));
  const byName: Record<string, Region> = {};
  for (const r of regions) byName[r.name] = r;
  const nations: Record<string, Nation> = {};
  for (const [id, d] of Object.entries(nationsData as Record<string, any>)) {
    if (id.startsWith('_')) continue;
    const cap = regions.find((r) => r.owner === id && r.cap);
    nations[id] = {
      id, name: d.name, adj: d.adj, color: d.color, quality: d.quality, air: d.air, resilience: d.resilience, align: d.align,
      blitz: !!d.blitz, slowCommand: !!d.slowCommand, maxAttack: d.maxAttack ?? 0, seaSupply: !!d.seaSupply, sealift: d.sealift ?? 0,
      atWar: [], alive: !d.dormant, dormant: !!d.dormant, offmap: d.offmap ?? null, capital: cap ? cap.id : -1, pp: 0, queue: [], corpsNo: firstCorpsNumber(id),
      opinion: homeOpinion(id, d.align), home: homeOpinion(id, d.align), pc: 0, armyPeak: 0, offStart: {}, lastFight: {}, lab: null,
    };
  }
  const order = [...ORDER, ...Object.keys(nations).filter((n) => !ORDER.includes(n))];
  const g: Game = {
    seed, rng: new Rng(seed), turn: 1, phase: 0, order, nations, regions, byName, corps: [], generals: [], nextCorps: 1, nextBuild: 1, human,
    reports: [], headlines: [], flags: {}, supplyDirty: true, supply: {}, reach: {}, capDist: {}, week: 1,
    seas: [], gates: [], fleets: [], nextFleet: 1, seaReports: [], convoys: {},
    airFleets: [], nextAir: 1, airReports: [], airPool: {}, sky: {}, raids: {}, stories: newStories(),
    links: md.links, width: md.width, height: md.height, land: md.land,
  };
  for (const row of (setup as any).corps) addCorps(g, row[0], row[1], row[2], row[3], row[4], row[5] ?? {});
  setupNavy(g);
  setupAir(g);
  setupResearch(g);
  // Depots: home regions that can feed an army even when cut off from the capital.
  (md.regions as any[]).forEach((r) => { if (r.flags.depot) g.flags['depot:' + r.id] = 1; });
  for (const n of Object.values(nations)) {
    if (n.capital < 0) continue;
    const d = new Int16Array(regions.length).fill(999);
    d[n.capital] = 0;
    const q = [n.capital];
    for (let i = 0; i < q.length; i++) for (const j of regions[q[i]].nb) if (d[j] === 999) { d[j] = d[q[i]] + 1; q.push(j); }
    g.capDist[n.id] = d;
  }
  // 1 September 1939.
  setWar(g, 'GER', 'POL');
  setWar(g, 'SVK', 'POL');
  setWar(g, 'UK', 'GER');
  setWar(g, 'FRA', 'GER');
  nations.GER.offStart.POL = 1;
  headline(g, 'war', 'Germany invades Poland. Britain and France declare war on Germany.', 'GER');
  for (const n of Object.values(nations)) n.armyPeak = armySize(g, n.id);
  openingStations(g);
  computeSupply(g);
  return g;
}

export function addCorps(g: Game, nat: string, name: string, kind: Kind, divs: number, region: string | number, o: any): Corps {
  const reg = typeof region === 'number' ? g.regions[region] : g.byName[region];
  const c: Corps = {
    id: g.nextCorps++, nat, name, kind, divs, str: 1, org: 1, xp: o.xp ?? 0.15, reg: reg.id, ap: 4, apMax: 4,
    general: null, hold: !!o.hold, dyle: o.dyle ?? null, supplied: true, fought: false, dead: false,
  };
  if (o.airborne) c.airborne = true;
  // Where and when it was raised: the first lines of its record.
  c.born = g.turn;
  if (reg.owner === nat) c.home = reg.id;
  g.corps.push(c);
  reg.units.push(c);
  if (o.gen) seat(g, c, o.gen, o.trait);
  return c;
}

/** A country's name as it reads in a sentence: "the United States", "Germany". */
export function theName(n: Nation, start = false): string {
  return /^(United|Soviet|Netherlands)/.test(n.name) ? `${start ? 'The' : 'the'} ${n.name}` : n.name;
}

export function headline(g: Game, kind: Headline['kind'], text: string, nat?: string, region?: number): void {
  g.headlines.push({ turn: g.turn, week: g.week, text, kind, nat, region });
}

// ---- Who stands where ---------------------------------------------------------------

export function bloc(n: Nation): 'allies' | 'axis' | null {
  if (n.align === 'allies' || n.align === 'allies_cobelligerent') return 'allies';
  if (n.align === 'axis' || n.align === 'axis_cobelligerent') return 'axis';
  return null;
}

export function allied(g: Game, a: string, b: string): boolean {
  if (a === b) return true;
  const A = g.nations[a], B = g.nations[b];
  if (!A || !B) return false;
  const ba = bloc(A);
  return ba !== null && ba === bloc(B);
}

export function atWar(g: Game, a: string, b: string): boolean {
  return g.nations[a].atWar.includes(b);
}

function setWar(g: Game, a: string, b: string): void {
  const A = g.nations[a], B = g.nations[b];
  if (!A.atWar.includes(b)) A.atWar.push(b);
  if (!B.atWar.includes(a)) B.atWar.push(a);
}

export function declareWar(g: Game, a: string, b: string): void {
  const A = g.nations[a], B = g.nations[b];
  if (!A.alive || !B.alive || atWar(g, a, b) || allied(g, a, b)) return;
  setWar(g, a, b);
  A.offStart[b] = g.turn;
  g.flags[`war:${a}:${b}`] = g.turn;
  // A neutral that leans toward a bloc and attacks that bloc's enemy becomes its co-belligerent.
  if (A.align === 'leaning_axis' || A.align === 'leaning_allies') {
    const side = A.align === 'leaning_axis' ? 'axis' : 'allies';
    if (B.atWar.some((x) => x !== a && bloc(g.nations[x]) === side)) {
      A.align = side === 'axis' ? 'axis_cobelligerent' : 'allies_cobelligerent';
      A.opinion = A.home = side === 'axis' ? 100 : -100;
      g.supplyDirty = true;
    }
  }
  // A neutral that is attacked joins the other side.
  const ba = bloc(A);
  if (bloc(B) === null && ba) {
    B.align = ba === 'axis' ? 'allies' : 'axis_cobelligerent';
    B.opinion = B.home = ba === 'axis' ? -100 : 100;
  }
  headline(g, 'war', `${theName(A, true)} declares war on ${theName(B)}.`, a);
  if (A.blitz && !g.flags[`tempo:${a}:${g.turn}`]) {
    g.flags[`tempo:${a}:${g.turn}`] = 1;
    for (const c of g.corps) if (c.nat === a && KIND[c.kind].mobile && g.order[g.phase] === a) { c.ap += 1; c.apMax += 1; }
  }
  g.supplyDirty = true;
  reactionMoves(g, a, b);
}

/** Armies with standing orders to go to a neighbour's aid march the day that neighbour is attacked. */
function reactionMoves(g: Game, aggressor: string, victim: string): void {
  let moved = 0;
  for (const c of g.corps) {
    if (!c.dyle || c.nat === g.human || !atWar(g, c.nat, aggressor)) continue;
    const to = g.byName[c.dyle];
    if (!to || to.owner !== victim || !allied(g, c.nat, to.ctrl)) continue;
    if (hostileUnits(g, to, c.nat).length || friendlyUnits(g, to, c.nat).length >= STACK) continue;
    place(g, c, to);
    c.dyle = null;
    moved++;
  }
  if (moved) headline(g, 'event', `Allied armies cross the frontier into ${g.nations[victim].name} to meet the attack.`, victim);
}

export const hostileUnits = (g: Game, r: Region, nat: string): Corps[] => r.units.filter((u) => atWar(g, u.nat, nat));
export const friendlyUnits = (g: Game, r: Region, nat: string): Corps[] => r.units.filter((u) => allied(g, u.nat, nat));

/** The army at home and on the Continent, in divisions. Troops in the colonies are not there to defend the homeland, and do not count. */
export function armySize(g: Game, nat: string): number {
  let s = 0;
  for (const c of g.corps) if (c.nat === nat && !g.regions[c.reg].overseas) s += c.divs * c.str;
  return s;
}

/** Every division a nation has in the field, wherever it stands. */
export function fieldArmy(g: Game, nat: string): number {
  let s = 0;
  for (const c of g.corps) if (c.nat === nat) s += c.divs * c.str;
  return s;
}

/** True where the desert's supply rules apply: desert ground, and the overseas coast that feeds it. */
export const inDesert = (r: Region): boolean => r.overseas || r.terrain === DESERT.terrain;

export function industry(g: Game, nat: string): number {
  let s = 0;
  for (const r of g.regions) if (r.owner === nat && r.ctrl === nat) s += r.fac;
  return s;
}

export function freshOffensive(g: Game, nat: string, enemy?: string): boolean {
  const n = g.nations[nat];
  if (!n.blitz) return false;
  const list = enemy ? [enemy] : n.atWar;
  return list.some((e) => n.offStart[e] !== undefined && g.turn - n.offStart[e] <= 1);
}

export function weather(g: Game): { mud: boolean; winter: boolean; text: string } {
  const m = turnToDate(g.turn).month;
  if (EASTERN.mud.includes(m)) return { mud: true, winter: false, text: 'Mud in the east slows every army there.' };
  if (EASTERN.winter.months.includes(m)) return { mud: false, winter: true, text: 'Winter in the east: armies without winter kit fight and march badly there.' };
  return { mud: false, winter: false, text: 'Clear campaigning weather.' };
}

/** True when this corps is campaigning on Soviet ground in winter without the equipment for it. */
export function frozen(g: Game, nat: string, r: Region): boolean {
  return weather(g).winter && EASTERN.deepOwners.includes(r.owner) && !EASTERN.winter.hardy.includes(nat);
}

/**
 * Extra movement cost for pushing on beyond the railheads: fuel and shells arrive slower than the
 * tanks can drive. It is paid on enemy ground and ground taken this month; marching up behind
 * the front through country already held is an ordinary march.
 */
export function lineDrag(g: Game, c: Corps, to: Region): number {
  if (to.terrain === DESERT.terrain) {
    // In the desert the same holds for everyone: the further from the port, the shorter each bound.
    if (allied(g, c.nat, to.ctrl) && to.since < g.turn) return 0;
    ensureSupply(g);
    const reach = g.reach[c.nat];
    if (!reach) return 0;
    const length = allied(g, c.nat, to.ctrl) ? reach[to.id] : reach[c.reg] + DESERT.step;
    for (const [max, extra] of DESERT.drag) if (length <= max) return extra;
    return 0;
  }
  if (!EASTERN.deepOwners.includes(to.owner) || EASTERN.deepOwners.includes(c.nat)) return 0;
  if (allied(g, c.nat, to.ctrl) && to.since < g.turn) return 0;
  ensureSupply(g);
  const reach = g.reach[c.nat];
  if (!reach) return EASTERN.roads;
  let length = reach[to.id];
  const roads = EASTERN.roads;
  if (!allied(g, c.nat, to.ctrl)) {
    const w = weather(g);
    length = reach[c.reg] + EASTERN.stepFresh * (w.mud || w.winter ? EASTERN.badWeather : 1);
  }
  for (const [max, extra] of EASTERN.drag) if (length <= max) return roads + extra;
  return roads;
}

/** How strong an attack is at the end of this corps' supply line: 1 at the frontier, less deep inside the Soviet Union. */
export function lineStrength(g: Game, c: Corps): { mul: number; length: number } {
  const length = g.reach[c.nat]?.[c.reg] ?? 0;
  // In the desert the line is counted from the nearest working port, and has its own scale.
  for (const [max, mul] of inDesert(g.regions[c.reg]) ? DESERT.bands : EASTERN.bands) if (length <= max) return { mul: cmdLine(g, c, mul), length };
  return { mul: 1, length };
}

export function moveCost(g: Game, c: Corps, to: Region, from = c.reg): number {
  // Marching through country that has been friendly since before this month is quick for anyone.
  const road = allied(g, c.nat, to.ctrl) && to.since < g.turn;
  let cost = road ? 1 : KIND[c.kind].move;
  // Off the roads every arm has ground it cannot cross quickly (the grand campaign's ground rules).
  if (!road) cost += groundMarch(g, c.kind, to);
  if (groundRules(g)) {
    // A river whose bridges are down is crossed in boats, and an enemy's minefields are felt for yard by yard.
    if (downBetween(g, from, to.id)) cost += ENGINEERING.blown.march;
    if (!allied(g, c.nat, to.ctrl) && minesAt(g, to)) cost += ENGINEERING.mines.march;
  }
  if (to.terrain === 'mountain' && c.kind !== 'mtn') cost += 1;
  if (to.terrain === 'marsh') cost += 1;
  if (weather(g).mud && EAST.has(to.owner)) cost += 1;
  if (frozen(g, c.nat, to) && cmdWinter(g, c) >= 1) cost += 1;
  cost += lineDrag(g, c, to);
  return cost;
}

// ---- Supply -------------------------------------------------------------------------

export function computeSupply(g: Game): void {
  const active = new Set(g.corps.map((c) => c.nat));
  // The seas each nation's supply ships can still use, worked out once: an ally's overseas ports feed its friends too.
  const lanes = new Map<string, Uint8Array | null>();
  const lanesOf = (id: string): Uint8Array | null => {
    if (!lanes.has(id)) { const n = g.nations[id]; lanes.set(id, n && (n.seaSupply || n.sealift > 0) ? openSeas(g, id) : null); }
    return lanes.get(id)!;
  };
  const withShips = Object.values(g.nations).filter((n) => n.alive && (n.seaSupply || n.sealift > 0)).map((n) => n.id);
  const shippers = (id: string): string[] => withShips.filter((a) => a !== id && allied(g, id, a));
  for (const id of active) {
    const ok = new Uint8Array(g.regions.length);
    const q: number[] = [];
    // Where the supply comes from, and in the desert how much of a head start each base gives.
    const head = new Float32Array(g.regions.length).fill(-1);
    const sunk = convoyDrag(g, id);
    // A nation with ships can also be fed through any friendly port its supply ships can still reach.
    const open = lanesOf(id);
    for (const r of g.regions) {
      if (!allied(g, id, r.ctrl)) continue;
      // An army is fed from its own country: its capital, its big industrial regions, its depots.
      const home = r.ctrl === id && r.owner === id;
      const depot = home && (r.cap || r.fac >= 3 || !!g.flags['depot:' + r.id]);
      let quay = !!open && r.port && r.seas.some((z) => open[z]);
      // Overseas, an ally's working port feeds the troops who came to fight beside him.
      if (!quay && r.overseas && r.port && r.ctrl !== id) { const theirs = lanesOf(r.ctrl); quay = !!theirs && r.seas.some((z) => theirs[z]); }
      // In a side theatre the allies share their shipping: a port any of them can still reach feeds them all.
      if (!quay && r.theatre >= 0 && r.port) quay = shippers(id).some((a) => { const theirs = lanesOf(a); return !!theirs && r.seas.some((z) => theirs[z]); });
      // A fortified port overseas keeps stores: with the sea behind it closed, it feeds its garrison a while longer.
      if (r.overseas && r.port && r.fort) {
        if (quay) g.flags[`stores:${r.id}`] = g.turn;
        else if (g.turn - (g.flags[`stores:${r.id}`] ?? -99) <= DESERT.stores) quay = true;
      }
      if (!depot && !quay) continue;
      ok[r.id] = 1; q.push(r.id);
      head[r.id] = depot || !r.overseas ? 0 : (DESERT.ports[r.name] ?? DESERT.port) + sunk;
    }
    for (let i = 0; i < q.length; i++) {
      for (const j of g.regions[q[i]].nb) {
        if (ok[j] || !allied(g, id, g.regions[j].ctrl)) continue;
        ok[j] = 1; q.push(j);
      }
    }
    g.supply[id] = ok;
    g.reach[id] = reachInto(g, id, ok);
    desertLines(g, ok, head, g.reach[id]);
  }
  // An army at peace is fed by its own country wherever it stands.
  for (const c of g.corps) c.supplied = !g.nations[c.nat].atWar.length || !!g.supply[c.nat]?.[c.reg];
  g.supplyDirty = false;
}

/** How much longer the desert supply line has grown for a nation fed across the sea, from the cargoes sunk on the way last month. */
export function convoyDrag(g: Game, id: string): number {
  if (!DESERT.route.feeds.includes(id)) return 0;
  const m = g.convoys[DESERT.route.id];
  return m && g.turn - m.turn <= 1 ? DESERT.route.perTenth * 10 * (m.loss ?? 0) : 0;
}

/**
 * The length of the supply line in the desert and along the overseas coast: counted from the
 * nearest working base, a step for every region of desert on the way. Written into the same
 * table as the line inside the Soviet Union; the two never overlap.
 */
function desertLines(g: Game, ok: Uint8Array, head: Float32Array, out: Float32Array): void {
  const d = new Float32Array(g.regions.length).fill(1e6);
  const q: number[] = [];
  for (let i = 0; i < head.length; i++) if (head[i] >= 0) { d[i] = head[i]; q.push(i); }
  for (let i = 0; i < q.length; i++) {
    const a = q[i];
    for (const j of g.regions[a].nb) {
      if (!ok[j]) continue;
      const far = d[a] + (g.regions[j].terrain === DESERT.terrain ? DESERT.step : 0);
      if (far < d[j] - 1e-6) { d[j] = far; q.push(j); }
    }
  }
  for (const r of g.regions) if (inDesert(r) && ok[r.id] && d[r.id] < 1e6) out[r.id] = d[r.id];
}

/**
 * The length of a nation's supply line in every region it can reach inside the Soviet Union,
 * counted from the frontier railheads. Regions held long enough to relay the track count for less.
 */
function reachInto(g: Game, id: string, ok: Uint8Array): Float32Array {
  const d = new Float32Array(g.regions.length);
  if (EASTERN.deepOwners.includes(id)) return d; // at home on their own railways
  const deep = (r: Region): boolean => EASTERN.deepOwners.includes(r.owner);
  const w = weather(g);
  const bad = w.mud || w.winter ? EASTERN.badWeather : 1;
  d.fill(1e6);
  const q: number[] = [];
  for (const r of g.regions) if (ok[r.id] && !deep(r)) { d[r.id] = 0; q.push(r.id); }
  for (let i = 0; i < q.length; i++) {
    const a = q[i];
    for (const j of g.regions[a].nb) {
      const r = g.regions[j];
      if (!ok[j] || !deep(r)) continue;
      const step = (g.turn - r.since >= EASTERN.convertAfter ? EASTERN.stepConverted : EASTERN.stepFresh) * bad;
      if (d[a] + step < d[j] - 1e-6) { d[j] = d[a] + step; q.push(j); }
    }
  }
  for (let i = 0; i < d.length; i++) if (d[i] >= 1e6) d[i] = 0;
  return d;
}

export function ensureSupply(g: Game): void {
  if (g.supplyDirty) computeSupply(g);
}

// ---- Battles ------------------------------------------------------------------------

/** How many corps can attack a region from each side: there is only so much road and front. */
export function frontage(r: Region): number {
  return r.terrain === 'plains' ? 4 : r.terrain === 'mountain' ? 2 : 3;
}

/** Trims an attacking force to what the roads into the target can carry, strongest first. */
export function limitAttackers(g: Game, corps: Corps[], target: Region): Corps[] {
  const cap = frontage(target);
  const used: Record<number, number> = {};
  const out: Corps[] = [];
  // A staff that cannot yet run a big battle brings fewer corps into it, however many stand ready.
  const most = corps.length ? g.nations[corps[0].nat].maxAttack || 99 : 99;
  for (const c of [...corps].sort((x, y) => corpsBase(g, y) * KIND[y.kind].atk - corpsBase(g, x) * KIND[x.kind].atk)) {
    if (out.length >= most) break;
    if (blocked(g, c, target)) continue; // tanks have no way up the steepest ground, and nothing on wheels crosses a river whose bridges are down
    if ((used[c.reg] ?? 0) >= cap) continue;
    used[c.reg] = (used[c.reg] ?? 0) + 1;
    out.push(c);
  }
  return out;
}

/** Which week of the month a corps reaches after spending this many more action points. */
export function weekOf(c: Corps, cost: number): number {
  return clamp(Math.ceil(((c.apMax - c.ap + cost) / Math.max(1, c.apMax)) * 4), 1, 4);
}

export function corpsBase(g: Game, c: Corps): number {
  // Tanks and lorries, or rifles and guns: a corps is as good as the designs it has been issued.
  const kit = techMul(g, c.nat, c.kind === 'arm' || c.kind === 'mot' ? 'armour' : 'infantry');
  return c.divs * c.str * Math.sqrt(0.15 + 0.85 * c.org) * g.nations[c.nat].quality * (0.85 + 0.4 * c.xp) * (c.supplied ? 1 : cmdCutOff(g, c)) * kit;
}

/** Share of the usual losses a defender takes when the attack is beaten off. */
const HELD_LOSS = 0.6;
/** How much stronger troops defend from trenches they have had a month to dig. */
export const DUG_IN = 1.25;
/** What artillery does in a land battle (the grand campaign's arsenal). */
export const GUNS = (arsenalData as unknown as { guns: { soften: number; softenMost: number; thicken: number; thickenMost: number } }).guns;
/** The weight of guns in a force: its artillery divisions, at the strength they stand at. */
const gunsIn = (cs: Corps[]): number => cs.reduce((s, c) => s + (c.kind === 'art' ? c.divs * c.str * (c.supplied ? 1 : 0.5) : 0), 0);

export const men = (c: Corps): number => Math.round(c.divs * c.str * MEN_PER_DIV);

function airOf(g: Game, nats: Set<string>, enemies: Set<string>): number {
  let s = 0;
  for (const id of nats) s += g.nations[id].air;
  for (const n of Object.values(g.nations)) {
    // An ally at war with the same enemy lends a share of its air force, if it has airfields on the map to fly from.
    if (!n.alive || nats.has(n.id) || n.capital < 0) continue;
    if ([...nats].some((a) => allied(g, a, n.id)) && [...enemies].some((e) => atWar(g, n.id, e))) s += n.air * 0.25;
  }
  return s;
}

export interface Assessment {
  A: number;
  D: number;
  odds: number;
  lines: TicketLine[];
  defenders: Corps[];
}

/** How an attack arrives: over land, or (sea) from landing craft at that share of its strength. */
export interface AttackOpts {
  sea?: number;
  surprise?: boolean;
  /** The landing comes from the air, not from ships. */
  air?: boolean;
  /** The most corps that follow up into the ground won. The rest stay where they attacked from. */
  occupy?: number;
}

/** Works out both sides' strength for an attack. Auto-resolve and the odds preview share it. */
export function assess(g: Game, attackers: Corps[], target: Region, opts: AttackOpts = {}): Assessment {
  ensureSupply(g);
  const attNat = attackers[0].nat;
  const defenders = hostileUnits(g, target, attNat);
  const lines: TicketLine[] = [];
  const aN = new Set(attackers.map((c) => c.nat)), dN = new Set(defenders.map((c) => c.nat));
  const defHasArmor = defenders.some((d) => d.kind === 'arm');
  // Open desert has no flanks: infantry on foot outside prepared works can be driven round by anything on wheels or tracks.
  const openDesert = target.terrain === DESERT.terrain && !target.fort && attackers.some((c) => KIND[c.kind].mobile);
  let turned = false;
  const tempo = [...dN].some((d) => freshOffensive(g, attNat, d)) || freshOffensive(g, attNat, target.ctrl);
  let A = 0, fortFaced = 0, armorShock = false, armorStuck = false, longest = 0, lineMul = 1, cold = false, coldDef = false, dugIn = 0;
  const sources = new Set<number>();
  for (const c of attackers) sources.add(c.reg);
  // Defenders with nowhere to fall back to: every way out is enemy ground or the attack itself.
  const pocket = defenders.length > 0 && !target.nb.some((j) => !sources.has(j) && allied(g, defenders[0].nat, g.regions[j].ctrl) && !hostileUnits(g, g.regions[j], defenders[0].nat).length);
  let cmdA = 0, cmdD = 0, rawA = 0, rawD = 0, trap = false, proudA = 1, proudD = 1;
  // Massed guns: in the attack they break up works and trenches, in the defence they thicken the line they stand in.
  // Only the grand campaign raises artillery, so a battle without any is worked out exactly as it always was.
  const gunsA = gunsIn(attackers), gunsD = gunsIn(defenders);
  const soften = gunsA ? Math.min(GUNS.softenMost, GUNS.soften * gunsA) : 0;
  const thicken = gunsD ? Math.min(GUNS.thickenMost, GUNS.thicken * gunsD) : 0;
  // The grand campaign's ground rules: each arm on the ground that suits it or does not, and tanks and infantry
  // worth more together than apart. A historical scenario has none of this and is worked out as it always was.
  const wide = groundRules(g);
  const land = opts.sea === undefined;
  // Tanks that cannot get over a river whose bridges are down are no help to the infantry that does.
  const armsA = wide ? combinedArms(g, land ? attackers.filter((c) => !noCrossing(g, c.kind, c.reg, target.id)) : attackers, target, false) : null, armsD = wide ? combinedArms(g, defenders, target, true) : null;
  let tooSteep = 0, noBridge = 0, overBridges = 0, inBoats = 0;
  const rivers = new Set<string>();
  // Engineers in the assault breach the works, clear lanes through the mines and put the infantry over a bridgeless river.
  const sap = wide ? sappers(attackers.filter((c) => !land || !blocked(g, c, target))) : 0;
  const breach = sap ? Math.min(ENGINEERING.breach.most, ENGINEERING.breach.perDiv * sap) : 0;
  const mineLv = wide ? minesAt(g, target) : 0;
  const mined = mineLv ? 1 - ENGINEERING.mines.attack * mineLv * (1 - Math.min(1, sap)) : 1;
  for (const c of attackers) {
    if (wide && barred(g, c, target)) { tooSteep++; continue; }
    if (wide && land && noCrossing(g, c.kind, c.reg, target.id)) { noBridge++; continue; }
    let p = corpsBase(g, c) * KIND[c.kind].atk;
    if (wide) {
      const suits = groundAttack(g, c.kind, target);
      if (suits !== 1) p *= suits;
      if (armsA) p *= c.kind === 'arm' ? armsA.armour : isFoot(c) ? armsA.infantry : 1;
      const x = land ? riverBetween(g, c.reg, target.id) : null;
      if (x) {
        rivers.add(riverName(x));
        if (bridgeDown(g, x)) { p *= sap ? ENGINEERING.blown.withEngineers : ENGINEERING.blown.attack; inBoats++; }
        else { p *= ENGINEERING.river.attack; overBridges++; }
      }
      if (mined !== 1) p *= mined;
    }
    if (c.kind === 'arm') {
      if (target.terrain === 'mountain' || target.terrain === 'urban' || target.terrain === 'marsh') { p *= 0.6; armorStuck = true; }
      else if (!defHasArmor) { p *= 1.25; armorShock = true; }
    }
    const lead = cmdAttack(g, c, pocket);
    if (pocket && lead > cmdAttack(g, c, false)) trap = true;
    if (tempo && KIND[c.kind].mobile) p *= 1.2;
    if (g.nations[c.nat].slowCommand) p *= 0.85;
    p *= techMul(g, c.nat, 'doctrine');
    if (opts.sea !== undefined) p *= opts.sea;
    const line = lineStrength(g, c);
    if (line.mul < 1) { p *= line.mul; longest = Math.max(longest, line.length); lineMul = Math.min(lineMul, line.mul); }
    if (frozen(g, c.nat, target)) { p *= 1 - (1 - EASTERN.winter.attack) * cmdWinter(g, c); cold = true; }
    const from = g.regions[c.reg];
    // What the guns and the engineers between them leave of the works. With neither, the works count in full, as they always did.
    const eased = breach ? 1 - (1 - soften) * (1 - breach) : soften;
    if (target.fort && target.fort.vs.includes(from.owner)) { p /= eased ? 1 + (FORT_MUL[target.fort.lv] - 1) * (1 - eased) : FORT_MUL[target.fort.lv]; fortFaced++; }
    // The player's honoured corps, a corps fighting for the place it was raised in, and one that has its old enemy in front of it.
    const proud = storyAttack(g, c, target, defenders);
    if (proud !== 1) { p *= proud; proudA = Math.max(proudA, proud); }
    rawA += p;
    cmdA += p * lead;
    A += p * lead;
  }
  const flank = Math.min(1.3, 1 + 0.1 * (sources.size - 1));
  A *= flank;
  let D = 0, thin = 1;
  for (const d of defenders) {
    let p = corpsBase(g, d) * KIND[d.kind].def * TERRAIN_DEF[target.terrain];
    if (wide) {
      const suits = groundDefence(g, d.kind, target);
      if (suits !== 1) p *= suits;
      if (armsD) p *= d.kind === 'arm' ? armsD.armour : isFoot(d, true) ? armsD.infantry : 1;
    }
    if (d.dug) { p *= soften ? 1 + (DUG_IN - 1) * (1 - soften) : DUG_IN; dugIn++; }
    if (thicken && d.kind !== 'art') p *= 1 + thicken;
    if (frozen(g, d.nat, target)) { p *= 1 - (1 - EASTERN.winter.defence) * cmdWinter(g, d); coldDef = true; }
    if (openDesert && !KIND[d.kind].mobile && d.kind !== 'fort') { p *= DESERT.open; turned = true; }
    const dl = lineStrength(g, d).mul;
    const share = inDesert(target) ? DESERT.lineDefence : EASTERN.lineDefence;
    if (dl < 1) { p *= 1 - (1 - dl) * share; thin = Math.min(thin, 1 - (1 - dl) * share); }
    const proud = storyDefence(g, d, target, attackers);
    if (proud !== 1) { p *= proud; proudD = Math.max(proudD, proud); }
    const lead = cmdDefence(g, d);
    rawD += p;
    cmdD += p * lead;
    D += p * lead;
  }
  const aAir = airOf(g, aN, dN), dAir = airOf(g, dN, aN);
  let airEdge = (aAir - dAir) / (aAir + dAir + 1);
  // Whoever has won the sky over this ground in the air war has it in the land battle too.
  const sky = skyOver(g, target.id);
  const skyWon = sky !== null && [...aN].some((n) => allied(g, n, sky));
  if (skyWon) airEdge = Math.min(1, airEdge + AIR.sky.land);
  A *= 1 + 0.3 * airEdge;
  D *= 1 - 0.2 * airEdge;
  D = Math.max(D, 0.01);

  const sum = (cs: Corps[]) => cs.reduce((s, c) => s + men(c), 0).toLocaleString('en-US');
  lines.push({ label: 'Forces', value: `${attackers.length} corps, ${sum(attackers)} men, against ${defenders.length} corps, ${sum(defenders)} men` });
  lines.push({ label: 'Terrain', value: target.terrain[0].toUpperCase() + target.terrain.slice(1), effect: TERRAIN_DEF[target.terrain] > 1 ? `Defenders fight ${Math.round((TERRAIN_DEF[target.terrain] - 1) * 100)}% stronger` : 'Open ground, no help to the defender' });
  if (wide) {
    const a = groundLine(g, attackers.filter((c) => !barred(g, c, target)), target, false), d = groundLine(g, defenders, target, true);
    const fx = [tooSteep ? `${tooSteep === 1 ? 'one armoured corps' : `${tooSteep} armoured corps`} cannot climb here and ${tooSteep === 1 ? 'takes' : 'take'} no part` : '', a, d].filter(Boolean).join('; ');
    if (fx) lines.push({ label: 'Ground', value: groundSays(g, target), effect: fx[0].toUpperCase() + fx.slice(1) });
    const pc = (m: number): number => Math.round(Math.abs(m - 1) * 100);
    if (armsA) {
      const lone = armsA.alone < 1, close = target.terrain === 'urban' ? 'city streets' : 'forest';
      const together = armsA.foot > 0;
      const tank = armsA.armour !== 1 ? `tank attack ${armsA.armour > 1 ? 'up' : 'down'} ${pc(armsA.armour)}%` : '';
      const foot = together ? `infantry attack up ${pc(armsA.infantry)}%` : '';
      const fx2 = [foot, tank].filter(Boolean).join(', ');
      lines.push({
        label: 'Combined arms',
        value: !together ? (lone ? `Tanks attacking into ${close} with no infantry beside them` : 'Tanks attacking without infantry')
          : armsA.cover < 1 ? 'Tanks and infantry attack together, but the infantry is too thin to cover all the armour'
          : armsA.share < 1 ? 'Tanks and infantry attack together, with tanks enough for part of the infantry' : 'Tanks and infantry attack together',
        effect: fx2 ? fx2[0].toUpperCase() + fx2.slice(1) : 'Infantry with the tanks would make both stronger',
      });
    }
    if (armsD && (armsD.foot > 0 || armsD.alone < 1)) lines.push({
      label: 'Combined arms',
      value: armsD.foot > 0 ? 'The defenders have tanks in reserve behind their infantry' : `Tanks holding ${target.terrain === 'urban' ? 'city streets' : 'forest'} with no infantry`,
      effect: [armsD.foot > 0 ? `Infantry defence up ${pc(armsD.infantry)}%` : '', armsD.alone < 1 ? `${armsD.foot > 0 ? 'tank' : 'Tank'} defence down ${pc(armsD.alone)}%` : ''].filter(Boolean).join(', '),
    });
  }
  if (wide && (rivers.size || noBridge)) {
    const name = rivers.size ? [...rivers].join(' and the ') : riverName(riverBetween(g, attackers.find((c) => noCrossing(g, c.kind, c.reg, target.id))!.reg, target.id)!);
    const crossing = overBridges + inBoats, all = attackers.length - tooSteep;
    const fx = [
      overBridges ? `over the bridges the attack is cut to ${Math.round(ENGINEERING.river.attack * 100)}%` : '',
      inBoats ? `infantry crossing in boats fights at ${Math.round((sap ? ENGINEERING.blown.withEngineers : ENGINEERING.blown.attack) * 100)}%${sap ? ', with engineers to get it over' : ''}` : '',
      noBridge ? `${noBridge === 1 ? 'one corps of tanks, lorries or guns' : `${noBridge} corps of tanks, lorries or guns`} cannot cross and ${noBridge === 1 ? 'takes' : 'take'} no part` : '',
    ].filter(Boolean).join('; ');
    lines.push({
      label: 'River',
      value: inBoats || noBridge ? `The bridges over the ${name} are down` : crossing < all ? `${crossing} of the ${all} attacking corps cross the ${name}` : `The attack crosses the ${name}`,
      effect: fx[0].toUpperCase() + fx.slice(1),
    });
  }
  if (mineLv) lines.push({
    label: 'Minefields', value: mineLv > 1 ? 'Two belts of mines and booby traps on every road in' : 'Mines and booby traps on every road in',
    effect: mined === 1 ? 'The engineers clear lanes: the mines cost the attack nothing' : `Attack cut to ${Math.round(mined * 100)}%${sap ? ', the engineers clearing what lanes they can' : ''}`,
  });
  if (breach && target.fort && fortFaced) lines.push({ label: 'Engineers', value: 'Assault engineers go in with the attack', effect: `They breach the works: the fortifications count for ${Math.round(breach * 100)}% less` });
  if (target.fort) {
    lines.push({ label: 'Fortifications', value: `Level ${target.fort.lv}`, effect: fortFaced === attackers.length ? `Attacked head-on: the attack is cut to ${Math.round(100 / FORT_MUL[target.fort.lv])}% strength` : fortFaced > 0 ? 'Partly outflanked: only the corps attacking head-on are slowed' : 'Outflanked: the works face the other way' });
  }
  if (soften || thicken) lines.push({
    label: 'Artillery',
    value: soften && thicken ? 'Massed guns on both sides' : soften ? 'Massed guns support the attack' : 'Massed guns stand in the defence',
    effect: [
      soften ? (target.fort && fortFaced) || dugIn ? `The bombardment takes ${Math.round(soften * 100)}% off the worth of the ${target.fort && fortFaced ? (dugIn ? 'works and trenches' : 'works') : 'trenches'}` : 'There are no works or trenches here for the guns to break' : '',
      thicken ? `${soften ? 'the' : 'The'} other defenders fight ${Math.round(thicken * 100)}% stronger under their own guns` : '',
    ].filter(Boolean).join('; '),
  });
  if (dugIn) lines.push({ label: 'Dug in', value: dugIn === defenders.length ? 'The defenders have had a month to entrench' : `${dugIn} of the ${defenders.length} defending corps have had a month to entrench`, effect: `Entrenched troops defend ${Math.round((DUG_IN - 1) * 100)}% stronger` });
  const aUns = attackers.filter((c) => !c.supplied).length, dUns = defenders.filter((c) => !c.supplied).length;
  lines.push({ label: 'Supply', value: `${aUns ? `${aUns} attacking corps cut off` : 'Attackers in supply'}; ${dUns ? `${dUns} defending corps cut off` : 'defenders in supply'}`, effect: aUns || dUns ? 'Corps that are cut off fight at half strength' : undefined });
  lines.push({ label: 'Air', value: skyWon ? `The ${g.nations[sky!].adj} air force has won the sky here` : airEdge > 0.15 ? 'Attacker holds the sky' : airEdge < -0.15 ? 'Defender holds the sky' : 'Contested', effect: Math.abs(airEdge) > 0.05 ? `${airEdge > 0 ? 'Attack' : 'Defence'} strengthened by about ${Math.round(Math.abs(airEdge) * 30)}%` : undefined });
  const gens = commandersIn(g, attackers), dgens = commandersIn(g, defenders);
  if (gens || dgens) {
    const up = rawA > 0 ? Math.round((cmdA / rawA - 1) * 100) : 0, dup = rawD > 0 ? Math.round((cmdD / rawD - 1) * 100) : 0;
    const fx = [up ? `Attack ${up > 0 ? 'up' : 'down'} ${Math.abs(up)}%` : '', dup ? `defence ${dup > 0 ? 'up' : 'down'} ${Math.abs(dup)}%` : ''].filter(Boolean).join(', ');
    lines.push({ label: 'Commanders', value: `${gens || 'No general named'} against ${dgens || 'no general named'}`, effect: fx ? fx[0].toUpperCase() + fx.slice(1) : undefined });
  }
  // Designs ahead of or behind their time, on either side.
  const kitOf = (cs: Corps[]): Field[] => [...new Set(cs.map((c): Field => (c.kind === 'arm' || c.kind === 'mot' ? 'armour' : 'infantry')))];
  const kitA = [...techNotes(g, aN, kitOf(attackers)), ...techNotes(g, aN, ['doctrine'])], kitD = techNotes(g, dN, kitOf(defenders));
  if (kitA.length || kitD.length) {
    const up = (k: number): string => `${k > 0 ? 'up' : 'down'} ${Math.abs(k)}%`;
    lines.push({
      label: 'Equipment', value: [...kitA, ...kitD].map((x) => x.text).join('; '),
      effect: [...kitA.map((x) => `Attack ${up(x.pct)}`), ...kitD.map((x) => `Defence ${up(x.pct)}`)].join(', '),
    });
  }
  if (proudA > 1 || proudD > 1) lines.push({
    label: 'Honours', value: proudA > 1 && proudD > 1 ? 'Honoured corps on both sides of this fight' : `Corps that carry honours, are fighting for their home ground or face an old enemy are ${proudA > 1 ? 'attacking' : 'defending'} here`,
    effect: [proudA > 1 ? `Attack up as much as ${Math.round((proudA - 1) * 100)}% for those corps` : '', proudD > 1 ? `${proudA > 1 ? 'defence' : 'Defence'} up as much as ${Math.round((proudD - 1) * 100)}% for those corps` : ''].filter(Boolean).join(', '),
  });
  if (opts.sea !== undefined) lines.push(opts.air
    ? { label: 'Airborne assault', value: 'By parachute and glider, without heavy weapons', effect: `Attack cut to ${Math.round(opts.sea * 100)}%` }
    : opts.surprise
      ? { label: 'Landing', value: 'From the sea, on the day war is declared', effect: 'The defenders are caught unready: the landing fights at full strength' }
      : { label: 'Landing', value: 'From the sea, against a defended shore', effect: `Attack cut to ${Math.round(opts.sea * 100)}%` });
  if (trap) lines.push({ label: 'Encirclement', value: 'The defenders have no line of retreat', effect: 'A general who knows how to close a pocket hits 15% harder' });
  if (lineMul < 1) lines.push(attackers.some((c) => inDesert(g.regions[c.reg]))
    ? { label: 'Supply line', value: `Stretched over ${Math.round(longest)} regions of desert from the nearest working port`, effect: `Attack cut to as little as ${Math.round(lineMul * 100)}% until a port nearer the front is working` }
    : { label: 'Supply line', value: `Stretched over ${Math.round(longest)} regions of Soviet railway`, effect: `Attack cut to as little as ${Math.round(lineMul * 100)}% until the track behind is relaid` });
  if (turned) lines.push({ label: 'Open desert', value: 'Infantry on foot with no works to anchor on, against tanks and lorried troops', effect: `Foot infantry defends at ${Math.round(DESERT.open * 100)}%` });
  if (cold) lines.push({ label: 'Winter', value: 'Attackers without winter equipment', effect: `Attack cut to ${Math.round(EASTERN.winter.attack * 100)}%` });
  if (thin < 1) lines.push({ label: 'Supply line', value: 'Defenders at the end of a long supply line', effect: `Defence cut to as little as ${Math.round(thin * 100)}%` });
  if (coldDef) lines.push({ label: 'Winter', value: 'Defenders without winter equipment', effect: `Defence cut to ${Math.round(EASTERN.winter.defence * 100)}%` });
  if (tempo) lines.push({ label: 'Tempo', value: 'New offensive', effect: 'Fast troops hit 20% harder in the first two turns' });
  if (armorShock) lines.push({ label: 'Armor', value: 'Tanks against a defence without armor', effect: 'Tank attack up 25%' });
  if (armorStuck) lines.push({ label: 'Armor', value: 'Tanks in close country', effect: 'Tank attack cut to 60%' });
  if (flank > 1) lines.push({ label: 'Direction', value: `Attack from ${sources.size} sides`, effect: `Attack up ${Math.round((flank - 1) * 100)}%` });
  return { A, D, odds: A / D, lines, defenders };
}

function reportUnit(c: Corps): ReportUnit {
  return { name: c.name, nat: c.nat, kind: c.kind, divs: c.divs, men: men(c), lost: 0, fate: '' };
}

export function removeCorps(g: Game, c: Corps): void {
  c.dead = true;
  const gen = generalOf(g, c);
  if (gen) { gen.corps = -1; c.general = null; }
  const r = g.regions[c.reg];
  r.units.splice(r.units.indexOf(c), 1);
  g.corps.splice(g.corps.indexOf(c), 1);
}

/** Puts a corps down in another region without a march: rail, ship, or an order from the capital. */
export function relocate(g: Game, c: Corps, to: Region): void { place(g, c, to); }

function place(g: Game, c: Corps, to: Region): void {
  const r = g.regions[c.reg];
  r.units.splice(r.units.indexOf(c), 1);
  c.reg = to.id;
  to.units.push(c);
  // Trenches stay where they were dug.
  if (c.dug) delete c.dug;
}

function retreatTarget(g: Game, c: Corps, from: Region, banned: Set<number>): Region | null {
  if (c.kind === 'fort') return null;
  let best: Region | null = null, bs = 1e9;
  const d = g.capDist[c.nat];
  for (const j of from.nb) {
    const r = g.regions[j];
    if (banned.has(j) || !allied(g, c.nat, r.ctrl) || hostileUnits(g, r, c.nat).length) continue;
    if (friendlyUnits(g, r, c.nat).length >= STACK + 2 || barred(g, c, r)) continue;
    const abroad = g.nations[c.nat].seaSupply && r.owner !== c.nat;
    // In the desert an army falls back along its own supply line, toward the port that feeds it.
    // It also keeps out of a dead end: a place with one way out, or none, is a trap for a beaten force.
    const ways = inDesert(r) ? r.nb.filter((k) => k !== from.id && !banned.has(k) && allied(g, c.nat, g.regions[k].ctrl)).length : 2;
    const back = (inDesert(r) ? g.reach[c.nat]?.[j] ?? 0 : abroad ? (r.port ? 0 : 3) : d ? d[j] : 0) + (ways === 0 ? 4 : ways === 1 ? 1.5 : 0);
    const s = back + (g.supply[c.nat]?.[j] ? 0 : 6) + r.units.length * 0.3;
    if (s < bs) { bs = s; best = r; }
  }
  return best;
}

/** The winners move in, fastest first. */
function moveIn(g: Game, attackers: Corps[], target: Region, opts: AttackOpts, attU: ReportUnit[]): void {
  const order = attackers.map((c, i) => ({ c, i })).sort((x, y) => KIND[x.c.kind].move - KIND[y.c.kind].move || corpsBase(g, y.c) - corpsBase(g, x.c));
  let moved = 0;
  for (const { c, i } of order) {
    // A force that came by sea all goes ashore together.
    const cost = opts.sea !== undefined ? 0 : moveCost(g, c, target);
    if (moved >= (opts.occupy ?? STACK) || (moved > 0 && c.ap < cost) || (opts.sea === undefined ? blocked(g, c, target) : barred(g, c, target))) { c.ap = Math.max(0, c.ap - 1); continue; }
    place(g, c, target);
    c.ap = Math.max(0, c.ap - cost);
    attU[i].fate = 'advanced';
    moved++;
  }
}

/** The odds at which troops under orders to give ground do so: the odds that carry a position. */
export const YIELD_ODDS = 1.2;
/** What slipping away costs a corps in cohesion. */
const YIELD_ORG = 0.15;

/**
 * Troops the player has ordered to give ground rather than be broken on it. When a blow that would carry the
 * position is about to land they slip away to the best ground behind them, whole but shaken. A corps does this
 * once a month: caught again before its own month comes round, it stands. Fortress troops never leave their works,
 * and a capital defended by its own army is never given up this way.
 */
function giveWay(g: Game, a: Assessment, attackers: Corps[], target: Region): ReportUnit[] {
  const gone: ReportUnit[] = [];
  if (a.odds < YIELD_ODDS) return gone;
  // Nobody gives up their own capital without a fight, whatever the standing order says.
  if (target.cap && a.defenders.some((c) => c.nat === target.owner)) return gone;
  const banned = new Set(attackers.map((c) => c.reg));
  for (const c of [...a.defenders]) {
    if (!c.yields || c.gaveWay) continue;
    const to = retreatTarget(g, c, target, banned);
    if (!to) continue;
    const unit = reportUnit(c);
    place(g, c, to);
    c.ap = 0;
    c.gaveWay = true;
    c.org = clamp(c.org - YIELD_ORG, 0, 1);
    unit.fate = 'yielded';
    gone.push(unit);
  }
  return gone;
}

/** The attackers take ground the defenders have given up without a fight. It is filed with the battles so the player can see what happened. */
function walkIn(g: Game, attackers: Corps[], target: Region, opts: AttackOpts, a: Assessment, gone: ReportUnit[]): BattleReport {
  const attNat = attackers[0].nat;
  const AN = g.nations[attNat];
  // The offensive is counted as under way all the same: the attackers came on and the ground changed hands.
  for (const d of new Set(gone.map((u) => u.nat))) {
    if (AN.lastFight[d] === undefined || g.turn - AN.lastFight[d] >= 3) AN.offStart[d] = AN.offStart[d] !== undefined && g.turn - AN.offStart[d] <= 1 ? AN.offStart[d] : g.turn;
    AN.lastFight[d] = g.turn;
  }
  const week = Math.max(...attackers.map((c) => weekOf(c, 1)));
  g.week = week;
  const attU = attackers.map(reportUnit);
  moveIn(g, attackers, target, opts, attU);
  capture(g, target, attNat);
  g.flags.battles = Math.max(g.flags.battles ?? 0, g.reports.length) + 1;
  const rep: BattleReport = {
    id: g.flags.battles, turn: g.turn, week, region: target.id, regionName: target.name, attNat, defNat: gone[0].nat,
    attackers: attU, defenders: gone, lines: [...a.lines, { label: 'Orders', value: 'The defenders gave ground before the attack', effect: 'No battle was fought' }],
    aPower: a.A, dPower: a.D, odds: a.odds, level: 'Close victory', taken: true, attLost: 0, defLost: 0, prisoners: 0, yielded: true,
  };
  g.reports.push(rep);
  // Ground walked into in a theatre that opened late is kept out of everyone else's count of battles.
  if (lateTheatre(target)) { g.flags.lateBattles = (g.flags.lateBattles ?? 0) + 1; g.flags.lateWalks = (g.flags.lateWalks ?? 0) + 1; }
  storyWalk(g, attackers, target);
  for (const d of new Set(gone.map((u) => u.nat))) checkCollapse(g, d, attNat);
  return rep;
}

export function resolveBattle(g: Game, attackers: Corps[], target: Region, opts: AttackOpts = {}): BattleReport {
  let a = assess(g, attackers, target, opts);
  const gone = giveWay(g, a, attackers, target);
  if (gone.length) {
    if (!hostileUnits(g, target, attackers[0].nat).length) return walkIn(g, attackers, target, opts, a, gone);
    // Whoever stayed behind, in the works or under other orders, fights alone.
    a = assess(g, attackers, target, opts);
  }
  const defenders = a.defenders;
  const attNat = attackers[0].nat;
  const defNat = defenders[0]?.nat ?? target.ctrl;
  const AN = g.nations[attNat];
  // An enemy's capital, for the records of the corps that take it: known before the fight, since its fall may end the war with him.
  const enemyCapital = target.cap && atWar(g, attNat, target.owner);
  for (const d of new Set(defenders.map((c) => c.nat))) {
    if (AN.lastFight[d] === undefined || g.turn - AN.lastFight[d] >= 3) AN.offStart[d] = AN.offStart[d] !== undefined && g.turn - AN.offStart[d] <= 1 ? AN.offStart[d] : g.turn;
    AN.lastFight[d] = g.turn;
  }
  const week = Math.max(...attackers.map((c) => weekOf(c, 1)));
  g.week = week;
  // Battles overseas and in a side theatre roll their own dice, so that a fight in the desert or the Balkans never changes the luck of the main fronts.
  const aside = target.overseas || target.theatre >= 0;
  const late = lateTheatre(target);
  const count = late ? `t${target.theatre}:${g.flags[`fought:${target.theatre}`] ?? 0}` : `${(g.flags.battles ?? 0) - (g.flags.lateBattles ?? 0)}`;
  const dice = aside ? new Rng(hashString(`land:${g.seed}:${g.turn}:${target.id}:${attNat}:${count}`)) : g.rng;
  const R = a.odds * dice.range(0.8, 1.2);
  const attU = attackers.map(reportUnit), defU = defenders.map(reportUnit);
  const attGens = attackers.map((c) => generalOf(g, c)), defGens = defenders.map((c) => generalOf(g, c));
  const mobile = attackers.some((c) => KIND[c.kind].mobile);
  const defFrac = clamp(0.07 + 0.1 * R, 0.06, 0.45);
  const attFrac = clamp(0.14 / R, 0.02, 0.2);
  let attLost = 0, defLost = 0, prisoners = 0;
  attackers.forEach((c, i) => {
    const before = men(c);
    c.str *= 1 - Math.min(0.6, attFrac * cmdLoss(g, c));
    c.org = clamp(c.org - (0.1 + 0.08 / R) * cmdSteady(g, c), 0, 1);
    c.xp = Math.min(1, c.xp + 0.05);
    c.fought = true;
    attU[i].lost = before - men(c);
    attLost += attU[i].lost;
  });
  const win = R >= 1.2;
  // Troops who hold their ground come out of the fight in better shape than troops driven off it.
  const stood = win ? 1 : HELD_LOSS;
  // Men on foot driven out of open desert by a mobile enemy do not all get away.
  const caught = win && mobile && target.terrain === DESERT.terrain && !target.fort ? DESERT.retreat[R >= 2 ? 1 : 0] : 1;
  defenders.forEach((c, i) => {
    const before = men(c);
    c.str *= 1 - Math.min(0.8, defFrac * (win && mobile ? 1.3 : 1) * (KIND[c.kind].mobile ? 1 : caught) * stood * cmdLoss(g, c));
    c.org = clamp(c.org - clamp(0.18 + 0.14 * R, 0.2, 0.75) * stood * cmdSteady(g, c), 0, 1);
    c.xp = Math.min(1, c.xp + 0.04);
    c.fought = true;
    defU[i].lost = before - men(c);
    defLost += defU[i].lost;
  });

  const level: VictoryLevel = R >= 3 ? 'Heroic victory' : R >= 2 ? 'Decisive victory' : R >= 1.2 ? (attLost > defLost ? 'Pyrrhic victory' : 'Close victory') : R >= 0.8 ? 'Close defeat' : 'Decisive defeat';
  const affected = new Set(defenders.map((c) => c.nat));
  if (win) {
    const banned = new Set(attackers.map((c) => c.reg));
    defenders.forEach((c, i) => {
      const to = c.str < 0.15 ? null : retreatTarget(g, c, target, banned);
      const N = g.nations[c.nat];
      if (to) { place(g, c, to); c.ap = 0; defU[i].fate = 'retreated'; }
      else if (c.str >= 0.15 && N.seaSupply && target.port && target.owner !== c.nat && N.capital >= 0 && seaOpenAt(g, c.nat, target)) {
        // Backs to the sea: the navy takes off what it can.
        const dest = refuge(g, c, target);
        place(g, c, dest);
        c.str *= 0.75; c.org = 0.2; c.ap = 0;
        defU[i].fate = 'evacuated';
        if (!g.flags[`evac:${g.turn}`]) { g.flags[`evac:${g.turn}`] = 1; headline(g, 'event', `${N.adj} troops are lifted off the beaches at ${target.name} under fire${dest.cap ? '' : ` and taken to ${dest.name}`}.`, c.nat, target.id); }
      } else {
        const left = men(c);
        defU[i].fate = c.str < 0.15 ? 'destroyed' : 'surrendered';
        defU[i].lost += left;
        prisoners += left;
        storyFallen(g, c, c.str < 0.15 ? 'destroyed' : 'surrendered', attNat, target.id);
        removeCorps(g, c);
      }
    });
    moveIn(g, attackers, target, opts, attU);
    capture(g, target, attNat);
  } else {
    for (const c of attackers) c.ap = Math.max(0, c.ap - 1);
  }
  // Battles are numbered through the whole war, so a saved game carries the count on.
  g.flags.battles = Math.max(g.flags.battles ?? 0, g.reports.length) + 1;
  const rep: BattleReport = {
    id: g.flags.battles, turn: g.turn, week, region: target.id, regionName: target.name, attNat, defNat,
    attackers: attU, defenders: gone.length ? [...defU, ...gone] : defU, lines: a.lines, aPower: a.A, dPower: a.D, odds: R, level, taken: win,
    attLost, defLost, prisoners,
  };
  g.reports.push(rep);
  // The generals' luck on the main fronts is counted off the main fronts' own battles, for the same reason.
  if (aside) g.flags.asideBattles = (g.flags.asideBattles ?? 0) + 1;
  if (late) { g.flags.lateBattles = (g.flags.lateBattles ?? 0) + 1; g.flags[`fought:${target.theatre}`] = (g.flags[`fought:${target.theatre}`] ?? 0) + 1; }
  const luck = late ? `late:${target.theatre}:${g.flags[`fought:${target.theatre}`]}` : aside ? `aside:${rep.id - (g.flags.lateBattles ?? 0)}` : `${rep.id - (g.flags.asideBattles ?? 0) - (g.flags.lateWalks ?? 0)}`;
  afterBattle(g, rep, target, a.odds, attackers, attGens, defenders, defGens, luck);
  storyBattle(g, rep, target, a.odds, attackers, defenders, { sea: opts.sea, air: opts.air, capital: enemyCapital });
  if (prisoners >= 40000) {
    const who = new Set(defU.filter((u) => u.fate === 'surrendered' || u.fate === 'destroyed').map((u) => u.nat));
    const adj = who.size === 1 ? g.nations[[...who][0]].adj : bloc(g.nations[defNat]) === 'allies' ? 'Allied' : 'Axis';
    headline(g, 'battle', `${prisoners.toLocaleString('en-US')} ${adj} troops surrender at ${target.name}.`, defNat, target.id);
  }
  for (const d of affected) checkCollapse(g, d, attNat);
  return rep;
}

// ---- Moving, taking ground, and falling ---------------------------------------------

export function capture(g: Game, r: Region, nat: string): void {
  const old = r.ctrl;
  if (old === nat) return;
  const owner = g.nations[r.owner];
  r.ctrl = owner.alive && allied(g, nat, r.owner) ? r.owner : nat;
  r.since = g.turn;
  r.prev = old;
  g.supplyDirty = true;
  if (groundRules(g)) {
    // The minefields are lifted by whoever takes the ground, and the side driven off it blows the bridges behind it.
    liftMines(g, r);
    if (atWar(g, nat, old)) bridgesOnCapture(g, r, nat);
  }
  if (r.cap && r.owner === old) headline(g, 'fall', `${r.name} falls to ${g.nations[nat].adj} forces.`, old, r.id);
  // Overseas the prizes are the ports: whoever holds them feeds the army that fights there.
  else if (r.overseas && r.port && r.vp > 0 && atWar(g, nat, old)) headline(g, 'battle', `${r.name} falls to ${g.nations[nat].adj} forces.`, old, r.id);
  checkCollapse(g, old, nat);
}

/**
 * Room for this corps in a region. Engineers work among the other troops: they are not counted against the
 * corps a province holds, and it takes two engineer corps at most.
 */
export function hasRoom(g: Game, to: Region, c: Corps): boolean {
  const here = friendlyUnits(g, to, c.nat);
  const eng = here.filter((u) => u.kind === 'eng').length;
  return c.kind === 'eng' ? eng < 2 : here.length - eng < STACK;
}

/** Moves one corps one region. Returns false if the move is not allowed. */
export function moveCorps(g: Game, c: Corps, to: Region): boolean {
  const from = g.regions[c.reg];
  if (!from.nb.includes(to.id) || c.kind === 'fort' || blocked(g, c, to)) return false;
  const cost = moveCost(g, c, to);
  if (c.ap < Math.min(cost, 4) || c.ap <= 0) return false;
  const friendly = allied(g, c.nat, to.ctrl);
  if (!friendly && !atWar(g, c.nat, to.ctrl)) return false;
  if (hostileUnits(g, to, c.nat).length) return false;
  if (!hasRoom(g, to, c)) return false;
  g.week = weekOf(c, cost);
  place(g, c, to);
  c.ap = Math.max(0, c.ap - cost);
  if (!friendly) capture(g, to, c.nat);
  return true;
}

/** Where a corps could go by rail this month: any friendly region joined to its own by friendly track. No railway crosses the desert. */
export function railReach(g: Game, c: Corps): Set<number> {
  const out = new Set<number>();
  if (c.kind === 'fort' || c.ap < 4 || g.regions[c.reg].terrain === DESERT.terrain) return out;
  const seen = new Uint8Array(g.regions.length);
  const q = [c.reg];
  seen[c.reg] = 1;
  for (let i = 0; i < q.length; i++) {
    for (const j of g.regions[q[i]].nb) {
      const r = g.regions[j];
      if (seen[j] || !allied(g, c.nat, r.ctrl) || r.terrain === DESERT.terrain) continue;
      if (downBetween(g, q[i], j)) continue; // no train crosses a river whose bridges are down
      seen[j] = 1; q.push(j);
      if (!barred(g, c, r)) out.add(j); // the trains run through the high passes, but tanks are not unloaded there
    }
  }
  return out;
}

/** Rail move: anywhere through friendly territory, for the whole month. */
export function railMove(g: Game, c: Corps, to: Region): boolean {
  if (c.kind === 'fort' || c.ap < 4 || !allied(g, c.nat, to.ctrl) || !hasRoom(g, to, c) || barred(g, c, to)) return false;
  if (to.id !== c.reg && !railReach(g, c).has(to.id)) return false;
  place(g, c, to);
  c.ap = 0;
  return true;
}

export function collapseScore(g: Game, nat: string): number {
  const n = g.nations[nat];
  if (n.capital < 0) return 0;
  const capLost = !allied(g, nat, g.regions[n.capital].ctrl) ? 1 : 0;
  let vp = 0, held = 0;
  // The colonies are fought over like any other ground, but a nation stands or falls by its home country.
  for (const r of g.regions) if (r.owner === nat && !r.overseas) { vp += r.vp; if (allied(g, nat, r.ctrl)) held += r.vp; }
  const army = n.armyPeak > 0 ? clamp(armySize(g, nat) / n.armyPeak, 0, 1) : 1;
  return 0.45 * capLost + 0.35 * (vp ? 1 - held / vp : 0) + 0.2 * (1 - army);
}

export function checkCollapse(g: Game, nat: string, by: string): void {
  const n = g.nations[nat];
  if (!n || !n.alive || n.capital < 0) return;
  if (collapseScore(g, nat) >= n.resilience) capitulate(g, nat, by);
}

function nearestFriendly(g: Game, c: Corps): Region | null {
  const seen = new Uint8Array(g.regions.length);
  const q = [c.reg];
  seen[c.reg] = 1;
  for (let i = 0; i < q.length; i++) {
    const r = g.regions[q[i]];
    if (allied(g, c.nat, r.ctrl) && q[i] !== c.reg) return r;
    for (const j of r.nb) if (!seen[j]) { seen[j] = 1; q.push(j); }
  }
  return null;
}

export function capitulate(g: Game, nat: string, by: string): void {
  const X = g.nations[nat];
  if (!X.alive) return;
  X.alive = false;
  X.fellOn = { turn: g.turn, week: g.week, to: by };
  for (const c of g.corps.filter((c) => c.nat === nat)) {
    const at = g.regions[c.reg];
    // The Army of Africa stays at its posts and answers to the new government.
    if (nat === 'FRA' && at.vichy && at.overseas && at.ctrl === 'FRA') { c.nat = 'VIC'; c.hold = true; continue; }
    removeCorps(g, c);
  }
  fleetsOnFall(g, nat);
  airOnFall(g, nat);
  const pact = g.nations.SOV.alive && !atWar(g, 'SOV', by);
  for (const r of g.regions) {
    if (r.owner === nat) {
      if (nat === 'FRA' && r.vichy && r.ctrl === 'FRA') { r.owner = 'VIC'; r.ctrl = 'VIC'; }
      else if (nat === 'POL' && r.sov && pact) r.ctrl = 'SOV';
      else if (r.ctrl === nat) {
        // Ground still held by the fallen nation's allies stays in their hands until it is taken.
        const holder = r.units.find((u) => u.nat !== nat && atWar(g, u.nat, by));
        r.ctrl = holder ? holder.nat : by;
      }
    } else if (r.ctrl === nat) {
      r.ctrl = g.nations[r.owner].alive ? r.owner : by;
    }
  }
  if (nat === 'FRA') {
    const V = g.nations.VIC;
    V.alive = true; V.dormant = false;
    const cap = g.regions.find((r) => r.owner === 'VIC' && r.name === 'Auvergne') ?? g.regions.find((r) => r.owner === 'VIC');
    if (cap) { cap.cap = true; V.capital = cap.id; }
    headline(g, 'fall', 'France signs an armistice. The north and the Atlantic coast are occupied; a new government at Vichy keeps the south.', 'FRA');
  } else {
    headline(g, 'fall', `${X.name} capitulates to ${g.nations[by].name}.`, nat);
  }
  for (const n of Object.values(g.nations)) n.atWar = n.atWar.filter((e) => e !== nat);
  X.atWar = [];
  g.supplyDirty = true;
  // Anyone left standing on ground that is no longer theirs to stand on.
  for (const c of [...g.corps]) {
    const r = g.regions[c.reg];
    if (allied(g, c.nat, r.ctrl)) continue;
    if (atWar(g, c.nat, r.ctrl) && hostileUnits(g, r, c.nat).length === 0) continue; // still occupying enemy ground
    const N = g.nations[c.nat];
    if (N.seaSupply && N.capital >= 0 && (!r.seas.length || seaOpenAt(g, c.nat, r))) {
      place(g, c, g.regions[N.capital]);
      c.str *= 0.7; c.org = 0.3; c.ap = 0;
      if (!g.flags[`evac:${g.turn}`]) { g.flags[`evac:${g.turn}`] = 1; headline(g, 'event', `${N.adj} troops are evacuated from the Continent, leaving their heavy equipment behind.`, c.nat); }
    } else {
      const to = nearestFriendly(g, c);
      if (to) { place(g, c, to); c.ap = 0; } else removeCorps(g, c);
    }
  }
  // The fall of one nation can bring down another.
  for (const n of Object.values(g.nations)) if (n.alive && n.atWar.includes(by)) checkCollapse(g, n.id, by);
}

/** Sea transport: a nation with shipping can send a corps from one friendly port to another. It takes the whole month, and the enemy may find the convoy. */
export function shipMove(g: Game, c: Corps, to: Region): boolean {
  return shipCorps(g, c, to);
}

/** Lifts a corps off the Continent from a port: to a haven of its theatre if there is one to go to, or else home. */
export function evacuate(g: Game, c: Corps): boolean {
  const r = g.regions[c.reg];
  const N = g.nations[c.nat];
  if (!N.seaSupply || !r.port || r.owner === c.nat || N.capital < 0 || !seaOpenAt(g, c.nat, r)) return false;
  const dest = refuge(g, c, r);
  place(g, c, dest);
  c.str *= 0.75; c.org = 0.3; c.ap = 0;
  if (!g.flags[`evac:${g.turn}`]) {
    g.flags[`evac:${g.turn}`] = 1;
    headline(g, 'event', dest.cap ? `${N.adj} troops are lifted off the beaches at ${r.name}.` : `${N.adj} troops are lifted off the beaches at ${r.name} and taken to ${dest.name}, without their heavy equipment.`, c.nat, r.id);
  }
  return true;
}

// ---- The start of a nation's month ---------------------------------------------------

const ordinal = (n: number): string => `${n}${n % 100 >= 11 && n % 100 <= 13 ? 'th' : ['th', 'st', 'nd', 'rd', 'th', 'th', 'th', 'th', 'th', 'th'][n % 10]}`;

/** A nation with a mobilisation plan raises a wave of new corps every month once it is at war. */
function mobilise(g: Game, nat: string): void {
  const plan = EASTERN.mobilisation[nat];
  const n = g.nations[nat];
  if (!plan || !n.atWar.length) return;
  const key = `mobilised:${nat}`;
  if (g.flags[key] === undefined) g.flags[key] = g.turn;
  const month = g.turn - g.flags[key];
  const count = plan.perMonth[month] ?? plan.after;
  const enemyNear = (r: Region): boolean => r.nb.some((j) => atWar(g, nat, g.regions[j].ctrl));
  for (let k = 0; k < count; k++) {
    // Form up in the industrial towns still held, emptiest first, away from the fighting.
    let best: Region | null = null, bs = 1e9;
    for (const r of g.regions) {
      if (r.owner !== nat || r.ctrl !== nat || r.fac < 1 || r.units.length >= STACK || enemyNear(r)) continue;
      const s = r.units.length * 10 - r.fac - (r.cap ? 3 : 0);
      if (s < bs) { bs = s; best = r; }
    }
    if (!best) break;
    const c = addCorps(g, nat, `${ordinal(n.corpsNo++)} ${plan.name}`, plan.kind, plan.divs, best.id, { xp: plan.xp });
    c.org = 0.7;
  }
  if (count > 0 && month === 0) headline(g, 'event', `${n.name} orders general mobilisation.`, nat);
  // Behind the front, the people dig: anti-tank ditches and gun lines around the great cities.
  // In free play nobody waits for history's calendar: the lines round a city are dug as soon as the enemy is three marches from it.
  const close = (r: Region): boolean => {
    const seen = new Set([r.id]);
    let ring = [r.id];
    for (let d = 0; d < 3; d++) {
      const next: number[] = [];
      for (const id of ring) for (const j of g.regions[id].nb) if (!seen.has(j)) { seen.add(j); next.push(j); if (atWar(g, nat, g.regions[j].ctrl)) return true; }
      ring = next;
    }
    return false;
  };
  for (const [when, place, lv] of plan.works ?? []) {
    const r = g.byName[place];
    const due = when === month || (g.flags.open === 1 && month >= 1 && month < when && !!r && close(r));
    if (!due || !r || r.ctrl !== nat || (r.fort?.lv ?? 0) >= lv) continue;
    r.fort = { lv, vs: r.fort?.vs ?? [nat] };
    headline(g, 'event', `Defence lines are dug around ${r.name}.`, nat, r.id);
  }
}

const THEATRE_LIST = Object.values(theatreData as Record<string, any>).filter((x) => typeof x !== 'string') as { name: string; regions: string[]; havens?: string[]; scripted?: boolean; opens?: number; works?: [number, string, number, string[], string, { taken?: boolean; port?: boolean; vs?: string[] }?][] }[];
/** The theatre each region is listed under, by name, whether or not that theatre has opened yet. */
const THEATRE_OF = new Map<string, number>();
THEATRE_LIST.forEach((t, k) => { for (const nm of t.regions) THEATRE_OF.set(nm, k); });

/**
 * A theatre with an opening month is ordinary ground until then: its regions are part of the main fronts and their
 * battles roll the main dice, exactly as if the theatre were not listed. From that month on it is a war apart like
 * the others. In free play it never opens: the war there stays the main staff's business.
 */
export function openTheatres(g: Game): void {
  if (g.flags.open === 1) return;
  THEATRE_LIST.forEach((t, k) => {
    if (t.opens === undefined || g.turn < t.opens) return;
    for (const nm of t.regions) { const r = g.byName[nm]; if (r && r.theatre !== k) { r.theatre = k; g.supplyDirty = true; } }
  });
}

/** True for ground in a theatre that opened late in the war. Its battles keep a count of their own, so that they never change the luck of the older theatres or of the main fronts. */
const lateTheatre = (r: Region): boolean => r.theatre >= 0 && THEATRE_LIST[r.theatre]?.opens !== undefined;

/** The name of the side theatre a region belongs to, as data/campaign/theatres.json gives it. */
export function theatreName(r: Region): string | null {
  return r.theatre >= 0 ? THEATRE_LIST[r.theatre]?.name ?? null : null;
}

/** The places troops lifted off this region's coast are taken to before anywhere else: its theatre's havens. */
export function havensOf(r: Region): string[] {
  return r.theatre >= 0 ? THEATRE_LIST[r.theatre]?.havens ?? [] : [];
}

/** Where troops lifted off a coast are landed: a haven of the same theatre still in friendly hands, with room and ships able to reach it, or else home. */
export function refuge(g: Game, c: Corps, from: Region): Region {
  for (const nm of havensOf(from)) {
    const r = g.byName[nm];
    if (r && r.id !== from.id && allied(g, c.nat, r.ctrl) && !hostileUnits(g, r, c.nat).length && friendlyUnits(g, r, c.nat).length < STACK && seaOpenAt(g, c.nat, r)) return r;
  }
  return g.regions[g.nations[c.nat].capital];
}

/**
 * A formation put together from an army already overseas ('from' names its theatre) takes its divisions out of
 * that army: the largest corps there give them up, and a corps left with none is struck off. Garrisons are not touched.
 */
function draw(g: Game, nat: string, theatre: string, divs: number, made?: Corps): void {
  const k = THEATRE_LIST.findIndex((t) => t.name === theatre);
  for (let need = divs; k >= 0 && need > 0;) {
    const c = g.corps.filter((x) => x !== made && x.nat === nat && !x.dead && !x.hold && x.kind !== 'fort' && g.regions[x.reg].theatre === k && !(made && g.flags[`formed:${x.id}`])).sort((a, b) => b.divs - a.divs || a.id - b.id)[0];
    if (!c) break;
    const take = Math.min(need, c.divs);
    if (take >= c.divs) removeCorps(g, c); else c.divs -= take;
    need -= take;
  }
  g.supplyDirty = true;
}

/**
 * Formations history sent to a war away from the main fronts. Unlike the home mobilisation they need a
 * friendly place to arrive at: one that has been lost, or is full, puts the move off, and after half a year
 * it is cancelled. Formations only lent to a campaign ('until') go back to the main army when their time is up.
 * Divisions sent to strengthen a corps already at the front ('joins' names it) go into that corps wherever it
 * stands: it keeps its general and what it has learned, and takes the new name. A formation made by breaking
 * up others ('from' lists them by name) strikes those corps off as it forms. One that waits on a defeat ('ifLost')
 * forms only once the enemy holds every place named.
 */
function expeditions(g: Game, nat: string): void {
  const gone: Corps[] = [];
  ((setup as any).expeditions as any[] ?? []).forEach((row, i) => {
    if (row[1] !== nat || g.turn < row[0]) return;
    const o = row[6] ?? {};
    if (g.flags[`sent:${i}`]) {
      // The main army's order of battle already counts these formations: the campaign only had the loan of them.
      const id = g.flags[`lent:${i}`];
      if (o.until && g.turn >= o.until && id > 0) {
        const c = g.corps.find((x) => x.id === id && x.nat === nat);
        if (c) gone.push(c);
        g.flags[`lent:${i}`] = -1;
      }
      return;
    }
    let reg = g.byName[row[5]];
    let due = row[0];
    if (g.flags.open === 1) {
      // Free play. An invasion force history put together for a landing of its own planning is nobody's to send.
      // A formation raised for a war waits for that war, however late it comes, and then has its half year.
      const k = reg ? THEATRE_OF.get(reg.name) ?? -1 : -1;
      if (k >= 0 && THEATRE_LIST[k].scripted && row[0] >= (THEATRE_LIST[k].opens ?? 0)) return;
      if (o.war) {
        if (!atWar(g, o.war[0], o.war[1])) return;
        due = g.flags[`due:${i}`] ??= g.turn;
      }
      // A country's own army, raised on its own soil, forms up at the capital if the place meant for it is already lost.
      const cap = g.nations[nat].capital;
      if (reg && reg.owner === nat && !allied(g, nat, reg.ctrl) && cap >= 0) reg = g.regions[cap];
    }
    if (g.turn - due > 6 || (o.war && !atWar(g, o.war[0], o.war[1])) || (o.until && g.turn >= o.until)) { if (g.turn - due > 6) g.flags[`sent:${i}`] = 1; return; }
    // An army put together out of a defeat forms only once the ground is really lost: while the nation or an ally
    // still holds any place named, the formations it would be made from fight on where they stand.
    if (o.ifLost && (o.ifLost as string[]).some((nm) => { const r = g.byName[nm]; return !r || allied(g, nat, r.ctrl); })) return;
    const old = o.joins ? g.corps.find((x) => x.nat === nat && x.name === o.joins && !x.dead) : undefined;
    if (old) {
      g.flags[`sent:${i}`] = 1;
      // Corps broken up to find the divisions are struck off, and a corps only lent to the campaign is remembered.
      if (Array.isArray(o.from)) for (const x of g.corps.filter((x) => x.nat === nat && !x.dead && o.from.includes(x.name))) removeCorps(g, x);
      if (o.until) g.flags[`lent:${i}`] = old.id;
      // The new divisions come at full strength; the corps they join is as rested as it was.
      old.str = (old.str * old.divs + row[4]) / (old.divs + row[4]);
      old.divs += row[4];
      old.name = row[2];
      old.kind = row[3];
      g.supplyDirty = true;
      if (o.news) headline(g, 'event', o.news, nat, old.reg);
      return;
    }
    // Troops sent to take over a stretch of an ally's line ('join') go in whatever already stands there.
    if (!reg || !allied(g, nat, reg.ctrl) || (!o.join && friendlyUnits(g, reg, nat).length >= STACK)) return;
    g.flags[`sent:${i}`] = 1;
    if (Array.isArray(o.from)) {
      for (const x of g.corps.filter((x) => x.nat === nat && !x.dead && o.from.includes(x.name))) removeCorps(g, x);
      g.supplyDirty = true;
    }
    const c = addCorps(g, nat, row[2], row[3], row[4], reg.id, o);
    if (o.from && !Array.isArray(o.from)) {
      // A formation put together where it stands, out of the army around it, is not itself broken up for the next one.
      const here = theatreName(reg) === o.from;
      if (here) g.flags[`formed:${c.id}`] = 1;
      draw(g, o.of ?? nat, o.from, row[4], here ? c : undefined);
    }
    if (o.until) g.flags[`lent:${i}`] = c.id;
    const the = /^(the |[A-Z] Force|Creforce)/i.test(c.name) ? '' : 'the ';
    // Troops come ashore at a port across the sea: a colony's, or a foreign one for a nation that lives by its ships.
    // An army that marched there, or is forming up at home, simply arrives.
    const bySea = reg.port && (reg.overseas || (g.nations[nat].seaSupply && reg.owner !== nat));
    // A row that tells its own news says more than the bare arrival would.
    if (o.news) headline(g, 'event', o.news, nat, reg.id);
    else if (o.gen) headline(g, 'event', `${o.gen} ${bySea ? 'lands at' : 'arrives in'} ${reg.name} with ${the}${c.name}.`, nat, reg.id);
    else if (nat === g.human) headline(g, 'info', `${c.name} has ${bySea ? 'landed at' : 'arrived in'} ${reg.name}.`, nat, reg.id);
  });
  if (gone.length) {
    const where = theatreName(g.regions[gone[0].reg]);
    const names = gone.map((c) => c.name);
    for (const c of gone) removeCorps(g, c);
    g.supplyDirty = true;
    headline(g, 'event', `${g.nations[nat].adj} formations lent to the campaign${where ? ` in ${where}` : ''} leave for other fronts: ${names.join('; ')}.`, nat);
  }
}

/** A theatre that opens in its own month digs nothing before it, and in free play, where it never opens, nothing at all. */
const worksDue = (g: Game, t: { opens?: number }): boolean => t.opens === undefined || (g.flags.open !== 1 && g.turn >= t.opens);

/**
 * Field works in the desert, and on the eastern front where history is followed, by history's calendar: in the month named, whoever of the nations listed holds the
 * place digs it in to that level, or strips its defences down to it. The works face the open desert: an attack
 * from ground of the same colony meets them. Works marked 'taken' are a beachhead's: they are dug as the month
 * closes by a holder who won the place in that month, where the others are dug as his month opens.
 */
function desertWorks(g: Game, nat: string, closing = false): void {
  // A theatre outside the desert can list works of its own in theatres.json: they are dug the same way.
  const rows: [number, string, number, string[], string, { taken?: boolean; port?: boolean; vs?: string[] }?][] = [...(DESERT.works ?? []), ...(g.flags.open !== 1 ? EASTERN.works ?? [] : []), ...THEATRE_LIST.flatMap((t) => (worksDue(g, t) ? t.works ?? [] : []))];
  for (const [when, place, lv, holders, news, o] of rows) {
    const r = g.byName[place];
    if (closing && !o?.taken) continue;
    if (when !== g.turn || !r || r.ctrl !== nat || !holders.includes(nat)) continue;
    // Nothing to do where the works already stand as the row wants them: the same level, facing the same way, the harbour built.
    const faces = o?.vs ?? r.fort?.vs ?? [r.owner];
    if ((r.fort?.lv ?? 0) === lv && (!o?.vs || (r.fort?.vs ?? []).join() === o.vs.join()) && (!o?.port || r.port)) continue;
    r.fort = lv > 0 ? { lv, vs: [...faces] } : null;
    // A harbour built off an open beach: from now on ships can land troops and stores there.
    if (o?.port && !r.port) { r.port = true; g.flags[`harbour:${r.id}`] = g.turn; g.supplyDirty = true; }
    headline(g, 'event', news, nat, r.id);
  }
}

/**
 * An army newly cut off from its supply is news the month it happens, whoever closed the ring. It is
 * told once: again only if the army is relieved and then cut off a second time.
 */
function cutOffNews(g: Game): void {
  const rings = new Map<number, { by: Record<string, number>; corps: number; men: number }>();
  for (const c of g.corps) {
    if (c.supplied || c.kind === 'fort') continue;
    const ring = rings.get(c.reg) ?? { by: {}, corps: 0, men: 0 };
    ring.by[c.nat] = (ring.by[c.nat] ?? 0) + 1;
    ring.corps++;
    ring.men += men(c);
    rings.set(c.reg, ring);
  }
  for (const [reg, ring] of rings) {
    const key = `cut:${reg}`;
    const fresh = (g.flags[key] ?? -99) < g.turn - 1;
    g.flags[key] = g.turn;
    if (!fresh || ring.corps < 3 || ring.men < 60000) continue;
    g.flags[`ring:${reg}`] = g.turn;
    const who = Object.keys(ring.by).sort((a, b) => ring.by[b] - ring.by[a]);
    const adj = who.map((id) => g.nations[id].adj).join(' and ');
    headline(g, 'battle', `The ring closes at ${g.regions[reg].name}: ${ring.corps} ${adj} corps, ${ring.men.toLocaleString('en-US')} men, are cut off from all supply.`, who[0], reg);
  }
}

/** The last thing in a nation's month: a beachhead won in it is dug in before the enemy's turn comes round. */
export function endPhase(g: Game, nat: string): void {
  if (g.nations[nat].alive) desertWorks(g, nat, true);
}

export function startPhase(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (!n.alive) return;
  // Reinforcements from the pre-war mobilisation, and the armies and fronts formed later from men already under arms.
  for (const row of [...(setup as any).reinforcements, ...((setup as any).formations ?? [])]) {
    if (row[0] !== g.turn || row[1] !== nat) continue;
    let reg = g.byName[row[5]];
    if (!allied(g, nat, reg.ctrl)) reg = g.regions[n.capital];
    if (!reg || !allied(g, nat, reg.ctrl)) continue;
    addCorps(g, nat, row[2], row[3], row[4], reg.id, row[6] ?? {});
  }
  expeditions(g, nat);
  desertWorks(g, nat);
  mobilise(g, nat);
  monthlyCommand(g, nat);
  navalUpkeep(g, nat);
  airUpkeep(g, nat);
  researchTick(g, nat);
  computeSupply(g);
  cutOffNews(g);
  // The month's production comes in, and in the grand campaign the forces are paid out of it. Losses are made good
  // from what is left. A treasury that is empty pays nobody: no drafts go out, the armies rest badly and men drift away.
  const earned = income(g, nat);
  n.pp += earned - upkeepTotal(g, nat);
  const credit = BUDGET.debt.floor * Math.max(1, earned);
  const owed = budgeted(g) && n.pp < 0 ? Math.min(1, -n.pp / credit) : 0;
  n.pc = Math.min(DIPLOMACY.pcCap, n.pc + pcIncome(nat));
  const perDiv = PRODUCTION.replacementCost;
  const tempo = freshOffensive(g, nat);
  const army = g.corps.filter((c) => c.nat === nat);
  // Men, tanks and rest reach a corps only as well as its supply line allows. The depots can
  // send out only so many trained men a month, and what there is is shared along the front.
  // Overseas, every man and tank has to come by sea first: the drafts arrive slower.
  // A corps the player has told to keep its veterans together asks for no drafts.
  const want = army.map((c) => (c.supplied && !(c.lean && nat === g.human) ? Math.min((1 - c.str) * c.divs, 0.2 * c.divs * lineStrength(g, c).mul * (g.regions[c.reg].overseas ? DESERT.replace : 1)) : 0));
  const asked = want.reduce((s, x) => s + x, 0);
  const share = asked > 0 ? Math.min(1, n.pp / perDiv / asked, (PRODUCTION.manpower * Math.max(n.armyPeak, armySize(g, nat))) / asked) : 0;
  // Corps starved into surrender this month, by where they stood: an army lost together is one piece of news.
  const starved = new Map<number, { names: string[]; men: number }>();
  army.forEach((c, i) => {
    if (c.supplied) {
      const give = want[i] * share;
      if (give > 0) {
        // Green drafts thin out what a corps of the player's has learned.
        drafted(g, c, c.str, give / c.divs);
        c.str += give / c.divs; n.pp -= give * perDiv;
      }
      // A Russian winter without winter clothing costs men to frostbite and leaves little rest.
      const cold = frozen(g, nat, g.regions[c.reg]);
      const bite = cold ? cmdWinter(g, c) : 0;
      if (cold) c.str *= 1 - EASTERN.winter.attrition * bite;
      const rest = (c.fought ? 0.25 : 0.45) * lineStrength(g, c).mul * (1 - (1 - EASTERN.winter.rest) * bite) * cmdRally(g, c);
      c.org = Math.min(1, c.org + (owed ? rest * (1 - BUDGET.debt.rest * owed) : rest));
      if (owed) c.str *= 1 - BUDGET.debt.desert * owed;
    } else {
      c.org -= 0.4;
      c.str *= 0.85;
      if (c.org <= 0) {
        const lost = starved.get(c.reg) ?? { names: [], men: 0 };
        lost.names.push(c.name);
        lost.men += men(c);
        starved.set(c.reg, lost);
        storyFallen(g, c, 'starved', n.atWar[0] ?? nat, c.reg);
        removeCorps(g, c);
        return;
      }
    }
    c.fought = false;
    if (c.gaveWay) delete c.gaveWay;
    c.ap = c.apMax = 4 + (tempo && KIND[c.kind].mobile ? 1 : 0) + cmdAp(g, c);
  });
  for (const [reg, lost] of starved) {
    // Small parties are not news, unless they are the end of an army the newsreel has already said was surrounded.
    if (lost.men <= 30000 && g.flags[`ring:${reg}`] === undefined) continue;
    const place = g.regions[reg].name;
    if (lost.names.length === 1) headline(g, 'battle', `Cut off and out of supply, ${lost.names[0]} lays down its arms at ${place}.`, nat, reg);
    else headline(g, 'battle', `Cut off and out of supply, the ${n.adj} army at ${place} lays down its arms: ${lost.names.length} corps, ${lost.men.toLocaleString('en-US')} men.`, nat, reg);
  }
  // Replacements can spend the treasury to the last point: a rounding error below nothing is not a debt.
  n.pp = budgeted(g) ? (n.pp > -1e-9 ? Math.max(0, n.pp) : Math.max(-credit, n.pp)) : Math.max(0, n.pp);
  if (budgeted(g)) {
    // The first month in the red is news, and so is the month the books balance again.
    const key = `debt:${nat}`;
    if (n.pp < 0 && g.flags[key] === undefined) {
      g.flags[key] = g.turn;
      if (nat === g.human) headline(g, 'info', `The treasury is empty. Until the books balance again no replacements go out to the armies, nothing new can be ordered, and unpaid troops rest badly and begin to drift away. Open the ledger to see where the money goes.`, nat);
    } else if (n.pp >= 0 && g.flags[key] !== undefined) {
      delete g.flags[key];
      if (nat === g.human) headline(g, 'info', `The books balance again: the treasury is out of debt.`, nat);
    }
  }
  deliver(g, nat);
  engineersMonth(g, nat);
  if (nat !== g.human) n.pp = Math.min(n.pp, PRODUCTION.aiBank[canBuildYet(g, nat) ? 1 : 0]);
  n.armyPeak = Math.max(n.armyPeak, armySize(g, nat));
  if (tempo) g.flags[`tempo:${nat}:${g.turn}`] = 1;
  storyMonth(g, nat);
  checkCollapse(g, nat, n.atWar[0] ?? nat);
}

/** The Red Army moves into eastern Poland once the Polish front is breaking, as the 1939 pact arranged. */
export function sovietEvent(g: Game): void {
  if (g.flags.sovietMove || !g.nations.POL.alive || !g.nations.SOV.alive || g.turn > 3) return;
  if (atWar(g, 'SOV', 'GER') || !atWar(g, 'GER', 'POL')) return;
  const lost = g.regions.filter((r) => r.owner === 'POL' && r.ctrl !== 'POL').length;
  if (lost < 5) return;
  g.flags.sovietMove = 1;
  g.week = 3;
  for (const r of g.regions) {
    if (!r.sov || r.ctrl !== 'POL') continue;
    for (const c of [...r.units]) if (c.nat === 'POL') removeCorps(g, c);
    r.ctrl = 'SOV';
  }
  const fronts = g.corps.filter((c) => c.nat === 'SOV' && !c.hold);
  const dest = ['Wilno', 'Brest', 'Lwow', 'Volhynia', 'Bialystok', 'Polesie'];
  fronts.forEach((c, i) => { const r = g.byName[dest[i % dest.length]]; if (r.ctrl === 'SOV') place(g, c, r); });
  g.supplyDirty = true;
  headline(g, 'event', 'The Red Army crosses into eastern Poland under the pact with Germany.', 'SOV');
  g.week = 4;
  checkCollapse(g, 'POL', 'GER');
}
