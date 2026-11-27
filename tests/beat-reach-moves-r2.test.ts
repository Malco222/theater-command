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
import { NAVY, beatOnMove, fleetReach, inPassage, navalUpkeep, rebase, setBeatReach, tieLapses, tiedNow, turnBack } from '../src/campaign/navy';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { bookFleets } from '../src/campaign/stations';
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
      expect(near).toBeTruthy();
      const kept = f2.station;
      expect(rebase(g2, f2, near)).toBe(true);
      expect(f2.station).toBe(kept);
    }
  });

  it("a beat in the new base's reach that is not its home sea is kept, on and off (the rule's first half)", () => {
    for (const on of [false, true]) {
      const g = world(on);
      const f = fleet(g, 'UK', 'Home Fleet');
      const to = g.byName['Northumbria'];
      // A sea in reach of both harbours and not Northumbria's home sea (the North Sea).
      const before = reach(g, f);
      f.base = to.id;
      const after = reach(g, f);
      f.base = g.byName['Highlands'].id;
      const beat = after.find((z) => before.includes(z) && z !== to.seas[0] && !to.seas.includes(z))!;
      expect(beat).toBeGreaterThanOrEqual(0);
      f.station = beat;
      expect(rebase(g, f, to)).toBe(true);
      expect(f.home).toBe(to.seas[0]);
      expect(f.home).not.toBe(beat);
      expect(f.station).toBe(beat);
    }
  });

  it('a fleet in passage turned back keeps a beat the harbour it turns back to reaches, though not its home sea', () => {
    for (const on of [false, true]) {
      const g = world(on);
      const f = fleet(g, 'UK', 'Home Fleet');
      const home = f.base, homeSea = f.home, reach0 = reach(g, f);
      const r = farHarbour(g, f);
      expect(rebase(g, f, r)).toBe(true);
      expect(inPassage(f)).toBe(true);
      const beat = reach0.find((z) => z !== homeSea && !g.regions[home].seas.includes(z))!;
      f.station = beat;
      expect(turnBack(g, f)).toBe(true);
      expect(f.base).toBe(home);
      expect(f.home).not.toBe(beat);
      expect(f.station).toBe(beat);
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

  it("a job's beat written after the move (as the cover move and standing forward do) comes from the same function", () => {
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

  it('a tied fleet moved where its beat is out of reach: on, the tie lapses with the beat (rider b598(2)); off, the tie lapses as it always did, its beat out of reach', () => {
    for (const on of [false, true]) {
      const g = world(on);
      const f = fleet(g, 'UK', 'Dover Fleet');
      expect(f.tied).toBe(true);
      const was = f.station, to = g.byName['Alexandria'];
      expect(rebase(g, f, to)).toBe(true);
      if (on) {
        expect(f.tied).toBe(false);
        expect(f.station).toBe(f.home);
      } else {
        expect(f.tied).toBe(true);
        expect(f.station).toBe(was);
        expect(tieLapses(g, f)).toBe(true);
      }
      expect(tiedNow(g, f)).toBe(false);
    }
  });

  it('a tied fleet moved where its beat stays in reach keeps its tie and its beat, on and off', () => {
    for (const on of [false, true]) {
      const g = world(on);
      const f = fleet(g, 'UK', 'Dover Fleet');
      const was = f.station, to = g.byName['West Country'];
      // (Its beat, the English Channel, in reach of West Country and not its home sea there, the Irish Sea.)
      expect(to.seas[0]).not.toBe(was);
      expect(rebase(g, f, to)).toBe(true);
      expect(f.home).not.toBe(was);
      expect(f.station).toBe(was);
      expect(f.tied).toBe(true);
      expect(tiedNow(g, f)).toBe(true);
    }
  });

  it("a fleet of the book's coming home after its months: on, the beat it had there in that base's reach; off, the beat it had", () => {
    for (const on of [false, true]) {
      const g = startOpen(createGame(3, null));
      setStop(g, 1, true);
      if (on) setBeatReach(g, true);
      const f3 = fleet(g, 'JAP', '3rd Fleet');
      const home = f3.base;
      for (const nm of ['Balikpapan', 'Cam Ranh']) g.byName[nm].ctrl = 'JAP';
      g.flags['st:done:JAP:south'] = 28;
      for (const e of ['USA', 'UK']) declareWar(g, 'JAP', e);
      g.turn = 30;
      bookFleets(g, 'JAP');
      expect(f3.bookBase).toBeTruthy();
      // The beat it had at home, written there before it left, now a sea out of that harbour's reach.
      const away = f3.base;
      f3.base = home;
      const out = g.seas.findIndex((z, i) => z.adj.length > 0 && !reach(g, f3).includes(i));
      f3.base = away;
      f3.bookBase![3] = out;
      g.turn = 32;
      bookFleets(g, 'JAP');
      expect([f3.base, f3.bookBase]).toEqual([home, undefined]);
      expect(f3.station).toBe(on ? f3.home : out);
    }
  });

  it("a raider's beat is read by its own longer reach: a hunting ground its boats reach from the new base is kept", () => {
    const g = world(true);
    const f = fleet(g, 'GER', 'U-boat Fleet');
    expect(f.duty).toBe('raid');
    f.reach = NAVY.range + 6;
    const beat = f.station;
    // A harbour of Germany's from which the hunting ground lies beyond a surface fleet's reach but within the boats'.
    const keep = [f.base, f.port, f.home, f.zone, f.stance] as const;
    let to: Region | undefined;
    for (const r of g.regions) {
      if (!r.port || !r.seas.length || r.ctrl !== 'GER' || r.id === f.base) continue;
      f.base = r.id;
      const plain = fleetReach(g, f, true).includes(beat), boats = fleetReach(g, f, true, true).includes(beat);
      [f.base, f.port, f.home, f.zone, f.stance] = keep;
      if (!plain && boats) { to = r; break; }
    }
    expect(to).toBeTruthy();
    expect(rebase(g, f, to!, true)).toBe(true);
    expect(f.station).toBe(beat);
  });
});
