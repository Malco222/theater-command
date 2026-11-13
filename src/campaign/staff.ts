// Free play ("open history"). The war starts as it did on 1 September 1939, and from then on no
// computer-run nation follows history's plan. Each has a staff that looks at the map every month:
// where it is in touch with an enemy, how strong each side is there, and whether an offensive is
// worth opening, pressing or calling off. The great decisions history took (the attack in the west,
// Italy's entry, the attack on the Soviet Union) are choices the staff may or may not make, and when.
// What comes out is an ordinary plan phase, so the armies carry it out by the same rules as in
// Historical mode. The numbers and the list of choices live in data/campaign/open.json.

import { deterred } from './league';
import { openTraits } from './traits';
import { openTech } from './tech';
import { openStores } from './resources';
import openData from '../../data/campaign/open.json';
import { hashString } from '../core/rng';
import { turnToDate } from '../core/ticket';
import { holds } from './diplomacy';
import { concertFor, landingsFor, nerveOf, seat } from './difficulty';
import { DOMINATION, dominating } from './domination';
import { openAirWar } from './air';
import { startEngineers } from './engineers';
import { NAVY } from './navy';
import { EASTERN, allied, atWar, collapseScore, corpsBase, frozen, weather } from './game';
import { FORT_MUL, KIND, TERRAIN_DEF, type Corps, type Game, type PlanPhase, type Region } from './types';

interface Temper { attackAt: number; persist: number; minOdds: number }

/** One of history's great decisions, written as a choice a staff may take up. */
export interface Op {
  id: string; name: string;
  declare?: string[]; if?: string[]; from?: number; to?: number;
  /** The odds in prospect it takes to go, and to keep preparing. */
  ratio?: number;
  /** Months spent bringing the army up before war is declared. */
  prep?: number;
  /** The chance, each month the conditions hold, that the staff takes the decision. */
  chance: number;
  /** Months of the year in which the blow may fall. */
  season?: number[];
  /** Months the offensive is pressed whatever the odds, once begun. */
  push?: number;
  /** A side operation: no build-up, and the main army is not taken for it. */
  side?: boolean;
  landings?: PlanPhase['landings'];
  staging?: Record<string, number>;
  join?: 'allies' | 'axis';
}

const OPEN = openData as unknown as {
  mood: number; major: number; rest: number; stall: number; headOn: number; commit: number; bound: number; boundOverseas: number; thrust: [number, number]; weak: number; foothold: number; general: number; guard: number; guardWithin: number;
  temper: Record<string, Partial<Temper>>;
  ops: Record<string, Op[]>;
};

/** True in a free-play war: the computer's nations make their own decisions. */
export const isOpen = (g: Game): boolean => g.flags.open === 1;

/**
 * Turns a game into a free-play war from this month on. In September 1939 history sets the start: Germany's
 * attack on Poland is already under way. Later in the war each staff finds its fronts as history left them.
 */
export function startOpen(g: Game): Game {
  g.flags.open = 1;
  if (g.turn === 1) {
    g.flags['st:on:GER:POL'] = 1;
    g.flags['st:gain:GER'] = 1;
  }
  startEngineers(g);
  openTraits(g);
  openTech(g);
  openStores(g);
  openAirWar(g);
  // The grand campaign counts its railheads with the supply picture: draw it afresh under these rules.
  g.supplyDirty = true;
  return g;
}

/** A draw between 0 and 1 that depends only on the war's seed and what is being decided, never on the battle dice. */
const draw = (g: Game, ...key: (string | number)[]): number => hashString(`${g.seed}:${key.join(':')}`) / 4294967296;

/** A nation's character in this war: its nerve differs a little from one war to the next. */
export function temper(g: Game, nat: string): Temper {
  const t = { attackAt: 99, persist: 0.85, minOdds: 1.4, ...OPEN.temper._default, ...OPEN.temper[nat] } as Temper;
  const mood = 1 + OPEN.mood * (2 * draw(g, nat, 'mood') - 1);
  // The level of the campaign steadies or stiffens the nerve of the player's rivals: at the fair level, neither.
  const lv = nerveOf(g, nat);
  // In the last act there is nobody to wait for: a staff strikes at shorter odds than it would in an ordinary war.
  const last = dominating(g) ? DOMINATION.nerve : { attackAt: 1, persist: 1 };
  return { attackAt: t.attackAt * mood * lv.nerve * last.attackAt, persist: t.persist * mood * lv.nerve * last.persist, minOdds: t.minOdds * lv.odds };
}

/** A stretch of country where this nation's side is in touch with an enemy: who is there, how strong, and the way in. */
export interface Front {
  /** The enemy who holds most of the country behind it. */
  key: string;
  enemies: string[];
  /** Friendly regions on it, and the enemy regions they face. */
  ours: number[];
  edge: number[];
  /** Steps into enemy country, 1 on the edge, counted from the stretches not covered by strong works. -1 elsewhere. */
  depth: Int16Array;
  /** The friendly land areas that touch it. */
  areas: Set<number>;
  /** Attack strength that can be brought to it, the enemy's defence near it, and the attack he could make from it. */
  atk: number; def: number; threat: number;
  /** Defence already standing on it in fixed positions: garrisons and fortress troops. */
  fixed: number;
  /** atk, less what other fronts need to stay safe, over def. */
  ratio: number;
}

