// The framework's Phase 3 Mediterranean war, second part: Greece and the Balkans. With every nation on
// the historical plan and nobody at the controls, Italy should march into the Greek mountains in October
// 1940 and be stopped there all winter; Germany should come through Bulgaria in April 1941 and take
// Yugoslavia and Greece inside a month or two; the British expedition should be lifted off; and Crete
// should fall to the airborne corps. None of it may disturb the build-up for Barbarossa.
//
// The plan only sets intentions. Every region still has to be won under the rules the player
// uses, so the bands below are loose on purpose and are checked across twenty sets of dice.
import { describe, expect, it } from 'vitest';
import { atWar, createGame, men } from '../../src/campaign/game';
import { runTurnAI } from '../../src/campaign/turn';
import setup from '../../data/campaign/setup-1939.json';
import theatres from '../../data/campaign/theatres.json';
import type { BattleReport, Game } from '../../src/campaign/types';

const SEPT_40 = 13, OCT_40 = 14, NOV_40 = 15, MARCH_41 = 19, APRIL_41 = 20, MAY_41 = 21, JUNE_41 = 22, AUG_41 = 24;
const BALKANS = (theatres as any).balkans.regions as string[];
const PLACES = ['Albania', 'Epirus', 'Salonika', 'Thessaly', 'Athens', 'Peloponnese', 'Crete', 'Belgrade', 'Croatia'];
/** Every German formation history sent to the Balkans, by name. */
const SENT = new Set(((setup as any).expeditions as any[]).filter((row) => row[1] === 'GER' && BALKANS.includes(row[5])).map((row) => row[2] as string));
const LENT = new Set(((setup as any).expeditions as any[]).filter((row) => row[6]?.until).map((row) => row[2] as string));

interface Run {
  g: Game;
  /** Who held each place at the end of each month. */
  held: Record<number, Record<string, string>>;
  /** Whether Greece and Yugoslavia were still in the war at the end of each month. */
  alive: Record<number, { GRE: boolean; YUG: boolean }>;
  /** Men in the Balkans at the end of each month, by nation. */
  army: Record<number, Record<string, number>>;
  /** Battles fought on the theatre's ground. */
  fights: BattleReport[];
  /** German corps found in the Balkans that history did not send there, and corps it did send found outside. */
  strangers: string[];
  strays: string[];
  /** Where the British expedition stood at the end of each month: a region, or null once it is gone. */
  wforce: Record<number, string | null>;
  /** German tanks and lorried troops still in the Balkans at the end of June 1941. */
  armourLeft: string[];
  barbarossa: boolean;
}

function history(seed: number): Run {
  const g = createGame(seed, null);
  const ids = new Set(BALKANS.map((n) => g.byName[n].id));
  const run: Run = { g, held: {}, alive: {}, army: {}, fights: [], strangers: [], strays: [], wforce: {}, armourLeft: [], barbarossa: false };
  let seen = 0;
  for (let t = 1; t <= AUG_41; t++) {
    runTurnAI(g);
    run.held[t] = Object.fromEntries(PLACES.map((n) => [n, g.byName[n].ctrl]));
    run.alive[t] = { GRE: g.nations.GRE.alive, YUG: g.nations.YUG.alive };
    run.army[t] = {};
    for (const c of g.corps) if (ids.has(c.reg)) run.army[t][c.nat] = (run.army[t][c.nat] ?? 0) + men(c);
    run.fights.push(...g.reports.slice(seen).filter((r) => ids.has(r.region)));
    seen = g.reports.length;
    if (t >= SEPT_40) {
      for (const c of g.corps) {
        if (c.nat !== 'GER') continue;
        if (ids.has(c.reg) && !SENT.has(c.name)) run.strangers.push(`${c.name} in ${g.regions[c.reg].name}, month ${t}`);
        if (!ids.has(c.reg) && SENT.has(c.name)) run.strays.push(`${c.name} in ${g.regions[c.reg].name}, month ${t}`);
      }
    }
    const w = g.corps.find((c) => c.name.startsWith('W Force'));
    run.wforce[t] = w ? g.regions[w.reg].name : null;
    if (t === JUNE_41) {
      run.armourLeft = g.corps.filter((c) => c.nat === 'GER' && ids.has(c.reg) && (c.kind === 'arm' || c.kind === 'mot')).map((c) => c.name);
      run.barbarossa = atWar(g, 'GER', 'SOV');
    }
  }
  return run;
}

const SEEDS = Array.from({ length: 20 }, (_, i) => i + 1);
let cache: Run[] | null = null;
const all = (): Run[] => (cache ??= SEEDS.map(history));
const count = (ok: (r: Run) => boolean): number => all().filter(ok).length;
const lost = (r: Run, by: string, from: number, to: number, side: 'att' | 'def'): number =>
  r.fights.filter((f) => f.turn >= from && f.turn <= to && (side === 'att' ? f.attNat === by : f.defNat === by)).reduce((s, f) => s + (side === 'att' ? f.attLost : f.defLost), 0);

