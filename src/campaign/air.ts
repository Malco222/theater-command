// The war in the air, on the campaign map. An air fleet is a count of fighters and bombers at
// a base. Left alone it supports the armies and defends its own sky. Given orders it fights a
// month-long air campaign against a region or a sea, settled like a land or sea battle from
// what the map knows: numbers, range, radar, readiness and commanders. Every number lives in
// data/campaign/air.json.

import data from '../../data/campaign/air.json';
import { Rng, hashString } from '../core/rng';
import { clamp } from '../core/math';
import type { TicketLine, VictoryLevel } from '../core/ticket';
import { allied, atWar, headline } from './game';
import { techMul, techNotes } from './research';
import { NAVY, atSea, shipCount, shipList, sinkShips, surfaceShips, zoneOf } from './navy';
import type { AirFleet, AirMission, AirPool, AirReport, AirSide, Game, Region, Ships } from './types';

type Sched = [number, number, number][];
interface NatSpec { quality: number; ftrRange: number; bmrRange: number }
interface Chain { nat: string; from: number; name: string; by: string; ground: number }
interface FleetRow { nat: string; name: string; base: string; from?: number; leader?: string; start?: [number, number]; est: Sched; ftrRange?: number; scripted?: boolean }
export interface AirOrder { fleet: string; base?: string[]; mission: AirMission; region?: string; sea?: string }
export interface AirPlan { nat: string; from: number; to: number; name: string; against: string; orders: AirOrder[] }
interface Reserve { ftr: number; bmr: number; pilots: number }

export const AIR = data as unknown as {
  kmPerUnit: number;
  ferry: number;
  nations: Record<string, NatSpec>;
  range: { fullAt: number; atLimit: number };
  radar: { without: number; chains: Chain[] };
  production: Record<string, Sched>;
  pilots: { home: number; away: number; perMonth: Record<string, [number, number][]> };
  reserve: Record<string, Reserve>;
  leader: { perLevel: number; maxLevel: number };
  combat: {
    fighters: number; bombers: number; escortShare: number; bomberDefence: number; maxBomberLoss: number; flak: number; seaCover: number;
    night: { fighters: number; accuracy: number; flak: number }; luck: number; intensity: Record<string, number>;
  };
  damage: {
    airfields: { scale: number; max: number; ground: number; maxGround: number };
    industry: { scale: number; max: number; maxShare: number };
    shipping: { scale: number; sink: number; maxSink: number; coastal: number; tradeScale: number };
  };
  sky: { win: number; least: number; lasts: number; land: number; seaOwn: number; seaFoe: number };
  airfields: { holds: number };
  ready: { support: number; rest: number; flying: number; sortie: number; nightSortie: number; defend: number; move: number; floor: number };
  fleets: FleetRow[];
  plan: AirPlan[];
};

export const MISSION: Record<AirMission, { label: string; what: string }> = {
  support: { label: 'Support the armies', what: 'Flies for the armies and defends its own sky.' },
  superiority: { label: 'Win the sky', what: 'Fighters and bombers go for the enemy air force and its airfields over one region.' },
  bomb: { label: 'Bomb industry', what: 'Bombers attack the factories of one region by day, with fighters if it is in their range.' },
  night: { label: 'Bomb by night', what: 'Bombers attack by night: few are lost, and few bombs find the factories.' },
  strike: { label: 'Strike shipping', what: 'Bombers hunt warships and merchant ships in one sea.' },
  rest: { label: 'Rest and refit', what: 'Stands down to refit. It gets replacements first and does not fly.' },
};
const OFFENSIVE: AirMission[] = ['superiority', 'bomb', 'night', 'strike'];
export const isOffensive = (m: AirMission): boolean => OFFENSIVE.includes(m);

const SURFACE = ['bb', 'cv', 'ca', 'dd'] as const;
const spec = (nat: string): NatSpec => AIR.nations[nat] ?? AIR.nations._default;
/** The air war has its own dice, so that it never changes how a land or sea battle falls out. */
const airRng = (g: Game, key: string): Rng => new Rng(hashString(`air:${g.seed}:${g.turn}:${key}`));
const row = <T extends number[]>(rows: T[] | undefined, turn: number): T | null => {
  let out: T | null = null;
  for (const r of rows ?? []) if (r[0] <= turn) out = r;
  return out;
};
const list = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const cap = (s: string): string => s[0].toUpperCase() + s.slice(1);
const planes = (n: number, one: string): string => `${n.toLocaleString('en-US')} ${one}${n === 1 ? '' : 's'}`;

export const airFleetsOf = (g: Game, nat: string): AirFleet[] => g.airFleets.filter((f) => f.nat === nat);
/** How far one fleet reaches: its nation's ranges, unless the fleet has long-range fighters of its own ('ftrRange' on its row in air.json). */
const reach = (f: { nat: string; name: string }): NatSpec => {
  const own = AIR.fleets.find((x) => x.nat === f.nat && x.name === f.name)?.ftrRange;
  return own ? { ...spec(f.nat), ftrRange: own } : spec(f.nat);
};
export const airRange = (f: { nat: string; name: string }): { ftr: number; bmr: number } => ({ ftr: reach(f).ftrRange, bmr: reach(f).bmrRange });

export function airPool(g: Game, nat: string): AirPool {
  return (g.airPool[nat] ??= { ftr: 0, bmr: 0, pilots: 0, lostFtr: 0, lostBmr: 0, lostPilots: 0 });
}

