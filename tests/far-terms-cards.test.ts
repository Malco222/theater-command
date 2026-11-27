// What the cards say of Japan's terms (ruling 196, point 8 and the planner's three lines; the UI director's sheet 25,
// box 5, with the riders of 05:53Z and 06:06Z, 17 November 2026).
//
// The rule is tests/far-terms.test.ts's: a computer Japan asks for terms when an enemy holds a twentieth of its home
// ground and privation has worn all it can off its will. Here are the words the rules hand both interfaces for it:
//   1. under the bar that shows how much fight Japan has left, a label and a line for each condition, met or not
//      yet, with no figure; the month both hold, what follows. Japan's own player, who is spared the rule, reads the
//      same two lines as the measure of his war;
//   2. the news of the month each condition first holds with the other not yet, once each, and never to Japan's own
//      player. The first has its own line for the month card ("War has reached Japan's home ground.", 36 letters, the
//      width of a row there) and stands before a battle's line for Japan;
//   3. the landing card's row "Holding it: brings the war to Japan's home ground", only where holding the place
//      would make the first condition hold.
// Each state is set by hand on the first day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { termsCard, warHome } from '../src/campaign/danger';
import { atWar, checkTerms, createGame, declareWar, headline, termsOf } from '../src/campaign/game';
import { monthCard, monthMark } from '../src/campaign/monthcard';
import { RES, storesOf } from '../src/campaign/resources';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

