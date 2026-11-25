// FIRST COMMAND, the first hour (the design director's ruling 129 and first-hour-brief.md; the words as cut by ruling
// 147; the door by ruling 141). One list for both games (data/campaign/lessons.json): the ten lessons in order,
// their words, the deed that earns each stamp, and when each comes due. These check the list itself, and the rules
// that read it against a real FIRST COMMAND campaign: Germany, September 1939, Cadet, Field command.
import { describe, expect, it } from 'vitest';
import { setDepth } from '../src/campaign/desks';
import { setLevel } from '../src/campaign/difficulty';
import { createGame, placeName } from '../src/campaign/game';
import {
  CLOSING, FIRST_COMMAND, LESSONS, LESSON_DOOR, LESSON_GROUPS, LESSON_NAMES, TIPS, WORDS_MOST, WORDS_MOST_RAID, allDone, bringBack, closingOwed,
  currentLesson, doneCount, firstCommand, freshNotebook, isDone, lessonDeed, lessonDue, lessonReady, lessonWords, lessonsAgain, lessonsOn,
  namedWords, pickupPlace, placeThe, putAway, readNotebook, showAgain, waitingCount, wordCount, type Notebook,
} from '../src/campaign/lessons';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import { townName } from '../src/campaign/towns';
import type { Game, Region } from '../src/campaign/types';

/** FIRST COMMAND as the door opens it, at the player's first month. */
function firstHour(seed = 5): Game {
  const g = startOpen(createGame(seed, FIRST_COMMAND.nat));
  setLevel(g, FIRST_COMMAND.level);
  setDepth(g, FIRST_COMMAND.depth);
  g.flags.firstCommand = 1;
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
}
const book = (done: string[] = []): Notebook => ({ ...freshNotebook(), done: [...done] });
const ids = LESSONS.map((l) => l.id);
const upTo = (id: string): string[] => ids.slice(0, ids.indexOf(id));

