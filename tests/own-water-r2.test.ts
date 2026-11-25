// Release 2's own water (the sea field, src/battle/sea/sim.ts ownWater; data/battle/sea.json _about.torp, its last
// paragraph): captains mind their own side's torpedoes. A destroyer holds its salvo while a ship of its own would be in
// the tracks' way, and every ship with way on keeps out of the water ahead of a running salvo of her own side's. The
// switch is the flag 'ownWater' that ticket.ts ownWaterOn reads, set as the sailor rules' is (setOwnWater); the options
// page throws it with act('ownwaterrule:on' / ':off'), TC.ownWater(on) at any time; the view carries it as 'ownWater'.
// Nothing sets it on its own: off, every war is the war without it, to the byte, and the field is the field as before.
import { describe, expect, it } from 'vitest';
import { DT, type FieldEvent, type Side } from '../src/battle/core/types';
import { seaAhead } from '../src/battle/sea/handoff';
import { SEA } from '../src/battle/sea/numbers';
import { PRACTICE } from '../src/battle/sea/practice';
import { SeaSim } from '../src/battle/sea/sim';
import { ownWaterOn, plainTicket, setOwnWater, type PlainFleet } from '../src/battle/sea/ticket';
import type { SeaClass, SeaUnit } from '../src/battle/sea/types';
import { createGame } from '../src/campaign/game';
import { sail, seaBattles, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import { setVoyageLog, voyageOn } from '../src/campaign/voyage';
import { hashString } from '../src/core/rng';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (globalThis as unknown as { TC: Record<string, Call> }).TC;
const fp = (g: Game): string => hashString(saveGame(g)).toString(16);

/** A grand campaign as Germany, at the player's first month (as the voyage log's test seats it). */
function germany(): Game {
  const g = startOpen(createGame(3, 'GER'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
}
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;

describe("the own-water rule's switch", () => {
  it('is off in a new war, in the browser and through the bridge, and in a save from before it', () => {
    const g = germany();
    expect(g.flags.open).toBe(1);
    expect(g.flags.ownWater).toBeUndefined();
    expect(ownWaterOn(g)).toBe(false);
    TC.start({ view: 'grand', human: 'GER', seed: 7 });
    const text = (TC.save() as { text: string }).text;
    expect(JSON.parse(text).flags.ownWater).toBeUndefined();
    expect((TC.view() as { ownWater: boolean }).ownWater).toBe(false);
    // A grand campaign saved before the switch (no flag at all) plays on with it off.
    const old = JSON.parse(text);
    delete old.flags.ownWater;
    TC.load(JSON.stringify(old));
    expect((TC.view() as { ownWater: boolean }).ownWater).toBe(false);
    const back = loadGame(JSON.stringify(old))!;
    expect(back.flags.open).toBe(1);
    expect(ownWaterOn(back)).toBe(false);
  });

  it('setOwnWater turns it on and off, writing only its flag; off, the save is as one that never had it', () => {
    const g = germany();
    const before = saveGame(g);
    setOwnWater(g, true);
    expect(g.flags.ownWater).toBe(1);
    expect(ownWaterOn(g)).toBe(true);
    const on = JSON.parse(saveGame(g)), was = JSON.parse(before);
    expect(on.flags).toEqual({ ...was.flags, ownWater: 1 });
    expect({ ...on, flags: null }).toEqual({ ...was, flags: null });
    setOwnWater(g, false);
    expect(ownWaterOn(g)).toBe(false);
    expect('ownWater' in g.flags).toBe(false);
    expect(saveGame(g)).toBe(before);
  });

  it('outside a grand campaign it does what the voyage log\'s switch does: the flag is written, and nothing is on', () => {
    for (const nat of ['GER', null]) {
      const a = createGame(3, nat), b = createGame(3, nat);
      expect(a.flags.open).toBeUndefined();
      setOwnWater(a, true);
      setVoyageLog(b, true);
      expect(a.flags.ownWater).toBe(1);
      expect(b.flags.voyageLog).toBe(1);
      expect(ownWaterOn(a)).toBe(false);
      expect(voyageOn(b)).toBe(false);
      setOwnWater(a, false);
      setVoyageLog(b, false);
      expect('ownWater' in a.flags).toBe(false);
      expect('voyageLog' in b.flags).toBe(false);
    }
  });

  it("act('ownwaterrule:on' / ':off') switches it, says so, and the view carries it; TC.ownWater(on) too", () => {
    const g = germany();
    TC.load(saveGame(g));
    expect((TC.view() as { ownWater: boolean }).ownWater).toBe(false);
    const on = TC.act('ownwaterrule:on') as { ok: boolean; note: string; view: { ownWater: boolean } };
    expect(on.ok).toBe(true);
    expect(on.note).toBe("Friendly torpedoes are watched: in fleet actions fought live on the sea field, a destroyer holds its torpedoes while one of its own ships is in their way, and ships keep out of the way of their own side's torpedoes.");
    expect(on.view.ownWater).toBe(true);
    expect((TC.view() as { ownWater: boolean }).ownWater).toBe(true);
    expect(JSON.parse((TC.save() as { text: string }).text).flags.ownWater).toBe(1);
    const off = TC.act('ownwaterrule:off') as { ok: boolean; note: string; view: { ownWater: boolean } };
    expect(off.ok).toBe(true);
    expect(off.note).toBe('Friendly torpedoes are no longer watched.');
    expect(off.view.ownWater).toBe(false);
    expect(JSON.parse((TC.save() as { text: string }).text).flags.ownWater).toBeUndefined();
    expect(TC.ownWater(true)).toEqual({ on: true });
    expect((TC.view() as { ownWater: boolean }).ownWater).toBe(true);
    expect(TC.ownWater(false)).toEqual({ on: false });
    expect((TC.view() as { ownWater: boolean }).ownWater).toBe(false);
    // Outside a grand campaign it says why nothing changed (the switch is not shown there: r2Switches is false).
    TC.start({ human: 'GER', seed: 7 });
    const plain = TC.act('ownwaterrule:on') as { ok: boolean; note: string; view: { ownWater: boolean; r2Switches: boolean } };
    expect(plain.view.r2Switches).toBe(false);
    expect(plain.view.ownWater).toBe(false);
    expect(plain.note).toBe('Watching for friendly torpedoes works in the grand campaign only.');
  });

  it('a fleet action in a grand campaign carries the rule on its ticket only with the switch on', () => {
    const ticketOf = (on: boolean) => {
      const g = germany();
      setOwnWater(g, on);
      const norw = zoneOf(g, 'norw');
      expect(sail(g, fleet(g, 'UK', 'Home Fleet'), norw, 'patrol')).toBe(true);
      expect(sail(g, fleet(g, 'GER', 'Battle Fleet'), norw, 'patrol')).toBe(true);
      const before = saveGame(g);
      const ahead = seaAhead(g, (c) => { seaBattles(c, 'UK'); });
      expect(saveGame(g)).toBe(before);
      expect(ahead.length).toBe(1);
      return ahead[0].ticket;
    };
    const off = ticketOf(false), on = ticketOf(true);
    expect('ownWater' in off.place).toBe(false);
    expect(on.place.ownWater).toBe(true);
    // Nothing else on the ticket moves.
    const { ownWater, ...rest } = on.place;
    void ownWater;
    expect(JSON.stringify({ ...on, place: rest })).toBe(JSON.stringify(off));
  });

  it('thrown on and back off, a war of a few months is the same to the byte as one never switched', () => {
    const a = germany(), b = germany();
    setOwnWater(b, true);
    setOwnWater(b, false);
    expect(fp(b)).toBe(fp(a));
    for (let k = 0; k < 4; k++) { month(a); month(b); }
    expect(a.turn).toBeGreaterThan(4);
    expect(fp(b)).toBe(fp(a));
    expect(saveGame(b)).toBe(saveGame(a));
  });
});

// ---- On the field -----------------------------------------------------------------------------------------------------

const LINE_A: PlainFleet = { name: 'Home Fleet', nat: 'UK', ships: { bb: 2, ca: 3, dd: 8 } };
const LINE_B: PlainFleet = { name: 'Battle Fleet', nat: 'GER', ships: { bb: 2, ca: 3, dd: 12 } };
const live = (u: SeaUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
const all = (s: any, side: Side, cls: string): SeaUnit[] => s.units.filter((u: SeaUnit) => u.side === side && u.extra.cls === cls);
/** Only these pieces on the field, where they are put (and on the heading given); the rest gone. */
const only = (s: any, keep: [SeaUnit, number, number, number?][]): void => {
  for (const u of s.units as SeaUnit[]) if (!keep.some(([k]) => k === u)) u.state = 'gone';
  for (const [u, x, y, face] of keep) { u.x = x; u.y = y; if (face !== undefined) u.face = face; }
};
/** A destroyer piece's full salvo: its bank at the cap (torp.bank seconds of its guns' weight now). */
const salvo = (s: any, u: SeaUnit): number => SEA.fire.rate * s.output(u) * s.day(u.side) * SEA.torp.bank;
/** The signed turn from a to b, + to starboard. */
const off = (a: number, b: number): number => Math.atan2(Math.sin(b - a), Math.cos(b - a));
/** The marks the own-water rule takes, and only those: holds, and turns to keep clear (with their seconds). */
const OWN = /^(att|def):(torpHeld|clear)(:s|>[a-z]+)?$/;
const ownMarks = (s: SeaSim): string[] => Object.keys(s.tally).filter((k) => OWN.test(k));
const practice = (name: string) => PRACTICE.find((p) => p.name === name)!;
const practiceSim = (name: string, seed: number, ownWater?: boolean): SeaSim => {
  const p = practice(name);
  return new SeaSim(plainTicket({ att: p.att, def: p.def, air: p.air, seed, human: null, zoneName: p.sea, title: p.name, ...(ownWater === undefined ? {} : { ownWater }) }));
};
/** The whole action, every event in order. */
function fight(s: SeaSim): FieldEvent[] {
  const ev: FieldEvent[] = [];
  for (let i = 0; i < 1e5 && !s.over; i++) { s.step(); ev.push(...s.view().events); }
  return ev;
}

/**
 * The measure, kept apart from the rule's own code: where the heads of every running salvo are and where every ship of
 * its side lies, tick by tick, and each time the heads come level with a point of one of them. A piece's ships are laid
 * as the 3D scene lays them, from our own record of the piece's places each tick: her leader at the piece's place on
 * its heading, the rest astern along the way it came, three of her lengths apart, the column drawn up straight astern
 * of the leader on the heading she started with. Each ship is looked at at her bow, her middle and her stern. A salvo
 * runs from the launcher's place (as its launch event gives it) to its aim point in its running time; its water is the
 * fan from there, as wide as the launcher's beam at the tubes, torp.radius more either side at the aim point, and a
 * ship's point counts as crossed when the heads pass it inside the fan widened by half her beam, between the tubes and
 * the aim point. The launcher's own ships are not counted.
 */
interface Crossing { t: number; from: number; t0: number; unit: number; ship: number; at: 'bow' | 'middle' | 'stern'; cls: string; a: number; b: number; room: number }
interface Running { from: number; side: Side; t0: number; x0: number; y0: number; L: number; ux: number; uy: number; T: number; tubes: number; ahead: Map<number, number> }
function watch(s: SeaSim): { after: () => FieldEvent[]; found: Crossing[] } {
  const wakes = new Map<number, { x: number[]; y: number[]; run: number[] }>();
  const lenOf = (u: SeaUnit): number => SEA.classes[u.extra.cls as SeaClass].len;
  const lay = (): void => {
    for (const u of s.units) {
      if (!live(u)) continue;
      let w = wakes.get(u.id);
      if (!w) {
        // Drawn up in line ahead: the column runs straight astern of her leader on her heading.
        const back = 3 * lenOf(u) * u.extra.ships0 + lenOf(u);
        w = { x: [u.x - Math.sin(u.face) * back], y: [u.y - Math.cos(u.face) * back], run: [0] };
        wakes.set(u.id, w);
      }
      const i = w.x.length - 1, d = Math.hypot(u.x - w.x[i], u.y - w.y[i]);
      if (d > 1e-9) { w.x.push(u.x); w.y.push(u.y); w.run.push(w.run[i] + d); }
    }
  };
  /** Her ships, leader first, each as [x, y, heading]. */
  const ships = (u: SeaUnit): [number, number, number][] => {
    const w = wakes.get(u.id)!, gap = 3 * lenOf(u), last = w.x.length - 1, out: [number, number, number][] = [[u.x, u.y, u.face]];
    for (let k = 1; k < u.extra.ships; k++) {
      const want = w.run[last] - k * gap;
      let lo = 0, hi = last; // the last point at or before 'want' along the way she came
      while (hi - lo > 1) { const m = (lo + hi) >> 1; if (w.run[m] <= want) lo = m; else hi = m; }
      const f = (want - w.run[lo]) / Math.max(1e-9, w.run[lo + 1] - w.run[lo]);
      const dx = w.x[lo + 1] - w.x[lo], dy = w.y[lo + 1] - w.y[lo];
      out.push([w.x[lo] + dx * f, w.y[lo] + dy * f, Math.atan2(dx, dy)]);
    }
    return out;
  };
  const AT = ['bow', 'middle', 'stern'] as const, ALONG = [0.5, 0, -0.5];
  const runs: Running[] = [], found: Crossing[] = [];
  lay();
  const after = (): FieldEvent[] => {
    lay();
    const ev = s.view().events;
    for (const e of ev) {
      if (e.what !== 'sea:torpedo') continue;
      const x = e.extra as { x: number; y: number; running: number }, by = s.units.find((u) => u.id === e.unit)!;
      const L = Math.hypot(x.x - e.x!, x.y - e.y!);
      if (L < 1) continue;
      runs.push({ from: e.unit!, side: e.side!, t0: e.t, x0: e.x!, y0: e.y!, L, ux: (x.x - e.x!) / L, uy: (x.y - e.y!) / L, T: x.running, tubes: by.w / 2, ahead: new Map() });
    }
    for (let r = runs.length - 1; r >= 0; r--) if (s.t > runs[r].t0 + runs[r].T + 1e-9) runs.splice(r, 1);
    if (!runs.length) return ev;
    const laid = new Map<number, [number, number, number][]>();
    for (const sv of runs) {
      const h = (sv.L * (s.t - sv.t0)) / sv.T;
      for (const u of s.units) {
        if (u.side !== sv.side || u.id === sv.from || !live(u)) continue;
        if (!laid.has(u.id)) laid.set(u.id, ships(u));
        laid.get(u.id)!.forEach(([hx, hy, hf], k) => {
          for (let j = 0; j < 3; j++) {
            const px = hx + Math.sin(hf) * ALONG[j] * lenOf(u), py = hy + Math.cos(hf) * ALONG[j] * lenOf(u);
            const a = (px - sv.x0) * sv.ux + (py - sv.y0) * sv.uy, b = (px - sv.x0) * sv.uy - (py - sv.y0) * sv.ux;
            const key = u.id * 64 + k * 3 + j, was = sv.ahead.get(key);
            sv.ahead.set(key, a - h);
            if (was === undefined || !(was > 0 && a - h <= 0) || a < 0 || a > sv.L) continue;
            const room = (SEA.torp.radius * a) / sv.L + sv.tubes + u.w / 2;
            if (Math.abs(b) <= room) found.push({ t: s.t, from: sv.from, t0: sv.t0, unit: u.id, ship: k, at: AT[j], cls: u.extra.cls, a: Math.round(a), b: Math.round(b), room: Math.round(room) });
          }
        });
      }
    }
    return ev;
  };
  return { after, found };
}
/** The whole action under the measure: every crossing, and the salvoes launched. */
function crossings(s: SeaSim): { found: Crossing[]; salvoes: number } {
  const w = watch(s);
  let salvoes = 0;
  for (let i = 0; i < 1e5 && !s.over; i++) { s.step(); salvoes += w.after().filter((e) => e.what === 'sea:torpedo').length; }
  return { found: w.found, salvoes };
}

describe('off, the field is the field as before', () => {
  for (const name of ['Two battle lines', 'Light ships against battleships']) {
    it(`${name}: without the rule on the ticket and with it set false, the same action event for event, and no ship minds her own side's torpedoes`, () => {
      for (const seed of [1, 2, 3]) {
        expect('ownWater' in plainTicket({ att: practice(name).att, def: practice(name).def, seed, ownWater: false }).place).toBe(false);
        const a = practiceSim(name, seed), b = practiceSim(name, seed, false);
        const e0 = fight(a), e1 = fight(b);
        expect(e0.some((e) => e.what === 'sea:torpedo'), `${seed}`).toBe(true); // salvoes ran: there was something to mind
        expect(JSON.stringify(e1)).toBe(JSON.stringify(e0));
        expect(b.over).toEqual(a.over);
        expect(b.tally).toEqual(a.tally);
        expect(JSON.stringify(b.units)).toBe(JSON.stringify(a.units));
        // No hold, no turn to keep clear: nothing of the rule is marked, kept or laid.
        for (const s of [a, b] as any[]) {
          expect(ownMarks(s)).toEqual([]);
          expect(s.trail.size).toBe(0);
          expect(s.clearing.size).toBe(0);
          expect(s.holding.size).toBe(0);
        }
      }
    }, 60_000);
  }
});

describe('a destroyer holds its torpedoes while a ship of its own is in their way', () => {
  /** A destroyer piece on a run, inside its press range of an enemy battleship lying stopped, and a cruiser of its own
   *  lying stopped between them; the guns have met. */
  const setUp = (on: boolean) => {
    const s = new SeaSim(plainTicket({ att: [LINE_A], def: [LINE_B], seed: 11, human: null, ownWater: on })) as any;
    s.contactAt = 0;
    const dd = all(s, 'att', 'dd').find((u) => !s.screen.has(u.id))!, ebb = all(s, 'def', 'bb')[0], ca = all(s, 'att', 'ca')[0];
    only(s, [[dd, 10000, 7600, 0], [ebb, 10000, 10000, Math.PI], [ca, 10000, 8800, 0]]);
    ebb.extra.way = 0; ca.extra.way = 0;
    s.bank[dd.id] = salvo(s, dd);
    s.beginRun(dd, ebb);
    expect(Math.hypot(dd.x - ebb.x, dd.y - ebb.y)).toBeLessThan(SEA.styles.methodical!.press);
    return { s, dd, ebb, ca };
  };
  const launches = (ev: FieldEvent[], id: number): FieldEvent[] => ev.filter((e) => e.what === 'sea:torpedo' && e.unit === id);

  it('on a run it holds and keeps closing at flank while a cruiser of its own lies between it and its target, and launches the first second she is out of the way', () => {
    const { s, dd, ebb, ca } = setUp(true);
    const flank = SEA.classes.dd.speed * SEA.speeds.flank, d0 = Math.hypot(dd.x - ebb.x, dd.y - ebb.y);
    for (let i = 0; i < 30; i++) {
      // She lies where she is, halfway between them.
      ca.x = (dd.x + ebb.x) / 2; ca.y = (dd.y + ebb.y) / 2; ca.extra.way = 0;
      s.step();
      expect(launches(s.view().events, dd.id), `${s.t}`).toEqual([]);
      expect(s.torps).toHaveLength(0);
      expect(s.runs.get(dd.id)?.phase).toBe('in');
    }
    expect(s.tally['att:torpHeld']).toBe(1); // one hold, however long
    expect(s.tally['att:torpHeld:s']).toBeCloseTo(30 * DT, 6);
    expect(s.holding.has(dd.id)).toBe(true);
    expect(dd.extra.way).toBeGreaterThan(SEA.classes.dd.speed * SEA.speeds.cruise); // working up to flank
    expect(dd.extra.way).toBeLessThanOrEqual(flank);
    expect(Math.hypot(dd.x - ebb.x, dd.y - ebb.y)).toBeLessThan(d0); // still closing on her
    // She moves off out of the tracks' way: it launches at once.
    ca.x = dd.x + 3000; ca.y = dd.y;
    s.step();
    const went = launches(s.view().events, dd.id);
    expect(went).toHaveLength(1);
    expect((went[0].extra as { target: number }).target).toBe(ebb.id);
    expect(s.torps).toHaveLength(1);
    expect(s.runs.get(dd.id)?.phase).toBe('away');
    expect(s.holding.has(dd.id)).toBe(false);
    expect(s.tally['att:torpHeld']).toBe(1);
    expect(s.tally['att:runs']).toBe(1);
  });

  it('with the rule off the same destroyer launches at once through her', () => {
    const { s, dd } = setUp(false);
    s.step();
    expect(launches(s.view().events, dd.id)).toHaveLength(1);
    expect(s.torps).toHaveLength(1);
    expect(ownMarks(s)).toEqual([]);
  });

  it('from the screen\'s station it holds too, looking again each second, and launches once the way is clear', () => {
    for (const on of [true, false]) {
      const s = new SeaSim(plainTicket({ att: [LINE_A], def: [LINE_B], seed: 11, human: null, ownWater: on })) as any;
      s.contactAt = 0;
      const sdd = all(s, 'att', 'dd').find((u) => s.screen.has(u.id))!, g = s.guarded(sdd) as SeaUnit;
      const ebb = all(s, 'def', 'bb')[0], edd = all(s, 'def', 'dd')[0], ca = all(s, 'att', 'ca').find((u) => u !== g)!;
      // (An enemy destroyer still afloat far off, so the screen's job is not done.)
      only(s, [[sdd, 10000, 10000, 0], [g, 10000, 9000, 0], [ebb, 10000, 12000, Math.PI], [ca, 10000, 11000, 0], [edd, 2000, 18000]]);
      ebb.extra.way = 0; ca.extra.way = 0;
      expect(s.bank[sdd.id]).toBeGreaterThan(0);
      s.startRuns();
      s.startRuns();
      if (!on) { expect(s.torps).toHaveLength(1); expect(ownMarks(s)).toEqual([]); continue; }
      expect(s.torps).toHaveLength(0);
      expect(launches(s.view().events, sdd.id)).toEqual([]);
      expect(s.tally['att:torpHeld']).toBe(1);
      expect(s.tally['att:torpHeld:s']).toBe(2); // a look a second
      ca.x = 14000;
      s.startRuns();
      expect(s.torps).toHaveLength(1);
      expect(s.torps[0].from).toBe(sdd.id);
      expect(launches(s.view().events, sdd.id)).toHaveLength(1);
      expect(s.tally['att:screenLaunch']).toBe(1);
      expect(s.holding.has(sdd.id)).toBe(false);
    }
  });
});

describe("every ship keeps clear of her own side's running torpedoes", () => {
  /** A salvo running due north at an enemy battleship lying stopped, and a cruiser of its side 600 m to one side of the
   *  tracks, halfway along them and well ahead of the heads, sent by the player to a point on the far side of them, so
   *  her course carries her in across them. (Fighting at will, the measure below has her in every action.) */
  const play = (on: boolean, hand: 1 | -1) => {
    const s = new SeaSim(plainTicket({ att: [LINE_A], def: [LINE_B], seed: 11, human: null, ownWater: on })) as any;
    const dd = all(s, 'att', 'dd')[0], ebb = all(s, 'def', 'bb')[0], ca = all(s, 'att', 'ca')[0];
    only(s, [[dd, 10000, 8000, 0], [ebb, 10000, 14000, 0], [ca, 10000 + hand * 600, 11000, -hand * Math.PI / 4]]);
    ebb.extra.way = 0;
    expect(s.command({ do: 'move', units: [ca.id], x: 10000 - hand * 1400, y: 12600 }, 'att')).toBe(true);
    const w = watch(s);
    s.bank[dd.id] = salvo(s, dd);
    s.launch(dd, ebb, null);
    const tp = s.torps[0], square = tp.dir + (hand * Math.PI) / 2;
    expect(tp.dir).toBe(0);
    const faces: number[] = [ca.face], held: boolean[] = [];
    while (s.t < tp.at + 1) { s.step(); w.after(); faces.push(ca.face); held.push(s.clearing.get(ca.id) === tp); }
    return { s, ca, dd, tp, square, faces, held, crossed: w.found.filter((c) => c.unit === ca.id) };
  };

  for (const hand of [1, -1] as const) {
    const side = hand > 0 ? 'starboard' : 'port';
    it(`a cruiser on the tracks' ${side} hand, sent across them ahead of the heads, turns square off them to ${side} until they are past her; with the rule off she steams on into them`, () => {
      const on = play(true, hand);
      expect(on.held[0]).toBe(true); // from the first tick after the launch
      expect(on.s.tally['att:clear']).toBe(1);
      expect(on.s.tally['att:clear>ca']).toBe(1);
      expect(on.s.tally['att:clear:s']).toBeGreaterThan(5);
      // Her heading swings toward square off the tracks on her own hand, and comes onto it while she holds it.
      const start = Math.abs(off(on.faces[0], on.square));
      expect(start).toBeCloseTo((3 * Math.PI) / 4, 6);
      expect(Math.abs(off(on.faces[100], on.square))).toBeLessThan(start - 0.5);
      const last = on.held.lastIndexOf(true);
      expect(Math.abs(off(on.faces[last + 1], on.square))).toBeLessThan(0.05);
      // She lets it go before it lands, once the heads are past her (then her order has her again), and the heads
      // never cross her.
      expect(on.tp.t0 + (last + 2) * DT).toBeLessThan(on.tp.at - 1);
      expect(on.s.clearing.has(on.ca.id)).toBe(false);
      expect(on.s.orders.get(on.ca.id)?.kind).toBe('move');
      expect(on.crossed).toEqual([]);
      // The launcher keeps clear of nothing of its own.
      expect(on.s.clearing.has(on.dd.id)).toBe(false);
      // Off, she keeps the course she was sent on, never nearer square off than she began, and the heads cross her.
      const was = play(false, hand);
      expect(was.held.some((h) => h)).toBe(false);
      for (const f of was.faces) expect(Math.abs(off(f, was.square))).toBeGreaterThan(start - 0.2);
      expect(was.crossed.length).toBeGreaterThan(0);
      expect(ownMarks(was.s)).toEqual([]);
    });
  }

  /** Two salvoes launched together running due north side by side, 1,300 m apart, each at an enemy battleship lying
   *  stopped, and a cruiser between them halfway along, sent to a point: across the western one (heading north-west),
   *  or straight on up the water between them. */
  const between = (on: boolean, across: boolean) => {
    const s = new SeaSim(plainTicket({ att: [LINE_A], def: [LINE_B], seed: 11, human: null, ownWater: on })) as any;
    const [d1, d2] = all(s, 'att', 'dd'), [e1, e2] = all(s, 'def', 'bb'), ca = all(s, 'att', 'ca')[0];
    only(s, [[d1, 10000, 8000, 0], [d2, 11300, 8000, 0], [e1, 10000, 14000, 0], [e2, 11300, 14000, 0], [ca, 10650, 11000, across ? -Math.PI / 4 : 0]]);
    e1.extra.way = 0; e2.extra.way = 0;
    expect(s.command({ do: 'move', units: [ca.id], ...(across ? { x: 8600, y: 12600 } : { x: 10650, y: 16000 }) }, 'att')).toBe(true);
    const w = watch(s);
    for (const [d, e] of [[d1, e1], [d2, e2]]) { s.bank[d.id] = salvo(s, d); s.launch(d, e, null); }
    const [west, east] = s.torps;
    const held: (0 | 1 | null)[] = [];
    while (s.t < west.at + 1) { s.step(); w.after(); const c = s.clearing.get(ca.id); held.push(c === west ? 0 : c === east ? 1 : null); }
    return { s, west, east, held, crossed: w.found.filter((c) => c.unit === ca.id) };
  };

  it('between two salvoes running side by side she takes up the one her course would carry her across; sent straight on up the water between them, the heads of neither cross her', () => {
    const across = between(true, true);
    expect([across.west.dir, across.east.dir]).toEqual([0, 0]);
    expect(across.held[0]).toBe(0);
    expect(across.s.tally['att:clear']).toBe(1);
    for (const on of [true, false]) {
      const up = between(on, false);
      expect(up.held.every((h) => h === null)).toBe(true);
      expect(up.crossed).toEqual([]);
    }
  });

  // Found while writing these tests: holding one salvo she once looked at no other, so her turn square off the western
  // salvo's tracks carried her east into the water of the other. She now takes the first course no own salvo's heads
  // would cross (clearAway: here, up the clear water between them) and steadies on it until the salvo is past.
  it('sent across one of two salvoes running side by side, her turn to keep clear of it does not carry her into the other', () => {
    expect(between(true, true).crossed).toEqual([]);
  });
});

describe('the measure: no ship of a side in the way of its own torpedoes', () => {
  for (const name of ['Light ships against battleships', 'Two battle lines']) {
    it(`${name}, seeds 1 to 10: with the rule on the heads of no running salvo cross a ship of its own side; off, they do`, () => {
      let salvoes = 0;
      for (let seed = 1; seed <= 10; seed++) {
        const s = practiceSim(name, seed, true);
        const r = crossings(s);
        salvoes += r.salvoes;
        expect(r.found, `seed ${seed}`).toEqual([]);
        expect(ownMarks(s).length, `seed ${seed}`).toBeGreaterThan(0); // the rule had work to do
      }
      expect(salvoes).toBeGreaterThan(20);
      // Off, the same measure finds a crossing: it can fail.
      let seen = 0;
      for (let seed = 1; seed <= 10 && !seen; seed++) seen = crossings(practiceSim(name, seed)).found.length;
      expect(seen).toBeGreaterThan(0);
    }, 180_000);
  }
});

describe('the rule draws no dice', () => {
  for (const [name, seed] of [['Light ships against battleships', 4], ['Two battle lines', 2]] as const) {
    it(`${name}, seed ${seed}: with the rule on the same action plays out the same twice, event for event`, () => {
      const a = practiceSim(name, seed, true), b = practiceSim(name, seed, true);
      const e0 = fight(a), e1 = fight(b);
      expect(ownMarks(a).length).toBeGreaterThan(0);
      expect(JSON.stringify(e1)).toBe(JSON.stringify(e0));
      expect(b.over).toEqual(a.over);
      expect(b.tally).toEqual(a.tally);
      expect(JSON.stringify(b.units)).toBe(JSON.stringify(a.units));
    }, 60_000);
  }
});
