// Height and warning in place of the attacker's flat edge in a fight of fighters alone (release 2, a step of Units
// decide; the design director's rulings of 24 Nov 2026 02:28Z and 15:20Z on air-battles/dogfight/sweep-height-form-1124.md).
// The sweep flies high; defenders warned in time climb above it, half-warned meet it level, unwarned are bounced. The
// edge a band (AIR.height.h) is the field's own bounce, measured; the field starts each side where the warning puts it
// and is armed without the height, so its bounce brings it back. Off, nothing of it is read.
import { describe, expect, it } from 'vitest';
import { planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { AIR, assessAir, forecastAir } from '../src/campaign/air';
import { techMul } from '../src/campaign/research';
import { FIGHTERS } from '../src/campaign/fighters';
import { loadGame } from '../src/campaign/save';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { AirMission, Game } from '../src/campaign/types';
import { AirSim } from '../src/battle/air/sim';
import { airTicket } from '../src/battle/air/ticket';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;
/**
 * Units decide flies each nation's fighter types (the dogfight switch folds into it), so the off game flies them too.
 * The tests that pin the height alone set k to 0: then the types give no edge on either side but the research lead
 * taken off on both, and only the height differs.
 */
const heightAlone = <T>(run: () => T): T => { const was = FIGHTERS.law.k; FIGHTERS.law.k = 0; try { return run(); } finally { FIGHTERS.law.k = was; } };

/** A grand save of `nat` from `from`, as a fresh copy each call, with Units decide on or off (a fixture: set directly). */
function saved(nat: string, from: number, seed: number): (on: boolean) => Game {
  TC.start({ view: 'grand', human: nat, from, seed });
  const text = TC.save().text;
  return (on) => {
    const g = loadGame(text)!;
    if (on) { g.flags.open = 1; g.flags.unitsDecide = 1; } else { delete g.flags.unitsDecide; g.flags.ftrtypes = 1; }
    // Honours are the player's own edge (storyEdge), which Units decide leaves out of a group's worth: cleared, so the
    // two weigh the groups alike and only the height differs.
    for (const f of g.airFleets) delete f.hon;
    return g;
  };
}
/** A raid of `kind` by `nat` on the named target, assessed with Units decide on and off. */
function raid(make: (on: boolean) => Game, nat: string, kind: RaidKind, name: string) {
  const g = make(true);
  const t = raidTargets(g, kind, nat).find((x) => x.name === name)!;
  const ids = planRaid(g, kind, t.id, undefined, nat).picked!;
  const pick = (h: Game) => h.airFleets.filter((f) => ids.includes(f.id));
  const m: AirMission = kind === 'superiority' ? 'superiority' : 'bomb';
  const on = make(true), off = make(false);
  return { t: t.id, ids, on: assessAir(on, pick(on), m, t.id), off: assessAir(off, pick(off), m, t.id), g: on, m };
}

describe('a sweep under Units decide: height follows warning (release 2)', () => {
  const ger = saved('GER', 33, 11), uk = saved('UK', 29, 11);

  it('over England with its radar chain the defenders are above: x -1, no escort share, and the radar priced once', () => heightAlone(() => {
    const { on, off, g, t, m } = raid(ger, 'GER', 'superiority', 'Hampshire');
    expect(off.height).toBeUndefined();
    expect(on.height).toEqual({ W: 1, x: -1, h: AIR.height.h });
    // The ratio of the powers: off it carried the defenders' radar; on it carries the height instead.
    const radar = off.def.reduce((s, x) => s + x.f.ftr * x.w, 0) > 0 ? techMul(g, off.def[0].f.nat, 'radar') : 1;
    expect((on.Epow / on.Dpow) / (off.Epow / off.Dpow)).toBeCloseTo((radar * Math.pow(AIR.height.h, -2)), 6);
    // The exchange is the square root of the power ratio, with no flat edge for the attacker.
    const fc = forecastAir(g, on, m, t);
    expect(fc.foeLoss / fc.ftrLoss).toBeCloseTo(Math.sqrt(on.Epow / on.Dpow), 6);
    expect(on.lines.find((l) => l.label === 'Height')?.value).toBe('The defenders will be above you');
  }));

  it('over ground their side holds, without a chain, observers meet the sweep level (rule C): Holland 1942, Alsace 1939', () => {
    // (The design director 24 Nov 16:09Z, rule C: observers count on ground held by anyone on the defenders' side, so
    // occupied Holland is watched by the German posts there. Defenders rise only over their side's ground, so a sweep
    // over land is never unwarned without an ambush; at sea, with no chain on the coast, it is: see the escort file.)
    const nl = raid(uk, 'UK', 'superiority', 'Holland');
    expect(nl.on.height).toMatchObject({ W: AIR.height.observers, x: 0 });
    expect(nl.on.lines.find((l) => l.label === 'Height')?.value).toBe('Level');
    // September 1939, an open start: France has no chain yet, so over its own ground its observers meet a German sweep
    // level (Alsace). A fight for the sky with bombers in it is an escort's (air-escort-height-r2.test.ts): unescorted,
    // as Lorraine's pick goes, there is no fighters' fight and no height.
    const g = startOpen(createGame(1, null));
    g.flags.unitsDecide = 1;
    const sweepOf = (name: string) => {
      const t = raidTargets(g, 'superiority', 'GER').find((x) => x.name === name)!;
      const ids = planRaid(g, 'superiority', t.id, undefined, 'GER').picked ?? [];
      return assessAir(g, g.airFleets.filter((f) => ids.includes(f.id)), 'superiority', t.id);
    };
    const al = sweepOf('Alsace');
    expect(al.B).toBe(0);
    expect(al.height).toMatchObject({ W: AIR.height.observers, x: 0 });
    expect(al.lines.find((l) => l.label === 'Height')?.value).toBe('Level');
    const lo = sweepOf('Lorraine');
    if (lo.B > 0 && lo.E <= 0) expect(lo.height).toBeUndefined();
  });

  it('the field starts each side where the warning puts it, armed without the height, and lands near the card', () => {
    // Kent: No. 10 Group's fields lie between the raid and the target on the board. Warned, it starts as far from the
    // raid's way in as the farthest group, and meets the sweep with the rest, not alone (it was 2.5 to the card's 0.35).
    const cases = [
      { make: ger, nat: 'GER', name: 'Hampshire', att: 1, def: 2 },
      { make: ger, nat: 'GER', name: 'Kent', att: 1, def: 2 },
      { make: uk, nat: 'UK', name: 'Holland', att: 1, def: 1 },
    ];
    for (const c of cases) {
      const { on, g, t, ids, m } = raid(c.make, c.nat, 'superiority', c.name);
      const tk = airTicket(c.make(true), 'superiority', t, ids, c.nat).ticket!;
      for (const f of tk.att.forces) expect(tk.place.starts[f.corps]).toMatchObject({ band: c.att, wait: false });
      for (const f of tk.def.forces) expect(tk.place.starts[f.corps]).toMatchObject({ band: c.def, wait: false });
      // Level with each other from the raid's way in (the board's edge may bring one a little nearer).
      const way = tk.att.forces.map((f) => tk.place.starts[f.corps]), ex = way.reduce((s, p) => s + p.x, 0) / way.length, ey = way.reduce((s, p) => s + p.y, 0) / way.length;
      const r = tk.def.forces.map((f) => Math.hypot(tk.place.starts[f.corps].x - ex, tk.place.starts[f.corps].y - ey));
      for (const x of r) expect(x).toBeGreaterThan(0.9 * Math.max(...r));
      const fc = forecastAir(g, on, m, t);
      let A = 0, D = 0;
      for (let s = 1; s <= 20; s++) {
        const k = JSON.parse(JSON.stringify(tk));
        k.seed = s * 7919;
        const sim = new AirSim(k, {});
        while (!sim.over) sim.step();
        for (const r of sim.outcome().groups) { if (r.side === 'att') A += r.lostFtr; else D += r.lostFtr; }
      }
      // The card's exchange, and the field's from its own bounce: within a third of each other.
      const card = fc.foeLoss / fc.ftrLoss, field = D / A;
      expect(Math.abs(Math.log(field / card))).toBeLessThan(Math.log(1.35));
    }
  });
});
