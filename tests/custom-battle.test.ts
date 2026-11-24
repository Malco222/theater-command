// A battle of your own (release 2, Rules/custom-bridge.ts): two armies bought from one price list, on a battlefield of
// the real map, fought by the campaign's own rules and the 3D game's land rules, with no campaign touched.
import { describe, expect, it } from 'vitest';
import { AIR_PLACES, LAND_PLACES, customArmy, customCalls, customCheck, customLook, customOffer, type CustomSpec } from '../unreal/TheaterCommand/Rules/custom-bridge';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { liveTicket } from '../src/battle/land/live';
import type { FieldResult } from '../src/battle/core/types';

const base = (o: Partial<CustomSpec> = {}): CustomSpec => ({
  domain: 'land', place: 'pomorze', size: 'battle', role: 'att',
  me: { nat: 'GER', kit: 3, train: 2, buys: { inf: 1, arm: 2 } }, them: { nat: 'SOV', kit: 3, train: 1, buys: null }, ...o,
});

function fight(look: { ticket: unknown; live?: unknown }, me: 'att' | 'def'): FieldResult {
  const L = liveTicket(structuredClone(look.ticket) as never, { ...(look.live as object ?? {}), me } as never);
  for (let guard = 0; !L.sim.over && guard < 100000; guard++) L.tick(0.5, 1000);
  expect(L.sim.over).toBeTruthy();
  return L.sim.over as FieldResult;
}

describe('a battle of your own: the offer', () => {
  it('lists nations, kits, training, sizes, the price lists and the battlefields', () => {
    const o = customOffer();
    expect(o.nations.length).toBeGreaterThan(20);
    expect(o.nations[0].major).toBe(true);
    expect(o.kits.map((k) => k.tier)).toEqual([1, 2, 3, 4]);
    expect(o.land.buys.map((b) => b.kind)).toEqual(['inf', 'mtn', 'mot', 'arm', 'art', 'eng']);
    expect(o.land.buys.find((b) => b.kind === 'arm')!.cost).toBe(66);
    expect(o.air.buys.map((b) => b.kind)).toEqual(['ftr', 'bmr']);
    expect(o.air.flak.costs).toEqual([8, 20, 38]);
    expect(o.places.length).toBe(LAND_PLACES.length);
    expect(o.sea.open).toBe(false);
  });
});

describe('a battle of your own: the computer\'s army', () => {
  it('fits its treasury and the room on the ground, and always brings a fighting corps', () => {
    for (const p of LAND_PLACES) for (const size of ['clash', 'battle', 'great']) for (const role of ['att', 'def'] as const) {
      const spec = base({ place: p.id, size, role });
      const a = customArmy(spec, 'them');
      expect(a.spent).toBeLessThanOrEqual(a.budget);
      const n = Object.values(a.buys).reduce((s, x) => s + x, 0);
      expect(n).toBeGreaterThan(0);
      expect(n).toBeLessThanOrEqual(a.room);
      expect(Object.entries(a.buys).some(([k, x]) => x > 0 && k !== 'art' && k !== 'eng')).toBe(true);
    }
  });
  it('brings a mix of corps for the ground, not one kind over and over', () => {
    for (const p of LAND_PLACES) for (const role of ['att', 'def'] as const) {
      const a = customArmy(base({ place: p.id, size: 'battle', role }), 'them');
      expect(Object.keys(a.buys).length, `${p.id} ${role} ${JSON.stringify(a.buys)}`).toBeGreaterThanOrEqual(2);
      expect(Object.values(a.buys).every((n) => n <= 3), `${p.id} ${role} ${JSON.stringify(a.buys)}`).toBe(true);
    }
  });
  it('digs in when it defends', () => {
    expect(customArmy(base({ role: 'att' }), 'them').works).toBe(1);
  });
  it('picks a raid with bombers under escort, and a defence of fighters', () => {
    const spec: CustomSpec = { domain: 'air', place: 'brussels', size: 'battle', role: 'att', kind: 'bomb', me: { nat: 'UK', buys: null }, them: { nat: 'GER', buys: null } };
    const mine = customArmy(spec, 'me'), theirs = customArmy(spec, 'them');
    expect(mine.buys.bmr).toBeGreaterThan(0);
    expect(mine.buys.ftr).toBeGreaterThan(0);
    expect(theirs.buys.bmr ?? 0).toBe(0);
    expect(mine.spent).toBeLessThanOrEqual(mine.budget);
  });
});

describe('a battle of your own: the checks', () => {
  it('leaves an empty army empty: only an army left to the computer (null) is picked for it', () => {
    const c = customCheck(base({ me: { nat: 'GER', kit: 3, train: 2, buys: {} } }));
    expect(c.ok).toBe(false);
    expect(c.why).toMatch(/nothing in it yet/);
    expect(c.them.count).toBeGreaterThan(0);
  });
  it('refuses an army over its treasury, over its room, with no fighting corps, or two of one nation', () => {
    expect(customCheck(base({ me: { nat: 'GER', buys: { arm: 4 } } })).why).toMatch(/more than the treasury/);
    expect(customCheck(base({ place: 'calabria', size: 'great', me: { nat: 'GER', buys: { mtn: 3 } } })).why).toMatch(/room for 2/);
    expect(customCheck(base({ me: { nat: 'GER', buys: { art: 2 } } })).why).toMatch(/cannot take or hold ground alone/);
    expect(customCheck(base({ them: { nat: 'GER', buys: null } })).why).toMatch(/two different nations/);
    expect(customCheck(base()).ok).toBe(true);
  });
});

