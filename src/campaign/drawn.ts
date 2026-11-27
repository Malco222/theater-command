// A division is laid once (release 2, the grand campaign; the design director's rulings of 13:43Z, 13:48Z, 13:50Z,
// 13:51Z and 14:28Z, 25 November 2026, on the joiner's six-year check and the historian's read of every pair:
// joins/named-twice-check-1125.md, historian/named-twice-reads-1125.md).
//
// The fault: a dated row (game.ts, expeditions) laid every division its name gives at full strength, though a living
// corps of its country already held one of them: the 2nd and 9th Panzer Divisions of the Balkans' month 19 beside
// XXII Corps, whose two divisions they are; Britain's 1st Armoured Division at Alexandria beside the one in England.
// The same division stood twice and its country had a second division's strength for nothing.
//
// The rule, one for every seat, by the game's own state on the row's day:
//   new        a division the row names that no living corps of its country holds is raised, as it always was: a
//              first arrival, or the re-raising of one that was destroyed.
//   drawn      one a living corps holds is not raised again. It is drawn out of that corps to the row: it brings its
//              share of the corps, one division's worth of its men, experience, order and morale as the corps stands
//              (an even share: what a division's card shows is a way of reading the corps, and does not travel), and
//              the corps keeps the rest, with the names and numbers of the divisions it keeps. A corps of the row's
//              arm all of whose divisions the row names comes itself: it moves to the row's place and takes the row's
//              name and standing orders. The row's corps always carries the row's name, whatever it holds of the
//              row's divisions: every other row that builds on it finds it by that name (`joins`, `from`, `takes`,
//              `unless`, the far side's fronts), and its cards read the divisions it holds.
//   stays      a row never pulls a division out of the theatre the row is for (her ruling 2 of 14:28Z): a division
//              already standing in the side theatre of the place the row names (North Africa, the Balkans, Italy), or
//              at the place where the row lands, stays with its corps, and so does every held division where the row
//              falls back to forming at home, its own place lost or at peace. The row forms only its new divisions.
//   the player his divisions are never moved for him (ruling 4): they stay with his corps, the row forms its new
//              divisions, and its line tells him the row names them, so that he can send them himself.
//   formed     a division the record formed out of another (a row's `was`: the 14th Panzer, out of the 4th Infantry)
//              is drawn as that other one, from the corps that holds it, or else out of the largest free infantry
//              corps on home soil; with neither it is not formed. It is never new strength.
//   the end    when a lent row's time is up (`until`), or a war row's war is over, each division it drew goes back to
//              the corps it came out of, where that corps lives and the two stand together, in one province; otherwise
//              it stands on where it is, as a corps of its own under its own name, free for its staff to move: a return
//              never moves a division for free (her rulings of 19:03Z and 19:17Z, 25 November 2026). A division
//              formed out of another never goes back. The divisions the row raised new leave, as they always did.
//              Nothing goes back to a corps that is itself leaving that month.
// Parts are not divisions: a row or a corps whose fullest size is under a division's (`est`) neither draws nor gives.
// A historical scenario keeps the order of battle it was measured with: nothing here is read outside the grand campaign.
//
// The stood war rows (the design director's rulings of 16:42Z to 17:19Z, 25 November 2026, on the historian's read and
// the joiner's count: data/campaign/counting.json, joins/s-rows-count-1125.md). A dated war row of Italy's or
// Romania's gives no division a name the board knows, so nothing above found it: its divisions, which stood on the
// first day, were laid new and whole beside the board's own army that stands for the same men. The table names, for
// each such row, the board corps it draws on, and the same rule lays it:
//   counted    so many of the row's divisions are that corps' divisions: its last ones come over, each with its even
//              share, under its own name and number unless the row's name gives it one. A corps drawn whole comes itself.
//   by share   where the record counts more divisions in a board corps than the game does (the Army of the Po: ten in
//              the game's three) the row's divisions come with their share of the corps as it stands, so many of the
//              record's divisions out of those it still stands for, and the corps keeps its own divisions, the weaker.
//   stays      as above: a corps in the row's own side theatre, the player's, one that cannot leave: the row forms
//              nothing for those divisions, and the player's line tells him where they are.
//   gone       a counting formation that is dead holds nothing to draw: what it no longer holds is raised new, as a
//              division destroyed always was.
//   the war's end  a row keyed to a war is lent for that war, every nation's: when the war is over each division it drew
//              goes back to the corps it came out of, by the same rule as a loan's end, or stands on where it is. What
//              the row raised new stays (only a loan's new divisions leave), and the corps keeps the row's name and
//              orders; a corps that came itself has its own name and orders back.

import words from '../../data/campaign/drawn.json';
import counting from '../../data/campaign/counting.json';
import { addCorps, atWar, givesWay, hostileUnits, relocate, removeCorps } from './game';
import { divisionWords, divisionsNamed, divisionsOf, keepNumbers, ordinal, setStandInCount, standInCount, standInNumbers } from './armies';
import { answering, frontAnswering } from './ai';
import { seat } from './commanders';
import { contactRuleActs } from './history';
import type { Corps, Game, Kind, Region } from './types';

export const DRAWN = words as unknown as { one: string; many: string; here: string; hereMany: string; hereSea: string; hereSeaMany: string; manyPart: string; ashore: string; afloat: string; conv: string; stood: string; from: string; fromMany: string };

/** A stood war row's line of the table (counting.json): the war it waits for, the board corps it draws on in order, a share it takes where it stands. */
export interface Counted {
  war?: [string, string];
  /** `divs` of the row's divisions come out of `corps`; with `record`, by share: they stand for that many of the divisions the record counts in it. */
  from?: { corps: string; divs: number; record?: number }[];
  takes?: { name: string; share: number; leaves?: string; leavesHere?: string };
}
const COUNT = counting as unknown as { parents: Record<string, Record<string, { of: number }>>; rows: Record<string, Record<string, Counted>> };

/** The table's line for a dated row of a country's own, in the grand campaign; nothing elsewhere, so no historical scenario reads it. */
export const countingFor = (g: Game, nat: string, row: any[]): Counted | undefined => (g.flags.open === 1 && row[1] === nat ? COUNT.rows[nat]?.[row[2]] : undefined);
/** How many divisions the record counts in a board corps (the historian's read), where it differs from the game's own count. */
export const recordCount = (nat: string, corps: string): number | undefined => COUNT.parents[nat]?.[corps]?.of;

/** One division as it passes between corps: its place in the corps it stands in, its name and number, its men (0 to 1) and experience, and its corps' order and morale. */
interface Part { slot: number; name: [string, string]; no: number; str: number; xp: number; org: number; mor?: number }

