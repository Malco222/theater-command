/**
 * The fall of nations and their return.
 *
 * Every nation that leaves the war goes on a roll (g.passings) that both interfaces read and announce: beaten in the
 * field, annexed by treaty, or divided by the terms of a peace. That part runs in every campaign and changes nothing
 * in the war itself.
 *
 * The return is a rule of the grand campaign only. A country beaten in the field comes back when its capital is in
 * the hands of an alliance member at war with its conqueror: it joins that alliance, takes back every province of
 * its own that its new allies hold, puts a small army in the field, and is run by the computer, or by the player if
 * it was the player's country. The numbers and the words are in data/campaign/liberation.json.
 */
import data from '../../data/campaign/liberation.json';
import { clamp } from '../core/math';
import { turnToDate } from '../core/ticket';
import { STACK, type Game, type Nation, type Passing, type Region } from './types';
import { addCorps, allied, armySize, atWar, bloc, headline, setWar, theName, type Bloc } from './game';
import { corpsName } from './economy';
import { homeOpinion } from './diplomacy';
import { enrol, small } from './league';

export const LIB = data as unknown as {
  gap: number;
  never: string[];
  army: { perVp: number; most: number; divs: number; str: number; org: number; xp: number };
  lines: { freed: string; freedAfter: string; freedSelf: string; sides: Record<Bloc, string> };
};

const fill = (line: string, vars: Record<string, string | number>): string => line.replace(/\{(\w+)\}/g, (_, k) => String(vars[k] ?? ''));

/** True where nations can come back: the grand campaign, until its last act dissolves the alliances. */
export const liberationRules = (g: Game): boolean => g.flags.open === 1 && !g.flags.dom;

/** The home provinces a country has on the map: its own ground, not its colonies. For France that includes what the armistice gave to Vichy. */
const homeOf = (X: Nation, r: Region): boolean => !r.overseas && (r.owner === X.id || (X.id === 'FRA' && r.owner === 'VIC' && r.vichy));

// ---- The roll -------------------------------------------------------------------------------

/**
 * Writes a nation's fall on the roll. Called at the moment it leaves the war, before its ground is shared out, so
 * that the count of provinces is the country as it stood.
 */
export function recordFall(g: Game, nat: string, by: string, how: Passing['how']): void {
  const X = g.nations[nat];
  if (!X) return;
  let provinces = 0;
  for (const r of g.regions) if (r.owner === nat && !r.overseas) provinces++;
  g.passings.push({
    id: g.passings.length + 1, turn: g.turn, week: g.week, kind: 'fell', nat, by, how, capital: X.capital, provinces,
    weeks: (g.turn - 1) * 4 + g.week, ...(g.human === nat ? { player: true } : {}),
  });
}

/** The roll for a campaign saved before it was kept: one entry for each nation that remembers its own fall. */
export function rollFromMemory(g: Game): Passing[] {
  const out: Passing[] = [];
  for (const X of Object.values(g.nations)) {
    if (X.alive || X.dormant || !X.fellOn) continue;
    let provinces = 0;
    for (const r of g.regions) if (homeOf(X, r)) provinces++;
    const how = X.fellOn.how ?? (X.capital >= 0 && g.regions[X.capital].owner !== X.id ? 'annexed' : 'beaten');
    out.push({ id: 0, turn: X.fellOn.turn, week: X.fellOn.week, kind: 'fell', nat: X.id, by: X.fellOn.to, how, capital: X.capital, provinces, weeks: (X.fellOn.turn - 1) * 4 + X.fellOn.week });
  }
  out.sort((a, b) => a.turn - b.turn || a.week - b.week || (a.nat < b.nat ? -1 : 1));
  out.forEach((p, i) => { p.id = i + 1; });
  return out;
}

/** The roll for one month, or the whole of it. */
export const passingsIn = (g: Game, turn?: number): Passing[] => (turn === undefined ? g.passings : g.passings.filter((p) => p.turn === turn));

// ---- Who can come back, and by whose hand ---------------------------------------------------

