// The month's report on one card (Version 82): what both interfaces draw it from (src/campaign/monthcard.ts).
//
// Brian's ask of 2026-11-16: "i dont love the turn update UI telling you what happend. its very plain and hard to
// read". The design director's rulings 64 and 65 say what the card holds and in what order; the art director's brief
// (art-director/turn-report) says how it looks. Display only: the card reads the game and changes nothing in it.
//
// 1. The order (ruling 64): what needs his decision first, his own war before anyone else's, bad news before good.
//    Ground: cities lost, then cities won, then battles, then provinces as one count a front. Then sea and air,
//    losses first. Then goals and honours.
// 2. The red bands: at most two. A great power entering the war or falling takes the first. What finds no band
//    stays red and sits as a row above the first battle row.
// 3. A month with nothing above class 5 says 'A quiet month.' and shows no picture. A month with a band and no
//    fighting says there was no fighting on his fronts, and never 'A quiet month.'.
// 4. About seven rows show, and the rest are folded behind a count.
// 5. The score: a box with nothing to count is left out; green when the month went his way, red when it did not.
// 6. At home, what his staff did, and elsewhere in the war.
import { describe, expect, it } from 'vitest';
import { addCorps, atWar, createGame, declareWar } from '../src/campaign/game';
import { monthCard, monthMark, monthOrders, type CardRow, type MonthCard } from '../src/campaign/monthcard';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { townSize } from '../src/campaign/towns';
import { runTurnAI } from '../src/campaign/turn';
import type { AirReport, BattleReport, Game, Headline, Region, SeaReport, SeaSide, Ships } from '../src/campaign/types';

const ME = 'GER';
/** September 1939 on the wider world with nobody playing, Germany at war with Poland. The card is Germany's. */
const START = ((): string => {
  const g = startOpen(createGame(25, null));
  if (!atWar(g, ME, 'POL')) declareWar(g, ME, 'POL');
  return saveGame(g);
})();
const game = (): Game => loadGame(START)!;
const isCity = (g: Game, r: Region): boolean => ['capital', 'city'].includes(townSize(g, r));
/** Polish provinces Poland holds, without a city: the ground of an ordinary battle. */
const fields = (g: Game): Region[] => g.regions.filter((r) => r.owner === 'POL' && r.ctrl === 'POL' && !isCity(g, r));
const polishCity = (g: Game): Region => g.regions.find((r) => r.owner === 'POL' && r.ctrl === 'POL' && isCity(g, r))!;
const germanCity = (g: Game): Region => g.regions.find((r) => r.owner === ME && r.ctrl === ME && isCity(g, r) && r.id !== g.nations[ME].capital)!;
/** Any ground that is not a city, for a month of many battles. */
const anywhere = (g: Game, n: number): Region[] => g.regions.filter((r) => r.owner !== ME && !isCity(g, r)).slice(0, n);