const atkP = (g: Game, c: Corps): number => corpsBase(g, c) * KIND[c.kind].atk * (g.nations[c.nat].slowCommand ? 0.85 : 1);
const defP = (g: Game, c: Corps, r: Region): number => corpsBase(g, c) * KIND[c.kind].def * TERRAIN_DEF[r.terrain];

export interface FrontOpts {
  /** Nations counted as enemies though no war has been declared yet: what a staff weighs before it decides. */
  extra?: Set<string>;
  /** Only this ground is looked at: an army overseas weighs its own theatre. */
  scope?: Set<number> | null;
  /** Ground left out: a side theatre that has a staff of its own. */
  skip?: (r: Region) => boolean;
  /** True for a front where an offensive is already under way: the troops committed to it are not free for another. */
  busy?: (f: Front) => boolean;
}

/** Every front this nation has, with the balance of strength on each. */
export function fronts(g: Game, nat: string, o: FrontOpts = {}): Front[] {
  const N = g.regions.length;
  const foe = (id: string): boolean => atWar(g, nat, id) || !!o.extra?.has(id);
  const inPlay = (r: Region): boolean => (o.scope ? o.scope.has(r.id) : !o.skip?.(r));
  // 1 friendly ground, 2 enemy ground.
  const side = new Int8Array(N);
  for (const r of g.regions) if (inPlay(r)) side[r.id] = foe(r.ctrl) ? 2 : allied(g, nat, r.ctrl) ? 1 : 0;
  const label = new Int16Array(N).fill(-1);
  let groups = 0;
  // In the last act every neighbour is an enemy and none of them is another's friend: each country is a front of
  // its own, to be weighed and taken on one at a time, not one wall of enemies right round the frontier.
  const each = dominating(g);
  for (const r of g.regions) {
    if (!side[r.id] || label[r.id] >= 0) continue;
    const q = [r.id];
    label[r.id] = groups;
    for (let i = 0; i < q.length; i++) {
      for (const j of g.regions[q[i]].nb) {
        if (label[j] >= 0 || side[j] !== side[r.id] || (each && side[j] === 2 && g.regions[j].ctrl !== r.ctrl)) continue;
        label[j] = groups; q.push(j);
      }
    }
    groups++;
  }
  // Friendly areas where some of this nation's army stands: only from those can it reach a front.
  const manned = new Uint8Array(groups);
  for (const c of g.corps) if (c.nat === nat && !c.dead && side[c.reg] === 1) manned[label[c.reg]] = 1;

  const found = new Map<number, { ours: Set<number>; edge: Set<number>; areas: Set<number> }>();
  for (const r of g.regions) {
    if (side[r.id] !== 2) continue;
    for (const j of r.nb) {
      if (side[j] !== 1 || !manned[label[j]]) continue;
      // An ally's country is a way in only if that ally is in this war too: nobody attacks out of a neutral's garden.
      const host = g.regions[j].ctrl;
      if (host !== nat && !atWar(g, host, r.ctrl)) continue;
      let f = found.get(label[r.id]);
      if (!f) found.set(label[r.id], (f = { ours: new Set(), edge: new Set(), areas: new Set() }));
      f.ours.add(j); f.edge.add(r.id); f.areas.add(label[j]);
    }
  }

  const wx = weather(g);
  const out: Front[] = [];
  for (const [comp, f] of found) {
    // Works only stop an attack that comes from the country they were built against: the best way in is what counts.
    const works = (e: Region): number => {
      if (!e.fort) return 1;
      let best = 99;
      for (const j of e.nb) if (f.ours.has(j)) best = Math.min(best, e.fort.vs.includes(g.regions[j].owner) ? FORT_MUL[e.fort.lv] : 1);
      return best === 99 ? 1 : best;
    };
    const edge = [...f.edge];
    const soft = edge.filter((id) => works(g.regions[id]) < 1.5);
    const depth = new Int16Array(N).fill(-1);
    const q = soft.length ? [...soft] : [...edge];
    for (const id of q) depth[id] = 1;
    for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (depth[j] < 0 && label[j] === comp && side[j] === 2) { depth[j] = depth[q[i]] + 1; q.push(j); }

    let def = 0, threat = 0;
    const share: Record<string, number> = {};
    const enemies = new Set<string>();
    for (const id of q) {
      const r = g.regions[id], d = depth[id];
      enemies.add(r.ctrl);
      const near = d === 1 ? 1 : d === 2 ? 0.6 : d === 3 ? 0.35 : 0.12;
      for (const u of r.units) {
        if (!foe(u.nat)) continue;
        enemies.add(u.nat);
        const still = u.hold || u.kind === 'fort';
        // An enemy caught by the Russian winter without the kit for it is weaker than he looks on the map.
        const cold = frozen(g, u.nat, r);
        const p = defP(g, u, r) * (d === 1 ? works(r) : 1) * near * (still && d > 1 ? 0.3 : 1) * (cold ? EASTERN.winter.defence : 1);
        def += p;
        share[u.nat] = (share[u.nat] ?? 0) + p;
        if (!still && d <= 2) threat += atkP(g, u) * (d === 1 ? 1 : 0.6) * (cold ? EASTERN.winter.attack : 1);
      }
    }
    // A stretch of the edge behind strong works is still a place the enemy can come out of.
    for (const id of edge) if (depth[id] > 2) for (const u of g.regions[id].units) if (foe(u.nat) && !u.hold && u.kind !== 'fort') threat += atkP(g, u) * 0.6;

    // Whoever on this side is at war with someone there, and can get there by land, counts toward the attack.
    const ours = [...f.ours];
    const close = new Uint8Array(N);
    for (const id of ours) { close[id] = 1; for (const j of g.regions[id].nb) if (side[j] === 1) close[j] = 1; }
    const fights = (id: string): boolean => id === nat || [...enemies].some((e) => atWar(g, id, e));
    let atk = 0, fixed = 0;
    // Deep in Russia in the bad months an army that has not learned the country attacks at a fraction of its strength.
    const deep = edge.filter((id) => EASTERN.deepOwners.includes(g.regions[id].owner)).length * 2 >= edge.length;
    const season = deep && !EASTERN.winter.hardy.includes(nat) ? (wx.winter ? EASTERN.winter.attack : wx.mud ? 0.8 : 1) : 1;
    for (const c of g.corps) {
      if (c.dead || side[c.reg] !== 1 || !f.areas.has(label[c.reg]) || !allied(g, nat, c.nat) || !fights(c.nat)) continue;
      const r = g.regions[c.reg];
      if (c.kind === 'fort' || c.hold) {
        if (f.ours.has(c.reg)) fixed += defP(g, c, r) * (r.fort ? FORT_MUL[r.fort.lv] : 1);
        // A garrison on the front itself would go in with the rest; one further back stays where it is.
        if (c.kind === 'fort' || c.nat !== nat || !close[c.reg]) continue;
      }
      atk += atkP(g, c) * season;
    }
    // The air forces: whoever is stronger in the air has that much more on the ground.
    const key = Object.keys(share).sort((a, b) => share[b] - share[a])[0] ?? g.regions[edge[0]].ctrl;
    const mine = g.nations[nat].air, his = g.nations[key]?.air ?? 0;
    const airEdge = (mine - his) / (mine + his + 1);
    atk *= 1 + 0.3 * airEdge;
    def *= 1 - 0.2 * airEdge;
    // With no way round, the works have to be stormed head-on, and that takes far more than the sums say.
    if (!soft.length) def *= OPEN.headOn;
    out.push({ key, enemies: [...enemies], ours, edge, depth, areas: f.areas, atk, def: Math.max(def, 0.01), threat, fixed, ratio: 0 });
  }
  // What a front can be given is what the others, reached from the same country, can spare: an offensive already
  // under way elsewhere keeps the force it needs, and a quiet front keeps enough to hold it.
  for (const f of out) {
    let kept = 0;
    for (const x of out) {
      if (x === f || ![...x.areas].some((a) => f.areas.has(a))) continue;
      const cover = Math.max(0, 0.55 * x.threat - 0.7 * x.fixed);
      kept += o.busy?.(x) ? Math.max(cover, Math.min(x.atk, OPEN.commit * x.def)) : cover;
    }
    f.ratio = Math.max(0, f.atk - kept) / f.def;
  }
  return out.sort((a, b) => b.def - a.def);
}

