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
// Computer nations keep a caretaker cabinet and no decrees, so nothing here moves their wars. The numbers are in
// data/campaign/government.json. The historical scenarios have none of this. Both interfaces read government()
// and give the orders through turn.ts.

import data from '../../data/campaign/government.json';
import { Rng, hashString } from '../core/rng';
import { turnToDate } from '../core/ticket';
import type { Directive, Game, GovState, Kind, Nation, Region } from './types';
import { BUDGET, corpsName, income, specOf } from './economy';
import { allied, armySize, atWar, headline, homeLost, theName } from './game';
import { DIPLOMACY, pcIncome } from './diplomacy';
import { civicSum } from './buildings';
import { besieged } from './siege';
import { prizesInReach, townName, townSize, type TownSize } from './towns';

export type FormId = 'democracy' | 'oneparty' | 'authoritarian';
/** What a minister or a decree can change: multipliers on income, replacements, the price of new corps, research and trade. */
export type Lever = 'income' | 'drafts' | 'cost' | 'research' | 'trade';
type Effects = Partial<Record<Lever | 'capital' | 'trust', number>>;
type Need = 'war' | 'long' | 'pressed';

interface Form { name: string; blurb: string; capital: number; swing: number; start: number; fallsAt: number; crisis: string }
interface Seat { id: string; name: string }
interface Minister { id: string; seat: string; name: string; plus: string; minus: string; effects: Effects }
interface Step { id: string; name: string; text: string; cost?: number; needs?: Need; effects: Effects }
interface Track { id: string; name: string; steps: Step[] }

export const GOV = data as unknown as {
  forms: Record<FormId, Form>;
  nations: Record<string, FormId>;
  capital: { lowTrust: number; lowShare: number; highTrust: number; highAdd: number };
  trust: { rest: number; drift: number; province: number; home: number; win: number; defeat: number; capitalTaken: number; debt: number; ringed: number; cap: number; crisisMonths: number; afterCrisis: number };
  appoint: { cost: number; wait: number };
  seats: Seat[];
  ministers: Minister[];
  decree: { wait: number };
  tracks: Track[];
  needs: { longMonths: number; pressedHome: number; pressedMonths: number };
  directives: {
    first: number; every: number; retry: number;
    success: { trust: number };
    missed: { base: number; step: number; most: number; rest: number; restMost: number };
    take: { months: Record<TownSize, number>; capital: Record<TownSize, number>; money: Record<TownSize, number> };
    hold: { months: number; capital: number; odds: number };
    raise: { months: number; grow: number; least: number; capital: number };
    relief: { months: number; capital: number; corps: Kind; pressed: number };
  };
};

const MINISTER: Record<string, Minister> = Object.fromEntries(GOV.ministers.map((m) => [m.id, m]));
const clamp = (x: number, a: number, b: number): number => Math.max(a, Math.min(b, x));
const date = (t: number): string => turnToDate(t).label;

/** True where governments are in play: the grand campaign. */
export const govRules = (g: Game): boolean => g.flags.open === 1;

/** A nation's form of government. */
export const formOf = (nat: string): FormId => GOV.nations[nat] ?? GOV.nations._default;

// ---- The state ---------------------------------------------------------------------------------

const heldBy = (g: Game, nat: string): number => g.regions.reduce((s, r) => s + (r.ctrl === nat ? 1 : 0), 0);
const homeHeld = (g: Game, nat: string): number => g.regions.reduce((s, r) => s + (r.owner === nat && r.ctrl === nat ? 1 : 0), 0);
const capitalsHeld = (g: Game, nat: string): number => g.regions.reduce((s, r) => s + (r.cap && r.owner !== nat && r.ctrl === nat ? 1 : 0), 0);
const lastReport = (g: Game): number => (g.reports.length ? g.reports[g.reports.length - 1].id : -1);