let nextId = 9000;
const unit = (nat: string, men = 30000): BattleReport['attackers'][number] => ({ name: 'I Corps', nat, kind: 'inf', divs: 3, men, lost: 0, fate: '' });
/** A land battle in the month's reports. Germany attacks and takes the ground unless told otherwise. */
function land(g: Game, at: Region, o: Partial<BattleReport> = {}): BattleReport {
  const attNat = o.attNat ?? ME, defNat = o.defNat ?? 'POL';
  const rep = {
    id: nextId++, turn: g.turn, week: 2, region: at.id, regionName: at.name, attNat, defNat, attackers: [unit(attNat)], defenders: [unit(defNat)],
    lines: [], aPower: 2, dPower: 1, odds: 2, level: 'win', taken: true, attLost: 1000, defLost: 2000, prisoners: 0, ...o,
  } as unknown as BattleReport;
  g.reports.push(rep);
  return rep;
}
const ships = (o: Partial<Ships> = {}): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...o });
const fleet = (nat: string, name: string, has: Partial<Ships>, sunk: Partial<Ships> = {}): SeaSide => ({ nat, fleet: name, admiral: null, ships: ships(has), sunk: ships(sunk) });
function sea(g: Game, o: Partial<SeaReport>): SeaReport {
  const rep = { id: nextId++, turn: g.turn, zone: 0, zoneName: 'the North Sea', attNat: 'UK', defNat: ME, att: [], def: [], lines: [], aPower: 1, dPower: 1, odds: 1, level: 'win', won: true, ...o } as unknown as SeaReport;
  g.seaReports.push(rep);
  return rep;
}
const wing = (nat: string, ftr: number, bmr: number, lostFtr = 0, lostBmr = 0): AirReport['att'][number] => ({ nat, fleet: 'Air Fleet', leader: null, ftr, bmr, lostFtr, lostBmr });
function air(g: Game, at: Region, o: Partial<AirReport>): AirReport {
  const rep = {
    id: nextId++, turn: g.turn, mission: 'bomb', region: at.id, zone: -1, place: at.name, attNat: ME, defNat: 'POL', att: [wing(ME, 40, 80, 0, 6)], def: [wing('POL', 20, 0, 5)],
    lines: [], odds: 1, level: 'win', won: true, sky: false, hold: 0, result: 'Its factories lose a fifth of the month.', ...o,
  } as unknown as AirReport;
  g.airReports.push(rep);
  return rep;
}
const news = (g: Game, h: Partial<Headline> & Pick<Headline, 'kind' | 'text'>): void => { g.headlines.push({ turn: g.turn, week: 2, ...h }); };
/** The card's rows in their order: what found no band, then the lead story, then the rest. */
const order = (c: MonthCard): CardRow[] => [...c.rows.filter((r) => r.cls === 2), ...(c.lead ? [c.lead] : []), ...c.rows.filter((r) => r.cls !== 2)];
const box = (c: MonthCard, key: string): MonthCard['boxes'][number] | undefined => c.boxes.find((b) => b.key === key);

describe('the month card is display only', () => {
  it('reads the game and changes nothing in it', () => {
    const g = game();
    const was = saveGame(g);
    const mark = monthMark(g, ME);
    monthOrders(g, ME);
    expect(saveGame(g)).toBe(was);
    land(g, fields(g)[0]);
    news(g, { kind: 'info', text: 'Research complete: Radar.', nat: ME });
    const now = saveGame(g);
    monthCard(g, mark, ME);
    expect(saveGame(g)).toBe(now);
  });

  it('for a country that is not in the game is an empty quiet month', () => {
    const g = game();
    const card = monthCard(g, monthMark(g, 'XXX'), 'XXX');
    expect(card.quiet).toBe('A quiet month.');
    expect(card.rows).toHaveLength(0);
    expect(card.boxes).toHaveLength(0);
  });
});

