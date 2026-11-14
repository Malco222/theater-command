// Playtest harness: plays grand campaigns through the same calls the interface makes (turn.ts), with a scripted
// "player" in the seat. Nothing here changes the game: it only reads state and gives orders a person could give.
import { createGame, atWar, allied, hostileUnits, armySize, collapseScore, bloc, limitAttackers } from '@tc/campaign/game';
import { startOpen } from '@tc/campaign/staff';
import * as T from '@tc/campaign/turn';
import { setLevel } from '@tc/campaign/difficulty';
import { openGoals, judgeGoals, goalState } from '@tc/campaign/goals';
import { takeOver } from '@tc/campaign/stories';
import { ledger } from '@tc/campaign/economy';
import { saveGame, loadGame } from '@tc/campaign/save';
import { railCut } from '@tc/campaign/rails';
import { townHeld, besieged, roomIn, garrisonOf, siegeRules } from '@tc/campaign/siege';
import { turnToDate } from '@tc/core/ticket';
import type { Game, Corps, Region } from '@tc/campaign/types';

export { T, atWar, allied, hostileUnits, armySize, collapseScore, bloc, ledger, saveGame, loadGame, turnToDate, goalState, judgeGoals };
export type { Game, Corps, Region };

export const date = (t: number): string => turnToDate(t).label;

export interface MonthRow {
  turn: number; date: string;
  aiMs: number;
  treasury: number; net: number; debt: number;
  myCorps: number; myDivs: number;
  myRegions: number;
  attacksOrdered: number; attacksWon: number; moves: number;
  hitByEnemy: number;      // battles where the player's ground was attacked this month
  gained: number; lost: number;
  attackOptions: number;   // enemy-held provinces next to my troops at the start of the month
  goodOptions: number;     // ... with odds of 1.3 or better
  headlines: number;
  wars: string;
  bought: string[];
  notes: string[];
}

export interface Run {
  g: Game; me: string; from: number; seed: number; level: string;
  rows: MonthRow[];
  problems: string[];
  fellOn: number | null;
  errors: string[];
}

/** Starts a grand campaign the way the start screen does. */
export function start(me: string | null, from: number, seed: number, level = 'colonel'): Game {
  const g = createGame(seed, from > 1 ? null : me);
  if (from === 1) startOpen(g);
  while (g.turn < from) T.runTurnAI(g);
  if (from > 1) startOpen(g);
  g.human = me;
  if (me) setLevel(g, level);
  if (from > 1) takeOver(g);
  if (me) openGoals(g, me, from);
  return g;
}

/** Lets the computer play until the player is up (as the interface does). Returns false if the player's nation is gone. */
export function advance(g: Game, onStep?: (nat: string) => void): boolean {
  let guard = 0;
  while (!T.humanUp(g) && guard++ < 80) {
    if (g.human && !g.nations[g.human].alive) break;
    const month = g.turn;
    const nat = T.stepAI(g);
    if (g.human && g.turn !== month) judgeGoals(g, g.human);
    onStep?.(nat);
  }
  if (g.human) judgeGoals(g, g.human);
  if (g.human && !g.nations[g.human].alive) return false;
  if (T.humanUp(g)) { T.beginHuman(g); return true; }
  return false;
}

// ---- Invariants: things that must be true of any game state ------------------------------------------------------

