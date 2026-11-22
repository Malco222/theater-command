// Stretcher bearers (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it off if
// someone doesnt want it"; the design director's rulings Q4 and Q5, 18:36Z, and 18:38Z on the light cases). Of each loss a
// block suffers a quarter are killed and the rest wounded; its bearers (its army's share of its men) carry the litter
// cases to its aid post 503 m back and walk back, 330 battle seconds a trip, its own blows the weaker by the men away while
// they are out; its light cases (17% of the wounded) are back in the line 30 s after they fell, if it still fights. The
// drill draws the wounded, the stretchers and the men back, looks only. Off, nothing of it is read: the battle of ec6f8e9c
// to the last number.
import { describe, expect, it } from 'vitest';
import type { FieldEvent, FieldTicket, Side } from '../src/battle/core';
import { LAND, LandBattle, PRACTICE_AIR, PRACTICE_AIR_DATE, bearerShare, killedShare, sampleTicket, withJoiners, withTeams, type LandPlace, type LandUnit, type MenBlock, type MenOut } from '../src/battle/land';
import { newCampaign, offers } from '../src/battle/ui/campaign';
import { orderAttack } from '../src/campaign/turn';
import { Rng, hashString } from '../src/core/rng';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';

type T = FieldTicket<LandPlace>;

/**
 * Everything a battle shows and does, every tick: the result, the time, every view key, the scale, the place of the dice,
 * every block's every number and key, the wrecks and craters, and every word said and logged. 'each' runs after each step.
 * 'looks' leaves out the craters and the shell events: with the men drawn, a tank's kill may go to her machine guns
 * rather than a shell, and a shell lands on the man it costs (the drill book, looks only; so on ec6f8e9c too).
 */
function signed(b: LandBattle, each?: (b: LandBattle, k: number) => void, n = 30000, looks = true): string {
  const ev: FieldEvent[] = [];
  const keys = new Set<string>();
  for (let k = 0; !b.over && k < n; k++) {
    b.step();
    const v = b.view();
    for (const x of Object.keys(v)) keys.add(x);
    ev.push(...v.events.filter((e) => looks || e.what !== 'shell'));
    each?.(b, k);
  }
  return JSON.stringify({
    over: b.over, t: b.t, keys: [...keys].sort(), scale: b.scale(), page: b.scale(undefined, LAND.scale.page),
    rng: inside(b).rng.state,
    units: b.units.map((u) => [u.id, u.corps, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged, u.reserve ?? null, u.power0,
      u.home, u.dry ?? null, u.order, u.path, u.cal ?? null, u.crew ?? null, u.taken ?? null, Object.keys(u).sort().join(',')]),
    wrecks: b.wrecks.map((z) => [z.x, z.y, z.at]),
    craters: looks ? b.craters.map((z) => [z.x, z.y, z.at]) : [],
    said: ev.map((e) => [e.t, e.what, e.unit ?? null, e.text]),
    log: b.log.map((e) => [e.t, e.what, e.unit ?? null, e.text]),
  });
}
const digest = (s: string): string => hashString(s).toString(16).padStart(8, '0') + hashString('#' + s).toString(16).padStart(8, '0');

/** The rules' private workings a test reads. */
interface Inside {
  rng: Rng;
  blow(u: LandUnit, t: LandUnit): { dmg: number };
  suffer(t: LandUnit, dmg: number, nerveMul: number): void;
  carry(): void;
  rout(u: LandUnit): void;
}
const inside = (b: LandBattle): Inside => b as unknown as Inside;
const steps = (b: LandBattle, n: number): void => { for (let k = 0; k < n && !b.over; k++) b.step(); };
/** A loss of d strength on a block, as a blow lands it (suffer). */
const hit = (b: LandBattle, u: LandUnit, d: number): void => inside(b).suffer(u, (d / u.str0) * u.power0, 1);
/** A blow's weight with the dice fixed (the same luck each time). */
const weigh = (b: LandBattle, u: LandUnit, t: LandUnit): number => { inside(b).rng = new Rng(9); return inside(b).blow(u, t).dmg; };
const footOf = (b: LandBattle, side: Side): LandUnit => b.units.find((u) => u.side === side && u.kind === 'inf')!;
const B = LAND.bearers, TRIP = (2 * B.aid) / B.pace;

/**
 * A quiet field: an American attack the player's, on a German defence, neither staff thinking, the guns off the field
 * (their fire reaches across it: a quiet battle's only harm), the day 20 minutes long, so every loss is the test's own.
 */
function quiet(on = true): LandBattle {
  const b = new LandBattle({ ...sampleTicket('plains', 1.0, { seed: 2, humanAtt: true, nat: { att: 'USA', def: 'GER' } }), limit: 1200 });
  b.ai.att = b.ai.def = false;
  for (const u of b.units) if (u.kind === 'art') u.state = 'withdrawn';
  b.bearers = on;
  return b;
}

