// Fighter types (the setup switch 'Fighter types', g.flags.ftrtypes; off as shipped): each nation's real fighters by
// research stage, and a small edge in the count from their measured performance at the mission's height.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { airUpkeep, assessAir } from '../src/campaign/air';
import { techMul } from '../src/campaign/research';
import { startOpen } from '../src/campaign/staff';
import { saveGame } from '../src/campaign/save';
import {
  FIGHTERS, bandOf, edge, fighterClock, fighterMonth, fighterPanel, fighterTypesOn, mixEdge, namesOf, raidCardLines, serviceNews,
  setFighterTypes, sideTypes, typeEdge, typeMix,
} from '../src/campaign/fighters';
import type { AirFleet, AirMission, Game } from '../src/campaign/types';

const fleetOf = (g: Game, nat: string): AirFleet => g.airFleets.find((f) => f.nat === nat && f.ship === undefined)!;
const on = (g: Game): Game => { expect(setFighterTypes(g, true)).toBe(true); return g; };

/** A fight for the sky over an enemy province both sides' fighters reach: this nation's first fighter group, moved to its ground nearest the enemy's capital. */
function aFight(g: Game, nat: string, foe: string, m: AirMission = 'superiority'): { f: AirFleet; target: number } | null {
  const f = g.airFleets.find((x) => x.nat === nat && x.ftr > 0 && x.ship === undefined)!;
  const t = g.regions[g.nations[foe].capital];
  const near = g.regions.filter((r) => r.ctrl === nat).sort((a, b) => Math.hypot(a.x - t.x, a.y - t.y) - Math.hypot(b.x - t.x, b.y - t.y));
  for (const r of near.slice(0, 6)) {
    f.base = r.id;
    const a = assessAir(g, [f], m, t.id);
    if (!a.why && a.Epow > 0 && a.Dpow > 0) return { f, target: t.id };
  }
  return null;
}

describe('the data', () => {
  it('every ladder names known types, and every mix shares out to 1', () => {
    for (const [nat, L] of Object.entries(FIGHTERS.ladders)) {
      const mixes = [...L.history.map((x) => x[1]), ...Object.values(L.rungs).flatMap((v) => v.map((x) => x[1])), ...(L.deck ?? []).map((x) => x[1])];
      for (const mix of mixes) {
        for (const id of Object.keys(mix)) expect(FIGHTERS.types[id], `${nat} ${id}`).toBeTruthy();
        expect(Object.values(mix).reduce((s, x) => s + x, 0), nat).toBeCloseTo(1, 6);
      }
    }
  });

  it('short names are unique, and the panel words follow the UI director\'s rules', () => {
    const shorts = Object.values(FIGHTERS.types).map((t) => t.short.toLowerCase());
    expect(new Set(shorts).size).toBe(shorts.length);
    for (const [id, t] of Object.entries(FIGHTERS.types)) {
      expect(t.plain, id).not.toMatch(/\b1[89]\d\d\b/);
      expect(t.plain, id).not.toMatch(/\d+-\d+ ?km\/h/);
      expect(t.history, id).toMatch(/\.$/);
      for (const bad of ['dogfight bonus', 'tier', 'our aircraft', 'stats', 'modifier', 'buff']) expect(`${t.plain} ${t.history}`.toLowerCase(), id).not.toMatch(new RegExp(`\\b${bad}\\b`));
    }
    expect(FIGHTERS.types.fw190_a3.short).toBe('Fw 190 A');
    expect(FIGHTERS.types.p11c.plain).toContain('about 170 km/h slower than a Bf 109 E');
    expect(FIGHTERS.words.setting).toBe('Fighter types');
  });

  it('heights go by mission, and night has none', () => {
    expect(bandOf('bomb')).toBe('high');
    expect(bandOf('superiority')).toBe('mid');
    expect(bandOf('town')).toBe('mid');
    for (const m of ['ground', 'line', 'strike', 'fleet', 'harbour'] as AirMission[]) expect(bandOf(m)).toBe('low');
    expect(bandOf('night')).toBeNull();
  });
});