const BAD_TEXT = /undefined|NaN|\[object|null\b|Infinity/;

export function check(g: Game, where: string, out: string[], seen: Set<string>): void {
  const add = (key: string, text: string): void => { if (!seen.has(key)) { seen.add(key); out.push(`[${date(g.turn)} ${where}] ${text}`); } };
  const ids = new Set<number>();
  for (const c of g.corps) {
    if (c.dead) { add(`deadlist:${c.id}`, `dead corps still on the list: ${c.name}`); continue; }
    if (ids.has(c.id)) add(`dup:${c.id}`, `two corps share id ${c.id}`);
    ids.add(c.id);
    const r = g.regions[c.reg];
    if (!r) { add(`noreg:${c.id}`, `${c.name} stands nowhere (region ${c.reg})`); continue; }
    const inField = r.units.includes(c), inTown = (r.garr ?? []).includes(c);
    if (!inField && !inTown) add(`notin:${c.id}`, `${c.name} is not in the list of ${r.name}`);
    if (inField && inTown) add(`both:${c.id}`, `${c.name} is both in the field and in the town of ${r.name}`);
    if (inTown && c.town === undefined) add(`townflag:${c.id}`, `${c.name} is in the garrison of ${r.name} but is not marked as in a town`);
    if (inField && c.town !== undefined) add(`fieldflag:${c.id}`, `${c.name} is marked as in a town but stands in the field of ${r.name}`);
    for (const k of ['str', 'org', 'xp', 'ap', 'divs'] as const) if (!Number.isFinite(c[k])) add(`nan:${c.id}:${k}`, `${c.name} has ${k} = ${c[k]}`);
    if (c.str < -1e-6 || c.str > 1.0001) add(`str:${c.id}`, `${c.name} strength out of range: ${c.str}`);
    if (c.org < -1e-6 || c.org > 1.0001) add(`org:${c.id}`, `${c.name} organisation out of range: ${c.org}`);
    if (c.mor !== undefined && (!Number.isFinite(c.mor) || c.mor < -1e-6 || c.mor > 1.0001)) add(`mor:${c.id}`, `${c.name} morale out of range: ${c.mor}`);
    if (c.ap < 0) add(`ap:${c.id}`, `${c.name} has negative moves: ${c.ap}`);
    if (c.divs <= 0) add(`divs:${c.id}`, `${c.name} has ${c.divs} divisions`);
    if (!g.nations[c.nat]) add(`nonat:${c.id}`, `${c.name} belongs to nobody (${c.nat})`);
    else if (!g.nations[c.nat].alive && !g.nations[c.nat].dormant) add(`ghost:${c.nat}`, `${g.nations[c.nat].name} has fallen but ${c.name} is still on the map at ${r.name}`);
  }
  for (const r of g.regions) {
    if (!g.nations[r.ctrl]) add(`ctrl:${r.id}`, `${r.name} is held by nobody (${r.ctrl})`);
    const nats = [...new Set(r.units.map((u) => u.nat))];
    for (let i = 0; i < nats.length; i++) for (let j = i + 1; j < nats.length; j++) if (atWar(g, nats[i], nats[j])) add(`mixed:${r.id}:${nats[i]}:${nats[j]}`, `${r.name} holds troops of two nations at war with each other: ${nats[i]} and ${nats[j]} (${r.units.map((u) => `${u.nat} ${u.name} [${u.kind}]`).join(', ')})`);
    for (const u of r.units) {
      if (u.dead) add(`deadin:${r.id}:${u.id}`, `${r.name} lists a dead corps: ${u.name}`);
      if (u.reg !== r.id) add(`wrongreg:${u.id}`, `${u.name} is listed in ${r.name} but thinks it is in ${g.regions[u.reg]?.name}`);
    }
    // Crowding: the player may stand five corps in a province.
    for (const n of nats) { const k = r.units.filter((u) => u.nat === n && u.kind !== 'eng' && u.kind !== 'fort').length; if (k > 7) add(`crowd:${r.id}:${n}:${Math.floor(k / 4)}`, `[note] crowd: ${k} ${n} corps stand in ${r.name} (${Math.round(r.units.filter((u) => u.nat === n).reduce((s, u) => s + u.divs * u.str, 0))} divisions)`); }
    // Troops standing on ground held by an enemy: the ground should have changed hands.
    // The town's garrison: allies of the holder only, and no more than the town has room for.
    const garr = garrisonOf(r);
    if (garr.length) {
      if (garr.length > roomIn(g, r)) add(`room:${r.id}:${garr.length}`, `${r.name} has ${garr.length} corps in a town with room for ${roomIn(g, r)}`);
      for (const u of garr) { if (u.dead) add(`deadgarr:${u.id}`, `${r.name} lists a dead corps in its garrison: ${u.name}`); if (u.reg !== r.id) add(`garrreg:${u.id}`, `${u.name} is in the garrison of ${r.name} but thinks it is in ${g.regions[u.reg]?.name}`); if (!allied(g, u.nat, r.ctrl)) add(`garrfoe:${r.id}:${u.nat}:${r.ctrl}`, `${u.nat} ${u.name} is inside the town of ${r.name}, which ${r.ctrl} holds (at war: ${atWar(g, u.nat, r.ctrl)})`); }
    }
    if (r.units.length && r.units.every((u) => atWar(g, u.nat, r.ctrl)) && !(siegeRules(g) && townHeld(g, r))) add(`squat:${r.id}:${g.turn}`, `${r.name} is held by ${r.ctrl} but only its enemies stand there (${nats.join(',')})`);
    // Troops standing in a country they are neither allied to nor at war with.
    for (const n of nats) if (!allied(g, n, r.ctrl) && !atWar(g, n, r.ctrl)) add(`trespass:${r.id}:${n}:${r.ctrl}`, `${n} troops stand in ${r.name}, held by ${r.ctrl}, which they are neither allied to nor at war with (${r.units.filter((u) => u.nat === n).map((u) => `${u.name} [${u.kind}]`).join(', ')}; ${n} is at war with: ${g.nations[n].atWar.join(',') || 'nobody'})`);
  }
  for (const n of Object.values(g.nations)) {
    if (!Number.isFinite(n.pp)) add(`pp:${n.id}`, `${n.name} treasury is ${n.pp}`);
    if (n.res) for (const k of ['oil', 'sup', 'oilMet', 'supMet'] as const) if (!Number.isFinite(n.res[k]) || n.res[k] < -1e-6) add(`res:${n.id}:${k}`, `${n.name} ${k} = ${n.res[k]}`);
    if (!Number.isFinite(n.opinion)) add(`op:${n.id}`, `${n.name} opinion is ${n.opinion}`);
    for (const e of n.atWar) {
      if (!g.nations[e]) { add(`war?:${n.id}:${e}`, `${n.name} is at war with nobody (${e})`); continue; }
      if (!g.nations[e].atWar.includes(n.id)) add(`oneway:${n.id}:${e}`, `${n.name} is at war with ${g.nations[e].name}, but not the other way round`);
      if (allied(g, n.id, e)) add(`allywar:${[n.id, e].sort().join(':')}`, `${n.name} and ${g.nations[e].name} are allies and at war with each other`);
    }
    if (n.alive && n.capital >= 0 && g.regions[n.capital] && !allied(g, n.id, g.regions[n.capital].ctrl) && !n.offmap) add(`capgone:${n.id}`, '[note] ' + `${n.name} fights on with its capital ${g.regions[n.capital].name} in ${g.regions[n.capital].ctrl} hands`);
  }
  for (const f of g.fleets) {
    for (const k of ['bb', 'cv', 'ca', 'dd', 'ss'] as const) if (!Number.isFinite(f.ships[k]) || f.ships[k] < 0) add(`ship:${f.id}:${k}`, `${f.name} has ${f.ships[k]} ${k}`);
    if (!g.seas[f.zone]) add(`zone:${f.id}`, `${f.name} is in no sea (${f.zone})`);
    if (!Number.isFinite(f.ready)) add(`ready:${f.id}`, `${f.name} readiness is ${f.ready}`);
  }
  for (const a of g.airFleets as any[]) {
    for (const [k, v] of Object.entries(a)) if (typeof v === 'number' && !Number.isFinite(v)) add(`air:${a.id}:${k}`, `air fleet ${a.name} has ${k} = ${v}`);
  }
  for (let i = Math.max(0, g.headlines.length - 60); i < g.headlines.length; i++) {
    const h = g.headlines[i];
    if (BAD_TEXT.test(h.text)) add(`headline:${h.text}`, `headline reads badly: "${h.text}"`);
  }
  for (let i = Math.max(0, g.reports.length - 40); i < g.reports.length; i++) {
    const r = g.reports[i];
    for (const l of r.lines) if (BAD_TEXT.test(l.value) || (l.effect && BAD_TEXT.test(l.effect))) add(`ticket:${l.label}:${l.value}`, `battle ticket line reads badly: ${l.label}: "${l.value}" / "${l.effect ?? ''}"`);
    if (!Number.isFinite(r.odds)) add(`odds:${r.id}`, `battle at ${r.regionName} has odds ${r.odds}`);
  }
}

// ---- The scripted player ------------------------------------------------------------------------------------------

export interface Plan {
  /** Do nothing but end the month. */
  idle?: boolean;
  /** Lowest odds at which the player attacks. */
  minOdds?: number;
  /** Version 57: go for the town the directive names: mass on it, and attack and storm it at shorter odds (default off). */
  chase?: boolean;
  /** Version 60: promise the cabinet a town whenever a promise can be made, of this boldness ('safe', 'fair', 'bold' or 'daring'; 'any' takes the boldest on offer), with the longest time on offer; the player then goes for that town as for a directive's (default: no promises). */
  vow?: string;
  /** Version 60: the answer when the cabinet asks for a general's dismissal: 'sack' or 'stand' (default: left unanswered). */
  head?: string;
  /** Version 59: switch the war cabinet off as the campaign opens (default: left on). */
  noCabinet?: boolean;
  /** Version 59: raise no new corps while the reserve of men stands under this share of what it opened with (default: never holds back). */
  menKeep?: number;
  /** Version 62: call the next class up early. 'half' or 'quarter': whenever it may be done with the reserve under that share of what it opened with; 'war': whenever it may be done; a number: once, in that month (default: never). */
  earlyClass?: string | number;
  /** Version 62: hold the standing call-up (a Soviet player's). [from, to]: held from month `from` to month `to`; 'first:6': held for the first six months it is on offer; 'short:0.85': held while the corps in the field are filled under that share, and let run again once they are a tenth fuller (default: never held). */
  holdCallUp?: [number, number] | string;
  /** Version 62: "Your office is secure": the player's government cannot fall (default: it can). */
  secure?: boolean;
  /** Version 62: carry out the cabinet's small jobs: order the cheapest building when it asks for a town built up, recruit an agent when it asks for one (default: left undone). */
  jobs?: boolean;
  chaseOdds?: number;
  /** Wars the player starts: [month number, nation]. */
  declare?: [number, string][];
  /** Never march out of the home country (a defender). */
  homeOnly?: boolean;
  /** What to buy first each month (arsenal offer keys), most wanted first. */
  buy?: string[];
  /** Try landings from the sea when there is no land front. */
  land?: boolean;
  /** Small country path to take in the first month: 'axis' | 'allies' | 'alone' | 'league'. */
  path?: string;
  /** The share of a landing force the player will risk at sea (default a quarter). */
  landRisk?: number;
  /** Technologies to research first. */
  research?: string[];
  /** Do not mass reserves on one point. */
  noMass?: boolean;
  /** Extra orders each month. */
  each?: (g: Game, run: Run) => void;
  /** Called at the start of the player's month, before any order is given (reads only). */
  first?: (g: Game, run: Run) => void;
  /** Version 58: told of every field attack the player orders, with the attack card's outlook as read just before (default off). */
  onAttack?: (g: Game, out: any, rep: any, target: any) => void;
  /** Version 58: shown every attack on offer before the player chooses (the province, the corps that would go, the card's outlook), each pass of the month (default off). */
  onLook?: (g: Game, options: { r: any; cands: any[]; odds: number; out: any }[], pass: number) => void;
  /** Version 58: attack only where the attack card gives this chance or better (default off: the odds alone decide). */
  minChance?: number;
  /** Version 58: keep this many artillery corps for every ten field corps, ordered ahead of everything else (default off: no guns are bought). */
  guns?: number;
  /** Version 58: bring up to two artillery corps into an attack in place of the weakest corps from the same province, where the card then gives a better chance (default off: the staff's own picker, which takes the strongest and leaves the guns behind). */
  useGuns?: boolean;
  /** Version 61: lean on a dug-in line. Attacks on troops with at least half a month of works are ordered from these odds up (under minOdds), since every blow, won or lost, takes time off the works. */
  grind?: number;
  /** Version 61: a player who goes by the card. An attack the card gives this chance or better is ordered whatever its odds (as well as those at minOdds or better). */
  byChance?: number;
  /** Version 61: the two-wave player. A dug-in line (half a month of works or more) is attacked from these odds up (under minOdds) only where a second wave stands by (at least three corps in reach beyond those the roads carry, or half as many again); a line already struck this month is struck again from these odds whatever is left. */
  waves?: number;
  /** Version 63: a player who digs the line in. A corps that has works and stands beside the enemy stays in them: it joins an attack only where the card reads these odds or better with everyone in, it is not railed away to mass elsewhere, and it does not march. Corps without works (reserves, troops that have just moved) attack at minOdds as before (default off: every corps attacks, masses and marches whatever works it leaves behind). */
  dig?: number;
}

function distanceToEnemy(g: Game, me: string, homeOnly: boolean): Int16Array {
  const d = new Int16Array(g.regions.length).fill(-1);
  const q: number[] = [];
  for (const r of g.regions) {
    const hostile = atWar(g, me, r.ctrl) || hostileUnits(g, r, me).length > 0;
    if (hostile && (!homeOnly || r.owner === me)) { d[r.id] = 0; q.push(r.id); }
  }
  // A defender who has lost nothing stands on the frontier: ground next to an enemy counts as the front.
  if (homeOnly && !q.length) for (const r of g.regions) if (atWar(g, me, r.ctrl)) { d[r.id] = 0; q.push(r.id); }
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) {
    if (d[j] >= 0) continue;
    const r = g.regions[j];
    if (!allied(g, me, r.ctrl)) continue;
    d[j] = d[q[i]] + 1; q.push(j);
  }
  return d;
}

