// The League: a third alliance, of smaller nations, that stands apart from the Axis and the Allies, and the path a
// small country in the player's hands chooses for itself: join the Axis, join the Allies, stand alone, or found or
// join the League. The grand campaign only. The numbers live in data/campaign/league.json.
//
// For computer-run nations the League is rare: it is founded by a circle of friends (relations.json) in the month
// after one of their friends is attacked, or once in a great while unprovoked, and afterwards a few more small
// countries may join it. It is a pact of mutual defence: members are allies of one another (bloc() answers 'league'),
// war declared on one of them is war with them all (declareWar in game.ts), and while it stands none of them can be
// courted, moved along the track or annexed by treaty.

import data from '../../data/campaign/league.json';
import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import worldData from '../../data/campaign/world.json';
import { hashString } from '../core/rng';
import type { Game, Nation } from './types';
import { atWar, bloc, headline } from './game';
import { DIPLOMACY, RELATIONS, friendsOf, step } from './diplomacy';
import { leagueMul } from './history';
import { shoreOf } from './world';
import { guaranteeDeters } from './guarantees';

export const LEAGUE = data as unknown as {
  name: string; full: string; color: string; never: string[];
  found: { shock: number; month: number; after: number; least: number };
  join: { friend: number; threat: number; most: number };
  invite: { cost: number };
  deter: number;
};

/** True for every country but the great powers: the ones that can belong to the League, and choose their own path in a player's hands. */
export const small = (id: string): boolean => !LEAGUE.never.includes(id);

const PARTS = worldData as unknown as { nations: Record<string, unknown>; parts: { names: Record<string, string>; of: Record<string, string>; new: string } };
/**
 * The part of the world a country lies in: Europe and the Mediterranean, or the Americas. A League is a pact of
 * neighbours (ruling 58): a founder or a member may ask only countries of its own part, so Brazil founds none with Belgium.
 */
export const partOf = (id: string): string => PARTS.parts.of[id] ?? (PARTS.nations[id] ? PARTS.parts.new : 'europe');
export const partName = (id: string): string => PARTS.parts.names[partOf(id)] ?? partOf(id);
const farFrom = (id: string, members: string[]): string | undefined => members.find((m) => partOf(m) !== partOf(id));

/**
 * The country the player leads in the grand campaign. None of history's events ends it, signs it out of the war or
 * deals out its ground (ruling 99): in its player's hands a country falls only by defeat in the field.
 */
export const playersOwn = (g: Game, id: string): boolean => g.flags.open === 1 && id === g.human;

/**
 * Whether history's hand is stayed from the country the player leads (rulings 99 and 103). An event may take the
 * player's country out of the war only where his campaign's card states it as the stake: never in the grand
 * campaign, and in a history campaign only from a seat the event names by the month it is taken (Italy's from July
 * 1943: a campaign in the desert is not lost in Rome). A game saved before seats were recorded (goals.ts, openGoals)
 * is left to history as it was.
 */
export const spared = (g: Game, id: string, stake: number[] = []): boolean =>
  id === g.human && (g.flags.open === 1 || (g.flags.seat !== undefined && !stake.includes(g.flags.seat)));

/** The nation whose place among the powers only its player decides: a small country in the player's hands, in the grand campaign. */
export const ownPath = (g: Game, id: string): boolean => playersOwn(g, id) && small(id);

/** The League's members still standing. */
export const leagueMembers = (g: Game): Nation[] => Object.values(g.nations).filter((n) => n.league && n.alive);

/** True once the League has been founded, whether or not any member is left. */
export const leagueFounded = (g: Game): boolean => g.flags['league:born'] !== undefined;

const listOf = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const dice = (g: Game, key: string): number => hashString(`league:${g.seed}:${g.turn}:${key}`) / 4294967296;

/**
 * True for a country with a nation at war on its border: it feels the war coming. A colony on the far side of the
 * world never brings the war home to it (ruling 231, 2: Portugal kept out with Goa beside British India, and kept out
 * again when Timor was occupied). The colonies of the old chart count as they always have (Spain looked to its
 * Morocco in November 1942), and so do those of the Americas.
 */
export function threatened(g: Game, id: string): boolean {
  for (const r of g.regions) {
    if (r.owner !== id || r.ctrl !== id || (r.overseas && shoreOf(r) === 2)) continue;
    for (const j of r.nb) {
      const o = g.regions[j].ctrl;
      if (o !== id && g.nations[o]?.alive && g.nations[o].atWar.length > 0) return true;
    }
  }
  return false;
}

