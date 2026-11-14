// Unit stories: every corps, air fleet and fleet keeps a record of what it has done, and
// what it has done can earn it honours, a name, rivals and a place on the memorial wall.
// The honours, the letters and every number are in data/campaign/stories.json.
//
// One rule runs through all of it. Every unit of every nation keeps its record and can
// earn its honours, so the whole war has names in it. But nothing here changes a number
// for a computer-run nation: the edge an honour gives, experience thinned by green drafts,
// home ties, rivalries and postings home apply to the player's own units alone, and
// nothing here draws on the dice of the land war. History's wars play out as they did.

import data from '../../data/campaign/stories.json';
import budget from '../../data/campaign/budget.json';
import { Rng, hashString } from '../core/rng';
import { turnToDate } from '../core/ticket';
import { clamp } from '../core/math';
import { DESERT, EASTERN, allied, atWar, headline, relocate, weather } from './game';
import { gainXp, generalOf } from './commanders';
import { DIPLOMACY } from './diplomacy';
import {
  STACK,
  type AirFleet, type Award, type BattleReport, type Cash, type Corps, type DiaryLine, type Fallen, type Fleet, type Game, type Moment,
  type Region, type Rivalry, type Storied, type Stories, type UnitKind,
} from './types';

export interface HonourDef {
  id: string;
  for: UnitKind;
  name: string;
  title: string;
  needs: [string, number];
  also?: [string, number];
  each?: boolean;
  tour?: boolean;
  last?: boolean;
  edge?: { attack?: number; defence?: number; all?: number };
  fame: number;
  ribbon: [string, string];
  nick?: string[];
  cite: string;
  real: string;
}

export const STORIES = data as unknown as {
  grades: [number, string][];
  veterans: { dilute: number; green: number; raised: number; perSchool: number; maxSchools: number };
  edgeCap: number;
  log: { own: number; other: number };
  cash: { months: number; bondsPp: number; bondsPc: number; schoolXp: number; ready: number; pilots: number };
  home: { defence: number; attack: number; lostOrg: number; freedOrg: number };
  rivals: { meets: number; edge: number; org: number; xp: number; pc: number };
  legacy: { xp: number };
  tour: { perMonth: number };
  lastStand: number;
  generalXp: number;
  designation: Record<string, string>;
  voices: string[];
  lettersPerMonth: number;
  honours: HonourDef[];
  letters: Record<string, [string, string][]>;
};

const DEF = new Map(STORIES.honours.map((h) => [h.id, h]));
export const honourDef = (id: string): HonourDef | undefined => DEF.get(id);

type Unit = (Corps | AirFleet | Fleet) & Storied;

export const newStories = (): Stories => ({ awards: [], fallen: [], rivals: [], meets: {}, schools: {}, mail: [], told: {} });

// ---- Looking things up ------------------------------------------------------------------

export const awardOf = (g: Game, id: number): Award | undefined => g.stories.awards[id - 1];
export const fallenOf = (g: Game, id: number): Fallen | undefined => g.stories.fallen[id - 1];
export const rivalryOf = (g: Game, id: number | undefined): Rivalry | undefined => (id ? g.stories.rivals[id - 1] : undefined);

/** The living unit an honour belongs to, if it still exists. */
export function unitOf(g: Game, kind: UnitKind, id: number): Unit | undefined {
  return kind === 'corps' ? g.corps.find((c) => c.id === id) : kind === 'air' ? g.airFleets.find((f) => f.id === id) : g.fleets.find((f) => f.id === id);
}

/** What a unit's experience is called. */
export function grade(xp: number): string {
  let out = STORIES.grades[0][1];
  for (const [from, name] of STORIES.grades) if (xp >= from) out = name;
  return out;
}

/** What a nation calls a formation that has been named in the order of the day. */
export const designation = (nat: string): string => STORIES.designation[nat] ?? STORIES.designation._default;

/** The honours a unit carries, oldest first. */
export const honoursOf = (g: Game, u: Storied): Award[] => (u.hon ?? []).map((id) => awardOf(g, id)).filter((a): a is Award => !!a);

/** True for a formation that has earned the highest honour: a Guards formation, in the Red Army. */
export const isGuards = (g: Game, u: Storied): boolean => honoursOf(g, u).some((a) => a.hon === 'guards' && !a.old);

/** One of a unit's counters. Months of service and honours held are worked out, not kept. */
export function stat(g: Game, u: Storied & { born?: number }, k: string): number {
  if (k === 'service') return g.turn - (u.born ?? 1);
  if (k === 'honours') return honoursOf(g, u).filter((a) => !a.old && a.hon !== 'guards').length;
  return u.rec?.[k] ?? 0;
}

const bump = (u: Storied, k: string, n = 1): void => { const r = (u.rec ??= {}); r[k] = (r[k] ?? 0) + n; };

/** Writes a line in a unit's diary. A second attack beaten off at the same place in the same month is the same line. */
function note(g: Game, u: Unit, what: string, where: number, n?: number): void {
  const log = (u.log ??= []);
  const last = log[log.length - 1];
  if (n === undefined && last && last[0] === g.turn && last[1] === what && last[2] === where && (what === 'held' || what === 'repulsed' || what === 'defend')) { last[3] = (last[3] ?? 1) + 1; return; }
  log.push(n === undefined ? [g.turn, what, where] : [g.turn, what, where, n]);
  const most = u.nat === g.human ? STORIES.log.own : STORIES.log.other;
  if (log.length > most) log.splice(0, log.length - most);
}

