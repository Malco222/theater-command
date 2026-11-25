// Home ground answers a landing from the sea, with the far side's switch on (rider b641 (f) to ruling 234, as the
// design director bounded it at 16:00Z of 19 November 2026; the Pacific playtester's finding 4).
//
// Every home corps of Japan's stood on hold while a player's Americans walked from Kushiro to Sendai. Beyond the old
// chart, on a piece of a country's homeland that an enemy reached from the sea (no frontier by land with ground an
// enemy holds outside it), the corps held on that piece answer for the month's fighting, and hold again where they
// then stand. A land front stays with its staff: China's line is not touched. Never a fortress, a row marked to stay
// or a row the plan has waiting; never for the player; nothing on the old chart.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { answering, homeBars, withAnswer } from '../src/campaign/ai';
import { addCorps, atWar, createGame, relocate, setWar } from '../src/campaign/game';
import { homeland } from '../src/campaign/middleeast';
import { startOpen } from '../src/campaign/staff';
import { current, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';
import { beyondEurope, farOn } from '../src/campaign/world';

const wide = (human: string | null = null): Game => startOpen(createGame(1, human));
/** A field corps of a country set down on a province, supplied and in good order. */
const invader = (g: Game, nat: string, at: Region): Corps => {
  const c = g.corps.find((x) => x.nat === nat && !x.dead && x.kind === 'inf' && !x.town && !x.hold)!;
  relocate(g, c, at);
  c.supplied = true; c.org = 1;
  return c;
};
const names = (cs: Corps[]): string[] => cs.map((c) => c.name).sort();
/** Japan's four rows held on the home islands when the war opens. */
const HOME = ['General reserve (16th Division, home from China)', 'General reserve (9th Division, home from China)', 'Home district depots', 'Imperial Guards Division'];

describe('home ground an enemy reached from the sea (rider b641 (f))', () => {
  it('nobody answers while no enemy stands on home ground, and China\'s line is its staff\'s own', () => {
    const g = wide();
    expect(farOn(g)).toBe(true);
    expect(answering(g, 'JAP')).toEqual([]);
    // Japan stands on China's home ground from the first day, by land from Manchukuo's: a land front.
    expect(atWar(g, 'CHN', 'JAP')).toBe(true);
    expect(g.regions.some((r) => homeland(r, 'CHN') && r.ctrl === 'JAP')).toBe(true);
    expect(g.corps.some((c) => c.nat === 'CHN' && c.hold && !c.stay && c.kind !== 'fort' && homeland(g.regions[c.reg], 'CHN'))).toBe(true);
    expect(answering(g, 'CHN')).toEqual([]);
  });

  it('Americans ashore at Kushiro are answered by the corps held on the home islands, and by no other', () => {
    const g = wide();
    setWar(g, 'JAP', 'USA');
    const kushiro = g.byName['Kushiro'];
    expect([kushiro.owner, homeland(kushiro, 'JAP'), beyondEurope(kushiro)]).toEqual(['JAP', true, true]);
    invader(g, 'USA', kushiro);
    // The four held at home. Not Karafuto's brigade, Formosa's army or Korea's (no home ground), not the Kwantung
    // Army, not a row of the China garrisons marked to stay.
    expect(names(answering(g, 'JAP'))).toEqual(HOME);
    // A fortress on the islands, a row marked to stay there, and a corps held on another island do not answer.
    addCorps(g, 'JAP', 'Tokyo Bay fortress', 'fort', 1, 'Tokyo', { hold: 1 });
    addCorps(g, 'JAP', 'Test garrison marked to stay', 'inf', 1, 'Sapporo', { hold: 1, stay: 1 });
    addCorps(g, 'JAP', 'Test garrison of Okinawa', 'inf', 1, 'Okinawa', { hold: 1 });
    const seventh = addCorps(g, 'JAP', 'Test division at Sapporo', 'inf', 1, 'Sapporo', { hold: 1 });
    expect(names(answering(g, 'JAP'))).toEqual([...HOME, seventh.name].sort());
    // An enemy who only holds the ground, with nobody standing on it, is answered too.
    for (const u of [...kushiro.units]) if (u.nat === 'USA') relocate(g, u, g.byName['Honolulu']);
    expect(answering(g, 'JAP')).toEqual([]);
    kushiro.ctrl = 'USA';
    expect(answering(g, 'JAP')).toHaveLength(5);
  });

  it('it is not read for the player, nor on the old chart, nor where an enemy holds ground next to the homeland by land', () => {
    const mine = wide('JAP');
    setWar(mine, 'JAP', 'USA');
    invader(mine, 'USA', mine.byName['Kushiro']);
    expect(answering(mine, 'JAP')).toEqual([]);
    // Britain is an island on the old chart, and Europe's wars are not touched.
    const g = wide();
    setWar(g, 'GER', 'UK');
    const kent = g.regions.find((r) => homeland(r, 'UK') && !beyondEurope(r) && r.port)!;
    invader(g, 'GER', kent);
    addCorps(g, 'UK', 'Test home division', 'inf', 1, kent.id, { hold: 1 });
    expect(answering(g, 'UK')).toEqual([]);
    // America's home ground: a landing from the sea is answered, and not once an enemy holds ground across a land frontier.
    setWar(g, 'JAP', 'USA');
    const coast = g.regions.find((r) => homeland(r, 'USA') && beyondEurope(r) && r.port && r.ctrl === 'USA')!;
    invader(g, 'JAP', coast);
    const held = addCorps(g, 'USA', 'Test home division', 'inf', 1, 'Washington', { hold: 1 });
    expect(answering(g, 'USA')).toContain(held);
    const border = g.regions.find((r) => r.owner !== 'USA' && beyondEurope(r) && r.nb.some((j) => homeland(g.regions[j], 'USA')))!;
    setWar(g, border.ctrl, 'USA');
    expect(atWar(g, 'USA', border.ctrl)).toBe(true);
    expect(answering(g, 'USA')).toEqual([]);
  });

  it("the hold is the record's: it comes off for the month's fighting and is back on when the staff's month is over", () => {
    const g = wide();
    setWar(g, 'JAP', 'USA');
    invader(g, 'USA', g.byName['Kushiro']);
    const answer = answering(g, 'JAP');
    expect(names(answer)).toEqual(HOME);
    while (current(g) !== 'JAP') stepAI(g);
    stepAI(g);
    for (const c of answer) expect(c.dead || c.hold, c.name).toBe(true);
    // None of them was shipped abroad: each still stands on the home islands.
    for (const c of answer) if (!c.dead) expect(homeland(g.regions[c.reg], 'JAP'), c.name).toBe(true);
  }, 120000);

  it('a row that answers marches, rides and fights only on its own piece of home ground, and never sails (the director, 16:17Z)', () => {
    const g = wide();
    setWar(g, 'JAP', 'USA');
    const coast = g.regions.find((r) => homeland(r, 'USA') && beyondEurope(r) && r.port && r.ctrl === 'USA')!;
    invader(g, 'JAP', coast);
    // A held division at Detroit, with Canada's Windsor across the river: allied ground, by land, and no home ground of its own.
    const detroit = g.byName['Detroit'], windsor = g.byName['Windsor'];
    expect(detroit.nb.includes(windsor.id) && windsor.owner === 'CAN').toBe(true);
    const held = addCorps(g, 'USA', 'Test home division', 'inf', 1, detroit.id, { hold: 1 });
    const field = addCorps(g, 'USA', 'Test field division', 'inf', 1, detroit.id, {});
    const inside = detroit.nb.map((j) => g.regions[j]).find((r) => homeland(r, 'USA'))!;
    expect([homeBars(held, windsor), held.hold]).toEqual([false, true]);
    const seen = withAnswer(g, 'USA', () => ({ hold: held.hold, out: homeBars(held, windsor), honolulu: homeBars(held, g.byName['Honolulu']), near: homeBars(held, inside), field: homeBars(field, windsor) }));
    // For the month it is off hold, barred from Canada and from every ground off its own piece (Hawaii is not
    // joined to it by land), and free on its own; a corps of the field army is not tied.
    expect(seen).toEqual({ hold: false, out: true, honolulu: true, near: false, field: false });
    // The month over, it holds again and nothing is tied.
    expect([homeBars(held, windsor), held.hold]).toEqual([false, true]);
    // A Japanese corps answering on the home islands may go nowhere off them: not Korea, not China.
    const at = addCorps(g, 'JAP', 'Test division at Sapporo', 'inf', 1, 'Sapporo', { hold: 1 });
    invader(g, 'USA', g.byName['Kushiro']);
    const far = withAnswer(g, 'JAP', () => ['Tokyo', 'Seoul', 'Shanghai', 'Okinawa'].map((nm) => homeBars(at, g.byName[nm])));
    expect(far).toEqual([false, true, true, true]);
  });
});
