// A player's country is never ended by an event (Version 82): the design director's rulings 99 and 100.
//
// The Playtester's find on Italy's seat: an Italian player's campaign was ended by the armistice of 1943 with its army
// intact, in 18 wars of 24, on Sardinia alone, with no warning, and "Your office is secure" did not stop it.
//
// 1. No event takes the player's own country out of the war or deals out its ground: an event that carries a
//    partition never fires on the country the player leads in the grand campaign, at any stop of the dial, whatever
//    the office setting. It falls only by defeat in the field.
// 2. For a player's Italy the Grand Council does not vote either: his government stands or falls by the cabinet's rules.
// 3. The computer's Italy is not changed.
// 4. A player at war on the side of the computer's Italy is warned: which ground, when Rome changes its government,
//    when it signs, and what becomes of Italy's ground.
// 5. "The Gustav Line is broken" and "Rome is free" are news of a fight: never said of ground that passed by the terms.
import { describe, expect, it } from 'vitest';
import { DIPLOMACY, fireEvent, holds, monthlyDiplomacy } from '../src/campaign/diplomacy';
import { addCorps, allied, createGame, setWar } from '../src/campaign/game';
import { setCanFall } from '../src/campaign/government';
import { HISTORY, setStop, stopOf } from '../src/campaign/history';
import { markFor, monthCard } from '../src/campaign/monthcard';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const DEC_42 = 40, JUNE_43 = 46, AUG_43 = 48, SEPT_43 = 49;
const game = (me: string | null, seed = 25): Game => startOpen(createGame(seed, me));
/** Italy at war with Britain, and one of its grounds in British hands. */
const lose = (g: Game, place = 'Sardinia'): Game => { setWar(g, 'ITA', 'UK'); g.byName[place].ctrl = 'UK'; return g; };
const army = (g: Game, nat: string): number => g.corps.filter((c) => c.nat === nat && !c.dead).length;
const ground = (g: Game, nat: string): number => g.regions.filter((r) => r.ctrl === nat).length;
const warns = (g: Game): string[] => g.headlines.filter((h) => h.warn === 'Rome wavers').map((h) => h.text);
const told = (g: Game, re: RegExp): number => g.headlines.filter((h) => re.test(h.text)).length;
const month = (g: Game, turn: number): Game => { g.turn = turn; monthlyDiplomacy(g); return g; };

describe('no event takes the player\'s own country out of the war', () => {
  it('a player\'s Italy with Sardinia lost plays on through September 1943 with its army, at every stop, office secure and not', () => {
    for (const stop of HISTORY.stops.map((s) => s.stop)) for (const secure of [false, true]) {
      const g = lose(game('ITA'));
      setStop(g, stop, true);
      expect(stopOf(g)).toBe(stop);
      if (secure) expect(setCanFall(g, false)).toBe(true);
      const men = army(g, 'ITA'), held = ground(g, 'ITA'), ships = g.fleets.filter((f) => f.nat === 'ITA').length;
      expect(men).toBeGreaterThan(5);
      for (let t = AUG_43; t <= SEPT_43 + 3; t++) month(g, t);
      expect(g.flags['event:mussolini-falls']).toBeUndefined();
      expect(g.flags['event:italian-armistice']).toBeUndefined();
      expect(g.nations.ITA.alive).toBe(true);
      expect(g.nations.ITA.atWar).toContain('UK');
      expect(army(g, 'ITA')).toBeGreaterThanOrEqual(men);
      expect(ground(g, 'ITA')).toBe(held);
      expect(g.fleets.filter((f) => f.nat === 'ITA').length).toBe(ships);
      expect(told(g, /voted against Mussolini|signed an armistice/)).toBe(0);
    }
  });

  it('the Grand Council\'s vote is for the computer\'s Italy only', () => {
    expect(DIPLOMACY.events.find((e) => e.id === 'mussolini-falls')!.if).toContain('computer:ITA,47');
    expect(holds(game('ITA'), 'computer:ITA')).toBe(false);
    expect(holds(game('GER'), 'computer:ITA')).toBe(true);
    expect(holds(game(null), 'computer:ITA')).toBe(true);
    // A history campaign is not the grand campaign: its Italy is history's, whoever leads it.
    expect(holds(createGame(25, 'ITA'), 'computer:ITA')).toBe(true);
  });

  it('the terms themselves never fire on the country the player leads, whoever stands on its soil', () => {
    const g = lose(game('ITA'));
    g.turn = SEPT_43;
    g.flags['event:mussolini-falls'] = AUG_43;
    addCorps(g, 'UK', 'Eighth Army', 'inf', 4, g.byName.Naples.id, {});
    addCorps(g, 'GER', 'Tenth Army', 'inf', 4, g.byName.Rome.id, {});
    const men = army(g, 'ITA'), held = ground(g, 'ITA');
    expect(fireEvent(g, 'italian-armistice')).toBe(false);
    expect(g.nations.ITA.alive).toBe(true);
    expect(army(g, 'ITA')).toBe(men);
    expect(ground(g, 'ITA')).toBe(held);
    // The rule is general: every event that deals out a country is held to it.
    for (const e of DIPLOMACY.events.filter((x) => x.partition)) {
      const nat = e.partition!.nation;
      const h = game(nat);
      if (!h.nations[nat]?.alive) continue;
      const before = ground(h, nat);
      expect(fireEvent(h, e.id)).toBe(false);
      expect(h.nations[nat].alive).toBe(true);
      expect(ground(h, nat)).toBe(before);
    }
    // (Iraq's armistice and the entry into Iran, ruling 228(3), deal out Iraq and Iran.)
    expect(DIPLOMACY.events.filter((x) => x.partition).map((x) => x.partition!.nation).sort()).toEqual(['BUL', 'IRN', 'IRN', 'IRQ', 'ITA', 'ROM', 'VIC']);
  });

  it('the computer\'s Italy is not changed: the vote in August, the armistice a month later', () => {
    for (const me of ['GER', null]) {
      const g = lose(game(me));
      month(g, AUG_43);
      expect(g.flags['event:mussolini-falls']).toBe(AUG_43);
      expect(g.nations.ITA.alive).toBe(true);
      month(g, SEPT_43);
      expect(g.flags['event:italian-armistice']).toBe(SEPT_43);
      expect(g.nations.ITA).toMatchObject({ alive: false, atWar: [] });
      expect(army(g, 'ITA')).toBe(0);
    }
  });
});

