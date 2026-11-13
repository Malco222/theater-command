// World Domination: the last act of the grand campaign.
//
// The war between the two great alliances is decided when one of them has lost a great power and has none left in
// the fight. From then on the player may take the victory, or choose World Domination: every alliance is dissolved,
// every nation still standing is at war with every other, and the last one standing has won.
//
// Nothing here happens by itself. A historical scenario has no last act, and a war with nobody at the table is
// never offered one, so both are fought exactly as before. The numbers and the words are in
// data/campaign/domination.json.
//
// State is kept in g.flags, so it is saved with the campaign:
//   dom        the month the last act began
//   dom:n      how many nations stood when it began
//   dom:seen   the month the player was first told the war was decided
//   dom:peace  the month the player chose to take the victory (the last act can still be chosen later)
//   dom:won    the month the last nation was left standing
import data from '../../data/campaign/domination.json';
import { airOnFall } from './air';
import { DIPLOMACY } from './diplomacy';
import { ledger } from './economy';
import { allied, bloc, collapseScore, fieldArmy, headline, relocate, removeCorps, setWar } from './game';
import { armyKnown, fogOn, knowsBooks } from './intel';
import { fleetsOnFall } from './navy';
import type { Corps, Game, Nation, Region } from './types';

export const DOMINATION = data as unknown as {
  nerve: { attackAt: number; persist: number };
  guests: { org: number };
  lines: { open: string; guests: string; leave: string; won: string };
};

type Side = 'axis' | 'allies';
const SIDES: Side[] = ['axis', 'allies'];
export const SIDE_NAME: Record<Side, string> = { axis: 'the Axis', allies: 'the Allies' };

/** The last act belongs to the grand campaign. */
export const domRules = (g: Game): boolean => g.flags.open === 1;

/** True once the last act has begun: no alliances, every nation for itself. */
export const dominating = (g: Game): boolean => (g.flags.dom ?? 0) > 0;

const fill = (line: string, vars: Record<string, string | number>): string => line.replace(/\{(\w+)\}/g, (_, k) => String(vars[k] ?? ''));

/** The nations still in the running: alive, with a capital on the map. */
export function contenders(g: Game): Nation[] {
  return Object.values(g.nations).filter((n) => n.alive && !n.dormant && n.capital >= 0);
}

export interface Decided {
  /** The alliances with no great power left in the fight. */
  beaten: Side[];
  /** The alliance that beat them, when exactly one of the two still has a great power standing. */
  victor: Side | null;
  /** The great powers that have fallen, and those still standing. */
  fallen: string[];
  standing: string[];
}

/**
 * Whether the war between the alliances is decided, and how. It is decided when a great power has fallen and no
 * two great powers still standing are at war with each other: the great fight is over. The alliance of the last
 * great power to fall is beaten, and so is an alliance that has lost a great power and has none left at all.
 */
export function warDecided(g: Game): Decided | null {
  if (!domRules(g)) return null;
  const majors = DIPLOMACY.majors.map((id) => g.nations[id]).filter((n): n is Nation => !!n);
  const fallen = majors.filter((n) => !n.alive && !!n.fellOn).sort((a, b) => a.fellOn!.turn - b.fellOn!.turn || a.fellOn!.week - b.fellOn!.week);
  if (!fallen.length) return null;
  const live = majors.filter((n) => n.alive);
  if (live.some((a) => a.atWar.some((e) => live.some((b) => b.id === e)))) return null;
  const last = bloc(fallen[fallen.length - 1]);
  const beaten = SIDES.filter((s) => fallen.some((n) => bloc(n) === s) && (s === last || !live.some((n) => bloc(n) === s)));
  if (!beaten.length) return null;
  const left = SIDES.filter((s) => !beaten.includes(s) && live.some((n) => bloc(n) === s));
  return { beaten, victor: left.length === 1 ? left[0] : null, fallen: fallen.map((n) => n.id), standing: live.map((n) => n.id) };
}

/** Why this nation's player cannot open the last act today, or null if he can. */
export function whyNotDomination(g: Game, nat: string | null = g.human): string | null {
  if (!domRules(g)) return 'World Domination belongs to the grand campaign.';
  if (!nat || !g.nations[nat]?.alive) return 'Your nation is out of the war.';
  if (dominating(g)) return 'The last war has already begun.';
  if (!warDecided(g)) return 'The war between the alliances is not decided yet: a great power must fall, and no two great powers may still be at war with each other.';
  return null;
}

