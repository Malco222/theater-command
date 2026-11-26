// Fighting on after the break (release 2, the switch `pursue`, off; sim.pursue; sim.ts "After the break").
//
// Brian, 26 November 2026, 03:53Z: "i feel like the battles end abruptly" and "if the enemy retreats, i have the option
// to continue on with the battle or end it". The design director's rulings of 03:58Z, 06:36Z, 12:16Z and 12:37Z:
//   - switch off, the battle is as it was; the verdict is fixed at the break;
//   - END at the break is today's bill exactly;
//   - under PURSUE a block of the broken army on the field at the break is billed what it has when it leaves the field
//     or the day ends, with no flat share on top, and one that left before the break keeps its share as under END;
//   - the broken side's runners run and its blocks in order fall back, the player picking the way, never forward; he
//     may hurry, not end; a computer general that won chooses at its own sight, never a fixed END;
//   - the day's clock ends all.
// What is tested here is that, and that the game hears of it (the live recording's views and its last delta).
import { describe, expect, it } from 'vitest';
import { livePiece, type LiveDelta } from '../src/battle/land/live';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { LAND } from '../src/battle/land/rules';
import { LandBattle } from '../src/battle/land/sim';
import type { Command, FieldResult, Side } from '../src/battle/core/types';

type U = {
  id: number; side: Side; kind: string; x: number; y: number; str: number; str0: number; power0: number; state: string;
  order: { kind: string; x: number; y: number; via?: boolean; run?: boolean } | null; engaged?: number; fellBack?: boolean;
};
const other = (s: Side): Side => (s === 'att' ? 'def' : 'att');
const units = (sim: LandBattle): U[] => sim.units as unknown as U[];
const STEPS = 40000;

/** A set piece with both staffs fighting, `human` in the seat named (his staff fights for him), the switch as given. */
function open(key: string, seed: number, human: Side | null, pursue: boolean): LandBattle {
  const sim = new LandBattle(ticketOf(PIECES.find((x) => x.key === key)!, seed, human));
  sim.ai = { att: true, def: true };
  sim.pursue = pursue;
  return sim;
}

/** Fights on until the battle is over, or `stop` says so. */
function run(sim: LandBattle, stop: (s: LandBattle) => boolean = () => false): void {
  for (let i = 0; i < STEPS && !sim.over && !stop(sim); i++) sim.step();
}

const asked = (s: LandBattle): boolean => s.aftermath?.phase === 'ask';

interface Case { key: string; seed: number; winner: Side; off: FieldResult; t: number }

/** The first battles, by piece and seed, that a player in one seat wins by breaking the other army (switch off). */
function cases(want: number): Case[] {
  const out: Case[] = [];
  for (const p of PIECES) for (let seed = 1; seed <= 12 && out.length < want; seed++) for (const me of ['att', 'def'] as Side[]) {
    const sim = open(p.key, seed, me, false);
    run(sim);
    const o = sim.over;
    if (o && o.how === 'rout' && o.broke === other(me) && units(sim).some((u) => u.side === other(me) && u.state !== 'gone' && u.state !== 'withdrawn')) { out.push({ key: p.key, seed, winner: me, off: o, t: sim.t }); break; }
  }
  return out;
}

/** The first battles a player LOSES by his army breaking to a computer general that pursues (switch on). */
function lost(want: number): { sim: LandBattle; me: Side }[] {
  const out: { sim: LandBattle; me: Side }[] = [];
  for (const p of PIECES) for (let seed = 1; seed <= 12 && out.length < want; seed++) for (const me of ['att', 'def'] as Side[]) {
    const sim = open(p.key, seed, me, true);
    run(sim, (s) => !!s.aftermath);
    if (sim.aftermath?.phase === 'pursue' && sim.aftermath.side === me) { out.push({ sim, me }); break; }
  }
  return out;
}

const CASES = cases(3);