const placeName = (g: Game, kind: UnitKind, at: number): string => (at < 0 ? '' : kind === 'fleet' ? g.seas[at]?.name ?? '' : g.regions[at]?.name ?? '');

const fill = (s: string, v: Record<string, string | number | undefined>): string => s.replace(/\{(\w+)\}/g, (_, k) => `${v[k] ?? ''}`).replace(/\s+/g, ' ').trim();

/** What an honour is called on the unit's colours: 'Sedan 1940', 'Held Tobruk', 'Guards'. */
export function awardTitle(g: Game, a: Award): string {
  const d = DEF.get(a.hon);
  if (!d) return a.hon;
  return fill(d.title, { place: placeName(g, a.kind, a.at), year: turnToDate(a.turn).year, rival: a.rival, designation: designation(a.nat) }) || d.name;
}

/** The citation read out when the honour is given. */
export function citation(g: Game, a: Award): string {
  const d = DEF.get(a.hon);
  if (!d) return '';
  const N = g.nations[a.nat];
  return fill(d.cite, { unit: a.name, place: placeName(g, a.kind, a.at) || 'the front', year: turnToDate(a.turn).year, n: a.n, rival: a.rival, designation: designation(a.nat), adj: N?.adj });
}

/** What an honour is worth when it is cashed in: more for the first of its kind in the nation. */
export const fameOf = (a: Award): number => (DEF.get(a.hon)?.fame ?? 1) + (a.first ? 1 : 0);

/** A corps by its number, in the field or kept as a cadre, as a diary names it. */
const corpsNamed = (g: Game, id: number | undefined): string =>
  (id === undefined ? undefined : g.corps.find((c) => c.id === id) ?? Object.values(g.nations).flatMap((n) => n.cadres ?? []).find((c) => c.id === id))?.name ?? 'another corps';

/** Writes a line in a corps' diary from outside this file: what happened, where, and a number that goes with it. */
export function noteCorps(g: Game, c: Corps, what: string, where: number, n?: number): void { note(g, c, what, where, n); }

/** One line of a diary as a sentence. */
export function diaryText(g: Game, u: { kind?: unknown; ships?: unknown; ftr?: unknown }, line: DiaryLine): string {
  const [, what, where, n] = line;
  const sea = 'ships' in u;
  const at = where < 0 ? 'the front' : sea || what === 'strike' ? `the ${g.seas[where]?.name ?? 'sea'}` : g.regions[where]?.name ?? 'the front';
  const times = n && n > 1 ? ` ${n} times` : '';
  switch (what) {
    case 'took': return `Took ${at}${n ? `, with ${n},000 prisoners` : ''}.`;
    case 'storm': return `Broke through at ${at}${n ? ` and took ${n},000 prisoners` : ''}.`;
    case 'capital': return `Fought its way into ${at}, the enemy's capital.`;
    case 'landed': return `Landed at ${at} from the sea and took it.`;
    case 'dropped': return `Came down on ${at} from the air and took it.`;
    case 'repulsed': return `Attacked ${at}${times} and was beaten off.`;
    case 'held': return `Held ${at} against attack${times}.`;
    case 'driven': return `Driven out of ${at}.`;
    case 'evac': return `Taken off by sea from ${at}.`;
    case 'cut': return `Cut off at ${at}.`;
    case 'relief': return `Back in supply at ${at} after ${n} months cut off.`;
    case 'honour': { const a = n ? awardOf(g, n) : undefined; return a ? `Honoured: ${awardTitle(g, a)}.` : 'Honoured.'; }
    case 'bonds': return 'Sent home on a bond tour.';
    case 'school': return 'Sent home to the training schools as instructors.';
    case 'back': return 'Back from home and ready for orders.';
    case 'homelost': return `${at} fell to the enemy: the place it was raised in.`;
    case 'homefree': return `${at} was freed: the place it was raised in.`;
    case 'rival': { const r = rivalryOf(g, n); return r ? `Met ${r.theirsName} again, at ${at}. The two are old enemies now.` : `Met an old enemy again at ${at}.`; }
    case 'score': { const r = rivalryOf(g, n); return r ? `Destroyed its old enemy ${r.theirsName} at ${at}.` : `Destroyed an old enemy at ${at}.`; }
    case 'drafts': return `Took in drafts: ${n}% of the corps is new to it.`;
    case 'reraised': return `Raised again in the name of the formation lost at ${at}.`;
    case 'folded': return `Took in the men of ${corpsNamed(g, n)} at ${at}.`;
    case 'cadre': return `Its men went to ${corpsNamed(g, n)} at ${at}. A cadre was kept to raise it again.`;
    case 'rebuilt': return `Raised again round its cadre at ${at}.`;
    case 'destroyed': return `Destroyed at ${at}.`;
    case 'surrendered': return `Laid down its arms at ${at}.`;
    case 'starved': return `Cut off and out of supply, laid down its arms at ${at}.`;
    case 'raid': return `Bombed ${at}.`;
    case 'night': return `Bombed ${at} by night.`;
    case 'strike': return `Struck at shipping in ${at}${n ? `: ${n} ${n === 1 ? 'ship' : 'ships'} sunk` : ''}.`;
    case 'sky': return `Won the sky over ${at}.`;
    case 'fight': return `Fought for the sky over ${at}.`;
    case 'defend': return `Fought off raids over ${at}${times}.`;
    case 'action': return `Won a fleet action in ${at}${n ? `: ${n} enemy ${n === 1 ? 'ship' : 'ships'} sunk` : ''}.`;
    case 'beaten': return `Beaten in a fleet action in ${at}${n ? `: ${n} ${n === 1 ? 'ship' : 'ships'} lost` : ''}.`;
    default: return what;
  }
}