describe('the order of the rows (ruling 64)', () => {
  it('ground: a city lost, then a city won, then the battles, then provinces as one count a front', () => {
    const g = game();
    const [a, b, c] = fields(g);
    const mark = monthMark(g, ME);
    land(g, a, { attLost: 9000, defLost: 20000 });
    land(g, polishCity(g), { attLost: 500, defLost: 800 });
    land(g, germanCity(g), { attNat: 'POL', defNat: ME, attLost: 300, defLost: 400 });
    b.ctrl = ME; c.ctrl = ME;
    const card = monthCard(g, mark, ME);
    const rows = order(card);
    expect(rows.map((r) => r.place)).toEqual([germanCity(g).name, polishCity(g).name, a.name, 'The Polish front']);
    expect(rows.map((r) => r.tag)).toEqual(['LOST', 'WON', 'WON', 'WON']);
    expect(rows.map((r) => r.tone)).toEqual(['bad', 'good', 'good', 'good']);
    // The lead story is the first of them, and it has its picture: a city is a town under the guns.
    expect(card.lead).toBe(rows[0]);
    expect(card.lead!.picture).toBe('town');
    expect(card.lead!.sign).toBe('art');
    expect(card.lead!.facts).toHaveLength(3);
    // A land battle carries two figures, ours first; the front's row carries its count.
    expect(rows[2].figures).toEqual([{ n: '9,000', label: 'lost' }, { n: '20,000', label: 'theirs' }]);
    expect(rows[3].figures).toEqual([{ n: '2', label: 'provinces' }]);
    expect(rows[3].line).toBe('Two provinces occupied without a battle.');
    // Each battle row opens its battle report, and shows its place on the map.
    expect(rows[2].report).toEqual({ type: 'land', id: g.reports[g.reports.length - 3].id });
    expect(rows[2].region).toBe(a.id);
  });

  it('a battle he defended shows fieldworks, and one he lost comes before one he won of the same kind', () => {
    const g = game();
    const [a, b] = fields(g);
    const mark = monthMark(g, ME);
    land(g, a, { attLost: 5000, defLost: 9000 });
    land(g, b, { attNat: 'POL', defNat: ME, taken: false, attLost: 4000, defLost: 1000 });
    land(g, germanCity(g), { attNat: 'POL', defNat: ME, taken: false, attLost: 100, defLost: 100 });
    const rows = order(monthCard(g, mark, ME));
    // All three are plain battles (no city changed hands), and all went his way: the larger first, and a fight
    // for a city is the largest.
    expect(rows.map((r) => r.tag)).toEqual(['HELD', 'WON', 'HELD']);
    expect(rows[0].place).toBe(germanCity(g).name);
    expect(rows[2].picture).toBe('fieldworks');
    expect(rows[2].line).toBe('A Polish attack was thrown back.');
    const lost = land(g, fields(g)[2], { taken: false, attLost: 200, defLost: 100 });
    const again = order(monthCard(g, mark, ME));
    expect(again[0].report).toEqual({ type: 'land', id: lost.id });
    expect(again[0].tag).toBe('LOST');
  });

  it('sea and air come after the ground, losses first; goals and honours come last', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'honour', text: 'The Iron Cross for I Corps.', nat: ME });
    air(g, polishCity(g), {});
    sea(g, { att: [fleet('UK', 'Home Fleet', { bb: 2, dd: 4 })], def: [fleet(ME, 'Baltic Fleet', { ca: 2, dd: 3 }, { dd: 2 })], won: true });
    land(g, fields(g)[0]);
    const card = monthCard(g, mark, ME);
    const rows = order(card);
    expect(rows.map((r) => r.cls)).toEqual([3, 4, 4, 5]);
    expect(rows.map((r) => r.arm)).toEqual(['land', 'sea', 'air', null]);
    // The fleet action was lost: it leads its class.
    expect(rows[1]).toMatchObject({ tag: 'LOST', tone: 'bad', sign: 'hull', picture: 'fleet', place: 'the North Sea', line: 'The Home Fleet beat the Baltic Fleet.' });
    expect(rows[1].figures).toEqual([{ n: '2', label: 'destroyers lost' }]);
    // His own raid went home: a bomber's sign, the target's own picture, one figure.
    expect(rows[2]).toMatchObject({ tag: 'HIT', tone: 'good', sign: 'bmr', picture: 'factories' });
    expect(rows[2].figures).toEqual([{ n: '6', label: 'bombers lost' }]);
    expect(rows[2].line).toBe('80 bombers hit its factories.');
    expect(rows[3]).toMatchObject({ sign: 'star', picture: null, place: 'Honour' });
  });

  it('carriers in a fleet action change its sign; a raid on his own ground is HIT in red or HELD in green', () => {
    const g = game();
    const mark = monthMark(g, ME);
    sea(g, { attNat: ME, defNat: 'UK', att: [fleet(ME, 'Baltic Fleet', { bb: 1, dd: 3 })], def: [fleet('UK', 'Home Fleet', { cv: 1, dd: 2 }, { cv: 1 })], won: true });
    const home = germanCity(g);
    air(g, home, { attNat: 'UK', defNat: ME, att: [wing('UK', 0, 60, 0, 4)], def: [wing(ME, 30, 0, 2)], won: true, mission: 'night' });
    air(g, home, { attNat: 'UK', defNat: ME, att: [wing('UK', 0, 60, 0, 9)], def: [wing(ME, 30, 0, 1)], won: false });
    const rows = order(monthCard(g, mark, ME));
    const seaRow = rows.find((r) => r.arm === 'sea')!;
    expect(seaRow).toMatchObject({ sign: 'cv', tag: 'WON', tone: 'good' });
    expect(seaRow.figures).toEqual([{ n: '1', label: 'carrier sunk' }]);
    const raids = rows.filter((r) => r.arm === 'air');
    expect(raids.map((r) => `${r.tag} ${r.tone}`)).toEqual(['HIT bad', 'HELD good']);
    expect(raids[0].line).toBe('Their bombers hit its factories by night.');
    expect(raids[1].figures).toEqual([{ n: '9', label: 'shot down' }]);
  });

  it('news of fighting no battle report tells becomes a row; one a report tells is not told twice', () => {
    const g = game();
    const [a, b] = fields(g);
    const mark = monthMark(g, ME);
    land(g, a);
    news(g, { kind: 'battle', arm: 'land', text: `German troops storm ${a.name}.`, nat: ME, region: a.id, won: ME, lost: 'POL' });
    b.ctrl = ME;
    news(g, { kind: 'event', arm: 'land', text: `Polish engineers blow the bridges at ${b.name}.`, nat: 'POL', region: b.id });
    const rows = order(monthCard(g, mark, ME));
    expect(rows.filter((r) => r.region === a.id)).toHaveLength(1);
    const bridges = rows.find((r) => r.sign === 'eng')!;
    expect(bridges).toMatchObject({ tag: 'BLOCKED', tone: 'warn', picture: 'town', place: b.name });
  });
});

