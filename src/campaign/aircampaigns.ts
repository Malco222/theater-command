// Air campaigns (ruling 236.2; the design director's b545, b549, b550; Brian's ask of 18 November 2026: "i want the air
// campaign to be a big part of the game like the battle of britian and other nations. I want countries to really use
// their air forces").
//
// A campaign is a standing order to an air force: a named set of targets, its groups and escorts, by day or by night.
// It is flown every month until it is called off, down the same road as any raid (fly() in air.ts). What it adds is
// memory, and the memory belongs to the ground, not to whoever ordered the raid (b549): with the switch on, every raid
// counts.
//  - The battle for the sky over a province carries over from month to month: contested, held or lost. It is won when
//    the defender's aircraft in reach stay under b446's 1.5 to 1 for two months running (where the defender has radar
//    over its own ground, only a fight won counts: b446's guard), or at once when its fighters there are knocked out.
//    While it is held, the sky is the holder's for every battle, landing and raid below it (g.sky, as a won fight).
//  - The damage bombs do carries over, and comes back each month at the historian's rate for the target
//    (air-campaign-236): only a campaign that comes back before the repair is done keeps output down. Oil is hit at its
//    refineries and fuel plants; a city's works held down wear its country's will to fight, a little (b549).
//  - At the stops that follow the book, the historian's campaigns open and close on their months for every country the
//    player does not fly; at the others the computer-opponent builder's staff reads the same table as doctrine.
// The grand campaign only, with the switch g.flags.ac on. Off, nothing here is read or written.

import data from '../../data/campaign/aircampaigns.json';
import farFields from '../../data/campaign/far-fields.json';
import { FAR_STAGE } from './stage';
import { turnToDate } from '../core/ticket';
import type { AirCampaign, AirFleet, AirMission, AirReport, AirWarMemory, BombStock, CampaignKind, CampaignMonth, Game, Region, SkyFight, SkyState } from './types';
import {
  AIR, AIRWAR, afloat, airFleetsOf, airOverBattle, airRange, closeRules, closeStaffRules, cover, crowding, fieldRoom, guardsLand, km, kmTo, rebaseAir, whyNotBase, whyNotMission,
} from './air';
import { allied, atWar, headline, theWords } from './game';
import { hasTech } from './tech';
import { byHand } from './depth';
import { stopDef } from './history';
import { majorsOf } from './diplomacy';
import { townSize } from './towns';
import { grounded, weatherOn } from './weather';
import { wrecked } from './airversion';
import { beyondEurope } from './world';

interface Phase { from: number; to: number; regions: string[]; escort?: number; bombers?: boolean; aim?: 'oil' | 'rail'; night?: boolean; kind?: 'sky' }
/**
 * A row of the book. A row that flies carries its 'theatre', its 'share' of that theatre's front-line aircraft by date
 * and the record's 'bases' by date (b569(3) and (4), from the historian's air-shares-b569).
 */
interface BookRow { nat: string; kind: string; what: string; targets: string; time: 'day' | 'night' | 'both'; from: number; to: number; conf: string; note?: string; fly?: Phase[]; book?: string; theatre?: string; share?: [number, number][]; bases?: [number, string[]][] }
interface RepairRow { target: string; perMonth: number; dispersed?: number }

export const AC = data as unknown as {
  on?: boolean;
  sky: { months: number; keep: number; knocked: number; idle: number };
  stock: { most: number; least: number };
  use: { night: string; day: string; oilfield: string; oil: string };
  dispersal: { nat: string; from: number; cut: number; rows: string[] };
  will: { per: number; most: number };
  bands: { from: number; words: string }[];
  repair: Record<string, RepairRow>;
  book: BookRow[];
  split: Record<string, [number, Record<string, number>][]>;
  fields: { rows: { region: string; from: number; class: number }[] };
  radar: { tech: string; control: string; ground: number; need: number; record: ChainRow[]; night: Record<string, [number, number][]> };
  gunners: { night: number; ground: number; day: Record<string, [number, number][]>; box: { nats: string[]; alone: number; escorted: number }; b29: { nat: string; vs: string[]; from: number; ratio: number; regions: string[] } };
};
interface ChainRow { nat: string; from: number; name: string; by: string; ground: number; plural?: boolean; regions?: string[]; where?: string }

/** True where air campaigns are flown: the grand campaign, with the switch on. */
export const acOn = (g: Game): boolean => g.flags.open === 1 && g.flags.ac === 1;

/**
 * The far side's airfields on the record (far-fields.json, the historian's table; ruling 234, the design director's of
 * 04:47Z of 21 November 2026 on rider b645 (2)), by province. The record's war in the south was a chain of airfields,
 * each hop made to take a field for the next, and a chart that seeds fields only at capitals, factory towns and the
 * bases of air rows cannot make that chain. Read with the far side's switch on, whatever the air campaigns' switch
 * says: the fields stood whoever flew from them. With the far switch off the table is not read at all.
 */
const FAR_FIELDS = new Map<string, { from: number; class: number }[]>();
if (FAR_STAGE) for (const f of (farFields as unknown as { rows: { region: string; from: number; class: number }[] }).rows) FAR_FIELDS.set(f.region, [...(FAR_FIELDS.get(f.region) ?? []), f]);
/** The far side's table as it is read: empty with the far switch off. */
export const farFieldRows = (): { region: string; from: number; class: number }[] => [...FAR_FIELDS.entries()].flatMap(([region, l]) => l.map((f) => ({ region, from: f.from, class: f.class })));

/**
 * The record's airfields today's chart does not seed: the class of airfield a province has at least from its month,
 * 0 elsewhere. The far side's (far-fields.json) are read behind the far switch alone; the Mediterranean's (b574:
 * Rhodes, Sardinia, the Libyan fields) with the air campaigns' switch on, and always 0 with it off.
 */
export function recordField(g: Game, region: number): number {
  const name = g.regions[region]?.name;
  let out = 0;
  for (const f of FAR_FIELDS.get(name) ?? []) if (f.from <= g.turn) out = Math.max(out, f.class);
  if (!acOn(g)) return out;
  // (The Mediterranean's rows by province, sorted once: every airfield question asked the whole table.)
  if (!MED_FIELDS) { MED_FIELDS = new Map(); for (const f of AC.fields.rows) MED_FIELDS.set(f.region, [...(MED_FIELDS.get(f.region) ?? []), f]); }
  for (const f of MED_FIELDS.get(name) ?? []) if (f.from <= g.turn) out = Math.max(out, f.class);
  return out;
}
/** The Mediterranean's record fields (aircampaigns.json) by province, made on the first question with the air campaigns on. */
let MED_FIELDS: Map<string, { from: number; class: number }[]> | undefined;

