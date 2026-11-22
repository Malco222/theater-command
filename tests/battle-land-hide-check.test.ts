// The historian's check of the ambush rules and the radio (22 November 05:20Z; the Normandy hedgerows, 1944: "they
// could not see the Germans, but the Germans could see them"). Hedge lines hide as woods do; a block found by its fire is
// not yet located; a moving tank in cover is heard; in true forest fire is held closer; an ambush shakes men moving in
// the open worst and its mortars are pre-sighted; reconnaissance by fire flushes hidden blocks out; and an order's wait
// by radio goes by each army's sets.
import { describe, expect, it } from 'vitest';
import { battleCalls, unitOut } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { LAND, LandBattle, sampleTicket, withTeams } from '../src/battle/land';
import { thinkFor } from '../src/battle/land/ai';
import { coverAt } from '../src/battle/land/field';
import type { LandUnit } from '../src/battle/land/units';
import type { Man } from '../src/battle/land/drill';
import { regimentWord } from '../src/battle/land/units';
import type { Side } from '../src/battle/core/types';

const run = (b: LandBattle, secs: number, each?: () => void): void => { const end = b.t + secs; for (let k = 0; !b.over && b.t < end - 1e-9 && k < 100000; k++) { b.step(); each?.(); } };
const foot = (b: LandBattle, side: Side): LandUnit[] => b.units.filter((u) => u.side === side && (u.kind === 'inf' || u.kind === 'mtn') && u.state !== 'gone');
const woods = (b: LandBattle, x: number, y: number): boolean => coverAt(b.field, x, y) === 'woods';
type Blow = { dmg: number; nerve: number };
const blowOf = (b: LandBattle, u: LandUnit, t: LandUnit): Blow => (b as unknown as { blow: (u: LandUnit, t: LandUnit) => Blow }).blow(u, t);
/** Stands 'a' straight in front of 'd' (toward the attack) with this many metres between their edges. */
function inFront(b: LandBattle, d: LandUnit, a: LandUnit, gap: number): void {
  a.x = d.x; a.face = 0;
  for (let off = 1200; off > 0; off -= 1) { a.y = d.y - off; if (b.gap(a, d) <= gap) break; }
  a.home = [a.x, a.y];
}
/** A plains field with no woods near the defence's first foot block, and a hedge laid along its middle, east to west. */
function hedged(seed = 5): { b: LandBattle; d: LandUnit; a: LandUnit } {
  for (const s of [seed, 1, 2, 3, 4, 6, 7, 8]) {
    const b = new LandBattle(withTeams(sampleTicket('plains', 1.3, { seed: s, humanAtt: true })));
    b.ai.att = b.ai.def = false;
    const d = foot(b, 'def')[0], a = foot(b, 'att')[0];
    if (woods(b, d.x, d.y)) continue;
    expect(b.hedgesOn([{ kind: 'hedgerow', points: [[d.x - 300, d.y], [d.x + 300, d.y]] }], [{ x: d.x + 150, y: d.y, kind: 'gate', width: 3.2 }])).toBe(true);
    inFront(b, d, a, 400);
    if (woods(b, a.x, a.y) || b.onHedge(a.x, a.y)) continue;
    return { b, d, a };
  }
  throw new Error('no open ground');
}

describe('hedge lines hide as woods do', () => {
  it('a block standing on a hedge line is hidden; one off it is not; the lines are given before the clock starts only', () => {
    const { b, d, a } = hedged();
    expect(b.onHedge(d.x, d.y)).toBe(true);
    expect(b.onHedge(d.x, d.y + LAND.hide.hedge.near + 15)).toBe(false);
    b.hideOn();
    expect(d.hid).toBe(true);
    expect(a.hid).toBeUndefined();
    expect(unitOut(d).hid).toBe(1);
    expect(b.gates.length).toBe(1);
    b.step();
    expect(b.hedgesOn([])).toBe(false);
    expect(b.hedges.length).toBe(1);
  });

  it('on a hedge line foot holds its fire to the hedgerow distance, not the forest\'s; lines off the field are dropped', () => {
    const { b, d, a } = hedged();
    b.hideOn();
    inFront(b, d, a, LAND.hide.ambush.foot - 20);
    expect(LAND.hide.ambush.foot - 20).toBeGreaterThan(LAND.hide.forest);
    expect(b.targetOf(d)?.id).toBe(a.id);
    const c = new LandBattle(withTeams(sampleTicket('plains', 1.3, { seed: 5 })));
    expect(c.hedgesOn([{ kind: 'wall', points: [[-500, -500], [-400, -400]] }, { kind: 'hedgerow', points: [[10, 10]] }])).toBe(true);
    expect(c.hedges.length).toBe(0);
    expect(c.onHedge(10, 10)).toBe(false);
  });

  it('the computer\'s defence lays its ambush on a hedge line where there are no woods', () => {
    let laid = 0;
    for (const seed of [1, 2, 3, 4, 5]) {
      const b = new LandBattle(withTeams(sampleTicket('plains', 1.3, { seed })));
      b.ai.att = b.ai.def = true;
      // Along the front of its ground, where its line stands.
      const y = b.field.zones.def[1] + 25;
      b.hedgesOn([{ kind: 'hedgerow', points: [[50, y], [b.field.w - 50, y]] }]);
      b.hideOn();
      b.step();
      laid += b.units.filter((u) => u.side === 'def' && u.hid && b.onHedge(u.x, u.y) && !woods(b, u.x, u.y)).length;
    }
    expect(laid).toBeGreaterThan(0);
  });
});

