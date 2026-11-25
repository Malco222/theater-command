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
//   the end    when a lent row's time is up (`until`), each division it drew goes back to the corps it came out of,
//              where that corps lives and stands in the same side theatre (the main fronts are one); otherwise it
//              stands on where it is, as a corps of its own under its own name, free for its staff to move. A division
//              formed out of another never goes back. The divisions the row raised new leave, as they always did.
//              Nothing goes back to a corps that is itself leaving that month.
// Parts are not divisions: a row or a corps whose fullest size is under a division's (`est`) neither draws nor gives.
// A historical scenario keeps the order of battle it was measured with: nothing here is read outside the grand campaign.

import words from '../../data/campaign/drawn.json';
import { relocate, removeCorps } from './game';
import { divisionWords, divisionsNamed, divisionsOf, keepNumbers, ordinal, setStandInCount, standInCount, standInNumbers } from './armies';
import { seat } from './commanders';
import type { Corps, Game, Kind, Region } from './types';

export const DRAWN = words as unknown as { one: string; many: string; conv: string; stood: string };

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
}

/**
 * For a test or a trace to watch. `tell`: each dated row that names a division a living corps holds, as it is read on
 * its day (each claim's holder by name and id, and where it stands). `laid`: such a row as it is laid: the corps that
 * stands for it, whether a living corps came itself, the divisions raised new and the strength of each, and the
 * strength of all its country's corps in divisions before and after. Null in play: nothing is worked out for them.
 */
export const drawnWatch: {
  tell: ((e: { turn: number; nat: string; row: string; place: string; lands: string; home: boolean; divs: number; claims: { name: string; from: string | null; id: number | null; at: string | null; stays: boolean; conv: string | null }[] }) => void) | null;
  laid: ((e: { turn: number; nat: string; row: string; corps: number; took: boolean; fresh: number; str: number; before: number; after: number }) => void) | null;
} = { tell: null, laid: null };

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
 */
export function heldAlready(g: Game, nat: string, row: any[], o: any, n: number, reg: Region, home: boolean, skip: (c: Corps) => boolean, busy: (c: Corps) => boolean): Claim[] {
  const names = divisionsNamed(row[2], nat, row[3] as Kind).slice(0, n);
  if (!names.length) return [];
  const whole = (c: Corps): boolean => c.nat === nat && !c.dead && c.kind !== 'fort' && !(c.full !== undefined && c.full < 1) && !skip(c);
  let held: Map<string, { c: Corps; at: number }> | null = null;
  const holder = (name: string): { c: Corps; at: number } | undefined => {
    if (!held) {
      held = new Map();
      for (const c of g.corps) if (whole(c)) for (const d of divisionsOf(g, c)) if (!held.has(d.name)) held.set(d.name, { c, at: d.slot });
    }
    return held.get(name);
  };
  // The side theatre the row is for: its own place's, wherever it lands.
  const k = g.byName[row[5]]?.theatre ?? -1;
  const stays = (c: Corps): boolean => home || nat === g.human || c.reg === reg.id || (k >= 0 && g.regions[c.reg].theatre === k) || busy(c);
  const was = o.was as Record<string, string> | undefined;
  const out: Claim[] = [];
  names.forEach((name, slot) => {
    let h = holder(name[0]);
    const old = !h ? was?.[name[0]] : undefined;
    if (old) {
      // Formed out of another division: from the corps that holds that one, or else out of the home army (never the
      // player's, unasked; never a corps that holds a division the row names, so that no place is asked for twice).
      h = holder(old);
      if (!h && nat !== g.human) {
        const named = new Set(names.map((nm) => holder(nm[0])?.c).filter((x) => !!x));
        const c = g.corps.filter((x) => whole(x) && x.kind === 'inf' && !x.hold && x.town === undefined && x.divs >= 2 && g.regions[x.reg].owner === nat && !busy(x) && !named.has(x)).sort((a, b) => b.divs - a.divs || a.id - b.id)[0];
        if (c) h = { c, at: c.divs - 1 };
      }
      if (!h) { out.push({ slot, name, from: null, at: -1, conv: old, stays: true }); return; }
    }
    if (!h) return;
    out.push({ slot, name, from: h.c, at: h.at, ...(old ? { conv: old } : {}), stays: stays(h.c) });
  });
  if (out.length) drawnWatch.tell?.({ turn: g.turn, nat, row: row[2], place: row[5], lands: reg.name, home, divs: n, claims: out.map((q) => ({ name: q.name[0], from: q.from?.name ?? null, id: q.from?.id ?? null, at: q.from ? g.regions[q.from.reg].name : null, stays: q.stays, conv: q.conv ?? null })) });
  return out;
}

