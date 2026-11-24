// Height by the escort's roles in a raid with bombers (release 2, a step of Units decide; the design director's ruling of
// 24 Nov 2026 15:33Z on air-battles/dogfight/escort-height-form-1124.md). The escort's flat share goes; the close escort
// flies with the bombers and top cover a band above; the defenders sit against the bombers by their warning. Each
// role's height is as the field fights it (AIR.height.escort), and warned defenders above the bombers dive on them too
// (AIR.height.bombers). The field starts each side there and is armed without the height, so its own dive brings it
// back. Off, nothing of it is read.
import { describe, expect, it } from 'vitest';
import { planRaid, raidTargets } from '../src/campaign/airwar';
import { AIR, UD_ROLES, assessAir, forecastAir, udRole } from '../src/campaign/air';
import { techMul } from '../src/campaign/research';
import { FIGHTERS } from '../src/campaign/fighters';
import { loadGame } from '../src/campaign/save';
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
/** The staff's bombers for a raid on the named target, the nearest fighter group with them, its fighters in `parts` groups (a fixture). */
function raid(make: (on: boolean) => Game, nat: string, name: string, parts: number) {
  const g0 = make(true);
  const t = raidTargets(g0, 'bomb', nat).find((x) => x.name === name)!;
  const plan = planRaid(g0, 'bomb', t.id, undefined, nat);
  const ids0 = [...plan.picked!, ...plan.seats.filter((s) => s.kind === 'ftr' && !plan.picked!.includes(s.id)).sort((a, b) => a.km - b.km).slice(0, 1).map((s) => s.id)];
  const game = (on: boolean) => {
    const g = make(on), ids = [...ids0];
    let next = Math.max(...g.airFleets.map((f) => f.id)) + 1;
    for (const id of ids0) {
      const f = g.airFleets.find((x) => x.id === id)!;
      if (f.ftr <= 0 || parts <= 1) continue;
      const per = Math.floor(f.ftr / parts);
      for (let i = 1; i < parts; i++) {
        const c = JSON.parse(JSON.stringify(f));
        Object.assign(c, { id: next++, ftr: per, bmr: 0, est: { ...c.est, ftr: per, bmr: 0 }, name: `${f.name} ${i}`, leader: null });
        g.airFleets.push(c); ids.push(c.id);
      }
      f.ftr -= per * (parts - 1);
    }
    return { g, ids };
  };
  const m: AirMission = 'bomb';
  const assess = (on: boolean) => { const { g, ids } = game(on); return { g, a: assessAir(g, g.airFleets.filter((f) => ids.includes(f.id)), m, t.id) }; };
  return { t: t.id, m, game, on: assess(true), off: assess(false) };
}