/**
 * True when a new grand campaign opens with air campaigns flown (aircampaigns.json `on`; on as shipped: the design
 * director, 22 November 12:28Z, b623 and b636). The interfaces read it as a grand campaign starts, right after
 * startOpen, where the war runners set g.flags.ac with TC_AC=1. Scenarios and history campaigns never read it, and a
 * saved game keeps the switch it was started with: one saved without the flag plays on with air campaigns off.
 */
export const airCampaignsShipped = (): boolean => AC.on === true;

/** Turns the air campaigns on or off (a new grand campaign as shipped, or the test kitchen). The bridge calls it by name. */
export function setAirCampaigns(g: Game, on: boolean): void {
  if (on) g.flags.ac = 1;
  else delete g.flags.ac;
}

const mem = (g: Game): AirWarMemory => (g.ac ??= { campaigns: [], next: 1, sky: {}, stock: {} });
const open = (g: Game): AirCampaign[] => (g.ac ? g.ac.campaigns.filter((c) => c.off === undefined) : []);
const month = (t: number): string => turnToDate(t).label;

// ---- The battle for the sky ------------------------------------------------------------------------------------

/** The defender's side over a province, for a fight by `nat`: whoever holds it and their allies at war with `nat`. */
const foesOf = (g: Game, nat: string, region: number): Set<string> => new Set([g.regions[region].ctrl]);

/** b446's guard: the defender has radar over its own home ground, and numbers alone do not win the sky there. */
function radarGuard(g: Game, nat: string, region: number): boolean {
  const r = g.regions[region], home = r.owner;
  return !r.overseas && atWar(g, nat, home) && allied(g, home, r.ctrl) && !!g.nations[home]?.alive && hasTech(g, home, AIRWAR.sky.radar);
}

/**
 * True when the defender's airfields over a province are knocked out: it has fighters in reach, and every one of
 * them is down, its field wrecked or its sky closed, or worn to 'knocked' readiness on a field this side has bombed
 * (a fight for the sky flown at it) since last month. Fighters worn out on fields nobody bombed still come up.
 */
export function knockedOut(g: Game, region: number, nat: string): boolean {
  const holder = g.regions[region].ctrl;
  const wx = weatherOn(g);
  const bombed = new Set<number>();
  for (let i = g.airReports.length - 1; i >= 0; i--) {
    const r = g.airReports[i];
    if (r.turn < g.turn - 1) break;
    if (r.mission === 'superiority' && r.region >= 0 && (r.attNat === nat || allied(g, r.attNat, nat))) bombed.add(r.region);
  }
  let all = 0, fit = 0;
  for (const f of g.airFleets) {
    if (f.ftr <= 0 || f.close || !atWar(g, f.nat, nat) || !guardsLand(g, f, region, holder)) continue;
    const n = f.ftr * cover(kmTo(g, f, region), airRange(f, g).ftr) * crowding(g, f);
    if (n <= 0) continue;
    all += n;
    const down = (wx && grounded(g, f)) || wrecked(g, f) || (f.ready <= AC.sky.knocked + 1e-9 && bombed.has(f.base));
    if (!down) fit += n;
  }
  return all >= AIR.sky.least && fit < AIR.sky.least;
}

/** The numbers over a province: the side fighting for it against its defenders, as b446 counts them (airOverBattle). */
function numbers(g: Game, nat: string, region: number): { a: number; d: number } {
  return { a: airOverBattle(g, new Set([nat]), foesOf(g, nat, region), region), d: airOverBattle(g, foesOf(g, nat, region), new Set([nat]), region) };
}

/** Where a fight stands this month: months without a fight break the run, and a held sky left alone too long is contested again. */
function settled(s: SkyFight, turn: number): { state: SkyState; streak: number } {
  const gap = turn - s.turn;
  if (gap <= 1) return { state: s.state, streak: s.streak };
  return { state: s.state === 'held' && gap <= AC.sky.idle + 1 ? 'held' : 'contested', streak: 0 };
}

/**
 * A fight for the sky over a province has been flown (fly(), 'superiority'): the month's result is weighed and the
 * fight's state moves on. Several fights by one side in a month count as the month's best one. Returns the state.
 */
export function skyFought(g: Game, nat: string, region: number, won: boolean): SkyState {
  const M = mem(g);
  let s = M.sky[region];
  const ours = !!s && (s.by === nat || allied(g, s.by, nat));
  if (!s || !ours) {
    // A side that begins to fight for a sky someone else was fighting for begins its own fight.
    s = M.sky[region] = { by: nat, state: 'contested', streak: 0, turn: g.turn, since: g.turn, best: -2, was: { state: 'contested', streak: 0 } };
  } else if (s.turn !== g.turn) {
    s.was = settled(s, g.turn);
    s.best = -2;
    s.turn = g.turn;
  }
  const { a, d } = numbers(g, nat, region);
  const T = AIRWAR.sky.own, least = AIR.sky.least;
  // (Fields knocked out win the sky at once only where the side also has the numbers there: b446's 1.5 to 1.)
  const out = a >= least && a >= T * d && knockedOut(g, region, nat);
  const guard = radarGuard(g, nat, region);
  const res = out ? 2 : won || (!guard && a >= least && a >= T * d) ? 1 : d >= least && d >= T * a ? -1 : 0;
  if (res <= s.best) return s.state;
  s.best = res;
  const before = s.state;
  let { state, streak } = s.was;
  if (res === 2) { state = 'held'; streak = Math.max(1, streak); }
  else if (state === 'held') {
    if (res < 0 || a < AC.sky.keep * d) { state = 'contested'; streak = 0; }
  } else {
    streak = res === 1 ? Math.max(0, streak) + 1 : res === -1 ? Math.min(0, streak) - 1 : 0;
    state = streak >= AC.sky.months ? 'held' : streak <= -AC.sky.months ? 'lost' : 'contested';
  }
  s.state = state;
  s.streak = streak;
  if (state === 'held') {
    if (before !== 'held') s.won = g.turn;
    g.sky[region] = { nat, turn: g.turn };
  } else delete s.won;
  if (state !== before && state !== 'contested') skyNews(g, s, region, out);
  return state;
}

function skyNews(g: Game, s: SkyFight, region: number, out: boolean): void {
  const adj = g.nations[s.by].adj, place = g.regions[region].name, foe = g.nations[g.regions[region].ctrl]?.adj ?? 'enemy';
  const text = s.state === 'held'
    ? `The ${adj} air force has won the sky over ${place}: ${out ? `the ${foe} fighters there are knocked out on their fields` : `for ${AC.sky.months} months running the ${foe} air force could not match it there`}. It holds that sky for as long as it keeps the numbers and keeps flying over it.`
    : `The ${adj} air force has lost the battle for the sky over ${place}: for ${AC.sky.months} months running the ${foe} fighters there outnumbered it.`;
  headline(g, 'battle', text, s.by, region, { arm: 'air', ...(s.state === 'held' ? { won: s.by, lost: g.regions[region].ctrl } : { won: g.regions[region].ctrl, lost: s.by }) });
}

