// The far side: a beaten corps is "too far gone to fall back as a formation" at under 0.15 of its own full size, not
// at a strength under 0.15 (the design director's ruling of 07:26Z, 22 November 2026, every nation; the 0.15 unchanged).
//
// Cause: the far side writes a small unit (a battalion, an island's garrison, a landing force) as a row of one division
// that never fills past its own size, so its strength is its worth: the 2/15th Punjab at Kuching is 1 x 0.1 of 0.1.
// The battle's line read the strength alone, and such a row was broken from the day it was placed: beaten, it was
// destroyed with open ground of its own side next door, where the record has the Punjabis fall back into Dutch Borneo
// and fight on until the Dutch capitulation. 45 far rows that are not forts start under 0.15. A fortress crew is
// already read against its own size. A corps of full size reads as it did.
// With the far side's switch off the old line stands.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, capitulate, computeSupply, createGame, declareWar, resolveBattle } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { BattleReport, Corps, Game } from '../src/campaign/types';

const PUNJAB = '2/15th Punjab (Kuching)';

/** The grand campaign (stop 3) with the Indies in Britain's keeping and Japan at war with Britain and the United States. */
const war = (): Game => {
  const g = startOpen(createGame(3, null));
  declareWar(g, 'GER', 'NED');
  capitulate(g, 'NED', 'GER');
  for (const e of ['USA', 'UK']) declareWar(g, 'JAP', e);
  return g;
};
/** A row of one division that never fills past `est`, standing at `str`. */
const put = (g: Game, nat: string, name: string, place: string, str: number, est?: number): Corps => {
  const c = addCorps(g, nat, name, 'inf', 1, place, est === undefined ? { xp: 0.1 } : { xp: 0.1, est });
  c.str = str;
  return c;
};
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;
/** Kuching with one British row in it and no town guard, attacked overland from a Miri in Japan's hands. */
const kuching = (g: Game, str: number, est?: number, name = PUNJAB): { p: Corps; fight: () => BattleReport } => {
  const at = g.byName.Kuching, miri = g.byName.Miri;
  at.units = [];
  g.flags[`guard:${at.id}`] = 100;
  miri.ctrl = 'JAP';
  miri.units = [];
  const p = put(g, 'UK', name, 'Kuching', str, est);
  const att = [1, 2].map((k) => put(g, 'JAP', `Japanese corps ${k}`, 'Miri', 0.9));
  g.supplyDirty = true;
  computeSupply(g);
  return { p, fight: () => resolveBattle(g, att, at) };
};

describe("the shatter line is read against a row's own full size (the director's ruling of 07:26Z, 22 November 2026)", () => {
  it('the 2/15th Punjab, beaten at Kuching at stop 3, falls back into Dutch Borneo and is not destroyed', () => {
    const g = war();
    const { p, fight } = kuching(g, 0.1, 0.1);
    expect([p.str, p.full]).toEqual([0.1, 0.1]);
    const rep = fight();
    expect(rep.taken).toBe(true);
    expect(rep.defenders.filter((u) => u.name === PUNJAB).map((u) => u.fate)).toEqual(['retreated']);
    expect(p.dead).toBe(false);
    // (The retreat rule's own choice among the open British provinces next door: an army abroad falls back on a port.)
    expect(where(g, p)).toBe('Pontianak');
    expect(g.byName.Pontianak.ctrl).toBe('UK');
    expect(p.str).toBeGreaterThan(0.015);
    expect(p.str).toBeLessThan(0.1);
  });

  it('a row at 0.1 of 0.1 falls back, and a row at 0.01 of 0.1 is destroyed', () => {
    const whole = war();
    const a = kuching(whole, 0.1, 0.1, 'A battalion at its full size');
    expect(a.fight().defenders[0].fate).toBe('retreated');
    expect(a.p.dead).toBe(false);
    const worn = war();
    const b = kuching(worn, 0.01, 0.1, 'A battalion worn to a tenth of itself');
    expect(b.fight().defenders[0].fate).toBe('destroyed');
    expect(b.p.dead).toBe(true);
  });

  it('a corps of full size reads as it did: at 0.14 it is destroyed, at 0.3 it falls back', () => {
    const thin = war();
    const a = kuching(thin, 0.14, undefined, 'A division worn to 0.14');
    expect(a.p.full).toBeUndefined();
    expect(a.fight().defenders[0].fate).toBe('destroyed');
    const firm = war();
    const b = kuching(firm, 0.3, undefined, 'A division at 0.3');
    expect(b.fight().defenders[0].fate).toBe('retreated');
    expect(b.p.dead).toBe(false);
  });
});
