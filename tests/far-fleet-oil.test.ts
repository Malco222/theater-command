// A fleet based beside the oil (ruling 194, point (g); the Pacific planner's paper 03, section 5).
//
// In 1944 Japan's battle line lay near Singapore and at Tawi-Tawi, and sailed for Leyte from Brunei, because the oil
// was there and the tankers to bring it home were not. The game keeps one stock for the nation, so nothing gave a
// fleet that reason. A fleet whose base is a harbour on a sea of the far side that touches a working oilfield overseas
// its side holds is not short of fuel while the nation's stock is dry. Its need stays in the nation's book. Each
// state is set by hand on the first day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { besideOil, fit, fleetFuel } from '../src/campaign/navy';
import { RES, fieldOverseas, fireField, fuelFleet, needOf, storesOf } from '../src/campaign/resources';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game } from '../src/campaign/types';
import { FAR_SEAS_FIRST } from '../src/campaign/world';

const grand = (): Game => startOpen(createGame(3, null));
const fleet = (g: Game, nat = 'JAP'): Fleet => g.fleets.filter((f) => f.nat === nat && f.base >= 0).sort((a, b) => b.ships.bb - a.ships.bb)[0];
const dry = (g: Game, nat = 'JAP'): void => { storesOf(g, nat)!.oilMet = 0; };
const base = (g: Game, f: Fleet, name: string): void => { f.base = g.byName[name].id; };

describe('a fleet based beside the oil is not short of fuel (ruling 194(g))', () => {
  it('a dry Japan\'s fleet at home steams at six tenths; based at Brunei beside its oil, it steams in full', () => {
    const g = grand();
    const f = fleet(g);
    expect(g.regions[f.base].owner).toBe('JAP');
    expect([fuelFleet(g, 'JAP'), fleetFuel(g, f), besideOil(g, f)]).toEqual([1, 1, false]);
    dry(g);
    expect(fuelFleet(g, 'JAP')).toBeCloseTo(1 - RES.oil.short.fleet, 12);
    expect(fleetFuel(g, f)).toBeCloseTo(0.6, 12);
    expect(fit(g, f)).toBeCloseTo((0.5 + 0.5 * f.ready) * 0.6, 12);
    // Brunei is Japan's, and the fleet lies there.
    const need = needOf(g, 'JAP', 'oil');
    g.byName['Brunei'].ctrl = 'JAP';
    expect(besideOil(g, f)).toBe(false);
    base(g, f, 'Brunei');
    expect(besideOil(g, f)).toBe(true);
    expect(fleetFuel(g, f)).toBe(1);
    expect(fit(g, f)).toBe(0.5 + 0.5 * f.ready);
    // Its need stays in the nation's book, and the rest of the navy is as dry as it was.
    expect(needOf(g, 'JAP', 'oil')).toBe(need);
    const home = g.fleets.find((x) => x.nat === 'JAP' && x !== f && x.base >= 0 && g.regions[x.base].owner === 'JAP')!;
    expect(fleetFuel(g, home)).toBeCloseTo(0.6, 12);
  });

  it('the harbour need only stand on a sea the field touches: Singapore beside Palembang', () => {
    const g = grand();
    const f = fleet(g);
    dry(g);
    for (const nm of ['Singapore', 'Palembang']) g.byName[nm].ctrl = 'JAP';
    base(g, f, 'Singapore');
    const shared = g.byName['Singapore'].seas.filter((z) => g.byName['Palembang'].seas.includes(z));
    expect(shared.length).toBeGreaterThan(0);
    for (const z of shared) expect(z).toBeGreaterThanOrEqual(FAR_SEAS_FIRST);
    expect(besideOil(g, f)).toBe(true);
    expect(fleetFuel(g, f)).toBe(1);
  });

  it('the field must be working and its side\'s: not one alight from end to end, not one an enemy holds', () => {
    const g = grand();
    const f = fleet(g);
    dry(g);
    g.byName['Brunei'].ctrl = 'JAP';
    base(g, f, 'Brunei');
    expect(besideOil(g, f)).toBe(true);
    expect(fireField(g, g.byName['Brunei'], 6, 1)).toBe(true);
    expect(besideOil(g, f)).toBe(false);
    expect(fleetFuel(g, f)).toBeCloseTo(0.6, 12);
    // Half alight, it still gives oil.
    g.flags[`oilcut:${g.byName['Brunei'].id}`] = 50;
    expect(besideOil(g, f)).toBe(true);
    g.flags[`oilout:${g.byName['Brunei'].id}`] = 0;
    // Britain takes it back, at war with Japan.
    if (!atWar(g, 'JAP', 'UK')) declareWar(g, 'JAP', 'UK');
    g.byName['Brunei'].ctrl = 'UK';
    expect(besideOil(g, f)).toBe(false);
    expect(fleetFuel(g, f)).toBeCloseTo(0.6, 12);
  });

  it('an oilfield at home is in the nation\'s own stock: a harbour beside Akita\'s wells is as dry as the rest of Japan', () => {
    const g = grand();
    const f = fleet(g);
    dry(g);
    expect([g.byName['Akita'].oil, g.byName['Akita'].port, fieldOverseas(g.byName['Akita'])]).toEqual([true, true, false]);
    base(g, f, 'Akita');
    expect(besideOil(g, f)).toBe(false);
    expect(fleetFuel(g, f)).toBeCloseTo(0.6, 12);
  });

  it('it is read in the far side\'s seas alone, and for every navy there: no fleet of Europe\'s moves', () => {
    const g = grand();
    for (const nat of ['GER', 'UK', 'FRA', 'ITA', 'SOV', 'USA']) dry(g, nat);
    // On the first day two fleets lie beside an oilfield overseas, both in the far side's own harbours: Britain's at
    // Singapore (Brunei's field stands on the same sea) and the Dutch at Soerabaja.
    const beside = g.fleets.filter((f) => besideOil(g, f)).map((f) => `${f.nat} ${f.name} at ${g.regions[f.base].name}`);
    expect(beside).toEqual(['UK China Fleet at Singapore', 'NED Dutch East Indies Fleet at Soerabaja']);
    for (const f of g.fleets) if (besideOil(g, f)) expect(g.regions[f.base].seas.some((z) => z >= FAR_SEAS_FIRST), f.name).toBe(true);
    // A harbour of the old chart stands on no sea of the far side, whatever oil is near it.
    const u = fleet(g, 'UK');
    expect(g.regions[u.base].seas.every((z) => z < FAR_SEAS_FIRST)).toBe(true);
    expect(fleetFuel(g, u)).toBeCloseTo(0.6, 12);
    // A British fleet at Brunei, Britain's own field: the rule is every navy's.
    base(g, u, 'Brunei');
    expect(g.byName['Brunei'].ctrl).toBe('UK');
    expect(besideOil(g, u)).toBe(true);
    expect(fleetFuel(g, u)).toBe(1);
  });
});
