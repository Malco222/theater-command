// Ruling 222 (the design director's log b362, 17 November 2026), Brian's ask in the project chat at 18:46Z: "maybe we
// change the british 60/40 that it depends on how they are doing with the war in europe". Britain's answer for a
// dominion is one seeded roll at the declaration, at the step her own war in Europe stands at that day, counting
// from the bottom the first that holds: 4, in danger at home, two in ten; 3, at war in Europe without France beside
// her, four; 2, at war there with France beside her, six; 1, no war with a great power of Europe, eight. The card
// names the odds and the reason (the UI director's words, words-ruling-211.md section 6). A roll already made stands.
import { describe, expect, it } from 'vitest';
import { warForecast } from '../src/campaign/diplomacy';
import { addCorps, atWar, capitulate, createGame, declareWar, dominionAnswers, patronPress, setWar } from '../src/campaign/game';
import { loadGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import { WORLD, answersOf } from '../src/campaign/world';
import brianSave from './ahead/saves/brian-usa-1940-12.json?raw';

/** The grand campaign on the wider world, played as America, at its first month (September 1939). */
function playing(human = 'USA', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** Of 100 seeded declarations by America on Canada this month, how many Britain answers. */
function answered(g: Game): number {
  let yes = 0;
  for (let s = 1; s <= 100; s++) { g.seed = s; if (dominionAnswers(g, 'UK', 'USA', 'CAN', g.turn)) yes++; }
  return yes;
}
const says = (g: Game): string => warForecast(g, 'USA', 'CAN').patronSays![1];

/** The four states of Britain's war in Europe, each made from the first month. */
const at = {
  2: (): Game => playing(),
  3: (): Game => { const g = playing(); capitulate(g, 'FRA', 'GER'); return g; },
  1: (): Game => { const g = playing(); capitulate(g, 'GER', 'UK'); return g; },
  4: (): Game => {
    // A German corps in the field round London (addCorps puts it inside the town, which no enemy holds).
    const g = playing(), L = g.byName.London, c = addCorps(g, 'GER', 'Test German infantry', 'inf', 2, L.id, {});
    L.garr = (L.garr ?? []).filter((x) => x !== c);
    delete c.town;
    L.units.push(c);
    return g;
  },
};

describe("Britain's odds follow her war in Europe (ruling 222)", () => {
  it('the four shares sit in the data beside the one share', () => {
    expect(WORLD.dominions!.UK).toMatchObject({ answers: 0.6, steps: [0.8, 0.6, 0.4, 0.2], europe: ['GER', 'ITA', 'FRA', 'SOV'], beside: 'FRA' });
    expect([1, 2, 3, 4].map((s) => answersOf('UK', s))).toEqual([0.8, 0.6, 0.4, 0.2]);
    expect(answersOf('UK')).toBe(0.6);
  });

  it('each step is reached, and of 100 seeded declarations about 80, 60, 40 and 20 answer', () => {
    const want = { 1: 80, 2: 60, 3: 40, 4: 20 } as const;
    for (const step of [1, 2, 3, 4] as const) {
      const g = at[step]();
      expect(patronPress(g, 'UK').step).toBe(step);
      expect(warForecast(g, 'USA', 'CAN').answers).toBe(answersOf('UK', step));
      const yes = answered(g);
      expect(yes).toBeGreaterThanOrEqual(want[step] - 12);
      expect(yes).toBeLessThanOrEqual(want[step] + 12);
    }
  });

  it('it reads both ways: a France freed and back in the war is beside her again, and France out of the war is not', () => {
    const g = at[3]();
    expect(patronPress(g, 'UK')).toMatchObject({ step: 3, fallen: true });
    const F = g.nations.FRA;
    F.alive = true;
    delete F.fellOn;
    F.atWar = [];
    expect(patronPress(g, 'UK')).toMatchObject({ step: 3, fallen: false });
    expect(says(g)).toBe('Four times in ten Britain declares war on you the same month, and six times in ten it stays out of this war: Britain is at war in Europe, and France stays out of it.');
    setWar(g, 'FRA', 'UK');
    expect(says(g)).toBe('Four times in ten Britain declares war on you the same month, and six times in ten it stays out of this war: Britain is at war in Europe, with France among its enemies.');
    g.nations.UK.atWar = g.nations.UK.atWar.filter((e) => e !== 'FRA');
    F.atWar = [];
    setWar(g, 'FRA', 'GER');
    expect(patronPress(g, 'UK').step).toBe(2);
  });

  it('her will to fight a third gone puts her in danger at home, as an enemy army on her ground does', () => {
    const g = playing();
    expect(patronPress(g, 'UK').step).toBe(2);
    // (The army alone weighs at most a fifth; a country that bears little is a third of the way there with it.)
    g.nations.UK.armyPeak = g.nations.UK.armyPeak * 40;
    expect(patronPress(g, 'UK').step).toBe(2);
    g.nations.UK.resilience = 0.5;
    expect(patronPress(g, 'UK').step).toBe(4);
  });

  it('the declare card names the odds as they stand today and the reason in one clause after them', () => {
    expect(says(at[1]())).toBe('Eight times in ten Britain declares war on you the same month, and twice in ten it stays out of this war: Britain has no war in Europe to hold it back.');
    expect(says(at[2]())).toBe('Six times in ten Britain declares war on you the same month, and four times in ten it stays out of this war: Britain is at war in Europe, with France still fighting beside it.');
    expect(says(at[3]())).toBe('Four times in ten Britain declares war on you the same month, and six times in ten it stays out of this war: France has fallen, and Britain fights on in Europe.');
    expect(says(at[4]())).toBe('Twice in ten Britain declares war on you the same month, and eight times in ten it stays out of this war: Britain must look to its own shores.');
    expect(warForecast(at[2](), 'USA', 'CAN').patronSays![0]).toBe('Britain may stand by it.');
  });

  it('the roll is the day of the declaration: one seed answers at six in ten and not at four', () => {
    let seed = 0;
    const g2 = at[2](), g3 = at[3]();
    for (let s = 1; s < 400 && !seed; s++) {
      g2.seed = s; g3.seed = s;
      if (dominionAnswers(g2, 'UK', 'USA', 'CAN', g2.turn) && !dominionAnswers(g3, 'UK', 'USA', 'CAN', g3.turn)) seed = s;
    }
    expect(seed).toBeGreaterThan(0);
    g2.seed = seed; g3.seed = seed;
    declareWar(g2, 'USA', 'CAN');
    declareWar(g3, 'USA', 'CAN');
    expect(atWar(g2, 'USA', 'UK')).toBe(true);
    expect(atWar(g3, 'USA', 'UK')).toBe(false);
  });

  it('a war made in an earlier month keeps the one share the rule had before: his own save, Britain still answers in January 1941', () => {
    const g = at[4]();
    let yes = 0;
    for (let s = 1; s <= 100; s++) { g.seed = s; if (dominionAnswers(g, 'UK', 'USA', 'CAN', g.turn - 1)) yes++; }
    expect(yes).toBeGreaterThanOrEqual(48);
    expect(yes).toBeLessThanOrEqual(72);
    const b = loadGame(brianSave)!;
    expect(atWar(b, 'USA', 'UK')).toBe(false);
    endHuman(b);
    for (let k = 0; !humanUp(b) && k < 400; k++) stepAI(b);
    beginHuman(b);
    expect(atWar(b, 'USA', 'UK')).toBe(true);
  });
});