/** How the battle for the sky over a province stands now, or null where nobody is fighting for it. */
export function skyFight(g: Game, region: number): (SkyFight & { now: SkyState; run: number }) | null {
  const s = g.ac?.sky[region];
  if (!s || !g.nations[s.by]?.alive || !atWar(g, s.by, g.regions[region].ctrl)) return null;
  const n = settled(s, g.turn);
  return { ...s, now: n.state, run: n.streak };
}

/** Each month a held sky stays held while its holder keeps the numbers and keeps flying there; the sky is its holder's (g.sky). */
function skyMonth(g: Game): void {
  const M = g.ac;
  if (!M) return;
  for (const k of Object.keys(M.sky)) {
    const region = Number(k), s = M.sky[region];
    if (!g.nations[s.by]?.alive || !atWar(g, s.by, g.regions[region].ctrl) || g.turn - s.turn > AC.sky.idle + 3) { delete M.sky[region]; continue; }
    if (s.state !== 'held') continue;
    const { a, d } = numbers(g, s.by, region);
    if (g.turn - s.turn > AC.sky.idle + 1 || a < AIR.sky.least || a < AC.sky.keep * d) { s.state = 'contested'; s.streak = 0; delete s.won; continue; }
    g.sky[region] = { nat: s.by, turn: g.turn };
  }
}

// ---- Bombing with memory ---------------------------------------------------------------------------------------

const rate = (row: string, dispersed: boolean): number => {
  const r = AC.repair[row];
  return dispersed && r.dispersed !== undefined ? r.dispersed : r.perMonth;
};
const dispersedAt = (g: Game, victim: string, row: string): boolean =>
  victim === AC.dispersal.nat && g.turn >= AC.dispersal.from && AC.dispersal.rows.includes(row);

/** The month a raid on `victim` by `by` falls on its production: as raid() reckons it in air.ts. */
const landsIn = (g: Game, by: string, victim: string): number => (g.order.indexOf(by) < g.order.indexOf(victim) ? g.turn : g.turn + 1);

function addStock(g: Game, r: Region, kind: 'works' | 'oil', by: string, cut: number, rt: number): void {
  if (cut <= 0) return;
  const M = mem(g), k = `${r.id}:${kind}`, victim = r.ctrl, when = landsIn(g, by, victim);
  let s = M.stock[k];
  if (!s || s.victim !== victim) s = M.stock[k] = { region: r.id, kind, by, victim, v: 0, rate: rt, at: when - 1, land: {}, carry: 0, mended: 0, done: 0 };
  // What is down and on its way, at its own rate, and this raid's at its rate: the province comes back at their blend.
  const have = s.v + Object.values(s.land).reduce((x, y) => x + y, 0);
  s.rate = have + cut > 0 ? (have * s.rate + cut * rt) / (have + cut) : rt;
  s.land[when] = Math.min(AC.stock.most, (s.land[when] ?? 0) + cut);
  s.by = by;
  s.last = { turn: g.turn, by, cut: (s.last && s.last.turn === g.turn && s.last.by === by ? s.last.cut : 0) + cut };
  if (when === g.turn) s.done += cut;
}

/**
 * A raid on a province's works or oil has landed (fly(), 'bomb' or 'night'): what it stops goes into the province's
 * stock of damage, and comes back at the historian's rate for what was hit. Returns the share of the province's output
 * it stops this month: Germany's works were spread out from March 1944, and a day raid on them does less.
 */
export function bombLands(g: Game, by: string, r: Region, cut: number, night: boolean, aim: 'works' | 'oil' | 'rail'): number {
  // A nation bombed by night reaches for Fighter control (b569(2)): the month is kept for its staff's research.
  if (night) (mem(g).night ??= {})[r.ctrl] = g.turn;
  if (cut <= 0) return cut;
  const row = night ? AC.use.night : AC.use.day;
  const spread = dispersedAt(g, r.ctrl, row);
  const out = spread ? cut * AC.dispersal.cut : cut;
  if (r.fac > 0 && aim === 'works') addStock(g, r, 'works', by, out, rate(row, spread));
  if (r.oil) addStock(g, r, 'oil', by, out, rate(AC.use.oilfield, false));
  else if (aim === 'oil' && r.fac > 0) addStock(g, r, 'oil', by, out, rate(AC.use.oil, false));
  return out;
}

/** The month's repair, at the start of each month: what is still down from earlier raids, and what came back. */
function stockMonth(g: Game): void {
  const M = g.ac;
  if (!M) return;
  for (const k of Object.keys(M.stock)) {
    const s = M.stock[k], r = g.regions[s.region];
    // Ground that has changed hands takes its damage with it: the new holder's account starts afresh.
    if (r.ctrl !== s.victim) { delete M.stock[k]; continue; }
    s.mended = 0;
    while (s.at < g.turn - 1) {
      s.at++;
      s.v = Math.min(AC.stock.most, s.v * (1 - s.rate) + (s.land[s.at] ?? 0));
      delete s.land[s.at];
    }
    s.mended = s.v * s.rate;
    s.carry = s.v * (1 - s.rate);
    s.done = s.land[g.turn] ?? 0;
    if (s.carry < AC.stock.least && !Object.keys(s.land).length) delete M.stock[k];
  }
}

/** The share of one province's output (or oil) held down this month: what is still down from before, and this month's raids. */
export function heldDown(g: Game, r: Region, kind: 'works' | 'oil'): number {
  const s = acOn(g) ? g.ac?.stock[`${r.id}:${kind}`] : undefined;
  if (!s || s.victim !== r.ctrl) return 0;
  return Math.min(AC.stock.most, s.carry + (s.land[g.turn] ?? 0));
}

/**
 * The share of a nation's production still held down this month by the damage of earlier raids (this month's own
 * raids are counted where they always were, in g.raids): the hook in airIncomeLoss.
 */
export function stockLoss(g: Game, nat: string): number {
  if (!acOn(g) || !g.ac) return 0;
  let total = 0, down = 0;
  for (const r of g.regions) if (r.ctrl === nat) total += r.fac;
  if (total <= 0) return 0;
  for (const s of Object.values(g.ac.stock)) {
    if (s.kind !== 'works' || s.victim !== nat) continue;
    const r = g.regions[s.region];
    if (r.ctrl === nat) down += s.carry * r.fac;
  }
  return down / total;
}

/** The share of an oilfield's yield, or of a province's fuel plants, held down by bombing this month (the hook in resources.ts). */
export const oilHeld = (g: Game, r: Region): number => heldDown(g, r, 'oil');

