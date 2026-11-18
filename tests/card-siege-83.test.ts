// A siege on the month card (the design director's rulings 104, 105 and 106; the art director's words of
// 2026-11-16). Display only: the card reads the game and changes nothing in it.
//
// What was wrong: a siege laid on the player's town was stamped HELD beside "5 corps held the line", though those
// corps had lost the field outside it, and the score counted the fight as a battle lost. The men who fell back into
// the town were in no count at all.
//
// 1. The words are true to what they count. "Men lost" is the month's dead, wounded and prisoners. The men shut in
//    a town that holds are "shut in": the corps in the town and its guard, as the card is made. They are in no
//    count of the lost.
// 2. A siege laid on his town that holds is neither a battle won nor a battle lost: the small line of the BATTLES
//    box says a town is besieged.
// 3. Its weight: the men lost in the fight, the place by its rank, and the men shut in, once, in the month the
//    siege is laid. In the months between, an assault thrown back weighs as any battle held. When the town falls
//    the row is LOST with all of them.
// 4. Ruling 95(d)'s first case, rewritten by ruling 106: for France in June 1940 the besieged capital leads, the
//    city held is the first row, the two rings come second and third, and the town lost after them. The numbers
//    here are those of the real game (France's seat, seed 25).
// 5. A ring that closes on a corps or more is never under the fold.
// 6. A band flies the flag of the country its news is about, and a FREED band is good news when his own side did
//    the freeing.
import { describe, expect, it } from 'vitest';
import { addCorps, atWar, createGame, declareWar, men, warning } from '../src/campaign/game';
import { monthCard, monthMark, type CardRow, type MonthCard } from '../src/campaign/monthcard';
import { loadGame, saveGame } from '../src/campaign/save';
import { besieged, garrisonOf, guardMen, joinTown } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { townName, townSize } from '../src/campaign/towns';
import type { BattleReport, Game, Headline, Region } from '../src/campaign/types';

const ME = 'GER';
const START = ((): string => {
  const g = startOpen(createGame(25, null));
  if (!atWar(g, ME, 'POL')) declareWar(g, ME, 'POL');
  return saveGame(g);
})();
const game = (): Game => loadGame(START)!;
const num = (n: number): string => Math.round(n).toLocaleString('en-US');
const isCity = (g: Game, r: Region): boolean => ['capital', 'city'].includes(townSize(g, r));
const capitalOf = (g: Game, nat: string): Region => g.regions[g.nations[nat].capital];
const germanCity = (g: Game): Region => g.regions.find((r) => r.owner === ME && r.ctrl === ME && isCity(g, r) && r.id !== g.nations[ME].capital)!;
const home = (g: Game): Region[] => g.regions.filter((r) => r.owner === ME && r.ctrl === ME && !r.cap && townSize(g, r) === 'village');
const order = (c: MonthCard): CardRow[] => [...c.rows.filter((r) => r.cls === 2), ...(c.lead ? [c.lead] : []), ...c.rows.filter((r) => r.cls !== 2)];
const box = (c: MonthCard, key: string): MonthCard['boxes'][number] | undefined => c.boxes.find((b) => b.key === key);

let nextId = 9500;
const unit = (nat: string, m = 30000): BattleReport['attackers'][number] => ({ name: 'I Corps', nat, kind: 'inf', divs: 3, men: m, lost: 0, fate: '' });
/** A land battle in the month's reports. Poland attacks Germany's ground unless told otherwise. */
function land(g: Game, at: Region, o: Partial<BattleReport> = {}): BattleReport {
  const rep = {
    id: nextId++, turn: g.turn, week: 2, region: at.id, regionName: at.name, attNat: 'POL', defNat: ME, attackers: [unit('POL')], defenders: [unit(ME)],
    lines: [], aPower: 2, dPower: 1, odds: 2, level: 'win', taken: true, attLost: 1000, defLost: 2000, prisoners: 0, ...o,
  } as unknown as BattleReport;
  g.reports.push(rep);
  return rep;
}
const news = (g: Game, h: Partial<Headline> & Pick<Headline, 'kind' | 'text'>): void => { g.headlines.push({ turn: g.turn, week: 2, ...h }); };
const ringAt = (g: Game, at: Region, corps: number, m: string): void =>
  news(g, { kind: 'battle', arm: 'land', text: `The ring closes at ${at.name}: ${corps} German corps, ${m} men, are cut off from all supply.`, nat: ME, region: at.id, lost: ME });