describe('the matchup', () => {
  it('gives the research table\'s edges (matchup.py), and is antisymmetric', () => {
    // The calibration anchors as fighters-table.json gives them (three places).
    const anchors: [string, string, 'low' | 'mid' | 'high', number][] = [
      ['spitfire_i', 'bf109_e', 'mid', 0.02], ['hurricane_i', 'bf109_e', 'mid', -0.15], ['fw190_a3', 'spitfire_v', 'mid', 0.12],
      ['p51_bc', 'bf109_g6', 'high', 0.21], ['f6f5', 'a6m5_52', 'mid', 0.17], ['p11c', 'bf109_e', 'mid', -0.26],
      ['mc202', 'spitfire_v_trop', 'mid', -0.02], ['cr42', 'hurricane_i', 'low', -0.19], ['tempest_v', 'fw190_a8', 'low', 0.19],
    ];
    for (const [a, b, band, e] of anchors) {
      expect(edge(a, b, band), `${a} v ${b}`).toBeCloseTo(e, 1);
      expect(Math.abs(edge(a, b, band) - e), `${a} v ${b}`).toBeLessThan(0.006);
      expect(edge(b, a, band)).toBeCloseTo(-edge(a, b, band), 12);
    }
    expect(edge('spitfire_i', 'spitfire_i', 'mid')).toBe(0);
  });

  it('never counts an airframe for more than the cap', () => {
    let most = 0;
    const ids = Object.keys(FIGHTERS.types);
    for (const band of ['low', 'mid', 'high'] as const) for (const a of ids) for (const b of ids) most = Math.max(most, Math.abs(edge(a, b, band)));
    expect(most).toBe(FIGHTERS.w.cap);
    // A jet against a biplane: every part at its most but the turn, which the faster side refuses.
    expect(edge('me262_a1a', 'gauntlet', 'high')).toBeCloseTo(0.29, 12);
  });

  it('weighs a mix against a mix by both shares', () => {
    const mix = mixEdge({ spitfire_i: 0.4, hurricane_i: 0.6 }, { bf109_e: 1 }, 'mid');
    expect(mix).toBeCloseTo(0.4 * edge('spitfire_i', 'bf109_e', 'mid') + 0.6 * edge('hurricane_i', 'bf109_e', 'mid'), 12);
  });
});

describe('which types a nation flies', () => {
  it('in history, on its fighter research clock or the calendar', () => {
    const g = createGame(1, null);
    expect(typeMix(g, fleetOf(g, 'GER'))).toEqual({ bf109_e: 0.9, bf110_c: 0.1 });
    expect(typeMix(g, fleetOf(g, 'POL'))).toEqual({ p11c: 1 });
    // A German fighter branch 24 months ahead of history flies the Fw 190 two years early; the calendar alone moves nobody.
    g.nations.GER.lab!.clock.fighters += 24;
    expect(fighterClock(g, 'GER')).toBe(g.turn + 24);
    expect(typeMix(g, fleetOf(g, 'GER'))).toEqual({ bf109_f4: 0.8, fw190_a3: 0.1, bf110_c: 0.1 });
    expect(fighterClock(g, 'POL')).toBe(g.turn);
  });

  it('a carrier wing flies its deck ladder, and a Spitfire V in the desert or at Malta carries its dust filter', () => {
    const g = createGame(1, null);
    const uk = fleetOf(g, 'UK');
    expect(typeMix(g, { ...uk, ship: 0 })).toEqual({ fulmar_ii: 1 });
    g.turn = 30;
    expect(typeMix(g, uk)).toEqual({ spitfire_v: 0.45, hurricane_ii: 0.45, p40_bc: 0.1 });
    const malta = g.byName['Malta'].id;
    expect(typeMix(g, { ...uk, base: malta })).toEqual({ spitfire_v_trop: 0.45, hurricane_ii: 0.45, p40_bc: 0.1 });
  });

  it('in the grand campaign, by fighter stage and the months at it, written down only with the switch on', () => {
    const off = startOpen(createGame(5, null));
    for (const nat of Object.keys(off.nations)) airUpkeep(off, nat);
    expect(Object.keys(off.flags).filter((k) => k.startsWith('ftr'))).toEqual([]);
    const g = on(startOpen(createGame(5, null)));
    airUpkeep(g, 'GER');
    const stage = Object.keys(g.flags).find((k) => k.startsWith('ftr:GER:'))!;
    expect(stage).toBe('ftr:GER:1');
    expect(g.flags[stage]).toBe(g.turn);
    expect(typeMix(g, fleetOf(g, 'GER'))).toEqual({ bf109_e: 0.9, bf110_c: 0.1 });
    // Twenty-five months at the stage bring the Fw 190 to it.
    g.flags[stage] = g.turn - 25;
    expect(typeMix(g, fleetOf(g, 'GER'))).toEqual({ bf109_f4: 0.8, fw190_a3: 0.1, bf110_c: 0.1 });
  });
});