/** A division a dated row names that is held, or formed out of one that is. */
export interface Claim {
  /** Its place among the divisions the row's name gives, from 0. */
  slot: number;
  /** The name the row gives it. */
  name: [string, string];
  /** The corps that holds it, and its place there; null for a division formed out of another where nobody has one to give. */
  from: Corps | null;
  at: number;
  /** Formed out of another division (the row's `was`): the name of the division it is formed out of. */
  conv?: string;
  /** It stays with its corps, and the row forms nothing in its place. */
  stays: boolean;
  /** (The stood war rows.) Found by the table's count in its counting formation, not by a name a corps holds. */
  counted?: true;
  /** (The same.) The row's own name gives this division: it carries that name where it comes. Else it keeps its card's own. */
  named?: true;
  /**
   * (The same.) Drawn by share: no card of the corps is taken (`at` is -1). It stands for `rec` of the `of` divisions
   * the record counts in the corps, and comes with that share of the corps as it stands.
   */
  share?: { rec: number; of: number };
}

/**
 * For a test or a trace to watch. `tell`: each dated row that names a division a living corps holds, as it is read on
 * its day (each claim's holder by name and id, and where it stands). `laid`: such a row as it is laid: the corps that
 * stands for it, whether a living corps came itself, the divisions raised new and the strength of each, and the
 * strength of all its country's corps in divisions before and after. Null in play: nothing is worked out for them.
 */
export const drawnWatch: {
  tell: ((e: { turn: number; nat: string; row: string; place: string; lands: string; home: boolean; divs: number; claims: { name: string; from: string | null; id: number | null; at: string | null; stays: boolean; conv: string | null; share: boolean }[] }) => void) | null;
  laid: ((e: { turn: number; nat: string; row: string; corps: number; took: boolean; fresh: number; str: number; before: number; after: number }) => void) | null;
  /** Each drawn division at a loan's or a war's end: `back` it rejoined its corps; else `near` true where its corps lived in the same side theatre (it went back before the rulings of 19:03Z and 19:17Z). */
  back: ((e: { turn: number; nat: string; name: string; war: boolean; back: boolean; near: boolean }) => void) | null;
} = { tell: null, laid: null, back: null };

const clamp = (x: number): number => Math.max(0, Math.min(1, x));

/** A corps' divisions as parts: each its name and number, and an even share of the corps as it stands. Together they are exactly the corps. */
function partsOf(g: Game, c: Corps): Part[] {
  return divisionsOf(g, c).map((d) => ({ slot: d.slot, name: [d.name, d.short] as [string, string], no: d.no, str: clamp(c.str), xp: clamp(c.xp), org: c.org, ...(c.mor !== undefined ? { mor: c.mor } : {}) }));
}

/** Makes a corps of these divisions: their men, their experience, order and morale by the men each brings, their names and their numbers. */
function assemble(g: Game, c: Corps, parts: Part[]): void {
  if (!parts.length) return;
  const men = parts.reduce((s, p) => s + p.str, 0);
  // (Divisions that all stand alike, as the ones a corps keeps do, are that figure itself: no sum is taken, so that
  // nothing drifts in the last place for being counted.)
  const one = parts[0], alike = parts.every((p) => p.str === one.str && p.xp === one.xp && p.org === one.org && p.mor === one.mor);
  const by = (f: (p: Part) => number, or: number): number => (alike ? f(one) : men > 1e-9 ? parts.reduce((s, p) => s + f(p) * p.str, 0) / men : or);
  const xp = by((p) => p.xp, c.xp), org = by((p) => p.org, c.org);
  const mor = c.mor !== undefined ? by((p) => p.mor ?? c.mor!, c.mor) : undefined;
  c.divs = parts.length;
  c.str = alike ? one.str : clamp(men / parts.length);
  c.xp = clamp(xp);
  c.org = clamp(org);
  if (mor !== undefined) c.mor = clamp(mor);
  c.names = parts.map((p) => p.name);
  if (g.flags.open === 1) keepNumbers(c, parts.map((p) => p.no));
  g.supplyDirty = true;
}

/** Takes the divisions at these places out of a corps and gives them back as parts; the corps keeps the rest, with their men, names and numbers. */
function splitOff(g: Game, c: Corps, slots: number[]): Part[] {
  const all = partsOf(g, c), pick = new Set(slots);
  assemble(g, c, all.filter((p) => !pick.has(p.slot)));
  return all.filter((p) => pick.has(p.slot));
}

/** The number a division's own name carries ("14th Panzer Division": 14), if it carries one. */
const numberIn = (name: string): number | undefined => { const m = /^(\d+)(?:st|nd|rd|th)\b/.exec(name); return m ? Number(m[1]) : undefined; };

/**
 * The name a corps takes from the divisions it holds: one division's own name, or the list of them as a dated row
 * writes one ("72nd and 164th Infantry Divisions"). Null where they do not read as divisions.
 */
export function formationName(names: [string, string][]): string | null {
  if (!names.length || names.some(([full]) => !/ Division$/.test(full))) return null;
  if (names.length === 1) return names[0][0];
  const items = names.map(([full]) => full.replace(/ Division$/, ''));
  const read = items.map((it) => /^(\d+(?:st|nd|rd|th))\s+(.+)$/.exec(it));
  const arm = read.every((m) => m && m[2] === read[0]![2]) ? read[0]![2] : null;
  const list = arm ? read.map((m) => m![1]) : items;
  return `${list.slice(0, -1).join(', ')} and ${list[list.length - 1]}${arm ? ` ${arm}` : ''} Divisions`;
}

/**
 * The divisions a dated row names that living corps of its country hold today, of the first `n` its name gives, with
 * where each one's corps stands by the rule: `reg` where the row lands, `home` true where it has fallen back to
 * forming at home, `skip` the corps the row's own words already account for (a listed `from`, a row that stands in
 * for this one), `busy` a corps no row may take (one that cannot leave where it is; the far side: one standing on a
 * live front of its staff's plan).
 * Forts and parts hold no division; an empty list is a row that is laid as it always was.
 * `leavingNow` (under the switch 'fromWait'): the corps the same act of the row takes away (game.ts `expeditions`: the
 * row's own `from` corps, the lent corps whose loan ends this pass, the stand-in this row stands down), which keep
 * nothing for a corps beside them, and neither does a corps this row's claims would empty (`emptied`).
 */