/**
 * A real siege of his town: so many corps of his inside it, nobody of his in the field, and a Polish army round it.
 * Returns the men shut in: the corps in the town and its guard.
 */
function shutUp(g: Game, r: Region, corps: number, divs = 3): number {
  for (const c of [...r.units]) { c.dead = true; }
  r.units = [];
  for (const c of [...garrisonOf(r)]) c.dead = true;
  r.garr = [];
  for (let i = 0; i < corps; i++) {
    const c = addCorps(g, ME, `Garrison ${i + 1}`, 'inf', divs, r.id, {});
    r.units.splice(r.units.indexOf(c), 1);
    joinTown(g, r, c);
  }
  addCorps(g, 'POL', 'Army Poznan', 'inf', 3, r.id, {});
  expect(besieged(g, r)).toBe(true);
  return garrisonOf(r).reduce((n, c) => n + men(c), 0) + guardMen(g, r);
}

describe('a siege laid on his town that holds', () => {
  it('shows the men shut in, the men lost and theirs, and the corps count in the line beneath', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const city = germanCity(g);
    const shut = shutUp(g, city, 4, 1);
    land(g, city, { siege: true, taken: false, attLost: 1822, defLost: 1204 } as Partial<BattleReport>);
    const card = monthCard(g, mark, ME);
    expect(shut).toBeGreaterThan(60000);
    // (Version 84.2.6, the fronts row: BESIEGED by its besieger, "Polish troops close round the town.")
    expect(card.lead).toMatchObject({ place: townName(city), tag: 'BESIEGED', tone: 'warn', head: `${townName(city)} is besieged`, line: 'Polish troops close round the town.' });
    expect(card.lead!.facts).toEqual([{ n: num(shut), label: 'men shut in' }, { n: '1,204', label: 'men lost' }, { n: '1,822', label: 'of theirs' }]);
    expect(card.lead!.follows).toBe(`4 corps are shut in at ${townName(city)}. The province is yours while the town holds.`);
    // (In the rows its one figure is the men shut in.)
    expect(card.lead!.figures).toEqual([{ n: num(shut), label: 'shut in' }]);
  });

  it('names the price first where the fight outside was heavy, and shows the men lost even at none', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const city = germanCity(g);
    const shut = shutUp(g, city, 4, 1);
    land(g, city, { siege: true, taken: false, attLost: 9000, defLost: 43295 } as Partial<BattleReport>);
    expect(monthCard(g, mark, ME).lead!.follows).toBe(`${townName(city)} holds, at a cost of 43,295 men. 4 corps are shut in.`);
    const h = game();
    const m2 = monthMark(h, ME);
    const c2 = germanCity(h);
    const s2 = shutUp(h, c2, 1, 2);
    land(h, c2, { siege: true, taken: false, attLost: 300, defLost: 0 } as Partial<BattleReport>);
    const lead = monthCard(h, m2, ME).lead!;
    expect(lead.facts).toEqual([{ n: num(s2), label: 'men shut in' }, { n: '0', label: 'men lost' }, { n: '300', label: 'of theirs' }]);
    expect(lead.follows).toBe(`1 corps is shut in at ${townName(c2)}. The province is yours while the town holds.`);
  });

  it('says so where only the guard is inside', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const city = germanCity(g);
    const shut = shutUp(g, city, 0);
    expect(shut).toBe(guardMen(g, city));
    expect(shut).toBeGreaterThan(0);
    land(g, city, { siege: true, taken: false, attLost: 500, defLost: 800 } as Partial<BattleReport>);
    const lead = monthCard(g, mark, ME).lead!;
    expect(lead.facts[0]).toEqual({ n: num(shut), label: 'men shut in' });
    expect(lead.follows).toBe(`${townName(city)} is held by its guard alone. The province is yours while the town holds.`);
    const h = game();
    const m2 = monthMark(h, ME);
    const c2 = germanCity(h);
    shutUp(h, c2, 0);
    land(h, c2, { siege: true, taken: false, attLost: 9000, defLost: 53782 } as Partial<BattleReport>);
    expect(monthCard(h, m2, ME).lead!.follows).toBe(`${townName(c2)} holds, at a cost of 53,782 men. Only its guard is inside.`);
  });

  it('is neither a battle won nor a battle lost, and the men shut in are in no count of the lost', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const city = germanCity(g);
    shutUp(g, city, 3);
    land(g, city, { siege: true, taken: false, attLost: 39331, defLost: 43295 } as Partial<BattleReport>);
    const card = monthCard(g, mark, ME);
    expect(box(card, 'battles')).toMatchObject({ figure: '0 – 0', sub: 'none fought · 1 town besieged', tone: 'plain' });
    expect(box(card, 'men')).toMatchObject({ figure: '43,295', sub: 'theirs: 39,331' });
    // With a battle lost elsewhere and a second town besieged, the small line says all three.
    const [a] = home(g);
    const capital = capitalOf(g, ME);
    shutUp(g, capital, 2);
    land(g, capital, { siege: true, taken: false, attLost: 100, defLost: 200 } as Partial<BattleReport>);
    land(g, a, { taken: true, attLost: 10, defLost: 20 });
    const c2 = monthCard(g, mark, ME);
    expect(box(c2, 'battles')).toMatchObject({ figure: '0 – 1', sub: '0 won · 1 lost · 2 towns besieged', tone: 'bad' });
    expect(box(c2, 'men')!.figure).toBe(num(43295 + 200 + 20));
  });

  it('with no siege standing as the card is made, speaks of the corps that fought outside and never of a line held', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const city = germanCity(g);
    land(g, city, { siege: true, taken: false, attLost: 700, defLost: 900 } as Partial<BattleReport>);
    const card = monthCard(g, mark, ME);
    const row = order(card).find((r) => r.region === city.id)!;
    expect(row).toMatchObject({ tag: 'BESIEGED', line: 'The field is lost. The town holds out.' });
    expect(row.facts).toEqual([{ n: '1 corps', label: 'fought outside' }, { n: '900', label: 'men lost' }, { n: '700', label: 'of theirs' }]);
    expect(order(card).some((r) => r.facts.some((f) => f.label === 'held the line'))).toBe(false);
    // (No battle won or lost, and no town besieged: the box has nothing to count.)
    expect(box(card, 'battles')).toBeUndefined();
  });

  it('a town that fell after the siege was laid, with no fight of its own, is LOST and a battle lost: it did not hold', () => {
    // (The find on Britain's July 1940: Rouen's guard was broken from the air after the field was lost, the province
    // was German as the card was made, and the row still said HELD, "The town holds out.")
    const g = game();
    const mark = monthMark(g, ME);
    const city = germanCity(g);
    land(g, city, { siege: true, taken: true, attLost: 2635, defLost: 41188 } as Partial<BattleReport>);
    city.ctrl = 'POL';
    const card = monthCard(g, mark, ME);
    const row = order(card).find((r) => r.region === city.id)!;
    expect(row).toMatchObject({ tag: 'LOST', tone: 'bad', line: 'The field was lost, and the town fell after it.', head: `${townName(city)} is lost` });
    expect(row.facts).toEqual([{ n: '1 corps', label: 'fought outside' }, { n: '41,188', label: 'men lost' }, { n: '2,635', label: 'of theirs' }]);
    // Beneath the lead's picture the truth of it comes first, then what the town was worth (the art director's review of 82.1).
    expect(row.follows.startsWith('The field was lost, and the town fell after it. ')).toBe(true);
    expect(row.follows.length).toBeGreaterThan('The field was lost, and the town fell after it. '.length + 8);
    expect(box(card, 'battles')).toMatchObject({ figure: '0 – 1', sub: '0 won · 1 lost' });
    // The besieger's side of the same month: the town he laid siege to is his as the card is made.
    const h = game();
    const m2 = monthMark(h, ME);
    const theirs = h.regions.find((r) => r.owner === 'POL' && r.ctrl === 'POL' && isCity(h, r) && !r.cap)!;
    land(h, theirs, { attNat: ME, defNat: 'POL', attackers: [unit(ME)], defenders: [unit('POL')], siege: true, taken: true, attLost: 900, defLost: 4000 } as Partial<BattleReport>);
    theirs.ctrl = ME;
    const c2 = monthCard(h, m2, ME);
    const won = order(c2).find((r) => r.region === theirs.id)!;
    expect(won).toMatchObject({ tag: 'WON', tone: 'good', line: 'The field was won, and the town fell after it.', head: `${townName(theirs)} is taken` });
    expect(won.follows.startsWith('The field was won, and the town fell after it. ')).toBe(true);
    expect(box(c2, 'battles')).toMatchObject({ figure: '1 – 0' });
    // Still besieged, it is as before: the field is won and the town is under siege.
    const k = game();
    const m3 = monthMark(k, ME);
    const still = k.regions.find((r) => r.owner === 'POL' && r.ctrl === 'POL' && isCity(k, r) && !r.cap)!;
    land(k, still, { attNat: ME, defNat: 'POL', attackers: [unit(ME)], defenders: [unit('POL')], siege: true, taken: true, attLost: 900, defLost: 4000 } as Partial<BattleReport>);
    expect(order(monthCard(k, m3, ME)).find((r) => r.region === still.id)).toMatchObject({ tag: 'WON', line: 'The field is won. The town is besieged.', head: `${townName(still)} is under siege` });
  });

  it('weighs its men shut in, once: a great garrison outranks an ordinary battle lost that it did not outrank before', () => {
    const stage = (inside: number): string[] => {
      const g = game();
      const mark = monthMark(g, ME);
      const city = germanCity(g);
      const [a] = home(g);
      if (inside) shutUp(g, city, inside);
      // The siege: 2,000 lost and a city (67,500) is 69,500. The battle lost in open country: 60,000 doubled.
      land(g, city, { siege: true, taken: false, attLost: 1000, defLost: 2000 } as Partial<BattleReport>);
      land(g, a, { taken: true, attLost: 1000, defLost: 60000 });
      return order(monthCard(g, mark, ME)).filter((r) => r.arm === 'land').map((r) => r.tag);
    };
    expect(stage(0)).toEqual(['LOST', 'BESIEGED']);
    expect(stage(3)).toEqual(['BESIEGED', 'LOST']);
  });

  it('told with the town\'s own fight in the same month, is one row that keeps the assault\'s words and the men shut in', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const city = germanCity(g);
    const shut = shutUp(g, city, 2);
    land(g, city, { siege: true, taken: false, attLost: 1000, defLost: 2000 } as Partial<BattleReport>);
    land(g, city, { town: true, taken: false, attLost: 5000, defLost: 300 } as Partial<BattleReport>);
    const card = monthCard(g, mark, ME);
    const told = order(card).filter((r) => r.region === city.id);
    expect(told).toHaveLength(1);
    expect(told[0]).toMatchObject({ tag: 'HELD', tone: 'good', line: 'An assault on the town was thrown back.' });
    expect(told[0].facts).toEqual([{ n: num(shut), label: 'men shut in' }, { n: '2,300', label: 'men lost' }, { n: '6,000', label: 'of theirs' }]);
    expect(told[0].figures.map((f) => f.n)).toEqual(['2,300', '6,000']);
    // The assault thrown back is a battle won; the siege laid is in neither number.
    expect(box(card, 'battles')).toMatchObject({ figure: '1 – 0', sub: '1 won · 0 lost · 1 town besieged' });
  });

  it('in a later month an assault thrown back weighs as any battle held, and its corps held the line', () => {
    const g = game();
    const city = germanCity(g);
    shutUp(g, city, 3);
    const [a] = home(g);
    const mark = monthMark(g, ME);
    // The assault: 2,000 lost and a city is 69,500, under the battle lost at 120,000 though 135,000 men are inside.
    land(g, city, { town: true, taken: false, attLost: 5000, defLost: 2000 } as Partial<BattleReport>);
    land(g, a, { taken: true, attLost: 1000, defLost: 60000 });
    const card = monthCard(g, mark, ME);
    expect(order(card).filter((r) => r.arm === 'land').map((r) => r.tag)).toEqual(['LOST', 'HELD']);
    const row = order(card).find((r) => r.region === city.id)!;
    expect(row.facts[0]).toEqual({ n: '1 corps', label: 'held the line' });
    expect(box(card, 'battles')).toMatchObject({ sub: '1 won · 1 lost' });
  });
});

