// AHEAD OF THE RULES: Version 84.2.5, Britain answers for a dominion six times in ten (ruling 211(b) as Brian changed
// it, project chat 2026-11-17 16:49Z: "i think it should be 60/40 if brittain declares war on you. they have enough
// going on in europe"), on HIS OWN SAVE. The rules builder's own tests cover the roll on new wars (guns-8425.test.ts);
// this is the case the design director asked for at 17:22Z: a campaign saved before the rule, with such a war
// running, rolls once at the first month's end after loading, with a headline either way, and the same month loaded
// twice gives the same answer. His save is the United States against Canada since October 1939, December 1940.
// Words: /mnt/project-files/ui-director/words-ruling-211.md, section 6 (d) and (e), passed by the design director.
//
// The rules builder's names (17:16Z): dominionAnswers(g, p, a, b, war) in game.ts is the roll; the headline carries a
// new field `head` ("Britain stands by Canada", kind 'war'; "Britain stays out of Canada's war", kind 'info').
// Every test here waits, skipped, until game.ts exports dominionAnswers.
import { describe, expect, it } from 'vitest';
import * as game from '../../src/campaign/game';
import { atWar } from '../../src/campaign/game';
import { loadGame } from '../../src/campaign/save';
import { beginHuman, endHuman, humanUp, stepAI } from '../../src/campaign/turn';
import type { Game } from '../../src/campaign/types';
import brianSave from './saves/brian-usa-1940-12.json?raw';

const waiting = typeof (game as unknown as Record<string, unknown>).dominionAnswers !== 'function';

const STANDS = 'Britain stands by Canada';
const STAYS = "Britain stays out of Canada's war";
const STANDS_TEXT = 'The United States has been at war with Canada since October 1939. Britain now declares war on the United States: it answers for its dominion.';
const STAYS_TEXT = 'The United States has been at war with Canada since October 1939. Britain does not declare war on the United States: Canada fights this war without it.';

function brian(): Game {
  const g = loadGame(brianSave);
  if (!g) throw new Error('the save did not load');
  return g;
}
function nextMonth(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
}
const head = (h: object): string | undefined => (h as { head?: string }).head;
/** Britain's answer for Canada in the news, either way. */
const answers = (g: Game) => g.headlines.filter((h) => head(h) === STANDS || head(h) === STAYS);

describe.skipIf(waiting)("84.2.5 Britain 60/40 (ruling 211(b), Brian's change): his own save rolls once", () => {
  it('as loaded: at war with Canada since October 1939, Britain at peace with him, and nothing said yet', () => {
    const g = brian();
    expect(humanUp(g)).toBe(true);
    expect(atWar(g, 'USA', 'CAN')).toBe(true);
    expect(atWar(g, 'USA', 'UK')).toBe(false);
    expect(answers(g)).toEqual([]);
  });

  it('at the first month\'s end after loading Britain answers or stays out, with its headline, and the same month loaded twice agrees', () => {
    const a = brian(), b = brian();
    nextMonth(a);
    nextMonth(b);
    expect(a.turn).toBe(17);
    const said = answers(a);
    expect(said.length).toBe(1);
    const came = atWar(a, 'USA', 'UK');
    if (came) {
      expect(head(said[0])).toBe(STANDS);
      expect(said[0].text).toBe(STANDS_TEXT);
      expect(said[0].kind).toBe('war');
    } else {
      expect(head(said[0])).toBe(STAYS);
      expect(said[0].text).toBe(STAYS_TEXT);
      // Staying out stands in the news band, never the red one.
      expect(said[0].kind).not.toBe('war');
    }
    // Nothing promises: the old certain words are gone, and staying out never names Europe or another war.
    for (const h of a.headlines) {
      expect(h.text).not.toMatch(/war on a dominion is war with Britain/);
      if (head(h) === STAYS) expect(h.text).not.toMatch(/Europe|another war/);
    }
    // Loaded again, the same month gives the same answer.
    expect(atWar(b, 'USA', 'UK')).toBe(came);
    expect(answers(b).map((h) => [head(h), h.text])).toEqual(said.map((h) => [head(h), h.text]));
  }, 60_000);

  it('no second roll: the month after, nothing more is said and Britain stands where she stood', () => {
    const g = brian();
    nextMonth(g);
    const came = atWar(g, 'USA', 'UK');
    nextMonth(g);
    nextMonth(g);
    expect(answers(g).length).toBe(1);
    // She may have come in by some other road in two months of war; by this rule she has not changed her mind.
    if (came) expect(atWar(g, 'USA', 'UK')).toBe(true);
  }, 60_000);
});
