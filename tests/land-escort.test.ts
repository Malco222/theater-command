// Infantry with tanks (release 2; Brian, 23 November 05:05Z: "if the troops are with tanks, have them advance behind the
// tank using it as cover like they would in real life ... make sure their players cant be run over by their own vechiles
// and they get out of the way. vechiles can run over enemy soldiers though which would kill them"). With
// LandBattle.escort on, riflemen near a friendly tank file in behind her, every man steps out of his own side's vehicles'
// way, and a man the rules take to an enemy tank on the move may be the one under her tracks. The looks change no
// result. The overrun (LAND.overrun; the design director, 05:48Z) is a rule: a tank block drives through foot in the
// open that has no anti-tank weapon near or of its own, and backs out; harm unchanged, measured paired (the gate). In
// Normandy in 1944 both sides' foot carry their own (bazookas, Panzerfausts) and the Germans are dug in: no overrun, the
// same battle. Against Italian foot standing in the open there, the Shermans drive through and men die under them.
import { describe, expect, it } from 'vitest';
import { livePiece, liveTicket, type LiveBattle } from '../src/battle/land/live';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import type { Round } from '../src/battle/land/drill';

const UPTO = 300;

interface InMan { k: number; role: string; x: number; y: number; face: number; went?: number; pose: string; fell?: number; esc?: number }
interface InSquad { u: { id: number; side: string; kind: string }; men: InMan[] }

/** Normandy, seed 11, with the defence Italian foot standing in the open: no tanks, not dug in, no anti-tank weapon of its own. */
function italianFoot() {
  const t = ticketOf(PIECES.find((x) => x.key === 'normandy-1944')!, 11, 'att');
  return { ...t, def: { ...t.def, nat: 'ITA', forces: t.def.forces.map((f) => ({ ...f, nat: 'ITA', dug: false, kind: f.kind === 'arm' ? 'inf' : f.kind })) } };
}

interface Run { L: LiveBattle; overruns: number; rounds: Round[]; units: string; followers: number; mostBehind: number; under: number; checks: number }

function run(escort: boolean, italians = false): Run {
  const L = italians ? liveTicket(italianFoot(), { me: 'att', escort }, 'normandy-1944') : livePiece('normandy-1944', 11, { me: 'att', escort });
  const rounds: Round[] = [];
  let followers = 0, mostBehind = 0, under = 0, checks = 0, overruns = 0;
  const driving = new Set<number>();
  const squads = (L.drill as unknown as { squads: Map<number, InSquad> }).squads;
  while (!L.sim.over && L.sim.t < UPTO - 1e-6) {
    const d = L.to(Math.min(UPTO, L.sim.t + 1), 100000);
    rounds.push(...(d.rounds as Round[]));
    for (const u of L.sim.units) { if (u.ovr && !driving.has(u.id)) { overruns++; driving.add(u.id); } if (!u.ovr) driving.delete(u.id); }
    // Men on their feet under a vehicle of their own side on the move: inside her length ahead and her width.
    const movers: { v: InMan; side: string }[] = [];
    for (const sq of squads.values()) for (const v of sq.men) if ((v.role === 'tank' || v.role === 'lorry') && v.fell === undefined && (v.went ?? 0) > 0.4) movers.push({ v, side: sq.u.side });
    let behind = 0;
    for (const sq of squads.values()) {
      if (sq.u.kind === 'arm' || sq.u.kind === 'art') continue;
      for (const m of sq.men) {
        if (m.fell !== undefined || m.role === 'tank' || m.role === 'lorry') continue;
        if (m.esc !== undefined) behind++;
        for (const o of movers) {
          if (o.side !== sq.u.side) continue;
          const f = o.v.pose === 'reverse' ? o.v.face + Math.PI : o.v.face;
          const dx = m.x - o.v.x, dy = m.y - o.v.y, a = dx * Math.sin(f) + dy * Math.cos(f), l = dx * Math.cos(f) - dy * Math.sin(f);
          checks++;
          if (a > 0 && a < 3 && Math.abs(l) < 1.2) under++;
        }
      }
    }
    followers += behind; mostBehind = Math.max(mostBehind, behind);
  }
  const units = JSON.stringify(L.sim.units.map((u) => [u.id, u.str, u.figures, u.state, Math.round(u.x), Math.round(u.y)]));
  return { L, overruns, rounds, units, followers, mostBehind, under, checks };
}

describe('infantry with tanks', () => {
  const off = run(false), on = run(true);
  const sideOf = new Map(on.L.sim.units.map((u) => [u.id, u.side]));
  const kindOf = new Map(on.L.sim.units.map((u) => [u.id, u.kind]));

  it('fights the very same battle: every block, figure and result', () => {
    expect(on.units).toBe(off.units);
    expect(JSON.stringify(on.L.sim.over)).toBe(JSON.stringify(off.L.sim.over));
  });

  it('files riflemen in behind their tanks, and only with the switch on', () => {
    console.log(`infantry with tanks: man-seconds behind a tank ${on.followers} (most at once ${on.mostBehind}); off ${off.followers}`);
    expect(off.followers).toBe(0);
    expect(on.followers).toBeGreaterThan(0);
  });

  it('keeps men out from under their own side\'s vehicles', () => {
    console.log(`infantry with tanks: own men in a moving vehicle's path, a second's look: off ${off.under}, on ${on.under} (of ${on.checks} looks)`);
    expect(on.under).toBeLessThanOrEqual(off.under);
    expect(on.under).toBeLessThanOrEqual(Math.max(2, Math.floor(off.under / 4)));
  });

  it('overruns no one where the foot carries its own anti-tank weapons and is dug in', () => {
    expect(on.overruns).toBe(0);
  });

  it('drives tanks through foot in the open with no anti-tank weapon, and runs men over there', () => {
    const offI = run(false, true), onI = run(true, true);
    const crushed = onI.rounds.filter((r) => r.w === 'track');
    console.log(`infantry with tanks: Italian foot in the open: overruns ${onI.overruns} (off ${offI.overruns}), men run over ${crushed.length} (off ${offI.rounds.filter((r) => r.w === 'track').length})`);
    expect(offI.overruns).toBe(0);
    expect(offI.rounds.filter((r) => r.w === 'track').length).toBe(0);
    expect(onI.overruns).toBeGreaterThan(0);
    expect(crushed.length).toBeGreaterThan(0);
    const sides = new Map(onI.L.sim.units.map((u) => [u.id, u.side]));
    for (const r of crushed) expect(sides.get(r.by!)).not.toBe(sides.get(r.on));
  }, 600000);

  it('runs men over only with an enemy tank, and only with the switch on', () => {
    const crushed = on.rounds.filter((r) => r.w === 'track');
    console.log(`infantry with tanks: men run over ${crushed.length}`);
    expect(off.rounds.filter((r) => r.w === 'track').length).toBe(0);
    for (const r of crushed) {
      expect(r.r).toBe('kill');
      expect(r.by).toBeDefined();
      expect(kindOf.get(r.by!)).toBe('arm');
      expect(sideOf.get(r.by!)).not.toBe(sideOf.get(r.on));
    }
  });
});
