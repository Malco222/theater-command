// What the history dial does to who goes to war with whom (Version 70, the grand campaign). Three things, each a
// month's work at the opening of the month, none of them at the middle stop:
//   the book's marks (stops 1 and 2): what history's calendar expects in each theatre. When a mark fails, because
//     the player or the fortunes of war have taken that theatre another way, the book is closed there and in the
//     theatres that depend on it, and the staffs decide for themselves from then on. The news says so.
//   the deal (stop 5): every neutral's leaning between the alliances is dealt afresh from the war's seed.
//   ambitions (stops 4 and 5): a computer-run country with a claim on ground a neighbour holds presses it when that
//     neighbour is weakened, and may march for it. No surprise without a reason and a warning: a claim can be read
//     beforehand (claimsView, the Powers tab), pressing it is news, and so is the march, with its reason.
// A claim is stored as named ground (data/campaign/history.json, 'ambitions'), so that diplomacy can read the same
// list. The dial leashes the computer-run world and never the player: no claim is pressed for him.

import { DIPLOMACY, holds, majorsOf, signPeace } from './diplomacy';
import { allied, atWar, bloc, collapseScore, declareWar, headline, joinedSide, keptStoresHolder, theName, warning } from './game';
import { HISTORY, dialDraw, dialRules, stopDef, stopOf, FREE, type Ambition, type Mark } from './history';
import { deterred } from './league';
import { fronts, launch, ops } from './staff';
import type { Game, Region } from './types';
import { feedClaims, lapseFeedClaims } from './bargains';

const listOf = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const cap = (s: string): string => (s ? s[0].toUpperCase() + s.slice(1) : s);

// ---- The book's marks ---------------------------------------------------------------------------------------

/** One condition of a mark: as in diplomacy.json, with 'a|b' for either. */
const markHolds = (g: Game, c: string): boolean => c.split('|').some((x) => holds(g, x));

/** True while a theatre whose book 'stays' (b530) may still outlast the closing of one it comes after. */
const mayStay = (g: Game, id: string): boolean => {
  const s = HISTORY.theatres[id]?.stays;
  return !!s && s.stops.includes(stopOf(g)) && g.turn <= s.by;
};

/**
 * Closes the book in a theatre and in every theatre that depends on it. Returns the names of those closed. A theatre
 * whose book 'stays' is not closed with one it comes after while it may still stand: it waits (bookMonth reads it).
 */
function closeBook(g: Game, theatre: string): string[] {
  const out: string[] = [];
  const shut = (id: string, by?: string): void => {
    if (g.flags[`hist:off:${id}`]) return;
    if (by && mayStay(g, id)) { g.flags[`hist:wait:${id}`] ??= g.turn; return; }
    delete g.flags[`hist:wait:${id}`];
    g.flags[`hist:off:${id}`] = g.turn;
    out.push(HISTORY.theatres[id]?.name ?? id);
    for (const [other, t] of Object.entries(HISTORY.theatres)) if (t.after.includes(id)) shut(other, id);
  };
  shut(theatre);
  return out;
}

/**
 * A theatre left waiting when one it comes after closed (b530): its book stands once its own conditions hold, and
 * closes, with the theatres after it, if its month is past without them. Returns the names of those closed.
 */
function waitingBooks(g: Game): string[] {
  const out: string[] = [];
  for (const [id, t] of Object.entries(HISTORY.theatres)) {
    if (g.flags[`hist:wait:${id}`] === undefined || g.flags[`hist:off:${id}`] || !t.stays) continue;
    if (t.stays.if.every((c) => markHolds(g, c))) { delete g.flags[`hist:wait:${id}`]; g.flags[`hist:kept:${id}`] = g.turn; continue; }
    if (g.turn <= t.stays.by) continue;
    const closed = closeBook(g, id);
    if (!closed.length) continue;
    const [first, ...rest] = closed;
    const told = `History's book is closed in ${first}: ${t.stays.why}.${rest.length ? ` With it the book is closed in ${listOf(rest)}.` : ''} From here on the staffs there decide for themselves.`;
    if (g.human) warning(g, 'History\'s book', told, g.human); else headline(g, 'event', told);
    out.push(...closed);
  }
  return out;
}

