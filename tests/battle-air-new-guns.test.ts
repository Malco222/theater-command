// The four models of 23 Nov fire from their own gunners' sockets (the 3D thread's reading of the files, 08:41Z): the
// Wellington's nose and tail turrets and beam windows, the Pe-2's fixed nose guns and its two gunners facing aft, the
// LeO 451's gunners above and below facing aft, and each of the D.520's four wing guns at its own muzzle.
import { describe, expect, it } from 'vitest';
import { bomberGuns, fighterGuns } from '../src/battle/air/guns';
import { bomberOf } from '../src/battle/air/doctrine';

const at = (L: ReturnType<typeof bomberGuns>) => L.guns.map((g) => `${g.mount}:${g.at.join('+')}:${g.n}`);

describe('the new models fire from their own sockets', () => {
  it('the Wellington: nose turret.1/.2, tail turret.3/.4, beams turret.5/.6; the Mk III four in the tail', () => {
    const ic = bomberGuns('Wellington', 'UK', { year: 1940, month: 6 });
    expect(ic.mark).toBe('Wellington IC');
    expect(at(ic)).toEqual(['turret:turret.1+turret.2:2', 'turret:turret.3+turret.4:2', 'hand:turret.5:1', 'hand:turret.6:1']);
    const iii = bomberGuns('Wellington', 'AUS', { year: 1941, month: 8 });
    expect(iii.mark).toBe('Wellington III');
    expect(iii.guns[1].n).toBe(4);
    // The tail gunners face aft, the nose forward, starboard beam to starboard.
    expect(ic.guns[1].arc!.yaw).toEqual([85, 275]);
    expect(ic.guns[2].arc!.yaw).toEqual([30, 150]);
  });

  it('the Pe-2: fixed guns at gun.1/.2, the navigator above aft at turret.1, the gunner below aft at turret.2', () => {
    for (const month of [6, 11]) {
      const L = bomberGuns('Pe2', 'SOV', { year: 1942, month });
      expect(L.guns.filter((g) => g.mount === 'fixed').flatMap((g) => g.at)).toEqual(['gun.1', 'gun.2']);
      const [up, down] = L.guns.filter((g) => g.mount === 'hand');
      expect([up.at, down.at]).toEqual([['turret.1'], ['turret.2']]);
      expect(up.arc!.pitch[0]).toBeGreaterThanOrEqual(0);
      expect(down.arc!.pitch[1]).toBeLessThanOrEqual(0);
    }
    expect(bomberGuns('Pe2', 'SOV', { year: 1943, month: 0 }).guns.some((g) => g.kind === 'hmg')).toBe(true);
  });

  it('the LeO 451: the cannon above at turret.1, a gun below at turret.2, the nose gun without a muzzle', () => {
    const L = bomberGuns('LeO451', 'FRA', { year: 1940, month: 4 });
    expect(L.mark).toBe('LeO 451');
    expect(L.guns.map((g) => [g.gun, g.at])).toEqual([['HS.404', ['turret.1']], ['MAC 1934', ['turret.2']], ['MAC 1934', []]]);
  });

  it('the D.520: the hub cannon and four wing guns at gun.1 to gun.4; the plain French fighter keeps two muzzles', () => {
    const L = fighterGuns('D520', 'FRA', { year: 1940, month: 5 });
    expect(L.guns.map((g) => g.at)).toEqual([['gun.nose'], ['gun.1', 'gun.2', 'gun.3', 'gun.4']]);
    expect(L.guns[1].n).toBe(4);
    expect(fighterGuns('Fighter:FRA', 'FRA', { year: 1940, month: 5 }).guns[1].at).toEqual(['gun.1', 'gun.2']);
  });

  it('the SM.79, the Nell and the Betty fire from their own sockets; Italy flies the SM.79 from the first', () => {
    expect(bomberOf('ITA', 1939)).toBe('SM79');
    const sm = bomberGuns('SM79', 'ITA', { year: 1941, month: 3 });
    expect(sm.mark).toBe('SM.79');
    expect(sm.guns.map((g) => `${g.mount}:${g.at.join('+')}`)).toEqual(['fixed:turret.1', 'hand:turret.2', 'hand:turret.3', 'hand:turret.4', 'hand:turret.5']);
    const nell = bomberGuns('Nell', 'JAP', { year: 1940, month: 6 });
    expect(nell.guns.flatMap((g) => g.at).sort()).toEqual(['turret.1', 'turret.2', 'turret.3', 'turret.4']);
    const g1 = bomberGuns('Betty', 'JAP', { year: 1942, month: 0 }), g2 = bomberGuns('Betty', 'JAP', { year: 1944, month: 0 });
    expect([g1.mark, g2.mark]).toEqual(['G4M1', 'G4M2']);
    expect(g1.guns.map((g) => g.at[0])).toEqual(['turret.1', 'turret.2', 'turret.3', 'turret.4', 'turret.5']);
    expect(g1.guns[4].gun).toBe('Type 99-1');
    expect(g2.guns.filter((g) => g.kind === 'cannon20').length).toBe(4);
  });

  it('the Pe-2 is drawn from 1941: a light bomber, with no heavy bomber research needed', () => {
    expect([bomberOf('SOV', 1940), bomberOf('SOV', 1941)]).toEqual(['Bomber:SOV', 'Pe2']);
  });
});
