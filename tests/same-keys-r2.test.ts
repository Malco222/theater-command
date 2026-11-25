// Same key, same throw (release 2; the design director's ruling, 25 November 04:55Z; the air thread's search,
// air-rules/same-keys-1125/SAME-KEYS.md rows 3 to 6). A second attack on one province in one month draws the land
// field's own stream afresh, as fortune already did, and never by the view or the hand-off. A province (or a crossing)
// that changes hands twice in one month throws the track, the bridge and the scorch rolls afresh with the missing side;
// the month's first throw keeps its key, so nothing moves where nothing repeats.
import { describe, expect, it, vi } from 'vitest';

// Every key a keyed throw is made from, as it is made (the rules' own hashString, recorded).
const seen = vi.hoisted(() => ({ keys: [] as string[] }));
vi.mock('../src/core/rng', async (orig) => {
  const m = await orig<typeof import('../src/core/rng')>();
  return { ...m, hashString: (s: string): number => { seen.keys.push(s); return m.hashString(s); } };
});

import { Rng, hashString } from '../src/core/rng';
import { LandBattle, sampleTicket } from '../src/battle/land';
import { atWar, capture, createGame, setWar } from '../src/campaign/game';
import { PRIZE_KINDS, hasPrize, prizeInside, prizeOut, setScorchPolicy } from '../src/campaign/prizes';
import { onNet, railCut } from '../src/campaign/rails';
import { blowChance, bridgeDown, crossingsAt, farBank } from '../src/campaign/rivers';
import { fall, invest, lift } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Game, Region } from '../src/campaign/types';

const roll = (key: string): number => new Rng(hashString(key)).next();
/** The keys recorded while `f` runs, those starting with `pre`. */
function keysOf(pre: string, f: () => void): string[] {
  const from = seen.keys.length;
  f();
  return seen.keys.slice(from).filter((k) => k.startsWith(pre));
}
const free = (seed: number): Game => startOpen(createGame(seed, null));

describe('the land field\'s own stream takes the attempt (row 3)', () => {
  const at = (seed: number, attempt: number, humanAtt = false) => ({ ...sampleTicket('plains', 1.2, { seed, humanAtt }), attempt });
  /** Handed to the staff before the first blow: the result's R. */
  const handedAtOnce = (t: ReturnType<typeof at>): number => { const b = new LandBattle(t); b.toDice(); return b.over!.R; };

  it('a second attack on the place this month, handed to the staff before the first blow, draws afresh in most of 12 seeds', () => {
    let differ = 0;
    for (let s = 1; s <= 12; s++) if (handedAtOnce(at(s * 7919 + 13, 0)) !== handedAtOnce(at(s * 7919 + 13, 1))) differ++;
    expect(differ, `${differ} of 12 differ`).toBeGreaterThanOrEqual(10);
  });

  it('attempt 0 draws exactly the stream it always drew: new Rng(ticket.seed)', () => {
    for (const s of [3, 17, 4242]) {
      const t = at(s, 0), b = new LandBattle(t) as unknown as { rng: Rng };
      const want = new Rng(t.seed);
      for (let i = 0; i < 5; i++) expect(b.rng.next()).toBe(want.next());
    }
    // (and attempt 1's is not it)
    const t1 = at(3, 1), b1 = new LandBattle(t1) as unknown as { rng: Rng };
    expect(b1.rng.next()).not.toBe(new Rng(t1.seed).next());
  });

  it('one battle, one draw: attempt 1 built twice gives the same draw handed over at once, and fought by hand to the same point, fog or none', () => {
    for (const s of [5, 29]) {
      // Its stream is the attempt's own, from the ticket's seed and attempt and nothing else.
      const t = at(s, 1), mine = new LandBattle(t) as unknown as { rng: Rng }, want = new Rng(hashString(`field-run:${t.seed}:1`));
      for (let i = 0; i < 3; i++) expect(mine.rng.next()).toBe(want.next());
      // The ticket the player fights by hand (humanAtt) draws that same stream: the play mode is no salt.
      const th = at(s, 1, true), hand = new LandBattle(th) as unknown as { rng: Rng }, same = new Rng(hashString(`field-run:${th.seed}:1`));
      expect(th.seed).toBe(t.seed);
      for (let i = 0; i < 3; i++) expect(hand.rng.next()).toBe(same.next());
      expect(handedAtOnce(at(s, 1))).toBe(handedAtOnce(at(s, 1)));
      const fought = (fog: boolean): number => {
        const b = new LandBattle(at(s, 1, true));
        b.fog = fog;
        b.ai.def = true;
        const mine = b.units.filter((u) => u.side === 'att' && u.kind !== 'fort');
        const foe = b.units.find((u) => u.side === 'def')!;
        b.command({ do: 'attack', units: mine.map((u) => u.id), target: foe.id }, 'att');
        for (let i = 0; i < 400 && !b.over; i++) b.step();
        if (!b.over) b.toDice();
        return b.over!.R;
      };
      const R = fought(false);
      expect(fought(false)).toBe(R);
      expect(fought(true)).toBe(R);
    }
  });
});

