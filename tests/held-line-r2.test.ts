// The 'held the line' medal (release 2; Brian's yes of 27 November 2026 13:40Z): a corps that holds its ground when the
// attack had three chances in four or better of carrying it (the battle's own win chance) earns it once; it is felt at once, and its morale settles higher for the rest of
// the war. Every army alike; no die is thrown. With the switch off the war is the one it always was.
import { describe, expect, it } from 'vitest';
import { HELD, chanceWords, heldOn, heldRibbon, heldTheLine, heldWords, setHeld } from '../src/campaign/heldline';
import { MORALE, levelOf } from '../src/campaign/morale';
import { addCorps, createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { loadGame, saveGame } from '../src/campaign/save';
import type { BattleReport, Corps, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const war = (seed = 3, human: string | null = 'GER'): Game => startOpen(createGame(seed, human));
const report = (extra: Partial<BattleReport> = {}) =>
  ({ id: 1, turn: 10, week: 0, region: 5, regionName: 'Here', attNat: 'SOV', defNat: 'GER', attackers: [], defenders: [], lines: [], aPower: 3, dPower: 1, odds: 1, level: 'Close defeat', taken: false, ...extra }) as unknown as BattleReport;
const corps = (g: Game, nat: string, kind: Corps['kind'] = 'inf'): Corps => addCorps(g, nat, `${nat} ${kind} held test corps`, kind, 2, 5, {});

describe("the 'held the line' medal (release 2)", () => {
  it('is off unless switched on: no medal, no ribbon, and morale settles where it did', () => {
    const g = war();
    const c = corps(g, 'GER');
    expect(heldOn(g)).toBe(false);
    expect(heldTheLine(g, report(), [c], 0.9)).toEqual([]);
    expect(c.held).toBeUndefined();
    expect(heldRibbon(g, c)).toBeNull();
    expect(levelOf(c)).toBe(MORALE.level);
  });

  it('a defence that holds with one chance in four or less earns it, is felt at once, and settles higher for good', () => {
    const g = war();
    setHeld(g, true);
    const c = corps(g, 'GER'), d = corps(g, 'GER');
    c.mor = 0.5;
    const got = heldTheLine(g, report(), [c, d], 0.8);
    expect(got.map((x) => x.id)).toEqual([c.id, d.id]);
    expect(c.held).toEqual({ at: 5, turn: g.turn, chance: 0.2 });
    expect(c.mor).toBeCloseTo(0.5 + HELD.felt, 12);
    expect(c.mw?.some((e) => e[0] === 'heldline')).toBe(true);
    expect(levelOf(c)).toBeCloseTo(MORALE.level + HELD.lift, 12);
    expect(heldRibbon(g, c)).toEqual({ title: `Held the line: ${g.regions[5].name}`, ribbon: HELD.ribbon });
    // Once to a corps.
    expect(heldTheLine(g, report(), [c], 0.95)).toEqual([]);
    expect(c.held!.chance).toBe(0.2);
  });

  it('does not stack: the lift is one award, bounded at one lift, and the settle level never passes 1', () => {
    // (The design director's gate, 27 Nov 14:12Z, item 3: an unbounded lasting lift is a runaway.)
    const g = war();
    setHeld(g, true);
    const c = corps(g, 'GER');
    c.mor = 0.5;
    c.will = 0.98;
    for (let i = 0; i < 5; i++) heldTheLine(g, report(), [c], 0.9);
    expect(levelOf(c)).toBe(1);
    c.will = MORALE.level;
    expect(levelOf(c)).toBeCloseTo(MORALE.level + HELD.lift, 12);
    expect(c.mw?.filter((e) => e[0] === 'heldline').length).toBe(1);
  });

  it('will not go on outside the grand campaign, where nothing keeps morale', () => {
    const g = createGame(3, 'GER');
    expect(g.flags.open).not.toBe(1);
    expect(setHeld(g, true)).toBe(false);
    expect(heldOn(g)).toBe(false);
  });

  it('not with a better chance of holding, not where the ground was carried, not for a walk-in, a fort or a corps destroyed', () => {
    const g = war();
    setHeld(g, true);
    const c = corps(g, 'GER');
    expect(heldTheLine(g, report(), [c], HELD.chance - 0.01)).toEqual([]);
    expect(heldTheLine(g, report({ taken: true, level: 'Close victory' }), [c], 0.9)).toEqual([]);
    expect(heldTheLine(g, report({ yielded: true }), [c], 0.9)).toEqual([]);
    const fort = corps(g, 'GER', 'fort');
    const gone = corps(g, 'GER');
    gone.dead = true;
    expect(heldTheLine(g, report(), [fort, gone], 0.9)).toEqual([]);
    expect(c.held).toBeUndefined();
  });

  it("the player is told when his corps earn it; switched off, the medals are taken back", () => {
    const g = war(3, 'GER');
    setHeld(g, true);
    const c = corps(g, 'GER');
    const got = heldTheLine(g, report(), [c], 0.8);
    expect(heldWords(g, got, 'Kiev')).toBe(`${c.name} held the line at Kiev with one chance in five, and wears the ribbon for it. The men will not forget it.`);
    expect([chanceWords(0.25), chanceWords(0.1), chanceWords(0)]).toEqual(['one chance in four', 'one chance in ten', 'no chance at all']);
    const s = corps(g, 'SOV');
    expect(heldWords(g, heldTheLine(g, report(), [s], 0.9), 'Kiev')).toBe('');
    setHeld(g, false);
    expect(c.held).toBeUndefined();
    expect(levelOf(c)).toBe(MORALE.level);
  });

  it('a war played with it on gives it only for long odds held, keeps it through a save, and shows it on the card', () => {
    const g = war(17, null);
    setHeld(g, true);
    for (let m = 0; m < 14; m++) runTurnAI(g);
    const held = g.corps.filter((c) => c.held);
    expect(held.every((c) => c.held!.chance <= 1 - HELD.chance + 1e-9 && c.kind !== 'fort')).toBe(true);
    const back = loadGame(saveGame(g))!;
    expect(back.corps.filter((c) => c.held).map((c) => [c.id, c.held])).toEqual(held.map((c) => [c.id, c.held]));
  });

  it('the 3D ship list turns it on once, and a switch the player turns off stays off', () => {
    TC().start({ view: 'grand', human: 'GER', seed: 11 });
    const read = (): Game => loadGame((TC().save() as { text: string }).text)!;
    expect(heldOn(read())).toBe(false);
    expect(TC().r2Ship(['heldlinerule'])).not.toBeNull();
    expect(heldOn(read())).toBe(true);
    TC().act('heldlinerule:off');
    expect(TC().r2Ship(['heldlinerule'])).toBeNull();
    expect(heldOn(read())).toBe(false);
  });
});