/** The province the standing directive wants taken, for a player who chases directives; -1 if none. */
function chaseOf(g: Game, plan: Plan): number {
  if (!plan.chase && !plan.vow) return -1;
  const v = (T as any).governmentOf?.(g), d = v?.directive;
  if (plan.chase && d && (d.kind === 'take' || d.kind === 'relief') && typeof d.region === 'number') return d.region;
  return plan.vow && v?.promise ? v.promise.region : -1;
}

/** Version 60: the general the cabinet wants dismissed, in one line: who, why, his record, how many others have two defeats running, and his last three battles with the card's odds. */
export function headDetail(g: Game, me: string, hd: any): string {
  const gen = g.generals.find((x: any) => x.id === hd.general) as any, corps = g.corps.find((c: any) => c.id === gen?.corps) as any;
  const mine = (us: any[]): boolean => !!corps && us.some((u: any) => u.name === corps.name && u.nat === me);
  const fights = [...g.reports].reverse().filter((r: any) => mine(r.attackers) || mine(r.defenders)).slice(0, 3)
    .map((r: any) => `${mine(r.attackers) ? 'attacked' : 'defended'} ${String(r.regionName)} at ${(r.dPower > 0 ? r.aPower / r.dPower : 99).toFixed(2)} to 1 (${r.attackers.length} corps on ${r.defenders.length})${r.taken || r.siege || r.gates ? ', attacker won' : ', attacker thrown back'}`);
  const others = g.generals.filter((x: any) => x.nat === me && x.status === 'active' && x.corps >= 0 && x.rec.streak >= 2 && x.id !== hd.general).length;
  return `${hd.rank} ${hd.name} (${hd.command}); ${hd.why} record ${gen?.rec.won} won of ${gen?.rec.battles}, level ${gen?.level}${hd.loved ? ', popular' : ''}; ${hd.left} months to answer; sack +${hd.gain}, refuse -${hd.cost}; other generals with two defeats running: ${others}; his last battles: ${fights.join(' | ') || 'none found'}`;
}

/** Version 62: the player's say over the call-up before the month's moves: a class called early, the standing call-up held or let run. What he did is kept on the run (run.men). */
function menMoves(run: Run, plan: Plan): void {
  if (plan.earlyClass === undefined && plan.holdCallUp === undefined) return;
  const g = run.g, me = run.me, X = T as any;
  const log = ((run as any).men ??= { early: [] as number[], heldMonths: 0, holds: [] as string[] });
  if (plan.earlyClass !== undefined && X.whyNotOrderEarlyClass?.(g) === null) {
    const share = X.manpowerOf?.(g, me)?.share ?? 1;
    const e = plan.earlyClass;
    const go = typeof e === 'number' ? g.turn === e : e === 'war' ? true : e === 'half' ? share < 0.5 : e === 'quarter' ? share < 0.25 : false;
    if (go && X.orderEarlyClass(g)) log.early.push(g.turn);
  }
  if (plan.holdCallUp !== undefined) {
    const v = X.callUpOf?.(g)?.callUp;
    if (v) {
      let want: boolean;
      if (Array.isArray(plan.holdCallUp)) want = g.turn >= plan.holdCallUp[0] && g.turn <= plan.holdCallUp[1];
      else if (String(plan.holdCallUp).startsWith('first:')) { log.seen = (log.seen ?? 0) + 1; want = log.seen <= Number(String(plan.holdCallUp).split(':')[1]); }
      else {
        const bar = Number(String(plan.holdCallUp).split(':')[1] ?? 0.85);
        const cs = g.corps.filter((c) => c.nat === me && !c.dead && c.kind !== 'fort');
        const d = cs.reduce((t, c) => t + c.divs, 0), f = cs.reduce((t, c) => t + c.divs * c.str, 0);
        const filled = d ? f / d : 1;
        want = v.held ? filled < Math.min(0.98, bar + 0.1) : filled < bar;
      }
      if (want !== v.held && X.orderCallUp(g, want)) log.holds.push(`${want ? 'held' : 'let run'} ${turnToDate(g.turn).label}`);
      if (X.callUpOf(g)?.callUp?.held) log.heldMonths++;
    }
  }
}