export function heldAlready(g: Game, nat: string, row: any[], o: any, n: number, reg: Region, home: boolean, skip: (c: Corps) => boolean, busy: (c: Corps) => boolean, leavingNow: ReadonlySet<Corps> = NO_LEAVING, heldBy?: (cs: Corps[]) => void): Claim[] {
  const names = divisionsNamed(row[2], nat, row[3] as Kind).slice(0, n);
  const count = countingFor(g, nat, row)?.from;
  if (!names.length && !count) return [];
  const whole = (c: Corps): boolean => c.nat === nat && !c.dead && c.kind !== 'fort' && !(c.full !== undefined && c.full < 1) && !skip(c);
  let held: Map<string, { c: Corps; at: number }> | null = null;
  // (A corps on a long passage is off the map and lives all the same: it holds its divisions, and stays with them.)
  const afloat = new Set<Corps>();
  const holder = (name: string): { c: Corps; at: number } | undefined => {
    if (!held) {
      held = new Map();
      for (const c of g.corps) if (whole(c)) for (const d of divisionsOf(g, c)) if (!held.has(d.name)) held.set(d.name, { c, at: d.slot });
      for (const v of g.atSea ?? []) if (whole(v.corps)) { afloat.add(v.corps); for (const d of divisionsOf(g, v.corps)) if (!held.has(d.name)) held.set(d.name, { c: v.corps, at: d.slot }); }
    }
    return held.get(name);
  };
  // The side theatre the row is for: its own place's, wherever it lands.
  const k = g.byName[row[5]]?.theatre ?? -1;
  // (Nor out of a fight, on either chart: a corps with an enemy at war on its province or beside it, holding the ground
  // or standing on it, keeps its divisions there, and the row forms without them, as for a corps that cannot leave.
  // A date never lifts a corps out of a fight: the design director's ruling of 18:47Z, 25 November 2026. `corpsInContact`.
  // That sentence acts at stops 3 to 5 only, on this door as on the 'from' rows': at the book's stops the book's rows
  // keep their book and draw a held division at its date, fight or not, still laid once. Her rulings of 21:25Z and 21:50Z.)
  // (Where the book of the row's own place is closed it acts there too: "a closed book brings the contact rule in", her
  // 22:05Z; history.ts `contactRuleActs`.)
  // (Under the switch 'fromWait' the one test reads who leaves in the same act: `gone`, set once every claim is known,
  // below. The design director's ruling of 06:10Z, 26 November 2026; with the switch off it reads nobody.)
  let gone: ReadonlySet<Corps> = NO_LEAVING;
  const fighting = (c: Corps): boolean => contactRuleActs(g, g.byName[row[5]]) && corpsInContact(g, nat, c, gone);
  // (A corps stays for a reason other than contact: home, the player's, afloat, where the row lands, in its theatre, busy.)
  const anyway = (c: Corps): boolean => home || nat === g.human || afloat.has(c) || c.reg === reg.id || (k >= 0 && g.regions[c.reg].theatre === k) || busy(c);
  const stays = (c: Corps): boolean => anyway(c) || fighting(c);
  const was = o.was as Record<string, string> | undefined;
  const out: Claim[] = [];
  // The divisions living corps hold under the row's own names first: a place one of them stands in is never asked for
  // again by a division formed out of another, wherever the two stand in the row's name.
  names.forEach((name, slot) => {
    const h = holder(name[0]);
    if (h) out.push({ slot, name, from: h.c, at: h.at, stays: false });
  });
  names.forEach((name, slot) => {
    const old = !holder(name[0]) ? was?.[name[0]] : undefined;
    if (!old) return;
    // Formed out of another division: from the corps that holds that one, or else out of the home army (never the
    // player's, unasked; never a corps that holds a division the row names or that another division of the row
    // is formed out of, so that no place is asked for twice).
    let h = holder(old);
    if (h && out.some((q) => q.from === h!.c && q.at === h!.at)) h = undefined;
    if (!h && nat !== g.human) {
      const named = new Set<Corps | null | undefined>([...names.map((nm) => holder(nm[0])?.c), ...out.map((q) => q.from)]);
      const c = g.corps.filter((x) => whole(x) && x.kind === 'inf' && !x.hold && x.town === undefined && x.divs >= 2 && g.regions[x.reg].owner === nat && !busy(x) && !named.has(x)).sort((a, b) => b.divs - a.divs || a.id - b.id)[0];
      if (c) h = { c, at: c.divs - 1 };
    }
    if (!h) { out.push({ slot, name, from: null, at: -1, conv: old, stays: true }); return; }
    out.push({ slot, name, from: h.c, at: h.at, conv: old, stays: false });
  });
  // The stood war rows: the divisions the table counts in a board corps, each entry the row's next places in order.
  // (A place a corps already holds by the row's own name is that corps', above. A counting formation that is dead, or
  // a part, or already another row's to break up, gives nothing: those places are raised new.)
  if (count) {
    for (const v of g.atSea ?? []) if (whole(v.corps)) afloat.add(v.corps);
    let next = 0;
    for (const f of count) {
      const first = next;
      next += f.divs;
      // (The oldest of that name, where two carry it. One on a long passage lives, and stays with its divisions.)
      const h = [...g.corps.filter(whole), ...afloat].filter((c) => c.name === f.corps).sort((a, b) => a.id - b.id)[0];
      if (!h) continue;
      const of = f.record !== undefined ? recordCount(nat, f.corps) : undefined;
      // (By share only while the record still counts divisions in it that no row has drawn.)
      if (of !== undefined && of - (h.out ?? 0) <= 0) continue;
      const places = Array.from({ length: f.divs }, (_, j) => first + j).filter((slot) => slot < n && !out.some((q) => q.slot === slot));
      const own = (slot: number): [string, string] | undefined => names[slot];
      if (of !== undefined) {
        for (const slot of places) out.push({ slot, name: own(slot) ?? ['', ''], from: h, at: -1, stays: false, counted: true, ...(own(slot) ? { named: true as const } : {}), share: { rec: f.record! / f.divs, of } });
        continue;
      }
      // Its last divisions, in the order they stand, but for one the row already holds by name, and for the ones that
      // already stand for an earlier row (`kept`: the cards counted as staying with it, which do not move). Where it
      // has fewer than the table counts, the row's last places are the ones raised new.
      const free = divisionsOf(g, h).filter((d) => !out.some((q) => q.from === h && q.at === d.slot));
      const cards = free.slice(0, Math.max(0, free.length - (h.kept ?? 0)));
      const last = cards.slice(Math.max(0, cards.length - places.length));
      last.forEach((d, j) => { const slot = places[j]; out.push({ slot, name: own(slot) ?? [d.name, d.short], from: h, at: d.slot, stays: false, counted: true, ...(own(slot) ? { named: true as const } : {}) }); });
    }
  }
  // Who stays, read once every claim is known (each claim was pushed with its place kept, `stays` false until here):
  // under the switch 'fromWait', a corps the row's claims would empty, all its divisions claimed and none of them by
  // share, and that has no reason but contact to stay (section 13 A11 of the design: exact), keeps nothing for a corps
  // beside it, and neither does a corps the same act takes away (`leavingNow`). The claim formed out of another with
  // nobody to give it (`from` null) stays as pushed.
  if (g.flags.fromWait === 1) {
    const emptied = new Set<Corps>(leavingNow);
    for (const h of new Set(out.map((q) => q.from).filter((x): x is Corps => !!x))) if (!anyway(h) && out.filter((q) => q.from === h && !q.share).length >= h.divs) emptied.add(h);
    gone = emptied;
  }
  for (const q of out) if (q.from) q.stays = stays(q.from);
  // (The corps that keep their claimed divisions for contact alone, no other reason: game.ts `noteHold`'s `claim`. Told
  // only where asked: the switch on.)
  if (heldBy) heldBy([...new Set(out.filter((q) => q.from && q.stays && !anyway(q.from)).map((q) => q.from!))]);
  // (In the order the row's name gives them, as they were always read.)
  out.sort((a, b) => a.slot - b.slot);
  if (out.length) drawnWatch.tell?.({ turn: g.turn, nat, row: row[2], place: row[5], lands: reg.name, home, divs: n, claims: out.map((q) => ({ name: q.name[0], from: q.from?.name ?? null, id: q.from?.id ?? null, at: q.from ? g.regions[q.from.reg].name : null, stays: q.stays, conv: q.conv ?? null, share: !!q.share })) });
  return out;
}