// ---- (1) off: the battle of ec6f8e9c ----------------------------------------------------------------------------------

/** Fought on ec6f8e9c (the switch not yet written): the result, the battle's length and the fingerprint. */
const OFF: [string, () => T, ((b: LandBattle) => void)?, [number, number, string]?][] = [
  ['an American attack on a German defence in 1944, the attack the player\'s with his staff, plains 1.1 seed 3',
    () => ({ ...sampleTicket('plains', 1.1, { seed: 3, humanAtt: true, nat: { att: 'USA', def: 'GER' } }), turn: 60 }), (b) => { b.perUnit = true; }, [1.084, 233, '5958fdcdc53e3e54']],
  ['a Soviet attack over a river into German woods, support teams, forest 1.3 seed 8',
    () => withTeams(sampleTicket('forest', 1.3, { seed: 8, river: true, nat: { att: 'SOV', def: 'GER' } })), undefined, [1.487, 323, '71f025370697a50a']],
  ['a British attack on an Italian city, joiners both sides, the defence the player\'s under fog, city 1.2 seed 5',
    () => withJoiners(sampleTicket('urban', 1.2, { seed: 5, town: 'city', nat: { att: 'UK', def: 'ITA' } }), { att: [{ joined: 0.5 }], def: [{ joined: 0.4 }] }),
    (b) => { b.ticket.def.human = true; b.fog = true; }, [1.07, 302, 'b1b00134a0d9390f']],
  ['fighters on call both sides, the relief on, hills 1.2 seed 11',
    () => withJoiners(sampleTicket('hills', 1.2, { seed: 11, air: PRACTICE_AIR, date: PRACTICE_AIR_DATE }), { att: [{ joined: 0.5 }] }), (b) => { b.relief = true; }, [1.44, 283, 'a1bd3b60b565d961']],
];
/** The drill of the first, sections and platoons, on ec6f8e9c: its view each 25th step and every round, digested. */
const OFF_DRILL: [number, number, string] = [70702, 94, '4acdf5c8010c6cf1'];

describe('(1) switched off: the battle of ec6f8e9c, and nothing of the bearers', () => {
  for (const [name, make, setup, pin] of OFF) it(name, () => {
    const b = new LandBattle(make());
    b.ai.att = b.ai.def = true;
    setup?.(b);
    expect(b.bearers).toBe(false);
    const s = signed(b);
    expect([Math.round(b.over!.R * 1000) / 1000, b.over!.seconds, digest(s)]).toEqual(pin);
    expect(s).not.toMatch(/"(bearers|aid|returns)"|land:bearers/);
    expect(b.returns).toEqual([]);
    expect(b.units.every((u) => b.litter(u) === undefined)).toBe(true);
    expect(b.bearerView()).toEqual({});
  }, 300_000);
  it('the drill: the men and their rounds of ec6f8e9c, no man wounded, carried or bearing', () => {
    const b = new LandBattle(OFF[0][1]());
    b.ai.att = b.ai.def = true; OFF[0][2]!(b);
    const d = b.drillOn({ sections: true, platoons: true });
    const parts: string[] = [];
    let last = 0, n = 0;
    for (let k = 0; !b.over && k < 30000; k++) {
      b.step();
      if (k % 25 === 0) { parts.push(digest(JSON.stringify(d.view()))); n++; }
      for (const r of d.rounds()) { if (r.n <= last) continue; last = r.n; parts.push(digest(JSON.stringify(r))); }
    }
    const end = d.view();
    parts.push(digest(JSON.stringify(end)));
    expect([parts.length, n, digest(parts.join('\n'))]).toEqual(OFF_DRILL);
    const men = end.flatMap((x) => x.men);
    expect(men.some((m) => m.wd !== undefined || m.cy || m.gone !== undefined || m.back !== undefined || m.br)).toBe(false);
    expect(men.some((m) => ['wounded', 'carried', 'stretcher_front', 'stretcher_rear'].includes(m.p))).toBe(false);
  }, 600_000);
  it('on with no one hurt: the same battle to the last number (the dice untouched)', () => {
    const fight = (on: boolean): { s: string; b: LandBattle; str: number[] } => {
      const b = quiet(on), str = b.units.map((u) => u.str);
      return { s: signed(b, undefined, 6000), b, str };
    };
    const off = fight(false), on = fight(true);
    expect(on.b.t).toBeGreaterThan(500);
    expect(on.b.units.map((u) => u.str)).toEqual(on.str);
    expect(on.s).toBe(off.s);
  }, 300_000);
});

// ---- (2) the bearers go out and come back ---------------------------------------------------------------------------

