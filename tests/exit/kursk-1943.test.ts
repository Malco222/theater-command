// Kursk and the road to the Dnieper, run against the campaign rules with every nation on the historical
// plan and nobody at the controls. The spring of 1943 is quiet. In June the Red Army digs in round the
// Kursk salient and brings up its reserve fronts and tank armies, and Germany gathers its armour at Orel
// and Kharkov. In July Germany attacks the salient once and is held. The Soviet strokes that follow take
// Orel and Kharkov, then Smolensk, Bryansk and the Donbas; in the autumn the Germans fall back to the
// Dnieper, the Red Army crosses it, and Kiev is free before the year is out.
//
// The plan only sets intentions. Every region still has to be won under the rules the player
// uses, so the bands below are loose on purpose and are checked across twenty sets of dice.
import { describe, expect, it } from 'vitest';
import { createGame, men } from '../../src/campaign/game';
import { runTurnAI } from '../../src/campaign/turn';
import { KIND, type Game } from '../../src/campaign/types';

const APRIL_43 = 44, MAY_43 = 45, JUNE_43 = 46, JULY_43 = 47, AUG_43 = 48, SEPT_43 = 49, OCT_43 = 50, NOV_43 = 51, DEC_43 = 52;
const WATCH = ['Kursk', 'Orel', 'Kharkov', 'Sumy', 'Smolensk', 'Bryansk', 'Poltava', 'Stalino', 'Novorossiysk', 'Chernigov', 'Dnepropetrovsk', 'Zaporozhye', 'Kiev', 'Zhitomir', 'Gomel',
  'Vitebsk', 'Mogilev', 'Minsk', 'Novgorod', 'Pskov', 'Crimea', 'Nikolaev', 'Odessa', 'Leningrad', 'Moscow', 'Stalingrad', 'Voronezh'];
/** The armies the Red Army forms for the summer, and where. */
const AT_KURSK = ['Central Front', 'Voronezh Front', '1st Tank Army', '2nd Tank Army'];
const BEHIND = ['Steppe Front', '5th Guards Tank Army', '3rd Guards Tank Army', '4th Tank Army'];

interface Fight { turn: number; place: string; by: string; odds: number; taken: boolean; attLost: number; defLost: number; corps: number; lostBy: Record<string, number> }

interface Run {
  g: Game;
  /** Who held each place at the end of each month from May 1943. */
  held: Record<number, Record<string, string>>;
  /** Every battle fought on Soviet soil from April 1943. */
  fights: Fight[];
  /** The Red Army as May ends and as June ends. */
  redArmy: Record<number, { quality: number; slow: boolean; maxAttack: number; air: number }>;
  /** As June ends: the works at Kursk, where each new army stands and whether it is tied to its ground, and Germany's fast corps on each shoulder. */
  june: { works: number; armies: Record<string, { at: string; held: boolean } | null>; panzers: { orel: number; kharkov: number } };
  /** German men on Soviet soil at the end of each month. */
  germans: Record<number, number>;
}

function history(seed: number): Run {
  const g = createGame(seed, null);
  const run: Run = { g, held: {}, fights: [], redArmy: {}, june: { works: 0, armies: {}, panzers: { orel: 0, kharkov: 0 } }, germans: {} };
  const east = (id: number): boolean => g.regions[id].owner === 'SOV';
  let seen = 0;
  for (let t = 1; t <= DEC_43; t++) {
    // The month's diplomacy is settled as the month opens: read the Red Army before the next one does.
    const n = g.nations.SOV;
    if (t === MAY_43 || t === JUNE_43) run.redArmy[t] = { quality: n.quality, slow: !!n.slowCommand, maxAttack: n.maxAttack ?? 0, air: n.air };
    runTurnAI(g);
    if (t >= APRIL_43) {
      for (const r of g.reports.slice(seen)) {
        if (!east(r.region)) continue;
        const lostBy: Record<string, number> = { [r.attNat]: r.attLost };
        for (const u of r.defenders) lostBy[u.nat] = (lostBy[u.nat] ?? 0) + u.lost;
        run.fights.push({ turn: t, place: r.regionName, by: r.attNat, odds: r.odds, taken: r.taken, attLost: r.attLost, defLost: r.defLost, corps: r.attackers.length, lostBy });
      }
    }
    seen = g.reports.length;
    if (t >= MAY_43) {
      run.held[t] = Object.fromEntries(WATCH.map((nm) => [nm, g.byName[nm].ctrl]));
      run.germans[t] = g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort' && east(c.reg)).reduce((s, c) => s + men(c), 0);
    }
    if (t === JUNE_43) {
      run.june.works = g.byName['Kursk'].fort?.lv ?? 0;
      for (const nm of [...AT_KURSK, ...BEHIND]) {
        const c = g.corps.find((x) => x.name === nm && x.nat === 'SOV' && !x.dead);
        run.june.armies[nm] = c ? { at: g.regions[c.reg].name, held: !!c.hold } : null;
      }
      const fast = (nm: string): number => g.byName[nm].units.filter((u) => u.nat === 'GER' && KIND[u.kind].mobile).length;
      run.june.panzers = { orel: fast('Orel'), kharkov: fast('Kharkov') };
    }
  }
  return run;
}