/** Why a fallen country can never return, or null if it can. A country that still stands has nothing to return from. */
export function whyNoReturn(g: Game, nat: string): string | null {
  const X = g.nations[nat];
  if (!X || X.alive) return 'It has not fallen.';
  if (g.flags.open !== 1) return 'Countries return only in the grand campaign.';
  if (g.flags.dom) return 'In the last act there are no alliances left to restore it.';
  if (X.dormant || !X.fellOn || X.capital < 0 || LIB.never.includes(nat)) return 'It has no country on the map to return to.';
  const cap = g.regions[X.capital];
  if (X.fellOn.how === 'annexed' || cap.owner !== nat) return 'It was annexed outright: there is no country left to restore.';
  if (X.fellOn.how === 'terms' || !cap.cap) return 'It left the war by treaty and was divided by the terms.';
  return null;
}

/**
 * Why this nation's hold on the capital would not free that country, or null if it would. 'soon' leaves out the wait
 * after a fall: it asks whether this nation could free it once the month is out.
 */
export function whyNotFreedBy(g: Game, nat: string, by: string, soon = false): string | null {
  const never = whyNoReturn(g, nat);
  if (never) return never;
  const X = g.nations[nat], H = g.nations[by];
  if (!H || !H.alive || by === nat) return 'Nobody is there to free it.';
  const V = g.nations[X.fellOn!.to];
  const side = bloc(H);
  if (V && (by === V.id || allied(g, by, V.id))) return `${theName(H, true)} stands with its conqueror.`;
  if (V?.alive && !atWar(g, by, V.id)) return `${theName(H, true)} is not at war with ${theName(V)}.`;
  if (!side) return `${theName(H, true)} belongs to no alliance: what it takes it keeps as a conquest.`;
  if (side === 'league' && !small(nat)) return 'The League restores only countries that could be members of it.';
  if (!soon && g.turn - X.fellOn!.turn < LIB.gap) return 'It fell only this month.';
  return null;
}

// ---- The return -----------------------------------------------------------------------------

/** Hands a country back every province of its own that its allies hold. Returns how many changed hands. */
function handBack(g: Game, X: Nation): number {
  let back = 0;
  for (const r of g.regions) {
    const mine = r.owner === X.id || (X.id === 'FRA' && r.owner === 'VIC' && r.vichy);
    if (!mine || r.ctrl === X.id || r.ctrl === 'VIC' || !allied(g, X.id, r.ctrl)) continue;
    // The ground does not change sides, so the roads and airfields on it stay in use: only the flag over it changes.
    r.ctrl = X.id;
    if (r.owner !== X.id) { r.owner = X.id; r.cap = false; }
    back++;
  }
  if (back) g.supplyDirty = true;
  return back;
}

/** Where a new corps of a freed country forms up: its capital if there is room, or the nearest ground of its own. */
function musterAt(g: Game, X: Nation): Region | null {
  const room = (r: Region): boolean => r.units.filter((u) => u.kind !== 'eng' && allied(g, u.nat, X.id)).length < STACK;
  const seen = new Uint8Array(g.regions.length);
  const q = [X.capital];
  seen[X.capital] = 1;
  for (let i = 0; i < q.length; i++) {
    const r = g.regions[q[i]];
    if (room(r)) return r;
    for (const j of r.nb) if (!seen[j] && g.regions[j].ctrl === X.id) { seen[j] = 1; q.push(j); }
  }
  return null;
}

/**
 * Brings a fallen country back into the war on its liberator's side. Returns false, and changes nothing, if this
 * liberator cannot free it.
 */
