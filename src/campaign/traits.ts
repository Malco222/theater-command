// National abilities and special units: what makes each country fight, build and earn in its own way.
// Everything lives in data/campaign/traits.json, one row per nation, so every version of the game reads the
// same list. The rules here apply in the grand campaign only: a historical scenario never sees them.

import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import data from '../../data/campaign/traits.json';
import { turnToDate } from '../core/ticket';
import { allied, atWar, headline, worksFor } from './game';
import { homeShare, levyMen, menLeft, menWords, poolAtStart } from './manpower';
import { farShare, provinceYield } from './economy';
import type { LedgerLine } from './economy';
import { techTraits } from './tech';
import { beyondEurope } from './world';
import type { Corps, Game, Kind, Region, Terrain } from './types';
import { KIND, MEN_PER_DIV } from './types';

/** When a trait counts in a battle. Everything listed must hold. */
export interface When {
  kinds?: Kind[];
  mobile?: boolean;
  terrain?: Terrain[];
  winter?: boolean;
  home?: boolean;
  works?: boolean;
  landing?: boolean;
  from?: number;
  until?: number;
}

export interface Trait {
  id: string;
  name: string;
  text: string;
  /** Already in the rules of every game: listed for the player to read, and adds nothing. What it does is in 'rule'. */
  builtin?: boolean;
  rule?: string;
  attack?: number;
  defence?: number;
  when?: When;
  income?: number;
  incomeMul?: number;
  incomeAt?: 'peace' | 'war';
  /**
   * The 'income' stands for works beyond the edge of the map (the Soviet works in the Urals and Siberia, ruling 228):
   * on a chart that shows the nation's own factories beyond Europe they earn as provinces, and the sum is not paid.
   */
  uncharted?: boolean;
  oil?: number;
  relocate?: number;
  drafts?: number;
  /**
   * The last levy (Version 68): with the enemy on its soil, its reserve of men under 'spent' of what it opened with
   * and its army in the field under 'under' divisions, 'men' divisions' worth of men join the reserve; 'most' times in
   * a campaign and never twice within 'gap' months. 'words': 'divisions' where the game's count of men to a division
   * would print a wrong figure (China's thin divisions, ruling 218): the card and the news then name the divisions and
   * no figure of men.
   */
  levy?: Levy;
  cost?: { what: string[]; mul: number };
  sealift?: number;
  alert?: boolean;
  partisans?: number;
  fleet?: number;
  seaLoss?: number;
}

/** The last levy's numbers (Version 68): see Trait.levy. */
export interface Levy { under: number; spent: number; men: number; gap: number; most: number; words?: 'divisions' }

/** A nation's special formation, raised from the arsenal like any corps. */
export interface Elite {
  id: string;
  label: string;
  word: string;
  kind: Kind;
  divs: number;
  cost: number;
  months: number;
  from?: number;
  most: number;
  starts?: number;
  attack?: number;
  defence?: number;
  when?: When;
  text: string;
}

export const TRAITS = data as unknown as {
  winter: number[];
  partisanGround: Terrain[];
  ai: { reserve: number };
  nations: Record<string, { traits: Trait[]; unit?: Elite }>;
};

/** National abilities are part of the grand campaign only. */
export const traitRules = (g: Game): boolean => g.flags.open === 1;

/** Every ability of a nation, in the order its row lists them. Read this for the nation picker and the in-game card. */
export const traitsOf = (nat: string): Trait[] => TRAITS.nations[nat]?.traits ?? [];

/** The nation's special formation, or null if it has none. */
export const eliteOf = (nat: string): Elite | null => TRAITS.nations[nat]?.unit ?? null;

/** The special formation this corps is, or null for an ordinary corps. */
export const eliteIn = (c: Corps): Elite | null => {
  if (!c.elite) return null;
  const u = eliteOf(c.nat);
  return u && u.id === c.elite ? u : null;
};

// ---- In battle --------------------------------------------------------------------------------------

