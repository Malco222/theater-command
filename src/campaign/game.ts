// The campaign rules: the map, who is at war, how armies move and are supplied, how a
// battle is auto-resolved, and how a nation falls. The same battle maths will sit under
// the playable 3D battles later, so that skipping a battle and fighting it are the same fight.

import mapData from '../generated/campaign-map.json';
import nationsData from '../../data/campaign/nations.json';
import setup from '../../data/campaign/setup-1939.json';
import { Rng } from '../core/rng';
import { clamp } from '../core/math';
import { turnToDate, type TicketLine, type VictoryLevel } from '../core/ticket';
import { PRODUCTION, canBuildYet, deliver, firstCorpsNumber, income } from './economy';
import { DIPLOMACY, homeOpinion, pcIncome } from './diplomacy';
import {
  FORT_MUL, KIND, MEN_PER_DIV, STACK, TERRAIN_DEF,
  type BattleReport, type Corps, type Game, type Headline, type Kind, type Nation, type Region, type ReportUnit,
} from './types';

const ORDER = ['GER', 'ITA', 'SVK', 'HUN', 'SOV', 'POL', 'FRA', 'UK', 'BEL', 'NED', 'LUX', 'DEN', 'NOR'];
const EAST = new Set(['POL', 'SOV', 'LIT', 'LAT', 'EST', 'ROM', 'FIN']);

export function createGame(seed: number, human: string | null): Game {
  const md = mapData as any;
  const regions: Region[] = md.regions.map((r: any): Region => ({
    id: r.id, name: r.name, owner: r.owner, ctrl: r.owner, terrain: r.terrain, vp: r.vp, fac: r.fac,
    cap: !!r.flags.cap, port: !!r.flags.port || !!r.flags.depot,
    fort: r.flags.fort ? { lv: r.flags.fort[0], vs: r.flags.fort.slice(1) } : null,
    vichy: !!r.flags.vichy, sov: !!r.flags.sov, oil: !!r.flags.oil,
    x: r.x, y: r.y, nb: r.nb, polys: r.polys, units: [], since: 0,
  }));
  const byName: Record<string, Region> = {};
  for (const r of regions) byName[r.name] = r;
  const nations: Record<string, Nation> = {};
  for (const [id, d] of Object.entries(nationsData as Record<string, any>)) {
    if (id.startsWith('_')) continue;
    const cap = regions.find((r) => r.owner === id && r.cap);
    nations[id] = {
      id, name: d.name, adj: d.adj, color: d.color, quality: d.quality, air: d.air, resilience: d.resilience, align: d.align,
      blitz: !!d.blitz, slowCommand: !!d.slowCommand, seaSupply: !!d.seaSupply,
      atWar: [], alive: !d.dormant, dormant: !!d.dormant, capital: cap ? cap.id : -1, pp: 0, queue: [], corpsNo: firstCorpsNumber(id),
      opinion: homeOpinion(id, d.align), home: homeOpinion(id, d.align), pc: 0, armyPeak: 0, offStart: {}, lastFight: {},
    };
  }
  const order = [...ORDER, ...Object.keys(nations).filter((n) => !ORDER.includes(n))];
  const g: Game = {
    seed, rng: new Rng(seed), turn: 1, phase: 0, order, nations, regions, byName, corps: [], nextCorps: 1, nextBuild: 1, human,
    reports: [], headlines: [], flags: {}, supplyDirty: true, supply: {}, capDist: {}, week: 1,
    links: md.links, width: md.width, height: md.height, land: md.land,
  };
  for (const row of (setup as any).corps) addCorps(g, row[0], row[1], row[2], row[3], row[4], row[5] ?? {});
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
  computeSupply(g);
  return g;
}