describe('a battle of your own: the land battle', () => {
  it('makes a ticket on every battlefield, the sides marked for who commands them', () => {
    for (const p of LAND_PLACES) {
      for (const role of ['att', 'def'] as const) {
        const look = customLook(base({ place: p.id, role, me: { nat: 'GER', kit: 3, train: 2, buys: { inf: 1, arm: 1 } } })) as { ticket: any; live: any; why?: string };
        expect(look.why ?? '', `${p.id} ${role}`).toBe('');
        expect(look.ticket.place.name).toBe(p.region);
        expect(look.ticket.att.human).toBe(role === 'att');
        expect(look.ticket.def.human).toBe(role === 'def');
        expect(look.ticket.att.nat).toBe(role === 'att' ? 'GER' : 'SOV');
        expect(look.live.fog).toBe(false);
      }
    }
  });

  it('is fought to its end by the land rules, either side his', { timeout: 300_000 }, () => {
    for (const role of ['att', 'def'] as const) {
      const look = customLook(base({ role })) as { ticket: any; live: any };
      const over = fight(look, role);
      expect(['att', 'def', null, undefined]).toContain((over as any).winner ?? null);
      expect(typeof over.R).toBe('number');
    }
  });

  it('decides by the units: better kit and training on the same ground raise the odds, whatever the flags', () => {
    const odds = (me: CustomSpec['me'], them: CustomSpec['them']) => (customLook(base({ me, them })) as any).ticket.reckoning.odds as number;
    const army = { inf: 1, arm: 2 }, foe = { inf: 2, arm: 1 };
    const low = odds({ nat: 'POL', kit: 1, train: 0, buys: army }, { nat: 'ITA', kit: 3, train: 2, buys: foe });
    const high = odds({ nat: 'POL', kit: 4, train: 3, buys: army }, { nat: 'ITA', kit: 3, train: 2, buys: foe });
    expect(high).toBeGreaterThan(low * 1.3);
    // the same armies under other flags price the same (no national handcuffs)
    const a = odds({ nat: 'GER', kit: 3, train: 2, buys: army }, { nat: 'SOV', kit: 3, train: 2, buys: foe });
    const b = odds({ nat: 'FRA', kit: 3, train: 2, buys: army }, { nat: 'USA', kit: 3, train: 2, buys: foe });
    expect(Math.abs(a / b - 1)).toBeLessThan(0.02);
  });

  it('leaves the campaign alone', () => {
    // (built twice the same: no state carried from one sandbox to the next)
    const a = customLook(base()) as any, b = customLook(base()) as any;
    expect(JSON.stringify(a.ticket)).toBe(JSON.stringify(b.ticket));
  });
});

void AIR_PLACES;

describe('a battle of your own: the air battle', () => {
  const air = (o: Partial<CustomSpec> = {}): CustomSpec => ({
    domain: 'air', place: AIR_PLACES[0].id, size: 'battle', role: 'att', kind: 'bomb',
    me: { nat: 'GER', kit: 3, train: 2, buys: null }, them: { nat: 'UK', kit: 3, train: 2, buys: null }, ...o,
  });
  const fly = (spec: CustomSpec) => {
    const TC = battleCalls({ game: () => { throw new Error('no grand campaign in a battle of your own'); }, attack: () => null });
    const C = customCalls({ airStartOn: TC.airStartOn });
    const me = customArmy(spec, 'me'), them = customArmy(spec, 'them');
    const full = { ...spec, me: { ...spec.me, buys: me.buys, works: me.works }, them: { ...spec.them, buys: them.buys, works: them.works } };
    expect(customCheck(full).ok).toBe(true);
    const st = C.customAirStart(full) as { sky: { att: { human: boolean }; def: { human: boolean } }; view: { units: unknown[] } } | null;
    return { TC, st, why: C.customWhy() };
  };
  it('flies on every air battlefield, either side his, the raid or the fight for the sky, to its after card', () => {
    for (const p of AIR_PLACES) for (const role of ['att', 'def'] as const) for (const kind of ['bomb', 'superiority']) {
      const { TC, st, why } = fly(air({ place: p.id, role, kind }));
      expect(st, `${p.id} ${role} ${kind}: ${why}`).toBeTruthy();
      expect(st!.sky.att.human).toBe(role === 'att');
      expect(st!.sky.def.human).toBe(role === 'def');
      TC.battleSpeed(4);
      let v: any = null;
      for (let k = 0; k < 4000 && !(v && v.over); k++) v = TC.battleTick(5);
      expect(v.over).toBeTruthy();
      const out: any = TC.battleEnd();
      expect(out?.ok, `${p.id} ${role} ${kind}: ${out?.note}`).toBe(true);
      expect(out.after.lost).toBeTruthy();
      expect(out.after.groups.some((x: any) => x.side === 'att') && out.after.groups.some((x: any) => x.side === 'def')).toBe(true);
      // (the card is told from his side: 'your' bombers when he raids, 'your' fighters when he defends)
      const words = out.after.lines.join(' ');
      if (role === 'def') expect(words, words).not.toMatch(/your \d+ bombers/);
    }
  }, 300_000);
});