describe('a player at war beside the computer\'s Italy is warned', () => {
  const ANY = 'while an enemy holds any of Sicily, Sardinia, Calabria, Apulia, Naples or Rome';
  const GROUND = 'Italy\'s ground then goes to whoever has an army on Italian soil, your own corps included.';

  it('from the month an Ally first holds one of the six grounds, and every month from two months before the date', () => {
    const g = game('GER');
    setWar(g, 'ITA', 'UK');
    expect(allied(g, 'GER', 'ITA')).toBe(true);
    month(g, DEC_42 - 1);
    expect(warns(g)).toEqual([]);
    g.byName.Sardinia.ctrl = 'UK';
    month(g, DEC_42);
    expect(warns(g)).toEqual([`Italy makes peace if the Allies still hold Sardinia in August 1943. If an enemy holds any of Sicily, Sardinia, Calabria, Apulia, Naples or Rome in August 1943 or later, Rome changes its government that month and signs an armistice the month after, if one is still held. ${GROUND}`]);
    const h = g.headlines.find((x) => x.warn === 'Rome wavers')!;
    expect(h).toMatchObject({ nat: 'GER', region: g.byName.Sardinia.id, turn: DEC_42 });
    // Not every month: said once, until something changes or the date is near.
    month(g, DEC_42 + 1);
    expect(warns(g)).toHaveLength(1);
    // A second ground lost is news again.
    g.byName.Sicily.ctrl = 'UK';
    month(g, DEC_42 + 2);
    expect(warns(g)).toHaveLength(2);
    expect(warns(g)[1]).toMatch(/^Italy makes peace if the Allies still hold Sicily and Sardinia in August 1943\./);
    month(g, JUNE_43 - 1);
    expect(warns(g)).toHaveLength(2);
    // Two months before the date, and every month from then on.
    month(g, JUNE_43);
    month(g, JUNE_43 + 1);
    expect(warns(g)).toHaveLength(4);
    // August: Rome has changed its government, and the warning says what is left.
    month(g, AUG_43);
    expect(g.flags['event:mussolini-falls']).toBe(AUG_43);
    expect(warns(g)).toHaveLength(5);
    expect(warns(g)[4]).toBe(`Italy signs an armistice next month unless Sicily and Sardinia are taken back. Rome has changed its government, and it signs ${ANY}. ${GROUND}`);
    // September: Italy is out, and there is nothing left to warn of.
    month(g, SEPT_43);
    expect(g.nations.ITA.alive).toBe(false);
    expect(warns(g)).toHaveLength(5);
  });

  it('a ground won back takes the warning with it, and lost again it is news again', () => {
    const g = lose(game('GER'));
    month(g, DEC_42);
    expect(warns(g)).toHaveLength(1);
    g.byName.Sardinia.ctrl = 'ITA';
    month(g, JUNE_43);
    month(g, AUG_43);
    expect(warns(g)).toHaveLength(1);
    expect(g.flags['event:mussolini-falls']).toBeUndefined();
    expect(g.flags['told:rome']).toBeUndefined();
    g.byName.Sardinia.ctrl = 'UK';
    month(g, AUG_43 + 1);
    expect(warns(g)).toHaveLength(2);
    expect(warns(g)[1]).toMatch(/^Italy signs an armistice next month unless Sardinia is taken back\./);
  });

  it('is said to nobody else: not to Italy\'s own player, not to its enemy, and not where nobody plays', () => {
    for (const me of ['ITA', 'UK', null]) {
      const g = lose(game(me));
      month(g, DEC_42);
      month(g, JUNE_43);
      expect(warns(g)).toEqual([]);
      expect(g.flags['told:rome']).toBeUndefined();
    }
  });

  it('stands in the month card\'s red band', () => {
    const g = game('GER');
    setWar(g, 'ITA', 'UK');
    g.turn = DEC_42;
    const mark = markFor(g, 'GER');
    g.byName.Sardinia.ctrl = 'UK';
    month(g, DEC_42 + 1);
    const band = monthCard(g, mark, 'GER').bands.find((b) => b.title === 'Rome wavers');
    expect(band).toMatchObject({ word: 'WARNING', head: 'Italy makes peace if the Allies still hold Sardinia in August 1943', nats: ['ITA'], region: g.byName.Sardinia.id });
    // (The band flies Italy's flag, the country the news is about, and not the player's own: the art director's rule.)
    expect(g.headlines.find((h) => h.warn === 'Rome wavers')!.about).toBe('ITA');
  });
});

