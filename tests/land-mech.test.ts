// Armoured infantry (release 2; Brian, 23 November 07:01Z, yes to "half-tracks carrying armoured infantry"; the
// historian's rows, 07:05Z, and the design director's rulings, 07:06Z). With LandBattle.mech on, an armoured block draws
// its division's riflemen riding behind its tanks, a section to each carrier of its army's row: in Normandy in 1944 the
// American armour's M3 half-tracks, the German armour's Sd.Kfz. 251s and lorries. They get down near the enemy as lorried
// infantry do, fall as their block's strength goes, and small arms do not reach a man aboard a half-track. Looks only:
// the battle is the very same.
import { describe, expect, it } from 'vitest';
import { livePiece, liveTicket, type LiveBattle } from '../src/battle/land/live';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { DRILL, type Round } from '../src/battle/land/drill';

const UPTO = 300;
const BASE = DRILL.mech.base;
const SMALL = new Set(['rifle', 'mg', 'grenade', 'at']);

interface InMan { k: number; j?: number; role: string; pose: string; fell?: number; ml?: string; lor?: number; sec?: number }
interface InSquad { u: { id: number; side: string; kind: string; nat: string }; men: InMan[]; veh: InMan[]; of?: number; tier: number }

interface Run {
  L: LiveBattle; units: string; rounds: Round[];
  carriers: Map<number, string[]>; riders0: number; rodeThenDown: number; riflemenFell: number;
  aboardFell: { w: string; ml: string }[]; ownerless: number;
}

function run(mech: boolean): Run {
  const L = livePiece('normandy-1944', 11, { me: 'att', mech });
  const squads = (L.drill as unknown as { squads: Map<number, InSquad> }).squads;
  const lorryOf = (sq: InSquad, m: InMan): InMan => sq.veh[(m.lor ?? (sq.tier >= 1 ? m.sec ?? m.k : m.k)) % sq.veh.length];
  const carriers = new Map<number, string[]>();
  for (const sq of squads.values()) if (sq.of !== undefined) carriers.set(sq.of, sq.veh.map((v) => v.ml ?? 'Truck'));
  const riders = [...squads.values()].filter((sq) => sq.of !== undefined).flatMap((sq) => sq.men.map((m) => [sq, m] as const));
  const riders0 = riders.filter(([, m]) => m.pose === 'mounted').length;
  const wasMounted = new Set<InMan>(), wasDown = new Set<InMan>();
  const aboard = new Map<InMan, string>();
  const rounds: Round[] = [];
  const aboardFell: { w: string; ml: string }[] = [];
  while (!L.sim.over && L.sim.t < UPTO - 1e-6) {
    // Who is aboard an armoured carrier as the step begins (the drill takes this step's losses from these places).
    aboard.clear();
    for (const [sq, m] of riders) if (m.fell === undefined && m.pose === 'mounted') aboard.set(m, lorryOf(sq, m).ml ?? 'Truck');
    const d = L.to(Math.min(UPTO, L.sim.t + 1), 100000);
    const got = d.rounds as Round[];
    rounds.push(...got);
    for (const [sq, m] of riders) {
      if (m.pose === 'mounted') wasMounted.add(m);
      else if (m.fell === undefined && wasMounted.has(m)) wasDown.add(m);
      // A man of a section (j) who fell this step while aboard a half-track: the round that took him.
      if (m.fell !== undefined && m.j !== undefined && aboard.has(m) && DRILL.mech.armoured.includes(aboard.get(m)!)) {
        const r = got.find((x) => x.on === sq.u.id && x.ok === m.k && x.oj === m.j && x.r === 'kill');
        if (r) aboardFell.push({ w: r.w, ml: aboard.get(m)! });
        aboard.delete(m);
      }
    }
  }
  const view = L.drill.view();
  const ids = new Set(view.map((b) => b.id));
  const ownerless = [...squads.values()].filter((sq) => sq.of !== undefined && !L.sim.allBlocks().some((u) => u.id === sq.of)).length + (ids.size === view.length ? 0 : 1);
  const units = JSON.stringify(L.sim.units.map((u) => [u.id, u.str, u.figures, u.state, Math.round(u.x), Math.round(u.y)]));
  const riflemenFell = riders.filter(([, m]) => m.fell !== undefined).length;
  return { L, units, rounds, carriers, riders0, rodeThenDown: wasDown.size, riflemenFell, aboardFell, ownerless };
}

