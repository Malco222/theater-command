// Ports cut off by sea tell of their stores (release 2; the UI director's words of 26 November 2026, 18:46Z,
// ui-director/port-stores-shut-sea-UI-MARKS.md, after the design director's rulings of 18:44Z; src/campaign/portcut.ts).
//
// Every word form of the marks, pinned against a real Malta: the Italian fleet across the Central Mediterranean, the
// one sea off Malta's harbour, under true reach (so that no ally's paper reach refills the stores), the stores last
// filled by a real draw of the supply picture in December 1940 (turn 16), and the corps charged by the rules' own month
// (game.ts startPhase). The months the lines name are the months the rule acts: the corps is struck off as the month
// the card named opens, not a month before or after. The shutting side's line reads only its own sight (how long its
// side has held the sea), never Britain's stores. With the switch off, nothing of it is said or written.
import { describe, expect, it } from 'vitest';
import HUD from '../unreal/TheaterCommand/Source/TheaterCommand/Campaign/CampaignHUD.cpp?raw';
import SESSION from '../unreal/TheaterCommand/Source/TheaterCommand/Campaign/CampaignSession.cpp?raw';
import { seaPicture } from '../src/campaign/carriers';
import { addCorps, computeSupply, createGame, declareWar, portKeep, removeCorps, startPhase } from '../src/campaign/game';
import { SIGHT, fogOn, sightOf } from '../src/campaign/intel';
import { closedTo, seaSide, zoneOf } from '../src/campaign/navy';
import { PORTSTORES_ROW, monthSaid, portCardLines, portCorpsLine, portCutOff, portStoresOn, portsDone, setPortStores, type PortLine } from '../src/campaign/portcut';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

const MALTA = 361;

/** A grand campaign with Units decide and true reach, the switch as asked, and the player's seat. */
function grand(human: string | null, on = true, forts = false): Game {
  const h = createGame(1, human);
  if (forts) h.flags.fortRows = 1;
  const g = startOpen(h);
  g.flags.unitsDecide = 1;
  g.flags.trueReach = 1;
  if (on) expect(setPortStores(g, true)).toBe(true);
  return g;
}
/** Every corps standing in a province taken off, and corps of the given names set down there instead. */
function garrison(g: Game, place: string, names: string[], nat = 'UK'): Corps[] {
  for (const c of [...g.byName[place].units]) removeCorps(g, c);
  return names.map((n) => addCorps(g, nat, n, 'inf', 2, place, {}));
}
/** Fleets of a nation put to sea in a sea on patrol (by name, or all of them). */
function sail(g: Game, nat: string, key: string, names?: string[]): void {
  const z = zoneOf(g, key);
  for (const f of g.fleets) if (f.nat === nat && (!names || names.includes(f.name))) { f.zone = z; f.stance = 'patrol'; }
}
/** Each nation's month as it opens (game.ts startPhase), in the order of the nations. */
function month(g: Game, turn: number, nats: string[] = ['UK']): void {
  g.turn = turn;
  for (const nat of [...nats].sort((a, b) => g.order.indexOf(a) - g.order.indexOf(b))) { g.phase = g.order.indexOf(nat); startPhase(g, nat); }
}
/**
 * Malta's stores filled by a real draw in month `fill` (Britain's ships reach it at peace), then Italy at war with
 * Britain and the fleets named at sea off it, and that month's own reckoning played.
 */
function malta(human: string | null, corps: string[] = ['9th Corps'], fill = 16, place: (g: Game) => void = (g) => sail(g, 'ITA', 'central'), on = true): { g: Game; cs: Corps[] } {
  const g = grand(human, on);
  const cs = garrison(g, 'Malta', corps);
  g.turn = fill;
  g.supplyDirty = true;
  computeSupply(g);
  expect(g.flags[`stores:${MALTA}`]).toBe(fill);
  declareWar(g, 'ITA', 'UK');
  place(g);
  g.supplyDirty = true;
  month(g, fill);
  expect(g.flags[`stores:${MALTA}`]).toBe(fill);
  return { g, cs };
}
/** One nation's month as it opens, in the month already set (game.ts startPhase). */
function phase(g: Game, nat: string): void {
  g.phase = g.order.indexOf(nat);
  startPhase(g, nat);
}
/**
 * Gibraltar held by a British 9th Corps, its stores filled by a real draw in December 1940 (turn 16), Italy and
 * Germany at war with Britain, France's fleet in harbour (so that the Western Mediterranean is not Britain's by her
 * ally's ships), and the fleets placed as asked.
 */
function gibraltar(human: string, place: (g: Game) => void): { g: Game; gib: number; ninth: Corps } {
  const g = grand(human);
  const [ninth] = garrison(g, 'Gibraltar', ['9th Corps']);
  g.turn = 16;
  g.supplyDirty = true;
  computeSupply(g);
  declareWar(g, 'ITA', 'UK');
  declareWar(g, 'GER', 'UK');
  for (const f of g.fleets) if (f.nat === 'FRA') f.stance = 'port';
  place(g);
  g.supplyDirty = true;
  return { g, gib: g.byName.Gibraltar.id, ninth };
}
/** The telling's own flags. */
const OWN = /^(portTold|seaShut|portLost|portHad):/;
const news = (g: Game, from = 0): string[] => g.headlines.slice(from).map((h) => h.text).filter((t) => /Malta|Gibraltar|stores/.test(t) && !/memorial|wireless|honour|ring broke|Rats of/.test(t));
const texts = (ls: PortLine[]): string[] => ls.map((l) => l.text);