describe('news of a fight is said only where there was one', () => {
  const lines = (g: Game): number => told(g, /The Gustav Line is broken|Rome is free/);

  it('says nothing of the Gustav Line or of Rome when the ground passes by the terms', () => {
    // Britain holds Sardinia and nobody else stands on Italian soil: the terms give Britain the whole country.
    const g = lose(game(null));
    month(g, AUG_43);
    month(g, SEPT_43);
    expect(g.nations.ITA.alive).toBe(false);
    expect(g.byName.Rome.ctrl).toBe('UK');
    expect(g.byName.Cassino.ctrl).toBe('UK');
    for (let t = SEPT_43 + 1; t <= SEPT_43 + 4; t++) month(g, t);
    expect(lines(g)).toBe(0);
    expect(g.flags['event:gustav-line-broken']).toBeUndefined();
    expect(g.flags['event:rome-liberated']).toBeUndefined();
    // Germany takes Rome, and Britain takes it back: that is a fight, and its news.
    g.byName.Rome.ctrl = 'GER';
    month(g, SEPT_43 + 5);
    expect(lines(g)).toBe(0);
    g.byName.Rome.ctrl = 'UK';
    month(g, SEPT_43 + 6);
    expect(told(g, /Rome is free/)).toBe(1);
    expect(told(g, /The Gustav Line is broken/)).toBe(0);
  });

  it('says both where Germany held the line after the armistice and an Allied army took it', () => {
    const g = lose(game(null));
    addCorps(g, 'GER', 'Tenth Army', 'inf', 4, g.byName.Rome.id, {});
    month(g, AUG_43);
    month(g, SEPT_43);
    expect(g.nations.ITA.fellOn).toMatchObject({ to: 'GER', how: 'terms' });
    expect(g.byName.Cassino.ctrl).toBe('GER');
    month(g, SEPT_43 + 1);
    expect(lines(g)).toBe(0);
    // Taken in the very month of the terms or later, it is news the month after.
    g.byName.Cassino.ctrl = 'USA';
    month(g, SEPT_43 + 2);
    expect(told(g, /The Gustav Line is broken/)).toBe(1);
    g.byName.Rome.ctrl = 'USA';
    month(g, SEPT_43 + 3);
    expect(told(g, /Rome is free/)).toBe(1);
  });
});