/** Kilometres between the centres of two regions. */
export function km(g: Game, a: number, b: number): number {
  const A = g.regions[a], B = g.regions[b];
  return Math.hypot(A.x - B.x, A.y - B.y) * AIR.kmPerUnit;
}

/** How far a base is from a sea: nothing if it stands on its shore, otherwise the distance to the nearest shore. */
export function seaKm(g: Game, base: number, z: number): number {
  let d = Infinity;
  for (const c of g.seas[z].coast) d = Math.min(d, c === base ? 0 : km(g, base, c));
  return d;
}

/** How much of a fighter force is still effective at this distance: all of it close in, less toward the limit of its range, none beyond. */
export function cover(d: number, range: number): number {
  if (d > range) return 0;
  const full = AIR.range.fullAt * range;
  if (d <= full) return 1;
  return 1 - (1 - AIR.range.atLimit) * ((d - full) / (range - full));
}

/** How much of a fleet can fly from where it is based: all of it, unless more aircraft share the region's airfields than they can serve. */
export function crowding(g: Game, f: AirFleet): number {
  const there = g.airFleets.reduce((s, x) => s + (x.base === f.base ? x.ftr + x.bmr : 0), 0);
  return there > AIR.airfields.holds ? AIR.airfields.holds / there : 1;
}

const leaderMul = (f: AirFleet): number => 1 + (f.leader ? AIR.leader.perLevel * (f.leader.level - 1) : 0);
/** What one of a fleet's aircraft is worth in a fight. */
const worth = (g: Game, f: AirFleet): number => spec(f.nat).quality * leaderMul(f) * (0.5 + 0.5 * f.ready) * techMul(g, f.nat, 'fighters');

/** Who holds the sky over a region, if anyone has won it lately. */
export function skyOver(g: Game, region: number): string | null {
  const s = g.sky[region];
  if (!s || g.turn - s.turn > AIR.sky.lasts || !g.nations[s.nat]?.alive) return null;
  return atWar(g, s.nat, g.regions[region].ctrl) ? s.nat : null;
}

/** The fighters that would come up over a region against this nation: every enemy fleet left to defend, as far as its range and airfields allow. */
export function fightersOver(g: Game, region: number, against: string): number {
  const holder = g.regions[region].ctrl;
  let n = 0;
  for (const f of g.airFleets) {
    if (f.ftr <= 0 || f.mission !== 'support' || !atWar(g, f.nat, against) || !allied(g, f.nat, holder)) continue;
    n += f.ftr * cover(km(g, f.base, region), reach(f).ftrRange) * crowding(g, f);
  }
  return n;
}

/** True when enemy fighters still defend the sky over a coast and the invader has not won it: no invasion fleet can cross under them. */
export function skyDenies(g: Game, nat: string, region: number): boolean {
  const sky = skyOver(g, region);
  if (sky !== null && allied(g, sky, nat)) return false;
  return fightersOver(g, region, nat) >= AIR.sky.least;
}

/** The share of a nation's production stopped this month by bombing and by strikes on its coastal trade. */
export function airIncomeLoss(g: Game, nat: string): number {
  const r = g.raids[nat];
  return r && r.turn === g.turn ? r.share : 0;
}

function raid(g: Game, by: string, victim: string, share: number): void {
  if (share <= 0) return;
  // The loss falls on the victim's next month of production: this month if it has yet to move, otherwise the next.
  const when = g.order.indexOf(by) < g.order.indexOf(victim) ? g.turn : g.turn + 1;
  const old = g.raids[victim];
  g.raids[victim] = { turn: when, share: Math.min(AIR.damage.industry.maxShare, (old && old.turn === when ? old.share : 0) + share) };
}

/** The radar chain that warns of raids on this ground, if its owner has one and still holds it. */
function radarAt(g: Game, r: Region): Chain | null {
  if (r.owner !== r.ctrl) return null;
  return AIR.radar.chains.find((c) => c.nat === r.ctrl && c.from <= g.turn) ?? null;
}

// ---- Setting up ---------------------------------------------------------------------------

export function setupAir(g: Game): void {
  g.airFleets = [];
  g.nextAir = 1;
  g.airReports = [];
  g.airPool = {};
  g.sky = {};
  g.raids = {};
  for (const r of AIR.fleets) if ((r.from ?? 1) <= 1) addAir(g, r);
}

function addAir(g: Game, r: FleetRow): AirFleet | null {
  const base = g.byName[r.base];
  if (!base) return null;
  const e = row(r.est, Math.max(g.turn, r.from ?? 1)) ?? r.est[0];
  const f: AirFleet = {
    id: g.nextAir++, nat: r.nat, name: r.name, base: base.id,
    ftr: r.start ? r.start[0] : e[1], bmr: r.start ? r.start[1] : e[2], est: { ftr: e[1], bmr: e[2] },
    mission: 'support', target: -1, ready: 1, leader: r.leader ? { name: r.leader, level: 1, xp: 0 } : null, moved: 0, lost: { ftr: 0, bmr: 0 },
  };
  g.airFleets.push(f);
  return f;
}

// ---- Bases and orders ---------------------------------------------------------------------

export function whyNotBase(g: Game, f: AirFleet, r: Region): string | null {
  if (r.id === f.base) return 'It is already based there.';
  if (!allied(g, f.nat, r.ctrl)) return 'That ground is not in friendly hands.';
  if (r.since >= g.turn) return 'Airfields taken this month are not yet in use.';
  if (f.moved === g.turn) return 'It has already changed base this month.';
  if (km(g, f.base, r.id) > AIR.ferry) return 'Too far to move in one month.';
  return null;
}