describe('(2) the bearers go out after the first loss and are back 330 s later', () => {
  it('the table: killed by army (the design director, 19:14Z), 4 to a stretcher, the aid post 503 m back, at 3.048 m a second: 330 s a trip', () => {
    const want: Record<string, number> = { USA: 0.192, UK: 0.23, CAN: 0.23, AUS: 0.23, NZL: 0.23, SAF: 0.23, IND: 0.23, GER: 0.279, JAP: 0.271 };
    for (const [nat, k] of Object.entries(want)) { expect(B.killed[nat]).toBe(k); expect(killedShare(nat)).toBe(k); }
    // The Soviets, Italy, France, Vichy and every other army: the middle of the laid ratios, 3.40 wounded to 1 killed.
    for (const nat of ['SOV', 'ITA', 'FRA', 'VIC', 'CHN', 'POL', 'HUN']) expect(killedShare(nat)).toBe(0.227);
    expect(Object.keys(B.killed).sort()).toEqual([...Object.keys(want), '_'].sort());
    expect(B.litter).toBe(4);
    expect(B.aid).toBe(503);
    expect(B.pace).toBe(3.048);
    expect(B.light).toEqual({ share: 0.17, at: 0, out: 0.5, back: 1, dress: 30 });
    expect(TRIP).toBeCloseTo(330.05, 2);
  });
  it('out at the next step after the loss, all the litter cases a trip can take; home at t + 330 s, not before; told once to his own side', () => {
    const b = quiet(), u = footOf(b, 'att'), foe = footOf(b, 'def');
    steps(b, 10);
    expect(b.litter(u)).toBeUndefined();
    expect(b.bearerView('att')).toEqual({});
    const d = 0.005, t0 = b.t;
    hit(b, u, d);
    const l = b.litter(u)!;
    expect(l.all).toBeCloseTo(bearerShare('USA') * u.str0, 12);
    expect(l.waiting).toBeCloseTo(d * (1 - killedShare(u.nat)) * (1 - B.light.share), 12);
    expect(l.trips).toEqual([]);
    expect(l.out).toBe(0);
    // The aid post: 503 m behind where it started, toward the attack's own edge (south), on the field.
    expect(l.aid).toEqual([u.home[0], Math.max(0, u.home[1] - 503)]);
    b.step();
    expect(l.trips.length).toBe(1);
    expect(l.trips[0].back - t0).toBeCloseTo(TRIP, 2);
    expect(l.out).toBeCloseTo(d * (1 - killedShare(u.nat)) * (1 - B.light.share) * B.litter, 12);
    expect(l.waiting).toBeCloseTo(0, 12);
    // Its side told, once, in his words; never the computer's.
    const told = b.log.filter((e) => e.what === 'land:bearers');
    expect(told.map((e) => [e.unit, e.side, e.text])).toEqual([[u.id, 'att', `${u.name}'s stretcher bearers are carrying the wounded back.`]]);
    const v = b.view('att');
    expect(v.bearers).toEqual([{ unit: u.id, out: Math.round((l.out * u.men0) / u.str0), waiting: 0, back: l.trips[0].back }]);
    expect(v.aid).toEqual([{ side: 'att', unit: u.id, x: Math.round(l.aid[0] * 10) / 10, y: Math.round(l.aid[1] * 10) / 10 }]);
    // Out the whole trip, home as it ends.
    const back = l.trips[0].back;
    let away = true;
    while (b.t + 1e-9 < back && !b.over) { b.step(); away &&= l.out > 0 && l.trips.length === 1; }
    expect(b.over).toBeFalsy();
    expect(away).toBe(true);
    b.step();
    expect(l.out).toBe(0);
    expect(l.trips).toEqual([]);
    expect(b.view('att').bearers).toBeUndefined();
    // A second loss: out again, and no second word of it. The computer's block: out, with no word to anyone.
    hit(b, u, d); hit(b, foe, d);
    b.step();
    expect(l.trips.length).toBe(1);
    expect(b.litter(foe)!.trips.length).toBe(1);
    expect(b.log.filter((e) => e.what === 'land:bearers').length).toBe(1);
  }, 300_000);
  it('more cases than its bearers can carry: they wait, and go as the bearers come home', () => {
    const b = quiet(), u = footOf(b, 'att');
    steps(b, 10);
    hit(b, u, 0.1);
    b.step();
    const l = b.litter(u)!, all = l.all;
    expect(l.out).toBeCloseTo(all, 12);
    expect(l.waiting).toBeCloseTo(0.1 * (1 - killedShare(u.nat)) * 0.83 - all / B.litter, 12);
    const w0 = l.waiting, back = l.trips[0].back;
    while (b.t + 1e-9 < back && !b.over) b.step();
    expect(l.waiting).toBe(w0);
    const went = b.t;
    b.step();
    // Home and straight out again with the next.
    expect(l.trips.length).toBe(1);
    expect(l.out).toBeCloseTo(all, 12);
    expect(l.waiting).toBeCloseTo(w0 - all / B.litter, 12);
    expect(l.trips[0].back - went).toBeCloseTo(TRIP, 2);
  }, 300_000);
});

