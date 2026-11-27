import { describe, it, expect } from 'vitest';
import { AIRCRAFT, climbRate, levelSpeed, sustainedTurn, stepFlight, navControl, type FlightState } from '../src/air/flight';
import rawAircraft from '../data/aircraft.json';

describe('flight model reproduces documented performance', () => {
  for (const t of Object.values(AIRCRAFT)) {
    it(`${t.name}: top speed and climb come out of the physics`, () => {
      const d = (rawAircraft as any)[t.key];
      const vmax = levelSpeed(t, d.vmaxAltM);
      expect(Math.abs(vmax - d.vmaxMs) / d.vmaxMs).toBeLessThan(0.02);
      const climb = climbRate(t, 0);
      // Climb is never entered as data. It has to land near the documented figure on its own.
      // Fighters are held tight. Bomber figures vary a lot with load, so they get a wider band.
      const band = t.role === 'bomber' ? 0.5 : 0.2;
      expect(Math.abs(climb - t.refClimbSl) / t.refClimbSl).toBeLessThan(band);
    });
  }

  it('the Spitfire and Hurricane out-turn the Bf 109 E, as the 1940 trials found', () => {
    const spit = sustainedTurn(AIRCRAFT.spitfire_mk1, 3600);
    const hurri = sustainedTurn(AIRCRAFT.hurricane_mk1, 3600);
    const bf = sustainedTurn(AIRCRAFT.bf109e4, 3600);
    expect(spit.rateDeg).toBeGreaterThan(bf.rateDeg);
    expect(hurri.rateDeg).toBeGreaterThan(bf.rateDeg);
    expect(spit.radius).toBeLessThan(bf.radius);
  });

  it('a Spitfire climbs to 20,000 ft in a believable time', () => {
    const t = AIRCRAFT.spitfire_mk1;
    const a: FlightState = { type: t, x: 0, y: 0, z: 50, v: 70, hdg: 0, gam: 0.1, bank: 0, thr: 1, cmdHdg: 0, cmdGam: 0.1, cmdG: 2.5, powerMul: 1, dragMul: 1, cutT: 0, n: 1 };
    let time = 0;
    while (a.z < 6096 && time < 1200) { navControl(a, 0, 6500, t.cruise, 2.5); stepFlight(a, 0.1); time += 0.1; }
    // Period figures put the Mk I at roughly 7.5 to 9.5 minutes to 20,000 ft.
    expect(time / 60).toBeGreaterThan(6);
    expect(time / 60).toBeLessThan(10.5);
  });
});
