// Better air unit names (release 2; Brian 25 November 2026: "can we come up with better names for the airforce
// squadrons?"; the design director's rulings of 03:18Z and 03:31Z, design-director/air-names-1125.md).
//
// (a) to (c) are words only, under the switch 'airNames' (airnames.ts, air.ts groupName): off, every name is ruling
// 220's; on, the arm's own numbers in forming order, its role form, its own plain form where no rung is near, and
// Japan's island groups by their airfield. (d) Earned names and colours for air groups go under the corps' switch
// 'nicknames' (airnicks.ts): missions completed, a hard fight counting double, a name at 'need', no cap, no edge.
//
// The real lists are the historian's (air-unit-names-r2.json, 25 November, laid into air-names.json and nicknames.json).
// Lists marked FIXTURE below are made up for these tests only, laid into the data for one test and taken out after it.
import { afterEach, describe, expect, it } from 'vitest';
import names from '../data/campaign/air-names.json';
import { foundAirFleet, formName, type Rung } from '../src/campaign/air';
import { AIRNAMES, airBlocked, airNamesOn, roleWord, setAirNames, shortName, type RungR2 } from '../src/campaign/airnames';
import { AIRCOLOURS, AIRNICKS, airEarn, airRaidFlown, airStyleKey, blockedAir, calledName, earnedAirName, placeBlocked } from '../src/campaign/airnicks';
import { groupRow } from '../src/campaign/airwar';
import { createGame } from '../src/campaign/game';
import { isBlocked, setNicknames } from '../src/campaign/nicknames';
import { loadGame, saveGame } from '../src/campaign/save';
import { AIRNAMES_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { stepAI } from '../src/campaign/turn';
import type { AirFleet, AirSide, Game } from '../src/campaign/types';

const LADDERS = (names as unknown as { ladders: Record<string, (Rung & RungR2)[]> }).ladders;
const KINDS = ['ftr', 'bmr'] as const;

/** A grand campaign at its first day, nobody at the table, with the air arms' own names on or off. */
function open(on: boolean, seed = 1): Game {
  const g = createGame(seed, null);
  if (on) g.flags.airNames = 1;
  return startOpen(g);
}
function months(g: Game, n: number): void {
  const end = g.turn + n;
  let guard = 0;
  while (g.turn < end && guard++ < 4000) stepAI(g);
}
const rung = (nat: string, name: string): Rung & RungR2 => LADDERS[nat].find((r) => r.rung === name)!;

// Fixtures laid into the data for one test, taken out after it.
const undo: (() => void)[] = [];
function lay<T extends object, K extends keyof T>(o: T, k: K, v: T[K]): void {
  const had = Object.prototype.hasOwnProperty.call(o, k), was = o[k];
  o[k] = v;
  undo.push(() => { if (had) o[k] = was; else delete o[k]; });
}
afterEach(() => { while (undo.length) undo.pop()!(); airEarn.skip = false; });

// The first day's names on the Europe map as ruling 220 gave them before this change (the probe of 25 November, base 1c67ea0d).
const TODAY: Record<string, string[]> = {
  GER: ['Jagdgeschwader 1', 'Jagdgeschwader 2', '1. Fliegerdivision', '2. Fliegerdivision', '1. Jagddivision', '3. Fliegerdivision', '4. Fliegerdivision', '2. Jagddivision', 'Kampfgeschwader 1', 'Kampfgeschwader 2', 'Jagdgeschwader 3', '5. Fliegerdivision', '6. Fliegerdivision'],
  UK: ['No. 11 Group', 'No. 12 Group', 'No. 13 Group', 'No. 1 Group', 'No. 2 Group', 'Home Fleet carrier fighters', 'Home Fleet strike wing', 'Mediterranean Fleet carrier fighters', 'Mediterranean Fleet strike wing'],
  FRA: ['Groupement de Chasse 1', 'Groupement de Chasse 2', '1st Bomber Group', 'Groupement de Chasse 3', 'Groupement de Bombardement 1', 'Atlantic Fleet carrier fighters', 'Atlantic Fleet strike wing'],
  ITA: ['1ª Divisione Aerea', '2ª Divisione Aerea', '3ª Divisione Aerea', '1ª Brigata Aerea', '4ª Divisione Aerea', '2ª Brigata Aerea', '5ª Divisione Aerea'],
  POL: ['Pursuit Brigade', 'Bomber Brigade'],
  SOV: ['1st Fighter Aviation Division', '2nd Fighter Aviation Division', '1st Bomber Aviation Division', '2nd Bomber Aviation Division', '3rd Fighter Aviation Division', '4th Fighter Aviation Division', '5th Fighter Aviation Division', '3rd Bomber Aviation Division', '4th Bomber Aviation Division', '6th Fighter Aviation Division', '7th Fighter Aviation Division', '8th Fighter Aviation Division', '5th Bomber Aviation Division', '6th Bomber Aviation Division', '9th Fighter Aviation Division', '10th Fighter Aviation Division', '7th Bomber Aviation Division', '11th Fighter Aviation Division', '12th Fighter Aviation Division'],
  USA: ['First Air Force', 'I Bomber Command', '1st Pursuit Squadron', '1st Bombardment Group', '1st Pursuit Group', '2nd Bombardment Group', '2nd Pursuit Squadron', '3rd Bombardment Group', 'Atlantic Fleet carrier fighters', 'Atlantic Fleet strike wing'],
};
const byNat = (g: Game): Record<string, string[]> => {
  const out: Record<string, string[]> = {};
  for (const f of g.airFleets) (out[f.nat] ??= []).push(f.name);
  return out;
};

describe('the switch', () => {
  it('ships off; off, every opening group is named as today and nothing of the switch is written', () => {
    expect(AIRNAMES_STAGE).toBe(false);
    const g = open(false);
    expect(airNamesOn(g)).toBe(false);
    expect(byNat(g)).toEqual(TODAY);
    expect(g.airNames).toBeUndefined();
    for (const f of g.airFleets) expect('short' in f || 'roleNamed' in f, f.name).toBe(false);
    expect(saveGame(g)).not.toContain('"airNames"');
  });

  it('on, the arms\' own numbers in forming order, France\'s bombers in French, and every number of the war the same', () => {
    const g = open(true);
    const on = byNat(g);
    expect(on.GER.slice(0, 2)).toEqual(['Jagdgeschwader 2', 'Jagdgeschwader 3']);
    expect(on.GER).toContain('Jagdgeschwader 26');
    expect(on.FRA).toEqual(['Groupement de Chasse 21', 'Groupement de Chasse 22', 'Groupement de Bombardement 6', 'Groupement de Chasse 23', 'Groupement de Bombardement 9', 'Atlantic Fleet carrier fighters', 'Atlantic Fleet strike wing']);
    expect(on.ITA[0]).toBe('1ª Divisione Aerea Caccia');
    expect(on.ITA[1]).toBe('3ª Divisione Aerea Bombardamento');
    expect(on.SOV[0]).toBe('6th Fighter Aviation Division');
    expect(on.UK).toEqual(TODAY.UK);
    expect(on.POL).toEqual(TODAY.POL);
    // Only words: the order and every number of the war are the same.
    expect(g.airFleets.map((f) => [f.id, f.nat, f.ftr, f.bmr, f.base])).toEqual(open(false).airFleets.map((f) => [f.id, f.nat, f.ftr, f.bmr, f.base]));
  });
});

describe('(a) numbers from the arm\'s own list (FIXTURES)', () => {
  it('are handed out in forming order and go on past the end from the largest, never one in use', () => {
    lay(rung('GER', 'Geschwader'), 'numbers', { ftr: [26, 51, 3], bmr: [] });
    const g = open(true);
    expect(byNat(g).GER.filter((n) => n.startsWith('Jagdgeschwader'))).toEqual(['Jagdgeschwader 26', 'Jagdgeschwader 51', 'Jagdgeschwader 3']);
    // An empty list (FIXTURE) numbers as ruling 220 does.
    lay(rung('GER', 'Geschwader'), 'numbers', { ftr: [], bmr: [] });
    expect(byNat(open(true)).GER.filter((n) => n.startsWith('Kampfgeschwader'))).toEqual(['Kampfgeschwader 1', 'Kampfgeschwader 2']);
    const r = g.regions.find((x) => x.ctrl === 'GER')!;
    expect(foundAirFleet(g, 'GER', r, 'X', 'ftr', 120).name).toBe('Jagdgeschwader 52');
    g.airFleets[0].name = 'Jagdgeschwader 53';
    expect(foundAirFleet(g, 'GER', r, 'X', 'ftr', 120).name).toBe('Jagdgeschwader 54');
    // Kept in a book of their own: no counter another rule reads has moved.
    expect(Object.keys(g.flags).some((k) => k.startsWith('agn:GER:ftr:Geschwader'))).toBe(false);
    expect(g.airNames!.at['GER:ftr:Geschwader']).toBe(6);
  });

  it('a list of whole labels goes on in its own manner past the end (FIXTURE), and Germany\'s Gruppen pass over a Geschwader in the field', () => {
    lay(rung('GER', 'Gruppe'), 'numbers', { ftr: ['I./JG 2', 'I./JG 9', 'II./JG 9'], bmr: [] });
    const g = open(true);
    const r = g.regions.find((x) => x.ctrl === 'GER')!;
    // I./JG 2 is passed over: Jagdgeschwader 2 is in the field.
    expect([1, 2, 3, 4, 5].map(() => foundAirFleet(g, 'GER', r, 'X', 'ftr', 40).name)).toEqual(['I./JG 9', 'II./JG 9', 'I./JG 10', 'II./JG 10', 'III./JG 10']);
  });

  it('a short form for the counter where the rung has one ("JG {n}")', () => {
    const g = open(true);
    const jg = g.airFleets.find((f) => f.name === 'Jagdgeschwader 2')!;
    expect(jg.short).toBe('JG 2');
    expect(shortName(jg)).toBe('JG 2');
    expect(groupRow(g, jg).short).toBe('JG 2');
    const uk = g.airFleets.find((f) => f.name === 'No. 11 Group')!;
    expect(shortName(uk)).toBe('No. 11 Group');
  });
});

describe('(b) the role', () => {
  it('is in the name where the arm said it, and shown beside the name where it did not', () => {
    const g = open(true);
    const jg = g.airFleets.find((f) => f.name === 'Jagdgeschwader 2')!, kg = g.airFleets.find((f) => f.name === 'Kampfgeschwader 1')!;
    const uk = g.airFleets.find((f) => f.name === 'No. 11 Group')!, ita = g.airFleets.find((f) => f.name === '3ª Divisione Aerea Bombardamento')!;
    expect(jg.roleNamed).toBe(true);
    expect(roleWord(g, jg)).toBeUndefined();
    expect(groupRow(g, kg).role).toBeUndefined();
    expect(roleWord(g, uk)).toBe('fighters');
    expect(groupRow(g, ita).role).toBeUndefined();
    expect(ita.roleNamed).toBe(true);
  });

  it('the role form is the switch\'s alone: ruling 220\'s form is left as it was', () => {
    const div = rung('ITA', 'divisione');
    expect(div.form.ftr).toBe('{n}ª Divisione Aerea');
    expect(div.formR2!.bmr).toBe('{n}ª Divisione Aerea Bombardamento');
    expect(byNat(open(false)).ITA).toEqual(TODAY.ITA);
  });
});

describe('(c) the plain fallback, and Japan\'s island groups', () => {
  it('no arm with a ladder of its own reads another country\'s word: its fallback is its own', () => {
    const g = open(true);
    const r = g.regions[0];
    for (const nat of Object.keys(LADDERS)) {
      if (nat === 'JAP_navy') continue;
      for (const k of KINDS) {
        const name = foundAirFleet(g, nat, r, 'X', k, 5000).name;
        // It is the arm's own plain form (the historian's), never airwar.json's English one ("3rd Bomber Group").
        expect(name, `${nat} ${k}`).not.toMatch(/^\d+(st|nd|rd|th) Bomber Group$/);
        const forms = [AIRNAMES.fallback[nat]?.[k], ...LADDERS[nat].map((x) => (x.formR2 ?? x.form)[k])].filter(Boolean) as string[];
        const own = forms.some((f) => new RegExp(`^${f.replace(/[.*+?^$()|[\]\\]/g, '\\$&').replace(/\{[^}]+\}/g, '.+')}$`).test(name));
        expect(own, `${nat} ${k}: ${name}`).toBe(true);
      }
    }
  });

  it('a fallback form in data (FIXTURE) is used where no rung is near', () => {
    lay(AIRNAMES.fallback, 'FRA', { ftr: 'Groupement Fixture de Chasse {n}', bmr: 'Groupement Fixture de Bombardement {n}' });
    const g = open(true);
    expect(byNat(g).FRA).toContain('Groupement Fixture de Bombardement 1');
  });

  it('Japan\'s groups on the navy\'s islands take the airfield\'s name at forming, kept for life', () => {
    expect(AIRNAMES.navyIslands).toContain('Truk');
    const g = open(true);
    const truk = { ...g.regions.find((x) => x.ctrl === 'GER')!, name: 'Truk' };
    const a = foundAirFleet(g, 'JAP', truk, 'X', 'ftr', 50), b = foundAirFleet(g, 'JAP', truk, 'X', 'bmr', 40);
    expect(a.name).toBe('Truk Kokutai');
    expect(a.short).toBe('Truk Ku');
    // A second group at the same field: the navy's numbered form (the historian's list).
    expect(b.name).toBe('701st Kokutai');
    expect(roleWord(g, a)).toBe('fighters');
    // Off, the island is the army's like everywhere else (ruling 220's navyBases are empty).
    const h = open(false);
    expect(foundAirFleet(h, 'JAP', { ...truk }, 'X', 'ftr', 50).name).toBe('1st Sentai');
    // Kept: the base moves, the name does not.
    a.base = 0;
    months(g, 1);
    expect(g.airFleets.find((f) => f.id === a.id)?.name ?? 'Truk Kokutai').toBe('Truk Kokutai');
  });

  it('the exiles\' series is a list of its own, going on past its end (FIXTURE list)', () => {
    lay(rung('POL', 'squadron in exile'), 'numbers', { ftr: [306, 315], bmr: [] });
    const g = open(true);
    g.turn = 12;
    g.regions[g.nations.POL.capital].ctrl = 'GER';
    const r = g.regions.find((x) => x.ctrl === 'UK')!;
    expect([1, 2, 3].map(() => foundAirFleet(g, 'POL', r, 'X', 'ftr', 16).name)).toEqual(['No. 306 (Polish) Squadron', 'No. 315 (Polish) Squadron', 'No. 316 (Polish) Squadron']);
    expect(g.airNames!.at['POL:ftr:eskadra']).toBeUndefined();
  });
});

describe('a name is kept', () => {
  it('a saved war keeps its names and its book, and names the next group as it would have', () => {
    lay(rung('GER', 'Geschwader'), 'numbers', { ftr: [26, 51], bmr: [] });
    const g = open(true);
    const back = loadGame(saveGame(g))!;
    expect(back.airFleets.map((f) => f.name)).toEqual(g.airFleets.map((f) => f.name));
    expect(back.airNames).toEqual(g.airNames);
    expect(saveGame(back)).toBe(saveGame(g));
    const r = g.regions.find((x) => x.ctrl === 'GER')!;
    expect(foundAirFleet(back, 'GER', back.regions[r.id], 'X', 'ftr', 120).name).toBe(foundAirFleet(g, 'GER', r, 'X', 'ftr', 120).name);
  });

  it('a group formed with the switch off keeps its name when it is turned on; new groups take the new names', () => {
    const g = open(false);
    const was = new Map(g.airFleets.map((f) => [f.id, f.name]));
    setAirNames(g, true);
    expect(airNamesOn(g)).toBe(true);
    months(g, 1);
    for (const f of g.airFleets) if (was.has(f.id)) expect(f.name).toBe(was.get(f.id));
    expect(foundAirFleet(g, 'FRA', g.regions[g.nations.FRA.capital], 'X', 'bmr', 5000).name).toMatch(/^Groupement de Bombardement \d+$/);
    setAirNames(g, false);
    expect(g.flags.airNames).toBeUndefined();
  });
});

describe('the block list', () => {
  it('holds no name the ladders, the island bases, the fixtures or the earned styles can give', () => {
    const all: string[] = [];
    // Every number of every list, and the first past its end, on every form of every rung; the island bases.
    for (const rungs of Object.values(LADDERS)) for (const r of rungs) for (const k of KINDS) for (const form of [r.form[k], r.formR2?.[k], r.short?.[k]]) {
      if (!form) continue;
      const ns: (number | string)[] = [1, 2, 3, ...(r.numbers?.[k] ?? [])];
      for (const n of ns) all.push(formName(form, n));
      if (/\{base\}/.test(form)) for (const base of AIRNAMES.navyIslands) all.push(form.replace(/\{base\}/g, base));
    }
    for (const f of Object.values(AIRNAMES.fallback)) for (const k of KINDS) for (const n of [1, 2, 3, ...(f.numbers?.[k] ?? [])]) all.push(formName(f[k], n));
    // The fixtures of this file.
    all.push('Jagdgeschwader 26', 'JG 2', 'No. 306 (Polish) Squadron', 'Groupement Fixture de Chasse 1', '1ª Divisione Aerea (fixture) Caccia');
    for (const [key, s] of Object.entries(AIRNICKS.styles)) {
      for (const n of s.any) all.push(n);
      for (const t of s.place) all.push(t.replace('{place}', 'Testplace'));
      if (s.guards) for (const row of Object.values(s.guards)) if (typeof row === 'object') for (const k of KINDS) for (const n of [1, ...row[k].numbers]) all.push(formName(row[k].form, n), formName(row[k].form.replace('Guards', s.guards.city.replace('{city}', 'Testplace')), n), formName(row[k].short, n));
      expect(s.any.length, key).toBeGreaterThanOrEqual(8);
      expect(new Set(s.any).size, key).toBe(s.any.length);
      expect(s.show).toContain('{name}');
      expect(s.show).toContain('{nick}');
    }
    expect(all.length).toBeGreaterThan(1000);
    for (const n of all) {
      expect(isBlocked(n), n).toBe(false);
      expect(airBlocked(n), n).toBe(false);
    }
    // The block list does block: the real names the thread named are kept out.
    expect(blockedAir('Falco')).toBe(true);
    expect(blockedAir('the Flying Tigers')).toBe(true);
  });

  it('Italy\'s earned list is its wind and myth words (its day-one names were dropped, 03:31Z)', () => {
    expect(airStyleKey('ITA')).toBe('italian');
    expect(AIRNICKS.styles.italian.any).toContain('Tramontana');
    for (const n of AIRNICKS.styles.italian.any) expect(blockedAir(n), n).toBe(false);
    expect((names as Record<string, unknown>).dayOne).toBeUndefined();
  });
});

// ---- (d) Earned names and colours for air groups, under the switch 'nicknames' ----

const side = (f: AirFleet, ftr: number, bmr: number, lostFtr = 0, lostBmr = 0): AirSide => ({ nat: f.nat, fleet: f.name, id: f.id, base: f.base, leader: null, ftr, bmr, lostFtr, lostBmr });
function nickWar(): Game {
  const g = open(false);
  setNicknames(g, true);
  return g;
}
const raid = { odds: 3, bombers: 100, bombed: 90, E: 50, D: 40, sky: false };

describe('missions completed', () => {
  it('a raid that reached its target counts one; turned back, or a month with no enemy met, counts nothing', () => {
    const g = nickWar();
    const [b, f, d] = [g.airFleets.find((x) => x.nat === 'GER' && x.kind === 'bmr')!, g.airFleets.find((x) => x.nat === 'GER' && x.kind === 'ftr')!, g.airFleets.find((x) => x.nat === 'POL')!];
    airRaidFlown(g, 'bomb', { ...raid, bombed: 0 }, [{ f: b, side: side(b, 0, 100) }], [], 5);
    expect(b.done).toBeUndefined();
    airRaidFlown(g, 'superiority', { odds: 9, bombers: 0, bombed: 0, E: 80, D: 0, sky: false }, [{ f, side: side(f, 80, 0) }], [], 5);
    expect(f.done).toBeUndefined();
    airRaidFlown(g, 'bomb', raid, [{ f: b, side: side(b, 0, 100) }], [{ f: d, side: side(d, 40, 0) }], 5);
    expect(b.done!.n).toBe(1);
    // (The defence met a raid three times its strength: held, a hard fight, double.)
    expect(d.done!.n).toBe(2);
    // One a month.
    airRaidFlown(g, 'bomb', raid, [{ f: b, side: side(b, 0, 100) }], [], 5);
    expect(b.done!.n).toBe(1);
    g.turn++;
    airRaidFlown(g, 'bomb', raid, [{ f: b, side: side(b, 0, 100) }], [], 5);
    expect(b.done!.n).toBe(2);
  });

  it('each of the three hard fights counts double: pressed home, held, and costly', () => {
    const g = nickWar();
    const [b, d, c] = [g.airFleets.filter((x) => x.nat === 'GER' && x.kind === 'bmr')[0], g.airFleets.find((x) => x.nat === 'POL')!, g.airFleets.filter((x) => x.nat === 'GER' && x.kind === 'bmr')[1]];
    // Pressed home: the raiders' fighter ratio at most even against a defence that came up.
    airRaidFlown(g, 'bomb', { ...raid, odds: AIRNICKS.hard.pressed }, [{ f: b, side: side(b, 0, 100) }], [], 5);
    expect(b.done).toMatchObject({ n: 2, h: 1 });
    // Held: a defence that met a raid at least as strong as itself.
    airRaidFlown(g, 'bomb', { ...raid, odds: AIRNICKS.hard.held }, [], [{ f: d, side: side(d, 40, 0) }], 5);
    expect(d.done).toMatchObject({ n: 2, h: 1 });
    // Costly: twice the arm's running loss rate on that kind of mission (2% here, once 'least' aircraft have flown it).
    g.airRates = { 'GER:bomb': [AIRNICKS.hard.least * 5, AIRNICKS.hard.least / 10] };
    airRaidFlown(g, 'bomb', raid, [{ f: c, side: side(c, 0, 100, 0, 3) }], [], 5);
    expect(c.done).toMatchObject({ n: 1, h: 0 });
    g.turn++;
    airRaidFlown(g, 'bomb', raid, [{ f: c, side: side(c, 0, 100, 0, 5) }], [], 5);
    expect(c.done).toMatchObject({ n: 3, h: 1 });
    // The arm's running tally took both raids in.
    expect(g.airRates['GER:bomb']).toEqual([AIRNICKS.hard.least * 5 + 200, AIRNICKS.hard.least / 10 + 8]);
  });
});

describe('earned names and colours', () => {
  it('come at the mark, in the arm\'s manner, unique, never blocked, with no cap; colours unique while the palette lasts', () => {
    const g = nickWar();
    const ger = g.airFleets.filter((x) => x.nat === 'GER');
    for (const f of ger) {
      f.done = { n: AIRNICKS.need - 1, at: 0, x: 0, h: 0 };
      airRaidFlown(g, 'bomb', raid, [{ f, side: side(f, 0, 100) }], [], g.nations.POL.capital);
    }
    // No cap: every group that reached the mark is named.
    expect(ger.every((f) => f.nick)).toBe(true);
    expect(new Set(ger.map((f) => f.nick)).size).toBe(ger.length);
    for (const f of ger) {
      expect(blockedAir(f.nick!), f.nick).toBe(false);
      expect(calledName(f)).toBe(`${f.name} „${f.nick}“`);
      expect(f.colours).toMatchObject({ tier: 1, since: g.turn });
      expect(f.log?.some((l) => l[1] === 'airnamed')).toBe(true);
    }
    expect(ger.length).toBeLessThanOrEqual(AIRCOLOURS.count);
    expect(new Set(ger.map((f) => f.colours!.scheme)).size).toBe(ger.length);
    // Past the palette's length: the least worn scheme.
    const r = g.regions.find((x) => x.ctrl === 'GER')!;
    while (g.airFleets.filter((x) => x.nat === 'GER').length < AIRCOLOURS.count + 2) foundAirFleet(g, 'GER', r, 'X', 'ftr', 100);
    for (const f of g.airFleets.filter((x) => x.nat === 'GER' && !x.colours)) {
      f.done = { n: AIRNICKS.need - 1, at: 0, x: 0, h: 0 };
      airRaidFlown(g, 'bomb', raid, [{ f, side: side(f, 0, 100) }], [], -1);
    }
    const worn = new Array(AIRCOLOURS.count).fill(0);
    for (const f of g.airFleets) if (f.nat === 'GER') worn[f.colours!.scheme]++;
    expect(Math.max(...worn) - Math.min(...worn)).toBeLessThanOrEqual(1);
    // Further tiers at further counts.
    const f = ger[0];
    f.done!.n = AIRCOLOURS.tiers[2] - 1; f.done!.at = 0;
    airRaidFlown(g, 'bomb', raid, [{ f, side: side(f, 0, 100) }], [], -1);
    expect(f.colours!.tier).toBe(3);
  });

  it('a Soviet group is made Guards: a new number from the Guards series of its rung (FIXTURE list) and the city it fought over', () => {
    const G = AIRNICKS.styles.soviet.guards!;
    lay(G.division.ftr, 'numbers', [5, 9]);
    const g = nickWar();
    const sov = g.airFleets.filter((x) => x.nat === 'SOV' && x.kind === 'ftr' && /Fighter Aviation Division$/.test(x.name)).slice(0, 4);
    const city = g.regions[g.nations.SOV.capital];
    const got = sov.slice(0, 3).map((f) => {
      const was = f.name;
      f.done = { n: AIRNICKS.need - 1, at: 0, x: 0, h: 0 };
      airRaidFlown(g, 'superiority', { odds: 1, bombers: 0, bombed: 0, E: 80, D: 60, sky: false }, [{ f, side: side(f, 80, 0) }], [], city.id);
      expect(f.was).toBe(was);
      return f.name;
    });
    // (Changed on the design director's ruling of 12:59Z, question 5: the city of a Guards title is that of the group's
    // latest hard fight, and an odds-1 fight is ordinary under the 0.8 / 1.25 gap of 05:12Z, so these groups have no hard
    // fight on record and take the title without a city. This test used to give them the city they flew over.)
    expect(sov.slice(0, 3).every((f) => f.hardAt === undefined)).toBe(true);
    expect(got).toEqual(['5th Guards Fighter Aviation Division', '9th Guards Fighter Aviation Division', '10th Guards Fighter Aviation Division']);
    const at = placeBlocked(city.name) ? '' : `${city.name} `;
    expect(sov[0].short).toBe('5 GvIAD');
    expect(sov[0].nick).toBeUndefined();
    expect(earnedAirName(g, sov[3], city.name).name).toBe(`11th Guards ${at}Fighter Aviation Division`);
  });

  it('off: nothing is counted or written', () => {
    const g = open(false);
    months(g, 6);
    expect(g.airRates).toBeUndefined();
    expect(g.airFleets.some((f) => f.done || f.colours || f.guards !== undefined)).toBe(false);
  }, 60_000);

  it('a name changes no number: a war with them earned plays as the same war with the air part left out', () => {
    const outcome = (g: Game): string => JSON.stringify({
      rng: g.rng.state, turn: g.turn,
      air: g.airFleets.map((f) => [f.id, f.nat, f.ftr, f.bmr, f.base, f.mission, f.target, f.ready, f.lost]),
      corps: g.corps.map((c) => [c.id, c.reg, c.divs, c.str, c.org, c.dead]),
      regions: g.regions.map((r) => [r.ctrl, r.owner]),
      pools: g.airPool,
    });
    const a = nickWar();
    months(a, 8);
    airEarn.skip = true;
    const b = nickWar();
    months(b, 8);
    expect(a.airFleets.some((f) => f.done)).toBe(true);
    expect(b.airFleets.some((f) => f.done)).toBe(false);
    expect(outcome(a)).toBe(outcome(b));
  }, 120_000);
});
