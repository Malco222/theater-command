// The framework's Phase 3 war in the east, second year. With every nation on the historical plan and nobody
// at the controls, 1942 should look like 1942: Germany gathers its tanks in the south and drives for the Don,
// the Volga and the Caucasus oil; Stalingrad is assaulted and holds; the Don flanks are left to the Hungarian,
// Italian and Romanian armies; in November the Red Army breaks those flanks and cuts off the army on the
// Volga, which is destroyed in the winter; the Soviet drive carries on to Kursk and Kharkov, and in March
// the German counter-stroke takes Kharkov back and the front settles for the spring thaw.
//
// The plan only sets intentions. Every region still has to be won under the rules the player
// uses, so the bands below are loose on purpose and are checked across twenty sets of dice.
import { describe, expect, it } from 'vitest';
import { atWar, createGame, men } from '../../src/campaign/game';
import { runTurnAI } from '../../src/campaign/turn';
import { KIND, type BattleReport, type Game } from '../../src/campaign/types';

const FEB_42 = 30, MAY_42 = 33, JUNE_42 = 34, JULY_42 = 35, AUG_42 = 36, SEPT_42 = 37, OCT_42 = 38, NOV_42 = 39, DEC_42 = 40, FEB_43 = 42, MARCH_43 = 43, APRIL_43 = 44;
const PLACES = [
  'Voronezh', 'Millerovo', 'Rostov', 'Don Bend', 'Kalach', 'Stalingrad', 'Kalmyk Steppe', 'Salsk', 'Kuban', 'Stavropol', 'Maikop', 'Novorossiysk',
  'Grozny', 'Astrakhan', 'Kharkov', 'Kursk', 'Stalino', 'Orel', 'Bryansk', 'Moscow', 'Leningrad',
];
const SOUTH = ['Kursk', 'Kharkov', 'Stalino'];
/** The Caucasus beyond the Kuban coast: the ground the German plan gives up in the winter. */
const INLAND = ['Terek', 'Stavropol', 'Salsk', 'Maikop'];

interface Run {
  g: Game;
  /** Whether each place was out of Soviet hands at the end of each month. */
  axis: Record<number, Record<string, boolean>>;
  /** Battles fought on Soviet ground from May 1942. */
  fights: BattleReport[];
  /** German tank and lorried corps standing in the three southern assembly areas at the end of each month. */
  fast: Record<number, number>;
  /** Corps on four stretches of the Don front at the end of October, by nation. */
  flanks: Record<string, Record<string, number>>;
  /** The German corps cut off on the land bridge when the ring closed, and their men. */
  pocket: { corps: number[]; men: number; turn: number };
  /** How many of those corps were still on the map at the end of February. */
  escaped: number;
  /** German corps lost in the Caucasus between December and March, beyond the coast. */
  caucasus: number;
  /** Italian and Hungarian corps left on Soviet ground at the end of May 1943. */
  guests: number;
  italy: boolean;
  /** What the newsreel said of the land bridge: the ring closing, and the end. */
  news: { ring: number; end: number };
}

function history(seed: number): Run {
  const g = createGame(seed, null);
  const run: Run = { g, axis: {}, fights: [], fast: {}, flanks: {}, pocket: { corps: [], men: 0, turn: 0 }, escaped: 0, caucasus: 0, guests: 0, italy: false, news: { ring: 0, end: 0 } };
  let seen = 0;
  for (let t = 1; t <= APRIL_43 + 1; t++) {
    const stood = new Map(g.corps.filter((c) => c.nat === 'GER').map((c) => [c.id, g.regions[c.reg].name]));
    runTurnAI(g);
    if (t >= DEC_42 && t <= MARCH_43) for (const [id, place] of stood) if (INLAND.includes(place) && !g.corps.some((c) => c.id === id)) run.caucasus++;
    const reps = g.reports.slice(seen);
    seen = g.reports.length;
    if (t < FEB_42) continue;
    run.axis[t] = Object.fromEntries(PLACES.map((n) => [n, g.byName[n].ctrl !== 'SOV']));
    run.fights.push(...reps.filter((r) => t >= MAY_42 && g.regions[r.region].owner === 'SOV'));
    run.fast[t] = SOUTH.reduce((s, n) => s + g.byName[n].units.filter((u) => u.nat === 'GER' && KIND[u.kind].mobile).length, 0);
    if (t === OCT_42) {
      for (const n of ['Voronezh', 'Millerovo', 'Don Bend', 'Kalmyk Steppe']) {
        run.flanks[n] = {};
        for (const u of g.byName[n].units) run.flanks[n][u.nat] = (run.flanks[n][u.nat] ?? 0) + 1;
      }
      run.italy = atWar(g, 'ITA', 'SOV');
    }
    const cut = g.byName['Kalach'].units.filter((u) => u.nat === 'GER' && !u.supplied);
    if (cut.length && !run.pocket.turn) run.pocket = { corps: cut.map((u) => u.id), men: cut.reduce((s, u) => s + men(u), 0), turn: t };
    if (t === FEB_43) run.escaped = run.pocket.corps.filter((id) => g.corps.some((c) => c.id === id)).length;
    if (t === APRIL_43 + 1) {
      const told = g.headlines.filter((h) => h.region === g.byName['Kalach'].id && h.nat === 'GER');
      run.news = { ring: told.find((h) => /^The ring closes/.test(h.text))?.turn ?? 0, end: told.find((h) => /lays? down (its|their) arms|troops surrender at/.test(h.text))?.turn ?? 0 };
    }
    if (t === APRIL_43 + 1) run.guests = g.corps.filter((c) => (c.nat === 'ITA' || c.nat === 'HUN') && g.regions[c.reg].owner === 'SOV').length;
  }
  return run;
}

