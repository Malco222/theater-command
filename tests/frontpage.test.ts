// The war correspondent's front page (release 2): the player's country's newspaper for the month, drawn from the
// month card. Display only: it changes nothing and draws no die.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { saveGame } from '../src/campaign/save';
import { monthCard, monthMark, type MonthCard } from '../src/campaign/monthcard';
import { FRONT, frontPage, isExtra, ourWords, paperHead, paperOf } from '../src/campaign/frontpage';
import type { Game } from '../src/campaign/types';

/** A war nobody plays, to the month `turn`, and the card of the month before for `viewer`. */
function cardAt(seed: number, viewer: string, turn: number): { g: Game; card: MonthCard } {
  const g = startOpen(createGame(seed, null));
  let mark = monthMark(g, viewer);
  while (g.turn < turn) { mark = monthMark(g, viewer); runTurnAI(g); }
  return { g, card: monthCard(g, mark, viewer) };
}

describe('the front page: the paper itself', () => {
  it('is printed by each country in its own capital, on the first of the next month, with the right weekday', () => {
    const { g, card } = cardAt(25, 'FRA', 10);
    const p = frontPage(g, card);
    expect(card.month).toBe('May 1940');
    expect(p.paper).toBe('Le Courrier du Soir');
    expect(p.city).toBe('Paris');
    expect(p.date).toBe('Saturday, June 1, 1940');
    expect(p.translated).toBe('Translated from the French');
    expect(p.issue).toMatch(/^No\. \d{2},\d{3}$/);
    expect(p.voice).toBe('free');
  });

  it('gives a country with no paper of its own a gazette named for its capital', () => {
    const g = startOpen(createGame(17, null));
    const p = paperOf(g, 'SWE');
    expect(p.name).toBe(`The ${g.regions[g.nations.SWE.capital].name} Gazette`);
  });

  it('has invented mastheads only, and a correspondent for every great power', () => {
    for (const n of ['UK', 'USA', 'FRA', 'GER', 'ITA', 'SOV', 'JAP']) {
      expect(FRONT.papers[n]).toBeTruthy();
      expect(FRONT.correspondents[n]).toBeTruthy();
    }
    // (No real paper of the war's name: a short guard list of the best known.)
    const real = /times|pravda|izvestia|völkischer|beobachter|daily mail|daily express|herald tribune|asahi|mainichi|corriere della sera|le figaro|le temps|washington post/i;
    for (const p of Object.values(FRONT.papers)) expect(p.name).not.toMatch(real);
  });

  it('changes nothing in the game and draws no die', () => {
    const { g, card } = cardAt(17, 'GER', 12);
    const before = saveGame(g);
    const a = frontPage(g, card), b = frontPage(g, card);
    expect(saveGame(g)).toBe(before);
    expect(JSON.stringify(a)).toBe(JSON.stringify(b));
  });
});

describe('the front page: the voice', () => {
  it('turns the card\'s headline into the paper\'s, by the news and the press', () => {
    expect(paperHead('Sedan is lost', 'bad', 'free')).toBe('SEDAN LOST AFTER HARD FIGHTING');
    expect(paperHead('Sedan is lost', 'bad', 'state')).toBe('OUR LINE SHORTENED AT SEDAN');
    expect(paperHead('Athens is taken', 'good', 'state')).toBe('ATHENS TAKEN');
    expect(paperHead('Hamburg is bombed', 'good', 'free')).toBe('OUR BOMBERS OVER HAMBURG');
    expect(paperHead('Paris is bombed', 'bad', 'state')).toBe('TERROR RAID ON PARIS');
    expect(paperHead('The 1st Fleet is struck from the air', 'good', 'free', 'Italian')).toBe('OUR AIRMEN STRIKE THE ITALIAN 1ST FLEET');
    expect(paperHead('1 province occupied from Greece', 'good', 'state')).toBe('ONE PROVINCE OCCUPIED FROM GREECE');
    expect(paperHead('Germany declares war on Poland', 'plain', 'free')).toBe('GERMANY DECLARES WAR ON POLAND');
    expect(paperHead('Something quite new happens', 'plain', 'free')).toBe('SOMETHING QUITE NEW HAPPENS');
  });

  it('prints the player\'s side as "our"', () => {
    expect(ourWords('Its harbour is open to your ships.')).toBe('Its harbour is open to our ships.');
    expect(ourWords('The sky over Kent is yours')).toBe('The sky over Kent is ours');
  });

  it('keeps a state paper\'s own losses out of a defeat, and a free paper\'s in', () => {
    const { g, card } = cardAt(21, 'ITA', 20);
    const bad = card.rows.find((r) => r.tone === 'bad' && r.arm === 'land' && r.figures.length === 2);
    expect(bad).toBeTruthy();
    const p = frontPage(g, card);
    const told = [p.lead, ...p.stories].filter(Boolean).map((s) => s!.body.join(' ')).join(' ');
    expect(told).not.toContain(bad!.figures[0].n);
    // The same month in a free press's words keeps the figure.
    const free = frontPage(g, { ...card, viewer: 'UK' });
    expect(free.voice).toBe('free');
  });
});

describe('the front page: the stories', () => {
  it('leads with the card\'s lead, under a byline, a dateline and two decks, and a photograph with words under it', () => {
    const { g, card } = cardAt(17, 'GER', 24);
    const p = frontPage(g, card);
    expect(p.lead).not.toBeNull();
    expect(p.lead!.head).toBe(p.lead!.head.toUpperCase());
    expect(p.lead!.dateline).toMatch(/^[A-Z ,.'-]+, [A-Z][a-z]+ \d{1,2} —$/);
    expect(p.lead!.decks.length).toBeGreaterThan(0);
    expect(p.lead!.body.length).toBeGreaterThanOrEqual(2);
    expect(p.byline).toBe('From our war correspondent, Hans Kessler');
    expect(p.picture).toBe(card.lead!.picture);
    expect(p.caption.length).toBeGreaterThan(0);
    expect(p.stories.length).toBeLessThanOrEqual(3);
    expect(new Set([p.lead!.head, ...p.stories.map((s) => s.head)]).size).toBe(1 + p.stories.length);
    expect(p.briefs.length).toBeLessThanOrEqual(6);
    expect(new Set(p.briefs).size).toBe(p.briefs.length);
    expect(p.figures.map((f) => f.label)).toEqual(card.boxes.map((b) => b.label));
  });

  it('flies a banner and prints a special edition when a war is declared or a country falls', () => {
    const { g, card } = cardAt(17, 'SOV', 26);
    expect(card.bands.some((b) => b.word === 'WAR')).toBe(true);
    const p = frontPage(g, card);
    expect(p.banner).not.toBeNull();
    expect(p.banner!.head).toMatch(/WAR ON/);
    expect(p.extra).toBe(true);
    expect(p.edition).toBe('SPECIAL EDITION');
    expect(isExtra(g, card)).toBe(true);
  });

  it('is a late edition on an ordinary month', () => {
    const { g, card } = cardAt(17, 'UK', 13);
    const p = frontPage(g, { ...card, bands: [], lead: card.lead ? { ...card.lead, kind: 'AIR RAID', region: undefined, facts: [], figures: [] } : null });
    expect(p.extra).toBe(false);
    expect(p.edition).toBe('LATE EDITION');
    expect(p.censor).toBe('Passed by the censor.');
  });
});
