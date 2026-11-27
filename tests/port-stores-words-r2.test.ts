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
import { signPeace } from '../src/campaign/diplomacy';
import { addCorps, allied, computeSupply, createGame, declareWar, portKeep, removeCorps, resolveBattle, startPhase, trueReach } from '../src/campaign/game';
import { crewStores, storesEaten } from '../src/campaign/fortrows';
import { SIGHT, fogOn, sightOf } from '../src/campaign/intel';
import { closedTo, seaSide, zoneOf } from '../src/campaign/navy';
import { PORTSTORES_ROW, monthSaid, portCardLines, portCorpsLine, portCutOff, portStoresOn, portsDone, setPortStores, type PortLine } from '../src/campaign/portcut';
import { loadGame, saveGame } from '../src/campaign/save';
import { fieldCtrl, joinTown } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';
import { skyAtSea } from '../src/campaign/weather';
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
const OWN = /^(portTold|portSpent|seaShut|portLost|portHad|portCrew|portDraw):/;
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
    expect(lines).toEqual([{ tone: 'note', text: 'Your fleets have held the Central Mediterranean since December 1940. Counting from then, the garrison of Malta is hungry by March and lost by the start of May, unless ships get through.' }]);
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
    expect(texts(portCardLines(two.g, MALTA, 'ITA'))).toEqual(['Italian and German fleets have held the Central Mediterranean since December 1940. Counting from then, the garrison of Malta is hungry by March and lost by the start of May, unless ships get through.']);
    const ally = malta('GER', ['9th Corps'], 16, (g) => { declareWar(g, 'GER', 'UK'); sail(g, 'ITA', 'central'); });
    month(ally.g, 17);
    expect(texts(portCardLines(ally.g, MALTA, 'GER'))).toEqual(['Italian fleets have held the Central Mediterranean since December 1940. Counting from then, the garrison of Malta is hungry by March and lost by the start of May, unless ships get through.']);
    const air = malta('ITA', ['9th Corps'], 16, (g) => { g.nations.ITA.air = 200; });
    month(air.g, 17);
    expect(texts(portCardLines(air.g, MALTA, 'ITA'))).toEqual(['Your aircraft have held the sky over the Central Mediterranean since December 1940. Counting from then, the garrison of Malta is hungry by March and lost by the start of May, unless ships get through.']);
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
    const line = 'Your fleets have held the Western Mediterranean and the waters off the Iberian coast since January. Counting from then, the garrison of Gibraltar is hungry by April and lost by the start of June, unless ships get through.';
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

  /**
   * Malta held by a British 9th Corps (and the crews given), its stores filled in December 1940, and Italy's aircraft
   * alone shutting the sea: no Italian fleet at sea, so nothing of Italy's sees Malta. December's months played.
   */
  function skyOnly(forts: boolean, crews: string[] = []): { g: Game; ninth: Corps } {
    const g = grand('ITA', true, forts);
    const [ninth] = garrison(g, 'Malta', ['9th Corps']);
    for (const n of crews) addCorps(g, 'UK', n, 'fort', 1, 'Malta', {});
    g.turn = 16;
    g.supplyDirty = true;
    computeSupply(g);
    declareWar(g, 'ITA', 'UK');
    g.nations.ITA.air = 200;
    g.supplyDirty = true;
    month(g, 16, ['ITA', 'UK']);
    return { g, ninth };
  }
  const SKY = 'Your aircraft have held the sky over the Central Mediterranean since December 1940. Counting from then, the garrison of Malta is hungry by March and lost by the start of May, unless ships get through.';

  it('out of its sight, the line is the same whether a British corps stands in Malta or none', () => {
    const { g, ninth } = skyOnly(false);
    month(g, 17, ['ITA', 'UK']);
    const r = g.regions[MALTA];
    expect([fogOn(g, 'ITA'), sightOf(g, 'ITA')[MALTA]]).toEqual([true, SIGHT.none]);
    expect(texts(portCardLines(g, MALTA, 'ITA'))).toEqual([SKY]);
    // The garrison taken off: Malta is still British, and Italy sees nothing of it, so its card says the same.
    removeCorps(g, ninth);
    expect([r.ctrl, sightOf(g, 'ITA')[MALTA]]).toEqual(['UK', SIGHT.none]);
    expect(texts(portCardLines(g, MALTA, 'ITA'))).toEqual([SKY]);
  });

  it('under the fortress rows, out of its sight, a crew it cannot see may keep the stores longer: no count, and the line waits, month by month while the crew\'s three months feed the garrison', () => {
    // (The review of the 23:50Z build, finding 2: counted from the port's own two, the line said "lost by the start of
    // May" from January to May, and the corps stood through May and was struck off as June opened.)
    const { g, ninth } = skyOnly(true, ['Malta fortress']);
    expect(portKeep(g, g.regions[MALTA])).toBe(3);
    const clock: [number, boolean, boolean][] = [];
    for (let t = 17; t <= 22; t++) {
      month(g, t, ['ITA', 'UK']);
      const f = portCutOff(g, MALTA, 'ITA')!;
      expect([t, sightOf(g, 'ITA')[MALTA], f.side, f.heldSince, f.keep, f.heldHungry, f.heldLost, portCardLines(g, MALTA, 'ITA')]).toEqual([t, SIGHT.none, 'shutting', 16, null, null, null, []]);
      clock.push([t, ninth.supplied, ninth.dead]);
    }
    // The rule's own clock: fed through March, hungry from April, struck off as June opens.
    expect(clock).toEqual([[17, true, false], [18, true, false], [19, true, false], [20, false, false], [21, false, false], [22, false, true]]);
    // With no crew there, the same: its sight cannot tell.
    const none = skyOnly(true);
    month(none.g, 17, ['ITA', 'UK']);
    expect([portKeep(none.g, none.g.regions[MALTA]), portCutOff(none.g, MALTA, 'ITA')!.keep, portCardLines(none.g, MALTA, 'ITA')]).toEqual([2, null, []]);
  });

  it('under the fortress rows, seen whole: the crew\'s stores are counted, and a port with no garrison for the rule to starve has no line', () => {
    const { g } = skyOnly(true, ['Malta fortress']);
    month(g, 17, ['ITA', 'UK']);
    const r = g.regions[MALTA];
    // With the fog lifted, Italy sees Malta whole: the crew and its stores are counted.
    g.flags.nofog = 1;
    expect(sightOf(g, 'ITA')[MALTA]).toBe(SIGHT.full);
    expect(portCutOff(g, MALTA, 'ITA')!.keep).toBe(3);
    expect(texts(portCardLines(g, MALTA, 'ITA'))).toEqual(['Your aircraft have held the sky over the Central Mediterranean since December 1940. Counting from then, the garrison of Malta is hungry by April and lost by the start of June, unless ships get through.']);
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
    expect(texts(portCardLines(g, gib, 'GER'))).toEqual(['Italian fleets have held the Western Mediterranean, and German fleets have held the waters off the Iberian coast since December 1940. Counting from then, the garrison of Gibraltar is hungry by March and lost by the start of May, unless ships get through.']);
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
    // his wars, so that a hold begun before his war with its holder is seen to begin: the UI director's 23:50Z, item 5;
    // and the mark of the last draw, one, whoever is at the table: the 05:03Z, item 5.)
    expect(Object.keys(us.g.flags).filter((k) => OWN.test(k) && !k.startsWith('seaShut:'))).toEqual(['portDraw:UK']);
    expect(us.g.flags['portDraw:UK']).toBe(21);
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

  it('turned off after the news of the stores spent was told: its mark goes with the rest, and the game saved holds none of it', () => {
    const { g } = malta('UK');
    for (let t = 17; t <= 19; t++) month(g, t);
    expect([g.flags[`portTold:${MALTA}`], g.flags[`portSpent:${MALTA}`]]).toEqual([17, 19]);
    expect(setPortStores(g, false)).toBe(true);
    expect(Object.keys(g.flags).filter((k) => k === 'portStores' || OWN.test(k))).toEqual([]);
    expect(saveGame(g)).not.toMatch(/portStores|portTold|portSpent|seaShut|portLost|portHad|portCrew|portDraw/);
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
      { tone: 'note', text: 'A fleet of yours at sea there, strong enough to open the Central Mediterranean, lets the ships in and fills the stores: one such month feeds the garrison through March. At sea in the Central Mediterranean today: yours 11, the enemy\'s 131, as found.' },
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
    expect(texts(portCardLines(g, MALTA, 'UK', { measure: true }))[1]).toMatch(/^A fleet of yours at sea there, strong enough to open the Central Mediterranean, lets the ships in and fills the stores: one such month feeds the garrison through May\. At sea in the Central Mediterranean today: yours \d+, the enemy's \d+, as found\.$/);
  });

  it('Gibraltar with both seas shut: a sentence for each sea the remedy line names, in its order', () => {
    const { g, gib } = gibraltar('UK', (h) => { sail(h, 'ITA', 'wmed', ['1st Fleet']); sail(h, 'ITA', 'iberia', ['2nd Fleet']); });
    month(g, 16);
    month(g, 17);
    expect(texts(portCardLines(g, gib, 'UK', { measure: true }))[1]).toBe('A fleet of yours at sea there, strong enough to open the Western Mediterranean or the waters off the Iberian coast, lets the ships in and fills the stores: one such month feeds the garrison through March. At sea in the Western Mediterranean today: yours 9, the enemy\'s 1, as found. At sea in the waters off the Iberian coast today: yours 5, the enemy\'s 0, as found.');
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
    // (The month's news whole: the struck-off line, and no line of the rule's beside it.)
    const one = malta('UK');
    for (let t = 17; t <= 20; t++) month(one.g, t);
    const o0 = one.g.headlines.length;
    month(one.g, 21);
    expect(news(one.g, o0)).toEqual(['Cut off on Malta, the 9th Corps has starved and is lost.']);
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
    expect(texts(portCardLines(g, MALTA, 'ITA'))).toEqual(['Your fleets have held the Central Mediterranean since April. Counting from then, the garrison of Malta is hungry by July and lost by the start of September, unless ships get through.']);
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
    expect(texts(portCardLines(g, gib, 'ITA'))).toEqual(['Your fleets have held the Western Mediterranean and the waters off the Iberian coast since January. Counting from then, the garrison of Gibraltar is hungry by April and lost by the start of June, unless ships get through.']);
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
    const line = 'German fleets have held the Central Mediterranean since October 1940. Counting from then, the garrison of Malta is hungry by January and lost by the start of March, unless ships get through.';
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

  /**
   * Gibraltar held by a British 9th Corps alone; Italy at war with Britain from August 1940, France's fleet in harbour;
   * Germany's Battle Fleet off the Iberian coast and Italy's 1st Fleet in the Western Mediterranean from October; and
   * `who`, at peace until then, comes into the war against Britain on the Axis side in January 1941. Its player reads
   * its card each month after Britain's.
   */
  function lateAtGibraltar(who: string): { g: Game; seas: number[]; card: Record<number, string[]>; clock: [number, boolean, boolean][] } {
    const g = grand(who);
    const gib = g.byName.Gibraltar.id;
    const [ninth] = garrison(g, 'Gibraltar', ['9th Corps']);
    const card: Record<number, string[]> = {}, clock: [number, boolean, boolean][] = [];
    for (let t = 12; t <= 19; t++) {
      g.turn = t;
      if (t === 12) { declareWar(g, 'ITA', 'UK'); for (const f of g.fleets) if (f.nat === 'FRA') f.stance = 'port'; }
      if (t === 17) { g.nations[who].align = 'axis'; declareWar(g, who, 'UK'); }
      g.supplyDirty = true;
      phase(g, 'GER');
      if (t === 14) { sail(g, 'GER', 'iberia', ['Battle Fleet']); g.supplyDirty = true; }
      phase(g, 'ITA');
      if (t === 14) { sail(g, 'ITA', 'wmed', ['1st Fleet']); g.supplyDirty = true; }
      phase(g, who);
      phase(g, 'UK');
      card[t] = texts(portCardLines(g, gib, who));
      clock.push([t, ninth.supplied, ninth.dead]);
    }
    return { g, seas: g.regions[gib].seas, card, clock };
  }

  it('Hungary comes into the war in January 1941, after Italy\'s and Germany\'s fleets shut both of Gibraltar\'s seas in October 1940: the month the switch saw, and the clock from then', () => {
    const { g, seas: [wmed, iberia], card, clock } = lateAtGibraltar('HUN');
    expect([g.flags[`seaShut:${wmed}:UK`], g.flags[`seaShut:${iberia}:UK`]]).toEqual([14, 14]);
    for (let t = 12; t <= 16; t++) expect([t, card[t]]).toEqual([t, []]);
    const line = 'Italian fleets have held the Western Mediterranean, and German fleets have held the waters off the Iberian coast since October 1940. Counting from then, the garrison of Gibraltar is hungry by January and lost by the start of March, unless ships get through.';
    expect([card[17], card[18]]).toEqual([[line], [line]]);
    expect(clock.slice(4)).toEqual([[16, true, false], [17, false, false], [18, false, false], [19, false, true]]);
  });

  it('Spain comes into the war at Gibraltar in January 1941, after the same holds: the ring closes at Britain\'s January, and while it stands the port is left to the pocket\'s lines and Spain\'s card has none (the UI director\'s 05:03Z, item 1); the record of the holds is kept, and the rule\'s clock is the same', () => {
    const { g, seas: [wmed, iberia], card, clock } = lateAtGibraltar('ESP');
    const gib = g.byName.Gibraltar.id;
    expect([g.flags[`seaShut:${wmed}:UK`], g.flags[`seaShut:${iberia}:UK`], g.flags[`pkt:${gib}`]]).toEqual([14, 14, 17]);
    // (The 03:13Z build gave Spain the line Hungary reads above, in January and February.)
    for (let t = 12; t <= 19; t++) expect([t, card[t]]).toEqual([t, []]);
    expect(clock.slice(4)).toEqual([[16, true, false], [17, false, false], [18, false, false], [19, false, true]]);
  });
});

describe('item 6: "by the start of" where the count can only be the latest month, "at" where it is exact', () => {
  const CARD = (month: string, word: string): string => `Cut off by sea: Italian fleets hold the Central Mediterranean, and no ship reaches the harbour. The garrison lives on the port's stores through February; unless the sea is opened, it then goes hungry and is lost ${word} the start of ${month}.`;

  it('the shutting side cannot see the garrison\'s order: its line always says "by"', () => {
    const { g } = malta('ITA');
    month(g, 17);
    expect(texts(portCardLines(g, MALTA, 'ITA'))).toEqual(['Your fleets have held the Central Mediterranean since December 1940. Counting from then, the garrison of Malta is hungry by March and lost by the start of May, unless ships get through.']);
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
    expect([f.lost, f.lostBy, f.garrison]).toEqual([20, true, [{ id: ninth.id, lost: 20, by: true, fed: 18 }]]);
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
    expect(portCutOff(g, MALTA, 'UK')!.garrison).toEqual([{ id: ninth.id, lost: 21, by: false, fed: 18 }]);
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
    // (The tray gives a card the width it asks for: the corps' line asks for no more than about four tiles' width, and
    // so wraps inside the card rather than stretch it. The review of the 23:50Z build, finding 4.)
    expect(HUD).toMatch(/SNew\(SBox\)\.MaxDesiredWidth\(340\.0f\)\s*\[\s*SNew\(STextBlock\)\.Visibility\(Card\.PortLine\.IsEmpty\(\)/);
  });
});

// ---- On the 23:50Z build (the UI director, 27 Nov 2026, 01:14Z) -------------------------------------------------------

describe('01:14Z, item 1: the measure\'s join, "as found" after the figure', () => {
  it('Malta in January 1941, the seas searched and not; Gibraltar\'s figures under 11 stay figures, a sentence for each sea', () => {
    const REMEDY = 'A fleet of yours at sea there, strong enough to open the Central Mediterranean, lets the ships in and fills the stores: one such month feeds the garrison through March.';
    const { g } = malta('UK');
    month(g, 17);
    expect(texts(portCardLines(g, MALTA, 'UK', { measure: true }))[1]).toBe(`${REMEDY} At sea in the Central Mediterranean today: yours 11, the enemy's 131, as found.`);
    delete g.flags.carriers;
    expect(texts(portCardLines(g, MALTA, 'UK', { measure: true }))[1]).toBe(`${REMEDY} At sea in the Central Mediterranean today: yours 11, the enemy's 151.`);
    const gib = gibraltar('UK', (h) => { sail(h, 'ITA', 'wmed', ['1st Fleet']); sail(h, 'ITA', 'iberia', ['2nd Fleet']); });
    month(gib.g, 16);
    month(gib.g, 17);
    expect(texts(portCardLines(gib.g, gib.gib, 'UK', { measure: true }))[1]).toBe('A fleet of yours at sea there, strong enough to open the Western Mediterranean or the waters off the Iberian coast, lets the ships in and fills the stores: one such month feeds the garrison through March. At sea in the Western Mediterranean today: yours 9, the enemy\'s 1, as found. At sea in the waters off the Iberian coast today: yours 5, the enemy\'s 0, as found.');
  });
});

describe('01:14Z, item 2: the shutting side\'s count is a bound at both ends, "is hungry by" and "lost by the start of"', () => {
  /**
   * Malta held by a British 9th Corps, Italy at war with Britain with her fleets in harbour (and, for the weather's
   * play, the weather on and her aircraft at `air`), and November 1940's months played: the sea open, the stores filled.
   */
  function openSea(human: string, air?: number): { g: Game; ninth: Corps } {
    const g = grand(human);
    const [ninth] = garrison(g, 'Malta', ['9th Corps']);
    declareWar(g, 'ITA', 'UK');
    for (const f of g.fleets) if (f.nat === 'ITA') f.stance = 'port';
    if (air !== undefined) { g.flags.weather = 1; g.nations.ITA.air = air; }
    g.supplyDirty = true;
    month(g, 15, ['ITA', 'UK']);
    expect([g.flags[`stores:${MALTA}`], g.flags['seaShut:20:UK']]).toEqual([15, -1]);
    return { g, ninth };
  }
  /** Italy's and Britain's months from January to May 1941: Italy's card after each, and the rule's own clock. */
  function play(g: Game, ninth: Corps, during?: (t: number) => void): { card: Record<number, string[]>; clock: [number, boolean, boolean][] } {
    const card: Record<number, string[]> = {}, clock: [number, boolean, boolean][] = [];
    for (let t = 17; t <= 21; t++) {
      g.turn = t;
      phase(g, 'ITA');
      during?.(t);
      phase(g, 'UK');
      card[t] = texts(portCardLines(g, MALTA, 'ITA'));
      clock.push([t, ninth.supplied, ninth.dead]);
    }
    return { card, clock };
  }
  const FLEETS = (since: string, hungry: string, lost: string): string => `Your fleets have held the Central Mediterranean since ${since}. Counting from then, the garrison of Malta is hungry by ${hungry} and lost by the start of ${lost}, unless ships get through.`;

  it('a hold begun in the month, after the holder\'s fill: the count is the rule\'s own, hungry from March and struck off as May opens', () => {
    const { g, ninth } = openSea('ITA');
    // December 1940: Italy's month opens with the sea open (the stores filled), then her fleets put to sea, and Britain's
    // month opens with it shut.
    g.turn = 16;
    phase(g, 'ITA');
    sail(g, 'ITA', 'central');
    g.supplyDirty = true;
    phase(g, 'UK');
    expect([g.flags[`stores:${MALTA}`], g.flags['seaShut:20:UK']]).toEqual([16, 16]);
    const { card, clock } = play(g, ninth);
    const line = FLEETS('December 1940', 'March', 'May');
    for (let t = 17; t <= 20; t++) expect([t, card[t]]).toEqual([t, [line]]);
    expect(clock).toEqual([[17, true, false], [18, true, false], [19, false, false], [20, false, false], [21, false, true]]);
  });

  it('a fleet put to sea between two months: first seen in January, after December\'s fill, so the rule acts a month sooner than counted, and "by" holds', () => {
    const { g, ninth } = openSea('ITA');
    month(g, 16, ['ITA', 'UK']);
    // (After the last month of December to open: in the war, a fleet sailed in the last nation's month.)
    sail(g, 'ITA', 'central');
    g.supplyDirty = true;
    const { card, clock } = play(g, ninth);
    expect([g.flags[`stores:${MALTA}`], g.flags['seaShut:20:UK']]).toEqual([16, 17]);
    const line = FLEETS('January', 'April', 'June');
    for (let t = 17; t <= 20; t++) expect([t, card[t]]).toEqual([t, [line]]);
    // The rule's own clock: hungry from March, by April; struck off as May opens, by the start of June. ("goes hungry
    // from April" would have named a month after the hunger began.)
    expect(clock).toEqual([[17, true, false], [18, true, false], [19, false, false], [20, false, false], [21, false, true]]);
  });

  it('the same record, the stores filled a month later: a draw as the month turns, with the sea still open, then the hold before the first month opens; Italy\'s card the same, and the rule acts as counted', () => {
    const { g, ninth } = openSea('ITA');
    month(g, 16, ['ITA', 'UK']);
    // (A computer's month ends with a draw of the supply picture, after the last one has turned the month: turn.ts stepAI.)
    g.turn = 17;
    g.supplyDirty = true;
    computeSupply(g);
    expect(g.flags[`stores:${MALTA}`]).toBe(17);
    sail(g, 'ITA', 'central');
    g.supplyDirty = true;
    const { card, clock } = play(g, ninth);
    // The hold first seen as Italy's January opens, as in the play before: the shutting side's sight is the same, and so
    // is its line. Only the holder's draws say whether his ships last got in in December or in January.
    expect([g.flags[`stores:${MALTA}`], g.flags['seaShut:20:UK']]).toEqual([17, 17]);
    const line = FLEETS('January', 'April', 'June');
    for (let t = 17; t <= 20; t++) expect([t, card[t]]).toEqual([t, [line]]);
    expect(clock).toEqual([[17, true, false], [18, true, false], [19, true, false], [20, false, false], [21, false, false]]);
    g.turn = 22;
    phase(g, 'ITA');
    phase(g, 'UK');
    expect(ninth.dead).toBe(true);
  });

  it('the weather turning as January opens: Italy\'s aircraft hold the sky over the sea under January\'s clear sky, not December\'s cloud; first seen in January, and "by" holds', () => {
    const { g, ninth } = openSea('ITA', 100);
    const z = zoneOf(g, 'central');
    month(g, 16, ['ITA', 'UK']);
    expect([skyAtSea(g, z), closedTo(g, z, 'UK'), g.flags[`stores:${MALTA}`]]).toEqual(['cloud', false, 16]);
    // (Italy's air is strengthened in her January month, so that the hold lasts through the cloud of the spring.)
    const { card, clock } = play(g, ninth, (t) => { if (t === 17) { g.nations.ITA.air = 150; g.supplyDirty = true; } });
    g.turn = 17;
    expect([skyAtSea(g, z), g.flags[`stores:${MALTA}`], g.flags['seaShut:20:UK']]).toEqual(['clear', 16, 17]);
    const line = 'Your aircraft have held the sky over the Central Mediterranean since January. Counting from then, the garrison of Malta is hungry by April and lost by the start of June, unless ships get through.';
    for (let t = 17; t <= 20; t++) expect([t, card[t]]).toEqual([t, [line]]);
    expect(clock).toEqual([[17, true, false], [18, true, false], [19, false, false], [20, false, false], [21, false, true]]);
  });
});

/**
 * Under the fortress rows: Malta held by a British 9th Corps and its fortress crew (the book's three months of stores
 * for a row it gives no figure), and by any corps `also` sets down; the stores filled by a real draw in December 1940
 * (turn 16), Italy at war with Britain and her fleets at sea off it, and December's months played.
 */
function crewed(human: string, also: (g: Game) => void = () => {}, nats = ['ITA', 'UK']): { g: Game; ninth: Corps; crew: Corps } {
  const g = grand(human, true, true);
  const [ninth] = garrison(g, 'Malta', ['9th Corps']);
  const crew = addCorps(g, 'UK', 'Malta fortress', 'fort', 1, 'Malta', {});
  also(g);
  g.turn = 16;
  g.supplyDirty = true;
  computeSupply(g);
  declareWar(g, 'ITA', 'UK');
  sail(g, 'ITA', 'central');
  g.supplyDirty = true;
  month(g, 16, nats);
  expect([g.flags[`stores:${MALTA}`], portKeep(g, g.regions[MALTA])]).toEqual([16, 3]);
  return { g, ninth, crew };
}
const HELD = 'Cut off by sea: Italian fleets hold the Central Mediterranean, and no ship reaches the harbour.';
const REMEDY = (through: string): string => `A fleet of yours at sea there, strong enough to open the Central Mediterranean, lets the ships in and fills the stores: one such month feeds the garrison through ${through}.`;
/** The remedy line where a fill this month may feed the garrison less long than the keep (the UI director's 03:13Z, item 1). */
const LEAST = (through: string): string => `A fleet of yours at sea there, strong enough to open the Central Mediterranean, lets the ships in and fills the stores: one such month feeds the garrison at least through ${through}.`;

describe('01:14Z, item 3: the keep the stores were filled with, under the fortress rows', () => {
  it('the holder, month by month: the crew starves as March is charged, the rule feeds the garrison through March on the crew\'s three months, and every line counts them', () => {
    const { g, ninth, crew } = crewed('UK');
    const r = g.regions[MALTA];
    expect(texts(portCardLines(g, MALTA, 'UK'))).toEqual(['Cut off by sea, its stores would feed the garrison for three months.']);
    const rows: unknown[] = [];
    for (let t = 17; t <= 22; t++) {
      const h0 = g.headlines.length;
      month(g, t);
      rows.push([t, crew.dead, crew.dead ? null : storesEaten(g, crew), portKeep(g, r), ninth.supplied, Math.round(ninth.org * 100) / 100, ninth.dead, news(g, h0), texts(portCardLines(g, MALTA, 'UK')), ninth.dead ? null : portCorpsLine(g, ninth)]);
    }
    const STORES = `${HELD} The garrison lives on the port's stores through March; unless the sea is opened, it then goes hungry and is lost at the start of June.`;
    const SPENT = 'Cut off by sea, and the port\'s stores are spent: the garrison goes hungry, losing order every month, and is lost at the start of June unless the sea is opened.';
    const HUNGRY = 'Hungry: Malta\'s stores are spent. Lost at the start of June unless ships get through.';
    // (January and February: a fill this month comes after Britain's reckoning, which has eaten the crew's month, so it
    // starves in March whatever: the remedy line counts the shorter and says it is the least, the UI director's 03:13Z,
    // item 1. Counted with the keep, the build said "through April" and "through May" here.)
    expect(rows).toEqual([
      [17, false, 1, 3, true, 1, false, ['No ship reaches Malta: Italian fleets hold the Central Mediterranean. The garrison lives on the port\'s stores through March; unless the sea is opened, it then goes hungry and is lost at the start of June.'], [STORES, LEAST('March')], 'Living on Malta\'s stores through March.'],
      [18, false, 2, 3, true, 1, false, [], [STORES, LEAST('April')], 'Living on Malta\'s stores through March.'],
      // March: the crew eats the last of its three months and starves in Britain's reckoning, after the draw that fed
      // the garrison for the month. The rule's keep is the port's own two from then on (game.ts portKeep). (Counted with
      // two, the build said here "the port's stores are spent" of a garrison fed this month, and no corps line.)
      [19, true, null, 2, true, 1, false, ['Cut off on Malta, the Malta fortress has starved and is lost.'], [STORES, REMEDY('May')], 'Living on Malta\'s stores through March.'],
      // April: hungry by either keep. (Counted with two, the build never told the stores spent.)
      [20, true, null, 2, false, 0.6, false, ['Malta\'s stores are spent and no ship has got through: the garrison goes hungry, and is lost at the start of June unless the sea is opened.'], [SPENT, REMEDY('June')], HUNGRY],
      [21, true, null, 2, false, 0.2, false, [], [SPENT, REMEDY('July')], HUNGRY],
      // Struck off as June opens, the month every line named.
      [22, true, null, 2, false, -0.2, true, ['Cut off on Malta, the 9th Corps has starved and is lost.'], [], null],
    ]);
    // (The crew's record, kept until the stores are filled again.)
    expect(Object.keys(g.flags).filter((k) => k.startsWith('portCrew:'))).toEqual([`portCrew:${MALTA}:${crew.id}:UK:3:${crew.born}`]);
    expect(g.flags[`portCrew:${MALTA}:${crew.id}:UK:3:${crew.born}`]).toBe(19);
  });

  it('the shutting side, seeing Malta whole: counted with the crew\'s three months before it starves and after, "is hungry by April and lost by the start of June", and so it is', () => {
    const { g, ninth, crew } = crewed('ITA');
    g.flags.nofog = 1;
    expect(sightOf(g, 'ITA')[MALTA]).toBe(SIGHT.full);
    const line = 'Your fleets have held the Central Mediterranean since December 1940. Counting from then, the garrison of Malta is hungry by April and lost by the start of June, unless ships get through.';
    const rows: unknown[] = [];
    for (let t = 17; t <= 22; t++) {
      month(g, t, ['ITA', 'UK']);
      rows.push([t, crew.dead, portCutOff(g, MALTA, 'ITA')!.keep, ninth.supplied, ninth.dead, texts(portCardLines(g, MALTA, 'ITA'))]);
    }
    // (Counted with the two the crew leaves, the line said from March "goes hungry from March and is lost by the start of
    // May": a month before the loss.)
    expect(rows).toEqual([
      [17, false, 3, true, false, [line]],
      [18, false, 3, true, false, [line]],
      [19, true, 3, true, false, [line]],
      [20, true, 3, false, false, [line]],
      [21, true, 3, false, false, [line]],
      // Struck off as June opens: seen whole, no garrison is left for the rule to starve, and the line goes.
      [22, true, 3, false, true, []],
    ]);
  });

  it('the rule changes the keep mid-way for a corps whose nation\'s month comes after the crew\'s: drawn in March with the crew gone, it is fed through February, and the count follows the rule', () => {
    // (A Canadian corps at Malta, Canada at war with Italy: its month opens after Britain's. Britain's player reads his
    // cards in his own month, before Canada's opens.)
    const { g, ninth } = crewed('UK', (h) => { addCorps(h, 'CAN', '1st Canadian Corps', 'inf', 2, 'Malta', {}); declareWar(h, 'ITA', 'CAN'); }, ['ITA', 'UK', 'CAN']);
    const canadian = g.corps.find((c) => c.name === '1st Canadian Corps')!;
    expect(g.order.indexOf('CAN')).toBeGreaterThan(g.order.indexOf('UK'));
    const rows: unknown[] = [], clock: unknown[] = [];
    for (let t = 17; t <= 22; t++) {
      const h0 = g.headlines.length;
      g.turn = t;
      phase(g, 'ITA');
      phase(g, 'UK');
      rows.push([t, news(g, h0), texts(portCardLines(g, MALTA, 'UK')).slice(0, 1), [ninth, canadian].map((c) => (c.dead ? null : portCorpsLine(g, c)))]);
      const fed = ninth.supplied;
      phase(g, 'CAN');
      // (Each corps as its own nation's draw left it for its reckoning.)
      clock.push([t, fed, ninth.dead, canadian.supplied, canadian.dead]);
    }
    const STORES = `${HELD} The garrison lives on the port's stores through February; unless the sea is opened, it then goes hungry and is lost at the start of May.`;
    const SPENT = (lost: string): string => `Cut off by sea, and the port's stores are spent: the garrison goes hungry, losing order every month, and is lost at the start of ${lost} unless the sea is opened.`;
    const HUNGRY = (lost: string): string => `Hungry: Malta's stores are spent. Lost at the start of ${lost} unless ships get through.`;
    expect(rows).toEqual([
      [17, ['No ship reaches Malta: Italian fleets hold the Central Mediterranean. The garrison lives on the port\'s stores through February; unless the sea is opened, it then goes hungry and is lost at the start of May.'], [STORES], ['Living on Malta\'s stores through March.', 'Living on Malta\'s stores through February.']],
      [18, [], [STORES], ['Living on Malta\'s stores through March.', 'Living on Malta\'s stores through February.']],
      // March: Britain's draw feeds the 9th on the crew's stores, and the crew starves in Britain's reckoning. Canada's
      // draw, after it, will read the port's own two: the Canadian corps' stores are spent this month. Its card reads the
      // rule's clock as it stands, though the picture drawn with the crew living still feeds it until that draw (the UI
      // director's 03:13Z, item 3: the build gave it no line here).
      [19, ['Malta\'s stores are spent and no ship has got through: the garrison goes hungry, and is lost at the start of May unless the sea is opened.', 'Cut off on Malta, the Malta fortress has starved and is lost.'], [SPENT('May')], ['Living on Malta\'s stores through March.', HUNGRY('May')]],
      [20, [], [SPENT('May')], [HUNGRY('June'), HUNGRY('May')]],
      [21, [], [SPENT('May')], [HUNGRY('June'), HUNGRY('May')]],
      // The Canadian corps struck off as May opens in Canada's month, told as Britain's next month opens with the 9th,
      // struck off as June opens.
      [22, ['Cut off on Malta, two corps have starved and are lost: the 1st Canadian Corps and the 9th Corps.'], [], [null, null]],
    ]);
    // The rule's own clock: the 9th fed through March, the Canadian through February.
    expect(clock).toEqual([
      [17, true, false, true, false], [18, true, false, true, false], [19, true, false, false, false],
      [20, false, false, false, false], [21, false, false, false, true], [22, false, true, false, true],
    ]);
  });

  it('an ally\'s corps whose own ships still reach the port is fed by them, and is no part of the garrison counted', () => {
    // (Canada at war with Germany only: the Central Mediterranean is open to her ships.)
    const { g } = crewed('UK', (h) => { addCorps(h, 'CAN', '1st Canadian Corps', 'inf', 2, 'Malta', {}); }, ['ITA', 'UK', 'CAN']);
    const canadian = g.corps.find((c) => c.name === '1st Canadian Corps')!;
    expect([g.nations.CAN.atWar.includes('ITA'), closedTo(g, zoneOf(g, 'central'), 'CAN')]).toEqual([false, false]);
    for (let t = 17; t <= 19; t++) month(g, t, ['ITA', 'UK', 'CAN']);
    expect([canadian.supplied, portCutOff(g, MALTA, 'UK')!.garrison.map((x) => x.id), portCorpsLine(g, canadian)]).toEqual([true, [g.corps.find((c) => c.name === '9th Corps')!.id], null]);
    expect(texts(portCardLines(g, MALTA, 'UK'))[0]).toBe(`${HELD} The garrison lives on the port's stores through March; unless the sea is opened, it then goes hungry and is lost at the start of June.`);
  });

  it('the crew\'s record goes once the stores are filled again, and with the switch', () => {
    const { g } = crewed('UK');
    for (let t = 17; t <= 19; t++) month(g, t);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('portCrew:'))).toHaveLength(1);
    for (const f of g.fleets) if (f.nat === 'ITA') f.stance = 'port';
    g.supplyDirty = true;
    month(g, 20);
    expect([g.flags[`stores:${MALTA}`], Object.keys(g.flags).filter((k) => k.startsWith('portCrew:'))]).toEqual([20, []]);
    expect(texts(portCardLines(g, MALTA, 'UK'))).toEqual(['Cut off by sea, its stores would feed the garrison for two months.']);
    const off = crewed('UK');
    for (let t = 17; t <= 19; t++) month(off.g, t);
    expect(setPortStores(off.g, false)).toBe(true);
    expect(Object.keys(off.g.flags).filter((k) => OWN.test(k))).toEqual([]);
  });
});

describe('03:13Z, item 1: the remedy line while a crew\'s clock runs, "at least through"', () => {
  it('Britain\'s own fleets fill Malta\'s stores in January after his reckoning, and the crew still starves in March: "one such month feeds the garrison at least through March", the stores filled "would feed the garrison for at least two months" (05:03Z, item 2), and so it is', () => {
    const { g, ninth, crew } = crewed('UK');
    const r = g.regions[MALTA];
    // January: Britain's reckoning has eaten a month of the crew's three. A fill now comes after it and does not restock
    // the crew (it is restocked only in a reckoning fed all month), so it starves in March whatever the fill: the line
    // counts that, and says it is the least. (Counted with the keep, the build said "through April".)
    month(g, 17);
    expect([storesEaten(g, crew), portKeep(g, r)]).toEqual([1, 3]);
    const STORES = `${HELD} The garrison lives on the port's stores through March; unless the sea is opened, it then goes hungry and is lost at the start of June.`;
    expect(texts(portCardLines(g, MALTA, 'UK'))).toEqual([STORES, LEAST('March')]);
    // His fleets put to sea in the Central Mediterranean, strong enough to open it: the next draw of the picture, still
    // in January, lets the ships in and fills the stores. The crew's count runs on.
    const home = g.fleets.filter((f) => f.nat === 'UK').map((f) => ({ f, zone: f.zone, stance: f.stance }));
    sail(g, 'UK', 'central');
    g.supplyDirty = true;
    computeSupply(g);
    expect([closedTo(g, zoneOf(g, 'central'), 'UK'), g.flags[`stores:${MALTA}`], storesEaten(g, crew)]).toEqual([false, 17, 1]);
    // With the sea open and the stores just filled, the line counts the crew's months as the remedy line does, and says
    // it is the least: the crew starves in March, and the fill feeds the garrison through February and March (the UI
    // director's 05:03Z, item 2; the rows below). (The 03:13Z build said "for three months", the keep.)
    expect(texts(portCardLines(g, MALTA, 'UK'))).toEqual(['Cut off by sea, its stores would feed the garrison for at least two months.']);
    // His fleets go home, and Italy's still hold the sea: one such month, and the months after it.
    for (const x of home) { x.f.zone = x.zone; x.f.stance = x.stance; }
    g.supplyDirty = true;
    const rows: unknown[] = [];
    for (let t = 18; t <= 22; t++) {
      const h0 = g.headlines.length;
      month(g, t);
      rows.push([t, crew.dead, crew.dead ? null : storesEaten(g, crew), ninth.supplied, Math.round(ninth.org * 100) / 100, ninth.dead, news(g, h0), texts(portCardLines(g, MALTA, 'UK')), ninth.dead ? null : portCorpsLine(g, ninth)]);
    }
    const SPENT = 'Cut off by sea, and the port\'s stores are spent: the garrison goes hungry, losing order every month, and is lost at the start of June unless the sea is opened.';
    const HUNGRY = 'Hungry: Malta\'s stores are spent. Lost at the start of June unless ships get through.';
    expect(rows).toEqual([
      // February: the crew eats its second month, and the line counts from it: a fill in February leaves the crew to
      // starve in March, and the port's own two feed the garrison through April.
      // The stores filled in January, and the sea shut again: relieved and shut again in one month, the news tells both,
      // in order (the UI director's 05:03Z, item 3). (The 03:13Z build told the relief alone, and the shut line never.)
      [18, false, 2, true, 1, false, ['Ships reach Malta again: its stores are filled.', `No ship reaches Malta: Italian fleets hold the Central Mediterranean. The garrison lives on the port's stores through March; unless the sea is opened, it then goes hungry and is lost at the start of June.`], [STORES, LEAST('April')], 'Living on Malta\'s stores through March.'],
      // March: the crew starves in Britain's reckoning, after the draw that fed the garrison on its stores, as January's
      // line counted. No crew's clock runs now: the line is as laid.
      [19, true, null, true, 1, false, ['Cut off on Malta, the Malta fortress has starved and is lost.'], [STORES, REMEDY('May')], 'Living on Malta\'s stores through March.'],
      // April: the stores the January fill gave are spent, through March and no further, the month January's line named.
      [20, true, null, false, 0.6, false, ['Malta\'s stores are spent and no ship has got through: the garrison goes hungry, and is lost at the start of June unless the sea is opened.'], [SPENT, REMEDY('June')], HUNGRY],
      [21, true, null, false, 0.2, false, [], [SPENT, REMEDY('July')], HUNGRY],
      [22, true, null, false, -0.2, true, ['Cut off on Malta, the 9th Corps has starved and is lost.'], [], null],
    ]);
  });

  it('elsewhere as laid: with the fortress rows off, and under them with no crew\'s clock running, the line counts the keep from this month', () => {
    const { g } = malta('UK', ['9th Corps'], 16);
    month(g, 17);
    expect(texts(portCardLines(g, MALTA, 'UK'))[1]).toBe(REMEDY('March'));
    const { g: h, crew } = crewed('UK');
    for (let t = 17; t <= 19; t++) month(h, t);
    expect([crew.dead, texts(portCardLines(h, MALTA, 'UK'))[1]]).toEqual([true, REMEDY('May')]);
  });
});

describe('03:13Z, item 2: a crew lost in battle, the count on the rule\'s own clock', () => {
  /**
   * Under the fortress rows: Gibraltar held by its fortress crew in the field (the book's Gibraltar garrison, 16 months
   * of stores) and a British 9th Corps in the town; the stores filled by a real draw in December 1940 (turn 16); Italy
   * and Germany at war with Britain, France's fleet in harbour, and Italy's 1st Fleet in the Western Mediterranean. Her
   * 2nd Fleet off the Iberian coast from the first, or with `watch` only from Italy's January (so that the switch sees
   * the hold begin, and her own January draw finds the Iberian waters still open and fills the stores). The switch on,
   * or as asked.
   */
  function gibCrewed(human: string, watch = false, on = true): { g: Game; gib: number; ninth: Corps; crew: Corps } {
    const g = grand(human, on, true);
    const r = g.byName.Gibraltar;
    const crew = r.units.find((c) => c.kind === 'fort')!;
    expect([crew.name, crew.nat, crewStores(crew)]).toEqual(['Gibraltar garrison', 'UK', 16]);
    const ninth = addCorps(g, 'UK', '9th Corps', 'inf', 2, 'Gibraltar', {});
    r.units.splice(r.units.indexOf(ninth), 1);
    joinTown(g, r, ninth);
    g.turn = 16;
    g.supplyDirty = true;
    computeSupply(g);
    declareWar(g, 'ITA', 'UK');
    declareWar(g, 'GER', 'UK');
    for (const f of g.fleets) if (f.nat === 'FRA') f.stance = 'port';
    sail(g, 'ITA', 'wmed', ['1st Fleet']);
    if (!watch) sail(g, 'ITA', 'iberia', ['2nd Fleet']);
    g.supplyDirty = true;
    expect(g.flags[`stores:${r.id}`]).toBe(16);
    return { g, gib: r.id, ninth, crew };
  }
  /**
   * The battle, in the attacker's own month: a landing from Sardinia (Italy's) or an attack from Cadiz (Spain's) on the
   * crew, worn to 12 in 100 of its strength. Beaten and shattered, it is destroyed (game.ts resolveBattle), and the
   * attackers do not stay: Gibraltar stays British, the 9th in its town, and no army stands round it.
   */
  function crewBeaten(g: Game, gib: number, crew: Corps, by: 'ITA' | 'ESP'): void {
    const att = addCorps(g, by, by === 'ITA' ? '30th Corps' : '1st Spanish Corps', 'inf', 4, by === 'ITA' ? 'Sardinia' : 'Cadiz', {});
    crew.str = 0.12;
    const rep = resolveBattle(g, [att], g.regions[gib], by === 'ITA' ? { sea: zoneOf(g, 'wmed'), occupy: 0 } : { occupy: 0 });
    expect([rep.defenders.map((d) => [d.name, d.fate]), crew.dead, g.regions[gib].ctrl, fieldCtrl(g, g.regions[gib])]).toEqual([[['Gibraltar garrison', 'destroyed']], true, 'UK', 'UK']);
  }
  const OURS = /^(No ship reaches |Ships reach |Cut off (on|in) |.*'s stores are spent and no ship)/;
  const ours = (g: Game, from: number): string[] => g.headlines.slice(from).map((h) => h.text).filter((t) => OURS.test(t));
  const HELD_G = 'Cut off by sea: Italian fleets hold the Western Mediterranean and the waters off the Iberian coast, and no ship reaches the harbour.';
  const STORES_G = (through: string, lost: string): string => `${HELD_G} The garrison lives on the port's stores through ${through}; unless the sea is opened, it then goes hungry and is lost at the start of ${lost}.`;
  const SPENT_G = (lost: string): string => `Cut off by sea, and the port's stores are spent: the garrison goes hungry, losing order every month, and is lost at the start of ${lost} unless the sea is opened.`;
  const REMEDY_G = (feeds: string): string => `A fleet of yours at sea there, strong enough to open the Western Mediterranean or the waters off the Iberian coast, lets the ships in and fills the stores: one such month feeds the garrison ${feeds}.`;
  const NEWS_SPENT = (lost: string): string => `Gibraltar's stores are spent and no ship has got through: the garrison goes hungry, and is lost at the start of ${lost} unless the sea is opened.`;
  const LIVING_G = (through: string): string => `Living on Gibraltar's stores through ${through}.`;
  const HUNGRY_G = (lost: string): string => `Hungry: Gibraltar's stores are spent. Lost at the start of ${lost} unless ships get through.`;
  /**
   * Britain's player at Gibraltar from January to the month the 9th is struck off, the crew lost in battle in `by`'s
   * April. Where the battle is Spain's, she is at war from then, closes the ring round the port as Britain's May opens
   * (siege.ts pocketMonth), and makes peace in her May.
   */
  function play(by: 'ITA' | 'ESP'): { rows: unknown[]; record: string[]; bare: number | null } {
    const { g, gib, ninth, crew } = gibCrewed('UK');
    const rows: unknown[] = [];
    let bare: number | null = null;
    for (let t = 17; ; t++) {
      const h0 = g.headlines.length;
      g.turn = t;
      phase(g, 'ITA');
      if (t === 20 && by === 'ITA') crewBeaten(g, gib, crew, 'ITA');
      phase(g, 'UK');
      if (t === 20 && by === 'ESP') {
        declareWar(g, 'ESP', 'UK');
        phase(g, 'ESP');
        crewBeaten(g, gib, crew, 'ESP');
      } else if (t > 20 && by === 'ESP') phase(g, 'ESP');
      if (t === 21 && by === 'ESP') expect(signPeace(g, 'ESP', 'UK')).toBe(true);
      rows.push([t, ninth.supplied, Math.round(ninth.org * 100) / 100, ninth.dead, ours(g, h0), texts(portCardLines(g, gib, 'UK')), ninth.dead ? null : portCorpsLine(g, ninth)]);
      if (t === 20) {
        // (The same month read without the record: the count the build made, from the port's own two.)
        const h = structuredClone(g);
        for (const k of Object.keys(h.flags)) if (k.startsWith('portCrew:')) delete h.flags[k];
        bare = portCutOff(h, gib, 'UK')!.feedsTo;
      }
      if (ninth.dead) break;
    }
    return { rows, record: Object.keys(g.flags).filter((k) => k.startsWith('portCrew:')), bare };
  }
  const BEFORE = (t: number): unknown[] => [t, true, 1, false, t === 17 ? ['No ship reaches Gibraltar: Italian fleets hold the Western Mediterranean and the waters off the Iberian coast. The garrison lives on the port\'s stores through April 1942; unless the sea is opened, it then goes hungry and is lost at the start of July 1942.'] : [], [STORES_G('April 1942', 'July 1942'), REMEDY_G('at least through April 1942')], LIVING_G('April 1942')];

  it('Italy\'s landing in her April, before Britain\'s draw: the draw reads the stores without the crew, the garrison is hungry from April, and every line says so in the month the rule acts', () => {
    const { rows, record, bare } = play('ITA');
    // (January to March, the crew living: the garrison lives on its 16 months, and a fill now would come after
    // Britain's reckoning, leaving the crew to starve in April 1942 as it would have.)
    expect(rows).toEqual([
      BEFORE(17), BEFORE(18), BEFORE(19),
      // April: the crew destroyed in Italy's month, before Britain's draw, which reads the port's own two, four months
      // after the fill: the 9th goes hungry. The month the stores run out is told in it.
      [20, false, 0.6, false, [NEWS_SPENT('June')], [SPENT_G('June'), REMEDY_G('through June')], HUNGRY_G('June')],
      [21, false, 0.2, false, [], [SPENT_G('June'), REMEDY_G('through July')], HUNGRY_G('June')],
      // Struck off as June opens, the month every line named.
      [22, false, -0.2, true, ['Cut off in Gibraltar, the 9th Corps has starved and is lost.'], [], null],
    ]);
    // The record: the last draw that saw the crew living was Italy's in April, and the count read it so. (Without it,
    // the count read the port's own two from December, "through February", and never told the month the stores ran out.)
    expect([record.length, record[0].endsWith(':UK:16:1:ITA'), bare]).toEqual([1, true, 18]);
  });

  it('Spain\'s attack from Cadiz in her April, after Britain\'s draw: that draw fed the garrison on the crew\'s stores, and it is hungry from May; the ring Spain closes in May is left to the pocket\'s lines, and once her peace breaks it, every line counts April as fed', () => {
    const { rows, record, bare } = play('ESP');
    expect(rows).toEqual([
      BEFORE(17), BEFORE(18), BEFORE(19),
      // April, read after the battle: Britain's draw, before it, fed the 9th on the crew's stores for April. The draws
      // after it read the port's own two: no month after April is fed. (Without the record, the build's count said here
      // that the stores were spent, of a garrison fed this month.)
      [20, true, 1, false, [], [STORES_G('April', 'July'), REMEDY_G('through June')], LIVING_G('April')],
      // May: Spain at war beside it, the ring closes as Britain's month opens, and the port is left to the pocket's own
      // lines (the UI director's 05:03Z, item 1): no line of the port's. (The 03:13Z build told the stores spent here.)
      [21, false, 0.6, false, [], [], null],
      // June: Spain's peace has broken the ring, and the port's lines come back on the rule's own clock, the record of
      // the crew lost in April kept through the pocket: hungry since May, and struck off as July opens. The news of the
      // stores spent, which May's reading could not tell, comes in June's, as laid (the UI director's 05:03Z, item 4).
      [22, false, 0.2, false, [NEWS_SPENT('July')], [SPENT_G('July'), REMEDY_G('through August')], HUNGRY_G('July')],
      [23, false, -0.2, true, ['Cut off in Gibraltar, the 9th Corps has starved and is lost.'], [], null],
    ]);
    expect([record.length, record[0].endsWith(':UK:16:1:ESP'), bare]).toEqual([1, true, 18]);
  });

  it('the shutting side, seeing Gibraltar whole: its count follows the same clock, "is hungry by May and lost by the start of July" once the crew is lost in May, and so it is', () => {
    const { g, gib, ninth, crew } = gibCrewed('ITA', true);
    g.flags.nofog = 1;
    const rows: unknown[] = [];
    for (let t = 16; t <= 23; t++) {
      g.turn = t;
      phase(g, 'ITA');
      if (t === 17) { sail(g, 'ITA', 'iberia', ['2nd Fleet']); g.supplyDirty = true; }
      if (t === 21) crewBeaten(g, gib, crew, 'ITA');
      // (Italy's player reads his card in his own month.)
      rows.push([t, texts(portCardLines(g, gib, 'ITA'))]);
      phase(g, 'UK');
      rows.push([t, ninth.supplied, ninth.dead]);
    }
    expect(g.flags[`stores:${gib}`]).toBe(17);
    const line = (hungry: string, lost: string): string[] => [`Your fleets have held the Western Mediterranean and the waters off the Iberian coast since January. Counting from then, the garrison of Gibraltar is hungry by ${hungry} and lost by the start of ${lost}, unless ships get through.`];
    expect(rows).toEqual([
      [16, []], [16, true, false],
      // (January: the Iberian waters are shut after Italy's month opens; the hold is seen to begin as Britain's opens.)
      [17, []], [17, true, false],
      // The crew living: counted for all of its 16 months.
      [18, line('June 1942', 'August 1942')], [18, true, false],
      [19, line('June 1942', 'August 1942')], [19, true, false],
      [20, line('June 1942', 'August 1942')], [20, true, false],
      // May, read after the battle: the draws before it fed the garrison on the crew's stores through April, and those
      // after it read the port's own two from the January fill. (The build, which kept no record of a crew lost in
      // battle, read the port's own two for every month: "is hungry by April and lost by the start of June", a month
      // before each, and in July no line.)
      [21, line('May', 'July')], [21, false, false],
      [22, line('May', 'July')], [22, false, false],
      // Struck off as July opens, in Britain's month.
      [23, line('May', 'July')], [23, false, true],
    ]);
  });

  it('05:03Z, item 4: the crew lost after Britain\'s player has read his April card, and a Canadian corps, whose nation\'s draw comes after, starving that month: the news of the stores spent, which April\'s reading could not tell, comes in May\'s, as laid', () => {
    const { g, gib, ninth, crew } = gibCrewed('UK');
    const canadian = addCorps(g, 'CAN', '1st Canadian Corps', 'inf', 2, 'Gibraltar', {});
    declareWar(g, 'ITA', 'CAN');
    g.supplyDirty = true;
    const state = (c: Corps): unknown => (c.dead ? null : [c.supplied, Math.round(c.org * 100) / 100]);
    const rows: unknown[] = [];
    for (let t = 17; t <= 23; t++) {
      const h0 = g.headlines.length;
      g.turn = t;
      phase(g, 'ITA');
      phase(g, 'UK');
      // (Britain's player reads his card in his own month; in April the crew is struck off after it, game.ts removeCorps,
      // whatever takes it: Canada's draw of April, after it, is made without the crew.)
      // (Each corps as its own nation's month leaves it.)
      const read = [state(ninth), ours(g, h0), texts(portCardLines(g, gib, 'UK')), ninth.dead ? null : portCorpsLine(g, ninth), canadian.dead ? null : portCorpsLine(g, canadian)];
      if (t === 20) removeCorps(g, crew);
      phase(g, 'CAN');
      rows.push([t, read[0], state(canadian), ...read.slice(1)]);
    }
    const STORES_C = STORES_G('March 1942', 'June 1942');
    const BEFORE = (t: number): unknown[] => [t, [true, 1], [true, 1], t === 17 ? ['No ship reaches Gibraltar: Italian fleets hold the Western Mediterranean and the waters off the Iberian coast. The garrison lives on the port\'s stores through March 1942; unless the sea is opened, it then goes hungry and is lost at the start of June 1942.'] : [], [STORES_C, REMEDY_G('at least through March 1942')], LIVING_G('April 1942'), LIVING_G('March 1942')];
    expect(rows).toEqual([
      BEFORE(17), BEFORE(18), BEFORE(19),
      // April: Britain's card is read with the crew living, and Canada's draw after the loss feeds its corps none.
      [20, [true, 1], [false, 0.6], [], [STORES_C, REMEDY_G('at least through March 1942')], LIVING_G('April 1942'), LIVING_G('March 1942')],
      // May: the stores have fed the Canadian corps no longer since April. The news of it, which April's reading could
      // not see, is told in May's, as laid, with the month the rule strikes the first corps off. (The 03:13Z build never
      // told it: its May reading looked only for stores run out in May.)
      [21, [false, 0.6], [false, 0.2], [NEWS_SPENT('June')], [SPENT_G('June'), REMEDY_G('through July')], HUNGRY_G('July'), HUNGRY_G('June')],
      [22, [false, 0.2], null, [], [SPENT_G('June'), REMEDY_G('through August')], HUNGRY_G('July'), HUNGRY_G('June')],
      // Struck off in Canada's month of June, and as July opens in Britain's: one line, at his reckoning.
      [23, null, null, ['Cut off in Gibraltar, two corps have starved and are lost: the 1st Canadian Corps and the 9th Corps.'], [], null, null],
    ]);
  });

  it('05:03Z, item 5: saved and loaded between France\'s draw of April and Britain\'s, and the crew lost after the load: the record names the draw the game was saved after, France\'s, and every line reads as the rule acts', () => {
    const at = gibCrewed('UK');
    let g = at.g, ninth = at.ninth;
    const rows: unknown[] = [];
    let kept: string[] = [];
    for (let t = 17; ; t++) {
      const h0 = g.headlines.length;
      g.turn = t;
      phase(g, 'ITA');
      phase(g, 'FRA');
      if (t === 20) {
        // (France's month opened, the month is handed on to Britain's, as a step of the turn does (turn.ts stepAI), and
        // the game is saved there and loaded. The crew is then struck off, whatever takes it (game.ts removeCorps),
        // before Britain's draw: the last draw that saw it living was France's.)
        g.phase = g.order.indexOf('FRA') + 1;
        expect(g.order[g.phase]).toBe('UK');
        g = loadGame(saveGame(g))!;
        expect(Object.keys(g.flags).filter((k) => k.startsWith('portDraw:')).map((k) => [k, g.flags[k]])).toEqual([['portDraw:FRA', 20]]);
        ninth = g.corps.find((c) => c.id === at.ninth.id)!;
        removeCorps(g, g.corps.find((c) => c.id === at.crew.id)!);
        kept = Object.keys(g.flags).filter((k) => k.startsWith('portCrew:')).map((k) => `${k}=${g.flags[k]}`);
      }
      phase(g, 'UK');
      rows.push([t, ninth.supplied, Math.round(ninth.org * 100) / 100, ninth.dead, ours(g, h0), texts(portCardLines(g, at.gib, 'UK')), ninth.dead ? null : portCorpsLine(g, ninth)]);
      if (ninth.dead) break;
    }
    // The record: France's draw of April, the one the game was saved after. (The 03:13Z build kept the mark of the last
    // draw in memory only, and a game loaded had none: the record named Britain's draw, not yet made, so that April's
    // card and corps line told a garrison going hungry that it "lives on the port's stores through April", and the news
    // of the stores spent came only in May.)
    expect(kept).toEqual([`portCrew:${at.gib}:${at.crew.id}:UK:16:1:FRA=20`]);
    expect(rows).toEqual([
      BEFORE(17), BEFORE(18), BEFORE(19),
      // April: Britain's draw, after the loss, reads the port's own two, four months after the fill: the 9th goes hungry,
      // and the month the stores run out is told in it, as where the crew is lost before his draw unsaved.
      [20, false, 0.6, false, [NEWS_SPENT('June')], [SPENT_G('June'), REMEDY_G('through June')], HUNGRY_G('June')],
      [21, false, 0.2, false, [], [SPENT_G('June'), REMEDY_G('through July')], HUNGRY_G('June')],
      [22, false, -0.2, true, ['Cut off in Gibraltar, the 9th Corps has starved and is lost.'], [], null],
    ]);
  });

  // (Words only: the record of a crew lost is written as the corps is struck off, game.ts removeCorps, inside the war.)
  it('Italy\'s landing in her April, with the switch on and off under the fortress rows: the record is written with it on and never with it off, and the war after it, saved and loaded and played four months by the computer, is the same war, its own words and marks aside', () => {
    const after = (on: boolean): { g: Game; kept: string[] } => {
      const { g, gib, crew } = gibCrewed('UK', false, on);
      for (let t = 17; t <= 20; t++) {
        g.turn = t;
        phase(g, 'ITA');
        if (t === 20) crewBeaten(g, gib, crew, 'ITA');
        phase(g, 'UK');
      }
      const kept = Object.keys(g.flags).filter((k) => k.startsWith('portCrew:'));
      // (Britain's month opened, the month is handed on to the next nation's, as a step of the turn does (turn.ts
      // stepAI); then saved, loaded, and played on by the computer for every side.)
      g.phase = g.order.indexOf('UK') + 1;
      const back = loadGame(saveGame(g))!;
      for (let m = 0; m < 4; m++) runTurnAI(back);
      return { g: back, kept };
    };
    const on = after(true), off = after(false);
    expect([on.kept.length, on.kept[0].endsWith(':UK:16:1:ITA'), off.kept]).toEqual([1, true, []]);
    expect([on.g.turn, off.g.turn]).toEqual([24, 24]);
    // (Its words are news, and the week's paper reads the news: the paper's own flag goes with them, as in the switch's
    // own test of a whole war.)
    const told = on.g.headlines.filter((h) => /^(No ship reaches |Ships reach |Cut off (on|in) .*(has|have) starved and (is|are) lost)|'s stores are spent and no ship has got through/.test(h.text));
    expect(told.slice(0, 2).map((h) => [h.turn, h.text])).toEqual([[17, 'No ship reaches Gibraltar: Italian fleets hold the Western Mediterranean and the waters off the Iberian coast. The garrison lives on the port\'s stores through April 1942; unless the sea is opened, it then goes hungry and is lost at the start of July 1942.'], [20, 'Gibraltar\'s stores are spent and no ship has got through: the garrison goes hungry, and is lost at the start of June unless the sea is opened.']]);
    const strip = (g: Game): string => {
      const h = structuredClone(g) as Game;
      h.headlines = h.headlines.filter((x) => !told.some((t) => t.text === x.text && t.turn === x.turn));
      for (const k of Object.keys(h.flags)) if (/^(portStores$|portTold:|portSpent:|seaShut:|portLost:|portHad:|portCrew:|portDraw:)/.test(k) || k === 'newsweek') delete h.flags[k];
      return saveGame(h);
    };
    expect(strip(on.g)).toBe(strip(off.g));
    expect(saveGame(off.g)).not.toMatch(/portStores|portTold|portSpent|seaShut|portLost|portHad|portCrew|portDraw/);
  }, 600000);
});

describe('03:13Z, item 3: a corps in a cut-off port always has its line', () => {
  it('the Canadian play: the picture drawn again within Britain\'s March, without the crew, and each corps\' line reads the rule\'s clock before and after it', () => {
    const { g, ninth } = crewed('UK', (h) => { addCorps(h, 'CAN', '1st Canadian Corps', 'inf', 2, 'Malta', {}); declareWar(h, 'ITA', 'CAN'); }, ['ITA', 'UK', 'CAN']);
    const canadian = g.corps.find((c) => c.name === '1st Canadian Corps')!;
    for (let t = 17; t <= 18; t++) month(g, t, ['ITA', 'UK', 'CAN']);
    g.turn = 19;
    phase(g, 'ITA');
    phase(g, 'UK');
    const read = (): unknown[] => [ninth.supplied, canadian.supplied, portCorpsLine(g, ninth), portCorpsLine(g, canadian)];
    const LIVING = 'Living on Malta\'s stores through March.';
    const HUNGRY = (lost: string): string => `Hungry: Malta's stores are spent. Lost at the start of ${lost} unless ships get through.`;
    // Britain's March, the crew starved in his reckoning: the picture as his draw left it, with the crew living, still
    // feeds the Canadian corps, whose own draw will not. (The build gave it no line.)
    expect(read()).toEqual([true, true, LIVING, HUNGRY('May')]);
    // A move of his redraws the picture within his month (game.ts ensureSupply), without the crew: neither corps is fed
    // by it now. The 9th was fed this month by its own draw, and lives on the stores through March. (The build gave it no
    // line.)
    g.supplyDirty = true;
    computeSupply(g);
    expect(read()).toEqual([false, false, LIVING, HUNGRY('May')]);
    // And as the rule acts: the Canadian corps hungry in its own month of March, the 9th in April; struck off as May and
    // June open, each in its own nation's month, the months the lines named.
    phase(g, 'CAN');
    const clock: unknown[] = [[19, ninth.org, Math.round(canadian.org * 100) / 100]];
    for (let t = 20; t <= 22; t++) {
      month(g, t, ['ITA', 'UK', 'CAN']);
      clock.push([t, ninth.dead ? null : Math.round(ninth.org * 100) / 100, canadian.dead ? null : Math.round(canadian.org * 100) / 100]);
    }
    expect(clock).toEqual([[19, 1, 0.6], [20, 0.6, 0.2], [21, 0.2, null], [22, null, null]]);
  });
});

describe('05:03Z, item 1: a port in a pocket is left to the pocket\'s own lines, as a besieged port is left to the siege\'s', () => {
  /**
   * Gibraltar with Spain at war beside it, so that the port is cut off by land as well as by sea: a pocket (siege.ts
   * pocketMonth), whose own lines tell the player both roads and ships ("Gibraltar is surrounded: no road and no ship
   * reaches it ...") on the rule's own clock. A British 9th Corps in the town and the 10th to the 13th in the field; the
   * stores filled by a real draw in December 1940 (turn 16); Italy and Germany at war with Britain, France's fleet in
   * harbour, and Italy's 1st Fleet in the Western Mediterranean and her 2nd off the Iberian coast. The switch as asked,
   * and the player's seat (Italy's seeing Gibraltar whole).
   */
  function pocketed(forts: boolean, on = true, human = 'UK'): { g: Game; gib: number; town: Corps; field: Corps[] } {
    const g = grand(human, on, forts);
    if (human === 'ITA') g.flags.nofog = 1;
    const r = g.byName.Gibraltar;
    const town = addCorps(g, 'UK', '9th Corps', 'inf', 2, 'Gibraltar', {});
    r.units.splice(r.units.indexOf(town), 1);
    joinTown(g, r, town);
    const field = ['10th Corps', '11th Corps', '12th Corps', '13th Corps'].map((n) => addCorps(g, 'UK', n, 'inf', 2, 'Gibraltar', {}));
    declareWar(g, 'ESP', 'UK');
    g.turn = 16;
    g.supplyDirty = true;
    computeSupply(g);
    declareWar(g, 'ITA', 'UK');
    declareWar(g, 'GER', 'UK');
    for (const f of g.fleets) if (f.nat === 'FRA') f.stance = 'port';
    sail(g, 'ITA', 'wmed', ['1st Fleet']);
    sail(g, 'ITA', 'iberia', ['2nd Fleet']);
    g.supplyDirty = true;
    expect(g.flags[`stores:${r.id}`]).toBe(16);
    return { g, gib: r.id, town, field };
  }
  const OURS = /^(No ship reaches |Ships reach |Cut off (on|in) |.*'s stores are spent and no ship)/;
  /** The lines of Gibraltar the rules told, other than the telling's own (the newsreel's wireless aside). */
  const SHIPPED = (g: Game, from = 0): string[] => g.headlines.slice(from).map((h) => h.text).filter((t) => /Gibraltar/.test(t) && !OURS.test(t) && !/wireless/.test(t));
  /**
   * Each month from December 1940 until the corps are struck off, Britain's player reading in his own month: whether the
   * pocket's record stands, the corps, the telling's own news, his card, each corps' line, and the pocket's own lines.
   * Spain makes peace with Britain in her month `peace`, where asked.
   */
  function play(forts: boolean, on = true, peace?: number): { g: Game; rows: unknown[][]; shipped: string[] } {
    const { g, gib, town, field } = pocketed(forts, on);
    const rows: unknown[][] = [], shipped: string[] = [];
    for (let t = 16; t <= 40; t++) {
      const h0 = g.headlines.length;
      month(g, t, ['GER', 'ITA', 'UK']);
      const all = [town, ...field];
      shipped.push(...SHIPPED(g, h0));
      rows.push([t, g.flags[`pkt:${gib}`] !== undefined, all.every((c) => c.dead) ? null : [all.every((c) => c.supplied), Math.round(town.org * 100) / 100], g.headlines.slice(h0).map((h) => h.text).filter((x) => OURS.test(x)), texts(portCardLines(g, gib, 'UK')), town.dead ? null : portCorpsLine(g, town), field[0].dead ? null : portCorpsLine(g, field[0])]);
      const h1 = g.headlines.length;
      phase(g, 'ESP');
      if (t === peace) expect(signPeace(g, 'ESP', 'UK')).toBe(true);
      shipped.push(...SHIPPED(g, h1));
      if (all.every((c) => c.dead)) break;
    }
    return { g, rows, shipped };
  }
  /** The war with the switch on and off, the telling's own words and marks aside (as the switch's own test of a whole war). */
  const wordsOnly = (g: Game): string => {
    const h = structuredClone(g) as Game;
    h.headlines = h.headlines.filter((x) => !OURS.test(x.text));
    for (const k of Object.keys(h.flags)) if (/^(portStores$|portTold:|portSpent:|seaShut:|portLost:|portHad:|portCrew:|portDraw:)/.test(k) || k === 'newsweek') delete h.flags[k];
    return saveGame(h);
  };
  const SURROUNDED = (months: string): string => `Gibraltar is surrounded: no road and no ship reaches it from the rest of the United Kingdom. 6 British corps are cut off there, with stores for ${months}; after that the troops go hungry.`;
  const POCKET_SPENT = 'The stores of Gibraltar are spent: 5 British corps, 150,000 men, are out of supply and cannot hold out long.';

  it('the fortress rows off: the ring closes at Britain\'s December 1940, and while it stands no port line, for him or for Italy; the pocket\'s own lines as shipped, its stores spent in February, the corps struck off as April opens', () => {
    const { g, rows, shipped } = play(false);
    // (The pocket's record stands every month; the corps fed through January, hungry from February.)
    expect(rows.map((x) => x.slice(0, 3))).toEqual([[16, true, [true, 1]], [17, true, [true, 1]], [18, true, [false, 0.6]], [19, true, [false, 0.2]], [20, true, null]]);
    // No news, card or corps line of the port's. (The 03:13Z build told "No ship reaches Gibraltar: ..." in January, and
    // its mends of the review, 9092b90e1 and b44a5d25b, read the pocket's stores on the port's lines: "Cut off by sea,
    // and the port's stores are spent: ..." from February.)
    expect(rows.map((x) => x.slice(3))).toEqual(rows.map(() => [[], [], null, null]));
    expect(shipped).toEqual([SURROUNDED('2 months'), POCKET_SPENT, 'Cut off and out of supply, the British army at Gibraltar lays down its arms: 6 corps, 101,332 men.']);
    // The pocket's own lines unchanged, and the same war with the switch off.
    const off = play(false, false);
    expect(off.shipped).toEqual(shipped);
    expect(off.rows.map((x) => x.slice(0, 3))).toEqual(rows.map((x) => x.slice(0, 3)));
    expect(wordsOnly(g)).toBe(wordsOnly(off.g));
  });

  it('under the fortress rows: the crew\'s 16 months for the port, the pocket\'s seven, spent in July 1941; no port line while the ring stands, and the corps struck off as September opens', () => {
    const { g, rows, shipped } = play(true);
    expect(rows.map((x) => x.slice(0, 3))).toEqual([16, 17, 18, 19, 20, 21, 22, 23, 24, 25].map((t) => [t, true, t < 23 ? [true, 1] : t === 23 ? [false, 0.6] : t === 24 ? [false, 0.2] : null]));
    // (The 03:13Z build told "No ship reaches Gibraltar: ... through April 1942; ... lost at the start of July 1942." in
    // January, and its mends read "by the start of July 1942" until July.)
    expect(rows.map((x) => x.slice(3))).toEqual(rows.map(() => [[], [], null, null]));
    expect(shipped).toEqual([SURROUNDED('7 months'), POCKET_SPENT, 'Cut off and out of supply, the British army at Gibraltar lays down its arms: 5 corps, 92,120 men.']);
    const off = play(true, false);
    expect(off.shipped).toEqual(shipped);
    expect(wordsOnly(g)).toBe(wordsOnly(off.g));
  });

  it('the ring broken by Spain\'s peace in her January: the record goes at Britain\'s February, and the port\'s lines come back that month on the rule\'s own clock, the sea still shut', () => {
    const { rows, shipped } = play(false, true, 17);
    const STORES = 'Cut off by sea: Italian fleets hold the Western Mediterranean and the waters off the Iberian coast, and no ship reaches the harbour. The garrison lives on the port\'s stores through February; unless the sea is opened, it then goes hungry and is lost at the start of May.';
    const SPENT = 'Cut off by sea, and the port\'s stores are spent: the garrison goes hungry, losing order every month, and is lost at the start of May unless the sea is opened.';
    const REMEDY = (through: string): string => `A fleet of yours at sea there, strong enough to open the Western Mediterranean or the waters off the Iberian coast, lets the ships in and fills the stores: one such month feeds the garrison through ${through}.`;
    const HUNGRY = 'Hungry: Gibraltar\'s stores are spent. Lost at the start of May unless ships get through.';
    expect(rows).toEqual([
      [16, true, [true, 1], [], [], null, null],
      [17, true, [true, 1], [], [], null, null],
      // February: the ring broken as Britain's month opens. The window feeds the garrison through February on the
      // December fill, and every line counts it.
      [18, false, [true, 1], [], [STORES, REMEDY('April')], 'Living on Gibraltar\'s stores through February.', 'Living on Gibraltar\'s stores through February.'],
      // March: the stores are spent, and it is told in the month they are.
      [19, false, [false, 0.6], ['Gibraltar\'s stores are spent and no ship has got through: the garrison goes hungry, and is lost at the start of May unless the sea is opened.'], [SPENT, REMEDY('May')], HUNGRY, HUNGRY],
      [20, false, [false, 0.2], [], [SPENT, REMEDY('June')], HUNGRY, HUNGRY],
      // Struck off as May opens, the month every line named. (The rule's own line of the surrender stands, and the
      // struck-off line gives way to it.)
      [21, false, null, [], [], null, null],
    ]);
    expect(shipped).toEqual([SURROUNDED('2 months'), 'The ring round Gibraltar is broken: the roads are open to the British troops there again.', 'The ring closes at Gibraltar: 5 British corps, 150,000 men, are cut off from all supply.', 'Cut off and out of supply, the British army at Gibraltar lays down its arms: 6 corps, 101,332 men.']);
  });

  it('the shutting side too: Italy\'s player, seeing Gibraltar whole, has no line while the ring stands; with Spain\'s peace in her January, it comes back at Italy\'s March, the month after the record goes, from the hold the switch saw begin', () => {
    const read = (peace?: number): unknown[] => {
      const { g, gib, town } = pocketed(false, true, 'ITA');
      const out: unknown[] = [];
      for (let t = 16; t <= 21 && !town.dead; t++) {
        g.turn = t;
        phase(g, 'GER');
        phase(g, 'ITA');
        // (Italy's player reads his card in his own month, before Britain's.)
        out.push([t, g.flags[`pkt:${gib}`] !== undefined, texts(portCardLines(g, gib, 'ITA'))]);
        phase(g, 'UK');
        phase(g, 'ESP');
        if (t === peace) signPeace(g, 'ESP', 'UK');
        out.push([t, town.supplied, town.dead]);
      }
      return out;
    };
    const line = (hungry: string, lost: string, since = 'December 1940'): string[] => [`Your fleets have held the Western Mediterranean and the waters off the Iberian coast since ${since}. Counting from then, the garrison of Gibraltar is hungry by ${hungry} and lost by the start of ${lost}, unless ships get through.`];
    // (December, before Britain's month closes the ring: the hold the switch saw begin, and the count from it.)
    const DEC = [16, false, line('March 1941', 'May 1941', 'December')];
    expect(read()).toEqual([DEC, [16, true, false], [17, true, []], [17, true, false], [18, true, []], [18, false, false], [19, true, []], [19, false, false], [20, true, []], [20, false, true]]);
    expect(read(17)).toEqual([DEC, [16, true, false], [17, true, []], [17, true, false], [18, true, []], [18, true, false], [19, false, line('March', 'May')], [19, false, false], [20, false, line('March', 'May')], [20, false, false], [21, false, line('March', 'May')], [21, false, true]]);
  });

  it('12:24Z, item 4: a Canadian corps starved in Canada\'s May 1941, before the ring, and kept for Britain\'s reckoning, is told in it, though the ring Spain closes stands as Britain\'s June opens: a loss on the port\'s clock that the pocket\'s lines never tell', () => {
    // (The fortress rows off: Gibraltar held by the book's garrison and a British 9th Corps in the town, with a 1st
    // Canadian Corps; the sea shut from December 1940. Spain comes into the war in her May, after Britain's month and
    // before Canada's, and the ring closes as Britain's June opens.)
    const play = (on: boolean): { g: Game; rows: unknown[]; can: number } => {
      const g = grand('UK', on);
      const r = g.byName.Gibraltar;
      const ninth = addCorps(g, 'UK', '9th Corps', 'inf', 2, 'Gibraltar', {});
      r.units.splice(r.units.indexOf(ninth), 1);
      joinTown(g, r, ninth);
      const canadian = addCorps(g, 'CAN', '1st Canadian Corps', 'inf', 2, 'Gibraltar', {});
      g.turn = 16;
      g.supplyDirty = true;
      computeSupply(g);
      for (const nat of ['UK', 'CAN']) { declareWar(g, 'ITA', nat); declareWar(g, 'GER', nat); }
      for (const f of g.fleets) if (f.nat === 'FRA') f.stance = 'port';
      sail(g, 'ITA', 'wmed', ['1st Fleet']);
      sail(g, 'ITA', 'iberia', ['2nd Fleet']);
      g.supplyDirty = true;
      expect(g.flags[`stores:${r.id}`]).toBe(16);
      const rows: unknown[] = [];
      for (let t = 16; t <= 22; t++) {
        const h0 = g.headlines.length;
        g.turn = t;
        let ring = false;
        for (const nat of ['GER', 'ITA', 'UK', 'ESP', 'CAN']) {
          if (nat === 'ESP' && t === 21) { g.nations.ESP.align = 'axis'; declareWar(g, 'ESP', 'UK'); declareWar(g, 'ESP', 'CAN'); }
          phase(g, nat);
          if (nat === 'UK') ring = g.flags[`pkt:${r.id}`] !== undefined;
        }
        // (The month: whether the ring stood at Britain's reckoning, the telling's own news, the rules' own lines of
        // Gibraltar, the corps struck off, and the corps kept for his next reckoning.)
        rows.push([t, ring, g.headlines.slice(h0).map((h) => h.text).filter((x) => OURS.test(x)), SHIPPED(g, h0), [ninth.dead, canadian.dead], Object.keys(g.flags).filter((k) => k.startsWith('portLost:'))]);
      }
      return { g, rows, can: canadian.id };
    };
    const { g, rows, can } = play(true);
    expect(rows).toEqual([
      [16, false, [], [], [false, false], []],
      [17, false, ['No ship reaches Gibraltar: Italian fleets hold the Western Mediterranean and the waters off the Iberian coast. The garrison lives on the port\'s stores through February; unless the sea is opened, it then goes hungry and is lost at the start of May.'], [], [false, false], []],
      [18, false, [], [], [false, false], []],
      [19, false, ['Gibraltar\'s stores are spent and no ship has got through: the garrison goes hungry, and is lost at the start of May unless the sea is opened.'], [], [false, false], []],
      [20, false, [], [], [false, false], []],
      // May: the British corps struck off in Britain's reckoning, before Spain's war, and told in it; the Canadian corps
      // in Canada's, after it, kept for Britain's next.
      [21, false, ['Cut off in Gibraltar, two corps have starved and are lost: the Gibraltar garrison and the 9th Corps.'], [], [true, true], [`portLost:${g.byName.Gibraltar.id}:${can}:CAN:1st Canadian Corps`]],
      // June: the ring closes as Britain's month opens, and its record stands at his reckoning. The corps kept was lost on
      // the port's clock before the ring, and the pocket's lines never tell it: it is told, true (the UI director's
      // 12:24Z, item 4), and its mark goes.
      [22, true, ['Cut off in Gibraltar, the 1st Canadian Corps has starved and is lost.'], ['Gibraltar is surrounded: no road and no ship reaches it from the rest of the United Kingdom. The town has stores for 4 months, and any troops who fall back on it will live on them.'], [true, true], []],
    ]);
    // (The same war with the switch off, its own words and marks aside.)
    expect(wordsOnly(g)).toBe(wordsOnly(play(false).g));
  });
});

describe('the shutting side with true reach off, as the stage ships it: a count only where the rule\'s clock runs', () => {
  /**
   * Malta held by a British 9th Corps, its stores filled by a real draw in December 1940, and Italy's fleets across the
   * Central Mediterranean, as in malta(), but with true reach off: Italy's player reads. Canada is at war with Germany
   * alone, so that Italy's fleets are no enemy of hers and her own ships' lanes still reach Malta's sea; with true reach
   * off, a quay of hers fills the stores for all (game.ts computeSupply). Or Canada with no ships.
   */
  function reachOff(canadaShips: boolean): unknown[] {
    const g = grand('ITA');
    delete g.flags.trueReach;
    const [ninth] = garrison(g, 'Malta', ['9th Corps']);
    expect([trueReach(g), g.nations.CAN.atWar, allied(g, 'CAN', 'UK')]).toEqual([false, ['GER'], true]);
    if (!canadaShips) { g.nations.CAN.seaSupply = false; g.nations.CAN.sealift = 0; }
    g.turn = 16;
    g.supplyDirty = true;
    computeSupply(g);
    declareWar(g, 'ITA', 'UK');
    sail(g, 'ITA', 'central');
    g.supplyDirty = true;
    const rows: unknown[] = [];
    for (let t = 16; t <= 21; t++) {
      g.turn = t;
      phase(g, 'ITA');
      // (Italy's player reads his card in his own month.)
      const card = texts(portCardLines(g, MALTA, 'ITA'));
      phase(g, 'UK');
      phase(g, 'CAN');
      rows.push([t, card, g.flags[`stores:${MALTA}`], ninth.dead ? null : ninth.supplied]);
    }
    return rows;
  }

  it('Canada\'s ships make a quay at Malta every month: the stores are filled at every draw, the rule\'s clock never starts, and Italy\'s card has no count', () => {
    // (The build said "Your fleets have held the Central Mediterranean since December 1940. Counting from then, the
    // garrison of Malta is hungry by March and lost by the start of May, unless ships get through." from December to
    // May, of a garrison fed every month.)
    expect(reachOff(true)).toEqual([16, 17, 18, 19, 20, 21].map((t) => [t, [], t, true]));
  });

  it('with no ships of Canada\'s, the stores are not filled, and the count is as laid: hungry from March, struck off as May opens', () => {
    const line = (hungry: string, lost: string, since = 'December 1940'): string[] => [`Your fleets have held the Central Mediterranean since ${since}. Counting from then, the garrison of Malta is hungry by ${hungry} and lost by the start of ${lost}, unless ships get through.`];
    expect(reachOff(false)).toEqual([
      [16, line('March 1941', 'May 1941', 'December'), 16, true],
      [17, line('March', 'May'), 16, true],
      [18, line('March', 'May'), 16, true],
      [19, line('March', 'May'), 16, false],
      [20, line('March', 'May'), 16, false],
      [21, line('March', 'May'), 16, null],
    ]);
  });
});
