// Government and the war cabinet (the grand campaign).
//
// Every nation has a form of government. The player's has a war cabinet: four seats he fills with ministers, each
// of whom brings a gain and a price. Appointments are paid for in political capital, the same that courts neutrals
// and signs trade agreements, and so are decrees, which put the economy and the call-up on a war footing in steps.
// The cabinet's trust in him rises with
// ground won, battles won and jobs done, and falls with ground lost, debt and jobs ignored; a cabinet that has lost
// its trust for months on end falls, and takes its ministers and its newest decrees with it.
//
// Every few months the cabinet, or an ally, sets a directive: an optional job with a deadline and a reward, chosen
// from the state of the war. Take this town, hold that one, bring the army up to strength, take the pressure off
// an ally. A job done earns political capital, money or a formation. Jobs ignored cost trust, more each time.
//
// The cabinet talks back (Version 60). Its ministers sit with the hawks or the doves, and the lean of the table
// shapes what it asks for and what it is hard on. The player can go to it with a promise of his own, a town by a
// day he names, and the odds on the day set what keeping it earns and breaking it costs. And after defeats the
// cabinet now and then asks for a general's head: dismissed, its trust recovers; stood by, he can be vindicated.
//
// Computer nations keep a caretaker cabinet and no decrees, so nothing here moves their wars. The numbers are in
// data/campaign/government.json. The historical scenarios have none of this. Both interfaces read government()
// and give the orders through turn.ts.

import data from '../../data/campaign/government.json';
import { Rng, hashString } from '../core/rng';
import { turnToDate } from '../core/ticket';
import { KIND, MEN_PER_DIV, STACK, type Corps, type Directive, type Game, type General, type GovState, type Kind, type LostBattle, type Nation, type Region, type Vow } from './types';
import { COMMAND, corpsOf, popular, rankOf, serving } from './commanders';
import { raisable } from './manpower';
import { BUDGET, corpsCost, corpsName, income, specOf, upkeepTotal } from './economy';
import { allied, assess, atWar, corpsBase, frontage, headline, homeLost, hostileUnits, limitAttackers, theName, DEFENCE } from './game';
import { DIPLOMACY, pcIncome } from './diplomacy';
import { CIVIC, civicLevel, civicOffers, civicSum } from './buildings';
import { besieged, besiegerOf, garrisonOf, townHeld } from './siege';
import { agentCost, agentsOf, homeBase, mostAgents, oddsBand, INTEL } from './intel';
import { armyOf } from './joint';
import { prizeOf, prizesInReach, townName, townSize, type Prize, type TownSize } from './towns';
import { dealsOf, whyNoTrade } from './trade';

export type FormId = 'democracy' | 'oneparty' | 'authoritarian';
/** What a minister or a decree can change: multipliers on income, replacements, the price of new corps, research, trade and the class of men that comes of age each month. */
export type Lever = 'income' | 'drafts' | 'cost' | 'research' | 'trade' | 'men';
type Effects = Partial<Record<Lever | 'capital' | 'trust' | 'missed', number>>;
type Need = 'war' | 'long' | 'pressed';

interface Form { name: string; blurb: string; capital: number; swing: number; start: number; fallsAt: number; crisis: string }
interface Seat { id: string; name: string }
interface Minister { id: string; seat: string; name: string; plus: string; minus: string; effects: Effects; /** The wing of the cabinet he sits with; a caretaker sits with neither. */ wing?: 'hawk' | 'dove' }
interface Band { id: string; name: string; under: number; trust: number; capital: number; money: number; price: number }
/** The wing of the cabinet that has the better of the table. */
export type Wing = 'hawks' | 'doves' | 'balanced';
interface Step { id: string; name: string; text: string; cost?: number; needs?: Need; effects: Effects }
interface Track { id: string; name: string; steps: Step[] }

export const GOV = data as unknown as {
  forms: Record<FormId, Form>;
  nations: Record<string, FormId>;
  capital: { lowTrust: number; lowShare: number; lowIncome: number; highTrust: number; highAdd: number };
  trust: { rest: number; drift: number; province: number; home: number; win: number; defeat: number; capitalTaken: number; debt: number; ringed: number; cap: number; crisisMonths: number; afterCrisis: number };
  appoint: { cost: number; costMinor: number; start: number; wait: number };
  seats: Seat[];
  ministers: Minister[];
  decree: { wait: number };
  tracks: Track[];
  needs: { longMonths: number; pressedHome: number; pressedMonths: number };
  directives: {
    first: number; every: number; retry: number;
    success: { trust: number };
    missed: { base: number; step: number; most: number; rest: number; restMost: number; aside: number };
    take: { months: Record<TownSize, number>; capital: Record<TownSize, number>; money: Record<TownSize, number>; odds: number; least: number; reach: number; hard: { months: number; reward: number }; again: number };
    hold: { months: number; capital: number; siegeCapital: number; threat: number; lost: number; fresh: number };
    raise: { months: number; grow: number; least: number; capital: number };
    trade: { months: number; capital: number; money: number };
    relief: { months: number; capital: number; corps: Kind; pressed: number; overrun: number };
    spare: { months: number; capital: number; worn: number; share: number };
    build: { months: number; capital: number };
    agent: { months: number; capital: number };
    small: { quiet: number; missed: number };
  };
  wings: { top: number; hawks: { won: number; wins: number; lost: number }; doves: { defeats: number; losses: number; spare: number; rest: number } };
  promise: { every: number; months: number[]; reach: number; memory: number; deep: number[]; fullOdds: number; oddsMost: number; perDepth: number; perMonth: number; haste: number; back: number; size: Record<TownSize, number>; bands: Band[] };
  head: { every: number; answer: number; streak: number; winnable: number; chance: number; sacked: number; refused: number; watch: number; vindicated: number; wrong: number };
};

const MINISTER: Record<string, Minister> = Object.fromEntries(GOV.ministers.map((m) => [m.id, m]));
const clamp = (x: number, a: number, b: number): number => Math.max(a, Math.min(b, x));
const date = (t: number): string => turnToDate(t).label;

/** True where governments are in play: the grand campaign, unless the player has switched the war cabinet off. */
export const govRules = (g: Game): boolean => g.flags.open === 1 && g.flags.nogov !== 1;

/** True in a grand campaign whose player has switched the war cabinet off (g.flags.nogov). */
export const cabinetOff = (g: Game): boolean => g.flags.open === 1 && g.flags.nogov === 1;

/** True where the player has made his office secure (the grand campaign): the cabinet sits, judges and withholds, but it can never bring him down. */
export const officeSecure = (g: Game): boolean => g.flags.open === 1 && g.flags.nofall === 1;

/** A nation's form of government. */
export const formOf = (nat: string): FormId => GOV.nations[nat] ?? GOV.nations._default;

// ---- The state ---------------------------------------------------------------------------------

const heldBy = (g: Game, nat: string): number => g.regions.reduce((s, r) => s + (r.ctrl === nat ? 1 : 0), 0);
const homeHeld = (g: Game, nat: string): number => g.regions.reduce((s, r) => s + (r.owner === nat && r.ctrl === nat ? 1 : 0), 0);
const capitalsHeld = (g: Game, nat: string): number => g.regions.reduce((s, r) => s + (r.cap && r.owner !== nat && r.ctrl === nat ? 1 : 0), 0);
const lastReport = (g: Game): number => (g.reports.length ? g.reports[g.reports.length - 1].id : -1);

/** A government as it stands on its first day: caretakers in every office, no decrees, the first directive some months off. */
const fresh = (g: Game, nat: string): GovState => ({
  trust: GOV.forms[formOf(nat)].start, seats: {}, steps: {}, wait: {}, next: g.turn + GOV.directives.first, missed: 0, done: 0, low: 0, log: [], since: g.turn, asked: {},
  mark: { held: heldBy(g, nat), home: homeHeld(g, nat), caps: capitalsHeld(g, nat), report: lastReport(g) },
});

const mine = (g: Game, nat: string | null): nat is string => !!nat && govRules(g) && nat === g.human && !!g.nations[nat];

/** The player's government as it stands, without making it: what a panel reads. */
const seen = (g: Game, nat: string | null): GovState | null => (mine(g, nat) ? g.nations[nat].gov ?? fresh(g, nat) : null);

/**
 * The player's government, made the first time an order or a month needs it. Null outside the grand campaign and
 * for a nation the computer plays: its cabinet is a caretaker one, and changes nothing.
 */
export function govOf(g: Game, nat: string | null): GovState | null {
  if (!mine(g, nat)) return null;
  const n = g.nations[nat];
  return (n.gov ??= fresh(g, nat));
}

/**
 * Switches the war cabinet off or on (the grand campaign). With it off there are no ministers, no decrees, no trust
 * to keep and no directives: no lever moves, and political capital comes in as it would with no government at all.
 * Its open business is settled on the way out, at the price of leaving each thing undone: a directive is set aside,
 * a promise is taken back, and a question about a general is answered by standing by him. A general it is watching
 * is watched still when it comes back, so switching it off and on again clears no bill (cabinetBill lists it).
 * Switched on again it takes its seats back, reads the news from that day and sets its next directive some months on.
 */
export function setCabinet(g: Game, on: boolean): boolean {
  if (g.flags.open !== 1 || (g.flags.nogov !== 1) === on) return false;
  const nat = g.human;
  if (!on) {
    const st = nat ? g.nations[nat]?.gov : undefined;
    if (st && nat) {
      const n = g.nations[nat];
      // What is already done or lost is judged first; the rest is settled as if he had set it aside himself.
      judgeDirective(g, nat);
      if (st.dir) close(g, n, st, 'aside');
      if (st.vow) closeVow(g, n, st, 'back');
      if (st.head) standBy(g, nat);
      st.offAt = g.turn;
      st.low = 0;
    }
    g.flags.nogov = 1;
    return true;
  }
  delete g.flags.nogov;
  const st = govOf(g, nat);
  if (st && nat) {
    st.mark = { held: heldBy(g, nat), home: homeHeld(g, nat), caps: capitalsHeld(g, nat), report: lastReport(g) };
    st.next = Math.max(st.next, g.turn + GOV.directives.first);
    st.low = 0;
    delete st.last;
    // The watch on a general it stood by waited while it was away.
    if (st.stood && st.offAt !== undefined) st.stood.until += g.turn - st.offAt;
    delete st.offAt;
    delete st.reach;
  }
  return true;
}

/**
 * Says whether the player's government can fall (the grand campaign). With his office secure the cabinet, its
 * ministers, its directives and its trust all work as before, and a cabinet that doubts him still withholds money
 * and political capital: but however low its trust goes it never falls, and he is never turned out of office. It is
 * a setting of its own, apart from the cabinet's switch, and may be changed in any month: the count of months
 * without trust starts afresh either way.
 */
export function setCanFall(g: Game, can: boolean): boolean {
  if (g.flags.open !== 1 || (g.flags.nofall !== 1) === can) return false;
  if (can) delete g.flags.nofall; else g.flags.nofall = 1;
  const st = g.human ? g.nations[g.human]?.gov : undefined;
  if (st) st.low = 0;
  return true;
}

/**
 * What switching the cabinet off would settle today, line by line, with the trust each costs: what the player is
 * shown before he confirms. Empty where nothing is open.
 */
export function cabinetBill(g: Game): { text: string; trust: number }[] {
  const nat = g.human, st = seen(g, nat);
  if (!st || !nat) return [];
  const swing = GOV.forms[formOf(nat)].swing, out: { text: string; trust: number }[] = [];
  if (st.dir && standing(g, st.dir) === 'open') out.push({ text: `The directive is set aside: ${st.dir.title}`, trust: round1(missedCost(st, true, st.dir) * swing) });
  const v = st.vow, vr = v ? g.regions[v.region] : undefined;
  if (v && vr && atWar(g, nat, vr.ctrl) && g.turn <= v.due) out.push({ text: `Your promise is taken back: ${v.title}`, trust: round1(v.price * GOV.promise.back) });
  const gen = st.head ? generalBy(g, st.head.general) : undefined;
  if (gen) out.push({ text: `The cabinet takes it that you stand by ${gen.name}, and will watch his next battle when it is back`, trust: round1(GOV.head.refused * swing) });
  return out;
}

/** Sets up the player's government as a grand campaign opens. */
export function openGovernment(g: Game): void {
  if (!govOf(g, g.human)) return;
  // Enough political capital in hand for a first appointment, so that the smallest country has a use for its cabinet.
  const n = g.nations[g.human!];
  n.pc = Math.max(n.pc, GOV.appoint.start);
}

/** What an appointment costs this nation in political capital: less for a country that is not a great power. */
export const appointCost = (nat: string): number => (DIPLOMACY.majors.includes(nat) ? GOV.appoint.cost : GOV.appoint.costMinor);

