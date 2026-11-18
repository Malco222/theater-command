// Version 80, display only: a fleet action weighed up before it is fought, and two tags on the news of fighting.
import { describe, expect, it } from 'vitest';
import { capture, createGame, headline, routineNews } from '../src/campaign/game';
import { fleetKnown } from '../src/campaign/carriers';
import { setFog } from '../src/campaign/intel';
import { CLASSES, NAVY, fleetPower, sail, seaBattles, shipCount, surfaceShips, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { planSeaBattle } from '../src/campaign/seaplan';
import { startOpen } from '../src/campaign/staff';
import { newsSide, runTurnAI, seaForecast } from '../src/campaign/turn';
import type { Fleet, Game, Headline } from '../src/campaign/types';

const fleet = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;

/** The Home Fleet and the German Battle Fleet both on patrol in the Norwegian Sea, with nobody at the table. */
function meet(seed: number): { g: Game; home: Fleet; battle: Fleet; z: number } {
  const g = createGame(seed, null);
  const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
  const z = zoneOf(g, 'norw');
  sail(g, home, z, 'patrol');
  sail(g, battle, z, 'patrol');
  return { g, home, battle, z };
}

/** A grand campaign with Britain at the table, and the seas sorted by how well Britain would know a German fleet there. */
function fogged(): { g: Game; home: Fleet; battle: Fleet; seas: Record<string, number[]> } {
  const g = startOpen(createGame(7, 'UK'));
  const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
  const seas: Record<string, number[]> = { exact: [], report: [], none: [] };
  for (const z of g.seas) {
    battle.zone = z.id; battle.stance = 'patrol';
    seas[fleetKnown(g, battle, 'UK')].push(z.id);
  }
  return { g, home, battle, seas };
}

describe('a fleet action weighed up before it is fought', () => {
  it('changes nothing: no dice, no ship, no flag, no news, and the save is the same', () => {
    const { g, home, battle, z } = meet(5);
    const before = saveGame(g), dice = g.rng.state;
    for (const id of [home.id, battle.id, fleet(g, 'U-boat Fleet').id]) for (const viewer of [null, 'UK', 'GER', 'FRA']) planSeaBattle(g, id, z, viewer);
    expect(saveGame(g)).toBe(before);
    expect(g.rng.state).toBe(dice);
    // And the fight it forecasts is the same fight whether it was forecast or not.
    const other = meet(5);
    expect(JSON.stringify(seaBattles(g, 'UK'))).toBe(JSON.stringify(seaBattles(other.g, 'UK')));
  });

  it('gives the odds the fight itself works out, and the fight ends inside its band', () => {
    let won = 0, ours = 0, theirs = 0, first = null as ReturnType<typeof planSeaBattle>;
    const N = 40;
    for (let seed = 1; seed <= N; seed++) {
      const { g, home, z } = meet(seed);
      const plan = planSeaBattle(g, home.id, z, null)!;
      first ??= plan;
      expect(plan.why).toBeNull();
      expect(plan.offers).toBe(true);
      const [r] = seaBattles(g, 'UK');
      expect(r, `seed ${seed}`).toBeDefined();
      expect(plan.ours.total).toBeCloseTo(r.aPower, 9);
      expect(plan.theirs.total).toBeCloseTo(r.dPower, 9);
      expect(plan.odds!.mid).toBeCloseTo(r.aPower / r.dPower, 9);
      expect(r.odds).toBeGreaterThanOrEqual(plan.odds!.low - 1e-9);
      expect(r.odds).toBeLessThanOrEqual(plan.odds!.high + 1e-9);
      // Every class on each side goes down within the forecast's fewest and most.
      for (const c of CLASSES) {
        expect(r.att[0].sunk[c], `seed ${seed} ours ${c}`).toBeGreaterThanOrEqual(plan.range.ours[0][c]);
        expect(r.att[0].sunk[c], `seed ${seed} ours ${c}`).toBeLessThanOrEqual(plan.range.ours[1][c]);
        expect(r.def[0].sunk[c], `seed ${seed} theirs ${c}`).toBeGreaterThanOrEqual(plan.range.theirs[0][c]);
        expect(r.def[0].sunk[c], `seed ${seed} theirs ${c}`).toBeLessThanOrEqual(plan.range.theirs[1][c]);
      }
      if (r.won) won++;
      ours += shipCount(r.att[0].sunk); theirs += shipCount(r.def[0].sunk);
    }
    // The band is the dice's fifth either way, and the likely losses are what forty fights lose on average.
    expect(first!.odds!.low).toBeCloseTo(first!.odds!.mid * 0.8, 9);
    expect(first!.odds!.high).toBeCloseTo(first!.odds!.mid * 1.2, 9);
    expect(won / N).toBeCloseTo(first!.win, 1);
    expect(first!.verdict).toBe('Very likely to win');
    expect(Math.abs(ours / N - shipCount(first!.likely.ours))).toBeLessThan(1);
    expect(Math.abs(theirs / N - shipCount(first!.likely.theirs))).toBeLessThan(1.5);
    // The heavier loser loses more, class for class.
    expect(shipCount(first!.likely.theirs)).toBeGreaterThan(shipCount(first!.likely.ours));
    expect(first!.lines.map((l) => l.label)).toEqual(['Forces', 'Odds', 'Search']);
  });

  it('tells the weaker admiral that he will not offer battle, and what it costs him if the enemy comes', () => {
    const { g, battle, home, z } = meet(5);
    const plan = planSeaBattle(g, battle.id, z, null)!;
    expect(plan.offers).toBe(false);
    expect(plan.seekAt).toBe(NAVY.battle.seek);
    expect(plan.odds!.mid).toBeLessThan(NAVY.battle.seek);
    expect(plan.win).toBe(0);
    expect(plan.verdict).toBe('Very likely to lose');
    expect(plan.notes[0]).toMatch(/will not offer battle/);
    // It is the same fight seen from the other side: what he is likely to lose is what the stronger fleet is likely to sink.
    const strong = planSeaBattle(g, home.id, z, null)!;
    expect(plan.likely.ours).toEqual(strong.likely.theirs);
    expect(plan.likely.theirs).toEqual(strong.likely.ours);
    // And the fight bears it out: Germany does not attack, Britain does and wins.
    expect(seaBattles(g, 'GER')).toEqual([]);
    expect(seaBattles(g, 'UK')[0].won).toBe(true);
  });

  it('says who may decline battle when one side is far the stronger', () => {
    const { g, home, battle, z } = meet(5);
    expect(planSeaBattle(g, home.id, z, null)!.declines).toBeNull();
    battle.ships = { bb: 0, cv: 0, ca: 2, dd: 6, ss: 0 };
    const strong = planSeaBattle(g, home.id, z, null)!;
    expect(strong.odds!.mid).toBeGreaterThan(NAVY.battle.evadeAt);
    expect(strong.declines).toEqual({ who: 'theirs', chance: NAVY.battle.evade });
    expect(strong.notes.some((s) => /declines battle about 40% of the time and makes for port, which leaves you the sea/.test(s))).toBe(true);
    const weak = planSeaBattle(g, battle.id, z, null)!;
    expect(weak.declines).toEqual({ who: 'ours', chance: NAVY.battle.evade });
  });

  it('counts every fleet of its side on patrol there, and leaves out one that is not', () => {
    const { g, home, z } = meet(5);
    const alone = planSeaBattle(g, home.id, z, null)!;
    const french = g.fleets.find((f) => f.nat === 'FRA' && surfaceShips(f) > 0)!;
    french.zone = z; french.stance = 'patrol';
    const both = planSeaBattle(g, home.id, z, null)!;
    expect(both.ours.fleets.map((f) => f.name)).toEqual([home.name, french.name]);
    expect(both.ours.ships).toBeCloseTo(alone.ours.ships + fleetPower(g, french), 9);
    // The fight counts them the same way.
    expect(seaBattles(g, 'UK')[0].aPower).toBeCloseTo(both.ours.total, 9);
    french.stance = 'raid';
    expect(planSeaBattle(g, home.id, z, null)!.ours.fleets.length).toBe(1);
    // A fleet is weighed for the sea it is asked about, wherever it lies today.
    const away = meet(5);
    sail(away.g, away.home, away.home.home, 'port');
    expect(planSeaBattle(away.g, away.home.id, away.z, null)!.ours.fleets[0].id).toBe(away.home.id);
  });

  it('has nothing to forecast for submarines, for a stranger, or for a sea with no enemy known', () => {
    const { g, home, battle, z } = meet(5);
    const subs = planSeaBattle(g, fleet(g, 'U-boat Fleet').id, z, null)!;
    expect(subs.why).toMatch(/Submarines fight no fleet action/);
    expect(subs.odds).toBeNull();
    // The enemy is not shown our plan; an ally is.
    expect(planSeaBattle(g, home.id, z, 'GER')).toBeNull();
    expect(planSeaBattle(g, home.id, z, 'FRA')).not.toBeNull();
    expect(planSeaBattle(g, 9999, z, null)).toBeNull();
    expect(planSeaBattle(g, home.id, 9999, null)).toBeNull();
    sail(g, battle, battle.home, 'port');
    const empty = planSeaBattle(g, home.id, z, null)!;
    expect(empty.why).toBe('No enemy fleet is known to be at sea in the Norwegian Sea.');
    expect(empty.theirs.fleets).toEqual([]);
  });

  it('counts only what the viewer has found: identified, reported, or not there at all', () => {
    const { g, home, battle, seas } = fogged();
    expect(seas.exact.length).toBeGreaterThan(0);
    expect(seas.report.length).toBeGreaterThan(0);
    expect(seas.none.length).toBeGreaterThan(0);
    const was = battle.zone, before = saveGame(g);

    // Identified: its ships by class and its weight, but not its readiness or its admiral.
    battle.zone = seas.exact[0];
    const known = planSeaBattle(g, home.id, battle.zone, 'UK')!;
    const row = known.theirs.fleets.find((f) => f.id === battle.id)!;
    expect(row).toMatchObject({ known: 'exact', name: 'Battle Fleet', counted: true, ready: null, admiral: null });
    expect(row.ships).toEqual(battle.ships);
    expect(row.power).toBeCloseTo(fleetPower(g, battle), 9);
    expect(known.odds).not.toBeNull();
    expect(known.ours.fleets[0]).toMatchObject({ id: home.id, ready: home.ready, admiral: home.admiral!.name });
    expect(known.notes).toContain('What your side has not found is not in these figures.');

    // Reported: a flag and a word for its size, and nothing in the figures.
    battle.zone = seas.report[0];
    const heard = planSeaBattle(g, home.id, battle.zone, 'UK')!;
    expect(heard.theirs.fleets).toEqual([{ id: battle.id, nat: 'GER', name: 'German ships reported', known: 'report', ships: null, count: -1, size: 'A fleet', power: null, ready: null, admiral: null, counted: false }]);
    expect(heard.reported).toBe(1);
    expect(heard.theirs.ships).toBe(0);
    expect(heard.odds).toBeNull();
    expect(heard.why).toMatch(/^No enemy fleet is known to be at sea in the .*\. One force is reported there and not identified: its strength is not known\.$/);

    // Not found: the forecast does not so much as hint at it.
    battle.zone = seas.none[0];
    const blind = planSeaBattle(g, home.id, battle.zone, 'UK')!;
    expect(blind.theirs.fleets).toEqual([]);
    expect(blind.reported).toBe(0);
    expect(JSON.stringify(blind)).not.toMatch(/Battle Fleet|German/);
    expect(blind.search).toMatch(/^Not searched/);
    // With the fog lifted the same sea is in full view.
    setFog(g, false);
    expect(planSeaBattle(g, home.id, battle.zone, 'UK')!.theirs.fleets[0]).toMatchObject({ known: 'exact', counted: true });
    // The looking itself left no mark.
    setFog(g, true);
    battle.zone = was;
    expect(saveGame(g)).toBe(before);
  });

  it('is offered to an interface for the fleets of the player and his allies only', () => {
    const { g, home, battle, seas } = fogged();
    battle.zone = seas.exact[0];
    expect(seaForecast(g, home.id, battle.zone)).toEqual(planSeaBattle(g, home.id, battle.zone, 'UK'));
    expect(seaForecast(g, battle.id, battle.zone)).toBeNull();
    expect(seaForecast(createGame(5, null), 1, 0)).toBeNull();
  });
});

describe('two tags on the news of fighting', () => {
  const news = (g: Game, re: RegExp): Headline => g.headlines.find((h) => re.test(h.text))!;

  it('a fleet action says it was fought at sea, who won and who lost', () => {
    const { g } = meet(5);
    seaBattles(g, 'UK');
    const h = news(g, /Home Fleet defeats the Battle Fleet/);
    expect(h).toMatchObject({ kind: 'battle', nat: 'UK', arm: 'sea', won: 'UK', lost: 'GER' });
    expect(newsSide(g, h, 'UK')).toBe('won');
    expect(newsSide(g, h, 'FRA')).toBe('won');
    expect(newsSide(g, h, 'GER')).toBe('lost');
    expect(newsSide(g, h, 'SWE')).toBeNull();
    expect(newsSide(g, h, null)).toBeNull();
  });

  it('a fleet that declines battle has lost the sea without a fight', () => {
    let seen = false;
    for (let seed = 1; seed <= 30 && !seen; seed++) {
      const { g, battle } = meet(seed);
      battle.ships = { bb: 0, cv: 0, ca: 2, dd: 6, ss: 0 };
      seaBattles(g, 'UK');
      const h = news(g, /declines battle/);
      if (!h) continue;
      seen = true;
      expect(h).toMatchObject({ arm: 'sea', won: 'UK', lost: 'GER' });
    }
    expect(seen).toBe(true);
  });

  it('a capital taken is news of the land war, won by one side and lost by the other', () => {
    const g = createGame(5, null);
    const warsaw = g.regions[g.nations.POL.capital];
    capture(g, warsaw, 'GER');
    const h = news(g, /falls to German forces/);
    expect(h).toMatchObject({ kind: 'fall', arm: 'land', won: 'GER', lost: 'POL', region: warsaw.id });
    expect(newsSide(g, h, 'GER')).toBe('won');
    expect(newsSide(g, h, 'POL')).toBe('lost');
    expect(newsSide(g, h, 'UK')).toBe('lost');
  });

  it('a line that names one side only still reads as good or bad news to its enemy', () => {
    const g = createGame(5, null);
    headline(g, 'battle', 'Bombers sink 2 destroyers of the Battle Fleet in the North Sea.', 'GER', undefined, { arm: 'air', lost: 'GER' });
    const h = g.headlines[g.headlines.length - 1];
    expect(newsSide(g, h, 'GER')).toBe('lost');
    expect(newsSide(g, h, 'UK')).toBe('won');
    expect(newsSide(g, h, 'SWE')).toBeNull();
    headline(g, 'battle', 'German troops seize the bridges.', 'GER', 3, { arm: 'land', won: 'GER' });
    expect(newsSide(g, g.headlines[g.headlines.length - 1], 'UK')).toBe('lost');
    // A fight between two others is nobody's won or lost.
    headline(g, 'battle', 'A fight far away.', 'ITA', undefined, { arm: 'land', won: 'ITA', lost: 'GRE' });
    expect(newsSide(g, g.headlines[g.headlines.length - 1], 'UK')).toBeNull();
  });

  it('news that is not of fighting carries neither tag, and a line reads as it always did', () => {
    const g = createGame(5, null);
    headline(g, 'info', 'A quiet month.', 'UK');
    const h = g.headlines[g.headlines.length - 1];
    expect(Object.keys(JSON.parse(JSON.stringify(h))).sort()).toEqual(['kind', 'nat', 'text', 'turn', 'week']);
    expect(newsSide(g, h, 'UK')).toBeNull();
    // News kept for the month's summary is tagged like any other when it is told in full.
    routineNews(g, 'test', 'battle', 'The fleet shells the coast.', 'the coast shelled', 'UK', 4, { arm: 'sea' });
    expect(g.headlines[g.headlines.length - 1]).toMatchObject({ arm: 'sea', region: 4 });
  });

  it('a month of a war tags every battle line with its arm, and the tags survive a save', () => {
    const g = createGame(5, null);
    const { g: sea } = meet(5);
    seaBattles(sea, 'UK');
    const back = loadGame(saveGame(sea));
    expect(back!.headlines.find((h) => /defeats the Battle Fleet/.test(h.text))).toMatchObject({ arm: 'sea', won: 'UK', lost: 'GER' });
    // September 1939 to the fall of France: battles on land, raids from the air, sinkings at sea.
    for (let t = 0; t < 11; t++) runTurnAI(g);
    const fights = g.headlines.filter((h) => h.kind === 'battle');
    expect(fights.length).toBeGreaterThan(5);
    for (const h of fights) expect(h.arm, h.text).toMatch(/^(land|sea|air)$/);
    const arms = new Set(g.headlines.map((h) => h.arm).filter(Boolean));
    expect(arms.has('land')).toBe(true);
    for (const h of g.headlines) {
      if (h.won !== undefined) expect(g.nations[h.won], h.text).toBeDefined();
      if (h.lost !== undefined) expect(g.nations[h.lost], h.text).toBeDefined();
      if (h.won !== undefined && h.lost !== undefined) expect(h.won, h.text).not.toBe(h.lost);
    }
  });
});
