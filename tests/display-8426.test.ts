// Version 84.2.6, the display of ruling 211 beyond the red row (tests/ahead/alarm-8426.test.ts holds the row itself).
// The UI director's words: /mnt/project-files/ui-director/words-ruling-211.md, sections 2, 5 and 6(g), and the design
// director's log b349 (the Playtester's finds on 84.2.5). The case is Brian's own America save of December 1940.
import { describe, expect, it } from 'vitest';
import { addCorps, capture, createGame, headline, hostileUnits, moveCorps, relocate, theWords } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { attackOutlook } from '../src/campaign/turn';
import { ARSENAL } from '../src/campaign/arsenal';
import { markFor, monthCard } from '../src/campaign/monthcard';
import { loadGame } from '../src/campaign/save';
import { besieged } from '../src/campaign/siege';
import { setFog } from '../src/campaign/intel';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import { motherLine, patronOf } from '../src/campaign/world';
import { friendsOf } from '../src/campaign/diplomacy';
import type { Corps, Game, Region } from '../src/campaign/types';
import brianSave from './ahead/saves/brian-usa-1940-12.json?raw';

const brian = (): Game => loadGame(brianSave)!;
const at = (g: Game, name: string): Region => g.byName[name];
const playMonth = (g: Game): void => {
  endHuman(g);
  for (let i = 0; !humanUp(g) && i < 300; i++) stepAI(g);
  beginHuman(g);
};

describe('"the United States" takes its article wherever it follows a word (ruling 211(f), section 5)', () => {
  it('every line of news is read with the article, and "The" at the head of a line', () => {
    for (const [was, now] of [
      ['United States joins the Allies.', 'The United States joins the Allies.'],
      ['Finland capitulates to Soviet Union.', 'Finland capitulates to the Soviet Union.'],
      ['Canada is at war with Italy, beside United Kingdom. United States declares war.', 'Canada is at war with Italy, beside the United Kingdom. The United States declares war.'],
      ['British forces take Curacao into their keeping: Netherlands has fallen.', 'British forces take Curacao into their keeping: the Netherlands has fallen.'],
      ['The month in the air and at sea, United States: Hamburg bombed.', 'The month in the air and at sea, the United States: Hamburg bombed.'],
    ]) expect(theWords(was)).toBe(now);
    // Already right, or not a name at all, it is left as it was.
    for (const same of ['against the United States', 'The United States gives in', 'a Soviet Union that is no ally', 'the Netherlands East Indies', 'Germany declares war on Poland.'])
      expect(theWords(same)).toBe(same);
  });

  it('headline() writes it so', () => {
    const g = brian();
    headline(g, 'fall', 'Finland capitulates to Soviet Union.');
    expect(g.headlines[g.headlines.length - 1].text).toBe('Finland capitulates to the Soviet Union.');
  });
});

describe('a siege of his town raised with no battle is one row, HELD against the besieger (log b349, the UI director)', () => {
  it('guns left alone round Pittsburgh fall back: "Pittsburgh HELD against Canada. The siege of Pittsburgh is raised."', () => {
    const g = brian();
    setFog(g, false);
    const P = at(g, 'Pittsburgh'), B = at(g, 'Buffalo');
    const gun = addCorps(g, 'CAN', 'Test Canadian artillery', 'art', 2, B.id, {});
    const inf = addCorps(g, 'CAN', 'Test Canadian infantry', 'inf', 2, B.id, {});
    gun.ap = gun.apMax; inf.ap = inf.apMax;
    expect(moveCorps(g, inf, P)).toBe(true);
    expect(moveCorps(g, gun, P)).toBe(true);
    relocate(g, inf, B);
    expect(besieged(g, P)).toBe(true);
    const mark = markFor(g);
    playMonth(g);
    const card = monthCard(g, mark);
    const rows = [card.lead, ...card.rows].filter((r) => r && r.region === P.id && /siege|guns/i.test(r.line));
    expect(rows).toHaveLength(1);
    expect(rows[0]).toMatchObject({ tag: 'HELD', line: 'The siege of Pittsburgh is raised.', join: { word: 'against', name: 'Canada' } });
  });
});

