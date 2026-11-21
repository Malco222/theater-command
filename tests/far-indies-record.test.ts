// The Indies' record as data (the design director's rulings of 15:16Z and 15:27Z of 21 November 2026 on the
// historian's indies-landings-1941-42.md: accepted as data, middles, and laid now: a fact is never held back, nor
// laid, to move a reading; data/campaign/far-shores.json and far-pacific-war.json).
//
// Ambon: the 228th 0.3 and the 1st Kure force 0.05 against the Molukken Brigade 0.19 at 0.85 and Gull Force 0.07,
// the Great East row split by place (Menado 0.1, Kendari 0.03; Timor's in Sparrow Force's row). Palembang: the
// paratroops 0.03 with the 229th against 0.15. Java: the Shoji Detachment 0.3; a KNIL division in the field 0.7; the
// 48th goes to Kragan whole. Borneo: the Kawaguchi Detachment 0.325, Kuching's defence 0.1, and the detachment's
// follow-ons on the record's dates. The director's 16:08Z on what was not laid: the 2nd Division at its cap, 1.0, with
// the 16th Army's troops, 0.1, a row of their own landing with it (the form of Hong Kong's guns), and Singkawang's
// KNIL detachment, 0.05. No rule is new. The states are set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { frontWatch, runAI } from '../src/campaign/ai';
import { addCorps, createGame, declareWar } from '../src/campaign/game';
import { raisedQuality, setStop } from '../src/campaign/history';
import { followKey, frontsClear, frontsState, frontsTick, planWaits, sailedKey } from '../src/campaign/planfronts';
import { ops, startOpen } from '../src/campaign/staff';
import { bookAttacks, bookWatch, stations } from '../src/campaign/stations';
import '../src/campaign/far-rows';
import farWar from '../data/campaign/far-pacific-war.json';
import world from '../data/campaign/world.json';
import setup from '../data/campaign/setup-1939.json';
import type { Corps, Game } from '../src/campaign/types';

const KAWAGUCHI = 'Kawaguchi Detachment (35th Brigade, of the 18th Division)';
const KURE = 'Naval landing forces (Kure 1st and 2nd)';
const GREAT_EAST = 'Great East garrisons (Ambon, Celebes, Timor)';
const MENADO = 'Menado garrison (Troepencommando Menado)';
const KENDARI = "Kendari garrison (the airfield's company)";
const D48 = '14th Army (48th Division)';
const T4 = '4th Tank Regiment';
const TANAKA = 'Tanaka and Kanno Detachments (2nd Formosa Infantry, of the 48th Division)';
const D2 = '16th Army (2nd Division)';
const ARMY16 = '16th Army troops (landing with the 2nd Division)';
const SINGKAWANG = "Singkawang garrison (the KNIL's West Borneo detachment)";
const MAKASSAR = "Makassar garrison (the KNIL's South Celebes command)";

/** A dated row as the game reads it once every file is laid: [month, country, name, kind, divisions, place, words]. */
const dated = (name: string): any[] => ((setup as any).expeditions as any[]).find((r) => r[2] === name);
/** A first-day row: [country, name, kind, divisions, place, words]. */
const first = (name: string): any[] => ((world as any).corps as any[]).find((r) => r[1] === name);
const indies = () => ops('JAP').find((op) => op.id === 'south')!.fronts!.find((F) => F.id === 'indies')!;
const INDIES = (): number => ops('JAP').find((op) => op.id === 'south')!.fronts!.findIndex((F) => F.id === 'indies');

/** Stop 3 in month `turn`, the strike south gone in month 28. */
const war = (turn: number): Game => {
  const g = startOpen(createGame(3, null));
  g.turn = turn;
  g.flags['st:done:JAP:south'] = 28;
  for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
  return g;
};
const put = (g: Game, name: string, place: string, str: number): Corps => {
  const c = addCorps(g, 'JAP', name, 'inf', 1, place, { xp: 0.3 });
  c.str = str;
  return c;
};
const japans = (g: Game, name: string): void => { const r = g.byName[name]; r.ctrl = 'JAP'; r.units = r.units.filter((u) => u.nat === 'JAP'); };
/** A place with nobody in its field and no town guard: a landing there walks in. */
const open = (g: Game, name: string): void => { const r = g.byName[name]; r.units = r.units.filter((u) => u.nat === 'JAP'); g.flags[`guard:${r.id}`] = 100; };
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;
const state = (g: Game, id: string) => frontsState(g, 'JAP').find((f) => f.id === id)!;

