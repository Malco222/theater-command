// Torch and Tunisia, run against the campaign rules with every nation on the historical plan and
// nobody at the controls. The United States comes into the war in December 1941. In November 1942
// American and British troops land in Morocco and Algeria; the French fight for three days and come
// over; Germany and Italy occupy the rest of France and fly troops into Tunis, and the first dash for
// Tunis fails. Tripoli falls in January. In February the panzers maul the Americans in front of
// Tebessa and are thrown back. The Mareth Line goes in March, Sfax in April and Tunis in May, and the
// Axis army in Africa lays down its arms.
//
// The plan only sets intentions. Every region still has to be won under the rules the player
// uses, so the bands below are loose on purpose and are checked across twenty sets of dice.
import { describe, expect, it } from 'vitest';
import { atWar, createGame, men } from '../../src/campaign/game';
import { runTurnAI } from '../../src/campaign/turn';
import type { Game } from '../../src/campaign/types';

const NOV_41 = 27, DEC_41 = 28, OCT_42 = 38, NOV_42 = 39, DEC_42 = 40, JAN_43 = 41, FEB_43 = 42, MARCH_43 = 43, APRIL_43 = 44, MAY_43 = 45, JUNE_43 = 46;
/** French North Africa west of Tunisia, and Tunisia. */
const WEST = ['Casablanca', 'Fez', 'Marrakech', 'Oran', 'Algiers', 'Constantine', 'Biskra'];
const TUNISIA = ['Tunis', 'Sfax', 'Mareth'];
const LIBYA = ['Tripoli', 'Nalut', 'Misrata', 'Sirte', 'Hon', 'El Agheila'];
const WATCH = [...WEST, ...TUNISIA, ...LIBYA, 'Marseille', 'Lyon', 'Corsica', 'Syria', 'Gibraltar'];

interface Fight { turn: number; place: string; by: string; odds: number; taken: boolean; attLost: number; defLost: number; prisoners: number; lostBy: Record<string, number> }

interface Run {
  g: Game;
  /** Who held each place at the end of each month from October 1942. */
  held: Record<number, Record<string, string>>;
  /** Whether the United States was in the war at the end of November and of December 1941. */
  usa: { before: boolean; after: boolean; corpsBeforeTorch: number };
  /** The landings of November 1942, as the newsreel told them, and any that were thrown back. */
  landed: string[];
  repulsed: number;
  /** Whether Vichy France was still a country at the end of October and of November 1942. */
  vichy: { before: boolean; after: boolean };
  /** Every battle fought from Morocco to Tripolitania from November 1942. */
  fights: Fight[];
  /** Axis men left in Africa at the end of each month. */
  axis: Record<number, number>;
  /** The month the newsreel said Africa was clear, or 0. */
  cleared: number;
}

const axis = (who: string): boolean => who === 'ITA' || who === 'GER';
const allies = (who: string): boolean => who === 'UK' || who === 'USA';

