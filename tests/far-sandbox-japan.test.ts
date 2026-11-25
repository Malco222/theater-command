// Japan's choices at stops 4 and 5 of the dial, with the far side's switch on (the sandbox playtest of 19 November:
// https://claude.ai/artifact/LWj6Ury1vyi1CVzQbaZrjE, and playtest/sandbox/lock-sweep.md's two Pacific rows). Three
// clocks there ran Japan down history's road: the demand on Indochina came by itself, the strike south came by the
// sixth month after the freeze ready or not, and it always declared on history's three. At stops 4 and 5 the demand
// is the staff's choice, the window has no last month, and the strike goes against those that stand in Japan's way.
// Stops 1 to 3 are as they were (far-pacific-south.test.ts and far-ready.test.ts). The states are made up by hand,
// as in those files; one month of Japan's own is played to see a blow fall. No war is played here.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { bookMonth } from '../src/campaign/claims';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { atWar, bloc, capitulate, createGame, declareWar, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { navalUpkeep } from '../src/campaign/navy';
import { PACIFIC, demand, inTheWay, mayDemand, pacificOpening } from '../src/campaign/pacific';
import { readiness, readinessMonth, readyWatch } from '../src/campaign/readiness';
import { asGoes, characterOf, decide, monthly, ops, staffPhase, startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';

const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.name === name && !c.dead);
const at = (g: Game, name: string): string | undefined => { const c = corps(g, name); return c && g.regions[c.reg].name; };
const news = (g: Game, turn: number, part: string): boolean => g.headlines.some((h) => h.turn === turn && h.text.includes(part));
const SOUTH = () => ops('JAP').find((op) => op.id === 'south')!;
const striking = (g: Game) => g.fleets.find((x) => x.nat === 'JAP' && x.name === 'Mobile Fleet')!;
const AMERICAN = ['Guam', 'Wake', 'Aparri', 'Legaspi', 'Lingayen', 'Davao', 'Jolo'];

/** A grand campaign on the whole world at a stop of the dial, France beaten by Germany in June 1940. */
const france = (stop: number, seed = 3): Game => {
  const g = startOpen(createGame(seed, null));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = 10;
  capitulate(g, 'FRA', 'GER');
  return g;
};

/** The month the staff's draw first brings the demand, from `from` on, with Germany and the Soviet Union at war from `east`. */
const firstDraw = (g: Game, from: number, east: number): number => {
  const C = PACIFIC!.demand.choose!, who = characterOf(g, 'JAP');
  for (let t = from; t < 99; t++) if (monthly(g, 'JAP', 'demand', t) < C[who] * (t >= east ? C.busy : 1)) return t;
  return 99;
};

/**
 * far-ready.test.ts's made-up December 1941 at a stop of the dial: the Netherlands and France struck down by hand,
 * the months read from June 1941, the demand on Indochina made in July (at stops 4 and 5 by hand, so the quays are
 * Japan's whatever the draw), Japan's dated rows taken in, America's fleet put at Pearl Harbor.
 */
const december = (stop: number, seed = 17, dutch = true): Game => {
  const g = startOpen(createGame(seed, null));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = 9;
  if (dutch) { declareWar(g, 'GER', 'NED'); capitulate(g, 'NED', 'GER'); }
  g.turn = 10;
  capitulate(g, 'FRA', 'GER');
  for (const t of [22, 23, 24, 25, 27]) {
    g.turn = t;
    if (t === 23 && mayDemand(g)) demand(g);
    bookMonth(g); monthlyDiplomacy(g);
    if (t === 24 || t === 27) startPhase(g, 'JAP');
  }
  for (const op of ops('JAP')) if (op.id !== 'south') g.flags[`st:done:JAP:${op.id}`] = 1;
  const sea = g.seas.findIndex((z) => z.key === 'hawaii');
  for (const f of g.fleets.filter((x) => x.nat === 'USA' && (x.name === 'Pacific Fleet' || x.name === 'Pacific Carrier Fleet'))) {
    f.base = g.byName['Pearl Harbor'].id; f.port = 'Pearl Harbor'; f.home = sea; f.zone = sea; f.station = sea; f.stance = 'port';
  }
  g.turn = 28;
  bookMonth(g);
  monthlyDiplomacy(g);
  navalUpkeep(g, 'USA');
  return g;
};
/** The freeze put so many months back, with the striking fleet gathered as the staff gathers it (where its strike flies one). */
const months = (g: Game, m: number, gathered = true): Game => {
  if (gathered) { g.flags['pac:freeze'] = g.turn - 2; readinessMonth(g, 'JAP'); }
  g.flags['pac:freeze'] = g.turn - m;
  return g;
};
/** Britain's ground in Malaya held so strongly that no staff would land against it: the test of strength fails there. */
const fortress = (g: Game): void => {
  for (const c of g.corps) if (c.nat === 'UK' && !c.dead && ['Kota Bharu', 'Kuantan', 'Ipoh', 'Penang', 'Kuala Lumpur', 'Johore', 'Singapore'].includes(g.regions[c.reg].name)) { c.divs = 9; c.str = 1; }
};
/** Britain and its dominions at peace with Germany and Italy. */
const britainAtPeace = (g: Game): void => {
  for (const id of ['UK', 'AUS', 'NZL', 'CAN', 'SAF', 'IND', 'NEP']) for (const e of ['GER', 'ITA']) if (g.nations[id]) {
    g.nations[id].atWar = g.nations[id].atWar.filter((x) => x !== e);
    g.nations[e].atWar = g.nations[e].atWar.filter((x) => x !== id);
  }
};
/** True where the staff's once-a-war draw leaves America out of its strike (America neutral, only its trade stopped). */
const leavesAmerica = (g: Game): boolean => monthly(g, 'JAP', 'flank', 'USA') >= PACIFIC!.ready!.flank![characterOf(g, 'JAP')];
/** The first war on these seeds whose staff the draw takes the stated way on America. */
const seedFor = (stop: number, out: boolean, first = 17): Game => {
  for (let seed = first; seed < first + 80; seed++) { const g = december(stop, seed); if (leavesAmerica(g) === out) return g; }
  throw new Error('no such seed');
};