/** True where a row with these claims has anything to form: a division of its own to raise, or one to bring. */
export const formsAny = (claims: Claim[], n: number): boolean => n - claims.length > 0 || claims.some((q) => q.from && !q.stays);

/**
 * (The stood war rows.) What of its establishment a corps still holds as its own: a corps the record's divisions were
 * drawn out of by share keeps all its cards, and the drafts make good only the share that is still its own, never
 * what it gave (the design director's ruling of 19:03Z, 25 November 2026: price once, carried past the first day).
 * 1 for any other corps.
 */
export function ownShare(c: Corps): number {
  return c.out && c.outOf ? clamp(1 - c.out / c.outOf) : 1;
}

/**
 * The battle last resolved on a province (the contact rule's fight half, the design director's ruling of 06:10Z,
 * 26 November 2026: "a corps is held while it took part in the battle last resolved on its ground (the board's record,
 * no window)"): the ids of the living corps that took part, attackers and defenders, every side, in place of whatever
 * the province's record held. Written by game.ts `resolveBattle` alone, and only under the switch 'fromWait'. A battle
 * given up without fighting (a walk-in), a probe, an empty beach, gates opened or a town starved write nothing.
 */
export function recordBattle(g: Game, at: Region, who: Corps[]): void {
  (g.lastBattle ??= {})[at.id] = who.filter((c) => !c.dead).map((c) => c.id);
}

/**
 * A corps made of a corps that took part (a part broken off, a part rejoined, a fold's kept corps, a row's corps over):
 * it took part too, wherever the game carries `fought` from the one to the other.
 */
export function carryBattle(g: Game, from: number, to: number): void {
  if (g.flags.fromWait !== 1 || !g.lastBattle) return;
  for (const k in g.lastBattle) { const l = g.lastBattle[k]; if (l.includes(from) && !l.includes(to)) l.push(to); }
}

/** A cadre takes the field again under its old id (economy.ts, reform): it took part in nothing. */
export function dropBattle(g: Game, id: number): void {
  if (!g.lastBattle) return;
  for (const k in g.lastBattle) { const l = g.lastBattle[k]; const j = l.indexOf(id); if (j >= 0) l.splice(j, 1); }
}

/** No corps leaving: the contact test's reading where the same act takes nobody away. */
export const NO_LEAVING: ReadonlySet<Corps> = new Set();

/**
 * A corps its own orders bind to its place this month, as the general reads it: its order to hold where it stands
 * (`hold`: set from its row's words, kept for good by `stay`, and lifted only by the general's own orders, ai.ts
 * `release` and the plan's `freed`), which every picker of the general's (ai.ts) reads before it moves a corps. Never a
 * corps the player orders: the general moves none of his, and his own orders are his. Read as it stands when the test
 * reads it. Writes nothing. A held corps that answers this month is not bound: the general's own answer rule takes its
 * hold off for the month's fighting (ai.ts `withAnswer`, over the corps that `answering` and `frontAnswering` list), so
 * it is free to come on (`enemiesBeside`, the rule sceptic's finding S4-1, round 4).
 */
const boundHere = (g: Game, u: Corps): boolean => !!u.hold && u.nat !== g.human && u.joint !== g.human;

/**
 * The enemies at war with `nat` that hold or stand on c's province or one beside it; with `first`, the first only.
 * With `free` (the alone half's reading, R4-1): an enemy corps bound to its place (`boundHere`) is not one, nor the
 * province such a corps of its holder's stands on; one free corps of an enemy's beside makes it one.
 */
function enemiesBeside(g: Game, nat: string, c: Corps, first = false, free = false): string[] {
  const out: string[] = [];
  const add = (x: string): void => { if (!out.includes(x)) out.push(x); };
  // (The held corps of each enemy that answer this month, ai.ts `answering` and `frontAnswering`, the two lists the
  // general's `withAnswer` takes the hold off: read once a call, for an enemy one of whose held corps is beside.)
  const answer = new Map<string, Set<Corps>>();
  const bound = (u: Corps): boolean => {
    if (!boundHere(g, u)) return false;
    let a = answer.get(u.nat);
    if (!a) { a = new Set([...answering(g, u.nat), ...frontAnswering(g, u.nat)]); answer.set(u.nat, a); }
    return !a.has(u);
  };
  const look = (r: Region): boolean => {
    if (r.ctrl && atWar(g, nat, r.ctrl) && !(free && r.units.some((u) => !u.dead && u.nat === r.ctrl && bound(u)))) { add(r.ctrl); if (first) return true; }
    for (const u of r.units) if (!u.dead && atWar(g, nat, u.nat) && !(free && bound(u))) { add(u.nat); if (first) return true; }
    return false;
  };
  const r = g.regions[c.reg];
  if (look(r) && first) return out;
  for (const j of r.nb) if (look(g.regions[j]) && first) return out;
  return out;
}

