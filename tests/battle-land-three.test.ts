// The three ground ideas of release 2 together (Brian, 22 November 17:33Z: "lets do all three but make it where we can
// toggle it off if someone doesnt want it"; the design director's rulings, 18:36Z): field telephone lines
// (LandBattle.phones), stretcher bearers (LandBattle.bearers) and the night patrol (LandBattle.patrol, which needs
// hiding), all on in one battle. Each switch's own file holds its rules and its off-identity
// (tests/battle-land-phones.test.ts, tests/battle-land-bearers.test.ts, tests/battle-land-patrol.test.ts); this one holds
// that the three live side by side: the battle runs to its end with each of them at work, the same seed replays it to
// the last number, and the drill draws the lineman and the stretchers together without touching the fight.
import { describe, expect, it } from 'vitest';
import type { FieldEvent } from '../src/battle/core';
import { LandBattle, sampleTicket, withTeams } from '../src/battle/land';
import { hashString, type Rng } from '../src/core/rng';

/**
 * The forest, America attacking Germany (two lines to each German wire), the support teams on both sides (the attack's
 * mortar team cuts wires; the sample armies have no guns of their own on the attack), the player on the defence so that
 * every event of the three is said (the wires' and the mends', his bearers', and the patrol's warning when it is caught),
 * both staffs fighting; hiding on and all three switches on.
 */
function three(seed: number, o: { drill?: boolean } = {}): LandBattle {
  const t = withTeams(sampleTicket('forest', 1.3, { seed, nat: { att: 'USA', def: 'GER' } }));
  t.def.human = true;
  const b = new LandBattle(t);
  b.ai.att = b.ai.def = true;
  b.hideOn();
  b.phones = true; b.bearers = true; b.patrol = true;
  if (o.drill) b.drillOn({ sections: true, platoons: true });
  return b;
}

/** The battle's own dice (private), read only to see that a replay leaves them in the same place. */
const dice = (b: LandBattle): number => (b as unknown as { rng: Rng }).rng.state;

/** What the three switches hold, at a moment: the wires, their cuts, mends and held orders, the bearers' returns, the patrol. */
const held = (b: LandBattle): unknown => ({
  wires: b.wires.map((w) => [w.id, w.unit, w.lines, w.cut, w.out ? w.out.man : null]),
  cuts: b.wireCuts, mends: b.wireMends, waits: b.wireHeld, returns: b.returns, patrol: b.nightPatrol,
});

/**
 * A battle fought to its end and everything it showed and did, digested: each step's view (both sides' and the whole
 * field's for the three switches' keys), every event said, every block, the three switches' records and the dice.
 */
function signed(b: LandBattle): { sig: string; said: FieldEvent[] } {
  const said: FieldEvent[] = [];
  let run = 0;
  for (let k = 0; !b.over && k < 30000; k++) {
    b.step();
    const v = b.view();
    said.push(...v.events);
    if (k % 10 === 0) {
      const mine = b.view('def'), theirs = b.view('att');
      run = hashString(`${run}|${JSON.stringify([v.phones, v.bearers, v.aid, v.returns, v.patrol, mine.phones, mine.bearers, mine.patrol, theirs.phones, theirs.bearers, theirs.patrol])}`);
    }
  }
  const sig = JSON.stringify({
    over: b.over, t: b.t, rng: dice(b), run,
    units: b.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged ?? null, u.order, u.hid ?? null, u.sprung ?? null, u.found ?? null]),
    held: held(b),
    said: said.map((e) => [e.t, e.what, e.unit ?? null, e.text, e.extra ?? null]),
    log: b.log.map((e) => [e.t, e.what, e.unit ?? null, e.text, e.extra ?? null]),
  });
  return { sig, said };
}

const count = (b: LandBattle, what: string): number => b.log.filter((e) => e.what === what).length;

