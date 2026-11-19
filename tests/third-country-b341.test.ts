// A third country's corps on ground the player fights for (rulings b341 and b343 of 17 November 2026, for 84.3, the
// grand campaign). Found by the rules helper's check: a British corps, at peace with the United States, standing in
// Canadian Ottawa was not in the reckoning at all. The card said "no army inside", the hover box named a British army,
// and once Ottawa fell the British corps stood on American ground. Now one sentence written in the rules is read by
// the cards and the hover box, and a corps at peace with the new holder goes home with one plain line of news.
import { describe, expect, it } from 'vitest';
import { allied, atWar, capture, createGame, setWar, standersAt, standersLine } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

const live = (c: Corps): boolean => !c.dead && c.kind !== 'fort';
function put(g: Game, c: Corps, r: Region): void {
  const from = g.regions[c.reg];
  from.units.splice(from.units.indexOf(c), 1);
  r.units.push(c);
  c.reg = r.id;
}

/** The United States at war with Canada, and British corps set down in a Canadian province beside American ground. */
function stage(britons = 1): { g: Game; X: Region; brits: Corps[] } {
  const g = startOpen(createGame(3, 'USA'));
  setWar(g, 'USA', 'CAN');
  const X = g.regions.find((r) => r.ctrl === 'CAN' && r.owner === 'CAN' && r.nb.some((j) => g.regions[j].ctrl === 'USA'))!;
  const brits = g.corps.filter((c) => c.nat === 'UK' && live(c) && c.kind === 'inf').slice(0, britons);
  for (const c of brits) put(g, c, X);
  return { g, X, brits };
}

describe('a third country\'s corps at peace with the player', () => {
  it('is named in one sentence, the same for the card and the hover box', () => {
    const { g, X } = stage(1);
    expect(atWar(g, 'USA', 'UK')).toBe(false);
    expect(allied(g, 'USA', 'UK')).toBe(false);
    expect(standersAt(g, X, 'USA').map(([nat, cs]) => [nat, cs.length])).toEqual([['UK', 1]]);
    expect(standersLine(g, X, 'USA')).toEqual([`A British corps stands at ${X.name} too, at peace with you: it takes no part in the fight, and goes home if the province changes hands.`]);
    // The country's own corps, and the player's enemies and allies, are not spoken of in it.
    expect(standersAt(g, X, 'USA').some(([nat]) => nat === 'CAN')).toBe(false);
  });

  it('counts several, and says nothing of a country at war with him or in a history campaign', () => {
    const { g, X } = stage(2);
    expect(standersLine(g, X, 'USA')).toEqual([`2 British corps stand at ${X.name} too, at peace with you: they take no part in the fight, and go home if the province changes hands.`]);
    setWar(g, 'USA', 'UK');
    expect(standersLine(g, X, 'USA')).toEqual([]);
    const h = createGame(3, 'GER');
    const r = h.regions.find((x) => x.units.some((c) => !c.dead))!;
    expect(h.flags.open).not.toBe(1);
    expect(standersLine(h, r, 'GER')).toEqual([]);
  });

  it('goes home to its own nearest ground when the province changes hands, with one plain line told to the player', () => {
    const { g, X, brits } = stage(1);
    for (const c of X.units.filter((c) => c.nat === 'CAN')) put(g, c, g.regions.find((r) => r.ctrl === 'CAN' && r.id !== X.id)!);
    const c = brits[0];
    c.ap = 3;
    const n = g.headlines.length;
    capture(g, X, 'USA');
    expect(X.ctrl).toBe('USA');
    expect(c.reg).not.toBe(X.id);
    expect(g.regions[c.reg].ctrl).toBe('UK');
    expect(c.ap).toBe(0);
    expect(X.units.includes(c)).toBe(false);
    const said = g.headlines.slice(n).filter((h) => /goes home/.test(h.text));
    expect(said.map((h) => [h.kind, h.nat, h.text])).toEqual([['info', 'USA', `The British corps at ${X.name} goes home: the province has changed hands.`]]);
  });

  it('several go home in one line', () => {
    const { g, X, brits } = stage(2);
    for (const c of X.units.filter((c) => c.nat === 'CAN')) put(g, c, g.regions.find((r) => r.ctrl === 'CAN' && r.id !== X.id)!);
    const n = g.headlines.length;
    capture(g, X, 'USA');
    expect(brits.every((c) => g.regions[c.reg].ctrl === 'UK')).toBe(true);
    expect(g.headlines.slice(n).filter((h) => /go home/.test(h.text)).map((h) => h.text)).toEqual([`The 2 British corps at ${X.name} go home: the province has changed hands.`]);
  });

  it('the new holder\'s own corps stays where it is', () => {
    const { g, X, brits } = stage(1);
    for (const c of X.units.filter((c) => c.nat === 'CAN')) put(g, c, g.regions.find((r) => r.ctrl === 'CAN' && r.id !== X.id)!);
    // Britain at war with Canada and taking the province itself: its own corps is no guest there.
    setWar(g, 'UK', 'CAN');
    capture(g, X, 'UK');
    expect(brits[0].reg).toBe(X.id);
  });
});