describe('the red bands (classes 1 and 2)', () => {
  it('are two at most: a great power entering the war takes the first, a warning the second', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'war', text: 'Poland calls on Lithuania. Lithuania declares war on Germany.', nat: 'POL' });
    news(g, { kind: 'event', warn: 'Coast watch', text: 'The Home Fleet is gathering transports at Dover.', nat: ME });
    // Italy was at peace as the month opened.
    expect(mark.warring).not.toContain('ITA');
    g.nations.ITA.atWar.push('FRA'); g.nations.FRA.atWar.push('ITA');
    news(g, { kind: 'war', text: 'Italy declares war on France. The Mediterranean is a front.', nat: 'ITA' });
    land(g, fields(g)[0]);
    const card = monthCard(g, mark, ME);
    expect(card.bands.map((b) => b.word)).toEqual(['WAR', 'WARNING']);
    expect(card.bands[0]).toMatchObject({ nats: ['ITA'], head: 'Italy declares war on France' });
    expect(card.bands[1]).toMatchObject({ title: 'Coast watch', nats: [ME] });
    // What found no band stays red and sits above the first battle row.
    expect(card.rows[0]).toMatchObject({ cls: 2, tone: 'bad', kind: 'WAR', picture: null });
    expect(card.rows[0].line).toMatch(/^Poland calls on Lithuania/);
    expect(card.lead!.cls).toBe(3);
    expect(card.quiet).toBe('');
  });

  it('a great power falling takes the first band too, ahead of two warnings', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'event', warn: 'Coast watch', text: 'Transports are gathering at Dover.', nat: ME });
    news(g, { kind: 'event', warn: 'Oil', text: 'The oil will last two months.', nat: ME });
    news(g, { kind: 'fall', text: 'France has fallen.', nat: 'FRA' });
    const card = monthCard(g, mark, ME);
    expect(card.bands.map((b) => b.word)).toEqual(['FALLEN', 'WARNING']);
    expect(card.rows.filter((r) => r.cls === 2)).toHaveLength(1);
  });

  it('a war between two small countries far from his own is no band of his', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'war', text: 'Peru declares war on Ecuador.', nat: 'PER' });
    const card = monthCard(g, mark, ME);
    expect(card.bands).toHaveLength(0);
    expect(card.elsewhere.map((e) => e.nat)).toEqual(['PER']);
  });

  it('a great power is banded for entering the war, once: one already at war that declares another is not', () => {
    const g = game();
    // Britain is at war with Germany as the month opens, and declares war on a far country that is no enemy of his.
    expect(atWar(g, 'UK', ME)).toBe(true);
    const mark = monthMark(g, ME);
    expect(mark.warring).not.toContain('USA');
    g.nations.USA.atWar.push('MEX'); g.nations.MEX.atWar.push('USA');
    news(g, { kind: 'war', text: 'Mexico declares war on the United States.', nat: 'MEX' });
    news(g, { kind: 'war', text: 'The United States declares war on Mexico.', nat: 'USA' });
    news(g, { kind: 'war', text: 'Britain declares war on Peru.', nat: 'UK' });
    const card = monthCard(g, mark, ME);
    // One band names the great power that entered, with both flags; the echo of it is news from elsewhere. And
    // Britain's new war touches his own (Britain is his enemy), so it is war and peace of his: the second band.
    expect(card.bands).toHaveLength(2);
    expect(card.bands[0].nats).toEqual(['MEX', 'USA']);
    expect(card.bands[1].nats).toEqual(['UK']);
    expect(card.rows.filter((r) => r.cls === 2)).toHaveLength(0);
    expect(card.elsewhere.map((e) => e.nat)).toEqual(['USA']);
  });
});