describe("ruling 106: the first case of ruling 95(d), rewritten", () => {
  it('France, June 1940, with the men in the towns: the besieged capital leads, the city held is the first row, then the two rings, then the town lost', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const [a, b, c] = home(g);
    const capital = capitalOf(g, ME), city = germanCity(g);
    // (Three corps in each, about the 120,274 and 128,429 men of the real game.)
    const inParis = shutUp(g, capital, 3, 2.6), inLille = shutUp(g, city, 3, 2.8);
    land(g, c, { taken: true, attLost: 5498, defLost: 43209 });
    land(g, city, { siege: true, taken: false, attLost: 23046, defLost: 116735 } as Partial<BattleReport>);
    land(g, capital, { siege: true, taken: false, attLost: 18261, defLost: 48342 } as Partial<BattleReport>);
    ringAt(g, b, 4, '144,102');
    ringAt(g, a, 5, '151,638');
    const card = monthCard(g, mark, ME);
    expect(inParis).toBeGreaterThan(110000);
    expect(inLille).toBeGreaterThan(120000);
    expect(card.lead).toMatchObject({ place: townName(capital), tag: 'BESIEGED', head: `${townName(capital)} is besieged`, follows: `${townName(capital)} holds, at a cost of 48,342 men. 3 corps are shut in. It is your capital: the country will not long outlast it.` });
    expect(card.lead!.facts).toEqual([{ n: num(inParis), label: 'men shut in' }, { n: '48,342', label: 'men lost' }, { n: '18,261', label: 'of theirs' }]);
    expect(card.rows.map((r) => `${r.tag} ${r.place}`)).toEqual([`BESIEGED ${townName(city)}`, `LOST ${a.name}`, `LOST ${b.name}`, `LOST ${c.name}`]);
    expect(card.rows[0]).toMatchObject({ line: 'Polish troops close round the town.', figures: [{ n: num(inLille), label: 'shut in' }] });
    // The score: one battle lost, two towns besieged, and nobody shut in is among the lost.
    expect(box(card, 'battles')).toMatchObject({ figure: '0 – 1', sub: '0 won · 1 lost · 2 towns besieged' });
    expect(box(card, 'men')!.figure).toBe(num(43209 + 116735 + 48342));
  });

  it('a ring that closes on a corps or more is never under the fold, and a lesser ring may be', () => {
    const stage = (cutOff: string): MonthCard => {
      const g = game();
      const mark = monthMark(g, ME);
      const grounds = home(g);
      // Nine battles lost, each heavier than the ring: 60,000 men doubled against 45,000 doubled.
      grounds.slice(1, 10).forEach((r, i) => land(g, r, { taken: true, attLost: 1000, defLost: 60000 + i }));
      ringAt(g, grounds[0], 1, cutOff);
      return monthCard(g, mark, ME);
    };
    const card = stage('45,000');
    expect(card.rows.length).toBeGreaterThan(card.show);
    const at = card.rows.findIndex((r) => /^The ring closes/.test(r.line));
    expect(at).toBe(card.show - 1);
    // (Nothing else is out of its order: the rows above it are the heaviest battles, and the first under the fold is the one it displaced.)
    expect(card.rows.slice(0, card.show - 1).every((r) => r.tag === 'LOST' && r.figures.length === 2)).toBe(true);
    const small = stage('30,000');
    expect(small.rows.findIndex((r) => /^The ring closes/.test(r.line))).toBeGreaterThanOrEqual(small.show);
  });

  it('above news of war and peace that found no band, though never above a warning (b444)', () => {
    // Seven items for the bands: two take them, five are rows above the battles, and the five fill every row that
    // shows beside the lead story. Nine battles lost, each heavier than the ring.
    const stage = (warnings: boolean): MonthCard => {
      const g = game();
      const mark = monthMark(g, ME);
      const grounds = home(g);
      for (let i = 0; i < 7; i++) {
        if (warnings) news(g, { kind: 'event', text: `Warning number ${i + 1} of the month. It needs an answer.`, nat: ME, warn: `Warning ${i + 1}` });
        else news(g, { kind: 'event', text: `News number ${i + 1} of the month.`, head: `News number ${i + 1}`, nat: ME });
      }
      grounds.slice(1, 10).forEach((r, i) => land(g, r, { taken: true, attLost: 1000, defLost: 60000 + i }));
      ringAt(g, grounds[0], 1, '45,000');
      return monthCard(g, mark, ME);
    };
    const ring = (c: MonthCard): number => c.rows.findIndex((r) => /^The ring closes/.test(r.line));
    const byClass = (rs: CardRow[]): boolean => rs.every((r, i) => !i || rs[i - 1].cls <= r.cls);
    const card = stage(false);
    expect(card.bands).toHaveLength(2);
    expect(card.show).toBe(5);
    // The ring takes the last row that shows, and the lightest news is the first under the fold.
    expect(ring(card)).toBe(card.show - 1);
    expect(card.rows.slice(0, card.show - 1).map((r) => r.tag)).toEqual(['NEWS', 'NEWS', 'NEWS', 'NEWS']);
    expect(card.rows[card.show]).toMatchObject({ cls: 2, tag: 'NEWS' });
    expect(byClass(card.rows.slice(0, card.show)) && byClass(card.rows.slice(card.show))).toBe(true);
    // Five warnings in the rows: the ring stays under the fold and every warning shows.
    const warned = stage(true);
    expect(warned.rows.slice(0, warned.show).map((r) => r.tag)).toEqual(['WARNING', 'WARNING', 'WARNING', 'WARNING', 'WARNING']);
    expect(ring(warned)).toBeGreaterThanOrEqual(warned.show);
    expect(byClass(warned.rows)).toBe(true);
  });
});