describe('the list', () => {
  it('holds ten lessons in four months, a closing note and FIRST COMMAND\'s campaign', () => {
    expect(ids).toEqual(['month', 'pickup', 'march', 'attack', 'end', 'card', 'city', 'rail', 'raid', 'build']);
    expect(LESSONS.map((l) => l.n)).toEqual([1, 2, 3, 4, 5, 6, 7, 8, 9, 10]);
    expect(LESSON_GROUPS.map((x) => x.from)).toEqual([0, 6, 8, 9]);
    expect(CLOSING.head).toBe('The war is yours');
    expect(FIRST_COMMAND).toEqual({ nat: 'GER', from: 1, level: 'cadet', depth: 'field' });
    expect(LESSON_DOOR).toEqual({ label: 'First command', fresh: 'New here? Learn as you play.', again: 'The ten lessons again.' });
  });

  it('keeps every note to 30 words, the raid to 36, in each game\'s own words', () => {
    for (const game of ['shared', 'browser', 'unreal']) {
      for (const l of LESSONS) {
        for (const text of [l.text, ...(l.there ? [l.there] : [])]) {
          const n = wordCount(namedWords(text, game, game === 'browser' ? ' under STAFF' : ''));
          expect(n, `${l.id} in ${game}`).toBeLessThanOrEqual(l.id === 'raid' ? WORDS_MOST_RAID : WORDS_MOST);
        }
      }
      expect(wordCount(namedWords(CLOSING.text, game))).toBeLessThanOrEqual(WORDS_MOST);
    }
  });

  it('gives every lesson but the first one deed line, and no note any red word', () => {
    for (const l of LESSONS) {
      if (l.id === 'month') expect(l.deed).toBe('');
      else expect(l.deed.length).toBeGreaterThan(0);
      expect(/\.\.\.|…/.test(l.text)).toBe(false);
    }
    for (const t of TIPS) expect(['kept', 'cut']).toContain(t.id);
  });

  it('swaps in the browser game\'s own words for its controls, and every swap is used', () => {
    const all = [...LESSONS.flatMap((l) => [l.text, l.deed]), CLOSING.text].join('\n');
    for (const game of ['browser', 'unreal']) for (const [from] of LESSON_NAMES[game]) expect(all.includes(from), `${game}: ${from}`).toBe(true);
    // The 3D game's own words (ruling b425): YOUR STAFF, the aircraft on the map, the card's pins.
    expect(namedWords(LESSONS.find((l) => l.id === 'raid')!.text, 'unreal')).toContain('In YOUR STAFF, take the air war yourself. Click your aircraft on the map');
    expect(namedWords(LESSONS.find((l) => l.id === 'card')!.deed, 'unreal')).toBe('Click the pin on one line of the card.');
    // Ruling b429: one closing text in both games, YOUR STAFF in 3D at 29 words; the verdict and the button's own words.
    expect(namedWords(CLOSING.text, 'unreal')).toMatch(/^YOUR STAFF hands you each desk when you want it: research, building, envoys, agents, air, navy\./);
    expect(wordCount(namedWords(CLOSING.text, 'unreal'))).toBe(29);
    expect(LESSONS.find((l) => l.id === 'attack')!.text).toContain('Attack when its verdict favours you.');
    expect(LESSONS.find((l) => l.id === 'end')!.text).toContain('When it reads NEXT IN THE REAR, END THE MONTH.');
    const byId = (id: string): string => namedWords(LESSONS.find((l) => l.id === id)!.text, 'browser', ' under STAFF');
    expect(byId('month')).toContain('END MONTH carries out');
    expect(byId('rail')).toContain('Right-click blue ground to ride there');
    expect(byId('build')).toContain('Winter is for building: place one order in BUILD under STAFF.');
    expect(namedWords(CLOSING.text, 'browser')).toContain('REGION shows what you are playing for');
    // The shared words are untouched for the other game.
    expect(namedWords(LESSONS[0].text, 'shared')).toContain('END THE MONTH');
  });

  it('gives places their "the" in running text', () => {
    expect(placeThe('Saar')).toBe('the Saar');
    expect(placeThe('Danzig Corridor')).toBe('the Danzig Corridor');
    expect(placeThe('Warsaw')).toBe('Warsaw');
  });
});

describe('the notebook', () => {
  it('is kept with the player: it reads what was saved and mends what was not', () => {
    expect(readNotebook(null)).toEqual({ on: true, set: false, done: [], later: [], tips: [] });
    // (One kept before b434 with the notes off was switched off by its player.)
    expect(readNotebook({ on: false, done: ['month', 7, 'nonsense'], later: 'x' })).toEqual({ on: false, set: true, done: ['month'], later: [], tips: [] });
    expect(readNotebook({ on: true, set: true })).toEqual({ on: true, set: true, done: [], later: [], tips: [] });
  });

  it('puts a note away and brings it back; the GUIDE counts what waits', () => {
    const b = book();
    putAway(b, 'attack');
    expect(waitingCount(b)).toBe(1);
    bringBack(b, 'attack');
    expect(waitingCount(b)).toBe(0);
  });

  it('shows a lesson again, and teaches all ten again from the door', () => {
    const b = book(ids);
    b.done.push(CLOSING.id);
    b.on = false;
    expect(allDone(b)).toBe(true);
    showAgain(b, 'march');
    expect(isDone(b, 'march')).toBe(false);
    expect(isDone(b, CLOSING.id)).toBe(false);
    expect(b.on).toBe(true);
    lessonsAgain(b);
    expect(doneCount(b)).toBe(0);
  });
});