/**
 * What the bombing of its cities has taken from a nation's will to fight (the hook in willLost, resources.ts): each
 * city or capital whose works are held down wears it by 'per' times the share held down, 'most' at most (b549: small).
 */
export function bombWorn(g: Game, nat: string): number {
  if (!acOn(g) || !g.ac) return 0;
  let w = 0;
  for (const s of Object.values(g.ac.stock)) {
    if (s.kind !== 'works' || s.victim !== nat) continue;
    const r = g.regions[s.region], size = townSize(g, r);
    if (size === 'city' || size === 'capital') w += AC.will.per * heldDown(g, r, 'works');
  }
  return Math.min(AC.will.most, w);
}

// ---- Radar by the tree (rider b553) ---------------------------------------------------------------------------

/**
 * The warning chain over a province with the air campaigns on (b553(a)): the record's two chains on their months at
 * every stop; at the stops that follow the book each other great power's on its record month, whatever its research;
 * at the other stops any nation's, the month it holds Early-warning radar. Only over ground its owner holds.
 */
export function chainAt(g: Game, r: Region, record: ChainRow[]): ChainRow | null {
  if (r.owner !== r.ctrl) return null;
  const nat = r.ctrl;
  const old = record.find((c) => c.nat === nat && c.from <= g.turn);
  if (old) return old;
  if (stopDef(g).book) {
    // A row may cover only part of the ground: the Freya stations the North Sea coast, the Redut stations two cities (b558(1)).
    const row = AC.radar.record.find((c) => c.nat === nat && c.from <= g.turn && (!c.regions || c.regions.includes(r.name)));
    return row && majorsOf(g).includes(nat) ? row : null;
  }
  if (!hasTech(g, nat, AC.radar.tech)) return null;
  const n = g.nations[nat];
  return { nat, from: g.turn, name: `the ${n?.adj ?? nat} radar chain`, by: n?.name ?? nat, ground: AC.radar.ground };
}

/**
 * The defending fighters the bombers' gunners bring down for each bomber the fighters bring down (rulings 237 b557 and
 * b559, the historian's historian/gunner-kills-b557.md), from the real losses, never the gunners' claims. The game
 * flies one bomber a nation, so the historian's kinds are read from what the game knows: night or day, a strike on
 * troops, the nation and month, whether an escort reached the target, and the B-29's targets.
 */
export function gunnerRatio(g: Game, nat: string, m: AirMission, target: number, night: boolean, escorted: boolean): number {
  const G = AC.gunners;
  if (night) return G.night;
  if (m === 'ground' || m === 'line') return G.ground;
  if (G.box.nats.includes(nat)) {
    const r = m === 'strike' || m === 'fleet' ? undefined : g.regions[target];
    if (r && nat === G.b29.nat && g.turn >= G.b29.from && G.b29.vs.includes(r.ctrl) && G.b29.regions.includes(r.name)) return G.b29.ratio;
    return escorted ? G.box.escorted : G.box.alone;
  }
  return rowAt(G.day[nat] ?? G.day._default, g.turn);
}

/**
 * What a staff whose ground is bombed by night researches first (b569(2)): Fighter control, once it holds Early-warning
 * radar, at stops 3 to 5, every nation alike (no bombed nation of the war failed to reach for it). Null otherwise: at
 * the stops that follow the book the night rows run by date whatever is researched.
 */
export function controlFirst(g: Game, nat: string): string | null {
  if (!acOn(g) || stopDef(g).book) return null;
  const at = g.ac?.night?.[nat];
  if (at === undefined || g.turn - at > AC.radar.need) return null;
  return hasTech(g, nat, AC.radar.tech) && !hasTech(g, nat, AC.radar.control) ? AC.radar.control : null;
}

/**
 * The tree cards' lines for the two radar steps (b553(d); the UI director's words, ui-director/words-radar-cards.md),
 * worked out from the numbers, for any nation's card: null for any other technology, or with the switch off.
 */
export function radarWords(g: Game, nat: string, id: string): string | null {
  if (!acOn(g)) return null;
  const record = AIR.radar.chains as ChainRow[], without = AIR.radar.without;
  if (id === AC.radar.tech) {
    if (stopDef(g).book) {
      // One clause a chain row, in order of its month, so the tense never changes as the months pass (b558).
      const rows = [...record.filter((c) => c.nat === nat), ...(majorsOf(g).includes(nat) ? AC.radar.record.filter((c) => c.nat === nat) : [])]
        .sort((a, b) => a.from - b.from);
      if (!rows.length) return null;
      return `Its warning chain follows history's, with or without this research: ${rows.map((c) => `over ${c.where ?? 'all its ground'} from ${month(c.from)}`).join(', ')}.`;
    }
    return `A warning chain over all the ground it holds: every fighter in reach gets up in time to meet a raid, where without one only ${Math.round(without * 10)} in 10 do, and fewer of its aircraft are caught on the ground.`;
  }
  if (id === AC.radar.control) {
    return stopDef(g).book
      ? 'Its night defence grows with the years as history\'s did, with or without this research.'
      : "Its fighters are steered onto the bombers in the dark, as well as each year's sets allow. Without this, its night defence stays as it was at the start of the war.";
  }
  return null;
}

/** What a defending fighter counts for by night (b553(b)): the controlled figure for a nation holding Fighter control. */
const rowAt = <T>(rows: [number, T][], t: number): T => {
  let v = rows[0][1];
  for (const [from, x] of rows) if (from <= t) v = x;
  return v;
};

/**
 * What a nation's fighters count for by night, as a share of their worth by day (b558(2)): the historian's row for the
 * nation, by date. At the stops that follow the book the row runs by date whatever is researched. At stops 3 to 5
 * Fighter control is the gate: without it the nation keeps its row's first figure; with it, the row's figure for the
 * month (an early Fighter control does not buy 1944's sets in 1941).
 */
export function nightFighters(g: Game, nat: string): number {
  const rows = AC.radar.night[nat] ?? AC.radar.night._default;
  return stopDef(g).book || hasTech(g, nat, AC.radar.control) ? rowAt(rows, g.turn) : rows[0][1];
}

// ---- Campaigns: the doors -------------------------------------------------------------------------------------

/** The mission a campaign's groups fly. */
const missionOf = (c: AirCampaign): 'superiority' | 'bomb' | 'night' => (c.kind === 'sky' ? 'superiority' : c.night ? 'night' : 'bomb');

/** The campaign a group flies in, if any. */
export function campaignOf(g: Game, f: AirFleet): AirCampaign | null {
  if (!acOn(g) || f.camp === undefined || !g.ac) return null;
  return g.ac.campaigns.find((c) => c.id === f.camp && c.off === undefined && (c.groups.includes(f.id) || c.escorts.includes(f.id))) ?? null;
}
/** True for a group its staff leaves alone: it flies in a campaign. */
export const inCampaign = (g: Game, f: AirFleet): boolean => campaignOf(g, f) !== null;