describe("the art director's two rules for a band", () => {
  it('a warning flies the flag of the country its news is about, and his own where it names none', () => {
    const g = game();
    const mark = monthMark(g, ME);
    warning(g, 'Rome wavers', 'Italy makes peace if the Allies still hold Sardinia in August 1943.', ME, g.byName.Sardinia.id, 'ITA');
    warning(g, 'History\'s book', 'History\'s book is closed in the west.', ME);
    const card = monthCard(g, mark, ME);
    expect(card.bands.map((x) => [x.word, x.title, x.nats])).toEqual([['WARNING', 'Rome wavers', ['ITA']], ['WARNING', 'History\'s book', [ME]]]);
    // (A saved game of an earlier version has no such tag on its warnings: the band falls back to his own flag.)
    const h = game();
    const m2 = monthMark(h, ME);
    news(h, { kind: 'event', warn: 'Coast watch', text: 'Transports are gathering at Dover.', nat: ME });
    expect(monthCard(h, m2, ME).bands[0].nats).toEqual([ME]);
  });

  it('a FREED band is good news when his own side did the freeing, and not when an enemy did', () => {
    const freed = (g: Game, nat: string, by: string): void => {
      const cap = g.nations[nat].capital;
      g.passings.push({ id: g.passings.length + 1, turn: g.turn, week: 2, kind: 'freed', nat, by, how: 'freed', capital: cap, provinces: 1, corps: 0, side: 'allies' } as unknown as Game['passings'][number]);
      news(g, { kind: 'event', week: 2, text: `${g.nations[nat].name} is liberated. ${g.nations[by].adj} troops hold ${g.regions[cap].name}, and ${g.nations[nat].name} returns to the war.`, nat, region: cap, arm: 'land', won: by });
    };
    const g = game();
    const mark = monthMark(g, ME);
    freed(g, 'LUX', ME);
    expect(monthCard(g, mark, ME).bands[0]).toMatchObject({ word: 'FREED', good: true });
    const h = game();
    const m2 = monthMark(h, ME);
    freed(h, 'LUX', 'POL');
    const band = monthCard(h, m2, ME).bands[0];
    expect(band.word).toBe('FREED');
    expect(band.good).toBeUndefined();
  });
});
