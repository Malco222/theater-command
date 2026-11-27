// The rear garrison of a taken port (the design director's rulings of 16:36Z, 16:44Z, 16:46Z and 19:24Z of
// 21 November 2026, on the trace pacific-r2/membership/READ-STOOD-12.md and the historian's
// japan-taken-towns-1942.md and rear-garrisons-us-britain-1941-45.md; staff.ts rearMonth, far-shores.json 'open.rear').
//
// In 12 wars the empty ground Japan's fronts had orders for stood empty with a corps beside it for 688
// province-months, and for 400 of them the bar was the last-guard rule: a landing force alone in the port it had
// taken, Rabaul's in every war. The rule worked; the record's rear was missing. No taken port was left empty: a base
// force or garrison troops took it over within weeks and the landing force went on. So, behind the far switch and at
// stops 3 to 5, a port taken in war gets its taker's rear-garrison row the month after it is taken, only while it is
// held, once for each taking, at the record's figure for the town, paid from the taker's reserve of men, and never by
// a date alone. Each country reads its own table, and a player's country gets the same row at the same cost, with a
// line in his month's report. At the book's stops the record's dated rows hold the ports.
// The states are set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { cityBound, runAI } from '../src/campaign/ai';
import { addCorps, allied, capture, createGame, declareWar, removeCorps } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { drawMen, menLeft } from '../src/campaign/manpower';
import { frontsClear } from '../src/campaign/planfronts';
import { rearMonth, rearWatch, startOpen } from '../src/campaign/staff';
import { beginHuman, current } from '../src/campaign/turn';
import '../src/campaign/far-rows';
import shores from '../data/campaign/far-shores.json';
import setup from '../data/campaign/setup-1939.json';
import type { Corps, Game, Region } from '../src/campaign/types';

const REAR = (shores as any).open.rear as { stops: number[]; by: Record<string, { each: number; name: string; of: string[]; towns: Record<string, [number, string?]> }> };
const r3 = (x: number): number => Math.round(x * 1000) / 1000;

/** Stop 3 in month `turn`, Japan at war with the countries of the south. */
const war = (turn: number, stop = 3): Game => {
  const g = startOpen(createGame(3, null));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = turn;
  g.flags['st:done:JAP:south'] = 28;
  for (const e of ['USA', 'UK', 'NED', 'AUS']) declareWar(g, 'JAP', e);
  return g;
};
/** The port is taken in the month the game stands in: nobody is left on it, and it changes hands as a battle would leave it. */
const take = (g: Game, name: string, nat: string): Region => {
  const r = g.byName[name];
  for (const c of [...r.units]) if (c.nat !== nat) removeCorps(g, c);
  capture(g, r, nat);
  return r;
};
const rows = (g: Game, name: string, nat: string): Corps[] => g.byName[name].units.filter((c) => !c.dead && c.nat === nat);
const fig = (g: Game, name: string, nat: string): [string, number][] => rows(g, name, nat).map((c) => [c.name, r3(c.divs * c.str)]);

type Told = { nat: string; at: string; name: string; worth: number };
const told: Told[] = [];
const listen = (): void => { told.length = 0; rearWatch.tell = (e) => { told.push({ nat: e.nat, at: e.at.name, name: e.corps.name, worth: r3(e.worth) }); }; };
afterEach(() => { rearWatch.tell = null; frontsClear(); });