/** True while the cabinet doubts the player: it withholds political capital and a share of the treasury's income. */
export const doubted = (st: GovState): boolean => st.trust < GOV.capital.lowTrust;

const ministerIn = (st: GovState, seat: string): Minister => MINISTER[st.seats[seat] ?? 'caretaker'] ?? MINISTER.caretaker;
const stepAt = (st: GovState, t: Track): Step => t.steps[clamp(st.steps[t.id] ?? 0, 0, t.steps.length - 1)];

function effectsOf(st: GovState): Effects[] {
  return [...GOV.seats.map((s) => ministerIn(st, s.id).effects), ...GOV.tracks.map((t) => stepAt(st, t).effects)];
}

/** What a nation's ministers and decrees do to one of the levers, as a multiplier: 1 for a computer nation and outside the grand campaign. */
export function govMul(g: Game, nat: string, lever: Lever): number {
  const st = g.nations[nat]?.gov;
  if (!st || !govRules(g) || nat !== g.human) return 1;
  let m = 1;
  for (const e of effectsOf(st)) m *= e[lever] ?? 1;
  // A cabinet that doubts him drags its feet: the ministries pay over less of what they collect.
  if (lever === 'income' && doubted(st)) m *= 1 - GOV.capital.lowIncome;
  return m;
}

const govAdd = (st: GovState, key: 'capital' | 'trust'): number => effectsOf(st).reduce((s, e) => s + (e[key] ?? 0), 0);

function capitalOf(nat: string, st: GovState): number {
  const C = GOV.capital;
  return GOV.forms[formOf(nat)].capital + govAdd(st, 'capital') - (st.trust < C.lowTrust ? pcIncome(nat) * C.lowShare : 0) + (st.trust >= C.highTrust ? C.highAdd : 0);
}

/**
 * What the player's government adds to his political capital each month, or takes from it: the form of government,
 * his ministers, and a cabinet that doubts him or believes in him. Nothing for a nation the computer plays.
 */
export function govCapital(g: Game, nat: string): number {
  const st = g.nations[nat]?.gov;
  return st && mine(g, nat) ? capitalOf(nat, st) : 0;
}

function note(g: Game, st: GovState, text: string): void {
  st.log.push({ turn: g.turn, text });
  if (st.log.length > 12) st.log.splice(0, st.log.length - 12);
}

// ---- Hawks and doves ---------------------------------------------------------------------------

/** The lean of the cabinet table: the ministers who sit with the hawks, less those who sit with the doves. From -4 to 4. */
function leanOf(st: GovState): number {
  let lean = 0;
  for (const s of GOV.seats) { const w = ministerIn(st, s.id).wing; lean += w === 'hawk' ? 1 : w === 'dove' ? -1 : 0; }
  return lean;
}

/** The wing that has the better of the table, if either has. */
const wingOf = (st: GovState): Wing => { const l = leanOf(st); return l >= GOV.wings.top ? 'hawks' : l <= -GOV.wings.top ? 'doves' : 'balanced'; };

const WING_NEWS: Record<Wing, string> = {
  hawks: 'The hawks now have the better of your war cabinet. It will ask you for towns, cheer ground and battles won the louder, and take ground lost hard.',
  doves: 'The doves now have the better of your war cabinet. It will ask you to hold, to build up, to trade and to spare the army, and it will take battles lost and heavy losses hard.',
  balanced: 'Neither wing has the better of your war cabinet now: its hawks and its doves balance each other.',
};

/** A shift in the balance of the table makes the news. */
function wingNews(g: Game, nat: string, st: GovState): void {
  const now = wingOf(st);
  if ((st.wing ?? 'balanced') === now) return;
  st.wing = now;
  note(g, st, now === 'balanced' ? 'The table is balanced again.' : `The ${now} have the better of the table.`);
  headline(g, 'info', WING_NEWS[now], nat);
}

/** The men a nation's corps have lost in battle in the reports after one, and the men in its army today. */
function menLostSince(g: Game, nat: string, after: number): number {
  let s = 0;
  for (const rep of g.reports) {
    if (rep.id <= after) continue;
    for (const u of rep.attackers) if (u.nat === nat && !u.guard) s += u.lost;
    for (const u of rep.defenders) if (u.nat === nat && !u.guard) s += u.lost;
  }
  return s;
}
const armyMen = (g: Game, nat: string): number => g.corps.reduce((s, c) => s + (c.nat === nat && !c.dead && c.kind !== 'fort' ? c.divs * c.str * MEN_PER_DIV : 0), 0);

// ---- Appointments and decrees ------------------------------------------------------------------

/** Months of war a nation has behind it: the longest of its wars still going. */
function warMonths(g: Game, nat: string): number {
  let most = 0;
  for (const foe of g.nations[nat].atWar) {
    const since = g.flags[`war:${nat}:${foe}`] ?? g.flags[`war:${foe}:${nat}`];
    most = Math.max(most, g.turn - (since ?? 1));
  }
  return most;
}

function whyNotNeed(g: Game, nat: string, need: Need | undefined): string | null {
  if (!need) return null;
  const n = g.nations[nat];
  if (!n.atWar.length) return 'Only a country at war accepts this.';
  if (need === 'war') return null;
  const lost = homeLost(g, nat), months = warMonths(g, nat);
  if (need === 'long') return months >= GOV.needs.longMonths || lost > 0 ? null : `The country will accept this after ${GOV.needs.longMonths} months of war, or once home ground is lost.`;
  return lost >= GOV.needs.pressedHome || months >= GOV.needs.pressedMonths ? null : `Only a country with its back to the wall accepts this: a tenth of the homeland lost, or ${GOV.needs.pressedMonths} months of war.`;
}

/** Why the player cannot put this minister in this seat, or null if he can. */
export function whyNotAppoint(g: Game, nat: string, seat: string, minister: string): string | null {
  const st = seen(g, nat);
  if (!st) return 'Only your own government takes your appointments, in the grand campaign.';
  const m = MINISTER[minister];
  if (!GOV.seats.some((s) => s.id === seat) || !m || (m.seat !== '*' && m.seat !== seat)) return 'Nobody of that kind is offered for that office.';
  if (ministerIn(st, seat).id === m.id) return 'He holds the office already.';
  const wait = (st.wait[`seat:${seat}`] ?? 0) - g.turn;
  if (wait > 0) return `The office changed hands lately: another change in ${wait === 1 ? 'a month' : `${wait} months`}.`;
  if (m.id !== 'caretaker' && g.nations[nat].pc < appointCost(nat)) return `It takes ${appointCost(nat)} political capital to carry the appointment: you have ${Math.floor(g.nations[nat].pc)}.`;
  return null;
}

/** Puts a minister in a seat of the war cabinet. A caretaker costs nothing. */
export function appoint(g: Game, nat: string, seat: string, minister: string): boolean {
  if (whyNotAppoint(g, nat, seat, minister)) return false;
  const st = govOf(g, nat)!, m = MINISTER[minister], s = GOV.seats.find((x) => x.id === seat)!;
  if (m.id !== 'caretaker') g.nations[nat].pc -= appointCost(nat);
  st.seats[seat] = m.id;
  st.wait[`seat:${seat}`] = g.turn + GOV.appoint.wait;
  note(g, st, `${m.name} becomes ${s.name}.`);
  headline(g, 'info', `A new ${s.name}: ${m.name.toLowerCase()} takes office. ${m.plus}. ${m.minus}.${m.wing ? ` He sits with the ${m.wing}s.` : ''}`, nat);
  wingNews(g, nat, st);
  return true;
}

/** Why the player cannot move a decree track to this step (one up, or one down), or null if he can. */
export function whyNotDecree(g: Game, nat: string, track: string, step: number): string | null {
  const st = seen(g, nat);
  if (!st) return 'Only your own government takes your decrees, in the grand campaign.';
  const t = GOV.tracks.find((x) => x.id === track);
  if (!t || !t.steps[step]) return 'There is no such decree.';
  const at = st.steps[track] ?? 0;
  if (step === at) return 'That is the law already.';
  if (step === at - 1) return null;
  if (step !== at + 1) return 'One step at a time.';
  const wait = (st.wait[`track:${track}`] ?? 0) - g.turn;
  if (wait > 0) return `This was relaxed lately: it cannot be tightened again for ${wait === 1 ? 'a month' : `${wait} months`}.`;
  const why = whyNotNeed(g, nat, t.steps[step].needs);
  if (why) return why;
  const cost = t.steps[step].cost ?? 0;
  if (g.nations[nat].pc < cost) return `It takes ${cost} political capital to carry this: you have ${Math.floor(g.nations[nat].pc)}.`;
  return null;
}

/** Moves a decree track one step up, for its price in political capital, or one step down, free. */
export function decree(g: Game, nat: string, track: string, step: number): boolean {
  if (whyNotDecree(g, nat, track, step)) return false;
  const st = govOf(g, nat)!, t = GOV.tracks.find((x) => x.id === track)!;
  const up = step > (st.steps[track] ?? 0);
  if (up) g.nations[nat].pc -= t.steps[step].cost ?? 0; else st.wait[`track:${track}`] = g.turn + GOV.decree.wait;
  st.steps[track] = step;
  note(g, st, `${up ? 'Decreed' : 'Relaxed to'}: ${t.steps[step].name}.`);
  headline(g, 'info', `${up ? 'By decree' : 'The government relaxes the law'}: ${t.steps[step].name.toLowerCase()}. ${t.steps[step].text}`, nat);
  return true;
}

// ---- Directives --------------------------------------------------------------------------------

const moneyShown = (x: number): string => Math.round(x * BUDGET.scale).toLocaleString('en-US');

function rewardWords(d: Directive): string {
  const parts: string[] = [];
  if (d.capital) parts.push(`${d.capital} political capital`);
  if (d.money) parts.push(`${moneyShown(d.money)} for the treasury`);
  if (d.corps) parts.push(`a new ${d.corps === 'inf' ? 'infantry' : d.corps} corps, armed by ${d.from === d.nat ? 'the state' : 'your ally'}`);
  return parts.length > 1 ? `${parts.slice(0, -1).join(', ')} and ${parts[parts.length - 1]}` : parts[0] ?? 'the cabinet\'s thanks';
}

/** The player's army as the map shows it: every division of every corps of his own, wherever it stands, and those still forming. */
export function armyCount(g: Game, nat: string): number {
  let s = 0;
  for (const c of g.corps) if (c.nat === nat && !c.dead && c.kind !== 'fort') s += c.divs;
  for (const b of g.nations[nat].queue) if (b.what === 'corps') s += b.divs ?? 0;
  return s;
}

const weight = (cs: Corps[]): number => cs.reduce((s, c) => s + (c.dead ? 0 : c.divs * c.str), 0);

/** The odds the cabinet will name at most: anything better is as good as certain. */
const ODDS_MOST = 99;

/** The corps of an army that an attack could draw on, where they stand: fit to march, under its own flag, and not shut up in a town's garrison. */
const fieldOf = (nat: string, x: Region): Corps[] => x.units.filter((c) => armyOf(c) === nat && !c.dead && c.kind !== 'fort' && !c.town);

/**
 * The force an army could bring against a province with the corps in reach brought up. Its corps beside the province
 * count where they stand. Those further off, to `reach` provinces, count as if they had marched up through their own
 * side's ground to a province beside it, spread over the roads in. `inside` is the same force as it would stand in
 * the province itself, for a town with no army left in the country round it. No more are brought up than the roads
 * carry, so an army three times the size the ground can use reads no better for it.
 */
