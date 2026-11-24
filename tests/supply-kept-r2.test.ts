// The supply pictures kept from one call to the next (the map-speed lane's month-end profile, 24 Nov 2026): with
// TC_SUPPLY_CHECK set, every kept picture is drawn again at every call and any difference stops the run. A grand
// campaign played by the computer for some months, a later start, and a capture mid-month must all run clean; and a
// change to what a picture is drawn from must give the fresh picture, not the kept one, even for a country that had no
// corps (and so was not looked at) in the month the change came.
import { describe, expect, it, vi } from 'vitest';

vi.hoisted(() => { (globalThis as { TC_SUPPLY_CHECK?: unknown }).TC_SUPPLY_CHECK = 1; });

import { EASTERN, capture, computeSupply, createGame, removeCorps } from '../src/campaign/game';
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
});