describe('an ambush, as the historian found it', () => {
  it('found by its fire, it is not yet located: the fire back is half aimed till 25 s pass or an enemy is near', () => {
    const twin = () => { const h = hedged(); h.b.hideOn(); inFront(h.b, h.d, h.a, 80); h.b.step(); return h; };
    const x = twin(), y = twin();
    expect(x.d.hid).toBeUndefined();
    expect(x.d.unloc).toBeCloseTo(x.b.t - 0.1 + LAND.hide.locate.secs, 0);
    expect(unitOut(x.d).vague).toBe(1);
    delete y.d.unloc;
    expect(blowOf(x.b, x.a, x.d).dmg / blowOf(y.b, y.a, y.d).dmg).toBeCloseTo(LAND.hide.locate.fire, 6);
    run(x.b, LAND.hide.locate.secs + 1);
    expect(x.d.unloc).toBeUndefined();
    // An enemy within its sight locates it at once.
    const z = twin();
    inFront(z.b, z.d, z.a, LAND.hide.sight.foot - 10);
    z.b.step();
    expect(z.d.unloc).toBeUndefined();
  });

  it('its first fire shakes men moving in the open worst, and its mortars strike what it fires on harder', () => {
    const { b, d, a } = hedged();
    b.hideOn();
    inFront(b, d, a, 80);
    b.step();
    expect(d.sprung).toBeDefined();
    a.speed = 0;
    const still = blowOf(b, d, a).nerve;
    a.speed = 2;
    expect(blowOf(b, d, a).nerve / still).toBeCloseTo(LAND.hide.surprise.open / LAND.hide.surprise.nerve, 6);
    // Twin fields: the defence's mortar team on the ambushed block, with the ambush firing on it and without.
    const twin = () => { const h = hedged(); h.b.hideOn(); inFront(h.b, h.d, h.a, 80); h.b.step(); return { ...h, m: h.b.units.find((u) => u.side === 'def' && u.team === 'mor')! }; };
    const p = twin(), q = twin();
    expect(p.m).toBeDefined();
    p.d.engaged = p.a.id; q.d.engaged = undefined;
    expect(blowOf(p.b, p.m, p.a).dmg / blowOf(q.b, q.m, q.a).dmg).toBeCloseTo(LAND.hide.surprise.mortar, 6);
  });

  it('a tank on the move in cover is heard well before it is seen; standing still it is not', () => {
    for (const moving of [true, false]) {
      const b = new LandBattle(withTeams(sampleTicket('plains', 1.3, { seed: 5, humanAtt: true })));
      b.ai.att = b.ai.def = false;
      // A tank of the attack on a hedge in mid-field, one enemy block 250 m off, nobody else near.
      const tk = b.units.find((u) => u.side === 'att' && u.kind === 'arm')!, enemy = foot(b, 'def')[0];
      [tk.x, tk.y] = [b.field.w / 2, b.field.zones.def[1] - 560];
      tk.home = [tk.x, tk.y];
      expect(b.hedgesOn([{ kind: 'hedgerow', points: [[tk.x - 200, tk.y], [tk.x + 200, tk.y]] }])).toBe(true);
      b.hideOn();
      expect(tk.hid).toBe(true);
      enemy.x = tk.x; enemy.y = tk.y + tk.d / 2 + enemy.d / 2 + 250;
      expect(b.units.filter((u) => u.side === 'def' && u !== enemy).every((u) => b.gap(u, tk) > LAND.hide.heard.tracks)).toBe(true);
      expect(b.gap(tk, enemy)).toBeGreaterThan(LAND.hide.sight.tracks);
      expect(b.gap(tk, enemy)).toBeLessThan(LAND.hide.heard.tracks);
      b.ai.att = b.ai.def = false;
      tk.speed = moving ? 3 : 0;
      (b as unknown as { spotHidden: () => void }).spotHidden();
      if (moving) {
        expect(tk.hid).toBeUndefined();
        expect((b.log.find((e) => e.what === 'land:found' && e.unit === tk.id)!.extra as { how: string }).how).toBe('heard');
      } else expect(tk.hid).toBe(true);
    }
  });
});

