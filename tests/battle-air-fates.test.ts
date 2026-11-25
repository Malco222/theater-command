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
import { collideWords } from '../src/battle/air/falls';
import { coolOf, crewOf, fateOf, freshMemory, HOWS, ramShare, seaAt, tanksOf, weightsOf, type Scene } from '../src/battle/air/fates';
import { twoOf } from '../src/battle/air/names';
import { airBattle, AirSim } from '../src/battle/air/sim';
import { airTicket } from '../src/battle/air/ticket';
import type { AirPlace, AirUnit, Fall } from '../src/battle/air/types';
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
        // Seen falling a moment first, so the eye can follow it down (the art director, 20 November).
        expect(e.t - by.t).toBeGreaterThanOrEqual(SKY.fate.near.min - 1e-9);
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

  it('a collision is two losses of the two sides at the same moment, each naming the other, told once in one line for both', () => {
    const was = { ...SKY.fate.ram };
    // Head-on collisions are rare: made common here so the test sees enough of them.
    Object.assign(SKY.fate.ram, { headon: 1, other: 1, tangle: 0.3 });
    let pairs = 0, mixedPairs = 0;
    try {
      // Also with the groups taken in the other order, so a fighter's loss can come before its bomber's.
      // More runs over Alsace, where a French fighter and the bomber it fires on fall together most.
      for (const [i, tk] of tickets.entries()) for (let k = 0; k < (i === 0 ? 30 : RUNS); k++) for (const flip of [false, true]) {
        const s = airBattle({ ...tk, seed: tk.seed + k }, { show: true });
        if (flip) s.units.reverse();
        s.ai.att = true; s.ai.def = true;
        while (!s.over) s.step();
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
          // The UI director, 15:02Z: 'A bomber of the 8th Bomb Group and a fighter of JG 26 collide, and both go down.'
          if (e.text) {
            // ("and one of the Home Fleet's carrier fighters" names an aircraft too: the design director, 24 Nov 2026 22:58Z)
            expect(e.text).toMatch(/^[A-Z].* and (an?|one of) .* collide, and both go down(, .*)?\.$/);
            // The bomber first, as the ruled line has it.
            if (fallOf(e).air !== fallOf(o).air) { mixedPairs++; expect(e.text).not.toMatch(/^(An? (\w+ )?fighter of |One of the [^,]*fighters and )/); }
            const c = fallOf(e).chutes + fallOf(o).chutes;
            if (c === 0) expect(e.text).not.toMatch(/parachute/);
            else expect(e.text).toContain(c === 1 ? ', one parachute' : `, ${c} parachutes`);
          }
          expect(fallOf(e).meant).toBeUndefined();
          pairs++;
        }
      }
    } finally { Object.assign(SKY.fate.ram, was); }
    expect(pairs).toBeGreaterThanOrEqual(2);
    expect(mixedPairs).toBeGreaterThanOrEqual(2);
  });
});