/**
 * A client state (world.json, 'clients': Manchukuo, Mongolia) founds and joins no League (the rider to ruling 209(c)):
 * its patron's army stands on its soil. There is none on the older chart.
 */
const client = (id: string): boolean => !!(worldData as unknown as { clients?: Record<string, unknown> }).clients?.[id];
const CLIENT_NO = 'A client state joins no League: its patron would not have it.';

/** A computer-run country that could found or join the League of its own accord: small, at peace, and leaning to neither side. */
function free(g: Game, n: Nation): boolean {
  return n.alive && !n.dormant && small(n.id) && !client(n.id) && n.id !== g.human && !n.atWar.length && bloc(n) === null && step(n) === 0;
}

/** Makes a country a member. It leaves the track between the two blocs and takes its stand in the middle. */
export function enrol(g: Game, n: Nation): void {
  n.league = true;
  n.align = 'independent';
  n.opinion = n.home = 0;
  g.flags[`league:${n.id}`] = g.turn;
  g.supplyDirty = true;
}

/** Founds the League with these countries. There is only ever one. */
function found(g: Game, founders: Nation[], why: string): void {
  g.flags['league:born'] = g.turn;
  for (const n of founders) enrol(g, n);
  headline(g, 'war', `${listOf(founders.map((n) => n.name))} found ${LEAGUE.full}${why}. They belong to neither the Axis nor the Allies, and an attack on one is an attack on all.`, founders[0].id);
}

/** Whether a country would accept a place in the League beside these members, and why or why not. Nothing is changed. */
export function willing(g: Game, id: string, members: string[]): { ok: boolean; why: string; friend: boolean } {
  const n = g.nations[id];
  const no = (why: string): { ok: boolean; why: string; friend: boolean } => ({ ok: false, why, friend: false });
  if (!n || !n.alive || n.dormant) return no('Not there to ask.');
  if (!small(id)) return no('A great power joins no league of small nations.');
  if (client(id)) return no(CLIENT_NO);
  if (n.league) return no('Already a member.');
  const far = farFrom(id, members);
  if (far) return no(`A pact of neighbours: ${g.nations[far]?.name ?? 'its founder'} lies in ${partName(far)}, another part of the world.`);
  const b = bloc(n);
  if (b) return no(`Already belongs to ${b === 'axis' ? 'the Axis' : 'the Allies'}.`);
  if (n.atWar.length) return no('At war: the League takes in no country that is already fighting.');
  if (step(n) !== 0) return no(`Leans toward ${step(n) > 0 ? 'the Axis' : 'the Allies'} and trades with ${step(n) > 0 ? 'it' : 'them'}.`);
  if (members.length >= LEAGUE.join.most) return no('The League is as large as it can hold together.');
  const tie = friendsOf(id).find((f) => members.includes(f.nat));
  if (tie) return { ok: true, why: `A friend of ${g.nations[tie.nat].name}: ${tie.why}.`, friend: true };
  if (threatened(g, id)) return { ok: true, why: 'Has a nation at war on its border and wants friends.', friend: false };
  return no('Sees no danger yet and would rather stand alone.');
}

/** The month's business for computer-run countries: once in a while a circle of friends founds the League, and now and then another country joins. */
export function leagueMonth(g: Game): void {
  if (g.flags.open !== 1) return;
  if (!leagueFounded(g)) {
    for (const gp of RELATIONS.groups) {
      const founders = gp.nations.map((id) => g.nations[id]).filter((n) => n && free(g, n));
      if (founders.length < LEAGUE.found.least) continue;
      // The month after a friend is attacked is when such pacts are made. Unprovoked, it happens once in a great while.
      const shocked = founders.some((n) => friendsOf(n.id).some((f) => g.flags[`shock:${f.nat}`] === g.turn - 1));
      // (The history dial: by the book no League is founded that history never saw; higher up it comes more readily.)
      const p = (shocked ? LEAGUE.found.shock : g.turn > LEAGUE.found.after ? LEAGUE.found.month : 0) * leagueMul(g);
      if (dice(g, `found:${gp.id}`) >= p) continue;
      found(g, founders, shocked ? ' as the war reaches their neighbours' : '');
      return;
    }
    return;
  }
  const members = leagueMembers(g).map((n) => n.id);
  if (!members.length) return; // every member has fallen: the League is history
  for (const n of Object.values(g.nations)) {
    if (!free(g, n) || members.length >= LEAGUE.join.most) continue;
    const w = willing(g, n.id, members);
    if (!w.ok || dice(g, `join:${n.id}`) >= (w.friend ? LEAGUE.join.friend : LEAGUE.join.threat) * leagueMul(g)) continue;
    enrol(g, n);
    members.push(n.id);
    headline(g, 'event', `${n.name} joins ${LEAGUE.name}: ${listOf(leagueMembers(g).filter((m) => m.id !== n.id).map((m) => m.name))} now stand with it against any attack.`, n.id);
  }
}

