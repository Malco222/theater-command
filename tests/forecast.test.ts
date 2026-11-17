import { describe, expect, it } from 'vitest';
import { AS_FORECAST_LOSS, asForecast, type Outlook } from '../src/campaign/turn';
import type { BattleReport } from '../src/campaign/types';

/** A forecast with a chance and what a win should cost the attacker. */
const look = (chance: number, ours: number | null, exact = true): Outlook => ({
  exact, odds: 2, lo: 2, hi: 2, chance, chanceLo: chance, chanceHi: chance, even: 1.2, unidentified: 0, stale: 0, note: '', wide: false, guess: chance, narrow: '',
  cost: { won: ours === null ? null : { ours, theirs: 1000, share: 0.2 }, lost: { ours: 3000, theirs: 400, share: 0.05 } }, lines: [], face: null,
});
/** A battle's report with a level and the attacker's losses. */
const rep = (taken: boolean, level: BattleReport['level'], attLost: number, corps: number[] = []): BattleReport => ({
  id: 1, turn: 1, week: 1, region: 0, regionName: 'Pomorze', attNat: 'GER', defNat: 'POL', attackers: corps.map((lost) => ({ lost })) as BattleReport['attackers'], defenders: [],
  lines: [], aPower: 2, dPower: 1, odds: 2, level, taken, attLost, defLost: 900, prisoners: 0,
});

describe('a battle that went as forecast (ruling 204)', () => {
  it('is a win the staff called, not Pyrrhic, at no more than half as much again as the card said', () => {
    expect(AS_FORECAST_LOSS).toBe(1.5);
    expect(asForecast(look(0.97, 2100), rep(true, 'Decisive victory', 2084))).toBe(true);
    expect(asForecast(look(0.62, 2100), rep(true, 'Close victory', 3150))).toBe(true);
    // The card said about 2,100 men: up to 3,150 is as forecast, one more is not.
    expect(asForecast(look(0.62, 2100), rep(true, 'Close victory', 3151))).toBe(false);
  });

  it('is never a defeat, a Pyrrhic victory, or a win the staff did not call', () => {
    expect(asForecast(look(0.97, 2100), rep(false, 'Close defeat', 900))).toBe(false);
    expect(asForecast(look(0.97, 2100), rep(true, 'Pyrrhic victory', 900))).toBe(false);
    // "It could go either way" and worse: the card opens however well it went.
    expect(asForecast(look(0.59, 2100), rep(true, 'Heroic victory', 100))).toBe(false);
    expect(asForecast(look(0.1, 2100), rep(true, 'Heroic victory', 100))).toBe(false);
    // No forecast was shown, or the forecast gave no figure for a win.
    expect(asForecast(null, rep(true, 'Decisive victory', 100))).toBe(false);
    expect(asForecast(look(0.97, null), rep(true, 'Decisive victory', 100))).toBe(false);
  });

  it('reads the verdict of a band as the card prints it', () => {
    // The defence is partly a guess: "The staff thinks the odds favour you" from 60 in 100 at the middle of the band.
    const band = { ...look(0.7, 2000, false), chanceLo: 0.4, chanceHi: 0.9 };
    expect(asForecast(band, rep(true, 'Decisive victory', 2500))).toBe(true);
    expect(asForecast({ ...band, chance: 0.5 }, rep(true, 'Decisive victory', 2500))).toBe(false);
  });

  it('counts the losses the report prints: the men lost in the fight, or the corps’ own losses where they are more', () => {
    expect(asForecast(look(0.97, 2000), rep(true, 'Decisive victory', 1000, [2000, 1200]))).toBe(false);
    expect(asForecast(look(0.97, 2000), rep(true, 'Decisive victory', 1000, [600, 400]))).toBe(true);
    // A place given up without a fight costs nothing and is as forecast.
    expect(asForecast(look(0.99, 0), { ...rep(true, 'Decisive victory', 0), yielded: true })).toBe(true);
  });
});