interface Scene { year: number; winter: boolean }
let sceneTurn = -1, scene: Scene = { year: 0, winter: false };
function sceneOf(g: Game): Scene {
  if (g.turn !== sceneTurn) {
    const d = turnToDate(g.turn);
    scene = { year: d.year, winter: TRAITS.winter.includes(d.month) };
    sceneTurn = g.turn;
  }
  return scene;
}

function holds(g: Game, w: When | undefined, c: Corps, target: Region, landing: boolean): boolean {
  if (!w) return true;
  if (w.kinds && !w.kinds.includes(c.kind)) return false;
  if (w.mobile && !KIND[c.kind].mobile) return false;
  if (w.terrain && !w.terrain.includes(target.terrain)) return false;
  if (w.home && target.owner !== c.nat) return false;
  if (w.works && !worksFor(g, target, c.nat)) return false;
  if (w.landing && !landing) return false;
  if (w.winter || w.from !== undefined || w.until !== undefined) {
    const s = sceneOf(g);
    if (w.winter && !s.winter) return false;
    if (w.from !== undefined && s.year < w.from) return false;
    if (w.until !== undefined && s.year > w.until) return false;
  }
  return true;
}

function edge(g: Game, c: Corps, target: Region, landing: boolean, side: 'attack' | 'defence', names?: Set<string>): number {
  if (!traitRules(g)) return 1;
  const row = TRAITS.nations[c.nat];
  let m = 1;
  if (row) for (const t of row.traits) {
    const x = t[side];
    if (x === undefined || t.builtin || !holds(g, t.when, c, target, landing)) continue;
    m *= x;
    names?.add(t.name);
  }
  // What the nation has researched counts the same way: mobile warfare, heavy artillery, landing craft.
  for (const t of techTraits(g, c.nat)) {
    const x = t[side];
    if (x === undefined || !holds(g, t.when, c, target, landing)) continue;
    m *= x;
    names?.add(t.name);
  }
  const u = c.elite && row?.unit && row.unit.id === c.elite ? row.unit : null;
  const x = u?.[side];
  if (u && x !== undefined && holds(g, u.when, c, target, landing)) { m *= x; names?.add(u.label); }
  return m;
}

/** What its nation's abilities, and its own if it is a special formation, are worth to this corps attacking this ground. 1 when nothing applies. */
export const traitAttack = (g: Game, c: Corps, target: Region, landing = false): number => edge(g, c, target, landing, 'attack');

/** The same for a corps defending this ground. */
export const traitDefence = (g: Game, c: Corps, target: Region): number => edge(g, c, target, false, 'defence');

/** The names of the abilities and special formations that count for these corps in this fight, for the battle ticket. */
export function traitNames(g: Game, cs: Corps[], target: Region, side: 'attack' | 'defence', landing = false): string[] {
  const names = new Set<string>();
  for (const c of cs) edge(g, c, target, side === 'attack' && landing, side, names);
  return [...names];
}

// ---- In the books -----------------------------------------------------------------------------------

export interface TraitMoney {
  /** Production points added every month, each with the ability that earns it. */
  lines: LedgerLine[];
  flat: number;
  /** What the month's earnings are multiplied by, and the ability that does it. */
  mul: number;
  mulBy: string;
}

const NO_MONEY: TraitMoney = { lines: [], flat: 0, mul: 1, mulBy: '' };

/** True where the chart shows factories of the nation's own beyond the map of Europe (the far side of the world). */
const chartsWorks = (g: Game, nat: string): boolean => g.regions.some((r) => r.owner === nat && r.fac > 0 && beyondEurope(r));