describe('the rear garrison comes by cause: the month after the port is taken, while it is held', () => {
  it("Rabaul, taken in month 29: nothing that month; in month 30 the 8th Base Force is in the port at 0.1, and it holds and stays", () => {
    const g = war(29);
    take(g, 'Rabaul', 'JAP');
    listen();
    rearMonth(g, 'JAP');
    expect(told).toEqual([]);
    g.turn = 30;
    rearMonth(g, 'JAP');
    expect(told).toEqual([{ nat: 'JAP', at: 'Rabaul', name: '8th Base Force (Rabaul)', worth: 0.1 }]);
    const [c] = rows(g, 'Rabaul', 'JAP');
    expect([c.name, c.kind, c.divs, r3(c.str), !!c.hold, !!c.stay]).toEqual(['8th Base Force (Rabaul)', 'inf', 1, 0.1, true, true]);
  });

  it('once for each taking: later months bring no second row, though the first is dead', () => {
    const g = war(29);
    take(g, 'Rabaul', 'JAP');
    listen();
    for (const t of [30, 31, 32]) { g.turn = t; rearMonth(g, 'JAP'); }
    expect(told.length).toBe(1);
    removeCorps(g, rows(g, 'Rabaul', 'JAP')[0]);
    g.turn = 33;
    rearMonth(g, 'JAP');
    expect(told.length).toBe(1);
  });

  it('lost and taken again, the port gets a new row the month after the second taking', () => {
    const g = war(29);
    take(g, 'Rabaul', 'JAP');
    listen();
    g.turn = 30; rearMonth(g, 'JAP');
    g.turn = 34; take(g, 'Rabaul', 'AUS');
    expect(g.byName.Rabaul.ctrl).toBe('AUS');
    g.turn = 36; take(g, 'Rabaul', 'JAP');
    rearMonth(g, 'JAP');
    expect(told.length).toBe(1);
    g.turn = 37; rearMonth(g, 'JAP');
    expect(told.map((t) => t.name)).toEqual(['8th Base Force (Rabaul)', '8th Base Force (Rabaul)']);
  });

  it('only while it is held: a port lost again before the month after gets nothing', () => {
    const g = war(29);
    take(g, 'Rabaul', 'JAP');
    g.turn = 30;
    take(g, 'Rabaul', 'AUS');
    listen();
    rearMonth(g, 'JAP');
    g.turn = 31; rearMonth(g, 'JAP');
    expect(told).toEqual([]);
  });

  it('never by a date alone: a port Japan has not taken has no row in any month', () => {
    const g = war(29);
    listen();
    for (const t of [30, 31, 36, 40]) { g.turn = t; rearMonth(g, 'JAP'); }
    expect(told).toEqual([]);
  });

  it('none while an enemy stands on the ground; the month he is gone, it comes', () => {
    const g = war(29);
    take(g, 'Rabaul', 'JAP');
    const foe = addCorps(g, 'AUS', 'Lark Force', 'inf', 1, 'Rabaul', { xp: 0.2 });
    listen();
    g.turn = 30; rearMonth(g, 'JAP');
    expect(told).toEqual([]);
    removeCorps(g, foe);
    g.turn = 31; rearMonth(g, 'JAP');
    expect(told.map((t) => t.at)).toEqual(['Rabaul']);
  });
});

describe("its size is the record's figure for the town, and 0.05 for a port the table does not name", () => {
  it("Japan's table: a base force's seat 0.1, a town's own figure, a lesser port 0.05, and Mandalay (a city, not a port) with the ports", () => {
    const g = war(31);
    for (const nm of ['Palembang', 'Makassar', 'Tarakan', 'Mandalay', 'Singapore']) take(g, nm, 'JAP');
    listen();
    g.turn = 32; rearMonth(g, 'JAP');
    const by = Object.fromEntries(told.map((t) => [t.at, [t.name, t.worth]]));
    expect(by).toEqual({
      Palembang: ['25th Army garrison (Palembang, the oil)', 0.125],
      Makassar: ['23rd Special Base Force (Makassar)', 0.07],
      Tarakan: ['Tarakan guard detachment', 0.05],
      Mandalay: ['15th Army rear (Mandalay)', 0.075],
      Singapore: ['10th Special Base Force (Singapore)', 0.1],
    });
    expect(g.byName.Mandalay.port).toBeFalsy();
    expect(cityBound(g, 'JAP', g.byName.Mandalay)).toBe(true);
  });

  it("the table's own shape: stops 3 to 5; every named town is on the chart and is a place the last guard binds; the five dated guards keep their figures", () => {
    expect(REAR.stops).toEqual([3, 4, 5]);
    const g = war(31);
    for (const [nat, T] of Object.entries(REAR.by)) {
      for (const nm of Object.keys(T.towns)) {
        const r = g.byName[nm];
        expect(r, `${nat}: ${nm}`).toBeTruthy();
        expect(r.port || r.vp > 0, `${nat}: ${nm}`).toBeTruthy();
        expect(r.owner === nat || T.of.includes(r.owner), `${nat}: ${nm} is ${r.owner}'s`).toBe(true);
      }
      expect(T.of.includes('CHN')).toBe(false);
      expect(T.of.includes(nat)).toBe(false);
      expect((T as any).says.length, nat).toBe(2);
      for (const line of (T as any).says) expect(line, nat).toContain('takes over the guard at {places}.'.replace('takes', line.startsWith('A ') ? 'takes' : 'take'));
    }
    const J = REAR.by.JAP.towns;
    expect([J.Guam[0], J.Kendari[0], J.Menado[0], J.Biak[0], J.Guadalcanal[0]]).toEqual([0.03, 0.04, 0.02, 0.02, 0.02]);
    expect(Object.keys(REAR.by)).toEqual(['JAP', 'USA', 'UK', 'AUS']);
    expect([REAR.by.JAP.each, REAR.by.USA.each, REAR.by.UK.each, REAR.by.AUS.each]).toEqual([0.05, 0.075, 0.075, 0.1]);
  });
});

