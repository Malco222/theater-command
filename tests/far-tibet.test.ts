// Tibet (rider b632 to ruling 234; historian/tibet-1937-45.md): the high plateau is a neutral's ground that no army
// enters or crosses.
//
// Two facts, carried by forms the game already had. The five provinces (Lhasa, Shigatse, Chamdo, Gartok, Changtang)
// are Tibet's, an owner's code with no row: the far stage's guard makes it a bare country at peace, independent, with
// no army (world.ts). And every border out of the five is on the map-maker's list of borders that are not there
// (far.mjs, PACIFIC_NO_BORDER), so no march, attack or supply line has a road across and the staffs see none. A war
// declared on Tibet leads nowhere, which is the record. Aircraft are not touched. With the far side's switch off
// there is no such ground and nothing here is read.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
// @ts-expect-error the map-maker's own file: plain JavaScript, with no types of its own
import { PACIFIC_NO_BORDER } from '../data/campaign/far.mjs';
import nationsData from '../data/campaign/nations.json';
import worldData from '../data/campaign/world.json';
import { afloat, airFleetsOf, kmTo, whyNotMission } from '../src/campaign/air';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { reach, runTurnAI } from '../src/campaign/turn';
import type { Game, Region } from '../src/campaign/types';
import { closedGround, farOn } from '../src/campaign/world';

const FIVE = ['Lhasa', 'Shigatse', 'Chamdo', 'Gartok', 'Changtang'];
const LINE = 'Tibet: neutral. No army can cross the high plateau.';
const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const five = (g: Game): Region[] => FIVE.map((nm) => g.byName[nm]);