/** The nearest ground a corps' own nation holds, overland; failing that its capital, by sea. */
function wayHome(g: Game, c: Corps): Region | null {
  const seen = new Uint8Array(g.regions.length);
  const q = [c.reg];
  seen[c.reg] = 1;
  for (let i = 0; i < q.length; i++) {
    const r = g.regions[q[i]];
    if (r.ctrl === c.nat && q[i] !== c.reg) return r;
    for (const j of r.nb) if (!seen[j]) { seen[j] = 1; q.push(j); }
  }
  const N = g.nations[c.nat];
  const cap = N.capital >= 0 ? g.regions[N.capital] : null;
  return cap && cap.ctrl === c.nat ? cap : null;
}

/** Who takes over ground a departing nation held: its owner if he still stands, else whoever holds most of the country round it. */
function heir(g: Game, r: Region, gone: string): string | null {
  const owner = g.nations[r.owner];
  if (owner?.alive && owner.id !== gone && owner.capital >= 0) return owner.id;
  const count: Record<string, number> = {};
  for (const j of r.nb) {
    const c = g.regions[j].ctrl;
    if (c !== gone && g.nations[c]?.alive && g.nations[c].capital >= 0) count[c] = (count[c] ?? 0) + 1;
  }
  return Object.keys(count).sort((a, b) => count[b] - count[a] || (a < b ? -1 : 1))[0] ?? null;
}

/** A nation whose homeland lies beyond the map cannot be conquered on it: it takes its forces home and is out of the war. */
function leave(g: Game, n: Nation): void {
  for (const c of g.corps.filter((c) => c.nat === n.id)) removeCorps(g, c);
  fleetsOnFall(g, n.id);
  airOnFall(g, n.id);
  // Ground it held goes back to its owner, or to whoever holds the country round it. Done in passes so that a
  // stretch of several provinces is handed over from its edge inward.
  for (let pass = 0; pass < 8; pass++) {
    let left = 0;
    for (const r of g.regions) {
      if (r.ctrl !== n.id) continue;
      const to = heir(g, r, n.id);
      if (to) r.ctrl = to; else left++;
    }
    if (!left) break;
  }
  for (const m of Object.values(g.nations)) m.atWar = m.atWar.filter((e) => e !== n.id);
  n.atWar = [];
  n.alive = false;
  n.dormant = true;
  headline(g, 'event', fill(DOMINATION.lines.leave, { name: n.name }), n.id);
}

/**
 * Opens the last act: the alliances are dissolved, every nation still standing is at war with every other, and
 * troops on a former ally's soil go home. Returns false, and changes nothing, if it cannot be opened today.
 */
export function startDomination(g: Game, by: string | null = g.human): boolean {
  if (whyNotDomination(g, by)) return false;
  // Who stands on an ally's ground is read off before the alliances go.
  const guests = g.corps.filter((c) => !c.dead && g.regions[c.reg].ctrl !== c.nat && allied(g, c.nat, g.regions[c.reg].ctrl));
  for (const n of Object.values(g.nations)) if (n.alive && !n.dormant && n.capital < 0) leave(g, n);
  const live = contenders(g);
  for (const n of live) {
    n.align = 'independent';
    n.league = false;
    n.opinion = n.home = 0;
  }
  for (let i = 0; i < live.length; i++) for (let j = i + 1; j < live.length; j++) setWar(g, live[i].id, live[j].id);
  g.flags.dom = g.turn;
  g.flags['dom:n'] = live.length;
  headline(g, 'war', fill(DOMINATION.lines.open, { n: live.length }), by ?? undefined);
  let home = 0;
  for (const c of guests) {
    if (c.dead) continue;
    const to = wayHome(g, c);
    if (!to) { removeCorps(g, c); continue; }
    relocate(g, c, to);
    c.ap = 0;
    c.org = Math.min(c.org, DOMINATION.guests.org);
    home++;
  }
  if (home) headline(g, 'event', fill(DOMINATION.lines.guests, { n: home }));
  g.supplyDirty = true;
  crown(g);
  return true;
}

