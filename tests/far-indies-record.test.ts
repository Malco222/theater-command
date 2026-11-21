// The Indies' record as data (the design director's rulings of 15:16Z and 15:27Z of 21 November 2026 on the
// historian's indies-landings-1941-42.md: accepted as data, middles, and laid now: a fact is never held back, nor
// laid, to move a reading; data/campaign/far-shores.json and far-pacific-war.json).
//
// Ambon: the 228th 0.3 and the 1st Kure force 0.05 against the Molukken Brigade 0.19 at 0.85 and Gull Force 0.07,
// the Great East row split by place (Menado 0.1, Kendari 0.03; Timor's in Sparrow Force's row). Palembang: the
// paratroops 0.03 with the 229th against 0.15. Java: the Shoji Detachment 0.3; a KNIL division in the field 0.7; the
// 48th goes to Kragan whole. Borneo: the Kawaguchi Detachment 0.325, Kuching's defence 0.1, and the detachment's
// follow-ons on the record's dates. No rule is new. The states are set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { frontWatch, runAI } from '../src/campaign/ai';
import { addCorps, createGame, declareWar } from '../src/campaign/game';
import { raisedQuality } from '../src/campaign/history';
import { followKey, frontsClear, frontsState, frontsTick, planWaits, sailedKey } from '../src/campaign/planfronts';
import { ops, startOpen } from '../src/campaign/staff';
import '../src/campaign/far-rows';
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

  it("the Great East row is split by place: the Celebes' garrisons stand at Menado and Kendari from the first day", () => {
    expect(first(MENADO)).toEqual(['NED', MENADO, 'inf', 1, 'Menado', { xp: 0.05, str: 0.1, est: 0.1, hold: 1, works: ['Menado', 0], will: 0.3 }]);
    expect(first(KENDARI)).toEqual(['NED', KENDARI, 'inf', 1, 'Kendari', { xp: 0.05, str: 0.03, est: 0.03, hold: 1, works: ['Kendari', 0], will: 0.3 }]);
    const g = startOpen(createGame(3, null));
    const at = (nm: string): (string | number | boolean)[] => { const c = g.corps.find((x) => x.name === nm && !x.dead)!; return [where(g, c), c.nat, c.str, !!c.hold]; };
    expect(at(MENADO)).toEqual(['Menado', 'NED', 0.1, true]);
    expect(at(KENDARI)).toEqual(['Kendari', 'NED', 0.03, true]);
    expect(at(GREAT_EAST)).toEqual(['Ambon', 'NED', 0.19, true]);
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
});

describe("the KNIL's quality, row by row", () => {
  it('a Java division in the field 0.7, the Molukken Brigade 0.85, the mobilisation row 0.5; the posts keep the army\'s 0.8', () => {
    const g = startOpen(createGame(3, null));
    const q = (name: string, raised?: string): number | null => raisedQuality(g, { name, raised });
    for (const nm of ['KNIL 1st Division (West Java)', 'KNIL 2nd Division (Central Java)', 'KNIL 3rd Division (East Java)']) expect([nm, q(nm, 'NED')]).toEqual([nm, 0.7]);
    expect(q(GREAT_EAST, 'NED')).toBe(0.85);
    expect(q("KNIL mobilisation (Java's militia and the Landstorm)", 'NED')).toBe(0.5);
    // (Regulars in their posts: the army's own figure, for the Celebes' new rows as for the rest.)
    for (const nm of [MENADO, KENDARI, 'Palembang garrison (the oil)', 'Borneo garrisons (Tarakan and Balikpapan)', 'Sumatra garrisons (Medan and Atjeh)']) expect([nm, q(nm, 'NED')]).toEqual([nm, 0.8]);
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

describe('the 48th Division goes to Java whole', () => {
  it("its 2nd Formosa Infantry, the Tanaka and Kanno Detachments' row, is Luzon's until the division's date and the Indies' from it", () => {
    const g = war(29);
    const d48 = put(g, D48, 'Lingayen', 0.65);
    const tanaka = put(g, TANAKA, 'Aparri', 0.25);
    const t4 = g.corps.find((c) => c.nat === 'JAP' && c.name === T4 && !c.dead)!;
    frontsTick(g, 'JAP');
    expect(state(g, 'luzon').rows).toEqual(expect.arrayContaining([D48, T4, TANAKA]));
    expect(planWaits(tanaka)).toBe(false);
    // Two months after the blow all three are the Indies', waiting where they stand, though Manila holds.
    g.turn = 30;
    frontsTick(g, 'JAP');
    expect(g.byName.Manila.ctrl).toBe('USA');
    expect(state(g, 'luzon').rows).not.toContain(TANAKA);
    expect(state(g, 'indies').waiting).toEqual(expect.arrayContaining([D48, T4, TANAKA]));
    expect([planWaits(d48), planWaits(t4), planWaits(tanaka)]).toEqual([true, true, true]);
  });
});
