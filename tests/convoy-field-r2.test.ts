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
  afterDusk, againstStaff, CONVOY, CONVOY_PRACTICE, ConvoySim, convoyNight, convoyNightsAhead, convoyPracticeTicket, fateWords, foughtOf, inRun, levelOfNight, lightOf, namesOf, nightBooked, nightHours, moonLit,
  plainConvoyTicket, rankIn, ranksOf, seaAt, staffNight, staffNights, type ConvoyKit, type ConvoyOverExtra, type ConvoyTicket, type ConvoyUnit, type ConvoyView,
} from '../src/battle/convoy';
import type { FieldEvent, Side } from '../src/battle/core/types';
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
      // (a card's 30 staff nights are one draw of the staff's spread, and the ranks are flat over the draws: the proof
      // pools six tickets of the card, 20 nights against each one's own 30)
      const seeds = [7, 8, 9, 10, 11, 12], M = 20 * seeds.length, mv: number[] = [], ss: number[] = [], swing: number[] = [];
      let upper = 0;
      for (const seed of seeds) {
        const t = convoyPracticeTicket({ ...p, boats: p.boats.map((b) => ({ ...b, expect: 1.4 })), escorts: p.escorts.map((e) => ({ ...e, expect: 0.3 })) }, seed, null);
        const staff = staffNights(t, 30);
        for (let i = 0; i < M / seeds.length; i++) {
          const sim = staffNight(t, hashString(`proof:${t.seed}:${i}`));
          sim.runOut();
          const ranks = ranksOf({ ...t, seed: hashString(`proofrank:${seed}:${i}`) }, sim.counts, staff);
          const f = foughtOf(t, ranks);
          mv.push(ranks.mv); ss.push(Object.values(ranks.ss)[0]); swing.push(f.swing);
          if (Object.values(f.ss)[0] === 2) upper++;
        }
      }
      const flat = (xs: number[]): number => { const s = [...xs].sort((a, b) => a - b); return Math.max(...s.map((x, i) => Math.abs(x - (i + 0.5) / s.length))); };
      expect(flat(mv)).toBeLessThan(0.2);
      expect(flat(ss)).toBeLessThan(0.2);
      expect(Math.abs(swing.reduce((a, b) => a + b, 0) / M - 1)).toBeLessThan(0.03);
      expect(Math.abs(upper / M - 0.4)).toBeLessThan(0.15);
    }
  }, 180_000);

  it('counts the night against the staff\'s own, plainly, for the after card (words only)', () => {
    const t = card(4, 3, kit(true, false, false), 5);
    const [boats, escorts] = t.place.fleets.map((f) => f.id);
    const staff = { n: 4, counts: { [`ss:${boats}`]: [0, 1, 2, 1], [`dd:${escorts}`]: [0, 0, 1, 0], mv: [5, 10, 15, 10] }, profile: {}, tallies: {} };
    const a = againstStaff(t, { ss: { [boats]: 1 }, dd: {}, mv: 10 }, staff);
    expect(a.n).toBe(4);
    // (sank more ships than the staff did in 1 of its 4 nights; lost fewer boats than in 1, as many in 2)
    expect(a.mv).toEqual({ less: 1, same: 2, more: 1 });
    expect(a.ss).toEqual({ less: 1, same: 2, more: 1 });
    expect(a.dd).toEqual({ less: 0, same: 3, more: 1 });
  });

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

// ---- The playtest's second read (results-r2/convoy-practice, 24 November): the night's own bugs -----------------------

/** Works the night on, a battle tick at a time, until 'done' or the night is over; the side's picture each tick, its events kept. */
function run(sim: ConvoySim, as: Side, done: (v: ConvoyView, told: FieldEvent[]) => boolean = () => false, most = Infinity): { v: ConvoyView; told: FieldEvent[] } {
  const told: FieldEvent[] = [];
  let v = sim.view(as);
  const until = sim.worked + most;
  while (!sim.over && sim.worked < until) {
    sim.advance();
    v = sim.view(as);
    told.push(...v.events);
    if (done(v, told)) break;
  }
  return { v, told };
}
const said = (told: FieldEvent[], what: string, unit?: number): FieldEvent[] => told.filter((e) => e.what === what && (unit === undefined || e.unit === unit || (e.extra as { units?: number[] } | undefined)?.units?.includes(unit)));
const boatsUp = (v: ConvoyView): ConvoyUnit[] => v.units.filter((u) => u.kind === 'ss' && u.state !== 'gone' && u.state !== 'withdrawn');