describe('the month card warning ends with the help, and SHOW ON MAP presses the red row (section 2)', () => {
  it('a town beside Washington taken this month: the WARNING band carries the alarm', () => {
    const g = brian();
    setFog(g, false);
    const mark = markFor(g);
    const P = at(g, 'Pittsburgh');
    for (const u of [...P.units]) relocate(g, u, at(g, 'Washington'));
    for (const u of [...(P.garr ?? [])]) relocate(g, u, at(g, 'Washington'));
    addCorps(g, 'CAN', 'Test Canadian infantry', 'inf', 2, P.id, {});
    capture(g, P, 'CAN');
    const card = monthCard(g, mark);
    const band = card.bands.find((b) => b.word === 'WARNING');
    expect(band).toBeDefined();
    expect(band!.alarm).toBe(true);
    expect(band!.text).toMatch(/Nearest help: .+\.$/);
  });
});

describe("a dominion's mother country, first among its friends on the declare card (section 6(g))", () => {
  it('says the year of the tie, and the tie moves no opinion', () => {
    expect(patronOf('CAN')).toBe('UK');
    expect(motherLine('UK', 'CAN', 'Canada')).toBe('The mother country: Canada has been a dominion since 1867.');
    expect(motherLine('UK', 'AUS', 'Australia')).toBe('The mother country: Australia has been a dominion since 1901.');
    expect(motherLine('UK', 'NZL', 'New Zealand')).toBe('The mother country: New Zealand has been a dominion since 1907.');
    expect(motherLine('UK', 'SAF', 'South Africa')).toBe('The mother country: South Africa has been a dominion since 1910.');
    // Display only: the friends whose opinion a war moves are as they were.
    expect(friendsOf('CAN').some((f) => f.nat === 'UK')).toBe(false);
  });
});

describe('the guns rule where he reads it first (log b357)', () => {
  it("the arsenal's Artillery and Engineer rows say they cannot take ground without infantry or tanks", () => {
    const blurb = (k: string): string => (ARSENAL as unknown as { land: Record<string, { blurb: string }> }).land[k].blurb;
    expect(blurb('art')).toBe('Massed guns. Breaks fortified lines and fieldworks in the attack, but cannot take ground without infantry or tanks. Thickens any defence it stands in. Slow, weak if caught alone, and as dear as a corps of tanks.');
    expect(blurb('eng')).toMatch(/A poor fighter on its own, and it cannot take ground without infantry or tanks\.$/);
  });

  it('the fog wins: lone guns he has not identified give the ticket no "Guns alone" row, and identified they do', () => {
    const g = startOpen(createGame(3, 'GER'));
    for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
    beginHuman(g);
    setFog(g, true);
    let found: { to: Region; inf: Corps } | null = null;
    for (const from of g.regions) {
      if (found || from.ctrl !== 'GER') continue;
      const inf = from.units.find((c) => c.nat === 'GER' && !c.dead && c.kind === 'inf' && c.ap > 0 && c.town === undefined);
      const to = inf ? from.nb.map((j) => g.regions[j]).find((x) => x.ctrl === 'POL' && hostileUnits(g, x, 'GER').length > 0) : undefined;
      if (inf && to) found = { to, inf };
    }
    const { to, inf } = found!;
    const far = g.regions.find((x) => x.ctrl === 'POL' && x.owner === 'POL' && x !== to && !to.nb.includes(x.id) && x.units.length < 3)!;
    for (const u of [...to.units]) relocate(g, u, far);
    // (The town's guard is spent: a city's guard beside the guns (ruling 232) means they are not alone.)
    g.flags[`guard:${to.id}`] = 100;
    addCorps(g, 'POL', 'Polish test artillery', 'art', 2, to.id, {});
    const fogged = attackOutlook(g, [inf], to)!;
    expect(fogged.exact).toBe(false);
    expect(fogged.lines.some((l) => l.label === 'Guns alone')).toBe(false);
    setFog(g, false);
    expect(attackOutlook(g, [inf], to)!.lines.some((l) => l.label === 'Guns alone')).toBe(true);
  });
});
