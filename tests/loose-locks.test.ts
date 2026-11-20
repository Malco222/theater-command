// History's locks at sea, loosened at stops 4 and 5 of the history dial (the sandbox playtest's sweep of 19 November
// 2026: playtest/sandbox/lock-sweep.md, findings [5]). At the loose stops what the record dated is earned instead:
// - the fifty old American destroyers come only while America is at peace with Britain and leans her way;
// - the Glorious leaving the Mediterranean and the Eagle coming in are not read (the Eagle still comes for a Glorious
//   that left before the dial was loosened);
// - Force H forms once Britain is at war in the western Mediterranean (or France has fallen), while Gibraltar is held,
//   from the Home Fleet's ships, and free to sail anywhere;
// - the landing ships that fight a force ashore at 0.8 are any nation's own research (Landing craft), not Britain's and
//   America's in June 1943.
// Each is read once in a war whatever the dial does, and only for a month that came in this game: a campaign begun
// later, or a save from before, never brings them again.
// Stop 3 (par) and the stops below it read exactly as before.
import { describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import { NAVY, assaultStrength, dealHolds, earned, looseDial, navalUpkeep } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { techOf } from '../src/campaign/tech';
import type { Fleet, Game } from '../src/campaign/types';

/** A grand campaign on its first day, at a stop of the history dial. */
function wide(stop = 3, human: string | null = null, seed = 1): Game {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) g.flags.hist = stop;
  return g;
}
const uk = (g: Game, name: string): Fleet | undefined => g.fleets.find((f) => f.nat === 'UK' && f.name === name);
const news = (g: Game, turn: number): string[] => g.headlines.filter((h) => h.turn === turn).map((h) => h.text);
const learn = (g: Game, nat: string, id: string): void => { const t = techOf(g, nat)!; if (!t.done.includes(id)) t.done.push(id); };
/** Runs Britain's navy month by month, and says how many destroyers the Western Approaches Fleet had each month. */
function months(g: Game, from: number, to: number, before?: (turn: number) => void): Record<number, number> {
  const out: Record<number, number> = {};
  for (let turn = from; turn <= to; turn++) {
    g.turn = turn;
    before?.(turn);
    navalUpkeep(g, 'UK');
    out[turn] = uk(g, 'Western Approaches Fleet')!.ships.dd;
  }
  return out;
}
/** The months a dozen destroyers came at once (the yards' own few a month aside: navy.json 'building'). */
const gains = (seen: Record<number, number>): number[] => Object.keys(seen).map(Number).filter((t) => seen[t] - (seen[t - 1] ?? seen[t]) >= 12).sort((a, b) => a - b);
/** What the yards' monthly rows add to one of Britain's fleets in a month. */
const built = (fleet: string, turn: number, c: string): number => NAVY.building.filter((b) => b.nat === 'UK' && b.fleet === fleet && b.cls === c && b.from <= turn && turn <= b.to).reduce((s, b) => s + b.perMonth, 0);

describe('the dial\'s loose stops', () => {
  it('are 4 and 5, in the grand campaign only', () => {
    expect([1, 2, 3, 4, 5].map((stop) => looseDial(wide(stop)))).toEqual([false, false, false, true, true]);
    const hist = createGame(1, null);
    hist.flags.hist = 5;
    expect(looseDial(hist)).toBe(false);
  });
});