/** The places an offensive on this front is aimed at: the enemy's capitals first, then what is worth having near the front. */
function objectives(g: Game, f: Front): string[] {
  const inside = g.regions.filter((r) => f.depth[r.id] >= 1);
  const caps = inside.filter((r) => r.cap && f.depth[r.id] <= 7 && g.nations[r.owner]?.alive && f.enemies.includes(r.owner)).sort((a, b) => f.depth[a.id] - f.depth[b.id]);
  const worth = inside.filter((r) => f.depth[r.id] <= 4 && (r.vp >= 1 || r.port)).sort((a, b) => f.depth[a.id] - f.depth[b.id] || b.vp - a.vp || a.id - b.id);
  const near = inside.filter((r) => f.depth[r.id] <= 2).sort((a, b) => f.depth[a.id] - f.depth[b.id] || a.id - b.id);
  return [...new Set([...caps, ...worth, ...near].map((r) => r.name))].slice(0, 30);
}

const GOING: Record<string, number> = { plains: 0, desert: 0.3, forest: 0.5, hills: 0.6, urban: 0.8, marsh: 1.5, mountain: 2 };

/** The line the fast troops are sent along: the easiest road from the front to the nearest enemy capital, round the works and the mountains. */
function axis(g: Game, nat: string, f: Front, placed = false): string[] {
  const N = g.regions.length;
  const cost = new Float32Array(N).fill(1e9);
  const from = new Int16Array(N).fill(-1);
  const step = (r: Region): number => 1 + (GOING[r.terrain] ?? 0) + (r.fort && r.fort.vs.includes(nat) ? 2.5 * r.fort.lv : 0) + 0.2 * r.units.length;
  const open: number[] = [];
  const doors = f.edge.filter((id) => f.depth[id] === 1);
  // With the army in the field, the road starts where the army is: a thrust cannot be launched out of a corner
  // of the front held by a corps or two, however short the way from there.
  const push = (id: number): number => {
    let sum = 0;
    for (const j of g.regions[id].nb) if (allied(g, nat, g.regions[j].ctrl)) for (const u of g.regions[j].units) if (u.nat === nat && u.kind !== 'fort' && !u.dead) sum += atkP(g, u);
    return sum;
  };
  const force = placed ? doors.map(push) : [];
  const floor = OPEN.foothold * Math.max(0, ...force);
  doors.forEach((id, k) => { if (!placed || force[k] >= floor) { cost[id] = step(g.regions[id]); open.push(id); } });
  // Few regions: a plain search, cheapest first.
  const done = new Uint8Array(N);
  for (;;) {
    let at = -1;
    for (const id of open) if (!done[id] && (at < 0 || cost[id] < cost[at])) at = id;
    if (at < 0) break;
    done[at] = 1;
    for (const j of g.regions[at].nb) {
      if (f.depth[j] < 1 || done[j]) continue;
      const c = cost[at] + step(g.regions[j]);
      if (c < cost[j]) { if (cost[j] === 1e9) open.push(j); cost[j] = c; from[j] = at; }
    }
  }
  let goal: Region | null = null;
  for (const r of g.regions) {
    if (cost[r.id] >= 1e9 || !r.cap || !g.nations[r.owner]?.alive || !f.enemies.includes(r.owner)) continue;
    if (!goal || cost[r.id] < cost[goal.id]) goal = r;
  }
  if (!goal) for (const r of g.regions) if (cost[r.id] < 1e9 && f.depth[r.id] <= 5 && r.vp >= 2 && (!goal || r.vp > goal.vp || (r.vp === goal.vp && cost[r.id] < cost[goal.id]))) goal = r;
  if (!goal) return [];
  const path: string[] = [];
  for (let id = goal.id; id >= 0; id = from[id]) path.unshift(g.regions[id].name);
  return path;
}

