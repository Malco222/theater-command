// The month's report on one card (Version 82): what both interfaces draw it from (src/campaign/monthcard.ts).
//
// Brian's ask of 2026-11-16: "i dont love the turn update UI telling you what happend. its very plain and hard to
// read". The design director's rulings 64 and 65 say what the card holds and in what order; the art director's brief
// (art-director/turn-report) says how it looks. Display only: the card reads the game and changes nothing in it.
//
// 1. The order (ruling 64): what needs his decision first, his own war before anyone else's, bad news before good.
//    Ground, then sea and air, then goals and honours. Within a class the larger first (rulings 84 and 95): what is
//    stamped LOST ranks at twice its weight; on the ground the weight is his side's men, lost or cut off, and the
//    place adds by its rank, a capital more than any ordinary battle of the month; a ring weighs all its men; the
//    provinces are one count a front, after the fighting; at sea a fleet meeting a fleet is never below a single
//    ship lost alone. The pictured lead is the heaviest of ground, sea and air together.
// 2. The red bands: at most two. A great power entering the war or falling takes the first. What finds no band
//    stays red and sits as a row above the first battle row.
// 3. A month with nothing above class 5 says 'A quiet month.' and shows no picture. A month with a band and no
//    fighting says there was no fighting on his fronts, and never 'A quiet month.'.
// 4. About seven rows show, and the rest are folded behind a count.
// 5. The score: a box with nothing to count is left out; green when the month went his way, red when it did not.
// 6. At home, what his staff did, and elsewhere in the war.
import { describe, expect, it } from 'vitest';
import { addCorps, atWar, createGame, declareWar } from '../src/campaign/game';
import { fitFoot, markFor, monthCard, monthMark, monthOrders, openingMark, staffShort, wholeWords, type CardRow, type MonthCard } from '../src/campaign/monthcard';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { townName, townSize } from '../src/campaign/towns';
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
/** Open country, anywhere that is not his: no town to add to a battle's weight. */
const open = (g: Game): Region[] => g.regions.filter((r) => r.owner !== ME && r.ctrl !== ME && !r.cap && townSize(g, r) === 'village');
/** Somebody else's city that is no capital, a town, and the capital of a country that still stands. */
const cityOf = (g: Game, nat: string): Region => g.regions.find((r) => r.owner === nat && r.ctrl === nat && !r.cap && townSize(g, r) === 'city')!;
const capitalOf = (g: Game, nat: string): Region => g.regions[g.nations[nat].capital];
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
  it('ground: a city lost leads a city won, a battle in the field follows, then provinces as one count a front', () => {
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
    // (Version 84.2.6: "WON from ..." and the count as the figure; the line is "No battle was fought.")
    expect(rows[3].line).toBe('No battle was fought.');
    expect(rows[3].figures[0].n).toBe('2');
    // Each battle row opens its battle report, and shows its place on the map.
    expect(rows[2].report).toEqual({ type: 'land', id: g.reports[g.reports.length - 3].id });
    expect(rows[2].region).toBe(a.id);
  });

  it('a battle he defended shows fieldworks, and one he lost leads one he won until the win is twice its size', () => {
    const g = game();
    const [a, b, c] = open(g);
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
    // A loss ranks at twice its weight: 3,000 men lost for nothing lead a win that cost 5,000, and not the fight for a city.
    const lost = land(g, c, { taken: false, attLost: 3000, defLost: 100 });
    const again = order(monthCard(g, mark, ME));
    expect(again.map((r) => r.tag)).toEqual(['HELD', 'LOST', 'WON', 'HELD']);
    expect(again[1].report).toEqual({ type: 'land', id: lost.id });
    // And a small one does not: 200 men lost sit below both.
    const small = land(g, open(g)[3], { taken: false, attLost: 200, defLost: 100 });
    expect(order(monthCard(g, mark, ME)).at(-1)!.report).toEqual({ type: 'land', id: small.id });
  });

  it('the rows keep the ground first, then sea and air, then goals and honours; a fleet action takes the picture from a light battle', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'honour', text: 'The Iron Cross for I Corps.', nat: ME });
    air(g, polishCity(g), {});
    sea(g, { att: [fleet('UK', 'Home Fleet', { bb: 2, dd: 4 })], def: [fleet(ME, 'Baltic Fleet', { ca: 2, dd: 3 }, { dd: 2 })], won: true });
    land(g, fields(g)[0]);
    const card = monthCard(g, mark, ME);
    // (The battle cost a thousand men, less than a corps: the fleet action is the lead story, ruling 95(e).)
    const rows = [card.rows[0], card.lead!, ...card.rows.slice(1)];
    expect(card.rows.map((r) => r.cls)).toEqual([3, 4, 5]);
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