/** Version 60: the player's own dealings with the cabinet before the month's moves: his answer on a general, and a promise. What he did is kept on the run (run.cab). */
function cabinetMoves(run: Run, plan: Plan): void {
  if (!plan.vow && !plan.head && !plan.jobs) return;
  const g = run.g, X = T as any, v = X.governmentOf?.(g);
  if (!v || !v.mine) return;
  if (plan.jobs && v.directive) {
    const jl = ((run as any).jobs ??= { build: 0, agent: 0, could: 0 });
    const d = v.directive;
    if (d.kind === 'agent' && X.whyNotRecruitAgent?.(g) === null && X.orderRecruitAgent(g)) jl.agent++;
    if (d.kind === 'build') {
      let best: { region: number; line: string; cost: number } | null = null;
      for (const r of g.regions) {
        if (r.ctrl !== run.me) continue;
        for (const o of (X.townBuildings?.(g, r.id)?.offers ?? []) as any[]) if (o.why === null && (!best || o.cost < best.cost)) best = { region: r.id, line: o.line, cost: o.cost };
      }
      if (best && X.orderBuilding(g, best.region, best.line)) jl.build++; else jl.could++;
    }
  }
  if (!plan.vow && !plan.head) return;
  const log = ((run as any).cab ??= { vows: [] as any[], heads: [] as any[] });
  if (plan.head && v.head) {
    const h = v.head;
    const detail = headDetail(g, run.me, h);
    const ok = plan.head === 'sack' ? X.orderSack(g, h.general) : X.orderStandBy(g);
    log.heads.push({ turn: g.turn, name: h.name, why: h.why, did: plan.head, ok, detail });
  }
  if (plan.vow && !v.promise && X.whyNotOrderPromise?.(g) === null) {
    const order = ['safe', 'fair', 'bold', 'daring'];
    const all = (X.promiseTargetsOf(g, 12) as any[]).flatMap((t) => t.terms as any[]);
    const pool = plan.vow === 'any' ? all.sort((a, b) => order.indexOf(b.band) - order.indexOf(a.band) || b.months - a.months || b.odds - a.odds) : all.filter((t) => t.band === plan.vow).sort((a, b) => b.months - a.months || b.odds - a.odds);
    const pick = pool[0];
    if (pick && X.orderPromise(g, pick.region, pick.months)) log.vows.push({ turn: g.turn, town: pick.town, band: pick.band, months: pick.months, odds: pick.odds, depth: pick.depth, trust: pick.trust, capital: pick.capital, price: pick.price });
  }
}