export const airBases = (g: Game, f: AirFleet): Region[] => g.regions.filter((r) => whyNotBase(g, f, r) === null);

/** Moves a fleet to airfields in another friendly region. Settling in costs it some readiness. */
export function rebaseAir(g: Game, f: AirFleet, r: Region): boolean {
  if (whyNotBase(g, f, r)) return false;
  f.base = r.id;
  f.moved = g.turn;
  f.ready = Math.max(AIR.ready.floor, f.ready - AIR.ready.move);
  if (isOffensive(f.mission) && whyNotMission(g, f, f.mission, f.target)) { f.mission = 'support'; f.target = -1; }
  return true;
}

/** True when a strike on this sea has something to hit: enemy ships at sea, or an enemy's trade along its coast. */
function shipping(g: Game, nat: string, z: number): boolean {
  if (g.fleets.some((x) => x.zone === z && atSea(x) && atWar(g, nat, x.nat))) return true;
  if (NAVY.convoys.some((c) => atWar(g, nat, c.to) && c.zones.includes(g.seas[z].key))) return true;
  return g.seas[z].coast.some((id) => g.regions[id].port && atWar(g, nat, g.regions[id].ctrl));
}

/** Why a fleet cannot fly this mission against this target, or null if it can. */
export function whyNotMission(g: Game, f: AirFleet, m: AirMission, target: number): string | null {
  if (m === 'support' || m === 'rest') return null;
  if (!g.nations[f.nat].atWar.length) return 'The nation is at peace.';
  const R = reach(f);
  if (m === 'strike') {
    if (!g.seas[target]) return 'Pick a sea.';
    if (!f.bmr) return 'It has no bombers.';
    if (seaKm(g, f.base, target) > R.bmrRange) return 'That sea is beyond its bombers.';
    return shipping(g, f.nat, target) ? null : 'There is no enemy shipping there.';
  }
  const r = g.regions[target];
  if (!r) return 'Pick a region.';
  if (!atWar(g, f.nat, r.ctrl)) return 'That region is not in enemy hands.';
  const d = km(g, f.base, target);
  if (m === 'superiority') return (f.ftr > 0 && d <= R.ftrRange) || (f.bmr > 0 && d <= R.bmrRange) ? null : 'It is out of range.';
  if (!f.bmr) return 'It has no bombers.';
  if (d > R.bmrRange) return 'It is beyond its bombers.';
  return r.fac > 0 ? null : 'There is no industry there to bomb.';
}

/** Everything a fleet could be sent against on this mission: region ids, or sea ids for a strike. */
export function airTargets(g: Game, f: AirFleet, m: AirMission): number[] {
  if (!isOffensive(m)) return [];
  const all = m === 'strike' ? g.seas.map((z) => z.id) : g.regions.map((r) => r.id);
  return all.filter((t) => whyNotMission(g, f, m, t) === null);
}

export function setMission(g: Game, f: AirFleet, m: AirMission, target = -1): boolean {
  if (whyNotMission(g, f, m, target)) return false;
  f.mission = m;
  f.target = isOffensive(m) ? target : -1;
  return true;
}

// ---- The month in the air -----------------------------------------------------------------

/** The start of a nation's month in the air: new formations, lost bases, new aircraft and pilots, and rest. */
export function airUpkeep(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (!n?.alive) return;
  for (const r of AIR.fleets) {
    if (r.nat !== nat || (r.from ?? 1) <= 1 || g.turn < r.from! || g.flags[`air:${r.name}`]) continue;
    // An air force history raised for an invasion of its own planning is nobody's to raise in free play.
    if (r.scripted && g.flags.open === 1) continue;
    const base = g.byName[r.base];
    if (!base || base.ctrl !== nat || base.since >= g.turn) continue;
    const f = addAir(g, r);
    if (!f) continue;
    g.flags[`air:${r.name}`] = g.turn;
    headline(g, 'info', `${f.name} forms in ${base.name}.`, nat, base.id);
  }
  const p = airPool(g, nat);
  const built = row(AIR.production[nat], g.turn), trained = row(AIR.pilots.perMonth[nat], g.turn);
  if (built) { p.ftr += built[1]; p.bmr += built[2]; }
  if (trained) p.pilots += trained[1];
  for (const f of airFleetsOf(g, nat)) {
    if (!allied(g, nat, g.regions[f.base].ctrl) && !fallBack(g, f)) continue;
    // The staff moves strength between fleets by changing what each is kept at.
    const r = AIR.fleets.find((x) => x.nat === nat && x.name === f.name);
    const e = r ? row(r.est, g.turn) : null;
    if (e) f.est = { ftr: e[1], bmr: e[2] };
    if (f.ftr > f.est.ftr) { const s = f.ftr - f.est.ftr; f.ftr -= s; p.ftr += s; p.pilots += s; }
    if (f.bmr > f.est.bmr) { const s = f.bmr - f.est.bmr; f.bmr -= s; p.bmr += s; }
  }
  // Replacements go to resting fleets first.
  const order = airFleetsOf(g, nat).sort((a, b) => (a.mission === 'rest' ? 0 : 1) - (b.mission === 'rest' ? 0 : 1) || a.id - b.id);
  const R = AIR.ready;
  for (const f of order) {
    const nf = Math.max(0, Math.min(f.est.ftr - f.ftr, p.ftr, p.pilots));
    f.ftr += nf; p.ftr -= nf; p.pilots -= nf;
    const nb = Math.max(0, Math.min(f.est.bmr - f.bmr, p.bmr));
    f.bmr += nb; p.bmr -= nb;
    f.ready = clamp(f.ready + (f.mission === 'rest' ? R.rest : f.mission === 'support' ? R.support : R.flying), R.floor, 1);
    // Orders that no longer make sense lapse.
    if (!n.atWar.length || (isOffensive(f.mission) && whyNotMission(g, f, f.mission, f.target))) { f.mission = 'support'; f.target = -1; }
  }
  const most = AIR.reserve[nat] ?? AIR.reserve._default;
  p.ftr = Math.min(p.ftr, most.ftr);
  p.bmr = Math.min(p.bmr, most.bmr);
  p.pilots = Math.min(p.pilots, most.pilots);
}