describe('the American destroyers', () => {
  it('are the three rows of the record, a gift of America\'s', () => {
    const rows = NAVY.arrivals.filter((r) => r[5]?.deal);
    expect(rows.map((r) => [r[0], r[1], r[2], r[3], r[5]])).toEqual([13, 14, 15].map((t) => [t, 'UK', 'Western Approaches Fleet', { dd: 12 }, { deal: 'USA' }]));
  });

  it('at stop 3 come on the record\'s months whatever America does, as before', () => {
    for (const lean of ['leaning_allies', 'leaning_axis']) {
      const g = wide(3);
      g.nations.USA.align = lean as never;
      expect(gains(months(g, 12, 16)), lean).toEqual([13, 14, 15]);
      expect(news(g, 13)).toContain('The first of fifty old American destroyers join the Western Approaches Fleet.');
    }
  });

  it('at stops 4 and 5 come only while America is at peace with Britain and leans her way, one row a month', () => {
    for (const stop of [4, 5]) {
      // Leaning to the Allies from the first: on the record's months.
      const friend = wide(stop);
      friend.nations.USA.align = 'leaning_allies';
      expect(gains(months(friend, 12, 16)), `stop ${stop}`).toEqual([13, 14, 15]);
      // Standing apart until the spring of 1941, then leaning to the Allies: the gift comes then, a row a month, with its news.
      const late = wide(stop);
      late.nations.USA.align = 'independent';
      const seen = months(late, 12, 24, (turn) => { if (turn === 20) late.nations.USA.align = 'leaning_allies'; });
      expect(gains(seen), `stop ${stop}`).toEqual([20, 21, 22]);
      // Each of the three brings a dozen over what the yards add that month.
      for (const t of [20, 21, 22]) expect(seen[t] - seen[t - 1] - built('Western Approaches Fleet', t, 'dd'), `stop ${stop} turn ${t}`).toBe(12);
      expect(news(late, 20)).toContain('The first of fifty old American destroyers join the Western Approaches Fleet.');
      expect(news(late, 21)).toContain('More of the American destroyers join the Western Approaches Fleet.');
      // Leaning to the Axis, or at war with Britain: none at all.
      for (const make of [(g: Game) => { g.nations.USA.align = 'leaning_axis'; }, (g: Game) => { g.nations.USA.align = 'leaning_allies'; declareWar(g, 'USA', 'UK'); }]) {
        const g = wide(stop);
        make(g);
        expect(gains(months(g, 12, 24)), `stop ${stop}`).toEqual([]);
        expect(g.headlines.some((h) => /American destroyers/.test(h.text))).toBe(false);
      }
    }
  });

  it('bind a player\'s Britain at the loose stops too, since the gift is America\'s to give', () => {
    const g = wide(4, 'UK');
    g.nations.USA.align = 'independent';
    expect(gains(months(g, 12, 16))).toEqual([]);
    expect(dealHolds(g, 'USA', 'UK')).toBe(false);
    g.nations.USA.align = 'leaning_allies';
    expect(dealHolds(g, 'USA', 'UK')).toBe(true);
  });

  it('never come twice when the dial is turned after a row was read', () => {
    // Read by date at stop 3, then the dial is loosened: the rows already read are not read again.
    const g = wide(3);
    g.nations.USA.align = 'leaning_allies';
    const seen = months(g, 12, 20, (turn) => { if (turn === 14) g.flags.hist = 4; });
    // (Turned in the month of the second row, before it is read: it and the third come at stop 4, in their months.)
    expect(gains(seen)).toEqual([13, 14, 15]);
    expect(gains(seen).length).toBe(3);
    // Held back at stop 4, then tightened to stop 3: the rows still owed come at once, a row a month, as stop 3 gives
    // what the record gave; none twice.
    const back = wide(4);
    back.nations.USA.align = 'independent';
    const later = months(back, 12, 22, (turn) => {
      if (turn === 16) back.nations.USA.align = 'leaning_allies';
      if (turn === 17) back.flags.hist = 3;
    });
    expect(gains(later)).toEqual([16, 17, 18]);
    // And held back at stop 4 with America standing apart, then tightened: owed, they come at stop 3 whatever she does.
    const apart = wide(4);
    apart.nations.USA.align = 'independent';
    expect(gains(months(apart, 12, 22, (turn) => { if (turn === 19) apart.flags.hist = 3; }))).toEqual([19, 20, 21]);
  });

  it('never come again in a campaign begun later, or a save from before, whose months went by without them', () => {
    // History's months read them by date; the grand campaign opened after, at stop 4, owes none.
    const g = createGame(1, null);
    g.nations.USA.align = 'leaning_allies';
    expect(gains(months(g, 12, 16))).toEqual([13, 14, 15]);
    g.flags.open = 1;
    g.flags.hist = 4;
    expect(looseDial(g)).toBe(true);
    expect(gains(months(g, 17, 22))).toEqual([]);
    // A save at stop 4 from a build before the gift was dealt: its rows were read by date and left no marks.
    const old = wide(4);
    old.nations.USA.align = 'leaning_allies';
    expect(gains(months(old, 20, 26))).toEqual([]);
  });
});