function broughtUp(g: Game, nat: string, r: Region, reach: number, deep?: Map<number, number>): { beside: Corps[]; inside: Corps[] } {
  // For a town that lies beyond the front (a promise), the provinces on the way to it count as opened: `deep` says how far each lies from his ground.
  const far0 = deep?.get(r.id) ?? 1, nearer = (x: Region): boolean => far0 > 1 && (deep?.get(x.id) ?? 99) < far0;
  const open = (x: Region): boolean => allied(g, nat, x.ctrl) || nearer(x);
  const gates = r.nb.filter((j) => open(g.regions[j]) || fieldOf(nat, g.regions[j]).length > 0);
  const here = fieldOf(nat, r);
  // Which of the provinces beside it each province further off can reach, through ground its side holds.
  const reaches = new Map<number, number[]>();
  for (const gate of gates) {
    const seen = new Set<number>([gate, r.id]);
    let ring = [gate];
    for (let d = 2; d <= reach && ring.length; d++) {
      const next: number[] = [];
      for (const i of ring) for (const j of g.regions[i].nb) {
        if (seen.has(j) || gates.includes(j) || !open(g.regions[j])) continue;
        seen.add(j); next.push(j);
        const via = reaches.get(j);
        if (via) via.push(gate); else reaches.set(j, [gate]);
      }
      ring = next;
    }
  }
  const power = (c: Corps): number => corpsBase(g, c) * KIND[c.kind].atk;
  const beside: Corps[] = [], load: Record<number, number> = {};
  for (const gate of gates) for (const c of fieldOf(nat, g.regions[gate])) { beside.push(c); load[gate] = (load[gate] ?? 0) + 1; }
  // More than the roads carry is no use to a province: the strongest come up first, each to the emptiest road in.
  const room = frontage(r) + DEFENCE.frontage.extra + DEFENCE.frontage.guns;
  const far: Corps[] = [];
  for (const [j] of reaches) far.push(...fieldOf(nat, g.regions[j]));
  far.sort((x, y) => power(y) - power(x) || x.id - y.id);
  for (const c of far) {
    const via = reaches.get(c.reg)!.filter((gate) => (load[gate] ?? 0) < room).sort((x, y) => (load[x] ?? 0) - (load[y] ?? 0) || x - y)[0];
    if (via === undefined) continue;
    load[via] = (load[via] ?? 0) + 1;
    beside.push({ ...c, reg: via });
  }
  const inside = [...here, ...beside.map((c) => ({ ...c, reg: r.id }))].sort((x, y) => power(y) - power(x) || x.id - y.id);
  return { beside, inside };
}

/**
 * What the attack card would read against a province for this army with the corps in reach brought up: the same
 * reckoning as the card, so works, ground, the quality of the troops and the room on the roads all count. Against
 * an army in the field it is the odds of the battle for the country round the town; with none left there, the odds
 * of storming the town. Where the defenders are not all identified it is the staff's estimate, as on the card.
 * 0 where nothing of his can reach it.
 */
function cardOdds(g: Game, nat: string, r: Region, reach: number, deep?: Map<number, number>): number {
  const force = broughtUp(g, nat, r, reach, deep);
  const foes = hostileUnits(g, r, nat);
  let going: Corps[], town = false;
  if (foes.length) going = limitAttackers(g, force.beside, r);
  else {
    if (!atWar(g, nat, r.ctrl)) return 0;
    const line = force.inside.filter((c) => c.kind !== 'eng');
    const most = line.length ? Math.min(STACK, g.nations[line[0].nat].maxAttack || 99) : 0;
    going = [...line.slice(0, most), ...(most ? force.inside.filter((c) => c.kind === 'eng').slice(0, 1) : [])];
    town = true;
    if (going.length && !townHeld(g, r)) return ODDS_MOST;
  }
  if (!going.length) return 0;
  const a = assess(g, going, r, town ? { town: true } : {});
  const band = oddsBand(g, a.defenders, a.odds, r.id, nat);
  return Math.min(ODDS_MOST, band.exact ? a.odds : band.mid);
}

/** The player's odds against a town, as the cabinet reckons them before it asks for it: what the attack card would read with his corps within two provinces brought up. */
export function directiveOdds(g: Game, nat: string, region: number): number {
  const r = g.regions[region];
  return r ? cardOdds(g, nat, r, GOV.directives.take.reach) : 0;
}

/**
 * The odds an enemy could bring against a place of the nation's own, and who: the attack card as the enemy's staff
 * would read it with its corps within two provinces brought up. Null where no enemy can reach it.
 */
export function threatOdds(g: Game, nat: string, region: number): { odds: number; by: string } | null {
  const r = g.regions[region];
  if (!r) return null;
  const foes = new Set<string>();
  for (const j of r.nb) for (const c of g.regions[j].units) if (!c.dead && atWar(g, armyOf(c), nat)) foes.add(armyOf(c));
  let best: { odds: number; by: string } | null = null;
  for (const e of [...foes].sort()) {
    if (!atWar(g, e, r.ctrl) && !hostileUnits(g, r, e).length) continue;
    const odds = cardOdds(g, e, r, GOV.directives.take.reach);
    if (odds > 0 && (!best || odds > best.odds)) best = { odds, by: e };
  }
  return best;
}

interface Target { p: Prize; hard: boolean; again: boolean; odds: number }

/**
 * The towns the cabinet could ask for, the best first: enemy towns in reach against which the attack card, with
 * the player's corps in reach brought up, reads fair odds. A town his side already stands round is about to fall,
 * and is not asked for. With no fair one it names a hard one, and only with nothing else one it has asked for and
 * not been given.
 */
function takeable(g: Game, nat: string, st: GovState, only: (r: Region) => boolean = () => true): Target[] {
  const T = GOV.directives.take;
  const all: Target[] = [];
  for (const p of prizesInReach(g, nat, 200)) {
    const r = g.regions[p.region];
    if (!only(r)) continue;
    // A town he has promised of his own accord is his word already: the cabinet does not order it as well.
    if (st.vow?.region === r.id) continue;
    if (besieged(g, r)) { const by = besiegerOf(g, r); if (by && (by === nat || allied(g, nat, by))) continue; }
    const odds = directiveOdds(g, nat, r.id);
    if (odds <= 0 || odds < T.least) continue;
    const asked = st.asked?.[r.id];
    all.push({ p, hard: odds < T.odds, again: asked !== undefined && g.turn - asked < T.again, odds });
  }
  const tiers = [all.filter((x) => !x.hard && !x.again), all.filter((x) => x.hard && !x.again), all.filter((x) => !x.hard && x.again), all.filter((x) => x.hard && x.again)];
  return tiers.find((t) => t.length) ?? [];
}

/** Who is pressing a country: whoever holds most of its home ground, or failing that stands round its capital. */
function pressing(g: Game, a: Nation): string | null {
  const count: Record<string, number> = {};
  for (const r of g.regions) if (r.owner === a.id && r.ctrl !== a.id && atWar(g, a.id, r.ctrl)) count[r.ctrl] = (count[r.ctrl] ?? 0) + Math.max(1, r.vp);
  const top = Object.keys(count).sort((x, y) => count[y] - count[x] || (x < y ? -1 : 1))[0];
  if (top) return top;
  const cap = a.capital >= 0 ? g.regions[a.capital] : null;
  return cap && cap.ctrl === a.id && besieged(g, cap) ? besiegerOf(g, cap) : null;
}

/** True while an enemy is on the move against the nation or its allies: it has attacked one of them within the months the cabinet counts as fresh. */
function onTheMove(g: Game, foe: string, nat: string): boolean {
  const last = g.nations[foe]?.lastFight ?? {};
  for (const d of Object.keys(last)) if ((d === nat || allied(g, nat, d)) && g.turn - last[d] <= GOV.directives.hold.fresh) return true;
  return false;
}

/**
 * A threatened province of the nation's own worth an order to hold: a city, a capital, a fortress or an oilfield
 * with troops in it and the enemy at its gates, or an enemy on the move who could bring a real attack against it:
 * the attack card as his staff would read it, with his corps in reach brought up, reads `hold.threat` or better.
 * An enemy who has sat still for months is not about to come. A place with nobody in it, a town under siege with
 * no garrison, or one the enemy outnumbers past all help (`hold.lost`) is a lost cause and is not asked for.
 */
function toHold(g: Game, nat: string): { r: Region; odds: number; by: string | null } | null {
  const H = GOV.directives.hold;
  let best: { r: Region; odds: number; by: string | null } | null = null, score = 0;
  const men = (r: Region): number => r.units.reduce((s, u) => s + (!u.dead && allied(g, u.nat, nat) ? u.divs * u.str : 0), 0);
  for (const r of g.regions) {
    if (r.ctrl !== nat) continue;
    const size = townSize(g, r);
    if (size !== 'city' && size !== 'capital' && !r.fort && !r.oil) continue;
    const shut = besieged(g, r), inTown = weight(garrisonOf(r));
    if (shut ? inTown <= 0 : men(r) + inTown <= 0) continue;
    const t = shut ? null : threatOdds(g, nat, r.id);
    if (!shut && !(t && t.odds >= H.threat && t.odds <= H.lost && onTheMove(g, t.by, nat))) continue;
    const s = r.vp * 10 + r.fac * 3 + (r.cap ? 40 : 0) + (r.oil ? 20 : 0) + (shut ? 15 : 0);
    if (s > score) { score = s; best = { r, odds: t?.odds ?? 0, by: shut ? besiegerOf(g, r) : t?.by ?? null }; }
  }
  return best;
}

/** Odds in words, as a staff would say them: "about 1.8 to 1", "about 3 to 1", "better than 10 to 1". */
function oddsWords(odds: number): string {
  if (odds >= 10) return 'better than 10 to 1';
  const x = odds >= 3 ? Math.round(odds * 2) / 2 : Math.round(odds * 10) / 10;
  return `about ${x} to 1`;
}

/** The buildings a nation has standing in the provinces it holds, level by level, with those its builders are at work on. */
function civicCount(g: Game, nat: string): number {
  let s = g.nations[nat].queue.reduce((k, b) => k + (b.what === 'civic' ? 1 : 0), 0);
  if (g.flags['cv:any']) for (const r of g.regions) if (r.ctrl === nat) for (const l of CIVIC.lines) s += civicLevel(g, l.id, r.id);
  return s;
}

/** The cheapest building the nation could order today, money apart: what, where and the price. */
function cheapestCivic(g: Game, nat: string): { label: string; town: string; cost: number } | null {
  let best: { label: string; town: string; cost: number } | null = null;
  for (const r of g.regions) {
    if (r.ctrl !== nat || r.owner !== nat) continue;
    for (const o of civicOffers(g, nat, r.id)) if ((o.why === null || o.poor) && (!best || o.cost < best.cost)) best = { label: o.label, town: townName(r), cost: o.cost };
  }
  return best;
}