// ---- Honours ----------------------------------------------------------------------------

const major = (nat: string): boolean => DIPLOMACY.majors.includes(nat);

/** A name the army might give the unit: one no other formation of its nation has. */
function nickFor(g: Game, u: Unit, d: HonourDef, kind: UnitKind, at: number): string | undefined {
  if (!d.nick?.length) return undefined;
  const place = placeName(g, kind, at);
  const taken = new Set<string>();
  for (const c of g.corps) if (c.nat === u.nat && c.nick) taken.add(c.nick);
  for (const f of g.airFleets) if (f.nat === u.nat && f.nick) taken.add(f.nick);
  for (const f of g.fleets) if (f.nat === u.nat && f.nick) taken.add(f.nick);
  for (const f of g.stories.fallen) if (f.nat === u.nat && f.nick) taken.add(f.nick);
  const all = d.nick.filter((t) => place || !t.includes('{place}')).map((t) => fill(t, { place })).filter((t) => !taken.has(t));
  if (!all.length) return undefined;
  return new Rng(hashString(`nick:${g.seed}:${u.nat}:${u.name}:${d.id}`)).pick(all);
}

const corpsRegion = (kind: UnitKind, u: Unit): number | undefined => (kind === 'corps' ? (u as Corps).reg : kind === 'air' ? (u as AirFleet).base : undefined);

/** Puts an honour on the roll and on the unit, and tells whoever should hear of it. */
function award(g: Game, kind: UnitKind, u: Unit, d: HonourDef, at: number, extra: { n?: number; rival?: string } = {}): Award {
  const S = g.stories, N = g.nations[u.nat];
  const first = !S.awards.some((a) => a.hon === d.id && a.nat === u.nat && !a.old);
  const a: Award = { id: S.awards.length + 1, turn: g.turn, hon: d.id, kind, unit: u.id, name: u.name, nat: u.nat, at };
  if (extra.n) a.n = extra.n;
  if (extra.rival) a.rival = extra.rival;
  if (first) a.first = true;
  S.awards.push(a);
  (u.hon ??= []).push(a.id);
  note(g, u, 'honour', at, a.id);
  const named = !u.nick && !d.last ? nickFor(g, u, d, kind, at) : undefined;
  if (named) u.nick = named;
  const service = kind === 'corps' ? 'army' : kind === 'air' ? 'air force' : 'navy';
  const where = kind === 'fleet' ? undefined : at >= 0 ? at : corpsRegion(kind, u);
  if (u.nat === g.human) {
    news(g, a, `${citation(g, a)}${named ? ` The ${service} has begun to call it ${named}.` : ''}`, where);
    if (kind === 'corps' && !d.last) {
      const c = u as Corps;
      // The general who led it shares in it.
      const gen = generalOf(g, c);
      if (gen) gainXp(g, gen, STORIES.generalXp);
      moment(g, 5, named ? 'nick' : 'honour', c, where ?? c.reg, { x: named ?? awardTitle(g, a) });
    }
  } else if (first && d.fame >= 2 && major(u.nat) && !d.last) {
    // The first of a great power's formations to make such a name is news everywhere.
    news(g, a, `${citation(g, a)}${named ? ` The ${N.adj} ${service} has begun to call it ${named}.` : ''}`, where);
  }
  return a;
}

/** While a battle's honours are being given out, the news of them waits here so that one honour won by several corps together is one piece of news. */
let batch: { a: Award; text: string; region?: number }[] | null = null;

function news(g: Game, a: Award, text: string, region?: number): void {
  if (batch) { batch.push({ a, text, region }); return; }
  headline(g, 'honour', text, a.nat, region);
  g.headlines[g.headlines.length - 1].award = a.id;
}

const listOf = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

/** Publishes the honours of one battle: each on its own, or together when several units earned the same one in the same place. */
function publish(g: Game): void {
  const all = batch ?? [];
  batch = null;
  const groups = new Map<string, typeof all>();
  const placed = (a: Award): boolean => !!DEF.get(a.hon)?.title.includes('{place}');
  for (const x of all) { const k = `${x.a.nat}:${x.a.hon}:${placed(x.a) ? x.a.at : ''}`; if (!groups.has(k)) groups.set(k, []); groups.get(k)!.push(x); }
  for (const xs of groups.values()) {
    if (xs.length === 1) { news(g, xs[0].a, xs[0].text, xs[0].region); continue; }
    const a = xs[0].a;
    const together = !placed(a) && xs.every((x) => x.a.at === a.at) && a.at >= 0;
    news(g, a, `${listOf(xs.map((x) => x.a.name))} have each earned the honour '${awardTitle(g, a)}'${together ? ` at ${placeName(g, a.kind, a.at)}` : ''}.`, together || placed(a) ? xs[0].region : undefined);
  }
}

/** Gives a unit whatever honours its record has now earned. */
function earn(g: Game, kind: UnitKind, u: Unit, at: number, extra: { n?: number; rival?: string } = {}): void {
  for (const d of STORIES.honours) {
    if (d.for !== kind || d.last) continue;
    const have = (u.hon ?? []).reduce((n, id) => { const a = awardOf(g, id); return n + (a && a.hon === d.id && !a.old ? 1 : 0); }, 0);
    const v = stat(g, u, d.needs[0]);
    if (d.each ? Math.floor(v / d.needs[1]) <= have : have > 0 || v < d.needs[1]) continue;
    if (d.also && stat(g, u, d.also[0]) < d.also[1]) continue;
    award(g, kind, u, d, at, extra);
  }
}