/**
 * The contact test's reading, whole: `beside` (an enemy at war with `nat` holds or stands on c's province or one beside
 * it), `battle` (c took part in the battle last resolved on its ground: `g.lastBattle`, recordBattle above), `kept`
 * (for every enemy faced that its own orders do not bind to its place, `boundHere`, some unit other than c stands on
 * c's province's field at war with it: the game's own holding
 * test, game.ts `hostileUnits`; a unit the same act takes away, `leaving`, or one under an order to give its province up,
 * game.ts `givesWay`, keeps nothing). Only `corpsInContact` combines them; a gate's observer may read them. Nothing else
 * in src may hold a copy of the test (tests/contact-fact.test.ts reads every file for one).
 */
export function contactRead(g: Game, nat: string, c: Corps, leaving: ReadonlySet<Corps> = NO_LEAVING): { beside: boolean; battle: boolean; kept: boolean } {
  const foes = enemiesBeside(g, nat, c);
  const r = g.regions[c.reg];
  const battle = !c.dead && (g.lastBattle?.[c.reg]?.includes(c.id) ?? false);
  // (The alone half faces only the enemies free to come on, R4-1: one its own orders bind to its place keeps nothing
  // from c's province, so where every enemy beside is bound, the province is kept. The design director's (1) of
  // 16:07Z, 27 November 2026: "An enemy its own orders bind to its place is not "the enemy beside" for the alone half."
  // `beside`, which the fight half reads, is every enemy, bound or free.)
  const free = enemiesBeside(g, nat, c, false, true);
  const kept = foes.length > 0 && free.every((e) => hostileUnits(g, r, e).some((u) => u !== c && !u.dead && !leaving.has(u) && !givesWay(g, u)));
  return { beside: foes.length > 0, battle, kept };
}

/**
 * Whether a corps is held where it stands: the ONE contact test every piece reads, rows and the draw alike (the design
 * director's ruling of 06:10Z, 26 November 2026). With the switch 'fromWait' off: an enemy at war with its country
 * holds or stands on its province or one beside it, read as the board stands, exactly as before (her ruling of 18:47Z,
 * 25 November 2026: a date never lifts a corps out of a fight). On: held while it took part in the battle last
 * resolved on its ground, or else where it stands beside the enemy with nobody of its side to keep its province, both
 * only while an enemy is beside it (her answers 1 and 4 of 12:48Z). `leaving`: the corps the same act takes away, which
 * keep nothing. Readers: the war rows' draw (`heldAlready`, its `emptied` and the row's `leavingNow`), game.ts's
 * `expeditions` (the far side's and the old chart's rows that take a named corps whole, and the theatre draw), and the
 * telling (game.ts `rowHolds`, `atFrontNow`). Writes nothing.
 */
export function corpsInContact(g: Game, nat: string, c: Corps, leaving: ReadonlySet<Corps> = NO_LEAVING): boolean {
  if (g.flags.fromWait !== 1) return enemiesBeside(g, nat, c, true).length > 0;
  const k = contactRead(g, nat, c, leaving);
  return k.beside && (k.battle || !k.kept);
}

/**
 * A row has landed or settled: each card a stood war row counted as staying with its corps (the player's, or one that
 * stands where the row is for) now stands for that row, and the next row that counts on the corps takes the cards
 * before it (`Corps.kept`). Cards that come move, and need no mark. Called once a row is settled: game.ts.
 */
export function keepCounted(claims: Claim[]): void {
  for (const q of claims) if (q.counted && q.stays && q.from && q.at >= 0 && !q.share) q.from.kept = (q.from.kept ?? 0) + 1;
}

/**
 * Lays a dated row some of whose divisions living corps hold (`heldAlready`, and `formsAny` true): the corps that
 * stands for the row, and whether it is a living corps come itself (`took`). `as` the row's options as they apply
 * where it lands, `str` the strength of a division it raises new, `lent` true for a row whose time with its campaign
 * ends (`until`: only then is it remembered where each division came from), `add` raises its corps (game.ts addCorps,
 * by the row's own words), `cut` the share of the row that another row took before it landed (game.ts, o.takes).
 */