describe('the pack closes as the convoy thins (playtest bug 1)', () => {
  it('never parks a boat with torpedoes off the convoy: she comes in to firing reach and the salvoes go on once an outer column is sunk', () => {
    for (const seed of [7, 3]) {
      const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[0], seed, 'att'));
      const parked = new Map<number, number>();
      let worst = 0, emptied = -1;
      run(sim, 'att', (v) => {
        const ships = v.units.filter((u) => u.kind === 'mv' && u.state !== 'gone');
        const cols = new Set(ships.map((m) => m.extra.column));
        if (emptied < 0 && (!cols.has(0) || !cols.has(8))) emptied = sim.tally.salvoes;
        for (const b of boatsUp(v)) {
          const near = Math.min(...ships.map((m) => Math.hypot(m.x - b.x, m.y - b.y)));
          // (in to attack, on the surface, torpedoes in the tubes, unheld, and still beyond firing reach of every ship)
          const idle = b.extra.doing === 'close' && b.extra.dive === 'surfaced' && (b.extra.tubes ?? 0) > 0 && !(b.extra.heard ?? []).length && near > CONVOY.boat.fireAt + 100 && near < 4000;
          if (idle) { const from = parked.get(b.id) ?? v.t; parked.set(b.id, from); worst = Math.max(worst, v.t - from); } else parked.delete(b.id);
        }
        return false;
      });
      // (before: Boats 1, 3 and 6 lay 2,075 m off from 3:37 to dawn on seed 7, salvoes stuck at 7)
      expect(worst).toBeLessThan(2700);
      expect(emptied).toBeGreaterThanOrEqual(0);
      expect(sim.tally.salvoes).toBeGreaterThan(emptied + 3);
    }
  });

  it('an attack ordered on a ship in an inner column takes the boat in to firing reach of her, and she fires at her', () => {
    const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[0], 7, 'att'));
    const { v } = run(sim, 'att', (w) => boatsUp(w).length > 0);
    const boat = boatsUp(v)[0];
    const inner = v.units.find((u) => u.kind === 'mv' && u.extra.column === 4 && u.state !== 'gone')!;
    expect(sim.command({ do: 'convoy:attack', units: [boat.id], target: inner.id, under: false }, 'att')).toBe(true);
    const { told } = run(sim, 'att', (_, t) => said(t, 'sea:torpedo', boat.id).length > 0, 3 * 3600);
    const salvo = said(told, 'sea:torpedo', boat.id)[0];
    expect(salvo).toBeDefined();
    expect((salvo.extra as { target: number }).target).toBe(inner.id);
  });
});