/** The edge a unit of the player's gets from the honours it still carries at the front. */
function edge(g: Game, u: Storied, key: 'attack' | 'defence' | 'all'): number {
  let e = 0;
  for (const id of u.hon ?? []) {
    const a = awardOf(g, id);
    if (!a || a.cash || a.old) continue;
    e += DEF.get(a.hon)?.edge?.[key] ?? 0;
  }
  return Math.min(STORIES.edgeCap, e);
}

const rivalIn = (g: Game, c: Corps, others: Corps[]): boolean => {
  const r = rivalryOf(g, c.rival);
  return !!r && !r.over && others.some((x) => x.id === r.theirs);
};

/** How much harder one of the player's corps attacks for its honours, for its home and against its rival. 1 for everyone else. */
export function storyAttack(g: Game, c: Corps, target: Region, defenders: Corps[]): number {
  if (c.nat !== g.human || c.kind === 'fort') return 1;
  let m = 1 + edge(g, c, 'attack');
  if (c.home === target.id && target.owner === c.nat) m += STORIES.home.attack;
  if (rivalIn(g, c, defenders)) m += STORIES.rivals.edge;
  return m;
}

/** The same for one of the player's corps defending. */
export function storyDefence(g: Game, c: Corps, target: Region, attackers: Corps[]): number {
  if (c.nat !== g.human || c.kind === 'fort') return 1;
  let m = 1 + edge(g, c, 'defence');
  if (c.home === target.id) m += STORIES.home.defence;
  if (rivalIn(g, c, attackers)) m += STORIES.rivals.edge;
  return m;
}

/** What the honours of one of the player's air fleets or fleets add to its fighting strength. 1 for everyone else. */
export const storyEdge = (g: Game, f: { nat: string } & Storied): number => (f.nat === g.human ? 1 + edge(g, f, 'all') : 1);

// ---- Letters ----------------------------------------------------------------------------

/** Remembers something that happened to one of the player's corps, for the month's letters. */
function moment(g: Game, w: number, m: string, c: { id: number; name: string; nat: string }, reg: number, o: { x?: string; n?: number; foe?: string } = {}): void {
  if (c.nat !== g.human) return;
  const gen = 'general' in c ? generalOf(g, c as Corps) : null;
  const it: Moment = { w, m, id: c.id, unit: c.name, reg };
  if (gen) it.gen = gen.name;
  if (o.x) it.x = o.x;
  if (o.n) it.n = o.n;
  if (o.foe) it.foe = o.foe;
  g.stories.mail.push(it);
}

const surname = (name: string): string => name.replace(/\s*\(.*\)\s*$/, '').trim().split(/\s+/).pop() ?? name;

/** Writes the month's letters: the weightiest moments, one to a unit and one of a kind. */
function post(g: Game): void {
  const S = g.stories;
  // A kind of letter that has been written often gives way to one that has not.
  const weight = (it: Moment): number => it.w - Math.min(3, 0.75 * (S.told[it.m] ?? 0));
  const mail = S.mail.sort((a, b) => weight(b) - weight(a));
  S.mail = [];
  const units = new Set<number>(), kinds = new Set<string>();
  let sent = 0;
  for (const it of mail) {
    if (sent >= STORIES.lettersPerMonth) break;
    if (units.has(it.id) || kinds.has(it.m)) continue;
    const bank = STORIES.letters[it.m];
    if (!bank?.length) continue;
    const told = S.told[it.m] ?? 0;
    S.told[it.m] = told + 1;
    const rng = new Rng(hashString(`letter:${g.seed}:${it.m}`));
    const [who, text] = bank[(rng.int(bank.length) + told) % bank.length];
    const c = g.corps.find((x) => x.id === it.id);
    const home = c?.home !== undefined ? g.regions[c.home].name : 'home';
    const body = fill(text, {
      place: g.regions[it.reg]?.name ?? 'the front', enemy: it.foe ?? 'enemy', general: it.gen ? `General ${surname(it.gen)}` : 'The general', home,
      rival: it.x, title: it.x, nick: it.x, first: it.x && it.n !== undefined ? g.regions[it.n]?.name : undefined, n: it.n,
    });
    const voice = who || new Rng(hashString(`voice:${g.seed}:${g.turn}:${it.id}:${it.m}`)).pick(STORIES.voices);
    headline(g, 'letter', body, g.human ?? undefined, it.reg >= 0 ? it.reg : undefined);
    const h = g.headlines[g.headlines.length - 1];
    h.by = `${voice}, ${it.unit}`;
    h.unit = it.id;
    units.add(it.id); kinds.add(it.m);
    sent++;
  }
}

// ---- What a battle writes into the records -------------------------------------------------

export interface FightFacts { sea?: number; air?: boolean; capital: boolean }

/**
 * A land battle is over: every corps in it has it in its record and its diary, the player's corps may have met an
 * old enemy again, and whoever has now done enough is honoured. Corps destroyed in it are already on the memorial wall.
 */