/** The player takes the victory and leaves the world as it is. The last act can still be opened later. */
export function keepPeace(g: Game): boolean {
  if (whyNotDomination(g) || g.flags['dom:peace']) return false;
  g.flags['dom:peace'] = g.turn;
  return true;
}

/** Notes the month the last nation is left standing. Safe to call as often as anyone likes. */
export function crown(g: Game): void {
  if (!dominating(g) || g.flags['dom:won']) return;
  const live = contenders(g);
  if (live.length !== 1) return;
  g.flags['dom:won'] = g.turn;
  headline(g, 'goal', fill(DOMINATION.lines.won, { name: live[0].name, adj: live[0].adj }), live[0].id);
}

/** The nation that has won the last act, or null while more than one stands. */
export function worldWinner(g: Game): string | null {
  if (!dominating(g) || !g.flags['dom:won']) return null;
  return contenders(g)[0]?.id ?? null;
}

export interface BoardRow {
  nat: string;
  name: string;
  you: boolean;
  alive: boolean;
  /** Provinces held, and the share of the map's worth (its victory points) they come to. */
  provinces: number;
  share: number;
  /** Its army in divisions, and whether that is the true figure or the viewer's estimate through the fog. */
  divisions: number;
  exact: boolean;
  /** True when all that is known through the fog is that the army is a small one: fewer than `divisions`. */
  few: boolean;
  /** Income a month, in production points: null for a foreign nation whose books the viewer cannot read. */
  income: number | null;
  /** How much fight it has left, from 1 down to 0 at the point of collapse. */
  will: number;
  /** When it fell and to whom: for the fallen. */
  fell: { turn: number; to: string } | null;
}

export interface WorldBoard {
  /** Whether the last act is on, and the month it began. */
  on: boolean;
  since: number;
  /** How many nations stood when it began, and how many stand now. */
  began: number;
  left: number;
  /** Those still standing, strongest first, then the fallen, latest first. */
  rows: BoardRow[];
  fallen: BoardRow[];
  /** The winner, once only one is left. */
  winner: string | null;
  /** One production point is shown as this many in the interface. */
  scale: number;
}

/** The standings of the last act, for an interface: who is left, how much of the world each holds, and who has fallen to whom. */
export function worldBoard(g: Game, viewer: string | null = g.human): WorldBoard {
  const held: Record<string, { n: number; vp: number }> = {};
  let all = 0;
  for (const r of g.regions) {
    all += r.vp;
    const h = (held[r.ctrl] ??= { n: 0, vp: 0 });
    h.n++; h.vp += r.vp;
  }
  let scale = 1;
  const row = (n: Nation): BoardRow => {
    const L = n.alive ? ledger(g, n.id) : null;
    if (L) scale = L.scale;
    const army: { divs: number; exact: boolean; few?: boolean } = !n.alive ? { divs: 0, exact: true } : viewer && n.id !== viewer ? armyKnown(g, n.id, viewer) : { divs: fieldArmy(g, n.id), exact: true };
    const open = !viewer || n.id === viewer || !fogOn(g, viewer) || knowsBooks(g, n.id, viewer);
    return {
      nat: n.id, name: n.name, you: n.id === viewer, alive: n.alive,
      provinces: n.alive ? held[n.id]?.n ?? 0 : 0,
      share: n.alive && all > 0 ? (held[n.id]?.vp ?? 0) / all : 0,
      divisions: army.divs, exact: army.exact, few: !!army.few,
      income: L && open ? L.gross : null,
      will: n.alive ? Math.max(0, Math.min(1, 1 - collapseScore(g, n.id) / n.resilience)) : 0,
      fell: n.fellOn ? { turn: n.fellOn.turn, to: n.fellOn.to } : null,
    };
  };
  const rows = contenders(g).map(row).sort((a, b) => b.share - a.share || b.divisions - a.divisions || (a.nat < b.nat ? -1 : 1));
  const fallen = Object.values(g.nations).filter((n) => !n.alive && !!n.fellOn).map(row).sort((a, b) => b.fell!.turn - a.fell!.turn || (a.nat < b.nat ? -1 : 1));
  return { on: dominating(g), since: g.flags.dom ?? 0, began: g.flags['dom:n'] ?? rows.length, left: rows.length, rows, fallen, winner: worldWinner(g), scale };
}