type Told = { what: string; force: string[]; to: string; from: string; taken?: boolean };
const told: Told[] = [];
const listen = (): void => { told.length = 0; frontWatch.tell = (e) => { told.push({ what: e.what, force: e.force.map((c) => c.name), to: e.to.name, from: e.from.name, taken: e.taken }); }; };
const lands = (): (string | boolean | undefined)[][] => told.filter((t) => t.what === 'land').map((t) => [t.force[0], t.from, t.to, t.taken]);
afterEach(() => { frontWatch.tell = null; frontsClear(); });

describe("the Indies' rows at the record's weights", () => {
  it('Ambon: the Molukken Brigade 0.19 and Gull Force 0.07 against the 228th 0.3 and the 1st Kure force 0.05', () => {
    expect(first(GREAT_EAST).slice(0, 5)).toEqual(['NED', GREAT_EAST, 'inf', 1, 'Ambon']);
    expect(first(GREAT_EAST)[5]).toMatchObject({ str: 0.19, est: 0.19, will: 0.3 });
    expect(dated('Gull Force (2/21st Battalion)')[6]).toMatchObject({ str: 0.07, est: 0.07 });
    expect(dated('228th Infantry Regiment (the Ito Detachment)')[6]).toMatchObject({ str: 0.3, est: 0.3 });
    // (The 2nd Kure force went with the Sakaguchi Detachment to Tarakan and is in its 0.33.)
    expect(dated(KURE)[6]).toMatchObject({ str: 0.05, est: 0.05 });
    expect(dated('Sakaguchi Detachment (56th Mixed Infantry Group)')[6]).toMatchObject({ str: 0.33, est: 0.33 });
    // The record's raw odds at Ambon: about 1.35.
    expect((0.3 + 0.05) / (0.19 + 0.07)).toBeCloseTo(1.35, 2);
  });

  it("the Great East row is split by place: the Celebes' garrisons stand at Menado, Kendari and Makassar from the first day", () => {
    expect(first(MENADO)).toEqual(['NED', MENADO, 'inf', 1, 'Menado', { xp: 0.05, str: 0.1, est: 0.1, hold: 1, works: ['Menado', 0], will: 0.3 }]);
    expect(first(KENDARI)).toEqual(['NED', KENDARI, 'inf', 1, 'Kendari', { xp: 0.05, str: 0.03, est: 0.03, hold: 1, works: ['Kendari', 0], will: 0.3 }]);
    const g = startOpen(createGame(3, null));
    const at = (nm: string): (string | number | boolean)[] => { const c = g.corps.find((x) => x.name === nm && !x.dead)!; return [where(g, c), c.nat, c.str, !!c.hold]; };
    expect(at(MENADO)).toEqual(['Menado', 'NED', 0.1, true]);
    expect(at(KENDARI)).toEqual(['Kendari', 'NED', 0.03, true]);
    expect(at(GREAT_EAST)).toEqual(['Ambon', 'NED', 0.19, true]);
    // (Makassar's: the South Celebes command, about 1,000 men, who had planned not to fight for the town.)
    expect(first(MAKASSAR)).toEqual(['NED', MAKASSAR, 'inf', 1, 'Makassar', { xp: 0.05, str: 0.07, est: 0.07, hold: 1, works: ['Makassar', 0], will: 0.3 }]);
    expect(at(MAKASSAR)).toEqual(['Makassar', 'NED', 0.07, true]);
    // (Timor's garrison of the KNIL, about 600 men, is in Sparrow Force's row.)
    expect(dated('Sparrow Force (2/40th Battalion)')[6]).toMatchObject({ str: 0.13, est: 0.13 });
  });

  it("Palembang: the paratroops 0.03 with the 229th's 0.3, against the KNIL's 0.13 and the gunners' 0.02 in the one row", () => {
    expect(dated('2nd Raiding Regiment (paratroops of the 1st Raiding Group)')[6]).toMatchObject({ str: 0.03, est: 0.03, airborne: 1 });
    expect(dated("229th Infantry Regiment (with the 38th Division's headquarters)")[6]).toMatchObject({ str: 0.3, est: 0.3 });
    expect(first('Palembang garrison (the oil)')[5]).toMatchObject({ str: 0.15, est: 0.15 });
    expect(0.13 + 0.02).toBeCloseTo(0.15, 6);
  });

  it('Java: the Shoji Detachment 0.3; Borneo: the Kawaguchi Detachment 0.325 and Kuching\'s defence 0.1', () => {
    expect(dated('230th Infantry Regiment (the Shoji Detachment)')[6]).toMatchObject({ str: 0.3, est: 0.3 });
    expect(dated(KAWAGUCHI)[6]).toMatchObject({ str: 0.325, est: 0.325, xp: 0.55 });
    // (The 2/15th Punjab, about 1,000 men, with the Sarawak Rangers and volunteers.)
    expect(dated('2/15th Punjab (Kuching)')[6]).toMatchObject({ str: 0.1, est: 0.1 });
    expect(0.07 + 0.03).toBeCloseTo(0.1, 6);
  });

  it("Merak: the 2nd Division at its cap, 1.0, and the 16th Army's troops, 0.1, a row of their own that goes with it", () => {
    expect(dated(D2)[6]).toMatchObject({ str: 1, est: 1 });
    // (At Cam Ranh a month after the war with Britain begins, as Palembang's paratroops come, and at every stop of the
    // dial: the director's 16:18Z. By the book the Java landing's dated attack names it beside the division.)
    expect(dated(ARMY16)).toEqual([29, 'JAP', ARMY16, 'inf', 1, 'Cam Ranh', { xp: 0.15, str: 0.1, est: 0.1, war: ['JAP', 'UK'], after: 1 }]);
    expect(((farWar as any).history.stations.attacks as any[]).find((x) => x.name === 'Java: Bantam Bay and Merak').force).toEqual([D2, ARMY16]);
    // The record's middle at Merak and Bantam Bay, about 1.0 to 1.2: the cap stays and the middle is kept.
    expect(dated(D2)[6].str + dated(ARMY16)[6].str).toBeCloseTo(1.1, 6);
    // It stands beside the division from the month after, and the division's follow-on for Batavia names it.
    expect(((farWar as any).history.stations.rows as any[]).find((r) => r.corps === ARMY16)).toEqual({ nat: 'JAP', corps: ARMY16, beside: [[30, D2]], steps: [] });
    const T = indies().then!.filter((f) => f.corps === D2);
    expect(T.map((f) => [f.month, f.to, f.with, f.after, f.from])).toEqual([[3, ['Batavia'], [ARMY16], ['Palembang'], ['Cam Ranh']]]);
  });

  it("Singkawang: the KNIL's West Borneo detachment, 0.05, stands at the airfield's province from the first day", () => {
    expect(first(SINGKAWANG)).toEqual(['NED', SINGKAWANG, 'inf', 1, 'Singkawang', { xp: 0.05, str: 0.05, est: 0.05, hold: 1, works: ['Singkawang', 0], will: 0.3 }]);
    const g = startOpen(createGame(3, null));
    const c = g.corps.find((x) => x.name === SINGKAWANG && !x.dead)!;
    expect([where(g, c), c.nat, c.str, !!c.hold]).toEqual(['Singkawang', 'NED', 0.05, true]);
    // (A post of the army: the KNIL's 0.8.)
    expect(raisedQuality(g, { name: SINGKAWANG, raised: 'NED' } as never)).toBe(0.8);
  });
});