describe('the weight of the news (rulings 84 and 95)', () => {
  /** His own ground: open country, the capital, a city that is no capital. */
  const home = (g: Game): Region[] => g.regions.filter((r) => r.owner === ME && r.ctrl === ME && !r.cap && townSize(g, r) === 'village');
  const ringAt = (g: Game, at: Region, corps: number, men: string, lost = ME): void =>
    news(g, { kind: 'battle', arm: 'land', text: `The ring closes at ${at.name}: ${corps} ${g.nations[lost].adj} corps, ${men} men, are cut off from all supply.`, nat: lost, region: at.id, lost });
  const against = { attNat: 'POL', defNat: ME };

  it('France, June 1940: the two rings are the lead and the first row, and the capital held comes before a city held', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const [a, b, c] = home(g);
    const capital = capitalOf(g, ME), city = germanCity(g);
    land(g, c, { ...against, taken: true, attLost: 5498, defLost: 43209 });
    land(g, city, { ...against, siege: true, taken: false, attLost: 23046, defLost: 116735 } as Partial<BattleReport>);
    land(g, capital, { ...against, siege: true, taken: false, attLost: 18261, defLost: 48342 } as Partial<BattleReport>);
    ringAt(g, b, 4, '144,102');
    ringAt(g, a, 5, '151,638');
    const card = monthCard(g, mark, ME);
    expect(card.lead).toMatchObject({ place: a.name, tag: 'LOST', tone: 'bad', head: `The ring closes at ${a.name}`, follows: '5 German corps are cut off from all supply.', line: 'The ring closes: 5 corps are cut off.' });
    expect(card.lead!.figures).toEqual([{ n: '151,638', label: 'men cut off' }]);
    expect(card.rows.map((r) => `${r.tag} ${r.place}`)).toEqual([`LOST ${b.name}`, `BESIEGED ${townName(capital)}`, `BESIEGED ${townName(city)}`, `LOST ${c.name}`]);
  });

  it('Britain, June 1940: the army cut off leads, the city held sits below, and the fleet action lost comes after the ground', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const [a] = home(g);
    const city = germanCity(g);
    sea(g, { att: [fleet('UK', 'Home Fleet', { bb: 2, dd: 4 })], def: [fleet(ME, 'Baltic Fleet', { ca: 2, dd: 8 }, { dd: 5 })], won: true });
    land(g, city, { ...against, siege: true, taken: false, attLost: 39331, defLost: 53782 } as Partial<BattleReport>);
    ringAt(g, a, 4, '128,417');
    const card = monthCard(g, mark, ME);
    expect(card.lead).toMatchObject({ place: a.name, tag: 'LOST' });
    expect(card.rows.map((r) => `${r.tag} ${r.arm}`)).toEqual(['BESIEGED land', 'LOST sea']);
  });

  it('a capital is never below a lesser place of the same stamp, whatever the lesser place cost', () => {
    const first = (stamp: string, set: (g: Game) => void): string => {
      const g = game();
      const mark = monthMark(g, ME);
      set(g);
      const rows = order(monthCard(g, mark, ME));
      expect(rows.map((r) => r.tag)).toEqual([stamp, stamp]);
      return rows[0].place;
    };
    const g0 = game();
    const berlin = capitalOf(g0, ME), warsaw = capitalOf(g0, 'POL');
    // Held: a hundred men at the capital, two hundred thousand at a city.
    expect(first('HELD', (g) => { land(g, germanCity(g), { ...against, taken: false, attLost: 900, defLost: 200000 }); land(g, capitalOf(g, ME), { ...against, taken: false, attLost: 900, defLost: 100 }); })).toBe(berlin.name);
    // Lost.
    expect(first('LOST', (g) => { land(g, germanCity(g), { ...against, taken: true, attLost: 900, defLost: 150000 }); land(g, capitalOf(g, ME), { ...against, taken: true, attLost: 900, defLost: 100 }); })).toBe(berlin.name);
    // Won.
    expect(first('WON', (g) => { land(g, cityOf(g, 'POL'), { attLost: 90000, defLost: 900 }); land(g, capitalOf(g, 'POL'), { attLost: 500, defLost: 900 }); })).toBe(warsaw.name);
    // And a city lost still leads a capital held when it cost enough: stamps differ, and a loss counts double.
    const g = game();
    const mark = monthMark(g, ME);
    land(g, capitalOf(g, ME), { ...against, taken: false, attLost: 900, defLost: 100 });
    land(g, germanCity(g), { ...against, taken: true, attLost: 900, defLost: 150000 });
    expect(order(monthCard(g, mark, ME)).map((r) => r.tag)).toEqual(['LOST', 'HELD']);
  });

  it('a ring that closes on the enemy is WON and weighs its men once', () => {
    const g = game();
    const [a, b] = open(g).filter((r) => r.owner === 'POL');
    // (His troops stand next to the pocket: the ring is his.)
    g.regions[a.nb[0]].ctrl = ME;
    const mine = monthMark(g, ME);
    news(g, { kind: 'battle', arm: 'land', text: `The ring closes at ${a.name}: 3 Polish corps, 90,000 men, are cut off from all supply.`, nat: 'POL', region: a.id, lost: 'POL' });
    const won = monthCard(g, mine, ME);
    expect(won.lead).toMatchObject({ tag: 'WON', tone: 'good', place: a.name, figures: [{ n: '90,000', label: 'men cut off' }] });
    // His own attack thrown back with 50,000 men lost ranks at 100,000: ahead of a ring of 90,000 won.
    land(g, b, { taken: false, attLost: 50000, defLost: 900 });
    expect(order(monthCard(g, mine, ME)).map((r) => `${r.tag} ${r.place}`)).toEqual([`LOST ${b.name}`, `WON ${a.name}`]);
    // A ring that closes far from his own ground is somebody else's news.
    const far = game();
    const fm = monthMark(far, ME);
    const away = open(far).find((r) => r.owner !== 'POL' && r.nb.every((n) => far.regions[n].ctrl !== ME))!;
    news(far, { kind: 'battle', arm: 'land', text: `The ring closes at ${away.name}: 3 Polish corps, 90,000 men, are cut off from all supply.`, nat: 'POL', region: away.id, lost: 'POL' });
    expect(order(monthCard(far, fm, ME))).toHaveLength(0);
  });

  it('Germany, September 1939: the fleet action won leads the sea and air, and the lost submarine is a row below', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'battle', arm: 'sea', text: 'The escorts of the Western Approaches Fleet sink 1 German submarine in the Western Approaches.', nat: 'UK', won: 'UK', lost: ME });
    sea(g, { attNat: ME, defNat: 'POL', zoneName: 'the Baltic Sea', att: [fleet(ME, 'Swinemunde Fleet', { ca: 2, dd: 6 })], def: [fleet('POL', 'Polish Navy', { dd: 4, ss: 5 }, { dd: 2, ss: 1 })], won: true });
    const card = monthCard(g, mark, ME);
    expect(card.lead).toMatchObject({ kind: 'FLEET ACTION', tag: 'WON', place: 'the Baltic Sea' });
    expect(card.rows.map((r) => `${r.tag} ${r.place}`)).toEqual(['SUNK Western Approaches']);
    // The score at sea counts what the rows say: three sunk in the Baltic, and the submarine the escorts sank.
    expect(card.boxes.find((b) => b.key === 'sea')).toMatchObject({ figure: '3 – 1', sub: '3 sunk · 1 lost' });
  });

  it('a fleet meeting a fleet is never below a single ship lost alone, and a fleet action lost leads a smaller one won', () => {
    const g = game();
    const mark = monthMark(g, ME);
    // A battleship lost alone, and a fleet action in which nobody lost a ship.
    news(g, { kind: 'battle', arm: 'sea', text: 'A submarine torpedoes and sinks a battleship of the Battle Fleet in the North Sea: the fleet has too few destroyers to screen its heavy ships.', nat: ME, lost: ME });
    sea(g, { attNat: ME, defNat: 'UK', zoneName: 'the Norwegian Sea', att: [fleet(ME, 'Battle Fleet', { bb: 1, dd: 3 })], def: [fleet('UK', 'Home Fleet', { ca: 2, dd: 2 })], won: true });
    const card = monthCard(g, mark, ME);
    expect(card.lead).toMatchObject({ kind: 'FLEET ACTION', place: 'the Norwegian Sea' });
    expect(card.rows[0]).toMatchObject({ tag: 'SUNK', tone: 'bad', place: 'North Sea', line: 'Torpedoed: a battleship of the Battle Fleet.', figures: [{ n: '1', label: 'battleship lost' }] });
    // And escorts that sink a submarine of his in the same month as a fleet action against the same navy are still a row.
    news(g, { kind: 'battle', arm: 'sea', text: 'The escorts of the Western Approaches Fleet sink 1 German submarine in the Western Approaches.', nat: 'UK', won: 'UK', lost: ME });
    expect(monthCard(g, mark, ME).rows.map((r) => r.place)).toEqual(['North Sea', 'Western Approaches']);
    // A fleet action lost, smaller than the one won beside it, still comes first: a loss counts double.
    const h = game();
    const hm = monthMark(h, ME);
    sea(h, { attNat: ME, defNat: 'UK', zoneName: 'the Norwegian Sea', att: [fleet(ME, 'Battle Fleet', { bb: 1, dd: 3 })], def: [fleet('UK', 'Home Fleet', { ca: 2, dd: 4 }, { dd: 3 })], won: true });
    sea(h, { att: [fleet('UK', 'Channel Force', { ca: 1, dd: 4 })], def: [fleet(ME, 'Coast Flotilla', { dd: 4 }, { dd: 1 })], won: true, zoneName: 'the English Channel' });
    const two = monthCard(h, hm, ME);
    expect([two.lead!, ...two.rows].map((r) => `${r.tag} ${r.place}`)).toEqual(['LOST the English Channel', 'WON the Norwegian Sea']);
  });

  it('the lead story is the heaviest of ground, sea and air together; a light ground item never takes the picture from the sea', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const [a, b] = open(g);
    // A count of provinces and a great ship sunk: the ship has the picture, and the rows keep the ground first.
    a.ctrl = ME;
    news(g, { kind: 'battle', arm: 'sea', text: 'A submarine torpedoes and sinks a battleship of the Battle Fleet in the North Sea: the fleet has too few destroyers to screen its heavy ships.', nat: ME, lost: ME });
    const light = monthCard(g, mark, ME);
    expect(light.lead).toMatchObject({ arm: 'sea', tag: 'SUNK', tone: 'bad', head: 'A battleship is torpedoed and sunk' });
    expect(light.rows.map((r) => r.arm)).toEqual(['land']);
    // A battle that cost more than a corps beside a fleet action lost with two destroyers: the battle leads.
    sea(g, { att: [fleet('UK', 'Home Fleet', { bb: 2, dd: 4 })], def: [fleet(ME, 'Baltic Fleet', { ca: 2, dd: 3 }, { dd: 2 })], won: true });
    land(g, b, { taken: false, attLost: 60000, defLost: 900 });
    const heavy = monthCard(g, mark, ME);
    expect(heavy.lead).toMatchObject({ arm: 'land', tag: 'LOST', place: b.name });
    expect(heavy.rows.map((r) => r.arm)).toEqual(['land', 'sea', 'sea']);
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
    // (Of a line that says two things, the sentence that names war on his own country is the news.)
    expect(card.rows[0].line).toBe('Lithuania declares war on Germany.');
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

  it('and to a country at peace the same month is one band still: the second declaration is not news from elsewhere', () => {
    const g = game();
    const mark = monthMark(g, 'BRA');
    g.nations.ITA.atWar.push('FRA', 'UK'); g.nations.FRA.atWar.push('ITA'); g.nations.UK.atWar.push('ITA');
    news(g, { kind: 'war', text: 'Italy declares war on France.', nat: 'ITA' });
    news(g, { kind: 'war', text: 'Italy declares war on the United Kingdom.', nat: 'ITA' });
    const card = monthCard(g, mark, 'BRA');
    expect(card.bands.map((b) => b.head)).toEqual(['Italy declares war on France and the United Kingdom']);
    expect(card.elsewhere.map((e) => e.nat)).not.toContain('ITA');
    expect(card.quiet).toBe('There was no fighting on your fronts this month.');
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
    expect(card.count).toBe('ALL 8 REPORTS');
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
    news(g, { kind: 'battle', arm: 'sea', text: 'The escorts of the Western Approaches Fleet sink 2 German submarines in the Western Approaches.', nat: 'UK', won: 'UK', lost: ME });
    const card = monthCard(g, mark, ME);
    expect(card.lead).toMatchObject({ place: 'Western Approaches', tag: 'SUNK', tone: 'bad', head: 'Two German submarines sunk', line: 'Sunk by escorts of the Western Approaches Fleet.' });
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

describe('the art director\'s mends and the 3D builder\'s three points', () => {
  it('for him the news is the declaration on him: that sentence is the band, under the flags of those who declare', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'war', text: 'Germany invades Poland. Britain and France declare war on Germany.', nat: ME });
    const card = monthCard(g, mark, ME);
    expect(card.bands).toHaveLength(1);
    expect(card.bands[0]).toMatchObject({ word: 'WAR', head: 'Britain and France declare war on Germany', nats: ['UK', 'FRA'] });
    // A band has a place to show: the capital of the first country it names that is not his own.
    expect(card.bands[0].region).toBe(g.nations.UK.capital);
    // To Poland the same line is the invasion, and the place is Berlin.
    const pol = monthCard(g, mark, 'POL');
    expect(pol.bands[0]).toMatchObject({ head: 'Germany invades Poland', region: g.nations.GER.capital });
  });

  it('several countries that declare war on him in a month do so in one band; his own declaration comes after it', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'war', text: 'Germany declares war on Denmark.', nat: ME });
    news(g, { kind: 'war', text: 'The United Kingdom declares war on Germany.', nat: 'UK' });
    news(g, { kind: 'war', text: 'France declares war on Germany.', nat: 'FRA' });
    const card = monthCard(g, mark, ME);
    expect(card.bands.map((b) => b.head)).toEqual(['The United Kingdom and France declare war on Germany', 'Germany declares war on Denmark']);
    expect(card.bands[0].nats).toEqual(['UK', 'FRA']);
    expect(card.bands[1].region).toBe(g.nations.DEN.capital);
    expect(card.rows.filter((r) => r.cls === 2)).toHaveLength(0);
  });

  it('a country that falls is shown at its capital', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'fall', text: 'Poland capitulates to Germany.', nat: 'POL' });
    expect(monthCard(g, mark, ME).bands[0]).toMatchObject({ word: 'FALLEN', region: g.nations.POL.capital });
  });

  it('what the staff did is said as the desk and the deed, in about forty letters and never a cut name', () => {
    const said: [string, string, string][] = [
      ['research', 'Your advisers put 2 research teams to work on Research institutes (6 months) and Ocean-going submarines (5 months).', 'Research: 2 teams set to work'],
      ['research', 'Your advisers put a research team to work on Medium bombers (5 months).', 'Research: 1 team on Medium bombers'],
      ['research', 'Your advisers put a research team to work on Long-range escort fighters and drop tanks (9 months).', 'Research: 1 team set to work'],
      ['build', 'Your staff ordered I Commonwealth Corps at London.', 'Build: I Commonwealth Corps, London'],
      ['build', 'Your staff ordered anti-aircraft guns at Midlands and the cooperative farms at Leeds.', 'Build: anti-aircraft guns and 1 more'],
      ['build', 'Your staff ordered anti-aircraft guns at Ruhr.', 'Build: anti-aircraft guns, Ruhr'],
      ['air', 'Your air staff flew a raid: the factories at Hamburg. It cost 4 aircraft.', 'Air: a raid on Hamburg\'s factories'],
      ['air', 'Your air staff flew a raid: the ships in harbour at Danzig Corridor. It cost 2 aircraft.', 'Air: a raid on Danzig Corridor\'s harbour'],
      ['air', 'Your air staff flew 3 raids: the factories at Lyon, the factories at Polish Silesia and the factories at Krakow. It cost 15 aircraft.', 'Air: 3 raids, 15 aircraft lost'],
      ['navy', 'Your admirals shelled Danzig Corridor.', 'Navy: shelled Danzig Corridor'],
      ['navy', 'Your admirals sent the Battle Fleet into the Baltic Sea and shelled Danzig Corridor.', 'Navy: the Battle Fleet to the Baltic Sea'],
      ['navy', 'Your admirals brought the Battle Fleet into harbour.', 'Navy: brought the Battle Fleet into harbour'],
      ['diplomacy', 'Your envoys courted Turkey, which has moved a step towards us.', 'Diplomacy: Turkey moves a step towards us'],
      ['diplomacy', 'Your envoys began courting Argentina.', 'Diplomacy: began courting Argentina'],
      ['spies', 'Your secret service recruited agent Chalice and sent Bellows to Berlin to read the books.', 'Spies: recruited agent Chalice and 1 more'],
    ];
    for (const [desk, text, short] of said) {
      expect(staffShort(desk, text), text).toBe(short);
      expect(staffShort(desk, text).length).toBeLessThanOrEqual(44);
    }
    // And on the card the short form stands beside the whole line.
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'info', text: said[3][1], nat: ME, desk: 'build' } as Parameters<typeof news>[1]);
    expect(monthCard(g, mark, ME).staff[0]).toMatchObject({ short: said[3][2], text: said[3][1], desk: 'build' });
  });

  it('the 3D game\'s card gives every line whole: its rows and boxes wrap, so no sentence is cut with three dots (its UI director, build 1628)', () => {
    const g = game();
    const mark = monthMark(g, ME);
    // (two such fights: one is the lead story, told whole on either card, and the other a row)
    const storm = (at: Region): string => `German troops storm ${at.name} after a long fight in its streets and take its bridges whole.`;
    for (const at of fields(g).slice(0, 2)) news(g, { kind: 'battle', arm: 'land', text: storm(at), nat: ME, region: at.id, won: ME, lost: 'POL' });
    news(g, { kind: 'info', text: 'Your secret service sent Heron to London to read the books.', nat: ME, desk: 'spies' } as Parameters<typeof news>[1]);
    // (the browser's card, in its typewriter rows, cuts them)
    const cut = monthCard(g, mark, ME);
    const row = cut.rows.find((r) => r.region !== undefined && /^German troops storm /.test(r.line))!;
    expect(row.line).toMatch(/…$/);
    expect(cut.staff[0].short).toBe('Spies: sent Heron to London to read…');
    // (the 3D game's does not)
    const whole = monthCard(g, mark, ME, { whole: true });
    expect(whole.rows.find((r) => r.region === row.region)!.line).toBe(storm(g.regions[row.region!]));
    expect(whole.staff[0].short).toBe('Spies: sent Heron to London to read the books');
    for (const r of [...whole.rows, ...(whole.lead ? [whole.lead] : [])]) expect(r.line, r.line).not.toMatch(/…$/);
    for (const x of [...whole.staff.map((y) => y.short), ...whole.also, ...whole.elsewhere.map((y) => y.text)]) expect(x, x).not.toMatch(/…$/);
    // A foot box's lines are each whole, as many as it has lines.
    const long = 'German troops close round Lille: the city is under siege, with one corps inside';
    expect(fitFoot([long, long], Infinity, 2, 3)).toEqual([{ text: `${long}.`, at: 0 }, { text: `${long}.`, at: 1 }]);
    expect(staffShort('spies', 'Your secret service recruited agent Chalice and sent Bellows to Berlin to read the books.', Infinity)).toBe('Spies: recruited agent Chalice and 1 more');
  });

  it('a line in a foot box keeps to whole words, never half a name, and a cut shows itself', () => {
    // (The room is a count of letters to a line, and two lines: the boxes are set in a typewriter face.)
    expect(wholeWords('Engineers finish the bridge at Lodz.', 30)).toBe('Engineers finish the bridge at Lodz.');
    // The whole sentence where it wraps into two lines.
    expect(wholeWords('German troops close round Paris: the capital of France is under siege.', 36)).toBe('German troops close round Paris: the capital of France is under siege.');
    // Else whole words, and the cut shows itself: never a full stop where the sentence did not end.
    expect(wholeWords('German troops close round Paris: the capital of France is under siege, with 3 corps inside.', 36)).toBe('German troops close round Paris: the capital of France is under siege…');
    // Else whole words: not a cut word, not a word that leads nowhere, not the first half of a name.
    expect(wholeWords('The coast watch reports that the invasion barges are gathering in the harbours of Danzig Corridor', 44)).toBe('The coast watch reports that the invasion barges are gathering in the harbours…');
    expect(wholeWords('The staff has studied the lost battle and believes the attack went in against Polish Silesia too early', 40)).toBe('The staff has studied the lost battle and believes the attack went…');
    expect(wholeWords('The staff believes the attack went in against Polish Silesia too early', 30)).toBe('The staff believes the attack went…');
    const wraps = (text: string, per: number): number => { let n = 1, at = 0; for (const w of text.split(' ')) { if (at && at + 1 + w.length > per) { n++; at = w.length; } else at += (at ? 1 : 0) + w.length; } return n; };
    for (const per of [20, 26, 30, 36, 40, 44]) {
      const cut = wholeWords('Your envoys report that the government of the United Kingdom has offered Portugal and the Soviet Union a trade agreement on wolfram', per);
      expect(wraps(cut, per), `${per}: ${cut}`).toBeLessThanOrEqual(2);
      expect(cut, `${per}`).not.toMatch(/\b(United|Soviet|the|of|and|a|has|that)[….]$/);
    }
    // On the card: other news of home is given whole where the box has the lines, and the whole line of news is kept for a hover.
    const g = game();
    const mark = monthMark(g, ME);
    const long = 'The staff has studied the lost battle at Polish Silesia and finds that the attack went in too early. It asks for a month to rest the corps.';
    news(g, { kind: 'info', text: long, nat: ME });
    const card = monthCard(g, mark, ME);
    expect(card.also).toEqual(['The staff has studied the lost battle at Polish Silesia and finds that the attack went in too early.']);
    expect(card.alsoFull).toEqual([long]);
  });

  it('a sea row has its sea, so the pin can show it on the chart', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const baltic = g.seas.findIndex((z) => /Baltic/.test(z.name));
    sea(g, { attNat: ME, defNat: 'POL', zone: baltic, zoneName: g.seas[baltic].name, att: [fleet(ME, 'Swinemunde Fleet', { ca: 2, dd: 6 })], def: [fleet('POL', 'Polish Navy', { dd: 4 }, { dd: 2 })], won: true });
    news(g, { kind: 'battle', arm: 'sea', text: `The escorts of the Western Approaches Fleet sink 1 German submarine in the ${g.seas[0].name.replace(/^the /i, '')}.`, nat: 'UK', won: 'UK', lost: ME });
    const card = monthCard(g, mark, ME);
    expect(card.lead).toMatchObject({ arm: 'sea', sea: baltic });
    expect(card.lead!.region).toBeUndefined();
    expect(card.rows[0]).toMatchObject({ arm: 'sea', sea: 0 });
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
    // (One word on the fold, whatever is behind it.)
    expect(card.count).toBe('ALL 9 REPORTS');
    news(g, { kind: 'goal', text: 'Warsaw is yours: a campaign goal is met.', nat: ME });
    expect(monthCard(g, mark, ME).count).toBe('ALL 10 REPORTS');
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

  it('guns and airfields are not counted: the line says what is ready, with the place and its level under it (ruling 133(g))', () => {
    const g = game();
    const N = g.nations[ME];
    const at = germanCity(g);
    N.queue.length = 0;
    N.queue.push({ id: 9921, what: 'work', item: 'aa', name: `Anti-aircraft guns at ${at.name}`, region: at.id, ordered: g.turn - 2, eta: g.turn, cost: 20 } as never);
    N.queue.push({ id: 9922, what: 'work', item: 'field', name: `Airfield at ${at.name}`, region: at.id, ordered: g.turn - 2, eta: g.turn, cost: 20 } as never);
    const mark = monthMark(g, ME);
    expect(mark.orders.map((o) => o.label).sort()).toEqual(['airfields', 'anti-aircraft guns']);
    // The month passes: both are finished, the guns at their second level.
    N.queue.length = 0;
    g.flags[`wk:aa:${at.id}`] = 2;
    g.flags[`wk:field:${at.id}`] = 1;
    // (The month's own news of the same two works, as the Arsenal tells it, and a line of other news.)
    news(g, { kind: 'info', text: `The anti-aircraft guns at ${at.name} are in place: level 2.`, nat: ME });
    news(g, { kind: 'info', text: `The airfields at ${at.name} are ready: level 1.`, nat: ME });
    news(g, { kind: 'info', text: 'A new bridge is open at Cologne.', nat: ME });
    const card = monthCard(g, mark, ME);
    // What stands among the deliveries is not said a second time as news; other news is.
    expect(card.alsoFull.join(' ')).not.toMatch(/are in place|are ready/);
    expect(card.alsoFull).toContain('A new bridge is open at Cologne.');
    expect(card.home).toHaveLength(2);
    expect(card.home.find((h) => h.sign === mark.orders.find((o) => o.label === 'anti-aircraft guns')!.mark)).toMatchObject({ text: 'Anti-aircraft guns ready', place: `${townName(at)}, level 2`, region: at.id, bare: true });
    expect(card.home.map((h) => h.text).sort()).toEqual(['Airfields finished', 'Anti-aircraft guns ready']);
    expect(card.home.every((h) => !/^\d/.test(h.text))).toBe(true);
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
    // The soldier is for infantry corps only: works and the other arms take the star (the art director's review of 82.1),
    // and anti-aircraft guns take her gun sign (her brief of the raid card's defences).
    for (const [text, sign] of [['Your staff ordered anti-aircraft guns at Ruhr.', 'aa'], ['Your staff ordered an armoured corps at Essen.', 'star'], ['Your staff ordered a factory at Essen and two infantry corps at Berlin.', 'star'], ['Your staff ordered I Commonwealth Corps at London.', 'inf']] as const) {
      const k = game();
      const mk = monthMark(k, ME);
      news(k, { kind: 'info', text, nat: ME, desk: 'build' });
      expect(monthCard(k, mk, ME).staff[0].sign, text).toBe(sign);
    }
    expect(card.elsewhere.map((e) => `${e.nat}: ${e.text}`)).toEqual(['BRA: Brazil declares war on Argentina.', 'SOV: Soviet troops take Viipuri.']);
    expect(card.letters).toEqual([{ text: 'We crossed the river at dawn.', by: 'A corporal of I Corps' }]);
    // What is on the card nowhere is counted: it is all in the newsreel.
    expect(card.more).toBe(3);
  });

  it('a line that opens with an abbreviation is not cut at its full stop (the Playtester, Version 82.1)', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'battle', text: 'No. 1 Bomber Group bombs Viipuri. The docks burn, and St. Olaf is lost.', nat: 'SOV' });
    const said = monthCard(g, mark, ME).elsewhere.map((e) => e.text);
    expect(said).toHaveLength(1);
    expect(said[0].replace(/\.$/, '')).toBe('No. 1 Bomber Group bombs Viipuri');
  });

  it('nor one that opens with a German ordinal, and a number that ends a sentence still ends it (the third playtester, 84.3.1)', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'battle', text: '2. Fliegerdivision bombs Rotterdam. The docks burn.', nat: 'SOV' });
    news(g, { kind: 'battle', text: 'The 10. Armee is cut down to 2. The rest are taken.', nat: 'BRA' });
    const said = monthCard(g, mark, ME).elsewhere.map((e) => e.text.replace(/\.$/, ''));
    expect(said).toContain('2. Fliegerdivision bombs Rotterdam');
    expect(said).toContain('The 10. Armee is cut down to 2');
  });

  it('an air formation\'s ordinal holds its sentence together wherever it stands (ruling 220\'s names)', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'battle', text: '3. Fliegerdivision and 1. Jagddivision strike the Home Fleet in the North Sea. Ships were hit.', nat: 'SOV' });
    news(g, { kind: 'battle', text: 'Kampfgeschwader 1 and 2. Jagddivision raid the French troops at Alsace. The raids cut the roads.', nat: 'BRA' });
    news(g, { kind: 'battle', text: 'Losses were 1 and 2. The rest came home.', nat: 'ARG' });
    const said = monthCard(g, mark, ME).elsewhere.map((e) => e.text.replace(/\.$/, ''));
    expect(said).toContain('3. Fliegerdivision and 1. Jagddivision strike the Home Fleet in the North Sea');
    expect(said).toContain('Kampfgeschwader 1 and 2. Jagddivision raid the French troops at Alsace');
    expect(said).toContain('Losses were 1 and 2');
  });

  it('several countries that declare war on him share one band, a sentence with its verb, "the" in lower case past the first', () => {
    const g = game();
    const mark = monthMark(g, ME);
    for (const nat of ['CUB', 'DOM']) declareWar(g, nat, ME);
    expect(monthCard(g, mark, ME).bands[0].head).toBe('Cuba and the Dominican Republic declare war on Germany');
    const h = game();
    const mark2 = monthMark(h, ME);
    const many = ['CUB', 'DOM', 'HAI', 'GUA', 'HON', 'NIC', 'COS', 'PAN', 'ELS', 'ECU'];
    for (const nat of many) declareWar(h, nat, ME);
    const head = monthCard(h, mark2, ME).bands[0].head;
    expect(head.length).toBeLessThanOrEqual(76);
    expect(head).toMatch(/^Cuba, the Dominican Republic, .+ and \d+ more declare war on Germany$/);
  });
});