/**
 * The month's bound: how much of the enemy's front the staff lets the offensive take on before the line is put in
 * order and supply brought up. No army attacks everywhere at once. The main thrust goes in along its road (five
 * regions deep in the opening month of a nation that makes war by speed, two after that) with the ground on either
 * side of it; on the rest of the front only the prizes and the weak places are attacked. A front with no thrust,
 * a side-show or a theatre overseas, is taken on whole, to the depth given.
 */
function bound(g: Game, nat: string, f: Front, way: string[], opening: boolean, reach = OPEN.bound): string[] {
  const out = new Set<string>();
  const thrust = way.slice(0, opening && g.nations[nat].blitz ? OPEN.thrust[0] : OPEN.thrust[1]).map((nm) => g.byName[nm]).filter(Boolean);
  // An army with overwhelming strength does not wait on one road: it presses the whole of the enemy's front.
  if (thrust.length && f.ratio >= OPEN.general) for (const id of f.edge) out.add(g.regions[id].name);
  if (!thrust.length) {
    for (const r of g.regions) if (f.depth[r.id] >= 1 && f.depth[r.id] <= reach) out.add(r.name);
    for (const id of f.edge) out.add(g.regions[id].name);
    return [...out];
  }
  // The thrust and its shoulders: enemy ground beside the road, and the enemy's front within two regions of it.
  const near = new Int8Array(g.regions.length).fill(-1);
  const q = thrust.map((r) => r.id);
  for (const id of q) near[id] = 0;
  for (let i = 0; i < q.length; i++) if (near[q[i]] < 2) for (const j of g.regions[q[i]].nb) if (near[j] < 0) { near[j] = near[q[i]] + 1; q.push(j); }
  for (const r of g.regions) {
    if (f.depth[r.id] < 1) continue;
    if (near[r.id] === 0 || near[r.id] === 1 || (near[r.id] === 2 && f.depth[r.id] === 1)) out.add(r.name);
  }
  // Elsewhere on the front: what is worth a battle, and what will fall to a push.
  for (const id of f.edge) {
    const r = g.regions[id];
    const held = r.units.reduce((s, u) => s + (atWar(g, nat, u.nat) ? u.divs * u.str : 0), 0);
    // A bulge in the line, with more of this army's ground around it than the enemy's, is a base for a stroke into
    // the flank of the advance. It is cleared: nothing is left standing behind the front.
    const round = r.nb.map((j) => g.regions[j]);
    const bulge = round.filter((x) => allied(g, nat, x.ctrl)).length > round.filter((x) => atWar(g, nat, x.ctrl)).length;
    if (r.cap || r.vp >= 2 || held <= OPEN.weak || bulge) out.add(r.name);
  }
  return [...out];
}

/** True if ground held by someone this nation is at war with lies within so many regions of a place. */
function nearEnemy(g: Game, nat: string, at: Region, within: number): boolean {
  const seen = new Set([at.id]);
  let ring = [at.id];
  for (let d = 0; d < within; d++) {
    const next: number[] = [];
    for (const id of ring) for (const j of g.regions[id].nb) if (!seen.has(j)) { seen.add(j); next.push(j); if (atWar(g, nat, g.regions[j].ctrl)) return true; }
    ring = next;
  }
  return false;
}