/** What a nation's abilities add to its books this month. */
export function traitMoney(g: Game, nat: string): TraitMoney {
  const row = TRAITS.nations[nat];
  if (!traitRules(g) || !row) return NO_MONEY;
  const n = g.nations[nat];
  const out: TraitMoney = { lines: [], flat: 0, mul: 1, mulBy: '' };
  const add = (t: Trait, amount: number, count?: number): void => {
    if (amount <= 1e-9) return;
    out.lines.push({ key: `trait:${t.id}`, label: t.name, amount, count });
    out.flat += amount;
  };
  for (const t of row.traits) {
    if (t.builtin) continue;
    const now = !t.incomeAt || (t.incomeAt === 'war') === n.atWar.length > 0;
    if (t.incomeMul && now) { out.mul *= t.incomeMul; out.mulBy = t.name; }
    if (t.oil) {
      let oil = 0, wells = 0;
      for (const r of g.regions) if (r.ctrl === nat && r.owner === nat && r.oil) { oil += provinceYield(g, r, nat).oil; wells++; }
      add(t, oil * (t.oil - 1), wells);
    }
    // (An ability that both earns its own sum and keeps overrun factories at work, the Soviet factories in the east
    // (ruling 228), is one line in the books: the works beyond the Urals and the factories moved there.)
    const flat = t.income && now && !(t.uncharted && chartsWorks(g, nat)) ? t.income : 0;
    if (t.relocate) {
      // (They count whole wherever they land. Only a factory lost on the far side of the world itself is worth no more
      // gone east than it counted where it stood: ruling b466, economy.ts farShare.)
      let fac = 0, counted = 0;
      for (const r of g.regions) if (r.owner === nat && r.ctrl !== nat && r.fac > 0 && atWar(g, nat, r.ctrl)) { fac += r.fac; counted += r.fac * farShare(r); }
      add(t, flat + counted * t.relocate, fac || undefined);
    } else if (flat) add(t, flat);
  }
  return out;
}

const product = (g: Game, nat: string, pick: (t: Trait) => number | undefined): number => {
  if (!traitRules(g)) return 1;
  let m = 1;
  for (const t of traitsOf(nat)) { const x = t.builtin ? undefined : pick(t); if (x !== undefined) m *= x; }
  for (const t of techTraits(g, nat)) { const x = pick(t); if (x !== undefined) m *= x; }
  return m;
};

/** How much faster, or slower, this nation makes good its losses. */
export const traitDrafts = (g: Game, nat: string): number => product(g, nat, (t) => t.drafts);

/** What an order costs this nation, against the list price: 'corps:inf', 'work:fort', 'ship:dd', 'air:bmr' and so on. */
export const costMul = (g: Game, nat: string, key: string): number => product(g, nat, (t) => (t.cost?.what.includes(key) ? t.cost.mul : undefined));

/** The price this nation pays for an order with this list price, to a tenth of a production point. */
export function priceOf(g: Game, nat: string, key: string, list: number): number {
  const m = costMul(g, nat, key);
  return m === 1 ? list : Math.round(list * m * 10) / 10;
}

/** The fighting power of this nation's fleets, against the ordinary. */
export const traitFleet = (g: Game, nat: string): number => product(g, nat, (t) => t.fleet);

/** How much of the convoy war's cost reaches this nation. */
export const traitSeaLoss = (g: Game, nat: string): number => product(g, nat, (t) => t.seaLoss);

/** True for a nation nobody catches by surprise in the month war is declared. */
export const alertNation = (g: Game, nat: string): boolean => traitRules(g) && traitsOf(nat).some((t) => t.alert);

/** Shipping for this many more corps a month, from the nation's abilities. Added once, when the grand campaign opens. */
export const traitSealift = (nat: string): number => traitsOf(nat).reduce((s, t) => s + (t.builtin ? 0 : t.sealift ?? 0), 0);

// ---- Special formations ------------------------------------------------------------------------------

/** How many of its special formations a nation has in the field and on order. */
export function eliteCount(g: Game, nat: string): number {
  let k = g.nations[nat].queue.filter((b) => b.elite).length;
  for (const c of g.corps) if (c.nat === nat && !c.dead && c.elite) k++;
  // (Corps at sea on a long passage still count against the cap.)
  for (const v of g.atSea ?? []) if (v.nat === nat && v.corps.elite) k++;
  return k;
}