export function layOnce(g: Game, nat: string, row: any[], n: number, reg: Region, as: any, claims: Claim[], str: number, lent: boolean, add: (divs: number) => Corps, cut = 0): { c: Corps; took: boolean } {
  const kind = row[3] as Kind;
  const names = divisionsNamed(row[2], nat, kind);
  const all = (): number => g.corps.reduce((s, x) => s + (x.nat === nat && !x.dead ? x.divs * x.str : 0), 0);
  const before = drawnWatch.laid ? all() : 0;
  keepCounted(claims);
  // (A division drawn by share takes no card of its corps: `shares`. The others come as cards: `come`.)
  const shares = claims.filter((q) => q.from && !q.stays && q.share);
  const come = claims.filter((q) => q.from && !q.stays && !q.share);
  const fresh = Math.max(0, n - claims.length);
  // A corps all of whose divisions the row names comes itself, and takes the row's name: one of the row's own arm,
  // none of whose divisions is formed into another (an infantry corps is not a Panzer row's for giving its last division).
  const base = come.map((q) => q.from!).find((c) => c.kind === kind && come.filter((q) => q.from === c).length >= c.divs && !come.some((q) => q.from === c && q.conv)) ?? null;
  const got = new Map<Claim, Part>();
  const drew: NonNullable<Corps['drew']> = [];
  const take = (q: Claim, p: Part, from: number, formed = false, rec?: number): void => {
    const no = q.conv ? numberIn(q.name[0]) ?? p.no : p.no;
    // (`cut`: the share of the row another row took before it landed, o.takes: a division that comes is the weaker by it, as one raised new is.)
    got.set(q, { ...p, name: q.name, no, str: p.str * (1 - cut) });
    // (`rec`: a division an earlier war row drew by share keeps the record's divisions it stands for, so its corps is paid back when it goes home.)
    drew.push({ name: q.name, no, from, ...(q.conv || formed ? { conv: true as const } : {}), ...(rec ? { rec } : {}) });
  };
  for (const h of [...new Set(come.map((q) => q.from!))]) {
    const mine = come.filter((q) => q.from === h);
    if (h === base) {
      // (A division an earlier loan drew for this corps is remembered by the corps it first came out of: it goes back
      // there when this row's time is up, not to the corps that only carried it between two loans.)
      for (const p of partsOf(g, h)) { const q = mine.find((x) => x.at === p.slot); const old = h.drew?.find((r) => r.name[0] === p.name[0]); if (q) take(q, p, old?.from ?? h.id, !!old?.conv, old?.rec); }
    } else if (mine.length >= h.divs) {
      // Another corps the row names whole: its divisions come over, and it is no more.
      for (const p of partsOf(g, h)) { const q = mine.find((x) => x.at === p.slot); if (q) take(q, p, h.id); }
      removeCorps(g, h);
    } else {
      for (const p of splitOff(g, h, mine.map((q) => q.at))) take(mine.find((x) => x.at === p.slot)!, p, h.id);
    }
  }
  let c: Corps;
  let nos: number[];
  let raw: { str: number; xp: number; org: number; mor?: number };
  if (base) {
    c = base;
    // (A lent row remembers the name and orders it came under, the first such row's where one follows another. A row
    // that keeps it for good ends whatever an earlier loan remembered: nothing of it goes back any more.)
    if (lent) c.rowed ??= { name: c.name, hold: c.hold, ...(c.stay ? { stay: true } : {}) };
    else { delete c.rowed; delete c.drew; }
    if (c.reg !== reg.id) relocate(g, c, reg);
    c.hold = !!as.hold;
    if (as.stay) c.stay = true;
    if (as.airborne) c.airborne = true;
    if (as.gen && c.general === null) seat(g, c, as.gen, as.trait);
    nos = standInNumbers(g, nat, c.kind, fresh + shares.length);
    raw = { str, xp: as.xp ?? 0.15, org: 1, ...(c.mor !== undefined ? { mor: c.mor } : {}) };
  } else {
    // (A corps is raised with one place at least: where the row raises no division of its own, the number that place was given goes back.)
    // (A division drawn by share is numbered as a new one is: no card of its corps came with it.)
    const before = standInCount(g, nat, kind), places = fresh + shares.length;
    c = add(Math.max(1, places));
    c.str = str;
    nos = places > 0 ? divisionsOf(g, c).map((d) => d.no) : [];
    if (places <= 0) setStandInCount(g, nat, kind, before);
    raw = { str: c.str, xp: c.xp, org: c.org, ...(c.mor !== undefined ? { mor: c.mor } : {}) };
  }
  // The stood war rows, by share: of the divisions the record still counts in the corps, the row's are so many, and
  // they come with that share of the corps as it stands, evenly between them and never more than whole. The corps
  // keeps its own divisions, the weaker by exactly what went, and remembers how many of the record's are out.
  const lot = new Map<Corps, number>();
  for (const h of [...new Set(shares.map((q) => q.from!))]) {
    const mine = shares.filter((q) => q.from === h);
    const recs = mine.reduce((sum, q) => sum + q.share!.rec, 0);
    const left = Math.max(recs, mine[0].share!.of - (h.out ?? 0));
    const men = h.divs * clamp(h.str);
    const took = Math.min((recs / left) * men, mine.length);
    h.str = clamp((men - took) / h.divs);
    h.out = (h.out ?? 0) + recs;
    if (h.outOf === undefined) h.outOf = mine[0].share!.of;
    lot.set(h, took / mine.length);
    g.supplyDirty = true;
  }
  // The row's divisions in the order its name gives them: those it brought, and those it raises new.
  const parts: Part[] = [];
  const word = divisionWords(nat, c.kind);
  let f = 0;
  for (let s = 0; s < n; s++) {
    const q = claims.find((x) => x.slot === s);
    if (q?.share) {
      const h = q.from;
      if (!h || q.stays) continue;
      const no = nos[f++] ?? 0;
      const name: [string, string] = q.named ? q.name : [`${ordinal(no)} ${word.name}`, `${ordinal(no)} ${word.short}`];
      parts.push({ slot: s, name, no, str: (lot.get(h) ?? 0) * (1 - cut), xp: clamp(h.xp), org: h.org, ...(h.mor !== undefined ? { mor: h.mor } : {}) });
      drew.push({ name, no, from: h.id, rec: q.share.rec });
      continue;
    }
    if (q) { const p = got.get(q); if (p) parts.push(p); continue; }
    const no = nos[f++] ?? 0;
    parts.push({ slot: s, name: names[s] ?? [`${ordinal(no)} ${word.name}`, `${ordinal(no)} ${word.short}`], no, ...raw });
  }
  assemble(g, c, parts);
  // It carries the row's name whatever it holds of the row's divisions: the rows that build on this one find it by
  // that name. Its cards read the divisions it holds (`names`).
  c.name = row[2];
  if (lent && drew.length) c.drew = [...(c.drew ?? []).filter((r) => !drew.some((d) => d.name[0] === r.name[0])), ...drew];
  drawnWatch.laid?.({ turn: g.turn, nat, row: row[2], corps: c.id, took: !!base, fresh, str, before, after: all() });
  return { c, took: !!base };
}

/**
 * A lent row's time with its campaign is up, for a corps that holds divisions the row drew (`drew`): each goes back
 * to the corps it came out of, where that corps lives and the two stand together, in one province (which is in one
 * side theatre: the bound of 14:28Z on which parent it may rejoin, never a move). Apart from it, or with its corps
 * gone, it stands on where it is, under its own name and free for its staff: a return never moves a division for
 * free, at a lent row's end as at a war's (the design director's rulings of 19:03Z and 19:17Z, 25 November 2026).
 * The divisions the row raised new leave. True where the corps stands on, with what could not go back, under its own name again and free
 * of the row's orders; false where nothing of it is left to stand, and it leaves as a lent row's corps always did.
 * `leaving` a corps that is itself on its way out this month: nothing goes back to it.
 * `war` true for a row whose war is over (the stood war rows, above): the divisions it raised new stay with it, and it
 * keeps the row's name and orders. A division drawn by share goes back as a division of its own, and the corps it
 * joins counts the record's divisions it stands for as its own again. A drawn division goes back only to a corps it
 * stands together with, in one province; one apart from it, or whose corps is no more, stands on where it is under
 * its own name, free for its staff, which sends it home by the rules if it wants: a return never moves a division for
 * free (the design director's ruling of 19:03Z, 25 November 2026). It leaves the row's corps as a corps of its own,
 * or, where the row's corps holds nothing else, that corps takes its name and is free of the row's orders.
 */
/**
 * Whether `rowOver` would leave the corps standing (true) or see it go (false), read without doing it: the same reading
 * of its divisions, and nothing written, no watch told. game.ts `loanHeld` reads it under the switch 'fromWait': a
 * loan's end that leaves its corps standing where it is takes nobody from the front, so the one test has nothing to
 * hold there, and the loan ends as it always did (the rule sceptic's finding S1-9 (a), round 3).
 */
export function rowStandsOn(g: Game, c: Corps, leaving: (x: Corps) => boolean = () => false, war = false): boolean {
  return partsOf(g, c).some((p) => {
    const rec = c.drew?.find((r) => r.name[0] === p.name[0]);
    if (!rec) return war;
    const own = rec.from === c.id || !!rec.conv;
    const parent = own ? undefined : g.corps.find((x) => x.id === rec.from && x.nat === c.nat && !x.dead && !leaving(x));
    return !(parent && parent.reg === c.reg);
  });
}

