// Ruling 233, rider b612 (4): the book's German rows for Italy. At the first two stops of the history dial a computer
// Germany's staff makes history's moves, and the moves into Italy in 1943 and 1944 are history's own: the roster's
// dated rows for Italian home ground (XIV and LXXVI Panzer Corps to Calabria, the 16th Panzer Division to Naples, the
// parachute and panzergrenadier divisions to Rome, the mountain corps to the Abruzzo, and those that follow). Free play
// sends none of them, because Italy is a scripted theatre; the roster's 'book' list sends them at stops 1 and 2 of the
// open grand campaign. By the record the moves answered the landing: each row waits until an enemy holds or stands on
// one of the grounds Italy's armistice names (the ward's trigger), and then comes at its date, or at once if its date
// has passed. Nothing is counted twice (b609): XIV Panzer Corps is made of the Hermann Goring Division and the 15th
// Panzer Division where they still live. The looser stops, the player's own Germany and the history campaigns are
// what they were. On 48 wars a stop a German corps then stood on Italian ground at the signing in 33 wars (5 and 3
// without the rows), and nothing before the signing moved.
import { describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { leavingThreat } from '../src/campaign/diplomacy';
import { atWar, capture, createGame, declareWar, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const JUL_43 = 47, AUG_43 = 48, SEP_43 = 49, OCT_43 = 50;
const XIV = 'XIV Panzer Corps (Hermann Goring and 15th Panzergrenadier Divisions)';
const LXXVI = 'LXXVI Panzer Corps (26th Panzer, 29th Panzergrenadier and 1st Parachute Divisions)';
const LATER = '5th Mountain and 65th Divisions';
const MADE_OF = ['Hermann Goring Division', '15th Panzer Division'];
const ITALY = ['Sicily', 'Calabria', 'Apulia', 'Naples', 'Cassino', 'Anzio', 'Abruzzo', 'Rome', 'Marche', 'Tuscany', 'Emilia', 'Liguria', 'Sardinia'];

/** A grand campaign at a stop of the dial, the war of 1943 on: Germany and Italy against Britain and America. */
function grand(stop: number, human: string | null = null): Game {
  const g = startOpen(createGame(3, human));
  setStop(g, stop, true);
  return at1943(g);
}
function at1943(g: Game): Game {
  for (const [a, b] of [['GER', 'UK'], ['ITA', 'UK'], ['GER', 'USA'], ['ITA', 'USA']]) if (!atWar(g, a, b)) declareWar(g, a, b);
  return g;
}
/** Germany's month opens in the month named, in a war nobody fights. */
const monthOpens = (g: Game, turn: number): void => { g.turn = turn; startPhase(g, 'GER'); };
const germans = (g: Game): Corps[] => g.corps.filter((c) => c.nat === 'GER' && !c.dead && ITALY.includes(g.regions[c.reg].name));
const named = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'GER' && !c.dead && c.name === name);
const place = (g: Game, name: string): string | undefined => { const c = named(g, name); return c ? g.regions[c.reg].name : undefined; };

describe('ruling 233, rider b612 (4): the book\'s German rows for Italy', () => {
  it('the list is the ruling\'s: stops 1 and 2, Germany\'s rows for Italian ground, XIV Panzer Corps made of its two divisions', () => {
    const book = (setup as unknown as { book: { stops: number[]; scripted: string[][]; from: Record<string, string[]> } }).book;
    expect(book.stops).toEqual([1, 2]);
    expect(book.scripted).toEqual([['GER', 'ITA']]);
    expect(book.from).toEqual({ [XIV]: MADE_OF });
  });

  it('a quiet Italy draws no German corps: the rows wait for an enemy on one of the armistice\'s grounds', () => {
    for (const stop of [1, 2]) {
      const g = grand(stop);
      expect(leavingThreat(g, 'ITA'), `stop ${stop}`).toBe(false);
      monthOpens(g, JUL_43);
      monthOpens(g, AUG_43);
      expect(germans(g), `stop ${stop}`).toHaveLength(0);
      expect(g.flags['book:ITA'], `stop ${stop}`).toBeUndefined();
    }
  });

  it('with the British on Sicily the rows come: those whose date has passed at once, the next at its own date', () => {
    for (const stop of [1, 2]) {
      const g = grand(stop);
      monthOpens(g, AUG_43);
      capture(g, g.byName.Sicily, 'UK');
      expect(leavingThreat(g, 'ITA'), `stop ${stop}`).toBe(true);
      monthOpens(g, SEP_43);
      expect(g.flags['book:ITA'], `stop ${stop}`).toBe(SEP_43);
      expect(place(g, XIV), `stop ${stop}`).toBe('Calabria');
      expect(place(g, LXXVI), `stop ${stop}`).toBe('Calabria');
      expect(place(g, '16th Panzer Division'), `stop ${stop}`).toBe('Naples');
      expect(place(g, '2nd Parachute and 3rd Panzergrenadier Divisions'), `stop ${stop}`).toBe('Rome');
      expect(place(g, 'LI Mountain Corps (44th and 305th Divisions)'), `stop ${stop}`).toBe('Abruzzo');
      expect(named(g, LATER), `stop ${stop}`).toBeUndefined();
      monthOpens(g, OCT_43);
      expect(place(g, LATER), `stop ${stop}`).toBe('Abruzzo');
    }
  });

  it('nothing is counted twice: XIV Panzer Corps is made of the two divisions the roster already names', () => {
    const g = grand(1);
    capture(g, g.byName.Sicily, 'UK');
    monthOpens(g, AUG_43);
    const xiv = named(g, XIV)!;
    expect(xiv).toBeDefined();
    expect(xiv.divs).toBe(2);
    for (const name of MADE_OF) expect(named(g, name), name).toBeUndefined();
  });

  it('the looser stops send none of them, as before, and break nothing up', () => {
    for (const stop of [3, 4, 5]) {
      const g = grand(stop);
      capture(g, g.byName.Sicily, 'UK');
      monthOpens(g, AUG_43);
      monthOpens(g, OCT_43);
      expect(germans(g), `stop ${stop}`).toHaveLength(0);
      expect(g.flags['book:ITA'], `stop ${stop}`).toBeUndefined();
      for (const name of MADE_OF) expect(named(g, name), `stop ${stop}, ${name}`).toBeDefined();
    }
  });

  it('never the player\'s Germany: his army is his own business', () => {
    const g = grand(1, 'GER');
    capture(g, g.byName.Sicily, 'UK');
    monthOpens(g, AUG_43);
    monthOpens(g, OCT_43);
    expect(germans(g)).toHaveLength(0);
    expect(g.flags['book:ITA']).toBeUndefined();
  });

  it('the history campaign keeps its own script: the rows come at their dates on quiet ground, and nothing waits', () => {
    const g = createGame(3, null);
    expect(g.flags.open).not.toBe(1);
    // (History brings America into the war by its own event of December 1941: stood in for here.)
    g.nations.USA.alive = true;
    g.nations.USA.dormant = false;
    at1943(g);
    expect(atWar(g, 'GER', 'USA')).toBe(true);
    monthOpens(g, JUL_43);
    expect(place(g, XIV)).toBe('Calabria');
    expect(named(g, LXXVI)).toBeUndefined();
    monthOpens(g, AUG_43);
    expect(place(g, LXXVI)).toBe('Calabria');
    expect(g.flags['book:ITA']).toBeUndefined();
  });
});
