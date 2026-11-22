// The battle's one scale (the true-scale plan, GR1 and GR3; Brian's standing rule of 22 November 04:14Z: "on-screen
// numbers reflect the actual units each side brings ... as many as the Mac draws smoothly"). One N a battle: every man
// both sides bring over the figure budget, rounded up to a multiple of 5. No block draws more men than its men over N:
// as platoons where they fit, else as sections, else its key men only. Looks only (ruling 217): nothing in the fight
// reads it, so a battle ends exactly the same with it on or off.
import { describe, expect, it } from 'vitest';
import { LAND, LandBattle, menDrawn, platoonOf, sampleTicket, sectionOf, withTeams, type LandUnit } from '../src/battle/land';
import type { FieldEvent as CoreEvent } from '../src/battle/core';

const FOOT = new Set(['officer', 'rifle', 'mg', 'at', 'sapper', 'holder', 'fo', 'leader', 'loader', 'crew']);
const ALL = { sections: true, platoons: true };

/** Normandy's armies on a practice field, with a machine-gun and a mortar team a side. */
const battle = (seed: number) => new LandBattle(withTeams(sampleTicket('plains', 1.3, { seed, nat: { att: 'USA', def: 'GER' } })));
const sig = (b: LandBattle, ev: CoreEvent[]): string => JSON.stringify({
  over: b.over, t: b.t,
  units: b.units.map((u: LandUnit) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged]),
  wrecks: b.wrecks.map((z) => [z.x, z.y, z.at]),
  said: ev.filter((e) => e.what !== 'shell').map((e) => [e.t, e.what, e.text]),
});
const fight = (b: LandBattle) => {
  const ev: CoreEvent[] = [];
  for (let k = 0; !b.over && k < 30000; k++) { b.step(); ev.push(...b.view().events); }
  return ev;
};
/** A block's men as the 3D game shows them (menDrawn's count): foot men, and three with each support-team weapon. */
const drawn = (blk: { men: { r: string }[] }) => blk.men.reduce((s, m) => s + (FOOT.has(m.r) ? 1 : m.r === 'hmg' || m.r === 'mortar' ? 3 : 0), 0);

describe('one N a battle (GR1)', () => {
  it('Sedan, Normandy, Prokhorovka and Kursk give 20, 20, 35 and 125; a battle that fits the budget whole, 1', () => {
    const b = battle(1);
    const N = (total: number): number => {
      for (const u of b.units) (u as { men0: number }).men0 = total / b.units.length;
      return b.scale();
    };
    expect(N(120_900 + 46_800)).toBe(20);
    expect(N(84_420 + 78_120)).toBe(20);
    expect(N(172_620 + 109_620)).toBe(35);
    expect(N(481_800 + 571_800)).toBe(125);
    expect(N(LAND.scale.budget)).toBe(1);
    expect(N(LAND.scale.budget + 10)).toBe(5);
  });

  it('counted as asked (a fogged field: what the player can know), a figure that is no number counting none', () => {
    const b = battle(1);
    expect(b.scale(() => 20_000)).toBe(Math.ceil((20_000 * b.units.length) / LAND.scale.budget / 5) * 5);
    const n = b.scale();
    (b.units[0] as { men0: number }).men0 = Number.NaN;
    expect(b.scale()).toBeGreaterThan(0);
    expect(b.scale()).toBeLessThanOrEqual(n);
  });

  it('the drill is given it only when asked; asked again, the drill already made stands', () => {
    const off = battle(1), on = battle(1), set = battle(1);
    expect(off.drillOn(ALL).scale).toBeNull();
    expect(on.drillOn({ ...ALL, scale: true }).scale).toBe(on.scale());
    expect(on.drillOn(ALL).scale).toBe(on.scale());
    expect(set.drillOn({ ...ALL, scale: 125 }).scale).toBe(125);
  });
});

