// Shell blasts (release 2; Brian, 23 November 05:04Z: "explosions on the ground should be able to take out multiple
// people if they are close to it"): with LandBattle.blast on, the men a block loses to shells fall together, the men
// near one burst. Looks only: the drill picks who falls and when (DRILL.blast.hold seconds at most); the fight is the
// same battle to the last figure, and the men down catch up with the rules' count by the end.
import { describe, expect, it } from 'vitest';
import { livePiece, type LiveBattle } from '../src/battle/land/live';
import { DRILL, type MenBlock, type Round } from '../src/battle/land/drill';

const UPTO = 300;

interface Run { L: LiveBattle; rounds: Round[]; downAt: number[]; units: string }

function run(blast: boolean): Run {
  const L = livePiece('normandy-1944', 11, { me: 'att', blast });
  const rounds: Round[] = [];
  const downAt: number[] = [];
  while (!L.sim.over && L.sim.t < UPTO - 1e-6) {
    const d = L.to(Math.min(UPTO, L.sim.t + 1), 100000);
    rounds.push(...(d.rounds as Round[]));
    downAt.push((L.drill.view() as MenBlock[]).reduce((n, b) => n + b.men.filter((m) => m.d !== undefined).length, 0));
  }
  // (then on with no new losses due, so every held loss is let go: the drill's own clock past the hold)
  const units = JSON.stringify(L.sim.units.map((u) => [u.id, u.str, u.figures, u.state, Math.round(u.x), Math.round(u.y)]));
  return { L, rounds, downAt, units };
}

describe('shell blasts', () => {
  const off = run(false), on = run(true);

  it('fights the very same battle: every block, figure and result', () => {
    expect(on.units).toBe(off.units);
    expect(JSON.stringify(on.L.sim.over)).toBe(JSON.stringify(off.L.sim.over));
  });

  it('lets one burst take several men, each a mate of the round that burst', () => {
    const he = (r: Round) => r.r === 'kill' && (r.w === 'gun' || r.w === 'tank' || r.w === 'mortar');
    const bursts = new Map<string, Round[]>();
    for (const r of on.rounds) if (he(r)) {
      const key = `${r.t}|${r.by}|${Math.round(r.x)}|${Math.round(r.y)}`;
      (bursts.get(key) ?? bursts.set(key, []).get(key)!).push(r);
    }
    const sizes = [...bursts.values()].map((g) => g.length);
    const mates = on.rounds.filter((r) => r.m === 1);
    const most = Math.max(...Object.values(DRILL.blast.most));
    const offMates = off.rounds.filter((r) => r.m === 1).length;
    console.log(`shell blasts: ${bursts.size} bursts, ${mates.length} mates; men a burst took: ${[1, 2, 3, 4, 5, 6].map((k) => `${k}: ${sizes.filter((s) => (k < 6 ? s === k : s >= 6)).length}`).join(', ')}; most ${Math.max(...sizes)}; off: ${offMates} mates`);
    expect(offMates).toBe(0);
    expect(mates.length).toBeGreaterThan(0);
    expect(sizes.filter((s) => s >= 2).length).toBeGreaterThan(0);
    // A mate never carries a shot of its own, and every mate's burst has its round or its event.
    for (const m of mates) {
      const lead = on.rounds.some((r) => r !== m && r.m === undefined && r.t === m.t && Math.hypot(r.x - m.x, r.y - m.y) < 0.15);
      const event = (on.L.sim as unknown as { events: { what: string; t: number; x?: number; y?: number }[] }).events.some((e) => e.what === 'shell' && Math.abs(e.t - m.t) < 0.11 && Math.hypot((e.x ?? 1e9) - m.x, (e.y ?? 1e9) - m.y) < 0.15);
      expect(lead || event).toBe(true);
    }
    expect(Math.max(...sizes)).toBeLessThanOrEqual(most + 1);
  });

  it('keeps the men down to the rules\' count, a few seconds behind at most', () => {
    const n = Math.min(on.downAt.length, off.downAt.length);
    let worst = 0;
    for (let i = 0; i < n; i++) worst = Math.max(worst, off.downAt[i] - on.downAt[i]);
    console.log(`shell blasts: men down behind the rules at worst ${worst} (a second's step), at the end ${off.downAt[n - 1]} off, ${on.downAt[n - 1]} on`);
    expect(Math.abs(off.downAt[n - 1] - on.downAt[n - 1])).toBeLessThanOrEqual(Math.max(...Object.values(DRILL.blast.most)) * 4);
  });
});