describe('reconnaissance by fire', () => {
  it('his blocks told to fire on a hedge strike what hides there blind, at half, and flush it out', () => {
    const { b, d, a } = hedged();
    b.hideOn();
    // Within its rifles' reach, beyond the hedgerow's hold-fire distance.
    inFront(b, d, a, 170);
    expect(170).toBeGreaterThan(LAND.hide.ambush.foot);
    expect(b.command({ do: 'land:fire', units: [a.id], x: d.x + 20, y: d.y }, 'att')).toBe(true);
    expect(b.command({ do: 'land:fire', units: [a.id], x: NaN, y: d.y }, 'att')).toBe(false);
    expect(a.search).toEqual([d.x + 20, d.y]);
    expect(unitOut(a).search).toEqual([Math.round(d.x + 20), Math.round(d.y)]);
    expect(b.targetOf(a)?.id).toBe(d.id);
    const str = d.str;
    let blind = 0;
    run(b, 120, () => { if (d.hid && a.engaged === d.id) blind++; });
    expect(blind).toBeGreaterThan(0);
    expect(d.str).toBeLessThan(str);
    expect(d.hid).toBeUndefined();
    expect((b.log.find((e) => e.what === 'land:found' && e.unit === d.id)!.extra as { how: string }).how).toBe('searched');
    // Any other order lifts it.
    b.command({ do: 'hold', units: [a.id] }, 'att');
    expect(a.search).toBeUndefined();
  });

  it('the computer\'s attack fires on the cover ahead once it has been ambushed, and not before', () => {
    const { b, a, d } = hedged();
    b.hideOn();
    thinkFor(b, 'att');
    expect(b.units.some((u) => u.side === 'att' && u.search)).toBe(false);
    b.ambushed.add('att');
    for (const u of b.units) if (u.side === 'att') u.engaged = undefined;
    inFront(b, d, a, 170); a.order = null;
    thinkFor(b, 'att');
    // It sweeps the cover ahead in its lane: the hedge, and the block hidden on it.
    expect(a.search).toBeDefined();
    expect(a.searchR).toBeGreaterThan(LAND.hide.search.reach);
    expect(d.hid).toBe(true);
    expect(b.targetOf(a)?.id).toBe(d.id);
  });

  it('off (no hiding), a fire order on ground holds the block and strikes nobody hidden', () => {
    const b = new LandBattle(withTeams(sampleTicket('plains', 1.3, { seed: 5, humanAtt: true })));
    b.ai.att = b.ai.def = false;
    const a = foot(b, 'att')[0];
    expect(b.command({ do: 'land:fire', units: [a.id], x: a.x, y: a.y + 300 }, 'att')).toBe(true);
    expect(b.targetOf(a)).toBeNull();
  });
});

describe('orders by radio, by each army\'s sets', () => {
  const at = (nat: string, turn: number) => {
    const t = withTeams(sampleTicket('plains', 1.3, { seed: 5, nat: { att: nat, def: 'GER' }, humanAtt: true }));
    t.turn = turn;
    return new LandBattle(t);
  };
  it('Soviet foot 1941-42 twice as long, Japanese twice, Italian 1.5, German foot 1.25; tanks as their sets', () => {
    const f = (b: LandBattle, kind = 'inf') => b.radioOf(b.units.find((u) => u.side === 'att' && u.kind === kind)!);
    expect(f(at('SOV', 25))).toBe(2);
    expect(f(at('SOV', 58))).toBe(1.25);
    expect(f(at('JAP', 40))).toBe(2);
    expect(f(at('ITA', 30))).toBe(1.5);
    expect(f(at('GER', 30))).toBe(1.25);
    expect(f(at('USA', 58))).toBe(1);
    expect(f(at('UK', 58))).toBe(1);
    // A German tank has a set in every tank; two T-34s in five had one to 1943.
    expect(f(at('GER', 58), 'arm')).toBe(1);
    expect(f(at('SOV', 34), 'arm')).toBeCloseTo(0.4 + 0.6 * 2, 6);
    expect(LAND.radio.cut).toBeGreaterThanOrEqual(3);
  });

  it('a Soviet block\'s order in 1941 lands twice the wait later', () => {
    const b = at('SOV', 25);
    b.ai.att = false; b.radio = true;
    run(b, 2);
    const u = foot(b, 'att')[0];
    expect(b.command({ do: 'move', units: [u.id], x: u.x, y: u.y + 200 }, 'att')).toBe(true);
    expect(u.orderAt).toBeCloseTo(b.t + LAND.radio.delay * 2, 6);
  });
});