describe('Force H', () => {
  it('at stop 3 forms in July 1940 with its own row\'s ships and its tie to the western Mediterranean, at peace or not', () => {
    const g = wide(3);
    g.turn = 11;
    navalUpkeep(g, 'UK');
    const f = uk(g, 'Force H')!;
    expect([f.ships, f.tied, g.seas[f.home].key]).toEqual([{ bb: 2, cv: 1, ca: 2, dd: 9, ss: 0 }, true, 'wmed']);
    expect(news(g, 11)).toContain('Force H forms at Gibraltar.');
  });

  it('at stops 4 and 5 waits for the war to reach the western Mediterranean, and forms from the Home Fleet\'s ships, untied', () => {
    for (const stop of [4, 5]) {
      const g = wide(stop);
      for (let turn = 2; turn <= 13; turn++) {
        g.turn = turn;
        navalUpkeep(g, 'UK');
      }
      // Britain at war with Germany alone: no Force H, though July 1940 is past.
      expect(uk(g, 'Force H'), `stop ${stop}`).toBeUndefined();
      expect(earned(g, NAVY.fleets.find((r) => r.name === 'Force H')!)).toBe(false);
      const home = uk(g, 'Home Fleet')!;
      const before = { ...home.ships };
      declareWar(g, 'ITA', 'UK');
      g.turn = 14;
      navalUpkeep(g, 'UK');
      const f = uk(g, 'Force H')!;
      // The row's ships, as far as half of what the Home Fleet has of each kind allows: the same ships, not new ones.
      const want = NAVY.fleets.find((r) => r.name === 'Force H')!.ships;
      for (const c of ['bb', 'cv', 'ca', 'dd', 'ss'] as const) {
        expect(f.ships[c], `${stop} ${c}`).toBe(Math.min(want[c] ?? 0, Math.floor(before[c] * 0.5)));
        expect(home.ships[c] + f.ships[c], `${stop} ${c}`).toBe(before[c] + built('Home Fleet', 14, c));
      }
      expect(f.ships.bb + f.ships.cv).toBeGreaterThan(0);
      expect([g.regions[f.base].name, g.seas[f.home].key, f.tied]).toEqual(['Gibraltar', 'wmed', false]);
      expect(news(g, 14)).toContain('Force H forms at Gibraltar, from ships of the Home Fleet.');
      // Once: sunk to the last ship and gone, it does not form again.
      g.fleets = g.fleets.filter((x) => x !== f);
      g.turn = 15;
      navalUpkeep(g, 'UK');
      expect(uk(g, 'Force H')).toBeUndefined();
    }
  });

  it('at the loose stops can be earned before July 1940, if the war comes to the western Mediterranean sooner', () => {
    const g = wide(5);
    declareWar(g, 'ITA', 'UK');
    g.turn = 6;
    navalUpkeep(g, 'UK');
    expect(uk(g, 'Force H')).toBeDefined();
    expect(news(g, 6)).toContain('Force H forms at Gibraltar, from ships of the Home Fleet.');
  });

  it('formed by its date and then gone, never forms again when the dial is loosened', () => {
    const g = wide(3);
    g.turn = 11;
    navalUpkeep(g, 'UK');
    g.fleets = g.fleets.filter((x) => !(x.nat === 'UK' && x.name === 'Force H')); // merged into another fleet, or sunk
    g.flags.hist = 4;
    declareWar(g, 'ITA', 'UK');
    for (let turn = 12; turn <= 16; turn++) { g.turn = turn; navalUpkeep(g, 'UK'); }
    expect(uk(g, 'Force H')).toBeUndefined();
    // Nor in a save from a build before, at stop 4 long after July 1940, which left no mark: its month is long past.
    const old = wide(4);
    declareWar(old, 'ITA', 'UK');
    for (let turn = 20; turn <= 24; turn++) { old.turn = turn; navalUpkeep(old, 'UK'); }
    expect(uk(old, 'Force H')).toBeUndefined();
  });

  it('unearned at a loose stop when July 1940 passes, forms at once when the dial is tightened, as stop 3 forms it', () => {
    const g = wide(4);
    for (let turn = 2; turn <= 13; turn++) { g.turn = turn; navalUpkeep(g, 'UK'); }
    expect(uk(g, 'Force H')).toBeUndefined();
    g.flags.hist = 3;
    g.turn = 14;
    navalUpkeep(g, 'UK');
    const f = uk(g, 'Force H')!;
    expect([f.ships, f.tied, g.seas[f.home].key]).toEqual([{ bb: 2, cv: 1, ca: 2, dd: 9, ss: 0 }, true, 'wmed']);
    expect(news(g, 14)).toContain('Force H forms at Gibraltar.');
    g.fleets = g.fleets.filter((x) => x !== f);
    g.turn = 15;
    navalUpkeep(g, 'UK');
    expect(uk(g, 'Force H')).toBeUndefined();
  });

  it('at the loose stops forms once France has fallen, even with Italy at peace, but never while Gibraltar is lost', () => {
    const g = wide(4);
    g.nations.FRA.fellOn = { turn: 10, week: 0, to: 'GER', how: 'beaten' };
    g.turn = 11;
    navalUpkeep(g, 'UK');
    expect(uk(g, 'Force H')).toBeDefined();
    const lost = wide(5);
    lost.nations.FRA.fellOn = { turn: 10, week: 0, to: 'GER', how: 'beaten' };
    declareWar(lost, 'ITA', 'UK');
    lost.byName.Gibraltar.ctrl = 'ESP';
    lost.turn = 11;
    navalUpkeep(lost, 'UK');
    expect(uk(lost, 'Force H')).toBeUndefined();
  });

  it('under a player\'s Britain at the loose stops is earned the same way, with the row\'s own ships: the dial never moves his', () => {
    const g = wide(4, 'UK');
    g.turn = 11;
    navalUpkeep(g, 'UK');
    expect(uk(g, 'Force H')).toBeUndefined();
    const home = { ...uk(g, 'Home Fleet')!.ships };
    declareWar(g, 'ITA', 'UK');
    g.turn = 12;
    navalUpkeep(g, 'UK');
    expect(uk(g, 'Force H')!.ships).toEqual({ bb: 2, cv: 1, ca: 2, dd: 9, ss: 0 });
    expect(uk(g, 'Home Fleet')!.ships).toEqual({ ...home, dd: home.dd + built('Home Fleet', 12, 'dd') });
    expect(news(g, 12)).toContain('Force H forms at Gibraltar.');
  });
});