/** What a bombing raid by these groups aims at: its campaign's aim, or the works. */
export function aimOf(g: Game, fleets: AirFleet[]): 'works' | 'oil' | 'rail' {
  for (const f of fleets) { const c = campaignOf(g, f); if (c) return c.aim; }
  return 'works';
}

export interface CampaignOrder {
  name?: string;
  night?: boolean;
  escorts?: number[];
  aim?: 'works' | 'oil' | 'rail';
}

/** Why a nation cannot open this campaign, or null if it can. */
export function whyNotCampaign(g: Game, nat: string, kind: CampaignKind, targets: number[], groups: number[], o: CampaignOrder = {}): string | null {
  if (!acOn(g)) return 'Air campaigns are flown in the grand campaign.';
  const n = g.nations[nat];
  if (!n?.alive) return 'That country is no more.';
  if (!n.atWar.length) return 'The nation is at peace.';
  if (!targets.length) return 'Pick at least one target.';
  if (!groups.length) return 'Pick at least one air group.';
  for (const t of targets) {
    const r = g.regions[t];
    if (!r) return 'Pick a province.';
    if (!atWar(g, nat, r.ctrl)) return `${r.name} is not in enemy hands.`;
  }
  const fs: AirFleet[] = [];
  for (const id of [...groups, ...(o.escorts ?? [])]) {
    const f = g.airFleets.find((x) => x.id === id);
    if (!f || f.nat !== nat) return 'Pick your own air groups.';
    if (afloat(f)) return `${f.name} flies from the carriers: the fleet's orders take it.`;
    if (inCampaign(g, f)) return `${f.name} already flies in a campaign.`;
    fs.push(f);
  }
  const main = fs.slice(0, groups.length);
  if (kind === 'bomb' && !main.some((f) => f.bmr > 0)) return 'A bombing campaign needs bombers.';
  if (kind === 'bomb' && o.night && o.escorts?.length) return 'Fighters do not fly with the night bombers.';
  if (kind === 'sky' && !main.some((f) => f.ftr > 0)) return 'A fight for the sky needs fighters.';
  const m = kind === 'sky' ? 'superiority' : kind === 'support' ? 'superiority' : o.night ? 'night' : 'bomb';
  if (kind !== 'support' && !main.some((f) => targets.some((t) => whyNotMission(g, f, m, t) === null))) return 'None of its groups can reach those targets.';
  return null;
}

const nameFor = (g: Game, nat: string, kind: CampaignKind, targets: number[], night: boolean): string => {
  const place = g.regions[targets[0]].name + (targets.length > 1 ? ` and ${targets.length - 1} more` : '');
  return theWords(kind === 'sky' ? `The battle for the sky over ${place}` : kind === 'support' ? `Support for the offensive at ${place}` : `The bombing of ${place}${night ? ' by night' : ''}`);
};

/** Opens a campaign: its groups fly it from this month on, until it is called off. Null if it cannot be opened. */
export function openCampaign(g: Game, nat: string, kind: CampaignKind, targets: number[], groups: number[], o: CampaignOrder = {}): AirCampaign | null {
  if (whyNotCampaign(g, nat, kind, targets, groups, o)) return null;
  return addCampaign(g, nat, kind, targets, groups, o);
}

function addCampaign(g: Game, nat: string, kind: CampaignKind, targets: number[], groups: number[], o: CampaignOrder, book?: number): AirCampaign {
  const M = mem(g);
  const night = kind === 'bomb' && !!o.night;
  const c: AirCampaign = {
    id: M.next++, nat, kind, name: o.name ?? nameFor(g, nat, kind, targets, night), targets: [...targets], next: 0,
    groups: [...groups], escorts: night ? [] : [...(o.escorts ?? [])], night, aim: o.aim ?? 'works', since: g.turn, log: [],
    ...(book !== undefined ? { book } : {}),
  };
  M.campaigns.push(c);
  for (const id of [...c.groups, ...c.escorts]) { const f = g.airFleets.find((x) => x.id === id); if (f) f.camp = c.id; }
  return c;
}

/** Ends a campaign: its groups go back on patrol. */
export function callOff(g: Game, id: number, why = 'You called it off.'): boolean {
  const c = g.ac?.campaigns.find((x) => x.id === id && x.off === undefined);
  if (!c) return false;
  endCampaign(g, c, why);
  return true;
}

function release(g: Game, c: AirCampaign, f: AirFleet): void {
  if (f.camp !== c.id) return;
  delete f.camp;
  if (c.kind === 'support') delete f.close;
  if (f.escorts !== undefined && c.escorts.includes(f.id)) delete f.escorts;
  if (f.flew !== g.turn) { f.mission = 'support'; f.target = -1; }
}

function endCampaign(g: Game, c: AirCampaign, why: string): void {
  closeLog(g, c);
  c.off = g.turn;
  c.why = why;
  for (const f of g.airFleets) if (f.camp === c.id) release(g, c, f);
  if (c.nat === g.human) headline(g, 'info', `${c.name} has ended after ${monthsWord(g.turn - c.since)}: ${why.charAt(0).toLowerCase()}${why.slice(1)}`, c.nat);
}

/** "August to September 1940" within a year, "August 1940 to May 1941" across years. */
const span = (a: number, b: number): string => {
  const A = turnToDate(a), B = turnToDate(b);
  return A.year === B.year ? `${A.label.split(' ')[0]} to ${B.label}` : `${A.label} to ${B.label}`;
};
const monthsWord = (n: number): string => (n <= 0 ? 'less than a month' : n === 1 ? 'a month' : `${n} months`);

// ---- Campaigns: the month ---------------------------------------------------------------------------------------

/** The groups a campaign holds that can still fly: its own, alive, with aircraft. */
const live = (g: Game, c: AirCampaign, ids: number[]): AirFleet[] =>
  ids.map((id) => g.airFleets.find((f) => f.id === id)).filter((f): f is AirFleet => !!f && f.nat === c.nat && f.ftr + f.bmr > 0 && !afloat(f));

/** A support campaign's raids behind the battle: an enemy province next to it that none of the nation's corps borders. */
function behind(g: Game, nat: string, t: number): number[] {
  const near = new Set<number>();
  for (const k of g.corps) if (k.nat === nat && !k.dead) { near.add(k.reg); for (const j of g.regions[k.reg].nb) near.add(j); }
  return g.regions[t].nb.filter((j) => atWar(g, nat, g.regions[j].ctrl) && !near.has(j));
}

const lostOf = (fs: AirFleet[]): number => fs.reduce((s, f) => s + f.lost.ftr + f.lost.bmr, 0);