describe('orders answered (playtest bugs 12-14 and 23)', () => {
  it('keeps the depth the commander chose when a ship is right-clicked: Attack submerged stays submerged', () => {
    const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[0], 7, 'att'));
    const { v } = run(sim, 'att', (w) => boatsUp(w).length > 0);
    const boat = boatsUp(v)[0];
    const ship = v.units.find((u) => u.kind === 'mv' && u.state !== 'gone')!;
    sim.command({ do: 'convoy:attack', units: [boat.id], under: true }, 'att');
    expect(said(sim.view('att').events, 'sea:order', boat.id)[0].text).toBe(`${boat.name} attacks submerged.`);
    // (a right-click on a ship gives no depth)
    sim.command({ do: 'convoy:attack', units: [boat.id], target: ship.id }, 'att');
    expect(said(sim.view('att').events, 'sea:order', boat.id)[0].text).toBe(`${boat.name} attacks ${ship.name} submerged.`);
    const { v: w, told } = run(sim, 'att', (x) => x.units.find((u) => u.id === boat.id)?.extra.dive !== 'surfaced', 3600);
    expect(w.units.find((u) => u.id === boat.id)!.extra.dive).toBe('periscope');
    expect(said(told, 'sea:dive', boat.id).some((e) => (e.extra as { dive: string }).dive === 'surfaced')).toBe(false);
  });

  it('Attack brings a dived boat up, and she stays up while only held: her captain takes her down only for an escort closing, and says so', () => {
    let checked = 0;
    for (const seed of [1, 2, 3, 4, 5, 6]) {
      const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[1], seed, 'att'));
      const { v } = run(sim, 'att', (w) => boatsUp(w).length > 0);
      const boat = boatsUp(v)[0];
      sim.command({ do: 'convoy:dive', units: [boat.id] }, 'att');
      expect(sim.view('att').units.find((u) => u.id === boat.id)!.extra.dive).toBe('periscope');
      sim.command({ do: 'convoy:attack', units: [boat.id], under: false }, 'att');
      const now = sim.view('att');
      expect(now.units.find((u) => u.id === boat.id)!.extra.dive).toBe('surfaced');
      expect(said(now.events, 'sea:order', boat.id)[0].text).toBe(`${boat.name} attacks on the surface.`);
      let held = 0;
      const { told } = run(sim, 'att', (w) => {
        const b = w.units.find((u) => u.id === boat.id);
        if (b && b.extra.order === 'attack' && b.extra.dive === 'surfaced' && (b.extra.heard ?? []).length) held++;
        return !b || b.extra.order !== 'attack';
      }, 2 * 3600);
      for (const e of said(told, 'sea:dive', boat.id)) {
        const x = e.extra as { dive: string; why: string; reason?: string; override?: boolean };
        if (x.dive === 'surfaced' || x.why === 'ordered' || x.why === 'attack') continue;
        // (a captain's dive says why; against the attack order, only an escort closing takes her down)
        expect(typeof x.reason).toBe('string');
        expect(e.text).toContain(`: ${x.reason}.`);
        // (forced down from the surface against the order; once down and held, she may go deep, and says why)
        if (x.override && x.dive === 'periscope') expect(x.reason).toBe('an escort is closing');
      }
      checked += held;
    }
    // (on these nights she is held by radar for a while and stays up to attack)
    expect(checked).toBeGreaterThan(0);
  }, 60_000);

  it('refuses the deck gun beyond its reach or dived, and a dive on a boat forced up, each with the reason in the log', () => {
    const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[0], 7, 'att'));
    const { v } = run(sim, 'att', (w) => boatsUp(w).length > 0);
    const boat = boatsUp(v)[0];
    const far = v.units.filter((u) => u.kind === 'mv' && u.state !== 'gone').sort((a, b) => Math.hypot(b.x - boat.x, b.y - boat.y) - Math.hypot(a.x - boat.x, a.y - boat.y))[0];
    expect(Math.hypot(far.x - boat.x, far.y - boat.y)).toBeGreaterThan(CONVOY.surface.gunAt);
    expect(sim.command({ do: 'convoy:gun', units: [boat.id], target: far.id }, 'att')).toBe(false);
    let no = said(sim.view('att').events, 'sea:order', boat.id);
    expect(no.length).toBe(1);
    expect((no[0].extra as { ok: boolean }).ok).toBe(false);
    expect(no[0].text).toMatch(new RegExp(`^${boat.name} can't fire her deck gun at ${far.name}: the ship is [0-9,]+ m away, and the gun reaches 2,500 m\\.$`));
    sim.command({ do: 'convoy:dive', units: [boat.id] }, 'att');
    sim.view('att');
    expect(sim.command({ do: 'convoy:gun', units: [boat.id], target: far.id }, 'att')).toBe(false);
    no = said(sim.view('att').events, 'sea:order', boat.id);
    expect(no[0].text).toBe(`${boat.name} can't fire her deck gun while she is dived: surface her first.`);
    // (the answers are the player's side's alone, and never the scene's)
    sim.command({ do: 'convoy:gun', units: [boat.id], target: far.id }, 'att');
    expect(said(sim.view().events, 'sea:order').length).toBe(0);
    sim.command({ do: 'convoy:gun', units: [boat.id], target: far.id }, 'att');
    expect(said(sim.view('def').events, 'sea:order').length).toBe(0);

    // A boat forced to the surface can't dive: the refusal is the rule's, and says so.
    let up: { sim: ConvoySim; id: number } | null = null;
    for (let seed = 1; seed <= 12 && !up; seed++) {
      const s2 = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[3], seed, 'att'));
      const { told } = run(s2, 'att', (_, t) => t.some((e) => e.what === 'sea:dive' && (e.extra as { why: string }).why === 'forced'));
      const e = told.find((x) => x.what === 'sea:dive' && (x.extra as { why: string }).why === 'forced');
      if (e && !s2.over) up = { sim: s2, id: e.unit! };
    }
    expect(up).not.toBeNull();
    const name = up!.sim.view('att').units.find((u) => u.id === up!.id)!.name;
    expect(up!.sim.command({ do: 'convoy:dive', units: [up!.id] }, 'att')).toBe(false);
    no = said(up!.sim.view('att').events, 'sea:order', up!.id);
    expect(no[0].text).toBe(`${name} can't dive: she was forced to the surface and is too badly damaged.`);
  }, 60_000);

  it('names the pieces an order went to in one phrase', () => {
    expect(namesOf(['Escort 5'])).toBe('Escort 5');
    expect(namesOf(['Escort 2', 'Escort 5'])).toBe('Escorts 2 and 5');
    expect(namesOf(['Boat 1', 'Boat 3', 'Boat 4'])).toBe('Boats 1, 3 and 4');
  });

  it('says a rise from deep to periscope depth as a rise, never a dive', () => {
    const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[0], 7, 'att'));
    const { v } = run(sim, 'att', (w) => boatsUp(w).length > 0);
    const boat = boatsUp(v)[0];
    sim.command({ do: 'convoy:dive', units: [boat.id], deep: true }, 'att');
    expect(said(sim.view('att').events, 'sea:dive', boat.id)[0].text).toBe(`${boat.name} goes deep.`);
    sim.command({ do: 'convoy:attack', units: [boat.id], under: true }, 'att');
    const told = sim.view('att').events;
    expect(said(told, 'sea:dive', boat.id)[0].text).toBe(`${boat.name} comes up to periscope depth.`);
    expect(said(told, 'sea:order', boat.id)[0].text).toBe(`${boat.name} attacks submerged.`);
  });
});