/** The friendly ground an offensive on this front starts from: garrisons there join in. Those facing strong works stay in their own. */
function jumpOff(g: Game, nat: string, f: Front): string[] {
  const cap = g.nations[nat].capital;
  return f.ours.filter((id) => id !== cap && g.regions[id].nb.some((j) => f.depth[j] === 1)).map((id) => g.regions[id].name);
}

// ---- What the staff remembers from month to month, kept with the game -----------------------------------

const flag = (g: Game, k: string): number => g.flags[k] ?? 0;
const onKey = (nat: string, e: string, where: string): string => `st:on:${nat}:${e}${where}`;

/** The month this nation's offensive against the enemies on a front began, or 0 if there is none. */
function onSince(g: Game, nat: string, f: Front, where = ''): number {
  let t = 0;
  for (const e of f.enemies) { const v = flag(g, onKey(nat, e, where)); if (v && (!t || v < t)) t = v; }
  return t;
}

function setOn(g: Game, nat: string, f: Front, turn: number, where = ''): void {
  for (const e of f.enemies) { if (turn) g.flags[onKey(nat, e, where)] = turn; else delete g.flags[onKey(nat, e, where)]; }
}

/**
 * On the hardest level the player's rivals time their blows together: while an ally of this nation has an offensive
 * under way against the player, it opens its own against him at a smaller ratio than it would alone. 1 otherwise.
 */
export function inConcert(g: Game, nat: string, f: Pick<Front, 'enemies'>): number {
  const k = concertFor(g, nat);
  const you = seat(g);
  if (k === 1 || you === null || !f.enemies.includes(you)) return 1;
  for (const a of Object.keys(g.nations)) if (a !== nat && g.nations[a].alive && allied(g, a, nat) && flag(g, onKey(a, you, ''))) return k;
  return 1;
}

/** One of the staff's own conditions, or one that reads as in diplomacy.json. */
function cond(g: Game, nat: string, c: string, real: Front[]): boolean {
  const [key, rest = ''] = c.split(/:(.*)/s);
  const a = rest.split(',');
  if (key === 'quiet') {
    // No serious land front: nothing the enemy could do there that the troops standing on it could not answer.
    const total = g.corps.reduce((s, x) => s + (x.nat === nat && !x.dead && x.kind !== 'fort' ? atkP(g, x) : 0), 0);
    return real.every((f) => f.def < OPEN.major * total);
  }
  if (key === 'collapsing') { const n = g.nations[a[0]]; return !!n && (n.alive ? collapseScore(g, a[0]) >= Number(a[1]) : !!n.fellOn); }
  if (key === 'month') return a.map(Number).includes(turnToDate(g.turn).month + 1);
  return holds(g, c);
}

/** Whom an operation would still add to this nation's enemies: the living, not on its own side, not yet at war with it. */
const targets = (g: Game, nat: string, op: Op): string[] => (op.declare ?? []).filter((e) => g.nations[e]?.alive && !g.nations[e].dormant && !allied(g, nat, e) && !atWar(g, nat, e));

/** The front a new war would open, weighed before it is declared. */
export function prospect(g: Game, nat: string, op: Op, skip: FrontOpts['skip']): Front | null {
  const extra = new Set(targets(g, nat, op));
  if (!extra.size) return null;
  return fronts(g, nat, { extra, skip, busy: (f) => onSince(g, nat, f) > 0 }).find((f) => f.enemies.some((e) => extra.has(e))) ?? null;
}

const inSeason = (g: Game, op: Op, turn = g.turn): boolean => !op.season || op.season.includes(turnToDate(turn).month + 1);

export interface Decision {
  /** 0 stand on the defensive, 1 attack, 2 bring the army up for a war not yet declared. */
  posture: 0 | 1 | 2;
  /** Wars declared this month, and the operation they belong to. */
  declare: string[];
  op: Op | null;
}

/** The great choices open to a nation's staff. */
export const ops = (nat: string): Op[] => OPEN.ops[nat] ?? [];

/**
 * The month's decisions for one computer-run nation, taken once before its army moves: which offensives go on,
 * which stop, whether a new one opens, and whether one of history's great choices is taken up or carried out.
 */
