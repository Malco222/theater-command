// Armoured cars and AA vehicles (release 2; Brian, 23 November 07:01Z, item 3: "armoured cars, jeeps, motorcycles and
// AA"; the historian's rows, 07:47Z, historian/armoured-cars-aa-1123.md; the design director, 07:47Z: accepted as
// proposed, the corps' kit first). With LandBattle.cars on, a block's armoured cars keep out ahead on its flanks till it
// meets the enemy, then behind it; with LandBattle.aa on, its AA vehicles keep behind it. Looks only: the battle is the
// same, none fires, and they are lost as their block's strength goes. A campaign battle's only.
import { describe, expect, it } from 'vitest';
import { liveTicket, type LiveBattle } from '../src/battle/land/live';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { DRILL } from '../src/battle/land/drill';
import type { FieldTicket } from '../src/battle/core/types';

const UPTO = 240;
const CARS = new Set(['SdKfz222', 'M8', 'ArmouredCar']);
const AA = new Set(['M16', 'FlakHalftrack', 'FlakTruck']);

interface InMan { k: number; role: string; pose: string; ml?: string; x: number; y: number; fell?: number }
interface InSquad { u: { id: number; side: string; kind: string; nat: string; team?: string }; men: InMan[]; of?: number; esc?: 'cars' | 'aa' }
type Kit = { cars?: number; aa?: number };

function ticket(key: string, kits?: { att?: Kit; def?: Kit }): FieldTicket<never> {
  const t = ticketOf(PIECES.find((x) => x.key === key)!, 11, 'att');
  const kit = (side: typeof t.att, k?: Kit) => (k ? { ...side, forces: side.forces.map((f) => ({ ...f, kit: { ...f.kit, ...k } })) } : side);
  return { ...t, att: kit(t.att, kits?.att), def: kit(t.def, kits?.def) } as unknown as FieldTicket<never>;
}

interface Run { L: LiveBattle; units: string; over: string; along: Map<InMan, { d: number; a: number[] }>; header: Record<string, unknown> }

/** A campaign battle on a set piece's ticket, to UPTO: each car's place along its block's facing, from its middle, each second. */
function run(t: FieldTicket<never>, o: { cars?: boolean; aa?: boolean }, piece?: string): Run {
  const L = liveTicket(t as never, { me: 'att', ...o }, piece);
  const header = L.header();
  const along = new Map<InMan, { d: number; a: number[] }>();
  const look = (): void => {
    for (const sq of escorts(L)) {
      if (sq.esc !== 'cars') continue;
      const u = L.sim.allBlocks().find((x) => x.id === sq.of)!;
      // (how far beyond its block's front edge: below minus its depth, behind its tray)
      for (const m of sq.men) if (m.fell === undefined) (along.get(m) ?? along.set(m, { d: u.d, a: [] }).get(m)!).a.push((m.x - u.x) * Math.sin(u.face) + (m.y - u.y) * Math.cos(u.face) - u.d / 2);
    }
  };
  look();
  while (!L.sim.over && L.sim.t < UPTO - 1e-6) { L.to(Math.min(UPTO, L.sim.t + 1), 100000); look(); }
  const units = JSON.stringify(L.sim.units.map((u) => [u.id, u.str, u.figures, u.state, Math.round(u.x), Math.round(u.y)]));
  return { L, units, over: JSON.stringify(L.sim.over), along, header };
}

const squadsOf = (L: LiveBattle): Map<number, InSquad> => (L.drill as unknown as { squads: Map<number, InSquad> }).squads;
const escorts = (L: LiveBattle): InSquad[] => [...squadsOf(L).values()].filter((sq) => sq.esc !== undefined);

