// ALL FALL BACK (release 1.5, { do: 'land:fallBack' }) meets fighting on after the break (release 2, the switch
// `pursue`, off). The 3D pre-check read three meetings of the two (the assembler, 26 November 2026, 18:47Z); the design
// director ruled them at 21:22Z, and the reasons the player reads are the UI director's of 21:27Z:
//   (1) the army that is falling back breaks: the winner is asked as ever and the verdict is unchanged; the player whose
//       army broke may still choose its way back and hurry it, as his screen tells him (the aftermath's rule stands in
//       the place of "falling back, the army takes no new orders");
//   (2) the winner is the one falling back when the enemy breaks: he IS asked (the offer reads the field, never his
//       standing order). END ends at the break, as today. PURSUE is a new order: it replaces ALL FALL BACK for his
//       blocks that pursue; a computer winner that is falling back chooses by its own sight, alike;
//   (3) ALL FALL BACK is never an order once an army has broken: not while the winner chooses, not in a pursuit (his
//       way to stop is END), not for the broken army (it is falling back already).
// How (2) comes about in the game: a player's orders go by radio and land seconds after he gives them (LAND.radio), so
// an enemy on the brink can break while his ALL FALL BACK is still on the air to some or all of his blocks. With
// `pursue` off none of this is reached, and the battle is as it was.
import { describe, expect, it } from 'vitest';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { LandBattle } from '../src/battle/land/sim';
import type { Command, Side } from '../src/battle/core/types';

type U = { id: number; side: Side; kind: string; x: number; y: number; str: number; state: string; taken?: boolean; order: { kind: string; x: number; y: number; via?: boolean; target?: number } | null };
const other = (s: Side): Side => (s === 'att' ? 'def' : 'att');
const units = (sim: LandBattle): U[] => sim.units as unknown as U[];
const STEPS = 40000;
const FALL = { do: 'land:fallBack' } as unknown as Command;
const quarry = (sim: LandBattle, u: U): U | null => (sim as unknown as { quarry(u: U): U | null }).quarry(u);
const onAir = (sim: LandBattle, s: Side): number => (sim as unknown as { pending: { id: number; smoke?: true }[] }).pending.filter((p) => !p.smoke && units(sim).find((u) => u.id === p.id)?.side === s).length;
const inOrder = (u: U): boolean => u.state === 'steady' || u.state === 'wavering';

/**
 * A set piece with both staffs fighting and the switch as given. With `human` in a seat it is seated as the game seats
 * a player (live.ts): his own orders go by radio, and the blocks he orders are his until he hands them back.
 */
function open(key: string, seed: number, human: Side | null, pursue: boolean): LandBattle {
  const sim = new LandBattle(ticketOf(PIECES.find((x) => x.key === key)!, seed, human));
  sim.ai = { att: true, def: true };
  sim.pursue = pursue;
  if (human) { sim.radio = true; sim.perUnit = true; }
  return sim;
}

/** Fights until the battle is over or `stop` says so, `who` ordering ALL FALL BACK the first tick `when` holds. Whether it was taken. */
function fight(sim: LandBattle, who: Side, when: (s: LandBattle) => boolean, stop: (s: LandBattle) => boolean = () => false): boolean {
  let ordered: boolean | null = null;
  for (let i = 0; i < STEPS && !sim.over && !stop(sim); i++) {
    if (ordered === null && !sim.aftermath && when(sim)) ordered = sim.command(FALL, who);
    sim.step();
  }
  return ordered === true;
}
const broke = (s: LandBattle): boolean => !!s.aftermath;

interface Found { key: string; seed: number; me: Side; at: number }
const SEEDS = 6;

/** (1) The first battle in which a player's army, ordered back once its bar stands at `at`, breaks to a computer general that pursues. */
function fallingBreaks(): Found | null {
  for (const p of PIECES) for (let seed = 1; seed <= SEEDS; seed++) for (const me of ['att', 'def'] as Side[]) for (const at of [0.7, 0.5, 0.85]) {
    const sim = open(p.key, seed, me, true);
    const ok = fight(sim, me, (s) => s.contactAt !== null && s.breaking(me) >= at, broke);
    if (ok && sim.aftermath?.phase === 'pursue' && sim.aftermath.side === me && sim.fallingBack(me) && units(sim).some((u) => u.side === me && inOrder(u) && u.kind !== 'fort')) return { key: p.key, seed, me, at };
  }
  return null;
}