// ---- (3) its blows while they are out -------------------------------------------------------------------------------

describe('(3) while they are out, the block\'s own blows are the weaker by the men away; nothing else of it changes', () => {
  it('a blow weighs (str - out) / str of the same blow with the switch off', () => {
    const b = quiet(), u = footOf(b, 'att'), foe = footOf(b, 'def');
    steps(b, 10);
    const whole = weigh(b, u, foe);
    hit(b, u, 0.1);
    b.step();
    const l = b.litter(u)!;
    expect(l.out).toBeGreaterThan(0);
    const on = weigh(b, u, foe);
    b.bearers = false;
    const off = weigh(b, u, foe);
    b.bearers = true;
    expect(on / off).toBeCloseTo((u.str - l.out) / u.str, 9);
    expect(off).toBeLessThan(whole);
    // The enemy's blows on it are not touched: its strength is all there to be struck.
    const at = weigh(b, foe, u);
    b.bearers = false;
    expect(weigh(b, foe, u)).toBe(at);
  });
  it('sending them out moves no strength, nerve, size or state of any block, nor the balance', () => {
    const b = quiet(), u = footOf(b, 'att');
    steps(b, 10);
    hit(b, u, 0.005);
    const snap = (): string => JSON.stringify([b.units.map((x) => [x.str, x.mor, x.figures, x.state, x.x, x.y, x.power0]), b.view().balance]);
    const s0 = snap();
    inside(b).carry();
    expect(b.litter(u)!.trips.length).toBe(1);
    expect(snap()).toBe(s0);
    // As the trip ends: the same.
    const back = b.litter(u)!.trips[0].back;
    while (b.t + 1e-9 < back && !b.over) b.step();
    const s1 = snap();
    inside(b).carry();
    expect(b.litter(u)!.trips.length).toBe(0);
    expect(snap()).toBe(s1);
  }, 300_000);
});

// ---- (4) the light cases -------------------------------------------------------------------------------------------

describe('(4) the light cases are back in the line 30 s after they fell, up to its strength at the start', () => {
  it('with the default at 0: back after 30 s, raising its strength by a sixth of the wounded, never its nerve', () => {
    const b = quiet(), u = footOf(b, 'att');
    steps(b, 10);
    const d = 0.05, t0 = b.t;
    hit(b, u, d);
    const s1 = u.str, n = d * (1 - killedShare(u.nat)) * B.light.share, lot = b.litter(u)!.light;
    expect(lot.length).toBe(1);
    expect(lot[0].at).toBe(Math.round((t0 + 30) * 1000) / 1000);
    expect(lot[0].n).toBeCloseTo(n, 12);
    let held = true;
    while (b.t + 1e-9 < t0 + 30 && !b.over) { b.step(); held &&= u.str === s1; }
    expect(held).toBe(true);
    expect(b.returns).toEqual([]);
    const mor = u.mor;
    inside(b).carry();
    expect(u.str).toBeCloseTo(s1 + n, 12);
    expect(u.mor).toBe(mor);
    expect(b.litter(u)!.light).toEqual([]);
    expect(b.returns).toEqual([{ t: Math.round(b.t * 1000) / 1000, unit: u.id, n: u.str - s1 }]);
    const r = b.returns[0], sec = Math.floor(r.t + 1e-9), want = [{ t: r.t, unit: u.id, n: Math.round((r.n * u.men0) / u.str0) }];
    expect(want[0].n).toBeGreaterThan(0);
    // Not shown while its battle second runs (more of the block's may come back in it); shown once it is over.
    expect(b.view('att').returns).toBeUndefined();
    while (Math.floor(b.t + 1e-9) <= sec && !b.over) { expect(b.view('att').returns).toBeUndefined(); b.step(); }
    b.fog = true;
    expect(b.view('att').returns).toEqual(want);
    expect(b.view('def').returns).toBeUndefined();
    // The same entry, unchanged, for the two battle seconds after its own; then told.
    while (Math.floor(b.t + 1e-9) < sec + 3 && !b.over) { expect(b.view('att').returns).toEqual(want); b.step(); }
    expect(b.view('att').returns).toBeUndefined();
    expect(b.returns.length).toBe(1);
  }, 300_000);
  it('a block\'s light cases back in one battle second: one entry, the sum, never changed once shown', () => {
    const b = quiet(), u = footOf(b, 'att');
    steps(b, 10);
    hit(b, u, 0.05);
    steps(b, 3);
    hit(b, u, 0.05);
    const lot = b.litter(u)!.light;
    expect(lot.length).toBe(2);
    expect(Math.floor(lot[0].at + 1e-9)).toBe(Math.floor(lot[1].at + 1e-9));
    const seen = new Map<string, number>();
    let shown = 0;
    for (let k = 0; k < 400 && !b.over; k++) {
      b.step();
      for (const x of b.view('att').returns ?? []) {
        const key = `${x.t}:${x.unit}`;
        if (seen.has(key)) expect(x.n).toBe(seen.get(key));
        else { seen.set(key, x.n); shown += x.n; }
      }
    }
    expect(b.returns.length).toBe(1);
    expect(seen.size).toBe(1);
    expect(shown).toBe(Math.round((b.returns[0].n * u.men0) / u.str0));
  }, 300_000);
  it('never above its strength at the start', () => {
    const b = quiet(), u = footOf(b, 'att');
    steps(b, 10);
    const t0 = b.t;
    hit(b, u, 0.05);
    u.str = u.str0 - 1e-4;
    while (b.t + 1e-9 < t0 + 30 && !b.over) b.step();
    b.step();
    expect(u.str).toBe(u.str0);
    expect(b.returns.map((r) => [r.unit, r.n])).toEqual([[u.id, expect.closeTo(1e-4, 12)]]);
  }, 300_000);
});