describe('the three ground ideas on together, with hiding: phones, bearers and the night patrol', () => {
  // Seed 1: the patrol finds a hidden post; seed 4: it is caught, and the defence is alerted (one more hidden post).
  for (const [seed, found, caught] of [[1, true, false], [4, false, true]] as const) {
    it(`seed ${seed}: the battle runs to its end, each of the three at work, and replays to the last number`, () => {
      const a = three(seed), one = signed(a);
      // To its end, without error.
      expect(a.over).not.toBeNull();
      expect(a.over!.seconds).toBeGreaterThan(0);
      expect(() => { a.view(); a.view('att'); a.view('def'); }).not.toThrow();

      // Telephone lines: every block of the German defence on the field has a wire of two lines; some are cut, a lineman
      // mends them, and the player's defence is told.
      expect(a.wires.length).toBeGreaterThan(0);
      for (const w of a.wires) { expect(w.side).toBe('def'); expect(w.lines).toBe(2); }
      expect(a.wireCuts.length).toBeGreaterThan(0);
      expect(a.wireMends.length).toBeGreaterThan(0);
      expect(count(a, 'land:wire')).toBeGreaterThan(0);
      expect(count(a, 'land:mend')).toBeGreaterThan(0);

      // Stretcher bearers: his blocks' bearers go out (told once a block), and light cases are back in the line, never
      // above a block's strength at the start.
      expect(count(a, 'land:bearers')).toBeGreaterThan(0);
      expect(a.returns.length).toBeGreaterThan(0);
      for (const u of a.units) expect(u.str).toBeLessThanOrEqual(u.str0 + 1e-9);

      // Night patrol: the computer's attack sent one, from a block of its own on foot.
      const p = a.nightPatrol!;
      expect(p).not.toBeNull();
      expect(a.unit(p.unit)?.side).toBe('att');
      expect(!!p.found).toBe(found);
      expect(p.alerted).toBe(caught);
      if (found) {
        // The post it found was revealed by the patrol, before any shot.
        const f = a.log.find((e) => e.what === 'land:found' && (e.extra as { how?: string } | undefined)?.how === 'patrol');
        expect(f?.unit).toBe(p.found!.unit);
      }
      if (caught) {
        // One more hidden post for the alerted defence, named in the player's warning; the patrol's men lost.
        expect(p.post).not.toBeNull();
        expect(a.log.some((e) => e.what === 'land:patrol' && e.side === 'def' && e.unit === p.post)).toBe(true);
        expect(p.lost).toBeGreaterThan(0);
      }

      // The same seed again: the same battle, to the last number.
      const b = three(seed), two = signed(b);
      expect(two.sig).toBe(one.sig);
      expect(digestOf(two.sig)).toBe(digestOf(one.sig));
    }, 300_000);
  }

  it('another seed is another battle (the signature sees the difference)', () => {
    expect(signed(three(2)).sig).not.toBe(signed(three(1)).sig);
  }, 300_000);

  it('the drill draws the linemen and the stretchers together, and the fight is the same with the men drawn as without', () => {
    // 2000 steps (200 battle seconds): wires cut and mended, bearers out and back, light cases up again.
    const N = 2000;
    const plain = three(1), drawn = three(1, { drill: true });
    let ln = 0, br = 0, wd = 0, cy = 0, back = 0;
    for (let k = 0; k < N && !plain.over; k++) {
      plain.step(); drawn.step();
      if (k % 25 === 0) {
        for (const blk of drawn.drill!.view()) {
          ln += blk.ln?.length ?? 0;
          for (const m of blk.men) { if (m.br) br++; if (m.wd !== undefined) wd++; if (m.cy) cy++; if (m.back !== undefined) back++; }
        }
        // Looks only: the drill never moves the fight (the blocks, the wires, the bearers' returns, the patrol).
        expect(JSON.stringify(drawn.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y]))).toBe(JSON.stringify(plain.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y])));
        expect(JSON.stringify(held(drawn))).toBe(JSON.stringify(held(plain)));
      }
    }
    expect(ln).toBeGreaterThan(0);
    expect(br).toBeGreaterThan(0);
    expect(wd).toBeGreaterThan(0);
    expect(cy).toBeGreaterThan(0);
    expect(back).toBeGreaterThan(0);
  }, 600_000);
});

/** A short name for a long signature, for the failure message. */
function digestOf(s: string): string { return hashString(s).toString(16).padStart(8, '0') + hashString('#' + s).toString(16).padStart(8, '0'); }
