// Self-propelled guns and tank destroyers (release 2; Brian, 23 November 07:01Z, yes to "a tank destroyer and a
// self-propelled gun for each major army"; the historian's rows, 07:05Z; the design director, 07:28Z, (a): a tank
// destroyer is one model in an armoured block's mix, the block's own figures and fight unchanged, the corps' kit first
// and the rows only the history start's; set pieces stay as the historian drew them). With LandBattle.spg on, some of a
// battery's guns are drawn as its army's self-propelled howitzer, with no tractor and no crew on foot; with
// LandBattle.td on, one tank in some armoured blocks is drawn as a tank destroyer. Looks only: the battle is the same.
import { describe, expect, it } from 'vitest';
import { livePiece, liveTicket, type LiveBattle, type TrackDelta } from '../src/battle/land/live';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import type { FieldTicket } from '../src/battle/core/types';

const UPTO = 240;
const SP = new Set(['M7', 'Sexton', 'Hummel', 'SU76']);
const TD = new Set(['M10', 'SU85', 'Semovente', 'StuG3']);

interface InMan { k: number; j?: number; sec?: number; role: string; pose: string; ml?: string }
interface InSquad { u: { id: number; side: string; kind: string; nat: string; team?: string }; men: InMan[]; veh: InMan[]; of?: number }

type Opts = { spg?: boolean; td?: boolean };
type Kit = { spg?: number; td?: number };

function ticket(key: string, kits?: { att?: Kit; def?: Kit }): FieldTicket<never> {
  const t = ticketOf(PIECES.find((x) => x.key === key)!, 11, 'att');
  const kit = (side: typeof t.att, k?: Kit) => (k ? { ...side, forces: side.forces.map((f) => ({ ...f, kit: { ...f.kit, ...k } })) } : side);
  return { ...t, att: kit(t.att, kits?.att), def: kit(t.def, kits?.def) } as unknown as FieldTicket<never>;
}

interface Run { L: LiveBattle; units: string; over: string; tracks: Map<number, TrackDelta>; poses: Map<number, Set<string>> }

/** A campaign battle (no set piece) on a set piece's ticket, fought to UPTO; every track's header and the poses seen. */
function run(t: FieldTicket<never>, o: Opts, piece?: string): Run {
  const L = liveTicket(t as never, { me: 'att', ...o }, piece);
  const tracks = new Map<number, TrackDelta>(), poses = new Map<number, Set<string>>();
  const take = (men: TrackDelta[]): void => {
    for (const d of men) {
      if (d.r !== undefined) tracks.set(d.n, d);
      for (const c of d.ch ?? []) { const p = (c as unknown as unknown[])[1]; if (typeof p === 'string') (poses.get(d.n) ?? poses.set(d.n, new Set()).get(d.n)!).add(p); }
    }
  };
  take(L.to(0, 1).men);
  while (!L.sim.over && L.sim.t < UPTO - 1e-6) take(L.to(Math.min(UPTO, L.sim.t + 2), 100000).men);
  const units = JSON.stringify(L.sim.units.map((u) => [u.id, u.str, u.figures, u.state, Math.round(u.x), Math.round(u.y)]));
  return { L, units, over: JSON.stringify(L.sim.over), tracks, poses };
}

const squadsOf = (L: LiveBattle): Map<number, InSquad> => (L.drill as unknown as { squads: Map<number, InSquad> }).squads;
const batteries = (L: LiveBattle): InSquad[] => [...squadsOf(L).values()].filter((sq) => sq.u.kind === 'art' && !sq.u.team);
const gunsOf = (sq: InSquad): InMan[] => sq.men.filter((m) => m.role === 'gun');

