// A main effort leaves no city or port empty next to the enemy, beyond the old chart (the design director's ruling
// of 16:52Z of 19 November 2026 on Canton, from rider b642's China trace to ruling 234).
//
// With no book, Japan's free staff opens its offensive against China in September 1939. The main effort sent
// Canton's one corps at Wuchow next door; it won and went in, and China walked into Canton behind it. Japan held its
// occupied cities through the war and its offensives went out from them: one corps stays in a city or port its
// country holds next to ground an enemy holds, while the others go forward. Read at the moment a corps would leave.
// The far side's alone; the old chart's staffs are as they were, and a month on the defensive is not bound.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { atkPower, cityBound, effortKeeps, effortSends } from '../src/campaign/ai';
import { addCorps, atWar, createGame, friendlyUnits, setWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { arrive, garrison, garrisonOf, townHeld } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { current, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';
import { beyondEurope, farOn } from '../src/campaign/world';

const wide = (seed = 1, human: string | null = null): Game => startOpen(createGame(seed, human));
const at = (g: Game, nm: string): Region => g.byName[nm];
const japans = (g: Game, nm: string): Corps[] => at(g, nm).units.filter((u) => u.nat === 'JAP' && !u.dead);

describe('a main effort leaves one corps in a city or port next to the enemy (the director on Canton, 16:52Z)', () => {
  it('the places bound: a city or a port the country holds, beyond the old chart, next to ground an enemy holds', () => {
    const g = wide();
    expect(farOn(g)).toBe(true);
    // Canton (a city and a port) and Nanchang (a city) face China's line.
    for (const nm of ['Canton', 'Nanchang', 'Hankow', 'Hangchow']) expect(cityBound(g, 'JAP', at(g, nm)), nm).toBe(true);
    // A city behind the line is not bound, nor a province on the line that is neither city nor port.
    const taiyuan = at(g, 'Taiyuan'), sinyang = at(g, 'Sinyang');
    expect([taiyuan.vp > 0, taiyuan.nb.some((j) => atWar(g, 'JAP', g.regions[j].ctrl)), cityBound(g, 'JAP', taiyuan)]).toEqual([true, false, false]);
    expect([sinyang.vp, sinyang.port, sinyang.nb.some((j) => atWar(g, 'JAP', g.regions[j].ctrl)), cityBound(g, 'JAP', sinyang)]).toEqual([0, false, true, false]);
    // It is the holder's own to keep: nobody else's, and nobody's at peace.
    expect(cityBound(g, 'MAN', at(g, 'Canton'))).toBe(false);
    expect(cityBound(g, 'USA', at(g, 'Manila'))).toBe(false);
    // Nothing on the old chart is bound, whoever is at war across the frontier.
    setWar(g, 'GER', 'POL');
    for (const r of g.regions) if (!beyondEurope(r)) for (const n of ['GER', 'POL', 'UK', 'FRA']) expect(cityBound(g, n, r), `${n} ${r.name}`).toBe(false);
  });

  it("the last of its side in such a place does not go, and with company it may", () => {
    const g = wide();
    const army = japans(g, 'Canton')[0];
    expect([army.name, friendlyUnits(g, at(g, 'Canton'), 'JAP').length]).toEqual(['21st Army (Canton)', 1]);
    expect(effortKeeps(g, army)).toBe(true);
    expect(effortSends(g, 'JAP', [army])).toEqual([]);
    const second = addCorps(g, 'JAP', 'Test brigade at Canton', 'inf', 1, 'Canton', {});
    expect([effortKeeps(g, army), effortKeeps(g, second)]).toEqual([false, false]);
    // A corps that is not in a bound place is never kept: behind the line, or on the line in open country.
    for (const nm of ['Taiyuan', 'Sinyang']) for (const c of japans(g, nm)) expect(effortKeeps(g, c), c.name).toBe(false);
  });

  it('where everybody in the place could go at a province, one stays: the weakest that fights on foot', () => {
    const g = wide();
    // (The 33rd Division stands at Nanchang on the first day, where the record held it in Kiangsi: china-setup.json,
    // `_kiangsu`, the record's rows of 21 November 2026. It is set at Hankow by hand here, so that the place holds two
    // corps on foot and one of tanks, as this case was written.)
    const div = g.corps.find((c) => c.name === '11th Army (33rd Division)')!, was = g.regions[div.reg];
    expect(was.name).toBe('Nanchang');
    was.units.splice(was.units.indexOf(div), 1); at(g, 'Hankow').units.push(div); div.reg = at(g, 'Hankow').id;
    const hankow = japans(g, 'Hankow');
    expect(hankow.map((c) => c.name).sort()).toEqual(['11th Army (33rd Division)', '11th Army (Wuhan)', '13th Tank Regiment']);
    const went = effortSends(g, 'JAP', hankow);
    expect(went).toHaveLength(2);
    const stayed = hankow.find((c) => !went.includes(c))!;
    const foot = hankow.filter((c) => c.kind === 'inf');
    expect(stayed).toBe([...foot].sort((a, b) => atkPower(g, a) - atkPower(g, b))[0]);
    // With one of them not on the list (it has no time left, or is held), the others may all go.
    expect(effortSends(g, 'JAP', hankow.slice(0, 2))).toEqual(hankow.slice(0, 2));
    // Places are counted one by one, and a corps from open country is never kept back.
    const open = japans(g, 'Sinyang');
    const both = effortSends(g, 'JAP', [...hankow, ...open, ...japans(g, 'Canton')]);
    expect(both).toHaveLength(2 + open.length);
    for (const c of open) expect(both).toContain(c);
  });

  // (The design director, 23:18Z of 20 November 2026, item 2, on the trace of ruling 216's six tests. By the book
  // Canton's town holds the 21st Army's row and the 38th Division stands in the field: the rule read the field
  // alone, called the division Canton's last and kept it there.)
  it("a place whose town holds a garrison row of its side is not empty: Canton's town row stands, the field corps may go, and Canton is not open to a walk-in", () => {
    const g = wide();
    const canton = at(g, 'Canton'), army = japans(g, 'Canton')[0];
    // Nobody in the town: the one corps in the field is Canton's last, as before.
    expect([garrisonOf(canton).length, effortKeeps(g, army)]).toEqual([0, true]);
    // A row of its own goes into the town. The field corps is no longer the last of its side there.
    const row = addCorps(g, 'JAP', 'Test garrison of Canton', 'inf', 1, 'Canton', {});
    expect(garrison(g, row)).toBe(true);
    expect([garrisonOf(canton).includes(row), friendlyUnits(g, canton, 'JAP').length]).toEqual([true, 1]);
    expect(effortKeeps(g, army)).toBe(false);
    expect(effortSends(g, 'JAP', [army])).toEqual([army]);
    // The field corps gone, an enemy who arrives finds the town held: he must shut it in, and Canton does not fall to a walk-in.
    canton.units.splice(canton.units.indexOf(army), 1);
    const tungkun = at(g, 'Tungkun');
    army.reg = tungkun.id; tungkun.units.push(army);
    expect([canton.units.length, townHeld(g, canton)]).toEqual([0, true]);
    arrive(g, canton, 'CHN');
    expect([canton.ctrl, garrisonOf(canton).includes(row), row.dead ?? false]).toEqual(['JAP', true, false]);
    // With the town's row gone the place is empty again, and the same arrival walks in.
    const g2 = wide();
    const canton2 = at(g2, 'Canton'), army2 = japans(g2, 'Canton')[0];
    canton2.units.splice(canton2.units.indexOf(army2), 1);
    army2.reg = at(g2, 'Tungkun').id; at(g2, 'Tungkun').units.push(army2);
    expect(townHeld(g2, canton2)).toBe(false);
    arrive(g2, canton2, 'CHN');
    expect(canton2.ctrl).toBe('CHN');
  });

  it('by the book the 38th Division is not Canton\'s last while the 21st Army\'s row holds the town (stop 1, as November 1939 opens)', () => {
    const g = wide(7);
    setStop(g, 1, true);
    while (g.turn < 3) stepAI(g);
    const canton = at(g, 'Canton');
    // (The record's rows of 22 November 2026, china-setup.json `_canton`: the 104th Division's fourth regiment is a
    // row of its own at Canton from the first month. A staff garrisons with its weakest row on foot, so the town's row
    // is the regiment's, and the 104th stands in the field beside the 38th. The case is as it was: neither is Canton's
    // last while a row of the 21st Army holds the town.)
    expect(garrisonOf(canton).map((c) => c.name)).toEqual(['21st Army (170th Infantry Regiment, of the 104th Division)']);
    const field = japans(g, 'Canton');
    expect(field.map((c) => c.name).sort()).toEqual(['21st Army (Canton)', '38th Division (Canton)']);
    expect(cityBound(g, 'JAP', canton)).toBe(true);
    for (const c of field) expect(effortKeeps(g, c), c.name).toBe(false);
    expect(effortSends(g, 'JAP', field)).toEqual(field);
  }, 120000);

  it('nobody is kept on the old chart', () => {
    const g = wide();
    setWar(g, 'GER', 'POL');
    const list = g.corps.filter((c) => c.nat === 'GER' && !c.dead && !beyondEurope(g.regions[c.reg]));
    expect(list.some((c) => g.regions[c.reg].vp > 0 && friendlyUnits(g, g.regions[c.reg], 'GER').length === 1)).toBe(true);
    expect(effortSends(g, 'GER', list)).toBe(list);
    for (const c of list) expect(effortKeeps(g, c), c.name).toBe(false);
  });

  it("Canton and Nanchang are held through Japan's September and October 1939, in three wars", () => {
    for (const seed of [1, 17, 22]) {
      const g = wide(seed);
      for (const month of [1, 2]) {
        while (current(g) !== 'JAP' || g.turn < month) stepAI(g);
        expect(g.turn, `seed ${seed}`).toBe(month);
        stepAI(g);
        for (const nm of ['Canton', 'Nanchang']) expect([at(g, nm).ctrl, japans(g, nm).length > 0], `seed ${seed}, month ${month}, ${nm}`).toEqual(['JAP', true]);
      }
      while (g.turn < 3) stepAI(g);
      for (const nm of ['Canton', 'Nanchang']) expect(at(g, nm).ctrl, `seed ${seed}, when November opens, ${nm}`).toBe('JAP');
    }
  }, 300000);
});