/** Closes the month before's line of a campaign's log: what its groups lost since it opened. */
function closeLog(g: Game, c: AirCampaign): void {
  const e = c.log[c.log.length - 1] as (CampaignMonth & { ids?: number[]; base?: number }) | undefined;
  if (!e || e.ids === undefined) return;
  e.lost = Math.max(0, lostOf(e.ids.map((id) => g.airFleets.find((f) => f.id === id)).filter((f): f is AirFleet => !!f)) - (e.base ?? 0));
  delete e.ids;
  delete e.base;
}

/**
 * A nation's campaigns are flown (airBattles, before its groups are sorted into raids): each campaign sets its groups'
 * orders for the month, so the raid goes down the same road as any other.
 */
export function campaignOrders(g: Game, nat: string): void {
  if (!acOn(g) || !g.ac) return;
  for (const c of open(g)) {
    if (c.nat !== nat) continue;
    closeLog(g, c);
    const main = live(g, c, c.groups), esc = live(g, c, c.escorts);
    c.groups = main.map((f) => f.id);
    c.escorts = esc.map((f) => f.id);
    const book = c.book !== undefined;
    if (!main.length) { if (!book) endCampaign(g, c, 'Its air groups are gone.'); continue; }
    const targets = c.targets.filter((t) => g.regions[t] && atWar(g, nat, g.regions[t].ctrl));
    if (!targets.length) { if (!book) endCampaign(g, c, c.kind === 'support' ? 'Its offensive is over, the ground taken.' : 'Its targets are no longer in enemy hands.'); continue; }
    const ready = main.filter((f) => f.flew !== g.turn);
    if (!ready.length) continue;
    let target = -1;
    if (c.kind === 'sky') {
      // The first of its skies not yet held; once all are held, each in turn, so that none lapses.
      const notHeld = targets.filter((t) => { const s = skyFight(g, t); return !(s && s.now === 'held' && allied(g, s.by, nat)); });
      const pool = notHeld.length ? notHeld : targets;
      const flyable = pool.filter((t) => ready.some((f) => whyNotMission(g, f, 'superiority', t) === null));
      target = flyable.length ? (notHeld.length ? flyable[0] : flyable[c.next++ % flyable.length]) : -1;
      for (const f of ready) if (target >= 0 && whyNotMission(g, f, 'superiority', target) === null) { f.mission = 'superiority'; f.target = target; f.camp = c.id; }
    } else if (c.kind === 'bomb') {
      const m = missionOf(c);
      for (let k = 0; k < targets.length; k++) {
        const t = targets[(c.next + k) % targets.length];
        if (!ready.some((f) => f.bmr > 0 && whyNotMission(g, f, m, t) === null)) continue;
        target = t;
        c.next = (c.next + k + 1) % targets.length;
        break;
      }
      if (target >= 0) {
        let lead: AirFleet | null = null;
        for (const f of ready) if (f.bmr > 0 && whyNotMission(g, f, m, target) === null) { f.mission = m; f.target = target; f.camp = c.id; lead ??= f; }
        // Its escorts ride with the bombers, as far as their fighters reach (the followers in airBattles).
        if (lead && !c.night) for (const f of esc) if (f.flew !== g.turn) { f.mission = 'support'; f.target = -1; f.escorts = lead.id; f.camp = c.id; }
      }
    } else {
      // Support behind a named offensive: the fighters on call over its battles (close air support, where the switch is
      // on; otherwise the fight for the sky over the first of them), the bombers on the troops there and on the ground
      // behind them, the road into the battle.
      const sky = targets.find((t) => ready.some((f) => f.ftr > 0 && whyNotMission(g, f, 'superiority', t) === null)) ?? -1;
      target = targets[0];
      for (const f of ready) {
        f.camp = c.id;
        if (f.bmr <= 0 || f.kind === 'ftr') {
          if (byHand(g, nat, 'air') ? closeRules(g) : closeStaffRules(g)) { f.mission = 'support'; f.target = -1; f.close = true; delete f.escorts; }
          else if (sky >= 0 && whyNotMission(g, f, 'superiority', sky) === null) { f.mission = 'superiority'; f.target = sky; }
          continue;
        }
        const troops = targets.find((t) => whyNotMission(g, f, 'ground', t) === null);
        const rear = targets.flatMap((t) => behind(g, nat, t)).find((t) => whyNotMission(g, f, 'bomb', t) === null);
        const pick = (g.turn + f.id) % 2 === 0 ? troops ?? rear : rear ?? troops;
        if (pick !== undefined) { f.mission = troops === pick ? 'ground' : 'bomb'; f.target = pick; }
      }
    }
    const flying = [...main, ...esc].filter((f) => f.camp === c.id);
    const e: CampaignMonth & { ids: number[]; base: number } = {
      turn: g.turn, target, sent: flying.reduce((s, f) => s + f.ftr + f.bmr, 0), lost: 0, downed: 0, done: 0, ids: flying.map((f) => f.id), base: lostOf(flying),
    };
    c.log.push(e);
  }
}

/** After the month's raids: what each of a nation's campaigns did, from the raids its groups flew. */
export function campaignResults(g: Game, nat: string, reps: AirReport[]): void {
  if (!acOn(g) || !g.ac) return;
  for (const c of open(g)) {
    if (c.nat !== nat) continue;
    const e = c.log[c.log.length - 1];
    if (!e || e.turn !== g.turn) continue;
    const ids = new Set([...c.groups, ...c.escorts]);
    for (const r of reps) {
      if (!r.att.some((x) => x.id !== undefined && ids.has(x.id))) continue;
      e.downed += r.def.reduce((s, x) => s + x.lostFtr + x.lostBmr, 0);
      if (r.mission === 'superiority' && r.region >= 0) { const s = skyFight(g, r.region); if (s) e.sky = s.now; }
    }
    if (c.kind !== 'sky' && e.target >= 0) {
      for (const k of ['works', 'oil'] as const) {
        const s = g.ac.stock[`${e.target}:${k}`];
        if (s?.last && s.last.turn === g.turn && s.last.by === nat) e.done = Math.max(e.done, s.last.cut);
      }
    }
  }
}

// ---- The book: the historian's campaigns on their months --------------------------------------------------------

const phaseOf = (row: BookRow, turn: number): Phase | null => row.fly?.find((p) => p.from <= turn && turn <= p.to) ?? null;
const kindOf = (row: BookRow, p: Phase): CampaignKind => (p.kind === 'sky' || row.kind === 'sky' ? 'sky' : 'bomb');

/**
 * A theatre of a nation's air force (b569(3)): which of its groups belong to it, and the part of the kind it holds.
 * Germany's west, east and Mediterranean are the historian's split of the whole Luftwaffe by date; the RAF at home is
 * its groups on British home ground; America's Europe and Pacific are its groups on either side of the world; any
 * other nation's theatre is its whole air force.
 */
