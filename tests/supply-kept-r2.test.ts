// The supply pictures kept from one call to the next (the map-speed lane's month-end profile, 24 Nov 2026): with
// TC_SUPPLY_CHECK set, every kept picture is drawn again at every call and any difference stops the run. A grand
// campaign played by the computer for some months, a later start, and a capture mid-month must all run clean; and a
// change to what a picture is drawn from must give the fresh picture, not the kept one, even for a country that had no
// corps (and so was not looked at) in the month the change came. The far chart's profile (24 Nov 2026) keeps more: the
// roads by holders and friends, and every nation's sea lanes (TC_SUPPLY_CHECK walks the seas afresh too): a friend made
// with no province changing hands, and wars, fleets, the sky and captures stirred up between calls, must run clean.
import { describe, expect, it, vi } from 'vitest';

vi.hoisted(() => { (globalThis as { TC_SUPPLY_CHECK?: unknown }).TC_SUPPLY_CHECK = 1; });

import { EASTERN, capture, computeSupply, createGame, removeCorps, setWar } from '../src/campaign/game';
import { openSeas } from '../src/campaign/navy';
import { garrisonOf } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';

describe('kept supply pictures', () => {
  it('a 1939 grand campaign: every kept picture equals a fresh one for eight months', () => {
    const g = startOpen(createGame(2, null));
    for (let t = 0; t < 8; t++) runTurnAI(g);
    expect(g.turn).toBe(9);
  }, 300_000);

  it('a 1941 start, into the east and the desert: every kept picture equals a fresh one for six months', () => {
    const g = createGame(4, null);
    for (let t = 0; t < 21; t++) runTurnAI(g);
    startOpen(g);
    for (let t = 0; t < 6; t++) runTurnAI(g);
    expect(Object.values(g.reach).some((r) => r.some((x) => x > 0))).toBe(true);
  }, 300_000);

  it('a province that changes hands between two calls gives the fresh picture', () => {
    const g = startOpen(createGame(3, null));
    computeSupply(g);
    const before = g.supply.POL;
    const r = g.byName['Poznan'];
    expect(before[r.id]).toBeGreaterThan(0);
    for (const u of [...r.units, ...garrisonOf(r)]) removeCorps(g, u);
    capture(g, r, 'GER');
    computeSupply(g);
    expect(g.supply.POL).not.toBe(before);
    expect(g.supply.POL[r.id]).toBe(0);
    computeSupply(g);
    expect(g.supply.POL[r.id]).toBe(0);
  }, 120_000);

  it('a country with no corps while the railway it reaches by is relaid draws its picture afresh when it has corps again', () => {
    const g = startOpen(createGame(3, null));
    computeSupply(g);
    // Poland takes a Soviet province on its border: its picture now reaches into the east, one fresh step deep.
    const r = g.regions.find((x) => x.owner === 'SOV' && x.ctrl === 'SOV' && x.nb.some((j) => g.regions[j].ctrl === 'POL' && g.supply.POL[j] > 0))!;
    expect(r).toBeDefined();
    for (const u of [...r.units, ...garrisonOf(r)]) removeCorps(g, u);
    capture(g, r, 'POL');
    computeSupply(g);
    const before = g.reach.POL[r.id];
    expect(before).toBeGreaterThan(0);
    // Poland's corps leave the roster for a while (lost, or at sea), and meanwhile the track there is relaid.
    const mine = g.corps.filter((c) => c.nat === 'POL');
    expect(mine.length).toBeGreaterThan(0);
    for (const c of mine) g.corps.splice(g.corps.indexOf(c), 1);
    computeSupply(g);
    r.since -= EASTERN.convertAfter;
    computeSupply(g);
    // They are back: the line there is the relaid one, and TC_SUPPLY_CHECK would have stopped on the old picture.
    g.corps.push(...mine);
    computeSupply(g);
    expect(g.reach.POL[r.id]).toBeCloseTo(before * EASTERN.stepConverted / EASTERN.stepFresh, 6);
  }, 120_000);

  it('a friend made between two calls, with no province changing hands, gives the fresh picture', () => {
    const g = startOpen(createGame(3, null));
    computeSupply(g);
    // A Hungarian province beside ground Germany's supply reaches: Hungary is no ally of Germany's yet.
    const r = g.regions.find((x) => x.ctrl === 'HUN' && x.nb.some((j) => g.supply.GER[j] > 0))!;
    expect(r).toBeDefined();
    expect(g.supply.GER[r.id]).toBe(0);
    g.nations.HUN.align = 'axis';
    computeSupply(g);
    expect(g.supply.GER[r.id]).toBeGreaterThan(0);
    g.nations.HUN.align = 'leaning_axis';
    computeSupply(g);
    expect(g.supply.GER[r.id]).toBe(0);
  }, 120_000);

  it('a war declared between two calls, with nothing at sea moving, gives the fresh sea lanes', () => {
    const g = startOpen(createGame(3, null));
    // Italy at war already (with Greece), so that its coasts put aircraft over the seas before as after: only who is
    // whose enemy changes between the two calls, and no sea's state.
    setWar(g, 'ITA', 'GRE');
    computeSupply(g);
    const before = openSeas(g, 'UK').join('');
    // Italy's fleets and aircraft in the Mediterranean are now the enemy's: some seas close to British ships.
    setWar(g, 'ITA', 'UK');
    setWar(g, 'ITA', 'FRA');
    expect(openSeas(g, 'UK').join('')).not.toBe(before);
    // TC_SUPPLY_CHECK: the kept lanes are walked again and compared.
    computeSupply(g);
  }, 120_000);

  it('the sea lanes and the roads home kept from call to call follow fleets, the sky, wars and captures', () => {
    /** A small steady stream of numbers, so the stirring is the same every run. */
    const stream = (seed: number) => () => ((seed = (Math.imul(seed, 1103515245) + 12345) >>> 0) / 2 ** 32);
    for (const seed of [3, 11]) {
      const g = startOpen(createGame(seed, null));
      const rnd = stream(seed), pick = <T>(xs: T[]): T => xs[Math.floor(rnd() * xs.length)];
      const nats = Object.keys(g.nations).filter((n) => g.nations[n].alive);
      const ships = nats.filter((n) => g.nations[n].seaSupply || g.nations[n].sealift > 0);
      const lanes = (): string => ships.map((n) => openSeas(g, n).join('')).join('/');
      computeSupply(g);
      let was = lanes();
      const moved = [0, 0];
      for (let k = 0; k < 60; k++) {
        // First the seas alone (fleets put to sea, some very strong; the sky won over coasts; air forces of every
        // size), then wars and captures as well.
        const x = rnd() * (k < 30 ? 0.7 : 1);
        if (x < 0.4) { const f = pick(g.fleets); f.zone = Math.floor(rnd() * g.seas.length); f.stance = rnd() < 0.8 ? 'patrol' : 'port'; if (rnd() < 0.3) f.ships = { ...f.ships, bb: f.ships.bb + 30 }; }
        else if (x < 0.55) { const z = pick(g.seas); if (z.coast.length) g.sky[pick(z.coast)] = { nat: pick(nats), turn: g.turn }; }
        else if (x < 0.7) g.nations[pick(nats)].air = Math.floor(rnd() * 400);
        else if (x < 0.85) { const a = pick(nats), b = pick(nats); if (a !== b) setWar(g, a, b); }
        else {
          const r = pick(g.regions.filter((y) => y.port));
          for (const u of [...r.units, ...garrisonOf(r)]) removeCorps(g, u);
          capture(g, r, pick(nats));
        }
        // TC_SUPPLY_CHECK: every kept picture is drawn again and every kept lane walked again; any difference throws.
        computeSupply(g);
        const now = lanes();
        if (now !== was) moved[k < 30 ? 0 : 1]++;
        was = now;
      }
      // The stirring does change the lanes, in both halves: the kept ones were put to the test.
      expect(moved[0]).toBeGreaterThan(0);
      expect(moved[1]).toBeGreaterThan(0);
    }
  }, 300_000);
});