/** The month's reading of history's calendar, at a stop that keeps the book. Returns the marks that failed. */
export function bookMonth(g: Game): Mark[] {
  const S = stopDef(g);
  if (!S.book) return [];
  const slack = S.slack ?? 0;
  const failed: Mark[] = [];
  HISTORY.marks.forEach((m, k) => {
    if (g.flags[`hist:off:${m.theatre}`]) return;
    let due = false;
    if (m.until !== undefined) due = g.turn < m.until - slack;
    // What should have happened by a month is asked once, the month after (the slack allowed).
    else if (m.by !== undefined && g.turn > m.by + slack && !g.flags[`hist:mk:${k}`]) { due = true; g.flags[`hist:mk:${k}`] = g.turn; }
    if (!due || m.if.every((c) => markHolds(g, c))) return;
    const closed = closeBook(g, m.theatre);
    failed.push(m);
    const [first, ...rest] = closed;
    // (A player is told on a card of its own: the war he is in has just changed its rules.)
    const told = `History's book is closed in ${first}: ${m.why}.${rest.length ? ` With it the book is closed in ${listOf(rest)}.` : ''} From here on the staffs there decide for themselves.`;
    if (g.human) warning(g, 'History\'s book', told, g.human); else headline(g, 'event', told);
  });
  waitingBooks(g);
  return failed;
}

// ---- The deal: every neutral's leaning dealt afresh ---------------------------------------------------------

const blocWord = (x: number): string => (x > 0 ? 'the Axis' : 'the Allies');

/** At a stop that deals the neutrals afresh, done once: each one's leaning is drawn from the war's seed. Returns those who changed their minds. */
export function dealMonth(g: Game): string[] {
  const S = stopDef(g);
  if (!S.deal || g.flags['hist:dealt']) return [];
  g.flags['hist:dealt'] = g.turn;
  const turned: string[] = [];
  for (const n of Object.values(g.nations)) {
    // The great powers, the countries already in the war or in an alliance, and the player's own are left as they are.
    if (!n.alive || n.dormant || n.league || n.atWar.length || bloc(n) || n.id === g.human || majorsOf(g).includes(n.id)) continue;
    const home = Math.round((2 * dialDraw(g, 'deal', n.id) - 1) * S.deal);
    const was = n.home;
    n.opinion = Math.max(-100, Math.min(100, n.opinion + (home - was)));
    n.home = home;
    if (Math.abs(home) >= 15 && Math.sign(home) !== Math.sign(was)) turned.push(`${n.name} toward ${blocWord(home)}`);
  }
  headline(g, 'event', turned.length
    ? `Nobody has read the history books this time. The chancelleries report new leanings: ${listOf(turned.slice(0, 6))}${turned.length > 6 ? ', and others besides' : ''}.`
    : 'Nobody has read the history books this time: any country may yet pick either side.');
  return turned;
}

// ---- Ambitions: claims on named ground -----------------------------------------------------------------------

export type ClaimState = 'claims' | 'pressing' | 'marched';

const C = HISTORY.claims;
const groundOf = (g: Game, a: Ambition): Region[] => a.ground.map((nm) => g.byName[nm]).filter((r): r is Region => !!r);
/** The claimed ground the holder still owns and holds. */
const stillHeld = (g: Game, a: Ambition): Region[] => groundOf(g, a).filter((r) => r.owner === a.on && r.ctrl === a.on);

/** True while a great power allied to the holder stands on the claimant's border with most of its will to fight left. */
function guarded(g: Game, a: Ambition): boolean {
  const hb = bloc(g.nations[a.on]);
  if (hb !== 'axis' && hb !== 'allies') return false;
  return majorsOf(g).some((id) => {
    const P = g.nations[id];
    if (!P?.alive || P.dormant || id === a.on || bloc(P) !== hb || collapseScore(g, id) / P.resilience >= C.guard) return false;
    return g.regions.some((r) => r.ctrl === id && r.nb.some((j) => g.regions[j].ctrl === a.nat));
  });
}