describe("the KNIL's quality, row by row", () => {
  it('a Java division in the field 0.7, the Molukken Brigade and Menado 0.85, Makassar 0.7, the mobilisation row 0.5; the other posts keep the army\'s 0.8', () => {
    const g = startOpen(createGame(3, null));
    const q = (name: string, raised?: string): number | null => raisedQuality(g, { name, raised });
    for (const nm of ['KNIL 1st Division (West Java)', 'KNIL 2nd Division (Central Java)', 'KNIL 3rd Division (East Java)']) expect([nm, q(nm, 'NED')]).toEqual([nm, 0.7]);
    expect(q(GREAT_EAST, 'NED')).toBe(0.85);
    expect(q("KNIL mobilisation (Java's militia and the Landstorm)", 'NED')).toBe(0.5);
    // (The Celebes by place, the historian's japan-taken-towns-1942.md: Menado's Menadonese 0.85, Makassar's native regulars and militia 0.7.)
    expect([q(MENADO, 'NED'), q(MAKASSAR, 'NED')]).toEqual([0.85, 0.7]);
    // (Regulars in their posts: the army's own figure, for Kendari's and Singkawang's new rows as for the rest.)
    for (const nm of [KENDARI, SINGKAWANG, 'Palembang garrison (the oil)', 'Borneo garrisons (Tarakan and Balikpapan)', 'Sumatra garrisons (Medan and Atjeh)']) expect([nm, q(nm, 'NED')]).toEqual([nm, 0.8]);
  });
});