afterEach(() => { readyWatch.tell = null; });

describe('the demand on Indochina at stops 4 and 5: the staff chooses', () => {
  it('has its figures: by character, likelier while Germany and the Soviet Union are at war', () => {
    expect(PACIFIC!.demand.choose).toEqual({ bold: 0.06, opportunist: 0.03, cautious: 0.015, busy: 5 });
  });

  it("no clock brings it: it comes in the month the staff's draw is for it, from France's fall, whenever that is", () => {
    const months = new Set<number>();
    for (const stop of [4, 5]) for (let seed = 1; seed <= 12; seed++) {
      const g = france(stop, seed);
      const want = firstDraw(g, 11, 20);
      for (let t = 11; t <= 40 && g.flags['pac:demand'] === undefined; t++) {
        g.turn = t;
        if (t === 20) declareWar(g, 'GER', 'SOV');
        pacificOpening(g);
      }
      expect(g.flags['pac:demand'] ?? 99, `stop ${stop} seed ${seed}`).toBe(want);
      if (want < 99) { expect(g.byName['Saigon'].ctrl).toBe('JAP'); months.add(want); }
    }
    // (Not history's month in every war: the demand comes before Germany turns east in some, later in others.)
    expect(months.size).toBeGreaterThan(4);
    expect([...months].some((t) => t < 20)).toBe(true);
    expect([...months].some((t) => t > 21)).toBe(true);
  });

  it('never once Germany is beaten, and never by a player\'s Japan', () => {
    const g = france(5);
    g.nations.GER.alive = false;
    for (let t = 11; t <= 60; t++) { g.turn = t; pacificOpening(g); }
    expect(g.flags['pac:demand']).toBeUndefined();
    const h = startOpen(createGame(3, 'JAP'));
    setStop(h, 5, true);
    h.turn = 10;
    capitulate(h, 'FRA', 'GER');
    for (let t = 11; t <= 60; t++) { h.turn = t; pacificOpening(h); }
    expect(h.flags['pac:demand']).toBeUndefined();
    expect(h.byName['Saigon'].ctrl).toBe('VIC');
  });

  it('stop 3 keeps its clock: the month after Germany and the Soviet Union go to war', () => {
    const g = france(3, 1);
    for (let t = 11; t <= 21; t++) { g.turn = t; if (t === 20) declareWar(g, 'GER', 'SOV'); pacificOpening(g); }
    expect(g.flags['pac:demand']).toBe(21);
  });
});