describe('the UI director\'s words, on a real Malta (the Italian fleet across the Central Mediterranean, true reach on)', () => {
  it('the holder: the news, the card and the corps card month by month from a December 1940 fill, and the corps struck off as May opens, the month named', () => {
    const { g, cs: [ninth] } = malta('UK');
    const r = g.regions[MALTA];
    expect([r.name, r.ctrl, r.seas, r.nb, g.turn]).toEqual(['Malta', 'UK', [zoneOf(g, 'central')], [], 16]);
    // December 1940, the month of the fill: the sea is open no longer, but a ship got in this month.
    const dec = portCutOff(g, MALTA, 'UK')!;
    expect([dec.side, dec.state, dec.cut, dec.lastFilled, dec.feedsTo, dec.hungry, dec.lost, dec.keep, dec.fleetInReach]).toEqual(['holder', 'filled', false, 16, 18, 19, 21, 2, true]);
    expect(dec.shut).toEqual([{ sea: zoneOf(g, 'central'), by: [{ nat: 'ITA', fleets: true, aircraft: false }] }]);
    expect(texts(portCardLines(g, MALTA, 'UK'))).toEqual(['Cut off by sea, its stores would feed the garrison for two months.']);
    expect(news(g)).toEqual([]);
    // January 1941: the first month no ship gets in.
    let h0 = g.headlines.length;
    month(g, 17);
    expect(news(g, h0)).toEqual(['No ship reaches Malta: Italian fleets hold the Central Mediterranean. The garrison lives on the port\'s stores through February; unless the sea is opened, it then goes hungry and is lost at the start of May.']);
    expect(portCardLines(g, MALTA, 'UK')).toEqual([
      { tone: 'warn', text: 'Cut off by sea: Italian fleets hold the Central Mediterranean, and no ship reaches the harbour. The garrison lives on the port\'s stores through February; unless the sea is opened, it then goes hungry and is lost at the start of May.' },
      { tone: 'note', text: 'A fleet of yours at sea there, strong enough to open the Central Mediterranean, lets the ships in and fills the stores: one such month feeds the garrison through March.' },
    ]);
    expect([ninth.supplied, ninth.org, portCorpsLine(g, ninth)]).toEqual([true, 1, 'Living on Malta\'s stores through February.']);
    expect(g.flags[`portTold:${MALTA}`]).toBe(17);
    // February: the last month the stores feed it. Nothing new in the news (once per moment).
    h0 = g.headlines.length;
    month(g, 18);
    expect(news(g, h0)).toEqual([]);
    expect(texts(portCardLines(g, MALTA, 'UK'))[1]).toBe('A fleet of yours at sea there, strong enough to open the Central Mediterranean, lets the ships in and fills the stores: one such month feeds the garrison through April.');
    expect(portCorpsLine(g, ninth)).toBe('Living on Malta\'s stores through February.');
    // March: the stores are spent; the corps is charged its first hungry month.
    h0 = g.headlines.length;
    month(g, 19);
    expect(news(g, h0)).toEqual(['Malta\'s stores are spent and no ship has got through: the garrison goes hungry, and is lost at the start of May unless the sea is opened.']);
    expect(texts(portCardLines(g, MALTA, 'UK'))).toEqual([
      'Cut off by sea, and the port\'s stores are spent: the garrison goes hungry, losing order every month, and is lost at the start of May unless the sea is opened.',
      'A fleet of yours at sea there, strong enough to open the Central Mediterranean, lets the ships in and fills the stores: one such month feeds the garrison through May.',
    ]);
    expect(portCardLines(g, MALTA, 'UK')[0].tone).toBe('bad');
    expect([ninth.supplied, Math.round(ninth.org * 100) / 100, portCorpsLine(g, ninth)]).toEqual([false, 0.6, 'Hungry: Malta\'s stores are spent. Lost at the start of May unless ships get through.']);
    // April: still standing, and still named for May.
    h0 = g.headlines.length;
    month(g, 20);
    expect(news(g, h0)).toEqual([]);
    expect([ninth.dead, portCutOff(g, MALTA, 'UK')!.lost, portCorpsLine(g, ninth)]).toEqual([false, 21, 'Hungry: Malta\'s stores are spent. Lost at the start of May unless ships get through.']);
    // May: struck off as Britain's month opens, the month every line named.
    h0 = g.headlines.length;
    month(g, 21);
    expect(ninth.dead).toBe(true);
    expect(news(g, h0)).toEqual(['Cut off on Malta, the 9th Corps has starved and is lost.']);
    // (The rule's own memorial line stays as it was, beside it.)
    expect(g.headlines.slice(h0).map((h) => h.text)).toContain('9th Corps is struck off the army\'s list and written on the memorial wall.');
    // No garrison is left to tell of.
    expect(portCardLines(g, MALTA, 'UK')).toEqual([]);
    // Every line was told once.
    const all = news(g);
    expect(all.length).toBe(new Set(all).size);
    expect(all).toHaveLength(3);
  });

  it('two corps struck off together; and the months said with their year where the year is not the present one', () => {
    const { g, cs } = malta('UK', ['9th Corps', '14th Corps'], 15);
    let h0 = g.headlines.length;
    month(g, 16);
    // December 1940: the stores feed through January 1941.
    expect(monthSaid(g, 17)).toBe('January 1941');
    expect(monthSaid(g, 16)).toBe('December');
    expect(news(g, h0)).toEqual(['No ship reaches Malta: Italian fleets hold the Central Mediterranean. The garrison lives on the port\'s stores through January 1941; unless the sea is opened, it then goes hungry and is lost at the start of April 1941.']);
    expect(cs.map((c) => portCorpsLine(g, c))).toEqual(['Living on Malta\'s stores through January 1941.', 'Living on Malta\'s stores through January 1941.']);
    for (let t = 17; t <= 20; t++) {
      h0 = g.headlines.length;
      month(g, t);
      expect(cs.map((c) => c.dead)).toEqual([t === 20, t === 20]);
    }
    // (The rule's own line for a loss of more than 30,000 men stays as it was: existing lines are unchanged. Told the
    // same month, the struck-off line gives way to it: the UI director's 23:50Z, item 4, below.)
    expect(news(g, h0)).toEqual(['Cut off and out of supply, the British army at Malta lays down its arms: 2 corps, 36,848 men.']);
  });

  it('a corps below full order: the card counts the rest it still gets from the stores, and it is struck off in the month named', () => {
    const { g, cs: [ninth, fourteenth] } = malta('UK', ['9th Corps', '14th Corps']);
    month(g, 17);
    // After January's reckoning the 14th has little order left: February's stores still rest it, then it goes hungry.
    fourteenth.org = 0.1;
    const f = portCutOff(g, MALTA, 'UK')!;
    const named = f.garrison.find((x) => x.id === fourteenth.id)!.lost;
    expect(f.garrison.find((x) => x.id === ninth.id)!.lost).toBe(21);
    expect(named).toBeLessThan(21);
    expect(f.lost).toBe(named);
    expect(texts(portCardLines(g, MALTA, 'UK'))[0]).toContain(`lost at the start of ${monthSaid(g, named)}.`);
    for (let t = 18; t <= 21; t++) {
      month(g, t);
      expect([t, fourteenth.dead]).toEqual([t, t >= named]);
      expect([t, ninth.dead]).toEqual([t, t >= 21]);
    }
  });

  it('relief, after the first line was told: the sea opened, the stores filled, and the news said once', () => {
    const { g } = malta('UK');
    month(g, 17);
    expect(g.flags[`portTold:${MALTA}`]).toBe(17);
    // Italy's fleets go back to harbour: Britain's own ships reach Malta again at the next draw.
    for (const f of g.fleets) if (f.nat === 'ITA') f.stance = 'port';
    g.supplyDirty = true;
    const h0 = g.headlines.length;
    month(g, 18);
    expect(g.flags[`stores:${MALTA}`]).toBe(18);
    expect(news(g, h0)).toEqual(['Ships reach Malta again: its stores are filled.']);
    expect(g.flags[`portTold:${MALTA}`]).toBeUndefined();
    expect(texts(portCardLines(g, MALTA, 'UK'))).toEqual(['Cut off by sea, its stores would feed the garrison for two months.']);
    month(g, 19);
    expect(news(g).filter((t) => /^Ships reach/.test(t))).toHaveLength(1);
  });

  it('who holds it: aircraft alone, fleets and aircraft, and two nations', () => {
    // Aircraft alone: no fleet at sea, and Italy's air over the coasts strong enough to shut the sea by itself.
    const air = malta('UK', ['9th Corps'], 16, (g) => { g.nations.ITA.air = 200; });
    month(air.g, 17);
    expect(news(air.g)[0]).toBe('No ship reaches Malta: Italian aircraft hold the sky over the Central Mediterranean. The garrison lives on the port\'s stores through February; unless the sea is opened, it then goes hungry and is lost at the start of May.');
    // Fleets and aircraft: the submarine fleet alone, and the air alone, would not shut it; together they do.
    const both = malta('UK', ['9th Corps'], 16, (g) => sail(g, 'ITA', 'central', ['Submarine Fleet']));
    month(both.g, 17);
    expect(portCutOff(both.g, MALTA, 'UK')!.shut[0].by).toEqual([{ nat: 'ITA', fleets: true, aircraft: true }]);
    expect(texts(portCardLines(both.g, MALTA, 'UK'))[0]).toBe('Cut off by sea: Italian fleets and aircraft hold the Central Mediterranean, and no ship reaches the harbour. The garrison lives on the port\'s stores through February; unless the sea is opened, it then goes hungry and is lost at the start of May.');
    // Two nations: neither fleet alone would shut it.
    const two = malta('UK', ['9th Corps'], 16, (g) => { declareWar(g, 'GER', 'UK'); sail(g, 'ITA', 'central', ['Submarine Fleet']); sail(g, 'GER', 'central', ['U-boat Fleet']); });
    month(two.g, 17);
    expect(portCutOff(two.g, MALTA, 'UK')!.shut[0].by).toEqual([{ nat: 'ITA', fleets: true, aircraft: false }, { nat: 'GER', fleets: true, aircraft: false }]);
    expect(news(two.g)[0]).toBe('No ship reaches Malta: Italian and German fleets hold the Central Mediterranean. The garrison lives on the port\'s stores through February; unless the sea is opened, it then goes hungry and is lost at the start of May.');
  });

  it('several seas: Gibraltar with both its seas shut, said together where one side holds them, a clause each where two do; "in" a province with ground beside it', () => {
    const run = (place: (g: Game) => void): Game => {
      const g = grand('UK');
      garrison(g, 'Gibraltar', ['9th Corps']);
      g.turn = 16;
      g.supplyDirty = true;
      computeSupply(g);
      declareWar(g, 'ITA', 'UK');
      declareWar(g, 'GER', 'UK');
      // (France's fleet in harbour, so that the Western Mediterranean is not Britain's by her ally's ships.)
      for (const f of g.fleets) if (f.nat === 'FRA') f.stance = 'port';
      place(g);
      g.supplyDirty = true;
      for (let t = 16; t <= 21; t++) month(g, t);
      return g;
    };
    const one = run((g) => { sail(g, 'ITA', 'wmed', ['1st Fleet']); sail(g, 'ITA', 'iberia', ['2nd Fleet']); });
    expect(news(one)).toEqual([
      'No ship reaches Gibraltar: Italian fleets hold the Western Mediterranean and the waters off the Iberian coast. The garrison lives on the port\'s stores through February; unless the sea is opened, it then goes hungry and is lost at the start of May.',
      'Gibraltar\'s stores are spent and no ship has got through: the garrison goes hungry, and is lost at the start of May unless the sea is opened.',
      'Cut off in Gibraltar, the 9th Corps has starved and is lost.',
    ]);
    const split = run((g) => { sail(g, 'ITA', 'wmed', ['1st Fleet']); sail(g, 'GER', 'iberia', ['Battle Fleet']); });
    expect(news(split)[0]).toBe('No ship reaches Gibraltar: Italian fleets hold the Western Mediterranean, and German fleets hold the waters off the Iberian coast. The garrison lives on the port\'s stores through February; unless the sea is opened, it then goes hungry and is lost at the start of May.');
  });

  it('with the sea open, what the stores would feed: two months, and under the fortress rows Gibraltar\'s crew\'s 16 (figures from 11)', () => {
    const g = grand('UK', true, true);
    garrison(g, 'Malta', ['9th Corps']);
    const gib = g.byName.Gibraltar;
    addCorps(g, 'UK', '14th Corps', 'inf', 2, 'Gibraltar', {});
    declareWar(g, 'GER', 'UK');
    g.supplyDirty = true;
    month(g, 2);
    expect(texts(portCardLines(g, MALTA, 'UK'))).toEqual(['Cut off by sea, its stores would feed the garrison for two months.']);
    expect(texts(portCardLines(g, gib.id, 'UK'))).toEqual(['Cut off by sea, its stores would feed the garrison for 16 months.']);
    // (A fortress crew keeps its own clock and its own line: it is no part of the garrison counted here.)
    expect(portCutOff(g, gib.id, 'UK')!.garrison.map((x) => g.corps.find((c) => c.id === x.id)!.name)).toEqual(['14th Corps']);
  });
});