describe('the 3D game hands the hedges over', () => {
  it('battleHedges lays them before the clock starts, and a block on one shows hid: 1', async () => {
    const { newCampaign, offers } = await import('../src/battle/ui/campaign');
    const { orderAttack } = await import('../src/campaign/turn');
    const g = newCampaign();
    const TC = battleCalls({ game: () => g, attack: (ids, to, f) => ({ report: orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f) }) });
    const o = offers(g)[0];
    const st = TC.battleStart(o.corps.map((c) => c.id), o.target.id, { hide: true, setup: true })!;
    type U = { id: number; side: string; kind: string; x: number; y: number; hid?: number };
    const mine = (st.view as { units: U[] }).units.find((u) => u.side === 'att' && (u.kind === 'inf' || u.kind === 'mtn'))!;
    const r = TC.battleHedges([{ kind: 'hedgerow', points: [[mine.x - 200, mine.y], [mine.x + 200, mine.y]] }], [{ x: mine.x, y: mine.y, kind: 'gate', width: 3.2 }]);
    expect(r.ok).toBe(true);
    expect(r.hedges).toBe(1);
    expect((r.view as { units: U[] }).units.find((u) => u.id === mine.id)!.hid).toBe(1);
  });
});

describe('the men drawn on foot go through the gates in a hedge (looks only)', () => {
  it('a block walked across a hedge line goes over it at its gates, abreast in each gap; with no gates, anywhere', () => {
    const W = 4;
    const crossings = (gated: boolean): { at: number[]; gates: number[] } => {
      const b = new LandBattle(withTeams(sampleTicket('plains', 1.3, { seed: 5, humanAtt: true })));
      b.ai.att = b.ai.def = false;
      const a = foot(b, 'att')[0];
      const y = a.y + a.d / 2 + 40, gates: number[] = [];
      for (let x = a.x - 660; x <= a.x + 660; x += 60) gates.push(x);
      expect(b.hedgesOn([{ kind: 'hedgerow', points: [[a.x - 1500, y], [a.x + 1500, y]] }], gated ? gates.map((x) => ({ x, y, kind: 'gate', width: W })) : [])).toBe(true);
      const d = b.drillOn({ sections: true });
      expect(b.command({ do: 'move', units: [a.id], x: a.x, y: a.y + 300 }, 'att')).toBe(true);
      const was = new Map<Man, number>(), at: number[] = [];
      run(b, 400, () => {
        for (const m of d.squad(a.id)!.men) {
          if (m.fell !== undefined || m.role === 'tank' || m.role === 'gun' || m.role === 'lorry') continue;
          const y0 = was.get(m);
          // Over it on his own feet (a step of a stride or two), never carried up to his block.
          if (y0 !== undefined && y0 < y && m.y >= y && m.y - y0 < 2) at.push(m.x);
          was.set(m, m.y);
        }
      });
      return { at, gates };
    };
    const near = (x: number, gs: number[]): number => Math.min(...gs.map((g) => Math.abs(g - x)));
    const on = crossings(true), off = crossings(false);
    expect(on.at.length).toBeGreaterThan(40);
    expect(off.at.length).toBeGreaterThan(40);
    const atGate = (c: { at: number[]; gates: number[] }): number => c.at.filter((x) => near(x, c.gates) <= W / 2 + 0.5).length / c.at.length;
    expect(atGate(on)).toBeGreaterThan(0.9);
    expect(atGate(off)).toBeLessThan(0.3);
    // Abreast in the gap, not one file on its middle point.
    expect(new Set(on.at.map((x) => Math.round(x - on.gates.reduce((p, g) => (Math.abs(g - x) < Math.abs(p - x) ? g : p))))).size).toBeGreaterThan(2);
  }, 120_000);
});