// ---- (5) a block that breaks --------------------------------------------------------------------------------------

describe('(5) a routed block takes none back and sends no bearers', () => {
  it('broken before its bearers go: no trip, no light case back, its strength as it was', () => {
    const b = quiet(), u = footOf(b, 'att');
    steps(b, 10);
    const t0 = b.t;
    hit(b, u, 0.05);
    const s1 = u.str, l = b.litter(u)!, w = l.waiting;
    inside(b).rout(u);
    // (Kept running: in a quiet field a broken block clear of the enemy soon rallies, and fights again.)
    let ran = true;
    while (b.t < t0 + 60 && !b.over) { u.mor = 0; b.step(); ran &&= u.state === 'routing' || u.state === 'gone'; }
    expect(b.t).toBeGreaterThan(t0 + 30);
    expect(ran).toBe(true);
    expect(l.trips).toEqual([]);
    expect(l.out).toBe(0);
    expect(l.waiting).toBe(w);
    expect(l.light).toEqual([]);
    expect(u.str).toBe(s1);
    expect(b.returns).toEqual([]);
  }, 300_000);
  it('broken with its bearers out: they still come home, and no more go', () => {
    const b = quiet(), u = footOf(b, 'att');
    steps(b, 10);
    hit(b, u, 0.1);
    b.step();
    const l = b.litter(u)!, w = l.waiting, back = l.trips[0].back;
    inside(b).rout(u);
    let ran = true;
    while (b.t < back + 1 && !b.over) { u.mor = 0; b.step(); ran &&= u.state === 'routing' || u.state === 'gone'; }
    expect(b.t).toBeGreaterThan(back);
    expect(ran).toBe(true);
    expect(l.trips).toEqual([]);
    expect(l.out).toBe(0);
    expect(l.waiting).toBe(w);
  }, 300_000);
});

// ---- (6) by army -----------------------------------------------------------------------------------------------------

describe('(6) the bearers\' share by army', () => {
  it('every army named, France, Vichy and Italy at 0.0275 (the design director\'s middle of 2 to 3.5%), the UK\'s kin with her', () => {
    const want: Record<string, number> = {
      USA: 0.02, UK: 0.025, CAN: 0.025, AUS: 0.025, NZL: 0.025, SAF: 0.025, IND: 0.025, GER: 0.03, SOV: 0.035, JAP: 0.02, ITA: 0.0275, FRA: 0.0275, VIC: 0.0275,
    };
    for (const [nat, s] of Object.entries(want)) { expect(B.share[nat]).toBe(s); expect(bearerShare(nat)).toBe(s); }
    // Every other army: the default, the same knob.
    for (const nat of ['RED', 'BLU', 'CHN', 'POL', 'HUN']) expect(bearerShare(nat)).toBe(0.0275);
    expect(Object.keys(B.share).sort()).toEqual([...Object.keys(want), '_'].sort());
  });
  it('a block\'s bearers: its army\'s share of its strength at the start', () => {
    const b = new LandBattle(sampleTicket('plains', 1.0, { seed: 2, nat: { att: 'SOV', def: 'ITA' } }));
    b.ai.att = b.ai.def = false; b.bearers = true;
    for (const side of ['att', 'def'] as Side[]) {
      const u = footOf(b, side);
      hit(b, u, 0.01);
      expect(b.litter(u)!.all).toBeCloseTo((side === 'att' ? 0.035 : 0.0275) * u.str0, 12);
    }
  });
});

// ---- (7) the fight with the drill: nothing on the dice, the drill looks only ------------------------------------------