describe('the shutting side, from its own sight only', () => {
  it('Italy\'s player: "Your fleets have held ... since December 1940", the count from then, and never Britain\'s stores', () => {
    const { g } = malta('ITA');
    expect(g.flags['seaShut:20:UK']).toBe(16);
    month(g, 17);
    const lines = portCardLines(g, MALTA, 'ITA');
    expect(lines).toEqual([{ tone: 'note', text: 'Your fleets have held the Central Mediterranean since December 1940. Counting from then, the garrison of Malta goes hungry from March and is lost by the start of May, unless ships get through.' }]);
    const f = portCutOff(g, MALTA, 'ITA')!;
    expect([f.side, f.state, f.lastFilled, f.feedsTo, f.lost, f.heldSince, f.heldHungry, f.heldLost]).toEqual(['shutting', null, null, null, null, 16, 19, 21]);
    // Britain's stores are not read: another month on Britain's flag changes nothing the shutting side is told.
    g.flags[`stores:${MALTA}`] = 12;
    expect(portCutOff(g, MALTA, 'ITA')).toEqual(f);
    expect(portCardLines(g, MALTA, 'ITA')).toEqual(lines);
    // Italy's player hears none of the holder's news.
    for (let t = 18; t <= 21; t++) month(g, t);
    expect(news(g)).toEqual([]);
  });

  it('with others, every nation by its adjective; an ally of the shutting side reads its ally\'s hold; aircraft hold the sky', () => {
    const two = malta('ITA', ['9th Corps'], 16, (g) => { declareWar(g, 'GER', 'UK'); sail(g, 'ITA', 'central', ['Submarine Fleet']); sail(g, 'GER', 'central', ['U-boat Fleet']); });
    month(two.g, 17);
    expect(texts(portCardLines(two.g, MALTA, 'ITA'))).toEqual(['Italian and German fleets have held the Central Mediterranean since December 1940. Counting from then, the garrison of Malta goes hungry from March and is lost by the start of May, unless ships get through.']);
    const ally = malta('GER', ['9th Corps'], 16, (g) => { declareWar(g, 'GER', 'UK'); sail(g, 'ITA', 'central'); });
    month(ally.g, 17);
    expect(texts(portCardLines(ally.g, MALTA, 'GER'))).toEqual(['Italian fleets have held the Central Mediterranean since December 1940. Counting from then, the garrison of Malta goes hungry from March and is lost by the start of May, unless ships get through.']);
    const air = malta('ITA', ['9th Corps'], 16, (g) => { g.nations.ITA.air = 200; });
    month(air.g, 17);
    expect(texts(portCardLines(air.g, MALTA, 'ITA'))).toEqual(['Your aircraft have held the sky over the Central Mediterranean since December 1940. Counting from then, the garrison of Malta goes hungry from March and is lost by the start of May, unless ships get through.']);
  });
});