describe('one story is told once, and in the room the card has', () => {
  it('a country that declares war on several does so in one band, his own country named first', () => {
    const g = game();
    const mark = monthMark(g, ME);
    expect(mark.warring).not.toContain('ITA');
    g.nations.ITA.atWar.push('FRA', ME); g.nations.FRA.atWar.push('ITA'); g.nations[ME].atWar.push('ITA');
    news(g, { kind: 'war', text: 'Italy declares war on France.', nat: 'ITA' });
    news(g, { kind: 'war', text: 'Italy declares war on Germany.', nat: 'ITA' });
    const card = monthCard(g, mark, ME);
    expect(card.bands.map((b) => b.head)).toEqual(['Italy declares war on Germany and France']);
    expect(card.rows.filter((r) => r.cls === 2)).toHaveLength(0);
  });

  it('a country falls once: its capital taken and its surrender are one band', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'fall', text: 'Warsaw has fallen to Germany.', nat: 'POL', region: g.nations.POL.capital });
    news(g, { kind: 'fall', text: 'Poland capitulates to Germany.', nat: 'POL' });
    const card = monthCard(g, mark, ME);
    expect(card.bands.map((b) => [b.word, b.head])).toEqual([['FALLEN', 'Poland capitulates to Germany']]);
    expect(card.rows.filter((r) => r.cls === 2)).toHaveLength(0);
  });

  it('what finds no band is a red row named for its country, with the word as its tag', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'event', warn: 'Coast watch', text: 'Transports are gathering at Dover.', nat: ME });
    news(g, { kind: 'event', warn: 'Oil', text: 'The oil will last two months.', nat: ME });
    news(g, { kind: 'fall', text: 'Poland capitulates to Germany.', nat: 'POL' });
    const card = monthCard(g, mark, ME);
    expect(card.bands.map((b) => b.word)).toEqual(['WARNING', 'WARNING']);
    expect(card.rows[0]).toMatchObject({ cls: 2, place: 'Poland', tag: 'FALLEN', tone: 'bad', line: 'Poland capitulates to Germany.' });
  });

  it('a second band takes the room of a row', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'event', warn: 'Coast watch', text: 'Transports are gathering at Dover.', nat: ME });
    for (const r of fields(g).slice(0, 8)) land(g, r);
    expect(monthCard(g, mark, ME).show).toBe(6);
    news(g, { kind: 'event', warn: 'Oil', text: 'The oil will last two months.', nat: ME });
    const card = monthCard(g, mark, ME);
    expect(card.bands).toHaveLength(2);
    expect(card.show).toBe(5);
    expect(card.count).toBe('ALL 8 BATTLES');
  });

  it('a siege laid and the town\'s own fight in the same month are one row with the losses of both', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const city = polishCity(g);
    land(g, city, { siege: true, taken: false, attLost: 1000, defLost: 2000 } as Partial<BattleReport>);
    land(g, city, { town: true, taken: true, attLost: 500, defLost: 300, prisoners: 4000 } as Partial<BattleReport>);
    const card = monthCard(g, mark, ME);
    const told = order(card).filter((r) => r.region === city.id);
    expect(told).toHaveLength(1);
    // (The prisoners are the defender's loss too.)
    expect(told[0].figures.map((f) => f.n)).toEqual(['1,500', '6,300']);
    expect(box(card, 'men')).toMatchObject({ figure: '1,500', sub: 'theirs: 6,300' });
  });

  it('submarines sunk are a headline and a figure, and the row says who sank them', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'battle', arm: 'sea', text: 'The escorts of the Western Approaches Command sink 2 German submarines in the Western Approaches.', nat: 'UK', won: 'UK', lost: ME });
    const card = monthCard(g, mark, ME);
    expect(card.lead).toMatchObject({ place: 'The Western Approaches', tag: 'SUNK', tone: 'bad', head: 'Two German submarines sunk', line: 'Sunk by escorts of the Western Approaches Command.' });
    expect(card.lead!.figures).toEqual([{ n: '2', label: 'submarines lost' }]);
  });

  it('an army cut off is a row with its men as the figure, and an army that lays down its arms says so shortly', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const [a, b] = fields(g);
    news(g, { kind: 'battle', arm: 'land', text: `The ring closes at ${a.name}: 4 Polish and British corps, 128,417 men, are cut off from all supply.`, nat: ME, region: a.id, won: ME, lost: 'POL' });
    news(g, { kind: 'battle', arm: 'land', text: `Cut off and out of supply, the Polish army at ${b.name} lays down its arms: 2 corps, 53,615 men.`, nat: 'POL', region: b.id, won: ME, lost: 'POL' });
    const rows = order(monthCard(g, mark, ME));
    expect(rows.find((r) => r.region === a.id)).toMatchObject({ line: 'The ring closes: 4 corps are cut off.', figures: [{ n: '128,417', label: 'men cut off' }] });
    expect(rows.find((r) => r.region === b.id)).toMatchObject({ line: `The Polish army at ${b.name} lays down its arms.`, figures: [{ n: '53,615', label: 'prisoners' }] });
  });

  it('a row\'s sentence is cut to the room the row has, at a clause or a word', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const [a] = fields(g);
    news(g, { kind: 'battle', arm: 'land', text: `The German armoured corps drive through the gap at ${a.name} for the third week running and the road to the capital lies open before them.`, nat: ME, region: a.id, won: ME, lost: 'POL' });
    for (const r of fields(g).slice(1, 4)) land(g, r);
    const rows = order(monthCard(g, mark, ME));
    for (const r of rows.slice(1)) expect(r.line.length, r.line).toBeLessThanOrEqual(r.figures.length > 1 ? 44 : 54);
    expect(rows.find((r) => r.region === a.id)!.line).toMatch(/…$/);
  });
});