describe('self-propelled guns and tank destroyers', () => {
  const normandy = ticket('normandy-1944');
  const off = run(normandy, {}), on = run(normandy, { spg: true, td: true });

  it('fights the very same battle: every block, figure and result', () => {
    expect(on.units).toBe(off.units);
    expect(on.over).toBe(off.over);
  });

  it('draws nothing other with the switches off, nor in a set piece', () => {
    for (const r of [off, run(normandy, { spg: true, td: true }, 'normandy-1944')]) {
      for (const sq of squadsOf(r.L).values()) for (const m of sq.men) expect(m.ml).toBeUndefined();
      for (const d of r.tracks.values()) if (d.r === 'tank' || d.r === 'gun') expect(d.ml).toBeUndefined();
    }
  });

  it('draws a battery\'s guns self-propelled by its army\'s row, the first guns first, without tractors or crews', () => {
    // Normandy, July 1944 (the ticket's month): two American armoured blocks bring three M7s each, so the American
    // battery is all M7s; two German ones half a Hummel each, so one of the German battery's guns is a Hummel.
    const by: Record<string, string[]> = {};
    for (const sq of batteries(on.L)) {
      const guns = gunsOf(sq), sp = guns.filter((g) => g.ml);
      by[sq.u.nat] = guns.map((g) => g.ml ?? 'towed');
      console.log(`self-propelled guns: ${sq.u.nat} battery ${sq.u.id}: ${by[sq.u.nat].join(', ')}; tractors ${sq.veh.length}`);
      for (const g of sp) expect(SP.has(g.ml!)).toBe(true);
      // the first guns first, and only the towed have a tractor and a crew on foot
      guns.forEach((g, i) => { if (g.ml) expect(guns.slice(0, i).every((x) => x.ml)).toBe(true); });
      expect(sq.veh.length).toBeLessThanOrEqual(guns.length - sp.length);
      if (sp.length === guns.length) expect(sq.veh.length).toBe(0);
      for (const g of sp) expect(sq.men.some((m) => m.role === 'crew' && m.sec === g.k)).toBe(false);
    }
    expect(by.USA?.every((m) => m === 'M7')).toBe(true);
    expect(by.GER?.filter((m) => m === 'Hummel').length).toBe(1);
    expect(by.GER?.[0]).toBe('Hummel');
  });

  it('sends the 3D game a self-propelled gun as a tank of her own model, driving and halting, never towed', () => {
    let seen = 0;
    for (const [n, d] of on.tracks) {
      if (!d.ml || !SP.has(d.ml)) continue;
      seen++;
      expect(d.r).toBe('tank');
      for (const p of on.poses.get(n) ?? []) expect(['drive', 'halt', 'fire', 'reverse']).toContain(p);
    }
    expect(seen).toBeGreaterThan(0);
  });

  it('keeps tank destroyers to the rows\' armies and months: none in Normandy\'s American and German armour', () => {
    for (const sq of squadsOf(on.L).values()) if (sq.u.kind === 'arm') for (const m of sq.men) expect(m.ml).toBeUndefined();
  });

  it('draws Kursk by its month: SU-76s and Hummels in July 1943, no SU-85 until August', () => {
    const r = run(ticket('prokhorovka-1943'), { spg: true, td: true });
    const count = (nat: string, ml: string): number => batteries(r.L).filter((sq) => sq.u.nat === nat).flatMap(gunsOf).filter((g) => g.ml === ml).length;
    const sov = r.L.sim.allBlocks().filter((u) => u.nat === 'SOV' && u.kind === 'inf').length;
    const ger = r.L.sim.allBlocks().filter((u) => u.nat === 'GER' && u.kind === 'arm').length;
    console.log(`Kursk: SU-76 ${count('SOV', 'SU76')} (${sov} rifle blocks), Hummel ${count('GER', 'Hummel')} (${ger} armoured blocks)`);
    expect(count('SOV', 'SU76')).toBe(Math.round(sov * 0.5));
    expect(count('GER', 'Hummel')).toBe(Math.round(ger * 0.5));
    for (const sq of squadsOf(r.L).values()) if (sq.u.kind === 'arm') for (const m of sq.men) expect(m.ml).toBeUndefined();
  });

  it('draws what the corps has where the campaign says it (Units decide), one tank destroyer in the mix', () => {
    const t = ticket('normandy-1944', { att: { td: 0.25, spg: 0 }, def: { td: 0.5, spg: 1 } });
    const r = run(t, { spg: true, td: true });
    for (const sq of squadsOf(r.L).values()) {
      const u = sq.u;
      if (u.kind === 'arm') {
        const tanks = sq.men.filter((m) => m.role === 'tank'), share = u.side === 'att' ? 0.25 : 0.5;
        const want = tanks.map((_, k) => Math.floor((k + 1) * share + 1e-9) > Math.floor(k * share + 1e-9));
        console.log(`tank destroyers, by kit: ${u.nat} ${u.id}: ${tanks.map((m) => m.ml ?? 'own').join(', ')}`);
        tanks.forEach((m, k) => expect(m.ml).toBe(want[k] ? (u.nat === 'USA' ? 'M10' : 'StuG3') : undefined));
        for (const m of tanks) if (m.ml) expect(TD.has(m.ml)).toBe(true);
      }
      if (u.kind === 'art' && !u.team) for (const g of gunsOf(sq)) expect(g.ml).toBe(u.side === 'att' ? undefined : 'Hummel');
    }
    // and the same battle as with nothing drawn
    expect(r.units).toBe(run(t, {}).units);
  });
}, 600000);
