// The scramble offer's stand-in on the bridge (release 2; Brian, 24 November 2026 22:21Z: "i want every air battle
// playable"). In the enemy's month the campaign is to stop where an enemy raid meets his fighters and offer SCRAMBLE:
// taken, he flies the defender's side in the 3D air battle, and the month runs on with what the battle decided. The
// warplanes thread builds the real stop in the month (the contract, /mnt/project-files/air-battles/every-kind/CONTRACT.md
// section 7). Until it lands, this file lets the 3D game's half be played end to end: it finds a raid an enemy's staff
// could fly on him now, offers it in the contract's own shape, and flies it with his side the defender. It is a
// DEMONSTRATION: nothing it does is booked, and the campaign is exactly as it was when the battle ends.
//
//   TC.scrambleMode(mode)    -> { mode }: 'every' (every raid that meets his fighters), 'big' (big raids only; the
//                               3D game's OPTIONS default) or 'never', the 3D game's saved setting, told before every
//                               TC.endMonth while its "Scramble your fighters" is on. Kept here, outside the save; it
//                               starts at 'never' (the reviewer, 25 November: with the switch off the 3D game asks the
//                               rules nothing new, so the rules must not stop a month it was never told it may). The
//                               demonstration offers nothing under 'never' and treats 'big' as 'every' (the real month
//                               says what a big raid is).
//   TC.scrambleDemo()        -> the contract's scramble object, { id, kind, target, name, nat, adj, words, ticket: true,
//                               bombers, escorts, fighters, groups, demo: true }, for the biggest day bombing raid an enemy
//                               at war with him could fly now on a province of his where his own fighters would rise; or
//                               null where there is none. Reads the war, writes nothing. The 3D game asks it once after
//                               TC.endMonth's answer, with -TheaterAirScrambleDemo only, as if the month had stopped there.
//   TC.scrambleStart(id)     -> { sky, view } as TC.airStart, the raid flown by the enemy and his side 'def' (the battle
//                               bridge's airStartOn); null for an id this file did not offer. TC.battleTick and the rest
//                               then drive it. TC.battleEnd answers { ok: true, note, after, demo: true }: the note says it
//                               was a demonstration; the after card is the one the raid would have had, told from his side,
//                               made on a throwaway copy of the war (nothing is booked on the war itself).
//   TC.scrambleLeave(id, { rest }) -> { ok: true }: the staff would meet it with the dice; in the demonstration nothing is
//                               flown. rest: true is the card's tick, "Settle the rest of this month's raids".
//
// campaign-bridge.ts adds these to TC with Object.assign(TC, scrambleCalls(game, battleTable.airStartOn)).

import { afterCard, type FoughtAir } from '../../../src/campaign/airfought';
import { launchRaid, planRaid, raidTargets, type RaidKind } from '../../../src/campaign/airwar';
import { airTicket } from '../../../src/battle/air/ticket';
import { loadGame, saveGame } from '../../../src/campaign/save';
import type { Game } from '../../../src/campaign/types';

export type ScrambleMode = 'every' | 'big' | 'never';

/** The scramble object of the contract (section 7), as TC.endMonth will carry it in place of the month's card. */
export interface ScrambleOffer {
  /** The raid's own key for TC.scrambleStart and TC.scrambleLeave. */
  id: string;
  kind: RaidKind;
  /** The province raided, and its name. */
  target: number;
  name: string;
  /** The raiding nation, and its word ("German"). */
  nat: string;
  adj: string;
  /** One or two plain sentences for the card: who is coming, about how many, what he has to meet them. */
  words: string;
  /** A battle in the air can be fought for it (always true when offered). */
  ticket: true;
  /** The raid's bombers and its escorting fighters, and his fighters that can rise to meet it (the ticket's figures). */
  bombers: number;
  escorts: number;
  fighters: number;
  /** His air groups that rise, by name. */
  groups: string[];
  /** Offered by this stand-in, not by the month. */
  demo: true;
}

/** The note TC.battleEnd answers at the demonstration's end, for the line under the map. */
export const DEMO_NOTE = 'That was a demonstration of the scramble: nothing was booked, and the month goes on as it was.';

/** One raid offered, as TC.scrambleStart flies it. */
interface Offered { kind: RaidKind; target: number; nat: string; ids: number[] }

const scrambleCount = (n: number): string => n.toLocaleString('en');
/** "A" or "An" before a word ("An Italian raid"). */
const scrambleAn = (word: string): string => (/^[AEIOU]/i.test(word) ? 'An' : 'A');

/**
 * The biggest day bombing raid an enemy's staff could fly on one of his provinces now, where his own fighters would rise
 * to meet it (planRaid's meets), with the battle's ticket to prove it: the ticket's defender is his nation and has forces.
 * Enemies in the order the war lists them, provinces nearest the enemy's airfields first; the raid with the most bombers
 * wins, the nearest on a tie. Reads the war; writes nothing.
 */