describe('the strike south at stops 4 and 5: no last month, and from it no waiting for the odds', () => {
  it('with its army not on its quays it waits, past the sixth month and on: nothing forces the blow', () => {
    for (const stop of [4, 5]) {
      const g = months(seedFor(stop, false), 6);
      const told: string[] = [];
      readyWatch.tell = (e) => { told.push(e.why); };
      for (const l of SOUTH().landings!) { const c = corps(g, l.corps); if (c) c.dead = true; }
      for (const m of [6, 7, 9, 14]) { months(g, m, false); readinessMonth(g, 'JAP'); }
      expect(g.flags['pac:strike'], `stop ${stop}`).toBeUndefined();
      expect(told, `stop ${stop}`).toEqual(['not ready', 'not ready', 'not ready', 'not ready']);
    }
  });

  it("before the window's last month the odds are asked; from it, the fleet fit and the army on its quays are enough, and the staff strikes on the draw", () => {
    const seen = new Set<string>();
    for (const stop of [4, 5]) for (let seed = 17; seed < 60 && seen.size < 4; seed++) {
      const g = months(december(stop, seed), 5);
      fortress(g);
      const before = readiness(g, 'JAP')!;
      expect(before.strength.Singapore, `stop ${stop} seed ${seed}`).toBe(false);
      expect(before.ready).toBe(false);
      months(g, 6, false);
      const now = readiness(g, 'JAP')!;
      if (!now.fleet || now.forces < now.asked) continue;
      expect(now.strength.Singapore).toBe(false);
      expect(now.ready, `stop ${stop} seed ${seed}`).toBe(true);
      const told: { why: string; struck: boolean }[] = [];
      readyWatch.tell = (e) => { told.push({ why: e.why, struck: e.struck }); };
      readinessMonth(g, 'JAP');
      const draw = monthly(g, 'JAP', 'south', 'ready', g.turn) < 0.5;
      expect(told[0], `stop ${stop} seed ${seed}`).toEqual({ why: draw ? 'ready, and the draw for it' : 'ready, and the draw against it', struck: draw });
      expect(g.flags['pac:strike']).toBe(draw ? g.turn : undefined);
      seen.add(`${stop}:${draw}`);
    }
    // A strike and a month held back, at both stops.
    expect([...seen].sort()).toEqual(['4:false', '4:true', '5:false', '5:true']);
  });

  it('stop 3 keeps its last month and its odds: in the sixth month it strikes, ready or not', () => {
    const g = months(december(3), 6);
    striking(g).ready = 0.5;
    readinessMonth(g, 'JAP');
    expect(g.flags['pac:strike']).toBe(g.turn);
    const h = months(december(3), 6);
    fortress(h);
    expect(readiness(h, 'JAP')!.ready).toBe(false);
  });

  it('nobody in its way, no strike: the flag stays down and the choice is not spent', () => {
    let g: Game | null = null;
    for (let seed = 17; seed < 200 && !g; seed++) {
      const h = december(5, seed);
      britainAtPeace(h);
      if (inTheWay(months(h, 4), 'JAP', SOUTH())!.length === 0) g = h;
    }
    expect(g).not.toBeNull();
    const told: string[] = [];
    readyWatch.tell = (e) => { told.push(e.why); };
    for (const m of [4, 6, 9]) { months(g!, m, false); readinessMonth(g!, 'JAP'); }
    expect(told).toEqual(['nobody in its way', 'nobody in its way', 'nobody in its way']);
    expect(g!.flags['pac:strike']).toBeUndefined();
    decide(g!, 'JAP');
    expect(g!.flags['st:done:JAP:south']).toBeUndefined();
  });
});