/** A government as it stands on its first day: caretakers in every office, no decrees, the first directive some months off. */
const fresh = (g: Game, nat: string): GovState => ({
  trust: GOV.forms[formOf(nat)].start, seats: {}, steps: {}, wait: {}, next: g.turn + GOV.directives.first, missed: 0, done: 0, low: 0, log: [],
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

/** Sets up the player's government as a grand campaign opens. */
export function openGovernment(g: Game): void {
  govOf(g, g.human);
}

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
  if (m.id !== 'caretaker' && g.nations[nat].pc < GOV.appoint.cost) return `It takes ${GOV.appoint.cost} political capital to carry the appointment: you have ${Math.floor(g.nations[nat].pc)}.`;
  return null;
}

/** Puts a minister in a seat of the war cabinet. A caretaker costs nothing. */
export function appoint(g: Game, nat: string, seat: string, minister: string): boolean {
  if (whyNotAppoint(g, nat, seat, minister)) return false;
  const st = govOf(g, nat)!, m = MINISTER[minister], s = GOV.seats.find((x) => x.id === seat)!;
  if (m.id !== 'caretaker') g.nations[nat].pc -= GOV.appoint.cost;
  st.seats[seat] = m.id;
  st.wait[`seat:${seat}`] = g.turn + GOV.appoint.wait;
  note(g, st, `${m.name} becomes ${s.name}.`);
  headline(g, 'info', `A new ${s.name}: ${m.name.toLowerCase()} takes office. ${m.plus}. ${m.minus}.`, nat);
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

/**
 * A threatened province of the nation's own worth an order to hold: a city, a capital, a fortress or an oilfield
 * with the enemy at its gates, or next door in greater strength than the troops who stand in it.
 */
function toHold(g: Game, nat: string): Region | null {
  let best: Region | null = null, score = 0;
  const men = (r: Region, foe: boolean): number => r.units.reduce((s, u) => s + (!u.dead && (foe ? atWar(g, u.nat, nat) : allied(g, u.nat, nat)) ? u.divs * u.str : 0), 0);
  for (const r of g.regions) {
    if (r.ctrl !== nat) continue;
    const size = townSize(g, r);
    if (size !== 'city' && size !== 'capital' && !r.fort && !r.oil) continue;
    const pressed = besieged(g, r) || r.nb.reduce((s, j) => s + men(g.regions[j], true), 0) > Math.max(1, men(r, false)) * GOV.directives.hold.odds;
    if (!pressed) continue;
    const s = r.vp * 10 + r.fac * 3 + (r.cap ? 40 : 0) + (r.oil ? 20 : 0) + (besieged(g, r) ? 15 : 0);
    if (s > score) { score = s; best = r; }
  }
  return best;
}

/** The directive the cabinet would set now, or null if nothing fits. The dice are the nation's own for the month. */
function draw(g: Game, nat: string, st: GovState): Directive | null {
  const D = GOV.directives, n = g.nations[nat];
  const rng = new Rng(hashString(`gov:${g.seed}:${g.turn}:${nat}`));
  const base = { id: g.turn, nat, issued: g.turn };
  const options: Directive[] = [];
  if (n.atWar.length) {
    const reach = prizesInReach(g, nat, 6);
    // An ally with its back to the wall asks for a blow at the enemy pressing it.
    for (const a of Object.values(g.nations)) {
      if (a.id === nat || !a.alive || !allied(g, nat, a.id) || !a.atWar.length) continue;
      const hard = homeLost(g, a.id) >= D.relief.pressed || (a.capital >= 0 && g.regions[a.capital].ctrl === a.id && besieged(g, g.regions[a.capital]));
      if (!hard) continue;
      const p = reach.find((x) => atWar(g, a.id, g.regions[x.region].ctrl));
      if (!p) continue;
      const r = g.regions[p.region], foe = g.nations[r.ctrl];
      options.push({ ...base, kind: 'relief', from: a.id, region: r.id, due: g.turn + D.relief.months, capital: D.relief.capital, money: 0, corps: D.relief.corps,
        title: `Take the pressure off ${theName(a)}`, text: `${theName(a, true)} is hard pressed and asks you to strike ${theName(foe)}: take ${townName(r)} by the end of ${date(g.turn + D.relief.months)}.` });
      break;
    }
    const h = toHold(g, nat);
    if (h) options.push({ ...base, kind: 'hold', from: nat, region: h.id, due: g.turn + D.hold.months, capital: D.hold.capital, money: 0,
      title: `Hold ${townName(h)}`, text: `The enemy is at ${besieged(g, h) ? 'the gates of' : 'the approaches to'} ${townName(h)}. The war cabinet wants it held to the end of ${date(g.turn + D.hold.months)}.` });
    const top = reach.slice(0, 3);
    if (top.length) {
      const p = top[rng.int(top.length)], r = g.regions[p.region], size = townSize(g, r);
      const months = D.take.months[size] ?? 4;
      options.push({ ...base, kind: 'take', from: nat, region: r.id, due: g.turn + months, capital: D.take.capital[size] ?? 15, money: Math.round(income(g, nat) * (D.take.money[size] ?? 0.25) * 10) / 10,
        title: `Take ${townName(r)}`, text: `The war cabinet wants ${townName(r)} taken from ${theName(g.nations[r.ctrl])} by the end of ${date(g.turn + months)}.` });
    }
  }
  if (!options.length) {
    const now = armySize(g, nat), target = Math.ceil(Math.max(now * D.raise.grow, now + D.raise.least));
    options.push({ ...base, kind: 'raise', from: nat, target, due: g.turn + D.raise.months, capital: D.raise.capital, money: 0,
      title: 'Bring the army up to strength', text: `${n.atWar.length ? 'The front is quiet' : 'The country is at peace'}, and the war cabinet wants it used: bring the army to ${target} divisions by the end of ${date(g.turn + D.raise.months)}.` });
  }
  // Not the same kind of job twice running, where there is a choice.
  const fresh = options.filter((o) => o.kind !== st.lastKind);
  const pool = fresh.length ? fresh : options;
  return pool[rng.int(pool.length)] ?? null;
}

/**
 * How a directive stands: done, failed, still open with months in hand, or void, because the town it names is
 * now held by a country the nation is not at war with.
 */
function standing(g: Game, d: Directive): 'done' | 'failed' | 'open' | 'void' {
  const late = g.turn > d.due;
  if (d.kind === 'raise') return armySize(g, d.nat) >= (d.target ?? 0) ? 'done' : late ? 'failed' : 'open';
  const r = g.regions[d.region ?? -1];
  if (!r) return 'void';
  const ours = allied(g, d.nat, r.ctrl);
  if (d.kind === 'hold') return !ours ? 'failed' : late ? 'done' : 'open';
  return ours ? 'done' : !atWar(g, d.nat, r.ctrl) ? 'void' : late ? 'failed' : 'open';
}

function missedCost(st: GovState): number {
  const M = GOV.directives.missed;
  return Math.min(M.most, M.base + M.step * st.missed);
}

/** The level the cabinet's trust settles at: lower for every directive ignored in a row, until one is carried out. */
function restOf(st: GovState): number {
  const M = GOV.directives.missed;
  return GOV.trust.rest - Math.min(M.restMost, M.rest * st.missed);
}

function close(g: Game, n: Nation, st: GovState, how: 'done' | 'failed' | 'aside' | 'void'): void {
  const d = st.dir!;
  const swing = GOV.forms[formOf(n.id)].swing;
  delete st.dir;
  st.lastKind = d.kind;
  st.next = g.turn + GOV.directives.every;
  if (how === 'void') {
    // Peace overtook the job: nothing is won or lost by it, and the cabinet thinks again soon.
    st.next = g.turn + GOV.directives.retry;
    note(g, st, `Directive withdrawn: ${d.title}.`);
    headline(g, 'info', `The directive is withdrawn: ${d.title}. The place is no longer in an enemy's hands, and the war cabinet will think again.`, n.id, d.region);
    return;
  }
  if (how === 'done') {
    st.done++;
    st.missed = 0;
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
  const cost = missedCost(st);
  st.missed++;
  st.trust = clamp(st.trust - cost * swing, 0, 100);
  (st.last ??= []).push({ label: how === 'aside' ? 'A directive set aside' : 'A directive not carried out', amount: -cost * swing });
  note(g, st, `Directive ${how === 'aside' ? 'set aside' : 'not carried out'}: ${d.title}.`);
  headline(g, 'info', how === 'aside'
    ? `You set the directive aside: ${d.title}. The war cabinet notes it${st.missed > 1 ? `, for the ${st.missed === 2 ? 'second' : st.missed === 3 ? 'third' : `${st.missed}th`} time running` : ''}.`
    : `The directive was not carried out: ${d.title}. The war cabinet's confidence in you falls${st.missed > 1 ? ', further than last time' : ''}.`, n.id, d.region);
}

/** Looks at the player's directive: carried out or failed, it is closed and its reward or its price paid. Called as his month opens and closes. */
export function judgeDirective(g: Game, nat: string): void {
  const st = govOf(g, nat), n = g.nations[nat];
  if (!st?.dir || !n.alive) return;
  const how = standing(g, st.dir);
  if (how !== 'open') close(g, n, st, how);
}

/** Why the player cannot set his directive aside, or null if he can. */
export function whyNotSetAside(g: Game, nat: string): string | null {
  const st = seen(g, nat);
  return st?.dir ? null : 'There is no directive to set aside.';
}

/** The player tells the cabinet the job will not be done. It costs what ignoring it would, at once, and the next comes sooner. */
export function setAside(g: Game, nat: string): boolean {
  const st = govOf(g, nat);
  if (!st?.dir) return false;
  close(g, g.nations[nat], st, 'aside');
  return true;
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
  const causes: [string, number][] = [
    ['Ground won', cap(Math.max(0, held - st.mark.held) * T.province)],
    ['Home ground lost', -cap(Math.max(0, st.mark.home - home) * T.home)],
    ['Battles won', cap(wins * T.win)],
    ['Battles lost', -cap(defeats * T.defeat)],
    ['An enemy capital taken', Math.max(0, caps - st.mark.caps) * T.capitalTaken],
    ['The treasury in debt', n.pp < 0 ? -T.debt : 0],
    ['The capital under siege', n.capital >= 0 && g.regions[n.capital].ctrl === nat && besieged(g, g.regions[n.capital]) ? -T.ringed : 0],
    ['Ministers and decrees', govAdd(st, 'trust')],
  ];
  const drift = (restOf(st) - st.trust) * T.drift;
  const news = causes.reduce((s, c) => s + c[1], 0);
  st.last = [...causes.map(([label, x]) => ({ label, amount: x * form.swing })), { label: drift < 0 ? 'Yesterday\'s good news fades' : 'Yesterday\'s bad news fades', amount: drift }].filter((c) => Math.abs(c.amount) >= 0.05);
  st.trust = clamp(st.trust + news * form.swing + drift, 0, 100);
  st.mark = { held, home, caps, report: lastReport(g) };
  // A cabinet that has had no trust in him for months on end falls.
  st.low = st.trust <= form.fallsAt ? st.low + 1 : 0;
  if (st.low >= T.crisisMonths) {
    st.low = 0;
    st.seats = {};
    for (const t of GOV.tracks) if ((st.steps[t.id] ?? 0) > 0) st.steps[t.id]--;
    n.pc = 0;
    st.trust = T.afterCrisis;
    st.last = [];
    note(g, st, 'The war cabinet fell.');
    headline(g, 'info', `The government falls: the war cabinet ${form.crisis}. Caretakers take every office, the newest decrees are withdrawn, and your political capital is spent.`, nat);
  }
  judgeDirective(g, nat);
  if (!st.dir && g.turn >= st.next) {
    const d = draw(g, nat, st);
    if (d) {
      st.dir = d;
      note(g, st, `Directive: ${d.title}.`);
      headline(g, 'info', `A directive from ${d.from === nat ? 'the war cabinet' : theName(g.nations[d.from])}: ${d.text} The reward: ${rewardWords(d)}.`, nat, d.region);
    } else st.next = g.turn + GOV.directives.retry;
  }
}

// ---- What a panel reads ------------------------------------------------------------------------

export interface MinisterView { id: string; name: string; plus: string; minus: string }
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
  /** What setting it aside, or letting it lapse, costs in trust. */
  price: number;
}
export interface GovView {
  nat: string;
  form: { id: FormId; name: string; blurb: string };
  /** True for the player's own government: the rest is filled in. */
  mine: boolean;
  trust: number;
  trustWord: string;
  /** The level trust drifts back to when there is no news: lower while directives stand ignored. */
  rest: number;
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
  done: number;
  missed: number;
  /** What the cabinet and the decrees come to, lever by lever, in words: "Income +4%". */
  sums: string[];
  /** What moved the cabinet's trust since it last read the news, cause by cause, to a tenth of a point. */
  moved: { label: string; amount: number }[];
  log: { turn: number; text: string }[];
}

const trustWord = (t: number, fallsAt: number): string => (t <= fallsAt ? 'None left' : t < 35 ? 'Doubtful' : t < 55 ? 'Uneasy' : t < 75 ? 'Steady' : t < 90 ? 'Firm' : 'Complete');
const mv = (m: Minister): MinisterView => ({ id: m.id, name: m.name, plus: m.plus, minus: m.minus });
const LEVER_NAME: Record<Lever, string> = { income: 'Income', drafts: 'Replacements', cost: 'Price of new corps', research: 'Research', trade: 'Trade agreements' };

function progressOf(g: Game, d: Directive): string {
  if (d.kind === 'raise') return `The army stands at ${Math.floor(armySize(g, d.nat))} of ${d.target} divisions.`;
  const r = g.regions[d.region!];
  if (d.kind === 'hold') return besieged(g, r) ? `${townName(r)} is under siege, and holds.` : `${townName(r)} is in your hands.`;
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
    capital: 0, capitalMonth: 0, capitalMost: DIPLOMACY.pcCap, appointCost: GOV.appoint.cost, seats: [], tracks: [], directive: null, nextIn: 0, done: 0, missed: 0, sums: [], moved: [], log: [],
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
  return {
    ...empty, mine: true, trust: Math.round(st.trust), trustWord: trustWord(st.trust, form.fallsAt), rest: restOf(st), low: st.low,
    capital: Math.floor(n.pc), capitalMonth: Math.round(Math.max(0, pcIncome(nat) + civicSum(g, nat).pc + cm) * 100) / 100, seats, tracks,
    directive: d ? { ...d, by: d.from === nat ? 'The war cabinet' : theName(g.nations[d.from], true), left: Math.max(0, d.due - g.turn + 1), progress: progressOf(g, d), reward: rewardWords(d), price: Math.round(missedCost(st) * form.swing * 10) / 10 } : null,
    nextIn: d ? 0 : Math.max(0, st.next - g.turn), done: st.done, missed: st.missed, sums,
    moved: (st.last ?? []).map((c) => ({ label: c.label, amount: Math.round(c.amount * 10) / 10 })).filter((c) => c.amount !== 0), log: [...st.log].reverse(),
  };
}