/** The first day's chart, with Japan at war with America and Britain; `human` leads one country, or nobody does. */
const war = (human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  for (const e of ['USA', 'UK']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
  return g;
};
const take = (g: Game, name: string, by: string): void => { g.byName[name].ctrl = by; };
/** Privation at its limit: the count of hungry months that takes the whole 30 in 100 off the will to fight. */
const starve = (g: Game, months = RES.will.most / RES.will.per): void => { storesOf(g, 'JAP')!.hunger = months; };
const GROUND = "The war has reached Japan's home ground. Japan asks for terms when its people can bear no more.";
const PEOPLE = "Japan's people can bear no more. It asks for terms once an enemy holds a part of its home ground.";
/** The two lines, as often as they were said. */
const news = (g: Game, from = 0): string[] => g.headlines.slice(from).map((h) => h.text).filter((t) => t === GROUND || t === PEOPLE);

describe('how near Japan is to terms: two lines under its bar, met or not yet (ruling 196, point 8; sheet 25)', () => {
  it('an enemy reads a label and the two conditions, each met or not yet, and no figure', () => {
    const g = war('USA');
    expect(termsCard(g, 'JAP')).toEqual({
      label: 'It asks for terms when both hold',
      rows: [{ text: 'The war has reached its home ground', met: false }, { text: 'Its people can bear no more', met: false }],
      then: null,
    });
    starve(g);
    expect(termsCard(g, 'JAP')!.rows.map((r) => r.met)).toEqual([false, true]);
    expect(termsCard(g, 'JAP')!.then).toBeNull();
    take(g, 'Okinawa', 'USA');
    // The month both hold, a third line says what follows.
    expect(termsCard(g, 'JAP')).toEqual({
      label: 'It asks for terms when both hold',
      rows: [{ text: 'The war has reached its home ground', met: true }, { text: 'Its people can bear no more', met: true }],
      then: 'Japan asks for terms as next month opens.',
    });
    // No figure anywhere: neither a share of the home ground nor a count of privation.
    expect(JSON.stringify(termsCard(g, 'JAP'))).not.toMatch(/\d/);
  });

  it('Japan\'s own player reads the same two lines as the measure of his war, and that he decides', () => {
    const g = war('JAP');
    take(g, 'Okinawa', 'USA');
    expect(termsCard(g, 'JAP')).toEqual({
      label: 'Led by the computer, Japan would ask for terms when both hold',
      rows: [{ text: 'The war has reached your home ground', met: true }, { text: 'Your people can bear no more', met: false }],
      then: 'You decide when you are beaten.',
    });
    starve(g);
    expect(termsCard(g, 'JAP')!.rows.map((r) => r.met)).toEqual([true, true]);
    expect(termsCard(g, 'JAP')!.then).toBe('You decide when you are beaten.');
  });

  it('nothing is shown for a country with no row, for Japan at peace, or once it has fallen', () => {
    const g = startOpen(createGame(3, 'USA'));
    if (!atWar(g, 'GER', 'UK')) declareWar(g, 'GER', 'UK');
    for (const nat of ['GER', 'UK', 'USA', 'SOV']) expect(termsCard(g, nat), nat).toBeNull();
    if (!g.nations.JAP.atWar.length) expect(termsCard(g, 'JAP')).toBeNull();
    const w = war('USA');
    expect(termsCard(w, 'JAP')).not.toBeNull();
    w.nations.JAP.alive = false;
    expect(termsCard(w, 'JAP')).toBeNull();
  });
});

describe('the news of the month each condition first holds (the planner\'s two lines)', () => {
  it('the war reaches Japan\'s home ground first: one line, said once, with its own line for the month card', () => {
    const g = war('USA');
    const from = g.headlines.length;
    expect(checkTerms(g, 'JAP')).toBe(false);
    expect(news(g, from)).toEqual([]);
    take(g, 'Okinawa', 'USA');
    expect(checkTerms(g, 'JAP')).toBe(false);
    expect(news(g, from)).toEqual([GROUND]);
    const h = g.headlines[g.headlines.length - 1];
    expect(h).toMatchObject({ turn: g.turn, kind: 'event', nat: 'JAP', card: "War has reached Japan's home ground." });
    expect(h.card!.length).toBe(36);
    // Said once: a second month, and the ground lost and taken again, add nothing.
    g.turn++;
    checkTerms(g, 'JAP');
    take(g, 'Okinawa', 'JAP');
    checkTerms(g, 'JAP');
    take(g, 'Okinawa', 'USA');
    checkTerms(g, 'JAP');
    expect(news(g, from)).toEqual([GROUND]);
    // The month both hold the news is a fall's, and the people's line is never said.
    starve(g);
    expect(checkTerms(g, 'JAP')).toBe(true);
    expect(news(g, from)).toEqual([GROUND]);
    // (Ruling 201, point 4: Europe is still at war, so the fall's band carries its second line.)
    expect(g.headlines.slice(from).filter((x) => x.kind === 'fall').map((x) => x.text)).toEqual(['Japan accepts terms and is out of the war. Japan is beaten. The war goes on in Europe: Britain, France and Germany are still at war.']);
  });

  it('its people can bear no more first: the other line, said once', () => {
    const g = war('USA');
    const from = g.headlines.length;
    starve(g);
    expect(checkTerms(g, 'JAP')).toBe(false);
    expect(news(g, from)).toEqual([PEOPLE]);
    expect(g.headlines[g.headlines.length - 1].card).toBeUndefined();
    g.turn++;
    checkTerms(g, 'JAP');
    expect(news(g, from)).toEqual([PEOPLE]);
    take(g, 'Okinawa', 'USA');
    expect(checkTerms(g, 'JAP')).toBe(true);
    expect(news(g, from)).toEqual([PEOPLE]);
  });

  it('Japan\'s own player is shown neither: they would say of him what is not true', () => {
    const g = war('JAP');
    const from = g.headlines.length;
    take(g, 'Okinawa', 'USA');
    checkTerms(g, 'JAP');
    take(g, 'Okinawa', 'JAP');
    starve(g);
    checkTerms(g, 'JAP');
    expect(news(g, from)).toEqual([]);
    expect(termsOf(g, 'JAP')).toMatchObject({ ground: false, privation: true });
  });

  it('on the month card it is Japan\'s line under ELSEWHERE, in 36 letters, before a battle\'s line for Japan', () => {
    const g = war('USA');
    const mark = monthMark(g, 'USA');
    headline(g, 'battle', 'Japan beats off the landing at Rabaul.', 'JAP');
    take(g, 'Okinawa', 'USA');
    checkTerms(g, 'JAP');
    const card = monthCard(g, mark, 'USA');
    const japan = card.elsewhere.filter((e) => e.nat === 'JAP');
    expect(japan.map((e) => [e.text, e.full])).toEqual([["War has reached Japan's home ground.", GROUND]]);
  });
});

describe('the landing card\'s row: "Holding it: brings the war to Japan\'s home ground"', () => {
  it('on Okinawa, with what is still wanted in its bracket', () => {
    const g = war('USA');
    const okinawa = g.byName['Okinawa'];
    expect(warHome(g, 'USA', okinawa)).toEqual({ nat: 'JAP', key: 'Holding it', text: "brings the war to Japan's home ground", note: 'Japan asks for terms when its people can bear no more' });
    starve(g);
    expect(warHome(g, 'USA', okinawa)!.note).toBe('Japan asks for terms as the next month opens');
  });

  it('only where holding the place would make the first condition hold: not Iwo Jima alone, and Iwo Jima once another of its worth is held', () => {
    const g = war('USA');
    const iwo = g.byName['Iwo Jima'];
    expect(warHome(g, 'USA', iwo)).toBeNull();
    take(g, 'Sapporo', 'USA');
    expect(termsOf(g, 'JAP')!.ground).toBe(false);
    expect(warHome(g, 'USA', iwo)).not.toBeNull();
    // Once the condition holds the row has nothing left to say, on any home province.
    take(g, 'Okinawa', 'USA');
    expect(termsOf(g, 'JAP')!.ground).toBe(true);
    expect(warHome(g, 'USA', iwo)).toBeNull();
    expect(warHome(g, 'USA', g.byName['Tokyo'])).toBeNull();
  });

  it('not on ground over the water, not for a country at peace with Japan, and not against a player\'s Japan', () => {
    const g = war('USA');
    const okinawa = g.byName['Okinawa'];
    for (const name of ['Taihoku', 'Pusan', 'Truk']) expect(warHome(g, 'USA', g.byName[name]), name).toBeNull();
    expect(atWar(g, 'SOV', 'JAP')).toBe(false);
    expect(warHome(g, 'SOV', okinawa)).toBeNull();
    expect(warHome(g, 'USA', g.byName['Berlin'])).toBeNull();
    const own = war('JAP');
    expect(warHome(own, 'USA', own.byName['Okinawa'])).toBeNull();
  });
});