export function decide(g: Game, nat: string, skip?: FrontOpts['skip']): void {
  const n = g.nations[nat];
  if (!n.alive || n.dormant) return;
  memo.get(g)?.delete(nat);
  const T = temper(g, nat);
  const real = fronts(g, nat, { skip, busy: (f) => onSince(g, nat, f) > 0 });
  // Ground gained since last month keeps an offensive alive; three months without any and it is called off.
  const held = g.regions.reduce((s, r) => s + (r.ctrl === nat ? 1 : 0), 0);
  if (held > flag(g, `st:n:${nat}`)) g.flags[`st:gain:${nat}`] = g.turn;
  g.flags[`st:n:${nat}`] = held;
  const stalled = g.turn - flag(g, `st:gain:${nat}`) >= OPEN.stall;
  const pushed = g.turn <= flag(g, `st:push:${nat}`);

  for (const f of real) {
    const since = onSince(g, nat, f);
    if (since) {
      const fresh = g.turn - since < 2 || (pushed && f.enemies.some((e) => flag(g, `st:pushed:${nat}:${e}`)));
      if (fresh || (f.ratio >= T.persist && (!stalled || f.ratio >= T.attackAt))) setOn(g, nat, f, since);
      else { setOn(g, nat, f, 0); for (const e of f.enemies) g.flags[`st:rest:${nat}:${e}`] = g.turn + OPEN.rest; }
    } else if (f.ratio >= T.attackAt * inConcert(g, nat, f) && f.enemies.every((e) => g.turn >= flag(g, `st:rest:${nat}:${e}`))) {
      setOn(g, nat, f, g.turn);
      g.flags[`st:gain:${nat}`] = g.turn;
    }
  }

  // History's great choices. One is prepared at a time, and none while a great offensive is still being fought.
  const list = ops(nat);
  const busy = real.some((f) => onSince(g, nat, f) && !cond(g, nat, 'quiet', [f]) && g.turn - flag(g, `st:gain:${nat}`) <= 1);
  let cur = flag(g, `st:op:${nat}`);
  const go = (k: number): void => {
    const op = list[k];
    const who = targets(g, nat, op);
    g.flags[`st:done:${nat}:${op.id}`] = g.turn;
    g.flags[`st:go:${nat}`] = g.turn;
    g.flags[`st:goOp:${nat}`] = k + 1;
    delete g.flags[`st:op:${nat}`];
    if (who.length) {
      g.flags[`st:push:${nat}`] = g.turn + (op.push ?? 2) - 1;
      g.flags[`st:gain:${nat}`] = g.turn;
      for (const e of who) { g.flags[onKey(nat, e, '')] = g.turn; g.flags[`st:pushed:${nat}:${e}`] = 1; }
    }
  };
  if (cur) {
    const op = list[cur - 1];
    const p = op ? prospect(g, nat, op, skip) : null;
    const still = !!op && (op.if ?? []).filter((c) => c !== 'quiet').every((c) => cond(g, nat, c, real));
    if (!op || !p || !still || (op.ratio !== undefined && p.ratio < 0.8 * op.ratio)) {
      // The moment has passed: stand the army down and think again in half a year.
      delete g.flags[`st:op:${nat}`];
      if (op) g.flags[`st:cool:${nat}:${op.id}`] = g.turn + 6;
      cur = 0;
    } else if (g.turn - flag(g, `st:opAt:${nat}`) >= (op.prep ?? 0) && inSeason(g, op) && (op.ratio === undefined || p.ratio >= op.ratio)) {
      go(cur - 1);
      cur = 0;
    }
  }
  if (!cur) {
    for (let k = 0; k < list.length; k++) {
      const op = list[k];
      if (flag(g, `st:done:${nat}:${op.id}`) || g.turn < (op.from ?? 1) || g.turn > (op.to ?? 999) || g.turn < flag(g, `st:cool:${nat}:${op.id}`)) continue;
      if (!op.side && busy) continue;
      if (!(op.if ?? []).every((c) => cond(g, nat, c, real))) continue;
      const who = targets(g, nat, op);
      if ((op.declare ?? []).length && !who.length) { g.flags[`st:done:${nat}:${op.id}`] = -1; continue; }
      if (op.ratio !== undefined) { const p = prospect(g, nat, op, skip); if (!p || p.ratio < 0.9 * op.ratio) continue; }
      // The blow has to be able to fall in season once the army is up.
      if (!op.side && op.season && ![0, 1, 2].some((late) => inSeason(g, op, g.turn + (op.prep ?? 0) + late))) continue;
      // A staff thinks twice before a blow that would bring the whole League down on it.
      if (draw(g, nat, op.id, g.turn) >= op.chance * deterred(g, nat, who)) continue;
      if (op.side || !(op.prep ?? 0)) {
        if (inSeason(g, op)) go(k);
      } else {
        g.flags[`st:op:${nat}`] = k + 1;
        g.flags[`st:opAt:${nat}`] = g.turn;
      }
      break;
    }
  }
}

// ---- The plan phase the armies are given ------------------------------------------------------------------

const memo = new WeakMap<Game, Map<string, { turn: number; phase: PlanPhase | null }>>();

/** This month's orders for a computer-run nation's main army, as its staff has decided them. */
export function staffPhase(g: Game, nat: string, skip?: FrontOpts['skip']): PlanPhase | null {
  if (nat === g.human) return null;
  let m = memo.get(g);
  if (!m) memo.set(g, (m = new Map()));
  const hit = m.get(nat);
  if (hit && hit.turn === g.turn) return hit.phase;
  const phase = build(g, nat, skip);
  m.set(nat, { turn: g.turn, phase });
  return phase;
}