describe('the four low keys take the missing side on a second change of hands in the month (rows 4, 5 and 6)', () => {
  it('rails capture (row 4): the track\'s throw for a province retaken in the month is its own; a single capture\'s is today\'s', () => {
    const g = free(1);
    let met = 0;
    for (const r of g.regions) {
      if (met >= 3) break;
      if (r.ctrl !== 'POL' || !onNet(r.id) || railCut(g, r.id)) continue;
      const old = `rail:${g.seed}:${g.turn}:${r.id}`;
      const first = keysOf(`rail:${g.seed}:`, () => capture(g, r, 'GER'));
      expect(first).toEqual([old]);
      if (railCut(g, r.id)) continue;
      const second = keysOf(`rail:${g.seed}:`, () => capture(g, r, 'POL'));
      // (The side driven out: Germany.)
      expect(second).toEqual([`${old}:GER`]);
      expect(roll(second[0])).not.toBe(roll(first[0]));
      met++;
    }
    expect(met).toBeGreaterThan(0);
  });

  it('a bridge (row 5): a crossing thrown for again in the month, by its own bank or the far one, throws its own; the first is today\'s', () => {
    /** A Polish crossing whose bridges Germany would seize whole, taking `first` bank (the throw's own key read ahead). */
    const whole = (g: Game, far: boolean): { r: Region; b: Region; old: string } => {
      for (const r of g.regions) {
        if (r.ctrl !== 'POL') continue;
        for (const c of crossingsAt(r.id)) {
          const b = g.regions[farBank(c, r.id)], old = `bridge:${g.seed}:${g.turn}:${c.a}:${c.b}`;
          if (b.ctrl === 'POL' && !bridgeDown(g, c) && roll(old) >= blowChance(g, c, far ? b : r, 'GER')) return { r, b, old };
        }
      }
      throw new Error('no crossing');
    };
    {
      // Its own bank taken, taken back, and taken again.
      const g = free(1), { r, old } = whole(g, false);
      expect(keysOf(old, () => capture(g, r, 'GER'))).toEqual([old]);
      capture(g, r, 'POL');
      const again = keysOf(old, () => capture(g, r, 'GER'));
      // (The captor: Germany.)
      expect(again).toEqual([`${old}:GER`]);
      expect(roll(again[0])).not.toBe(roll(old));
    }
    {
      // The far bank taken and taken back, then this one taken.
      const g = free(1), { r, b, old } = whole(g, true);
      expect(keysOf(old, () => capture(g, b, 'GER'))).toEqual([old]);
      capture(g, b, 'POL');
      const then = keysOf(old, () => capture(g, r, 'GER'));
      expect(then).toEqual([`${old}:GER`]);
      expect(roll(then[0])).not.toBe(roll(old));
    }
  });

  /** Both sides burn everywhere: every loss throws for the prizes standing. */
  const scorching = (seed: number): Game => {
    const g = free(seed);
    for (const n of ['GER', 'POL']) setScorchPolicy(g, n, 'everywhere');
    return g;
  };
  const standing = (g: Game, r: Region, outside: boolean): string[] =>
    PRIZE_KINDS.filter((k) => k !== 'junction' && hasPrize(g, r, k) && prizeOut(g, r, k) <= 0 && prizeInside(g, r, k) !== outside);

  it('two scorch rolls (row 6a): a province lost whole twice in the month burns on its own dice; the first loss on today\'s', () => {
    const g = scorching(1);
    let met = 0;
    for (const r of g.regions) {
      if (met >= 2) break;
      if (r.ctrl !== 'POL' || !standing(g, r, true).length || g.flags[`siege:${r.id}`] !== undefined) continue;
      const pre = `prize:${g.seed}:${g.turn}:${r.id}:`;
      const first = keysOf(pre, () => fall(g, r, 'GER', 'empty'));
      for (const k of first) expect(k.split(':').length).toBe(5);
      const left = standing(g, r, true).filter((k) => first.includes(`${pre}${k}`));
      if (!left.length) continue;
      const second = keysOf(pre, () => fall(g, r, 'POL', 'empty'));
      for (const k of left) {
        // (The side driven off the second time: Germany.)
        const again = second.find((x) => x.startsWith(`${pre}${k}:`) || x === `${pre}${k}`);
        expect(again).toBe(`${pre}${k}:GER`);
        expect(roll(again!)).not.toBe(roll(`${pre}${k}`));
      }
      met++;
    }
    expect(met).toBeGreaterThan(0);
  });

  it('a siege lifted by relief and begun again in the month (row 6a): the second loss of the field burns on its own dice', () => {
    const g = scorching(1);
    const ger = Object.keys(g.nations).indexOf('GER');
    let met = 0;
    for (const r of g.regions) {
      if (met >= 2) break;
      if (r.ctrl !== 'POL' || !standing(g, r, true).length || g.flags[`siege:${r.id}`] !== undefined) continue;
      const pre = `prize:${g.seed}:${g.turn}:${r.id}:`;
      // Besieged by Germany since last month, relieved by Poland now: the besiegers burn what they held.
      g.flags[`siege:${r.id}`] = g.turn - 1;
      g.flags[`sgby:${r.id}`] = ger;
      const first = keysOf(pre, () => lift(g, r, 'POL'));
      expect(first.length).toBeGreaterThan(0);
      for (const k of first) expect(k.split(':').length).toBe(5);
      const left = standing(g, r, true).filter((k) => first.includes(`${pre}${k}`));
      if (!left.length) continue;
      const second = keysOf(pre, () => invest(g, r, 'GER'));
      for (const k of left) {
        // (The side driven off the field: its holder, Poland.)
        const again = second.find((x) => x.startsWith(`${pre}${k}:`) || x === `${pre}${k}`);
        expect(again).toBe(`${pre}${k}:POL`);
        expect(roll(again!)).not.toBe(roll(`${pre}${k}`));
      }
      met++;
    }
    expect(met).toBeGreaterThan(0);
  });

  it('the scorch roll on the railway (row 6b): a province retaken in the month tears up the track on its own dice', () => {
    const g = scorching(1);
    let met = 0;
    for (const r of g.regions) {
      if (met >= 2) break;
      if (r.ctrl !== 'POL' || !onNet(r.id) || railCut(g, r.id)) continue;
      const old = `prize:${g.seed}:${g.turn}:${r.id}:rail`;
      const first = keysOf(old, () => capture(g, r, 'GER'));
      if (railCut(g, r.id) || !first.length) continue;
      expect(first).toEqual([old]);
      const second = keysOf(old, () => capture(g, r, 'POL'));
      expect(second).toEqual([`${old}:GER`]);
      expect(roll(second[0])).not.toBe(roll(old));
      met++;
    }
    expect(met).toBeGreaterThan(0);
  });

  it('a later start (row 4): ground laid or handed over as the campaign opens is no change of hands: its first capture keeps today\'s key', () => {
    const g = createGame(1, null);
    while (g.turn < 10) runTurnAI(g);
    startOpen(g);
    expect(g.flags.openFrom).toBe(g.turn);
    // (In a month-10 start the Baltic states and Bessarabia carry this month, with no capture behind them.)
    const laid = g.regions.filter((r) => r.since === g.turn && g.flags[`fellP:${r.id}`] === undefined && r.ctrl === 'SOV' && onNet(r.id) && !railCut(g, r.id));
    expect(laid.length).toBeGreaterThan(0);
    setWar(g, 'GER', 'SOV');
    let met = 0;
    for (const r of laid) {
      const old = `rail:${g.seed}:${g.turn}:${r.id}`;
      expect(keysOf(`rail:${g.seed}:`, () => capture(g, r, 'GER'))).toEqual([old]);
      if (railCut(g, r.id)) continue;
      expect(keysOf(`rail:${g.seed}:`, () => capture(g, r, 'SOV'))).toEqual([`${old}:GER`]);
      met++;
    }
    expect(met).toBeGreaterThan(0);
  });

  it('taken whole, then besieged by a third side in the month (row 6a): what lies outside burns on the new holder\'s own dice', () => {
    const g = scorching(1);
    setWar(g, 'FRA', 'GER');
    let met = 0;
    for (const r of g.regions) {
      if (met >= 2) break;
      if (r.ctrl !== 'POL' || !standing(g, r, true).length || g.flags[`siege:${r.id}`] !== undefined) continue;
      const pre = `prize:${g.seed}:${g.turn}:${r.id}:`;
      const first = keysOf(pre, () => fall(g, r, 'GER', 'empty'));
      for (const k of first) expect(k.split(':').length).toBe(5);
      const left = standing(g, r, true).filter((k) => first.includes(`${pre}${k}`));
      if (!left.length) continue;
      const second = keysOf(pre, () => invest(g, r, 'FRA'));
      for (const k of left) expect(second).toContain(`${pre}${k}:GER`);
      for (const k of second) expect(k.split(':').length).toBe(6);
      met++;
    }
    expect(met).toBeGreaterThan(0);
  });

  it('relieved, then lost whole in the month (row 6a): only what lay outside takes the side; what lies inside keeps its key', () => {
    let met = 0;
    // (A town with prizes both outside and inside its walls, whose field the besiegers driven off did not burn: Bordeaux
    // or Marseille, in the first seeds that leave one standing.)
    for (let seed = 1; seed <= 12 && met < 2; seed++) {
      const g = scorching(seed);
      setWar(g, 'FRA', 'GER');
      setScorchPolicy(g, 'FRA', 'everywhere');
      const ger = Object.keys(g.nations).indexOf('GER');
      for (const r of g.regions) {
        if (met >= 2) break;
        if ((r.ctrl !== 'POL' && r.ctrl !== 'FRA') || !standing(g, r, true).length || !standing(g, r, false).length || g.flags[`siege:${r.id}`] !== undefined) continue;
        const pre = `prize:${g.seed}:${g.turn}:${r.id}:`, holder = r.ctrl;
        // Besieged by Germany since last month, relieved by its holder now: the besiegers throw for the field.
        g.flags[`siege:${r.id}`] = g.turn - 1;
        g.flags[`sgby:${r.id}`] = ger;
        const first = keysOf(pre, () => lift(g, r, holder));
        for (const k of first) expect(k.split(':').length).toBe(5);
        const out = standing(g, r, true).filter((k) => first.includes(`${pre}${k}`)), inside = standing(g, r, false);
        if (!out.length) continue;
        // Then lost whole, no siege standing: the holder throws for everything, the field a second time.
        const second = keysOf(pre, () => fall(g, r, 'GER', 'field'));
        for (const k of out) expect(second).toContain(`${pre}${k}:${holder}`);
        for (const k of inside) expect(second).toContain(`${pre}${k}`);
        for (const k of inside) expect(second).not.toContain(`${pre}${k}:${holder}`);
        met++;
      }
    }
    expect(met).toBeGreaterThan(0);
  });

  // A change of hands that threw nothing leaves the month's first throw to come: it keeps its key (the checkers' gate
  // runs, seed 2 month 49 and seeds 1 and 2's first bridge differences: a first throw salted after such a change).
  it('a province taken by a side at peace with its holder, then taken in war (row 4): the track\'s throw is the month\'s first, today\'s key', () => {
    const g = free(1);
    expect(atWar(g, 'SOV', 'POL')).toBe(false);
    setWar(g, 'GER', 'SOV');
    let met = 0;
    for (const r of g.regions) {
      if (met >= 3) break;
      if (r.ctrl !== 'POL' || !onNet(r.id) || railCut(g, r.id)) continue;
      expect(keysOf(`rail:${g.seed}:`, () => capture(g, r, 'SOV'))).toEqual([]);
      expect(keysOf(`rail:${g.seed}:`, () => capture(g, r, 'GER'))).toEqual([`rail:${g.seed}:${g.turn}:${r.id}`]);
      met++;
    }
    expect(met).toBeGreaterThan(0);
  });

  it('taken whole from a side that burns nothing, then besieged by a third side (row 6a): what lies outside is thrown for the first time, today\'s key', () => {
    const g = scorching(1);
    setScorchPolicy(g, 'POL', 'never');
    setWar(g, 'FRA', 'GER');
    let met = 0;
    for (const r of g.regions) {
      if (met >= 2) break;
      if (r.ctrl !== 'POL' || !standing(g, r, true).length || g.flags[`siege:${r.id}`] !== undefined) continue;
      const pre = `prize:${g.seed}:${g.turn}:${r.id}:`;
      expect(keysOf(pre, () => fall(g, r, 'GER', 'empty'))).toEqual([]);
      const second = keysOf(pre, () => invest(g, r, 'FRA'));
      expect(second.length).toBeGreaterThan(0);
      for (const k of standing(g, r, true)) expect(second).toContain(`${pre}${k}`);
      for (const k of second) expect(k.split(':').length).toBe(5);
      met++;
    }
    expect(met).toBeGreaterThan(0);
  });

  it('a bridge whose bank was taken by a side at peace with the far bank (row 5): its first throw of the month keeps today\'s key', () => {
    const g = free(1);
    setWar(g, 'GER', 'SOV');
    let met = 0;
    for (const r of g.regions) {
      if (met >= 2 || r.ctrl !== 'POL') continue;
      for (const c of crossingsAt(r.id)) {
        const b = g.regions[farBank(c, r.id)], old = `bridge:${g.seed}:${g.turn}:${c.a}:${c.b}`;
        if (b.ctrl !== 'POL' || bridgeDown(g, c) || met >= 2) continue;
        // The Red Army takes this bank (at peace with Poland: no throw); then Germany takes it from the Red Army, at
        // war with Poland across the river: the crossing's first throw of the month.
        expect(keysOf(old, () => capture(g, r, 'SOV'))).toEqual([]);
        expect(keysOf(old, () => capture(g, r, 'GER'))).toEqual([old]);
        met++;
        break;
      }
    }
    expect(met).toBeGreaterThan(0);
  });
});