describe('the shutting side: a count only where the rule\'s clock runs, from its own sight', () => {
  it('Gibraltar with only the Western Mediterranean held by Italy and the waters off the Iberian coast open: the stores are filled every month, and Italy\'s card has no count', () => {
    const { g, gib, ninth } = gibraltar('ITA', (h) => sail(h, 'ITA', 'wmed', ['1st Fleet']));
    const [wmed, iberia] = g.regions[gib].seas;
    expect([g.seas[wmed].name, g.seas[iberia].name]).toEqual(['Western Mediterranean', 'Iberian Coast']);
    for (let t = 16; t <= 22; t++) {
      g.turn = t;
      phase(g, 'ITA');
      phase(g, 'UK');
      // (One sea still open is a quay: game.ts computeSupply, `r.seas.some((z) => open[z])`.)
      expect([t, closedTo(g, wmed, 'UK'), closedTo(g, iberia, 'UK'), g.flags[`stores:${gib}`], ninth.org, ninth.supplied]).toEqual([t, true, false, t, 1, true]);
      expect(portCutOff(g, gib, 'ITA')).toBeNull();
      expect(portCardLines(g, gib, 'ITA')).toEqual([]);
    }
    expect(ninth.dead).toBe(false);
  });

  it('the clock starts when the last of the port\'s seas is shut: the second Italian fleet off the Iberian coast in January 1941, counted from then, and the corps struck off as the month named opens', () => {
    const { g, gib, ninth } = gibraltar('ITA', (h) => sail(h, 'ITA', 'wmed', ['1st Fleet']));
    g.turn = 16;
    phase(g, 'ITA');
    phase(g, 'UK');
    expect(g.flags[`seaShut:${g.regions[gib].seas[0]}:UK`]).toBe(16);
    expect(portCardLines(g, gib, 'ITA')).toEqual([]);
    // January 1941: Italy's month opens with the Iberian waters still open (the stores are filled), then her second
    // fleet sails there, and Britain's month opens with both seas shut.
    g.turn = 17;
    phase(g, 'ITA');
    sail(g, 'ITA', 'iberia', ['2nd Fleet']);
    g.supplyDirty = true;
    phase(g, 'UK');
    expect([g.flags[`stores:${gib}`], g.flags[`seaShut:${g.regions[gib].seas[1]}:UK`]]).toEqual([17, 17]);
    const line = 'Your fleets have held the Western Mediterranean and the waters off the Iberian coast since January. Counting from then, the garrison of Gibraltar goes hungry from April and is lost by the start of June, unless ships get through.';
    expect(texts(portCardLines(g, gib, 'ITA'))).toEqual([line]);
    const f = portCutOff(g, gib, 'ITA')!;
    expect([f.heldSince, f.heldHungry, f.heldLost]).toEqual([17, 20, 22]);
    // The rule's own clock, month by month: fed through March, hungry from April, struck off as June opens.
    for (let t = 18; t <= 22; t++) {
      g.turn = t;
      phase(g, 'ITA');
      phase(g, 'UK');
      expect([t, ninth.supplied, ninth.dead]).toEqual([t, t < 20, t >= 22]);
      if (t < 22) expect(texts(portCardLines(g, gib, 'ITA'))).toEqual([line]);
    }
    expect(g.flags[`stores:${gib}`]).toBe(17);
  });

  it('out of its sight, the line is the same whether a British corps or a fortress crew stands in Malta or none; seen whole, the crew\'s stores are counted and an empty port has no line', () => {
    // Italy's aircraft alone shut the sea: no Italian fleet at sea, so nothing of Italy's sees Malta.
    const g = grand('ITA', true, true);
    const [ninth] = garrison(g, 'Malta', ['9th Corps']);
    g.turn = 16;
    g.supplyDirty = true;
    computeSupply(g);
    declareWar(g, 'ITA', 'UK');
    g.nations.ITA.air = 200;
    g.supplyDirty = true;
    month(g, 16, ['ITA', 'UK']);
    month(g, 17, ['ITA', 'UK']);
    const r = g.regions[MALTA];
    expect([fogOn(g, 'ITA'), sightOf(g, 'ITA')[MALTA]]).toEqual([true, SIGHT.none]);
    const line = 'Your aircraft have held the sky over the Central Mediterranean since December 1940. Counting from then, the garrison of Malta goes hungry from March and is lost by the start of May, unless ships get through.';
    expect(texts(portCardLines(g, MALTA, 'ITA'))).toEqual([line]);
    // The garrison taken off: Malta is still British, and Italy sees nothing of it, so its card says the same.
    removeCorps(g, ninth);
    expect([r.ctrl, sightOf(g, 'ITA')[MALTA]]).toEqual(['UK', SIGHT.none]);
    expect(texts(portCardLines(g, MALTA, 'ITA'))).toEqual([line]);
    // A British fortress crew put in (under the fortress rows its stores are the port's: three months, not two). The
    // holder's own count reads them; the shutting side, which cannot see them, counts the port's own two.
    addCorps(g, 'UK', '9th Corps', 'inf', 2, 'Malta', {});
    addCorps(g, 'UK', 'Malta fortress', 'fort', 1, 'Malta', {});
    expect(portKeep(g, r)).toBe(3);
    expect(portCutOff(g, MALTA, 'ITA')!.keep).toBe(2);
    expect(texts(portCardLines(g, MALTA, 'ITA'))).toEqual([line]);
    // With the fog lifted, Italy sees Malta whole: the crew and its stores are counted.
    g.flags.nofog = 1;
    expect(sightOf(g, 'ITA')[MALTA]).toBe(SIGHT.full);
    expect(texts(portCardLines(g, MALTA, 'ITA'))).toEqual(['Your aircraft have held the sky over the Central Mediterranean since December 1940. Counting from then, the garrison of Malta goes hungry from April and is lost by the start of June, unless ships get through.']);
    // And seen whole with no garrison for the rule to starve (the crew keeps its own clock), there is no line.
    for (const c of [...r.units]) if (c.kind !== 'fort') removeCorps(g, c);
    expect(portCardLines(g, MALTA, 'ITA')).toEqual([]);
  });

  it('a glimpse (an Italian fleet at sea off Malta sees the flags and the count): with no corps of Britain\'s counted there, no line', () => {
    const { g, cs: [ninth] } = malta('ITA');
    month(g, 17);
    expect(sightOf(g, 'ITA')[MALTA]).toBe(SIGHT.glimpse);
    expect(portCardLines(g, MALTA, 'ITA')).toHaveLength(1);
    removeCorps(g, ninth);
    expect(portCardLines(g, MALTA, 'ITA')).toEqual([]);
  });

  it('with another nation in the line, every nation by its adjective and no "Your" in the middle of it: Germany\'s player at a Gibraltar shut by Italy and Germany', () => {
    const { g, gib } = gibraltar('GER', (h) => { sail(h, 'ITA', 'wmed', ['1st Fleet']); sail(h, 'GER', 'iberia', ['Battle Fleet']); });
    for (const t of [16, 17]) { g.turn = t; phase(g, 'GER'); phase(g, 'ITA'); phase(g, 'UK'); }
    expect(texts(portCardLines(g, gib, 'GER'))).toEqual(['Italian fleets have held the Western Mediterranean, and German fleets have held the waters off the Iberian coast since December 1940. Counting from then, the garrison of Gibraltar goes hungry from March and is lost by the start of May, unless ships get through.']);
    // (Italy's player reads the same line by the adjectives: Germany is in it too.)
    const it = gibraltar('ITA', (h) => { sail(h, 'ITA', 'wmed', ['1st Fleet']); sail(h, 'GER', 'iberia', ['Battle Fleet']); });
    for (const t of [16, 17]) { it.g.turn = t; phase(it.g, 'GER'); phase(it.g, 'ITA'); phase(it.g, 'UK'); }
    expect(texts(portCardLines(it.g, it.gib, 'ITA'))[0]).toMatch(/^Italian fleets have held the Western Mediterranean, and German fleets have held the waters off the Iberian coast since December 1940\./);
  });
});