describe('the count', () => {
  it('off: assessAir carries no type edge and writes nothing', () => {
    const g = createGame(3, null);
    expect(fighterTypesOn(g)).toBe(false);
    const x = aFight(g, 'GER', 'POL')!;
    expect(x).toBeTruthy();
    const before = saveGame(g);
    const a = assessAir(g, [x.f], 'superiority', x.target);
    expect('typeEdge' in a).toBe(false);
    expect(raidCardLines(g, a, 'superiority')).toBeNull();
    expect(fighterPanel(g, x.f)).toEqual([]);
    expect(saveGame(g)).toBe(before);
  });

  it('on: the escort and the defenders move by exp(+E/2) and exp(-E/2), the edge the types give', () => {
    const off = createGame(3, null), g = on(createGame(3, null));
    const x = aFight(off, 'GER', 'POL')!;
    const y = g.airFleets.find((f) => f.id === x.f.id)!;
    y.base = x.f.base;
    const a0 = assessAir(off, [x.f], 'superiority', x.target);
    const a1 = assessAir(g, [y], 'superiority', x.target);
    const E = a1.typeEdge!;
    expect(E).toBeGreaterThan(0.2);
    expect(E).toBeLessThanOrEqual(FIGHTERS.w.cap);
    expect(a1.Epow / a0.Epow).toBeCloseTo(Math.exp(E / 2), 10);
    expect(a1.Dpow / a0.Dpow).toBeCloseTo(Math.exp(-E / 2), 10);
    // The Bf 109 E against the P.11c, on schedule (techMul 1 on both sides): the airframes' edge alone.
    expect(E).toBeCloseTo(mixEdge({ bf109_e: 0.9, bf110_c: 0.1 }, { p11c: 1 }, 'mid'), 10);
  });

  it('at night there is no matchup', () => {
    const g = on(createGame(3, null));
    const f = fleetOf(g, 'GER'), p = fleetOf(g, 'POL');
    expect(typeEdge(g, [{ f, w: 1 }], [{ f: p, w: 1 }], 'night')).toBe(0);
  });

  it('a research lead the count already gives (techMul) is not counted twice (guard A)', () => {
    const g = on(createGame(3, null));
    g.nations.GER.lab!.clock.fighters += 24;
    g.nations.GER.lab!.months += 0;
    const f = fleetOf(g, 'GER'), p = fleetOf(g, 'POL');
    const lead = Math.log(techMul(g, 'GER', 'fighters') / techMul(g, 'POL', 'fighters'));
    expect(lead).toBeGreaterThan(0);
    const e = mixEdge(typeMix(g, f)!, typeMix(g, p)!, 'mid');
    const want = Math.max(-FIGHTERS.w.cap, Math.min(FIGHTERS.w.cap, e - lead));
    expect(typeEdge(g, [{ f, w: 2 }], [{ f: p, w: 3 }], 'superiority')).toBeCloseTo(want, 12);
  });
});