/** The directive the cabinet would set now, or null if nothing fits. The dice are the nation's own for the month. */
function draw(g: Game, nat: string, st: GovState): Directive | null {
  const D = GOV.directives, n = g.nations[nat];
  const rng = new Rng(hashString(`gov:${g.seed}:${g.turn}:${nat}`));
  const base = { id: g.turn, nat, issued: g.turn };
  const wing = wingOf(st);
  const options: Directive[] = [];
  const takeJob = (t: Target, from: string, kind: 'take' | 'relief', title: string, lead: string, months: number, capital: number, money: number, corps?: Kind): Directive => {
    const r = g.regions[t.p.region], more = t.hard ? D.take.hard.months : 0, mul = t.hard ? D.take.hard.reward : 1;
    const due = g.turn + months + more;
    return { ...base, kind, from, region: r.id, due, capital: Math.round(capital * mul), money: Math.round(money * mul * 10) / 10, ...(corps ? { corps } : {}), ...(t.hard ? { hard: true } : {}), ...(t.again ? { again: true } : {}),
      title, text: `${lead}, by you or an ally, by the end of ${date(due)}. ${READS}${oddsWords(t.odds)}.${t.hard ? ' The place is strongly held: the cabinet knows it, and gives more time and a larger reward.' : ''}` };
  };
  if (n.atWar.length) {
    // An ally with its back to the wall asks for a blow at the enemy pressing it; one that was overrun asks to be freed.
    for (const a of Object.values(g.nations)) {
      if (a.id === nat || !a.alive || !allied(g, nat, a.id) || !a.atWar.length) continue;
      const lost = homeLost(g, a.id);
      const cap = a.capital >= 0 ? g.regions[a.capital] : null;
      if (lost < D.relief.pressed && !(cap && cap.ctrl === a.id && besieged(g, cap))) continue;
      const foe = pressing(g, a);
      if (!foe || !atWar(g, nat, foe)) continue;
      const overrun = lost >= D.relief.overrun || !cap || cap.ctrl !== a.id;
      const t = takeable(g, nat, st, (r) => r.ctrl === foe && (!overrun || r.owner === a.id))[0];
      if (!t) continue;
      const r = g.regions[t.p.region];
      options.push(overrun
        ? takeJob(t, a.id, 'relief', `Liberate ${townName(r)}`, `${theName(a, true)} was overrun by ${theName(g.nations[foe])}, and its government asks you to free ${townName(r)}`, D.relief.months, D.relief.capital, 0, D.relief.corps)
        : takeJob(t, a.id, 'relief', `Take the pressure off ${theName(a)}`, `${theName(a, true)} is hard pressed by ${theName(g.nations[foe])} and asks you to strike back: take ${townName(r)}`, D.relief.months, D.relief.capital, 0, D.relief.corps));
      break;
    }
    const h = toHold(g, nat);
    // With the hawks on top the cabinet orders a place held only once the enemy is at its gates.
    if (h && (wing !== 'hawks' || besieged(g, h.r))) {
      const shut = besieged(g, h.r);
      const coming = !shut && h.by ? ` ${theName(g.nations[h.by], true)} is on the move, and its corps in reach could attack the place at ${oddsWords(h.odds)}.` : '';
      options.push({ ...base, kind: 'hold', from: nat, region: h.r.id, due: g.turn + D.hold.months, capital: shut ? D.hold.siegeCapital : D.hold.capital, money: 0, tested: shut, seen: lastReport(g),
        title: `Hold ${townName(h.r)}`, text: `The enemy is at ${shut ? 'the gates of' : 'the approaches to'} ${townName(h.r)}.${coming} The war cabinet wants it held to the end of ${date(g.turn + D.hold.months)}.${shut ? '' : ' If the enemy never comes, the order is withdrawn.'}` });
    }
    const top = takeable(g, nat, st).slice(0, 3);
    if (top.length) {
      const t = top[rng.int(top.length)], r = g.regions[t.p.region], size = townSize(g, r);
      options.push(takeJob(t, nat, 'take', `Take ${townName(r)}`, t.again ? `The war cabinet asks again for ${townName(r)}: it is to be taken from ${theName(g.nations[r.ctrl])}` : `The war cabinet wants ${townName(r)} taken from ${theName(g.nations[r.ctrl])}`,
        D.take.months[size] ?? 4, D.take.capital[size] ?? 15, income(g, nat) * (D.take.money[size] ?? 0.25)));
    }
  }
  if (n.atWar.length && wing === 'doves') {
    // With the doves on top a worn army is to be spared: so many men and no more lost in battle while the order runs.
    const field = g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort');
    const divs = field.reduce((s, c) => s + c.divs, 0), strength = divs > 0 ? field.reduce((s, c) => s + c.divs * c.str, 0) / divs : 1;
    const allowed = Math.round((armyMen(g, nat) * D.spare.share) / 1000) * 1000;
    if (strength < D.spare.worn && allowed > 0) {
      options.push({ ...base, kind: 'spare', from: nat, target: allowed, lost: 0, seen: lastReport(g), due: g.turn + D.spare.months, capital: D.spare.capital, money: 0,
        title: 'Spare the army', text: `The army is worn: its corps stand at ${Math.round(strength * 100)}% of their strength. The war cabinet wants it spared: lose no more than ${allowed.toLocaleString('en-US')} men in battle to the end of ${date(g.turn + D.spare.months)}.` });
    }
  }
  if (!options.length || wing === 'doves' || (wing === 'balanced' && (st.lastKind === 'take' || st.lastKind === 'relief') && !options.some((o) => o.kind === 'relief'))) {
    // Nothing presses, or the doves have the table, or a balanced table has just asked for a town and no ally is calling
    // for help: the cabinet looks to the army, to trade and to the country. It asks for no more men than the treasury can pay for.
    const spec = specOf(g, 'inf'), now = armyCount(g, nat);
    const purse = Math.max(0, n.pp) + Math.max(0, income(g, nat) - upkeepTotal(g, nat)) * (D.raise.months - 1);
    const corps = spec ? Math.min(Math.ceil(Math.max(now * (D.raise.grow - 1), D.raise.least) / spec.divs), Math.floor((purse * 0.6) / corpsCost(g, nat, 'inf')), raisable(g, nat, spec.divs)) : 0;
    if (spec && corps >= 1) {
      const target = now + corps * spec.divs;
      options.push({ ...base, kind: 'raise', from: nat, target, due: g.turn + D.raise.months, capital: D.raise.capital, money: 0,
        title: 'Build up the army', text: `${n.atWar.length ? 'The' : `${PEACE}the`} war cabinet wants the army built up: bring it from ${now} to ${target} divisions, in the field or forming, by the end of ${date(g.turn + D.raise.months)}.` });
    }
    // Small jobs for a quiet time: a town built up, and eyes abroad. They are asked of a country at peace, or of one that
    // has fought no battle for `small.quiet` months, and never of a treasury that could not pay for the thing today.
    const spare = Math.min(purse * 0.6, Math.max(0, n.pp));
    const quiet = !n.atWar.length || quietMonths(g, nat) >= D.small.quiet;
    const site = quiet ? cheapestCivic(g, nat) : null;
    if (site && site.cost <= spare) {
      options.push({ ...base, kind: 'build', from: nat, target: civicCount(g, nat) + 1, due: g.turn + D.build.months, capital: D.build.capital, money: 0,
        title: 'Build up a town', text: `The war cabinet wants the country built up: order a new building in one of your towns by the end of ${date(g.turn + D.build.months)}. The cheapest today is the ${site.label.toLowerCase()} at ${site.town}, for ${moneyShown(site.cost)}.` });
    }
    if (quiet && agentsOf(g, nat).length < mostAgents(nat) && homeBase(g, nat) >= 0 && INTEL.agents.cost <= spare) {
      options.push({ ...base, kind: 'agent', from: nat, target: agentsOf(g, nat).length + 1, due: g.turn + D.agent.months, capital: D.agent.capital, money: 0,
        title: 'Recruit an agent', text: `The war cabinet wants eyes abroad: recruit an agent for the secret service by the end of ${date(g.turn + D.agent.months)}. One costs ${agentCost().toLocaleString('en-US')} to find and train.` });
    }
    const ctx = new Map();
    const partner = Object.values(g.nations).find((b) => b.id !== nat && b.alive && whyNoTrade(g, nat, b.id, ctx) === null);
    if (partner) {
      options.push({ ...base, kind: 'trade', from: nat, target: dealsOf(g, nat).length + 1, due: g.turn + D.trade.months, capital: D.trade.capital, money: Math.round(income(g, nat) * D.trade.money * 10) / 10,
        title: 'Sign a trade agreement', text: `The war cabinet wants the country's trade put to work: sign a new trade agreement by the end of ${date(g.turn + D.trade.months)}. ${theName(partner, true)} would sign one today.` });
    }
  }
  // Not the same kind of job twice running, where there is a choice. A war has many towns to take, but with nothing
  // to ask for except the same again the cabinet asks for nothing this time.
  // The wing on top shapes the choice. Hawks ask for a town whenever there is one to ask for, and in a quiet month for
  // the army before trade. Doves ask for a town only when nothing else fits, and for trade before the army.
  const attack = (o: Directive): boolean => o.kind === 'take' || o.kind === 'relief';
  let opts = options;
  if (wing === 'hawks') {
    const towns = opts.filter(attack);
    if (towns.length) return towns[rng.int(towns.length)];
    if (opts.some((o) => o.kind === 'raise')) opts = opts.filter((o) => o.kind !== 'trade' && o.kind !== 'build' && o.kind !== 'agent');
  } else if (wing === 'doves') {
    // A table of doves that finds nothing of its own to ask for asks for nothing: it does not send the army at a town for want of other business.
    const calm = opts.filter((o) => o.kind !== 'take');
    if (!calm.length) return null;
    opts = calm.some((o) => o.kind === 'trade') ? calm.filter((o) => o.kind !== 'raise') : calm;
  }
  const fresh = opts.filter((o) => o.kind !== st.lastKind);
  const pool = fresh.length ? fresh : opts.filter(attack);
  return pool.length ? pool[rng.int(pool.length)] : null;
}

/** How a job to build up the army opens in a country at peace: dropped from the words once it is at war. */
const PEACE = 'While the country is at peace, ';
/** How a town to take gives its odds on the day it is asked for: said in the past once that month is over, beside what the card reads today. */
const READS = 'With your corps in reach brought up, the attack card reads ';
const READ = 'When the order was written the attack card, with your corps in reach brought up, read ';

/** A directive's words as the player reads them today. */
function textShown(g: Game, n: Nation, d: Directive): string {
  if (d.kind === 'raise' && n.atWar.length && d.text.startsWith(PEACE)) return `T${d.text.slice(PEACE.length + 1)}`;
  if ((d.kind === 'take' || d.kind === 'relief') && g.turn > d.issued) return d.text.replace(READS, READ);
  return d.text;
}

/** Notes whether a town the player was told to hold has been attacked or besieged since the order came. */
function test(g: Game, d: Directive): void {
  if (d.kind === 'spare') { d.lost = (d.lost ?? 0) + menLostSince(g, d.nat, d.seen ?? -1); d.seen = lastReport(g); return; }
  if (d.kind !== 'hold' || d.tested) return;
  const r = g.regions[d.region ?? -1];
  if (!r) return;
  if (besieged(g, r)) d.tested = true;
  for (const rep of g.reports) if (rep.id > (d.seen ?? -1) && rep.region === r.id && allied(g, d.nat, rep.defNat)) d.tested = true;
  d.seen = lastReport(g);
}

/**
 * How a directive stands: done, failed, still open with months in hand, or void, because the town it names is
 * now held by a country the nation is not at war with, or because the enemy never came for a town it was told to hold.
 */
function standing(g: Game, d: Directive): 'done' | 'failed' | 'open' | 'void' {
  const late = g.turn > d.due;
  if (d.kind === 'raise') return armyCount(g, d.nat) >= (d.target ?? 0) ? 'done' : late ? 'failed' : 'open';
  if (d.kind === 'trade') return dealsOf(g, d.nat).length >= (d.target ?? 0) ? 'done' : late ? 'failed' : 'open';
  if (d.kind === 'build') return civicCount(g, d.nat) >= (d.target ?? 0) ? 'done' : late ? 'failed' : 'open';
  if (d.kind === 'agent') return agentsOf(g, d.nat).length >= (d.target ?? 0) ? 'done' : late ? 'failed' : 'open';
  if (d.kind === 'spare') return (d.lost ?? 0) > (d.target ?? 0) ? 'failed' : late ? 'done' : 'open';
  const r = g.regions[d.region ?? -1];
  if (!r) return 'void';
  const ours = allied(g, d.nat, r.ctrl);
  if (d.kind === 'hold') return !ours ? 'failed' : late ? (d.tested ? 'done' : 'void') : 'open';
  return ours ? 'done' : !atWar(g, d.nat, r.ctrl) ? 'void' : late ? 'failed' : 'open';
}

/** A small job: a town built up, an agent recruited. Missed, it costs a small flat price and is not counted with the directives ignored. */
export const smallJob = (d: Pick<Directive, 'kind'> | undefined): boolean => d?.kind === 'build' || d?.kind === 'agent';

/**
 * Months since the nation last fought a battle of its own, as attacker or defender, or since the newest of its wars
 * began, if that is later: a war just declared is no quiet time. Without end for a country at peace that never fought.
 */
export function quietMonths(g: Game, nat: string): number {
  let last = -Infinity;
  for (const o of Object.values(g.nations)) {
    if (o.id === nat) { for (const t of Object.values(o.lastFight)) last = Math.max(last, t); }
    else if (o.lastFight[nat] !== undefined) last = Math.max(last, o.lastFight[nat]);
  }
  // A war the campaign opened with has no day written down: it is taken as begun the day the government was formed.
  const opened = g.nations[nat].gov?.since ?? 0;
  for (const e of g.nations[nat].atWar) last = Math.max(last, g.flags[`war:${nat}:${e}`] ?? g.flags[`war:${e}:${nat}`] ?? opened);
  return g.turn - last;
}

/**
 * The trust a directive costs when it is missed, or set aside: more for each one ignored in a row, less under a
 * paymaster. A small job costs the same small price however many stand ignored.
 */
function missedCost(st: GovState, aside = false, d?: Directive): number {
  const M = GOV.directives.missed;
  let c = smallJob(d) ? GOV.directives.small.missed : Math.min(M.most, M.base + M.step * st.missed);
  for (const e of effectsOf(st)) c *= e.missed ?? 1;
  return aside ? c * M.aside : c;
}

/** The level the cabinet's trust settles at: lower for every directive ignored in a row, until one is carried out. */
function restOf(st: GovState): number {
  const M = GOV.directives.missed;
  // A table of doves is a calm one, and settles higher, for as long as no directive of theirs stands ignored.
  const calm = st.missed ? 0 : GOV.wings.doves.rest * Math.max(0, -leanOf(st));
  return GOV.trust.rest + calm - Math.min(M.restMost, M.rest * st.missed);
}

