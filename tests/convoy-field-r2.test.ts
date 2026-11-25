// The wolf pack and convoy battle's field (release 2; ruling 241 D2 and the design director's rider of 24 November
// 12:34Z): the night itself, the names the 3D sea scene reads, the kit that moves it, and the hand-back by rank
// (217's seam): computer against computer, a night books the dice's own spread.
import { afterEach, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { convoyField, type ConvoyMeeting } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import { convoyCloseWith, type ConvoyPlayed } from '../src/campaign/convoyhand';
import { loadGame, saveGame } from '../src/campaign/save';
import type { Game } from '../src/campaign/types';
import {
  CONVOY, CONVOY_PRACTICE, ConvoySim, convoyNight, convoyNightsAhead, convoyPracticeTicket, foughtOf, inRun, nightBooked, nightHours, moonLit,
  plainConvoyTicket, rankIn, ranksOf, staffNight, staffNights, type ConvoyKit, type ConvoyTicket,
} from '../src/battle/convoy';
import { hashString } from '../src/core/rng';

afterEach(() => { convoyField.fought = undefined; });

const kit = (radar: boolean, hedgehog: boolean, groups: boolean): ConvoyKit => ({ asdic: true, radar, hedgehog, groups });
const card = (boats: number, escorts: number, k: ConvoyKit, seed: number, year = 1942): ConvoyTicket =>
  plainConvoyTicket({ title: 't', sea: 'natl', year, month: 9, boats: [{ name: 'Pack', nat: 'GER', n: boats, expect: 1.4 }], escorts: [{ name: 'Escorts', nat: 'UK', n: escorts, expect: 0.3 }], kit: { UK: k, GER: kit(false, false, false) } }, seed, null);
const total = (r: Record<number, number>): number => Object.values(r).reduce((s, n) => s + n, 0);
/** What the scene reads (field-side-read.md §3, form §5, the sea scene's stand-ins): every event a night tells is one of these. */
const SCENE_EVENTS = new Set(['contact', 'end', 'withdrawn', 'sea:report', 'sea:dive', 'sea:sonar', 'sea:torpedo', 'sea:torpedoHit', 'sea:sunk', 'sea:burning', 'sea:men', 'sea:oil',
  'sea:depthCharge', 'sea:depthChargeHit', 'sea:starshell', 'sea:gun', 'sea:gunHit', 'sea:abandon', 'sea:surrender', 'sea:rescue']);

describe('the sky', () => {
  it('gives a northern night its length by the season and the moon its phase by the date', () => {
    expect(nightHours(58, 1942, 11, 21)).toBeGreaterThan(16.5);
    expect(nightHours(58, 1942, 5, 21)).toBeLessThan(6.5);
    expect(nightHours(50, 1941, 2, 20)).toBeGreaterThan(11.5);
    expect(nightHours(50, 1941, 2, 20)).toBeLessThan(12.5);
    // 2 January 1942 was a full moon, 16 January 1942 a new one.
    expect(moonLit(1942, 0, 2)).toBeGreaterThan(0.95);
    expect(moonLit(1942, 0, 16)).toBeLessThan(0.05);
  });
});

describe('the night', () => {
  it('replays the same night from the same ticket, watched or not', () => {
    const t = card(10, 7, kit(true, false, false), 11);
    const a = new ConvoySim(t), b = new ConvoySim(t);
    a.runOut();
    const told: string[] = [];
    while (!b.over) { b.step(); if (Math.round(b.t * 10) % 50 === 0) for (const e of b.view().events) told.push(e.what); }
    expect(b.counts).toEqual(a.counts);
    expect(b.worked).toBe(a.worked);
    expect(told.length).toBeGreaterThan(0);
  });

  it('tells only what the sea scene reads, in its names', () => {
    const seen = new Set<string>();
    let dives = 0, sonar = 0, charges = 0;
    for (let s = 1; s <= 4; s++) {
      const sim = new ConvoySim(card(12, 8, kit(true, true, true), s, 1943));
      while (!sim.over) {
        for (let k = 0; k < 300; k++) sim.step();
        const v = sim.view();
        for (const e of v.events) {
          seen.add(e.what);
          const x = e.extra ?? {};
          if (e.what === 'sea:dive') { dives++; expect(['surfaced', 'periscope', 'deep']).toContain(x.dive); expect(typeof x.why).toBe('string'); }
          if (e.what === 'sea:sonar' && x.lost === undefined) { sonar++; expect(['asdic', 'radar', 'sight']).toContain(x.kind); expect(typeof x.range).toBe('number'); expect(typeof x.bearing).toBe('number'); }
          if (e.what === 'sea:depthCharge') { charges++; expect(['pattern', 'ahead']).toContain(x.kind); for (const k of ['on', 'x', 'y', 'n', 'set', 'sinking', 'spread']) expect(typeof x[k]).toBe('number'); }
          if (e.what === 'sea:sunk') { expect(['ss', 'dd', 'mv']).toContain(x.cls); expect(typeof x.how).toBe('string'); }
          if (e.what === 'sea:torpedo') for (const k of ['target', 'x', 'y', 'running', 'fish', 'spread']) expect(typeof x[k]).toBe('number');
        }
        for (const u of v.units) {
          // (a boat's depth is 'keel', never 'd' or 'depth': the scene reads a piece's d as its length, SeaFeed.cpp:102)
          expect(u.extra.depth).toBeUndefined();
          if (u.kind === 'ss' && u.state !== 'gone') { expect(['surfaced', 'periscope', 'deep']).toContain(u.extra.dive); expect(Array.isArray(u.extra.heard)).toBe(true); }
        }
      }
    }
    for (const w of seen) expect(SCENE_EVENTS.has(w)).toBe(true);
    expect(dives).toBeGreaterThan(0);
    expect(sonar).toBeGreaterThan(0);
    expect(charges).toBeGreaterThan(0);
    expect(seen.has('sea:torpedoHit')).toBe(true);
  }, 60_000);

  it('is decided by the kit the escorts\' navy has researched, never the year', () => {
    const N = 12;
    const mean = (k: ConvoyKit, year: number): { mv: number; ss: number } => {
      let mv = 0, ss = 0;
      for (let s = 1; s <= N; s++) { const sim = new ConvoySim(card(10, 7, k, s, year)); sim.runOut(); mv += sim.counts.mv; ss += total(sim.counts.ss); }
      return { mv: mv / N, ss: ss / N };
    };
    const bare = mean(kit(false, false, false), 1942), radar = mean(kit(true, false, false), 1942), all = mean(kit(true, true, true), 1942);
    expect(radar.mv).toBeLessThan(bare.mv);
    expect(all.ss).toBeGreaterThan(bare.ss);
    expect(all.mv).toBeLessThan(bare.mv);
    // The same kit in another year (same month, same sea) fights the same night: the year moves nothing but the calendar's moon.
    const t1 = card(10, 7, kit(true, true, true), 5, 1941), t2 = card(10, 7, kit(true, true, true), 5, 1944);
    const a = new ConvoySim({ ...t1, place: { ...t1.place, light: t2.place.light } }), b = new ConvoySim(t2);
    a.runOut(); b.runOut();
    expect(a.counts).toEqual(b.counts);
  });

  it('forces boats up, and their crews fight, dive away or give them up (Brian, 23 November)', () => {
    const t = { att: 0, up: 0, scuttled: 0, rammed: 0, crushed: 0, abandon: 0, men: 0 };
    for (let s = 1; s <= 10; s++) {
      const sim = new ConvoySim(card(15, 10, kit(true, true, true), s, 1943));
      while (!sim.over) { for (let k = 0; k < 600; k++) sim.step(); for (const e of sim.view().events) { if (e.what === 'sea:abandon') t.abandon++; if (e.what === 'sea:men') t.men++; } }
      t.up += sim.tally.forcedUp; t.scuttled += sim.tally.scuttled; t.rammed += sim.tally.rammed; t.crushed += sim.tally.crushed;
    }
    expect(t.up).toBeGreaterThan(0);
    expect(t.scuttled).toBeGreaterThan(0);
    expect(t.crushed).toBeGreaterThan(0);
    expect(t.abandon).toBeGreaterThanOrEqual(t.scuttled);
    expect(t.men).toBeGreaterThan(0);
  });

  it('shows the player only his own side\'s contacts', () => {
    const t = card(10, 7, kit(true, false, false), 3);
    const sim = new ConvoySim({ ...t, def: { ...t.def, human: true } });
    let checked = 0;
    while (!sim.over && checked < 200) {
      for (let k = 0; k < 100; k++) sim.step();
      const all = sim.view(), mine = sim.view('def');
      const boats = mine.units.filter((u) => u.kind === 'ss' && u.state !== 'gone');
      for (const b of boats) expect(all.units.find((u) => u.id === b.id)).toBeDefined();
      expect(boats.length).toBeLessThanOrEqual(all.units.filter((u) => u.kind === 'ss' && u.state !== 'gone').length);
      checked++;
    }
  });
});

describe('the hand-back (217\'s seam: booked by rank)', () => {
  it('ranks a count among the computer\'s nights, ties shared by the draw', () => {
    expect(rankIn(0, [0, 0, 1, 2], 0)).toBe(0);
    expect(rankIn(0, [0, 0, 1, 2], 1)).toBe(0.5);
    expect(rankIn(3, [0, 0, 1, 2], 0.3)).toBe(1);
    expect(inRun(1.3, 0.69)).toBe(1);
    expect(inRun(1.3, 0.71)).toBe(2);
    expect(inRun(0, 0.99)).toBe(0);
  });

  it('computer against computer books the dice\'s own spread: ranks flat, the upper count at the dice\'s odds, a swing of 1 on average', () => {
    for (const p of [CONVOY_PRACTICE[1], CONVOY_PRACTICE[3]]) {
      const t = convoyPracticeTicket({ ...p, boats: p.boats.map((b) => ({ ...b, expect: 1.4 })), escorts: p.escorts.map((e) => ({ ...e, expect: 0.3 })) }, 7, null);
      const staff = staffNights(t, 30);
      const M = 60, mv: number[] = [], ss: number[] = [], swing: number[] = [];
      let upper = 0;
      for (let i = 0; i < M; i++) {
        const sim = staffNight(t, hashString(`proof:${t.seed}:${i}`));
        sim.runOut();
        const ranks = ranksOf({ ...t, seed: hashString(`proofrank:${i}`) }, sim.counts, staff);
        const f = foughtOf(t, ranks);
        mv.push(ranks.mv); ss.push(Object.values(ranks.ss)[0]); swing.push(f.swing);
        if (Object.values(f.ss)[0] === 2) upper++;
      }
      const flat = (xs: number[]): number => { const s = [...xs].sort((a, b) => a - b); return Math.max(...s.map((x, i) => Math.abs(x - (i + 0.5) / s.length))); };
      expect(flat(mv)).toBeLessThan(0.2);
      expect(flat(ss)).toBeLessThan(0.2);
      expect(Math.abs(swing.reduce((a, b) => a + b, 0) / M - 1)).toBeLessThan(0.03);
      expect(Math.abs(upper / M - 0.4)).toBeLessThan(0.15);
    }
  }, 120_000);

  it('a night handed to the staff before anything is fought is none: the month is the dice\'s', () => {
    const t = card(8, 6, kit(true, false, false), 9);
    const { sim, staff } = convoyNight(t, 4);
    sim.command({ do: 'dice' }, 'def');
    expect(sim.over).not.toBeNull();
    expect(nightBooked(sim, staff)).toBeUndefined();
  });

  it('handed over midway, the rest is settled at the computer\'s own rate from that hour', () => {
    const t = card(10, 7, kit(true, false, false), 4);
    const { sim, staff } = convoyNight(t, 8);
    while (!sim.over && sim.view().contact === null) for (let k = 0; k < 100; k++) sim.step();
    for (let k = 0; k < 3000 && !sim.over; k++) sim.step();
    const before = sim.counts.mv;
    sim.command({ do: 'dice' }, 'def');
    expect(sim.over).not.toBeNull();
    expect(sim.counts.mv).toBeGreaterThanOrEqual(before);
    expect(nightBooked(sim, staff)).toBeDefined();
  });
});

describe('the campaign\'s night', () => {
  function seat(nat: string, seed = 1): Game {
    const g = startOpen(createGame(seed, nat));
    g.flags.convoyBattle = 1;
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    beginHuman(g);
    return g;
  }

  it('writes a British seat\'s night from the look-ahead\'s own copy, and books it inside every run', () => {
    const g = seat('UK');
    const before = saveGame(g);
    const ahead = convoyNightsAhead(g);
    expect(saveGame(g)).toBe(before);
    expect(ahead.length).toBe(1);
    const [a] = ahead;
    const t = a.ticket;
    expect(t.place.route).toBe('atlantic');
    expect(['wapp', 'natl', 'irish']).toContain(t.place.sea);
    expect(t.def.human).toBe(true);
    expect(t.place.fleets.filter((f) => f.side === 'att').reduce((s, f) => s + f.n, 0)).toBeLessThanOrEqual(20);
    expect(t.place.ships).toBe(CONVOY.convoy.ships.atlantic);
    const { sim, staff } = convoyNight(t, 6);
    sim.command({ do: 'auto', side: 'def', on: true }, 'def');
    sim.runOut();
    const booked = nightBooked(sim, staff)!;
    for (const f of t.place.fleets) {
      const got = f.side === 'att' ? booked.fought.ss[f.id] : booked.fought.dd[f.id];
      if (f.n <= 0) { expect(got).toBeUndefined(); continue; }
      expect(got).toBeGreaterThanOrEqual(Math.floor(f.expect));
      expect(got).toBeLessThanOrEqual(Math.floor(f.expect) + 1);
    }
    expect(booked.fought.swing).toBeGreaterThanOrEqual(0.8);
    expect(booked.fought.swing).toBeLessThanOrEqual(1.2);
    const played: ConvoyPlayed = new Map([[a.key, { meeting: a.meeting as ConvoyMeeting, fought: booked.fought }]]);
    const twin = loadGame(saveGame(g))!;
    convoyCloseWith(twin, played);
    expect(twin.convoys.atlantic.fought).toBe('field');
  }, 60_000);
});