function theatreOf(g: Game, nat: string, th: string | undefined): { mine: (f: AirFleet) => boolean; part: number } {
  const side = th?.split('-')[1];
  const split = AC.split[nat];
  if (split && side) return { mine: () => true, part: rowAt(split, g.turn)[side] ?? 1 };
  if (th === 'UK-home') return { mine: (f) => { const r = g.regions[f.base]; return !!r && (r.first ?? r.owner) === 'UK' && !r.overseas; }, part: 1 };
  if (th === 'USA-eur') return { mine: (f) => !beyondEurope(f.base), part: 1 };
  if (th === 'USA-pac') return { mine: (f) => beyondEurope(f.base), part: 1 };
  return { mine: () => true, part: 1 };
}

/** The front-line aircraft of a kind in a nation's theatre: fighters, or bombers. */
function strength(g: Game, nat: string, th: { mine: (f: AirFleet) => boolean }, bombers: boolean): number {
  return airFleetsOf(g, nat).reduce((s, f) => s + (!afloat(f) && th.mine(f) ? (bombers ? (f.kind !== 'ftr' ? f.bmr : 0) : (f.kind !== 'bmr' ? f.ftr : 0)) : 0), 0);
}

/**
 * The groups a book row takes in its month (b569(3) and (4)): 'want' aircraft of the kind, those already in reach of
 * its targets first, the nearest first. Groups are whole, so it takes the nearest whole groups to its want: it always
 * takes one, and takes another only while that comes nearer the want than stopping does. A group out of reach is
 * moved, as the record's air forces moved, to the field its side holds nearest the record's bases for the month that
 * is in reach of the targets, one with room for it first; with no such field it stays where it is and the row does
 * without it. With none of the record's bases on the chart, no group is moved.
 */
function bookGroups(g: Game, nat: string, targets: number[], bombers: boolean, want: number, taken: Set<number>, th: { mine: (f: AirFleet) => boolean }, bases: number[]): AirFleet[] {
  const fs = airFleetsOf(g, nat).filter((f) => !afloat(f) && th.mine(f) && !taken.has(f.id) && (bombers ? f.bmr > 0 && f.kind !== 'ftr' : f.ftr > 0 && f.kind !== 'bmr'));
  const size = (f: AirFleet): number => (bombers ? f.bmr : f.ftr);
  const range = (f: AirFleet): number => (bombers ? airRange(f, g).bmr : airRange(f, g).ftr);
  const nearFrom = (at: number): number => Math.min(...targets.map((t) => km(g, at, t)));
  const near = (f: AirFleet): number => Math.min(...targets.map((t) => kmTo(g, f, t)));
  const toBase = (at: number): number => Math.min(...bases.map((b) => km(g, at, b)));
  const out: AirFleet[] = [];
  let got = 0;
  const enough = (f: AirFleet): boolean => want <= 0 || (got > 0 && got + size(f) / 2 > want);
  const can = fs.filter((f) => near(f) <= range(f)).sort((a, b) => near(a) - near(b) || a.id - b.id);
  for (const f of can) {
    if (enough(f)) return out;
    out.push(f);
    got += size(f);
  }
  if (!bases.length) return out;
  const rest = fs.filter((f) => !can.includes(f)).sort((a, b) => toBase(a.base) - toBase(b.base) || a.id - b.id);
  for (const f of rest) {
    if (enough(f)) break;
    const fields = g.regions.filter((r) => nearFrom(r.id) <= range(f) && whyNotBase(g, f, r, true) === null)
      .sort((a, b) => Number(fieldRoom(g, b.id, f)) - Number(fieldRoom(g, a.id, f)) || toBase(a.id) - toBase(b.id) || a.id - b.id);
    if (!fields.length || !rebaseAir(g, f, fields[0], true)) continue;
    out.push(f);
    got += size(f);
  }
  return out;
}

/** At the stops that follow the book: the historian's campaigns open and close on their months, for every country the player does not fly. */
function bookMonth(g: Game): void {
  if (!stopDef(g).book) return;
  const M = mem(g);
  const taken = new Set<number>();
  for (const c of M.campaigns) if (c.off === undefined && c.book === undefined) for (const id of [...c.groups, ...c.escorts]) taken.add(id);
  AC.book.forEach((row, i) => {
    const c = M.campaigns.find((x) => x.book === i && x.off === undefined);
    const p = phaseOf(row, g.turn);
    const n = g.nations[row.nat];
    const can = !!p && !!n?.alive && n.atWar.length > 0 && !byHand(g, row.nat, 'air');
    const targets = can ? p!.regions.map((nm) => g.byName[nm]?.id).filter((t): t is number => t !== undefined && atWar(g, row.nat, g.regions[t].ctrl)) : [];
    if (!can || !targets.length) {
      if (c) endCampaign(g, c, p ? 'Its targets are out of reach.' : 'Its months are over.');
      return;
    }
    const kind = kindOf(row, p!);
    const night = kind === 'bomb' && (p!.night ?? row.time === 'night');
    // The groups it takes this month: those in reach, up to its share, and for a fight for the sky its bombers on the fields.
    for (const f of airFleetsOf(g, row.nat)) if (f.camp === c?.id) delete f.camp;
    // Its share of its theatre this month, and the record's bases: the groups it needs are moved up near them (b569(4)).
    const share = row.share ? rowAt(row.share, g.turn) : 1;
    const th = theatreOf(g, row.nat, row.theatre);
    const bases = (row.bases ? rowAt(row.bases, g.turn) : []).map((nm) => g.byName[nm]?.id).filter((b): b is number => b !== undefined);
    // The share is of the theatre's whole front-line strength, as the historian gives it, whatever other rows hold
    // this month: a bombing row fills it with bombers, a fight for the sky with each kind it flies at that share.
    const ftr = strength(g, row.nat, th, false) * th.part, bmr = strength(g, row.nat, th, true) * th.part;
    const main = kind === 'sky'
      ? [...bookGroups(g, row.nat, targets, false, share * ftr, taken, th, bases), ...(p!.bombers ? bookGroups(g, row.nat, targets, true, share * bmr, taken, th, bases) : [])]
      : bookGroups(g, row.nat, targets, true, share * (ftr + bmr), taken, th, bases);
    for (const f of main) taken.add(f.id);
    // Its escort: 'escort' of the theatre's fighters (the feature builder's reading).
    const esc = kind === 'bomb' && !night && p!.escort ? bookGroups(g, row.nat, targets, false, p!.escort * ftr, taken, th, bases) : [];
    for (const f of esc) taken.add(f.id);
    const aim = p!.aim ?? 'works';
    if (!c) {
      if (!main.length) return;
      addCampaign(g, row.nat, kind, targets, main.map((f) => f.id), { name: row.what, night, escorts: esc.map((f) => f.id), aim }, i);
      return;
    }
    c.kind = kind;
    c.night = night;
    c.aim = aim;
    if (c.targets.join() !== targets.join()) { c.targets = targets; c.next = 0; }
    c.groups = main.map((f) => f.id);
    c.escorts = esc.map((f) => f.id);
    for (const f of [...main, ...esc]) f.camp = c.id;
  });
}