/**
 * (2) The first battle a player wins by breaking the other army in which, had he ordered ALL FALL BACK `at` seconds
 * before, the enemy still breaks: `mixed` false, with every one of his orders still on the air; true, with some of his
 * blocks already falling back that can catch one of the broken army's, and some orders still on the air.
 */
function winnerFalling(mixed: boolean): Found | null {
  for (const p of PIECES) for (let seed = 1; seed <= SEEDS; seed++) for (const me of ['att', 'def'] as Side[]) {
    const base = open(p.key, seed, me, true);
    for (let i = 0; i < STEPS && !base.over && !base.aftermath; i++) base.step();
    if (base.aftermath?.winner !== me) continue;
    const T = base.aftermath.t;
    for (const d of mixed ? [7, 6, 8] : [2]) {
      const sim = open(p.key, seed, me, true);
      const ok = fight(sim, me, (s) => s.t >= T - d - 1e-6, broke);
      if (!ok || sim.aftermath?.winner !== me || sim.aftermath.phase !== 'ask' || !sim.fallingBack(me)) continue;
      const chasers = units(sim).filter((u) => u.side === me && inOrder(u) && u.order?.kind === 'withdraw' && quarry(sim, u)).length;
      if (mixed ? chasers > 0 && onAir(sim, me) > 0 : chasers === 0 && onAir(sim, me) > 0 && !units(sim).some((u) => u.side === me && u.order?.kind === 'withdraw')) return { key: p.key, seed, me, at: T - d };
    }
  }
  return null;
}