const SEEDS = Array.from({ length: 20 }, (_, i) => i + 1);
const runs = SEEDS.map(history);
const count = (ok: (r: Run) => boolean): number => runs.filter(ok).length;
/** The wars in which the summer opens as it did: the Red Army in Kursk, with Germany at Orel and Kharkov on either side of it. */
const salient = (r: Run): boolean => r.held[JUNE_43]['Kursk'] === 'SOV' && r.held[JUNE_43]['Orel'] === 'GER' && r.held[JUNE_43]['Kharkov'] === 'GER';
const citadel = (r: Run): Fight[] => r.fights.filter((f) => f.by === 'GER' && f.place === 'Kursk' && f.turn >= JUNE_43);
const soviet = (r: Run, t: number, ...places: string[]): boolean => places.every((nm) => r.held[t][nm] === 'SOV');
const german = (r: Run, t: number, ...places: string[]): boolean => places.every((nm) => r.held[t][nm] === 'GER');
/** Men each side lost in the battles of these months. */
const lost = (r: Run, from: number, to: number): { ger: number; sov: number } => {
  const out = { ger: 0, sov: 0 };
  for (const f of r.fights) if (f.turn >= from && f.turn <= to) { out.ger += f.lostBy.GER ?? 0; out.sov += f.lostBy.SOV ?? 0; }
  return out;
};