export function playMonth(run: Run, plan: Plan, row: MonthRow): void {
  const g = run.g, me = run.me;
  plan.first?.(g, run);
  if (plan.idle) { plan.each?.(g, run); return; }
  cabinetMoves(run, plan);
  menMoves(run, plan);
  const minOdds = plan.minOdds ?? 1.5;

  // A small country chooses its path in its first month.
  if (plan.path) {
    const opts = T.paths(g);
    const o = opts.find((p: any) => p.id === plan.path);
    if (o && !(o as any).why && !run.g.flags[`pt:path`]) {
      const partner = plan.path === 'league' ? T.leaguePartners(g).find((p: any) => p.yes)?.nat : undefined;
      if (T.orderPath(g, plan.path as any, partner)) { g.flags['pt:path'] = 1; row.notes.push(`took path ${plan.path}${partner ? ' with ' + partner : ''}`); delete g.flags['pt:path']; (run as any).pathDone = true; }
    }
  }

  // Wars the player starts.
  for (const [turn, on] of plan.declare ?? []) {
    if (g.turn >= turn && g.nations[on]?.alive && !atWar(g, me, on) && !allied(g, me, on) && !(run as any)[`declared:${on}`]) {
      if (T.declarable(g).includes(on)) {
        if (T.orderDeclare(g, on)) { row.notes.push(`declared war on ${on}`); (run as any)[`declared:${on}`] = true; }
      }
    }
  }

  // Research: put every free team on the quickest open technology.
  const tree = T.techTree(g);
  if (tree) {
    let free = tree.teams - tree.busy;
    const first = plan.research ?? ['winterkit', 'mediums', 'tanks', 'motor'];
    const rank = (id: string): number => { const i = first.indexOf(id); return i < 0 ? 99 : i; };
    const ready = tree.nodes.filter((n) => n.state === 'ready' && n.why === null).sort((a, b) => rank(a.id) - rank(b.id) || a.months - b.months);
    for (const n of ready) { if (free <= 0) break; if (T.orderResearch(g, n.id)) { free--; row.bought.push(`research:${n.id}`); } }
  }

  // Trade: sign with whoever will.
  const book = T.tradeLedger(g) as any;
  if (book?.offers) for (const o of book.offers) if (!o.why && T.orderTrade(g, o.nat)) row.bought.push(`trade:${o.nat}`);

  // The arsenal: buy what is most wanted and can be afforded, keeping a small reserve.
  const want0 = plan.buy ?? ['corps:arm', 'corps:inf', 'corps:mot', 'air:ftr', 'work:factory'];
  const gunsDue = (run as any).gunsDue ?? ((run as any).gunsDue = [] as number[]);
  for (let n = 0; n < 6; n++) {
    const offers = T.arsenalOffers(g, g.nations[me].capital);
    // Guns on order arrive in five months; count them with the guns in the field.
    const mine = g.corps.filter((c) => c.nat === me && !(c as any).dead && c.kind !== 'fort');
    const needGuns = plan.guns !== undefined && mine.filter((c) => c.kind === 'art').length + gunsDue.filter((t: number) => t > g.turn).length < Math.floor((mine.length * plan.guns) / 10);
    const men = plan.menKeep !== undefined ? (T as any).manpowerOf?.(g, me) : null;
    const want1 = men && men.share < plan.menKeep! ? want0.filter((k) => !k.startsWith('corps:')) : want0;
    const want = needGuns && want1 === want0 ? ['corps:art', ...want0] : want1;
    const o = want.map((k) => offers.find((x) => x.key === k)).find((x) => x && x.why === null);
    if (!o) break;
    const b = T.orderArsenal(g, o.key, o.at);
    if (!b) break;
    if (o.key === 'corps:art') gunsDue.push(g.turn + 5);
    row.bought.push(o.key);
  }

  // Engineers: mend what is broken where they stand, or go by rail or road to the nearest cut line behind the army.
  for (const c of g.corps) {
    if (c.nat !== me || c.kind !== 'eng' || !T.canOrder(g, c) || c.job) continue;
    const jobs = T.engineerJobs(g, c);
    const job = ['rail', 'bridge', 'plant'].map((k) => jobs.find((j) => j.key === k && j.why === null)).find(Boolean);
    if (job) { if (T.orderEngineers(g, c, job.key)) { row.bought.push(`eng:${job.key}`); (c as any).ptBusy = g.turn; } continue; }
    // Nothing to mend here: look for a cut line on our ground.
    const m = T.reach(g, c);
    let best = -1, bestCost = 1e9;
    for (const [id, info] of m) if (!info.attack && id !== c.reg && allied(g, me, g.regions[id].ctrl) && railCut(g, id) && info.cost < bestCost) { best = id; bestCost = info.cost; }
    if (best >= 0) { if (T.orderMove(g, c, best)) (c as any).ptBusy = g.turn; }
    else {
      const cut = g.regions.filter((r) => r.ctrl === me && railCut(g, r.id)).sort((a, b) => Math.hypot(a.x - g.regions[c.reg].x, a.y - g.regions[c.reg].y) - Math.hypot(b.x - g.regions[c.reg].x, b.y - g.regions[c.reg].y))[0];
      if (cut && T.orderRail(g, c, cut.id)) (c as any).ptBusy = g.turn;
    }
  }

  // Towns (Version 54): a threatened city gets one corps inside, the capital two, as the computer's staff does.
  if (siegeRules(g)) {
    const d0 = distanceToEnemy(g, me, false);
    for (const r of g.regions) {
      if (r.ctrl !== me || !(d0[r.id] >= 0 && d0[r.id] <= 1)) continue;
      const view = T.townSiege(g, r.id);
      const hold = view?.size === 'capital' ? 2 : view?.size === 'city' ? 1 : 0;
      let inside = T.myGarrison(g, r.id).length;
      for (const c of r.units.filter((u) => u.nat === me && u.kind === 'inf' && !u.dead).sort((a, b) => a.divs * a.str - b.divs * b.str)) {
        if (inside >= hold) break;
        if (T.whyNotGarrisonTown(g, c) === null && T.orderGarrison(g, c)) { inside++; (run as any).garrisoned = ((run as any).garrisoned ?? 0) + 1; }
      }
    }
  }

  // The army.
  const my = (): Corps[] => g.corps.filter((c) => c.nat === me && T.canOrder(g, c) && c.ap > 0 && !(c.kind === 'eng' && (c.job || (c as any).ptBusy === g.turn)));
  const capital = g.nations[me].capital;
  const chaseId = chaseOf(g, plan), chaseOdds = plan.chaseOdds ?? 1.1;
  // Version 63: the corps that stand in their works beside the enemy (for a player who digs the line in).
  const lineDug = (c: Corps): boolean => plan.dig !== undefined && !!(c as any).dug && g.regions[c.reg].nb.some((j) => hostileUnits(g, g.regions[j], me).length > 0);
  const attackersFor = (r: Region): Corps[] => {
    const all = T.candidates(g, r).filter((c) => !(c.reg === capital && g.regions[capital].units.filter((u) => u.nat === me && u.kind !== 'fort').length <= 1));
    const full = pickAttackers(g, all, r, plan);
    if (plan.dig === undefined || !full.some(lineDug)) return full;
    const out = full.length ? T.attackOutlook(g, full, r) : null;
    if (out && out.odds >= plan.dig) return full;
    (run as any).dugKept = ((run as any).dugKept ?? 0) + 1;
    return pickAttackers(g, all.filter((c) => !lineDug(c)), r, plan);
  };
  for (let pass = 0; pass < 4; pass++) {
    let did = 0;
    // Attacks, best odds first.
    const targets = new Set<number>();
    for (const c of my()) for (const j of g.regions[c.reg].nb) if (hostileUnits(g, g.regions[j], me).length) targets.add(j);
    const looked = [...targets].map((j) => {
      const r = g.regions[j];
      const cands = attackersFor(r);
      const out = cands.length ? T.attackOutlook(g, cands, r) : null;
      if (out && (plan.grind !== undefined || plan.waves !== undefined)) withSum(g, out, cands, r);
      if (out && plan.waves !== undefined) { (out as any).spare = T.candidates(g, r).filter((c) => c.kind !== 'art' && c.kind !== 'eng').length - cands.length; (out as any).n = cands.length; }
      return { r, cands, odds: out?.odds ?? 0, out };
    });
    plan.onLook?.(g, looked, pass);
    const struck: Set<number> = (run as any).struckTurn === g.turn ? (run as any).struck : ((run as any).struckTurn = g.turn, (run as any).struck = new Set<number>());
    const secondWave = (id: number, o: any): boolean => plan.waves !== undefined && !!o && (struck.has(id) || ((o.works ?? 0) >= 0.5 && (o.spare ?? 0) >= Math.max(3, Math.ceil((o.n ?? 0) / 2))));
    const barFor = (id: number, o: any): number => id === chaseId ? chaseOdds : secondWave(id, o) ? Math.min(minOdds, plan.waves!) : plan.grind !== undefined && o && (o.works ?? 0) >= 0.5 ? Math.min(minOdds, plan.grind) : minOdds;
    const goes = (id: number, o: any): boolean => !!o && (o.odds >= barFor(id, o) || (plan.byChance !== undefined && (o.chance ?? 0) >= plan.byChance));
    const scored = looked.filter((x) => goes(x.r.id, x.out) && (!plan.homeOnly || x.r.owner === me || x.r.id === chaseId)).sort((a, b) => Number(b.r.id === chaseId) - Number(a.r.id === chaseId) || b.odds - a.odds);
    for (const x of scored) {
      const cands = attackersFor(x.r);
      if (!cands.length || !hostileUnits(g, x.r, me).length) continue;
      const out = T.attackOutlook(g, cands, x.r);
      if (out && (plan.grind !== undefined || plan.waves !== undefined || plan.onAttack)) withSum(g, out, cands, x.r);
      if (out && plan.waves !== undefined) { (out as any).spare = T.candidates(g, x.r).filter((c) => c.kind !== 'art' && c.kind !== 'eng').length - cands.length; (out as any).n = cands.length; }
      if (!out || !goes(x.r.id, out)) continue;
      if (out.odds < minOdds && x.r.id !== chaseId) (run as any).grinds = ((run as any).grinds ?? 0) + 1;
      if (plan.minChance !== undefined && ((out as any).chance ?? 1) < plan.minChance) continue;
      const rep = T.orderAttack(g, cands, x.r);
      if (rep) { row.attacksOrdered++; if (rep.taken || (rep as any).siege) row.attacksWon++; did++; plan.onAttack?.(g, out, rep, x.r); if (plan.waves !== undefined && (out as any).works >= 0.5 || struck.has(x.r.id)) { if (struck.has(x.r.id)) (run as any).secondBlows = ((run as any).secondBlows ?? 0) + 1, (rep.taken || (rep as any).siege) && ((run as any).secondWon = ((run as any).secondWon ?? 0) + 1); struck.add(x.r.id); } }
    }
    // Towns (Version 54): storm the town the army stands round when the odds are good, sooner settled for once the
    // siege has lasted two months; and bring a garrison out against the besiegers when it can win.
    if (siegeRules(g)) {
      const sr = run as any;
      for (const r of g.regions) {
        if (!atWar(g, me, r.ctrl) || !r.units.some((u) => u.nat === me && !u.dead)) continue;
        if (plan.homeOnly && r.owner !== me && r.id !== chaseId) continue;
        if ((plan as any).noStorm || T.whyNotStormTown(g, r.id) !== null) continue;
        const out = T.stormOutlook(g, r.id);
        const view = T.townSiege(g, r.id);
        const sat = view && view.since >= 0 ? g.turn - view.since : 0;
        const want = sat >= 2 ? Math.min(minOdds, 1.1) : Math.min(minOdds, 1.3);
        // Version 55: assault dice run against the attacker, so the card's chance is what an average player goes by:
        // six in ten the first two months round a town, an even chance after that.
        const fair = out && typeof (out as any).even === 'number' ? out.chance >= (r.id === chaseId ? 0.4 : (plan as any).stormAt ?? (sat >= 2 ? 0.5 : 0.6)) : !!out && out.odds >= want;
        if (!out || !fair) { if (out) sr.stormsHeld = (sr.stormsHeld ?? 0) + 1; continue; }
        const rep = T.orderStorm(g, r.id);
        if (rep && typeof (out as any).chance === 'number') { const b = Math.min(9, Math.floor(out.chance * 10)); sr.calib = sr.calib ?? {}; sr.calib[b] = sr.calib[b] ?? [0, 0]; sr.calib[b][0]++; if (rep.taken) sr.calib[b][1]++; }
        if (rep) { sr.storms = (sr.storms ?? 0) + 1; row.attacksOrdered++; if (rep.taken) { row.attacksWon++; sr.stormsWon = (sr.stormsWon ?? 0) + 1; } did++; }
      }
      for (const r of g.regions) {
        if (!allied(g, me, r.ctrl) || !besieged(g, r) || !T.myGarrison(g, r.id).length) continue;
        if (T.whyNotSallyOut(g, r.id) !== null) continue;
        const out = T.sallyOutlook(g, r.id);
        if (out && out.odds >= 1.4) { const rep = T.orderSally(g, r.id); if (rep) { sr.sallies = (sr.sallies ?? 0) + 1; row.attacksOrdered++; did++; } }
      }
    }
    // Mass on a point: reserves go by rail to stand beside the enemy province the staff has picked to break next.
    if (pass === 0 && !plan.noMass) mass(run, plan, row, minOdds);
    // Marches: walk into empty enemy ground, otherwise close up to the front.
    const d = distanceToEnemy(g, me, !!plan.homeOnly);
    for (const c of my()) {
      const here = g.regions[c.reg];
      if (c.reg === capital && here.units.filter((u) => u.nat === me && u.kind !== 'fort').length <= 1) continue; // someone stays in the capital
      if (lineDug(c)) continue; // a player who digs in leaves the line in its works
      const m = T.reach(g, c);
      let best = -1, bestScore = -1e9;
      for (const [id, info] of m) {
        if (info.attack || id === c.reg) continue;
        const r = g.regions[id];
        const enemyGround = atWar(g, me, r.ctrl);
        if (plan.homeOnly && r.owner !== me && id !== chaseId) continue;
        const dist = d[id] < 0 ? 99 : d[id];
        const score = (enemyGround ? 50 + r.vp * 10 + (r.cap ? 40 : 0) : 0) - dist * 4 - info.cost * 0.5 + (id === chaseId ? 300 : 0);
        if (score > bestScore) { bestScore = score; best = id; }
      }
      const cur = (d[c.reg] < 0 ? 99 : d[c.reg]);
      if (best >= 0) {
        const r = g.regions[best];
        const better = atWar(g, me, r.ctrl) || (d[best] >= 0 && d[best] < cur && d[best] >= 1);
        if (better) { const n = T.orderMove(g, c, best); if (n) { row.moves++; did++; } }
      }
    }
    if (!did) break;
  }

  // Troopships: corps with no enemy in reach by land sail to the friendly port nearest a front.
  if (!plan.homeOnly) {
    const d = distanceToEnemy(g, me, false);
    let lifted = 0;
    for (const c of g.corps) {
      if (c.nat !== me || !T.canOrder(g, c) || c.kind === 'fort' || c.kind === 'eng' || !g.regions[c.reg].port) continue;
      if (d[c.reg] >= 0 && d[c.reg] <= 6) continue;
      if (c.reg === capital && g.regions[capital].units.filter((u) => u.nat === me && u.kind !== 'fort').length <= 2) continue;
      // Half the army stays at home.
      if (g.regions[c.reg].owner === me && g.corps.filter((u) => u.nat === me && g.regions[u.reg].owner === me && !g.regions[u.reg].overseas).length <= g.corps.filter((u) => u.nat === me).length / 2) continue;
      let ports: Region[] = [];
      try { ports = T.shipTargets(g, c); } catch (e) { run.errors.push(`shipTargets threw: ${(e as Error).message}`); }
      const to = ports.filter((r) => d[r.id] >= 1 && d[r.id] <= 3).sort((a, b) => d[a.id] - d[b.id])[0];
      if (to && T.orderShip(g, c, to.id)) { lifted++; }
    }
    if (lifted) row.notes.push(`shipped ${lifted} corps overseas`);
  }

  // Landings from the sea, when asked for.
  if (plan.land) tryLanding(run, row, minOdds, plan);

  plan.each?.(g, run);
}