function build(g: Game, nat: string, skip?: FrontOpts['skip']): PlanPhase | null {
  const n = g.nations[nat];
  if (!n.alive || n.dormant) return null;
  const T = temper(g, nat);
  const list = ops(nat);
  const prep = list[flag(g, `st:op:${nat}`) - 1] ?? null;
  // The operation launched this month or last: its landings may take two months to get ashore.
  const went = g.turn - flag(g, `st:go:${nat}`) <= 1 && flag(g, `st:go:${nat}`) ? list[flag(g, `st:goOp:${nat}`) - 1] ?? null : null;
  const declare = went && flag(g, `st:go:${nat}`) === g.turn ? targets(g, nat, went) : [];
  // The fronts as they will stand once this month's declarations are made.
  const real = fronts(g, nat, { extra: new Set(declare), skip });
  const live = real.filter((f) => onSince(g, nat, f) > 0 || f.enemies.some((e) => declare.includes(e)));
  const main = live[0] ?? null;
  const phase: PlanPhase = { name: '', from: g.turn, to: g.turn, posture: 'defend' } as PlanPhase;
  if (declare.length) phase.declare = declare;
  if (went?.landings) { phase.landings = went.landings; phase.to = flag(g, `st:go:${nat}`) + 6; }
  if (live.length) {
    phase.enemies = [...new Set(live.flatMap((f) => f.enemies))];
    phase.objectives = [...new Set(live.flatMap((f) => objectives(g, f)))];
    phase.minOdds = T.minOdds;
    phase.release = [...new Set(live.flatMap((f) => jumpOff(g, nat, f)))];
    phase.targets = [...new Set(live.flatMap((f) => bound(g, nat, f, [], false)))];
  }
  if (prep) {
    const p = prospect(g, nat, prep, skip);
    phase.posture = 'redeploy';
    phase.name = `Building up: ${prep.name}`;
    phase.from = flag(g, `st:opAt:${nat}`);
    if (p) {
      const first = p.ours.filter((id) => allied(g, nat, g.regions[id].ctrl));
      const way = axis(g, nat, p);
      const door = way.length ? g.byName[way[0]] : null;
      const second = new Set<number>();
      for (const id of first) for (const j of g.regions[id].nb) if (g.regions[j].ctrl === nat && !first.includes(j) && j !== n.capital) second.add(j);
      phase.assembly = [...first, ...second].map((id) => g.regions[id].name);
      // The fast troops gather opposite the way in.
      const at = door ? first.filter((id) => door.nb.includes(id)) : [];
      const beside = first.filter((id) => !at.includes(id) && at.some((a) => g.regions[a].nb.includes(id)));
      phase.armour = [...at, ...beside].slice(0, 3).map((id) => g.regions[id].name);
      // Occupation troops stay in the great cities of beaten countries, away from where the army is gathering.
      const gather = new Set([...first, ...second]);
      phase.garrison = g.regions
        .filter((r) => r.ctrl === nat && r.owner !== nat && !g.nations[r.owner]?.alive && (r.cap || r.vp >= 2) && !gather.has(r.id))
        .sort((a, b) => b.vp - a.vp || a.id - b.id).slice(0, 6).map((r) => r.name);
    }
  } else if (main) {
    phase.posture = 'attack';
    phase.name = went && declare.length ? went.name : `The offensive against ${g.nations[main.key]?.name ?? main.key}`;
    phase.from = onSince(g, nat, main) || g.turn;
    const way = axis(g, nat, main, true);
    const opening = phase.from === g.turn;
    phase.axis = way.slice(0, opening && n.blitz ? OPEN.thrust[0] : OPEN.thrust[1]);
    phase.targets = [...new Set(live.flatMap((f) => bound(g, nat, f, f === main ? way : [], opening)))];
    // No army takes a capital deep in enemy country in its first bound: the opening month goes to breaking the front.
    if (opening) phase.haltOrder = g.regions.filter((r) => r.cap && main.depth[r.id] >= 3 && main.enemies.includes(r.owner)).map((r) => r.name);
  } else if (went) phase.name = went.name;
  // With the enemy a few marches from the capital, the staff keeps a force in it whatever else is going on.
  const cap = n.capital >= 0 ? g.regions[n.capital] : null;
  if (cap && cap.ctrl === nat && nearEnemy(g, nat, cap, OPEN.guardWithin)) phase.staging = { ...(phase.staging ?? {}), [cap.name]: OPEN.guard };
  // On the hardest level a rival with shipping keeps a force by the quay for a descent on the player's coast.
  const quay = seaBase(g, nat);
  if (quay) phase.staging = { ...(phase.staging ?? {}), [quay.name]: Math.max(phase.staging?.[quay.name] ?? 0, landingsFor(g, nat)!.force) };
  // A force kept ready for a stroke still to come.
  for (const op of list) {
    if (!op.staging || flag(g, `st:done:${nat}:${op.id}`) || g.turn > (op.to ?? 999)) continue;
    phase.staging = { ...(phase.staging ?? {}), ...op.staging };
  }
  return phase;
}

/**
 * The harbour a rival's staff keeps its landing force at, on the level that has it land on the player's coasts:
 * a home port of its own, in touch with the capital by land, and the one with the shortest crossing to a harbour
 * the player holds. The choice is kept while it still serves and looked at again every half year.
 */