function history(seed: number): Run {
  const g = createGame(seed, null);
  const run: Run = { g, held: {}, usa: { before: false, after: false, corpsBeforeTorch: 0 }, landed: [], repulsed: 0, vichy: { before: false, after: false }, fights: [], axis: {}, cleared: 0 };
  const africa = new Set([...WEST, ...TUNISIA, ...LIBYA].map((n) => g.byName[n].id));
  let seen = 0, read = 0;
  for (let t = 1; t <= JUNE_43; t++) {
    runTurnAI(g);
    if (t === NOV_41 - 1) run.usa.before = g.nations.USA.alive;
    if (t === DEC_41) run.usa.after = g.nations.USA.alive && atWar(g, 'USA', 'GER') && atWar(g, 'USA', 'ITA');
    if (t === OCT_42 - 1) { run.vichy.before = g.nations.VIC.alive; run.usa.corpsBeforeTorch = g.corps.filter((c) => c.nat === 'USA').length; }
    if (t === NOV_42) run.vichy.after = g.nations.VIC.alive;
    if (t >= OCT_42) {
      run.held[t] = Object.fromEntries(WATCH.map((n) => [n, g.byName[n].ctrl]));
      run.axis[t] = g.corps.filter((c) => axis(c.nat) && africa.has(c.reg)).reduce((s, c) => s + men(c), 0);
      for (const r of g.reports.slice(seen)) {
        if (!africa.has(r.region)) continue;
        const lostBy: Record<string, number> = { [r.attNat]: r.attLost };
        for (const u of r.defenders) lostBy[u.nat] = (lostBy[u.nat] ?? 0) + u.lost;
        run.fights.push({ turn: t, place: r.regionName, by: r.attNat, odds: r.odds, taken: r.taken, attLost: r.attLost, defLost: r.defLost, prisoners: r.prisoners, lostBy });
      }
      for (const h of g.headlines.slice(read)) {
        if (h.turn === NOV_42 && / troops land at /.test(h.text)) run.landed.push(h.text);
        if (h.turn === NOV_42 && /thrown back into the sea/.test(h.text)) run.repulsed++;
        if (/masters of the North African shores/.test(h.text)) run.cleared = h.turn;
      }
    }
    seen = g.reports.length;
    read = g.headlines.length;
  }
  return run;
}

const SEEDS = Array.from({ length: 20 }, (_, i) => i + 1);
const runs = SEEDS.map(history);
const count = (ok: (r: Run) => boolean): number => runs.filter(ok).length;
/** Men each side lost in the battles of these months: the Americans, the British (and the French with them), the Axis. */
const lost = (r: Run, from: number, to: number): { usa: number; uk: number; axis: number } => {
  const out = { usa: 0, uk: 0, axis: 0 };
  for (const f of r.fights) {
    if (f.turn < from || f.turn > to) continue;
    out.usa += f.lostBy.USA ?? 0; out.uk += f.lostBy.UK ?? 0; out.axis += (f.lostBy.GER ?? 0) + (f.lostBy.ITA ?? 0);
  }
  return out;
};