/** A fleet whose base has fallen flies out to the nearest ground its nation still holds, leaving some aircraft behind. */
function fallBack(g: Game, f: AirFleet): boolean {
  const old = g.regions[f.base];
  let best: Region | null = null, bd = Infinity;
  for (const r of g.regions) {
    if (r.ctrl !== f.nat) continue;
    const d = Math.hypot(r.x - old.x, r.y - old.y);
    if (d < bd) { bd = d; best = r; }
  }
  const p = airPool(g, f.nat);
  if (!best) {
    g.airFleets.splice(g.airFleets.indexOf(f), 1);
    p.lostFtr += f.ftr; p.lostBmr += f.bmr;
    headline(g, 'event', `With nowhere left to fly from, ${f.name} is disbanded.`, f.nat, old.id);
    return false;
  }
  const lf = Math.round(f.ftr * 0.1), lb = Math.round(f.bmr * 0.1);
  f.ftr -= lf; f.bmr -= lb;
  f.lost.ftr += lf; f.lost.bmr += lb;
  p.lostFtr += lf; p.lostBmr += lb; p.pilots += lf;
  f.base = best.id;
  f.ready = Math.max(AIR.ready.floor, f.ready - AIR.ready.move);
  f.mission = 'support';
  f.target = -1;
  headline(g, 'event', `With ${old.name} lost, ${f.name} falls back to airfields in ${best.name}.`, f.nat, best.id);
  return true;
}

/** When a nation falls its air force leaves the war. */
export function airOnFall(g: Game, nat: string): void {
  g.airFleets = g.airFleets.filter((f) => f.nat !== nat);
  for (const k of Object.keys(g.sky)) if (g.sky[+k].nat === nat) delete g.sky[+k];
}

// ---- An air campaign, weighed up -----------------------------------------------------------

export interface AirAssess {
  /** Why the mission cannot be flown at all, or null. */
  why: string | null;
  place: string;
  att: { f: AirFleet; d: number; esc: number; crowd: number; ftr: number; bmr: number }[];
  def: { f: AirFleet; d: number; w: number }[];
  /** Escorting fighters that count over the target, and their fighting weight. */
  E: number;
  Epow: number;
  /** Bombers that can reach it. */
  B: number;
  /** Defending fighters that will meet the raids, and their fighting weight. */
  D: number;
  Dpow: number;
  radar: Chain | null;
  /** Share of the bombers expected to be lost. */
  bomberLoss: number;
  lines: TicketLine[];
}