describe('armoured cars and AA vehicles', () => {
  const normandy = ticket('normandy-1944');
  const off = run(normandy, {}), on = run(normandy, { cars: true, aa: true });
  const blocks = on.L.sim.allBlocks();
  const owner = (sq: InSquad) => blocks.find((u) => u.id === sq.of)!;

  it('fights the very same battle: every block, figure and result', () => {
    expect(on.units).toBe(off.units);
    expect(on.over).toBe(off.over);
  });

  it('draws none with the switches off, nor in a set piece', () => {
    expect(escorts(off.L).length).toBe(0);
    expect(escorts(run(normandy, { cars: true, aa: true }, 'normandy-1944').L).length).toBe(0);
  });

  it('brings each army\'s cars and AA by the historian\'s rows (Normandy, July 1944)', () => {
    const got = new Map<number, string[]>();
    for (const sq of escorts(on.L)) got.set(sq.of!, [...(got.get(sq.of!) ?? []), ...sq.men.map((m) => m.ml!)]);
    for (const u of blocks) console.log(`cars and AA: ${u.nat} ${u.kind}${u.team ? '/' + u.team : ''} ${u.id}: ${(got.get(u.id) ?? []).join(', ') || 'none'}`);
    const of = (nat: string, kind: string) => blocks.filter((u) => u.nat === nat && u.kind === kind && !u.team).map((u) => got.get(u.id) ?? []);
    // America: an M8 and an M16 with every armoured block, and with every third infantry block
    for (const l of of('USA', 'arm')) expect(l.sort()).toEqual(['M16', 'M8']);
    of('USA', 'inf').forEach((l, i) => expect(l.sort()).toEqual((i + 1) % 3 === 0 ? ['M16', 'M8'] : []));
    // Germany: a flak half-track with each armoured block and a 222 with every other; a flak lorry with every third infantry block
    of('GER', 'arm').forEach((l, i) => expect(l.sort()).toEqual(i % 2 ? ['FlakHalftrack', 'SdKfz222'] : ['FlakHalftrack']));
    of('GER', 'inf').forEach((l, i) => expect(l).toEqual((i + 1) % 3 === 0 ? ['FlakTruck'] : []));
    for (const l of [...of('USA', 'art'), ...of('GER', 'art')]) expect(l).toEqual([]);
    for (const u of blocks.filter((x) => x.team)) expect(got.has(u.id)).toBe(false);
    for (const sq of escorts(on.L)) for (const m of sq.men) { expect(m.role).toBe('tank'); expect((sq.esc === 'cars' ? CARS : AA).has(m.ml!)).toBe(true); }
  });

  it('keeps the cars out ahead before contact, then back behind their block', () => {
    let out = 0, back = 0;
    for (const [, { d, a }] of on.along) {
      if (a[0] > 0) out++;
      if (a[0] > 0 && a.some((x) => x < -d)) back++;
    }
    console.log(`cars and AA: ${on.along.size} cars, ${out} out ahead at the start, ${back} of them back behind later`);
    expect(out).toBeGreaterThan(0);
    expect(back).toBeGreaterThan(0);
  });

  it('loses them as their block\'s strength goes, never while it holds half its figures', () => {
    let fell = 0;
    for (const sq of escorts(on.L)) {
      const u = owner(sq), c = sq.men.length, keep = Math.round((c * Math.max(0, u.figures)) / Math.max(1, u.figures0));
      sq.men.forEach((m, i) => expect(m.fell !== undefined).toBe(u.figures <= 0 || i >= keep));
      fell += sq.men.filter((m) => m.fell !== undefined).length;
    }
    console.log(`cars and AA: ${fell} lost`);
  });

  it('tells the 3D game which models to read before the battle opens', () => {
    const hb = on.header.blocks as { id: number; mls?: string[] }[];
    for (const sq of escorts(on.L)) for (const m of sq.men) expect(hb.find((b) => b.id === sq.of)?.mls).toContain(m.ml);
    expect((off.header.blocks as { mls?: string[] }[]).some((b) => b.mls)).toBe(false);
  });

  it('brings what the corps has where the campaign says it (Units decide)', () => {
    const r = run(ticket('normandy-1944', { att: { cars: 2, aa: 0 }, def: { cars: 0, aa: 0.5 } }), { cars: true, aa: true });
    const bl = r.L.sim.allBlocks().filter((u) => !u.team && u.kind !== 'fort');
    for (const u of bl) {
      const mine = escorts(r.L).filter((sq) => sq.of === u.id).flatMap((sq) => sq.men.map((m) => m.ml));
      if (u.side === 'att') expect(mine).toEqual(['M8', 'M8']);
      else {
        const i = bl.filter((x) => x.side === u.side && x.corps === u.corps && x.kind === u.kind && x.id < u.id).length;
        expect(mine).toEqual(i % 2 ? ['FlakHalftrack'] : []);
      }
    }
    expect(DRILL.cars.kit.models.USA).toBe('M8');
  });
}, 600000);