const STRETCHER: ReadonlySet<string> = new Set(['carried', 'stretcher_front', 'stretcher_rear']);
const ofMan = (bl: MenBlock, by: number): MenOut | undefined => bl.men.find((m) => m.k === Math.floor(by / 1000) && m.j === by % 1000);
const ahead = (m: MenOut, by: number): [number, number] => [m.x + Math.sin(m.f) * by, m.y + Math.cos(m.f) * by];
const menOf = (d: object): Map<number, number> => new Map([...(d as { squads: Map<number, { men: unknown[] }> }).squads].map(([id, sq]) => [id, sq.men.length]));

/**
 * A whole battle with the bearers on (an American attack the player's, with his staff, on a German defence, plains 1.2
 * seed 4), fought plain, and again with the drill drawing its men (sections and platoons), every draw of the battle's
 * dice counted while the bearers' rules or the drill's bearers are at work. The drill's view each 2nd step (0.2 s) is
 * looked over as it goes: each stretcher (its spacing, its bearers) and each step of it (its pace, its turn).
 */
const fought = (() => {
  const make = (): LandBattle => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { seed: 4, humanAtt: true, nat: { att: 'USA', def: 'GER' } }));
    b.ai.att = b.ai.def = true; b.bearers = true;
    return b;
  };
  const plain = make();
  const s0 = signed(plain, undefined, 30000, false);
  const b = make();
  const d = b.drillOn({ sections: true, platoons: true });
  // Count the dice while the bearers work.
  let depth = 0, draws = 0;
  const calls: Record<string, number> = {};
  const rng = inside(b).rng, next = rng.next.bind(rng);
  rng.next = () => { if (depth) draws++; return next(); };
  const wrap = (o: object, name: string): void => {
    const rec = o as Record<string, (...a: unknown[]) => unknown>, f = rec[name];
    rec[name] = function (this: unknown, ...a: unknown[]) { depth++; calls[name] = (calls[name] ?? 0) + 1; try { return f.apply(this, a); } finally { depth--; } };
  };
  for (const name of ['wounded', 'carry', 'atHand', 'bearerView', 'aidOf', 'litter']) wrap(b, name);
  for (const name of ['hurt', 'rise', 'litters', 'carryStep']) wrap(d, name);
  // The stretchers as drawn: each carried man's (spaced: any amiss), each stretcher man's step to step (paced: any amiss).
  const look = { carried: 0, steps: 0, spaced: [] as string[], paced: [] as string[] };
  const most = 2 * B.pace * 0.1 + 0.15;
  let prev: Map<number, MenBlock> | undefined;
  const watch = (v: MenBlock[]): void => {
    for (const bl of v) {
      const was = prev?.get(bl.id);
      for (const m of bl.men) {
        if (!STRETCHER.has(m.p)) continue;
        if (m.p === 'carried') {
          look.carried++;
          const c = m.cy?.[m.cy.length - 1], f = c && ofMan(bl, c[2]), r = c && ofMan(bl, c[3]);
          const [fx, fy] = ahead(m, 1.12), [rx, ry] = ahead(m, -1.35);
          const ok = !!f && !!r && f.br === 1 && r.br === 1 && f.p === 'stretcher_front' && r.p === 'stretcher_rear' && m.wd !== undefined && m.d !== undefined
            && Math.hypot(f.x - fx, f.y - fy) < 0.2 && Math.hypot(r.x - rx, r.y - ry) < 0.2 && Math.abs(f.f - m.f) < 0.02 && Math.abs(r.f - m.f) < 0.02;
          if (!ok && look.spaced.length < 5) look.spaced.push(JSON.stringify({ block: bl.id, m, f, r }));
        }
        const p = was?.men.find((x) => x.k === m.k && x.j === m.j);
        if (!p || !STRETCHER.has(p.p)) continue;
        look.steps++;
        if ((Math.hypot(m.x - p.x, m.y - p.y) > most || Math.abs(m.f - p.f) >= 0.02) && look.paced.length < 5) look.paced.push(JSON.stringify({ block: bl.id, m, p }));
      }
    }
    prev = new Map(v.map((bl) => [bl.id, bl]));
  };
  const men0 = menOf(d);
  const s1 = signed(b, (x, k) => { if (k % 2 === 0) watch(d.view()); }, 30000, false);
  const end = d.view();
  return { plain, b, d, s0, s1, draws, calls, look, end, men0, men1: menOf(d), rng };
})();