describe('the escorts\' contacts as the player reads them (playtest bugs 9, 10 and 16)', () => {
  it('reports a contact once, says when it is lost, keeps the boat\'s last known spot three minutes, and hunts there', () => {
    const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[1], 3, 'def'));
    // The first boat lost from touch: her last known spot, and an escort sent to hunt her there.
    const { v, told } = run(sim, 'def', (w) => w.lastSeen.length > 0);
    const spot = v.lastSeen[0];
    expect(v.units.some((u) => u.id === spot.id)).toBe(false);
    expect(spot.cls).toBe('ss');
    expect(sim.worked - spot.t).toBeLessThanOrEqual(180);
    const lostLine = told.filter((e) => e.what === 'sea:sonar' && (e.extra as { on: number; gone?: boolean }).on === spot.id && (e.extra as { gone?: boolean }).gone);
    expect(lostLine.length).toBeGreaterThan(0);
    const escort = v.units.find((u) => u.kind === 'dd' && u.state !== 'gone' && u.extra.echo === undefined)!;
    expect(sim.command({ do: 'convoy:hunt', units: [escort.id], target: spot.id }, 'def')).toBe(true);
    const w = sim.view('def');
    expect(said(w.events, 'sea:order', escort.id)[0].text).toBe(`${escort.name} hunts ${spot.name} where she was last seen.`);
    expect(w.units.find((u) => u.id === escort.id)!.extra.chase).toBe(spot.id);
    // (three minutes on, the spot is too old to hunt: the order is refused, and says why)
    run(sim, 'def', () => false, 200);
    const later = sim.view('def');
    expect(later.lastSeen.filter((p) => p.id === spot.id && p.t === spot.t).length).toBe(0);
    if (!later.units.some((u) => u.id === spot.id) && !later.lastSeen.some((p) => p.id === spot.id)) {
      const other = later.units.find((u) => u.kind === 'dd' && u.state !== 'gone' && u.id !== escort.id)!;
      expect(sim.command({ do: 'convoy:hunt', units: [other.id], target: spot.id }, 'def')).toBe(false);
      expect(said(sim.view('def').events, 'sea:order', other.id)[0].text).toBe(`${other.name} can't hunt ${spot.name}: no escort holds her, and she was last seen more than three minutes ago.`);
    }
    // Over the night: a contact found again by the same escort within two minutes is the same contact (no new line).
    const rest = run(sim, 'def', () => false, 4 * 3600).told.concat(told);
    const sonar = rest.filter((e) => e.what === 'sea:sonar');
    const gained = sonar.filter((e) => (e.extra as { fresh?: boolean }).fresh !== undefined);
    expect(gained.some((e) => !(e.extra as { fresh: boolean }).fresh)).toBe(true);
    for (const e of gained) expect(!!e.text).toBe((e.extra as { fresh: boolean }).fresh);
    for (const e of gained) if (e.text) expect(e.text).toMatch(/^Escort \d+ (has Boat \d+ on (radar|sonar)|sights Boat \d+)\.$/);
    // A contact lost is told on a line of its own once it is sure: when the same escort has not found her again within
    // two minutes (the scene's own end of the contact is at once, and says nothing).
    for (const e of sonar.filter((e) => (e.extra as { lost?: string }).lost !== undefined)) expect(e.text).toBe('');
    const lost = rest.filter((e) => e.what === 'sea:lost');
    expect(lost.length).toBeGreaterThan(0);
    // (told two minutes on, with the hour she was lost: when the map's mark of her began)
    for (const e of lost) {
      expect(e.text).toMatch(/^Escort \d+ lost Boat \d+( when she dived,)? at \d+:\d\d, and has not found her again\.$/);
      const at = (e.extra as { at: number }).at;
      expect(e.t - at).toBe(120);
      expect(e.text).toContain(`at ${Math.floor(at / 3600)}:${String(Math.floor((at % 3600) / 60)).padStart(2, '0')},`);
    }
    // Every contact reported is told lost before the same escort reports her again: never a boat that comes and goes
    // with no line at either end (the escort sent hunting aside, whose hunt lets her contacts go).
    const lines = rest.filter((e) => e.text && (e.what === 'sea:sonar' || e.what === 'sea:lost') && e.unit !== escort.id).sort((a, b) => a.t - b.t);
    const open = new Set<string>();
    for (const e of lines) {
      const k = `${e.unit}:${(e.extra as { on: number }).on}`;
      if (e.what === 'sea:lost') { expect(open.has(k)).toBe(true); open.delete(k); } else { expect(open.has(k)).toBe(false); open.add(k); }
    }
    // The first contact of the night is not the convoy under attack: that is said at the first salvo only.
    const first = rest.find((e) => e.what === 'contact')!;
    expect(first.text).toBe('');
    const attack = rest.filter((e) => e.what === 'sea:attack');
    expect(attack.length).toBeLessThanOrEqual(1);
    for (const e of attack) { expect(e.text).toBe('The convoy is under attack.'); expect(e.t).toBeGreaterThanOrEqual(first.t); }
  }, 60_000);

  it('tells the convoy it is under attack at the first salvo, and the pack when one of its boats is found or depth-charged', () => {
    const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[3], 2, 'att'));
    const { told } = run(sim, 'att', (_, t) => t.some((e) => e.what === 'sea:found') && t.some((e) => e.what === 'sea:depthCharge') && t.some((e) => e.what === 'sea:attack'));
    const attack = told.filter((e) => e.what === 'sea:attack');
    expect(attack.length).toBe(1);
    expect(attack[0].text).toBe('The convoy is under attack.');
    expect(told.find((e) => e.what === 'sea:torpedo')!.t).toBe(attack[0].t);
    // (found on the surface, a boat that dives for it says so herself, once: 'Boat 3 dives: an escort has found her.')
    const found = told.find((e) => e.what === 'sea:found')!;
    if (found.text) expect(found.text).toMatch(/^An escort has found Boat \d+\.$/);
    else expect(said(told, 'sea:dive', found.unit).find((e) => e.t >= found.t)!.text).toMatch(/^Boat \d+ (dives|goes deep): an escort has found her\.$/);
    const charge = told.find((e) => e.what === 'sea:depthCharge')!;
    expect(charge.side).toBe('att');
    expect(charge.text).toMatch(/^Boat \d+ is (depth-charged|attacked with ahead-throwers)\.$/);
    // (the pack is never told which escort holds or attacks her)
    expect(told.some((e) => e.what === 'sea:sonar')).toBe(false);
  }, 60_000);
});

