// What a bomber carries to tell it from the next one.
//
// Brian, 20 November 2026: "i want the same mechanics I talked about bombing to be applied for all nations not just the
// US which I was using as an example", and "nose art only for US bombers, other countries didnt do that". So an
// American bomber has a name from the nose-art list, and every other nation's has the marking its air force painted:
// Britain's a squadron's two letters and its own ('KM-G', called 'G for George'), Germany's a unit code ('5J+DM'),
// Japan's a tail code and number, Italy's a squadron and aircraft number, the Soviet Union's a coloured number, and
// everyone else's a plain number. The words and numbers are in data/campaign/bomber-marks.json, from the historian's
// note (historian/bomber-tours-identity.md).
//
// A marking is worked out from the group it belongs to and the bomber's place in it, never drawn from the dice of the
// war, so the same war always paints the same bombers.

import list from '../../data/battle/nose-art.json';
import data from '../../data/campaign/bomber-marks.json';
import { Rng, hashString } from '../core/rng';

export type MarkStyle = 'nose' | 'letter' | 'code' | 'tail' | 'squadron' | 'tactical' | 'number';

export interface BomberMark {
  /** What the bomber is called in a sentence: 'Prize Pumpkin', 'G for George', '5J+DM'. */
  name: string;
  /** What is painted on it besides a name: 'KM-G', '5J+DM'. '' for an American bomber, whose letters the scene paints. */
  mark: string;
  /** The radio word of its letter, where its air force used one: 'George', 'Dora'. */
  word?: string;
}

const MARKS = data as unknown as {
  letter: { from: number; early: string[]; late: string[]; block: string[]; never: string[] };
  code: { aircraft: string; squadrons: string; words: string[] };
  tail: { codes: string[]; first: number; per: number; never: string[] };
  squadron: { from: number; to: number; per: number };
  tactical: { colours: string[]; per: number };
};
const NAMES = (list as { names: string[] }).names;
const NUMERALS = ['', '', ' II', ' III', ' IV', ' V', ' VI', ' VII', ' VIII', ' IX', ' X'];
const ALPHABET = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ';

/** How many names there are on the nose-art list: numbers 1 to this. */
export const NOSE_ART = NAMES.length;

/** A group's own order of some list: every group draws its codes differently, and the same group always the same way. */
const shuffles = new Map<string, string[]>();
function shuffled(key: string, what: string, all: () => string[]): string[] {
  const k = `${what}:${key}`;
  let o = shuffles.get(k);
  if (!o) {
    const rng = new Rng(hashString(`mark:${k}`));
    o = all();
    for (let i = o.length - 1; i > 0; i--) { const j = rng.int(i + 1); [o[i], o[j]] = [o[j], o[i]]; }
    if (shuffles.size > 600) shuffles.clear();
    shuffles.set(k, o);
  }
  return o;
}

const squadronLetters = (): string[] => {
  const out: string[] = [], L = ALPHABET.replace('I', '');
  for (const a of L) for (const b of L) if (a !== b && !MARKS.letter.block.includes(a + b)) out.push(a + b);
  return out;
};
const wingCodes = (): string[] => {
  const out: string[] = [], L = ALPHABET.replace('I', '').replace('O', '');
  for (let d = 1; d <= 9; d++) for (const a of L) { out.push(`${d}${a}`); out.push(`${a}${d}`); }
  return out;
};

/**
 * What a bomber carries. 'key' is its group in its war (seed and group), 'slot' its place in the group from 1, 'ord'
 * which of that name it is for a named bomber (the second is 'II'), and 'year' the year, which chooses the words the
 * Royal Air Force used for its letters.
 */
export function markOf(style: MarkStyle, key: string, slot: number, ord = 1, year = 1943): BomberMark {
  if (slot <= 0) return { name: '', mark: '' };
  const i = slot - 1;
  switch (style) {
    case 'nose': return { name: `${NAMES[i] ?? `No. ${slot}`}${NUMERALS[ord] ?? ` ${ord}`}`, mark: '' };
    case 'letter': {
      const codes = shuffled(key, 'letter', squadronLetters);
      const l = i % 26, word = (year >= MARKS.letter.from ? MARKS.letter.late : MARKS.letter.early)[l];
      // A famous real aircraft's letters are never worn (the modeller's layer paints nothing for them): the bomber whose
      // turn it would be takes the last letters in its group's order, which no group is big enough to reach.
      const N = MARKS.letter.never, never = (c: string): boolean => N.includes(c) || N.includes(`${c}-${ALPHABET[l]}`);
      let at = Math.floor(i / 26) % codes.length;
      for (let spare = codes.length - 1; spare >= 0 && never(codes[at]); spare--) at = spare;
      return { name: `${ALPHABET[l]} for ${word}`, mark: `${codes[at]}-${ALPHABET[l]}`, word };
    }
    case 'code': {
      const C = MARKS.code, codes = shuffled(key, 'code', wingCodes);
      const per = C.aircraft.length, wing = per * C.squadrons.length, w = i % wing;
      const a = C.aircraft[w % per];
      const mark = `${codes[Math.floor(i / wing) % codes.length]}+${a}${C.squadrons[Math.floor(w / per)]}`;
      return { name: mark, mark, word: C.words[ALPHABET.indexOf(a)] };
    }
    case 'tail': {
      const T = MARKS.tail, codes = shuffled(key, 'tail', () => [...T.codes]);
      const code = codes[Math.floor(i / T.per) % codes.length], n = T.first + (i % T.per);
      // Two famous real aircraft's markings are never carried: that bomber's number is one past its unit's own run.
      const mark = `${code}-${T.never.includes(`${code}-${n}`) ? n + T.per : n}`;
      return { name: mark, mark };
    }
    case 'squadron': {
      const S = MARKS.squadron;
      const first = S.from + new Rng(hashString(`mark:squadron:${key}`)).int(S.to - S.from + 1);
      const mark = `${first + Math.floor(i / S.per)}-${(i % S.per) + 1}`;
      return { name: mark, mark };
    }
    case 'tactical': {
      const T = MARKS.tactical, all = T.per * T.colours.length;
      const mark = `${T.colours[Math.floor(i / T.per) % T.colours.length]} ${(i % T.per) + 1 + T.per * Math.floor(i / all)}`;
      return { name: mark, mark };
    }
    default: return { name: `No. ${slot}`, mark: `${slot}` };
  }
}