describe('fighting on after the break (pursue)', () => {
  it('finds battles a player wins by breaking the other army', () => {
    expect(CASES.length).toBeGreaterThanOrEqual(2);
  });

  it('asks the player who won, and does not step until he answers', () => {
    for (const c of CASES) {
      const sim = open(c.key, c.seed, c.winner, true);
      run(sim, asked);
      expect(sim.over).toBeNull();
      expect(sim.aftermath).toMatchObject({ side: other(c.winner), winner: c.winner, how: 'rout', phase: 'ask' });
      // The break came at the very second it ends the battle with the switch off.
      expect(sim.t).toBe(c.t);
      const before = JSON.stringify(units(sim));
      for (let i = 0; i < 20; i++) sim.step();
      expect(sim.t).toBe(c.t);
      expect(JSON.stringify(units(sim))).toBe(before);
      // Nothing is ordered while he chooses, by either side; the loser may not answer for him.
      const mine = units(sim).filter((u) => u.side === c.winner && u.state === 'steady').map((u) => u.id);
      expect(sim.command({ do: 'move', units: mine, x: 1500, y: 1000 }, c.winner)).toBe(false);
      expect(sim.command({ do: 'dice' }, c.winner)).toBe(false);
      expect(sim.command({ do: 'withdrawAll', side: c.winner }, c.winner)).toBe(false);
      expect(sim.command({ do: 'land:pursue' } as Command, other(c.winner))).toBe(false);
      expect(sim.command({ do: 'land:end' } as Command, other(c.winner))).toBe(false);
      expect(JSON.stringify(units(sim))).toBe(before);
      const after = sim.view().after!;
      expect(after).toMatchObject({ side: other(c.winner), winner: c.winner, phase: 'ask' });
      // (the break's second, never before it and within a hundredth)
      expect(after.t).toBeGreaterThanOrEqual(c.t);
      expect(after.t - c.t).toBeLessThan(0.0101);
      // The two armies by the rules' own names, for the screen's line; no dispatch asks (the break's own line stands).
      expect(after.broke).toMatch(/ army$/);
      expect(after.by).toMatch(/ army$/);
      expect(after.broke).not.toBe(after.by);
      expect(sim.log.some((e) => e.what === 'land:ask')).toBe(false);
      // An order refused while he chooses says he must choose first; the other side's refusal is not the aftermath's to explain.
      expect(sim.afterWhy(c.winner)).toBe(`Choose first: pursue ${after.broke[0].toLowerCase()}${after.broke.slice(1)}, or end the battle.`);
      expect(sim.afterWhy(other(c.winner))).toBeNull();
    }
  });

  it('END at the break is the battle as it ends with the switch off, to the last figure', () => {
    for (const c of CASES) {
      const sim = open(c.key, c.seed, c.winner, true);
      run(sim, asked);
      expect(sim.command({ do: 'land:end' } as Command, c.winner)).toBe(true);
      expect(JSON.stringify(sim.over)).toBe(JSON.stringify(c.off));
      expect(sim.aftermath).toBeNull();
      expect(sim.view().after).toBeUndefined();
      // And everything said, to the last word: the question is the screen's, never a dispatch.
      const off = open(c.key, c.seed, c.winner, false);
      run(off);
      expect(JSON.stringify(sim.log)).toBe(JSON.stringify(off.log));
      expect(sim.afterWhy(c.winner)).toBeNull();
    }
  });

  it('PURSUE fights on with the verdict fixed: the broken army only falls back, and is billed what it has when it leaves', () => {
    for (const c of CASES) {
      const sim = open(c.key, c.seed, c.winner, true);
      run(sim, asked);
      const loser = other(c.winner), on = new Map(sim.aftermath!.on);
      const goneBefore = units(sim).filter((u) => u.side === loser && !on.has(u.id));
      const mine0 = units(sim).filter((u) => u.side === c.winner).reduce((a, u) => a + u.str, 0);
      expect(sim.command({ do: 'land:pursue' } as Command, c.winner)).toBe(true);
      expect(sim.aftermath!.phase).toBe('pursue');
      expect(sim.log[sim.log.length - 1]).toMatchObject({ what: 'land:pursuit', text: 'Your army takes up the pursuit.' });
      // (his own orders in a pursuit are refused for their own reasons, never the aftermath's)
      expect(sim.afterWhy(c.winner)).toBeNull();
      // He cannot hand a decided day to the staff, nor pull his whole army out of it.
      expect(sim.command({ do: 'dice' }, c.winner)).toBe(false);
      expect(sim.command({ do: 'withdrawAll', side: c.winner }, c.winner)).toBe(false);
      let ticks = 0;
      for (; ticks < STEPS && !sim.over; ticks++) {
        sim.step();
        if (sim.over) break;
        for (const u of units(sim)) {
          if (u.side !== loser) continue;
          // In order, it falls back and strikes nothing; running, it runs; and none of it rallies.
          if (u.state === 'steady' || u.state === 'wavering') { expect(u.order?.kind).toBe('withdraw'); expect(u.engaged).toBeUndefined(); }
          else expect(['routing', 'gone', 'withdrawn']).toContain(u.state);
        }
      }
      const o = sim.over!;
      expect(o).not.toBeNull();
      expect(ticks).toBeGreaterThan(1);
      // The verdict as the break had it.
      expect(o.winner).toBe(c.off.winner);
      expect(o.how).toBe('rout');
      expect(o.broke).toBe(loser);
      expect(o.brokeBy).toEqual(c.off.brokeBy);
      expect(o.R >= 1.2).toBe(c.off.R >= 1.2);
      // (the first line still tells the break by its own second, not the day's last)
      expect(o.lines[0]).toBe(c.off.lines[0]);
      expect(o.seconds).toBeGreaterThanOrEqual(c.off.seconds);
      expect(sim.t).toBeLessThanOrEqual(sim.ticket.limit + 1);
      // The broken army struck nothing after the break (no fighters on call in these battles).
      expect(units(sim).filter((u) => u.side === c.winner).reduce((a, u) => a + u.str, 0)).toBeCloseTo(mine0, 9);
      // The bill: its blocks on the field at the break by what they have, no flat share; those gone before by their share.
      const P = (sim as unknown as { power: Record<Side, number> }).power[loser];
      let want = 0, took = 0;
      for (const u of units(sim)) {
        if (u.side !== loser) continue;
        const was = on.get(u.id);
        if (was !== undefined) { want += u.power0 * (1 - u.str / u.str0); took += u.power0 * Math.max(0, was - u.str) / u.str0; }
        else want += u.power0 * (1 - (u.state === 'gone' ? u.str * (1 - LAND.count.gone[loser]) : u.str) / u.str0);
      }
      expect(o.lost[loser]).toBeCloseTo(want / P, 9);
      expect(o.pursued).toBeDefined();
      expect(o.pursued!.at).toBeCloseTo(c.t, 1);
      expect(o.pursued!.seconds).toBe(Math.round(sim.t - c.t));
      expect(o.pursued!.took).toBeCloseTo(took / P, 3);
      expect(['left', 'off', 'time', 'spent']).toContain(o.pursued!.ended);
      // The after card: how long he pursued and what it cost them, under the break's line; the strength line counts both.
      expect(o.lines[1]).toMatch(/^Your army pursued (the|their) .*army for (half a minute|a minute( and a half)?|\w+( and a half)? minutes) (but did it no more harm|and cost it [\d,]+ more men, (about \d+%|less than 1%) of its strength|and cost it (about \d+%|less than 1%) more of its strength)\.$/);
      if (o.pursued!.took >= 1e-4 && o.pursued!.men >= 1) expect(o.lines[1]).toContain(` ${o.pursued!.men.toLocaleString('en-GB')} more men, `);
      expect(o.lines.some((l) => l.includes('counting the rout and the pursuit'))).toBe(c.off.lines.some((l) => l.includes('counting the rout')));
      // And its last dispatch, by how it ended.
      const said = sim.log.filter((e) => e.what === 'land:pursuit').map((e) => e.text);
      if (o.pursued!.ended === 'time') expect(said[said.length - 1]).toBe('The day is done, and the pursuit with it.');
      if (o.pursued!.ended === 'left') expect(said[said.length - 1]).toMatch(/^The last of (the|their) .*army has left the field\.$/);
      if (o.pursued!.ended === 'spent') expect(said[said.length - 1]).toBe('Your army can pursue no further, and the pursuit ends.');
      // (a block that had fled before the break was not on the field at it)
      for (const u of goneBefore) expect(['gone', 'withdrawn']).toContain(u.state);
    }
  });

  it('with the pursuit\'s own clock (pursueLate) it may run past the day\'s, and ends a day\'s length from the break at the latest', () => {
    // The design director, 26 November 2026, 18:04Z: the latest end is the break plus the ticket's own limit, both
    // sides alike, no new figure; off, the day's clock ends it as first built.
    for (const c of CASES) {
      const day = open(c.key, c.seed, c.winner, true);
      run(day, asked);
      expect(day.afterView().after!.until).toBeUndefined();
      day.command({ do: 'land:pursue' } as Command, c.winner);
      run(day);
      const late = open(c.key, c.seed, c.winner, true);
      late.pursueLate = true;
      run(late, asked);
      const until = late.aftermath!.t + late.ticket.limit;
      expect(late.aftermath!.t).toBeCloseTo(day.over!.pursued!.at, 1);
      expect(late.afterView().after).toMatchObject({ phase: 'ask', until: Math.round(until * 100) / 100 });
      late.command({ do: 'land:pursue' } as Command, c.winner);
      run(late);
      const o = late.over!;
      expect(o).not.toBeNull();
      expect(o.winner).toBe(c.off.winner);
      expect(o.how).toBe('rout');
      expect(o.pursued).toBeDefined();
      expect(late.t).toBeLessThanOrEqual(until + 1);
      // Where the day's clock cut the pursuit short, its own clock lets it run on: to the field's own end, or to a
      // day's length from the break.
      if (day.over!.pursued!.ended === 'time') {
        expect(late.t).toBeGreaterThanOrEqual(day.t);
        if (o.pursued!.ended === 'time') {
          expect(late.t).toBeGreaterThanOrEqual(until - 1);
          const said = late.log.filter((e) => e.what === 'land:pursuit').map((e) => e.text);
          expect(said[said.length - 1]).toBe('The pursuit has run its course.');
        }
      } else {
        // (ended by the field before the day's clock: the same battle either way)
        expect(JSON.stringify(o)).toBe(JSON.stringify(day.over));
      }
    }
  });

  it('the player who pursues may call it off, and the day ends there by the pursuit\'s own count', () => {
    const c = CASES[0];
    const sim = open(c.key, c.seed, c.winner, true);
    run(sim, asked);
    sim.command({ do: 'land:pursue' } as Command, c.winner);
    for (let i = 0; i < 25 && !sim.over; i++) sim.step();
    if (sim.over) { expect(sim.over.pursued).toBeDefined(); return; }
    expect(sim.command({ do: 'land:pursue' } as Command, c.winner)).toBe(false);
    expect(sim.command({ do: 'land:end' } as Command, other(c.winner))).toBe(false);
    expect(sim.command({ do: 'land:end' } as Command, c.winner)).toBe(true);
    expect(sim.over).not.toBeNull();
    expect(sim.over!.pursued).toMatchObject({ ended: 'off', seconds: Math.round(sim.t - c.t) });
    expect(sim.over!.winner).toBe(c.off.winner);
    const said = sim.log.filter((e) => e.what === 'land:pursuit').map((e) => e.text);
    expect(said[said.length - 1]).toBe('Your army calls off the pursuit.');
  });

  it('a player whose army broke may pick the way back and hurry, and no more', () => {
    const found = lost(1);
    expect(found.length).toBe(1);
    const { sim, me } = found[0], them = other(me);
    const edge = me === 'att' ? 0 : sim.field.h;
    const u = units(sim).find((x) => x.side === me && (x.state === 'steady' || x.state === 'wavering'))!;
    expect(u).toBeDefined();
    const foe = units(sim).find((x) => x.side === them && x.state === 'steady')!;
    expect(sim.command({ do: 'attack', units: [u.id], target: foe.id }, me)).toBe(false);
    expect(sim.command({ do: 'hold', units: [u.id] }, me)).toBe(false);
    expect(sim.command({ do: 'line', units: [u.id], x0: u.x - 50, y0: u.y, x1: u.x + 50, y1: u.y }, me)).toBe(false);
    expect(sim.command({ do: 'dice' }, me)).toBe(false);
    expect(sim.command({ do: 'withdrawAll', side: me }, me)).toBe(false);
    expect(sim.command({ do: 'land:end' } as Command, me)).toBe(false);
    expect(sim.over).toBeNull();
    // Why, in the general's note; and the dispatch that told him who pursues and where to go.
    expect(sim.afterWhy(me)).toBe('Your army has broken: it can only fall back. Choose its way back, or hurry it.');
    expect(sim.afterWhy(them)).toBeNull();
    expect(sim.log.filter((e) => e.what === 'land:pursuit')[0].text).toMatch(/^(The|Their) .*army takes up the pursuit\. Get your men back to your own edge of the field\.$/);
    // A point FORWARD of it and to one side: it goes back all the same, by that side, at the double.
    const forward = me === 'att' ? Math.min(sim.field.h - 20, u.y + 400) : Math.max(20, u.y - 400);
    const aside = Math.max(100, Math.min(sim.field.w - 100, u.x + (u.x < sim.field.w / 2 ? 300 : -300)));
    expect(sim.command({ do: 'move', units: [u.id], x: aside, y: forward, run: true }, me)).toBe(true);
    // (his staff fights for him in this battle and the order may go by radio: it lands, and stays a way back)
    const y0 = u.y;
    for (let i = 0; i < 100 && !sim.over && u.order?.via !== true && u.state === 'steady'; i++) sim.step();
    if (!sim.over && u.state === 'steady') {
      expect(u.order).toMatchObject({ kind: 'withdraw', y: edge, via: true, run: true });
      for (let i = 0; i < 50 && !sim.over && u.state === 'steady'; i++) sim.step();
      if (me === 'att') expect(u.y).toBeLessThanOrEqual(y0 + 1e-6); else expect(u.y).toBeGreaterThanOrEqual(y0 - 1e-6);
    }
    run(sim);
    expect(sim.over!.pursued).toBeDefined();
    expect(sim.over!.broke).toBe(me);
    expect(sim.over!.lines.some((l) => /^(The|Their) .*army pursued yours for /.test(l))).toBe(true);
  });

  it('two computer generals: the winner chooses at its own sight, and a battle it ends at the break is the battle with the switch off', () => {
    let pursued = 0, ended = 0;
    for (const p of PIECES) for (let seed = 21; seed <= 26; seed++) {
      const off = open(p.key, seed, null, false), on = open(p.key, seed, null, true);
      run(off);
      run(on);
      expect(on.over).not.toBeNull();
      expect(on.t).toBeLessThanOrEqual(on.ticket.limit + 1);
      if (on.over!.pursued) {
        pursued++;
        expect(on.over!.winner).toBe(off.over!.winner);
        expect(on.over!.broke).toBe(off.over!.broke);
        expect(on.over!.R >= 1.2).toBe(off.over!.R >= 1.2);
      } else {
        ended++;
        expect(JSON.stringify(on.over)).toBe(JSON.stringify(off.over));
        expect(JSON.stringify(on.log)).toBe(JSON.stringify(off.log));
      }
    }
    // Never a fixed END: some of these it pursues.
    expect(pursued).toBeGreaterThan(0);
    expect(pursued + ended).toBe(PIECES.length * 6);
  });
});