/** Adds the sum behind the card to an outlook (Version 61): both sides' strength, how far the line is prepared, how old its works are, what this blow wears off them, the run of the dice. */
function withSum(g: Game, out: any, cands: Corps[], r: any): void {
  const a = (T as any).previewAttack?.(g, cands, r);
  if (a) { out.A = a.A; out.D = a.D; out.prepared = a.prepared; out.works = a.works; out.wear = a.wear; out.dice = a.dice; }
}

/** The corps for an attack: the staff's pick under the frontage limit, or (plan.useGuns) the same with up to two artillery corps swapped in where that reads better on the card. */
function pickAttackers(g: Game, all: Corps[], r: any, plan: Plan): Corps[] {
  const base = limitAttackers(g, all, r);
  if (!plan.useGuns || !base.length) return base;
  const spare = all.filter((c) => c.kind === 'art' && !base.includes(c));
  if (!spare.length || base.filter((c) => c.kind === 'art').length >= 2) return base;
  let best = base, bestOut = T.attackOutlook(g, base, r) as any;
  let cur = [...base];
  for (const gun of spare.slice(0, 2 - base.filter((c) => c.kind === 'art').length)) {
    // The frontage is counted by the province the corps start from: the gun takes the place of the weakest corps beside it.
    const mates = cur.filter((c) => c.reg === gun.reg && c.kind !== 'art').sort((x, y) => x.divs * x.str - y.divs * y.str);
    const next = mates.length ? cur.map((c) => (c === mates[0] ? gun : c)) : [...cur, gun];
    const o = T.attackOutlook(g, next, r) as any;
    if (o && bestOut && (o.chance > bestOut.chance + 1e-9 || (o.chance >= bestOut.chance - 1e-9 && o.odds > bestOut.odds))) { best = next; bestOut = o; cur = next; }
  }
  return best;
}

