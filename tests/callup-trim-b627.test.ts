// Ruling 228's rider b481, sized at ruling 233's rider b627: the one trim of the Soviet Union's free call-up. At the
// dial's stops the plan names, a share of the corps each month's wave and the standing call-up would raise are not
// raised; their men stay in the reserve, and the fraction is carried from month to month. The call-up only, never the
// factories; nothing outside the grand campaign; nothing where the cut is 0, as it ships until it is sized.
import { afterEach, describe, expect, it } from 'vitest';
import { EASTERN, callUpCut, createGame, standingCallUp, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { menLeft } from '../src/campaign/manpower';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const plan = EASTERN.mobilisation.SOV;
const shipped = plan.trim ? { ...plan.trim, stops: [...plan.trim.stops] } : undefined;
afterEach(() => { plan.trim = shipped ? { ...shipped, stops: [...shipped.stops] } : undefined; });

/** A grand campaign in 1942: the Soviet Union and Germany at war, the first waves long in, the standing call-up running. */
function standing(stop = 3, seed = 2): Game {
  const g = startOpen(createGame(seed, null));
  if (stop !== 3) setStop(g, stop, true);
  for (const [a, b] of [['GER', 'SOV'], ['SOV', 'GER']]) if (!g.nations[a].atWar.includes(b)) g.nations[a].atWar.push(b);
  g.turn = 30;
  g.flags['mobilised:SOV'] = g.turn - plan.perMonth.length - 2;
  return g;
}
const rifles = (g: Game): number => g.corps.filter((c) => c.nat === 'SOV' && !c.dead && /Rifle Corps$/.test(c.name)).length;
/** The corps the call-up raises in each of so many months. */
function months(g: Game, n: number): number[] {
  const out: number[] = [];
  for (let k = 0; k < n; k++) { const before = rifles(g); startPhase(g, 'SOV'); out.push(rifles(g) - before); }
  return out;
}

describe('the Soviet call-up trim (b481, b627)', () => {
  it('ships as a knob that cuts nothing until it is sized, at stops 3 to 5', () => {
    expect(plan.trim).toEqual({ stops: [3, 4, 5], cut: 0 });
    for (const stop of [1, 2, 3, 4, 5]) expect(callUpCut(standing(stop), 'SOV'), `stop ${stop}`).toBe(0);
  });

  it('cut, raises that share fewer corps, carrying the fraction from month to month', () => {
    const whole = months(standing(), 2);
    expect(whole).toEqual([plan.after, plan.after]);
    plan.trim = { stops: [3, 4, 5], cut: 0.3 };
    const g = standing();
    expect(callUpCut(g, 'SOV')).toBeCloseTo(0.3, 9);
    expect(standingCallUp(g, 'SOV')!.corps).toBe(3);
    // 5 x 0.7 = 3.5: three corps and half a corps carried; then 3.5 + 0.5 = 4, and nothing carried.
    expect(months(g, 2)).toEqual([3, 4]);
    expect(g.flags['mtr:SOV']).toBeCloseTo(0, 9);
  });

  it('leaves the men of the corps it does not raise in the reserve', () => {
    const a = standing(), b = standing();
    plan.trim = undefined;
    months(a, 1);
    plan.trim = { stops: [3, 4, 5], cut: 0.4 };
    months(b, 1);
    expect(menLeft(b, 'SOV') - menLeft(a, 'SOV')).toBeCloseTo(2 * plan.divs, 6);
  });

  it('with a first month, cuts nothing before it and its share from it on', () => {
    plan.trim = { stops: [3, 4, 5], cut: 0.4, from: 31 };
    const g = standing();
    expect(callUpCut(g, 'SOV')).toBe(0);
    expect(months(g, 1)).toEqual([plan.after]);
    g.turn = 31;
    expect(callUpCut(g, 'SOV')).toBeCloseTo(0.4, 9);
    expect(months(g, 1)).toEqual([3]);
  });

  it('cuts nothing at a stop it does not name, nor outside the grand campaign, nor for a nation without a trim', () => {
    plan.trim = { stops: [3, 4, 5], cut: 0.5 };
    expect(callUpCut(standing(1), 'SOV')).toBe(0);
    expect(callUpCut(standing(2), 'SOV')).toBe(0);
    expect(callUpCut(standing(4), 'SOV')).toBeCloseTo(0.5, 9);
    expect(months(standing(1), 1)).toEqual([plan.after]);
    expect(callUpCut(createGame(2, null), 'SOV')).toBe(0);
    expect(callUpCut(standing(), 'GER')).toBe(0);
  });
});