const SEEDS = Array.from({ length: 20 }, (_, i) => i + 1);
let cache: Run[] | null = null;
const all = (): Run[] => (cache ??= SEEDS.map(history));
const count = (ok: (r: Run) => boolean): number => all().filter(ok).length;
const by = (r: Run, place: string, t: number): boolean => r.axis[t][place];
/** Men lost by a side in the battles of a stretch of months: its attackers and its defenders both. */
const lost = (r: Run, side: (nat: string) => boolean, from: number, to: number): number =>
  r.fights.filter((f) => f.turn >= from && f.turn <= to).reduce((s, f) => s + (side(f.attNat) ? f.attLost : 0) + (side(f.defNat) ? f.defLost : 0), 0);
const axisSide = (nat: string): boolean => ['GER', 'ROM', 'ITA', 'HUN'].includes(nat);
const soviet = (nat: string): boolean => nat === 'SOV';

describe('exit test: the eastern front from Case Blue to the spring of 1943 on the historical plan', () => {
  it('Germany gathers its tanks in the south in the spring of 1942', { timeout: 600000 }, () => {
    // In February they are scattered along the whole front; by May five corps or more stand between Kursk and the Donets.
    expect(count((r) => r.fast[FEB_42] <= 2)).toBeGreaterThanOrEqual(18);
    expect(count((r) => r.fast[MAY_42] >= 5)).toBeGreaterThanOrEqual(16);
  });

  it('the Soviet offensive at Kharkov in May fails, at a heavy price', () => {
    const tried = (r: Run): BattleReport[] => r.fights.filter((f) => f.turn === MAY_42 && f.attNat === 'SOV' && f.regionName === 'Kharkov');
    expect(count((r) => tried(r).length > 0 && tried(r).every((f) => !f.taken))).toBeGreaterThanOrEqual(16);
    expect(count((r) => by(r, 'Kharkov', MAY_42))).toBeGreaterThanOrEqual(18);
    for (const r of all()) for (const f of tried(r)) expect(f.attLost).toBeGreaterThan(2 * f.defLost);
  });

  it('Case Blue takes Voronezh, then Rostov and the bend of the Don', () => {
    expect(count((r) => by(r, 'Voronezh', JULY_42))).toBeGreaterThanOrEqual(18);
    expect(count((r) => by(r, 'Rostov', AUG_42) && by(r, 'Don Bend', AUG_42))).toBeGreaterThanOrEqual(17);
    // The summer costs the Red Army far more than it costs the invader.
    expect(count((r) => lost(r, soviet, JUNE_42, OCT_42) > 2 * lost(r, axisSide, JUNE_42, OCT_42))).toBeGreaterThanOrEqual(18);
  });

  it('the Volga is reached in August or September, and the Caucasus oilfields in the autumn', () => {
    expect(count((r) => by(r, 'Kalach', AUG_42))).toBeGreaterThanOrEqual(14);
    expect(count((r) => by(r, 'Kalach', SEPT_42))).toBeGreaterThanOrEqual(18);
    expect(count((r) => by(r, 'Stavropol', OCT_42) && by(r, 'Kuban', OCT_42))).toBeGreaterThanOrEqual(16);
    expect(count((r) => by(r, 'Maikop', NOV_42))).toBeGreaterThanOrEqual(16);
    // The oil of Grozny and the mouth of the Volga are never reached.
    for (const r of all()) for (let t = MAY_42; t <= APRIL_43; t++) { expect(by(r, 'Grozny', t)).toBe(false); expect(by(r, 'Astrakhan', t)).toBe(false); }
  });

  it('Stalingrad is assaulted and holds', () => {
    const assaults = (r: Run): BattleReport[] => r.fights.filter((f) => f.attNat === 'GER' && f.regionName === 'Stalingrad');
    expect(count((r) => { for (let t = MAY_42; t <= APRIL_43; t++) if (by(r, 'Stalingrad', t)) return false; return true; })).toBeGreaterThanOrEqual(18);
    expect(count((r) => assaults(r).length > 0)).toBeGreaterThanOrEqual(10);
    // The assaults go in between September and November, against the odds, and cost the attacker more than the city's defenders.
    for (const r of all()) for (const f of assaults(r)) {
      expect(f.turn).toBeGreaterThanOrEqual(SEPT_42);
      expect(f.turn).toBeLessThanOrEqual(NOV_42);
    }
    expect(count((r) => assaults(r).every((f) => f.attLost > f.defLost))).toBeGreaterThanOrEqual(18);
  });

  it('the Don flanks are handed to the Hungarian, Italian and Romanian armies', () => {
    for (const r of all()) expect(r.italy).toBe(true);
    expect(count((r) => (r.flanks['Voronezh'].HUN ?? 0) >= 2)).toBeGreaterThanOrEqual(17);
    expect(count((r) => (r.flanks['Millerovo'].ITA ?? 0) >= 3)).toBeGreaterThanOrEqual(17);
    expect(count((r) => (r.flanks['Don Bend'].ROM ?? 0) >= 2)).toBeGreaterThanOrEqual(17);
    expect(count((r) => (r.flanks['Kalmyk Steppe'].ROM ?? 0) >= 1)).toBeGreaterThanOrEqual(14);
    // Germany takes its own troops out of those stretches, all but a corps to stiffen each.
    expect(count((r) => (r.flanks['Don Bend'].GER ?? 0) <= 1 && (r.flanks['Millerovo'].GER ?? 0) <= 1)).toBeGreaterThanOrEqual(16);
  });

  it('the Red Army stands on the defensive all summer and saves its strength', () => {
    for (const r of all()) expect(r.fights.filter((f) => f.attNat === 'SOV' && f.turn >= JUNE_42 && f.turn <= OCT_42)).toEqual([]);
  });

  it('in November it breaks both flanks and cuts off the army on the Volga', () => {
    expect(count((r) => !by(r, 'Don Bend', NOV_42) && !by(r, 'Kalmyk Steppe', DEC_42))).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.pocket.turn === NOV_42 || r.pocket.turn === DEC_42)).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.pocket.corps.length >= 3 && r.pocket.men >= 90000)).toBeGreaterThanOrEqual(16);
  });

  it('and that army is destroyed in the winter: nobody gets out', () => {
    expect(count((r) => r.pocket.turn > 0 && !by(r, 'Kalach', FEB_43))).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.pocket.turn > 0 && r.escaped === 0)).toBeGreaterThanOrEqual(16);
    // The newsreel tells it: the ring closing in November or December, the end by February.
    expect(count((r) => r.news.ring === r.pocket.turn && r.pocket.turn > 0)).toBeGreaterThanOrEqual(16);
    expect(count((r) => r.news.end > r.news.ring && r.news.ring > 0 && r.news.end <= FEB_43)).toBeGreaterThanOrEqual(16);
  });

  it('the winter offensive carries the Red Army back to Voronezh, Rostov and Kursk', () => {
    expect(count((r) => !by(r, 'Voronezh', FEB_43))).toBeGreaterThanOrEqual(18);
    expect(count((r) => !by(r, 'Rostov', FEB_43))).toBeGreaterThanOrEqual(15);
    expect(count((r) => !by(r, 'Kursk', FEB_43))).toBeGreaterThanOrEqual(16);
    expect(count((r) => !by(r, 'Stavropol', FEB_43) && !by(r, 'Salsk', FEB_43))).toBeGreaterThanOrEqual(16);
    // The winter costs the Axis the best part of half a million men.
    expect(count((r) => lost(r, axisSide, NOV_42, MARCH_43) > 300000)).toBeGreaterThanOrEqual(17);
  });

  it('the army in the Caucasus is not lost with it: it falls back to the Kuban in time', () => {
    expect(count((r) => r.caucasus <= 2)).toBeGreaterThanOrEqual(17);
    expect(count((r) => by(r, 'Stavropol', NOV_42) && !by(r, 'Stavropol', FEB_43))).toBeGreaterThanOrEqual(14);
  });

  it('the German counter-stroke in March holds or retakes Kharkov, and the front settles for the thaw', () => {
    expect(count((r) => by(r, 'Kharkov', MARCH_43))).toBeGreaterThanOrEqual(17);
    // The old line holds at both ends: where Germany stood at Orel and in the Donets basin a year ago, it stands there still.
    expect(count((r) => ['Orel', 'Stalino'].every((n) => !by(r, n, MAY_42) || by(r, n, APRIL_43)))).toBeGreaterThanOrEqual(17);
    expect(count((r) => by(r, 'Stalino', APRIL_43))).toBeGreaterThanOrEqual(15);
    // Kursk stays Soviet: the bulge in the line that the summer of 1943 will be fought over.
    expect(count((r) => !by(r, 'Kursk', APRIL_43))).toBeGreaterThanOrEqual(15);
    // A bridgehead is kept on the Kuban coast, fed from the Crimea.
    expect(count((r) => by(r, 'Novorossiysk', APRIL_43))).toBeGreaterThanOrEqual(13);
    for (const r of all()) {
      expect(by(r, 'Moscow', APRIL_43)).toBe(false);
      expect(by(r, 'Leningrad', APRIL_43)).toBe(false);
    }
    // April is quiet: the roads have gone, and neither plan orders anything.
    expect(count((r) => r.fights.filter((f) => f.turn === APRIL_43).length <= 1)).toBeGreaterThanOrEqual(16);
  });

  it('what is left of the Italian and Hungarian armies goes home in the spring', () => {
    for (const r of all()) expect(r.guests).toBe(0);
  });
});