/** What the map says about one air campaign before it is flown: who can reach, who will meet them, and what helps each side. */
export function assessAir(g: Game, fleets: AirFleet[], m: AirMission, target: number): AirAssess {
  const C = AIR.combat;
  const sea = m === 'strike', night = m === 'night';
  const nat = fleets[0]?.nat ?? '';
  const place = sea ? `the ${g.seas[target]?.name ?? 'sea'}` : g.regions[target]?.name ?? '';
  const out: AirAssess = { why: null, place, att: [], def: [], E: 0, Epow: 0, B: 0, D: 0, Dpow: 0, radar: null, bomberLoss: 0, lines: [] };
  if (!fleets.length) { out.why = 'No air fleet has these orders.'; return out; }
  for (const f of fleets) {
    const d = sea ? seaKm(g, f.base, target) : km(g, f.base, target);
    const R = reach(f);
    const esc = night ? 0 : cover(d, R.ftrRange);
    const crowd = crowding(g, f);
    const a = { f, d, esc, crowd, ftr: f.ftr * esc * crowd, bmr: d <= R.bmrRange ? f.bmr * crowd : 0 };
    if (a.ftr <= 0 && a.bmr <= 0) continue;
    out.att.push(a);
    out.E += a.ftr;
    out.Epow += a.ftr * worth(g, f);
    out.B += a.bmr;
  }
  if (!out.att.length) { out.why = 'Nothing can reach the target.'; return out; }
  if (m !== 'superiority' && out.B <= 0) { out.why = 'No bombers can reach the target.'; return out; }

  // Radar, and the fighters that come up to meet the raids.
  if (sea) {
    for (const id of g.seas[target].coast) {
      const c = radarAt(g, g.regions[id]);
      if (c && atWar(g, nat, c.nat)) { out.radar = c; break; }
    }
  } else out.radar = radarAt(g, g.regions[target]);
  const holder = sea ? null : g.regions[target].ctrl;
  for (const f of g.airFleets) {
    if (f.ftr <= 0 || f.mission !== 'support' || !atWar(g, f.nat, nat)) continue;
    if (holder && !allied(g, f.nat, holder)) continue;
    const d = sea ? seaKm(g, f.base, target) : km(g, f.base, target);
    let w = cover(d, reach(f).ftrRange) * crowding(g, f);
    if (sea) w *= C.seaCover;
    if (night) w *= C.night.fighters;
    if (!out.radar) w *= AIR.radar.without;
    if (w <= 0) continue;
    out.def.push({ f, d, w });
    out.D += f.ftr * w;
    // Fighters over their own ground are only as good as the radar and controllers that put them onto the raid.
    out.Dpow += f.ftr * w * worth(g, f) * techMul(g, f.nat, 'radar');
  }
  const guns = out.B * C.bomberDefence;
  const I = C.intensity[m];
  out.bomberLoss = out.B > 0 ? Math.min(C.maxBomberLoss, C.bombers * I * (out.Dpow / Math.max(1, out.Dpow + out.Epow + guns)) + (night ? C.night.flak : C.flak) * I) : 0;

  // The Battle Ticket.
  const af = out.att.reduce((s, a) => s + a.f.ftr, 0);
  const ab = out.att.reduce((s, a) => s + (a.bmr > 0 ? a.f.bmr : 0), 0);
  const df = out.def.reduce((s, x) => s + x.f.ftr, 0);
  out.lines.push({
    label: 'Forces',
    value: `${[af ? planes(af, 'fighter') : '', ab ? planes(ab, 'bomber') : ''].filter(Boolean).join(' and ')} against ${df ? planes(df, 'fighter') : 'no fighters'}`,
    effect: out.D < df ? `About ${Math.round(out.D).toLocaleString('en-US')} of the defending fighters can meet the raids` : undefined,
  });
  if (night) out.lines.push({ label: 'Night', value: 'Flown in darkness', effect: 'Few fighters find the bombers, and only about a third of the bombs find the target' });
  else if (af > 0) {
    const far = out.att.filter((a) => a.f.ftr > 0).sort((a, b) => a.esc - b.esc)[0];
    const pct = Math.round((out.att.reduce((s, a) => s + a.f.ftr * a.esc, 0) / af) * 100);
    out.lines.push({
      label: 'Range',
      value: pct >= 100 ? 'The target is well inside fighter range' : pct > 0 ? `The fighters are at ${pct}% effect over the target` : 'The target is beyond fighter range',
      effect: pct >= 100 ? undefined : pct > 0 ? `${far.f.name} flies ${Math.round(far.d)} km to get there, near the limit of its fighters` : 'The bombers go alone',
    });
  } else if (out.B > 0) out.lines.push({ label: 'Range', value: 'No escort', effect: 'The bombers go alone' });
  const packed = [...out.att.map((a) => a.f), ...out.def.map((x) => x.f)].filter((f) => crowding(g, f) < 1);
  if (packed.length) {
    const bases = [...new Set(packed.map((f) => g.regions[f.base].name))];
    const least = Math.min(...packed.map((f) => crowding(g, f)));
    out.lines.push({
      label: 'Airfields',
      value: `The airfields of ${list(bases)} are crowded`,
      effect: `${list(packed.map((f) => f.name))} can fly only ${Math.round(least * 100)}% of ${packed.length === 1 ? 'its' : 'their'} aircraft from there`,
    });
  }
  if (df > 0 || out.radar) {
    out.lines.push(out.radar
      ? { label: 'Radar', value: `${cap(out.radar.name)} gives warning`, effect: 'Every defending fighter in reach gets up in time, and fewer aircraft are caught on the ground' }
      : { label: 'Radar', value: 'No radar warning', effect: `Only ${Math.round(AIR.radar.without * 100)}% of the defending fighters meet the raids` });
  }
  const kitA = techNotes(g, out.att.map((a) => a.f.nat), out.B > 0 ? ['fighters', 'bombers'] : ['fighters']), kitD = techNotes(g, out.def.map((x) => x.f.nat), ['fighters', 'radar']);
  if (kitA.length || kitD.length) {
    const up = (k: number): string => `${k > 0 ? 'up' : 'down'} ${Math.abs(k)}%`;
    out.lines.push({ label: 'Equipment', value: [...kitA, ...kitD].map((x) => x.text).join('; '), effect: [...kitA.map((x) => `Attacker ${up(x.pct)}`), ...kitD.map((x) => `Defender ${up(x.pct)}`)].join(', ') });
  }
  const names = (fs: AirFleet[]): string => list(fs.filter((f) => f.leader).map((f) => `${f.leader!.name}${f.leader!.level > 1 ? ` (level ${f.leader!.level})` : ''}`)) || 'no commander named';
  out.lines.push({ label: 'Commanders', value: `${names(out.att.map((a) => a.f))} against ${names(out.def.map((x) => x.f))}` });
  const worn = [...out.att.map((a) => a.f), ...out.def.map((x) => x.f)].filter((f) => f.ready < 0.8);
  if (worn.length) out.lines.push({ label: 'Readiness', value: `${list(worn.map((f) => f.name))} not fully fit to fly`, effect: 'A worn fleet fights below its strength' });
  return out;
}

/** The player's look at a mission before ordering it: this fleet together with any others already given the same orders. */
export function previewAir(g: Game, f: AirFleet, m: AirMission, target: number): AirAssess {
  const group = airFleetsOf(g, f.nat).filter((x) => x.id === f.id || (x.mission === m && x.target === target));
  return assessAir(g, group, m, target);
}

// ---- Flying it ------------------------------------------------------------------------------

function gainXp(f: AirFleet, won: boolean): void {
  const l = f.leader;
  if (!l) return;
  l.xp += won ? 2 : 1;
  if (l.level < AIR.leader.maxLevel && l.xp >= l.level * 3) { l.xp = 0; l.level++; }
}