export function addCorps(g: Game, nat: string, name: string, kind: Kind, divs: number, region: string | number, o: any): Corps {
  const reg = typeof region === 'number' ? g.regions[region] : g.byName[region];
  const c: Corps = {
    id: g.nextCorps++, nat, name, kind, divs, str: 1, org: 1, xp: o.xp ?? 0.15, reg: reg.id, ap: 4, apMax: 4,
    general: o.gen ?? null, trait: o.trait ?? null, hold: !!o.hold, dyle: o.dyle ?? null, supplied: true, fought: false, dead: false,
  };
  g.corps.push(c);
  reg.units.push(c);
  return c;
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
  // A neutral that is attacked joins the other side.
  const ba = bloc(A);
  if (bloc(B) === null && ba) {
    B.align = ba === 'axis' ? 'allies' : 'axis_cobelligerent';
    B.opinion = B.home = ba === 'axis' ? -100 : 100;
  }
  headline(g, 'war', `${A.name} declares war on ${B.name}.`, a);
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

export function armySize(g: Game, nat: string): number {
  let s = 0;
  for (const c of g.corps) if (c.nat === nat) s += c.divs * c.str;
  return s;
}

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

export function weather(g: Game): { mud: boolean; text: string } {
  const m = turnToDate(g.turn).month;
  if (m === 9 || m === 10 || m === 2 || m === 3) return { mud: true, text: 'Mud in the east slows every army there.' };
  if (m === 11 || m === 0 || m === 1) return { mud: false, text: 'Winter.' };
  return { mud: false, text: 'Clear campaigning weather.' };
}

export function moveCost(g: Game, c: Corps, to: Region): number {
  // Marching through country that has been friendly since before this month is quick for anyone.
  let cost = allied(g, c.nat, to.ctrl) && to.since < g.turn ? 1 : KIND[c.kind].move;
  if (to.terrain === 'mountain' && c.kind !== 'mtn') cost += 1;
  if (to.terrain === 'marsh') cost += 1;
  if (weather(g).mud && EAST.has(to.owner)) cost += 1;
  return cost;
}

// ---- Supply -------------------------------------------------------------------------

export function computeSupply(g: Game): void {
  const active = new Set(g.corps.map((c) => c.nat));
  for (const id of active) {
    const n = g.nations[id];
    const ok = new Uint8Array(g.regions.length);
    const q: number[] = [];
    for (const r of g.regions) {
      if (!allied(g, id, r.ctrl)) continue;
      // An army is fed from its own country: its capital, its big industrial regions, its depots.
      // A nation with command of the sea can also be fed through any friendly port.
      const home = r.ctrl === id && r.owner === id;
      if ((home && (r.cap || r.fac >= 3 || g.flags['depot:' + r.id])) || (n.seaSupply && r.port)) { ok[r.id] = 1; q.push(r.id); }
    }
    for (let i = 0; i < q.length; i++) {
      for (const j of g.regions[q[i]].nb) {
        if (ok[j] || !allied(g, id, g.regions[j].ctrl)) continue;
        ok[j] = 1; q.push(j);
      }
    }
    g.supply[id] = ok;
  }
  // An army at peace is fed by its own country wherever it stands.
  for (const c of g.corps) c.supplied = !g.nations[c.nat].atWar.length || !!g.supply[c.nat]?.[c.reg];
  g.supplyDirty = false;
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
  for (const c of [...corps].sort((x, y) => corpsBase(g, y) * KIND[y.kind].atk - corpsBase(g, x) * KIND[x.kind].atk)) {
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
  return c.divs * c.str * Math.sqrt(0.15 + 0.85 * c.org) * g.nations[c.nat].quality * (0.85 + 0.4 * c.xp) * (c.supplied ? 1 : 0.5);
}

export const men = (c: Corps): number => Math.round(c.divs * c.str * MEN_PER_DIV);

function airOf(g: Game, nats: Set<string>, enemies: Set<string>): number {
  let s = 0;
  for (const id of nats) s += g.nations[id].air;
  for (const n of Object.values(g.nations)) {
    if (!n.alive || nats.has(n.id)) continue;
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

/** Works out both sides' strength for an attack. Auto-resolve and the odds preview share it. */
export function assess(g: Game, attackers: Corps[], target: Region): Assessment {
  ensureSupply(g);
  const attNat = attackers[0].nat;
  const defenders = hostileUnits(g, target, attNat);
  const lines: TicketLine[] = [];
  const aN = new Set(attackers.map((c) => c.nat)), dN = new Set(defenders.map((c) => c.nat));
  const defHasArmor = defenders.some((d) => d.kind === 'arm');
  const tempo = [...dN].some((d) => freshOffensive(g, attNat, d)) || freshOffensive(g, attNat, target.ctrl);
  let A = 0, fortFaced = 0, armorShock = false, armorStuck = false;
  const sources = new Set<number>();
  for (const c of attackers) {
    let p = corpsBase(g, c) * KIND[c.kind].atk;
    if (c.kind === 'arm') {
      if (target.terrain === 'mountain' || target.terrain === 'urban' || target.terrain === 'marsh') { p *= 0.6; armorStuck = true; }
      else if (!defHasArmor) { p *= 1.25; armorShock = true; }
    }
    if (c.trait === 'attacker' || c.trait === 'panzer') p *= 1.1;
    if (tempo && KIND[c.kind].mobile) p *= 1.2;
    if (g.nations[c.nat].slowCommand) p *= 0.85;
    const from = g.regions[c.reg];
    if (target.fort && target.fort.vs.includes(from.owner)) { p /= FORT_MUL[target.fort.lv]; fortFaced++; }
    sources.add(c.reg);
    A += p;
  }
  const flank = Math.min(1.3, 1 + 0.1 * (sources.size - 1));
  A *= flank;
  let D = 0;
  for (const d of defenders) {
    let p = corpsBase(g, d) * KIND[d.kind].def * TERRAIN_DEF[target.terrain];
    if (d.trait === 'defender') p *= 1.1;
    D += p;
  }
  const aAir = airOf(g, aN, dN), dAir = airOf(g, dN, aN);
  const airEdge = (aAir - dAir) / (aAir + dAir + 1);
  A *= 1 + 0.3 * airEdge;
  D *= 1 - 0.2 * airEdge;
  D = Math.max(D, 0.01);

  const sum = (cs: Corps[]) => cs.reduce((s, c) => s + men(c), 0).toLocaleString('en-US');
  lines.push({ label: 'Forces', value: `${attackers.length} corps, ${sum(attackers)} men, against ${defenders.length} corps, ${sum(defenders)} men` });
  lines.push({ label: 'Terrain', value: target.terrain[0].toUpperCase() + target.terrain.slice(1), effect: TERRAIN_DEF[target.terrain] > 1 ? `Defenders fight ${Math.round((TERRAIN_DEF[target.terrain] - 1) * 100)}% stronger` : 'Open ground, no help to the defender' });
  if (target.fort) {
    lines.push({ label: 'Fortifications', value: `Level ${target.fort.lv}`, effect: fortFaced === attackers.length ? `Attacked head-on: the attack is cut to ${Math.round(100 / FORT_MUL[target.fort.lv])}% strength` : fortFaced > 0 ? 'Partly outflanked: only the corps attacking head-on are slowed' : 'Outflanked: the works face the other way' });
  }
  const aUns = attackers.filter((c) => !c.supplied).length, dUns = defenders.filter((c) => !c.supplied).length;
  lines.push({ label: 'Supply', value: `${aUns ? `${aUns} attacking corps cut off` : 'Attackers in supply'}; ${dUns ? `${dUns} defending corps cut off` : 'defenders in supply'}`, effect: aUns || dUns ? 'Corps that are cut off fight at half strength' : undefined });
  lines.push({ label: 'Air', value: airEdge > 0.15 ? 'Attacker holds the sky' : airEdge < -0.15 ? 'Defender holds the sky' : 'Contested', effect: Math.abs(airEdge) > 0.05 ? `${airEdge > 0 ? 'Attack' : 'Defence'} strengthened by about ${Math.round(Math.abs(airEdge) * 30)}%` : undefined });
  const gens = attackers.filter((c) => c.general).map((c) => c.general);
  const dgens = defenders.filter((c) => c.general).map((c) => c.general);
  if (gens.length || dgens.length) lines.push({ label: 'Commanders', value: `${gens.join(', ') || 'none named'} against ${dgens.join(', ') || 'none named'}` });
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
}

function retreatTarget(g: Game, c: Corps, from: Region, banned: Set<number>): Region | null {
  if (c.kind === 'fort') return null;
  let best: Region | null = null, bs = 1e9;
  const d = g.capDist[c.nat];
  for (const j of from.nb) {
    const r = g.regions[j];
    if (banned.has(j) || !allied(g, c.nat, r.ctrl) || hostileUnits(g, r, c.nat).length) continue;
    if (friendlyUnits(g, r, c.nat).length >= STACK + 2) continue;
    const abroad = g.nations[c.nat].seaSupply && r.owner !== c.nat;
    const s = (abroad ? (r.port ? 0 : 3) : d ? d[j] : 0) + (g.supply[c.nat]?.[j] ? 0 : 6) + r.units.length * 0.3;
    if (s < bs) { bs = s; best = r; }
  }
  return best;
}

export function resolveBattle(g: Game, attackers: Corps[], target: Region): BattleReport {
  const a = assess(g, attackers, target);
  const defenders = a.defenders;
  const attNat = attackers[0].nat;
  const defNat = defenders[0]?.nat ?? target.ctrl;
  const AN = g.nations[attNat];
  for (const d of new Set(defenders.map((c) => c.nat))) {
    if (AN.lastFight[d] === undefined || g.turn - AN.lastFight[d] >= 3) AN.offStart[d] = AN.offStart[d] !== undefined && g.turn - AN.offStart[d] <= 1 ? AN.offStart[d] : g.turn;
    AN.lastFight[d] = g.turn;
  }
  const week = Math.max(...attackers.map((c) => weekOf(c, 1)));
  g.week = week;
  const R = a.odds * g.rng.range(0.8, 1.2);
  const attU = attackers.map(reportUnit), defU = defenders.map(reportUnit);
  const mobile = attackers.some((c) => KIND[c.kind].mobile);
  const defFrac = clamp(0.07 + 0.1 * R, 0.06, 0.45);
  const attFrac = clamp(0.14 / R, 0.02, 0.2);
  let attLost = 0, defLost = 0, prisoners = 0;
  attackers.forEach((c, i) => {
    const before = men(c);
    c.str *= 1 - attFrac;
    c.org = clamp(c.org - (0.1 + 0.08 / R), 0, 1);
    c.xp = Math.min(1, c.xp + 0.05);
    c.fought = true;
    attU[i].lost = before - men(c);
    attLost += attU[i].lost;
  });
  const win = R >= 1.2;
  defenders.forEach((c, i) => {
    const before = men(c);
    c.str *= 1 - defFrac * (win && mobile ? 1.3 : 1);
    c.org = clamp(c.org - clamp(0.18 + 0.14 * R, 0.2, 0.75), 0, 1);
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
      else if (c.str >= 0.15 && N.seaSupply && target.port && target.owner !== c.nat && N.capital >= 0) {
        // Backs to the sea: the navy takes off what it can.
        place(g, c, g.regions[N.capital]);
        c.str *= 0.75; c.org = 0.2; c.ap = 0;
        defU[i].fate = 'evacuated';
        if (!g.flags[`evac:${g.turn}`]) { g.flags[`evac:${g.turn}`] = 1; headline(g, 'event', `${N.adj} troops are lifted off the beaches at ${target.name} under fire.`, c.nat, target.id); }
      } else {
        const left = men(c);
        defU[i].fate = c.str < 0.15 ? 'destroyed' : 'surrendered';
        defU[i].lost += left;
        prisoners += left;
        removeCorps(g, c);
      }
    });
    // The winners move in, fastest first.
    const order = attackers.map((c, i) => ({ c, i })).sort((x, y) => KIND[x.c.kind].move - KIND[y.c.kind].move || corpsBase(g, y.c) - corpsBase(g, x.c));
    let moved = 0;
    for (const { c, i } of order) {
      const cost = moveCost(g, c, target);
      if (moved >= STACK || (moved > 0 && c.ap < cost)) { c.ap = Math.max(0, c.ap - 1); continue; }
      place(g, c, target);
      c.ap = Math.max(0, c.ap - cost);
      attU[i].fate = 'advanced';
      moved++;
    }
    capture(g, target, attNat);
  } else {
    for (const c of attackers) c.ap = Math.max(0, c.ap - 1);
  }
  const rep: BattleReport = {
    id: g.reports.length + 1, turn: g.turn, week, region: target.id, regionName: target.name, attNat, defNat,
    attackers: attU, defenders: defU, lines: a.lines, aPower: a.A, dPower: a.D, odds: R, level, taken: win,
    attLost, defLost, prisoners,
  };
  g.reports.push(rep);
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
  g.supplyDirty = true;
  if (r.cap && r.owner === old) headline(g, 'fall', `${r.name} falls to ${g.nations[nat].adj} forces.`, old, r.id);
  checkCollapse(g, old, nat);
}

/** Moves one corps one region. Returns false if the move is not allowed. */
export function moveCorps(g: Game, c: Corps, to: Region): boolean {
  const from = g.regions[c.reg];
  if (!from.nb.includes(to.id) || c.kind === 'fort') return false;
  const cost = moveCost(g, c, to);
  if (c.ap < Math.min(cost, 4) || c.ap <= 0) return false;
  const friendly = allied(g, c.nat, to.ctrl);
  if (!friendly && !atWar(g, c.nat, to.ctrl)) return false;
  if (hostileUnits(g, to, c.nat).length) return false;
  if (friendlyUnits(g, to, c.nat).length >= STACK) return false;
  g.week = weekOf(c, cost);
  place(g, c, to);
  c.ap = Math.max(0, c.ap - cost);
  if (!friendly) capture(g, to, c.nat);
  return true;
}

/** Rail move: anywhere through friendly territory, for the whole month. */
export function railMove(g: Game, c: Corps, to: Region): boolean {
  if (c.kind === 'fort' || c.ap < 4 || !allied(g, c.nat, to.ctrl) || friendlyUnits(g, to, c.nat).length >= STACK) return false;
  const seen = new Uint8Array(g.regions.length);
  const q = [c.reg];
  seen[c.reg] = 1;
  for (let i = 0; i < q.length; i++) {
    if (q[i] === to.id) { place(g, c, to); c.ap = 0; return true; }
    for (const j of g.regions[q[i]].nb) if (!seen[j] && allied(g, c.nat, g.regions[j].ctrl)) { seen[j] = 1; q.push(j); }
  }
  return false;
}

export function collapseScore(g: Game, nat: string): number {
  const n = g.nations[nat];
  if (n.capital < 0) return 0;
  const capLost = !allied(g, nat, g.regions[n.capital].ctrl) ? 1 : 0;
  let vp = 0, held = 0;
  for (const r of g.regions) if (r.owner === nat) { vp += r.vp; if (allied(g, nat, r.ctrl)) held += r.vp; }
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
  for (const c of g.corps.filter((c) => c.nat === nat)) removeCorps(g, c);
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
    if (N.seaSupply && N.capital >= 0) {
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

/** Sea transport: a nation that commands the sea can ship a corps from one friendly port to another. It takes the whole month. */
export function shipMove(g: Game, c: Corps, to: Region): boolean {
  const from = g.regions[c.reg];
  if (!g.nations[c.nat].seaSupply || c.kind === 'fort' || c.ap < Math.min(4, c.apMax) || !from.port || !to.port || to.id === from.id) return false;
  if (!allied(g, c.nat, to.ctrl) || hostileUnits(g, to, c.nat).length || friendlyUnits(g, to, c.nat).length >= STACK) return false;
  place(g, c, to);
  c.ap = 0;
  g.week = 2;
  return true;
}

/** Lifts a corps off the Continent from a port. */
export function evacuate(g: Game, c: Corps): boolean {
  const r = g.regions[c.reg];
  const N = g.nations[c.nat];
  if (!N.seaSupply || !r.port || r.owner === c.nat || N.capital < 0) return false;
  place(g, c, g.regions[N.capital]);
  c.str *= 0.75; c.org = 0.3; c.ap = 0;
  if (!g.flags[`evac:${g.turn}`]) { g.flags[`evac:${g.turn}`] = 1; headline(g, 'event', `${N.adj} troops are lifted off the beaches at ${r.name}.`, c.nat, r.id); }
  return true;
}

// ---- The start of a nation's month ---------------------------------------------------

export function startPhase(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (!n.alive) return;
  for (const row of (setup as any).reinforcements) {
    if (row[0] !== g.turn || row[1] !== nat) continue;
    let reg = g.byName[row[5]];
    if (!allied(g, nat, reg.ctrl)) reg = g.regions[n.capital];
    if (!reg || !allied(g, nat, reg.ctrl)) continue;
    addCorps(g, nat, row[2], row[3], row[4], reg.id, row[6] ?? {});
  }
  computeSupply(g);
  // The month's production comes in. Losses are made good first.
  n.pp += income(g, nat);
  n.pc = Math.min(DIPLOMACY.pcCap, n.pc + pcIncome(nat));
  const perDiv = PRODUCTION.replacementCost;
  const tempo = freshOffensive(g, nat);
  for (const c of g.corps.filter((c) => c.nat === nat)) {
    if (c.supplied) {
      const need = (1 - c.str) * c.divs;
      const give = Math.min(need, 0.2 * c.divs, n.pp / perDiv);
      if (give > 0) { c.str += give / c.divs; n.pp -= give * perDiv; }
      c.org = Math.min(1, c.org + (c.fought ? 0.25 : 0.45));
    } else {
      c.org -= 0.4;
      c.str *= 0.85;
      if (c.org <= 0) {
        if (men(c) > 30000) headline(g, 'battle', `Cut off and out of supply, ${c.name} lays down its arms at ${g.regions[c.reg].name}.`, nat, c.reg);
        removeCorps(g, c);
        continue;
      }
    }
    c.fought = false;
    c.ap = c.apMax = 4 + (tempo && KIND[c.kind].mobile ? 1 : 0) + (c.trait === 'panzer' ? 1 : 0);
  }
  deliver(g, nat);
  if (nat !== g.human) n.pp = Math.min(n.pp, PRODUCTION.aiBank[canBuildYet(g, nat) ? 1 : 0]);
  n.armyPeak = Math.max(n.armyPeak, armySize(g, nat));
  if (tempo) g.flags[`tempo:${nat}:${g.turn}`] = 1;
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