/** Why this nation cannot order its special formation now, apart from money and a place to raise it; null when it can. */
export function whyNoElite(g: Game, nat: string): string | null {
  const u = eliteOf(nat);
  if (!traitRules(g) || !u) return 'This nation has no special formation.';
  if (u.from !== undefined && sceneOf(g).year < u.from) return `Not before ${u.from}.`;
  if (eliteCount(g, nat) >= u.most) return `The nation can keep ${u.most} of these at once, in the field and on order.`;
  return null;
}

/**
 * When the grand campaign opens: the formations history had already raised (Italy's Alpine corps, Poland's cavalry)
 * take their place, and the merchant fleets of the seafaring nations are counted into their shipping.
 */
export function openTraits(g: Game): void {
  if (!traitRules(g) || g.flags['traits:open']) return;
  g.flags['traits:open'] = 1;
  for (const [nat, row] of Object.entries(TRAITS.nations)) {
    const n = g.nations[nat];
    if (!n) continue;
    n.sealift += traitSealift(nat);
    const u = row.unit;
    if (!u?.starts) continue;
    let left = u.starts;
    for (const c of g.corps) {
      if (left <= 0) break;
      if (c.nat === nat && !c.dead && c.kind === u.kind && !c.elite) { c.elite = u.id; left--; }
    }
  }
}

// ---- Month by month ---------------------------------------------------------------------------------

/** The men a nation has in the field, in divisions' worth: every corps at the strength it stands at. */
export function fieldStrength(g: Game, nat: string): number {
  let s = 0;
  for (const c of g.corps) if (c.nat === nat && !c.dead && c.kind !== 'fort') s += c.divs * c.str;
  return s;
}

/**
 * The last levy (Version 68): a nation with the ability, at war with the enemy on its own soil, whose reserve of men
 * is all but spent and whose army in the field has been driven under so many divisions, calls up every man fit to
 * bear arms. The men join the reserve, and the call-up and the depots do the rest. So an army that is being worn
 * away comes back, and one that is in no trouble is no larger for it. The same for the player and the computer.
 * Flags: lv:<nat> the levies called so far, lvt:<nat> the month of the last. Returns the men it brought, in divisions' worth.
 */
export function traitLevy(g: Game, nat: string): number {
  const n = g.nations[nat];
  const L = traitRules(g) && n?.alive && n.atWar.length ? traitsOf(nat).find((t) => t.levy)?.levy : undefined;
  if (!L) return 0;
  const count = g.flags[`lv:${nat}`] ?? 0, last = g.flags[`lvt:${nat}`];
  if (count >= L.most || (last !== undefined && g.turn - last < L.gap)) return 0;
  if (homeShare(g, nat) >= 1 || menLeft(g, nat) >= L.spent * poolAtStart(g, nat) || fieldStrength(g, nat) >= L.under) return 0;
  levyMen(g, nat, L.men);
  g.flags[`lv:${nat}`] = count + 1;
  g.flags[`lvt:${nat}`] = g.turn;
  headline(g, 'event', `${/^(United|Soviet|Netherlands|Dominican Republic)/.test(n.name) ? `The ${n.name}` : n.name} calls up every man fit to bear arms: ${L.words === 'divisions' ? `${levyBrings(L)}.` : `${levyBrings(L)} for its armies.`}`, nat);
  return L.men;
}

/**
 * The partisan war: every enemy corps standing on a nation's occupied ground, in the country that suits it,
 * loses part of its strength to raids and ambushes. It goes on after the country has fallen, against whoever
 * conquered it and his allies.
 */
