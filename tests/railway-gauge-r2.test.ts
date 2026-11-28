// Railway gauge (release 2; Brian's yes of 27 November 2026 17:10Z; the design director's rulings of 17:15Z and 17:48Z
// on r2-three-ideas/FORM-rails-rivers-1127.md, part A): a nation's trains run only on track of its own gauge; its
// railway troops re-lay captured track a few provinces a month; in the east the line is counted from the invader's own
// railheads and every step costs the roads' alone. With the switch off nothing is written or read.
import { describe, expect, it } from 'vitest';
import { GAUGE, fitsGauge, gaugeOn, homeGauge, relayChoice, setGauge, trackGauge } from '../src/campaign/gauge';
import { railheads, railsAt, relayMonth, onNet } from '../src/campaign/rails';
import { EASTERN, addCorps, computeSupply, removeCorps, createGame, ensureSupply, freshStep, railReach, weather, whyNoTrain } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { loadGame, saveGame } from '../src/campaign/save';
import type { Corps, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

const war = (seed = 3, human: string | null = 'GER'): Game => startOpen(createGame(seed, human));
const corps = (g: Game, nat: string, reg: number, kind: Corps['kind'] = 'inf'): Corps => addCorps(g, nat, `${nat} ${kind} gauge test corps`, kind, 2, reg, {});
const gaugeFlags = (g: Game): string[] => Object.keys(g.flags).filter((k) => k.startsWith('ga:'));
/**
 * A way into Soviet track from German track: a German province (home), a Polish one joined to it by rail (pol), and a
 * Soviet one joined to that by rail (sov). Germany is given the Polish and the Soviet one, taken this month.
 */
function into(g: Game): { home: number; pol: number; sov: number } {
  // The shortest way by rail from German ground through Polish ground to the first Soviet province.
  const prev = new Map<number, number>();
  const q: number[] = [];
  for (const r of g.regions) if (r.ctrl === 'GER' && r.owner === 'GER' && onNet(r.id)) { prev.set(r.id, -1); q.push(r.id); }
  for (let i = 0; i < q.length; i++) {
    for (const j of railsAt(q[i])) {
      if (prev.has(j)) continue;
      const R = g.regions[j];
      if (R.owner === 'SOV' && g.regions[q[i]].owner === 'POL') {
        const path = [j];
        for (let at = q[i]; prev.get(at)! >= 0 || g.regions[at].owner === 'POL'; at = prev.get(at)!) { path.unshift(at); if (prev.get(at)! < 0) break; }
        let home = prev.get(path[0])!;
        if (home < 0) home = path.shift()!;
        // Emptied of the armies and town guards that stand there, so the field is Germany's too.
        for (const x of path) {
          const X = g.regions[x];
          for (const u of [...X.units]) if (u.nat !== 'GER') removeCorps(g, u);
          X.garr = [];
          X.ctrl = 'GER'; X.since = g.turn;
        }
        g.supplyDirty = true;
        return { home, pol: path[path.length - 2], sov: j };
      }
      if (R.owner !== 'POL') continue;
      prev.set(j, q[i]); q.push(j);
    }
  }
  throw new Error('no way into Soviet track');
}

describe('railway gauge (release 2)', () => {
  it('is off unless switched on: every train runs on every track and nothing is written', () => {
    const g = war();
    const { sov } = into(g);
    expect(gaugeOn(g)).toBe(false);
    expect(fitsGauge(g, 'GER', sov)).toBe(true);
    computeSupply(g);
    expect(railheads(g, 'GER')![sov]).toBe(0);
    expect(relayMonth(g, 'GER')).toEqual([]);
    expect(gaugeFlags(g)).toEqual([]);
  });

  it('the table: the Soviet Union and Finland broad, Spain and Portugal Iberian, everyone else standard; track by its owner at the opening', () => {
    const g = war();
    setGauge(g, true);
    expect(homeGauge('SOV')).toBe(homeGauge('FIN'));
    expect(homeGauge('SOV')).not.toBe(homeGauge('GER'));
    expect(homeGauge('ESP')).not.toBe(homeGauge('SOV'));
    expect(homeGauge('GER')).toBe(homeGauge('POL'));
    const { pol, sov } = into(g);
    expect(trackGauge(g, sov)).toBe(homeGauge('SOV'));
    expect(trackGauge(g, pol)).toBe(homeGauge('POL'));
    expect(fitsGauge(g, 'GER', sov)).toBe(false);
    expect(fitsGauge(g, 'SOV', pol)).toBe(false);
  });

  it('on: German trains stop at the Soviet track; railway troops re-lay it, then they run there', () => {
    const g = war();
    setGauge(g, true);
    const { home, pol, sov } = into(g);
    const c = corps(g, 'GER', home);
    computeSupply(g);
    expect(railheads(g, 'GER')![pol]).toBe(0);
    expect(railheads(g, 'GER')![sov]).not.toBe(0);
    expect(railReach(g, c).has(sov)).toBe(false);
    const why = whyNoTrain(g, c, sov)!;
    expect(why.code).toBe('gauge');
    expect(why.line).toMatch(/broad gauge and your trains need standard gauge/);
    // The month's re-laying: the Soviet province touches the working net, so it is re-laid; then trains run there.
    expect(relayMonth(g, 'GER')).toContain(sov);
    expect(trackGauge(g, sov)).toBe(homeGauge('GER'));
    expect(g.headlines.some((h) => /railway troops re-lay the track at/.test(h.text))).toBe(true);
    ensureSupply(g);
    expect(railheads(g, 'GER')![sov]).toBe(0);
    expect(railReach(g, c).has(sov)).toBe(true);
    // Whoever takes it next finds that gauge: Soviet trains do not run on it.
    expect(fitsGauge(g, 'SOV', sov)).toBe(false);
    // The gauge keeps through a save; off forgets every track re-laid.
    expect(trackGauge(loadGame(saveGame(g))!, sov)).toBe(homeGauge('GER'));
    setGauge(g, false);
    expect(gaugeFlags(g)).toEqual([]);
  });

  it('re-lays up to the relay figure a month, main lines first, never cut track nor track off its working net', () => {
    const g = war();
    setGauge(g, true);
    const size = g.regions.length;
    // A hand-made net: provinces 0 and 1 on it (1 a depot), candidates 2 (three lines), 3 (one line), 4 (cut), 5 (off the net).
    const net = new Int16Array(size).fill(-1); net[0] = 0; net[1] = 0;
    const links = (r: number): number[] => ({ 0: [1, 2, 3], 1: [0, 2], 2: [0, 1, 6], 3: [0], 4: [1], 5: [6] } as Record<number, number[]>)[r] ?? [];
    for (const r of [2, 3, 4, 5]) { g.regions[r].ctrl = 'GER'; g.flags[`ga:${r}`] = homeGauge('SOV'); }
    const cut = (r: number): boolean => r === 4;
    const got = relayChoice(g, 'GER', net, (r) => r === 1, links, () => true, cut);
    expect(got).toEqual([2, 3].slice(0, GAUGE.relay));
    expect(got).not.toContain(4);
    expect(got).not.toContain(5);
  });

  it('in the east the line is counted from the invader\'s own railheads, every step the roads\' alone', () => {
    const g = war();
    const { sov } = into(g);
    const w = weather(g), bad = w.mud || w.winter ? EASTERN.badWeather : 1;
    computeSupply(g);
    // Off: ground taken this month costs a fresh step.
    expect(g.reach.GER[sov]).toBeCloseTo(EASTERN.stepFresh * bad, 6);
    expect(freshStep(g)).toBe(EASTERN.stepFresh);
    setGauge(g, true);
    computeSupply(g);
    expect(freshStep(g)).toBe(EASTERN.stepConverted);
    expect(g.reach.GER[sov]).toBeCloseTo(EASTERN.stepConverted * bad, 6);
    // Re-laid, it is on the invader's own railhead.
    relayMonth(g, 'GER');
    ensureSupply(g);
    expect(g.reach.GER[sov]).toBe(0);
  });
});
