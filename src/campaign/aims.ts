// War aims dealt to the great powers (the sandbox's lock sweep and playtest, finding 4; the pool is the historian's,
// historian/sandbox-thresholds.md part 8; after release 1). At the stops history.json's claims 'deal' names, a
// computer-run great power's claims on named ground (history.json 'ambitions') are its war aims:
//   it holds two or three of them in any one war, dealt from the war's seed (the claims of one 'set' are one aim);
//   they can be read on the Powers tab from the first month (claims.ts, claimsView);
//   its staff takes one up as a great choice of its own (staff.ts: an operation made from the aim), when its fronts
//     are quiet and it can bring the claims' own odds against the holder; the army comes up for the claims' months
//     of notice, the holder is warned, and then it marches. So a great power need not be at peace to go for one;
//   its moment is the claims' own, read for a great power. Where the stop is not bold it needs something to hide
//     behind: the holder is at war already, or another great power is hard pressed (its will to fight down by the
//     claims' 'weak' share) or has gone under, and those who might have stopped it have their hands full. Where
//     anything goes it needs only the odds and its own month, dealt from the war's seed;
//   the war that follows is a claim's war as before (claims.ts): the holder's great ally may stand by it, a
//     claimant outside both alliances fights beside the holder's enemies, and a war between two countries outside
//     both alliances ends in a treaty once the claimant holds what it claimed.
// A claim that is the staff's own war of history's menu (an ambition with 'op': the Winter War) is no part of the
// deal. The powers that fought to restore borders hold no claims in the data, so nothing is dealt to them. Nothing
// here is read below those stops, or for the player's own country.

import diplomacyData from '../../data/campaign/diplomacy.json';
import worldData from '../../data/campaign/world.json';
import { holds, majorsOf } from './diplomacy';
import { allied, atWar, bloc, collapseScore, joinedSide } from './game';
import { HISTORY, dialDraw, stopDef, type Ambition } from './history';
import type { Game } from './types';

const C = HISTORY.claims, D = C.deal;
// (Every country that is a great power on some chart, read from the data: this file is loaded before diplomacy.ts has run.)
const GREAT = new Set([...(diplomacyData as unknown as { majors: string[] }).majors, ...(worldData as unknown as { great: string[] }).great]);

/** An aim a great power may be dealt: one claim, or the claims of one set. */
export interface Aim { key: string; nat: string; name: string; rows: Ambition[] }

/** Every aim in the pool, by claimant, in the order of the data. */
export const AIMS: Record<string, Aim[]> = {};
if (D) for (const a of HISTORY.ambitions) {
  if (a.op || !GREAT.has(a.nat)) continue;
  const key = a.set ?? a.id, list = (AIMS[a.nat] ??= []);
  let aim = list.find((x) => x.key === key);
  if (!aim) list.push((aim = { key, nat: a.nat, name: (a.set ? D.sets?.[a.set] : undefined) ?? a.name, rows: [] }));
  aim.rows.push(a);
}

/** True where a nation's claims are war aims dealt to it: a computer-run great power, at a stop the deal names. */
export const dealStop = (g: Game, nat: string): boolean => {
  const S = stopDef(g);
  return !!D && !!S.ambitions && D.stops.includes(S.stop) && nat !== g.human && majorsOf(g).includes(nat);
};

/** True for a claim that belongs to the deal where the deal runs: its claimant's war aim if dealt, and nobody's if not. */
export const inDeal = (g: Game, a: Ambition): boolean => !a.op && dealStop(g, a.nat);

/** The aim a claim belongs to, if it is in the pool. */
export const aimOf = (a: Ambition): Aim | null => AIMS[a.nat]?.find((x) => x.rows.includes(a)) ?? null;

/** The aims dealt to a great power in this war: two or three of those its stop reads, by the war's seed. */
export function dealt(g: Game, nat: string): Aim[] {
  if (!dealStop(g, nat)) return [];
  const stop = stopDef(g).stop;
  const pool = (AIMS[nat] ?? []).filter((x) => x.rows.some((a) => (a.stop ?? 4) <= stop));
  const [lo, hi] = D!.count;
  const n = lo + Math.floor(dialDraw(g, 'aims', nat) * (hi - lo + 1));
  return pool.map((x) => [dialDraw(g, 'aim', nat, x.key), x] as [number, Aim]).sort((p, q) => p[0] - q[0]).slice(0, n).map((p) => p[1]);
}