describe('exit test: Greece and the Balkans on the historical plan', () => {
  it('Italy attacks Greece from Albania in October 1940', { timeout: 600000 }, () => {
    for (const r of all()) {
      expect(r.g.flags['war:ITA:GRE']).toBe(OCT_40);
      expect(r.g.nations.GRE.align).toBe('allies');
      // The first assault goes in against the Epirus Army in its mountains, and fails.
      const first = r.fights.find((f) => f.turn === OCT_40 && f.attNat === 'ITA');
      expect(first?.regionName).toBe('Epirus');
      expect(first?.taken).toBe(false);
    }
  });

  it('and is held on the frontier all winter, the spring offensive included', () => {
    expect(count((r) => r.held[NOV_40].Epirus === 'GRE')).toBeGreaterThanOrEqual(18);
    expect(count((r) => r.held[MARCH_41].Epirus === 'GRE' && r.held[MARCH_41].Salonika === 'GRE')).toBeGreaterThanOrEqual(18);
    // March 1941: the Italians try again, and are beaten off again.
    expect(count((r) => r.fights.some((f) => f.turn === MARCH_41 && f.attNat === 'ITA' && !f.taken))).toBeGreaterThanOrEqual(16);
    // Albania is never lost: the map has it as one region, so the Greek advance to Koritsa is not drawn.
    for (const r of all()) for (let t = OCT_40; t <= AUG_41; t++) expect(r.held[t].Albania).toBe('ITA');
  });

  it('the mountain war costs Italy far more than it costs Greece', () => {
    expect(count((r) => lost(r, 'ITA', OCT_40, MARCH_41, 'att') > 2 * lost(r, 'GRE', OCT_40, MARCH_41, 'def'))).toBeGreaterThanOrEqual(18);
    expect(count((r) => lost(r, 'ITA', OCT_40, MARCH_41, 'att') > 20000)).toBeGreaterThanOrEqual(18);
  });

  it('Britain sends a garrison to Crete and an expedition to the mainland', () => {
    for (const r of all()) {
      expect(r.army[NOV_40].UK ?? 0).toBeGreaterThan(0);
      expect(r.wforce[MARCH_41]).toBe('Thessaly');
    }
  });

  it('Germany comes through Bulgaria in April 1941 and Yugoslavia falls within the month', () => {
    for (const r of all()) {
      expect(r.g.flags['war:GER:YUG']).toBe(APRIL_41);
      expect(r.g.flags['war:GER:GRE']).toBe(APRIL_41);
      expect(r.army[MARCH_41].GER ?? 0).toBeGreaterThan(100000);
    }
    expect(count((r) => r.held[APRIL_41].Belgrade === 'GER' && !r.alive[APRIL_41].YUG)).toBeGreaterThanOrEqual(18);
    for (const r of all()) expect(r.alive[MAY_41].YUG).toBe(false);
  });

  it('and Greece by the end of May', () => {
    expect(count((r) => r.held[APRIL_41].Salonika === 'GER')).toBeGreaterThanOrEqual(18);
    expect(count((r) => r.held[MAY_41].Athens === 'GER' && !r.alive[MAY_41].GRE)).toBeGreaterThanOrEqual(18);
    // It is a walkover: the whole campaign costs Germany a small fraction of what it costs the defenders.
    expect(count((r) => lost(r, 'GER', APRIL_41, MAY_41, 'att') < 40000)).toBeGreaterThanOrEqual(18);
  });

  it('the British expedition is lifted off rather than lost', () => {
    // Crete first, then home when Crete falls: either way most of it gets away.
    expect(count((r) => r.wforce[AUG_41] !== null)).toBeGreaterThanOrEqual(14);
    expect(count((r) => r.wforce[APRIL_41] === 'Crete' || r.wforce[MAY_41] === 'Crete')).toBeGreaterThanOrEqual(12);
    for (const r of all()) expect(BALKANS.filter((n) => n !== 'Crete')).not.toContain(r.wforce[AUG_41]);
  });

  it('Crete falls to the airborne corps', () => {
    expect(count((r) => r.held[JUNE_41].Crete === 'GER')).toBeGreaterThanOrEqual(15);
    expect(count((r) => r.held[AUG_41].Crete === 'GER')).toBeGreaterThanOrEqual(16);
    for (const r of all()) {
      const drop = r.fights.find((f) => f.regionName === 'Crete' && f.attNat === 'GER');
      if (!drop) continue;
      expect(drop.attackers.map((u) => u.name)).toEqual(['XI Air Corps']);
      expect(drop.lines.some((l) => l.label === 'Airborne assault')).toBe(true);
      // No walkover: the defenders are dug in, and the paratroops pay for the island.
      expect(drop.attLost).toBeGreaterThan(2000);
    }
    // The garrison that wins the island is still fed there in August.
    for (const r of all()) if (r.held[AUG_41].Crete === 'GER') expect(r.g.corps.some((c) => c.name === 'XI Air Corps' && r.g.regions[c.reg].name === 'Crete' && c.supplied)).toBe(true);
  });

  it('the panzers are back with the main army in time for Barbarossa', () => {
    for (const r of all()) {
      expect(r.armourLeft).toEqual([]);
      expect(r.barbarossa).toBe(true);
      for (const name of LENT) expect(r.g.corps.some((c) => c.name === name), name).toBe(false);
      // An occupation force stays behind.
      expect(r.army[AUG_41].GER ?? 0).toBeGreaterThan(60000);
    }
  });

  it('the two wars keep to themselves: nobody from the main army strays in, and nobody sent there strays out', () => {
    for (const r of all()) {
      expect(r.strangers).toEqual([]);
      expect(r.strays).toEqual([]);
    }
  });
});
