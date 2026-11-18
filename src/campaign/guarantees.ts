/**
 * The guarantees of 1939 (ruling 229, part 3; data/campaign/guarantees.json). The declare-war card names a small
 * country's guarantors, and in the grand campaign the game keeps the promise: when the country is attacked by a power
 * its guarantee covered, each guarantor the computer runs declares war on the attacker the same day. A guarantor the
 * player leads is never forced: he is told. A computer-run staff weighs the guarantors before it opens an operation,
 * as it weighs the League. Friendships (relations.json) are another thing: they move opinion, and bring nobody into a
 * war. The words are the UI director's.
 */
import data from '../../data/campaign/guarantees.json';
import type { Game } from './types';
import { allied, atWar, bloc, newsName, setWar, theName, type Bloc } from './game';
import { beyondEurope } from './world';

interface Row { by: string; for: string; against: string[] | 'any' | 'europe'; from: number; to?: number; consult?: boolean; tie: string; keeps: string; yours: string }
type Stand = 'comes' | 'war' | 'consult' | 'free';

export const GUARANTEES = data as unknown as {
  deter: number;
  rows: Row[];
  lines: { clauses: Record<Stand, string>; head: string; text: string; calls: string; asks: string };
};

/**
 * A guarantor of a country as the card lists it for the one who would attack it. `stands`: 'comes', it declares war
 * on him the same month; 'war', it is at war with him already; 'consult', its guarantee binds it only to consult
 * (Poland's, against anyone but Germany); 'free', its guarantee does not cover him. `why` is the card's line.
 */
export interface Guarantor { nat: string; stands: Stand; why: string; comes: boolean }

const fill = (line: string, vars: Record<string, string>): string => line.replace(/\{(\w+)\}/g, (_, k) => vars[k] ?? '');
const listed = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

/** True where a row's guarantee covers an attack by this power. */
function covers(g: Game, row: Row, by: string): boolean {
  if (row.against === 'any') return true;
  if (row.against === 'europe') { const cap = g.nations[by]?.capital ?? -1; return cap >= 0 && !beyondEurope(cap); }
  return row.against.includes(by);
}

/** The rows that stand for `on` this month: the grand campaign's, inside their months, with a guarantor alive and never fallen. */
function rowsFor(g: Game, by: string, on: string, was: Bloc | null): Row[] {
  if (g.flags.open !== 1 || g.flags.dom) return [];
  return GUARANTEES.rows.filter((row) => {
    if (row.for !== on || row.by === by || g.turn < row.from || (row.to !== undefined && g.turn > row.to)) return false;
    const G = g.nations[row.by];
    // (France's guarantees end at its armistice, and a freed France does not take them up again.)
    if (!G?.alive || G.dormant || G.fellOn) return false;
    // (A country that has joined another alliance than its guarantor's, the attacker's or a third, has no guarantee from
    // it; one in the League of small nations keeps it, as the record's guarantees had no word about such a league.)
    return !was || was === 'league' || was === bloc(G);
  });
}

/**
 * The guarantors of `on`, as they stand toward an attack by `by` this month. `was` is the alliance `on` stood in
 * before the attack, read before an attacked neutral takes a side. A guarantor allied to `by` stands by nobody
 * against it and is left off.
 */
export function guarantorsOf(g: Game, by: string, on: string, was: Bloc | null = g.nations[on] ? bloc(g.nations[on]) : null): Guarantor[] {
  const out: Guarantor[] = [];
  const Name = g.nations[on] ? theName(g.nations[on], true) : on;
  for (const row of rowsFor(g, by, on, was)) {
    if (g.nations[by] && allied(g, row.by, by)) continue;
    const stands: Stand = !covers(g, row, by) ? (row.consult ? 'consult' : 'free') : atWar(g, row.by, by) ? 'war' : 'comes';
    out.push({ nat: row.by, stands, comes: stands === 'comes', why: `${row.tie}: ${fill(GUARANTEES.lines.clauses[stands], { Name })}` });
  }
  return out;
}

/**
 * The guarantees kept, the day `a` declares war on `b` (declareWar, the grand campaign): every computer-run guarantor
 * whose guarantee covers `a`, and that is not at war with it already, declares war on it too, and the news says so in
 * one story. It declares nothing else by it: its allies stay as they are, and a country outside both blocs stays
 * outside. A guarantor the player leads is never forced: a story of its own tells him, and the Declare war card is
 * his. Returns the guarantors that came in.
 */
export function honour(g: Game, a: string, b: string, was: Bloc | null): string[] {
  const A = g.nations[a], B = g.nations[b];
  if (!A || !B) return [];
  const rows = rowsFor(g, a, b, was).filter((row) => covers(g, row, a) && !atWar(g, row.by, a) && !allied(g, row.by, a));
  const came: string[] = [];
  for (const row of rows) {
    if (row.by === g.human) {
      g.headlines.push({ turn: g.turn, week: g.week, kind: 'info', nat: row.by, region: B.capital >= 0 ? B.capital : undefined,
        head: fill(GUARANTEES.lines.calls, { Name: theName(B, true) }),
        text: fill(GUARANTEES.lines.asks, { Foe: theName(A, true), foe: theName(A), name: theName(B), yours: row.yours }) });
      continue;
    }
    setWar(g, row.by, a);
    g.flags[`war:${row.by}:${a}`] = g.turn;
    came.push(row.by);
  }
  if (!came.length) return came;
  const names = listed(came.map((id) => newsName(g.nations[id]))), many = came.length > 1;
  g.headlines.push({ turn: g.turn, week: g.week, kind: 'war', nat: came[0], region: B.capital >= 0 ? B.capital : undefined,
    head: fill(GUARANTEES.lines.head, { Guarantors: names, stand: many ? 'stand' : 'stands', Name: theName(B, true) }),
    text: fill(GUARANTEES.lines.text, { Guarantors: names, declare: many ? 'declare' : 'declares', foe: theName(A), they: many ? 'they keep' : 'it keeps', keeps: rows.find((r) => r.by === came[0])!.keeps }) });
  g.supplyDirty = true;
  return came;
}

/**
 * How much less likely a computer-run staff is to open an operation against these countries because a guarantor would
 * come in against it (GUARANTEES.deter), as the League's members would: 1 when none would.
 */
export function guaranteeDeters(g: Game, nat: string, targets: string[]): number {
  if (g.flags.open !== 1) return 1;
  return targets.some((on) => guarantorsOf(g, nat, on).some((G) => G.comes)) ? GUARANTEES.deter : 1;
}