describe('the after card counts the whole night (playtest bugs 4, 5 and 17)', () => {
  it('after a hand-over, the night\'s measures take the staff\'s share as its dice fell, split as the lines read them, and the night runs to dawn', () => {
    const t = card(10, 7, kit(true, false, false), 4);
    const { sim, staff } = convoyNight(t, 8);
    while (!sim.over && sim.view().contact === null) sim.advance();
    for (let k = 0; k < 1500 && !sim.over; k++) sim.advance();
    const before = { ...sim.tally }, at = sim.worked;
    sim.command({ do: 'dice' }, 'def');
    const over = sim.over!;
    const x = over.extra as unknown as ConvoyOverExtra;
    expect(over.seconds).toBe(sim.limit);
    expect(x.handedAt).toBe(at);
    const part = x.staff!;
    for (const k of Object.keys(before) as (keyof typeof before)[]) expect(x.tally[k]).toBe(before[k] + part.tally[k]);
    // (every boat lost went some way, every ship the staff sank took a torpedo, and a salvo carries four)
    const lostB = total(x.counts.ss), ways = x.tally.crushed + x.tally.scuttled + x.tally.rammed + x.tally.shelled + x.tally.captured;
    expect(ways).toBe(lostB);
    expect(part.tally.hits).toBe(part.counts.mv + total(part.counts.dd));
    expect(part.tally.salvoes * CONVOY.boat.salvo).toBeGreaterThanOrEqual(part.tally.hits);
    expect(part.tally.forcedUp).toBeGreaterThanOrEqual(part.tally.scuttled + part.tally.rammed + part.tally.shelled + part.tally.captured + part.tally.escaped);
    expect(x.counts.mv).toBe(sim.counts.mv);
    expect(over.lines.some((l) => l.startsWith('The staff fought the night from '))).toBe(true);
    // (the booking reads the counts as before: the night is ranked and booked)
    expect(nightBooked(sim, staff)).toBeDefined();
    expect(staff.tallies.salvoes.length).toBe(10);
  });

  it('counts the whole pack in the pack\'s score, boats still to come and gone included', () => {
    const t = convoyPracticeTicket(CONVOY_PRACTICE[0], 7, 'att');
    const sim = new ConvoySim(t);
    const { v } = run(sim, 'att', (w) => boatsUp(w).length > 0);
    expect(v.units.filter((u) => u.kind === 'ss').length).toBe(1);
    expect(v.pack).toBe(6);
    expect(sim.packSize).toBe(6);
    expect(sim.view('def').pack).toBe(6);
    sim.runOut();
    expect(sim.over!.lines.some((l) => / of 6[.:]/.test(l) && /boats? lost/i.test(l))).toBe(true);
  });

  it('says how every boat lost went, rammed, shelled and taken too, and those forced up that got away', () => {
    expect(fateWords({ attacks: 9, hunts: 4, salvoes: 3, hits: 2, forcedUp: 4, escaped: 1, scuttled: 0, rammed: 1, shelled: 1, crushed: 2, captured: 1 }, 5, 8))
      .toEqual(['5 boats lost of 8: 2 sunk by depth charges, 1 rammed, 1 sunk by gunfire and 1 taken by an escort.', '4 boats were forced to the surface; 1 of them dived again and got away.']);
    expect(fateWords({ attacks: 0, hunts: 0, salvoes: 0, hits: 0, forcedUp: 1, escaped: 1, scuttled: 0, rammed: 0, shelled: 0, crushed: 0, captured: 0 }, 0, 6))
      .toEqual(['No boats lost of 6.', '1 boat was forced to the surface and dived again to get away.']);
    expect(fateWords({ attacks: 3, hunts: 1, salvoes: 0, hits: 0, forcedUp: 1, escaped: 0, scuttled: 1, rammed: 0, shelled: 0, crushed: 0, captured: 0 }, 1, 6))
      .toEqual(['1 boat lost of 6: 1 scuttled by her crew.', '1 boat was forced to the surface.']);
    // On the hunters' nights every boat lost is told by how she went, and the ways add up to the boats lost.
    let rammed = 0;
    for (let seed = 1; seed <= 6; seed++) {
      const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[3], seed, null));
      sim.runOut();
      const tl = sim.tally, lostB = total(sim.counts.ss);
      expect(tl.crushed + tl.scuttled + tl.rammed + tl.shelled + tl.captured).toBe(lostB);
      expect(sim.over!.lines).toContain(fateWords(tl, lostB, sim.packSize)[0]);
      rammed += tl.rammed + tl.shelled;
    }
    expect(rammed).toBeGreaterThan(0);
  }, 60_000);
});