// ---- The player's path ------------------------------------------------------------------------------------------

export type PathId = 'axis' | 'allies' | 'alone' | 'found' | 'join';

export interface PathOption {
  id: PathId;
  /** True when the player can take it today. */
  ok: boolean;
  /** Why not, or for the path already taken, where the country stands. */
  why: string;
  /** True for the path the country is on now. */
  now: boolean;
}

/** The player's country, if its path is its own to choose. */
// In the last act of the grand campaign there are no alliances left to choose between.
const chooser = (g: Game, nat: string): Nation | null => (ownPath(g, nat) && g.nations[nat]?.alive && !g.flags.dom ? g.nations[nat] : null);

/**
 * The paths open to a small country in the player's hands: join the Axis, join the Allies, stand alone, found the
 * League or join it. Joining an alliance cannot be undone. Empty for a great power and outside the grand campaign.
 */
export function pathOptions(g: Game, nat: string): PathOption[] {
  const n = chooser(g, nat);
  if (!n) return [];
  const mine = bloc(n);
  const out: PathOption[] = [];
  for (const b of ['axis', 'allies'] as const) {
    const leader = g.nations[DIPLOMACY.leaders[b]];
    const name = b === 'axis' ? 'the Axis' : 'the Allies';
    let why = '';
    if (mine === b) why = `You are a member of ${name}.`;
    else if (mine) why = `You have given your word to ${mine === 'league' ? LEAGUE.name : mine === 'axis' ? 'the Axis' : 'the Allies'}.`;
    else if (!leader?.alive || bloc(leader) !== b) why = `${name[0].toUpperCase()}${name.slice(1)} ${b === 'axis' ? 'has' : 'have'} no leader left to join.`;
    else if (n.atWar.some((x) => bloc(g.nations[x]) === b)) why = `You are at war with ${name}.`;
    out.push({ id: b, ok: why === '', why, now: mine === b });
  }
  out.push({ id: 'alone', ok: mine === null && step(n) !== 0, why: mine ? 'An alliance, once joined, is not left.' : step(n) === 0 ? 'You answer to nobody.' : '', now: mine === null && step(n) === 0 });
  if (!leagueFounded(g)) {
    const partner = !mine && !n.atWar.length && founders(g, nat).some((x) => x.ok);
    if (client(nat)) out.push({ id: 'found', ok: false, why: CLIENT_NO, now: false });
    else out.push({ id: 'found', ok: partner, why: mine ? `You have given your word to ${mine === 'axis' ? 'the Axis' : 'the Allies'}.` : n.atWar.length ? 'A country at war finds no partners: the League is founded in peace.' : partner ? '' : 'No other country is ready to found it with you yet.', now: false });
  } else {
    const members = leagueMembers(g);
    let why = '';
    if (mine === 'league') why = `You are a member of ${LEAGUE.name}.`;
    else if (mine) why = `You have given your word to ${mine === 'axis' ? 'the Axis' : 'the Allies'}.`;
    else if (!members.length) why = 'The League has fallen: none of its members is left.';
    else if (n.atWar.length) why = 'The League takes in no country that is already at war.';
    else if (members.length >= LEAGUE.join.most) why = 'The League is as large as it can hold together.';
    else if (farFrom(nat, members.map((m) => m.id))) why = `The League is a pact of neighbours, and its members lie in ${partName(members[0].id)}.`;
    if (client(nat) && mine !== 'league') why = CLIENT_NO;
    out.push({ id: 'join', ok: why === '', why, now: mine === 'league' });
  }
  return out;
}

export interface Invite { nat: string; ok: boolean; why: string; friend: boolean }

/** The countries the player could found the League with, each with its answer. Willing ones first. */
export function founders(g: Game, nat: string): Invite[] {
  return asked(g, [nat]);
}

