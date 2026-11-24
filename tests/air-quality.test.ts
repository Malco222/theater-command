// The stage switch 'airQuality' (the design director, 22 Nov 2026, 06:26Z): an air force's quality from the record where
// air.json's default sells it short. Off as shipped; a harness turns it on (TC_AIRQ=1).
import { afterEach, describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import { AIR_QUALITY, assessAir } from '../src/campaign/air';
import { saveGame } from '../src/campaign/save';
import type { AirFleet, Game } from '../src/campaign/types';

afterEach(() => { AIR_QUALITY().on = false; });

/** A Finnish fighter group beside the Soviet border, at war with the Soviet Union, and its fight for the sky over Leningrad. */
function winter(): { g: Game; fin: AirFleet; target: number } {
  const g = createGame(4, null);
  declareWar(g, 'SOV', 'FIN');
  const lenin = g.byName['Leningrad'];
  const near = g.regions.filter((r) => r.ctrl === 'FIN').sort((a, b) => Math.hypot(a.x - lenin.x, a.y - lenin.y) - Math.hypot(b.x - lenin.x, b.y - lenin.y))[0];
  const pol = g.airFleets.find((f) => f.nat === 'POL' && f.ftr > 0)!;
  const fin: AirFleet = { ...pol, id: 9999, nat: 'FIN', name: 'Finnish fighters', base: near.id, mission: 'support', leader: null } as AirFleet;
  g.airFleets.push(fin);
  return { g, fin, target: lenin.id };
}

describe('the air quality switch', () => {
  it('holds one row of facts for Finland, with its source, and is off as shipped', () => {
    const Q = AIR_QUALITY();
    expect(Q.on).toBe(false);
    expect(Object.keys(Q.rows)).toEqual(['FIN']);
    expect(Q.rows.FIN.quality).toBe(1.0);
    expect(Q.rows.FIN.facts).toBe('Winter War about 190-200 claimed for 26-30 fighters lost; 1941-44 about 1,600 claimed for 200-250 lost');
    expect(Q.rows.FIN.source).toBe('historian/fighter-types-signoff-1122.md s4');
  });

  it('on, Finland\'s fighters count at 1.0 in place of the 0.7 default; nobody else moves; off, nothing changes', () => {
    const { g, fin, target } = winter();
    const a0 = assessAir(g, [fin], 'superiority', target);
    expect(a0.why).toBeNull();
    expect(a0.Epow).toBeGreaterThan(0);
    expect(a0.Dpow).toBeGreaterThan(0);
    const before = saveGame(g);
    AIR_QUALITY().on = true;
    const a1 = assessAir(g, [fin], 'superiority', target);
    expect(a1.Epow / a0.Epow).toBeCloseTo(1.0 / 0.7, 10);
    expect(a1.Dpow).toBe(a0.Dpow);
    AIR_QUALITY().on = false;
    const a2 = assessAir(g, [fin], 'superiority', target);
    expect(a2.Epow).toBe(a0.Epow);
    expect(saveGame(g)).toBe(before);
  });
});
