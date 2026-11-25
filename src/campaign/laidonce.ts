// A division is laid once (1.5.3, the grand campaign; data/campaign/laidonce.json).
//
// The design director's rulings of 13:43Z and 19:17Z, 25 November 2026, on the joiner's six-year check
// (joins/named-twice-check-1125.md) and the historian's read of each row (historian/named-twice-reads-1125.md). Seven
// dated rows named a division a living corps of their country already held, and raised it a second time at full
// strength. For those rows only: a division a living corps holds is drawn out of it, with its own strength, experience,
// order and morale, instead of being raised again. It never leaves the side theatre the row is for, a fight (an enemy at
// war on its province or beside it: her ruling of 18:47Z), a town the enemy stands round, a passage at sea, or the
// player's own corps: there it stays, and the row comes without it. A row lent for a while gives its drawn divisions
// back at the loan's end where their corps stands with it, bringing the row's corps' figures as they stand (her ruling of
// 19:03Z, as release 2 reads it: the corps' men and experience are one); otherwise they stand on where they are, free.
// Only what the row raised new leaves (her 19:17Z: a return never moves a division for free).
//
// History's campaigns keep the order of battle they were measured with: nothing here runs outside the grand campaign.
import data from '../../data/campaign/laidonce.json';
import setup from '../../data/campaign/setup-1939.json';
import { divisionsOf, growCorps, keepNumbers, numbersOf } from './armies';
import { atWar, removeCorps } from './game';
import { onBook } from './history';
import type { Corps, Game, Region, Voyage } from './types';

/**
 * A holder of a row's division: its corps' name, the divisions it gives, the numbers they carry where the record knows
 * them, and (optional) a key shared by holders that stand for the same division: the first of them alive is read, the
 * rest not (W Force and the 2nd New Zealand Division, which stands in for it, both carry the New Zealand Division).
 */
type Holder = [string, number, number[], string?];
const ROWS = new Map<string, Holder[]>(Object.entries((data as unknown as { rows: Record<string, Holder[]> }).rows));
const WORDS = data as unknown as { from: string; fromMany: string; held: string; heldMany: string; ashore: string; afloat: string };

/** A watch on the rule, for the patch's gate (null in play): each read row as it comes, what it drew and what stayed. Changes nothing. */
export const laidWatch: { tell: ((e: { turn: number; nat: string; row: string; drew: number; stayed: number; n: number }) => void) | null } = { tell: null };

/** Divisions drawn out of a living corps for a row: how many, the numbers they carry into it, their strength, experience, order and morale. */
export interface Drawn { id: number; k: number; nos: number[]; str: number; xp: number; org: number; mor?: number }

/** Divisions with figures of their own, as a corps is made of them: how many, and each one's strength, experience, order and morale. */
interface Fig { k: number; str: number; xp: number; org: number; mor?: number }

const clamp = (x: number): number => Math.max(0, Math.min(1, x));

/**
 * Sets a corps' figures from the divisions it is made of: strength the mean of theirs, experience, order and morale each
 * weighted by the men who bring it (release 2's drawn.ts assemble reads them so). Nothing is made or lost: the corps'
 * men are exactly the sum of theirs. Its count of divisions is theirs.
 */
function blend(c: Corps, parts: Fig[]): void {
  const divs = parts.reduce((s, p) => s + p.k, 0);
  if (divs <= 0) return;
  const men = parts.reduce((s, p) => s + p.k * p.str, 0);
  const by = (f: (p: Fig) => number, or: number): number => (men > 1e-9 ? parts.reduce((s, p) => s + p.k * p.str * f(p), 0) / men : or);
  c.divs = divs;
  c.str = clamp(men / divs);
  c.xp = clamp(by((p) => p.xp, c.xp));
  c.org = clamp(by((p) => p.org, c.org));
  if (c.mor !== undefined) c.mor = clamp(by((p) => p.mor ?? c.mor!, c.mor));
}

/** True where an enemy at war with the country holds or stands on the corps' province or one beside it (the 18:47Z test). */
function inContact(g: Game, nat: string, c: Corps): boolean {
  const enemy = (r: Region): boolean => (!!r.ctrl && atWar(g, nat, r.ctrl)) || r.units.some((u) => !u.dead && atWar(g, nat, u.nat));
  const r = g.regions[c.reg];
  return enemy(r) || r.nb.some((j) => enemy(g.regions[j]));
}

/**
 * True where a corps keeps the division where it stands and the row forms without it: the player's own; one on a
 * passage at sea (it lands with its divisions); one already in the side theatre the row is for, or at its place (a row
 * never pulls a division out of its own theatre: her ruling 2 of 14:28Z); one in a fight; one shut in a town the enemy
 * stands round. The fight acts at stops 3 to 5, or where the west's book is closed (the book every later one on this
 * chart hangs on): while the book is kept, the book's rows keep their book and draw a held division at its date, fight
 * or not, still laid once (the design director's rulings of 21:25Z, 21:50Z, 22:05Z and 22:12Z).
 */