describe('(7) a whole battle: nothing on the dice; the drill looks only', () => {
  it('no draw on the battle\'s dice from the bearers\' rules or the drill\'s bearers, every one of them at work', () => {
    expect(fought.rng).toBe(inside(fought.b).rng);
    for (const name of ['wounded', 'carry', 'atHand', 'bearerView', 'hurt', 'rise', 'litters', 'carryStep']) expect(fought.calls[name] ?? 0, name).toBeGreaterThan(0);
    expect(fought.draws).toBe(0);
  });
  it('the same battle with the men drawn as without them (the drill\'s own shells and craters aside, as on ec6f8e9c)', () => {
    expect(fought.s1).toBe(fought.s0);
    expect(fought.b.returns.length).toBeGreaterThan(0);
    expect(fought.b.units.some((u) => (fought.b.litter(u)?.all ?? 0) > 0)).toBe(true);
  });
  it('one word a block at most, to the player\'s own side only', () => {
    const told = fought.b.log.filter((e) => e.what === 'land:bearers');
    expect(told.length).toBeGreaterThan(0);
    expect(new Set(told.map((e) => e.unit)).size).toBe(told.length);
    expect(told.every((e) => e.side === 'att')).toBe(true);
  });
});

// ---- (8) the drill: the wounded, the stretchers, the men back ---------------------------------------------------------

describe('(8) the drill draws the wounded, the stretchers and the light cases back, its men count untouched', () => {
  const end = fought.end, men = end.flatMap((x) => x.men);
  it('every block keeps its men, man for man', () => {
    expect([...fought.men1]).toEqual([...fought.men0]);
  });
  it('about three in four of the men who fall lie wounded (vehicles and crewed weapons aside); light cases back up after 30 s', () => {
    const fell = men.filter((m) => (m.d !== undefined || m.back !== undefined) && !['tank', 'gun', 'lorry', 'hmg', 'mortar'].includes(m.r));
    const hurt = fell.filter((m) => m.wd !== undefined);
    expect(fell.length).toBeGreaterThan(100);
    expect(hurt.length / fell.length).toBeGreaterThan(0.65);
    expect(hurt.length / fell.length).toBeLessThan(0.85);
    // The killed lie as ever.
    expect(men.some((m) => m.wd === undefined && (m.p === 'wounded' || m.p === 'carried' || m.cy || m.gone !== undefined))).toBe(false);
    // Up again after their dressing, on their feet (or down again later, killed: a man is wounded once).
    const up = men.filter((m) => m.back !== undefined);
    expect(up.length).toBeGreaterThan(0);
    for (const m of up) {
      expect(m.back! - m.wd!).toBeGreaterThanOrEqual(B.light.dress - 0.1);
      expect(m.cy).toBeUndefined();
      if (m.d === undefined) expect(m.p).not.toBe('wounded');
      else { expect(m.d).toBeGreaterThan(m.back!); expect(m.p).toBe('down'); }
    }
  });
  it('the bearers: riflemen of the block, two to a stretcher, as many as its army\'s share carries at once', () => {
    let blocks = 0;
    for (const bl of end) {
      const brs = bl.men.filter((m) => m.br);
      if (!brs.length) continue;
      blocks++;
      expect(brs.length % 2).toBe(0);
      expect(brs.every((m) => m.j !== undefined)).toBe(true);
      const u = fought.b.unit(bl.id)!;
      expect(brs.length / 2).toBeLessThanOrEqual(Math.max(1, Math.round((bearerShare(u.nat) * fought.men0.get(bl.id)!) / B.litter)));
      // Every carry's two bearers are among them.
      for (const m of bl.men) for (const c of m.cy ?? []) { expect(ofMan(bl, c[2])?.br).toBe(1); expect(ofMan(bl, c[3])?.br).toBe(1); }
    }
    expect(blocks).toBeGreaterThan(0);
  });
  it('carried head first: his place the stretcher\'s middle, facing the way, the front bearer 1.12 m ahead and the rear 1.35 m behind', () => {
    expect(fought.look.carried).toBeGreaterThan(0);
    expect(fought.look.spaced).toEqual([]);
  });
  it('the stretcher keeps the bearers\' pace and its way, step to step', () => {
    expect(fought.look.steps).toBeGreaterThan(0);
    expect(fought.look.paced).toEqual([]);
  });
  it('each man carried reaches his block\'s aid post, and lies there', () => {
    const f = fought.b.field;
    let there = 0;
    for (const bl of end) for (const m of bl.men) {
      if (m.gone === undefined) continue;
      there++;
      const [ax, ay] = fought.b.aidOf(fought.b.unit(bl.id)!);
      expect(Math.hypot(m.x - Math.min(f.w - 3, Math.max(3, ax)), m.y - Math.min(f.h - 3, Math.max(3, ay)))).toBeLessThan(0.15);
      expect(m.p).toBe('wounded');
      const [from, to] = m.cy![m.cy!.length - 1];
      expect(to).toBeGreaterThan(from);
      expect(Math.abs(m.gone - to)).toBeLessThanOrEqual(0.2);
    }
    expect(there).toBeGreaterThan(0);
  });
});

// ---- (9) the view by side, and the bridge ------------------------------------------------------------------------------