export function seaBase(g: Game, nat: string): Region | null {
  const L = landingsFor(g, nat);
  const you = seat(g);
  const n = g.nations[nat];
  if (!L || you === null || !n.alive || n.sealift <= 0 || !atWar(g, nat, you) || n.capital < 0) return null;
  const key = `st:quay:${nat}`, at = `st:quayAt:${nat}`;
  const kept = g.regions[flag(g, key) - 1];
  if (kept && kept.ctrl === nat && g.turn - flag(g, at) < 6) return kept;
  // Home ground in touch with the capital.
  const seen = new Uint8Array(g.regions.length);
  const q = [n.capital];
  seen[n.capital] = 1;
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (!seen[j] && g.regions[j].ctrl === nat) { seen[j] = 1; q.push(j); }
  const shores = g.regions.filter((r) => r.ctrl === you && r.port && r.seas.length > 0);
  let best: Region | null = null, bd = Infinity;
  for (const id of q) {
    const r = g.regions[id];
    if (!r.port || r.owner !== nat || r.overseas) continue;
    // Seas between this quay and the nearest of the player's harbours, by the shortest way round.
    const d = seaHops(g, r.seas, shores);
    if (d < bd) { bd = d; best = r; }
  }
  if (!best || bd > NAVY.landing.range) { delete g.flags[key]; return null; }
  g.flags[key] = best.id + 1;
  g.flags[at] = g.turn;
  return best;
}

/** How many seas lie between a coast and the nearest of some shores: 1 when they share a sea. */
function seaHops(g: Game, from: number[], shores: Region[]): number {
  const want = new Uint8Array(g.seas.length);
  for (const r of shores) for (const z of r.seas) want[z] = 1;
  const d = new Int16Array(g.seas.length).fill(-1);
  const q = [...from];
  for (const z of from) d[z] = 1;
  for (let i = 0; i < q.length; i++) {
    if (want[q[i]]) return d[q[i]];
    for (const j of g.seas[q[i]].adj) if (d[j] < 0) { d[j] = d[q[i]] + 1; q.push(j); }
  }
  return Infinity;
}

/** What follows a declaration the staff has just made: a neutral that marches of its own accord takes its side. */
export function afterDeclaring(g: Game, nat: string): void {
  const list = ops(nat);
  const op = flag(g, `st:go:${nat}`) === g.turn ? list[flag(g, `st:goOp:${nat}`) - 1] : null;
  const n = g.nations[nat];
  // In the last act there are no sides left to take.
  if (dominating(g)) return;
  if (!op?.join || !n.alive || n.align === 'allies' || n.align === 'axis') return;
  const others = n.atWar.some((e) => { const b = g.nations[e].align; return op.join === 'allies' ? b === 'axis' || b === 'axis_cobelligerent' : b === 'allies' || b === 'allies_cobelligerent'; });
  if (!others) return;
  n.align = op.join === 'allies' ? 'allies_cobelligerent' : 'axis_cobelligerent';
  n.opinion = n.home = op.join === 'allies' ? -100 : 100;
  g.supplyDirty = true;
}

/** This month's orders for a computer-run nation's army in a theatre overseas that keeps a staff of its own. */
export function theatreStaff(g: Game, nat: string, scope: Set<number>, k: number): PlanPhase | null {
  const n = g.nations[nat];
  if (!n.alive || !n.atWar.length) return null;
  const T = temper(g, nat);
  const where = `:t${k}`;
  const real = fronts(g, nat, { scope, busy: (f) => onSince(g, nat, f, where) > 0 });
  const stalled = g.turn - flag(g, `st:gain:${nat}${where}`) >= OPEN.stall;
  const held = [...scope].reduce((s, id) => s + (g.regions[id].ctrl === nat ? 1 : 0), 0);
  if (flag(g, `st:nAt:${nat}${where}`) !== g.turn) {
    if (held > flag(g, `st:n:${nat}${where}`)) g.flags[`st:gain:${nat}${where}`] = g.turn;
    g.flags[`st:n:${nat}${where}`] = held;
    g.flags[`st:nAt:${nat}${where}`] = g.turn;
    for (const f of real) {
      const since = onSince(g, nat, f, where);
      if (since) {
        if (g.turn - since < 2 || (f.ratio >= T.persist && (!stalled || f.ratio >= T.attackAt))) setOn(g, nat, f, since, where);
        else { setOn(g, nat, f, 0, where); for (const e of f.enemies) g.flags[`st:rest:${nat}:${e}${where}`] = g.turn + OPEN.rest; }
      } else if (f.ratio >= T.attackAt && f.enemies.every((e) => g.turn >= flag(g, `st:rest:${nat}:${e}${where}`))) {
        setOn(g, nat, f, g.turn, where);
        g.flags[`st:gain:${nat}${where}`] = g.turn;
      }
    }
  }
  const live = real.filter((f) => onSince(g, nat, f, where) > 0);
  if (!live.length) return { name: '', from: g.turn, to: g.turn, posture: 'defend' } as PlanPhase;
  return {
    name: `The offensive against ${g.nations[live[0].key]?.name ?? live[0].key}`, from: onSince(g, nat, live[0], where), to: g.turn, posture: 'attack',
    enemies: [...new Set(live.flatMap((f) => f.enemies))], objectives: [...new Set(live.flatMap((f) => objectives(g, f)))], minOdds: T.minOdds,
    targets: [...new Set(live.flatMap((f) => bound(g, nat, f, [], false, OPEN.boundOverseas)))],
    release: [...new Set(live.flatMap((f) => f.ours.map((id) => g.regions[id].name)))],
  } as PlanPhase;
}