describe('the practice nights\' sky (playtest bug 15)', () => {
  it('works out a card\'s night in its own month: the long winter night is November\'s, about seventeen hours', () => {
    const p = CONVOY_PRACTICE[2];
    expect(p.month).toBe(11);
    const [lat] = seaAt(p.sea);
    let hours = 0;
    for (let seed = 1; seed <= 10; seed++) {
      const t = convoyPracticeTicket(p, seed, null);
      expect(t.place.when).toEqual({ year: 1942, month: 10 });
      expect(t.place.light).toEqual(lightOf(seed, lat, 1942, 10));
      // (a November night at the North Atlantic's anchor, from its day of the month: under December's, about 18 hours)
      expect(t.place.light.hours).toBeGreaterThan(15);
      expect(t.place.light.hours).toBeLessThan(17.7);
      expect(t.place.light.hours).toBe(Math.round(nightHours(lat, 1942, 10, t.place.light.day) * 100) / 100);
      expect(t.limit).toBe(Math.round(t.place.light.hours * 3600));
      hours += t.place.light.hours;
    }
    expect(Math.round(hours / 10)).toBe(17);
    // The first pack's September is September's: shorter than October's nights at the same day.
    expect(lightOf(1, lat, 1940, 8).hours).toBeLessThan(lightOf(1, lat, 1940, 9).hours);
    expect(convoyPracticeTicket(CONVOY_PRACTICE[0], 1, null).place.light).toEqual(lightOf(1, seaAt(CONVOY_PRACTICE[0].sea)[0], 1940, 8));
  });
});

// ---- The re-check of the second read (24 November, evening): the radar nights, the hunt kept by name, the clock ------

describe('a radar night\'s boats press home (the re-check: night 2 fired almost nothing)', () => {
  it('a boat sent down by radar goes deep only when sonar finds her below, and goes in again under water once unheard', () => {
    let salvoes = 0;
    for (let seed = 1; seed <= 5; seed++) {
      const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[2], seed, null));
      let up = 0, deep = 0;
      while (!sim.over) {
        sim.advance();
        const v = sim.view();
        for (const e of v.events) {
          if (e.what !== 'sea:dive' || (e.extra as { dive: string; why: string }).dive !== 'deep' || (e.extra as { why: string }).why !== 'evade') continue;
          // (her own captain's deep dive, found while evading: an escort holds her by sonar as she goes)
          if ((e.extra as { reason?: string }).reason) expect(v.units.some((u) => u.kind === 'dd' && u.extra.echo?.on === e.unit && u.extra.echo?.kind === 'asdic')).toBe(true);
        }
        if (sim.worked % 60 === 0) for (const u of boatsUp(v)) { up++; if (u.extra.dive === 'deep') deep++; }
      }
      expect(deep / Math.max(1, up)).toBeLessThan(0.3);
      salvoes += sim.tally.salvoes;
    }
    expect(salvoes / 5).toBeGreaterThan(3);
  }, 120_000);
});

