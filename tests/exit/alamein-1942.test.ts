// The desert in 1942, run against the campaign rules with every nation on the historical plan
// and nobody at the controls. The Axis army gets away from Operation Crusader to El Agheila,
// comes back in January to the Gazala line, breaks it in May, takes Tobruk in June and chases
// the British into Egypt. El Alamein holds through the summer. In October the British, now the
// stronger army and with their armour gathered, break the Axis line, and by the end of the year
// they are back at El Agheila with Tripoli still ahead of them.
//
// The plan only sets intentions. Every region still has to be won under the rules the player
// uses, so the bands below are loose on purpose and are checked across twenty sets of dice.
import { describe, expect, it } from 'vitest';
import { createGame, men } from '../../src/campaign/game';
import { runTurnAI } from '../../src/campaign/turn';
import type { Game } from '../../src/campaign/types';

const DEC_41 = 28, JAN_42 = 29, MARCH_42 = 31, APRIL_42 = 32, MAY_42 = 33, JUNE_42 = 34, JULY_42 = 35, AUG_42 = 36, SEPT_42 = 37, OCT_42 = 38, NOV_42 = 39, DEC_42 = 40, JAN_43 = 41;
/** Egypt behind the Alamein line. */
const DELTA = ['El Alamein', 'Alexandria', 'Cairo', 'Port Said', 'Suez'];
const ROAD = ['Tripoli', 'Sirte', 'El Agheila', 'Agedabia', 'Benghazi', 'Derna', 'Mechili', 'Bir Hakeim', 'Tobruk', 'Bardia', 'Sidi Barrani', 'Mersa Matruh'];
const DESERT = [...DELTA, ...ROAD, 'Nalut', 'Misrata', 'Hon', 'Jalo', 'Jarabub', 'Siwa', 'Qattara Depression', 'Faiyum'];

interface Fight { turn: number; place: string; by: string; taken: boolean; axisLost: number; ukLost: number }

interface Run {
  g: Game;
  /** Who held each place at the end of each month. */
  held: Record<number, Record<string, string>>;
  /** Men in the field in Libya and Egypt at the end of each month: the Axis, and the British. */
  army: Record<number, { axis: number; uk: number }>;
  /** Every battle fought in the desert. */
  fights: Fight[];
  /** Months in which any part of Egypt behind the Alamein line was not British. */
  deltaLost: number[];
  /** Where Montgomery's corps stood at the end of September 1942. */
  monty: string | null;
  /** The level of the works at Tobruk at the end of May, and at Mersa Matruh at the end of September. */
  works: { tobruk: number; matruh: number };
  /** The months in which the newsreel reported the two changes to the works. */
  news: { tobruk: number; matruh: number };
}

const axis = (who: string): boolean => who === 'ITA' || who === 'GER';

function history(seed: number): Run {
  const g = createGame(seed, null);
  const run: Run = { g, held: {}, army: {}, fights: [], deltaLost: [], monty: null, works: { tobruk: -1, matruh: -1 }, news: { tobruk: 0, matruh: 0 } };
  const desert = new Set(DESERT.map((n) => g.byName[n].id));
  let seen = 0, read = 0;
  for (let t = 1; t <= JAN_43; t++) {
    runTurnAI(g);
    run.held[t] = Object.fromEntries([...DELTA, ...ROAD, 'Malta'].map((n) => [n, g.byName[n].ctrl]));
    const army = { axis: 0, uk: 0 };
    for (const c of g.corps) {
      if (!desert.has(c.reg) || c.kind === 'fort' || c.hold) continue;
      if (axis(c.nat)) army.axis += men(c); else if (c.nat === 'UK') army.uk += men(c);
    }
    run.army[t] = army;
    for (const r of g.reports.slice(seen)) {
      if (!desert.has(r.region)) continue;
      const ours = axis(r.attNat);
      run.fights.push({ turn: t, place: r.regionName, by: r.attNat, taken: r.taken, axisLost: ours ? r.attLost : r.defLost, ukLost: ours ? r.defLost : r.attLost });
    }
    seen = g.reports.length;
    for (const h of g.headlines.slice(read)) {
      if (/mines and wire of Tobruk/.test(h.text)) run.news.tobruk = t;
      if (/digs in before El Alamein/.test(h.text)) run.news.matruh = t;
    }
    read = g.headlines.length;
    if (DELTA.some((n) => g.byName[n].ctrl !== 'UK')) run.deltaLost.push(t);
    if (t === MAY_42) run.works.tobruk = g.byName['Tobruk'].fort?.lv ?? 0;
    if (t === SEPT_42) {
      run.works.matruh = g.byName['Mersa Matruh'].fort?.lv ?? 0;
      const gen = g.generals.find((x) => x.name === 'Bernard Montgomery');
      const c = gen ? g.corps.find((x) => x.id === gen.corps) : undefined;
      run.monty = c ? g.regions[c.reg].name : null;
    }
  }
  return run;
}

const SEEDS = Array.from({ length: 20 }, (_, i) => i + 1);
const runs = SEEDS.map(history);
const count = (ok: (r: Run) => boolean): number => runs.filter(ok).length;
const lost = (r: Run, from: number, to: number): { axis: number; uk: number } => {
  const out = { axis: 0, uk: 0 };
  for (const f of r.fights) if (f.turn >= from && f.turn <= to) { out.axis += f.axisLost; out.uk += f.ukLost; }
  return out;
};