describe("the Kawaguchi Detachment's follow-ons on the record's dates", () => {
  it('after its own landing at Brunei it takes Miri and Kuching in a month, the table giving both the same month', () => {
    const g = war(29);
    japans(g, 'Brunei');
    for (const nm of ['Miri', 'Kuching', 'Jesselton', 'Sandakan', 'Singkawang', 'Pontianak']) open(g, nm);
    const k = put(g, KAWAGUCHI, 'Brunei', 0.325);
    // (Its first landing, the strike's own, was made in the blow's month.)
    g.flags[sailedKey(k)] = 28;
    listen();
    runAI(g, 'JAP');
    expect(lands()).toEqual([[KAWAGUCHI, 'Brunei', 'Miri', true], [KAWAGUCHI, 'Miri', 'Kuching', true]]);
    expect([g.byName.Miri.ctrl, g.byName.Kuching.ctrl, g.byName.Jesselton.ctrl === 'JAP']).toEqual(['JAP', 'JAP', false]);
    // Ashore at Kuching it waits there as the plan's for the next of its list.
    expect([where(g, k), k.hold]).toEqual(['Kuching', true]);
    frontsTick(g, 'JAP');
    expect(planWaits(k)).toBe(true);
    expect(state(g, 'indies').waiting).toContain(KAWAGUCHI);
  });

  it("its own first landing still owed, it makes that first, the strike's, and no follow-on that month", () => {
    const g = war(29);
    for (const nm of ['Brunei', 'Miri', 'Kuching']) open(g, nm);
    japans(g, 'Cam Ranh');
    const k = put(g, KAWAGUCHI, 'Cam Ranh', 0.325);
    listen();
    runAI(g, 'JAP');
    // (The strike's landing is the plan's own, not a follow-on: Brunei, the first of its two shores.)
    expect(where(g, k)).toBe('Brunei');
    expect(lands()).toEqual([]);
    expect([g.byName.Brunei.ctrl, g.byName.Miri.ctrl === 'JAP', g.byName.Kuching.ctrl === 'JAP']).toEqual(['JAP', false, false]);
    const T = indies().then!;
    expect(T.filter((f, i) => f.corps === KAWAGUCHI && g.flags[followKey('JAP', INDIES(), i)] !== undefined)).toEqual([]);
  });

  it('the list ends at Cebu and Mindanao, four months after the blow', () => {
    const T = indies().then!.filter((f) => f.corps === KAWAGUCHI).map((f) => [f.month, f.to[0]]);
    expect(T).toEqual([[0, 'Miri'], [0, 'Kuching'], [1, 'Jesselton'], [1, 'Sandakan'], [1, 'Singkawang'], [1, 'Pontianak'], [4, 'Cebu'], [4, 'Del Monte']]);
    // (They stand after the list's others, which keep their places: Timor's was the last.)
    const all = indies().then!;
    expect(all.findIndex((f) => f.corps === KAWAGUCHI)).toBe(all.findIndex((f) => f.to[0] === 'Koepang') + 1);
    const g = startOpen(createGame(3, null));
    for (const [, nm] of T) expect(g.byName[nm as string], nm as string).toBeTruthy();
  });
});

describe("the 16th Army's troops go to Batavia with the 2nd Division", () => {
  it("they are the Indies' front's row, waiting as the plan's with the division, and their staff's own if the division is lost", () => {
    const g = war(30);
    japans(g, 'Cam Ranh');
    const d2 = put(g, D2, 'Cam Ranh', 1);
    const army = put(g, ARMY16, 'Cam Ranh', 0.1);
    frontsTick(g, 'JAP');
    expect(state(g, 'indies').waiting).toEqual(expect.arrayContaining([D2, ARMY16]));
    expect([planWaits(d2), planWaits(army)]).toEqual([true, true]);
    d2.dead = true;
    g.turn = 31;
    frontsTick(g, 'JAP');
    expect(planWaits(army)).toBe(false);
  });

  it("on the division's quay, Palembang taken, they land at Batavia in its wave", () => {
    const g = war(31);
    japans(g, 'Cam Ranh'); japans(g, 'Palembang');
    const d2 = put(g, D2, 'Cam Ranh', 1);
    const army = put(g, ARMY16, 'Cam Ranh', 0.1);
    open(g, 'Batavia');
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.to === 'Batavia' && t.what === 'land').map((t) => t.force)).toEqual([[D2, ARMY16]]);
    expect(g.byName.Batavia.ctrl).toBe('JAP');
    expect([where(g, d2), where(g, army)]).toEqual(['Batavia', 'Batavia']);
  });

  // (The director's check of 16:36Z of 21 November 2026: at stops 3 to 5 a rider sails only with its lead's landing,
  // never on its date alone.)
  it("their date come and the division still behind its hop, they sail nowhere: nobody lands at Batavia, and both wait on the quay", () => {
    const g = war(31);
    japans(g, 'Cam Ranh');
    const d2 = put(g, D2, 'Cam Ranh', 1);
    const army = put(g, ARMY16, 'Cam Ranh', 0.1);
    open(g, 'Batavia');
    listen();
    runAI(g, 'JAP');
    expect(told.filter((t) => t.force.includes(ARMY16) && t.what !== 'wait')).toEqual([]);
    expect(g.byName.Batavia.ctrl).not.toBe('JAP');
    expect([where(g, d2), where(g, army), !!army.hold]).toEqual(['Cam Ranh', 'Cam Ranh', true]);
  });
});