describe('a year of war with nobody playing, a card a month for four countries', () => {
  it('every card keeps the brief: two bands at most, a pictured lead, a line and a place in every row, the game untouched', { timeout: 240000 }, () => {
    // (Seed 7 since the computer's air forces fight back, airwar.json ai.back: with air battles in more months, seeds 1
    // to 12 and 31 gave these four no quiet month but seed 7, Britain's fourth. Seed 31 before.)
    const g = startOpen(createGame(7, null));
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
        // The rows in order of class, the red ones first, above the fold and below it (a ring of his lifted above the
        // fold may put a row of war and peace under it: ruling 106, b444); the lead story is the heaviest of ground, sea and air.
        for (const part of [card.rows.slice(0, card.show), card.rows.slice(card.show)]) expect(part.map((r) => r.cls)).toEqual([...part.map((r) => r.cls)].sort((a, b) => a - b));
        const fighting = rows.filter((r) => r.cls === 3 || r.cls === 4);
        if (fighting.length) { leads++; expect(card.lead, `${nat} ${card.month}`).not.toBeNull(); if (!card.lead!.stake) expect(card.lead!.picture).not.toBeNull(); expect(card.quiet).toBe(''); expect(card.show).toBe(card.bands.length > 1 ? 5 : 6); }
        else { quiet++; expect(card.lead).toBeNull(); expect(card.quiet).not.toBe(''); expect(card.show).toBe(card.bands.length > 1 ? 6 : 7); if (card.bands.length) expect(card.quiet).not.toBe('A quiet month.'); }
        for (const r of rows) {
          expect(r.place, `${nat} ${card.month}`).not.toBe('');
          expect(r.line, `${nat} ${card.month} ${r.place}`).not.toBe('');
          expect(r.figures.length).toBeLessThanOrEqual(r.report?.type === 'land' ? 2 : 1);
          if (r.report?.type === 'land') expect(r.figures).toHaveLength(r.figures[0]?.label === 'shut in' ? 1 : 2);
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

describe('the first month of a war (the card for September 1939)', () => {
  const fresh = (human: string): Game => { const g = startOpen(createGame(17, human)); g.human = human; return g; };
  it('carries the declarations that opened the war, though they were made before his first month', () => {
    const g = fresh('GER');
    expect(monthCard(g, monthMark(g)).bands).toEqual([]);
    const card = monthCard(g, markFor(g));
    expect(card.bands).toHaveLength(1);
    expect(card.bands[0]).toMatchObject({ word: 'WAR', nats: ['UK', 'FRA'], head: 'Britain and France declare war on Germany', region: g.nations.UK.capital });
  });
  it('for another country the headline is the invasion, under the flag of the country that made it, shown at the country invaded', () => {
    for (const me of ['UK', 'FRA', 'USA']) {
      const g = fresh(me);
      const card = monthCard(g, markFor(g));
      expect(card.bands[0], me).toMatchObject({ word: 'WAR', nats: ['GER'], head: 'Germany invades Poland', region: g.nations.POL.capital });
    }
    const g = fresh('POL');
    expect(monthCard(g, markFor(g)).bands[0]).toMatchObject({ nats: ['GER'], head: 'Germany invades Poland', region: g.nations.GER.capital });
  });
  it('from the second month the mark is the month as it opens', () => {
    const g = fresh('GER');
    g.turn = 2;
    expect(markFor(g).h).toBe(g.headlines.length);
    expect(markFor(g).warring).toContain('GER');
    expect(openingMark(g)).toMatchObject({ h: 0, r: 0, s: 0, a: 0, shore: 0, warring: [] });
  });
  it('a line that names two places with "between" ends before the word when it has room for one', () => {
    expect(wholeWords('Polish troops blow the bridges over the Vistula between Danzig Corridor and West Prussia as they fall back', 36)).toBe('Polish troops blow the bridges over the Vistula…');
  });
});

describe("the foot boxes give a line whole where the box has room (the art director's rider)", () => {
  it('each line is whole while the lines last, and the last is cut where they will not fit', () => {
    const a = 'Polish troops blow the bridges over the Vistula between Danzig Corridor and Elbing', b = 'Force H forms at Gibraltar';
    // Six lines of 36 letters: three lines and one.
    expect(fitFoot([a, b], 36, 6, 3)).toEqual([{ text: `${a}.`, at: 0 }, { text: `${b}.`, at: 1 }]);
    // Two lines: the first is cut after a whole word, with the mark of a cut, and the box ends there.
    expect(fitFoot([a, b], 36, 2, 3)).toEqual([{ text: 'Polish troops blow the bridges over the Vistula…', at: 0 }]);
  });
  it('a line that loses its point when cut is not cut: the next is shown in its place', () => {
    const lille = 'Your staff on the battle at Lille: the line was dug in and it still broke', short = 'Force H forms at Gibraltar';
    expect(fitFoot([lille, short], 30, 2, 3)).toEqual([{ text: `${short}.`, at: 1 }]);
    expect(fitFoot([lille, short], 35, 6, 3).map((x) => x.text)).toEqual([`${lille}.`, `${short}.`]);
    // A line with a colon may be cut once the first clause after the colon is whole; and no cut ends on a word that leads nowhere.
    expect(fitFoot(['German troops close round Lille: the city is under siege, with one corps inside'], 36, 2, 3)).toEqual([{ text: 'German troops close round Lille: the city is under siege, with one corps…', at: 0 }]);
    expect(fitFoot(['German troops seize the bridges over the Meuse at Aube before they can be blown'], 36, 2, 3)).toEqual([{ text: 'German troops seize the bridges over the Meuse at Aube…', at: 0 }]);
    // (A cut that keeps less than most of the sentence says nothing either.)
    expect(fitFoot(['The staff believes the attack went in against Polish Silesia a month too early and with too few guns'], 30, 1, 3)).toEqual([]);
  });
  it('at home: a directive is said by its title, from the edge of the box, and a quiet card has the lines for it', () => {
    const g = game();
    const mark = monthMark(g, ME);
    g.nations[ME].gov = { ...(g.nations[ME].gov ?? {}), dir: { id: g.turn, nat: ME, kind: 'agent', from: ME, title: 'Recruit an agent', text: 'x', until: g.turn + 5 } } as never;
    news(g, { kind: 'info', text: 'A directive from the war cabinet: The war cabinet wants eyes abroad: recruit an agent for the secret service by the end of March 1940. One costs 600 to find and train. The reward: 3 political capital.', nat: ME });
    const card = monthCard(g, mark, ME);
    expect(card.also).toEqual(['A directive from the war cabinet: Recruit an agent.']);
    expect(card.alsoFull[0]).toContain('One costs 600');
  });
  it('where the town held, the staff speaks of the battle there, whole', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const city = germanCity(g);
    // The field is lost and the town holds: the row is stamped BESIEGED (HELD before Version 84.2.6).
    land(g, city, { attNat: 'POL', defNat: ME, attackers: [unit('POL')], defenders: [unit(ME)], taken: true, siege: true } as never);
    news(g, { kind: 'info', text: `Your staff on the loss of ${city.name}: the line was dug in and it still broke. Artillery standing in a line makes every corps beside it fight stronger.`, nat: ME });
    const card = monthCard(g, mark, ME);
    const held = [card.lead, ...card.rows].some((r) => r?.place === city.name && (r.tag === 'HELD' || r.tag === 'BESIEGED'));
    expect(card.also).toEqual([`Your staff on the ${held ? 'battle at' : 'loss of'} ${city.name}: the line was dug in and it still broke.`]);
  });
  it('marches: a halt comes first, and armies that arrive at one place are one line', () => {
    const g = game();
    const mark = monthMark(g, ME);
    news(g, { kind: 'info', text: 'Berlin garrison has arrived at Frankfurt.', nat: ME });
    news(g, { kind: 'info', text: '1st Engineer Corps has arrived at Frankfurt.', nat: ME });
    news(g, { kind: 'info', text: '2nd Engineer Corps has arrived at Frankfurt.', nat: ME });
    news(g, { kind: 'info', text: 'IX Corps has halted at Saxony on its march to Vienna: the way is closed. It waits for orders.', nat: ME });
    const card = monthCard(g, mark, ME);
    expect(card.also).toEqual(['IX Corps has halted at Saxony on its march to Vienna: the way is closed.', 'Berlin garrison and 2 more corps have arrived at Frankfurt.']);
  });
});

describe("the 3D builder's two wording finds", () => {
  const freed = (g: Game, nat: string, by: string, week: number): void => {
    const cap = g.nations[nat].capital;
    g.passings.push({ id: g.passings.length + 1, turn: g.turn, week, kind: 'freed', nat, by, how: 'freed', capital: cap, provinces: 1, corps: 0, side: 'allies' } as unknown as Game['passings'][number]);
    news(g, { kind: 'event', week, text: `${g.nations[nat].name} is liberated. ${g.nations[by].adj} troops hold ${g.regions[cap].name}, and ${g.nations[nat].name} returns to the war.`, nat, region: cap, arm: 'land', won: by });
  };
  const fell = (g: Game, nat: string, by: string, week: number): void => {
    g.passings.push({ id: g.passings.length + 1, turn: g.turn, week, kind: 'fell', nat, by, how: 'beaten', capital: g.nations[nat].capital, provinces: 1, weeks: (g.turn - 1) * 4 + week } as unknown as Game['passings'][number]);
    news(g, { kind: 'fall', week, text: `${g.nations[nat].name} capitulates to ${g.nations[by].name}.`, nat, arm: 'land', won: by, lost: nat });
  };
  const tagged = (c: MonthCard, tag: string): CardRow[] => order(c).filter((r) => r.tag === tag);

  it('a country that falls is FALLEN though it was freed earlier the same month, under the flag of whoever took it', () => {
    const g = game();
    const mark = monthMark(g, ME);
    // Poland frees Luxembourg in the first week, and Germany takes it back in the third.
    freed(g, 'LUX', 'POL', 1);
    fell(g, 'LUX', ME, 3);
    const card = monthCard(g, mark, ME);
    expect(card.bands.map((b) => [b.word, b.head, b.nats])).toEqual([['FALLEN', 'Luxembourg capitulates to Germany', ['LUX', ME]]]);
    expect(tagged(card, 'FREED')).toEqual([]);
    // With two warnings above it the fall is a red row, and it is FALLEN there too.
    const h = game();
    const m2 = monthMark(h, ME);
    news(h, { kind: 'event', warn: 'Coast watch', text: 'Transports are gathering at Dover.', nat: ME });
    news(h, { kind: 'event', warn: 'Oil', text: 'The oil will last two months.', nat: ME });
    freed(h, 'LUX', 'POL', 1);
    fell(h, 'LUX', ME, 3);
    const c2 = monthCard(h, m2, ME);
    expect(tagged(c2, 'FREED')).toEqual([]);
    expect(tagged(c2, 'FALLEN')[0]).toMatchObject({ place: 'Luxembourg', ours: 'LUX', theirs: ME, tone: 'bad', line: 'Luxembourg capitulates to Germany.' });
  });

  it('a liberation that touches his war is FREED, under the flags of the country and of who freed it, and is no row besides', () => {
    const g = game();
    const mark = monthMark(g, ME);
    freed(g, 'LUX', 'POL', 2);
    const card = monthCard(g, mark, ME);
    expect(card.bands).toHaveLength(1);
    expect(card.bands[0]).toMatchObject({ word: 'FREED', head: 'Luxembourg is liberated', nats: ['LUX', 'POL'], region: g.nations.LUX.capital });
    expect(order(card).filter((r) => /liberated/.test(r.line))).toEqual([]);
    // Freed and fallen: whichever came later is the news.
    const h = game();
    const m2 = monthMark(h, ME);
    fell(h, 'LUX', ME, 1);
    freed(h, 'LUX', 'POL', 3);
    expect(monthCard(h, m2, ME).bands.map((b) => b.word)).toEqual(['FREED']);
  });

  it('news dated after the month turned names its month, on a band and on a row', () => {
    const g = game();
    const mark = monthMark(g, ME);
    const [a, b] = fields(g);
    land(g, a);
    fell(g, 'LUX', ME, 4);
    // The month turns: those who move before him have their first week of October.
    g.turn++;
    fell(g, 'POL', ME, 1);
    land(g, b, { week: 1 });
    const card = monthCard(g, mark, ME);
    expect(card.month).toBe('September 1939');
    const band = (nat: string) => card.bands.find((x) => x.nats[0] === nat)!;
    expect(band('LUX').month).toBeUndefined();
    expect(band('LUX').when).toBe('week 4');
    // News of the month that has just turned is another country's move: the card gives the month and no week of it
    // (ruling 102: a June card said "July, week 4" before his own July had opened).
    expect(band('POL')).toMatchObject({ month: 'October', week: 1, when: 'as October opens' });
    const rows = order(card);
    expect(rows.find((r) => r.region === a.id)!.month).toBeUndefined();
    expect(rows.find((r) => r.region === a.id)!.when).toMatch(/^week [1-4]$/);
    expect(rows.find((r) => r.region === b.id)).toMatchObject({ month: 'October', week: 1, when: 'as October opens' });
    for (const x of [...card.bands, ...card.rows, ...(card.lead ? [card.lead] : [])]) expect(x.when).toBe(x.month ? `as ${x.month} opens` : `week ${x.week}`);
  });
});