describe('what opens it, with several seas shut', () => {
  it('Gibraltar with both seas held by Italian fleets: either sea opened lets the ships in, so the card names them joined by "or"', () => {
    const run = (home: string | null): Game => {
      const { g } = gibraltar('UK', (h) => { sail(h, 'ITA', 'wmed', ['1st Fleet']); sail(h, 'ITA', 'iberia', ['2nd Fleet']); });
      month(g, 16);
      month(g, 17);
      if (home) { for (const f of g.fleets) if (f.nat === 'ITA' && f.name === home) f.stance = 'port'; g.supplyDirty = true; month(g, 18); }
      return g;
    };
    const g = run(null), gib = g.byName.Gibraltar.id;
    expect(texts(portCardLines(g, gib, 'UK'))[1]).toBe('A fleet of yours at sea there, strong enough to open the Western Mediterranean or the waters off the Iberian coast, lets the ships in and fills the stores: one such month feeds the garrison through March.');
    // Either one sent home, the other sea still shut: the stores are filled at the next draw.
    for (const home of ['1st Fleet', '2nd Fleet']) {
      const h = run(home);
      expect([home, h.regions[gib].seas.map((z) => closedTo(h, z, 'UK')).filter(Boolean).length, h.flags[`stores:${gib}`]]).toEqual([home, 1, 18]);
      expect(texts(portCardLines(h, gib, 'UK'))).toEqual(['Cut off by sea, its stores would feed the garrison for two months.']);
    }
  });
});

describe('who is told', () => {
  it('a nation with corps in the port is told as the holder is, while it has corps there; a player with no part in it hears nothing', () => {
    const fr = malta('FRA', ['9th Corps'], 16, (g) => { declareWar(g, 'ITA', 'FRA'); sail(g, 'ITA', 'central'); });
    const french = addCorps(fr.g, 'FRA', '3rd Corps', 'inf', 2, 'Malta', {});
    for (let t = 17; t <= 21; t++) month(fr.g, t, ['FRA', 'UK']);
    expect(news(fr.g)).toEqual([
      'No ship reaches Malta: Italian fleets hold the Central Mediterranean. The garrison lives on the port\'s stores through February; unless the sea is opened, it then goes hungry and is lost at the start of May.',
      'Malta\'s stores are spent and no ship has got through: the garrison goes hungry, and is lost at the start of May unless the sea is opened.',
      'Cut off on Malta, the 3rd Corps has starved and is lost.',
    ]);
    // (France's month opens before Britain's, so Britain's corps is struck off after France's month has been told. France
    // had corps there as the month opened, and hears of it as its next month opens.)
    expect([french.dead, fr.cs[0].dead]).toEqual([true, true]);
    const h0 = fr.g.headlines.length;
    month(fr.g, 22, ['FRA', 'UK']);
    expect(news(fr.g, h0)).toEqual(['Cut off on Malta, the 9th Corps has starved and is lost.']);
    expect(Object.keys(fr.g.flags).filter((k) => /^(portLost|portHad):/.test(k))).toEqual([]);
    // Nothing more after that: France has no corps there now.
    month(fr.g, 23, ['FRA', 'UK']);
    expect(news(fr.g, h0)).toHaveLength(1);
    const us = malta('USA');
    for (let t = 17; t <= 21; t++) month(us.g, t);
    expect(news(us.g)).toEqual([]);
    expect(portCardLines(us.g, MALTA, 'USA')).toEqual([]);
    // (Nothing is kept for him but the record of the holds, which watches the seas of every port of the kind whatever
    // his wars, so that a hold begun before his war with its holder is seen to begin: the UI director's 23:50Z, item 5.)
    expect(Object.keys(us.g.flags).filter((k) => OWN.test(k) && !k.startsWith('seaShut:'))).toEqual([]);
    expect(us.g.flags['seaShut:20:UK']).toBe(16);
  });

  it('corps of two nations struck off in one port in one month: one line to the holder, in the "Several" form, in the order they fell', () => {
    const { g, cs: [ninth] } = malta('UK', ['9th Corps'], 16, (h) => { declareWar(h, 'ITA', 'FRA'); sail(h, 'ITA', 'central'); });
    const french = addCorps(g, 'FRA', '3rd Corps', 'inf', 2, 'Malta', {});
    for (let t = 17; t <= 20; t++) month(g, t, ['FRA', 'UK']);
    const h0 = g.headlines.length;
    month(g, 21, ['FRA', 'UK']);
    expect([french.dead, ninth.dead]).toEqual([true, true]);
    expect(news(g, h0)).toEqual(['Cut off on Malta, two corps have starved and are lost: the 3rd Corps and the 9th Corps.']);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('portLost:'))).toEqual([]);
    month(g, 22, ['FRA', 'UK']);
    expect(news(g, h0)).toHaveLength(1);
  });

  it('a war watched with nobody at the table: no card and no corps line, and the rules\' own reading as it was', () => {
    const { g, cs: [ninth] } = malta(null);
    month(g, 17);
    expect(g.human).toBeNull();
    expect(portCardLines(g, MALTA, null)).toEqual([]);
    expect(portCorpsLine(g, ninth)).toBeNull();
    // (The news and the corps struck off read the holder's facts with no viewer: they stay.)
    expect(portCutOff(g, MALTA, null)).toMatchObject({ side: 'holder', state: 'stores', cut: true, feedsTo: 18, lost: 21 });
    for (let t = 18; t <= 21; t++) month(g, t);
    expect(news(g)).toEqual([]);
    expect(Object.keys(g.flags).filter((k) => OWN.test(k))).toEqual([]);
  });

  it('with the switch off nothing is said or written, and the war goes as it does with it on', () => {
    const off = malta('UK', ['9th Corps'], 16, (g) => sail(g, 'ITA', 'central'), false);
    const on = malta('UK');
    expect([portStoresOn(off.g), portStoresOn(on.g)]).toEqual([false, true]);
    for (let t = 17; t <= 21; t++) { month(off.g, t); month(on.g, t); }
    expect(news(off.g)).toEqual([]);
    expect(portCardLines(off.g, MALTA, 'UK')).toEqual([]);
    expect(portCorpsLine(off.g, off.cs[0])).toBeNull();
    expect(Object.keys(off.g.flags).filter((k) => k === 'portStores' || OWN.test(k))).toEqual([]);
    expect([off.cs[0].dead, on.cs[0].dead]).toEqual([true, true]);
    expect(news(on.g)).toHaveLength(3);
    expect(PORTSTORES_ROW.id).toBe('portstores');
  });
});

// ---- On the build (the UI director, 26 Nov 2026, 23:50Z) -------------------------------------------------------------

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