export function storyBattle(g: Game, rep: BattleReport, target: Region, preOdds: number, attackers: Corps[], defenders: Corps[], f: FightFacts): void {
  const win = rep.taken;
  const decisive = rep.level === 'Decisive victory' || rep.level === 'Heroic victory';
  const winter = weather(g).winter && EASTERN.deepOwners.includes(target.owner);
  const desert = target.terrain === DESERT.terrain;
  const share = attackers.length ? Math.round(rep.prisoners / attackers.length) : 0;
  const me = g.human;
  const defAdj = g.nations[rep.defNat]?.adj, attAdj = g.nations[rep.attNat]?.adj;
  attackers.forEach((c, i) => {
    const u = rep.attackers[i];
    const lossFrac = u.men > 0 ? u.lost / u.men : 0;
    bump(c, 'battles');
    if (u.lost) bump(c, 'lost', u.lost);
    if (!win) { note(g, c, 'repulsed', target.id); if (lossFrac >= 0.2) moment(g, 5, 'costly', c, target.id, { foe: defAdj }); return; }
    bump(c, 'won'); bump(c, 'taken');
    if (decisive) bump(c, 'stormed');
    if (share > 0) bump(c, 'pris', share);
    // The honour of a capital goes to the corps that marched in, not to everyone who fired a shot at it.
    const entered = f.capital && u.fate === 'advanced';
    if (entered) bump(c, 'caps');
    if (winter) bump(c, 'winter');
    if (desert) bump(c, 'desert');
    if (f.sea !== undefined) bump(c, f.air ? 'drops' : 'landings');
    if (c.home === target.id && target.owner === c.nat) bump(c, 'freed');
    const k = Math.round(share / 1000);
    note(g, c, entered ? 'capital' : f.sea !== undefined ? (f.air ? 'dropped' : 'landed') : decisive ? 'storm' : 'took', target.id, k > 0 && !entered && f.sea === undefined ? k : undefined);
    if (c.nat !== me) return;
    if (entered) moment(g, 8, 'capital', c, target.id, { foe: defAdj });
    else if (f.sea !== undefined && !f.air) moment(g, 6, 'landing', c, target.id, { foe: defAdj });
    else if (lossFrac >= 0.2) moment(g, 5, 'costly', c, target.id, { foe: defAdj });
    else if (decisive) moment(g, 4, 'victory', c, target.id, { foe: defAdj });
    else if (winter) moment(g, 3, 'winter', c, target.id, { foe: defAdj });
    else if (desert) moment(g, 3, 'desert', c, target.id, { foe: defAdj });
  });
  defenders.forEach((c, i) => {
    const u = rep.defenders[i];
    bump(c, 'battles');
    if (u.lost) bump(c, 'lost', u.lost);
    if (win) {
      if (!c.dead) { note(g, c, u.fate === 'evacuated' ? 'evac' : 'driven', target.id); moment(g, 3, 'defeat', c, target.id, { foe: attAdj }); }
      return;
    }
    bump(c, 'won');
    if (preOdds >= 1) bump(c, 'stood');
    if (winter) bump(c, 'winter');
    if (desert) bump(c, 'desert');
    note(g, c, 'held', target.id);
    if (preOdds >= 1) moment(g, 4, 'held', c, target.id, { foe: attAdj });
  });
  if (me) {
    if (attackers.some((c) => c.nat === me)) rivals(g, attackers, defenders, win, target);
    else if (defenders.some((c) => c.nat === me)) rivals(g, defenders, attackers, !win, target);
  }
  batch = [];
  try {
    for (const c of attackers) if (!c.dead) earn(g, 'corps', c, target.id);
    for (const c of defenders) if (!c.dead) earn(g, 'corps', c, target.id);
  } finally { publish(g); }
}

/** Ground taken without a fight goes in the diaries of the corps that marched in. */
export function storyWalk(g: Game, attackers: Corps[], target: Region): void {
  for (const c of attackers) { if (c.reg !== target.id) continue; bump(c, 'taken'); note(g, c, 'took', target.id); }
}

/** One of the player's corps and an enemy corps that keep meeting become rivals; a rivalry ends when one destroys the other. */
function rivals(g: Game, mine: Corps[], theirs: Corps[], won: boolean, target: Region): void {
  const S = g.stories, me = g.human;
  const field = theirs.filter((c) => c.kind !== 'fort');
  if (!field.length) return;
  // The corps they would remember: the biggest in front of them.
  const lead = [...field].sort((a, b) => b.divs - a.divs || a.id - b.id)[0];
  for (const c of mine) {
    if (c.nat !== me || c.kind === 'fort') continue;
    const R = rivalryOf(g, c.rival);
    if (R && !R.over) {
      const foe = theirs.find((x) => x.id === R.theirs);
      if (!foe) continue;
      R.met++;
      if (won) R.won++; else R.lost++;
      if (c.dead) { R.over = 'lost'; R.ended = g.turn; }
      else if (foe.dead && won) {
        R.over = 'won'; R.ended = g.turn;
        bump(c, 'scores');
        c.org = clamp(c.org + STORIES.rivals.org, 0, 1);
        c.xp = Math.min(1, c.xp + STORIES.rivals.xp);
        const n = g.nations[c.nat];
        n.pc = Math.min(DIPLOMACY.pcCap, n.pc + STORIES.rivals.pc);
        note(g, c, 'score', target.id, R.id);
        moment(g, 8, 'score', c, target.id, { x: R.theirsName, n: R.first });
        delete c.rival;
        earn(g, 'corps', c, target.id, { rival: R.theirsName });
      }
      continue;
    }
    if (c.dead || lead.dead || S.rivals.some((r) => !r.over && r.theirs === lead.id)) continue;
    const key = `${c.id}:${lead.id}`;
    const m = (S.meets[key] ??= [0, target.id]);
    m[0]++;
    if (m[0] < STORIES.rivals.meets) continue;
    const r: Rivalry = { id: S.rivals.length + 1, mine: c.id, theirs: lead.id, mineName: c.name, theirsName: lead.name, theirsNat: lead.nat, since: g.turn, at: target.id, first: m[1], met: m[0], won: 0, lost: 0 };
    S.rivals.push(r);
    delete S.meets[key];
    c.rival = r.id;
    note(g, c, 'rival', target.id, r.id);
    moment(g, 5, 'rival', c, target.id, { x: lead.name });
    headline(g, 'info', `${c.name} and the ${g.nations[lead.nat].adj} ${lead.name} have now met ${m[0]} times. The men on both sides know who is in front of them.`, c.nat, target.id);
  }
}

