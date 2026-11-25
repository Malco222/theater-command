// A base move is a voyage (release 2, the far chart; the design director's ruling of 04:33Z, 25 November 2026, 1; the
// 1.5.1 playtest's finding 39: Japan's Mobile Fleet took MOVE BASE TO KONIGSBERG and was there that month). A fleet
// steams its reach (three seas) in a month: a move within it is made that month, as always. A longer one takes a month
// for each reach of seas. Meanwhile the fleet is in passage: at sea on its way, seen and open to attack as any fleet at
// sea, and it strikes, patrols, escorts and holds nothing. It comes in with news, and it can be turned back. Played on
// the whole-world chart as Japan.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { beforeAll, describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import { whyNotMission } from '../src/campaign/air';
import { fleetCard, whyNotBombard, whyNotDetach, whyNotMerge } from '../src/campaign/fleets';
import {
  NAVY, atSea, baseTargets, cameIn, canSailTo, fleetPower, inPassage, laneNear, moveCost, navalAI, navalUpkeep, rebase, sail, seaBattles, seaRoutes, seaSide,
  turnBack, voyageMonths, whyNotEscort,
} from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { fleetOptions, humanUp, orderFleet, orderRebase, stepAI } from '../src/campaign/turn';
import type { Fleet, Game, Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const said = (g: Game, from: number, re: RegExp): boolean => g.headlines.slice(from).some((h) => re.test(h.text));
/** The next month at sea for Japan: its fleets in passage steam on, and those due come in. */
const nextMonth = (g: Game): void => { g.turn++; navalUpkeep(g, 'JAP'); };

let base: string;
beforeAll(() => {
  const g = startOpen(createGame(3, 'JAP'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  base = saveGame(g);
}, 120000);
/** Japan's month of September 1939, with Konigsberg made Japan's for the test: the playtest's far move. */
function japan(): { g: Game; f: Fleet; kure: Region; kw: Region; kon: Region } {
  const g = loadGame(base)!;
  const kon = g.byName['Konigsberg'];
  kon.ctrl = 'JAP';
  const f = fleet(g, 'JAP', 'Combined Fleet');
  expect(f.port).toBe('Kure');
  return { g, f, kure: g.regions[f.base], kw: g.byName['Kwajalein'], kon };
}

describe('a move of base on the whole world', () => {
  it('within a month\'s steaming is made that month, as always', () => {
    const { g, f, kure } = japan();
    const near = baseTargets(g, f).find((r) => voyageMonths(g, f, r) === 0)!;
    expect(near).toBeTruthy();
    expect(moveCost(g, f, near)).toBe(1);
    expect(orderRebase(g, f.id, near.id)).toBe(true);
    expect([f.stance, f.base, f.zone]).toEqual(['port', near.id, near.seas[0]]);
    expect(f.passage).toBeUndefined();
    expect(f.base).not.toBe(kure.id);
  });

  it('beyond it is a voyage: in passage, its new base its own at once, and in with news a month for each reach of seas', () => {
    const { g, f, kure, kw } = japan();
    expect(voyageMonths(g, f, kw)).toBe(1);
    expect(moveCost(g, f, kw)).toBe(2);
    const news = g.headlines.length;
    expect(orderRebase(g, f.id, kw.id)).toBe(true);
    expect(inPassage(f)).toBe(true);
    expect(atSea(f)).toBe(true);
    expect([f.base, f.port, f.home]).toEqual([kw.id, 'Kwajalein', kw.seas[0]]);
    expect(f.passage).toMatchObject({ from: kure.id, fromPort: 'Kure', set: g.turn, due: g.turn + 1 });
    expect(f.passage!.way.length).toBeGreaterThan(NAVY.range);
    expect(f.zone).toBe(f.passage!.way[NAVY.range - 1]);
    expect(said(g, news, /^The Combined Fleet sails for its new base at Kwajalein, \d+ seas off, due in October 1939\.$/)).toBe(true);
    const at = g.headlines.length;
    nextMonth(g);
    expect([f.stance, f.zone, f.base]).toEqual(['port', f.home, kw.id]);
    expect(f.passage).toBeUndefined();
    expect(said(g, at, /^The Combined Fleet comes in to its new base at Kwajalein after a month in passage\.$/)).toBe(true);
  });

  it('a long one steams a reach a month, the sea it is on moving on each month', () => {
    const { g, f, kon } = japan();
    const m = voyageMonths(g, f, kon);
    expect(m).toBeGreaterThanOrEqual(3);
    expect(orderRebase(g, f.id, kon.id)).toBe(true);
    const p = f.passage!;
    expect(p.due).toBe(g.turn + m);
    for (let k = 1; k < m; k++) {
      nextMonth(g);
      expect(inPassage(f)).toBe(true);
      expect(f.zone).toBe(p.way[Math.min((k + 1) * NAVY.range, p.way.length) - 1]);
    }
    nextMonth(g);
    expect([f.stance, f.base, f.zone]).toEqual(['port', kon.id, f.home]);
    expect(g.headlines.some((h) => h.text === `The Combined Fleet comes in to its new base at Konigsberg after ${m} months in passage.`)).toBe(true);
  });

  it('is the same for the computer, and its admirals leave a fleet in passage on its way', () => {
    const g = startOpen(createGame(3, null));
    const f = fleet(g, 'JAP', 'Combined Fleet'), kon = g.byName['Konigsberg'];
    kon.ctrl = 'JAP';
    declareWar(g, 'JAP', 'USA');
    expect(rebase(g, f, kon)).toBe(true);
    expect(inPassage(f)).toBe(true);
    const z = f.zone;
    navalAI(g, 'JAP');
    expect([f.stance, f.zone, f.base]).toEqual(['passage', z, kon.id]);
  });

  it('in from its passage, it lies that month in harbour, as a fleet whose base its staff moved that month does', () => {
    const g = startOpen(createGame(3, null));
    const f = fleet(g, 'JAP', 'Combined Fleet'), kw = g.byName['Kwajalein'];
    declareWar(g, 'JAP', 'USA');
    expect(voyageMonths(g, f, kw)).toBe(1);
    expect(rebase(g, f, kw)).toBe(true);
    nextMonth(g);
    expect(cameIn(g, f)).toBe(true);
    navalAI(g, 'JAP');
    expect([f.stance, f.zone, f.base]).toEqual(['port', f.home, kw.id]);
    nextMonth(g);
    expect(cameIn(g, f)).toBe(false);
  });

  it("the book's dated moves are made in their month, as the book's own sailings are", () => {
    const { g, f, kon } = japan();
    expect(rebase(g, f, kon, true)).toBe(true);
    expect([f.stance, f.base, f.zone]).toEqual(['port', kon.id, f.home]);
    expect(f.passage).toBeUndefined();
  });
});

describe('a fleet in passage', () => {
  it('strikes, patrols, escorts, shells, splits and joins nothing, and its page offers only turning back', () => {
    const { g, f, kon } = japan();
    expect(orderRebase(g, f.id, kon.id)).toBe(true);
    expect(sail(g, f, f.zone, 'patrol')).toBe(false);
    expect(orderFleet(g, f.id, f.zone, 'patrol')).toBe(false);
    expect(orderFleet(g, f.id, f.zone, 'raid')).toBe(false);
    expect(orderRebase(g, f.id, g.byName['Kwajalein'].id)).toBe(false);
    const o = fleetOptions(g, f.id)!;
    expect([o.seas, o.lanes, o.ports, o.split]).toEqual([[], [], [], false]);
    expect(o.held).toMatch(/^The Combined Fleet is in passage to Konigsberg, at sea on the way, due in .*: it strikes, patrols and escorts nothing until it comes in\. It can be turned back to Kure\.$/);
    expect(whyNotDetach(g, f, { dd: 1 })).toMatch(/in passage/);
    const other = fleet(g, 'JAP', 'Mobile Fleet');
    expect(whyNotMerge(g, other, f)).toMatch(/in passage/);
    expect(whyNotEscort(g, f, seaRoutes(g)[0].id)).toMatch(/in passage/);
    const coast = g.seas[f.zone].coast[0];
    if (coast !== undefined) expect(whyNotBombard(g, f, g.regions[coast])).toMatch(/in passage/);
    expect(laneNear(g, f, seaRoutes(g)[0])).toBe(false);
    const card = fleetCard(g, f);
    expect(card.order).toBe('In passage');
    expect(card.doing).toMatch(/^In passage to Konigsberg .*, in by /);
  });

  it("its carriers' aircraft fly nothing on the way; their standing cover over it is not barred", () => {
    const { g, kon } = japan();
    const mf = fleet(g, 'JAP', 'Mobile Fleet');
    const wing = g.airFleets.find((w) => w.ship === mf.id)!;
    expect(wing).toBeTruthy();
    expect(orderRebase(g, mf.id, kon.id)).toBe(true);
    declareWar(g, 'JAP', 'USA');
    expect(whyNotMission(g, wing, 'support', -1)).toBeNull();
    expect(whyNotMission(g, wing, 'harbour', g.byName['Pearl Harbor'].id)).toMatch(/^The Mobile Fleet is in passage/);
  });

  it('holds no sea: its weight is not counted in the sea it crosses', () => {
    const { g, f, kon } = japan();
    expect(orderRebase(g, f.id, kon.id)).toBe(true);
    const crossing = seaSide(g, f.zone, 'JAP', false).ships;
    f.stance = 'patrol';
    expect(seaSide(g, f.zone, 'JAP', false).ships).toBeCloseTo(crossing + fleetPower(g, f), 6);
    f.stance = 'passage';
  });

  it('is found as a fleet not on patrol is: caught at the chance of a raider, and fought or slipping on, never sent into harbour', () => {
    for (const catchRaider of [0, 1]) {
      const { g, f, kon } = japan();
      expect(orderRebase(g, f.id, kon.id)).toBe(true);
      declareWar(g, 'JAP', 'USA');
      const us = fleet(g, 'USA', 'Pacific Fleet');
      us.zone = f.zone; us.stance = 'patrol';
      for (const c of ['bb', 'cv', 'ca', 'dd'] as const) us.ships[c] += 40;
      const was = NAVY.battle.catchRaider;
      NAVY.battle.catchRaider = catchRaider;
      const news = g.headlines.length;
      let reps;
      try { reps = seaBattles(g, 'USA'); } finally { NAVY.battle.catchRaider = was; }
      const met = reps.some((r) => r.def.some((d) => d.fleet === f.name)) || said(g, news, /Combined Fleet declines battle/);
      expect(met).toBe(catchRaider === 1);
      // Beaten or slipping away, it keeps on its way.
      if (f.ships.bb + f.ships.cv + f.ships.ca + f.ships.dd > 0) expect([f.stance, f.base]).toEqual(['passage', kon.id]);
    }
  });

  it('turned back within its reach is in harbour again that month', () => {
    const { g, f, kure, kon } = japan();
    expect(orderRebase(g, f.id, kon.id)).toBe(true);
    const news = g.headlines.length;
    expect(orderFleet(g, f.id, f.home, 'port')).toBe(true);
    expect([f.stance, f.base, f.port, f.zone]).toEqual(['port', kure.id, 'Kure', f.home]);
    expect(f.passage).toBeUndefined();
    expect(said(g, news, /^The Combined Fleet turns back, and is in harbour at Kure again\.$/)).toBe(true);
  });

  it('turned back beyond its reach is in passage back, by the same reckoning, and in with news', () => {
    const { g, f, kure, kon } = japan();
    expect(orderRebase(g, f.id, kon.id)).toBe(true);
    nextMonth(g);
    nextMonth(g);
    expect(inPassage(f)).toBe(true);
    const news = g.headlines.length;
    expect(turnBack(g, f)).toBe(true);
    expect(inPassage(f)).toBe(true);
    expect([f.base, f.port]).toEqual([kure.id, 'Kure']);
    expect(f.passage).toMatchObject({ from: kon.id, fromPort: 'Konigsberg', back: true, set: g.turn });
    expect(said(g, news, /^The Combined Fleet turns back for Kure, \d+ seas off, due in /)).toBe(true);
    for (let k = 0; k < 12 && inPassage(f); k++) nextMonth(g);
    expect([f.stance, f.base]).toEqual(['port', kure.id]);
    expect(g.headlines.some((h) => /^The Combined Fleet is back in harbour at Kure, \d+ months? after it turned back\.$/.test(h.text) || /^The Combined Fleet is back in harbour at Kure, a month after it turned back\.$/.test(h.text))).toBe(true);
  });

  it('whose new base falls on the way makes for the nearest harbour it can, from where it is', () => {
    const { g, f, kon } = japan();
    expect(orderRebase(g, f.id, kon.id)).toBe(true);
    kon.ctrl = 'UK';
    const news = g.headlines.length;
    nextMonth(g);
    expect(f.base).not.toBe(kon.id);
    expect(g.regions[f.base].ctrl).toBe('JAP');
    expect(said(g, news, /^With Konigsberg lost, the Combined Fleet moves its base to /)).toBe(true);
  });

  it('steams from the sea it is on: what it can sail to is read from there', () => {
    const { g, f, kure, kon } = japan();
    expect(orderRebase(g, f.id, kon.id)).toBe(true);
    nextMonth(g);
    expect(canSailTo(g, f, kure)).toBe(true);
    expect(voyageMonths(g, f, kure)).toBeGreaterThan(0);
  });

  it('is saved and loaded with its voyage', () => {
    const { g, f, kon } = japan();
    expect(orderRebase(g, f.id, kon.id)).toBe(true);
    const h = loadGame(saveGame(g))!;
    const again = h.fleets.find((x) => x.id === f.id)!;
    expect(again.stance).toBe('passage');
    expect(again.passage).toEqual(f.passage);
  });
});

describe("a fleet's page lists the lanes within two months' steaming of it", () => {
  it("America's Pacific fleets are offered no Atlantic lane", () => {
    const g = loadGame(base)!;
    const pf = fleet(g, 'USA', 'Pacific Fleet');
    for (const r of seaRoutes(g).filter((x) => /Atlantic|Arctic/.test(x.name))) expect(laneNear(g, pf, r)).toBe(false);
    const home = g.fleets.find((x) => x.nat === 'UK' && x.name === 'Home Fleet')!;
    expect(seaRoutes(g).some((r) => laneNear(g, home, r))).toBe(true);
  });
});

// ---- Through the 3D game's doorway ------------------------------------------------------------------------------
type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface PageOut { top: { label: string; act: string; ok: boolean; tip: string }[] }
interface Panel { lines: string[]; options: { key: string; label: string; detail: string; ok: boolean; why: string }[] }
interface View { fleets: { id: number; stance: string; reach: number[]; bases: number[] }[] }

describe('in passage on its page, its panel and the map', () => {
  it('offers no sea and no base, and TURN BACK turns it back', () => {
    const { g, f, kon } = japan();
    expect(orderRebase(g, f.id, kon.id)).toBe(true);
    TC().load(saveGame(g));
    const v = (TC().view() as View).fleets.find((x) => x.id === f.id)!;
    expect([v.stance, v.reach, v.bases]).toEqual(['passage', [], []]);
    const panel = TC().force('fleet', f.id) as Panel;
    expect(panel.options.some((o) => /:(patrol|raid)$/.test(o.key))).toBe(false);
    const back = panel.options.find((o) => o.key.endsWith(':port'))!;
    expect([back.label, back.ok]).toEqual(['Turn back to Kure', true]);
    expect(panel.lines.some((l) => /is in passage to Konigsberg/.test(l))).toBe(true);
    const button = (TC().page(`fleet:${f.id}`) as PageOut).top.find((b) => b.label === 'TURN BACK')!;
    expect(button.ok).toBe(true);
    expect((TC().act(button.act) as { ok: boolean }).ok).toBe(true);
    const after = TC().force('fleet', f.id) as Panel;
    expect(after.options.some((o) => /:patrol$/.test(o.key))).toBe(true);
  });
});
