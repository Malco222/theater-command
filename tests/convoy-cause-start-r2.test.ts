// The convoy war by cause (Units decide): a campaign that opens later in the war, with the switch on, has its staffs
// take the switch's research rows in their catch-up months like any other row (found in review, 23 Nov 2026).
import { describe, expect, it, vi } from 'vitest';

vi.hoisted(() => { (globalThis as { TC_UNITS?: unknown }).TC_UNITS = 1; });

import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { unitsDecide } from '../src/campaign/units';

describe('a later start with Units decide', () => {
  it('opens with the kit Britain\'s staff would have researched by then', () => {
    const g = createGame(1, null);
    for (let t = 0; t < 30; t++) runTurnAI(g);
    startOpen(g);
    expect(unitsDecide(g)).toBe(true);
    const s = g.nations.UK.tech!;
    const rows = ['asvradar', 'patrolaircraft', 'hedgehog'];
    expect(rows.some((id) => s.done.includes(id) || s.work.some((w) => w.id === id))).toBe(true);
  }, 120_000);
});