/** True for a claim that can be read at this stop: both countries stand, the claimant is the computer's, and the holder still has the ground. */
export function readable(g: Game, a: Ambition): boolean {
  const S = stopDef(g);
  if (!S.ambitions || S.stop < (a.stop ?? 4)) return false;
  const n = g.nations[a.nat], H = g.nations[a.on];
  if (!n?.alive || !H?.alive || n.dormant || H.dormant || a.nat === g.human) return false;
  if (!(a.if ?? []).every((c) => holds(g, c))) return false;
  // A claim that is the staff's own war (the Winter War, ruling 228): not pressed while that war may still come, and
  // put away once it has. Only a war that never came (its chance passed, or its target out of reach) leaves the claim.
  if (a.op) {
    const op = ops(a.nat).find((o) => o.id === a.op), done = g.flags[`st:done:${a.nat}:${a.op}`] ?? 0;
    if (done > 0 || (!done && op && g.turn <= (op.to ?? 999))) return false;
  }
  // Where anything goes, not every claim is alive in every war: who holds which is dealt from the war's seed.
  // (A claim for patience spent is the war's own, not dealt: release 2's bargains.)
  if (!a.feeds && S.share !== undefined && dialDraw(g, 'claim', a.id) >= S.share) return false;
  return stillHeld(g, a).length > 0;
}

/** Why a claimant thinks the moment has come, or null if it has not. */
export function moment(g: Game, a: Ambition): string | null {
  if (a.feeds) return feedMoment(g, a);
  const S = stopDef(g);
  const n = g.nations[a.nat], H = g.nations[a.on];
  // A claimant marches from peace: a country already at war has its hands full. Nor does it march on an ally or from inside the League.
  if (n.atWar.length || n.league || allied(g, a.nat, a.on)) return null;
  // A great power does not walk into the great war over a border claim: it marches only on a country outside both alliances.
  if (majorsOf(g).includes(a.nat) && bloc(H)) return null;
  const held = stillHeld(g, a);
  if (!held.length) return null;
  const lost = collapseScore(g, a.on) / H.resilience;
  const busy = H.atWar.filter((e) => g.nations[e]?.alive);
  // The balance of strength where the two would meet, as a staff weighs a war not yet declared. Ground across the sea
  // has no front to weigh: it is weakly held when few stand on it.
  // (The front the claimed ground lies on, weighed against what stands on the line and a province behind it.)
  const f = fronts(g, a.nat, { extra: new Set([a.on]) }).find((x) => x.enemies.includes(a.on) && held.some((r) => x.depth[r.id] > 0));
  const thin = held.every((r) => r.units.reduce((s, u) => s + (u.nat === a.on && !u.dead ? u.divs : 0), 0) <= C.thin);
  const good = f ? f.near >= C.odds : thin;
  // Where anything goes each claimant bides its own time, some for years: the month is dealt from the war's seed.
  const waited = g.turn >= 3 + Math.floor(dialDraw(g, 'wait', a.id) * (S.wait ?? 0));
  if (busy.length) {
    // A great power of the holder's alliance on the claimant's own border deters it, until that power is hard pressed
    // itself (not asked where anything goes).
    if (!S.bold && guarded(g, a)) return null;
    if (lost >= C.weak) return `${H.name} is hard pressed by ${listOf(busy.slice(0, 2).map((e) => g.nations[e].name))}`;
    // (Ground across the sea is not gone for on a thin garrison alone, except where anything goes and its month has come.)
    if (good && (f || (S.bold && waited))) return f ? `${theName(H, true)}'s armies are away fighting ${listOf(busy.slice(0, 2).map((e) => g.nations[e].name))}` : `${theName(H, true)} holds it with a handful of troops while it fights ${listOf(busy.slice(0, 2).map((e) => g.nations[e].name))}`;
    return null;
  }
  // Where anything goes a claimant needs no war to hide behind, only the odds and its month.
  if (!S.bold || !good || !waited) return null;
  return f ? 'the frontier is thinly held' : 'it is held by a handful of troops';
}

/**
 * Why a great power at war thinks the moment has come to march on a neutral that feeds its enemy (release 2's
 * bargains, patience spent, stops 4 and 5), or null: it is not hard pressed itself, and its front against the
 * neutral's border ground would have the odds a claim needs.
 */
function feedMoment(g: Game, a: Ambition): string | null {
  const n = g.nations[a.nat], H = g.nations[a.on];
  if (n.league || allied(g, a.nat, a.on) || atWar(g, a.nat, a.on)) return null;
  const held = stillHeld(g, a);
  if (!held.length || collapseScore(g, a.nat) / n.resilience >= C.weak) return null;
  const f = fronts(g, a.nat, { extra: new Set([a.on]) }).find((x) => x.enemies.includes(a.on) && held.some((r) => x.depth[r.id] > 0));
  if (!f || f.near < C.odds) return null;
  return `${theName(H, true)}'s frontier is thinly held`;
}