function mass(run: Run, plan: Plan, row: MonthRow, minOdds: number): void {
  const g = run.g, me = run.me;
  const st = run as any;
  const capital = g.nations[me].capital;
  // The focus: an enemy-held province with troops in it, next to ground we or our allies hold.
  const chase = chaseOf(g, plan);
  const front = g.regions.filter((r) => hostileUnits(g, r, me).length && (!plan.homeOnly || r.owner === me || r.id === chase) && r.nb.some((j) => g.regions[j].ctrl === me));
  if (!front.length) { st.focus = -1; return; }
  if (chase >= 0 && front.some((r) => r.id === chase)) { if (st.focus !== chase) { st.focus = chase; st.focusSince = g.turn; } }
  else if (st.focus === undefined || st.focus < 0 || !front.some((r) => r.id === st.focus) || g.turn - (st.focusSince ?? 0) > 5) {
    let best = -1, bestScore = -1;
    for (const r of front) {
      const defence = hostileUnits(g, r, me).reduce((s, u) => s + u.divs * u.str, 0) * (r.fort ? 1 + 0.5 * r.fort.lv : 1) * (r.terrain === 'mountain' ? 1.6 : r.terrain === 'plains' ? 1 : 1.25);
      const score = (1 + r.vp + (r.cap ? 3 : 0)) / (1 + defence);
      if (score > bestScore) { bestScore = score; best = r.id; }
    }
    st.focus = best; st.focusSince = g.turn;
  }
  const focus = g.regions[st.focus];
  const staging = focus.nb.map((j) => g.regions[j]).filter((r) => allied(g, me, r.ctrl) && !hostileUnits(g, r, me).length);
  if (!staging.length) return;
  const stagingIds = new Set(staging.map((r) => r.id));
  const nextToEnemy = (c: Corps): boolean => g.regions[c.reg].nb.some((j) => hostileUnits(g, g.regions[j], me).length > 0);
  let sent = 0;
  for (const c of g.corps) {
    if (c.nat !== me || !T.canOrder(g, c) || c.ap < 4 || c.kind === 'eng' || stagingIds.has(c.reg)) continue;
    if (plan.dig !== undefined && (c as any).dug && nextToEnemy(c)) continue; // Version 63: a player who digs in masses his reserves, not his line
    // The line is thinned, not stripped: two corps stay in every province that faces the enemy.
    if (nextToEnemy(c)) {
      const here = g.regions[c.reg].units.filter((u) => u.nat === me && u.kind !== 'eng' && u.kind !== 'fort');
      if (here.length <= 2 || here.indexOf(c) < 2) continue;
    }
    if (c.reg === capital && g.regions[capital].units.filter((u) => u.nat === me && u.kind !== 'fort').length <= 1) continue;
    let went = false;
    for (const r of staging) { if (T.orderRail(g, c, r.id)) { sent++; went = true; break; } }
    if (!went) for (const r of staging) { const m = T.reach(g, c); if (m.has(r.id) && !m.get(r.id)!.attack && T.orderMove(g, c, r.id)) { sent++; break; } }
  }
  if (sent) row.notes.push(`massing ${sent} corps against ${focus.name}`);
}

function tryLanding(run: Run, row: MonthRow, minOdds: number, plan: Plan = {}): void {
  const g = run.g, me = run.me;
  const ports = g.regions.filter((r) => r.port && r.ctrl === me && r.units.some((u) => u.nat === me && T.canOrder(g, u) && u.ap > 0 && u.kind !== 'fort'));
  for (const p of ports) {
    const cs = p.units.filter((u) => u.nat === me && T.canOrder(g, u) && u.ap > 0 && u.kind !== 'fort').slice(0, Math.max(1, g.nations[me].sealift));
    if (!cs.length) continue;
    let targets: Region[] = [];
    try { targets = T.landTargets(g, cs); } catch (e) { run.errors.push(`landTargets threw: ${(e as Error).message}`); continue; }
    let best: { r: Region; score: number } | null = null;
    const st = run as any; st.landLog ??= [];
    if (!targets.length && st.landLog.length < 400) st.landLog.push(`${date(g.turn)} ${p.name}: no coast in reach (sealift ${g.nations[me].sealift})`);
    for (const r of targets) {
      const pl = T.previewLanding(g, cs, r.id);
      if (st.landLog.length < 400) st.landLog.push(`${date(g.turn)} ${p.name}->${r.name}: ${pl.why ?? `odds ${pl.assess ? pl.assess.odds.toFixed(2) : 'undefended'} risk ${pl.risk.toFixed(2)} sortie ${pl.sortie.toFixed(1)} sky ${pl.sky}`}`);
      if (pl.why) continue;
      const odds = pl.assess ? pl.assess.odds : 9;
      if (odds < minOdds || pl.risk > (plan.landRisk ?? 0.25)) continue;
      const score = r.vp * 10 + (r.port ? 5 : 0) + odds - pl.risk * 20;
      if (!best || score > best.score) best = { r, score };
    }
    if (best) {
      const res = T.orderLand(g, cs, best.r.id);
      row.notes.push(`landing at ${best.r.name} from ${p.name}: ${res.ok ? (res.taken ? 'ashore' : 'thrown back') : 'refused: ' + res.why}${res.lostAtSea ? `, ${Math.round(res.lostAtSea * 100) / 100} lost at sea` : ''}`);
      return; // one a month
    }
  }
}

// ---- A whole campaign ---------------------------------------------------------------------------------------------

export interface Opts { until?: number; level?: string; checkSave?: number[]; quiet?: boolean; onMonth?: (run: Run, row: MonthRow) => void }

export function campaign(me: string, from: number, seed: number, plan: Plan, o: Opts = {}): Run {
  const until = o.until ?? 76;
  const g = start(me, from, seed, o.level ?? 'colonel');
  const run: Run = { g, me, from, seed, level: o.level ?? 'colonel', rows: [], problems: [], fellOn: null, errors: [] };
  if (plan.noCabinet) (T as any).orderCabinet?.(g, false);
  if (plan.secure) (T as any).orderOffice?.(g, false);
  const seen = new Set<string>();
  let up: boolean;
  const t0 = Date.now();
  try {
    if (from === 1 && T.beginEve(g)) {
      const n = plan.idle ? 0 : T.orderEvePlan(g);
      T.endEve(g);
      run.problems.push(`[note] eve of war: staff plan moved ${n} corps`);
    }
    up = advance(g, (nat) => check(g, `after ${nat}`, run.problems, seen));
  } catch (e) { run.errors.push(`start: ${(e as Error).stack}`); return run; }
  let aiMs = Date.now() - t0;
  while (up && g.turn <= until) {
    const L = ledger(g, me);
    const mine = g.corps.filter((c) => c.nat === me && !c.dead);
    const before = g.regions.map((r) => r.ctrl);
    const r0 = g.reports.length, h0 = g.headlines.length;
    const targets = new Set<number>();
    for (const c of mine) if (T.canOrder(g, c)) for (const j of g.regions[c.reg].nb) if (hostileUnits(g, g.regions[j], me).length) targets.add(j);
    let good = 0;
    for (const j of targets) { const cands = limitAttackers(g, T.candidates(g, g.regions[j]), g.regions[j]); const out = cands.length ? T.attackOutlook(g, cands, g.regions[j]) : null; if (out && out.odds >= 1.3) good++; }
    const row: MonthRow = {
      turn: g.turn, date: date(g.turn), aiMs, treasury: Math.round(L.treasury * L.scale), net: Math.round(L.net * L.scale), debt: Math.round(L.debt * 100) / 100,
      myCorps: mine.length, myDivs: Math.round(mine.reduce((s, c) => s + c.divs * c.str, 0)), myRegions: g.regions.filter((r) => r.ctrl === me).length,
      attacksOrdered: 0, attacksWon: 0, moves: 0, hitByEnemy: 0, gained: 0, lost: 0, attackOptions: targets.size, goodOptions: good, headlines: 0,
      wars: g.nations[me].atWar.filter((e) => g.nations[e].alive).join(','), bought: [], notes: [],
    };
    try {
      playMonth(run, plan, row);
      check(g, 'after the player', run.problems, seen);
      if (o.checkSave?.includes(g.turn)) saveCheck(run);
      judgeGoals(g, me);
      T.endHuman(g);
    } catch (e) { run.errors.push(`${date(g.turn)} player month: ${(e as Error).stack}`); break; }
    const t1 = Date.now();
    try {
      up = advance(g, (nat) => check(g, `after ${nat}`, run.problems, seen));
    } catch (e) { run.errors.push(`${date(g.turn)} computer month: ${(e as Error).stack}`); break; }
    aiMs = Date.now() - t1;
    if (siegeRules(g)) {
      const log = ((run as any).sieges ??= new Map<number, { since: number; holder: string }>()), done = ((run as any).siegesDone ??= [] as { name: string; months: number; fell: boolean; by: string; holder: string; ended: number }[]);
      for (const r of g.regions) {
        const on = besieged(g, r), was = log.get(r.id);
        if (on && !was) log.set(r.id, { since: g.turn, holder: r.ctrl });
        else if (was && (!on || r.ctrl !== was.holder)) { done.push({ name: r.name, months: g.turn - was.since, fell: r.ctrl !== was.holder, by: r.ctrl, holder: was.holder, ended: g.turn }); log.delete(r.id); if (on) log.set(r.id, { since: g.turn, holder: r.ctrl }); }
      }
    }
    row.hitByEnemy = g.reports.slice(r0).filter((r) => r.attNat !== me && (r.defNat === me || r.defenders.some((u) => u.nat === me))).length;
    row.gained = g.regions.filter((r, i) => r.ctrl === me && before[i] !== me).length;
    row.lost = g.regions.filter((r, i) => r.ctrl !== me && before[i] === me).length;
    row.headlines = g.headlines.length - h0;
    run.rows.push(row);
    if ((row.turn - 5) % 12 === 0) (run as any).armies = [...((run as any).armies ?? []), `${row.date}: ` + Object.values(g.nations).filter((n) => n.alive && armySize(g, n.id) >= 20).map((n) => `${n.id} ${Math.round(armySize(g, n.id))}`).join(', ')];
    o.onMonth?.(run, row);
  }
  if (!g.nations[me].alive) run.fellOn = g.nations[me].fellOn?.turn ?? g.turn;
  return run;
}