describe('the hunt kept by name, and the pack told once a hunt (the re-check: question a, and the pack\'s log)', () => {
  it('an escort sent after a boat it holds still names her, and searches where she was, once it loses her', () => {
    const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[1], 3, 'def'));
    const { v } = run(sim, 'def', (w) => w.units.some((u) => u.kind === 'dd' && u.extra.echo !== undefined));
    const e = v.units.find((u) => u.kind === 'dd' && u.extra.echo !== undefined)!, boat = e.extra.echo!.on;
    expect(sim.command({ do: 'convoy:hunt', units: [e.id], target: boat }, 'def')).toBe(true);
    const { v: w } = run(sim, 'def', (x) => { const me = x.units.find((u) => u.id === e.id)!; return me.extra.echo === undefined && me.extra.doing === 'search'; }, 3600);
    const me = w.units.find((u) => u.id === e.id)!;
    expect(me.extra.doing).toBe('search');
    expect(me.extra.echo).toBeUndefined();
    expect(me.extra.prey).toBe(boat);
    expect(me.extra.goal).toBeDefined();
    expect(typeof me.extra.giveUp).toBe('number');
  });

  it('tells the pack a boat is found once a hunt, never again while an escort hunts her', () => {
    const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[3], 17, 'att'));
    // (what the staff's escorts are about, by the whole picture: who hunts which boat)
    let hunting = new Set<number>(), found = 0;
    while (!sim.over && sim.worked < 6 * 3600) {
      sim.advance();
      for (const e of sim.view('att').events) if (e.what === 'sea:found') { found++; expect(hunting.has(e.unit!)).toBe(false); }
      hunting = new Set(sim.view().units.filter((u) => u.kind === 'dd' && u.extra.prey !== undefined && ['run', 'search', 'attack', 'open'].includes(u.extra.doing as string)).map((u) => u.extra.prey as number));
    }
    expect(found).toBeGreaterThan(0);
  }, 60_000);

  it('counts the seconds run on at dusk as the player\'s, so a night he commanded throughout is his on the after card', () => {
    const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[1], 2, 'def'));
    sim.runOut();
    const x = sim.over!.extra as unknown as ConvoyOverExtra;
    expect(x.steered.def).toBe(sim.worked);
    expect(x.steered.att).toBe(0);
  });
});

// ---- The re-check of the third read (24 November, night): the captain's deep dive, the dived approach, the words ----

describe('a dived boat\'s way in, and the order she keeps (the re-check: override-dive, n3s7)', () => {
  it('a boat ordered to periscope depth whom her captain takes deep comes back up to it once unheard, and says when', () => {
    const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[1], 11, 'att'));
    // (a boat an escort holds by sonar, ordered to periscope depth: her captain takes her deep, against the order)
    const { v } = run(sim, 'att', () => sim.view().units.some((u) => u.kind === 'dd' && u.extra.echo?.kind === 'asdic'));
    const boat = sim.view().units.find((u) => u.kind === 'dd' && u.extra.echo?.kind === 'asdic')!.extra.echo!.on;
    expect(v.units.some((u) => u.id === boat)).toBe(true);
    sim.command({ do: 'convoy:dive', units: [boat] }, 'att');
    const { told } = run(sim, 'att', (_, t) => said(t, 'sea:dive', boat).some((e) => (e.extra as { dive: string }).dive === 'deep'), 3600);
    const deep = said(told, 'sea:dive', boat).find((e) => (e.extra as { dive: string }).dive === 'deep')!;
    expect((deep.extra as { override?: boolean }).override).toBe(true);
    // While she is down against the order the view says when she will come back up; she does, once no escort has heard her for E.lostFor.
    let heard = deep.t, upAt: number | undefined;
    const { told: later } = run(sim, 'att', (w, t) => {
      const me = w.units.find((u) => u.id === boat);
      if (me && (me.extra.heard ?? []).length) heard = sim.worked;
      if (me && me.extra.dive === 'deep') { expect(typeof me.extra.upAt).toBe('number'); upAt = me.extra.upAt as number; }
      return said(t, 'sea:dive', boat).length > 0 || !me || me.state === 'gone';
    }, 6 * 3600);
    const back = said(later, 'sea:dive', boat)[0];
    expect(back.text).toMatch(/^Boat \d+ comes up to periscope depth\.$/);
    expect(back.t - heard).toBeGreaterThanOrEqual(CONVOY.escort.lostFor);
    expect(back.t - heard).toBeLessThanOrEqual(CONVOY.escort.lostFor + 2 * CONVOY.check);
    expect(upAt).toBe(back.t);
  }, 60_000);

  it('never lets a boat creep in dived or lie off the convoy for hours: she gets in, or runs round on the surface', () => {
    // (before: on night 3 seed 7 Boat 10 lay 1.4-3.5 km off from 3:32 to 10:35, closing at periscope depth at 3.9 m/s against the convoy's 3.6)
    let salvoes = 0;
    for (const seed of [7, 17, 3]) {
      const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[3], seed, null));
      const since = new Map<number, number>();
      let worst = 0;
      run(sim, 'att', (v) => {
        const ships = v.units.filter((u) => u.kind === 'mv' && u.state !== 'gone');
        for (const b of boatsUp(v)) {
          const near = Math.min(...ships.map((m) => Math.hypot(m.x - b.x, m.y - b.y)));
          const idle = b.extra.doing === 'close' && (b.extra.tubes ?? 0) > 0 && near > CONVOY.boat.fireAt && near < 4000;
          if (idle) { const from = since.get(b.id) ?? v.t; since.set(b.id, from); worst = Math.max(worst, v.t - from); } else since.delete(b.id);
        }
        return false;
      });
      expect(worst).toBeLessThan(5400);
      salvoes += sim.tally.salvoes;
    }
    expect(salvoes).toBeGreaterThan(9);
  }, 120_000);

  it('an escort holding the boat it hunts gives up at its hunt\'s own hour, never a search\'s quarter hour', () => {
    const sim = new ConvoySim(convoyPracticeTicket(CONVOY_PRACTICE[1], 3, 'def'));
    const { v } = run(sim, 'def', (w) => w.units.some((u) => u.kind === 'dd' && u.extra.echo !== undefined));
    const e = v.units.find((u) => u.kind === 'dd' && u.extra.echo !== undefined)!;
    sim.command({ do: 'convoy:hunt', units: [e.id], target: e.extra.echo!.on }, 'def');
    let holding = 0, searching = 0;
    run(sim, 'def', (w) => {
      const me = w.units.find((u) => u.id === e.id)!;
      if (typeof me.extra.giveUp !== 'number') return me.extra.doing === 'back' || me.extra.doing === 'screen';
      if (me.extra.echo) { holding++; expect(me.extra.giveUp).toBe(me.extra.huntUntil); }
      else if (me.extra.doing === 'search') { searching++; expect(me.extra.giveUp).toBeLessThanOrEqual(me.extra.huntUntil as number); }
      return false;
    }, 3 * 3600);
    expect(holding).toBeGreaterThan(0);
    expect(searching).toBeGreaterThan(0);
  }, 60_000);

  it('tells the page a boat forced up too badly hurt to save is doomed: such a boat never gets away', () => {
    let doomed = 0;
    for (let seed = 1; seed <= 10; seed++) {
      const sim = new ConvoySim(card(15, 10, kit(true, true, true), seed, 1943));
      const marked = new Set<number>();
      run(sim, 'att', (v) => { for (const u of v.units) if (u.kind === 'ss' && u.extra.doomed) marked.add(u.id); return false; });
      const fin = sim.view();
      for (const id of marked) { doomed++; expect(fin.units.find((u) => u.id === id)!.state).toBe('gone'); }
    }
    expect(doomed).toBeGreaterThan(0);
  }, 120_000);
});