describe('landing ships', () => {
  it('at stop 3 and in a history campaign come to Britain and America in June 1943, whatever anyone has researched', () => {
    for (const g of [wide(3), createGame(1, null)]) {
      if (techOf(g, 'GER')) { learn(g, 'GER', 'landingcraft'); learn(g, 'GER', 'amphibious'); }
      g.turn = 45;
      expect([assaultStrength(g, 'UK'), assaultStrength(g, 'GER')]).toEqual([0.6, 0.6]);
      g.turn = 46;
      expect([assaultStrength(g, 'UK'), assaultStrength(g, 'USA'), assaultStrength(g, 'GER')]).toEqual([0.8, 0.8, 0.6]);
    }
  });

  it('at stops 4 and 5 are any nation\'s own research, in whatever month it gets there', () => {
    for (const stop of [4, 5]) {
      const g = wide(stop);
      g.turn = 46;
      expect(assaultStrength(g, 'UK'), `stop ${stop}`).toBe(0.6);
      // Landing craft researched: the purpose-built ships that the record gave Britain and America in June 1943.
      g.turn = 8;
      learn(g, 'GER', 'landingcraft');
      expect(assaultStrength(g, 'GER'), `stop ${stop}`).toBe(0.8);
      expect(NAVY.landing.byTech).toEqual({ tech: 'landingcraft', to: 0.8 });
    }
  });
});