// ---- The memorial wall ----------------------------------------------------------------------

/**
 * A corps is about to be struck off after a battle or a siege: it goes on the memorial wall with its record. One that
 * went down after months cut off is given a last honour there. Corps that lay down their arms with their country,
 * or are broken up to make other formations, are not written here.
 */
export function storyFallen(g: Game, c: Corps, how: Fallen['how'], by: string, reg: number): void {
  if (c.kind === 'fort') return;
  const S = g.stories;
  note(g, c, how, reg);
  // A corps that fought on for months with no supply and went down fighting, not one that was simply starved out.
  const months = how === 'starved' ? 0 : c.rec?.cutNow ?? 0;
  if (months >= STORIES.lastStand) award(g, 'corps', c, DEF.get('laststand')!, reg, { n: months });
  const f: Fallen = { id: S.fallen.length + 1, nat: c.nat, name: c.name, kind: c.kind, divs: c.divs, turn: g.turn, reg, how, by, born: c.born ?? 1, home: c.home ?? -1, xp: Math.round(c.xp * 100) / 100 };
  if (c.rec) f.rec = c.rec;
  if (c.hon?.length) f.hon = c.hon;
  if (c.nick) f.nick = c.nick;
  // Everybody's dead are on the wall; the diaries kept are the player's own.
  if (c.log && c.nat === g.human) f.log = c.log;
  const gen = generalOf(g, c);
  if (gen) f.general = gen.name;
  S.fallen.push(f);
  if (c.nat === g.human) {
    moment(g, 9, 'fallen', c, reg, { foe: g.nations[by]?.adj });
    const said = months >= STORIES.lastStand ? ` It had been cut off for ${months} months.` : '';
    headline(g, 'info', `${c.name}${c.nick ? `, ${c.nick},` : ''} is struck off the army's list and written on the memorial wall.${said}`, c.nat, reg);
  }
}

/** Destroyed formations of a nation that could be raised again as this kind of corps, newest first. */
export function revivable(g: Game, nat: string, kind: string): Fallen[] {
  const queued = new Set(g.nations[nat]?.queue.map((b) => b.revive).filter((x) => x !== undefined));
  return g.stories.fallen.filter((f) => f.nat === nat && f.kind === kind && f.heir === undefined && !queued.has(f.id)).reverse();
}

/** A new corps takes up the name of a destroyed one: its ribbons, the name the army gave it, and part of what it knew. */
export function inherit(g: Game, c: Corps, id: number): void {
  const f = fallenOf(g, id);
  if (!f || f.heir !== undefined) return;
  f.heir = c.id;
  c.heir = f.id;
  if (f.nick) c.nick = f.nick;
  for (const old of honoursOf(g, f)) {
    const a: Award = { ...old, id: g.stories.awards.length + 1, unit: c.id, old: true, seen: true };
    delete a.cash; delete a.cashed;
    g.stories.awards.push(a);
    (c.hon ??= []).push(a.id);
  }
  c.xp = Math.min(1, c.xp + STORIES.legacy.xp * Math.max(0, f.xp - STORIES.veterans.green));
  note(g, c, 'reraised', f.reg);
}

// ---- Veterans and replacements ----------------------------------------------------------------

/** How well a nation's drafts are trained: better for every veteran cadre the player has sent to the schools. */
export const training = (g: Game, nat: string): number => STORIES.veterans.green + STORIES.veterans.perSchool * (g.stories.schools[nat] ?? 0);

/** The experience a new corps of the player's starts with. */
export const raisedXp = (g: Game, nat: string): number => STORIES.veterans.raised + STORIES.veterans.perSchool * (g.stories.schools[nat] ?? 0);

/**
 * Drafts have joined one of the player's corps: 'add' of its full strength, on top of the 'before' it had. Men who
 * have never been under fire thin out what the corps knows, and men trained by veterans thin it less.
 */
export function drafted(g: Game, c: Corps, before: number, add: number): void {
  if (c.nat !== g.human || add <= 0) return;
  const w = add * STORIES.veterans.dilute;
  const was = c.xp;
  c.xp = (c.xp * before + training(g, c.nat) * w) / (before + w);
  const fresh = add / (before + add);
  if (fresh >= 0.2 && was >= 0.4) {
    note(g, c, 'drafts', c.reg, Math.round(fresh * 100));
    moment(g, 2, 'drafts', c, c.reg);
  }
}

// ---- The monthly round ----------------------------------------------------------------------

/**
 * The start of a nation's month, once its supply is known: who is cut off and who has been relieved, whose home
 * has fallen or been freed, who is back from leave, and the honours that only time brings. For the player it
 * also closes rivalries that ended somewhere else and writes the month's letters.
 */