describe('a month without fighting', () => {
  it('with nothing above class 5 says "A quiet month." and shows no picture', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'honour', text: 'The Iron Cross for I Corps.', nat: ME });
    news(g, { kind: 'info', text: 'Research complete: Radar.', nat: ME });
    const card = monthCard(g, mark, ME);
    expect(card.quiet).toBe('A quiet month.');
    expect(card.lead).toBeNull();
    expect(card.bands).toHaveLength(0);
    expect(card.rows.map((r) => r.cls)).toEqual([5]);
    expect(card.research).toEqual(['Radar']);
    // The score keeps only what there is to count.
    expect(card.boxes.map((b) => b.key)).toEqual(['treasury']);
  });

  it('under a band says there was no fighting on his fronts, and never "A quiet month."', () => {
    const g = game();
    const mark = monthMark(g, ME);
    g.nations.SOV.atWar.push('FIN'); g.nations.FIN.atWar.push('SOV');
    news(g, { kind: 'war', text: 'The Soviet Union declares war on Finland.', nat: 'SOV' });
    const card = monthCard(g, mark, ME);
    expect(card.bands).toHaveLength(1);
    expect(card.lead).toBeNull();
    expect(card.quiet).toBe('There was no fighting on your fronts this month.');
  });
});

describe('the fold', () => {
  it('six rows show beside a lead story and the rest wait behind a count', () => {
    const g = game();
    const mark = monthMark(g, ME);
    for (const r of anywhere(g, 9)) land(g, r);
    const card = monthCard(g, mark, ME);
    expect(card.lead).not.toBeNull();
    expect(card.rows).toHaveLength(8);
    expect(card.show).toBe(6);
    expect(card.count).toBe('ALL 9 BATTLES');
    news(g, { kind: 'goal', text: 'Warsaw is yours: a campaign goal is met.', nat: ME });
    expect(monthCard(g, mark, ME).count).toBe('ALL 10 ITEMS');
  });

  it('seven rows or fewer are all shown, with no count', () => {
    const g = game();
    const mark = monthMark(g, ME);
    for (const r of anywhere(g, 7)) land(g, r);
    const card = monthCard(g, mark, ME);
    expect(card.rows).toHaveLength(6);
    expect(card.count).toBe('');
  });
});

