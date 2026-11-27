// Captured tanks pressed into service (release 2; Brian's yes of 27 November 2026 05:23Z): after a land battle the
// beaten side's tanks knocked out on the field the winner holds, a quarter of them repairable, join the winner's own
// armoured and motorised corps that fought, up to the strength each is short of, each worth half one of its own; they
// wear out for want of spare parts (a tenth a month, twice that out of supply); the same for every army; the battle's
// look follows them. No die is thrown. With the switch off the war is the one it always was.
import { describe, expect, it } from 'vitest';
import { PRESSED, kitLine, kitLook, pressTanks, pressedMonth, pressedOn, pressedWords, setPressed } from '../src/campaign/pressed';
import { addCorps, createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { loadGame, saveGame } from '../src/campaign/save';
import type { BattleReport, Corps, Game, ReportUnit } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const war = (seed = 3, human: string | null = 'GER'): Game => startOpen(createGame(seed, human));
const unit = (nat: string, kind: ReportUnit['kind'], divs: number, men: number, lost: number): ReportUnit => ({ name: `${nat} ${kind}`, nat, kind, divs, men, lost, fate: '' });
const report = (level: BattleReport['level'], attackers: ReportUnit[], defenders: ReportUnit[], extra: Partial<BattleReport> = {}) =>
  ({ id: 1, turn: 10, week: 0, region: 5, regionName: 'Here', attNat: attackers[0].nat, defNat: defenders[0].nat, attackers, defenders, lines: [], aPower: 1, dPower: 1, odds: 1, level, ...extra }) as unknown as BattleReport;
/** A corps of the war's own, made for the test, at a strength. */
function corps(g: Game, nat: string, kind: Corps['kind'], divs: number, str: number): Corps {
  const c = addCorps(g, nat, `${nat} ${kind} test corps`, kind, divs, 5, {});
  c.str = str;
  return c;
}

describe('captured tanks pressed into service (release 2)', () => {
  it('is off unless switched on: nothing pressed, nothing written', () => {
    const g = war();
    expect(pressedOn(g)).toBe(false);
    const ger = corps(g, 'GER', 'arm', 2, 0.8);
    const rep = report('Decisive victory', [unit('GER', 'arm', 2, 30000, 3000)], [unit('SOV', 'arm', 1, 12000, 3600)]);
    expect(pressTanks(g, rep, [ger], [])).toBeNull();
    expect(ger.str).toBe(0.8);
    expect(ger.kit).toBeUndefined();
    expect(kitLook(g, [ger])).toBeNull();
    expect(kitLine(g, ger)).toBe('');
  });

  it("the beaten side's repairable tanks join the winner's armour that fought, by the gap each has, at half worth", () => {
    const g = war();
    setPressed(g, true);
    const arm = corps(g, 'GER', 'arm', 2, 0.8), mot = corps(g, 'GER', 'mot', 1, 0.9), inf = corps(g, 'GER', 'inf', 3, 0.7);
    // SOV loses 30% of an armoured division (150 x 0.3 = 45 tanks) and of a motorised one (40 x 0.3 = 12): 57 left,
    // a quarter of them repairable.
    const rep = report('Decisive victory', [unit('GER', 'arm', 2, 30000, 3000)], [unit('SOV', 'arm', 1, 12000, 3600), unit('SOV', 'mot', 1, 10000, 3000)]);
    const p = pressTanks(g, rep, [arm, mot, inf], [])!;
    const fixable = 57 * PRESSED.repair;
    expect(p.from).toBe('SOV');
    expect(p.n).toBeCloseTo(fixable, 9);
    // Gaps: armour 0.2 x 2 x 150 = 60 vehicles, motorised 0.1 x 1 x 40 = 4; infantry takes none.
    const toArm = (fixable * 60) / 64, toMot = (fixable * 4) / 64;
    expect(p.took).toEqual([{ id: arm.id, n: toArm }, { id: mot.id, n: toMot }]);
    expect(arm.str).toBeCloseTo(0.8 + (toArm * PRESSED.worth) / 300, 12);
    expect(mot.str).toBeCloseTo(0.9 + (toMot * PRESSED.worth) / 40, 12);
    expect(inf.str).toBe(0.7);
    expect(arm.kit).toEqual({ from: 'SOV', n: toArm, turn: g.turn });
    expect(kitLine(g, arm)).toBe(`${Math.round(toArm)} captured Soviet tanks in its own colours.`);
  });

  it('a defence that holds takes the attackers\' tanks; a walk-in, a winner with no armour, or too few take none', () => {
    const g = war();
    setPressed(g, true);
    const sov = corps(g, 'SOV', 'arm', 1, 0.6);
    const lost = report('Decisive defeat', [unit('GER', 'arm', 2, 30000, 9000)], [unit('SOV', 'arm', 1, 12000, 2000)]);
    expect(pressTanks(g, lost, [], [sov])?.from).toBe('GER');
    expect(sov.kit?.from).toBe('GER');
    const g2 = war();
    setPressed(g2, true);
    const inf = corps(g2, 'GER', 'inf', 3, 0.5);
    expect(pressTanks(g2, report('Decisive victory', [unit('GER', 'inf', 3, 40000, 4000)], [unit('SOV', 'arm', 1, 12000, 6000)]), [inf], [])).toBeNull();
    const arm = corps(g2, 'GER', 'arm', 1, 0.5);
    expect(pressTanks(g2, report('Decisive victory', [unit('GER', 'arm', 1, 15000, 1000)], [unit('SOV', 'arm', 1, 12000, 6000)], { yielded: true }), [arm], [])).toBeNull();
    // 40 x 0.05 = 2 motorised vehicles left, half a vehicle repairable: under the least worth booking.
    expect(pressTanks(g2, report('Close victory', [unit('GER', 'arm', 1, 15000, 1000)], [unit('SOV', 'mot', 1, 10000, 500)]), [arm], [])).toBeNull();
    // A winner already at full strength has no gap to fill.
    const full = corps(g2, 'GER', 'arm', 1, 1);
    expect(pressTanks(g2, report('Decisive victory', [unit('GER', 'arm', 1, 15000, 100)], [unit('SOV', 'arm', 1, 12000, 6000)]), [full], [])).toBeNull();
    expect(arm.str).toBe(0.5);
  });

  it('they wear out for want of parts, faster out of supply, and the corps is the weaker for each that breaks down', () => {
    const g = war();
    setPressed(g, true);
    const a = corps(g, 'GER', 'arm', 2, 0.8), b = corps(g, 'GER', 'arm', 2, 0.8);
    a.kit = { from: 'SOV', n: 20, turn: g.turn };
    b.kit = { from: 'SOV', n: 20, turn: g.turn };
    b.supplied = false;
    pressedMonth(g, 'GER');
    expect(a.kit.n).toBeCloseTo(20 * (1 - PRESSED.wear), 12);
    expect(a.str).toBeCloseTo(0.8 - (20 * PRESSED.wear * PRESSED.worth) / 300, 12);
    expect(b.kit!.n).toBeCloseTo(20 * (1 - PRESSED.wear * PRESSED.cut), 12);
    // Another nation's month leaves them be; the last of them gone, the corps carries none.
    pressedMonth(g, 'SOV');
    expect(a.kit.n).toBeCloseTo(18, 12);
    a.kit.n = 1.05;
    pressedMonth(g, 'GER');
    expect(a.kit).toBeUndefined();
  });

  it('the battle draws a few tank blocks as the captured make, by their share of the side\'s armour', () => {
    const g = war();
    setPressed(g, true);
    const a = corps(g, 'GER', 'arm', 2, 1);
    expect(kitLook(g, [a])).toBeNull();
    a.kit = { from: 'SOV', n: 12, turn: g.turn };
    expect(kitLook(g, [a])).toEqual({ from: 'SOV', n: 1 });
    a.kit.n = 45;
    expect(kitLook(g, [a])).toEqual({ from: 'SOV', n: 2 });
    a.kit.n = 200;
    expect(kitLook(g, [a])).toEqual({ from: 'SOV', n: PRESSED.blocks });
  });

  it("the player is told when his corps take tanks, and when the enemy takes his; switched off, the books are cleared", () => {
    const g = war(3, 'GER');
    setPressed(g, true);
    const arm = corps(g, 'GER', 'arm', 2, 0.8);
    const p = pressTanks(g, report('Decisive victory', [unit('GER', 'arm', 2, 30000, 3000)], [unit('SOV', 'arm', 1, 12000, 3600)]), [arm], [])!;
    expect(pressedWords(g, p, 'Kiev')).toBe(`11 Soviet tanks knocked out at Kiev are got running again and join ${arm.name}, in our colours. Without spare parts they will not last for ever.`);
    const g2 = war(3, 'SOV');
    setPressed(g2, true);
    const arm2 = corps(g2, 'GER', 'arm', 2, 0.8);
    const p2 = pressTanks(g2, report('Decisive victory', [unit('GER', 'arm', 2, 30000, 3000)], [unit('SOV', 'arm', 1, 12000, 3600)]), [arm2], [])!;
    expect(pressedWords(g2, p2, 'Kiev')).toBe('The enemy has got 11 of our tanks left on the field at Kiev running again, and drives them in his own colours.');
    expect(pressedWords(war(3, 'USA'), p2, 'Kiev')).toBe('');
    setPressed(g, false);
    expect(pressedOn(g)).toBe(false);
    expect(arm.kit).toBeUndefined();
  });

  it('a war played with it on presses tanks, keeps them through a save, and reads back the same', () => {
    const g = war(17, null);
    setPressed(g, true);
    for (let m = 0; m < 14; m++) runTurnAI(g);
    const withKit = g.corps.filter((c) => c.kit);
    expect(withKit.length).toBeGreaterThan(0);
    expect(withKit.every((c) => c.kit!.n >= 1 && c.kit!.from !== c.nat && (c.kind === 'arm' || c.kind === 'mot'))).toBe(true);
    const back = loadGame(saveGame(g))!;
    expect(back.corps.filter((c) => c.kit).map((c) => [c.id, c.kit])).toEqual(withKit.map((c) => [c.id, c.kit]));
  });

  it('the 3D ship list turns it on once, and a switch the player turns off stays off', () => {
    TC().start({ view: 'grand', human: 'GER', seed: 11 });
    const read = (): Game => loadGame((TC().save() as { text: string }).text)!;
    expect(pressedOn(read())).toBe(false);
    expect(TC().r2Ship(['pressedtanksrule'])).not.toBeNull();
    expect(pressedOn(read())).toBe(true);
    TC().act('pressedtanksrule:off');
    expect(TC().r2Ship(['pressedtanksrule'])).toBeNull();
    expect(pressedOn(read())).toBe(false);
  });
});