function close(g: Game, n: Nation, st: GovState, how: 'done' | 'failed' | 'aside' | 'void'): void {
  const d = st.dir!;
  const swing = GOV.forms[formOf(n.id)].swing;
  delete st.dir;
  st.lastKind = d.kind;
  st.next = g.turn + GOV.directives.every;
  const town = (d.kind === 'take' || d.kind === 'relief') && d.region !== undefined;
  if (how === 'void') {
    // Peace overtook the job, or the enemy never came: nothing is won or lost by it, and the cabinet thinks again soon.
    st.next = g.turn + GOV.directives.retry;
    note(g, st, `Directive withdrawn: ${d.title}.`);
    headline(g, 'info', d.kind === 'hold'
      ? `The directive is withdrawn: ${d.title}. The enemy did not come for it, and the war cabinet turns to other things. Nothing is won or lost.`
      : `The directive is withdrawn: ${d.title}. The place is no longer in an enemy's hands, and the war cabinet will think again.`, n.id, d.region);
    return;
  }
  if (how === 'done') {
    st.done++;
    st.missed = 0;
    if (town && st.asked) delete st.asked[d.region!];
    n.pc = clamp(n.pc + d.capital, 0, Math.max(n.pc, DIPLOMACY.pcCap));
    n.pp += d.money;
    const spec = d.corps ? specOf(g, d.corps) : undefined;
    if (d.corps && spec && n.capital >= 0 && g.regions[n.capital].ctrl === n.id) {
      n.queue.push({ id: g.nextBuild++, what: 'corps', kind: d.corps, divs: spec.divs, name: corpsName(n.id, d.corps, n.corpsNo++), region: n.capital, ordered: g.turn, eta: g.turn + 1, cost: 0 });
    }
    st.trust = clamp(st.trust + GOV.directives.success.trust * swing, 0, 100);
    (st.last ??= []).push({ label: 'A directive carried out', amount: GOV.directives.success.trust * swing });
    note(g, st, `Directive carried out: ${d.title}.`);
    headline(g, 'info', `Directive carried out: ${d.title}. The war cabinet's confidence in you rises, and it votes ${rewardWords(d)}.`, n.id, d.region);
    return;
  }
  // Set aside, the job costs less than failing it would, and the next comes on the day it would have.
  const small = smallJob(d);
  if (how === 'failed' && !small) st.blame = g.turn;
  const cost = missedCost(st, how === 'aside', d);
  if (how === 'aside') st.next = Math.max(g.turn + GOV.directives.retry, d.due + 1 + GOV.directives.every);
  if (town) (st.asked ??= {})[d.region!] = g.turn;
  // A small job left undone is a small matter: it is not counted with the directives ignored, and the price never climbs.
  if (!small) st.missed++;
  st.trust = clamp(st.trust - cost * swing, 0, 100);
  (st.last ??= []).push({ label: small ? (how === 'aside' ? 'A small job set aside' : 'A small job left undone') : how === 'aside' ? 'A directive set aside' : 'A directive not carried out', amount: -cost * swing });
  note(g, st, `Directive ${how === 'aside' ? 'set aside' : 'not carried out'}: ${d.title}.`);
  if (small) {
    headline(g, 'info', how === 'aside'
      ? `You set the directive aside: ${d.title}. It was a small matter, and the war cabinet does not count it against you.`
      : `The directive was not carried out: ${d.title}. It was a small matter: the war cabinet's confidence in you slips a little, and it is not counted with the directives you have ignored.`, n.id, d.region);
    return;
  }
  headline(g, 'info', how === 'aside'
    ? `You set the directive aside: ${d.title}. The war cabinet notes it${st.missed > 1 ? `, for the ${st.missed === 2 ? 'second' : st.missed === 3 ? 'third' : `${st.missed}th`} time running` : ''}.`
    : `The directive was not carried out: ${d.title}. The war cabinet's confidence in you falls${st.missed > 1 ? ', further than last time' : ''}.`, n.id, d.region);
}

/** Looks at the player's directive: carried out or failed, it is closed and its reward or its price paid. Called as his month opens and closes. */
export function judgeDirective(g: Game, nat: string): void {
  const st = govOf(g, nat), n = g.nations[nat];
  if (!st || !n.alive) return;
  if (st.dir) {
    test(g, st.dir);
    const how = standing(g, st.dir);
    if (how !== 'open') close(g, n, st, how);
  }
  judgeVow(g, n, st);
  watchStood(g, n, st);
}

/** Why the player cannot set his directive aside, or null if he can. */
export function whyNotSetAside(g: Game, nat: string): string | null {
  const st = seen(g, nat);
  return st?.dir ? null : 'There is no directive to set aside.';
}

/** The player tells the cabinet the job will not be done. It costs half what failing it would, at once, and the next comes when it would have. */
export function setAside(g: Game, nat: string): boolean {
  const st = govOf(g, nat);
  if (!st?.dir) return false;
  close(g, g.nations[nat], st, 'aside');
  return true;
}

// ---- Call your shot: the player's own promise --------------------------------------------------

const round1 = (x: number): number => Math.round(x * 10) / 10;
const monthsWord = (k: number): string => (k === 1 ? 'a month' : `${k} months`);

/** How far every province lies from the ground the nation holds or its troops stand on: 0 there, 1 beside it, to the promise's reach. */
function depths(g: Game, nat: string): Map<number, number> {
  const out = new Map<number, number>();
  let ring: number[] = [];
  for (const r of g.regions) if (r.ctrl === nat || r.units.some((c) => c.nat === nat && !c.dead)) { out.set(r.id, 0); ring.push(r.id); }
  for (let d = 1; d <= GOV.promise.reach && ring.length; d++) {
    const next: number[] = [];
    for (const i of ring) for (const j of g.regions[i].nb) if (!out.has(j)) { out.set(j, d); next.push(j); }
    ring = next;
  }
  return out;
}

/** What a promise would be worth, as the cabinet reckons it on the day. */
export interface PromiseTerms {
  region: number;
  town: string;
  /** Who holds it now. */
  holder: string;
  months: number;
  /** The last month it can be kept in: the months he allows himself count this one. */
  due: number;
  /** What the attack card reads against the town with the player's corps within the promise's reach brought up, the best of today and the last months; and how many provinces off the town lies (1 beside his ground or his troops). */
  odds: number;
  depth: number;
  /** How bold the cabinet reckons it: 'safe', 'fair', 'bold' or 'daring', and that in words. */
  band: string;
  bandName: string;
  /** Kept: the trust gained, political capital and money (as the rules count money). Broken: the trust lost. Taken back: the trust lost, less. */
  trust: number;
  capital: number;
  money: number;
  price: number;
  back: number;
  /** The reward in words, and the whole bargain in a sentence or two. */
  reward: string;
  text: string;
}

/**
 * The odds a promise is reckoned on: the best the attack card has read against the town for the player's corps
 * within the promise's whole reach, brought up, today or as any of the last months opened (`then`). A man who
 * marches his army back a province before he names his bet is still reckoned on where it stood.
 */
function promiseOdds(g: Game, nat: string, r: Region, deep = depths(g, nat)): Reading {
  const P = GOV.promise, depth = deep.get(r.id) ?? P.reach;
  let best: Reading = { odds: cardOdds(g, nat, r, P.reach, deep), depth, then: null };
  for (const m of seen(g, nat)?.reach ?? []) {
    const was = g.turn - m.t < P.memory ? m.odds[r.id] : undefined;
    if (was === undefined) continue;
    const at = m.depth?.[r.id] ?? depth;
    if (boldness(was, at) < boldness(best.odds, best.depth) - 1e-9) best = { odds: was, depth: at, then: m.t };
  }
  return best;
}

/** A reading of the card against a town: the odds, how deep the town lay beyond the player's ground, and the month it was taken in (null for today's). */
interface Reading { odds: number; depth: number; then: number | null }

/**
 * How bold a promise of a town is before the time allowed is counted: more the worse the odds, and more with every
 * province of depth. What stands on the way to a town beyond the front is not on the card, so its reading counts for
 * less with every province.
 */
function boldness(odds: number, depth: number): number {
  const P = GOV.promise, felt = odds * (P.deep[Math.min(depth, P.deep.length) - 1] ?? 1);
  return clamp(2 * Math.log2(P.fullOdds / Math.max(felt, 0.01)), 0, P.oddsMost) + (depth - 1) * P.perDepth;
}

/** As the player's month opens the cabinet notes what the attack card reads against every town he could promise: it remembers for `promise.memory` months. */
function noteReach(g: Game, nat: string, st: GovState): void {
  const P = GOV.promise;
  const kept = (st.reach ?? []).filter((m) => m.t !== g.turn && g.turn - m.t < P.memory);
  if (!g.nations[nat].atWar.length) { if (kept.length) st.reach = kept; else delete st.reach; return; }
  const odds: Record<string, number> = {}, depth: Record<string, number> = {}, deep = depths(g, nat);
  for (const [id, d] of deep) {
    const r = g.regions[id];
    if (d < 1 || !atWar(g, nat, r.ctrl)) continue;
    const o = cardOdds(g, nat, r, P.reach, deep);
    if (o > 0) { odds[id] = Math.round(o * 100) / 100; depth[id] = d; }
  }
  st.reach = [...kept, { t: g.turn, odds, depth }];
}

/**
 * Once the player's month has fully opened (the drafts in, the scouts' reports back) the cabinet notes its readings
 * again, so that the one it remembers for the month is the one his page shows him: the first is taken as the month
 * turns, before either, and read a fifth lower.
 */
export function noteReadings(g: Game): void {
  const nat = g.human, st = govOf(g, nat);
  if (nat && st && g.nations[nat].alive && st.reach?.some((m) => m.t === g.turn)) noteReach(g, nat, st);
}

function termsAt(g: Game, nat: string, r: Region, months: number, read = promiseOdds(g, nat, r)): PromiseTerms {
  const P = GOV.promise, swing = GOV.forms[formOf(nat)].swing;
  // The reading the cabinet goes by: the odds and the depth together, as they stood on the best day it remembers.
  const odds = read.odds, depth = read.depth;
  const score = Math.max(0, boldness(odds, depth) - (months - P.months[0]) * P.perMonth);
  const band = P.bands.find((b) => score < b.under) ?? P.bands[P.bands.length - 1];
  const size = P.size[townSize(g, r)] ?? 1;
  // Within a band a shorter day pays a little more: the longest is no longer always the right pick.
  const longest = P.months[P.months.length - 1], haste = 1 + (P.haste * (longest - months)) / Math.max(1, longest - P.months[0]);
  const capital = band.capital ? Math.max(1, Math.round(band.capital * size * haste)) : 0, money = round1(income(g, nat) * band.money * size * haste);
  const due = g.turn + months - 1, trust = round1(band.trust * swing * haste), price = round1(band.price * swing), back = round1(band.price * P.back * swing);
  const parts = [`the cabinet's trust rises ${trust}`];
  if (capital) parts.push(`it votes ${capital} political capital`);
  if (money) parts.push(`${moneyShown(money)} for the treasury`);
  const reward = parts.length > 1 ? `${parts.slice(0, -1).join(', ')} and ${parts[parts.length - 1]}` : parts[0];
  const oddsWord = odds >= 10 ? 'better than 10 to 1' : `${odds >= 1 ? round1(odds) : Math.round(odds * 100) / 100} to 1`;
  const where = depth <= 1 ? 'beside your ground or your troops' : `${depth === 2 ? 'two' : depth === 3 ? 'three' : 'four'} provinces from your ground and your troops`;
  const past = read.then !== null, when = !past ? '' : read.then === g.turn ? ', as it did when the month opened' : `, as it did in ${date(read.then!)}`;
  return {
    region: r.id, town: townName(r), holder: r.ctrl, months, due, odds: Math.round(odds * 100) / 100, depth, band: band.id, bandName: band.name,
    trust, capital, money, price, back, reward,
    text: `${band.name}. With your corps within ${P.reach} provinces brought up, the attack card reads ${oddsWord} against ${townName(r)}${when}. It ${past ? 'lay' : 'lies'} ${where}${past ? ' then' : ''}, and you allow yourself ${monthsWord(months)}, this one counted, to the end of ${date(due)}. Kept: ${reward}. Broken: its trust falls ${price}.`,
  };
}

/** What promising this town in this many months would be worth today, or null where no such promise can be reckoned. */
export function promiseTerms(g: Game, nat: string, region: number, months: number): PromiseTerms | null {
  const r = g.regions[region];
  if (!r || !g.nations[nat] || !atWar(g, nat, r.ctrl) || !GOV.promise.months.includes(months)) return null;
  const depth = depths(g, nat).get(r.id);
  return depth === undefined || depth < 1 ? null : termsAt(g, nat, r, months);
}

function whyNotTown(g: Game, nat: string, st: GovState, r: Region, depth: number | undefined): string | null {
  if (!atWar(g, nat, r.ctrl)) return allied(g, nat, r.ctrl) ? `${townName(r)} is in your side's hands already.` : `You are not at war with ${theName(g.nations[r.ctrl])}.`;
  if (st.dir && (st.dir.kind === 'take' || st.dir.kind === 'relief') && st.dir.region === r.id) return `${townName(r)} is the cabinet's own directive already: a promise would add nothing.`;
  if (besieged(g, r)) { const by = besiegerOf(g, r); if (by && (by === nat || allied(g, nat, by))) return `Your side stands round ${townName(r)} already: the cabinet expects it to fall, and a promise would impress nobody.`; }
  if (depth === undefined || depth < 1) return `${townName(r)} lies more than ${GOV.promise.reach} provinces beyond your ground and your troops: the cabinet will not hear of it yet.`;
  return null;
}

