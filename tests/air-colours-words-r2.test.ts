// The art director's words for each earned colour scheme, as issued (release 2; earned names and colours under the
// switch 'nicknames'). data/campaign/air-colours.json "words" is the art director's unit-colours-words.json copied whole,
// no word changed: the fifth issue of 25 November 2026 (12:34Z), five card lines changed from the third (Britain 7,
// Italy 3, 4, 5 and 8), scheme numbers and count unchanged.
import { describe, expect, it } from 'vitest';
import colours from '../data/campaign/air-colours.json';

const ISSUED = '/mnt/project-files/art-director/air-colours/unit-colours-words.json';
const game = colours as unknown as { count: number; words: Record<string, string[]> };

describe('the earned colours\' words', () => {
  it('the game\'s words are the art director\'s file, line for line (where the project\'s files are mounted)', async (ctx) => {
    type Issued = { count: number; words: Record<string, string[]> };
    // (Off the project's machine the file cannot be read; the issued lines in the next test stand for it there. The test
    // then shows as skipped, by its name, never as passed with nothing checked: the review of the air follow-on, note 4.)
    const issued: Issued | null = await import(/* @vite-ignore */ ISSUED).then((m: { default: Issued }) => m.default, () => null);
    if (!issued) return ctx.skip(`the issued file ${ISSUED} cannot be read here`);
    expect.hasAssertions();
    expect(issued.words.UK.length).toBe(16);
    expect(game.count).toBe(issued.count);
    expect(Object.keys(game.words)).toEqual(Object.keys(issued.words));
    for (const nat of Object.keys(issued.words)) {
      expect(game.words[nat].length, nat).toBe(issued.words[nat].length);
      issued.words[nat].forEach((line, i) => expect(game.words[nat][i], `${nat} ${i + 1}`).toBe(line));
    }
  });

  it('the fifth issue\'s five changed lines, and the count', () => {
    expect(game.count).toBe(16);
    for (const list of Object.values(game.words)) expect(list.length).toBe(game.count);
    expect(game.words.UK[6]).toBe('Turquoise spinner, wine red nose band, tail banded in turquoise and wine red');
    expect(game.words.ITA[2]).toBe('Moss green spinner, turquoise nose band, moss green and turquoise checked tail');
    expect(game.words.ITA[3]).toBe('Turquoise spinner, azure blue nose band, tail banded in turquoise and azure blue');
    expect(game.words.ITA[4]).toBe('Turquoise spinner, slate blue nose band, turquoise and slate blue striped tail');
    expect(game.words.ITA[7]).toBe('Pale blue spinner, wine red nose band, tail halved, pale blue over wine red');
    // Every line splits at its first two commas into spinner, nose band and tail (tiers 1, 2 and 3).
    for (const [nat, list] of Object.entries(game.words)) for (const line of list) expect(line.split(',').length, `${nat}: ${line}`).toBeGreaterThanOrEqual(3);
  });
});
