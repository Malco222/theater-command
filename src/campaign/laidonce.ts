// A division is laid once (1.5.3, the grand campaign; data/campaign/laidonce.json).
//
// The design director's rulings of 13:43Z and 19:17Z, 25 November 2026, on the joiner's six-year check
// (joins/named-twice-check-1125.md) and the historian's read of each row (historian/named-twice-reads-1125.md). Seven
// dated rows named a division a living corps of their country already held, and raised it a second time at full
// strength. For those rows only: a division a living corps holds is drawn out of it, with its own strength and
// experience, instead of being raised again. It never leaves the side theatre the row is for, a fight (an enemy at
// war on its province or beside it: her ruling of 18:47Z), a town the enemy stands round, or the player's own corps:
// there it stays, and the row comes without it. A row lent for a while gives its drawn divisions back at the loan's end where their corps stands
// with it; otherwise they stand on where they are, free. Only what the row raised new leaves (her 19:17Z: a return
// never moves a division for free).
//
// History's campaigns keep the order of battle they were measured with: nothing here runs outside the grand campaign.
import data from '../../data/campaign/laidonce.json';
import { divisionsOf, growCorps, keepNumbers, numbersOf } from './armies';
import { atWar, removeCorps } from './game';
import type { Corps, Game, Region } from './types';

/** A holder of a row's division: its corps' name, the divisions it gives, the numbers they carry where the record knows them. */
type Holder = [string, number, number[]];
const ROWS = new Map<string, Holder[]>(Object.entries((data as unknown as { rows: Record<string, Holder[]> }).rows));
const WORDS = data as unknown as { from: string; fromMany: string };

/** A watch on the rule, for the patch's gate (null in play): each read row as it comes, what it drew and what stayed. Changes nothing. */
export const laidWatch: { tell: ((e: { turn: number; nat: string; row: string; drew: number; stayed: number; n: number }) => void) | null } = { tell: null };

/** Divisions drawn out of a living corps for a row: how many, the numbers they carry into it, their strength and experience. */
export interface Drawn { id: number; k: number; nos: number[]; str: number; xp: number }

/** True where an enemy at war with the country holds or stands on the corps' province or one beside it (the 18:47Z test). */
function inContact(g: Game, nat: string, c: Corps): boolean {
  const enemy = (r: Region): boolean => (!!r.ctrl && atWar(g, nat, r.ctrl)) || r.units.some((u) => !u.dead && atWar(g, nat, u.nat));
  const r = g.regions[c.reg];
  return enemy(r) || r.nb.some((j) => enemy(g.regions[j]));
}

/**
 * True where a corps keeps the division where it stands and the row forms without it: the player's own; one already in
 * the side theatre the row is for, or at its place (a row never pulls a division out of its own theatre: her ruling 2 of
 * 14:28Z); one in a fight; one shut in a town the enemy stands round.
 */
function keeps(g: Game, nat: string, row: any[], place: Region, c: Corps): boolean {
  const k = g.byName[row[5]]?.theatre ?? -1;
  return nat === g.human || c.reg === place.id || (k >= 0 && g.regions[c.reg].theatre === k) || inContact(g, nat, c)
    || (c.town !== undefined && g.regions[c.reg].units.some((u) => !u.dead && atWar(g, nat, u.nat)));
}

/**
 * The divisions a dated row names that living corps of its country hold (the read rows only), as the row comes in
 * the grand campaign: drawn out of each holder that can give them, and held back where it cannot. Returns what was
 * drawn, how many divisions the row is to form (fewer by those held back: none is raised a second time), and, where
 * the player's own corps holds them, the line that tells him. Nothing changes for any other row, or outside the
 * grand campaign.
 */