describe("its weight is drawn from its owner's reserve of men", () => {
  it('the reserve gives the row its men as it comes', () => {
    const g = war(29);
    take(g, 'Rabaul', 'JAP');
    g.turn = 30;
    const before = menLeft(g, 'JAP');
    rearMonth(g, 'JAP');
    expect(r3(before - menLeft(g, 'JAP'))).toBe(0.1);
  });

  it('while the reserve cannot give it no row comes, and the month it can, the row comes', () => {
    const g = war(29);
    take(g, 'Rabaul', 'JAP');
    drawMen(g, 'JAP', menLeft(g, 'JAP') - 0.04);
    listen();
    g.turn = 30; rearMonth(g, 'JAP');
    expect(told).toEqual([]);
    expect(r3(menLeft(g, 'JAP'))).toBe(0.04);
    g.flags['mp:JAP'] = 1;
    if (menLeft(g, 'JAP') < 0.1) throw new Error('the reserve was not refilled by hand: the test reads the wrong flag');
    g.turn = 31; rearMonth(g, 'JAP');
    expect(told.map((t) => t.at)).toEqual(['Rabaul']);
  });
});

describe('whose ports, and at which stops', () => {
  it("one rule for everybody: a player's Japan gets the same row at the same cost, as his month opens, and his report says so in a line", () => {
    const g = war(29);
    take(g, 'Rabaul', 'JAP');
    g.human = 'JAP';
    g.turn = 30;
    while (current(g) !== 'JAP') g.phase = (g.phase + 1) % g.order.length;
    const before = menLeft(g, 'JAP'), points = g.nations.JAP.pp;
    listen();
    beginHuman(g);
    expect(told).toEqual([{ nat: 'JAP', at: 'Rabaul', name: '8th Base Force (Rabaul)', worth: 0.1 }]);
    const [c] = rows(g, 'Rabaul', 'JAP');
    expect([c.name, r3(c.str), !!c.hold, !!c.stay]).toEqual(['8th Base Force (Rabaul)', 0.1, true, true]);
    expect(g.headlines.filter((h) => h.turn === 30 && h.text.includes('the guard at')).map((h) => [h.kind, h.text])).toEqual([['info', 'A base force takes over the guard at Rabaul.']]);
    // (Men from his reserve and no points: whatever else his month's opening gives or takes, the reserve is down by the row's weight at least.)
    expect(g.nations.JAP.pp).toBeGreaterThanOrEqual(points);
    expect(menLeft(g, 'JAP')).toBeLessThan(before);
  });

  it('several ports in one month make one line of his report: three by name, then how many more (the UI director\'s 19:48Z)', () => {
    const lines = (ports: string[]): string[] => {
      const g = war(29);
      for (const nm of ports) take(g, nm, 'JAP');
      g.human = 'JAP';
      g.turn = 30;
      while (current(g) !== 'JAP') g.phase = (g.phase + 1) % g.order.length;
      beginHuman(g);
      for (const nm of ports) expect(rows(g, nm, 'JAP').length, nm).toBe(1);
      return g.headlines.filter((h) => h.turn === 30 && h.text.includes('the guard at')).map((h) => h.text);
    };
    // (The ports are named in the chart's own order, whatever order they fell in.)
    const order = (ports: string[]): string[] => { const g = war(29); return [...ports].sort((a, b) => g.byName[a].id - g.byName[b].id); };
    const two = order(['Rabaul', 'Kuching']);
    expect(lines(two)).toEqual([`Base forces take over the guard at ${two[0]} and ${two[1]}.`]);
    const three = order(['Rabaul', 'Kuching', 'Menado']);
    expect(lines(three)).toEqual([`Base forces take over the guard at ${three[0]}, ${three[1]} and ${three[2]}.`]);
    const five = order(['Rabaul', 'Kuching', 'Menado', 'Kendari', 'Makassar']);
    expect(lines(five)).toEqual([`Base forces take over the guard at ${five[0]}, ${five[1]}, ${five[2]} and 2 more.`]);
  });

  it("each country's line is its own noun, and a table with none reads the plain one", () => {
    const says = (shores as any).open.rear;
    expect(says.by.JAP.says).toEqual(['A base force takes over the guard at {places}.', 'Base forces take over the guard at {places}.']);
    expect(says.by.USA.says).toEqual(['A base command takes over the guard at {places}.', 'Base commands take over the guard at {places}.']);
    for (const nat of ['UK', 'AUS']) expect(says.by[nat].says, nat).toEqual(['A base sub-area takes over the guard at {places}.', 'Base sub-areas take over the guard at {places}.']);
    expect(says.says).toEqual(['A garrison detachment takes over the guard at {places}.', 'Garrison detachments take over the guard at {places}.']);
  });

  it("a computer's country gets no line in anybody's report", () => {
    const g = war(29);
    take(g, 'Rabaul', 'JAP');
    g.turn = 30; rearMonth(g, 'JAP');
    expect(g.headlines.some((h) => h.text.includes('takes over the guard'))).toBe(false);
  });

  it("at the book's stops nothing comes by cause: the record's dated rows hold the ports there", () => {
    for (const stop of [1, 2]) {
      const g = war(29, stop);
      take(g, 'Rabaul', 'JAP');
      listen();
      g.turn = 30; rearMonth(g, 'JAP');
      expect(told, `stop ${stop}`).toEqual([]);
    }
  });

  it("the five dated guard rows are the book's stops' alone, so that no port has both", () => {
    const names = ['54th Guard Unit (Guam)', 'Kendari naval air base guard', 'Menado naval guard detachment', 'Biak naval guard', 'Guadalcanal airfield guard (with the 11th and 13th Construction Units)'];
    for (const nm of names) {
      const row = ((setup as any).expeditions as any[]).find((r) => r[2] === nm);
      expect(row, nm).toBeTruthy();
      expect(row[6].stops, nm).toEqual([1, 2]);
    }
  });

  it("not on China's ground: a taken Chinese port keeps the garrison its own rule asks", () => {
    const g = war(29);
    take(g, 'Wenchow', 'JAP');
    take(g, 'Foochow', 'JAP');
    listen();
    g.turn = 30; rearMonth(g, 'JAP');
    expect(told).toEqual([]);
  });

  it('not on ground that only passed into keeping: the Indies in British keeping were not taken', () => {
    const g = war(29);
    const r = g.byName.Batavia;
    r.ctrl = 'UK'; r.since = 29; r.prev = null;
    listen();
    g.turn = 30; rearMonth(g, 'UK');
    expect(told).toEqual([]);
  });
});