/** Saves and loads in the middle of a month and checks the loaded game is the same game. */
export function saveCheck(run: Run): void {
  const g = run.g;
  const text = saveGame(g);
  const back = loadGame(text);
  if (!back) { run.problems.push(`[${date(g.turn)}] the game could not be loaded from its own save (${Math.round(text.length / 1024)} KB)`); return; }
  const again = saveGame(back);
  if (again !== text) {
    const a = JSON.parse(text), b = JSON.parse(again);
    const diff = Object.keys(a).filter((k) => JSON.stringify(a[k]) !== JSON.stringify(b[k]));
    // The same contents with the keys of a table in another order is not a different game (Version 54: the supply table's countries can change places).
    const canon = (v: unknown): string => JSON.stringify(v, (_k, x) => (x && typeof x === 'object' && !Array.isArray(x) ? Object.fromEntries(Object.entries(x).sort(([p1], [q1]) => (p1 < q1 ? -1 : p1 > q1 ? 1 : 0))) : x));
    const real = diff.filter((k) => canon(a[k]) !== canon(b[k]));
    if (real.length) run.problems.push(`[${date(g.turn)}] save, load, save again does not give the same save: differs in ${real.join(', ')}`);
    else run.problems.push(`[${date(g.turn)}] save, load, save again gives the same contents with the keys in another order: ${diff.join(', ')}`);
  }
  run.problems.push(`[note] save at ${date(g.turn)} is ${Math.round(text.length / 1024)} KB`);
}

export function summary(run: Run): string {
  const g = run.g, me = run.me;
  const rows = run.rows;
  const last = rows[rows.length - 1];
  const quiet = rows.filter((r) => r.attacksOrdered === 0 && r.hitByEnemy === 0 && r.gained === 0 && r.lost === 0).length;
  // Longest stretch of months in which nothing happened to the player at all.
  let best = 0, cur = 0, bestEnd = 0;
  for (const r of rows) { if (r.attacksOrdered === 0 && r.hitByEnemy === 0 && r.gained === 0 && r.lost === 0) { cur++; if (cur > best) { best = cur; bestEnd = r.turn; } } else cur = 0; }
  const fell = Object.values(g.nations).filter((n) => n.fellOn).map((n) => `${n.id} ${date(n.fellOn!.turn)}→${n.fellOn!.to}${n.alive ? ' (back)' : ''}`).join('; ');
  const ms = rows.map((r) => r.aiMs).sort((a, b) => a - b);
  return [
    `${me} from ${date(run.from)} seed ${run.seed} level ${run.level}: played ${rows.length} months to ${last ? last.date : '-'}; ${g.nations[me].alive ? 'still standing' : `FELL ${run.fellOn ? date(run.fellOn) : ''}`}`,
    `  goals: short ${goalState(g, 'short')}, long ${goalState(g, 'long')}`,
    `  quiet months (no battle, no ground changing hands for the player): ${quiet} of ${rows.length}; longest quiet run ${best} months ending ${best ? date(bestEnd) : '-'}`,
    `  attacks ordered ${rows.reduce((s, r) => s + r.attacksOrdered, 0)}, won ${rows.reduce((s, r) => s + r.attacksWon, 0)}; attacked by the enemy ${rows.reduce((s, r) => s + r.hitByEnemy, 0)} times`,
    `  treasury at end ${last?.treasury}, net ${last?.net}/month, army ${last?.myCorps} corps ${last?.myDivs} divs, provinces ${last?.myRegions}`,
    `  computer's months took: median ${ms[Math.floor(ms.length / 2)]} ms, worst ${ms[ms.length - 1]} ms`,
    `  fallen: ${fell || 'nobody'}`,
    ...(siegeRules(g) ? [siegeLine(run)] : []),
    `  problems: ${run.problems.filter((p) => !p.startsWith('[note]')).length}, errors: ${run.errors.length}`,
  ].join('\n');
}

/** What the sieges of a run came to: the rules' own tallies, the player's assaults, and the longest sieges seen. */
export function siegeLine(run: Run): string {
  const g = run.g, sr = run as any;
  const tally = Object.keys(g.flags).filter((k) => k.startsWith('sgn:')).map((k) => `${k.slice(4)} ${g.flags[k]}`).join(', ');
  const done: { name: string; months: number; fell: boolean; by: string; holder: string; ended: number }[] = sr.siegesDone ?? [];
  const open: Map<number, { since: number; holder: string }> = sr.sieges ?? new Map();
  const long = [...done].sort((a, b) => b.months - a.months).slice(0, 4).map((x) => `${x.name} ${x.months} months (${x.fell ? `fell to ${x.by}` : 'raised'}, ${date(x.ended)})`).join('; ');
  const still = [...open].map(([id, x]) => `${g.regions[id].name} since ${date(x.since)} (${g.turn - x.since} months)`).sort().slice(0, 6).join('; ');
  return `  sieges: ${done.length} ended (${done.filter((x) => x.fell).length} towns fell, ${done.filter((x) => !x.fell).length} raised), mean ${done.length ? (done.reduce((s2, x) => s2 + x.months, 0) / done.length).toFixed(1) : '-'} months; my assaults ${sr.storms ?? 0} (${sr.stormsWon ?? 0} took the town), held back ${sr.stormsHeld ?? 0} times, sorties ${sr.sallies ?? 0}, corps sent into towns ${sr.garrisoned ?? 0}\n    assault card against result: ${Object.keys(sr.calib ?? {}).sort().map((b) => `${Number(b) * 10}%+ ${sr.calib[b][1]}/${sr.calib[b][0]}`).join(', ') || 'none'}\n    rules' tallies: ${tally || 'none'}\n    longest: ${long || '-'}\n    still running at the end: ${still || 'none'}`;
}