/** The countries a member could invite into the League, each with its answer. Willing ones first. */
export function invites(g: Game, nat: string): Invite[] {
  return g.nations[nat]?.league ? asked(g, leagueMembers(g).map((n) => n.id)) : [];
}

function asked(g: Game, members: string[]): Invite[] {
  const out: Invite[] = [];
  for (const n of Object.values(g.nations)) {
    if (!n.alive || n.dormant || n.league || members.includes(n.id) || !small(n.id) || client(n.id) || bloc(n)) continue;
    // Only neighbours are asked: a country in another part of the world is not on the list at all.
    if (farFrom(n.id, members)) continue;
    out.push({ nat: n.id, ...willing(g, n.id, members) });
  }
  return out.sort((a, b) => Number(b.ok) - Number(a.ok) || Number(b.friend) - Number(a.friend) || g.nations[a.nat].name.localeCompare(g.nations[b.nat].name));
}

/** Why the player cannot invite this country today, or null if he can. */
export function whyNotInvite(g: Game, by: string, target: string): string | null {
  const B = g.nations[by];
  if (g.flags.open !== 1 || !B?.alive || !B.league) return 'Only a member of the League can invite others.';
  const w = willing(g, target, leagueMembers(g).map((n) => n.id));
  if (!w.ok) return w.why;
  if (B.pc < LEAGUE.invite.cost) return `Needs ${LEAGUE.invite.cost} political capital.`;
  return null;
}

/** Brings a willing country into the League, for political capital. */
export function invite(g: Game, by: string, target: string): boolean {
  if (whyNotInvite(g, by, target) !== null) return false;
  const B = g.nations[by], T = g.nations[target];
  B.pc -= LEAGUE.invite.cost;
  enrol(g, T);
  headline(g, 'event', `${T.name} joins ${LEAGUE.name} at ${B.name}'s invitation.`, target);
  return true;
}

/** Takes a path. For founding the League, 'partner' is the country that founds it with the player. */
export function takePath(g: Game, nat: string, id: PathId, partner?: string): boolean {
  const n = chooser(g, nat);
  const opt = pathOptions(g, nat).find((o) => o.id === id);
  if (!n || !opt?.ok) return false;
  if (id === 'axis' || id === 'allies') {
    const s = id === 'axis' ? 1 : -1;
    n.align = id;
    n.opinion = n.home = 100 * s;
    g.flags[`step:${n.id}`] = g.turn;
    g.supplyDirty = true;
    headline(g, 'war', `${n.name} joins ${id === 'axis' ? 'the Axis' : 'the Allies'}.`, n.id);
    return true;
  }
  if (id === 'alone') {
    const was = step(n);
    n.align = 'independent';
    n.opinion = n.home = 0;
    g.flags[`step:${n.id}`] = g.turn;
    g.supplyDirty = true;
    headline(g, 'event', `${n.name} steps back from ${was > 0 ? 'the Axis' : 'the Allies'} to strict neutrality.`, n.id);
    return true;
  }
  if (id === 'found') {
    const p = partner ? g.nations[partner] : undefined;
    if (!p || !founders(g, nat).some((x) => x.nat === partner && x.ok)) return false;
    found(g, [n, p], '');
    return true;
  }
  enrol(g, n);
  headline(g, 'event', `${n.name} joins ${LEAGUE.name}: ${listOf(leagueMembers(g).filter((m) => m.id !== n.id).map((m) => m.name))} now stand with it against any attack.`, n.id);
  return true;
}

/**
 * How much less likely a computer-run staff is to open an operation against these countries, because some of them
 * belong to the League and the rest of it would come in (1 when none does, or when the staff's own country is a
 * member), and because a guarantor would declare war on it for one of them (ruling 229, part 3: the staff reads the
 * card the player reads).
 */
export function deterred(g: Game, nat: string, targets: string[]): number {
  if (g.flags.open !== 1) return 1;
  const league = !g.nations[nat]?.league && targets.some((id) => g.nations[id]?.league && standsWith(g, nat, id).length > 0) ? LEAGUE.deter : 1;
  return league * guaranteeDeters(g, nat, targets);
}

/** The members who would go to war beside this one if an outsider attacked it. */
export function standsWith(g: Game, by: string, on: string): string[] {
  const B = g.nations[on];
  if (!B?.league || g.nations[by]?.league) return [];
  return leagueMembers(g).filter((m) => m.id !== on && !atWar(g, m.id, by)).map((m) => m.id);
}