/**
 * Why the player cannot go to the cabinet with a promise now, or null if he can. With a town named, why not that
 * town; with months named too, why not in that time.
 */
export function whyNotPromise(g: Game, nat: string, region?: number, months?: number): string | null {
  const st = seen(g, nat);
  if (!st) return 'Only your own war cabinet takes your promises, in the grand campaign.';
  if (!g.nations[nat].atWar.length) return 'Only a country at war has towns to promise.';
  if (st.vow) return `You have a promise standing: ${st.vow.title}. Keep it, or take it back, before you make another.`;
  const wait = (st.vowNext ?? 0) - g.turn;
  if (wait > 0) return `The cabinet heard your last promise lately: it will hear another in ${monthsWord(wait)}.`;
  if (region === undefined) return null;
  const r = g.regions[region];
  if (!r) return 'There is no such place.';
  const why = whyNotTown(g, nat, st, r, depths(g, nat).get(r.id));
  if (why) return why;
  if (months !== undefined && !GOV.promise.months.includes(months)) return `A promise is for ${GOV.promise.months.slice(0, -1).join(', ')} or ${GOV.promise.months[GOV.promise.months.length - 1]} months.`;
  return null;
}

/** A town the player could promise, with the terms for each length of time he may allow himself. */
export interface PromiseTarget { region: number; town: string; size: TownSize; holder: string; odds: number; depth: number; terms: PromiseTerms[] }

/**
 * The towns the player could promise the cabinet today: enemy towns within reach of his ground or his troops, up to
 * three capitals first and then the nearest, the richest ahead. Any other town in reach can be promised by its
 * province (promiseTerms). Empty when he cannot make a promise now.
 */
export function promiseTargets(g: Game, nat: string | null = g.human, most = 8): PromiseTarget[] {
  const st = seen(g, nat);
  if (!st || !nat || whyNotPromise(g, nat) !== null) return [];
  const deep = depths(g, nat);
  const rows: { r: Region; depth: number; w: number }[] = [];
  for (const r of g.regions) {
    const depth = deep.get(r.id);
    if (depth === undefined || depth < 1 || whyNotTown(g, nat, st, r, depth) !== null) continue;
    const p = prizeOf(g, r.id, nat);
    if (!p) continue;
    rows.push({ r, depth, w: (p.frees ? 2000 : 0) + (p.capitalOf ? 1000 : 0) + Math.max(p.income, p.incomeNow / 2) + p.vp * 25 + (p.oil > 0 ? 60 : 0) });
  }
  // Capitals come first, the nearest ahead; then the towns beside him, the richest ahead, and so outward.
  const cap = (x: { w: number }): number => (x.w >= 1000 ? 0 : 1);
  rows.sort((a, b) => cap(a) - cap(b) || a.depth - b.depth || b.w - a.w || a.r.id - b.r.id);
  const caps = rows.filter((x) => cap(x) === 0).slice(0, 3), rest = rows.filter((x) => cap(x) === 1).slice(0, Math.max(0, most - caps.length));
  return [...caps, ...rest].map(({ r, depth }) => {
    const read = promiseOdds(g, nat, r, deep);
    const terms = GOV.promise.months.map((m) => termsAt(g, nat, r, m, read));
    return { region: r.id, town: townName(r), size: townSize(g, r), holder: r.ctrl, odds: terms[0].odds, depth, terms };
  });
}

/** The player gives the cabinet his word: this town, by you or an ally, in this many months. One promise at a time, and not often. */
export function promise(g: Game, nat: string, region: number, months: number): boolean {
  if (whyNotPromise(g, nat, region, months) !== null) return false;
  const t = promiseTerms(g, nat, region, months), st = govOf(g, nat);
  if (!t || !st) return false;
  st.vow = { region, issued: g.turn, due: t.due, months, band: t.band, odds: t.odds, depth: t.depth, trust: t.trust, capital: t.capital, money: t.money, price: t.price, title: `${t.town} by the end of ${date(t.due)}` };
  st.vowNext = g.turn + GOV.promise.every;
  note(g, st, `Your promise: ${st.vow.title}.`);
  headline(g, 'info', `You give the war cabinet your word: ${st.vow.title}. ${t.bandName}: kept, ${t.reward}; broken, its trust falls ${t.price}.`, nat, region);
  return true;
}

function bump(st: GovState, label: string, amount: number): void {
  st.trust = clamp(st.trust + amount, 0, 100);
  (st.last ??= []).push({ label, amount });
}

function closeVow(g: Game, n: Nation, st: GovState, how: 'kept' | 'broken' | 'back' | 'void'): void {
  const v = st.vow!;
  delete st.vow;
  if (how === 'void') {
    st.vowNext = g.turn;
    note(g, st, `Promise released: ${v.title}.`);
    headline(g, 'info', `Your promise is released: ${v.title}. The place is no longer in an enemy's hands, and the cabinet holds you to nothing.`, n.id, v.region);
    return;
  }
  if (how === 'kept') {
    st.kept = (st.kept ?? 0) + 1;
    // A man who names his own target and takes it is not idle, whatever else he has left undone. A safe promise
    // proves nothing of the kind: the town was about to fall anyway.
    if (v.band !== 'safe') st.missed = Math.max(0, st.missed - 1);
    n.pc = clamp(n.pc + v.capital, 0, Math.max(n.pc, DIPLOMACY.pcCap));
    n.pp += v.money;
    bump(st, 'A promise kept', v.trust);
    note(g, st, `Promise kept: ${v.title}.`);
    headline(g, 'info', `You kept your word: ${v.title}. The war cabinet's confidence in you rises${v.capital ? `, and it votes ${v.capital} political capital${v.money ? ` and ${moneyShown(v.money)} for the treasury` : ''}` : ''}.`, n.id, v.region);
    return;
  }
  const cost = how === 'back' ? round1(v.price * GOV.promise.back) : v.price;
  if (how === 'broken') st.broken = (st.broken ?? 0) + 1;
  bump(st, how === 'back' ? 'A promise taken back' : 'A promise broken', -cost);
  note(g, st, `Promise ${how === 'back' ? 'taken back' : 'broken'}: ${v.title}.`);
  headline(g, 'info', how === 'back'
    ? `You take back your word: ${v.title}. The war cabinet notes it.`
    : `Your word was not kept: ${v.title}. The war cabinet remembers what it was promised, and its confidence in you falls.`, n.id, v.region);
}

/** Looks at the player's promise: kept as soon as the town is in his side's hands, broken when its last month is out. */
function judgeVow(g: Game, n: Nation, st: GovState): void {
  const v = st.vow;
  if (!v) return;
  const r = g.regions[v.region];
  if (!r) { closeVow(g, n, st, 'void'); return; }
  if (allied(g, n.id, r.ctrl)) closeVow(g, n, st, 'kept');
  else if (!atWar(g, n.id, r.ctrl)) closeVow(g, n, st, 'void');
  else if (g.turn > v.due) closeVow(g, n, st, 'broken');
}

/** Why the player cannot take his promise back, or null if he can. */
export function whyNotTakeBack(g: Game, nat: string): string | null {
  return seen(g, nat)?.vow ? null : 'You have no promise standing.';
}

/** The player takes his promise back before its day: it costs a share of what breaking it would. */
export function takeBack(g: Game, nat: string): boolean {
  const st = govOf(g, nat);
  if (!st?.vow) return false;
  closeVow(g, g.nations[nat], st, 'back');
  return true;
}

// ---- The cabinet wants a head ------------------------------------------------------------------

const generalBy = (g: Game, id: number): General | undefined => g.generals.find((x) => x.id === id);

/**
 * The defeats a cabinet holds against a general: the battles he has lost since he last won one that could have been
 * won. A defence lost to an attack the card gave less than an even chance (under the day's even odds: about 1.2 to 1 in
 * the open, more against prepared positions), or an attack of his own thrown back at `head.winnable` to 1 or better
 * and better than even. A line the attacker was likelier than not to carry, and an attack he was sent into at long
 * odds, are not his fault.
 */
export const heldAgainst = (gen: General): LostBattle[] => (gen.rec.lost ?? []).filter((b) => (b.att ? b.odds >= Math.max(GOV.head.winnable, b.even ?? 1.2) : b.odds < (b.even ?? 1.2)));

const oddsNum = (x: number): string => `${x >= 10 ? Math.round(x) : Math.round(x * 10) / 10}`;

/** One lost battle as the cabinet tells it: "lost the defence of Kharkov in March 1943 to an attack at 1.3 to 1". */
function lostWords(g: Game, b: LostBattle): string {
  const place = g.regions[b.reg]?.name ?? 'the field';
  return b.att ? `was thrown back from ${place} in ${date(b.t)}, attacking at ${oddsNum(b.odds)} to 1` : `lost the defence of ${place} in ${date(b.t)} to an attack at ${oddsNum(b.odds)} to 1`;
}

/** The general the cabinet would name: the one with the most defeats running that could have been won, a commander in the field it has not been proved wrong about. */
function toBlame(g: Game, nat: string, st: GovState): { gen: General; why: string; lost: LostBattle[] } | null {
  const blamed = st.blame !== undefined && g.turn - st.blame <= 1;
  const need = blamed ? 1 : GOV.head.streak;
  let best: General | null = null, most = 0;
  for (const gen of g.generals) {
    if (gen.nat !== nat || gen.status !== 'active' || gen.corps < 0) continue;
    if (gen.traits.includes('vindicated') || st.stood?.general === gen.id) continue;
    const k = heldAgainst(gen).length;
    if (k < need) continue;
    if (!best || k > most || (k === most && (gen.rec.costly > best.rec.costly || (gen.rec.costly === best.rec.costly && gen.id < best.id)))) { best = gen; most = k; }
  }
  if (!best) return null;
  const lost = heldAgainst(best).slice(-3), told = lost.map((b) => lostWords(g, b));
  const said = told.length > 1 ? `${told.slice(0, -1).join(', ')} and ${told[told.length - 1]}` : told[0];
  return { gen: best, lost, why: `He ${said}. The cabinet holds that ${lost.length === 1 ? 'it was a battle' : 'these were battles'} he could have won${blamed ? ', and its directive was not carried out' : ''}.` };
}

/** The general the war cabinet is asking the player to dismiss, by id, or null: cheap enough to ask for every row of a list. */
export const headOf = (g: Game, nat: string | null = g.human): number | null => seen(g, nat)?.head?.general ?? null;

/** True while the war cabinet is asking for this general's dismissal. */
export function headAsked(g: Game, gen: General): boolean {
  return seen(g, gen.nat)?.head?.general === gen.id;
}

/** The player has dismissed the general the cabinet named: its trust recovers, and it carries what his popularity would have cost. */
export function headGiven(g: Game, gen: General, refund = 0): void {
  const st = govOf(g, gen.nat), n = g.nations[gen.nat];
  if (!st?.head || st.head.general !== gen.id) return;
  delete st.head;
  st.headNext = g.turn + GOV.head.every;
  n.pc += refund;
  bump(st, 'A general dismissed at the cabinet\'s wish', GOV.head.sacked * GOV.forms[formOf(n.id)].swing);
  note(g, st, `${gen.name} dismissed, as the cabinet asked.`);
  headline(g, 'info', `The war cabinet has its head: ${gen.name} goes, and its confidence in you recovers. What he knew goes with him.`, n.id);
}

/** Why the player cannot stand by a general, or null if the cabinet is asking for one. */
export function whyNotStandBy(g: Game, nat: string): string | null {
  return seen(g, nat)?.head ? null : 'The cabinet is asking for nobody\'s dismissal.';
}

/** The player refuses the cabinet its head. Its trust falls, and it watches the general: his next battle proves one of them right. */
export function standBy(g: Game, nat: string): boolean {
  const st = govOf(g, nat);
  const gen = st?.head ? generalBy(g, st.head.general) : undefined;
  if (!st?.head || !gen) return false;
  const H = GOV.head;
  delete st.head;
  st.headNext = g.turn + H.every;
  st.stood = { general: gen.id, until: g.turn + H.watch - 1, battles: gen.rec.battles, won: gen.rec.won };
  // The count starts afresh: the cabinet cannot come back for him over the same battles.
  delete gen.rec.lost;
  bump(st, 'You stood by a general', -H.refused * GOV.forms[formOf(nat)].swing);
  note(g, st, `You stood by ${gen.name}.`);
  headline(g, 'info', `You stand by ${gen.name}. The war cabinet does not hide its displeasure, and it will be watching his next battle.`, nat);
  return true;
}