export function findScramble(g: Game): (ScrambleOffer & { ids: number[] }) | null {
  const me = g.human;
  if (!me || !g.nations[me]?.atWar.length) return null;
  const kind: RaidKind = 'bomb';
  const found: { nat: string; target: number; ids: number[]; bombers: number; km: number }[] = [];
  for (const nat of g.nations[me].atWar) {
    if (!g.nations[nat]?.alive) continue;
    for (const t of raidTargets(g, kind, nat)) {
      if (t.sea || t.holder !== me) continue;
      const plan = planRaid(g, kind, t.id, undefined, nat);
      if (plan.why || !plan.picked.length || plan.bombers <= 0) continue;
      // (his own fighters would rise to meet it)
      if (!plan.meets.some((m) => m.nat === me && m.able > 0)) continue;
      found.push({ nat, target: t.id, ids: plan.picked, bombers: plan.bombers, km: t.km });
    }
  }
  found.sort((a, b) => b.bombers - a.bombers || a.km - b.km || a.target - b.target);
  // (the battle's ticket for the best few: the field must have a fight to show, his side the defender)
  for (const c of found.slice(0, 6)) {
    const t = airTicket(g, kind, c.target, c.ids, c.nat);
    if (!t.ticket || t.ticket.def.nat !== me || !t.ticket.def.forces.length) continue;
    const sum = (rows: typeof t.ticket.att.forces, k: string): number => rows.filter((f) => f.kind === k).reduce((s, f) => s + f.men, 0);
    const bombers = sum(t.ticket.att.forces, 'bmr'), escorts = sum(t.ticket.att.forces, 'ftr'), fighters = sum(t.ticket.def.forces, 'ftr');
    const adj = g.nations[c.nat]?.adj ?? c.nat;
    const name = g.regions[c.target].name;
    const groups = [...new Set(t.ticket.def.forces.map((f) => f.name))];
    const words = `${scrambleAn(adj)} ${adj} raid is heading for ${name}: about ${scrambleCount(bombers)} bombers, ${escorts > 0 ? `with ${scrambleCount(escorts)} fighters as escort` : 'with no escort'}. `
      + `About ${scrambleCount(fighters)} of your fighters can rise to meet it${groups.length ? ` (${groups.slice(0, 3).join(', ')}${groups.length > 3 ? ' and more' : ''})` : ''}.`;
    return {
      id: `demo:${kind}:${c.target}:${c.nat}:${c.ids.join('.')}`, kind, target: c.target, name, nat: c.nat, adj, words, ticket: true,
      bombers, escorts, fighters, groups, demo: true, ids: c.ids,
    };
  }
  return null;
}

/**
 * The calls. 'airStartOn' is the battle bridge's (battle-bridge.ts): the raid's battle on the shared clock with the side
 * given, settled through the settle given (here: nothing booked).
 */
export function scrambleCalls(
  game: () => Game,
  airStartOn: (g: Game, kind: string, target: number, ids: number[], nat: string, me: 'att' | 'def', settle?: (fought?: FoughtAir) => unknown) => unknown,
) {
  // (starts at 'never' until the 3D game tells it otherwise: its switch off, it sends nothing)
  let mode: ScrambleMode = 'never';
  // (the raids offered, by id: only an id this file offered is flown)
  const offered = new Map<string, Offered>();
  return {
    scrambleMode(m: string) {
      if (m === 'every' || m === 'big' || m === 'never') mode = m;
      return { mode };
    },
    scrambleDemo() {
      if (mode === 'never') return null;
      const g = game();
      const offer = findScramble(g);
      if (!offer) return null;
      const { ids, ...out } = offer;
      offered.set(out.id, { kind: out.kind, target: out.target, nat: out.nat, ids });
      return out;
    },
    scrambleStart(id: string) {
      const r = offered.get(String(id));
      if (!r) return null;
      const g = game();
      // The end: the after card the raid would have had, told from his side, made on a copy of the war that is then
      // thrown away. The war itself is not touched (the save text is the same before and after).
      const settle = (fought?: FoughtAir): unknown => {
        let after: unknown;
        const copy = fought ? loadGame(saveGame(g)) : null;
        const rep = copy ? launchRaid(copy, r.kind, r.target, r.ids, r.nat, fought) : null;
        if (copy && rep && fought) after = afterCard(copy, rep, fought);
        return { ok: true, note: DEMO_NOTE, demo: true, ...(after ? { after } : {}) };
      };
      return airStartOn(g, r.kind, r.target, r.ids, r.nat, 'def', settle);
    },
    scrambleLeave(id: string, opts?: { rest?: boolean }) {
      offered.delete(String(id));
      void opts;
      return { ok: true };
    },
  };
}