describe('the regiment blocks named as their armies named them (the historian; release 2, a switch)', () => {
  it('each army\'s word for a third of a division, by the battle\'s month', () => {
    const M = (y: number, m: number): number => y * 12 + m;
    expect(regimentWord('USA', 'inf', 33, 2, M(1944, 6))).toBe('33rd Infantry Regiment');
    expect(regimentWord('SOV', 'inf', 31, 0, M(1941, 7))).toBe('31st Rifle Regiment');
    expect(regimentWord('UK', 'inf', 32, 1, M(1941, 7))).toBe('32nd Infantry Brigade');
    expect(regimentWord('AUS', 'inf', 32, 1, M(1941, 7))).toBe('32nd Infantry Brigade');
    expect(regimentWord('GER', 'inf', 33, 2, M(1942, 9))).toBe('Infanterie-Regiment 33');
    expect(regimentWord('GER', 'inf', 33, 2, M(1942, 10))).toBe('Grenadier-Regiment 33');
    expect(regimentWord('ITA', 'inf', 33, 2, M(1940, 6))).toBe('33rd Blackshirt Legion');
    expect(regimentWord('ITA', 'inf', 32, 1, M(1940, 6))).toBe('32nd Infantry Regiment');
    expect(['A', 'B', 'R'].map((x, i) => regimentWord('USA', 'arm', 1, i, M(1944, 6)))).toEqual(['Combat Command A', 'Combat Command B', 'Combat Command R']);
    expect(regimentWord('GER', 'arm', 3, 0, M(1941, 6))).toBe('Panzer-Regiment 3');
    expect(regimentWord('SOV', 'arm', 3, 0, M(1941, 6))).toBe('3rd Tank Regiment');
    expect(regimentWord('SOV', 'arm', 3, 0, M(1942, 6))).toBe('3rd Tank Brigade');
    expect(regimentWord('UK', 'arm', 3, 0, M(1942, 6))).toBe('3rd Armoured Brigade');
  });

  it('on, a regiment block reads its own word over its division\'s full name; off, as before; before the clock only', () => {
    const t = withTeams(sampleTicket('plains', 1.3, { seed: 5, nat: { att: 'GER', def: 'UK' } }));
    t.turn = 40;
    // One division a side: each is cut into its three regiments (brigades).
    for (const s of ['att', 'def'] as const) t[s].forces = [{ ...t[s].forces[0], divs: 1, share: 1, divisions: [{ name: s === 'att' ? '11th Infantry Division' : '50th Infantry Division', short: s === 'att' ? '11th Infantry' : '50th Infantry', str: 0.9, xp: 0.3, men: 15000 }] }];
    const off = new LandBattle(t), on = new LandBattle(t);
    const regs = (b: LandBattle): LandUnit[] => b.units.filter((u) => / Regiment$/.test(u.short));
    expect(regs(off).length).toBeGreaterThan(0);
    expect(regs(off).every((u) => /^\d+(st|nd|rd|th) Regiment$/.test(u.short))).toBe(true);
    expect(on.namesOn()).toBe(true);
    const ger = on.units.filter((u, i) => u.side === 'att' && off.units[i].short !== u.short);
    const uk = on.units.filter((u, i) => u.side === 'def' && off.units[i].short !== u.short);
    expect(ger.length + uk.length).toBe(regs(off).length);
    for (const u of ger) { expect(u.short).toMatch(/^(Grenadier|Panzergrenadier|Gebirgsjäger|Reiter|Pionier)-Regiment \d+$|^Panzer-Regiment \d+$/); expect(u.parent).toMatch(/Division$/); }
    for (const u of uk) expect(u.short).toMatch(/Brigade$/);
    expect(ger.map((u) => u.short)).toEqual(['Grenadier-Regiment 31', 'Grenadier-Regiment 32', 'Grenadier-Regiment 33']);
    expect(ger.every((u) => u.parent === '11th Infantry Division' && u.name.endsWith(', 11th Infantry'))).toBe(true);
    expect(uk.map((u) => u.short)).toEqual(['148th Infantry Brigade', '149th Infantry Brigade', '150th Infantry Brigade']);
    // Whole divisions keep their names; the fight is untouched.
    for (const [i, u] of on.units.entries()) if (off.units[i].short === u.short) expect(u.name).toBe(off.units[i].name);
    on.ai.att = on.ai.def = off.ai.att = off.ai.def = true;
    run(on, 300); run(off, 300);
    expect(on.units.map((u) => [u.x, u.y, u.str])).toEqual(off.units.map((u) => [u.x, u.y, u.str]));
    expect(on.namesOn()).toBe(false);
  }, 120_000);
});
