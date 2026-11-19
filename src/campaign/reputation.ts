// Reputations (the sandbox; Brian, 19 November 2026: "i want airplane squadrons to get nick names if they keep
// winning battles and get a reputation", "for example the Red Tails", "same with tanks and battalions of troops").
//
// Every air group, corps and fleet keeps a reputation: points for a battle won, more for a big one or for beating a
// unit the enemy already fears, points off for a battle lost, and a count of battles won running. A unit with enough
// points on a run of wins makes its name. A historical formation gets the name history gave it; any other gets one in
// its own country's style, most often from the paint its men put on to be known by, as the Red Tails were. The paint
// is kept with the name as the unit's mark, for the 3D game to show on its machines. A ship's crew names her once she
// has enough battle stars (shipNick, for hulls.ts).
//
// One rule from the stories runs through it: every unit of every nation keeps a reputation and can make a name, but
// the edge a reputation gives is for the player's own units alone, so nothing here changes a number for a
// computer-run nation. Names are drawn from the game's seed, never from the dice of the war. Off unless the sandbox is on.

import data from '../../data/campaign/reputation.json';
import { Rng, hashString } from '../core/rng';
import { generalOf } from './commanders';
import { atWar, headline } from './game';
import { sandbox } from './history';
import type { AirFleet, Corps, Fleet, Game, Storied, UnitKind, UnitMark } from './types';

export type Arm = 'air' | 'tank' | 'troops' | 'fleet';
interface Part { name: string; at: string; says: string }
interface Style { forms: [string, number][]; colours: string[]; adj: string[]; beasts: [string, string][] }
interface Tier { at: number; word: string; edge: number }
export interface RealName { nat: string; name: string; nick: string; mark: UnitMark; says: string; why: string }

export const REPUTATION = data as unknown as {
  gain: { win: number; big: number; famous: number; loss: number };
  odds: { easy: number; hard: number; light: number };
  named: { rep: number; run: number; share: number };
  tiers: Tier[];
  famousTier: number;
  colours: Record<string, string>;
  parts: Record<Arm, Part[]>;
  emblemAt: Record<Arm, { at: string; says: string }>;
  bandAt: Record<Arm, { at: string; says: string }>;
  who: Record<Arm, string>;
  share: Record<string, string>;
  styles: Record<string, Style>;
  ship: { stars: number; forms: [string, number][]; adj: string[]; nouns: string[] };
  blocked: string[];
  real: RealName[];
};

const R = REPUTATION;
const BLOCKED = new Set(R.blocked);
type Unit = (Corps | AirFleet | Fleet) & Storied;

// ---- Looking things up ------------------------------------------------------------------------------------------

/** What a unit is, for its name and its mark: an air group, a tank corps, troops, or a fleet. */
export const armOf = (kind: UnitKind, u: object): Arm => (kind === 'air' ? 'air' : kind === 'fleet' ? 'fleet' : (u as { kind?: unknown }).kind === 'arm' ? 'tank' : 'troops');

/** The country's style of name: its own, the one it shares, or the plain one. */
export const styleOf = (nat: string): Style => R.styles[nat] ?? R.styles[R.share[nat]] ?? R.styles._default;

/** How far a unit's name has gone: the tier its points have reached, -1 for none. */
export function tierIndex(u: Storied): number {
  const p = u.rep ?? 0;
  let i = -1;
  R.tiers.forEach((t, k) => { if (p >= t.at) i = k; });
  return i;
}
export const tierOf = (u: Storied): Tier | undefined => R.tiers[tierIndex(u)];

/** The edge one of the player's units has from its reputation, beside its honours'. 0 for everyone else, and with the sandbox off. */
export const repEdge = (g: Game, u: { nat: string } & Storied): number => (sandbox(g) && u.nat === g.human ? tierOf(u)?.edge ?? 0 : 0);

/** The name history gave this formation, where it gave one. */
export const realOf = (u: { nat: string; name: string }): RealName | undefined => R.real.find((r) => r.nat === u.nat && r.name === u.name);

