// The raid-card gate's two new shapes (the design director, 25 November 2026 16:24Z: "The gates missed it, so the
// raid-card gate gains two shapes: a cut-escort raid (one escort group, two or more boxes, defence sending groups at the
// escort) and a long stream. Read bomber, escort and defender losses beside the wins"). The shipped loss gates
// (battle-air.test.ts, battle-air-styles.test.ts) fly raids with one box, or no escort, or an escort group for each
// box, and the fairness gate reads wins only; none of them flies either shape, so none caught Hampshire (the Hampshire
// trace, 25 November 2026). Each shape here is the staff's raid as the play file sends it (raidGroups: the bombers and
// every fighter group that can go from the nation's own airfields), from a grand campaign started at a turn where the
// shape stands: the month and the place are only where the forces come from, and nothing below reads either.
//
// Each shape is flown with the computer on both sides, the ticket's seed + k for battle k, and read against the card
// the field is armed from (place.forecast, place.chance): bombers, escort and defenders lost each within a tenth of the
// card, and the share won within 0.1 of the card's chance.
import { describe, expect, it } from 'vitest';
import { planRaid, raidTargets } from '../src/campaign/airwar';
import { loadGame } from '../src/campaign/save';
import type { Game } from '../src/campaign/types';
import type { FieldTicket } from '../src/battle/core/types';
import { airBattle } from '../src/battle/air/sim';
import { airTicket, raidGroups } from '../src/battle/air/ticket';
import type { AirPlace } from '../src/battle/air/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;
type Ticket = FieldTicket<AirPlace>;

/** A grand campaign of `nat` started at turn `from` (seed 11), and the staff's day raid on the named province. */
function raid(nat: string, from: number, name: string): Ticket {
  TC.start({ view: 'grand', human: nat, from, seed: 11 });
  const g: Game = loadGame(TC.save().text)!;
  const t = raidTargets(g, 'bomb', nat).find((x) => planRaid(g, 'bomb', x.id, undefined, nat).place === name);
  if (!t) throw new Error(`no raid on ${name} for ${nat}`);
  const { ticket, why } = airTicket(g, 'bomb', t.id, raidGroups(g, 'bomb', t.id, nat), nat);
  if (!ticket) throw new Error(why ?? 'no ticket');
  return ticket;
}

interface Read { n: number; bmr: number; ftr: number; foe: number; won: number; seBmr: number; hunters: number }

/** Flies the raid `n` times (seed + k), the computer on both sides: the mean losses, the share won, the bombers' standard error, and the most groups the defence sent at the escort. */
function fly(tk: Ticket, n: number): Read {
  const bm: number[] = [];
  let ftr = 0, foe = 0, won = 0, hunters = 0;
  for (let k = 0; k < n; k++) {
    const s = airBattle({ ...tk, seed: tk.seed + k });
    s.ai.att = true; s.ai.def = true;
    let most = 0;
    while (!s.over) { s.step(); most = Math.max(most, s.units.filter((u) => u.side === 'def' && u.def === 'escort').length); }
    hunters = Math.max(hunters, most);
    let b = 0;
    for (const gr of s.outcome().groups) if (gr.side === 'def') foe += gr.lostFtr; else { ftr += gr.lostFtr; b += gr.lostBmr; }
    bm.push(b);
    if (s.over!.level.includes('victory')) won++;
  }
  const bmr = bm.reduce((a, x) => a + x, 0) / n;
  const sd = Math.sqrt(bm.reduce((a, x) => a + (x - bmr) ** 2, 0) / (n - 1));
  return { n, bmr, ftr: ftr / n, foe: foe / n, won: won / n, seBmr: sd / Math.sqrt(n), hunters };
}

const boxes = (tk: Ticket): number => tk.att.forces.filter((f) => f.kind === 'bmr').length;
const escorts = (tk: Ticket): number => tk.att.forces.filter((f) => f.kind !== 'bmr').length;
const say = (what: string, got: number, want: number): string => `${what}: field ${got.toFixed(2)} against card ${want.toFixed(2)}`;