export function storyMonth(g: Game, nat: string): void {
  const mine = nat === g.human;
  batch = [];
  for (const c of g.corps) {
    if (c.nat !== nat || c.kind === 'fort') continue;
    if (!c.supplied) {
      bump(c, 'cutNow'); bump(c, 'cut');
      if (c.rec!.cutNow === 1) { note(g, c, 'cut', c.reg); moment(g, 6, 'cut', c, c.reg); }
    } else if (c.rec?.cutNow) {
      const n = c.rec.cutNow;
      delete c.rec.cutNow;
      if (n >= 2) {
        bump(c, 'relieved');
        note(g, c, 'relief', c.reg, n);
        moment(g, 7, 'relief', c, c.reg, { n });
        earn(g, 'corps', c, c.reg, { n });
      }
    }
    if (c.home !== undefined) {
      const h = g.regions[c.home];
      const lost = atWar(g, nat, h.ctrl);
      if (lost && !c.rec?.exile) {
        bump(c, 'exile');
        note(g, c, 'homelost', h.id);
        if (mine && c.reg !== h.id) { c.org = clamp(c.org - STORIES.home.lostOrg, 0, 1); moment(g, 6, 'homeLost', c, h.id, { foe: g.nations[h.ctrl]?.adj }); }
      } else if (!lost && c.rec?.exile && allied(g, nat, h.ctrl)) {
        delete c.rec.exile;
        note(g, c, 'homefree', h.id);
        if (mine) { c.org = clamp(c.org + STORIES.home.freedOrg, 0, 1); moment(g, 6, 'homeFreed', c, h.id); }
      }
    }
    if (c.leave !== undefined && g.turn >= c.leave) {
      delete c.leave;
      note(g, c, 'back', c.reg);
      moment(g, 2, 'back', c, c.reg);
      if (mine) headline(g, 'info', `${c.name} is back from home and ready for orders at ${g.regions[c.reg].name}.`, nat, c.reg);
    }
    earn(g, 'corps', c, c.reg);
  }
  publish(g);
  if (!mine) return;
  // A rivalry whose other half was lost somewhere else is over, and meetings with corps that no longer exist are forgotten.
  const S = g.stories;
  const alive = new Set(g.corps.map((c) => c.id));
  for (const r of S.rivals) {
    if (r.over || (alive.has(r.mine) && alive.has(r.theirs))) continue;
    r.over = alive.has(r.mine) ? 'gone' : 'lost';
    r.ended = g.turn;
    const c = g.corps.find((x) => x.id === r.mine);
    if (c) delete c.rival;
  }
  for (const k of Object.keys(S.meets)) { const [a, b] = k.split(':'); if (!alive.has(+a) || !alive.has(+b)) delete S.meets[k]; }
  post(g);
}

// ---- The air war and the war at sea ------------------------------------------------------------

/** An air operation is over: what each fleet that flew in it did goes in its record. */
export function storyAir(g: Game, mission: string, target: number, att: AirFleet[], def: AirFleet[], facts: { sky: boolean; sunk: number; attLost: number; defLost: number }): void {
  const sea = mission === 'strike';
  const bombing = mission === 'bomb' || mission === 'night' || sea;
  for (const f of att) {
    bump(f, 'flown');
    if (bombing) bump(f, 'missions', STORIES.tour.perMonth);
    if (mission === 'night') bump(f, 'nights');
    if (sea && facts.sunk) bump(f, 'sunk', Math.max(1, Math.round(facts.sunk / att.length)));
    if (facts.sky) bump(f, 'skies');
    if (facts.defLost) bump(f, 'kills', Math.round(facts.defLost / att.length));
    note(g, f, sea ? 'strike' : mission === 'night' ? 'night' : mission === 'bomb' ? 'raid' : facts.sky ? 'sky' : 'fight', target, sea && facts.sunk ? facts.sunk : undefined);
    const had = f.hon?.length ?? 0;
    earn(g, 'air', f, sea ? -1 : target);
    if (f.nat === g.human && (f.hon?.length ?? 0) > had && honoursOf(g, f).slice(had).some((a) => a.hon === 'tour')) g.stories.mail.push({ w: 7, m: 'tour', id: -f.id, unit: f.name, reg: f.base });
  }
  const fighters = def.reduce((s, x) => s + x.ftr, 0);
  for (const f of def) {
    bump(f, 'defended');
    if (facts.attLost) bump(f, 'saved', Math.round(facts.attLost * (fighters > 0 ? f.ftr / fighters : 1 / def.length)));
    note(g, f, 'defend', f.base);
    earn(g, 'air', f, f.base);
  }
}

/** A fleet action is over: the winners have it as a victory, with any great ship they sank, and the losers as a defeat. */
export function storySea(g: Game, zone: number, winners: Fleet[], losers: Fleet[], sunkCapital: number, sunkOfLosers: number, sunkOfWinners: number): void {
  for (const f of winners) {
    bump(f, 'actions');
    if (sunkCapital) bump(f, 'capital', sunkCapital);
    note(g, f, 'action', zone, sunkOfLosers || undefined);
    earn(g, 'fleet', f, zone);
  }
  for (const f of losers) note(g, f, 'beaten', zone, sunkOfWinners || undefined);
  const mine = winners.find((f) => f.nat === g.human);
  if (mine && sunkCapital) g.stories.mail.push({ w: 4, m: 'sea', id: -1000 - mine.id, unit: mine.name, reg: -1 });
}

/** A month on a convoy route: the raiders share the tonnage they sank, and the escorts have a month under attack to their names. */
export function storyConvoy(g: Game, raiders: Fleet[], escorts: Fleet[], tons: number, attacked: boolean): void {
  if (tons > 0) for (const f of raiders) { bump(f, 'tons', Math.round(tons / raiders.length)); earn(g, 'fleet', f, f.zone); }
  if (attacked) for (const f of escorts) { bump(f, 'escort'); earn(g, 'fleet', f, f.zone); }
}

// ---- What the player does with a unit's fame -------------------------------------------------