/** True when a name is history's for some other unit, and so never made up. */
export const isBlocked = (nick: string): boolean => BLOCKED.has(nick.toLowerCase().replace(/^the /, ''));

const fill = (s: string, v: Record<string, string>): string => s.replace(/\{(\w+)\}/g, (_, k) => v[k] ?? '').replace(/\s+/g, ' ').trim();
const article = (w: string): string => (/^[aeiou]/i.test(w) ? 'an' : 'a');
const cap = (w: string): string => w.charAt(0).toUpperCase() + w.slice(1);
const surname = (name: string): string => name.replace(/\s*\(.*\)\s*$/, '').trim().split(/\s+/).pop() ?? name;
const WORDS = ['no', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten', 'eleven', 'twelve'];
const count = (n: number): string => WORDS[n] ?? `${n}`;

/** Names a nation's units go by already: none is given twice in one army. */
function takenBy(g: Game, nat: string): Set<string> {
  const out = new Set<string>();
  for (const xs of [g.corps, g.airFleets, g.fleets, g.stories.fallen] as Storied[][]) for (const x of xs) if ((x as { nat?: string }).nat === nat && x.nick) out.add(x.nick);
  return out;
}

/** Where a unit made its name, for the names that carry a place: the ground fought over, an air group's field, a fleet's sea. */
function placeOf(g: Game, kind: UnitKind, u: Unit, at: number): string {
  if (kind === 'fleet') { const s = g.seas[at >= 0 ? at : (u as Fleet).zone]; return s ? s.name : ''; }
  const r = g.regions[at >= 0 ? at : kind === 'air' ? (u as AirFleet).base : (u as Corps).reg];
  return r ? r.name : '';
}

/** The surname of the man who led it to its name. */
function leaderOf(g: Game, kind: UnitKind, u: Unit): string {
  if (kind === 'air') return (u as AirFleet).leader ? surname((u as AirFleet).leader!.name) : '';
  if (kind === 'fleet') return (u as Fleet).admiral ? surname((u as Fleet).admiral!.name) : '';
  const gen = generalOf(g, u as Corps);
  return gen ? surname(gen.name) : '';
}

function weighted(rng: Rng, forms: [string, number][]): string {
  const all = forms.reduce((s, [, w]) => s + w, 0);
  let r = rng.next() * all;
  for (const [f, w] of forms) { r -= w; if (r < 0) return f; }
  return forms[forms.length - 1][0];
}

/** What people see of a mark, in a sentence: 'Its crews have painted their tails red.' */
export function markWords(arm: Arm, m: UnitMark): string {
  const v = { colour: m.colour, a: article(m.colour), emblem: m.emblem ?? '' };
  if (m.emblem) return fill(R.emblemAt[arm].says, v);
  const part = R.parts[arm].find((p) => p.at === m.at);
  if (part) return `${R.who[arm]} ${fill(part.says, v)}.`;
  return fill(R.bandAt[arm].says, v);
}

/** The words for a unit's own mark: history's, where its name is history's. */
export const markOf = (kind: UnitKind, u: Unit & { name: string }): string => (u.mark ? (realOf(u)?.nick === u.nick ? realOf(u)!.says : markWords(armOf(kind, u), u.mark)) : '');

// ---- Making a name ------------------------------------------------------------------------------------------------

/**
 * A name in the unit's country's style, and the mark that goes with it, drawn from the game's seed and the unit: never
 * one its army already uses, never one of history's. With leave, an honour's name it already carries stays, and only
 * the paint is drawn.
 */
export function drawName(g: Game, kind: UnitKind, u: Unit, at: number, keep = false): { nick: string; mark: UnitMark } | undefined {
  const arm = armOf(kind, u), st = styleOf(u.nat);
  const rng = new Rng(hashString(`rep:${g.seed}:${u.nat}:${kind}:${u.id}`));
  if (keep && u.nick) {
    const part = rng.pick(R.parts[arm]);
    return { nick: u.nick, mark: { at: part.at, colour: rng.pick(st.colours) } };
  }
  const taken = takenBy(g, u.nat), place = placeOf(g, kind, u, at), leader = leaderOf(g, kind, u);
  const forms = st.forms.filter(([f]) => (!f.includes('{place}') || place) && (!f.includes('{leader}') || leader) && (!f.includes('{adj}') || st.adj.length));
  if (!forms.length) return undefined;
  for (let i = 0; i < 60; i++) {
    const f = weighted(rng, forms);
    const colour = rng.pick(st.colours), part = rng.pick(R.parts[arm]), beast = rng.pick(st.beasts), adj = st.adj.length ? rng.pick(st.adj) : '';
    // A sea is 'the North Sea' after 'of'; a province is plain.
    const where = kind === 'fleet' && / of \{place\}/.test(f) && !/^the /i.test(place) ? `the ${place}` : place;
    const nick = fill(f, { colour: cap(colour), part: part.name, beast: beast[0], adj, place: where, leader });
    if (isBlocked(nick) || taken.has(nick)) continue;
    const mark: UnitMark = f.includes('{part}') ? { at: part.at, colour }
      : f.includes('{beast}') ? { at: R.emblemAt[arm].at, colour, emblem: beast[1] }
      : { at: R.bandAt[arm].at, colour };
    return { nick, mark };
  }
  return undefined;
}

/** The live units of one arm of a nation. */
const armyOf = (g: Game, kind: UnitKind, nat: string): Storied[] =>
  (kind === 'corps' ? g.corps.filter((c) => c.nat === nat && !c.dead) : kind === 'air' ? g.airFleets.filter((f) => f.nat === nat) : g.fleets.filter((f) => f.nat === nat));

/** True while a nation's army still has room for one more name of this arm: it talks about its best few only. */
export function roomForName(g: Game, kind: UnitKind, nat: string): boolean {
  const all = armyOf(g, kind, nat);
  return all.filter((x) => x.mark).length < Math.max(1, Math.ceil(R.named.share * all.length));
}

/** A unit that has won enough, running, makes its name: once, with its mark, and the news of it. */
function makeName(g: Game, kind: UnitKind, u: Unit, at: number): void {
  if (u.mark || (u.rep ?? 0) < R.named.rep || (u.run ?? 0) < R.named.run) return;
  if ((u as Corps).kind === 'fort' || !roomForName(g, kind, u.nat)) return;
  const real = realOf(u);
  const taken = real ? takenBy(g, u.nat) : undefined;
  const got = real && !taken!.has(real.nick) ? { nick: real.nick, mark: { ...real.mark } } : drawName(g, kind, u, at, !!u.nick);
  if (!got) return;
  const fresh = got.nick !== u.nick;
  u.nick = got.nick;
  u.mark = got.mark;
  tell(g, kind, u, at, fresh, real?.nick === got.nick ? real : undefined);
}

function tell(g: Game, kind: UnitKind, u: Unit, at: number, fresh: boolean, real: RealName | undefined): void {
  const me = g.human;
  if (!me) return;
  const service = kind === 'corps' ? 'army' : kind === 'air' ? 'air force' : 'navy';
  const where = kind === 'fleet' ? undefined : at >= 0 ? at : kind === 'air' ? (u as AirFleet).base : (u as Corps).reg;
  const looks = markOf(kind, u);
  const run = `has won ${count(u.run ?? 0)} battles running`;
  if (u.nat === me) {
    const named = fresh ? `, and the ${service} has begun to call it ${u.nick}` : `, and the ${service} knows it everywhere as ${u.nick}`;
    headline(g, 'honour', `${u.name} ${run}${named}. ${looks}${real ? ` ${real.why}` : ''}`, u.nat, where);
  } else if (atWar(g, me, u.nat)) {
    const ours = kind === 'corps' ? 'soldiers' : kind === 'air' ? 'pilots' : 'sailors';
    headline(g, 'honour', `The ${g.nations[u.nat].adj} ${u.name} ${run}, and our ${ours} ${fresh ? 'have a name for it' : 'have learned its name'}: ${u.nick}. ${looks}`, u.nat, where);
  }
}

// ---- Battles --------------------------------------------------------------------------------------------------------

/** What a land battle won is worth to the winners' names, from the odds before it: 2 a big win, 1 a win, 0 a walkover. */
export function landWin(attackerWon: boolean, preOdds: number, decisive: boolean): 0 | 1 | 2 {
  const O = R.odds;
  if (attackerWon) return preOdds >= O.easy ? 0 : decisive || preOdds < O.hard ? 2 : 1;
  return preOdds >= 1 ? 2 : preOdds >= O.light ? 1 : 0;
}

/**
 * A battle is over (the sandbox): the winners' reputations go up, by more for a big win (size 2: decisive, a sky won,
 * ships sunk, a stand against the odds) and by more again for beating a unit the enemy already fears; a walkover
 * (size 0) does nothing for them. The losers' go down and their run of wins is over. Anyone who has now won enough,
 * running, makes a name.
 */
export function repBattle(g: Game, kind: UnitKind, won: Unit[], lost: Unit[], at: number, size: 0 | 1 | 2 = 1): void {
  if (!sandbox(g)) return;
  const G = R.gain;
  const giants = lost.filter((u) => tierIndex(u) >= R.famousTier);
  for (const u of won) {
    if (!size || (u as Corps).dead || (u as Corps).kind === 'fort') continue;
    u.rep = (u.rep ?? 0) + (size === 2 ? G.big : G.win) + (giants.length ? G.famous : 0);
    u.run = (u.run ?? 0) + 1;
    if (u.run > (u.best ?? 0)) u.best = u.run;
  }
  for (const u of lost) {
    if (!u.rep && !u.run) continue;
    u.rep = Math.max(0, (u.rep ?? 0) + G.loss);
    u.run = 0;
  }
  const me = g.human;
  if (me && size && won.some((u) => u.nat === me)) {
    for (const x of giants) if (x.nick && x.nat !== me) headline(g, 'honour', `${x.nick}, the ${g.nations[x.nat].adj} ${x.name}, ${(x as Corps).dead ? 'is no more' : 'has been beaten'}. The news is all along our line.`, me, kind === 'fleet' ? undefined : at >= 0 ? at : undefined);
  }
  if (size) for (const u of won) if (!(u as Corps).dead) makeName(g, kind, u, at);
}

/**
 * An air battle's verdict for each side, from what each lost and what was won: the side that brought down more than it
 * lost has won it, a sky won or ships sunk is a big win, and a raid nobody rose to meet is no battle at all.
 */
export function repAir(g: Game, target: number, att: AirFleet[], def: AirFleet[], f: { sky: boolean; sunk: number; attLost: number; defLost: number }): void {
  if (!sandbox(g)) return;
  const big = f.sky || f.sunk > 0;
  if (!def.length) { if (f.sunk > 0) repBattle(g, 'air', att, [], target, 2); return; }
  if (big || f.defLost > f.attLost) repBattle(g, 'air', att, def, target, big ? 2 : 1);
  else if (f.attLost > f.defLost) repBattle(g, 'air', def, att, -1, f.attLost >= 3 * Math.max(1, f.defLost) ? 2 : 1);
}

/** What a ship's crew calls her once she has come through enough battles (her stars): for hulls.ts. Undefined before then. */
export function shipNick(g: Game, nat: string, ship: string, stars: number): string | undefined {
  if (!sandbox(g) || stars < R.ship.stars) return undefined;
  const rng = new Rng(hashString(`ship:${g.seed}:${nat}:${ship}`));
  for (let i = 0; i < 20; i++) {
    const nick = fill(weighted(rng, R.ship.forms), { adj: rng.pick(R.ship.adj), noun: rng.pick(R.ship.nouns) });
    if (!isBlocked(nick)) return nick;
  }
  return undefined;
}
