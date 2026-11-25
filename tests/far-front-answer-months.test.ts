// Held armies answer an attack by land, read over months of a war nobody plays (the design director's ruling B11 of
// 25 November 2026, design-director/pacific-japan-1125.md: "every held row on that front is freed for that front's
// defence (not beyond it), as b641 does from the sea").
//
// The skeptic's findings of 25 November 2026 on the piece as first built, which the one-month tests of
// far-front-answer did not reach: with Japan and the Soviet Union at war from the first day, Japan's 6th Army, a held
// row answering in Manchukuo, was lifted off the quay at Chinchow in the second month and ended it in Tokyo; and the
// Transbaikal district's army at Chita, a held row all the while, followed the Red Army's own offensive over the
// river to Heiho in the second month and stood at Tatung in China in the fifth. Here the staff's own answer is read
// each month as its fighting opens (ai.ts `answerWatch`), and every row that answered is read again when that
// staff's month is over: on the ground it was tied to, a held row again, and never moved onto ground that is not its
// side's own (a country of its side's, or ground its side held on the first day).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { answerWatch } from '../src/campaign/ai';
import { clientMonth } from '../src/campaign/far';
import { FAR_SETUP } from '../src/campaign/far-rows';
import { allied, createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { current, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';
import { farOn, shoreOf } from '../src/campaign/world';

describe('held armies answer an attack by land, month after month (the design director\'s B11; the skeptic\'s findings)', () => {
  it('six months of a Soviet war with Japan: every row that answers ends its month on its own ground, held, and none on ground that is not its side\'s', () => {
    const g: Game = startOpen(createGame(17, null));
    expect(farOn(g)).toBe(true);
    setWar(g, 'JAP', 'SOV');
    clientMonth(g, true);
    // (Who held each province on the first day, read from the far side's own setup and the chart before a month is played.)
    expect(FAR_SETUP.held['Peiping']).toBe('JAP');
    const first = new Map(g.regions.map((r) => [r.id, FAR_SETUP.held[r.name] ?? r.owner]));
    const own = (c: Corps, r: Region): boolean => allied(g, c.nat, r.owner) || allied(g, c.nat, first.get(r.id)!);
    const place = (r: Region): string => `${r.name} (${r.owner}'s, held by ${r.ctrl})`;
    let now: { nat: string; rows: { corps: Corps; on: Set<number>; from: number }[] } | null = null;
    answerWatch.tell = (e) => { now = { nat: e.nat, rows: e.rows.map((x) => ({ corps: x.corps, on: x.on, from: x.corps.reg })) }; };
    const wrong: string[] = [], answered: Record<string, number> = {};
    let moved = 0;
    try {
      while (g.turn <= 6) {
        const nat = current(g), t = g.turn;
        now = null;
        stepAI(g);
        const seen = now as { nat: string; rows: { corps: Corps; on: Set<number>; from: number }[] } | null;
        if (!seen) continue;
        expect(seen.nat).toBe(nat);
        answered[nat] = (answered[nat] ?? 0) + seen.rows.length;
        for (const x of seen.rows) {
          const c = x.corps;
          // Every row that answers is a row of the far side, and its own post is always its ground.
          expect([shoreOf(x.from), x.on.has(x.from)], c.name).toEqual([2, true]);
          if (c.dead) continue;
          const here = g.regions[c.reg];
          if (c.reg !== x.from) moved++;
          if (!x.on.has(c.reg)) wrong.push(`month ${t}, ${c.name}: off its ground, ${place(g.regions[x.from])} to ${place(here)}`);
          else if (c.reg !== x.from && !own(c, here)) wrong.push(`month ${t}, ${c.name}: onto ground not its side's, ${place(g.regions[x.from])} to ${place(here)}`);
          if (!c.hold) wrong.push(`month ${t}, ${c.name}: not held again at ${place(here)}`);
        }
      }
    } finally { answerWatch.tell = null; }
    expect(wrong).toEqual([]);
    // The reading is of something: the three armies on that frontier and China's all had rows answering, and rows moved.
    for (const nat of ['SOV', 'JAP', 'CHN']) expect(answered[nat] ?? 0, nat).toBeGreaterThan(0);
    expect(moved).toBeGreaterThan(0);
  }, 900000);
});
