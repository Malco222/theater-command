// The 1.5.1 patch (Brian, 24 November 2026: "you cant bomb germany from london with the US bombers").
// The rules always let bombers fly at any enemy ground in reach, seen or not. The page that lists a raid's targets
// stopped at the twelve nearest, which from London were all in occupied France and Belgium, so no German city could be
// picked. The page now lists every target in reach, by country where they lie in more than one.
import { describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { raidTargetsFor } from '../src/campaign/turn';
import { airPage } from '../src/campaign/airpages';
import { sightOf } from '../src/campaign/intel';
import type { Game } from '../src/campaign/types';

/** America at war with Germany, a bomber group of hers in London, and three provinces across the Channel in German hands. */
function overTheChannel(): { g: Game; bomber: number } {
  const g = createGame(1, 'USA');
  startOpen(g);
  declareWar(g, 'USA', 'GER');
  const f = g.airFleets.find((x) => x.nat === 'USA' && x.bmr > 0)!;
  f.base = g.regions.find((r) => r.name === 'London')!.id;
  for (const n of ['Calais', 'Lille', 'Brussels']) g.regions.find((r) => r.name === n)!.ctrl = 'GER';
  return { g, bomber: f.id };
}

const rowsOf = (p: NonNullable<ReturnType<typeof airPage>>) => p.sections.flatMap((s) => s.rows);

describe('the list of a raid\'s targets leaves nothing in reach off (the 1.5.1 patch)', () => {
  it('lists every target of a bombing raid, more than the twelve it used to stop at, Germany\'s unseen cities among them', () => {
    const { g, bomber } = overTheChannel();
    const all = raidTargetsFor(g, 'bomb', bomber);
    expect(all.length).toBeGreaterThan(12);
    const page = airPage(g, `raid:bomb:${bomber}`)!;
    const rows = rowsOf(page);
    expect(rows.length).toBe(all.length);
    expect(new Set(rows.map((r) => r.buttons[0].act.split(':')[3])).size).toBe(all.length);
    expect(page.headline).toBe(`${all.length} targets in reach, by country`);
    // The Ruhr is not in sight, and is on the list with the way to plan the raid.
    const ruhr = g.regions.find((r) => r.name === 'Ruhr')!;
    expect(sightOf(g, 'USA')[ruhr.id]).toBe(0);
    expect(rows.find((r) => r.title === 'Ruhr')!.buttons[0].act).toBe(`open:plan:bomb:${ruhr.id}:${bomber}`);
  });

  it('bombing by day or by night: the enemy\'s own country first, then the ground he holds, the nearest first inside each', () => {
    const { g, bomber } = overTheChannel();
    for (const kind of ['bomb', 'night'] as const) {
      const all = raidTargetsFor(g, kind, bomber);
      // (Calais is the nearest target of all, and stood first on the old list.)
      expect(all[0].name).toBe('Calais');
      const page = airPage(g, `raid:${kind}:${bomber}`)!;
      expect(page.sections.map((s) => s.heading.split('  ·  ')[0])).toEqual(['GERMANY', 'FRANCE, HELD BY GERMANY', 'BELGIUM, HELD BY GERMANY']);
      const home = all.filter((t) => g.regions[t.id].owner === 'GER');
      expect(page.sections[0].heading).toBe(`GERMANY  ·  ${home.length} TARGETS`);
      expect(page.sections[0].rows.map((r) => r.title)).toEqual(home.map((t) => t.name));
      expect(page.sections[1].rows.map((r) => r.title)).toEqual(['Calais', 'Lille']);
      expect(page.sections[2].heading).toBe('BELGIUM, HELD BY GERMANY  ·  1 TARGET');
    }
  });

  it('every other raid, and a reconnaissance: the nearest country first', () => {
    const { g, bomber } = overTheChannel();
    const sky = airPage(g, 'raid:superiority')!;
    expect(sky.sections[0].heading.startsWith('FRANCE, HELD BY GERMANY')).toBe(true);
    expect(rowsOf(sky).length).toBe(raidTargetsFor(g, 'superiority').length);
    const scout = airPage(g, `scout:${bomber}`)!;
    const seen = raidTargetsFor(g, 'recon', bomber);
    expect(seen.length).toBeGreaterThan(14);
    expect(rowsOf(scout).length).toBe(seen.length);
    expect(scout.headline).toBe(`${seen.length} provinces it could photograph, by country`);
    expect(scout.sections.every((s) => /PROVINCES?$/.test(s.heading))).toBe(true);
    expect(rowsOf(scout).map((r) => r.title)).toContain('Berlin');
  });

  it('targets in one country, at sea, or none at all are one list under the old heading', () => {
    const { g, bomber } = overTheChannel();
    const sea = airPage(g, 'raid:strike')!;
    expect(sea.sections.map((s) => s.heading)).toEqual(['TARGETS']);
    expect(sea.headline.endsWith('nearest first')).toBe(true);
    // Germany alone: one list, nearest first, as before.
    for (const n of ['Calais', 'Lille', 'Brussels']) { const r = g.regions.find((x) => x.name === n)!; r.ctrl = r.owner; }
    const one = airPage(g, `raid:bomb:${bomber}`)!;
    expect(one.sections.map((s) => s.heading)).toEqual(['TARGETS']);
    expect(one.sections[0].rows.map((r) => r.title)).toEqual(raidTargetsFor(g, 'bomb', bomber).map((t) => t.name));
    expect(one.headline).toBe(`${one.sections[0].rows.length} targets in reach, nearest first`);
    // Nothing in reach: the one section says what to do about it.
    const empty = (['fleet', 'line', 'town', 'harbour'] as const).filter((k) => raidTargetsFor(g, k).length === 0);
    expect(empty.length).toBeGreaterThan(0);
    for (const k of empty) {
      const none = airPage(g, `raid:${k}`)!;
      expect(none.headline).toBe('No target is in reach');
      expect(none.sections.length).toBe(1);
      expect(none.sections[0].heading).toBe('TARGETS');
      expect(none.sections[0].rows).toEqual([]);
      expect(none.sections[0].text.length).toBeGreaterThan(20);
    }
  });
});