/** Where a corps sent home goes: the capital if there is room, or else the nearest of its own country's towns with room. */
function homeFor(g: Game, c: Corps): Region | null {
  const n = g.nations[c.nat];
  const room = (r: Region): boolean => r.owner === c.nat && r.ctrl === c.nat && !r.overseas && (r.id === c.reg || r.units.filter((u) => allied(g, u.nat, c.nat)).length < STACK);
  if (n.capital >= 0 && room(g.regions[n.capital])) return g.regions[n.capital];
  const here = g.regions[c.reg];
  let best: Region | null = null, bd = Infinity;
  for (const r of g.regions) {
    if (!room(r) || r.fac <= 0) continue;
    const d = Math.hypot(r.x - here.x, r.y - here.y);
    if (d < bd) { bd = d; best = r; }
  }
  return best;
}

/** Why an honour cannot be cashed in this way, or '' when it can. */
export function whyNotCash(g: Game, a: Award, how: Cash): string {
  if (a.nat !== g.human) return 'This is not your unit.';
  if (a.old) return 'This honour was earned by the formation that carried the name before. It is a ribbon now, and no more.';
  if (a.cash) return a.cash === 'bonds' ? 'This honour has already been home on a bond tour.' : 'The men who earned this honour are already at the schools.';
  const u = unitOf(g, a.kind, a.unit);
  if (!u) return 'The unit no longer exists.';
  if (a.kind === 'fleet' && how === 'school') return 'A fleet cannot be spared to the schools.';
  if (a.kind === 'corps') {
    const c = u as Corps;
    if (c.leave !== undefined) return 'The corps is already at home.';
    if (!c.supplied) return 'The corps is cut off. There is no way home.';
    if (!homeFor(g, c)) return 'There is nowhere at home for the corps to go.';
  }
  if (how === 'school' && (g.stories.schools[a.nat] ?? 0) >= STORIES.veterans.maxSchools) return 'The schools have all the instructors they can use.';
  return '';
}

/** What cashing an honour in would bring, in words, for the card. */
export function cashWorth(g: Game, a: Award, how: Cash): string {
  const C = STORIES.cash, fame = fameOf(a);
  if (how === 'bonds') return `+${(C.bondsPp * fame * budget.scale).toLocaleString('en-US')} for the treasury and +${C.bondsPc * fame} political capital`;
  const steps = Math.min(fame, STORIES.veterans.maxSchools - (g.stories.schools[a.nat] ?? 0));
  const better = `every later draft and new corps is trained ${Math.round(STORIES.veterans.perSchool * steps * 100)} points better`;
  return a.kind === 'air' ? `${C.pilots * fame} more trained pilots at once, and ${better}` : better;
}

/**
 * The player cashes in an honour. A bond tour sends the unit's heroes home to be seen: production and political
 * capital. Instructors go to the schools: every later draft is better trained. Either way the honour stops giving
 * its edge at the front, and a corps is out of the line for two months.
 */
export function cashHonour(g: Game, id: number, how: Cash): boolean {
  const a = awardOf(g, id);
  if (!a || whyNotCash(g, a, how)) return false;
  const u = unitOf(g, a.kind, a.unit)!;
  const C = STORIES.cash, fame = fameOf(a);
  const n = g.nations[a.nat];
  let gain: string;
  if (how === 'bonds') {
    n.pp += C.bondsPp * fame;
    n.pc = Math.min(DIPLOMACY.pcCap, n.pc + C.bondsPc * fame);
    gain = `The bond drive brings in ${(C.bondsPp * fame * budget.scale).toLocaleString('en-US')} for the treasury and ${C.bondsPc * fame} political capital.`;
  } else {
    g.stories.schools[a.nat] = Math.min(STORIES.veterans.maxSchools, (g.stories.schools[a.nat] ?? 0) + fame);
    if (a.kind === 'air') { const p = g.airPool[a.nat]; if (p) p.pilots += C.pilots * fame; }
    gain = 'From now on every draft reaches the front better trained.';
  }
  if (a.kind === 'corps') {
    const c = u as Corps;
    const to = homeFor(g, c)!;
    if (how === 'school') c.xp = Math.max(STORIES.veterans.green, c.xp - C.schoolXp * fame);
    if (to.id !== c.reg) relocate(g, c, to);
    c.ap = 0;
    c.leave = g.turn + C.months;
    delete c.yields; delete c.digs;
    note(g, c, how, to.id);
    moment(g, 5, how, c, a.at >= 0 ? a.at : c.reg);
    g.supplyDirty = true;
    headline(g, 'info', `${c.name} goes home to ${to.name} ${how === 'bonds' ? 'on a bond tour' : 'to train the next drafts'} and is out of the line for ${C.months} months. ${gain}`, a.nat, to.id);
  } else {
    const f = u as AirFleet | Fleet;
    f.ready = Math.max(a.kind === 'air' ? 0.3 : 0, f.ready - C.ready);
    note(g, f, how, -1);
    headline(g, 'info', `The men of ${f.name} who earned its honour go home ${how === 'bonds' ? 'on a bond tour' : 'to the training schools'}. ${gain}`, a.nat);
  }
  a.cash = how;
  a.cashed = g.turn;
  a.seen = true;
  return true;
}

/** The player's honours whose citation he has not yet read. */
export const unseen = (g: Game): Award[] => (g.human ? g.stories.awards.filter((a) => a.nat === g.human && !a.seen && !a.old) : []);

/** A player who takes a country over in the middle of the war has read everything written before he came. */
export function takeOver(g: Game): void {
  for (const a of g.stories.awards) a.seen = true;
  g.stories.mail = [];
}
