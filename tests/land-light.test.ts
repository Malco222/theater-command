// Jeeps, field cars and motorcycles (release 2; Brian, 23 November 07:01Z, item 3: "armoured cars, jeeps, motorcycles
// and AA"; the historian's rows, 08:18Z, historian/light-cars-motorcycles-1123.md; done as the armoured cars, the design
// director's 07:47Z ruling: the corps' kit first). With LandBattle.light on, a block's light vehicles ride out ahead on
// its flanks with its screen, or park at its rear corners, a motorcycle there riding despatch back and up again; never
// more than two to a block. Looks only: the battle is the same, none fires, and they are lost as their block's strength
// goes. A campaign battle's only.
import { describe, expect, it } from 'vitest';
import { liveTicket, type LiveBattle } from '../src/battle/land/live';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { DRILL, lookOf } from '../src/battle/land/drill';
import type { LandUnit } from '../src/battle/land/units';
import type { FieldTicket } from '../src/battle/core/types';

const UPTO = 240;
const CARS = new Set(['Jeep', 'Kubelwagen', 'GAZ67', 'Fiat508', 'Kurogane', 'FieldCar']);
const BIKES = new Set(['R75', 'M72', 'Rikuo', 'WLA', 'M20', 'Alce', 'Motorcycle']);

interface InMan { k: number; role: string; pose: string; ml?: string; sw?: Record<string, string>; duty?: string; x: number; y: number; fell?: number }
interface InSquad { u: { id: number }; men: InMan[]; of?: number; esc?: string }
type Kit = { jeeps?: number; bikes?: number };
type Light = { ml: string; duty: string; sw?: Record<string, string> };

function ticket(key: string, kits?: { att?: Kit; def?: Kit }): FieldTicket<never> {
  const t = ticketOf(PIECES.find((x) => x.key === key)!, 11, 'att');
  const kit = (side: typeof t.att, k?: Kit) => (k ? { ...side, forces: side.forces.map((f) => ({ ...f, kit: { ...f.kit, ...k } })) } : side);
  return { ...t, att: kit(t.att, kits?.att), def: kit(t.def, kits?.def) } as unknown as FieldTicket<never>;
}

interface Run { L: LiveBattle; units: string; over: string; along: Map<InMan, { d: number; a: number[] }>; header: Record<string, unknown> }

/** A campaign battle on a set piece's ticket, to UPTO: each light vehicle's place along its block's facing beyond its front edge, each second. */
function run(t: FieldTicket<never>, o: { light?: boolean }, piece?: string): Run {
  const L = liveTicket(t as never, { me: 'att', ...o }, piece);
  const header = L.header();
  const along = new Map<InMan, { d: number; a: number[] }>();
  const look = (): void => {
    for (const sq of lights(L)) {
      const u = L.sim.allBlocks().find((x) => x.id === sq.of)!;
      for (const m of sq.men) if (m.fell === undefined) (along.get(m) ?? along.set(m, { d: u.d, a: [] }).get(m)!).a.push((m.x - u.x) * Math.sin(u.face) + (m.y - u.y) * Math.cos(u.face) - u.d / 2);
    }
  };
  look();
  while (!L.sim.over && L.sim.t < UPTO - 1e-6) { L.to(Math.min(UPTO, L.sim.t + 1), 100000); look(); }
  const units = JSON.stringify(L.sim.units.map((u) => [u.id, u.str, u.figures, u.state, Math.round(u.x), Math.round(u.y)]));
  return { L, units, over: JSON.stringify(L.sim.over), along, header };
}

const squadsOf = (L: LiveBattle): Map<number, InSquad> => (L.drill as unknown as { squads: Map<number, InSquad> }).squads;
const lights = (L: LiveBattle): InSquad[] => [...squadsOf(L).values()].filter((sq) => sq.esc === 'light');
const lightOf = (L: LiveBattle, u: LandUnit): Light[] => (L.drill as unknown as { lightOf(u: LandUnit): Light[] }).lightOf(u);