describe('(9) the view: each side its own blocks\' (all of them with no side, as a recording is); the bridge', () => {
  it('his own bearers, aid posts and men back; the enemy\'s only in a view asked with no side, or unfogged', () => {
    const b = quiet(), u = footOf(b, 'att'), foe = footOf(b, 'def');
    b.fog = true;
    steps(b, 10);
    hit(b, u, 0.05); hit(b, foe, 0.05);
    b.step();
    const sides = (as?: Side): number[] => (b.view(as).aid ?? []).map((a) => a.unit);
    expect(sides('att')).toEqual([u.id]);
    expect(sides('def')).toEqual([foe.id]);
    expect(sides().sort((x, y) => x - y)).toEqual([u.id, foe.id].sort((x, y) => x - y));
    expect((b.view('att').bearers ?? []).map((x) => x.unit)).toEqual([u.id]);
    expect(b.view().aid!.find((a) => a.unit === foe.id)!.side).toBe('def');
    // The defence's aid post: behind it, toward the north edge.
    const a = b.view().aid!.find((x) => x.unit === foe.id)!;
    expect(a.y).toBeCloseTo(Math.min(b.field.h, foe.home[1] + 503), 1);
    // Unfogged (the card knew the defence): every side's, as the telephone lines are.
    b.fog = false;
    expect(sides('att').sort((x, y) => x - y)).toEqual([u.id, foe.id].sort((x, y) => x - y));
    expect(sides('def').sort((x, y) => x - y)).toEqual([u.id, foe.id].sort((x, y) => x - y));
  });
  const rig = () => {
    const g = newCampaign();
    const TC = battleCalls({ game: () => g, attack: (ids, to, f) => ({ report: orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f) }) });
    return { g, TC };
  };
  type V = { me?: string; over: unknown; events: { what: string; unit?: number; side?: string; text: string }[]; units: { id: number; side: string }[]; bearers?: { unit: number }[]; aid?: { side: string; unit: number }[]; returns?: { unit: number }[] };
  type M = { blocks: { id: number; men: Record<string, unknown>[] }[] };
  const run = (TC: ReturnType<typeof rig>['TC'], until: (v: V) => boolean): { v: V; said: V['events'] } => {
    TC.battleSpeed(4);
    let v = TC.battleView() as unknown as V;
    const said: V['events'] = [];
    for (let i = 0; i < 600 && !v.over && !until(v); i++) { v = TC.battleTick(1) as unknown as V; said.push(...v.events); TC.battleMen(); }
    return { v, said };
  };
  it('off ({ bearers: false }): no bearers, aid posts or men back in the view, and no man wounded or bearing', () => {
    const { g, TC } = rig();
    const o = offers(g)[0];
    TC.battleStart(o.corps.map((c) => c.id), o.target.id, { bearers: false });
    TC.battleMen();
    const { v, said } = run(TC, (x) => x.units.some((u) => u.side === 'att') && (x as unknown as { t: number }).t > 400);
    expect(v.bearers ?? v.aid ?? v.returns).toBeUndefined();
    expect(said.some((e) => e.what === 'land:bearers')).toBe(false);
    const men = (TC.battleMen() as unknown as M).blocks.flatMap((x) => x.men);
    expect(men.some((m) => 'wd' in m || 'br' in m || 'cy' in m)).toBe(false);
  }, 300_000);
  it('on ({ bearers: true }): his own blocks\' only, his word, the wounded and the bearers in his men', () => {
    const { g, TC } = rig();
    const o = offers(g)[0];
    const st = TC.battleStart(o.corps.map((c) => c.id), o.target.id, { bearers: true })!;
    const units = st.view.units as { id: number; side: string }[], me = st.view.me as string;
    const mine = new Set(units.filter((u) => u.side === me).map((u) => u.id));
    if (me === 'att') TC.battleOrder({ do: 'attack', units: [...mine], target: units.find((u) => u.side !== me)!.id });
    TC.battleMen();
    let any = false;
    const { said } = run(TC, (x) => {
      for (const a of x.aid ?? []) { expect(a.side).toBe(me); expect(mine.has(a.unit)).toBe(true); }
      for (const a of x.bearers ?? []) expect(mine.has(a.unit)).toBe(true);
      for (const a of x.returns ?? []) expect(mine.has(a.unit)).toBe(true);
      any ||= !!x.bearers?.length;
      return any && (x.returns?.length ?? 0) > 0;
    });
    expect(any).toBe(true);
    const told = said.filter((e) => e.what === 'land:bearers');
    expect(told.length).toBeGreaterThan(0);
    for (const e of told) { expect(mine.has(e.unit!)).toBe(true); expect(e.text).toMatch(/'s stretcher bearers are carrying the wounded back\.$/); }
    const men = (TC.battleMen() as unknown as M).blocks.filter((x) => mine.has(x.id)).flatMap((x) => x.men);
    expect(men.some((m) => m.br === 1)).toBe(true);
    expect(men.some((m) => typeof m.wd === 'number')).toBe(true);
  }, 300_000);
});