describe('Tibet: a neutral whose ground no army enters (rider b632 to ruling 234)', () => {
  it("makes the five provinces Tibet's own, and Tibet a bare country at peace with no army", () => {
    const g = grand();
    expect(farOn(g)).toBe(true);
    for (const r of five(g)) { expect(r, 'on the chart').toBeDefined(); expect([r.name, r.owner, r.ctrl]).toEqual([r.name, 'TIB', 'TIB']); }
    expect(g.regions.filter((r) => r.owner === 'TIB').map((r) => r.name).sort()).toEqual([...FIVE].sort());
    // No row of its own anywhere: the guard's bare country, named.
    expect((nationsData as any).TIB ?? (nationsData as any).nations?.TIB ?? (worldData as any).nations.TIB).toBeUndefined();
    const T = g.nations.TIB;
    expect([T.name, T.adj, T.align, T.alive]).toEqual(['Tibet', 'Tibetan', 'independent', true]);
    expect(g.regions[T.capital].name).toBe('Lhasa');
    expect(g.byName.Lhasa.cap).toBe(true);
    expect(T.atWar).toEqual([]);
    expect(g.corps.filter((c) => c.nat === 'TIB' && !c.dead)).toEqual([]);
    expect(airFleetsOf(g, 'TIB')).toEqual([]);
    expect(g.order).toContain('TIB');
    // China has lost nothing else: Sikang, Sining and Khotan are still hers.
    for (const nm of ['Sikang', 'Sining', 'Khotan', 'Tsaidam', 'Chengtu']) expect(g.byName[nm].owner, nm).toBe('CHN');
  });

  it('has no border out of the five: the map-maker struck all of them, and nobody outside lists one', () => {
    const g = grand();
    const ids = new Set(five(g).map((r) => r.id));
    for (const r of five(g)) for (const j of r.nb) expect(ids.has(j), `${r.name} still borders ${g.regions[j].name}`).toBe(true);
    for (const r of g.regions) if (!ids.has(r.id)) for (const j of r.nb) expect(ids.has(j), `${r.name} still lists ${g.regions[j].name}`).toBe(false);
    // The five hang together, so the country is one piece.
    const seen = new Set([g.byName.Lhasa.id]), q = [g.byName.Lhasa.id];
    for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (!seen.has(j)) { seen.add(j); q.push(j); }
    expect([...seen].sort()).toEqual([...ids].sort());
    // The map-maker's list names the borders of the provinces it draws; Shigatse's go with Lhasa's (it is cut from it).
    const plateau = (PACIFIC_NO_BORDER as [string, string][]).filter(([a, b]) => FIVE.includes(a) !== FIVE.includes(b));
    expect(plateau.length).toBe(18);
    for (const [a, b] of plateau) { expect(g.byName[a], a).toBeDefined(); expect(g.byName[b], b).toBeDefined(); expect(g.byName[a].nb).not.toContain(g.byName[b].id); }
    // India, Burma, Nepal and China all stood against it and no longer do.
    for (const [a, b] of [['Darjeeling', 'Lhasa'], ['Kathmandu', 'Lhasa'], ['Sikang', 'Chamdo'], ['Sining', 'Chamdo'], ['Khotan', 'Gartok'], ['Leh', 'Gartok'], ['Myitkyina', 'Chamdo'], ['Tsaidam', 'Changtang']]) {
      expect(g.byName[a].nb, `${a}-${b}`).not.toContain(g.byName[b].id);
    }
  });

  it('gives no army a road in: a war declared on Tibet leads nowhere', () => {
    const g = grand('CHN');
    declareWar(g, 'CHN', 'TIB');
    expect(atWar(g, 'CHN', 'TIB')).toBe(true);
    const ids = new Set(five(g).map((r) => r.id));
    // A fresh corps of China's on each province that stood against the plateau reaches none of the five, to march or to attack.
    const c = g.corps.find((x) => x.nat === 'CHN' && !x.dead && x.kind === 'inf')!;
    for (const nm of ['Sikang', 'Sining', 'Chengtu', 'Khotan', 'Tsaidam']) {
      const at = g.byName[nm];
      const was = g.regions[c.reg];
      was.units = was.units.filter((u) => u !== c);
      at.units.push(c); c.reg = at.id; c.ap = 12;
      const m = reach(g, c);
      expect(m.size, nm).toBeGreaterThan(1);
      for (const id of m.keys()) expect(ids.has(id), `${nm} reaches ${g.regions[id].name}`).toBe(false);
    }
  });

  it('leaves the air as it was: the ranges are flown over by distance, as before', () => {
    const g = grand();
    const L = g.byName.Lhasa;
    declareWar(g, 'JAP', 'TIB');
    // A bomber group based within its range of Lhasa may be sent over it: the struck borders are an army's, not an airman's.
    const f = airFleetsOf(g, 'JAP').find((x) => x.bmr > 0 && !afloat(x))!;
    // (Myitkyina's field, 782 km from Lhasa on the globe: inside the 1,048 its bombers fly.)
    const from = g.byName.Myitkyina;
    from.ctrl = 'JAP'; f.base = from.id;
    expect(kmTo(g, f, L.id)).toBeGreaterThan(500);
    expect(kmTo(g, f, L.id)).toBeLessThan(1048);
    expect(whyNotMission(g, f, 'superiority', L.id) ?? '').not.toMatch(/out of range|beyond/);
    // And from farther off it is distance that stops it.
    f.base = g.byName.Tokyo.id;
    expect(whyNotMission(g, f, 'superiority', L.id) ?? '').toMatch(/out of range|beyond/);
  });

  it("says so on a province's page while it is Tibet's own, and nowhere else", () => {
    const g = grand();
    for (const r of five(g)) expect(closedGround(g, r), r.name).toBe(LINE);
    for (const nm of ['Sikang', 'Kathmandu', 'Darjeeling', 'Tokyo']) expect(closedGround(g, g.byName[nm]), nm).toBeNull();
    const lhasa = g.byName.Lhasa;
    lhasa.ctrl = 'CHN';
    expect(closedGround(g, lhasa)).toBeNull();
  });

  it('stays at peace with no army through half a year nobody plays, and nobody stands on its ground', () => {
    const g = grand(null, 22);
    for (let i = 0; i < 6; i++) runTurnAI(g);
    const T = g.nations.TIB;
    expect(T.alive).toBe(true);
    expect(T.atWar).toEqual([]);
    expect(g.corps.filter((c) => c.nat === 'TIB' && !c.dead)).toEqual([]);
    for (const r of five(g)) { expect([r.owner, r.ctrl], r.name).toEqual(['TIB', 'TIB']); expect(r.units.filter((u) => !u.dead), r.name).toEqual([]); }
  }, 180000);
});