export function traitsMonth(g: Game): void {
  if (!traitRules(g)) return;
  for (const [nat, row] of Object.entries(TRAITS.nations)) {
    const t = row.traits.find((x) => x.partisans);
    const X = g.nations[nat];
    if (!t || !X) continue;
    const victor = X.fellOn?.to;
    const foe = (id: string): boolean => (X.alive ? atWar(g, nat, id) : !!victor && (id === victor || allied(g, id, victor)));
    let hit = 0, where = -1;
    for (const r of g.regions) {
      if (r.owner !== nat || r.ctrl === nat || !TRAITS.partisanGround.includes(r.terrain) || !foe(r.ctrl)) continue;
      for (const c of r.units) {
        if (c.dead || !foe(c.nat) || c.str <= 0.3) continue;
        c.str = Math.max(0.3, c.str - t.partisans!);
        if (c.nat === g.human || nat === g.human) { hit++; where = r.id; }
      }
    }
    // The player hears of it when it is his army that bleeds, or his partisans that draw the blood: twice a year is enough.
    if (hit && g.turn % 6 === 0) {
      const mine = nat === g.human;
      headline(g, 'info', mine ? `The partisans go on fighting in the occupied provinces: ${hit === 1 ? 'one enemy corps' : `${hit} enemy corps`} lost men to raids and ambushes this month.` : `Partisans in occupied ${X.name} raid roads and depots: ${hit === 1 ? 'one of your corps there' : `${hit} of your corps there`} lost men this month. The mountains, hills and forests are their country.`, g.human ?? undefined, where, { arm: 'land' });
    }
  }
}

// ---- In words ---------------------------------------------------------------------------------------

const pct = (m: number): string => `${Math.round(Math.abs(m - 1) * 100)}%`;
const KIND_WORD: Record<string, string> = { inf: 'infantry', mot: 'motorised troops', arm: 'tanks', cav: 'cavalry', mtn: 'mountain troops', fort: 'fortress troops', art: 'artillery', eng: 'engineers' };
const DIV_WORD: Record<string, string> = { inf: 'infantry', mot: 'motorised', arm: 'armoured', cav: 'cavalry', mtn: 'mountain', art: 'artillery', eng: 'engineer' };
const ORDER_WORD: Record<string, string> = {
  'corps:inf': 'infantry corps', 'corps:mtn': 'mountain corps', 'corps:mot': 'motorised corps', 'corps:arm': 'armoured corps', 'corps:cav': 'cavalry', 'corps:art': 'artillery corps', 'corps:eng': 'engineer corps',
  'work:fort': 'fortifications', 'work:aa': 'anti-aircraft guns', 'work:lights': 'searchlights', 'ship:dd': 'destroyers', 'ship:ss': 'submarines', 'ship:ca': 'cruisers', 'ship:bb': 'battleships', 'ship:cv': 'carriers', 'ship:tp': 'troopships',
  'air:ftr': 'fighter wings', 'air:bmr': 'bomber wings',
};
const list = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

function whenWords(w: When | undefined, who: string): string {
  if (!w) return '';
  const out: string[] = [];
  if (w.kinds) out.push(`for ${list(w.kinds.map((k) => KIND_WORD[k] ?? k))}`);
  if (w.mobile) out.push('for tanks, motorised troops and cavalry');
  if (w.landing) out.push('in a landing from the sea');
  if (w.terrain) out.push(`in ${list(w.terrain.map((t) => (t === 'urban' ? 'cities' : t === 'mountain' ? 'mountains' : t === 'forest' ? 'forest' : t)))}`);
  if (w.works) out.push('behind fortifications');
  if (w.home) out.push(`on ${who} own ground`);
  if (w.winter) out.push('from December to March');
  if (w.from !== undefined) out.push(`from ${w.from}`);
  if (w.until !== undefined) out.push(`until the end of ${w.until}`);
  return out.length ? ` ${out.join(' ')}` : '';
}

function fightWords(x: { attack?: number; defence?: number; when?: When }, who: string): string {
  const a = x.attack, d = x.defence;
  if (a === undefined && d === undefined) return '';
  const head = a !== undefined && d !== undefined ? (a === d ? `Attack and defence up ${pct(a)}` : `Attack up ${pct(a)}, defence up ${pct(d)}`) : a !== undefined ? `Attack up ${pct(a)}` : `Defence up ${pct(d!)}`;
  return `${head}${whenWords(x.when, who)}.`;
}

