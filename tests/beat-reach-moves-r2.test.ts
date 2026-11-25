// A fleet's beat lies within its base's reach after every move of its base (release 2; the design director's rulings of
// 20:02Z, 20:19Z and 21:09Z (a), 25 November 2026). In the 24 whole-world wars the moves the first mend did not reach
// left the beat out of reach 95 times in 534: a fleet whose base fell (the China Fleet from Palu to Darwin, its beat the
// Natuna Sea for 34 months), a navy escaping (the Soviet Northern Fleet from Murmansk to Iceland, its beat the Barents
// Sea for 50). One rule, beatOnMove, read by every move of base (rebase, turnBack) under its own switch (beatReach): the
// beat it had while that lies in reach of the new base, else the new home sea. Off, every beat is as shipped.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { beatOnMove, fleetReach, inPassage, navalUpkeep, rebase, setBeatReach, turnBack } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game, Region } from '../src/campaign/types';

const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const sea = (g: Game, name: string): number => g.seas.findIndex((z) => z.name === name);
const reach = (g: Game, f: Fleet): number[] => fleetReach(g, f, true, f.duty === 'raid');

/** The whole world in its first month, Japan at war with Britain. */
function world(on: boolean): Game {
  const g = startOpen(createGame(3, null));
  if (on) setBeatReach(g, true);
  if (!atWar(g, 'JAP', 'UK')) declareWar(g, 'JAP', 'UK');
  return g;
}

/** A harbour of the fleet's side its staff could move it to, with the fleet's present beat out of its reach. */
function farHarbour(g: Game, f: Fleet): Region {
  const beat = f.station, keep = [f.base, f.port, f.home, f.zone, f.stance] as const;
  for (const r of g.regions) {
    if (!r.port || !r.seas.length || r.ctrl !== f.nat || r.id === f.base) continue;
    f.base = r.id;
    const out = !reach(g, f).includes(beat);
    [f.base, f.port, f.home, f.zone, f.stance] = keep;
    if (out) return r;
  }
  throw new Error('no harbour');
}

describe("every move of a fleet's base keeps its beat in the new base's reach (beatReach)", () => {
  it('a fleet whose base falls: on, its beat is its new home sea where the old one is out of reach; off, it keeps the beat it had', () => {
    // Where the China Fleet goes as Singapore falls, and a beat out of that harbour's reach (its beat of a month before).
    const g0 = world(false), f0 = fleet(g0, 'UK', 'China Fleet');
    g0.regions[f0.base].ctrl = 'JAP';
    navalUpkeep(g0, 'UK');
    const to = f0.base, out = g0.seas.findIndex((_, z) => !reach(g0, f0).includes(z) && g0.seas[z].adj.length > 0);
    expect(g0.regions[to].name).not.toBe('Singapore');
    for (const on of [false, true]) {
      const g = world(on);
      const f = fleet(g, 'UK', 'China Fleet');
      const lost = g.regions[f.base];
      f.station = out;
      lost.ctrl = 'JAP';
      const news = g.headlines.length;
      navalUpkeep(g, 'UK');
      expect(f.base).toBe(to);
      expect(g.headlines.slice(news).some((h) => h.text.startsWith(`With ${lost.name} lost`))).toBe(true);
      expect(f.station).toBe(on ? f.home : out);
      if (on) expect(reach(g, f)).toContain(f.station);
      // Its beat in reach of the new base, it keeps it.
      const g2 = world(on), f2 = fleet(g2, 'UK', 'China Fleet'), kept = f2.station;
      g2.regions[f2.base].ctrl = 'JAP';
      navalUpkeep(g2, 'UK');
      expect(reach(g2, f2)).toContain(kept);
      expect(f2.station).toBe(kept);
    }
  });

  it('any move of base by its staff or by the player: on, a beat out of reach becomes the new home sea and one in reach is kept; off, as shipped', () => {
    for (const on of [false, true]) {
      const g = world(on);
      const f = fleet(g, 'UK', 'Home Fleet');
      const was = f.station;
      const r = farHarbour(g, f);
      expect(rebase(g, f, r)).toBe(true);
      expect(f.base).toBe(r.id);
      expect(f.station).toBe(on ? f.home : was);
      // In reach of the new base, the beat it has is kept, at sea or in harbour.
      const g2 = world(on);
      const f2 = fleet(g2, 'UK', 'Home Fleet');
      const near = g2.regions.find((x) => x.port && x.seas.length && x.ctrl === 'UK' && x.id !== f2.base && x.seas.includes(f2.home))!;
      if (near) {
        const kept = f2.station;
        expect(rebase(g2, f2, near)).toBe(true);
        expect(f2.station).toBe(kept);
      }
    }
  });

  it('a fleet in passage turned back: on, its beat lies in reach of the harbour it turns back to', () => {
    for (const on of [false, true]) {
      const g = world(on);
      const f = fleet(g, 'UK', 'Home Fleet');
      const home = f.base;
      const r = farHarbour(g, f);
      expect(rebase(g, f, r)).toBe(true);
      expect(inPassage(f)).toBe(true);
      // Its beat set where the turn-back finds it out of reach of the harbour it left: the far harbour's home sea.
      const there = f.home;
      f.station = there;
      expect(turnBack(g, f)).toBe(true);
      expect(f.base).toBe(home);
      if (on) expect(f.station).toBe(beatOnMove(g, f, there, null));
      else expect(f.station).toBe(there);
      if (on) expect(reach(g, f)).toContain(f.station);
    }
  });

  it('a staff that moves a fleet for a job writes the job\'s beat after the move, by the same function', () => {
    const g = world(true);
    const f = fleet(g, 'UK', 'Home Fleet');
    const r = farHarbour(g, f);
    const was = f.station;
    expect(rebase(g, f, r)).toBe(true);
    // The move's own rule: the old beat out of reach, the new home sea.
    expect(f.station).toBe(f.home);
    // A job's seas in reach (a landing's cover, a quay) take it, as beatOnMove reads them with the old beat.
    const z = reach(g, f).find((x) => x !== f.home)!;
    expect(beatOnMove(g, f, was, { seas: [z], keep: true })).toBe(z);
  });
});