/** The claimant takes the field: war on the holder, a side if the holder has enemies, and an offensive pressed for a season. */
function march(g: Game, a: Ambition, why: string): void {
  const n = g.nations[a.nat], H = g.nations[a.on];
  if (a.lift) { n.sealift = Math.max(n.sealift ?? 0, a.lift); g.flags[`st:claim:${a.nat}`] = Object.keys(g.nations).indexOf(a.on) + 1; }
  const hb = bloc(H);
  declareWar(g, a.nat, a.on);
  if (!atWar(g, a.nat, a.on)) return;
  // A country that marches on a member of one alliance fights beside the other, if the other is at war with the holder.
  if ((hb === 'axis' || hb === 'allies') && bloc(n) === null) {
    const other = hb === 'axis' ? 'allies' : 'axis';
    if (H.atWar.some((x) => x !== a.nat && g.nations[x]?.alive && bloc(g.nations[x]) === other)) {
      n.align = other === 'axis' ? 'axis_cobelligerent' : 'allies_cobelligerent';
      n.opinion = n.home = other === 'axis' ? 100 : -100;
      g.supplyDirty = true;
      joinedSide(g, a.nat);
    }
  }
  launch(g, a.nat, [a.on], C.push);
  g.flags[`amb:go:${a.id}`] = g.turn;
  g.flags['hist:live'] = 1;
  headline(g, 'war', `${n.name} marches for ${a.name}: ${why}. ${cap(a.why)}.`, a.nat, a.on === g.human ? groundOf(g, a)[0]?.id : undefined);
}

/** A great power of the holder's alliance that borders the claimant may stand by its ally. */
function aid(g: Game, a: Ambition): void {
  const n = g.nations[a.nat], H = g.nations[a.on];
  const hb = bloc(H);
  if (hb !== 'axis' && hb !== 'allies') return;
  for (const id of majorsOf(g)) {
    const G = g.nations[id];
    if (!G?.alive || G.dormant || id === g.human || id === a.on || bloc(G) !== hb || atWar(g, id, a.nat) || allied(g, id, a.nat)) continue;
    const borders = g.regions.some((r) => r.ctrl === id && r.nb.some((j) => g.regions[j].ctrl === a.nat));
    if (!borders || dialDraw(g, 'aid', a.id, id, g.turn) >= C.aid) continue;
    declareWar(g, id, a.nat);
    if (atWar(g, id, a.nat)) headline(g, 'war', `${G.name} stands by ${H.name}: ${n.name} went to war for ${a.name}, and now has ${theName(G)} to reckon with.`, id);
  }
}

/** A war between two countries outside both alliances ends when the claimant has held what it claims for a season: the ground is its own by treaty. */
function treaty(g: Game, a: Ambition): boolean {
  const n = g.nations[a.nat], H = g.nations[a.on];
  if (bloc(n) || bloc(H) || a.nat === g.human || a.on === g.human) return false;
  const ground = groundOf(g, a);
  const all = ground.length > 0 && ground.every((r) => r.ctrl === a.nat);
  if (!all) { delete g.flags[`amb:held:${a.id}`]; return false; }
  const since = g.flags[`amb:held:${a.id}`] ?? (g.flags[`amb:held:${a.id}`] = g.turn);
  if (g.turn - since + 1 < C.treaty) return false;
  // (Under true reach the claimant's kept stores in a port it already holds stay his: game.ts keptStoresHolder.)
  for (const r of ground) { keptStoresHolder(g, r, a.nat); r.owner = a.nat; r.since = g.turn; }
  if (!signPeace(g, a.nat, a.on)) return false;
  g.supplyDirty = true;
  g.flags[`amb:done:${a.id}`] = g.turn;
  headline(g, 'event', `${n.name} and ${H.name} sign a treaty: ${a.name} is ${n.adj}, and the two countries are at peace.`, a.nat);
  return true;
}