describe('FIRST COMMAND, played', () => {
  it('is a grand campaign from the first month; the notes stand aside where the setting is off', () => {
    const g = firstHour();
    expect(firstCommand(g)).toBe(true);
    expect(g.turn).toBe(1);
    expect(lessonsOn(g, book())).toBe(true);
    const off = { ...book(), on: false, set: true };
    expect(lessonsOn(g, off)).toBe(false);
    expect(currentLesson(g, off, { card: true })).toBeNull();
    expect(lessonDeed(g, off, 'card', { card: true })).toEqual([]);
  });

  it('opens with lesson 1 on the month card, and its deed is putting the card down', () => {
    const g = firstHour();
    const b = book();
    expect(currentLesson(g, b, { card: true })?.id).toBe('month');
    // Lesson 2 waits for lesson 1.
    expect(lessonReady(g, b, LESSONS[1])).toBe(false);
    expect(lessonDeed(g, b, 'card', {}).length).toBe(0);
    expect(lessonDeed(g, b, 'card', { card: true }).map((l) => l.id)).toEqual(['month']);
    expect(currentLesson(g, b, {})?.id).toBe('pickup');
  });

  it('points lesson 2 at an army at the front near the capital, named in the note', () => {
    const g = firstHour();
    const at = pickupPlace(g);
    expect(at).toBeGreaterThanOrEqual(0);
    expect(g.regions[at].units.some((c) => c.nat === 'GER')).toBe(true);
    expect(g.regions[at].name).not.toBe('Saar');
    const w = lessonWords(g, LESSONS[1], 'browser');
    expect(w.text).toContain(`Click the flag at ${placeThe(g.regions[at].name)} to pick the army up.`);
    expect(w.deed).toBe('Pick up one army.');
  });

  it('brings lesson 3 when an army is in hand, and lesson 4 beside the enemy, in that order', () => {
    const g = firstHour();
    const b = book(['month', 'pickup']);
    const army = g.regions[pickupPlace(g)].units.filter((c) => c.nat === 'GER').map((c) => c.id);
    expect(currentLesson(g, b, { inHand: army })?.id).toBe('march');
    expect(lessonDue(g, b, LESSONS[3], {})).toBe(true);
    expect(currentLesson(g, b, {})?.id).toBe('attack');
    expect(lessonDeed(g, b, 'march').map((l) => l.id)).toEqual(['march']);
    expect(lessonDeed(g, b, 'attack').map((l) => l.id)).toEqual(['attack']);
    // Lesson 5 comes once lesson 4 is done, and the month's end is its deed.
    expect(currentLesson(g, b, {})?.id).toBe('end');
    expect(lessonDeed(g, b, 'end').map((l) => l.id)).toEqual(['end']);
  });

  it('a deed done before its note still earns its stamp, as long as its turn has come', () => {
    const g = firstHour();
    const b = book();
    // The month's end before lesson 4 is done: lesson 5 is not yet ready, so nothing is stamped.
    expect(lessonDeed(g, b, 'end')).toEqual([]);
    // A train ridden in the first month: lesson 8 has no lesson before it, so it is stamped.
    expect(lessonDeed(g, b, 'rail').map((l) => l.id)).toEqual(['rail']);
  });

  it('brings lesson 6 on the second month\'s card, and the raid and the arsenal as the months go by', () => {
    const g = firstHour();
    const b = book(upTo('card'));
    expect(currentLesson(g, b, { card: true })).toBeNull();
    month(g);
    expect(g.turn).toBe(2);
    expect(currentLesson(g, b, { card: true })?.id).toBe('card');
    expect(lessonDeed(g, b, 'line', { card: true }).map((l) => l.id)).toEqual(['card']);
    // The raid waits for the railway, or for two months.
    const raid = LESSONS.find((l) => l.id === 'raid')!, build = LESSONS.find((l) => l.id === 'build')!;
    expect(lessonReady(g, b, raid)).toBe(false);
    expect(lessonReady(g, b, build)).toBe(false);
    month(g);
    expect(lessonReady(g, b, raid)).toBe(true);
    expect(lessonReady(g, b, build)).toBe(false);
    month(g);
    expect(lessonReady(g, b, build)).toBe(true);
  });

  it('brings lesson 8 only where the map lights a railway: never for an army with a corps short of its month', () => {
    const g = firstHour();
    const b = book(upTo('rail'));
    const rail = LESSONS.find((l) => l.id === 'rail')!;
    // (An army as a click on its flag picks it up: every corps of his there with moves left.)
    const armies = g.regions.map((r) => r.units.filter((c) => c.nat === 'GER' && c.ap > 0 && !c.dead)).filter((cs) => cs.length);
    const rides = armies.find((cs) => lessonDue(g, b, rail, { inHand: cs.map((c) => c.id) }));
    expect(rides).toBeDefined();
    rides![0].ap = 3;
    expect(lessonDue(g, b, rail, { inHand: rides!.map((c) => c.id) })).toBe(false);
  });

  it('teaches NEXT ARMY and the month\'s end at peace too: lesson 5 comes in the second month without a battle', () => {
    const g = firstHour();
    const b = book(upTo('attack'));
    const end = LESSONS.find((l) => l.id === 'end')!;
    expect(lessonReady(g, b, end)).toBe(false);
    month(g);
    expect(lessonReady(g, b, end)).toBe(true);
    // The first month's end, read as the second opens, is not its deed: lesson 5 was not yet due then (the review of 85.1).
    expect(lessonDeed(g, b, 'end', { ended: g.turn - 1 })).toEqual([]);
    expect(lessonDue(g, b, end, {})).toBe(true);
    // The second month's end is.
    month(g);
    expect(lessonDeed(g, b, 'end', { ended: g.turn - 1 }).map((l) => l.id)).toEqual(['end']);
  });

  it('names for lesson 2 at peace an army that does not stand in the capital', () => {
    const g = startOpen(createGame(5, 'ITA'));
    for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
    beginHuman(g);
    const at = pickupPlace(g);
    expect(at).toBeGreaterThanOrEqual(0);
    expect(at).not.toBe(g.nations.ITA.capital);
    expect(g.regions[at].units.some((c) => c.nat === 'ITA')).toBe(true);
  });

  it('owes the closing note once all ten are done, and only once', () => {
    const g = firstHour();
    const b = book(ids);
    expect(closingOwed(g, b)).toBe(true);
    b.done.push(CLOSING.id);
    expect(closingOwed(g, b)).toBe(false);
  });

  it('moves lesson 10 on once the ARSENAL is open: the note asks for the artillery corps, not for the page (build 1640)', () => {
    const g = firstHour();
    const build = LESSONS.find((l) => l.id === 'build')!;
    for (const game of ['shared', 'browser', 'unreal']) {
      const there = lessonWords(g, build, game, '', true);
      expect(lessonWords(g, build, game).text).not.toBe(there.text);
      expect(there.text).toMatch(/^Order an artillery corps\./);
      expect(there.text).not.toMatch(/\bopen\b/i);
      expect(there.deed).toBe(lessonWords(g, build, game).deed);
    }
    // (A lesson with no words for its page reads the same either way.)
    expect(lessonWords(g, LESSONS[2], 'browser', '', true)).toEqual(lessonWords(g, LESSONS[2], 'browser'));
  });

  it('fills the city lesson\'s place names, and reads the design director\'s words where no siege stands', () => {
    const g = firstHour();
    const w = lessonWords(g, LESSONS.find((l) => l.id === 'city')!, 'browser');
    expect(w.text.startsWith('The province is not yours until its city falls.')).toBe(true);
  });

  it('names a province once where it is named for its city, and its town where it is not (the cities chart)', () => {
    const g = firstHour();
    const city = LESSONS.find((l) => l.id === 'city')!;
    const ger = g.corps.find((c) => c.nat === 'GER' && !c.dead)!;
    const enemy = g.regions.filter((r) => r.ctrl === 'POL');
    const besiege = (r: Region): string => {
      for (const x of g.regions) x.units = x.units.filter((u) => u !== ger);
      r.units.push(ger);
      return lessonWords(g, city, 'browser').text;
    };
    const same = enemy.find((r) => townName(r) === r.name)!, other = enemy.find((r) => townName(r) !== r.name)!;
    expect(g.flags.sieges).toBe(1);
    expect(besiege(same).startsWith(`${same.name} is not yours until its city falls.`)).toBe(true);
    expect(besiege(other).startsWith(`${placeName(other.name, true)} is not yours until ${townName(other)} falls.`)).toBe(true);
  });
});