describe('ALL FALL BACK meets the pursuit', () => {
  const ONE = fallingBreaks(), AIR = winnerFalling(false), MIXED = winnerFalling(true);
  const asked = (c: Found): LandBattle => {
    const sim = open(c.key, c.seed, c.me, true);
    expect(fight(sim, c.me, (s) => s.t >= c.at - 1e-6, broke)).toBe(true);
    expect(sim.over).toBeNull();
    expect(sim.aftermath).toMatchObject({ side: other(c.me), winner: c.me, phase: 'ask' });
    return sim;
  };

  it('finds the meetings among the set pieces', () => {
    expect(ONE).not.toBeNull();
    expect(AIR).not.toBeNull();
    expect(MIXED).not.toBeNull();
  });

  it('(1) the falling army breaks: the verdict stands, and its player may still choose its way back', () => {
    const c = ONE!, me = c.me;
    const sim = open(c.key, c.seed, me, true);
    expect(fight(sim, me, (s) => s.contactAt !== null && s.breaking(me) >= c.at, broke)).toBe(true);
    expect(sim.aftermath).toMatchObject({ side: me, winner: other(me), phase: 'pursue' });
    expect(sim.fallingBack(me)).toBe(true);
    // Its blocks in order are falling back, as the break's own line and his order both have them.
    const mine = units(sim).filter((u) => u.side === me && inOrder(u) && u.kind !== 'fort');
    expect(mine.length).toBeGreaterThan(0);
    for (const u of mine) expect(u.order?.kind).toBe('withdraw');
    // ALL FALL BACK is no longer his to give, and the reason is the broken army's.
    expect(sim.fallBackWhy(me)).toBe('Your army has broken and is already falling back.');
    expect(sim.command(FALL, me)).toBe(false);
    // The aftermath's own words to him are true: he chooses a block's way back (never forward), and may hurry it.
    expect(sim.afterWhy(me)).toBe('Your army has broken: it can only fall back. Choose its way back, or hurry it.');
    const u = mine[0], x = Math.min(sim.field.w - 200, u.x + 150);
    expect(sim.command({ do: 'move', units: [u.id], x, y: sim.field.h / 2, run: true } as Command, me)).toBe(true);
    // (by radio, as any order of his: it lands within the radio's longest wait)
    for (let i = 0; i < 500 && !sim.over && inOrder(u) && !u.order?.via; i++) sim.step();
    if (!sim.over && inOrder(u)) expect(u.order).toMatchObject({ kind: 'withdraw', via: true, y: me === 'att' ? 0 : sim.field.h });
    // He may not attack, nor hand the decided day to the dice.
    const foe = units(sim).find((e) => e.side !== me && inOrder(e));
    if (foe && !sim.over) expect(sim.command({ do: 'attack', units: [u.id], target: foe.id } as Command, me)).toBe(false);
    if (!sim.over) expect(sim.command({ do: 'dice' }, me)).toBe(false);
    for (let i = 0; i < STEPS && !sim.over; i++) sim.step();
    // The verdict is the break's: his army broke, and the other won.
    expect(sim.over).toMatchObject({ winner: other(me), how: 'rout', broke: me });
    expect(sim.over!.pursued).toBeDefined();
  });

  it('(2) the winner who is falling back is asked; END is the battle as it ends with the switch off', () => {
    for (const c of [AIR!, MIXED!]) {
      const me = c.me, sim = asked(c);
      expect(sim.fallingBack(me)).toBe(true);
      // While he chooses, ALL FALL BACK's reason is the choice before him, by the broken army's own name.
      const after = sim.view().after!;
      expect(sim.fallBackWhy(me)).toBe(`${after.broke} has broken: pursue it, or end the battle.`);
      expect(sim.command(FALL, me)).toBe(false);
      expect(sim.fallBackWhy(other(me))).toBe('Your army has broken and is already falling back.');
      expect(sim.command(FALL, other(me))).toBe(false);
      expect(sim.command({ do: 'land:end' } as Command, me)).toBe(true);
      const off = open(c.key, c.seed, me, false);
      expect(fight(off, me, (s) => s.t >= c.at - 1e-6)).toBe(true);
      expect(JSON.stringify(sim.over)).toBe(JSON.stringify(off.over));
      expect(JSON.stringify(sim.log)).toBe(JSON.stringify(off.log));
    }
  });

  it('(2) PURSUE with his order still on the air: it is void, no block of his turns back, and his staff runs the pursuit', () => {
    const c = AIR!, me = c.me, sim = asked(c);
    const mine = units(sim).filter((u) => u.side === me && inOrder(u) && u.kind !== 'fort');
    expect(onAir(sim, me)).toBeGreaterThan(0);
    // (ALL FALL BACK took every block of his into his hand, as any order of his own does)
    for (const u of mine) expect(u.taken).toBe(true);
    expect(sim.command({ do: 'land:pursue' } as Command, me)).toBe(true);
    expect(sim.aftermath!.phase).toBe('pursue');
    expect(sim.fallingBack(me)).toBe(false);
    expect(onAir(sim, me)).toBe(0);
    // The blocks the fall back took are his staff's again.
    for (const u of mine) expect(u.taken).toBeUndefined();
    // In a pursuit, ALL FALL BACK is not the way to stop: END is.
    expect(sim.fallBackWhy(me)).toBe('Your army is pursuing: to stop, call off the pursuit.');
    expect(sim.command(FALL, me)).toBe(false);
    expect(sim.fallingBack(me)).toBe(false);
    // Past the radio's longest wait: none of his blocks was sent back, and his staff has sent some after the broken army.
    let sent = false;
    for (let i = 0; i < 400 && !sim.over; i++) {
      sim.step();
      for (const u of mine) if (inOrder(u)) expect(u.order?.kind).not.toBe('withdraw');
      if (mine.some((u) => u.order?.kind === 'attack')) sent = true;
    }
    expect(sent).toBe(true);
    if (!sim.over) {
      expect(sim.command({ do: 'land:end' } as Command, me)).toBe(true);
      expect(sim.over).toMatchObject({ winner: me, how: 'rout' });
      expect(sim.over!.pursued).toMatchObject({ ended: 'off' });
    }
  });

  it('(2) PURSUE with blocks already falling back: those that can catch something give up their way back, the rest keep it', () => {
    const c = MIXED!, me = c.me, sim = asked(c);
    const mine = units(sim).filter((u) => u.side === me && inOrder(u) && u.kind !== 'fort');
    const back = mine.filter((u) => u.order?.kind === 'withdraw');
    const chasers = back.filter((u) => quarry(sim, u)), rest = back.filter((u) => !quarry(sim, u));
    expect(chasers.length).toBeGreaterThan(0);
    expect(onAir(sim, me)).toBeGreaterThan(0);
    expect(sim.command({ do: 'land:pursue' } as Command, me)).toBe(true);
    expect(sim.fallingBack(me)).toBe(false);
    expect(onAir(sim, me)).toBe(0);
    for (const u of chasers) expect(u.order).toBeNull();
    for (const u of rest) expect(u.order?.kind).toBe('withdraw');
    for (const u of mine) expect(u.taken).toBeUndefined();
    // His own orders are taken again: the pursuit is his to run too.
    const u = chasers[0], e = quarry(sim, u)!;
    expect(sim.command({ do: 'attack', units: [u.id], target: e.id } as Command, me)).toBe(true);
    // No block of his that can catch something is turned back to its edge, and his staff sends them on.
    let sent = false;
    for (let i = 0; i < 400 && !sim.over; i++) {
      sim.step();
      for (const b of chasers) if (inOrder(b) && quarry(sim, b)) expect(b.order?.kind).not.toBe('withdraw');
      if (mine.some((b) => b !== u && b.order?.kind === 'attack')) sent = true;
    }
    expect(sent).toBe(true);
  });

  it('(2) a block that was his before ALL FALL BACK stays his after PURSUE', () => {
    const c = AIR!, me = c.me;
    const sim = open(c.key, c.seed, me, true);
    // (he takes one block in hand a minute before, with an order of his own)
    let mineId = -1;
    for (let i = 0; i < STEPS && !sim.over && !sim.aftermath; i++) {
      if (mineId < 0 && sim.t >= c.at - 60) { const u = units(sim).find((b) => b.side === me && inOrder(b) && b.kind === 'inf')!; mineId = u.id; expect(sim.command({ do: 'hold', units: [u.id] }, me)).toBe(true); }
      if (mineId >= 0 && !sim.fallingBack(me) && sim.t >= c.at - 1e-6) expect(sim.command(FALL, me)).toBe(true);
      sim.step();
    }
    // (taking a block in hand changes the fight: only where the enemy still breaks with him falling back is there anything to read)
    if (!sim.aftermath || sim.aftermath.winner !== me || sim.aftermath.phase !== 'ask' || !sim.fallingBack(me)) return;
    const own = units(sim).find((b) => b.id === mineId)!;
    if (!inOrder(own)) return;
    expect(sim.command({ do: 'land:pursue' } as Command, me)).toBe(true);
    expect(own.taken).toBe(true);
    for (const u of units(sim)) if (u.side === me && u.id !== mineId && inOrder(u) && u.kind !== 'fort') expect(u.taken).toBeUndefined();
  });

  it('(2) a computer winner that is falling back chooses by its own sight, and pursuing falls back no longer', () => {
    // (A computer general's orders land at once, so its fall back stops its fire and the enemy no longer breaks by it:
    // the meeting is set by hand. Nobody plays; the winner-to-be is ordered back a second before the break, and the
    // blocks of the other army still in order are then worn by hand, one by one, until its bar is full.)
    const key = PIECES[0].key, seed = 1;
    const base = open(key, seed, null, true);
    for (let i = 0; i < STEPS && !base.over && !base.aftermath; i++) base.step();
    expect(base.aftermath).not.toBeNull();
    const w = base.aftermath!.winner, l = other(w), T = base.aftermath!.t;
    const sim = open(key, seed, null, true);
    for (let i = 0; i < STEPS && !sim.over && !sim.aftermath && sim.t < T - 1 - 1e-6; i++) sim.step();
    expect(sim.aftermath).toBeNull();
    expect(sim.command(FALL, w)).toBe(true);
    expect(sim.fallingBack(w)).toBe(true);
    const mine = units(sim).filter((u) => u.side === w && inOrder(u) && u.kind !== 'fort');
    expect(mine.length).toBeGreaterThan(0);
    for (const u of mine) expect(u.order?.kind).toBe('withdraw');
    for (const u of units(sim).filter((b) => b.side === l && inOrder(b) && b.kind !== 'fort')) {
      if (sim.breaking(l) >= 1) break;
      u.str *= 0.5;
    }
    expect(sim.breaking(l)).toBeGreaterThanOrEqual(1);
    sim.step();
    // It sees blocks it can catch, so it pursues, and then falls back no longer.
    expect(sim.aftermath).toMatchObject({ side: l, winner: w, phase: 'pursue' });
    expect(sim.fallingBack(w)).toBe(false);
    const chasers = mine.filter((u) => inOrder(u) && quarry(sim, u));
    expect(chasers.length).toBeGreaterThan(0);
    for (const u of chasers) expect(u.order?.kind).not.toBe('withdraw');
    let sent = false;
    for (let i = 0; i < 300 && !sim.over; i++) {
      sim.step();
      if (mine.some((u) => u.order?.kind === 'attack')) sent = true;
      for (const u of chasers) if (inOrder(u) && quarry(sim, u)) expect(u.order?.kind).not.toBe('withdraw');
    }
    expect(sent).toBe(true);
  });
});