describe("by the book the 16th Army's troops sail with the 2nd Division, and the record's fight is as it was", () => {
  it("at stop 1 the Java landing's dated attack takes them in the division's lift; Japan's side comes to the record's 0.25 with them or without", () => {
    const fight = (withArmy: boolean): { a: number; d: number; taken: boolean; at: string[] } => {
      const g = startOpen(createGame(3, null));
      setStop(g, 1, true);
      g.turn = 31;
      g.flags['st:done:JAP:south'] = 28;
      for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
      stations()!.attacks!.forEach((x, i) => { if (x.name !== 'Java: Bantam Bay and Merak') g.flags[`ba:${i}`] = 1; });
      japans(g, 'Cam Ranh');
      const d2 = put(g, D2, 'Cam Ranh', 1);
      const army = withArmy ? put(g, ARMY16, 'Cam Ranh', 0.1) : null;
      let rep: any = null, taken = false;
      bookWatch.tell = (e) => { if (e.what === 'made') { rep = e.report; taken = !!e.taken; } };
      bookAttacks(g, 'JAP');
      bookWatch.tell = null;
      return { a: +(rep?.aPower ?? 0).toFixed(6), d: +(rep?.dPower ?? 0).toFixed(6), taken, at: [where(g, d2), army ? where(g, army) : ''] };
    };
    const without = fight(false), beside = fight(true);
    expect(without.a).toBeGreaterThan(0);
    expect([beside.a, beside.d, beside.taken]).toEqual([without.a, without.d, without.taken]);
    expect(beside.at[1]).toBe(beside.at[0]);
  });
});

describe('the 48th Division goes to Java whole', () => {
  it("its 2nd Formosa Infantry, the Tanaka and Kanno Detachments' row, is Luzon's while Manila holds and the Indies' with the division once it has fallen", () => {
    const g = war(29);
    const d48 = put(g, D48, 'Lingayen', 0.65);
    const tanaka = put(g, TANAKA, 'Aparri', 0.25);
    const t4 = g.corps.find((c) => c.nat === 'JAP' && c.name === T4 && !c.dead)!;
    frontsTick(g, 'JAP');
    expect(state(g, 'luzon').rows).toEqual(expect.arrayContaining([D48, T4, TANAKA]));
    expect(planWaits(tanaka)).toBe(false);
    // Two months after the blow, the sailing's date, all three are Luzon's still: Manila holds, and the sailing waits
    // for it (the director's order for Luzon, (2), of 21 November 2026).
    g.turn = 30;
    frontsTick(g, 'JAP');
    expect(g.byName.Manila.ctrl).toBe('USA');
    expect(state(g, 'luzon').rows).toEqual(expect.arrayContaining([D48, T4, TANAKA]));
    expect([planWaits(d48), planWaits(t4), planWaits(tanaka)]).toEqual([false, false, false]);
    // Manila taken: all three are the Indies', waiting where they stand for the ships to Jolo.
    const manila = g.byName.Manila;
    manila.ctrl = 'JAP';
    manila.units = manila.units.filter((u) => u.nat === 'JAP');
    g.turn = 31;
    frontsTick(g, 'JAP');
    expect(state(g, 'luzon').live).toBe(false);
    expect(state(g, 'indies').waiting).toEqual(expect.arrayContaining([D48, T4, TANAKA]));
    expect([planWaits(d48), planWaits(t4), planWaits(tanaka)]).toEqual([true, true, true]);
  });
});