export function drawHeld(g: Game, nat: string, row: any[], place: Region, n: number): { n: number; drawn: Drawn[]; line: string | null } {
  const holders = g.flags.open === 1 ? ROWS.get(`${nat}:${row[2]}`) : undefined;
  if (!holders) return { n, drawn: [], line: null };
  const drawn: Drawn[] = [];
  const lines: string[] = [];
  let stayed = 0;
  for (const [name, want, record] of holders) {
    const h = g.corps.find((x) => x.nat === nat && !x.dead && x.name === name && x.kind !== 'fort');
    if (!h || n <= 0) continue;
    const k = Math.min(want, h.divs, n);
    if (k <= 0) continue;
    const list = numbersOf(h).slice(0, h.divs);
    // The numbers the record names, where the holder carries them; the rest from its last divisions.
    const give: number[] = record.filter((x) => list.includes(x)).slice(0, k);
    for (let j = list.length - 1; give.length < k && j >= 0; j--) if (!give.includes(list[j])) give.push(list[j]);
    if (keeps(g, nat, row, place, h)) {
      n -= k;
      stayed += k;
      if (nat === g.human) {
        const cards = divisionsOf(g, h).filter((d) => give.includes(d.no)).map((d) => `the ${d.name}`);
        const fill = (line: string, as: Record<string, string>): string => line.replace(/\{(\w+)\}/g, (all, key: string) => as[key] ?? all);
        const as = { names: cards.join(' and '), place: place.name, army: h.name, where: g.regions[h.reg].name, count: String(k), formation: row[2] };
        const line = fill(k === 1 ? WORDS.from : WORDS.fromMany, as);
        lines.push(line.charAt(0).toUpperCase() + line.slice(1));
      }
      continue;
    }
    drawn.push({ id: h.id, k, nos: h.kind === row[3] ? give : [], str: h.str, xp: h.xp });
    if (k >= h.divs) removeCorps(g, h);
    else { h.divs -= k; keepNumbers(h, list.filter((x) => !give.includes(x))); }
    g.supplyDirty = true;
  }
  laidWatch.tell?.({ turn: g.turn, nat, row: row[2], drew: drawn.reduce((s, d) => s + d.k, 0), stayed: stayed, n });
  return { n, drawn, line: lines.length ? lines.join(' ') : null };
}

/**
 * The row's new corps takes in what it drew: their strength and experience (the rest come at the row's own), and the
 * numbers they carry, first. Each holder and how many it gave is kept by the row's place in the list, for its loan's end.
 */
export function layDrawn(g: Game, i: number, c: Corps, drawn: Drawn[]): void {
  if (!drawn.length) return;
  const K = Math.min(c.divs, drawn.reduce((s, d) => s + d.k, 0));
  const fresh = c.divs - K;
  c.str = (drawn.reduce((s, d) => s + d.k * d.str, 0) + fresh * c.str) / c.divs;
  c.xp = (drawn.reduce((s, d) => s + d.k * d.xp, 0) + fresh * c.xp) / c.divs;
  const own = numbersOf(c).slice(0, c.divs);
  const carried = drawn.flatMap((d) => d.nos);
  keepNumbers(c, [...carried, ...own.filter((x) => !carried.includes(x))].slice(0, c.divs));
  for (const d of drawn) g.flags[`drew:${i}:${d.id}`] = (g.flags[`drew:${i}:${d.id}`] ?? 0) + d.k;
}

/**
 * A lent row's loan ends (its `until`): true where the whole corps leaves, as before, because it drew nothing. Where it
 * drew divisions they do not leave: each goes back to the corps it came out of where the two stand together, and the
 * rest stand on where they are under the row's name, free for new orders. Only what the row raised new leaves.
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
  for (const p of parts) {
    const k = Math.min(p.k, left);
    if (k <= 0) break;
    left -= k;
    const mine = list.slice(0, k);
    list = list.slice(k);
    const h = g.corps.find((x) => x.id === p.id && x.nat === c.nat && !x.dead);
    if (h && h.reg === c.reg) {
      if (h.kind === c.kind) { h.divs += k; keepNumbers(h, [...numbersOf(h).slice(0, h.divs - k), ...mine]); }
      else growCorps(g, h, k, h.kind);
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