describe('the after card\'s words from the night\'s own record (the re-check: the winner line, the hours)', () => {
  it('grades the winner\'s night: close, clear, overwhelming or costly, never close for every night', () => {
    // (as the pack reads it: the escorts' wins are its defeats)
    expect(levelOfNight(21, 44, 0, 6, 0)).toBe('Heroic victory');
    expect(levelOfNight(10, 44, 0, 6, 0)).toBe('Decisive victory');
    expect(levelOfNight(20, 44, 3, 6, 0)).toBe('Decisive victory');
    expect(levelOfNight(5, 44, 1, 6, 0)).toBe('Close victory');
    expect(levelOfNight(8, 44, 2, 4, 0)).toBe('Pyrrhic victory');
    expect(levelOfNight(0, 44, 0, 6, 0)).toBe('Decisive defeat');
    expect(levelOfNight(0, 44, 1, 6, 0)).toBe('Decisive defeat');
    expect(levelOfNight(0, 44, 3, 6, 0)).toBe('Crushing defeat');
    expect(levelOfNight(3, 44, 4, 15, 0)).toBe('Decisive defeat');
    expect(levelOfNight(8, 44, 3, 6, 0)).toBe('Close defeat');
    const levels = new Set<string>();
    for (const p of CONVOY_PRACTICE) for (const seed of [1, 2]) { const sim = new ConvoySim(convoyPracticeTicket(p, seed, null)); sim.runOut(); levels.add(sim.over!.level); }
    expect(levels.size).toBeGreaterThan(2);
  }, 120_000);

  it('says the hour of a hand-over the way the card does: minutes under an hour, the clock past it', () => {
    expect(afterDusk(30)).toBe('dusk');
    expect(afterDusk(60)).toBe('1 minute after dusk');
    expect(afterDusk(180)).toBe('3 minutes after dusk');
    expect(afterDusk(2 * 3600 + 56 * 60 + 10)).toBe('2:56 after dusk');
    const t = card(10, 7, kit(true, false, false), 4);
    const { sim } = convoyNight(t, 8);
    for (let k = 0; k < 60 && !sim.over; k++) sim.advance();
    sim.command({ do: 'dice' }, 'def');
    expect(sim.over!.lines).toContain('The staff fought the night from 3 minutes after dusk to dawn; these lines count the whole night.');
  });
});