/** The month's ambitions: claims are pressed, dropped or marched for, allies stand by, and private wars end in treaties. */
export function claimsMonth(g: Game): void {
  // (A neutral's bargains, release 2: a warning for a feed claim no longer listed lapses first.)
  lapseFeedClaims(g);
  let live = false;
  for (const a of [...HISTORY.ambitions, ...feedClaims(g)] as Ambition[]) {
    const went = g.flags[`amb:go:${a.id}`];
    if (went) {
      // A war begun for a claim runs its course whatever the dial says now.
      const n = g.nations[a.nat], H = g.nations[a.on];
      if (g.flags[`amb:done:${a.id}`] || !n?.alive || !H?.alive || !atWar(g, a.nat, a.on)) continue;
      if (treaty(g, a)) continue;
      live = true;
      aid(g, a);
      continue;
    }
    const warned = g.flags[`amb:${a.id}`];
    if (!readable(g, a)) { if (warned) { delete g.flags[`amb:${a.id}`]; delete g.flags[`amb:last:${a.id}`]; } continue; }
    const n = g.nations[a.nat], H = g.nations[a.on];
    const why = moment(g, a);
    if (why) {
      g.flags[`amb:last:${a.id}`] = g.turn;
      if (!warned) {
        g.flags[`amb:${a.id}`] = g.turn;
        const told = `${n.name} presses its claim to ${a.name}: ${why}. ${cap(a.why)}. ${theName(H, true)} is warned that it may march.`;
        // (A claim on the player's own ground is a warning to him, on a card of its own, with the ground to look at.)
        if (a.on === g.human) warning(g, 'A claim on your ground', `${told} It will not march for ${C.notice === 1 ? 'a month' : `${C.notice} months`} at least, and what it weighs is the troops you keep there.`, a.on, groundOf(g, a)[0]?.id, a.nat);
        else headline(g, 'event', told, a.nat);
      } else if (g.turn - warned >= C.notice && dialDraw(g, 'march', a.id, g.turn) < C.chance * deterred(g, a.nat, [a.on])) {
        // (A staff thinks twice before a march that would bring the whole League down on it.)
        march(g, a, why);
        live = live || !!g.flags[`amb:go:${a.id}`];
      }
    } else if (warned && g.turn - (g.flags[`amb:last:${a.id}`] ?? warned) >= C.lapse) {
      delete g.flags[`amb:${a.id}`];
      delete g.flags[`amb:last:${a.id}`];
      if (g.human === a.on) headline(g, 'event', `${n.name} lets its claim to ${a.name} rest for now.`, a.nat);
    }
  }
  if (live) g.flags['hist:live'] = 1; else delete g.flags['hist:live'];
}

/** A claim as the player reads it on the Powers tab. */
export interface ClaimView {
  id: string; nat: string; on: string; name: string; why: string; ground: string[];
  state: ClaimState;
  /** The month it began to press, or marched. */
  since: number;
  /** True when the ground claimed is the player's. */
  onYou: boolean;
}

/** Every claim that can be read now: those merely held, those being pressed, and those a country has gone to war for. */
export function claimsView(g: Game): ClaimView[] {
  if (!dialRules(g)) return [];
  const out: ClaimView[] = [];
  for (const a of [...HISTORY.ambitions, ...feedClaims(g)] as Ambition[]) {
    const went = g.flags[`amb:go:${a.id}`];
    const n = g.nations[a.nat], H = g.nations[a.on];
    if (!n || !H) continue;
    const base = { id: a.id, nat: a.nat, on: a.on, name: a.name, why: a.why, ground: a.ground, onYou: g.human === a.on };
    if (went) {
      if (!g.flags[`amb:done:${a.id}`] && n.alive && H.alive && atWar(g, a.nat, a.on)) out.push({ ...base, state: 'marched', since: went });
      continue;
    }
    if (!readable(g, a)) continue;
    const warned = g.flags[`amb:${a.id}`];
    out.push({ ...base, state: warned ? 'pressing' : 'claims', since: warned ?? 0 });
  }
  const rank = { marched: 0, pressing: 1, claims: 2 };
  return out.sort((x, y) => Number(y.onYou) - Number(x.onYou) || rank[x.state] - rank[y.state]);
}

// ---- The month ------------------------------------------------------------------------------------------------

/** The history dial's month, at the opening of every month of a grand campaign. At the middle stop it does nothing at all. */
export function historyMonth(g: Game): void {
  if (!dialRules(g) || (g.flags.dom ?? 0) > 0) return;
  if (stopOf(g) === FREE && !g.flags['hist:live']) return;
  bookMonth(g);
  dealMonth(g);
  claimsMonth(g);
}