/** A scene to draw from: a bomber of a twin-engined box at middle height, over land, hit by a fighter, all else plain. */
const scene = (o: Partial<Scene> = {}): Scene => ({
  air: 'bmr', engines: 2, nat: 'USA', model: 'Bomber:USA', when: { year: 1942, month: 6 }, gun: 'hmg', flak: null, pass: null,
  band: 1, alt: 4_500, loaded: false, strag: null, sea: false, night: false, back: false, over: 'enemy', crew: 5, near: null, ...o,
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

/** Shares of a mix of scenes, each from the weights alone (no memory of the last way shown). */
function mixed(mk: (r: Rng) => Scene, n = 6_000): Record<string, number> {
  const rng = new Rng(7), pick = new Rng(3), out: Record<string, number> = {};
  for (let i = 0; i < n; i++) { const f = fateOf(rng, mk(pick), freshMemory()); out[f.how] = (out[f.how] ?? 0) + 1 / n; }
  return out;
}
const of = (o: Record<string, number>, ...ks: string[]) => ks.reduce((t, k) => t + (o[k] ?? 0), 0);
const oneOf = <T,>(r: Rng, xs: T[]): T => xs[Math.floor(r.next() * xs.length)];

describe('the historian\'s check of the ways down (historian/air-fates-check.md, 20 November)', () => {
  it('Japan\'s tanks: bare until mid-1944, the G4M until November 1944; partly protected after, never as well as the others', () => {
    expect(tanksOf('JAP', 'Zero', { year: 1943, month: 8 })).toBe('bare');
    expect(tanksOf('JAP', 'Zero', { year: 1944, month: 8 })).toBe('part');
    expect(tanksOf('JAP', 'Bomber:JAP', { year: 1944, month: 8 })).toBe('bare');
    expect(tanksOf('JAP', 'G4M', { year: 1944, month: 9 })).toBe('bare');
    expect(tanksOf('JAP', 'Bomber:JAP', { year: 1945, month: 1 })).toBe('part');
    expect(tanksOf('USA', 'B17', { year: 1942, month: 1 })).toBeNull();
    const zero = (month: number, year: number) => scene({ air: 'ftr', engines: 1, nat: 'JAP', model: 'Zero', when: { year, month }, crew: 1 });
    const fl = HOWS.indexOf('flamer'), w = (sc: Scene) => weightsOf(sc, freshMemory())[fl];
    const none = scene({ air: 'ftr', engines: 1, model: 'Warhawk', crew: 1 });
    // Flamers about three times as often bare, about two times after (the historian: cut to about two, not to none).
    expect(w(zero(3, 1943)) / w(none)).toBeCloseTo(3, 5);
    expect(w(zero(12, 1944)) / w(none)).toBeCloseTo(2, 5);
    const bare = shares(zero(3, 1943)), part = shares(zero(12, 1944)), plain = shares(none);
    expect(bare.flamer).toBeGreaterThan(part.flamer);
    expect(part.flamer).toBeGreaterThan(1.4 * plain.flamer);
  });

  it('the record\'s shares: day heavies come down slowly, night kills burn, unprotected types burn, cannon take structure off, head-on kills the pilot', () => {
    // The Eighth Air Force's heavies by day: German fighters with cannon, or flak; high; loaded or not.
    const heavy = mixed((r) => {
      const flak = r.next() < 0.3;
      return scene({ engines: 4, model: 'B17', when: { year: 1943, month: 10 }, gun: flak ? null : r.next() < 0.8 ? 'cannon20' : 'hmg', flak: flak ? oneOf(r, ['heavy', 'heavy', 'medium'] as const) : null,
        pass: flak ? null : oneOf(r, ['quarter', 'quarter', 'headon', 'astern', 'close'] as const), band: 2, alt: 7_500, loaded: r.next() < 0.5, crew: 10 });
    });
    expect(of(heavy, 'glide', 'burn')).toBeGreaterThanOrEqual(0.4);
    expect(of(heavy, 'explode')).toBeLessThan(0.1);
    // A Lancaster brought down by a night fighter's cannon from below and astern: about three in five flamers or blown up.
    const night = mixed((r) => scene({ engines: 4, nat: 'UK', model: 'Lancaster', when: { year: 1944, month: 2 }, gun: 'cannon20', pass: oneOf(r, ['below', 'below', 'quarter', 'astern'] as const), band: 2, alt: 6_000, loaded: r.next() < 0.5, night: true, crew: 7 }));
    expect(of(night, 'flamer', 'explode')).toBeGreaterThan(0.55);
    expect(of(night, 'flamer', 'explode')).toBeLessThan(0.75);
    // Unprotected: about three in five burn, flame or blow up.
    const bare = mixed((r) => r.next() < 0.5
      ? scene({ air: 'ftr', engines: 1, nat: 'JAP', model: 'Zero', when: { year: 1943, month: 3 }, band: oneOf(r, [0, 1, 1, 2] as const), crew: 1 })
      : scene({ nat: 'JAP', model: 'Bomber:JAP', when: { year: 1943, month: 3 }, pass: oneOf(r, ['astern', 'quarter', 'beam', 'headon'] as const), loaded: r.next() < 0.5, crew: 7 }));
    expect(of(bare, 'burn', 'flamer', 'explode')).toBeGreaterThan(0.55);
    expect(of(bare, 'burn', 'flamer', 'explode')).toBeLessThan(0.7);
    // Cannon: about a third lose a wing or the tail or come apart.
    const cannon = mixed((r) => r.next() < 0.5
      ? scene({ air: 'ftr', engines: 1, gun: 'cannon20', band: oneOf(r, [0, 1, 2] as const), crew: 1 })
      : scene({ gun: 'cannon20', pass: oneOf(r, ['quarter', 'quarter', 'astern', 'beam', 'headon'] as const), loaded: r.next() < 0.5, band: oneOf(r, [0, 1, 2] as const) }));
    expect(of(cannon, 'wing', 'tail', 'breakup')).toBeGreaterThan(0.3);
    expect(of(cannon, 'wing', 'tail', 'breakup')).toBeLessThan(0.42);
    // Head-on: about one in five has its pilot killed.
    const head = mixed((r) => scene({ engines: r.next() < 0.5 ? 4 : 2, gun: oneOf(r, ['cannon20', 'cannon20', 'hmg', 'rifle'] as const), pass: 'headon', loaded: r.next() < 0.5, band: oneOf(r, [1, 2] as const) }));
    expect(of(head, 'pilot')).toBeGreaterThan(0.15);
    expect(of(head, 'pilot')).toBeLessThan(0.27);
  });

  it('by night no bomber is struck by another (there is no tight box in the stream), and a heavy that catches up stays with the stream', () => {
    let regain = 0, lost = 0;
    for (const [i, tk] of tickets.entries()) {
      const dark = structuredClone(tk);
      dark.place.night = true;
      dark.place.models![RAIDS[i][0]].bmr = 'Lancaster';
      for (let k = 0; k < RUNS; k++) {
        const h = fight(dark, k).history();
        for (const e of lostOf(h)) { lost++; expect(fallOf(e).how).not.toBe('struck'); }
        for (const e of h.filter((x) => x.what === 'air:regain')) { regain++; expect(e.text).toMatch(/, on three engines, stays with the stream\.$/); }
      }
    }
    expect(lost).toBeGreaterThan(50);
    expect(regain).toBeGreaterThan(0);
  });

  it('a fighter rams on purpose where and when pilots did: one line, the one who rams first, and his pilot may get out and fly again', () => {
    const was = SKY.fate.ram.meant;
    // Made certain here for the French over Alsace, so the test sees enough of them (a fighter and the bomber it fires
    // on seldom fall in the same moment).
    SKY.fate.ram.meant = [{ nat: 'FRA', from: 1900, to: 2000, p: 1 }];
    let rams = 0, back = 0;
    try {
      for (let k = 0; k < 30; k++) {
        const lost = lostOf(fight(tickets[0], k).history());
        for (const e of lost) {
          const f = fallOf(e);
          if (!f.meant) continue;
          expect(f.how).toBe('collide');
          const o = lost.find((x) => fallOf(x).fid === f.with)!;
          expect(fallOf(o).meant).toBe(true);
          expect(new Set([f.air, fallOf(o).air])).toEqual(new Set(['ftr', 'bmr']));
          if (f.air === 'ftr' && f.back) { back++; expect(f.chutes).toBe(1); }
          if (e.text) {
            rams++;
            expect(e.text).toMatch(/^An? [^,]*fighter of .* rams (an? |one of the ).*, and both go down(, .*)?\.$/);
            // The endings of the two together.
            const c = f.chutes + fallOf(o).chutes;
            if (c === 0) expect(e.text).not.toMatch(/parachute/);
            else expect(e.text).toContain(c === 1 ? ', one parachute' : `, ${c} parachutes`);
          }
        }
      }
    } finally { SKY.fate.ram.meant = was; }
    expect(rams).toBeGreaterThan(2);
    expect(back).toBeGreaterThan(0);
  });

  it('rams on purpose keep to the nations and months of the record (historian/nose-art-rams-check.md): months count from 0', () => {
    const at = (year: number, month: number) => ({ year, month });
    // The Soviet taran: from the invasion, most in 1941 and 1942, fewer in 1943, none after.
    expect(ramShare('SOV', at(1941, 4), 2)).toBe(0);
    expect(ramShare('SOV', at(1941, 5), 2)).toBe(0.2);
    expect(ramShare('SOV', at(1942, 11), 2)).toBe(0.2);
    expect(ramShare('SOV', at(1943, 0), 2)).toBe(0.1);
    expect(ramShare('SOV', at(1944, 0), 2)).toBe(0);
    // Japan against the four-engined only: one-offs from June 1944, the ramming flights from November.
    expect(ramShare('JAP', at(1944, 4), 4)).toBe(0);
    expect(ramShare('JPN', at(1944, 7), 4)).toBe(0.05);
    expect(ramShare('JAP', at(1944, 9), 4)).toBe(0.05);
    expect(ramShare('JAP', at(1944, 10), 4)).toBe(0.12);
    expect(ramShare('JAP', at(1944, 10), 2)).toBe(0);
    // Germany's Sturm groups: the heavies, on the close pass from astern, to April 1945.
    expect(ramShare('GER', at(1944, 7), 4, 'close')).toBe(0.08);
    expect(ramShare('GER', at(1944, 7), 4, 'headon')).toBe(0);
    expect(ramShare('GER', at(1944, 7), 2, 'close')).toBe(0);
    expect(ramShare('GER', at(1943, 11), 4, 'close')).toBe(0);
    expect(ramShare('GER', at(1945, 4), 4, 'close')).toBe(0);
    expect(ramShare('UK', at(1942, 0), 4, 'close')).toBe(0);
  });
});

describe('two of one group collide (the UI director, 20 November 15:38Z; the historian: it happened inside the tight boxes)', () => {
  it('the group is named once and the count is a word, each form as one aircraft of it is named', () => {
    expect(collideWords(twoOf('8th Bomb Group', 'bomber'), '', 0, 0)).toBe('Two bombers of the 8th Bomb Group collide, and both go down.');
    expect(collideWords(twoOf('JG 26', 'fighter'), '', 2, 0)).toBe('Two fighters of JG 26 collide, and both go down, 2 parachutes.');
    expect(twoOf('JG 26', 'fighter', 'German')).toBe('two German fighters of JG 26');
    expect(twoOf('Shokaku and Zuikaku dive bombers', 'bomber', 'Japanese')).toBe('two of the Japanese Shokaku and Zuikaku dive bombers');
    expect(twoOf('Home Fleet carrier fighters', 'fighter', 'British')).toBe('two British carrier fighters of the Home Fleet');
    expect(twoOf('Home Fleet carrier fighters', 'fighter')).toBe("two of the Home Fleet's carrier fighters");
    expect(twoOf('Home Fleet strike wing', 'bomber')).toBe("two bombers of the Home Fleet's strike wing");
    expect(twoOf('Polish Navy strike wing', 'bomber', 'Polish')).toBe("two bombers of the Polish Navy's strike wing");
  });

  it('two bombers of a tight box by day, or two fighters in a fight, lost in the same moment: one line for both; never by night, never a pilot the rules bring back', () => {
    // Two of one group all but never fall in the same tenth of a second, so the two losses are handed to the sim here.
    type Inner = { units: AirUnit[]; fid: number; place: AirPlace; fated: (lost: unknown[]) => void };
    const was = { ...SKY.fate.ram.same };
    const s = airBattle({ ...tickets[0], seed: tickets[0].seed }, { show: true });
    s.ai.att = true; s.ai.def = true;
    const inner = s as unknown as Inner;
    const two = (u: AirUnit, back = false) => [0, 1].map(() => ({ u, fall: { kind: 'burn', chutes: 1, over: back ? 'own' : 'enemy', back, captured: back ? 0 : 1 }, by: null, s: null, alt: u.alt, fid: inner.fid++ }));
    const tell = (u: AirUnit, back = false) => { inner.fated(two(u, back)); return lostOf(s.history()).slice(-2); };
    const paired = (e: FieldEvent[]) => fallOf(e[0]).how === 'collide' && fallOf(e[1]).how === 'collide' && fallOf(e[0]).with === fallOf(e[1]).fid && fallOf(e[1]).with === fallOf(e[0]).fid;
    try {
      let box: AirUnit | undefined, ftr: AirUnit | undefined;
      while (!s.over && !(box && ftr)) {
        s.step();
        box = inner.units.find((u) => u.air === 'bmr' && u.state === 'steady' && u.n > 6);
        ftr = inner.units.find((u) => u.air === 'ftr' && u.n > 6 && (u.fire.length > 0 || u.contacts.length > 0));
      }
      expect(box && ftr).toBeTruthy();

      SKY.fate.ram.same.bmr = 1; SKY.fate.ram.same.ftr = 0;
      const b = tell(box!);
      expect(paired(b)).toBe(true);
      expect(b[0].text).toMatch(/^Two (of the .+|(?:[A-Z][a-z]+ )?bombers of .+) collide, and both go down(, .+)?\.$/);
      expect(b[1].text).toBe('');
      expect(fallOf(b[0]).meant).toBeUndefined();
      const c = fallOf(b[0]).chutes + fallOf(b[1]).chutes;
      if (c === 0) expect(b[0].text).not.toMatch(/parachute/);
      else expect(b[0].text).toContain(c === 1 ? ', one parachute' : `, ${c} parachutes`);
      // Fighters at their own share, here none.
      const f0 = tell(ftr!);
      expect(paired(f0)).toBe(false);
      expect(f0.every((e) => e.text !== '' && !/collide/.test(e.text))).toBe(true);
      // Never in the night stream.
      (inner.place as { night?: boolean }).night = true;
      expect(paired(tell(box!))).toBe(false);
      (inner.place as { night?: boolean }).night = false;

      SKY.fate.ram.same.bmr = 0; SKY.fate.ram.same.ftr = 1;
      expect(paired(tell(box!))).toBe(false);
      const f1 = tell(ftr!);
      expect(paired(f1)).toBe(true);
      expect(f1[0].text).toMatch(/^Two (of the .+|(?:[A-Z][a-z]+ )?(?:carrier )?fighters of .+) collide, and both go down(, .+)?\.$/);
      // Not a group that is in no fight, and never a pilot the rules bring back.
      const { fire, contacts } = ftr!;
      ftr!.fire = []; ftr!.contacts = [];
      expect(paired(tell(ftr!))).toBe(false);
      ftr!.fire = fire; ftr!.contacts = contacts;
      const home = tell(ftr!, true);
      expect(paired(home)).toBe(false);
      expect(home.every((e) => fallOf(e).back && fallOf(e).chutes === 1)).toBe(true);
    } finally { Object.assign(SKY.fate.ram.same, was); }
  });
});

describe('the art director\'s and the UI director\'s rulings (20 November, 15:02Z)', () => {
  it('each group says how its engines are cooled, beside its model, for the shade of what a hit one trails', () => {
    expect(coolOf('Bf109')).toBe('liquid');
    expect(coolOf('Spitfire')).toBe('liquid');
    expect(coolOf('Lancaster')).toBe('liquid');
    expect(coolOf('Zero')).toBe('radial');
    expect(coolOf('B17')).toBe('radial');
    expect(coolOf('D3A')).toBe('radial');
    const s = airBattle({ ...tickets[0], seed: tickets[0].seed }, { show: true });
    s.ai.att = true; s.ai.def = true;
    for (let i = 0; i < 300 && !s.over; i++) s.step();
    const groups = s.units.filter((u) => u.extra);
    expect(groups.length).toBeGreaterThan(2);
    for (const u of groups) expect((u.extra as { cool?: string }).cool).toBe(coolOf(u.model));
  });

  it('a fire put out is told from the aircraft and leaves its propeller feathered; a heavy regains its box only once its fire is out, most with it feathered, and says the engines it has left', () => {
    let out = 0, feather = 0, wind = 0;
    for (const [i, tk] of tickets.entries()) {
      const heavy = structuredClone(tk);
      heavy.place.models![RAIDS[i][0]].bmr = 'B17';
      for (let k = 0; k < RUNS; k++) {
        const h = fight(heavy, k).history();
        for (const e of h.filter((x) => x.what === 'air:fireout')) {
          out++;
          expect(e.text).toMatch(/^On (an? |one of the ).*, the crew put out the engine fire\.$/);
          expect((e.extra as { prop: string }).prop).toBe('feather');
        }
        for (const e of h.filter((x) => x.what === 'air:regain')) {
          const id = (e.extra as { id: number }).id;
          expect(e.text).toMatch(/^(An? |One of the ).*, on three engines, has caught up and is back in formation\.$/);
          const fell = h.find((x) => x.what === 'air:straggle' && (x.extra as { id: number }).id === id)!;
          if ((fell.extra as { fire?: boolean }).fire) {
            const put = h.find((x) => x.what === 'air:fireout' && (x.extra as { id: number }).id === id);
            expect(put && put.t <= e.t, 'its fire was out first').toBe(true);
          }
          if ((e.extra as { prop: string }).prop === 'feather') feather++; else wind++;
        }
      }
    }
    expect(out).toBeGreaterThan(5);
    expect(feather).toBeGreaterThan(2 * wind);
  });
});

describe('each bomber type shows its real crew get out (the historian, air-fates-check.md section 6; Brian\'s yes of 20 November)', () => {
  const at = (year: number, month: number) => ({ year, month });
  /** A fate without who got out. */
  const less = (f: ReturnType<typeof fateOf>) => { const { chutes: _c, chuteAt: _a, out: _o, ...rest } = f; return rest; };

  it('the crew aboard is by the type and the month: two in a dive bomber, three in a torpedo bomber, ten in a B-17', () => {
    for (const m of ['D3A', 'D4Y', 'SBD', 'SB2C', 'Skua', 'Stuka', 'Dive:FRA']) expect(crewOf(m, at(1942, 0)), m).toBe(2);
    for (const m of ['B5N', 'B6N', 'TBD', 'TBF', 'Swordfish', 'Barracuda', 'Torpedo:ITA']) expect(crewOf(m, at(1942, 0)), m).toBe(3);
    // The He 111 flew with four until an extra gunner joined in 1941.
    expect([crewOf('He111', at(1940, 11)), crewOf('He111', at(1941, 0))]).toEqual([4, 5]);
    expect([crewOf('G4M', at(1942, 0)), crewOf('Lancaster', at(1943, 0))]).toEqual([7, 7]);
    // The B-17 carried ten, often nine from mid-1944 with one waist gunner dropped (months count from 0: 5 is June).
    expect([crewOf('B17', at(1943, 11)), crewOf('B17', at(1944, 5)), crewOf('B17', at(1944, 6))]).toEqual([10, 10, 9]);
    // A nation's plain twin: five, Japan's read as the G4M and the Soviet as the SB.
    expect(['Bomber:GER', 'Bomber:UK', 'Bomber:JAP', 'Bomber:JPN', 'Bomber:SOV', 'Unknown'].map((m) => crewOf(m, at(1942, 0)))).toEqual([5, 5, 7, 7, 3, 5]);
  });

  it('never more are seen out than are aboard, every one of a crew can be, and each parachute has its moment', () => {
    for (const crew of [2, 3, 4, 5, 7, 10]) {
      const rng = new Rng(100 + crew);
      let most = 0, mostOut = 0, leastOut = crew;
      for (let i = 0; i < 6_000; i++) {
        const f = fateOf(rng, scene({ crew, band: 2, alt: 7_000 }), freshMemory());
        expect(f.chutes + (f.out ?? 0)).toBeLessThanOrEqual(crew);
        expect((f.chuteAt ?? []).length).toBe(f.chutes);
        const ts = f.chuteAt ?? [];
        expect(ts).toEqual([...ts].sort((a, b) => a - b));
        for (const t of ts) expect(t).toBeLessThanOrEqual(Math.max(0.5, (f.boom ?? f.dur) - 0.5));
        most = Math.max(most, f.chutes);
        if (f.how === 'glide') { mostOut = Math.max(mostOut, f.out ?? 0); leastOut = Math.min(leastOut, f.out ?? 0); }
      }
      expect(most, `crew ${crew}`).toBe(crew);
      // A crew that glides in climbs out all but a few (at most a third short).
      expect(mostOut, `crew ${crew}`).toBe(crew);
      expect(leastOut, `crew ${crew}`).toBeGreaterThanOrEqual(crew - Math.ceil(crew / 3) + 1);
    }
  });

  it('the crew aboard never moves another look: the same dice give the same way down, and a smaller crew is the first of the same men', () => {
    let bigger = 0;
    for (let seed = 1; seed <= 400; seed++) {
      const fates = [2, 5, 10].map((crew) => {
        const rng = new Rng(seed), mem = freshMemory();
        // Three losses running, so a stream that had moved would show in the next one.
        const three = [0, 1, 2].map(() => fateOf(rng, scene({ crew, band: 2, alt: 7_000, model: 'B17', engines: 4 }), mem));
        return { three, state: rng.state };
      });
      const [two, five, ten] = fates;
      expect(two.state).toBe(five.state);
      expect(ten.state).toBe(five.state);
      for (let i = 0; i < 3; i++) {
        const a = two.three[i], b = five.three[i], c = ten.three[i];
        expect(less(a)).toEqual(less(b));
        expect(less(c)).toEqual(less(b));
        expect(a.chuteAt ?? []).toEqual((b.chuteAt ?? []).slice(0, a.chutes));
        expect((c.chuteAt ?? []).slice(0, b.chutes)).toEqual(b.chuteAt ?? []);
        expect(c.chutes).toBeGreaterThanOrEqual(b.chutes);
        if (c.chutes > 5) bigger++;
      }
    }
    expect(bigger).toBeGreaterThan(20);
  });

  it('in a battle, whatever the crews are, the counts and every other look are the same; a B-17 box shows up to ten get out, and its line says so', () => {
    const was = { crews: SKY.fall.crews, dated: SKY.fall.crewsDated };
    const b17 = structuredClone(tickets[0]);
    for (const nat of Object.keys(b17.place.models ?? {})) b17.place.models![nat].bmr = 'B17';
    const strip = (s: AirSim) => s.history().map((e) => {
      const x = { ...(e.extra ?? {}) } as Record<string, unknown>;
      const lost = e.what === 'air:lost';
      if (lost) for (const k of ['chutes', 'chuteAt', 'chuteCue', 'out', 'captured']) delete x[k];
      return { ...e, text: lost ? '' : e.text, extra: x };
    });
    const seenOut = (s: AirSim) => lostOf(s.history()).filter((e) => fallOf(e).air === 'bmr').map((e) => fallOf(e).chutes + (fallOf(e).out ?? 0));
    try {
      let most = 0, lines = 0;
      for (let k = 0; k < 6; k++) {
        const real = fight(b17, k);
        for (const e of lostOf(real.history())) {
          const f = fallOf(e);
          if (f.air !== 'bmr') continue;
          expect(f.chutes + (f.out ?? 0)).toBeLessThanOrEqual(10);
          most = Math.max(most, f.chutes + (f.out ?? 0));
          if (f.chutes > 5 && f.how !== 'collide') { lines++; expect(e.text).toContain(`, ${f.chutes} parachutes`); }
        }
        SKY.fall.crews = { Bomber: 1 }; SKY.fall.crewsDated = [];
        const ones = fight(b17, k);
        SKY.fall.crews = { Bomber: 12 };
        const twelves = fight(b17, k);
        SKY.fall.crews = was.crews; SKY.fall.crewsDated = was.dated;
        for (const other of [ones, twelves]) {
          expect(other.outcome()).toEqual(real.outcome());
          expect(other.over!.lines).toEqual(real.over!.lines);
          expect(strip(other)).toEqual(strip(real));
        }
        expect(Math.max(...seenOut(ones))).toBe(1);
        expect(Math.max(...seenOut(twelves))).toBeGreaterThan(Math.max(...seenOut(real)) - 1);
        expect(Math.max(...seenOut(twelves))).toBeLessThanOrEqual(12);
      }
      expect(most).toBeGreaterThan(5);
      expect(lines).toBeGreaterThan(0);
    } finally { SKY.fall.crews = was.crews; SKY.fall.crewsDated = was.dated; }
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
