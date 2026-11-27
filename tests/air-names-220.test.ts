// Air groups named for formations of their own size (ruling 220, logs b352 and b359; the historian's ladders in
// data/campaign/air-names.json). The campaign's groups were named for real formations far smaller than their counts
// ("Groupe de Chasse 3: 175 fighters", where a real groupe had 25 to 30). The strengths stay; each group takes, once,
// the name of the rung of its country's ladder nearest its full strength (near: half to twice the rung's), and
// where no rung is near, the plain form, never another country's word. Data and words only.
import { describe, expect, it } from 'vitest';
import names from '../data/campaign/air-names.json';
import { foundAirFleet, formName, nearestRung, rungsOf, type Rung } from '../src/campaign/air';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { loadGame, saveGame } from '../src/campaign/save';
import { runTurnAI } from '../src/campaign/turn';
import type { AirFleet, Game } from '../src/campaign/types';

const LADDERS = (names as unknown as { ladders: Record<string, Rung[]> }).ladders;
const KINDS = ['ftr', 'bmr'] as const;
/** A form of name as a pattern: any number where a token stands. */
const pattern = (form: string): RegExp => new RegExp(`^${form.replace(/[.*+?^$()|[\]\\]/g, '\\$&').replace(/\{[^}]+\}/g, '\\S+')}$`);
const PLAIN = /^\d+(st|nd|rd|th) (Fighter|Bomber) Group$/;
const full = (f: AirFleet): number => Math.max(f.est[f.kind!], f[f.kind!]);

describe('the ladders of air formation names', () => {
  it('use only the tokens ruled: {n}, {nth}, {n+K}, {nth_fr}, {nth_nl}', () => {
    for (const [nat, rungs] of Object.entries(LADDERS)) for (const r of rungs) for (const k of KINDS) {
      for (const t of r.form[k].match(/\{[^}]+\}/g) ?? []) expect(/^\{(n|nth|n\+\d+|nth_fr|nth_nl)\}$/.test(t), `${nat} ${r.rung}: ${t}`).toBe(true);
      expect(r[k], `${nat} ${r.rung} ${k}`).toBeGreaterThan(0);
    }
  });

  it('write their numbers in each language', () => {
    expect(formName('{nth_fr} Escadre de Chasse', 1)).toBe('1re Escadre de Chasse');
    expect(formName('{nth_fr} Régiment Aérien', 1)).toBe('1er Régiment Aérien');
    expect(formName('{nth_fr} Escadre de Chasse', 3)).toBe('3e Escadre de Chasse');
    expect(formName('{nth_nl} Luchtvaartregiment', 2)).toBe('2e Luchtvaartregiment');
    expect(formName('No. {n+301} (Polish) Squadron', 1)).toBe('No. 302 (Polish) Squadron');
    expect(formName('{nth} Fighter Group', 22)).toBe('22nd Fighter Group');
  });
});

describe('an air group\'s name', () => {
  const g: Game = startOpen(createGame(1, null));
  const split = g.airFleets.filter((f) => f.kind && f.parent && f.name !== f.parent);

  it('is the rung nearest its full strength, within half to twice the rung, or the plain form', () => {
    expect(split.length).toBeGreaterThan(40);
    for (const f of split) {
      const r = nearestRung(rungsOf(g, f.nat, g.regions[f.base]), f.kind!, full(f));
      if (!r) { expect(f.name, `${f.nat} ${full(f)}`).toMatch(PLAIN); continue; }
      expect(f.name, `${f.nat} ${full(f)} ${f.kind} as ${r.rung}`).toMatch(pattern(r.form[f.kind!]));
      expect(full(f) / r[f.kind!]).toBeGreaterThanOrEqual(0.5);
      expect(full(f) / r[f.kind!]).toBeLessThanOrEqual(2);
    }
    // France's groupes no longer carry six times their strength.
    expect(split.filter((f) => /^Groupe de Chasse/.test(f.name) && full(f) > 52)).toEqual([]);
    expect(new Set(split.map((f) => `${f.nat}:${f.name}`)).size).toBe(split.length);
  });

  it('is taken once and kept as the group is worn down', () => {
    const h: Game = startOpen(createGame(2, null));
    const before = new Map(h.airFleets.map((f) => [f.id, f.name]));
    for (let t = 0; t < 3; t++) runTurnAI(h);
    for (const f of h.airFleets) if (before.has(f.id)) expect(f.name).toBe(before.get(f.id));
  });

  it('is given to a group the factories form, by the wing it forms with', () => {
    const h: Game = startOpen(createGame(3, null));
    const r = h.regions.find((x) => x.ctrl === 'GER')!;
    expect(foundAirFleet(h, 'GER', r, 'X', 'ftr', 25).name).toMatch(/^Jagdgruppe \d+$/);
    expect(foundAirFleet(h, 'GER', r, 'X', 'bmr', 600).name).toMatch(PLAIN);
  });
});

describe('the rungs a country may use', () => {
  it('follow the calendar: America\'s pursuit groups are fighter groups from May 1942', () => {
    const g: Game = startOpen(createGame(1, null));
    expect(rungsOf(g, 'USA', null).map((r) => r.form.ftr)).toContain('{nth} Pursuit Group');
    g.turn = 33;
    expect(rungsOf(g, 'USA', null).map((r) => r.form.ftr)).toContain('{nth} Fighter Group');
    expect(rungsOf(g, 'USA', null).map((r) => r.form.ftr)).not.toContain('{nth} Pursuit Group');
  });

  it('are the forms of exile only while the homeland is lost', () => {
    const g: Game = startOpen(createGame(1, null));
    g.turn = 12;
    expect(rungsOf(g, 'POL', null).some((r) => /in exile/.test(r.rung))).toBe(false);
    g.regions[g.nations.POL.capital].ctrl = 'GER';
    const exile = rungsOf(g, 'POL', null);
    expect(exile.length).toBeGreaterThan(0);
    expect(exile.every((r) => /in exile/.test(r.rung))).toBe(true);
  });

  it('are the Japanese army\'s wherever the map cannot tell an island base', () => {
    const g: Game = startOpen(createGame(1, null));
    expect(rungsOf(g, 'JAP', g.regions[0]).map((r) => r.rung)).toEqual(LADDERS.JAP.map((r) => r.rung));
  });
});

describe('a squadron in a save', () => {
  it('comes back a squadron: the old fleet words are not read into a grand campaign written today', () => {
    const g: Game = startOpen(createGame(3, 'USA'));
    const sq = g.airFleets.find((f) => f.nat === 'USA' && /Squadron$/.test(f.name))!;
    expect(sq.name).toBe('1st Pursuit Squadron');
    const text = saveGame(g);
    expect(text.startsWith('{"v":1,"words":1,')).toBe(true);
    const h = loadGame(text)!;
    expect(h.airFleets.find((f) => f.id === sq.id)!.name).toBe('1st Pursuit Squadron');
    expect(saveGame(h)).toBe(text);
  });

  it('is no mark on a historical campaign\'s save, which is written as it always was', () => {
    expect(saveGame(createGame(3, 'GER')).startsWith('{"v":1,"seed":3,')).toBe(true);
  });
});