describe('the raid card against the field: the cut-escort raid and the long stream (release 2)', () => {
  it('a cut-escort raid (one escort group, two boxes, the defence sending groups at the escort) loses what its card says', () => {
    // GER on Hampshire from turn 33: two bomber boxes, one fighter group with them, four defending groups.
    const tk = raid('GER', 33, 'Hampshire');
    expect(boxes(tk)).toBeGreaterThanOrEqual(2);
    expect(escorts(tk)).toBe(1);
    const f = tk.place.forecast;
    const r = fly(tk, 32);
    // The defence did send groups at the escort.
    expect(r.hunters).toBeGreaterThanOrEqual(1);
    // 32 battles hold the bombers' standard error under 3 in 100 of the card.
    expect(r.seBmr / f.bmr).toBeLessThan(0.03);
    console.log(`cut-escort: n ${r.n} bombers ${r.bmr.toFixed(2)} (se ${r.seBmr.toFixed(2)}) card ${f.bmr.toFixed(2)}; escort ${r.ftr.toFixed(2)} card ${f.ftr.toFixed(2)}; defenders ${r.foe.toFixed(2)} card ${f.foe.toFixed(2)}; won ${r.won.toFixed(3)} chance ${tk.place.chance.toFixed(3)}; hunters ${r.hunters}`);
    expect(Math.abs(r.bmr - f.bmr), say('bombers', r.bmr, f.bmr)).toBeLessThan(0.1 * f.bmr);
    expect(Math.abs(r.ftr - f.ftr), say('escort', r.ftr, f.ftr)).toBeLessThan(0.1 * f.ftr);
    expect(Math.abs(r.foe - f.foe), say('defenders', r.foe, f.foe)).toBeLessThan(0.1 * f.foe);
    expect(Math.abs(r.won - tk.place.chance), say('won', r.won, tk.place.chance)).toBeLessThanOrEqual(0.1);
  }, 120_000);

  it('a long stream (seven boxes, two escort groups): what is within the card is held there; what is not is pinned as known', () => {
    // SOV on Kaluga from turn 41: seven bomber boxes, two fighter groups with them, one defending group.
    const tk = raid('SOV', 41, 'Kaluga');
    expect(boxes(tk)).toBeGreaterThanOrEqual(5);
    expect(escorts(tk)).toBeGreaterThanOrEqual(1);
    expect(escorts(tk)).toBeLessThanOrEqual(2);
    const f = tk.place.forecast;
    const r = fly(tk, 24);
    expect(r.seBmr / f.bmr).toBeLessThan(0.03);
    console.log(`long stream: n ${r.n} bombers ${r.bmr.toFixed(2)} (se ${r.seBmr.toFixed(2)}) card ${f.bmr.toFixed(2)}; escort ${r.ftr.toFixed(2)} card ${f.ftr.toFixed(2)}; defenders ${r.foe.toFixed(2)} card ${f.foe.toFixed(2)}; won ${r.won.toFixed(3)} chance ${tk.place.chance.toFixed(3)}`);
    expect(Math.abs(r.foe - f.foe), say('defenders', r.foe, f.foe)).toBeLessThan(0.1 * f.foe);
    // KNOWN, not mended (the design director, 25 November 2026 16:24Z: "Kaluga's gap is its own shape: trace it apart").
    // Today the long stream loses fewer bombers and fewer escorts than its card, and wins more often than its chance;
    // the mend that closes that gap must change these lines on purpose.
    expect(r.bmr, say('bombers (known: under the card)', r.bmr, f.bmr)).toBeLessThan(0.9 * f.bmr);
    expect(r.ftr, say('escort (known: under the card)', r.ftr, f.ftr)).toBeLessThan(0.9 * f.ftr);
    expect(r.won, say('won (known: over the chance)', r.won, tk.place.chance)).toBeGreaterThan(tk.place.chance + 0.1);
  }, 120_000);
});