describe('the words', () => {
  it('the raid card: types at the height, escort first, three names a side, and one figure', () => {
    const g = on(createGame(1, null));
    const ger = fleetOf(g, 'GER'), sov = fleetOf(g, 'SOV');
    const card = (E: number, escort = 1, m: AirMission = 'bomb') => raidCardLines(g, { att: [{ f: ger, ftr: escort }], def: [{ f: sov, w: 1 }], typeEdge: E }, m);
    g.turn = 38; // October 1942: the Soviet mix has eight types
    g.nations.GER.lab!.clock.fighters = 0; g.nations.GER.lab!.months = 0;
    g.nations.SOV.lab!.clock.fighters = 0; g.nations.SOV.lab!.months = 0;
    expect(card(0.2)!.types).toBe('Fighters at 7,500 m: Bf 109 G and Fw 190 A against Yak-1, LaGG-3, Yak-7B and 5 more.');
    expect(card(0.2)!.edge).toBe("The escort's aircraft are about 11% stronger.");
    expect(card(-0.2)!.edge).toBe("The defenders' aircraft are about 11% stronger.");
    expect(card(0.03)!.edge).toBe('The aircraft are about even.');
    expect(card(0.2, 1, 'superiority')!.types).toMatch(/^Fighters at 4,500 m: /);
    expect(card(0.2, 0)).toEqual({ types: 'Defending fighters at 7,500 m: Yak-1, LaGG-3, Yak-7B and 5 more.', edge: null });
    expect(card(0.2, 1, 'night')).toBeNull();
    expect(namesOf(['bf109_g6', 'fw190_a8', 'fw190_d9', 'me262_a1a'])).toBe('Bf 109 G-6, Fw 190 A-8, Fw 190 D and one more');
    expect(namesOf(['p51_d', 'p47_d25', 'p38_jl'])).toBe('Mustang D, Thunderbolt D and Lightning J');
    expect(sideTypes(g, [{ f: ger, w: 1 }])).toEqual(['bf109_g2', 'fw190_a3']);
  });

  it('the news: a type entering service, the player\'s own and another nation\'s', () => {
    const g = on(createGame(1, 'GER'));
    expect(serviceNews(g, 'GER', 'fw190_a3')).toBe('The Fw 190 A enters service. It is faster than a Spitfire V, climbs, dives and rolls better, and nothing can follow it when it changes direction.');
    g.human = 'UK';
    expect(serviceNews(g, 'GER', 'fw190_a3')).toBe('Germany puts the Fw 190 A into service. It is faster than a Spitfire V, climbs, dives and rolls better, and nothing can follow it when it changes direction.');
  });

  it('a new type in an enemy\'s fighters is news once, and the first month is not', () => {
    const g = on(createGame(1, 'UK'));
    const n0 = g.headlines.length;
    fighterMonth(g, 'GER');
    expect(g.headlines.length).toBe(n0);
    g.nations.GER.lab!.clock.fighters += 24;
    fighterMonth(g, 'GER');
    const news = g.headlines.slice(n0).map((h) => h.text);
    expect(news).toEqual([
      'Germany puts the Bf 109 F into service. It is the best-balanced Bf 109 and out-climbs and out-runs a Hurricane or Spitfire V at height.',
      'Germany puts the Fw 190 A into service. It is faster than a Spitfire V, climbs, dives and rolls better, and nothing can follow it when it changes direction.',
    ]);
    fighterMonth(g, 'GER');
    expect(g.headlines.length).toBe(n0 + 2);
  });

  it('the air group panel lists each type with both kinds of words', () => {
    const g = on(createGame(1, null));
    const rows = fighterPanel(g, fleetOf(g, 'GER'));
    expect(rows.map((r) => r.short)).toEqual(['Bf 109 E', 'Bf 110']);
    expect(rows[0].name).toBe(FIGHTERS.types.bf109_e.name);
    expect(rows[0].history).toMatch(/1940/);
  });
});

describe('the switch', () => {
  it('is set at setup only', () => {
    const g = createGame(1, null);
    expect(setFighterTypes(g, true)).toBe(true);
    expect(g.flags.ftrtypes).toBe(1);
    expect(setFighterTypes(g, false)).toBe(true);
    expect('ftrtypes' in g.flags).toBe(false);
    g.turn = 2;
    expect(setFighterTypes(g, true)).toBe(false);
    expect(fighterTypesOn(g)).toBe(false);
  });
});