describe('exit test: the desert in 1942 on the historical plan', () => {
  it('the Axis army gets away from Operation Crusader and stands at El Agheila', () => {
    expect(count((r) => axis(r.held[DEC_41]['El Agheila']) && r.held[DEC_41]['Tobruk'] === 'UK')).toBeGreaterThanOrEqual(18);
    // Beaten, and still an army: it marched out of Cyrenaica where a year ago it would have stood and been destroyed.
    expect(count((r) => r.army[DEC_41].axis > 80000)).toBeGreaterThanOrEqual(18);
  });

  it("Rommel's second offensive: Agedabia and Benghazi in January, and the line settles at Gazala", () => {
    expect(count((r) => axis(r.held[JAN_42]['Agedabia']) && axis(r.held[JAN_42]['Benghazi']))).toBeGreaterThanOrEqual(18);
    expect(count((r) => axis(r.held[APRIL_42]['Mechili']) && axis(r.held[APRIL_42]['Derna']))).toBeGreaterThanOrEqual(18);
    expect(count((r) => r.held[APRIL_42]['Bir Hakeim'] === 'UK' && r.held[APRIL_42]['Tobruk'] === 'UK')).toBeGreaterThanOrEqual(18);
    // Both armies sit and build up through the spring.
    expect(count((r) => !r.fights.some((f) => f.turn === MARCH_42 || f.turn === APRIL_42))).toBeGreaterThanOrEqual(18);
  });

  it('Gazala: Bir Hakeim falls in May or June, and Tobruk in June', () => {
    expect(count((r) => axis(r.held[MAY_42]['Bir Hakeim']))).toBeGreaterThanOrEqual(14);
    expect(count((r) => axis(r.held[JUNE_42]['Bir Hakeim']) && axis(r.held[JUNE_42]['Tobruk']))).toBeGreaterThanOrEqual(17);
    // The fortress that stood eight months of siege in 1941 had been stripped to build the line in front of it.
    for (const r of runs) if (r.held[APRIL_42]['Tobruk'] === 'UK') { expect(r.works.tobruk).toBe(1); expect(r.news.tobruk).toBe(MAY_42); }
    // The worst British defeat of the desert war: tens of thousands lost, several times what it cost the Axis.
    expect(count((r) => lost(r, MAY_42, JUNE_42).uk > 40000)).toBeGreaterThanOrEqual(17);
    expect(count((r) => lost(r, MAY_42, JUNE_42).uk > 3 * lost(r, MAY_42, JUNE_42).axis)).toBeGreaterThanOrEqual(17);
  });

  it('the Axis crosses the frontier and reaches Mersa Matruh by July', () => {
    expect(count((r) => axis(r.held[JULY_42]['Bardia']) && axis(r.held[JULY_42]['Sidi Barrani']) && axis(r.held[JULY_42]['Mersa Matruh']))).toBeGreaterThanOrEqual(17);
  });

  it('El Alamein: the Axis tries the line in July or August and is thrown back every time', () => {
    const tries = (r: Run): Fight[] => r.fights.filter((f) => f.place === 'El Alamein' && axis(f.by));
    expect(count((r) => tries(r).some((f) => f.turn === JULY_42 || f.turn === AUG_42))).toBeGreaterThanOrEqual(16);
    for (const r of runs) expect(tries(r).filter((f) => f.taken)).toEqual([]);
  });

  it('by the end of September the British are the stronger army, with their armour gathered under Montgomery', () => {
    expect(count((r) => r.army[SEPT_42].uk > 1.15 * r.army[SEPT_42].axis)).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.monty === 'El Alamein')).toBeGreaterThanOrEqual(16);
    // The Axis has dug in behind minefields in front of him.
    for (const r of runs) if (axis(r.held[SEPT_42]['Mersa Matruh'])) { expect(r.works.matruh).toBe(1); expect(r.news.matruh).toBe(SEPT_42); }
  });

  it('October 1942: the British break the line and the Axis loses several times what they do', () => {
    expect(count((r) => r.held[OCT_42]['Mersa Matruh'] === 'UK')).toBeGreaterThanOrEqual(17);
    expect(count((r) => !r.fights.some((f) => f.by === 'UK' && f.turn >= JULY_42 && f.turn <= SEPT_42))).toBeGreaterThanOrEqual(18);
    expect(count((r) => lost(r, OCT_42, OCT_42).axis > 30000 && lost(r, OCT_42, OCT_42).axis > 3 * lost(r, OCT_42, OCT_42).uk)).toBeGreaterThanOrEqual(16);
  });

  it('the pursuit: Tobruk and Benghazi in November, El Agheila by the end of the year', () => {
    expect(count((r) => r.held[NOV_42]['Tobruk'] === 'UK' && r.held[NOV_42]['Benghazi'] === 'UK')).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.held[DEC_42]['El Agheila'] === 'UK')).toBeGreaterThanOrEqual(16);
    // The Axis army is beaten and not destroyed: it falls back on Tripoli, which is still to be taken.
    for (const r of runs) expect(axis(r.held[JAN_43]['Tripoli'])).toBe(true);
    expect(count((r) => r.army[DEC_42].axis > 50000 && r.army[DEC_42].axis < 0.75 * r.army[SEPT_42].axis)).toBeGreaterThanOrEqual(16);
  });

  it('Egypt behind the Alamein line never falls, and Malta holds out', () => {
    for (const { deltaLost, held } of runs) {
      expect(deltaLost).toEqual([]);
      expect(held[JAN_43]['Malta']).toBe('UK');
    }
  });

  it('nobody collapses over a colony', () => {
    for (const { g } of runs) {
      expect(g.nations.ITA.alive).toBe(true);
      expect(g.nations.UK.alive).toBe(true);
      expect(g.nations.GER.alive).toBe(true);
    }
  });
});