describe('item 3: the browser\'s card follows the remedy line with the sea card\'s measure for this seat', () => {
  it('Malta in January 1941: yours and the enemy\'s as found, the sea card\'s own figures as it prints them; the plain form where the seas are not searched; none in 3D', () => {
    const { g } = malta('UK');
    month(g, 17);
    const z = zoneOf(g, 'central');
    // (The sea card's own sums for this seat: its side, and the enemy as found where the carriers' rules search the seas.)
    expect([g.flags.carriers, Math.round(seaSide(g, z, 'UK', false).total), Math.round(seaPicture(g, z, 'UK').theirs), Math.round(seaSide(g, z, 'UK', true).total)]).toEqual([1, 11, 131, 151]);
    const lines = portCardLines(g, MALTA, 'UK', { measure: true });
    expect(lines).toEqual([
      { tone: 'warn', text: 'Cut off by sea: Italian fleets hold the Central Mediterranean, and no ship reaches the harbour. The garrison lives on the port\'s stores through February; unless the sea is opened, it then goes hungry and is lost at the start of May.' },
      { tone: 'note', text: 'A fleet of yours at sea there, strong enough to open the Central Mediterranean, lets the ships in and fills the stores: one such month feeds the garrison through March. At sea in the Central Mediterranean today: yours 11, the enemy\'s, as found, 131.' },
    ]);
    // Where the seas are not searched, the sea card gives the enemy whole, with no "as found".
    delete g.flags.carriers;
    expect(texts(portCardLines(g, MALTA, 'UK', { measure: true }))[1]).toBe('A fleet of yours at sea there, strong enough to open the Central Mediterranean, lets the ships in and fills the stores: one such month feeds the garrison through March. At sea in the Central Mediterranean today: yours 11, the enemy\'s 151.');
    g.flags.carriers = 1;
    // The 3D game's sea card shows no strengths: its card leaves the sentence out.
    TC().load(saveGame(g));
    const cut = ((TC().province as Call)(MALTA) as { portCut: { text: string; tone: string }[] }).portCut;
    expect(cut.map((l) => l.text)).toEqual(texts(portCardLines(g, MALTA, 'UK')));
    expect(cut[1].text).toBe('A fleet of yours at sea there, strong enough to open the Central Mediterranean, lets the ships in and fills the stores: one such month feeds the garrison through March.');
    // Hungry, the same: the remedy line and its measure.
    month(g, 18);
    month(g, 19);
    expect(texts(portCardLines(g, MALTA, 'UK', { measure: true }))[1]).toMatch(/^A fleet of yours at sea there, strong enough to open the Central Mediterranean, lets the ships in and fills the stores: one such month feeds the garrison through May\. At sea in the Central Mediterranean today: yours \d+, the enemy's, as found, \d+\.$/);
  });

  it('Gibraltar with both seas shut: a sentence for each sea the remedy line names, in its order', () => {
    const { g, gib } = gibraltar('UK', (h) => { sail(h, 'ITA', 'wmed', ['1st Fleet']); sail(h, 'ITA', 'iberia', ['2nd Fleet']); });
    month(g, 16);
    month(g, 17);
    expect(texts(portCardLines(g, gib, 'UK', { measure: true }))[1]).toBe('A fleet of yours at sea there, strong enough to open the Western Mediterranean or the waters off the Iberian coast, lets the ships in and fills the stores: one such month feeds the garrison through March. At sea in the Western Mediterranean today: yours 9, the enemy\'s, as found, 1. At sea in the waters off the Iberian coast today: yours 5, the enemy\'s, as found, 0.');
    // (With the sea open there is no remedy line, and so no measure.)
    const h = malta('UK').g;
    expect(texts(portCardLines(h, MALTA, 'UK', { measure: true }))).toEqual(['Cut off by sea, its stores would feed the garrison for two months.']);
  });
});

describe('item 4: two lines for one event, the rule\'s own line of a surrender stands and the struck-off line gives way to it', () => {
  const SHIPPED = 'Cut off and out of supply, the British army at Malta lays down its arms: 2 corps, 36,848 men.';

  it('the holder told the rule\'s line the same month: his struck-off line gives way, and nothing is kept for it', () => {
    const { g, cs } = malta('UK', ['9th Corps', '14th Corps']);
    for (let t = 17; t <= 20; t++) month(g, t);
    const h0 = g.headlines.length;
    month(g, 21);
    expect(cs.map((c) => c.dead)).toEqual([true, true]);
    expect(news(g, h0)).toEqual([SHIPPED]);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('portLost:'))).toEqual([]);
    // (A party the rule's line does not tell keeps the struck-off line: one corps, under 30,000 men.)
    const one = malta('UK');
    for (let t = 17; t <= 21; t++) month(one.g, t);
    expect(news(one.g).slice(-1)).toEqual(['Cut off on Malta, the 9th Corps has starved and is lost.']);
    expect(one.g.headlines.some((h) => /lays down its arms/.test(h.text))).toBe(false);
  });

  it('corps of two nations in one month: the rule\'s line tells Britain\'s, and the struck-off line names only the French corps it does not tell', () => {
    // (A British corps of four divisions, more than 30,000 men when it starves, and a French one of two: two corps, so
    // no ring was told, and the rule's line tells the British corps alone.)
    const { g, cs: [ninth] } = malta('UK', ['9th Corps'], 16, (h) => { declareWar(h, 'ITA', 'FRA'); sail(h, 'ITA', 'central'); });
    ninth.divs = 4;
    const french = addCorps(g, 'FRA', '3rd Corps', 'inf', 2, 'Malta', {});
    for (let t = 17; t <= 20; t++) month(g, t, ['FRA', 'UK']);
    const h0 = g.headlines.length;
    month(g, 21, ['FRA', 'UK']);
    expect([french.dead, ninth.dead, g.flags[`ring:${MALTA}`]]).toEqual([true, true, undefined]);
    expect(news(g, h0)).toEqual(['Cut off and out of supply, 9th Corps lays down its arms at Malta.', 'Cut off on Malta, the 3rd Corps has starved and is lost.']);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('portLost:'))).toEqual([]);
  });

  it('where the newsreel told the ring, the rule\'s line tells every corps, even one alone: the struck-off line gives way to both', () => {
    const { g, cs } = malta('UK', ['9th Corps', '14th Corps'], 16, (h) => { declareWar(h, 'ITA', 'FRA'); sail(h, 'ITA', 'central'); });
    const french = addCorps(g, 'FRA', '3rd Corps', 'inf', 2, 'Malta', {});
    for (let t = 17; t <= 20; t++) month(g, t, ['FRA', 'UK']);
    const h0 = g.headlines.length;
    month(g, 21, ['FRA', 'UK']);
    expect([french.dead, ...cs.map((c) => c.dead), g.flags[`ring:${MALTA}`] !== undefined]).toEqual([true, true, true, true]);
    expect(news(g, h0)).toEqual(['Cut off and out of supply, 3rd Corps lays down its arms at Malta.', SHIPPED]);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('portLost:'))).toEqual([]);
  });

  it('an ally with corps there reads the one news: told the rule\'s line of the British corps, it keeps the struck-off line of its own corps only', () => {
    const { g, cs: [ninth] } = malta('FRA', ['9th Corps'], 16, (h) => { declareWar(h, 'ITA', 'FRA'); sail(h, 'ITA', 'central'); });
    ninth.divs = 4;
    const french = addCorps(g, 'FRA', '3rd Corps', 'inf', 2, 'Malta', {});
    for (let t = 17; t <= 20; t++) month(g, t, ['FRA', 'UK']);
    const h0 = g.headlines.length;
    // (France's month opens before Britain's: its own corps is told in its month, the British corps' surrender in
    // Britain's, and France's next month tells nothing more.)
    month(g, 21, ['FRA', 'UK']);
    month(g, 22, ['FRA', 'UK']);
    expect([french.dead, ninth.dead]).toEqual([true, true]);
    expect(news(g, h0)).toEqual(['Cut off on Malta, the 3rd Corps has starved and is lost.', 'Cut off and out of supply, 9th Corps lays down its arms at Malta.']);
    expect(Object.keys(g.flags).filter((k) => /^(portLost|portHad):/.test(k))).toEqual([]);
  });
});