/** True where a row with these claims has anything to form: a division of its own to raise, or one to bring. */
export const formsAny = (claims: Claim[], n: number): boolean => n - claims.length > 0 || claims.some((q) => q.from && !q.stays);

/**
 * Lays a dated row some of whose divisions living corps hold (`heldAlready`, and `formsAny` true): the corps that
 * stands for the row, and whether it is a living corps come itself (`took`). `as` the row's options as they apply
 * where it lands, `str` the strength of a division it raises new, `lent` true for a row whose time with its campaign
 * ends (`until`: only then is it remembered where each division came from), `add` raises its corps (game.ts addCorps,
 * by the row's own words).
 */
export function layOnce(g: Game, nat: string, row: any[], n: number, reg: Region, as: any, claims: Claim[], str: number, lent: boolean, add: (divs: number) => Corps): { c: Corps; took: boolean } {
  const kind = row[3] as Kind;
  const names = divisionsNamed(row[2], nat, kind);
  const all = (): number => g.corps.reduce((s, x) => s + (x.nat === nat && !x.dead ? x.divs * x.str : 0), 0);
  const before = drawnWatch.laid ? all() : 0;
  const come = claims.filter((q) => q.from && !q.stays);
  const fresh = Math.max(0, n - claims.length);
  // A corps all of whose divisions the row names comes itself, and takes the row's name: one of the row's own arm,
  // none of whose divisions is formed into another (an infantry corps is not a Panzer row's for giving its last division).
  const base = come.map((q) => q.from!).find((c) => c.kind === kind && come.filter((q) => q.from === c).length >= c.divs && !come.some((q) => q.from === c && q.conv)) ?? null;
  const got = new Map<Claim, Part>();
  const drew: NonNullable<Corps['drew']> = [];
  const take = (q: Claim, p: Part, from: number): void => {
    const no = q.conv ? numberIn(q.name[0]) ?? p.no : p.no;
    got.set(q, { ...p, name: q.name, no });
    drew.push({ name: q.name, no, from, ...(q.conv ? { conv: true as const } : {}) });
  };
  for (const h of [...new Set(come.map((q) => q.from!))]) {
    const mine = come.filter((q) => q.from === h);
    if (h === base) {
      for (const p of partsOf(g, h)) { const q = mine.find((x) => x.at === p.slot); if (q) take(q, p, h.id); }
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
    if (as.gen && c.general === null) seat(g, c, as.gen, as.trait);
    nos = standInNumbers(g, nat, c.kind, fresh);
    raw = { str, xp: as.xp ?? 0.15, org: 1, ...(c.mor !== undefined ? { mor: c.mor } : {}) };
  } else {
    // (A corps is raised with one place at least: where the row raises no division of its own, the number that place was given goes back.)
    const before = standInCount(g, nat, kind);
    c = add(Math.max(1, fresh));
    c.str = str;
    nos = fresh > 0 ? divisionsOf(g, c).map((d) => d.no) : [];
    if (fresh <= 0) setStandInCount(g, nat, kind, before);
    raw = { str: c.str, xp: c.xp, org: c.org, ...(c.mor !== undefined ? { mor: c.mor } : {}) };
  }
  // The row's divisions in the order its name gives them: those it brought, and those it raises new.
  const parts: Part[] = [];
  const word = divisionWords(nat, c.kind);
  let f = 0;
  for (let s = 0; s < n; s++) {
    const q = claims.find((x) => x.slot === s);
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
 * to the corps it came out of, where that corps lives and stands in the same side theatre; the divisions the row
 * raised new leave. True where the corps stands on, with what could not go back, under its own name again and free
 * of the row's orders; false where nothing of it is left to stand, and it leaves as a lent row's corps always did.
 * `leaving` a corps that is itself on its way out this month: nothing goes back to it.
 */
export function rowOver(g: Game, c: Corps, leaving: (x: Corps) => boolean = () => false): boolean {
  const keep: Part[] = [];
  const back = new Map<Corps, Part[]>();
  for (const p of partsOf(g, c)) {
    const rec = c.drew?.find((r) => r.name[0] === p.name[0]);
    if (!rec) continue;
    const parent = rec.from === c.id || rec.conv ? undefined : g.corps.find((x) => x.id === rec.from && x.nat === c.nat && !x.dead && !leaving(x));
    if (parent && g.regions[parent.reg].theatre === g.regions[c.reg].theatre) back.set(parent, [...(back.get(parent) ?? []), p]);
    else keep.push(p);
  }
  for (const [parent, ps] of back) assemble(g, parent, [...partsOf(g, parent), ...ps]);
  if (!keep.length) return false;
  assemble(g, c, keep);
  if (c.rowed) {
    c.name = c.rowed.name;
    c.hold = c.rowed.hold;
    if (c.rowed.stay) c.stay = true; else delete c.stay;
  } else {
    c.name = formationName(keep.map((p) => p.name)) ?? c.name;
    c.hold = false;
    delete c.stay;
  }
  delete c.drew;
  delete c.rowed;
  return true;
}

/**
 * A corps that carries its divisions' own names (one the rule has laid, or drawn a division out of) takes more
 * divisions in from a dated row (`joins`): the new ones are named as the row's name gives them. That name lists the
 * whole corps after the join, so the ones it brings are those no living corps of the country holds; where the name
 * gives too few, the rest read by their numbers. Without this they would all read by numbers, and a later row that
 * names one of them would not find it held. `added` the divisions the row brought, already counted in the corps.
 */
export function joinNames(g: Game, c: Corps, row: any[], added: number): void {
  if (!c.names || added <= 0) return;
  const kept = c.names.slice(0, c.divs - added);
  const held = new Set<string>(kept.map(([full]) => full));
  for (const x of g.corps) if (x !== c && x.nat === c.nat && !x.dead && x.kind !== 'fort') for (const d of divisionsOf(g, x)) held.add(d.name);
  const fresh = divisionsNamed(row[2], c.nat, row[3] as Kind).filter(([full]) => !held.has(full)).slice(0, added);
  c.names = [...kept, ...fresh];
}

/** The line the player reads when a dated arrival names divisions he already holds, or null where it names none of his. */
export function heldLine(g: Game, row: any[], claims: Claim[]): string | null {
  const the = (name: string): string => (/^the /i.test(name) ? name : `the ${name}`);
  const list = (xs: string[]): string => (xs.length > 1 ? `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}` : xs[0] ?? '');
  const cap = (s: string): string => s.charAt(0).toUpperCase() + s.slice(1);
  const where = (qs: Claim[]): string => list([...new Set(qs.map((q) => g.regions[q.from!.reg].name))]);
  const lines: string[] = [];
  const mine = claims.filter((q) => q.from && !q.conv), made = claims.filter((q) => q.from && q.conv);
  if (mine.length) lines.push((mine.length === 1 ? DRAWN.one : DRAWN.many).replace('{names}', cap(list(mine.map((q) => the(q.name[0]))))).replace('{place}', row[5]).replace('{where}', where(mine)));
  for (const q of made) lines.push(DRAWN.conv.replace('{names}', cap(the(q.name[0]))).replace('{place}', row[5]).replace('{was}', q.conv!).replace('{where}', where([q])));
  return lines.length ? lines.join(' ') : null;
}
