// How a sea is said in running text (the UI director, 18 November 2026: ui-director/words-sea-names.md). Every sea
// has a 'where' form, the words after "The action" or a verb of place: "in the North Sea" by default, "off the Azores"
// for a sea named for the land beside it, "in Hudson Bay" where no article belongs. From it comes the 'what' form,
// for a sea that is a sentence's object: "the North Sea", "the waters off the Azores", "Hudson Bay". One list serves
// both games and every sentence that names a sea; the map's labels keep the map names. Words only: no draw, no rule.

import data from '../../data/campaign/sea-words.json';

const WHERE: Record<string, string> = (data as { where: Record<string, string> }).where;

/** Where a sea is, in running text: "in the North Sea", "off the Azores", "in Hudson Bay", "in the approaches to Hawaii". */
export function seaWhere(name: string): string {
  return WHERE[name] ?? `in the ${name}`;
}

/** A sea as a sentence's object: "the North Sea", "the waters off the Azores", "Hudson Bay", "the approaches to Hawaii". */
export function seaWhat(name: string): string {
  const w = seaWhere(name);
  return w.startsWith('off ') ? `the waters ${w}` : w.replace(/^in /, '');
}

/** The what form at a sentence's head: "The North Sea", "The waters off the Azores", "Hudson Bay". */
export function seaHead(name: string): string {
  const w = seaWhat(name);
  return w.charAt(0).toUpperCase() + w.slice(1);
}

/**
 * Whether a sea's what form takes a plural verb: "the waters off the Azores are closed", "the Western Approaches
 * are", against "the North Sea is". Read from the head noun: the last word before "of", "to" or "off".
 */
export function seaPlural(name: string): boolean {
  const head = seaWhat(name).replace(/^the /, '').split(/ (?:of|to|off) /)[0].split(' ').pop() ?? '';
  return /[^s]s$/.test(head);
}

/** "is" or "are", as the sea's what form takes it. */
export const seaIs = (name: string): string => (seaPlural(name) ? 'are' : 'is');

/** Words joined as a list: "a", "a and b", "a, b and c". */
const joined = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

/** Several seas' where forms joined: "in the North Sea and off the Azores". */
export const seaWhereList = (names: string[]): string => joined(names.map(seaWhere));

/** Several seas' what forms joined: "the North Sea, the Bay of Biscay and the waters off the Azores". */
export const seaWhatList = (names: string[]): string => joined(names.map(seaWhat));

const ORDINAL = ['', '', 'second', 'third', 'fourth', 'fifth', 'sixth', 'seventh', 'eighth', 'ninth', 'tenth'];

/**
 * A fleet action's name: "The action" and where the sea is ("The action off the Azores"); the second in the same sea
 * in the same month is "The second action off the Azores", the third "The third action ...". With no sea known, "at sea".
 */
export function seaAction(name: string | null, nth = 1): string {
  return `${nth <= 1 ? 'The action' : `The ${ORDINAL[nth] ?? `${nth}th`} action`} ${name ? seaWhere(name) : 'at sea'}`;
}

const reEsc = (s: string): string => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

/**
 * The sea a line of news names, found by its where form ("... sinks a cruiser of the Home Fleet off the Azores: ..."):
 * its index in `names` and the words that say it, with the space before them. The earliest in the line wins, then
 * the longest. A where form counts only where a clause ends after it (" and ", a stop, a comma, a colon, the end).
 */
export function seaSaid(text: string, names: string[]): { i: number; said: string } | null {
  let i = -1, said = '', at = Infinity;
  for (let k = 0; k < names.length; k++) {
    const w = ` ${seaWhere(names[k])}`;
    const m = new RegExp(`${reEsc(w)}(?= and |[.,:]|$)`).exec(text);
    if (m && (m.index < at || (m.index === at && w.length > said.length))) { i = k; said = w; at = m.index; }
  }
  return i >= 0 ? { i, said } : null;
}
