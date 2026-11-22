// The three switches of release 2 (Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it off
// if someone doesnt want it"), measured: each paired on against off, within the tenth, on the five grounds the relief's
// measure used (tests/battle-land-relief.test.ts, its section 10), at odds 1.0 and 1.4, with the player on the attack
// and on the defence, every battle fought by both staffs. Skipped unless LONG=1. THREE names the switches to measure
// (a comma list: THREE=phones, THREE=phones,bearers); THREE_SEEDS the seeds a case (default 80). The tenth is held
// only from 40 seeds a case: below that the measure runs and prints, a smoke of itself (THREE_SEEDS=4), and a win share
// moves by a quarter on one battle.
//
// A switch is measured here once it is in SWITCHES by its name, which is all a new one needs.
import { describe, expect, it } from 'vitest';
import type { FieldTicket, Side } from '../src/battle/core';
import { LandBattle, sampleTicket, withTeams, type LandPlace } from '../src/battle/land';

type T = FieldTicket<LandPlace>;
const env = (globalThis as { process?: { env: Record<string, string | undefined>; stdout?: { write(s: string): void } } }).process;
const LONG = !!env?.env.LONG;
const SEEDS = Math.max(1, Math.floor(Number(env?.env.THREE_SEEDS ?? 80)) || 80);
const WANT = (env?.env.THREE ?? '').split(',').map((s) => s.trim()).filter(Boolean);

/** A switch as the measure fights it. */
interface Switch {
  /** The ticket both battles of a pair are fought on (what the switch needs on the field to do anything at all). */
  ticket?: (t: T) => T;
  /** Set on both battles of a pair, before the switch. */
  base?: (b: LandBattle) => void;
  /** The switch itself: set on the 'on' battle alone. */
  on: (b: LandBattle) => void;
  /** What it did in an 'on' battle, counted, summed over a case and printed beside it. */
  note?: (b: LandBattle) => Record<string, number>;
}

const SWITCHES: Record<string, Switch> = {
  // Field telephone lines. The practice armies carry no guns, so both battles of a pair carry the support teams: the
  // attack's mortar team is what cuts a wire with a blow (its tanks cut by crossing one). The wires are the defence's.
  phones: {
    ticket: (t) => withTeams(t),
    on: (b) => { b.phones = true; },
    note: (b) => ({ wires: b.wires.length, cuts: b.wireCuts.length, mends: b.wireMends.length, held: b.wireHeld.length }),
  },
  // The others go here by name as they land, as: bearers: { on: (b) => { b.bearers = true; } }, and
  // patrol: { base: (b) => { b.hide = true; }, on: (b) => { ... } } (both battles of a patrol pair with hide on).
};

/** The relief measure's five grounds. */
const GROUNDS: [string, (seed: number, odds: number) => T][] = [
  ['plains', (seed, odds) => sampleTicket('plains', odds, { seed })],
  ['forest', (seed, odds) => sampleTicket('forest', odds, { seed })],
  ['hills', (seed, odds) => sampleTicket('hills', odds, { seed })],
  ['city', (seed, odds) => sampleTicket('urban', odds, { seed, town: 'city' })],
  ['river', (seed, odds) => sampleTicket('plains', odds, { seed, river: true })],
];

/** The player's side on the ticket: his flag, as a played battle carries it (both staffs fight either way). */
const played = (t: T, side: Side): T => ({ ...t, att: { ...t.att, human: side === 'att' }, def: { ...t.def, human: side === 'def' } });

function fight(sw: Switch, t: T, on: boolean): LandBattle {
  const b = new LandBattle(sw.ticket ? sw.ticket(t) : t);
  b.ai.att = b.ai.def = true;
  sw.base?.(b);
  if (on) sw.on(b);
  for (let k = 0; !b.over && k < 30000; k++) b.step();
  return b;
}

describe('the three switches, each paired on against off: the attack\'s win share moves within a tenth (a measure: LONG=1)', () => {
  // Only what THREE names is fought; with LONG alone, each switch is listed and skipped.
  for (const name of Object.keys(SWITCHES)) {
    const sw = SWITCHES[name];
    it.runIf(LONG && WANT.includes(name))(`${name}: every ground, each odds, the player on either side, ${SEEDS} seeds`, () => {
      const rows: string[] = [], bad: string[] = [];
      env?.stdout?.write(`\n${name}, ${SEEDS} seeds a case: the attack's wins off and on (share), the shift, the mean end in seconds off and on\n`);
      for (const [ground, make] of GROUNDS) for (const odds of [1.0, 1.4]) for (const side of ['att', 'def'] as Side[]) {
        let off = 0, on = 0, endOff = 0, endOn = 0;
        const notes: Record<string, number> = {};
        for (let seed = 1; seed <= SEEDS; seed++) {
          const t = played(make(seed, odds), side);
          const a = fight(sw, t, false), b = fight(sw, t, true);
          off += a.over?.winner === 'att' ? 1 : 0; on += b.over?.winner === 'att' ? 1 : 0;
          endOff += a.over?.seconds ?? a.t; endOn += b.over?.seconds ?? b.t;
          for (const [k, v] of Object.entries(sw.note?.(b) ?? {})) notes[k] = (notes[k] ?? 0) + v;
        }
        const shift = (on - off) / SEEDS;
        const row = `${ground.padEnd(6)} odds ${odds.toFixed(1)} player ${side === 'att' ? 'attack ' : 'defence'}: attack wins off ${(off / SEEDS).toFixed(3)}, on ${(on / SEEDS).toFixed(3)}, shift ${shift >= 0 ? '+' : ''}${shift.toFixed(3)}` +
          ` | mean end off ${Math.round(endOff / SEEDS)} s, on ${Math.round(endOn / SEEDS)} s` +
          (Object.keys(notes).length ? ` | on, a battle: ${Object.entries(notes).map(([k, v]) => `${k} ${(v / SEEDS).toFixed(1)}`).join(', ')}` : '');
        rows.push(row);
        env?.stdout?.write(`${row}\n`);
        if (Math.abs(shift) > 0.1) bad.push(row);
      }
      expect(rows.length).toBe(GROUNDS.length * 2 * 2);
      if (SEEDS >= 40) expect(bad, bad.join('\n')).toEqual([]);
    }, 7_200_000);
  }
  // A name THREE asks for that this build has not got: said, never silently passed.
  for (const name of WANT.filter((n) => !SWITCHES[n])) it.runIf(LONG)(`${name}: not in this build`, () => {
    env?.stdout?.write(`\n${name}: no such switch in this build (SWITCHES has ${Object.keys(SWITCHES).join(', ')})\n`);
  });
});