describe("the strike south at stops 4 and 5: those that stand in Japan's way", () => {
  it('stops 1 and 2 read history\'s choice exactly: the same choice, its three, its strike and its ten landings; stop 3 reads those in its way', () => {
    for (const stop of [1, 2]) {
      const g = months(december(stop), 4);
      expect(inTheWay(g, 'JAP', SOUTH())).toBeNull();
      expect(asGoes(g, 'JAP', SOUTH())).toBe(SOUTH());
    }
    // (Units decide audit item 45, the design director's ruling of 16:30Z, 25 November 2026: at stops 3 to 5 alike.)
    const g = months(december(3), 4);
    expect(inTheWay(g, 'JAP', SOUTH())).toEqual(leavesAmerica(g) ? ['UK'] : ['USA', 'UK']);
  });

  it('Britain, at war with Germany, always; America, neutral with only its trade stopped, by the draw: a cautious staff leaves it out more often than a bold one', () => {
    const R = PACIFIC!.ready!;
    expect(R.flank).toEqual({ bold: 0.85, opportunist: 0.5, cautious: 0.25 });
    const g = months(december(5), 4);
    expect(atWar(g, 'UK', 'GER')).toBe(true);
    expect(atWar(g, 'USA', 'GER')).toBe(false);
    const seen = { in: 0, out: 0 };
    for (let seed = 1; seed <= 40; seed++) {
      g.seed = seed;
      const way = inTheWay(g, 'JAP', SOUTH())!;
      expect(way, `seed ${seed}`).toEqual(leavesAmerica(g) ? ['UK'] : ['USA', 'UK']);
      seen[leavesAmerica(g) ? 'out' : 'in']++;
    }
    expect(seen.in).toBeGreaterThan(0);
    expect(seen.out).toBeGreaterThan(0);
  });

  it("America at war with Germany stands in the way whatever the draw; and with no freeze and Britain at peace, nobody does", () => {
    const g = months(seedFor(5, true), 4);
    expect(inTheWay(g, 'JAP', SOUTH())).toEqual(['UK']);
    g.nations.USA.align = 'allies';
    declareWar(g, 'USA', 'GER');
    expect(inTheWay(g, 'JAP', SOUTH())).toEqual(['USA', 'UK']);
    const h = months(seedFor(5, true), 4);
    britainAtPeace(h);
    expect(h.nations.UK.atWar.some((e) => ['GER', 'ITA'].includes(e))).toBe(false);
    delete h.flags['pac:freeze'];
    expect(inTheWay(h, 'JAP', SOUTH())).toEqual([]);
  });

  it('a Netherlands still neutral is not dragged in beside Britain: the tie holds only for one that stands with it', () => {
    let g: Game | null = null;
    for (let seed = 17; seed < 120 && !g; seed++) { const h = december(5, seed, false); if (leavesAmerica(h)) g = h; }
    months(g!, 4);
    expect(g!.nations.NED.alive).toBe(true);
    expect(bloc(g!.nations.NED)).toBeNull();
    expect(inTheWay(g!, 'JAP', SOUTH())).toEqual(['UK']);
    // Standing with Britain, it is at war with Japan the day Britain is, and so it is in the way.
    g!.nations.NED.align = 'allies';
    declareWar(g!, 'NED', 'GER');
    expect(inTheWay(g!, 'JAP', SOUTH())).toEqual(['UK', 'NED']);
  });

  it('leaving America out, the choice flies no strike at Pearl Harbor, gathers no fleet, lands on no American ground, and asks nothing of Manila', () => {
    const g = months(seedFor(5, true), 4);
    const op = asGoes(g, 'JAP', SOUTH());
    expect(op.declare).toEqual(['UK']);
    expect(op.strikes).toBeUndefined();
    const to = op.landings!.flatMap((L) => L.to);
    expect(to.some((nm) => AMERICAN.includes(nm))).toBe(false);
    expect(to).toContain('Kota Bharu');
    g.flags['pac:freeze'] = g.turn - 2;
    readinessMonth(g, 'JAP');
    expect(g.regions[striking(g).base].name).toBe('Hiroshima');
    months(g, 4);
    const now = readiness(g, 'JAP')!;
    expect(now.fleet).toBe(true);
    expect(now.strength.Manila).toBeNull();
    // Striking it as well, the choice is history's whole.
    const h = months(seedFor(5, false), 4);
    const all = asGoes(h, 'JAP', SOUTH());
    expect(all.declare).toEqual(['USA', 'UK']);
    expect(all.strikes!.map((s) => s.harbour)).toEqual(['Pearl Harbor']);
    expect(all.landings!.length).toBe(SOUTH().landings!.length);
  });

  it("the flag up, the month's orders declare on Britain alone, with no strike and no landing on America's ground", () => {
    const g = months(seedFor(5, true), 4);
    g.flags['pac:strike'] = g.turn;
    decide(g, 'JAP');
    expect(g.flags['st:done:JAP:south']).toBe(g.turn);
    const ph = staffPhase(g, 'JAP')!;
    expect(ph.declare).toEqual(['UK']);
    expect(ph.strikes).toBeUndefined();
    expect(ph.landings!.length).toBeGreaterThan(0);
    expect(ph.landings!.some((L) => L.to.some((nm) => AMERICAN.includes(nm)))).toBe(false);
  });

  it('the flag up, the strike goes on Britain and those that follow it, and Japan leaves America alone', () => {
    const g = months(seedFor(5, true), 4);
    g.flags['pac:strike'] = g.turn;
    g.phase = g.order.indexOf('JAP');
    runTurnAI(g);
    expect(g.flags['st:done:JAP:south']).toBe(28);
    expect(g.flags['war:JAP:UK']).toBe(28);
    expect(atWar(g, 'JAP', 'AUS')).toBe(true);
    expect(g.flags['war:JAP:USA']).toBeUndefined();
    expect(news(g, 28, 'Pearl Harbor')).toBe(false);
    // (America still enters the war by history's row as this December closes, and its tie with Britain brings it into
    // Japan's war the month after: that row is the what-if events' to loosen, not this choice's.)
    expect(g.flags['war:USA:JAP']).toBe(29);
    expect(g.corps.some((c) => c.nat === 'JAP' && !c.dead && AMERICAN.includes(g.regions[c.reg].name))).toBe(false);
    // The landings on Britain's ground go in: Malaya and Borneo.
    const ashore = SOUTH().landings!.filter((L) => !L.to.some((nm) => AMERICAN.includes(nm)) && L.to.includes(at(g, L.corps) ?? '')).length;
    expect(ashore).toBeGreaterThanOrEqual(2);
  });
});