describe('an escorted raid under Units decide: height by the escort\'s roles (release 2)', () => {
  const ger = saved('GER', 33, 11), uk = saved('UK', 29, 11);

  it('over England, warned: defenders above the close escort and the bombers, no escort share, the radar priced once', () => heightAlone(() => {
    const { on, off, t, m } = raid(ger, 'GER', 'Hampshire', 1);
    const a = on.a, o = off.a;
    expect(o.height).toBeUndefined();
    expect(a.B).toBeGreaterThan(0);
    expect(a.E).toBeGreaterThan(0);
    // One group: a close escort, at W 1 the measured cell.
    expect(a.height).toMatchObject({ W: 1, x: AIR.height.escort.close[2], h: AIR.height.h });
    // The fighters' fight: the radar left out of the defenders' weight (the warning is it), the height in.
    const radar = techMul(on.g, o.def[0].f.nat, 'radar');
    expect((a.Epow / a.Dpow) / (o.Epow / o.Dpow)).toBeCloseTo(radar * Math.pow(AIR.height.h, 2 * a.height!.x), 6);
    const fc = forecastAir(on.g, a, m, t);
    expect((fc.foeLoss - fc.foeGuns) / fc.ftrLoss).toBeCloseTo(Math.sqrt(a.Epow / a.Dpow), 6);
    // The bombers: the guns' share as it was, the fighters' share moved by a band of defenders above them.
    expect(a.height!.plain).toBeCloseTo(o.bomberLoss, 9);
    expect(a.gunLoss).toBeCloseTo(o.gunLoss, 9);
    expect(a.bomberLoss - a.gunLoss).toBeCloseTo((o.bomberLoss - o.gunLoss) * AIR.height.bombers, 9);
    const line = a.lines.find((l) => l.label === 'Height')!;
    expect(line.value).toBe('The defenders will be above your escort');
    expect(line.effect).toContain('dive on your bombers');
  }));

  it('three groups: two close, then top cover, weighed by their fighters; the field flies those roles from those bands', () => {
    expect([0, 1, 2, 3, 4].map(udRole)).toEqual(['close', 'close', 'top', 'close', 'top']);
    const { on, game, t } = raid(ger, 'GER', 'Hampshire', 3);
    const a = on.a, groups = a.att.filter((x) => x.esc > 0 && Math.round(x.f.ftr * x.crowd) > 0);
    expect(groups.length).toBe(3);
    // The escort's height is its groups' cells weighed by their fighters: the top cover's share of the weight is the third
    // group's share of the fighters (its parts are alike but for the fighters' worth, a close thing).
    const close = AIR.height.escort.close[2], top = AIR.height.escort.top[2];
    const share = (a.height!.x - close) / (top - close), third = groups[2].ftr / groups.reduce((s, x) => s + x.ftr, 0);
    expect(Math.abs(share - third)).toBeLessThan(0.05);
    const { g, ids } = game(true);
    const tk = airTicket(g, 'bomb', t, ids, 'GER').ticket!;
    expect(tk.place.roles).toEqual({ GER: [...UD_ROLES] });
    const ftr = tk.att.forces.filter((f) => f.kind === 'ftr'), bmr = tk.att.forces.filter((f) => f.kind === 'bmr');
    const carries = new Set(bmr.map((f) => f.corps));
    ftr.forEach((f, k) => expect(tk.place.starts[f.corps].band).toBe(udRole(k) === 'top' && !carries.has(f.corps) ? 2 : 1));
    for (const f of bmr) expect(tk.place.starts[f.corps].band).toBe(1);
    for (const f of tk.def.forces) expect(tk.place.starts[f.corps]).toMatchObject({ band: 2, wait: false });
  });

  it('over Holland, watched by observers (rule C): the defenders meet the bombers level; the bombers as before', () => heightAlone(() => {
    const { on, off } = raid(uk, 'UK', 'Holland', 1);
    expect(on.a.height).toMatchObject({ W: AIR.height.observers, x: AIR.height.escort.close[1] });
    expect(on.a.height!.plain).toBeUndefined();
    expect(on.a.bomberLoss).toBeCloseTo(off.a.bomberLoss, 9);
    expect(on.a.lines.find((l) => l.label === 'Height')?.value).toBe('Level');
  }));

  it('over the Channel with no chain on its coast, unwarned: the defenders come up from below and meet the close escort about level', () => heightAlone(() => {
    const make = uk, g0 = make(true);
    const sea = g0.seas.find((z) => z.name === 'English Channel') ?? g0.seas.find((z) => /Channel/.test(z.name))!;
    const plan = planRaid(g0, 'strike', sea.id, undefined, 'UK');
    const ids = [...plan.picked!, ...plan.seats.filter((s) => s.kind === 'ftr' && !plan.picked!.includes(s.id)).slice(0, 2).map((s) => s.id)];
    const a = assessAir(g0, g0.airFleets.filter((f) => ids.includes(f.id)), 'strike', sea.id);
    const g1 = make(false), o = assessAir(g1, g1.airFleets.filter((f) => ids.includes(f.id)), 'strike', sea.id);
    expect(a.B).toBeGreaterThan(0);
    expect(a.height!.W).toBe(0);
    expect(a.height!.x).toBeGreaterThanOrEqual(Math.min(AIR.height.escort.close[0], AIR.height.escort.top[0]));
    expect(a.height!.plain).toBeUndefined();
    expect(a.bomberLoss).toBeCloseTo(o.bomberLoss, 9);
    expect(a.lines.find((l) => l.label === 'Height')?.effect).toContain('Unwarned');
  }));

  it('the field lands near the card: the fighter exchange and the bombers lost, warned and unwarned', () => {
    for (const c of [{ make: ger, nat: 'GER', name: 'Hampshire', parts: 3 }, { make: uk, nat: 'UK', name: 'Holland', parts: 3 }]) {
      const { on, game, t, m } = raid(c.make, c.nat, c.name, c.parts);
      const fc = forecastAir(on.g, on.a, m, t);
      const { g, ids } = game(true);
      const tk = airTicket(g, 'bomb', t, ids, c.nat).ticket!;
      let A = 0, D = 0, B = 0;
      for (let s = 1; s <= 20; s++) {
        const k = JSON.parse(JSON.stringify(tk));
        k.seed = s * 7919;
        const sim = new AirSim(k, {});
        while (!sim.over) sim.step();
        for (const r of sim.outcome().groups) { if (r.side === 'att') { A += r.lostFtr; B += r.lostBmr; } else D += r.lostFtr; }
      }
      // Within a third of each other (measured, 32 seeds: exchange 0.57 v 0.53 and 0.68 v 0.74; bombers 72 v 88 and 23 v 21).
      expect(Math.abs(Math.log((D / A) / (fc.foeLoss / fc.ftrLoss)))).toBeLessThan(Math.log(1.35));
      expect(Math.abs(Math.log(B / 20 / fc.bmrLoss))).toBeLessThan(Math.log(1.35));
    }
  });
});