describe('armoured infantry', () => {
  const off = run(false), on = run(true);
  const unit = (id: number) => on.L.sim.allBlocks().find((u) => u.id === id)!;

  it('fights the very same battle: every block, figure and result', () => {
    expect(on.units).toBe(off.units);
    expect(JSON.stringify(on.L.sim.over)).toBe(JSON.stringify(off.L.sim.over));
  });

  it('draws each armoured block\'s carriers by its army\'s row, and only with the switch on', () => {
    expect(off.carriers.size).toBe(0);
    const arm = on.L.sim.allBlocks().filter((u) => u.kind === 'arm');
    expect(arm.length).toBeGreaterThan(0);
    for (const u of arm) {
      const c = on.carriers.get(u.id);
      console.log(`armoured infantry: ${u.nat} ${u.name}: ${c?.join(', ') ?? 'none'}`);
      expect(c).toBeDefined();
      if (u.nat === 'USA') expect(c!.every((m) => m === 'M3Halftrack')).toBe(true);
      if (u.nat === 'GER') { expect(c).toContain('Hanomag'); for (const m of c!) expect(['Hanomag', 'Truck']).toContain(m); }
      expect(c!.length).toBe(Math.max(1, Math.round(((u.nat === 'USA' ? 3 : 4) * u.tray) / 4)));
    }
    for (const id of on.carriers.keys()) expect(unit(id).kind).toBe('arm');
  });

  it('draws them with their block, numbered clear of its tanks', () => {
    expect(on.ownerless).toBe(0);
    const view = on.L.drill.view();
    const riders = view.flatMap((b) => b.men.filter((m) => m.k >= BASE).map((m) => [b.id, m] as const));
    expect(riders.length).toBeGreaterThan(0);
    for (const [id] of riders) expect(unit(id).kind).toBe('arm');
    const carriers = view.flatMap((b) => b.men.filter((m) => m.r === 'lorry'));
    for (const v of carriers) if (unit(view.find((b) => b.men.includes(v))!.id).kind === 'arm') expect(v.ml).toBeDefined();
    expect(off.L.drill.view().some((b) => unit(b.id).kind === 'arm' && b.men.some((m) => m.k >= BASE || m.r === 'lorry'))).toBe(false);
  });

  it('rides in, gets down near the enemy, and falls as the block loses strength', () => {
    console.log(`armoured infantry: aboard at the start ${on.riders0}, got down ${on.rodeThenDown}, fell ${on.riflemenFell}`);
    expect(on.riders0).toBeGreaterThan(0);
    expect(on.rodeThenDown).toBeGreaterThan(0);
    expect(on.riflemenFell).toBeGreaterThan(0);
    const armIds = new Set(on.L.sim.allBlocks().filter((u) => u.kind === 'arm').map((u) => u.id));
    const kills = on.rounds.filter((r) => r.r === 'kill' && armIds.has(r.on) && (r.ok ?? 0) >= BASE);
    expect(kills.length).toBeGreaterThan(0);
    for (const r of kills) { expect(r.by).toBeDefined(); expect(unit(r.by!).side).not.toBe(unit(r.on).side); }
  });

  it('carries what the corps has where the campaign says (Units decide), the calendar only for the history start', () => {
    const t = ticketOf(PIECES.find((x) => x.key === 'normandy-1944')!, 11, 'att');
    const kit = (side: typeof t.att, halftracks: number, at: boolean) => ({ ...side, forces: side.forces.map((f) => ({ ...f, kit: { halftracks, at } })) });
    const L = liveTicket({ ...t, att: kit(t.att, 0, false), def: kit(t.def, 1, true) }, { me: 'att', mech: true }, 'normandy-1944');
    const squads = (L.drill as unknown as { squads: Map<number, InSquad> }).squads;
    const sim = L.sim as unknown as { ownAntiTank(u: unknown): boolean };
    for (const sq of squads.values()) {
      if (sq.of === undefined) continue;
      const u = L.sim.allBlocks().find((x) => x.id === sq.of)!, c = sq.veh.map((v) => v.ml);
      console.log(`armoured infantry, by kit: ${u.nat} ${u.name}: ${c.join(', ')}`);
      expect(c.length).toBe(Math.max(1, Math.round((DRILL.mech.kit.per * u.tray) / 4)));
      expect(c.every((m) => m === (u.side === 'att' ? 'Truck' : 'Hanomag'))).toBe(true);
    }
    for (const u of L.sim.allBlocks()) if (u.kind !== 'arm' && u.kind !== 'art') expect(sim.ownAntiTank(u)).toBe(u.side === 'def');
  });

  it('keeps a man aboard a half-track from small arms', () => {
    console.log(`armoured infantry: men taken aboard a half-track ${on.aboardFell.length} (${on.aboardFell.map((x) => x.w).join(', ')})`);
    for (const x of on.aboardFell) expect(SMALL.has(x.w)).toBe(false);
  });
}, 600000);