export function rowOver(g: Game, c: Corps, leaving: (x: Corps) => boolean = () => false, war = false): boolean {
  const keep: Part[] = [];
  // (The war over: the drawn divisions that stand on apart from the corps they came out of.)
  const apart: Part[] = [];
  const back = new Map<Corps, Part[]>();
  const home = new Map<Corps, number>();
  for (const p of partsOf(g, c)) {
    const rec = c.drew?.find((r) => r.name[0] === p.name[0]);
    if (!rec) { if (war) keep.push(p); continue; }
    const own = rec.from === c.id || !!rec.conv;
    const parent = own ? undefined : g.corps.find((x) => x.id === rec.from && x.nat === c.nat && !x.dead && !leaving(x));
    if (!own) drawnWatch.back?.({ turn: g.turn, nat: c.nat, name: p.name[0], war, back: !!parent && parent.reg === c.reg, near: !!parent && g.regions[parent.reg].theatre === g.regions[c.reg].theatre });
    if (parent && parent.reg === c.reg) {
      back.set(parent, [...(back.get(parent) ?? []), p]);
      if (rec.rec) home.set(parent, (home.get(parent) ?? 0) + rec.rec);
    }
    else (war && !own ? apart : keep).push(p);
  }
  for (const [parent, ps] of back) {
    assemble(g, parent, [...partsOf(g, parent), ...ps]);
    const rec = home.get(parent);
    if (rec) { const out = (parent.out ?? 0) - rec; if (out > 1e-9) parent.out = out; else { delete parent.out; delete parent.outOf; } }
    // (A parent that is itself on loan holds them as its own from here: when its own row's time is up they stand on
    // with it, whichever of the two rows is read first, and do not leave as divisions that row raised new. One it had
    // itself drawn from another corps keeps that record: it goes back there when the parent's own time is up.)
    if (parent.drew || parent.rowed) {
      const had = parent.drew ?? [];
      parent.drew = [...had, ...ps.filter((p) => !had.some((r) => r.name[0] === p.name[0])).map((p) => ({ name: p.name, no: p.no, from: parent.id }))];
    }
  }
  // (A corps of their own beside the row's, where the row's corps keeps divisions of its own; the row's corps itself,
  // free of its orders, where it keeps nothing else. Its numbers are theirs: no stand-in number is spent on it.)
  let freed = false;
  if (apart.length && keep.length) {
    const before = standInCount(g, c.nat, c.kind);
    const f = addCorps(g, c.nat, formationName(apart.map((p) => p.name)) ?? c.name, c.kind, apart.length, c.reg, {});
    setStandInCount(g, c.nat, c.kind, before);
    f.ap = c.ap;
    f.apMax = c.apMax;
    f.supplied = c.supplied;
    f.fought = c.fought;
    carryBattle(g, c.id, f.id);
    if (c.mor !== undefined) f.mor = c.mor; else delete f.mor;
    assemble(g, f, apart);
  } else if (apart.length) { keep.push(...apart); freed = true; }
  if (!keep.length) return false;
  assemble(g, c, keep);
  if (c.rowed) {
    c.name = c.rowed.name;
    c.hold = c.rowed.hold;
    if (c.rowed.stay) c.stay = true; else delete c.stay;
  } else if (!war || freed) {
    c.name = formationName(keep.map((p) => p.name)) ?? c.name;
    c.hold = false;
    delete c.stay;
  }
  // (A war row's corps is not on its way anywhere: it keeps the row's name, by which the rows that build on it find
  // it, and the row's own orders, which never hung on the war. Only the divisions that could go back have gone.)
  delete c.drew;
  delete c.rowed;
  return true;
}

/** Every division name the living corps of a country hold, a corps on a long passage among them, but for one corps. Forts hold none. */
function heldBut(g: Game, c: Corps): Set<string> {
  const out = new Set<string>();
  for (const x of g.corps) if (x !== c && x.nat === c.nat && !x.dead && x.kind !== 'fort') for (const d of divisionsOf(g, x)) out.add(d.name);
  for (const v of g.atSea ?? []) if (v.corps !== c && v.nat === c.nat && !v.corps.dead && v.corps.kind !== 'fort') for (const d of divisionsOf(g, v.corps)) out.add(d.name);
  return out;
}

/**
 * A corps takes more divisions in from a dated row (`joins`), and the row's name with them. That name lists the whole
 * corps after the join, the old divisions first, and before the rule the corps simply read it: so a name on that
 * list that another living corps held was read twice. Now the corps carries its divisions' names written out:
 * `before` what it read as it stood, taken before it grew (game.ts). A division the row's name still lists keeps its
 * place, and so does one a loan must give back by its name; the new places take the last of the row's names that no
 * living corps holds, and read by their numbers where there is none; a division the name has dropped (the record
 * exchanged it: the VI Corps of Salerno is not the VI Corps of Anzio) takes a name that is left and that nobody
 * holds, and keeps its own where there is none. Where the name gives fewer divisions than the corps has, the old
 * ones keep their names. No name another living corps holds is ever taken. `added` the divisions the row brought,
 * already counted in the corps.
 */
export function joinNames(g: Game, c: Corps, row: any[], added: number, before: [string, string][]): void {
  if (added <= 0) return;
  const was = before.slice(0, c.divs - added);
  const gives = divisionsNamed(row[2], c.nat, row[3] as Kind);
  const whole = gives.length >= c.divs;
  const owed = new Set<string>((c.drew ?? []).map((r) => r.name[0]));
  const elsewhere = heldBut(g, c);
  const out: ([string, string] | undefined)[] = was.map((nm) => (!whole || owed.has(nm[0]) || gives.some(([full]) => full === nm[0]) ? nm : undefined));
  const mine = new Set<string>(out.filter((nm) => !!nm).map((nm) => nm![0]));
  const free = gives.filter(([full]) => !mine.has(full) && !elsewhere.has(full));
  const fresh = free.splice(Math.max(0, free.length - added), added);
  was.forEach((nm, i) => { if (!out[i]) out[i] = free.shift() ?? nm; });
  c.names = [...(out as [string, string][]), ...fresh];
}

/** A corps' divisions' names as it reads them now, to be written out. */
export const namesNow = (g: Game, c: Corps): [string, string][] => divisionsOf(g, c).map((d) => [d.name, d.short] as [string, string]);

/**
 * A row that adds divisions to a corps already landed (`joins`), laid by itself because that corps is gone: its name
 * lists the whole corps after the join, the old divisions first, so the ones it brings are the last of its names that
 * no living corps holds; with too few of those the rest read by their numbers. Before the rule it read the first
 * names of its list, the old corps' own, whoever held them.
 */
