// How an aircraft that is hit is seen to go, from its situation (Brian, 20 November 2026, after the Pearl Harbor film:
// "i want many different ways a plan can go down, a another plane falling hitting another one, a win gets blown off
// ect. not all planes that get hit go down"; "i just dont want it to be the same visual effects each time"; "it
// should be variables and different situations"). Shown, never counted: the counts, the pilots back and the bombs are
// what they were whatever the look tables say.
import { describe, expect, it, vi } from 'vitest';
import { planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import type { FieldEvent, FieldTicket } from '../src/battle/core/types';
import { SKY } from '../src/battle/air/book';
import { fateOf, freshMemory, HOWS, seaAt, weightsOf, type Scene } from '../src/battle/air/fates';
import { airBattle, AirSim } from '../src/battle/air/sim';
import { airTicket } from '../src/battle/air/ticket';
import type { AirPlace, Fall } from '../src/battle/air/types';
import { Rng } from '../src/core/rng';

type Ticket = FieldTicket<AirPlace>;
const WARS: Record<string, string[]> = { GER: ['UK', 'FRA'], UK: ['GER'], FRA: ['GER'] };

/** The staff's raid on a named province with every fighter group that can go by day (tests/battle-air-styles.test.ts). */
function raid(nat: string, kind: RaidKind, name: string, turn = 0): Ticket {
  const g: Game = startOpen(createGame(7, nat));
  for (const w of WARS[nat]) setWar(g, nat, w);
  if (turn) g.turn = turn;
  const t = raidTargets(g, kind, nat).find((x) => planRaid(g, kind, x.id, undefined, nat).place === name);
  if (!t) throw new Error(`no ${kind} raid on ${name} for ${nat}`);
  const day = planRaid(g, kind, t.id, undefined, nat);
  const ids = [...day.picked, ...day.seats.filter((s) => !s.why && !s.picked && s.kind === 'ftr').map((s) => s.id)];
  return airTicket(g, kind, t.id, ids, nat).ticket!;
}

function fight(tk: Ticket, k: number, show = true): AirSim {
  const s = airBattle({ ...tk, seed: tk.seed + k }, { show });
  s.ai.att = true; s.ai.def = true;
  while (!s.over) s.step();
  return s;
}

const RAIDS: [string, RaidKind, string, number][] = [['GER', 'bomb', 'Alsace', 0], ['GER', 'bomb', 'Kent', 0], ['FRA', 'bomb', 'Rhineland', 30], ['UK', 'bomb', 'Ruhr', 30]];
const RUNS = 8;
const tickets = RAIDS.map(([nat, kind, name, turn]) => raid(nat, kind, name, turn));
const lostOf = (h: FieldEvent[]) => h.filter((e) => e.what === 'air:lost');
const fallOf = (e: FieldEvent) => e.extra as unknown as Fall & { by?: number | 'flak' | null; strag?: number; air: string };

describe('how a loss goes never moves a count (ruling 237)', () => {
  it('with no way down drawn at all, and no hit shown, every battle ends the same: the looks touch none of the rules\' dice', () => {
    const runs = tickets.map((tk) => [0, 1, 2].map((k) => fight(tk, k)));
    const sim = AirSim.prototype as unknown as { fated: () => void; hitShown: () => void };
    const a = vi.spyOn(sim, 'fated').mockImplementation(() => {});
    const b = vi.spyOn(sim, 'hitShown').mockImplementation(() => {});
    try {
      tickets.forEach((tk, i) => [0, 1, 2].forEach((k) => {
        const s = fight(tk, k);
        expect(s.outcome()).toEqual(runs[i][k].outcome());
        expect(s.over!.lines).toEqual(runs[i][k].over!.lines);
        expect(lostOf(s.history())).toHaveLength(0);
        expect(lostOf(runs[i][k].history()).length).toBeGreaterThan(0);
      }));
    } finally { a.mockRestore(); b.mockRestore(); }
  });

  it('with every look table turned upside down, every battle ends the same: counts, bombs, pilots back, when each loss falls', () => {
    const base = structuredClone(SKY.fate.base);
    const runs = tickets.map((tk) => [0, 1, 2].map((k) => fight(tk, k)));
    try {
      SKY.fate.base.ftr = [...base.ftr].reverse();
      SKY.fate.base.bmr = [...base.bmr].reverse();
      tickets.forEach((tk, i) => [0, 1, 2].forEach((k) => {
        const a = runs[i][k], b = fight(tk, k);
        expect(b.outcome()).toEqual(a.outcome());
        expect(b.over!.lines).toEqual(a.over!.lines);
        const when = (s: AirSim) => lostOf(s.history()).map((e) => [e.t, e.unit, fallOf(e).back, fallOf(e).by]);
        expect(when(b)).toEqual(when(a));
        expect(lostOf(b.history()).map((e) => fallOf(e).how)).not.toEqual(lostOf(a.history()).map((e) => fallOf(e).how));
      }));
    } finally {
      SKY.fate.base.ftr = base.ftr; SKY.fate.base.bmr = base.bmr;
    }
  });
});

describe('many ways down, never the same twice running (Brian: "i just dont want it to be the same visual effects each time")', () => {
  const all: FieldEvent[][] = tickets.flatMap((tk) => Array.from({ length: RUNS }, (_, k) => fight(tk, k).history()));

  it('every way a weighted draw can give is seen, and none is more than a quarter of the losses', () => {
    const n: Record<string, number> = {};
    let total = 0;
    for (const h of all) for (const e of lostOf(h)) { n[fallOf(e).how!] = (n[fallOf(e).how!] ?? 0) + 1; total++; }
    for (const h of HOWS) expect(n[h] ?? 0, h).toBeGreaterThan(0);
    expect(n.struck ?? 0).toBeGreaterThan(0);
    for (const [h, v] of Object.entries(n)) expect(v / total, h).toBeLessThan(0.25);
  });

  it('a drawn way is never the one shown just before it in the same battle', () => {
    let pairs = 0;
    for (const h of all) {
      const ways = lostOf(h).map((e) => fallOf(e).how!);
      for (let i = 1; i < ways.length; i++) {
        if (ways[i] === 'struck' || ways[i] === 'collide') continue;
        expect(ways[i]).not.toBe(ways[i - 1]);
        pairs++;
      }
    }
    expect(pairs).toBeGreaterThan(500);
  });

  it('each loss says how it goes in its words, and only a way it can be seen: a saved pilot always under a parachute, never blown up', () => {
    for (const h of all) for (const e of lostOf(h)) {
      const f = fallOf(e);
      expect(f.how).toBeDefined();
      expect(f.dur!).toBeGreaterThan(0);
      if (f.back) { expect((SKY.fate.saved as readonly string[]).includes(f.how!) || f.how === 'collide').toBe(true); expect(f.chutes).toBe(1); }
      if (f.how === 'explode') { expect(f.end).toBe('air'); expect(f.chutes).toBe(0); }
      if (f.ditch) { expect(f.how).toBe('glide'); expect(f.end).toBe('sea'); }
      const until = f.boom ?? f.dur!;
      for (const t of f.chuteAt ?? []) expect(t).toBeLessThanOrEqual(until);
      expect((f.chuteAt ?? []).length).toBe(f.chutes);
      if (f.how !== 'collide') expect(e.text.length, JSON.stringify(f)).toBeGreaterThan(10);
    }
  });
});

describe('a falling aircraft strikes another; a fighter collides with the bomber it attacked (Brian: "a another plane falling hitting another one")', () => {
  const all: FieldEvent[][] = tickets.flatMap((tk) => Array.from({ length: RUNS }, (_, k) => fight(tk, k).history()));

  it('a struck bomber names a loss of its own box moments before, or a fighter attacking it, never a straggler; each loss strikes at most one other', () => {
    let seen = 0;
    for (const h of all) {
      const lost = lostOf(h);
      const used = new Set<number>();
      for (const e of lost) {
        const f = fallOf(e);
        if (f.how !== 'struck') continue;
        seen++;
        const by = lost.find((x) => fallOf(x).fid === f.with)!;
        expect(by, 'the loss that struck it').toBeDefined();
        if (fallOf(by).air === 'ftr') expect(by.side).not.toBe(e.side);
        else expect(by.unit).toBe(e.unit);
        expect(e.t - by.t).toBeGreaterThanOrEqual(0);
        expect(e.t - by.t).toBeLessThanOrEqual(SKY.fate.near.t);
        expect(fallOf(by).strag).toBeUndefined();
        expect(f.strag).toBeUndefined();
        expect(used.has(f.with!)).toBe(false);
        used.add(f.with!);
        expect(f.cause).toBe(fallOf(by).how === 'explode' || fallOf(by).how === 'breakup' ? 'debris' : 'falling');
      }
    }
    expect(seen).toBeGreaterThan(5);
  });

  it('a collision is two losses of the two sides at the same moment, each naming the other, told once', () => {
    const was = { ...SKY.fate.ram };
    // Head-on collisions are rare: made common here so the test sees enough of them.
    Object.assign(SKY.fate.ram, { headon: 1, other: 0.5, tangle: 0.3 });
    let pairs = 0;
    try {
      for (const tk of tickets) for (let k = 0; k < RUNS; k++) {
        const s = fight(tk, k);
        const lost = lostOf(s.history());
        for (const e of lost) {
          const f = fallOf(e);
          if (f.how !== 'collide') continue;
          const o = lost.find((x) => fallOf(x).fid === f.with)!;
          expect(fallOf(o).how).toBe('collide');
          expect(fallOf(o).with).toBe(f.fid);
          expect(o.t).toBe(e.t);
          expect(o.side).not.toBe(e.side);
          expect([e.text, o.text].filter(Boolean).length).toBe(1);
          if (e.text) expect(e.text).toMatch(/collides with/);
          pairs++;
        }
      }
    } finally { Object.assign(SKY.fate.ram, was); }
    expect(pairs).toBeGreaterThanOrEqual(2);
  });
});

/** A scene to draw from: a bomber of a twin-engined box at middle height, over land, hit by a fighter, all else plain. */
const scene = (o: Partial<Scene> = {}): Scene => ({
  air: 'bmr', engines: 2, nat: 'USA', model: 'Bomber:USA', when: { year: 1942, month: 6 }, gun: 'hmg', flak: null, pass: null,
  band: 1, alt: 4_500, loaded: false, strag: null, sea: false, back: false, over: 'enemy', crew: 5, near: null, ...o,
});
function shares(sc: Scene, n = 4_000): Record<string, number> {
  const rng = new Rng(11), out: Record<string, number> = {};
  for (let i = 0; i < n; i++) { const f = fateOf(rng, sc, freshMemory()); out[f.how] = (out[f.how] ?? 0) + 1 / n; }
  return out;
}

describe('the situation decides (Brian: "it should be variables and different situations")', () => {
  it('Japan\'s unprotected tanks burn: far more flamers than an American bomber in the same spot', () => {
    expect(shares(scene({ nat: 'JAP', model: 'G4M' })).flamer).toBeGreaterThan(2 * shares(scene()).flamer);
  });
  it('head-on, the cockpit is hit; from astern, the tail', () => {
    const h = shares(scene({ pass: 'headon' })), a = shares(scene({ pass: 'astern' }));
    expect(h.pilot).toBeGreaterThan(2 * a.pilot);
    expect(a.tail).toBeGreaterThan(2 * h.tail);
  });
  it('cannon shells take wings off; rifle-calibre guns seldom do', () => {
    expect(shares(scene({ gun: 'cannon20' })).wing).toBeGreaterThan(2 * shares(scene({ gun: 'rifle' })).wing);
  });
  it('a bomber with its bombs aboard blows up far more often than one that has dropped; a heavy flak shell more than a light one', () => {
    expect(shares(scene({ loaded: true })).explode).toBeGreaterThan(2 * shares(scene()).explode);
    expect(shares(scene({ gun: null, flak: 'heavy' })).explode).toBeGreaterThan(2 * shares(scene({ gun: null, flak: 'light' })).explode);
  });
  it('low down it glides in, and over the sea it ditches; high up it spins', () => {
    const lo = shares(scene({ band: 0, alt: 1_800 })), hi = shares(scene({ band: 2, alt: 7_200 }));
    expect(lo.glide).toBeGreaterThan(1.5 * hi.glide);
    expect(hi.spin).toBeGreaterThan(1.5 * lo.spin);
    const rng = new Rng(5);
    for (let i = 0; i < 500; i++) {
      const sea = fateOf(rng, scene({ sea: true }), freshMemory()), land = fateOf(rng, scene(), freshMemory());
      if (sea.how !== 'explode') expect(sea.end).toBe('sea');
      if (land.how !== 'explode') expect(land.end).toBe('land');
      expect(land.ditch).toBeUndefined();
      if (sea.how === 'glide') expect(sea.ditch).toBe(true);
    }
  });
  it('a straggler already on fewer engines burns or glides in far more often, from the engine it lost', () => {
    const s = shares(scene({ strag: [1] })), p = shares(scene());
    expect((s.burn ?? 0) + (s.glide ?? 0)).toBeGreaterThan(1.5 * ((p.burn ?? 0) + (p.glide ?? 0)));
    const rng = new Rng(3);
    for (let i = 0; i < 300; i++) expect(fateOf(rng, scene({ strag: [2] }), freshMemory()).engineAt).toBeUndefined();
  });
  it('a pilot the rules save gets out under one parachute, from a way he can get out of', () => {
    const rng = new Rng(9);
    for (let i = 0; i < 500; i++) {
      const f = fateOf(rng, scene({ air: 'ftr', engines: 1, back: true, over: 'own', crew: 1 }), freshMemory());
      expect(SKY.fate.saved as readonly string[]).toContain(f.how);
      expect(f.chutes).toBe(1);
    }
  });
  it('the last way shown is never drawn next, unless it is the only way left', () => {
    const mem = freshMemory(), rng = new Rng(21);
    let last = '';
    for (let i = 0; i < 2_000; i++) {
      const f = fateOf(rng, scene({ gun: i % 2 ? 'rifle' : 'cannon20' }), mem);
      expect(f.how).not.toBe(last);
      last = f.how;
    }
    const only = { ...scene({ air: 'ftr', back: true, over: 'own' }) };
    const w = weightsOf(only, { last: ['burn'], looks: {} });
    expect(w[HOWS.indexOf('burn')]).toBe(0);
  });
});

describe('not every aircraft that is hit goes down (Brian: "not all planes that get hit go down")', () => {
  const runs = tickets.flatMap((tk) => Array.from({ length: RUNS }, (_, k) => fight(tk, k)));

  it('hits it survives: holes, smoke, pieces, a bomber\'s engine fire put out; at most one a group each few seconds', () => {
    const marks = new Set<string>();
    for (const s of runs) {
      const last = new Map<number, number>();
      for (const e of s.history().filter((x) => x.what === 'air:hit')) {
        const x = e.extra as { mark: string; air: string; for: number };
        marks.add(x.mark);
        if (x.mark === 'fireout') expect(x.air).toBe('bmr');
        if (last.has(e.unit!)) expect(e.t - last.get(e.unit!)!).toBeGreaterThanOrEqual(SKY.hit.gap - 1e-9);
        last.set(e.unit!, e.t);
        expect(e.text).toBe('');
      }
    }
    expect([...marks].sort()).toEqual(['fireout', 'holes', 'pieces', 'smoke']);
  });

  it('a straggler that fell out burning may have its fire put out, once, while it is still flying', () => {
    let out = 0;
    for (const s of runs) {
      const h = s.history();
      const burning = new Set(h.filter((e) => e.what === 'air:straggle' && (e.extra as { fire?: boolean }).fire).map((e) => (e.extra as { id: number }).id));
      const ended = new Map<number, number>();
      for (const e of h) {
        const x = e.extra as { id?: number; strag?: number };
        if (e.what === 'air:lost' && x.strag !== undefined) ended.set(x.strag, e.t);
        if (e.what === 'air:limped') ended.set(x.id!, e.t);
      }
      const seen = new Set<number>();
      for (const e of h.filter((x) => x.what === 'air:fireout')) {
        const id = (e.extra as { id: number }).id;
        expect(burning.has(id)).toBe(true);
        expect(seen.has(id)).toBe(false);
        seen.add(id);
        if (ended.has(id)) expect(e.t).toBeLessThanOrEqual(ended.get(id)!);
        out++;
      }
    }
    expect(out).toBeGreaterThan(5);
  });

  it('a heavy on three engines may catch its box up after the drop, and is then back in formation, never lost or limping home as a straggler', () => {
    let back = 0;
    for (const [i, tk] of tickets.entries()) {
      const heavy = structuredClone(tk);
      heavy.place.models![RAIDS[i][0]].bmr = 'B17';
      for (let k = 0; k < RUNS; k++) {
        const h = fight(heavy, k).history();
        for (const e of h.filter((x) => x.what === 'air:regain')) {
          const id = (e.extra as { id: number }).id;
          const fell = h.find((x) => x.what === 'air:straggle' && (x.extra as { id: number }).id === id)!;
          expect((fell.extra as { eng: number[] }).eng.length).toBe(1);
          const bombs = h.find((x) => x.what === 'air:bombs' && x.unit === e.unit) ?? h.find((x) => x.what === 'air:jettison' && x.unit === e.unit) ?? h.find((x) => x.what === 'routing' && x.unit === e.unit);
          expect(bombs && bombs.t <= e.t).toBe(true);
          expect(h.some((x) => x.what === 'air:lost' && (x.extra as { strag?: number }).strag === id)).toBe(false);
          expect(h.some((x) => x.what === 'air:limped' && (x.extra as { id: number }).id === id)).toBe(false);
          back++;
        }
        // A twin never does.
        expect(fight(tk, k).history().some((x) => x.what === 'air:regain')).toBe(false);
      }
    }
    expect(back).toBeGreaterThan(3);
  });
});

describe('the sea under a loss', () => {
  it('is off every land ring of the ground; no ground or an inland board is land', () => {
    const ground = { land: [[[0, 0], [10, 0], [10, 10], [0, 10]]] as [number, number][][], inland: false };
    expect(seaAt(ground, 5, 5)).toBe(false);
    expect(seaAt(ground, 15, 5)).toBe(true);
    expect(seaAt({ ...ground, inland: true }, 15, 5)).toBe(false);
    expect(seaAt(undefined, 15, 5)).toBe(false);
  });
});