function fly(g: Game, nat: string, m: AirMission, target: number, fleets: AirFleet[]): AirReport | null {
  const a = assessAir(g, fleets, m, target);
  if (a.why) return null;
  const C = AIR.combat, I = C.intensity[m];
  const sea = m === 'strike', night = m === 'night';
  const rng = airRng(g, `${m}:${target}:${nat}`);
  const luck = (): number => rng.range(1 - C.luck, 1 + C.luck);
  const before = (f: AirFleet): AirSide => ({ nat: f.nat, fleet: f.name, leader: f.leader?.name ?? null, ftr: f.ftr, bmr: f.bmr, lostFtr: 0, lostBmr: 0 });
  const att = a.att.map((x) => before(x.f)), def = a.def.map((x) => before(x.f));

  // Fighter against fighter, then the defenders against the bombers.
  // Numbers tell, but less than in proportion: the smaller side picks its moments.
  const se = Math.sqrt(a.Epow), sd = Math.sqrt(a.Dpow);
  const share = se + sd > 0 ? se / (se + sd) : 0;
  const clash = Math.min(a.E, a.D);
  const dLoss = C.fighters * I * clash * 2 * share * luck();
  const eLoss = C.fighters * I * clash * 2 * (1 - share) * C.escortShare * luck();
  const bLoss = a.B * Math.min(C.maxBomberLoss, a.bomberLoss * luck());
  // Pilots who come down over their own ground mostly fly again; over the enemy's, or the sea, few do.
  const saved = (home: boolean): number => (sea ? (AIR.pilots.home + AIR.pilots.away) / 2 : home ? AIR.pilots.home : AIR.pilots.away);
  const down = (f: AirFleet, side: AirSide, n: number, home: boolean): void => {
    const k = Math.min(f.ftr, rng.roll(Math.max(0, n)));
    if (!k) return;
    const p = airPool(g, f.nat);
    const back = rng.roll(k * saved(home));
    f.ftr -= k; f.lost.ftr += k; side.lostFtr += k;
    p.lostFtr += k; p.pilots += back; p.lostPilots += k - back;
  };
  const bomber = (f: AirFleet, side: AirSide, n: number): void => {
    const k = Math.min(f.bmr, rng.roll(Math.max(0, n)));
    f.bmr -= k; f.lost.bmr += k; side.lostBmr += k;
    airPool(g, f.nat).lostBmr += k;
  };
  a.def.forEach((x, i) => down(x.f, def[i], a.D > 0 ? dLoss * ((x.f.ftr * x.w) / a.D) : 0, true));
  a.att.forEach((x, i) => {
    down(x.f, att[i], a.E > 0 ? eLoss * (x.ftr / a.E) : 0, false);
    bomber(x.f, att[i], a.B > 0 ? bLoss * (x.bmr / a.B) : 0);
  });
  const lostB = att.reduce((s, x) => s + x.lostBmr, 0);
  const through = Math.max(0, a.B - lostB) * (night ? C.night.accuracy : 1) * techMul(g, nat, 'bombers');

  // What the bombers did.
  let result = '', value = 0, ground = 0;
  if (m === 'superiority') {
    const A = AIR.damage.airfields;
    const hit = g.airFleets.filter((f) => f.base === target && atWar(g, f.nat, nat));
    const cut = Math.min(A.max, through / A.scale);
    for (const f of hit) {
      f.ready = Math.max(AIR.ready.floor, f.ready - cut);
      const all = f.ftr + f.bmr;
      const n = Math.min(all * A.maxGround, (through * A.ground * (a.radar ? a.radar.ground : 1)) / hit.length);
      const gf = all ? Math.min(f.ftr, rng.roll(n * (f.ftr / all))) : 0, gb = all ? Math.min(f.bmr, rng.roll(n * (f.bmr / all))) : 0;
      const p = airPool(g, f.nat);
      f.ftr -= gf; f.bmr -= gb; f.lost.ftr += gf; f.lost.bmr += gb;
      p.lostFtr += gf; p.lostBmr += gb; p.pilots += gf;
      const side = def.find((s) => s.fleet === f.name && s.nat === f.nat);
      if (side) { side.lostFtr += gf; side.lostBmr += gb; }
      ground += gf + gb;
      value += cut * 300;
    }
    result = through <= 0 ? '' : hit.length
      ? `The airfields of ${list(hit.map((f) => f.name))} were bombed: readiness cut by ${Math.round(cut * 100)}%${ground ? ` and ${planes(ground, 'aircraft').replace('aircrafts', 'aircraft')} destroyed on the ground` : ''}.`
      : 'The bombers found no airfields in use.';
  } else if (m === 'bomb' || night) {
    const N = AIR.damage.industry;
    const r = g.regions[target];
    const total = g.regions.reduce((s, x) => s + (x.ctrl === r.ctrl ? x.fac : 0), 0);
    const cut = Math.min(N.max, through / N.scale);
    const nation = total > 0 ? cut * (r.fac / total) : 0;
    raid(g, nat, r.ctrl, nation);
    value += cut * 1000;
    result = `The bombing stopped ${Math.round(cut * 100)}% of ${r.name}'s output for a month: ${(nation * 100).toFixed(1)}% of ${g.nations[r.ctrl].adj} production.`;
  } else {
    const S = AIR.damage.shipping;
    const zone = g.seas[target];
    const frac = Math.min(S.maxSink, S.sink * (through / (through + S.scale)));
    const none = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
    const sunk = none();
    for (const f of g.fleets) {
      if (f.zone !== target || !atSea(f) || !atWar(g, nat, f.nat) || !surfaceShips(f)) continue;
      const s = sinkShips(f, frac, rng, [...SURFACE]);
      f.ready = Math.max(0, f.ready - 0.1);
      for (const c of SURFACE) sunk[c] += s[c];
    }
    // Trade: an ocean convoy route through this sea, or the coasters along an enemy shore.
    const hurt: Record<string, number> = {};
    const weight = (through / (through + S.tradeScale)) * zone.air;
    for (const c of NAVY.convoys) if (atWar(g, nat, c.to) && c.zones.includes(zone.key)) hurt[c.to] = Math.max(hurt[c.to] ?? 0, (c.share / c.zones.length) * weight);
    for (const id of zone.coast) {
      const r = g.regions[id];
      if (r.port && r.owner === r.ctrl && atWar(g, nat, r.ctrl)) hurt[r.ctrl] = Math.max(hurt[r.ctrl] ?? 0, S.coastal * weight);
    }
    for (const [victim, s] of Object.entries(hurt)) raid(g, nat, victim, s);
    const worst = Object.entries(hurt).sort((x, y) => y[1] - x[1])[0];
    value += shipCount(sunk) * 30 + (worst ? worst[1] * 3000 : 0);
    result = [shipCount(sunk) ? `${cap(shipList(sunk))} sunk.` : '', worst ? `Merchant shipping losses cost ${g.nations[worst[0]].name} ${(worst[1] * 100).toFixed(1)}% of a month's production.` : ''].filter(Boolean).join(' ') || 'The bombers found little to hit.';
  }

  // Has the sky been won? The attacker's surviving fighters against what the defender can put up once his losses are made good.
  let sky = false, hold = 0;
  if (m === 'superiority') {
    const Eafter = a.att.reduce((s, x) => s + x.f.ftr * x.esc * x.crowd, 0);
    const EpowAfter = a.att.reduce((s, x) => s + x.f.ftr * x.esc * x.crowd * worth(g, x.f), 0);
    const lostBy: Record<string, number> = {};
    def.forEach((s) => { lostBy[s.nat] = (lostBy[s.nat] ?? 0) + s.lostFtr; });
    let Dnext = 0;
    a.def.forEach((x, i) => {
      const p = airPool(g, x.f.nat);
      const built = row(AIR.production[x.f.nat], g.turn + 1), trained = row(AIR.pilots.perMonth[x.f.nat], g.turn + 1);
      const can = Math.min(p.ftr + (built ? built[1] : 0), p.pilots + (trained ? trained[1] : 0));
      const lost = lostBy[x.f.nat] ?? 0;
      const back = lost > 0 ? Math.min(lost, can) * (def[i].lostFtr / lost) : 0;
      Dnext += Math.min(x.f.est.ftr, x.f.ftr + back) * x.w * worth(g, x.f) * techMul(g, x.f.nat, 'radar');
    });
    hold = Dnext > 0 ? EpowAfter / Dnext : 99;
    sky = Eafter >= AIR.sky.least && hold >= AIR.sky.win;
    if (sky) g.sky[target] = { nat, turn: g.turn };
  }

  // Wear, and what the commanders learned.
  const aLost = att.reduce((s, x) => s + x.lostFtr + x.lostBmr, 0);
  const dLost = def.reduce((s, x) => s + x.lostFtr + x.lostBmr, 0);
  const ratio = (dLost + value + 10) / (aLost + 10);
  const won = sky || ratio >= 1;
  for (const x of a.att) { x.f.ready = Math.max(AIR.ready.floor, x.f.ready - (night ? AIR.ready.nightSortie : AIR.ready.sortie)); gainXp(x.f, won); }
  for (const x of a.def) { x.f.ready = Math.max(AIR.ready.floor, x.f.ready - AIR.ready.defend * I * (night ? 0.2 : 1)); gainXp(x.f, !won); }
  const level: VictoryLevel = sky || ratio >= 2 ? 'Decisive victory' : ratio >= 1 ? 'Close victory' : ratio >= 0.5 ? 'Close defeat' : 'Decisive defeat';

  g.flags.airBattles = (g.flags.airBattles ?? 0) + 1;
  const defNat = def[0]?.nat ?? (sea ? '' : g.regions[target].ctrl);
  const rep: AirReport = {
    id: g.flags.airBattles, turn: g.turn, mission: m, region: sea ? -1 : target, zone: sea ? target : -1, place: a.place,
    attNat: nat, defNat, att, def, lines: a.lines, odds: a.Dpow > 0 ? a.Epow / a.Dpow : 9, level, won, sky, hold, result,
  };
  g.airReports.push(rep);

  const who = list(fleets.filter((f) => a.att.some((x) => x.f === f)).map((f) => f.name));
  const cost = `${planes(aLost, 'aircraft').replace('aircrafts', 'aircraft')} lost`;
  const adj = defNat ? g.nations[defNat].adj : 'enemy';
  // Once the British and American air forces fight the same enemy side by side, the sky they win is the Allies'.
  const together = !!defNat && (nat === 'UK' || nat === 'USA') && ['UK', 'USA'].every((x) => g.nations[x]?.alive && g.nations[x].atWar.includes(defNat));
  let text: string;
  if (m === 'superiority') text = `${who} ${a.att.length === 1 ? 'fights' : 'fight'} for the sky over ${a.place}: ${dLost} ${adj} aircraft destroyed, ${cost}. ${sky ? (together ? 'The Allied air forces now hold the sky there.' : `The ${g.nations[nat].adj} air force now holds the sky there.`) : df(def) ? 'The defenders still hold their sky.' : 'No fighters came up to meet them.'}`;
  else if (sea) text = `${who} ${a.att.length === 1 ? 'strikes' : 'strike'} at shipping in ${a.place}. ${result} ${cap(cost)}.`;
  else text = `${who} ${a.att.length === 1 ? 'bombs' : 'bomb'} ${a.place}${night ? ' by night' : ''}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  headline(g, 'battle', text, nat, sea ? undefined : target);
  return rep;
}

const df = (sides: AirSide[]): number => sides.reduce((s, x) => s + x.ftr, 0);

/** The air campaigns a nation ordered this month, flown once all its orders are given. */
export function airBattles(g: Game, nat: string): AirReport[] {
  const out: AirReport[] = [];
  if (!g.nations[nat]?.alive) return out;
  const groups = new Map<string, AirFleet[]>();
  for (const f of airFleetsOf(g, nat)) {
    if (!isOffensive(f.mission) || whyNotMission(g, f, f.mission, f.target)) continue;
    const k = `${OFFENSIVE.indexOf(f.mission)}:${f.target}`;
    if (!groups.has(k)) groups.set(k, []);
    groups.get(k)!.push(f);
  }
  // The fight for the sky comes first, so that bombed airfields tell on the raids that follow.
  for (const k of [...groups.keys()].sort()) {
    const fleets = groups.get(k)!;
    const rep = fly(g, nat, fleets[0].mission, fleets[0].target, fleets);
    if (rep) out.push(rep);
  }
  return out;
}

// ---- The computer's air staff ----------------------------------------------------------------

/** Orders for one computer-run nation's air fleets: history's plan where it has one, otherwise support for the armies. */
export function airAI(g: Game, nat: string): void {
  if (nat === g.human) return;
  const n = g.nations[nat];
  const mine = airFleetsOf(g, nat);
  for (const f of mine) { f.mission = 'support'; f.target = -1; }
  if (!n.alive || !n.atWar.length) return;
  // In free play history's air plan does not apply: the air fleets support the armies.
  if (g.flags.open === 1) return;
  for (const ph of AIR.plan) {
    if (ph.nat !== nat || g.turn < ph.from || g.turn > ph.to || !atWar(g, nat, ph.against)) continue;
    for (const o of ph.orders) {
      const f = mine.find((x) => x.name === o.fleet);
      if (!f) continue;
      const bases = (o.base ?? []).map((b) => g.byName[b]).filter(Boolean);
      if (bases.length && !bases.some((r) => r.id === f.base)) for (const r of bases) if (rebaseAir(g, f, r)) break;
      const target = o.mission === 'strike' ? zoneOf(g, o.sea ?? '') : o.region ? g.byName[o.region]?.id ?? -1 : -1;
      setMission(g, f, o.mission, target);
    }
  }
}

/** What history's plan has this nation's air fleets doing this month, if anything. */
export function airPlanFor(g: Game, nat: string, turn = g.turn): AirPlan | null {
  if (g.flags.open === 1) return null;
  return AIR.plan.find((p) => p.nat === nat && turn >= p.from && turn <= p.to && atWar(g, nat, p.against)) ?? null;
}

/** The name history gives this month's air operations, if the plan has one for this nation. */
export function airPlanName(g: Game, nat: string): string | null {
  return airPlanFor(g, nat)?.name ?? null;
}

/** New aircraft and pilots a nation gets each month at this point in the war. */
export function airDeliveries(g: Game, nat: string): { ftr: number; bmr: number; pilots: number } {
  const built = row(AIR.production[nat], g.turn), trained = row(AIR.pilots.perMonth[nat], g.turn);
  return { ftr: built ? built[1] : 0, bmr: built ? built[2] : 0, pilots: trained ? trained[1] : 0 };
}

/**
 * What a mission is likely to do, without the dice, for the player to see before he gives the order:
 * the fighter odds going in, the share of the bombers likely to be lost, and what those that get through should achieve.
 * `cut` is readiness lost by the groups based there (superiority), the region's output stopped (bombing) or the share of ships sunk (strike);
 * `nation` is the share of the victim's whole production stopped by bombing.
 */
export function forecastAir(g: Game, a: AirAssess, m: AirMission, target: number): { odds: number; bomberLoss: number; cut: number; nation: number } {
  const through = a.B * (1 - a.bomberLoss) * (m === 'night' ? AIR.combat.night.accuracy : 1) * techMul(g, a.att[0]?.f.nat ?? '', 'bombers');
  const odds = a.Dpow > 0 ? a.Epow / a.Dpow : 9;
  let cut = 0, nation = 0;
  if (m === 'superiority') {
    const based = g.airFleets.some((f) => f.base === target && atWar(g, f.nat, a.att[0]?.f.nat ?? ''));
    cut = based ? Math.min(AIR.damage.airfields.max, through / AIR.damage.airfields.scale) : 0;
  } else if (m === 'bomb' || m === 'night') {
    const r = g.regions[target];
    const total = g.regions.reduce((s, x) => s + (x.ctrl === r.ctrl ? x.fac : 0), 0);
    cut = Math.min(AIR.damage.industry.max, through / AIR.damage.industry.scale);
    nation = total > 0 ? cut * (r.fac / total) : 0;
  } else if (m === 'strike') {
    cut = Math.min(AIR.damage.shipping.maxSink, AIR.damage.shipping.sink * (through / (through + AIR.damage.shipping.scale)));
  }
  return { odds, bomberLoss: a.bomberLoss, cut, nation };
}