describe('the score', () => {
  it('counts provinces, battles, men, ships and aircraft, green or red by how the month went', () => {
    const g = game();
    const [a, b, c] = fields(g);
    const mark = monthMark(g, ME);
    land(g, a, { attLost: 3000, defLost: 7000 }); a.ctrl = ME;
    land(g, b, { attLost: 2000, defLost: 6000 }); b.ctrl = ME;
    land(g, c, { taken: false, attLost: 4000, defLost: 1000 });
    sea(g, { att: [fleet('UK', 'Home Fleet', { bb: 2 })], def: [fleet(ME, 'Baltic Fleet', { dd: 3 }, { dd: 2 })], won: true });
    air(g, polishCity(g), {});
    const card = monthCard(g, mark, ME);
    expect(card.boxes.map((x) => x.key)).toEqual(['provinces', 'battles', 'men', 'sea', 'air', 'treasury']);
    expect(box(card, 'provinces')).toMatchObject({ figure: '+2', sub: '2 won · none lost', tone: 'good' });
    expect(box(card, 'battles')).toMatchObject({ figure: '2 – 1', sub: '2 won · 1 lost', tone: 'good' });
    expect(box(card, 'men')).toMatchObject({ figure: '9,000', sub: 'theirs: 14,000', tone: 'plain' });
    expect(box(card, 'sea')).toMatchObject({ figure: '0 – 2', sub: '0 sunk · 2 lost', tone: 'bad' });
    expect(box(card, 'air')).toMatchObject({ figure: '5 – 6', sub: '5 shot down · 6 lost', tone: 'bad' });
    expect(box(card, 'treasury')!.sub).toMatch(/ in (hand|debt)$/);
  });

  it('men lost turn red only when he lost more than he cost the enemy', () => {
    const g = game();
    const mark = monthMark(g, ME);
    land(g, fields(g)[0], { taken: false, attLost: 8000, defLost: 2000 });
    const card = monthCard(g, mark, ME);
    expect(box(card, 'men')).toMatchObject({ figure: '8,000', tone: 'bad' });
    expect(box(card, 'battles')).toMatchObject({ figure: '0 – 1', tone: 'bad' });
    expect(box(card, 'provinces')).toBeUndefined();
    expect(box(card, 'sea')).toBeUndefined();
  });
});

describe('at home, what his staff did, and elsewhere', () => {
  it('what was on order when he handed the month over and is on order no longer was delivered; what is a month off is next', () => {
    const g = game();
    const N = g.nations[ME];
    const at = germanCity(g);
    N.queue.length = 0;
    N.queue.push({ id: 9901, what: 'corps', kind: 'inf', divs: 2, name: 'XXV Corps', region: at.id, ordered: g.turn - 3, eta: g.turn, cost: 30 });
    N.queue.push({ id: 9902, what: 'corps', kind: 'arm', divs: 2, name: 'XXVI Corps', region: at.id, ordered: g.turn - 1, eta: g.turn + 1, cost: 60 });
    N.queue.push({ id: 9903, what: 'corps', kind: 'inf', divs: 2, name: 'XXVII Corps', region: at.id, ordered: g.turn, eta: g.turn + 4, cost: 30 });
    const mark = monthMark(g, ME);
    expect(mark.orders.map((o) => o.id).sort()).toEqual([9901, 9902, 9903]);
    expect(monthOrders(g, ME)).toEqual(mark.orders);
    // The month passes: the first corps is handed over.
    N.queue.splice(0, 1);
    addCorps(g, ME, 'XXV Corps', 'inf', 2, at.id, {});
    const card = monthCard(g, mark, ME);
    expect(card.home).toHaveLength(2);
    expect(card.home[0]).toMatchObject({ sign: 'inf', count: 1, region: at.id });
    expect(card.home[0].text).toMatch(/ ready$/);
    expect(card.home[0].next).toBeUndefined();
    expect(card.home[1]).toMatchObject({ sign: 'arm', count: 1, next: true });
    expect(card.home[1].text).toMatch(/ next month$/);
  });

  it('an order he struck off himself was not delivered', () => {
    const g = game();
    const N = g.nations[ME];
    N.queue.length = 0;
    N.queue.push({ id: 9911, what: 'corps', kind: 'inf', divs: 2, name: 'XXXI Corps', region: germanCity(g).id, ordered: g.turn - 3, eta: g.turn + 2, cost: 30 });
    const mark = monthMark(g, ME);
    N.queue.length = 0;
    // (The interface keeps his orders again as he ends his month: what he struck off is no longer among them.)
    mark.orders = monthOrders(g, ME);
    expect(monthCard(g, mark, ME).home).toHaveLength(0);
  });

  it('what his staff did carries its desk, and elsewhere has one line a country, a war before a battle', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'info', text: 'Your staff ordered two infantry corps at Essen.', nat: ME, desk: 'build' });
    news(g, { kind: 'battle', text: 'Soviet troops take Viipuri.', nat: 'SOV' });
    news(g, { kind: 'event', text: 'Brazil calls up its reserves.', nat: 'BRA' });
    news(g, { kind: 'war', text: 'Brazil declares war on Argentina.', nat: 'BRA' });
    // (A line that names a country this game does not have is nobody's ally or enemy, and has no flag to show: it stays in the newsreel.)
    news(g, { kind: 'event', text: 'Tannu Tuva mobilises.', nat: 'TUV' });
    news(g, { kind: 'battle', text: 'Soviet troops take Sortavala.', nat: 'SOV' });
    news(g, { kind: 'letter', text: 'We crossed the river at dawn.', nat: ME, by: 'A corporal of I Corps' });
    const card = monthCard(g, mark, ME);
    expect(card.staff).toHaveLength(1);
    expect(card.staff[0]).toMatchObject({ desk: 'build', sign: 'inf' });
    expect(card.elsewhere.map((e) => `${e.nat}: ${e.text}`)).toEqual(['BRA: Brazil declares war on Argentina.', 'SOV: Soviet troops take Viipuri.']);
    expect(card.letters).toEqual([{ text: 'We crossed the river at dawn.', by: 'A corporal of I Corps' }]);
    // What is on the card nowhere is counted: it is all in the newsreel.
    expect(card.more).toBe(3);
  });
});

