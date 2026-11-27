// Support teams at work, held poses (release 2, looks only; the art director, 27 November 2026, 17:57Z, on the Mac's
// P11c film: "No drill state drawn for under 2 s; no man in pieces"). Drill.steady keeps each man of a team (its crews
// and its mortars' spotters) in the pose he is drawn in DR.teamwork.held seconds at least before another, a shot excepted
// (a burst's or a bomb's fire, and the pose either side of it). These tests read the men as the 3D game does (the live
// delta's tracks: each man's changes of pose, with their battle times) and hold that no man of a team at work changes
// pose sooner, in four fights, the Mac's own among them (Normandy, seed 11); and, as the control that the count can see
// one, that the old drill of the teams (the row off) is drawn changing in under 2 s often.
import { describe, expect, it } from 'vitest';
import { sampleTicket, withTeams } from '../src/battle/land';
import { livePiece, liveTicket, type LiveBattle } from '../src/battle/land/live';
import { DRILL as DR } from '../src/battle/land/drill';

const SECONDS = 120;
type Open = (teamwork: boolean) => LiveBattle;
const FIGHTS: { name: string; open: Open }[] = [
  { name: 'Normandy, seed 11 (the Mac\'s P11c)', open: (teamwork) => livePiece('normandy-1944', 11, { teamwork, me: null }) },
  { name: 'Normandy, seed 2', open: (teamwork) => livePiece('normandy-1944', 2, { teamwork, me: null }) },
  { name: 'Prokhorovka, seed 3', open: (teamwork) => livePiece('prokhorovka-1943', 3, { teamwork, me: null }) },
  {
    name: 'plains, seed 5 (USA on GER, both sides\' teams)',
    open: (teamwork) => liveTicket(withTeams(sampleTicket('plains', 1.3, { seed: 5, nat: { att: 'USA', def: 'GER' }, style: { att: 'methodical', def: 'bold' } })), { teamwork, me: null }),
  },
];

interface Held { teamMen: number; changes: number; short: string[]; shots: number }

/** Fights SECONDS of one battle live, reading every change of pose of every man of a support team as the scene gets it. */
function held(open: Open, teamwork: boolean): Held {
  const L = open(teamwork);
  const teams = new Set(L.sim.units.filter((u) => u.team).map((u) => u.id));
  const block = new Map<number, number>(), last = new Map<number, [string, number]>();
  const out: Held = { teamMen: 0, changes: 0, short: [], shots: 0 };
  for (let k = 0; k < SECONDS * 10 && !L.sim.over; k++) {
    const d = L.tick(0.1, 1);
    for (const m of d.men) {
      if (m.b !== undefined) { block.set(m.n, m.b); if (teams.has(m.b)) out.teamMen++; }
      const b = block.get(m.n);
      if (b === undefined || !teams.has(b)) continue;
      for (const c of m.ch ?? []) {
        const t = c[0] as number, pose = String(c[1]), was = last.get(m.n);
        if (was && was[0] !== pose) {
          out.changes++;
          if (was[0] === 'fire' || pose === 'fire') out.shots++;
          else if (t - was[1] < DR.teamwork.held - 0.05 && was[1] > 0) out.short.push(`${was[1].toFixed(1)} to ${t.toFixed(1)} s, block ${b}, man ${m.n}: ${was[0]}, then ${pose}`);
        }
        if (!was || was[0] !== pose) last.set(m.n, [pose, t]);
      }
    }
  }
  return out;
}

describe('support teams at work: every pose held 2 s at least, a shot excepted (the art director, 27 November)', () => {
  it('holds it at 2 s', () => {
    expect(DR.teamwork.held).toBe(2);
  });
  for (const f of FIGHTS) {
    it(`${f.name}: no man of a team drawn in a pose for under 2 s`, () => {
      const h = held(f.open, true);
      expect(h.teamMen).toBeGreaterThan(20);
      expect(h.changes).toBeGreaterThan(100);
      expect(h.short, h.short.slice(0, 12).join('\n')).toEqual([]);
    }, 600_000);
  }
  it('control: the old drill of the teams (the row off) is drawn changing in under 2 s often, so the count would see one', () => {
    const h = held(FIGHTS[0].open, false);
    expect(h.short.length).toBeGreaterThan(100);
  }, 600_000);
});