describe('exit test: Kursk and the Dnieper on the historical plan', () => {
  it('the summer opens as it did in most wars: a Soviet salient at Kursk between Orel and Kharkov', () => {
    expect(count(salient)).toBeGreaterThanOrEqual(14);
  });

  it('June 1943 is quiet: both armies wait, and nothing is fought in the east', () => {
    expect(count((r) => r.fights.every((f) => f.turn !== JUNE_43))).toBeGreaterThanOrEqual(18);
    expect(count((r) => r.fights.filter((f) => f.turn === APRIL_43 || f.turn === MAY_43).length <= 2)).toBeGreaterThanOrEqual(16);
  });

  it('two years of war have remade the Red Army by June 1943', () => {
    for (const r of runs) {
      expect(r.redArmy[MAY_43]).toEqual({ quality: 0.7, slow: true, maxAttack: 8, air: 70 });
      expect(r.redArmy[JUNE_43]).toEqual({ quality: 0.8, slow: false, maxAttack: 14, air: 100 });
    }
  });

  it('the Red Army digs in round Kursk and brings up its reserve fronts and tank armies behind it', () => {
    for (const r of runs) {
      // The armies are formed whatever the map looks like: none of them is lost if the place they were meant for has fallen.
      for (const nm of [...AT_KURSK, ...BEHIND]) expect(r.june.armies[nm]).not.toBeNull();
      if (!salient(r)) continue;
      expect(r.june.works).toBe(3);
      for (const nm of AT_KURSK) expect(r.june.armies[nm]).toEqual({ at: 'Kursk', held: true });
      expect(r.june.armies['Steppe Front']).toMatchObject({ held: false });
    }
    expect(count((r) => salient(r) && r.june.armies['Steppe Front']!.at === 'Voronezh' && r.june.armies['5th Guards Tank Army']!.at === 'Voronezh')).toBeGreaterThanOrEqual(13);
  });

  it('Germany gathers its tanks on both shoulders of the salient', () => {
    expect(count((r) => salient(r) && r.june.panzers.orel + r.june.panzers.kharkov >= 6)).toBeGreaterThanOrEqual(11);
    expect(count((r) => salient(r) && r.june.panzers.kharkov >= 2)).toBeGreaterThanOrEqual(13);
  });

  it('Citadel: Germany attacks the salient once, in July, in great strength, and is held', () => {
    expect(count((r) => salient(r) && citadel(r).length === 1 && citadel(r)[0].turn === JULY_43 && !citadel(r)[0].taken)).toBeGreaterThanOrEqual(13);
    // It is never renewed, in July or after, and Kursk is not lost.
    for (const r of runs) {
      if (!salient(r)) continue;
      expect(citadel(r).length).toBeLessThanOrEqual(1);
      for (const t of [JULY_43, AUG_43, SEPT_43, OCT_43, NOV_43, DEC_43]) expect(r.held[t]['Kursk']).toBe('SOV');
    }
    expect(count((r) => citadel(r).some((f) => f.corps >= 10 && f.odds < 1.2 && f.attLost > 45000))).toBeGreaterThanOrEqual(13);
  });

  it('Kutuzov and Rumyantsev: Orel falls in July or August, and Kharkov by the end of September', () => {
    expect(count((r) => soviet(r, AUG_43, 'Orel'))).toBeGreaterThanOrEqual(18);
    expect(count((r) => soviet(r, AUG_43, 'Kharkov'))).toBeGreaterThanOrEqual(14);
    expect(count((r) => soviet(r, SEPT_43, 'Orel', 'Kharkov', 'Sumy'))).toBeGreaterThanOrEqual(18);
    // The Red Army takes them by assault: these are not places the Germans walked away from.
    expect(count((r) => r.fights.some((f) => f.by === 'SOV' && f.place === 'Orel' && f.taken) || r.fights.some((f) => f.by === 'SOV' && f.place === 'Kharkov' && f.taken))).toBeGreaterThanOrEqual(17);
  });

  it('September: Smolensk, Bryansk, Poltava and the Donbas are free, and the Germans are falling back to the Dnieper', () => {
    expect(count((r) => soviet(r, SEPT_43, 'Smolensk', 'Bryansk'))).toBeGreaterThanOrEqual(16);
    expect(count((r) => soviet(r, SEPT_43, 'Poltava', 'Stalino'))).toBeGreaterThanOrEqual(17);
    expect(count((r) => soviet(r, OCT_43, 'Novorossiysk', 'Chernigov'))).toBeGreaterThanOrEqual(16);
    // The line of the river is still German at the end of September.
    expect(count((r) => german(r, SEPT_43, 'Kiev', 'Zaporozhye'))).toBeGreaterThanOrEqual(14);
  });

  it('October: the Red Army is over the Dnieper at Dnepropetrovsk and Zaporozhye', () => {
    expect(count((r) => soviet(r, OCT_43, 'Dnepropetrovsk'))).toBeGreaterThanOrEqual(16);
    expect(count((r) => soviet(r, OCT_43, 'Zaporozhye'))).toBeGreaterThanOrEqual(13);
    expect(count((r) => soviet(r, DEC_43, 'Dnepropetrovsk', 'Zaporozhye'))).toBeGreaterThanOrEqual(16);
  });

  it('Kiev holds through October and is free before the year is out, with Zhitomir and Gomel beyond it', () => {
    expect(count((r) => german(r, OCT_43, 'Kiev'))).toBeGreaterThanOrEqual(15);
    expect(count((r) => soviet(r, DEC_43, 'Kiev'))).toBeGreaterThanOrEqual(15);
    expect(count((r) => soviet(r, DEC_43, 'Gomel'))).toBeGreaterThanOrEqual(15);
    expect(count((r) => soviet(r, DEC_43, 'Kiev', 'Zhitomir'))).toBeGreaterThanOrEqual(13);
  });

  it('the year ends with Germany still holding White Russia, the north, the Crimea and the Black Sea coast', () => {
    expect(count((r) => german(r, DEC_43, 'Vitebsk', 'Mogilev', 'Minsk'))).toBeGreaterThanOrEqual(16);
    expect(count((r) => german(r, DEC_43, 'Novgorod', 'Pskov'))).toBeGreaterThanOrEqual(17);
    expect(count((r) => german(r, DEC_43, 'Nikolaev', 'Odessa'))).toBeGreaterThanOrEqual(16);
    expect(count((r) => r.held[DEC_43]['Crimea'] !== 'SOV')).toBeGreaterThanOrEqual(15);
  });

  it('the second half of 1943 costs each side several hundred thousand men and Germany a third of its army in the east', () => {
    expect(count((r) => { const l = lost(r, JULY_43, DEC_43); return l.ger > 400000 && l.ger < 1100000; })).toBeGreaterThanOrEqual(16);
    expect(count((r) => { const l = lost(r, JULY_43, DEC_43); return l.sov > 330000 && l.sov < 1000000; })).toBeGreaterThanOrEqual(16);
    expect(count((r) => r.germans[DEC_43] < 0.82 * r.germans[JUNE_43])).toBeGreaterThanOrEqual(15);
    // The German army in the east is beaten back, not destroyed.
    expect(count((r) => r.germans[DEC_43] > 700000)).toBeGreaterThanOrEqual(17);
  });

  it('Leningrad, Moscow, Stalingrad and Voronezh are never lost', () => {
    for (const r of runs) for (const t of Object.keys(r.held)) for (const nm of ['Leningrad', 'Moscow', 'Stalingrad', 'Voronezh']) expect(r.held[+t][nm]).toBe('SOV');
  });
});