describe('the live battle after the break (pursue)', () => {
  /** A live battle the player in `me` wins by breaking the other army, fought to the question; null where he does not. */
  function toAsk(key: string, seed: number, me: Side): { L: ReturnType<typeof livePiece>; deltas: LiveDelta[] } | null {
    const L = livePiece(key, seed, { me, pursue: true });
    const deltas: LiveDelta[] = [];
    for (let i = 0; i < 4000 && !L.sim.over && L.sim.aftermath?.phase !== 'ask'; i++) deltas.push(JSON.parse(JSON.stringify(L.tick(5, 100000))));
    return L.sim.aftermath?.phase === 'ask' ? { L, deltas } : null;
  }
  const c = CASES[0];

  it('tells the game the winner is asked, holds until he answers, and hands back END or the pursuit', () => {
    let found: ReturnType<typeof toAsk> = null, at: { key: string; seed: number; me: Side } | null = null;
    search: for (const tryIt of [{ key: c.key, seed: c.seed, me: c.winner }, ...PIECES.flatMap((p) => [1, 2, 3, 4].flatMap((seed) => (['att', 'def'] as Side[]).map((me) => ({ key: p.key, seed, me }))))]) {
      found = toAsk(tryIt.key, tryIt.seed, tryIt.me);
      if (found) { at = tryIt; break search; }
    }
    expect(found).not.toBeNull();
    const { L, deltas } = found!;
    // The last view fought says who is asked.
    const views = deltas.flatMap((d) => d.views);
    expect(views[views.length - 1].after).toMatchObject({ winner: at!.me, phase: 'ask' });
    expect((views[views.length - 1].after as { broke: string }).broke).toMatch(/ army$/);
    // (no dispatch asks: the screen does, from the view)
    expect(deltas.some((d) => d.events.some((e) => e.what === 'land:ask'))).toBe(false);
    // An order given before he has chosen is refused, and says he must choose first.
    expect(L.order({ do: 'hold', units: [] } as Command)).toBe(false);
    expect(L.afterWhy()).toMatch(/^Choose first: pursue .*army, or end the battle\.$/);
    // Asked again and again, the rules fight nothing more.
    const t0 = L.sim.t;
    const idle = L.tick(5, 100000);
    expect(idle.t).toBe(Math.round(t0 * 100) / 100);
    expect(idle.views.length).toBe(0);
    expect(idle.over).toBeUndefined();
    // (the game reads `ask` off the delta's head, straight after t, and stops asking for more)
    expect(idle.ask).toBe(true);
    expect(JSON.stringify(idle).startsWith(`{"t":${idle.t},"ask":true,`)).toBe(true);
    expect(deltas[deltas.length - 1].ask).toBe(true);
    expect(deltas.slice(0, -1).some((d) => d.ask)).toBe(false);
    // END: the last delta carries the result, and it is the result with the switch off.
    const off = livePiece(at!.key, at!.seed, { me: at!.me });
    while (!off.sim.over) off.tick(5, 100000);
    expect(L.order({ do: 'land:end' } as Command)).toBe(true);
    const last = L.tick(5, 100000);
    expect(JSON.stringify(last.over)).toBe(JSON.stringify(off.sim.over));
    expect(last.end).toBe(Math.round(t0 * 100) / 100);
    expect(last.views[last.views.length - 1].over).toBeDefined();
    expect(last.events.some((e) => e.what === 'end')).toBe(true);
    // PURSUE, the same battle again: it fights on, the views say so, and the result says what the pursuit took.
    const again = toAsk(at!.key, at!.seed, at!.me)!;
    expect(again.L.order({ do: 'land:pursue' } as Command)).toBe(true);
    const more: LiveDelta[] = [];
    for (let i = 0; i < 4000 && !again.L.sim.over; i++) more.push(JSON.parse(JSON.stringify(again.L.tick(5, 100000))));
    expect(more[0].t).toBeGreaterThan(Math.round(t0 * 100) / 100);
    expect(more.some((d) => d.ask)).toBe(false);
    expect(more.flatMap((d) => d.views).some((v) => (v.after as { phase?: string } | undefined)?.phase === 'pursue')).toBe(true);
    const over = more[more.length - 1].over as FieldResult;
    expect(over.pursued).toBeDefined();
    expect(over.winner).toBe(off.sim.over!.winner);
  }, 240000);
});