describe('no block draws more men than its men over N (GR3)', () => {
  it('a platoon\'s men, a section\'s and the key men\'s, as the drill draws them', () => {
    for (const nat of ['USA', 'GER', 'UK', 'SOV', 'JAP', 'ITA', 'FRA']) {
      const P = platoonOf(nat), sec = sectionOf(nat, 'rifle').length;
      expect(menDrawn(nat, ['rifle'], 2)).toBe(P.squads * (1 + sec) + P.hq);
      expect(menDrawn(nat, ['rifle'], 1)).toBe(1 + sec);
      expect(menDrawn(nat, ['officer', 'mg'], 2)).toBe(menDrawn(nat, ['officer', 'mg'], 1));
    }
    // As the 3D game shows them: a gun is no man (her crew is), a support team's weapon comes with three.
    expect(menDrawn('GER', ['gun'], 1)).toBe(7);
    expect(menDrawn('GER', ['hmg'], 1)).toBe(7);
    expect(menDrawn('GER', ['mortar'], 1)).toBe(8);
    expect(menDrawn('GER', ['tank'], 2)).toBe(0);
    expect(menDrawn('GER', ['rifle'], 2)).toBe(37);
    expect(menDrawn('USA', ['rifle'], 2)).toBe(41);
  });

  it('a battle the size of Normandy draws exactly what it drew before', () => {
    const a = battle(11), b = battle(11);
    expect(b.scale()).toBeLessThanOrEqual(25);
    const da = a.drillOn(ALL), db = b.drillOn({ ...ALL, scale: true });
    expect(JSON.stringify(db.view())).toBe(JSON.stringify(da.view()));
  });

  it('a battle the size of Kursk: each block fills its cap, its key men always and never more than its whole draw', () => {
    for (const N of [25, 45, 60, 70, 125, 400, 600]) {
      const b = battle(3), d = b.drillOn({ ...ALL, scale: N });
      const whole = battle(3).drillOn(ALL), keys = battle(3).drillOn({});
      for (const blk of d.view()) {
        const u = b.unit(blk.id)!, cap = Math.floor(u.men0 / N), n = drawn(blk);
        const at = (x: typeof d) => drawn(x.view().find((v) => v.id === blk.id)!);
        const [k0, all] = [at(keys), at(whole)];
        if (u.kind === 'arm') { expect(n).toBe(0); continue; }
        // All of it where it fits; its key men where they alone are more; else its cap to the man.
        expect(n, `block ${u.id} (${u.nat} ${u.kind}${u.team ?? ''}) at 1:${N}`).toBe(all <= cap ? all : k0 >= cap ? k0 : cap);
      }
    }
  });

  it('the same N on both sides: every block with more men than its cap draws its men over N, within 5%, both armies', () => {
    for (const [att, def] of [['USA', 'GER'], ['SOV', 'GER'], ['UK', 'ITA']]) {
      for (const N of [45, 60, 125, 200]) {
        const b = new LandBattle(withTeams(sampleTicket('plains', 1.3, { seed: 4, nat: { att, def } })));
        const d = b.drillOn({ ...ALL, scale: N }), whole = new LandBattle(b.ticket).drillOn(ALL);
        const keys = new LandBattle(b.ticket).drillOn({});
        let checked = 0;
        for (const blk of d.view()) {
          const u = b.unit(blk.id)!, cap = u.men0 / N;
          const at = (x: typeof d) => drawn(x.view().find((v) => v.id === blk.id)!);
          if (u.kind === 'arm' || at(whole) <= cap || at(keys) >= cap) continue;
          expect(Math.abs(drawn(blk) / cap - 1), `${u.nat} block ${u.id} at 1:${N}`).toBeLessThanOrEqual(0.05);
          checked++;
        }
        if (N >= 60) expect(checked).toBeGreaterThan(0);
      }
    }
  });

  it('a block drawn short keeps its sections whole in squad order, the last one short, and its leaders by their men', () => {
    const b = battle(3), d = b.drillOn({ ...ALL, scale: 125 });
    for (const blk of d.view()) {
      const u = b.unit(blk.id)!;
      if (u.kind !== 'inf') continue;
      const per = new Map<number, number>();
      for (const m of blk.men) if (m.j !== undefined) per.set(m.k, (per.get(m.k) ?? 0) + 1);
      const ks = [...per.keys()].sort((a, c) => a - c);
      // Sections go to the key men in order: no gap before the last one drawn.
      ks.forEach((k, i) => expect(k).toBe(i));
      // A rifle key man shows as a leader only with men of his own.
      for (const m of blk.men) if (m.j === undefined && m.r === 'leader') expect(per.get(m.k) ?? 0).toBeGreaterThan(0);
    }
  });
});

describe('looks only (ruling 217)', () => {
  it('a battle ends exactly the same with its men drawn at Kursk\'s scale, drawn whole, or not drawn', () => {
    for (const seed of [2, 7]) {
      const off = battle(seed), whole = battle(seed), scaled = battle(seed);
      whole.drillOn(ALL);
      scaled.drillOn({ ...ALL, scale: 125 });
      const a = sig(off, fight(off));
      expect(sig(whole, fight(whole))).toBe(a);
      expect(sig(scaled, fight(scaled))).toBe(a);
    }
  }, 600_000);
});