/** The general the player stood by: a battle won vindicates him for the rest of the war, a battle lost first that he could have won proves the cabinet right. */
function watchStood(g: Game, n: Nation, st: GovState): void {
  const w = st.stood;
  if (!w) return;
  const gen = generalBy(g, w.general);
  if (!gen || !serving(gen)) { delete st.stood; return; }
  const swing = GOV.forms[formOf(n.id)].swing, H = GOV.head;
  if (gen.rec.won > w.won) {
    delete st.stood;
    if (!gen.traits.includes('vindicated')) gen.traits.push('vindicated');
    bump(st, 'A general vindicated', H.vindicated * swing);
    note(g, st, `${gen.name} vindicated.`);
    headline(g, 'info', `Vindicated: ${gen.name}, whose dismissal the war cabinet asked for, has won his battle. The cabinet admits it was wrong, its confidence in you rises, and he carries the mark for the rest of the war.`, n.id);
  } else if (heldAgainst(gen).length > 0) {
    delete st.stood;
    delete gen.rec.lost;
    bump(st, 'The general you stood by lost again', -H.wrong * swing);
    note(g, st, `${gen.name} lost again.`);
    headline(g, 'info', `${gen.name}, the general you stood by, has lost again. The war cabinet says it told you so.`, n.id);
  } else if (g.turn > w.until) delete st.stood;
}

/** Once a month: a demand left unanswered is taken as a refusal, and after defeats the cabinet may ask for a head. */
function headMonth(g: Game, n: Nation, st: GovState): void {
  const H = GOV.head;
  if (st.head) {
    const gen = generalBy(g, st.head.general);
    if (!gen || gen.status !== 'active') delete st.head;
    else if (g.turn > st.head.due) standBy(g, n.id);
  }
  if (st.head || !n.atWar.length || g.turn < (st.headNext ?? 0)) return;
  const c = toBlame(g, n.id, st);
  if (!c || !new Rng(hashString(`head:${g.seed}:${g.turn}:${n.id}`)).chance(H.chance)) return;
  st.head = { general: c.gen.id, issued: g.turn, due: g.turn + H.answer - 1, why: c.why, lost: c.lost };
  note(g, st, `The cabinet asks for the dismissal of ${c.gen.name}.`);
  headline(g, 'info', `The war cabinet wants a head: it asks you to dismiss ${c.gen.name}. ${c.why} Dismiss him and its confidence recovers; stand by him and it falls, unless his next battle proves the cabinet wrong.`, n.id);
}

// ---- The month ---------------------------------------------------------------------------------

/**
 * The start of the player's month: the cabinet reads the news and its trust moves, a cabinet without trust falls,
 * and the directive is judged or a new one set. Nothing happens for a nation the computer plays.
 */
export function govMonth(g: Game, nat: string): void {
  const st = govOf(g, nat), n = g.nations[nat];
  if (!st || !n.alive) return;
  const T = GOV.trust, form = GOV.forms[formOf(nat)];
  const cap = (x: number): number => clamp(x, -T.cap, T.cap);
  // The month's news.
  const held = heldBy(g, nat), home = homeHeld(g, nat), caps = capitalsHeld(g, nat);
  let wins = 0, defeats = 0;
  for (const rep of g.reports) {
    if (rep.id <= st.mark.report || rep.yielded) continue;
    const attWon = rep.level.includes('victory');
    if (rep.attNat === nat) { if (attWon) wins++; else defeats++; } else if (rep.defNat === nat) { if (attWon) defeats++; else wins++; }
  }
  // The news as each wing hears it: hawks make more of ground and battles won and of ground lost, doves of battles
  // lost and of the men they cost.
  const W = GOV.wings, lean = leanOf(st), hawk = Math.max(0, lean), dove = Math.max(0, -lean);
  const fallen = dove > 0 ? menLostSince(g, nat, st.mark.report) : 0, army = armyMen(g, nat) + fallen;
  const causes: [string, number][] = [
    ['Ground won', cap(Math.max(0, held - st.mark.held) * T.province) * (1 + W.hawks.won * hawk)],
    ['Home ground lost', -cap(Math.max(0, st.mark.home - home) * T.home) * (1 + W.hawks.lost * hawk)],
    ['Battles won', cap(wins * T.win) * (1 + W.hawks.wins * hawk)],
    ['Battles lost', -cap(defeats * T.defeat) * (1 + W.doves.defeats * dove)],
    ['Heavy losses', dove > 0 && army > 0 ? -cap(dove * W.doves.losses * Math.max(0, fallen / army - W.doves.spare) * 100) : 0],
    ['An enemy capital taken', Math.max(0, caps - st.mark.caps) * T.capitalTaken],
    ['The treasury in debt', n.pp < 0 ? -T.debt : 0],
    ['The capital under siege', n.capital >= 0 && g.regions[n.capital].ctrl === nat && besieged(g, g.regions[n.capital]) ? -T.ringed : 0],
  ];
  // What his ministers and decrees cost in trust is felt in full under every form of government.
  const own = govAdd(st, 'trust');
  const drift = (restOf(st) - st.trust) * T.drift;
  const news = causes.reduce((s, c) => s + c[1], 0);
  st.last = [...causes.map(([label, x]) => ({ label, amount: x * form.swing })), { label: 'Ministers and decrees', amount: own }, { label: drift < 0 ? 'Yesterday\'s good news fades' : 'Yesterday\'s bad news fades', amount: drift }].filter((c) => Math.abs(c.amount) >= 0.05);
  st.trust = clamp(st.trust + news * form.swing + own + drift, 0, 100);
  st.mark = { held, home, caps, report: lastReport(g) };
  // A decree that takes political capital to keep lapses a step when there is none left to pay for it.
  for (let guard = 0; guard < 4; guard++) {
    const net = pcIncome(nat) + civicSum(g, nat).pc + capitalOf(nat, st);
    if (net >= 0 || n.pc + net >= 0) break;
    const t = GOV.tracks.filter((x) => (stepAt(st, x).effects.capital ?? 0) < 0).sort((x, y) => (stepAt(st, x).effects.capital ?? 0) - (stepAt(st, y).effects.capital ?? 0))[0];
    if (!t) break;
    const was = stepAt(st, t);
    st.steps[t.id] = (st.steps[t.id] ?? 0) - 1;
    st.wait[`track:${t.id}`] = g.turn + GOV.decree.wait;
    note(g, st, `Lapsed for want of political capital: ${was.name}.`);
    headline(g, 'info', `The decree lapses: ${was.name.toLowerCase()}. There is no political capital left to hold the country to it, and the law falls back to ${stepAt(st, t).name.toLowerCase()}.`, nat);
  }
  // A cabinet that has had no trust in him for months on end falls.
  // (Trust is counted in whole points, as the card shows it.)
  // A player who has made his office secure is never brought down: the months are not counted at all.
  st.low = !officeSecure(g) && Math.floor(st.trust) <= form.fallsAt ? st.low + 1 : 0;
  if (st.low >= T.crisisMonths) {
    st.low = 0;
    st.seats = {};
    for (const t of GOV.tracks) if ((st.steps[t.id] ?? 0) > 0) st.steps[t.id]--;
    n.pc = 0;
    st.trust = T.afterCrisis;
    st.last = [];
    // The men a promise was made to are gone, and so is their grievance against a general.
    delete st.vow;
    const named = st.head ? generalBy(g, st.head.general) : undefined;
    if (named) delete named.rec.lost;
    delete st.head;
    note(g, st, 'The war cabinet fell.');
    headline(g, 'info', `The government falls: the war cabinet ${form.crisis}. Caretakers take every office, the newest decrees are withdrawn, and your political capital is spent.`, nat);
    wingNews(g, nat, st);
  }
  judgeDirective(g, nat);
  headMonth(g, n, st);
  noteReach(g, nat, st);
  if (!st.dir && g.turn >= st.next) {
    const d = draw(g, nat, st);
    if (d) {
      st.dir = d;
      note(g, st, `Directive: ${d.title}.`);
      headline(g, 'info', `A directive from ${d.from === nat ? 'the war cabinet' : theName(g.nations[d.from])}: ${d.text} The reward: ${rewardWords(d)}.`, nat, d.region);
    } else {
      // Nothing fits, or nothing but the same job again: the cabinet waits, and may ask for it next time.
      st.next = g.turn + GOV.directives.retry;
      delete st.lastKind;
    }
  }
}

// ---- What a panel reads ------------------------------------------------------------------------

export interface MinisterView { id: string; name: string; plus: string; minus: string; /** The wing he sits with: 'hawk', 'dove', or null for a caretaker. */ wing: 'hawk' | 'dove' | null }
/** Hawks and doves: how the table leans, and what that does. */
export interface WingsView {
  /** Ministers with each wing, and hawks less doves: from -4 to 4. */
  hawks: number;
  doves: number;
  lean: number;
  /** The wing that has the better of the table (at `at` points of lean), in a word and a sentence. */
  top: Wing;
  at: number;
  word: string;
  text: string;
  /** What the lean does to the cabinet's reading of the news, line by line. */
  effects: string[];
}
/** The player's promise as it stands. */
export interface PromiseView extends Vow {
  town: string;
  bandName: string;
  /** Months in hand, this one counted. */
  left: number;
  progress: string;
  reward: string;
  /** What taking it back costs in trust. */
  back: number;
}
/** The general the cabinet wants dismissed, and what each answer does. */
export interface HeadView {
  general: number;
  name: string;
  rank: string;
  /** His corps, or '' if he has none. */
  command: string;
  why: string;
  /** Months left to answer, this one counted: unanswered, the cabinet takes it as a refusal. */
  left: number;
  /** Trust gained by dismissing him, lost by standing by him; then what his next battle moves it by, won or lost. */
  gain: number;
  cost: number;
  vindicated: number;
  wrong: number;
  /** True if the army thinks well of him: his corps takes his going badly. */
  loved: boolean;
  /** His record: battles fought and won. And the defeats the cabinet holds against him, each in a sentence, with the odds it was fought at. */
  fought: number;
  won: number;
  battles: { when: string; where: string; odds: number; attack: boolean; text: string }[];
}
export interface SeatView {
  id: string; name: string;
  minister: MinisterView;
  /** Who else could hold the office, each with the reason he cannot be appointed now, or null. */
  options: (MinisterView & { why: string | null })[];
}
export interface StepView { id: string; name: string; text: string; cost: number; on: boolean; /** Why it cannot be decreed now; null for the step in force and for one that can be. */ why: string | null }
export interface TrackView { id: string; name: string; at: number; steps: StepView[] }
export interface DirectiveView extends Directive {
  /** Who set it, in words: "The war cabinet", "Britain". */
  by: string;
  /** Months in hand, this one counted. */
  left: number;
  /** How it stands, in a sentence. */
  progress: string;
  reward: string;
  /** What letting it lapse costs in trust, and what setting it aside costs: less. */
  price: number;
  /** A small job (a building, an agent): its price is flat, and missing it is not counted with the directives ignored. */
  small: boolean;
  asidePrice: number;
  /** The month the next directive would be set if this one were set aside today. */
  asideNext: number;
  /** The odds as they stand today, in a sentence: for a town to take, what the attack card reads with the corps in reach brought up; for a place to hold, what the enemy's corps in reach could attack it at. Null for other jobs. */
  oddsLine: string | null;
}
export interface GovView {
  nat: string;
  form: { id: FormId; name: string; blurb: string };
  /** True for the player's own government: the rest is filled in. */
  mine: boolean;
  /** The cabinet's trust in whole points, rounded down: every mark it is measured against (doubt, the fall) is counted the same way. */
  trust: number;
  trustWord: string;
  /** The level trust drifts back to when there is no news: lower while directives stand ignored. */
  rest: number;
  /** True while the cabinet doubts the player (trust under `capital.lowTrust`), and what it withholds, in a sentence; null otherwise. */
  doubts: boolean;
  withheld: string | null;
  /** The month this government was formed. */
  since: number;
  /** True where the player has made his office secure: the cabinet cannot fall, whatever its trust. */
  secure: boolean;
  /** At or under this for `crisisMonths` months running the cabinet falls; `low` is how many months it has been there. */
  fallsAt: number;
  crisisMonths: number;
  low: number;
  /** Political capital in hand, what comes in each month from every source, and the most that can be held. */
  capital: number;
  capitalMonth: number;
  capitalMost: number;
  appointCost: number;
  seats: SeatView[];
  tracks: TrackView[];
  directive: DirectiveView | null;
  /** Months until the next directive is set, when none is open. */
  nextIn: number;
  /** Hawks and doves. */
  wings: WingsView;
  /** The player's own promise to the cabinet, if one stands; why he cannot make one now, or null if he can; and promises kept and broken. */
  promise: PromiseView | null;
  promiseWhy: string | null;
  kept: number;
  broken: number;
  /** The general the cabinet wants dismissed, if it is asking; and the one the player stood by, while the cabinet watches him. */
  head: HeadView | null;
  stoodBy: { general: number; name: string; left: number } | null;
  done: number;
  missed: number;
  /** What the cabinet and the decrees come to, lever by lever, in words: "Income +4%". */
  sums: string[];
  /** What moved the cabinet's trust since it last read the news, cause by cause, to a tenth of a point. */
  moved: { label: string; amount: number }[];
  log: { turn: number; text: string }[];
}

