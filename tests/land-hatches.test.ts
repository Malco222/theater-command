// Heads up, hatches down (release 2; Brian, 25 November 2026, 15:11Z: "do those 3 ideas": tank commanders ride with
// heads out for a better view, then button up when bullets fly). Looks only (ruling 217): Drill.hatches gives each
// tank's commander his own nerve (drill.ts alarm and hatchNow, DR.hatch). These tests hold that it changes nothing in
// the fight, that a block's tanks no longer close up as one, that commanders close up when rounds come at them and
// come up again when it is quiet, that green crews are down more than veterans, and that the rules of the roof gun
// and the struck commander still hold. What a closed-up tank can see is the rules' business and is not touched here.
import { describe, expect, it } from 'vitest';
import { LandBattle, sampleTicket, withTeams } from '../src/battle/land';

const sig = (b: LandBattle): string => JSON.stringify({ over: b.over, t: b.t, units: b.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged]) });
const ticket = (seed = 5, att = 'USA', def = 'GER') => withTeams(sampleTicket('plains', 1.3, { seed, nat: { att, def }, style: { att: 'methodical', def: 'bold' } }));

interface Seen { sig: string; samples: number; shut: number; far: number; farShut: number; flips: number; opened: number; asOne: number; mixed: number; blocks: number; roofShut: number; cmBad: number; cm: number }

/** Fights the battle, reading twice a second which tanks are closed up. `xp` sets every armoured block's experience first. */
function watch(hatches: boolean, seed = 5, xp?: number): Seen {
  const b = new LandBattle(ticket(seed));
  if (xp !== undefined) for (const u of b.units) if (u.kind === 'arm') u.xp = xp;
  const d = b.drillOn({ sections: true, platoons: true, ...(hatches ? { hatches: true } : {}) });
  const s: Seen = { sig: '', samples: 0, shut: 0, far: 0, farShut: 0, flips: 0, opened: 0, asOne: 0, mixed: 0, blocks: 0, roofShut: 0, cmBad: 0, cm: 0 };
  const last = new Map<string, boolean>(), struck = new Set<string>();
  let seenRound = 0;
  for (let k = 0; !b.over && k < 30000; k++) {
    b.step();
    let view: ReturnType<typeof d.view> | null = null;
    for (const r of d.rounds()) {
      if (r.n <= seenRound) continue;
      seenRound = r.n;
      if (r.cm) { struck.add(`${r.on}:${r.ok}`); s.cm++; }
      if (r.g === 'roof' && r.by !== undefined) {
        view ??= d.view();
        if (view.find((x) => x.id === r.by)?.men.find((m) => m.k === r.k)?.b) s.roofShut++;
      }
    }
    if (k % 5) continue;
    view ??= d.view();
    for (const blk of view) {
      const u = b.unit(blk.id)!;
      if (u.kind !== 'arm') continue;
      const tanks = blk.men.filter((m) => m.r === 'tank' && m.d === undefined);
      if (!tanks.length) continue;
      let gap = Infinity;
      for (const e of b.units) if (e.side !== u.side && e.figures > 0 && e.state !== 'gone' && e.state !== 'withdrawn') gap = Math.min(gap, b.gap(u, e));
      let changed = 0, down = 0;
      for (const m of tanks) {
        const key = `${blk.id}:${m.k}`, was = last.get(key), now = !!m.b;
        if (was !== undefined && was !== now) { s.flips++; changed++; if (!now) s.opened++; }
        last.set(key, now);
        s.samples++; if (now) { s.shut++; down++; }
        if (gap > 300) { s.far++; if (now) s.farShut++; }
        if (struck.has(key) && !now) s.cmBad++;
      }
      if (tanks.length >= 3) { s.blocks++; if (changed === tanks.length) s.asOne++; if (down > 0 && down < tanks.length) s.mixed++; }
    }
  }
  s.sig = sig(b);
  return s;
}

describe('heads up, hatches down (looks only)', () => {
  const off = watch(false), on = watch(true);

  it('changes nothing in the fight', () => {
    expect(on.sig).toBe(off.sig);
    expect(watch(true, 23).sig).toBe(watch(false, 23).sig);
  }, 300_000);

  it('commanders close up when rounds come at them, wherever the enemy is; off, only inside 120 m', () => {
    // Off: no tank more than 300 m from every enemy is closed up, but one whose commander was struck in his hatch.
    expect(off.farShut).toBeLessThanOrEqual(off.cm * 600);
    expect(on.farShut).toBeGreaterThan(off.farShut + 20);
    // And they are not simply shut for good: most of the time out there they ride heads up, and they come up again.
    expect(on.farShut / on.far).toBeLessThan(0.6);
    expect(on.opened).toBeGreaterThan(3);
  });

  it('each commander in his own time: a block never closes or opens as one', () => {
    expect(on.asOne).toBe(0);
    // Some of a block's tanks closed up while others still ride heads up: seen often with the switch on.
    expect(on.mixed).toBeGreaterThan(Math.max(10, off.mixed * 3));
  });

  it('green crews are down early and long, veterans late and briefly', () => {
    const raw = watch(true, 5, 0.05), vet = watch(true, 5, 0.9);
    expect(raw.shut / raw.samples).toBeGreaterThan(vet.shut / vet.samples + 0.1);
  }, 300_000);

  it('the roof gun never fires from a closed tank, and a struck commander stays down', () => {
    for (const s of [on, watch(true, 11)]) { expect(s.roofShut).toBe(0); expect(s.cmBad).toBe(0); }
  }, 300_000);
});