describe('a year of war with nobody playing, a card a month for four countries', () => {
  it('every card keeps the brief: two bands at most, a pictured lead, a line and a place in every row, the game untouched', { timeout: 240000 }, () => {
    const g = startOpen(createGame(31, null));
    const who = ['GER', 'POL', 'UK', 'FRA'];
    let cards = 0, leads = 0, bands = 0, quiet = 0;
    for (let m = 0; m < 12; m++) {
      const marks = who.map((nat) => monthMark(g, nat));
      runTurnAI(g);
      const was = saveGame(g);
      who.forEach((nat, i) => {
        if (!g.nations[nat].alive) return;
        const card = monthCard(g, marks[i], nat);
        cards++;
        expect(card.month).not.toBe('');
        expect(card.bands.length).toBeLessThanOrEqual(2);
        bands += card.bands.length;
        const rows = order(card);
        // In order of class, the red ones first.
        expect(rows.map((r) => r.cls)).toEqual([...rows.map((r) => r.cls)].sort((a, b) => a - b));
        const fighting = rows.filter((r) => r.cls === 3 || r.cls === 4);
        if (fighting.length) { leads++; expect(card.lead, `${nat} ${card.month}`).not.toBeNull(); expect(card.lead!.picture).not.toBeNull(); expect(card.quiet).toBe(''); expect(card.show).toBe(card.bands.length > 1 ? 5 : 6); }
        else { quiet++; expect(card.lead).toBeNull(); expect(card.quiet).not.toBe(''); expect(card.show).toBe(card.bands.length > 1 ? 6 : 7); if (card.bands.length) expect(card.quiet).not.toBe('A quiet month.'); }
        for (const r of rows) {
          expect(r.place, `${nat} ${card.month}`).not.toBe('');
          expect(r.line, `${nat} ${card.month} ${r.place}`).not.toBe('');
          expect(r.figures.length).toBeLessThanOrEqual(r.report?.type === 'land' ? 2 : 1);
          if (r.report?.type === 'land') expect(r.figures).toHaveLength(2);
          if (r.cls === 3 || r.cls === 4) expect(['good', 'bad', 'warn', 'plain']).toContain(r.tone);
        }
        expect(card.elsewhere.length).toBeLessThanOrEqual(4);
        expect(new Set(card.elsewhere.map((e) => e.nat)).size).toBe(card.elsewhere.length);
        expect(card.boxes.at(-1)!.key).toBe('treasury');
      });
      expect(saveGame(g)).toBe(was);
    }
    // The year had every kind of month in it.
    expect(cards).toBeGreaterThan(30);
    expect(leads).toBeGreaterThan(5);
    expect(bands).toBeGreaterThan(0);
    expect(quiet).toBeGreaterThan(0);
  });
});