describe('item 5: the shutting side\'s "have held ... since" only for a hold the switch saw begin', () => {
  it('switched on mid-war with Malta\'s sea already shut: the line waits; the sea opened and shut again, it gives the month it saw', () => {
    // (The war runs with the switch off: Italy's fleets have held the Central Mediterranean since December 1940.)
    const { g } = malta('ITA', ['9th Corps'], 16, (h) => sail(h, 'ITA', 'central'), false);
    month(g, 17, ['ITA', 'UK']);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('seaShut:'))).toEqual([]);
    // January 1941: the player turns it on. Its first look finds the sea already shut: no month is given.
    expect(setPortStores(g, true)).toBe(true);
    expect(g.flags['seaShut:20:UK']).toBe(0);
    expect(portCutOff(g, MALTA, 'ITA')).toMatchObject({ side: 'shutting', heldSince: null, heldHungry: null, heldLost: null });
    expect(portCardLines(g, MALTA, 'ITA')).toEqual([]);
    month(g, 18, ['ITA', 'UK']);
    expect([g.flags['seaShut:20:UK'], portCardLines(g, MALTA, 'ITA')]).toEqual([0, []]);
    // March: Italy's fleets go home and the sea opens; April: they sail again, and the switch sees the hold begin.
    for (const f of g.fleets) if (f.nat === 'ITA') f.stance = 'port';
    g.supplyDirty = true;
    month(g, 19, ['ITA', 'UK']);
    expect([g.flags['seaShut:20:UK'], g.flags[`stores:${MALTA}`]]).toEqual([-1, 19]);
    sail(g, 'ITA', 'central');
    g.supplyDirty = true;
    month(g, 20, ['ITA', 'UK']);
    expect(g.flags['seaShut:20:UK']).toBe(20);
    expect(texts(portCardLines(g, MALTA, 'ITA'))).toEqual(['Your fleets have held the Central Mediterranean since April. Counting from then, the garrison of Malta goes hungry from July and is lost by the start of September, unless ships get through.']);
  });

  it('Gibraltar: the Western Mediterranean already shut at the first look, the Iberian waters shut in January under its watch: the count runs from January', () => {
    const { g, gib } = gibraltar('ITA', (h) => sail(h, 'ITA', 'wmed', ['1st Fleet']));
    const [wmed, iberia] = g.regions[gib].seas;
    // (The switch off through December's months, and on after them.)
    expect(setPortStores(g, false)).toBe(true);
    g.turn = 16;
    phase(g, 'ITA');
    phase(g, 'UK');
    expect(setPortStores(g, true)).toBe(true);
    expect([g.flags[`seaShut:${wmed}:UK`], g.flags[`seaShut:${iberia}:UK`]]).toEqual([0, -1]);
    g.turn = 17;
    phase(g, 'ITA');
    sail(g, 'ITA', 'iberia', ['2nd Fleet']);
    g.supplyDirty = true;
    phase(g, 'UK');
    expect([g.flags[`seaShut:${wmed}:UK`], g.flags[`seaShut:${iberia}:UK`]]).toEqual([0, 17]);
    expect(texts(portCardLines(g, gib, 'ITA'))).toEqual(['Your fleets have held the Western Mediterranean and the waters off the Iberian coast since January. Counting from then, the garrison of Gibraltar goes hungry from April and is lost by the start of June, unless ships get through.']);
  });

  it('the player falls: the holds watched for him go with him', () => {
    const { g } = malta('ITA');
    month(g, 17, ['ITA', 'UK']);
    expect(g.flags['seaShut:20:UK']).toBe(16);
    portsDone(g, 'ITA');
    expect(Object.keys(g.flags).filter((k) => k.startsWith('seaShut:'))).toEqual([]);
  });

  // (The review of the 23:50Z build, finding 1: a hold begun before the player's war with the port's holder. The seas
  // of every port of the kind are watched whatever the player's wars: a hold the switch saw begin gives its month, and
  // one it did not see begin waits. The fleets put to sea in their own nation's month, after its opening, as a staff's
  // orders do.)
  /**
   * Malta held by a British 9th Corps; Germany's fleets put to sea off it in October 1940 (turn 14), Italy at war with
   * Britain from January 1941 (turn 17); Germany's, Italy's and Britain's months played from August 1940 to March 1941,
   * the switch on from the start or turned on after the month given; Italy's card read after each month.
   */
  function lateWar(switchedOn: number | null): { g: Game; ninth: Corps; card: Record<number, string[]>; clock: [number, boolean, boolean][] } {
    const g = grand('ITA', switchedOn === null);
    const [ninth] = garrison(g, 'Malta', ['9th Corps']);
    const card: Record<number, string[]> = {}, clock: [number, boolean, boolean][] = [];
    for (let t = 12; t <= 19; t++) {
      g.turn = t;
      if (t === 17) declareWar(g, 'ITA', 'UK');
      g.supplyDirty = true;
      phase(g, 'GER');
      if (t === 14) { sail(g, 'GER', 'central'); g.supplyDirty = true; }
      phase(g, 'ITA');
      phase(g, 'UK');
      if (t === switchedOn) setPortStores(g, true);
      card[t] = texts(portCardLines(g, MALTA, 'ITA'));
      clock.push([t, ninth.supplied, ninth.dead]);
    }
    return { g, ninth, card, clock };
  }

  it('Italy comes into the war in January 1941, after Germany\'s fleets shut Malta\'s sea in October 1940 under the switch\'s watch: the month it saw the hold begin, and the rule\'s own clock from then', () => {
    const { g, card, clock } = lateWar(null);
    expect(g.flags['seaShut:20:UK']).toBe(14);
    for (let t = 12; t <= 16; t++) expect([t, card[t]]).toEqual([t, []]);
    const line = 'German fleets have held the Central Mediterranean since October 1940. Counting from then, the garrison of Malta goes hungry from January and is lost by the start of March, unless ships get through.';
    expect([card[17], card[18]]).toEqual([[line], [line]]);
    // Fed through December, hungry from January, struck off as March opens.
    expect(clock).toEqual([[12, true, false], [13, true, false], [14, true, false], [15, true, false], [16, true, false], [17, false, false], [18, false, false], [19, false, true]]);
  });

  it('the same war with the switch turned on in November 1940, after the hold began: Italy\'s line waits', () => {
    const { g, card, clock } = lateWar(15);
    expect(g.flags['seaShut:20:UK']).toBe(0);
    for (let t = 12; t <= 19; t++) expect([t, card[t]]).toEqual([t, []]);
    expect(clock.slice(-1)).toEqual([[19, false, true]]);
  });

  it('Spain comes into the war at Gibraltar in January 1941, after Italy\'s and Germany\'s fleets shut both its seas in October 1940: the month the switch saw, and the clock from then', () => {
    const g = grand('ESP');
    const gib = g.byName.Gibraltar.id, [wmed, iberia] = g.regions[gib].seas;
    const [ninth] = garrison(g, 'Gibraltar', ['9th Corps']);
    const card: Record<number, string[]> = {}, clock: [number, boolean, boolean][] = [];
    for (let t = 12; t <= 19; t++) {
      g.turn = t;
      if (t === 12) { declareWar(g, 'ITA', 'UK'); for (const f of g.fleets) if (f.nat === 'FRA') f.stance = 'port'; }
      if (t === 17) { g.nations.ESP.align = 'axis'; declareWar(g, 'ESP', 'UK'); }
      g.supplyDirty = true;
      phase(g, 'GER');
      if (t === 14) { sail(g, 'GER', 'iberia', ['Battle Fleet']); g.supplyDirty = true; }
      phase(g, 'ITA');
      if (t === 14) { sail(g, 'ITA', 'wmed', ['1st Fleet']); g.supplyDirty = true; }
      phase(g, 'ESP');
      phase(g, 'UK');
      card[t] = texts(portCardLines(g, gib, 'ESP'));
      clock.push([t, ninth.supplied, ninth.dead]);
    }
    expect([g.flags[`seaShut:${wmed}:UK`], g.flags[`seaShut:${iberia}:UK`]]).toEqual([14, 14]);
    for (let t = 12; t <= 16; t++) expect([t, card[t]]).toEqual([t, []]);
    const line = 'Italian fleets have held the Western Mediterranean, and German fleets have held the waters off the Iberian coast since October 1940. Counting from then, the garrison of Gibraltar goes hungry from January and is lost by the start of March, unless ships get through.';
    expect([card[17], card[18]]).toEqual([[line], [line]]);
    expect(clock.slice(4)).toEqual([[16, true, false], [17, false, false], [18, false, false], [19, false, true]]);
  });
});