function keeps(g: Game, nat: string, row: any[], place: Region, c: Corps, afloat: boolean): boolean {
  const k = g.byName[row[5]]?.theatre ?? -1;
  return nat === g.human || afloat || c.reg === place.id || (k >= 0 && g.regions[c.reg].theatre === k) || (!onBook(g, 'west') && inContact(g, nat, c))
    || (c.town !== undefined && g.regions[c.reg].units.some((u) => !u.dead && atWar(g, nat, u.nat)));
}

/** The arm each holder's divisions are numbered in, as the war's opening order of battle has it: read for a holder now gone. */
const ARM = new Map<string, string>(((setup as any).corps as any[]).map((r) => [`${r[0]}:${r[1]}`, r[2] as string] as [string, string]).reverse());

/** What a holder gives: the numbers the record names, where it carries them; where the record names none, its last divisions. */
function gives(h: Corps, want: number, record: number[]): number[] {
  const list = numbersOf(h).slice(0, h.divs);
  const known = record.length > 0 && (h.nos !== undefined || h.dn !== undefined);
  const give: number[] = known ? record.filter((x) => list.includes(x)) : [];
  if (!known) for (let j = list.length - 1; give.length < want && j >= 0; j--) give.push(list[j]);
  return give;
}

/** The words' blanks filled. */
const fill = (line: string, as: Record<string, string>): string => line.replace(/\{(\w+)\}/g, (all, key: string) => as[key] ?? all);

/**
 * The player's line for one of his corps that keeps the divisions a row names: which they are, where it stands (or, on
 * a passage, where it is bound), and that none is raised twice.
 */
function told(g: Game, row: any[], place: Region, x: Corps, nos: number[], v: Voyage | undefined): string {
  const all = divisionsOf(g, x);
  const cards = all.filter((d) => nos.includes(d.no)).map((d) => `the ${d.name}`);
  const at = v ? fill(WORDS.afloat, { to: g.regions[v.to].name }) : fill(WORDS.ashore, { where: g.regions[x.reg].name });
  const as = { names: cards.join(' and '), place: place.name, army: x.name, at, count: String(nos.length), formation: row[2] };
  // (A corps named after a division it carries, as a part he broke off is: the line says he holds it, not that it comes
  // from itself, in the same tense as the lines beside it.)
  const self = all.some((d) => d.name === x.name);
  const line = fill(self ? (nos.length === 1 ? WORDS.held : WORDS.heldMany) : nos.length === 1 ? WORDS.from : WORDS.fromMany, as);
  return line.charAt(0).toUpperCase() + line.slice(1);
}

/**
 * The divisions a dated row names that living corps of its country hold (the read rows only), as the row comes in
 * the grand campaign: drawn out of each holder that can give them, and held back where it cannot. Returns what was
 * drawn, how many divisions the row is to form (fewer by those held back: none is raised a second time), and, where
 * the player's own corps holds them, the line that tells him. Nothing changes for any other row, or outside the
 * grand campaign.
 *
 * A holder is a living corps of the country by the record's name, on the map or at sea. Where the record knows the
 * numbers its divisions carry, it gives only those it still carries: a corps raised again under the name, carrying
 * none of them, holds none of the row's divisions and gives nothing. The player's own divisions never move: any corps
 * of his of the holder's arm that carries the record's numbers keeps them (a part he broke off a holder, the part he
 * put it back into, one that outlived it), and the row comes without them. A row never takes in more divisions than it
 * is to form.
 */
export function drawHeld(g: Game, nat: string, row: any[], place: Region, n: number): { n: number; drawn: Drawn[]; line: string | null } {
  const holders = g.flags.open === 1 ? ROWS.get(`${nat}:${row[2]}`) : undefined;
  if (!holders) return { n, drawn: [], line: null };
  const drawn: Drawn[] = [];
  const lines: string[] = [];
  const read = new Set<string>();
  const sea = new Map((g.atSea ?? []).filter((v) => v.nat === nat && !v.corps.dead).map((v) => [v.corps, v] as const));
  let stayed = 0;
  let took = 0;
  for (const [name, want, record, alt] of holders) {
    if (alt !== undefined && read.has(alt)) continue;
    const h = g.corps.find((x) => x.nat === nat && !x.dead && x.name === name && x.kind !== 'fort')
      ?? [...sea.keys()].find((x) => x.name === name && x.kind !== 'fort');
    if (nat === g.human) {
      // His own stay where they are, and the row comes without them: the holder's, and those of the record's numbers
      // any other corps of his of the holder's arm carries (a part he broke off it, the part he put it back into, or a
      // part that outlived it). His divisions never move.
      if (n <= 0) continue;
      const arm = h?.kind ?? ARM.get(`${nat}:${name}`);
      const others = record.length > 0 && arm !== undefined
        ? [...g.corps, ...sea.keys()].filter((x) => x !== h && x.nat === nat && !x.dead && x.kind === arm && (x.nos !== undefined || x.dn !== undefined))
        : [];
      let left = Math.min(want, n);
      let kept = 0;
      const said = new Set<number>();
      for (const x of [...(h ? [h] : []), ...others]) {
        if (left <= 0) break;
        const mine = (x === h ? gives(h, want, record) : record.filter((r) => numbersOf(x).slice(0, x.divs).includes(r) && !said.has(r))).slice(0, Math.min(left, x.divs));
        if (!mine.length) continue;
        for (const r of mine) said.add(r);
        left -= mine.length;
        kept += mine.length;
        n -= mine.length;
        stayed += mine.length;
        lines.push(told(g, row, place, x, mine, sea.get(x)));
      }
      if (alt !== undefined && kept > 0) read.add(alt);
      continue;
    }
    if (!h || n - took <= 0) continue;
    const list = numbersOf(h).slice(0, h.divs);
    const give = gives(h, want, record);
    const k = Math.min(want, h.divs, n - took, give.length);
    if (k <= 0) continue;
    give.length = k;
    if (alt !== undefined) read.add(alt);
    if (keeps(g, nat, row, place, h, sea.has(h))) {
      n -= k;
      stayed += k;
      continue;
    }
    took += k;
    drawn.push({ id: h.id, k, nos: h.kind === row[3] ? give : [], str: h.str, xp: h.xp, org: h.org, ...(h.mor !== undefined ? { mor: h.mor } : {}) });
    if (k >= h.divs) removeCorps(g, h);
    else { h.divs -= k; keepNumbers(h, list.filter((x) => !give.includes(x))); }
    g.supplyDirty = true;
  }
  laidWatch.tell?.({ turn: g.turn, nat, row: row[2], drew: took, stayed: stayed, n });
  return { n, drawn, line: lines.length ? lines.join(' ') : null };
}

