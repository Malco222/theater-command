// A division is laid once, over whole wars (release 2, the grand campaign; src/campaign/drawn.ts).
//
// The design director's tests for her rulings of 25 November 2026 (13:43Z to 14:28Z): over six years of a war nobody
// plays, on the four seeds the joiner's check was made with (joins/named-twice-check-1125.md: 1, 2, 5 and 11), no
// division is held by two living corps of one country in any month; every dated row that names a division a living
// corps holds either brings it from that corps or leaves it there, by the rule; and no row's strength is laid for a
// division that is held: the country is the stronger by the row's new divisions and by nothing else. Before the rule
// each of these wars had a division standing twice from the month its second row landed (the 1st Armoured in England
// and at Alexandria from month 32 in every one).
//
// Slow: a six-year war to each seed. The rule itself is read on a board set by hand in price-once-r2.test.ts.
import { afterEach, describe, expect, it } from 'vitest';
import { divisionsOf } from '../src/campaign/armies';
import { drawnWatch } from '../src/campaign/drawn';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';

type Told = Parameters<NonNullable<typeof drawnWatch.tell>>[0];

afterEach(() => { drawnWatch.tell = null; drawnWatch.laid = null; });

const holds = (g: Game, c: Corps, name: string): boolean => divisionsOf(g, c).some((d) => d.name === name);

describe('six years of a grand war nobody plays: no division stands twice, and none is raised while it is held', () => {
  it.each([1, 2, 5, 11])('seed %i', (seed) => {
    const g = startOpen(createGame(seed, null));
    const told: Told[] = [];
    const faults: string[] = [];
    let laid = 0, came = 0, stayed = 0, raised = 0;
    drawnWatch.tell = (e) => { told.push(e); };
    drawnWatch.laid = (e) => {
      laid++;
      // The row as it was read this month: each division it found held is with the corps the rule says.
      const t = [...told].reverse().find((x) => x.turn === e.turn && x.nat === e.nat && x.row === e.row);
      if (!t) { faults.push(`month ${e.turn} ${e.nat} "${e.row}": laid without being read`); return; }
      const mine = g.corps.find((c) => c.id === e.corps);
      if (!mine || mine.dead) { faults.push(`month ${e.turn} ${e.nat} "${e.row}": no corps stands for it`); return; }
      for (const q of t.claims) {
        const holder = q.id === null ? undefined : g.corps.find((c) => c.id === q.id && !c.dead);
        if (q.stays) {
          stayed++;
          // (One formed out of another stays as that other, in the corps that has it; with nobody to give one, nothing is formed.)
          if (q.id !== null && !(holder && holds(g, holder, q.conv ?? q.name))) faults.push(`month ${e.turn} ${e.nat} "${e.row}": the ${q.conv ?? q.name} was to stay with "${q.from}" and is not there`);
          if (holds(g, mine, q.name) && holder !== mine) faults.push(`month ${e.turn} ${e.nat} "${e.row}": the ${q.name} stays with "${q.from}" and was laid as well`);
        } else {
          came++;
          if (!holds(g, mine, q.name)) faults.push(`month ${e.turn} ${e.nat} "${e.row}": the ${q.name} did not come from "${q.from}"`);
          if (holder && holder !== mine && holds(g, holder, q.conv ?? q.name)) faults.push(`month ${e.turn} ${e.nat} "${e.row}": the ${q.conv ?? q.name} came and "${q.from}" has it still`);
        }
      }
      // No strength is laid for a division that is held: the country is the stronger by its new divisions alone.
      raised += e.fresh;
      if (Math.abs(e.after - e.before - e.fresh * e.str) > 1e-9) faults.push(`month ${e.turn} ${e.nat} "${e.row}": ${e.fresh} new at ${e.str}, and its country went from ${e.before} to ${e.after}`);
    };
    while (g.turn <= 72) {
      const month = g.turn;
      runTurnAI(g);
      for (const nat of Object.keys(g.nations)) {
        const seen = new Map<string, Corps>();
        for (const c of g.corps) if (c.nat === nat && !c.dead) for (const d of divisionsOf(g, c)) {
          const was = seen.get(d.name);
          if (was && was !== c) faults.push(`month ${month} ${nat}: the ${d.name} stands twice, in "${was.name}" and in "${c.name}"`);
          seen.set(d.name, c);
        }
      }
      if (faults.length > 20) break;
    }
    expect(faults).toEqual([]);
    // (The war did meet the rule: each of these four has a row that names a held division, Britain's 1st Armoured
    // for Alexandria at the least.)
    expect(told.some((t) => t.nat === 'UK' && t.row === '1st Armoured Division')).toBe(true);
    expect(came + stayed).toBeGreaterThan(0);
    expect(laid).toBeLessThanOrEqual(told.length);
    expect(raised).toBeGreaterThanOrEqual(0);
  }, 900_000);
});
