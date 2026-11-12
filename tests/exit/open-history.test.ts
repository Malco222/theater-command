// Free play, run as whole wars with every nation on the computer and nobody at the controls: twelve wars from
// September 1939 to the end of 1944. No nation follows history's plan. Each staff weighs its fronts every month
// and decides for itself, with a nerve that differs from one war to the next.
//
// What is checked is that these are wars and not one war twelve times: the opening is history's, the great
// decisions come at different times or not at all, and they do not all end the same way. The bands are loose
// on purpose. Which war goes which way is the business of the dice and the staffs.
import { describe, expect, it } from 'vitest';
import { createGame } from '../../src/campaign/game';
import { startOpen } from '../../src/campaign/staff';
import { runTurnAI } from '../../src/campaign/turn';
import type { Game } from '../../src/campaign/types';

const DEC_44 = 64;
const SEEDS = Array.from({ length: 12 }, (_, i) => i + 1);
const PRIZES = ['Paris', 'Moscow', 'Berlin', 'Warsaw', 'Kiev', 'Leningrad', 'Rome', 'Athens', 'Belgrade', 'Oslo', 'Cairo'];

interface War {
  g: Game;
  /** The month each country fell in. */
  fell: Record<string, number>;
  /** Who held the great cities when 1944 ended. */
  ends: string;
  /** The first month Germany and the Soviet Union were at war, and who began it. */
  east: { month: number; by: string } | null;
}

function war(seed: number): War {
  const g = startOpen(createGame(seed, null));
  const fell: Record<string, number> = {};
  while (g.turn <= DEC_44) {
    const month = g.turn;
    runTurnAI(g);
    for (const n of Object.values(g.nations)) if (!n.alive && !n.dormant && fell[n.id] === undefined) fell[n.id] = month;
  }
  const a = g.flags['war:GER:SOV'], b = g.flags['war:SOV:GER'];
  const east = a ? { month: a, by: 'GER' } : b ? { month: b, by: 'SOV' } : null;
  return { g, fell, ends: PRIZES.map((nm) => g.byName[nm].ctrl).join(' '), east };
}

const wars = SEEDS.map(war);
const count = (test: (w: War) => boolean): number => wars.filter(test).length;
const spread = (of: (w: War) => number | string | undefined): number => new Set(wars.map(of)).size;

describe('twelve wars with no plan', () => {
  it('all begin as the war did: Poland is beaten in the autumn of 1939', () => {
    for (const w of wars) expect(w.fell.POL, `seed ${w.g.seed}`).toBeLessThanOrEqual(3);
  });

  it('leave the map in order: every region has a living or beaten owner, and every corps stands somewhere real', () => {
    for (const w of wars) {
      for (const r of w.g.regions) expect(w.g.nations[r.ctrl], r.name).toBeDefined();
      for (const c of w.g.corps) {
        expect(w.g.regions[c.reg].units, c.name).toContain(c);
        expect(Number.isFinite(c.str) && c.str >= 0 && c.str <= 1.0001, c.name).toBe(true);
      }
    }
  });

  it('see Germany attack in the west in most of them, in different months', () => {
    expect(count((w) => w.g.flags['war:GER:BEL'] !== undefined)).toBeGreaterThanOrEqual(9);
    expect(spread((w) => w.g.flags['war:GER:BEL'])).toBeGreaterThanOrEqual(4);
  });

  it('see France fall where it is attacked in strength, but not on history\'s timetable every time', () => {
    expect(count((w) => w.fell.FRA !== undefined)).toBeGreaterThanOrEqual(8);
    expect(spread((w) => w.fell.FRA)).toBeGreaterThanOrEqual(3);
  });

  it('see Italy choose its own moment, and in some wars choose Greece or the Balkans', () => {
    expect(spread((w) => w.g.flags['war:ITA:UK'])).toBeGreaterThanOrEqual(3);
    const balkans = count((w) => w.g.flags['war:ITA:GRE'] !== undefined || w.g.flags['war:GER:YUG'] !== undefined);
    expect(balkans).toBeGreaterThanOrEqual(1);
    expect(balkans).toBeLessThan(SEEDS.length);
  });

  it('see war in the east in most of them, begun by either side, and never in the first winter', () => {
    const fought = wars.filter((w) => w.east);
    expect(fought.length).toBeGreaterThanOrEqual(7);
    expect(spread((w) => w.east?.month)).toBeGreaterThanOrEqual(4);
    for (const w of fought) expect(w.east!.month, `seed ${w.g.seed}`).toBeGreaterThan(8);
  });

  it('do not all end the same way', () => {
    expect(spread((w) => w.ends)).toBeGreaterThanOrEqual(5);
    // Moscow and Berlin are both held by their owners at the end in some wars and not in others.
    const moscow = count((w) => w.g.byName['Moscow'].ctrl === 'SOV'), berlin = count((w) => w.g.byName['Berlin'].ctrl === 'GER');
    expect(moscow).toBeGreaterThan(0);
    expect(berlin).toBeGreaterThan(0);
    expect(moscow + berlin).toBeLessThan(2 * SEEDS.length);
  });

  it('never see a nation attack an ally or a friend of its own side', () => {
    for (const w of wars) for (const key of Object.keys(w.g.flags)) {
      if (!key.startsWith('war:')) continue;
      const [, a, b] = key.split(':');
      const axis = ['GER', 'ITA', 'ROM', 'HUN', 'SVK', 'BUL', 'FIN'], allies = ['UK', 'FRA', 'POL', 'USA'];
      expect(axis.includes(a) && axis.includes(b), key).toBe(false);
      expect(allies.includes(a) && allies.includes(b), key).toBe(false);
    }
  });

  it('do not send the invasion forces history assembled for landings of its own planning', () => {
    for (const w of wars) expect(w.g.corps.filter((c) => /^(Fifth|Seventh) Army:/.test(c.name)).map((c) => c.name)).toEqual([]);
  });
});