/**
 * The row's new corps takes in what it drew: their strength, experience, order and morale (the rest come at the row's
 * own), and the numbers they carry, first. Each holder and how many it gave is kept by the row's place in the list, for
 * its loan's end.
 */
export function layDrawn(g: Game, i: number, c: Corps, drawn: Drawn[]): void {
  if (!drawn.length) return;
  const K = Math.min(c.divs, drawn.reduce((s, d) => s + d.k, 0));
  const fresh = c.divs - K;
  const own: Fig = { k: fresh, str: c.str, xp: c.xp, org: c.org, ...(c.mor !== undefined ? { mor: c.mor } : {}) };
  blend(c, [...drawn.map((d) => ({ k: d.k, str: d.str, xp: d.xp, org: d.org, mor: d.mor })), ...(fresh > 0 ? [own] : [])]);
  const nos = numbersOf(c).slice(0, c.divs);
  const carried = drawn.flatMap((d) => d.nos);
  keepNumbers(c, [...carried, ...nos.filter((x) => !carried.includes(x))].slice(0, c.divs));
  for (const d of drawn) g.flags[`drew:${i}:${d.id}`] = (g.flags[`drew:${i}:${d.id}`] ?? 0) + d.k;
}

/**
 * A lent row's loan ends (its `until`): true where the whole corps leaves, as before, because it drew nothing. Where it
 * drew divisions they do not leave: each goes back to the corps it came out of where the two stand together, bringing
 * its share of the row's corps as it stands (its men, experience, order and morale: the holder's figures become the
 * two together, and no man is made or lost), and the rest stand on where they are under the row's name, free for new
 * orders. Only what the row raised new leaves.
 */
export function loanOver(g: Game, i: number, c: Corps): boolean {
  const key = `drew:${i}:`;
  const parts = Object.keys(g.flags).filter((k) => k.startsWith(key)).map((k) => ({ id: +k.slice(key.length), k: g.flags[k] as number }));
  if (!parts.length) return true;
  for (const p of parts) delete g.flags[`${key}${p.id}`];
  // The drawn divisions are the first of the corps' list: losses are counted from the new ones first.
  let left = Math.min(c.divs, parts.reduce((s, p) => s + p.k, 0));
  let list = numbersOf(c).slice(0, c.divs);
  const kept: number[] = [];
  const share = (k: number): Fig => ({ k, str: c.str, xp: c.xp, org: c.org, ...(c.mor !== undefined ? { mor: c.mor } : {}) });
  for (const p of parts) {
    const k = Math.min(p.k, left);
    if (k <= 0) break;
    left -= k;
    const mine = list.slice(0, k);
    list = list.slice(k);
    const h = g.corps.find((x) => x.id === p.id && x.nat === c.nat && !x.dead);
    if (h && h.reg === c.reg) {
      const was: Fig = { k: h.divs, str: h.str, xp: h.xp, org: h.org, ...(h.mor !== undefined ? { mor: h.mor } : {}) };
      if (h.kind === c.kind) { const back = numbersOf(h).slice(0, h.divs); h.divs += k; keepNumbers(h, [...back, ...mine]); }
      else growCorps(g, h, k, h.kind);
      blend(h, [was, share(k)]);
    } else kept.push(...mine);
  }
  g.supplyDirty = true;
  if (!kept.length) { removeCorps(g, c); return false; }
  c.divs = kept.length;
  keepNumbers(c, kept);
  c.hold = false;
  delete c.stay;
  return false;
}