describe('each country reads its own table and nobody another\'s', () => {
  it("the United States on Japan's ground: an island command at 0.075, Saipan's at 0.2, a base command at a port the table does not name", () => {
    const g = war(60);
    for (const nm of ['Kwajalein', 'Eastern Marshalls', 'Saipan', 'Truk']) take(g, nm, 'USA');
    listen();
    g.turn = 61; rearMonth(g, 'USA');
    const by = Object.fromEntries(told.map((t) => [t.at, [t.nat, t.name, t.worth]]));
    expect(by).toEqual({
      Kwajalein: ['USA', 'Kwajalein island command (army garrison force)', 0.075],
      'Eastern Marshalls': ['USA', '1st Defense Battalion (Majuro)', 0.075],
      Saipan: ['USA', 'Saipan island command (army garrison force)', 0.2],
      Truk: ['USA', 'Truk base command', 0.075],
    });
    // Japan's table gives nothing for them, and America's nothing for Japan's.
    rearMonth(g, 'JAP');
    expect(told.length).toBe(4);
  });

  it("Britain on Japan's ground: a base sub-area at 0.075", () => {
    const g = war(60);
    take(g, 'Truk', 'UK');
    listen();
    g.turn = 61; rearMonth(g, 'UK');
    expect(told).toEqual([{ nat: 'UK', at: 'Truk', name: 'Truk base sub-area', worth: 0.075 }]);
  });

  it("a living ally's port goes back to that ally as it is freed, and the row is that ally's own: Tarawa is Britain's, whoever freed it, and Rabaul Australia's", () => {
    const g = war(34);
    take(g, 'Rabaul', 'JAP');
    take(g, 'Tarawa', 'JAP');
    g.turn = 50;
    g.nations.USA.align = 'allies';
    expect([allied(g, 'USA', 'AUS'), allied(g, 'USA', 'UK')]).toEqual([true, true]);
    take(g, 'Rabaul', 'USA');
    take(g, 'Tarawa', 'USA');
    expect([g.byName.Rabaul.ctrl, g.byName.Tarawa.ctrl]).toEqual(['AUS', 'UK']);
    listen();
    g.turn = 51; rearMonth(g, 'USA');
    expect(told).toEqual([]);
    rearMonth(g, 'UK'); rearMonth(g, 'AUS');
    expect(told).toEqual([{ nat: 'UK', at: 'Tarawa', name: 'Tarawa base sub-area', worth: 0.075 }, { nat: 'AUS', at: 'Rabaul', name: 'Rabaul base sub-area', worth: 0.1 }]);
  });

  it("a country's own port that it takes back gets its own row: Guam's island command at 0.2, Buna's base at 0.05; one never lost gets none", () => {
    const g = war(34);
    take(g, 'Guam', 'JAP');
    take(g, 'Buna', 'JAP');
    g.turn = 59;
    take(g, 'Guam', 'USA');
    take(g, 'Buna', 'AUS');
    listen();
    g.turn = 60; rearMonth(g, 'USA'); rearMonth(g, 'AUS');
    expect(told).toEqual([{ nat: 'USA', at: 'Guam', name: 'Guam island command', worth: 0.2 }, { nat: 'AUS', at: 'Buna', name: 'Buna base (Oro Bay)', worth: 0.05 }]);
    expect(rows(g, 'Pearl Harbor', 'USA').some((c) => c.name.includes('base command'))).toBe(false);
  });

  it('a country with no table gets no row: New Zealand', () => {
    const g = war(60);
    take(g, 'Truk', 'NZL');
    listen();
    g.turn = 61; rearMonth(g, 'NZL');
    expect(told).toEqual([]);
  });

  it("a fallen ally's port stays the taker's, and gets the taker's row: Hollandia, the Netherlands fallen", () => {
    const g = war(34);
    g.nations.NED.alive = false;
    take(g, 'Hollandia', 'JAP');
    g.turn = 56;
    take(g, 'Hollandia', 'USA');
    expect(g.byName.Hollandia.ctrl).toBe('USA');
    listen();
    g.turn = 57; rearMonth(g, 'USA');
    expect(told).toEqual([{ nat: 'USA', at: 'Hollandia', name: 'Hollandia base command', worth: 0.075 }]);
  });
});