/** True for a claim its claimant was dealt in this war. */
export const isDealt = (g: Game, a: Ambition): boolean => dealt(g, a.nat).some((x) => x.rows.includes(a));

/** True while some other great power is hard pressed or has gone under: the balance is upset, and nobody is free to stop a march. */
export const upset = (g: Game, nat: string): boolean => majorsOf(g).some((id) => {
  const P = g.nations[id];
  return id !== nat && !!P && !P.dormant && (P.alive ? collapseScore(g, id) / P.resilience >= C.weak : !!P.fellOn);
});

/**
 * The holders a great power may march on for an aim this month: none unless the aim was dealt to it and its month
 * has come. A holder must stand, be at peace with it and no ally of its own, keep some of the ground claimed, and
 * be outside both alliances except at the stops where the claims' table lets a great power march on a member of an
 * alliance not its own ('inBloc'). Where the stop is not bold the holder must be at war already, or the balance
 * among the great powers upset.
 */
export function aimHolders(g: Game, nat: string, key: string): string[] {
  const aim = dealt(g, nat).find((x) => x.key === key);
  const n = g.nations[nat];
  if (!aim || !n?.alive || n.dormant || n.league) return [];
  const S = stopDef(g);
  // Where anything goes each claimant bides its own time, some for years: the month is dealt from the war's seed.
  if (g.turn < 3 + Math.floor(dialDraw(g, 'wait', nat, key) * (S.wait ?? 0))) return [];
  const cover = !!S.bold || upset(g, nat);
  const out: string[] = [];
  for (const a of aim.rows) {
    const H = g.nations[a.on];
    if ((a.stop ?? 4) > S.stop || !H?.alive || H.dormant || out.includes(a.on) || atWar(g, nat, a.on) || allied(g, nat, a.on)) continue;
    if (!cover && !H.atWar.some((e) => g.nations[e]?.alive)) continue;
    if (bloc(H) && !(C.inBloc ?? []).includes(S.stop)) continue;
    if (!(a.if ?? []).every((c) => holds(g, c))) continue;
    if (!a.ground.some((nm) => { const r = g.byName[nm]; return !!r && r.owner === a.on && r.ctrl === a.on; })) continue;
    out.push(a.on);
  }
  return out;
}

/**
 * What follows the declaration of a war for an aim, in the month it is made: the claims it was declared for are
 * marked as marched for (claims.ts takes them from there), shipping is gathered for ground across the sea, and a
 * claimant outside both alliances that has marched on a member of one fights beside the other if the other is at
 * war with the holder.
 */
export function aimMarched(g: Game, nat: string, key: string): void {
  const aim = AIMS[nat]?.find((x) => x.key === key), n = g.nations[nat];
  if (!aim || !n) return;
  for (const a of aim.rows) {
    const H = g.nations[a.on];
    if (!H || g.flags[`amb:go:${a.id}`] || g.flags[`war:${nat}:${a.on}`] !== g.turn || !atWar(g, nat, a.on)) continue;
    if (a.lift) { n.sealift = Math.max(n.sealift ?? 0, a.lift); g.flags[`st:claim:${nat}`] = Object.keys(g.nations).indexOf(a.on) + 1; }
    const hb = bloc(H);
    if ((hb === 'axis' || hb === 'allies') && bloc(n) === null) {
      const other = hb === 'axis' ? 'allies' : 'axis';
      if (H.atWar.some((x) => x !== nat && g.nations[x]?.alive && bloc(g.nations[x]) === other)) {
        n.align = other === 'axis' ? 'axis_cobelligerent' : 'allies_cobelligerent';
        n.opinion = n.home = other === 'axis' ? 100 : -100;
        g.supplyDirty = true;
        joinedSide(g, nat);
      }
    }
    g.flags[`amb:go:${a.id}`] = g.turn;
    g.flags['hist:live'] = 1;
  }
}