export function liberate(g: Game, nat: string, by: string, after?: string): boolean {
  if (whyNotFreedBy(g, nat, by)) return false;
  const X = g.nations[nat], H = g.nations[by];
  const side = bloc(H)!;
  const cap = g.regions[X.capital];
  // Its place in the war: beside whoever freed it.
  X.alive = true;
  X.league = false;
  if (side === 'league') enrol(g, X);
  else {
    if (bloc(X) !== side) X.align = side === 'allies' ? 'allies' : 'axis_cobelligerent';
    X.opinion = X.home = homeOpinion(nat, X.align);
  }
  X.atWar = [];
  for (const e of [...H.atWar]) if (e !== nat && g.nations[e]?.alive && !allied(g, nat, e)) setWar(g, nat, e);
  // Its ground: whatever its new allies hold of it is its own again.
  cap.cap = true;
  const provinces = handBack(g, X);
  // Its army: a few corps raised from the men who never stopped fighting.
  let vp = 0;
  for (const r of g.regions) if (homeOf(X, r)) vp += r.vp;
  const want = clamp(1 + Math.floor(vp / LIB.army.perVp), 1, LIB.army.most);
  let corps = 0;
  for (let k = 0; k < want; k++) {
    const at = musterAt(g, X);
    if (!at) break;
    const c = addCorps(g, nat, corpsName(nat, 'inf', X.corpsNo++), 'inf', LIB.army.divs, at.id, { xp: LIB.army.xp });
    c.str = LIB.army.str; c.org = LIB.army.org; c.ap = 0;
    corps++;
  }
  X.armyPeak = armySize(g, nat);
  X.pp = Math.max(0, X.pp);
  X.queue = [];
  X.freedOn = { turn: g.turn, week: g.week, by };
  g.supplyDirty = true;
  // The player's own country is the player's to lead again.
  const player = g.human === null && g.flags[`was:${nat}`] !== undefined;
  if (player) { g.human = nat; delete g.flags[`was:${nat}`]; }
  const foe = after ? g.nations[after] : null;
  headline(g, 'event', fill(player ? LIB.lines.freedSelf : foe ? LIB.lines.freedAfter : LIB.lines.freed, { Name: theName(X, true), name: theName(X), adj: H.adj, capital: cap.name, side: LIB.lines.sides[side], foe: foe ? theName(foe) : '' }), nat, cap.id);
  g.passings.push({ id: g.passings.length + 1, turn: g.turn, week: g.week, kind: 'freed', nat, by, how: 'freed', capital: cap.id, provinces, corps, side, ...(after ? { after } : {}), ...(player ? { player: true } : {}) });
  return true;
}

/** Frees a fallen country if whoever holds its capital can. */
function tryFree(g: Game, X: Nation, after?: string): boolean {
  if (X.alive || !X.fellOn || X.capital < 0) return false;
  return liberate(g, X.id, g.regions[X.capital].ctrl, after);
}

/**
 * Called when ground has just changed hands (the grand campaign). Ground that was France's before the armistice goes
 * back to a freed France when its allies take it from Vichy, and a fallen country whose capital is now held by its
 * conqueror's enemies returns.
 */
export function retaken(g: Game, r: Region, by: string): void {
  if (!liberationRules(g)) return;
  const F = g.nations.FRA;
  if (r.owner === 'VIC' && r.vichy && F?.alive && F.freedOn && r.ctrl !== 'VIC' && allied(g, r.ctrl, 'FRA')) {
    r.owner = 'FRA'; r.ctrl = 'FRA'; r.cap = false;
    g.supplyDirty = true;
    return;
  }
  const X = g.nations[r.owner === 'VIC' && r.vichy ? 'FRA' : r.owner];
  if (X && !X.alive && by !== X.id) tryFree(g, X);
}

/**
 * Called when a nation has just left the war (the grand campaign). Its fall can put another country's capital into
 * the hands of that country's friends, and Vichy's ground goes back to a France that has been freed.
 */
export function afterFall(g: Game, nat: string): void {
  if (!liberationRules(g)) return;
  const F = g.nations.FRA;
  if (nat === 'VIC' && F?.alive && F.freedOn) handBack(g, F);
  for (const X of Object.values(g.nations)) if (X.id !== nat) tryFree(g, X, nat);
}

/** A freed country stands for as long as its capital is in its own side's hands. */
export const standsFreed = (g: Game, n: Nation): boolean => !!n.freedOn && g.flags.open === 1 && n.capital >= 0 && allied(g, n.id, g.regions[n.capital].ctrl);

/**
 * The player's country has fallen and the player steps back to watch. The seat is remembered: if the country is
 * freed, the player is given it back.
 */