const trustWord = (t: number, fallsAt: number): string => (Math.floor(t) <= fallsAt ? 'None left' : t < 35 ? 'Doubtful' : t < 55 ? 'Uneasy' : t < 75 ? 'Steady' : t < 90 ? 'Firm' : 'Complete');
const mv = (m: Minister): MinisterView => ({ id: m.id, name: m.name, plus: m.plus, minus: m.minus, wing: m.wing ?? null });
const pct = (x: number): number => Math.round(x * 100);

function wingsView(st: GovState | null): WingsView {
  const W = GOV.wings;
  let hawks = 0, doves = 0;
  if (st) for (const s of GOV.seats) { const w = ministerIn(st, s.id).wing; if (w === 'hawk') hawks++; else if (w === 'dove') doves++; }
  const lean = hawks - doves, top: Wing = lean >= W.top ? 'hawks' : lean <= -W.top ? 'doves' : 'balanced', k = Math.abs(lean);
  const effects = lean > 0
    ? [`Ground won counts ${pct(W.hawks.won * k)}% more with the cabinet`, `Battles won count ${pct(W.hawks.wins * k)}% more`, `Home ground lost counts ${pct(W.hawks.lost * k)}% more against you`]
    : lean < 0
      ? [`Battles lost count ${pct(W.doves.defeats * k)}% more against you`, `Losing more than ${pct(W.doves.spare)}% of the army's men in battle in a month costs trust`, `The cabinet's trust settles ${W.doves.rest * k} higher while no directive stands ignored`]
      : [];
  return {
    hawks, doves, lean, top, at: W.top,
    word: top === 'hawks' ? 'The hawks have the table' : top === 'doves' ? 'The doves have the table' : lean > 0 ? 'Leaning to the hawks' : lean < 0 ? 'Leaning to the doves' : 'Evenly balanced',
    text: top === 'hawks' ? 'The cabinet asks for a town whenever there is one to ask for, orders a place held only with the enemy at its gates, and in a quiet month wants the army built up before it wants trade.'
      : top === 'doves' ? 'The cabinet asks you to hold, to build up the army and the towns, to trade, to help an ally and to spare a worn army. It does not ask for towns: with nothing of its own to ask for, it asks for nothing.'
        : `Neither wing shapes the directives yet: it takes ${W.top} more ministers of one wing than of the other. Every minister tilts how the cabinet reads the news.`,
    effects,
  };
}
const LEVER_NAME: Record<Lever, string> = { income: 'Income', drafts: 'Replacements', cost: 'Price of new corps', research: 'Research', trade: 'Trade agreements', men: 'Men coming of age' };

/** The odds on a directive as they stand today, in a sentence. */
function oddsLineOf(g: Game, d: Directive): string | null {
  const r = g.regions[d.region ?? -1];
  if (!r) return null;
  if (d.kind === 'take' || d.kind === 'relief') {
    if (!atWar(g, d.nat, r.ctrl)) return null;
    const odds = directiveOdds(g, d.nat, r.id);
    return odds > 0 ? `With your corps in reach brought up, the attack card reads ${oddsWords(odds)} today.` : `None of your corps is within ${GOV.directives.take.reach === 2 ? 'two' : GOV.directives.take.reach} provinces of it today.`;
  }
  if (d.kind === 'hold') {
    if (besieged(g, r)) return null;
    const t = threatOdds(g, d.nat, r.id);
    return t ? `${theName(g.nations[t.by], true)} could attack it today at ${oddsWords(t.odds)} with its corps in reach.` : 'No enemy corps is within reach of it today.';
  }
  return null;
}

function progressOf(g: Game, d: Directive): string {
  if (d.kind === 'raise') return `The army stands at ${armyCount(g, d.nat)} of ${d.target} divisions, in the field or forming.`;
  if (d.kind === 'trade') { const k = dealsOf(g, d.nat).length; return `You have ${k === 0 ? 'no trade agreements' : k === 1 ? 'one trade agreement' : `${k} trade agreements`}: one more is asked for. Open Trade to sign it.`; }
  if (d.kind === 'build') return `You have ${civicCount(g, d.nat)} buildings standing or ordered: one more is asked for. Pick a town on the map and open its buildings.`;
  if (d.kind === 'agent') { const k = agentsOf(g, d.nat).length; return `You have ${k === 0 ? 'no agents' : k === 1 ? 'one agent' : `${k} agents`}: one more is asked for. Open the secret service to recruit one.`; }
  if (d.kind === 'spare') { const lost = (d.lost ?? 0) + menLostSince(g, d.nat, d.seen ?? -1); return `The army has lost ${lost.toLocaleString('en-US')} men in battle since the order came: the cabinet will stand for ${(d.target ?? 0).toLocaleString('en-US')}.`; }
  const r = g.regions[d.region!];
  if (d.kind === 'hold') return besieged(g, r) ? `${townName(r)} is under siege, and holds.` : d.tested ? `${townName(r)} has been attacked, and holds.` : `${townName(r)} is in your hands, and the enemy has not come for it yet.`;
  const mine = r.units.some((u) => !u.dead && allied(g, u.nat, d.nat));
  return mine && besieged(g, r) ? `Your side stands round ${townName(r)}: the town has still to fall.` : `${townName(r)} is held by ${theName(g.nations[r.ctrl])}.`;
}

/** A nation's government, for a panel: its form, and for the player his cabinet, decrees, political capital, the cabinet's trust and the directive. Null outside the grand campaign. */
export function government(g: Game, nat: string | null = g.human): GovView | null {
  if (!nat || !govRules(g) || !g.nations[nat]) return null;
  const id = formOf(nat), form = GOV.forms[id], n = g.nations[nat];
  const st = seen(g, nat);
  const empty: GovView = {
    nat, form: { id, name: form.name, blurb: form.blurb }, mine: false, trust: form.start, trustWord: trustWord(form.start, form.fallsAt), rest: GOV.trust.rest, fallsAt: form.fallsAt, crisisMonths: GOV.trust.crisisMonths, low: 0,
    doubts: false, withheld: null, since: 0, secure: officeSecure(g),
    capital: 0, capitalMonth: 0, capitalMost: DIPLOMACY.pcCap, appointCost: appointCost(nat), seats: [], tracks: [], directive: null, nextIn: 0, done: 0, missed: 0, sums: [], moved: [], log: [],
    wings: wingsView(null), promise: null, promiseWhy: null, kept: 0, broken: 0, head: null, stoodBy: null,
  };
  if (!st) return empty;
  const seats: SeatView[] = GOV.seats.map((s) => ({
    id: s.id, name: s.name, minister: mv(ministerIn(st, s.id)),
    options: GOV.ministers.filter((m) => (m.seat === s.id || m.seat === '*') && m.id !== ministerIn(st, s.id).id).map((m) => ({ ...mv(m), why: whyNotAppoint(g, nat, s.id, m.id) })),
  }));
  const tracks: TrackView[] = GOV.tracks.map((t) => {
    const at = st.steps[t.id] ?? 0;
    return { id: t.id, name: t.name, at, steps: t.steps.map((x, i) => ({ id: x.id, name: x.name, text: x.text, cost: x.cost ?? 0, on: i === at, why: i === at ? null : whyNotDecree(g, nat, t.id, i) })) };
  });
  const sums: string[] = [];
  for (const k of Object.keys(LEVER_NAME) as Lever[]) {
    const m = govMul(g, nat, k);
    if (Math.abs(m - 1) > 1e-9) sums.push(`${LEVER_NAME[k]} ${m > 1 ? '+' : '−'}${Math.abs(Math.round((m - 1) * 100))}%`);
  }
  const tr = govAdd(st, 'trust');
  if (Math.abs(tr) > 1e-9) sums.push(`Trust ${tr > 0 ? '+' : '−'}${Math.abs(Math.round(tr * 10) / 10)} a month`);
  const cm = capitalOf(nat, st);
  if (Math.abs(cm) > 1e-9) sums.push(`Political capital ${cm > 0 ? '+' : '−'}${Math.abs(Math.round(cm * 10) / 10)} a month`);
  const d = st.dir;
  const C = GOV.capital, doubts = doubted(st);
  const v = st.vow, vr = v ? g.regions[v.region] : undefined;
  const asked = st.head ? generalBy(g, st.head.general) : undefined, askedCorps = asked ? corpsOf(g, asked) : null;
  const stood = st.stood ? generalBy(g, st.stood.general) : undefined;
  const H = GOV.head, P = GOV.promise;
  return {
    ...empty, mine: true, trust: Math.floor(st.trust + 1e-9), trustWord: trustWord(st.trust, form.fallsAt), rest: restOf(st), low: st.low, since: st.since ?? 0,
    doubts, withheld: doubts ? `It withholds ${Math.round(C.lowShare * 100)}% of the political capital you would earn each month and ${Math.round(C.lowIncome * 100)}% of the treasury's income until its trust is back to ${C.lowTrust}.` : null,
    capital: Math.floor(n.pc), capitalMonth: Math.round((pcIncome(nat) + civicSum(g, nat).pc + cm) * 100) / 100, seats, tracks,
    directive: d ? { ...d, text: textShown(g, n, d), small: smallJob(d), oddsLine: oddsLineOf(g, d), by: d.from === nat ? 'The war cabinet' : theName(g.nations[d.from], true), left: Math.max(0, d.due - g.turn + 1), progress: progressOf(g, d), reward: rewardWords(d), price: Math.round(missedCost(st, false, d) * form.swing * 10) / 10, asidePrice: Math.round(missedCost(st, true, d) * form.swing * 10) / 10, asideNext: Math.max(g.turn + GOV.directives.retry, d.due + 1 + GOV.directives.every) } : null,
    nextIn: d ? 0 : Math.max(0, st.next - g.turn), done: st.done, missed: st.missed, sums,
    wings: wingsView(st),
    promise: v && vr ? {
      ...v, town: townName(vr), bandName: P.bands.find((b) => b.id === v.band)?.name ?? 'A promise', left: Math.max(0, v.due - g.turn + 1), back: round1(v.price * P.back),
      progress: besieged(g, vr) && vr.units.some((u) => !u.dead && allied(g, u.nat, nat)) ? `Your side stands round ${townName(vr)}: the town has still to fall.` : `${townName(vr)} is held by ${theName(g.nations[vr.ctrl])}.`,
      reward: [`the cabinet's trust rises ${v.trust}`, ...(v.capital ? [`${v.capital} political capital`] : []), ...(v.money ? [`${moneyShown(v.money)} for the treasury`] : [])].join(', '),
    } : null,
    promiseWhy: whyNotPromise(g, nat), kept: st.kept ?? 0, broken: st.broken ?? 0,
    head: st.head && asked ? {
      general: asked.id, name: asked.name, rank: rankOf(asked), command: askedCorps?.name ?? '', why: st.head.why, left: Math.max(0, st.head.due - g.turn + 1),
      gain: round1(H.sacked * form.swing), cost: round1(H.refused * form.swing), vindicated: round1(H.vindicated * form.swing), wrong: round1(H.wrong * form.swing), loved: popular(asked),
      fought: asked.rec.battles, won: asked.rec.won,
      battles: (st.head.lost ?? []).map((b) => ({ when: date(b.t), where: g.regions[b.reg]?.name ?? '', odds: b.odds, attack: b.att, text: `${b.att ? 'Attacked' : 'Defended'} ${g.regions[b.reg]?.name ?? 'the field'} in ${date(b.t)}, the attacker at ${oddsNum(b.odds)} to 1: ${b.att ? 'thrown back' : 'lost'}.` })),
    } : null,
    stoodBy: st.stood && stood ? { general: stood.id, name: stood.name, left: Math.max(0, st.stood.until - g.turn + 1) } : null,
    moved: (st.last ?? []).map((c) => ({ label: c.label, amount: Math.round(c.amount * 10) / 10 })).filter((c) => c.amount !== 0), log: [...st.log].reverse(),
  };
}