describe('jeeps, field cars and motorcycles', () => {
  const normandy = ticket('normandy-1944');
  const off = run(normandy, {}), on = run(normandy, { light: true });
  const blocks = on.L.sim.allBlocks();
  const owner = (sq: InSquad) => blocks.find((u) => u.id === sq.of)!;
  const got = new Map<number, InMan[]>();
  for (const sq of lights(on.L)) got.set(sq.of!, [...(got.get(sq.of!) ?? []), ...sq.men]);

  it('fights the very same battle: every block, figure and result', () => {
    expect(on.units).toBe(off.units);
    expect(on.over).toBe(off.over);
  });

  it('draws none with the switch off, nor in a set piece', () => {
    expect(lights(off.L).length).toBe(0);
    expect(lights(run(normandy, { light: true }, 'normandy-1944').L).length).toBe(0);
  });

  it('brings each army\'s by the historian\'s rows (Normandy, July 1944), never more than two to a block', () => {
    const say = (m: InMan) => `${m.ml}${m.duty ? ' ' + m.duty : ''}${m.sw ? ' ' + JSON.stringify(m.sw) : ''}`;
    for (const u of blocks) console.log(`jeeps and motorcycles: ${u.nat} ${u.kind}${u.team ? '/' + u.team : ''} ${u.id}: ${(got.get(u.id) ?? []).map(say).join(', ') || 'none'}`);
    const of = (nat: string, kind: string) => blocks.filter((u) => u.nat === nat && u.kind === kind && !u.team).map((u) => got.get(u.id) ?? []);
    for (const l of got.values()) expect(l.length).toBeLessThanOrEqual(DRILL.light.most);
    // America: a jeep with every block (out ahead with the armour's cavalry, a staff jeep without her gun with the rest),
    // a WLA with every third armoured and infantry block and every fourth battery, despatch
    of('USA', 'arm').forEach((l, i) => {
      expect(l.map((m) => m.ml)).toEqual((i + 1) % 3 === 0 ? ['Jeep', 'WLA'] : ['Jeep']);
      expect(l[0].duty).toBe('ahead');
      expect(l[0].sw).toBeUndefined();
      if (l[1]) expect(l[1].duty).toBe('despatch');
    });
    of('USA', 'inf').forEach((l, i) => {
      expect(l.map((m) => m.ml)).toEqual((i + 1) % 3 === 0 ? ['Jeep', 'WLA'] : ['Jeep']);
      expect(l[0]).toMatchObject({ duty: 'behind', sw: { mg: 'off' } });
    });
    // Germany, 1944: a Kubelwagen with every armoured block, one in two infantry; the R75s behind on despatch by then
    of('GER', 'arm').forEach((l, i) => expect(l.map((m) => m.ml)).toEqual(i % 2 ? ['Kubelwagen', 'R75'] : ['Kubelwagen']));
    of('GER', 'inf').forEach((l, i) => expect(l.map((m) => m.ml)).toEqual(i % 2 ? ['Kubelwagen', 'R75'] : []));
    for (const l of [...of('GER', 'arm'), ...of('GER', 'inf')]) for (const m of l) expect(m.duty).toBe(m.ml === 'R75' ? 'despatch' : 'behind');
    for (const u of blocks.filter((x) => x.team || x.kind === 'fort')) expect(got.has(u.id)).toBe(false);
    for (const l of got.values()) for (const m of l) { expect(m.role).toBe('tank'); expect(CARS.has(m.ml!) || BIKES.has(m.ml!)).toBe(true); }
  });

  it('sends the 3D game each one\'s model and swaps on her track', () => {
    let seen = 0;
    for (const b of on.L.drill.view()) for (const m of b.men) {
      if (m.k < DRILL.mech.base + 300 || m.k >= DRILL.mech.base + 400) continue;
      seen++;
      expect(m.r).toBe('tank');
      expect(CARS.has(m.ml!) || BIKES.has(m.ml!)).toBe(true);
      if (m.ml === 'Jeep' && m.sw) expect(m.sw).toEqual({ mg: 'off' });
    }
    expect(seen).toBeGreaterThan(0);
  });

  it('keeps those ahead out in front before contact, then behind; a despatch rider rides back and comes up again', () => {
    let out = 0, back = 0, rides = 0, returns = 0;
    for (const [m, { d, a }] of on.along) {
      if (m.duty === 'ahead') {
        if (a[0] > 0) out++;
        if (a[0] > 0 && a.some((x) => x < -d)) back++;
      }
      if (m.duty === 'despatch') {
        const far = a.findIndex((x) => x < -d - 150);
        if (far >= 0) rides++;
        if (far >= 0 && a.slice(far).some((x) => x > -d - 60)) returns++;
      }
    }
    console.log(`jeeps and motorcycles: ${on.along.size} in all; ${out} out ahead at the start, ${back} of them back behind later; ${rides} despatch rides back, ${returns} up again`);
    expect(out).toBeGreaterThan(0);
    expect(back).toBeGreaterThan(0);
    expect(rides).toBeGreaterThan(0);
    expect(returns).toBeGreaterThan(0);
  });

  it('loses them as their block\'s strength goes', () => {
    for (const sq of lights(on.L)) {
      const u = owner(sq), c = sq.men.length, keep = Math.round((c * Math.max(0, u.figures)) / Math.max(1, u.figures0));
      sq.men.forEach((m, i) => expect(m.fell !== undefined).toBe(u.figures <= 0 || i >= keep));
    }
  });

  it('tells the 3D game which models to read before the battle opens', () => {
    const hb = on.header.blocks as { id: number; mls?: string[] }[];
    for (const sq of lights(on.L)) for (const m of sq.men) expect(hb.find((b) => b.id === sq.of)?.mls).toContain(lookOf(m.ml!, m.sw));
    expect(hb.some((b) => b.mls?.includes('Jeep|mg=off'))).toBe(true);
    expect((off.header.blocks as { mls?: string[] }[]).some((b) => b.mls)).toBe(false);
  });

  it('draws Kursk by its month: the Soviet cars half GAZ-67s and half lend-lease jeeps, the Germans\' R75s behind', () => {
    const r = run(ticket('prokhorovka-1943'), { light: true });
    const all = lights(r.L).flatMap((sq) => sq.men.map((m) => ({ nat: r.L.sim.allBlocks().find((u) => u.id === sq.of)!.nat, m })));
    const sov = all.filter((x) => x.nat === 'SOV' && CARS.has(x.m.ml!)).map((x) => x.m.ml);
    console.log(`Kursk: Soviet cars ${sov.join(', ')}; German ${all.filter((x) => x.nat === 'GER').map((x) => x.m.ml).join(', ')}`);
    expect(sov.length).toBeGreaterThan(1);
    expect(Math.abs(sov.filter((x) => x === 'Jeep').length - sov.filter((x) => x === 'GAZ67').length)).toBeLessThanOrEqual(1);
    for (const x of all) if (x.m.ml === 'R75') expect(x.m.duty).toBe('despatch');
    for (const x of all) if (x.nat === 'SOV' && BIKES.has(x.m.ml!)) expect([x.m.ml, x.m.duty]).toEqual(['M72', 'ahead']);
  });

  it('brings what the corps has where the campaign says it (Units decide); the rest\'s motorcycle with a sidecar by nation', () => {
    const r = run(ticket('normandy-1944', { att: { jeeps: 1, bikes: 1 }, def: { jeeps: 0, bikes: 0.5 } }), { light: true });
    const bl = r.L.sim.allBlocks().filter((u) => !u.team && u.kind !== 'fort');
    for (const u of bl) {
      const mine = lights(r.L).filter((sq) => sq.of === u.id).flatMap((sq) => sq.men.map((m) => m.ml));
      if (u.side === 'att') expect(mine).toEqual(['Jeep', 'WLA']);
      else {
        const i = bl.filter((x) => x.side === u.side && x.corps === u.corps && x.kind === u.kind && x.id < u.id).length;
        expect(mine).toEqual(i % 2 ? ['R75'] : []);
      }
    }
    expect(r.units).toBe(run(ticket('normandy-1944'), {}).units);
    // (the plain motorcycle: a sidecar for the French and Poles, solo for the Finns)
    const u0 = bl.find((u) => u.side === 'att' && u.kind === 'inf')!;
    const as = (nat: string): Light[] => lightOf(r.L, Object.assign(Object.create(u0) as LandUnit, { nat, kit: { jeeps: 0, bikes: 1 } }));
    expect(as('FRA')).toEqual([{ ml: 'Motorcycle', duty: 'despatch', sw: { sidecar: 'on' } }]);
    expect(as('FIN')).toEqual([{ ml: 'Motorcycle', duty: 'despatch', sw: { sidecar: 'off' } }]);
  });
}, 600000);