export function standDown(g: Game): string | null {
  const was = g.human;
  if (!was) return null;
  g.flags[`was:${was}`] = g.turn;
  g.human = null;
  return was;
}

// ---- For the interfaces ---------------------------------------------------------------------

/** A fallen country as an interface shows it: when it fell, to whom, and what it would take to bring it back. */
export interface FallenRow {
  nat: string;
  name: string;
  fell: { turn: number; week: number; to: string; how: Passing['how'] };
  /** Its capital, and who holds it today. */
  capital: number;
  capitalName: string;
  holder: string;
  /** Whether it can ever return, and whether the viewer taking (or holding) its capital would bring it back. */
  canReturn: boolean;
  yours: boolean;
  /** The alliance it would join if the viewer freed it. */
  side: Bloc | null;
  /** One sentence for the interface: how it comes back, or why it cannot. */
  note: string;
}

/** Every fallen country, the latest fall first, as the viewer sees its chances. */
export function fallenNations(g: Game, viewer: string | null = g.human): FallenRow[] {
  const out: FallenRow[] = [];
  for (const X of Object.values(g.nations)) {
    if (X.alive || X.dormant || !X.fellOn) continue;
    const never = whyNoReturn(g, X.id);
    const cap = X.capital >= 0 ? g.regions[X.capital] : null;
    const V = g.nations[X.fellOn.to];
    const me = viewer ? g.nations[viewer] : null;
    const why = never ?? (me ? whyNotFreedBy(g, X.id, me.id, true) : null);
    const yours = !never && !!me && why === null;
    const side = yours ? bloc(me!) : null;
    const wait = g.turn - X.fellOn.turn < LIB.gap;
    let note: string;
    if (never) note = never;
    else if (yours && cap) note = cap.ctrl === me!.id || allied(g, cap.ctrl, me!.id)
      ? `Your side holds ${cap.name}: ${theName(X)} returns the next time any of its ground is taken${wait ? ', from next month on' : ''}.`
      : `${wait ? `From next month, take` : 'Take'} ${cap.name} and ${theName(X)} is liberated: it returns to the war ${LIB.lines.sides[side!]}.`;
    else if (me && why) note = `${why} It returns if a member of an alliance at war with ${V ? theName(V) : 'its conqueror'} takes ${cap?.name ?? 'its capital'}.`;
    else note = `It returns if a member of an alliance at war with ${V ? theName(V) : 'its conqueror'} takes ${cap?.name ?? 'its capital'}.`;
    out.push({
      nat: X.id, name: X.name, fell: { turn: X.fellOn.turn, week: X.fellOn.week, to: X.fellOn.to, how: X.fellOn.how ?? 'beaten' },
      capital: X.capital, capitalName: cap?.name ?? '', holder: cap?.ctrl ?? '', canReturn: !never, yours, side, note,
    });
  }
  return out.sort((a, b) => b.fell.turn - a.fell.turn || b.fell.week - a.fell.week || (a.nat < b.nat ? -1 : 1));
}

/** The words of an announcement: a headline, the line over it, the story, and what follows from it. */
export interface PassingCard {
  id: number;
  kind: Passing['kind'];
  nat: string;
  by: string;
  region: number;
  kicker: string;
  title: string;
  text: string;
  /** What it means from here on: how the country can come back, or what it brings with it. */
  more: string;
  /** Three figures for the foot of the card. */
  facts: { label: string; value: string }[];
  /** For a fall: the countries its fall set free the same day. Their own announcements say 'after' this nation. */
  freed: string[];
  /** For a return that followed a conqueror's fall: that conqueror. Such a return is told on the conqueror's own card. */
  after: string | null;
  /** True if it is the player's own country. */
  player: boolean;
}

const NTH = ['', 'first', 'second', 'third', 'fourth', 'fifth', 'sixth', 'seventh', 'eighth', 'ninth', 'tenth', 'eleventh', 'twelfth'];
const count = (n: number, one: string, many = `${one}s`): string => `${n} ${n === 1 ? one : many}`;