/** The air war's month, at the start of each month: the damage is repaired, held skies are kept, and the book's campaigns open and close. */
export function airWarMonth(g: Game): void {
  if (!acOn(g)) return;
  stockMonth(g);
  skyMonth(g);
  bookMonth(g);
}

// ---- The planner ------------------------------------------------------------------------------------------------

export interface CampaignRow {
  id: number;
  name: string;
  kind: CampaignKind;
  /** Which month of the campaign this is (1 the first), and the month it opened. */
  month: number;
  since: string;
  night: boolean;
  aim: 'works' | 'oil' | 'rail';
  targets: string[];
  groups: string[];
  escorts: string[];
  /** The battle for the sky over each target. */
  skies: { place: string; state: SkyState | 'not yet fought'; words: string }[];
  /** What the bombs keep down at each target: done this month, still down, and what came back this month (shares of its output). */
  damage: { place: string; kind: 'works' | 'oil'; done: number; down: number; back: number; words: string }[];
  /** Its losses: this month's and in all, what it sent, the share lost a raid, and where that sits among the record's bands. */
  losses: { month: number; all: number; sent: number; rate: number; band: string };
  /** What the nation's works build and its schools train a month, beside what the campaign loses. */
  replace: { built: number; trained: number; words: string };
  /** A line for the book's campaigns: whose plan it is. */
  book?: string;
}

const pc = (x: number): string => `${Math.round(x * 100)}%`;

/** The band of the record a raid's losses sit in (the historian's part 4), in plain words. */
export function lossBand(rate: number): string {
  return (AC.bands.find((b) => rate >= b.from) ?? AC.bands[AC.bands.length - 1]).words;
}

const row = (rows: number[][] | undefined, turn: number): number[] | null => {
  if (!rows) return null;
  let out: number[] | null = null;
  for (const r of rows) if (r[0] <= turn) out = r;
  return out;
};

/** The air page's campaigns: each of a nation's standing campaigns, as the planner shows it. */
export function campaignView(g: Game, nat: string): CampaignRow[] {
  if (!acOn(g) || !g.ac) return [];
  const out: CampaignRow[] = [];
  const P = AIR as unknown as { production: Record<string, number[][]>; pilots: { perMonth: Record<string, number[][]> } };
  const built = row(P.production[nat], g.turn), trained = row(P.pilots.perMonth[nat], g.turn);
  for (const c of open(g)) {
    if (c.nat !== nat) continue;
    const name = (id: number): string => g.airFleets.find((f) => f.id === id)?.name ?? '';
    const e = c.log[c.log.length - 1] as (CampaignMonth & { ids?: number[]; base?: number }) | undefined;
    const now = e && e.ids ? Math.max(0, lostOf(e.ids.map((id) => g.airFleets.find((f) => f.id === id)).filter((f): f is AirFleet => !!f)) - (e.base ?? 0)) : e?.lost ?? 0;
    const all = c.log.reduce((s, x) => s + (x === e ? now : x.lost), 0);
    const sent = e?.sent ?? 0, rate = sent > 0 ? now / sent : 0;
    const skies = c.kind === 'sky' || c.kind === 'support' ? c.targets.map((t) => {
      const s = skyFight(g, t), place = g.regions[t].name;
      if (!s || !allied(g, s.by, nat)) return { place, state: 'not yet fought' as const, words: theWords(`The battle for the sky over ${place} has not begun.`) };
      const left = AC.sky.months - s.run;
      const words = s.now === 'held' ? `Held since ${month(s.won ?? s.turn)}. It stays held while the air force keeps the numbers there and keeps flying over it.`
        : s.now === 'lost' ? `Lost: the enemy's fighters have outnumbered yours there for ${-s.run} months running.`
        : s.run > 0 ? `Contested: your numbers have held it for ${monthsWord(s.run)}. Hold it ${left === 1 ? 'one more month' : `${left} more months`} and the sky is yours.`
        : radarGuard(g, nat, t) ? 'Contested: the enemy has a warning chain over this ground, so numbers alone do not win it here; only a fight won counts.'
        : "Contested: to win it, outnumber the enemy aircraft that can reach it by half again, two months running, or knock out the enemy's fighters on their fields.";
      return { place, state: s.now, words };
    }) : [];
    const damage: CampaignRow['damage'] = [];
    if (c.kind !== 'sky') for (const t of c.targets) for (const k of ['works', 'oil'] as const) {
      const s = g.ac.stock[`${t}:${k}`];
      if (!s || s.victim !== g.regions[t].ctrl) continue;
      const place = g.regions[t].name, down = heldDown(g, g.regions[t], k), done = s.land[g.turn] ?? 0, back = s.mended;
      const what = k === 'oil' ? `${place}'s oil` : `${place}'s output`;
      damage.push({ place, kind: k, done, down, back, words: theWords(`${pc(down)} of ${what} is down${done > 0 ? `: ${pc(done)} from this month's raids, the rest from earlier raids not yet repaired` : ', all from earlier raids not yet repaired'}. ${pc(back)} has come back since last month. Only raids repeated before the repairs are done keep it down.`) });
    }
    const b = built ? (c.kind === 'sky' ? built[1] : built[2] ?? built[1]) : 0, tr = trained ? trained[1] : 0;
    out.push({
      id: c.id, name: c.name, kind: c.kind, month: g.turn - c.since + 1, since: month(c.since), night: c.night, aim: c.aim,
      targets: c.targets.map((t) => g.regions[t].name), groups: c.groups.map(name).filter(Boolean), escorts: c.escorts.map(name).filter(Boolean),
      skies, damage,
      losses: { month: now, all, sent, rate, band: sent > 0 ? lossBand(rate) : 'Nothing flown yet this month.' },
      replace: { built: b, trained: tr, words: `The works build about ${b} ${c.kind === 'sky' ? 'fighters' : 'bombers'} a month and the schools train about ${tr} pilots. This month the campaign lost ${now} aircraft.` },
      ...(c.book !== undefined ? { book: `History's campaign: ${AC.book[c.book].what}, ${span(AC.book[c.book].from, AC.book[c.book].to)}.` } : {}),
    });
  }
  return out;
}

/** The doctrine table, for a staff that reads it (the computer-opponent builder, stops 3 to 5): the rows of a nation flown at this month. */
export function doctrineOf(nat: string, turn: number): BookRow[] {
  return AC.book.filter((r) => r.nat === nat && r.from <= turn && turn <= r.to);
}