describe('what it is for: the landing force alone in the port it took is no longer its last guard', () => {
  const SSD = 'South Seas Detachment';
  /** New Britain in month 30: Rabaul taken in month `taken`, the detachment alone in it, Cape Gloucester empty beside it. */
  const newBritain = (taken: number): { g: Game; ssd: Corps } => {
    const g = war(taken);
    for (const c of g.corps.filter((x) => x.name === SSD)) removeCorps(g, c);
    take(g, 'Rabaul', 'JAP');
    const cg = g.byName['Cape Gloucester'];
    for (const c of [...cg.units]) removeCorps(g, c);
    g.flags[`guard:${cg.id}`] = 100;
    const ssd = addCorps(g, 'JAP', SSD, 'inf', 1, 'Rabaul', { xp: 0.3 });
    ssd.str = 0.33;
    g.turn = 30;
    return { g, ssd };
  };

  it('Rabaul taken the month before: the base force comes, and the detachment is free to go', () => {
    const { g, ssd } = newBritain(29);
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.at === 'Rabaul').map((t) => t.name)).toEqual(['8th Base Force (Rabaul)']);
    expect(g.byName.Rabaul.ctrl).toBe('JAP');
    expect(rows(g, 'Rabaul', 'JAP').map((c) => c.name)).toContain('8th Base Force (Rabaul)');
    expect(g.regions[ssd.reg].name).not.toBe('Rabaul');
  });

  it('Rabaul taken this very month: no row yet, and the detachment is the last guard and stays', () => {
    const { g, ssd } = newBritain(30);
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.at === 'Rabaul')).toEqual([]);
    expect(g.regions[ssd.reg].name).toBe('Rabaul');
  });
});
