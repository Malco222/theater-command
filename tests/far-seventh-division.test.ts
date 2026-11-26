// The 7th Division in Hokkaido (rider b641 to ruling 234; historian/seventh-division-hokkaido-b641.md).
//
// A fact of the day, at every stop of the dial: the division was in Manchuria with the 6th Army in September 1939,
// came home to Asahikawa in August 1940 as the high command's reserve, and stood in Hokkaido to the surrender. It
// never sailed as a division. The first-day row '6th Army (after Khalkhin Gol)' is cut in two by name so that the
// same men are not counted twice, and the row by date is made from the half cut off. It is held and not marked to
// stay, so it answers a landing on the home islands (rider b641 (f)).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { answering } from '../src/campaign/ai';
import { createGame, relocate, rowOffers, setWar, startPhase, takeOffer } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const war = (stop: number, human: string | null = null): Game => {
  const g = startOpen(createGame(5, human));
  if (stop !== 3) setStop(g, stop, true);
  return g;
};
const row = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'JAP'); };
const HOME = '7th Division (Asahikawa)', AWAY = '6th Army (7th Division)', REST = '6th Army (after Khalkhin Gol)';

describe('the 7th Division (rider b641)', () => {
  it('on the first day it is in Manchuria with the 6th Army, a row of its own, and nobody is counted twice', () => {
    const g = war(3);
    const away = row(g, AWAY)!, rest = row(g, REST)!;
    expect([away.divs, away.str, away.hold, g.regions[away.reg].name]).toEqual([1, 0.7, true, g.regions[rest.reg].name]);
    expect([rest.divs, rest.str]).toEqual([1, 0.7]);
    expect(row(g, HOME)).toBeUndefined();
  });

  for (const stop of [1, 3]) it(`comes home to Sapporo's province in August 1940 and holds there, at stop ${stop}`, () => {
    const g = war(stop);
    month(g, 11);
    expect(row(g, HOME)).toBeUndefined();
    month(g, 12);
    const c = row(g, HOME)!;
    expect([g.regions[c.reg].name, c.divs, c.str, c.hold, !!c.stay, c.xp]).toEqual(['Sapporo', 1, 1, true, false, 0.3]);
    expect(row(g, AWAY)).toBeUndefined();
    expect(row(g, REST)!.divs).toBe(1);
    // Its size by the record: the Ichiki Detachment leaves in May 1942, a battalion goes to Attu in June.
    month(g, 32);
    expect([c.str, c.full]).toEqual([1, undefined]);
    month(g, 33);
    expect([c.str, c.full]).toEqual([0.85, 0.85]);
    month(g, 34);
    expect([c.str, c.full]).toEqual([0.8, 0.8]);
    expect(g.regions[c.reg].name).toBe('Sapporo');
  });

  // (Changed by the design director's ruling B8 of 25 November 2026. This test read "a player's Japan has the row
  // too", and the row formed of itself, striking his 6th Army's 7th Division off the map in Manchuria. A row that
  // takes a corps of the player's is an offer now: nothing is taken until he says yes, and taken it forms as it did.)
  it("a player's Japan is offered the row, and has it where he takes it", () => {
    const g = war(3, 'JAP');
    // (His rows are offers only where the switch 'offers' is on: stage.json, off until its screen passes, the design
    // director's 13:39Z of 25 November 2026. A campaign opened with it on carries the flag; the test sets it by hand.)
    g.flags.offers = 1;
    month(g, 12);
    expect([row(g, HOME), row(g, AWAY)?.name]).toEqual([undefined, AWAY]);
    const offers = rowOffers(g).filter((o) => o.name === HOME);
    expect(offers.map((o) => [g.regions[o.region].name, o.takes.map((t) => t.name), o.why])).toEqual([['Sapporo', [AWAY], null]]);
    expect(takeOffer(g, offers[0].id)).toBeNull();
    expect([g.regions[row(g, HOME)!.reg].name, row(g, AWAY)]).toEqual(['Sapporo', undefined]);
  });

  it('it answers a landing on the home islands with the rows held there', () => {
    const g = war(3);
    month(g, 12);
    setWar(g, 'JAP', 'USA');
    const landed = g.corps.find((x) => x.nat === 'USA' && !x.dead && x.kind === 'inf' && !x.town && !x.hold)!;
    relocate(g, landed, g.byName['Kushiro']);
    expect(answering(g, 'JAP').map((c) => c.name)).toContain(HOME);
  });
});