describe('item 6: "by the start of" where the count can only be the latest month, "at" where it is exact', () => {
  const CARD = (month: string, word: string): string => `Cut off by sea: Italian fleets hold the Central Mediterranean, and no ship reaches the harbour. The garrison lives on the port's stores through February; unless the sea is opened, it then goes hungry and is lost ${word} the start of ${month}.`;

  it('the shutting side cannot see the garrison\'s order: its line always says "by"', () => {
    const { g } = malta('ITA');
    month(g, 17);
    expect(texts(portCardLines(g, MALTA, 'ITA'))).toEqual(['Your fleets have held the Central Mediterranean since December 1940. Counting from then, the garrison of Malta goes hungry from March and is lost by the start of May, unless ships get through.']);
    expect(portCutOff(g, MALTA, 'ITA')!.lostBy).toBeNull();
  });

  it('the holder\'s treasury in debt pays for less rest in a fed month: "by", and the corps is lost sooner than the month named; at full order the debt changes nothing, and it stays "at"', () => {
    const { g, cs: [ninth] } = malta('UK');
    month(g, 17);
    // After January's reckoning: the corps low in order, and Britain's treasury deep in debt.
    ninth.org = 0.1;
    expect(texts(portCardLines(g, MALTA, 'UK'))[0]).toBe(CARD('April', 'at'));
    g.nations.UK.pp = -1e6;
    const f = portCutOff(g, MALTA, 'UK')!;
    expect([f.lost, f.lostBy, f.garrison]).toEqual([20, true, [{ id: ninth.id, lost: 20, by: true }]]);
    expect(texts(portCardLines(g, MALTA, 'UK'))[0]).toBe(CARD('April', 'by'));
    // (The debt halves February's rest: the corps is struck off as March opens, before April.)
    month(g, 18);
    month(g, 19);
    expect(ninth.dead).toBe(true);
    // At full order a fed month's rest, halved or not, leaves it full: the count is exact, and says "at".
    const full = malta('UK');
    month(full.g, 17);
    full.g.nations.UK.pp = -1e6;
    expect([full.cs[0].org, portCutOff(full.g, MALTA, 'UK')!.lostBy, texts(portCardLines(full.g, MALTA, 'UK'))[0]]).toEqual([1, false, CARD('May', 'at')]);
  });

  it('the news: the first month no ship gets in, told with the treasury in debt and the corps low in order, says "by"', () => {
    const { g, cs: [ninth] } = malta('UK');
    ninth.org = 0.1;
    g.nations.UK.pp = -1e6;
    const h0 = g.headlines.length;
    month(g, 17);
    expect(news(g, h0)).toEqual(['No ship reaches Malta: Italian fleets hold the Central Mediterranean. The garrison lives on the port\'s stores through February; unless the sea is opened, it then goes hungry and is lost by the start of May.']);
    // (The same news with the treasury paying says "at".)
    const paid = malta('UK');
    paid.cs[0].org = 0.1;
    const p0 = paid.g.headlines.length;
    month(paid.g, 17);
    expect(news(paid.g, p0)).toEqual(['No ship reaches Malta: Italian fleets hold the Central Mediterranean. The garrison lives on the port\'s stores through February; unless the sea is opened, it then goes hungry and is lost at the start of May.']);
  });

  it('a corps that marched this month does not count its next month as rested: "by" only where that can bring the loss sooner', () => {
    // Low in order, the month without a rested month's rest brings it to the same month: "at", and so it is.
    const low = malta('UK');
    month(low.g, 17);
    low.cs[0].org = 0.1;
    low.cs[0].ap = 0;
    expect([portCutOff(low.g, MALTA, 'UK')!.lostBy, texts(portCardLines(low.g, MALTA, 'UK'))[0]]).toEqual([false, CARD('April', 'at')]);
    for (let t = 18; t <= 20; t++) { month(low.g, t); expect([t, low.cs[0].dead]).toEqual([t, t >= 20]); }
    // A little higher, a rested month would take it to May and an unrested one only to April: "by the start of May".
    const mid = malta('UK');
    month(mid.g, 17);
    mid.cs[0].org = 0.3;
    mid.cs[0].ap = 0;
    expect([portCutOff(mid.g, MALTA, 'UK')!.lostBy, texts(portCardLines(mid.g, MALTA, 'UK'))[0]]).toEqual([true, CARD('May', 'by')]);
    for (let t = 18; t <= 20; t++) { month(mid.g, t); expect([t, mid.cs[0].dead]).toEqual([t, t >= 20]); }
  });

  it('hungry, nothing is left out (the hungry months take their order whatever else): the card, the news and the corps line say "at", in debt too', () => {
    const { g, cs: [ninth] } = malta('UK');
    month(g, 17);
    g.nations.UK.pp = -1e6;
    month(g, 18);
    const h0 = g.headlines.length;
    month(g, 19);
    expect(news(g, h0)).toEqual(['Malta\'s stores are spent and no ship has got through: the garrison goes hungry, and is lost at the start of May unless the sea is opened.']);
    expect(texts(portCardLines(g, MALTA, 'UK'))[0]).toBe('Cut off by sea, and the port\'s stores are spent: the garrison goes hungry, losing order every month, and is lost at the start of May unless the sea is opened.');
    expect(portCorpsLine(g, ninth)).toBe('Hungry: Malta\'s stores are spent. Lost at the start of May unless ships get through.');
    expect(portCutOff(g, MALTA, 'UK')!.garrison).toEqual([{ id: ninth.id, lost: 21, by: false }]);
  });
});

describe('item 8: the 3D cards (C++ for the Mac pass, not compiled here)', () => {
  it('the bridge gives the province\'s portCut and the corps\' portLine; the session reads both, and the HUD draws them in their tones', () => {
    const { g, cs: [ninth] } = malta('UK');
    month(g, 17);
    TC().load(saveGame(g));
    const view = TC().view() as { corps: { id: number; portLine?: string }[] };
    expect(view.corps.find((c) => c.id === ninth.id)!.portLine).toBe('Living on Malta\'s stores through February.');
    expect(((TC().province as Call)(MALTA) as { portCut: { tone: string }[] }).portCut.map((l) => l.tone)).toEqual(['warn', 'note']);
    expect(SESSION).toContain('Object->TryGetArrayField(TEXT("portCut"), PortCut)');
    expect(SESSION).toContain('Entry->TryGetStringField(TEXT("tone"), Line.Tone);');
    expect(SESSION).toContain('Unit->TryGetStringField(TEXT("portLine"), C.PortLine);');
    expect(HUD).toContain('for (const FPortCutLine& Line : Info.PortCut)');
    expect(HUD).toContain('const FLinearColor Tone = Line.Tone == TEXT("bad") ? Alarm : Line.Tone == TEXT("warn") ? BrightBrass : Muted;');
    expect(HUD).toContain('.Text(FText::FromString(Card.PortLine)).Font(Font(TEXT("Regular"), 9)).ColorAndOpacity(bGoing ? Muted : Faint).AutoWrapText(true)');
  });
});