/** An entry on the roll in words, the same for both interfaces. */
export function passingCard(g: Game, p: Passing): PassingCard {
  const X = g.nations[p.nat], B = g.nations[p.by];
  const when = turnToDate(p.turn).label;
  const cap = p.capital >= 0 ? g.regions[p.capital] : null;
  const Name = theName(X, true), name = theName(X);
  const freedBy = p.kind === 'fell' ? g.passings.filter((q) => q.kind === 'freed' && q.after === p.nat && q.turn === p.turn && q.id > p.id).map((q) => q.nat) : [];
  const base = { id: p.id, kind: p.kind, nat: p.nat, by: p.by, region: p.capital, freed: freedBy, after: p.after ?? null, player: !!p.player };
  if (p.kind === 'freed') {
    const side = LIB.lines.sides[p.side ?? 'allies'];
    const army = p.corps ? `${count(p.corps, 'new corps', 'new corps')} ${p.corps === 1 ? 'is' : 'are'} forming${cap ? ` at ${cap.name}` : ''}` : 'its army has yet to be raised';
    return {
      ...base, kicker: `Liberated · ${when}`, title: `${Name} is free`,
      text: `${p.after && g.nations[p.after] ? `With ${theName(g.nations[p.after])} beaten, ${cap?.name ?? 'the capital'} is in ${B.adj} hands` : `${B.adj} troops hold ${cap?.name ?? 'the capital'}`}. ${Name} returns to the war ${side}: ${p.provinces ? `${count(p.provinces, 'province')} ${p.provinces === 1 ? 'is' : 'are'} back under its own flag` : 'its flag flies over its capital again'}, and ${army}.`,
      more: p.player ? 'The country is yours to lead again. What its allies win back of it from now on is handed over to you at once.' : `It is run by its own government as ${p.by === g.human ? 'your ally' : `an ally of ${theName(B)}`}. What its allies win back of its ground from now on is handed over to it at once, and it stands for as long as ${cap?.name ?? 'its capital'} does.`,
      facts: [{ label: 'Freed by', value: B.name }, { label: 'Provinces returned', value: String(p.provinces) }, { label: 'Army raised', value: count(p.corps ?? 0, 'corps', 'corps') }],
    };
  }
  const week = p.weeks !== undefined ? ((p.weeks - 1) % 4) + 1 : p.week;
  const text = p.how === 'annexed' ? `${Name} was annexed by ${theName(B)} in ${when}. Its ${count(p.provinces, 'province')} ${p.provinces === 1 ? 'is' : 'are'} part of ${theName(B)} now.`
    : p.how === 'terms' ? `${Name} left the war in ${when}. Its ground is divided by the terms, and its armies have laid down their arms.`
      : `${Name} asked ${theName(B)} for terms in the ${NTH[week] ?? 'last'} week of ${when}. Its armies have laid down their arms, and its ${count(p.provinces, 'home province')} ${p.provinces === 1 ? 'is' : 'are'} under foreign flags.`;
  const never = X.alive ? null : whyNoReturn(g, p.nat);
  const more = X.alive ? `${Name} has since been liberated and is back in the war.`
    : never ? (g.flags.open === 1 ? `${Name} will not return. ${never}` : '')
      : `${Name} can come back. If a member of an alliance at war with ${theName(B)} takes ${cap?.name ?? 'its capital'}, ${name} is liberated and returns to the war on that side${p.player ? ', and you are given it back to lead' : ''}.`;
  return {
    ...base, kicker: `${p.how === 'annexed' ? 'Annexed' : p.how === 'terms' ? 'Out of the war' : 'Eliminated'} · ${when}`,
    title: p.how === 'annexed' ? `${Name} is annexed` : p.how === 'terms' ? `${Name} is out of the war` : `${Name} has fallen`,
    text, more,
    facts: [{ label: p.how === 'annexed' ? 'Annexed by' : 'Fell to', value: B.name }, { label: 'Home provinces', value: String(p.provinces) }, { label: 'Week of the war', value: String(p.weeks ?? (p.turn - 1) * 4 + p.week) }],
  };
}