const TIMES: Record<number, string> = { 3: 'three', 4: 'four', 5: 'five' };

/** What a levy brings, in words: men at the game's count, or the divisions alone where the row asks for it. */
const levyBrings = (L: Levy): string => (L.words === 'divisions' ? `men enough for ${+L.men.toFixed(1)} divisions` : `${menWords(L.men * MEN_PER_DIV)} more men`);

/**
 * The last levy in plain words, as the card gives it. A row the game already has in its rules ('builtin') carries its
 * words by hand in 'rule', and a levy on such a row ends that text with this sentence (tests/traits.test.ts).
 */
export function levyRule(L: Levy): string {
  return `With the enemy on its soil, its reserve of men all but spent and its army in the field under ${L.under} divisions, it calls up every man fit to bear arms: ${levyBrings(L)}, ${L.most === 1 ? 'once' : L.most === 2 ? 'twice' : `${TIMES[L.most] ?? L.most} times`} in a war at most and never twice within ${L.gap === 12 ? 'a year' : `${L.gap} months`}.`;
}

/**
 * What an ability does, in plain words worked out from its numbers, so the words and the rule cannot drift apart.
 * 'scale' turns production points into the money the player sees (BUDGET.scale).
 */
export function traitRule(t: Trait, scale = 100): string {
  if (t.builtin) return t.rule ?? 'Part of the rules of every campaign.';
  const out: string[] = [];
  const fight = fightWords(t, 'its');
  if (fight) out.push(fight);
  const at = t.incomeAt === 'peace' ? ' while at peace' : t.incomeAt === 'war' ? ' while at war' : '';
  if (t.income) out.push(`Earns ${Math.round(t.income * scale).toLocaleString('en-US')} a month${at}${t.uncharted ? ', for works beyond the edge of the map' : ''}.`);
  if (t.incomeMul) out.push(`Earnings up ${pct(t.incomeMul)}${at}.`);
  if (t.oil) out.push(`Home oilfields earn ${t.oil === 2 ? 'double' : `${t.oil} times as much`}.`);
  if (t.relocate) out.push(`${Math.round(t.relocate * 100)}% of the factories in home provinces an enemy holds go on earning.`);
  if (t.drafts) out.push(`Losses are made good ${pct(t.drafts)} ${t.drafts > 1 ? 'faster' : 'slower'}.`);
  if (t.levy) out.push(levyRule(t.levy));
  if (t.cost) out.push(`${list(t.cost.what.map((k) => ORDER_WORD[k] ?? k)).replace(/^./, (ch) => ch.toUpperCase())} cost ${pct(t.cost.mul)} ${t.cost.mul < 1 ? 'less' : 'more'}.`);
  if (t.sealift) out.push(`Shipping for ${t.sealift === 1 ? 'one more corps' : `${t.sealift} more corps`} a month.`);
  if (t.alert) out.push('No enemy gets the surprise of the first month against it.');
  if (t.partisans) out.push(`Enemy corps on its occupied mountains, hills and forests lose ${Math.round(t.partisans * 100)}% of their strength a month.`);
  if (t.fleet) out.push(`Fleets fight ${pct(t.fleet)} harder.`);
  if (t.seaLoss) out.push(`Convoy losses down ${pct(t.seaLoss)}.`);
  return out.join(' ');
}

/** What a special formation does, in plain words: its size, its edge in battle, when it can be raised and how many. */
export function eliteRule(u: Elite): string {
  const fight = fightWords(u, 'its');
  const size = `${u.divs === 1 ? `One ${DIV_WORD[u.kind] ?? u.kind} division` : `${u.divs} ${DIV_WORD[u.kind] ?? u.kind} divisions`}.`;
  return [size, fight, u.from !== undefined ? `Raised from ${u.from}.` : '', `At most ${u.most} at once.`].filter(Boolean).join(' ');
}