export function joinsAlone(g: Game, c: Corps, row: any[]): void {
  const gives = divisionsNamed(row[2], c.nat, row[3] as Kind);
  if (!gives.length) return;
  const elsewhere = heldBut(g, c);
  const free = gives.filter(([full]) => !elsewhere.has(full));
  const word = divisionWords(c.nat, c.kind);
  const fresh = free.slice(Math.max(0, free.length - c.divs));
  const cards = divisionsOf(g, c);
  // (Named from the first place on; a place left over reads by its number, written out so that it never reads the row's list.)
  c.names = cards.map((d, i) => fresh[i] ?? [`${ordinal(d.no)} ${word.name}`, `${ordinal(d.no)} ${word.short}`]);
}

/**
 * A cadre takes the field again under the names its divisions had (economy.ts, reform). One whose name a living corps
 * has come to hold meanwhile, laid by a dated row while the cadre was off the map, reads by a new number of its
 * nation's: the row's division is the one of that name. The grand campaign only.
 */
export function reformNames(g: Game, c: Corps): void {
  if (g.flags.open !== 1 || c.kind === 'fort') return;
  const cards = divisionsOf(g, c);
  const elsewhere = heldBut(g, c);
  const hit = cards.filter((d) => elsewhere.has(d.name));
  if (!hit.length) return;
  const nos = standInNumbers(g, c.nat, c.kind, hit.length);
  const word = divisionWords(c.nat, c.kind);
  let k = 0;
  const now = cards.map((d) => { if (!elsewhere.has(d.name)) return { no: d.no, name: [d.name, d.short] as [string, string] }; const no = nos[k++]; return { no, name: [`${ordinal(no)} ${word.name}`, `${ordinal(no)} ${word.short}`] as [string, string] }; });
  c.names = now.map((p) => p.name);
  keepNumbers(c, now.map((p) => p.no));
}

/** The line the player reads when a dated arrival names divisions he already holds, or null where it names none of his. */
export function heldLine(g: Game, row: any[], claims: Claim[]): string | null {
  const the = (name: string): string => (/^the /i.test(name) ? name : `the ${name}`);
  const list = (xs: string[]): string => (xs.length > 1 ? `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}` : xs[0] ?? '');
  const cap = (s: string): string => s.charAt(0).toUpperCase() + s.slice(1);
  // (Where his own stand: in the place, or, for a corps on a passage, at sea and bound for its harbour, never the port it
  // sailed from. The UI director's marks of 21:52Z.)
  const bound = (c: Corps): string | undefined => { const v = (g.atSea ?? []).find((x) => x.corps === c); return v ? g.regions[v.to].name : undefined; };
  const at = (cs: Corps[]): string => {
    const ashore = [...new Set(cs.filter((c) => bound(c) === undefined).map((c) => g.regions[c.reg].name))];
    const afloat = [...new Set(cs.map(bound).filter((x): x is string => x !== undefined))];
    return list([...(ashore.length ? [fill(DRAWN.ashore, { where: list(ashore) })] : []), ...(afloat.length ? [fill(DRAWN.afloat, { to: list(afloat) })] : [])]);
  };
  const where = (qs: Claim[]): string => at(qs.map((q) => q.from!));
  const lines: string[] = [];
  // (The stood war rows: a division the table counts in one of his corps under a name the row gives it, or by share,
  // is not one he holds by name: `drawn`, told by the corps it was to be drawn from. One counted under its card's own
  // name he holds, and it is told with the others.)
  const drawn = claims.filter((q) => q.from && q.counted && (q.named || q.share));
  const mine = claims.filter((q) => q.from && !q.conv && !drawn.includes(q)), made = claims.filter((q) => q.from && q.conv);
  // (A line may say a place more than once: every one of its places is filled.)
  const fill = (line: string, as: Record<string, string>): string => line.replace(/\{(\w+)\}/g, (all, k: string) => as[k] ?? all);
  // (Divisions that read by numbers of one arm are said as a dated row writes them: "the 9th and 10th Infantry
  // Divisions"; the others each with its article. The UI director's mark of 17:26Z.)
  const said = (qs: Claim[]): string => {
    const read = qs.map((q) => /^(\d+(?:st|nd|rd|th))\s+(.+) Division$/.exec(q.name[0]));
    const joined = qs.length > 1 && read.every((m) => m && m[2] === read[0]![2]) ? formationName(qs.map((q) => q.name)) : null;
    return cap(joined ? the(joined) : list(qs.map((q) => the(q.name[0]))));
  };
  // (Where every one of them stands at the row's own place already, the place is said once: the UI director's `here`.)
  const there = mine.length > 0 && mine.every((q) => bound(q.from!) === undefined && g.regions[q.from!.reg].name === row[5]);
  // (Every one of them on a passage bound for the row's own place: he has sent them already. The UI director's 21:52Z.)
  const sailing = mine.length > 0 && mine.every((q) => bound(q.from!) === row[5]);
  // (Some on their way there and some not: the promise is only of those not. The UI director's 21:59Z.)
  const partly = !sailing && mine.some((q) => bound(q.from!) === row[5]);
  const one = mine.length === 1;
  if (mine.length) lines.push(fill(there ? (one ? DRAWN.here : DRAWN.hereMany) : sailing ? (one ? DRAWN.hereSea : DRAWN.hereSeaMany) : one ? DRAWN.one : partly ? DRAWN.manyPart : DRAWN.many, { names: said(mine), place: row[5], at: where(mine) }));
  for (const q of made) lines.push(fill(DRAWN.conv, { names: cap(the(q.name[0])), place: row[5], was: q.conv!, at: where([q]) }));
  // One line for each corps the row was to draw on: a division the row names by itself, or so many to make up the row.
  for (const h of [...new Set(drawn.map((q) => q.from!))]) {
    const qs = drawn.filter((q) => q.from === h);
    const named = qs.filter((q) => q.named), rest = qs.filter((q) => !q.named);
    const as = { place: row[5], army: h.name.replace(/^the /i, ''), at: at([h]), formation: String(row[2]).replace(/^the /i, '') };
    for (const q of named) lines.push(fill(DRAWN.from, { ...as, names: cap(the(q.name[0])) }));
    if (rest.length === 1) lines.push(fill(DRAWN.from, { ...as, names: 'One division' }));
    else if (rest.length) lines.push(fill(DRAWN.fromMany, { ...as, count: cap(inWords(rest.length)) }));
  }
  return lines.length ? lines.join(' ') : null;
}

/** A count as the news writes it: in words to ten. */
const inWords = (n: number): string => ['no', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten'][n] ?? String(n);