describe('exit test: Torch and Tunisia on the historical plan', () => {
  it('the United States enters the war in December 1941 and has no army in Europe or Africa until Torch', () => {
    for (const r of runs) {
      expect(r.usa.before).toBe(false);
      expect(r.usa.after).toBe(true);
      expect(r.usa.corpsBeforeTorch).toBe(0);
    }
  });

  it('Torch: three landings in November 1942, none thrown back, and the Americans take Casablanca and Oran', () => {
    expect(count((r) => r.landed.length >= 3 && r.repulsed === 0)).toBeGreaterThanOrEqual(18);
    expect(count((r) => r.landed.some((t) => /^American/.test(t)) && r.landed.some((t) => /^British/.test(t)))).toBeGreaterThanOrEqual(18);
    // The French in Morocco fight before they come over.
    expect(count((r) => r.fights.some((f) => f.turn === NOV_42 && f.by === 'USA' && f.place === 'Casablanca' && f.taken))).toBeGreaterThanOrEqual(15);
  });

  it("Darlan's ceasefire: French North Africa comes over, the Axis occupies the rest of France and holds Tunisia", () => {
    for (const r of runs) {
      expect(r.vichy.before).toBe(true);
      if (r.landed.length === 0) continue;
      expect(r.vichy.after).toBe(false);
      const h = r.held[NOV_42];
      for (const n of WEST) expect(allies(h[n])).toBe(true);
      expect(h['Casablanca']).toBe('USA');
      expect(h['Algiers']).toBe('UK');
      for (const n of TUNISIA) expect(axis(h[n])).toBe(true);
      expect(h['Marseille']).toBe('GER');
      expect(h['Lyon']).toBe('GER');
      expect(h['Corsica']).toBe('ITA');
      expect(h['Syria']).toBe('UK');
    }
  });

  it('the race for Tunis is lost: the British try in December and are beaten off, and Tunis holds until the spring', () => {
    const tries = (r: Run): Fight[] => r.fights.filter((f) => f.turn === DEC_42 && f.place === 'Tunis' && f.by === 'UK');
    expect(count((r) => tries(r).length > 0 && tries(r).every((f) => !f.taken))).toBeGreaterThanOrEqual(14);
    expect(count((r) => [DEC_42, JAN_43, FEB_43, MARCH_43, APRIL_43].every((t) => axis(r.held[t]['Tunis'])))).toBeGreaterThanOrEqual(18);
  });

  it('Tripoli falls in January or February 1943, and the Axis army gets away into Tunisia', () => {
    expect(count((r) => r.held[JAN_43]['Tripoli'] === 'UK')).toBeGreaterThanOrEqual(14);
    expect(count((r) => r.held[FEB_43]['Tripoli'] === 'UK')).toBeGreaterThanOrEqual(18);
    expect(count((r) => axis(r.held[FEB_43]['Mareth']) && r.axis[FEB_43] > 150000)).toBeGreaterThanOrEqual(17);
  });

  it('Kasserine: in February the panzers break the front before Tebessa, at several times the cost to the Allies, and are thrown back', () => {
    const stroke = (r: Run): Fight[] => r.fights.filter((f) => f.turn === FEB_43 && f.place === 'Biskra' && f.by === 'GER');
    expect(count((r) => stroke(r).some((f) => f.taken))).toBeGreaterThanOrEqual(14);
    // The Americans' first battle against the German army goes badly.
    expect(count((r) => { const s = stroke(r).find((f) => f.taken); return !!s && s.defLost > 2 * s.attLost && (s.lostBy.USA ?? 0) > 3000; })).toBeGreaterThanOrEqual(13);
    expect(count((r) => allies(r.held[MARCH_43]['Biskra']))).toBeGreaterThanOrEqual(18);
  });

  it('the Mareth Line is broken in March or April, and Sfax falls by the end of April', () => {
    expect(count((r) => r.held[MARCH_43]['Mareth'] === 'UK')).toBeGreaterThanOrEqual(15);
    expect(count((r) => r.held[APRIL_43]['Mareth'] === 'UK' && allies(r.held[APRIL_43]['Sfax']))).toBeGreaterThanOrEqual(15);
  });

  it('May 1943: Tunis falls, the Axis army in Africa lays down its arms, and the newsreel says Africa is clear', () => {
    expect(count((r) => allies(r.held[MAY_43]['Tunis']))).toBeGreaterThanOrEqual(16);
    expect(count((r) => r.axis[MAY_43] === 0)).toBeGreaterThanOrEqual(16);
    expect(count((r) => r.fights.filter((f) => f.turn >= MARCH_43 && allies(f.by)).reduce((s, f) => s + f.prisoners, 0) > 60000)).toBeGreaterThanOrEqual(15);
    for (const r of runs) if (allies(r.held[MAY_43]['Tunis']) && r.axis[MAY_43] === 0) expect(r.cleared).toBe(JUNE_43);
  });

  it('the campaign costs the Axis several times what it costs the Allies, and the Americans pay their share', () => {
    expect(count((r) => { const l = lost(r, NOV_42, MAY_43); return l.axis > 180000 && l.axis > 3 * (l.uk + l.usa); })).toBeGreaterThanOrEqual(16);
    expect(count((r) => { const l = lost(r, NOV_42, MAY_43); return l.usa > 8000 && l.usa < 40000; })).toBeGreaterThanOrEqual(15);
    expect(count((r) => { const l = lost(r, NOV_42, MAY_43); return l.uk > 25000 && l.uk < 80000; })).toBeGreaterThanOrEqual(16);
  });

  it('Gibraltar is never lost', () => {
    for (const r of runs) for (const t of Object.keys(r.held)) expect(r.held[+t]['Gibraltar']).toBe('UK');
  });
});
