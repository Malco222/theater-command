// Battle stars (the design director's b477, Brian's own pick): every ship that comes through a fleet action its side
// won earns a star, on the dice, settled by the staff or fought out on the field; the loser none, a draw none. The
// record is kept ship by ship (stars.ts), in step with the counts through every loss, split, join and new ship, and
// it is a record only: the campaign's random draws are the same, call for call, with it or without it.
import { describe, expect, it, vi } from 'vitest';
import { Rng } from '../src/core/rng';
import { closeWith, seaAhead } from '../src/battle/sea/handoff';
import { createGame } from '../src/campaign/game';
import { deliver } from '../src/campaign/economy';
import { detach, fleetCard, merge } from '../src/campaign/fleets';
import { sail, seaBattles, shipCount, sinkShips, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { bookSeaAction, mendStars, seaActionName, shipStars, starRow, starSplit } from '../src/campaign/stars';
import { orderArsenal, runTurnAI } from '../src/campaign/turn';
import type { Fleet, Game, ShipClass } from '../src/campaign/types';

const CLASSES: ShipClass[] = ['bb', 'cv', 'ca', 'dd', 'ss'];
const fleet = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;
/** The Home Fleet and the Battle Fleet both on patrol in the Norwegian Sea: the British seek an action there (as in navy.test.ts). */
function meet(seed: number): { g: Game; home: Fleet; battle: Fleet } {
  const g = createGame(seed, null);
  const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
  const norw = zoneOf(g, 'norw');
  expect(sail(g, home, norw, 'patrol')).toBe(true);
  expect(sail(g, battle, norw, 'patrol')).toBe(true);
  return { g, home, battle };
}
const ukClose = (g: Game): void => { seaBattles(g, 'UK'); };
/** Every ship of a fleet, class by class, as the number of stars it has. */
const starsOf = (f: Fleet): Record<string, number[]> => Object.fromEntries(CLASSES.map((c) => [c, shipStars(f, c).map((s) => s.length)]));
const starred = (f: Fleet): number => CLASSES.reduce((n, c) => n + shipStars(f, c).filter((s) => s.length).length, 0);
/** The record as it is kept never runs past the counts, and every star is an action the war has written down. */
function inStep(g: Game): void {
  const n = g.stories.seaActions?.length ?? 0;
  for (const f of g.fleets) {
    for (const c of CLASSES) {
      const list = f.stars?.[c] ?? [];
      expect(list.length, `${f.name} ${c}`).toBeLessThanOrEqual(f.ships[c]);
      if (list.length) expect(list[list.length - 1].length, `${f.name} ${c}: no empty entry is kept at the back`).toBeGreaterThan(0);
      for (const s of list) for (const at of s) expect(at).toBeLessThan(n);
    }
  }
}

describe('how a row of stars is drawn', () => {
  it('up to five bronze; from the sixth, every five is one silver', () => {
    const row = (n: number): [number, number] => { const s = starSplit(n); return [s.silver, s.bronze]; };
    expect(row(0)).toEqual([0, 0]);
    expect(row(1)).toEqual([0, 1]);
    expect(row(3)).toEqual([0, 3]);
    expect(row(5)).toEqual([0, 5]);
    expect(row(6)).toEqual([1, 1]);
    expect(row(7)).toEqual([1, 2]);
    expect(row(10)).toEqual([2, 0]);
    expect(row(11)).toEqual([2, 1]);
  });
});

describe('who earns a star', () => {
  it('a won action settled by the staff: every ship of the winner still afloat, none of those sunk, none of the loser', () => {
    const { g, home, battle } = meet(5);
    const [r] = seaBattles(g, 'UK');
    expect(r.won).toBe(true);
    expect(shipCount(r.att[0].sunk)).toBeGreaterThan(0); // some of the winner's ships went down: they earn nothing
    expect(g.stories.seaActions).toEqual([[g.turn, zoneOf(g, 'norw')]]);
    for (const c of CLASSES) {
      // The winner's survivors, and only they, carry the action; the ships sunk took no entry with them.
      expect(home.ships[c], c).toBe(r.att[0].ships[c] - r.att[0].sunk[c]);
      expect(shipStars(home, c), c).toEqual(Array.from({ length: home.ships[c] }, () => [0]));
      expect(home.stars?.[c]?.length ?? 0).toBe(home.ships[c]);
    }
    expect(starred(home)).toBe(shipCount(home.ships));
    expect(battle.stars).toBeUndefined();
    expect(starred(battle)).toBe(0);
    expect(seaActionName(g, 0)).toEqual({ name: 'The action in the Norwegian Sea', month: 'September 1939', turn: g.turn, zone: zoneOf(g, 'norw') });
    inStep(g);
  });

  it('an action fought out on the field (closeWith) is booked the same way, for whichever side won it', () => {
    // The attacker wins on the field: the Home Fleet's survivors earn it.
    const a = meet(5);
    const [x] = seaAhead(a.g, ukClose);
    expect(x.ticket.title).toBe('The action in the Norwegian Sea');
    const dd = a.home.ships.dd;
    closeWith(a.g, new Map([[x.key, { meeting: x.meeting, fought: { R: 1.05, sunk: { [a.battle.id]: { dd: 3 }, [a.home.id]: { dd: 1 } } } }]]), ukClose);
    expect(a.home.ships.dd).toBe(dd - 1);
    expect(shipStars(a.home, 'dd')).toEqual(Array.from({ length: dd - 1 }, () => [0]));
    expect(starred(a.home)).toBe(shipCount(a.home.ships));
    expect(starred(a.battle)).toBe(0);
    // The stars name the action as the field's ticket does.
    expect(seaActionName(a.g, 0).name).toBe(x.ticket.title);
    // The attacker loses on the field: the defender's survivors earn it, and the beaten Home Fleet nothing.
    const b = meet(5);
    b.battle.ships.ss = 6; // submarines take no part on the field, but they sail with the fleet that won
    const [y] = seaAhead(b.g, ukClose);
    closeWith(b.g, new Map([[y.key, { meeting: y.meeting, fought: { R: 0.9, sunk: { [b.battle.id]: { dd: 1 }, [b.home.id]: { ca: 2, dd: 2 } } } }]]), ukClose);
    expect(b.g.seaReports[0].won).toBe(false);
    expect(starred(b.home)).toBe(0);
    expect(starred(b.battle)).toBe(shipCount(b.battle.ships));
    expect(b.battle.ships.ss).toBeGreaterThan(0); // the Battle Fleet's submarines were in the action, and earn it with the rest
    expect(shipStars(b.battle, 'ss').every((s) => s.length === 1)).toBe(true);
    inStep(a.g); inStep(b.g);
  });

  it('a draw earns nothing, and neither does a battle declined', () => {
    // The rules never decide a draw (R at 1 or more is the attacker's win); an action nobody won is never written down.
    const { g, home } = meet(5);
    expect(bookSeaAction(g, zoneOf(g, 'norw'), null)).toBe(-1);
    expect(bookSeaAction(g, zoneOf(g, 'norw'), [])).toBe(-1);
    expect(g.stories.seaActions).toBeUndefined();
    expect(home.stars).toBeUndefined();
    // A fleet far outmatched slips away to port: no action, no report, no star.
    let declined = 0, fought = 0;
    for (let seed = 1; seed <= 30 && (!declined || !fought); seed++) {
      const m = meet(seed);
      m.battle.ships = { bb: 0, cv: 0, ca: 1, dd: 2, ss: 0 };
      const reps = seaBattles(m.g, 'UK');
      if (reps.length) { fought++; expect(starred(m.home)).toBe(shipCount(m.home.ships)); continue; }
      expect(m.g.headlines.some((h) => /declines battle in the Norwegian Sea/.test(h.text))).toBe(true);
      expect(m.g.stories.seaActions).toBeUndefined();
      expect(m.g.fleets.some((f) => f.stars)).toBe(false);
      declined++;
    }
    expect(declined).toBeGreaterThan(0);
    expect(fought).toBeGreaterThan(0);
  });
});

describe('the record keeps in step with the ships', () => {
  /** A fleet whose five destroyers have, oldest first, 2, 1, 1 and 0 and 0 stars, from two actions in the North Sea in one month. */
  function veterans(g: Game, f: Fleet): void {
    const nsea = zoneOf(g, 'nsea');
    g.stories.seaActions = [[g.turn, nsea], [g.turn, nsea]];
    f.ships.dd = 5;
    f.stars = { dd: [[0, 1], [0], [1]] };
  }

  it('ships lost go from the back of their class, and new ships join there with none', () => {
    const g = createGame(5, null);
    const home = fleet(g, 'Home Fleet');
    veterans(g, home);
    // Some lost, by a path that writes the loss in the record itself: those that go are the last.
    for (let seed = 1; home.ships.dd === 5 && seed < 50; seed++) sinkShips(home, 0.4, new Rng(seed), ['dd']);
    expect(home.ships.dd).toBeLessThan(5);
    expect(starsOf(home).dd).toEqual([2, 1, 1, 0, 0].slice(0, home.ships.dd));
    expect(home.stars?.dd?.length ?? 0).toBeLessThanOrEqual(home.ships.dd);
    inStep(g);
    // A loss written nowhere else (a save from before, a path that only counts) is mended the same way when it is read.
    home.ships.dd = 1;
    expect(starsOf(home).dd).toEqual([2]);
    // New ships are added only after the record is mended: they join at the back with no stars, and inherit none.
    mendStars(home, 'dd');
    home.ships.dd += 4;
    expect(starsOf(home).dd).toEqual([2, 0, 0, 0, 0]);
    inStep(g);
  });

  it('ships built in the yards join with no stars, even after losses the record did not see', () => {
    const g = startOpen(createGame(3, 'GER'));
    g.nations.GER.pp = 500;
    const z = zoneOf(g, 'nsea');
    g.stories.seaActions = [[g.turn, z]];
    const ger = (): Fleet[] => g.fleets.filter((f) => f.nat === 'GER');
    const dd = (): number => ger().reduce((n, f) => n + f.ships.dd, 0);
    // Every German destroyer has a star; then one of each fleet's is lost by a path that only counts.
    for (const f of ger()) if (f.ships.dd > 0) { f.stars = { dd: Array.from({ length: f.ships.dd }, () => [0]) }; f.ships.dd -= 1; }
    const veteransLeft = dd();
    expect(veteransLeft).toBeGreaterThan(0);
    const b = orderArsenal(g, 'ship:dd')!;
    expect(b).not.toBeNull();
    b.eta = g.turn;
    deliver(g, 'GER');
    expect(dd()).toBeGreaterThan(veteransLeft);
    // The starred ships are the veterans left, at the front of each fleet's destroyers; the new ones are at the back with none.
    expect(ger().reduce((n, f) => n + shipStars(f, 'dd').filter((s) => s.length).length, 0)).toBe(veteransLeft);
    for (const f of ger()) {
      const each = starsOf(f).dd, k = each.filter((n) => n > 0).length;
      expect(each, f.name).toEqual([...Array(k).fill(1), ...Array(each.length - k).fill(0)]);
    }
    // (A fleet that got no new ships still keeps its unseen loss in its record, read mended: that is the rule at work.)
  });

  it('a split takes the last ships of each class and their stars; a join brings them in at the back', () => {
    const g = startOpen(createGame(3, 'UK'));
    const home = fleet(g, 'Home Fleet');
    veterans(g, home);
    const n = shipCount(home.ships);
    // The last two destroyers have no stars: the new fleet has none.
    const a = detach(g, home, { dd: 2 })!;
    expect(a.ships.dd).toBe(2);
    expect(a.stars).toBeUndefined();
    expect(starsOf(home).dd).toEqual([2, 1, 1]);
    // Two more: the last two of the three left, with their stars.
    const b = detach(g, home, { dd: 2 })!;
    expect(starsOf(b).dd).toEqual([1, 1]);
    expect(b.stars).toEqual({ dd: [[0], [1]] });
    expect(starsOf(home).dd).toEqual([2]);
    // Moved, not shared: a star added to one is not on the other.
    b.stars!.dd![0].push(1);
    expect(home.stars).toEqual({ dd: [[0, 1]] });
    b.stars!.dd![0].pop();
    inStep(g);
    // Joined back, each comes in at the back of the Home Fleet's destroyers, the empty ones kept in their place.
    expect(merge(g, a, home)).toBe(true);
    expect(starsOf(home).dd).toEqual([2, 0, 0]);
    expect(merge(g, b, home)).toBe(true);
    expect(starsOf(home).dd).toEqual([2, 0, 0, 1, 1]);
    expect(home.stars).toEqual({ dd: [[0, 1], [], [], [0], [1]] });
    expect(shipCount(home.ships)).toBe(n);
    expect(a.stars).toBeUndefined();
    expect(b.stars).toBeUndefined();
    inStep(g);
  });
});

describe('the cards', () => {
  it("carry the most decorated ship's row, its silver and bronze, and the actions with how many ships came through each", () => {
    const g = createGame(5, null);
    const home = fleet(g, 'Home Fleet');
    const nsea = zoneOf(g, 'nsea'), norw = zoneOf(g, 'norw');
    g.turn = 9; // May 1940
    g.stories.seaActions = [[9, nsea], [9, nsea], [10, norw]];
    home.ships.dd = 3;
    home.stars = { dd: [[0, 1], [0]] };
    const card = fleetCard(g, home).cards.find((c) => c.cls === 'dd')!;
    expect(card.stars).toEqual({
      most: 2, silver: 0, bronze: 2, each: [2, 1, 0],
      battles: [
        { name: 'The action in the North Sea', month: 'May 1940', turn: 9, zone: nsea, ships: 2, of: 3, line: 'The action in the North Sea, May 1940 (2 of 3 ships)' },
        { name: 'The second action in the North Sea', month: 'May 1940', turn: 9, zone: nsea, ships: 1, of: 3, line: 'The second action in the North Sea, May 1940 (1 of 3 ships)' },
      ],
      words: [
        'The most decorated of these ships has 2 battle stars, one for each sea battle it came through on the winning side.',
        'The action in the North Sea, May 1940 (2 of 3 ships)',
        'The second action in the North Sea, May 1940 (1 of 3 ships)',
      ],
    });
    // Classes with no star carry no row.
    expect(fleetCard(g, home).cards.find((c) => c.cls === 'bb')!.stars).toBeUndefined();
    // One ship with seven actions: one silver and two bronze; every one of its actions is the whole card's.
    home.ships.bb = 1;
    g.stories.seaActions = Array.from({ length: 7 }, (_, i) => [3 + i, norw] as [number, number]);
    home.stars = { bb: [[0, 1, 2, 3, 4, 5, 6]] };
    const bb = starRow(g, home, 'bb')!;
    expect([bb.most, bb.silver, bb.bronze]).toEqual([7, 1, 2]);
    expect(bb.words[0]).toBe('This ship has 7 battle stars, one for each sea battle it came through on the winning side.');
    expect(bb.words[1]).toBe('A silver star stands for five.');
    expect(bb.battles.map((b) => b.line)).toEqual([
      'The action in the Norwegian Sea, November 1939', 'The action in the Norwegian Sea, December 1939', 'The action in the Norwegian Sea, January 1940',
      'The action in the Norwegian Sea, February 1940', 'The action in the Norwegian Sea, March 1940', 'The action in the Norwegian Sea, April 1940',
      'The action in the Norwegian Sea, May 1940',
    ]);
    home.stars = { bb: [[0]] };
    expect(starRow(g, home, 'bb')!.words[0]).toBe('This ship has one battle star, for a sea battle it came through on the winning side.');
    // Nothing on the card is taken from the stars but the row itself: the fleet's strength is the same without them.
    const power = fleetCard(g, home).power;
    delete home.stars;
    expect(fleetCard(g, home).power).toBe(power);
    expect(fleetCard(g, home).cards.every((c) => !c.stars)).toBe(true);
  });
});

describe('saves', () => {
  it('a war fought for months keeps its stars through a save, and one saved before them loads and plays on', () => {
    const a = createGame(21, null);
    for (let t = 1; t <= 9; t++) { runTurnAI(a); inStep(a); }
    expect(a.seaReports.length).toBeGreaterThan(0);
    expect(a.stories.seaActions!.length).toBe(a.flags.seaBattles);
    expect(a.fleets.some((f) => f.stars)).toBe(true);
    const b = loadGame(saveGame(a))!;
    expect(b.fleets).toEqual(a.fleets);
    expect(b.stories.seaActions).toEqual(a.stories.seaActions);
    // The same war saved before there were stars: no fleet has any, and it plays on as the other does.
    const d = JSON.parse(saveGame(a));
    for (const f of d.fleets) delete f.stars;
    delete d.stories.seaActions;
    const old = loadGame(JSON.stringify(d))!;
    expect(old.fleets.some((f) => f.stars)).toBe(false);
    expect(old.stories.seaActions).toBeUndefined();
    for (let t = 0; t < 3; t++) { runTurnAI(a); runTurnAI(old); inStep(old); }
    const bare = (g: Game): unknown => { const x = JSON.parse(saveGame(g)); for (const f of x.fleets) delete f.stars; delete x.stories.seaActions; return x; };
    expect(bare(old)).toEqual(bare(a));
    // Its stars are the actions won since it was loaded, and none from before.
    const since = old.stories.seaActions ?? [];
    expect(since.length).toBe(a.stories.seaActions!.length - d.flags.seaBattles);
  });
});

describe('a record and nothing more', () => {
  it('the campaign draws the same, call for call, with the stars kept or with them left out', async () => {
    // A history campaign of ten months, and a grand campaign of eighteen (its hunts, torpedoes, convoys, strikes and yards).
    type Mods = [typeof import('../src/campaign/game'), typeof import('../src/campaign/turn'), typeof import('../src/campaign/staff')];
    const load = async (): Promise<Mods> => [await import('../src/campaign/game'), await import('../src/campaign/turn'), await import('../src/campaign/staff')];
    const play = ([G, T, O]: Mods, each: (g: Game) => void = () => {}): Game[] => {
      const history = G.createGame(21, null);
      for (let t = 1; t <= 10; t++) { T.runTurnAI(history); each(history); }
      const grand = O.startOpen(G.createGame(3, null));
      for (let t = 1; t <= 18; t++) { T.runTurnAI(grand); each(grand); }
      return [history, grand];
    };
    const kept = play(await load(), inStep);
    for (const g of kept) expect(g.fleets.some((f) => f.stars)).toBe(true);
    // The same wars with every star call stubbed out: nothing is booked, mended, split or joined.
    vi.resetModules();
    vi.doMock('../src/campaign/stars', async (orig) => ({
      ...(await orig<typeof import('../src/campaign/stars')>()),
      bookSeaAction: () => -1, mendStars: () => {}, splitStars: () => undefined, joinStars: () => {},
    }));
    try {
      const S = await import('../src/campaign/save');
      const left = play(await load());
      const bare = (text: string): unknown => { const x = JSON.parse(text); for (const f of x.fleets) delete f.stars; delete x.stories.seaActions; return x; };
      kept.forEach((k, i) => {
        const l = left[i];
        expect(l.fleets.some((f) => f.stars)).toBe(false);
        expect(l.stories.seaActions).toBeUndefined();
        // The dice, the fleets, the battles, the news: everything but the stars themselves.
        expect(l.rng.state).toBe(k.rng.state);
        expect(JSON.stringify(l.seaReports)).toBe(JSON.stringify(k.seaReports));
        expect(bare(S.saveGame(l))).toEqual(bare(saveGame(k)));
      });
    } finally {
      vi.doUnmock('../src/campaign/stars');
      vi.resetModules();
    }
  }, 180_000);
});
