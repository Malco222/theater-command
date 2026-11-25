// Better air unit names (release 2; Brian 25 November 2026: "can we come up with better names for the airforce
// squadrons?"; the design director's ruling of 03:18Z, design-director/air-names-1125.md). Words only.
//
// Ruling 220's strengths and rungs stay: a group takes the rung nearest its full strength, once, as it forms. Under the
// switch 'airNames' the words on the rung change (air.ts groupName reads this file's data):
//   (a) numbers from the lists the real air arm used, in the order the game forms its groups, going on past the end;
//   (b) the role in the name only where the arm said it there, else shown beside the name (roleWord);
//   (c) Japan's island groups by the airfield they form at, and every arm its own plain form where no rung is near.
//       (Italy's day-one names were dropped by Brian's answer, 03:31Z: its words are its earned list, airnicks.ts.)
// The forming-order positions live in g.airNames, a field of their own: no counter another rule reads is moved, and
// naming throws no dice. A group keeps the name it was given for life, whatever the switch says later.
//
// Off, nothing here is read and every name is ruling 220's.

import namesData from '../../data/campaign/air-names.json';
import type { AirFleet, Game } from './types';

type Pair<T> = { ftr: T; bmr: T };
/** A number in an arm's list, or a whole label where the arm wrote its units so ("I./JG 26", "II/5"). */
export type Label = number | string;

/** The switch's parts of a rung of air-names.json, beside ruling 220's (all optional; an empty or missing list numbers as before). */
export interface RungR2 { numbers?: Pair<Label[]>; formR2?: Pair<string>; short?: Pair<string>; skipIfTaken?: Pair<string> }

export const AIRNAMES = namesData as unknown as {
  navyIslands: string[];
  navyStations: Record<string, string>;
  fallback: Record<string, Pair<string> & { numbers?: Pair<Label[]> }>;
  blocked: { names: string[] };
};

/** True where this campaign names its new air groups by the switch's rule. */
export const airNamesOn = (g: Game): boolean => g.flags.open === 1 && g.flags.airNames === 1;

/** Turns the air arms' own names on or off for the groups formed from now on. A name already given stays either way. */
export function setAirNames(g: Game, on: boolean): void {
  if (on) g.flags.airNames = 1;
  else delete g.flags.airNames;
}

/** The forming-order book (g.airNames), made on first use only: a war that never names by the switch never carries it. */
export function nameBook(g: Game): { at: Record<string, number>; top: Record<string, number> } {
  return (g.airNames ??= { at: {}, top: {} });
}

/**
 * The next number of a series for a new group: the list's next entry in forming order, and past its end the series goes
 * on in the same manner: the next whole number above the largest yet handed out or listed; for a list of labels, the
 * label's last number counted on (and for Roman-numbered labels, "I./JG 77", the I, II and III of each new number).
 * Never one whose name `taken` says the nation already carries, nor one `skip` passes over.
 */
export function nextNumber(g: Game, key: string, list: Label[] | undefined, name: (n: Label) => string, taken: (x: string) => boolean, skip?: (n: Label) => boolean): { n: Label; name: string } {
  const B = nameBook(g);
  const L = list ?? [];
  const nums = L.filter((x): x is number => typeof x === 'number');
  for (let guard = 0; ; guard++) {
    const i = B.at[key] ?? 0;
    B.at[key] = i + 1;
    let n: Label;
    if (i < L.length) n = L[i];
    else if (L.length && typeof L[L.length - 1] === 'string') n = labelPast(L as string[], i - L.length);
    else n = Math.max(B.top[key] ?? 0, ...nums, 0) + 1;
    if (typeof n === 'number') B.top[key] = Math.max(B.top[key] ?? 0, n);
    const x = name(n);
    if ((!skip || !skip(n) || guard > 10000) && !taken(x)) return { n, name: x };
  }
}

const ROMAN = ['I', 'II', 'III'];
/** The k-th label past the end of a list of labels, in the list's manner. */
function labelPast(L: string[], k: number): string {
  const last = L[L.length - 1];
  const top = Math.max(0, ...L.map((x) => Number(/(\d+)$/.exec(x)?.[1] ?? 0)));
  const roman = /^([IVX]+)(\D+?)(\d+)$/.exec(last);
  if (roman) return `${ROMAN[k % 3]}${roman[2]}${top + 1 + Math.floor(k / 3)}`;
  const plain = /^(.*?)(\d+)$/.exec(last);
  if (plain) return `${plain[1]}${top + 1 + k}`;
  return `${last} ${k + 2}`;
}

/** The plain role word shown beside a group's name where the name does not say it: 'fighters' or 'bombers'. */
export function roleWord(f: AirFleet): string | undefined {
  if (f.roleNamed) return undefined;
  const kind = f.kind ?? (f.bmr > f.ftr ? 'bmr' : 'ftr');
  return kind === 'bmr' ? 'bombers' : 'fighters';
}

/** The short form for a map counter: the rung's own where the data gives one, else the name itself. */
export const shortName = (f: AirFleet): string => f.short ?? f.name;

// Matched as whole words, so that a short real name ("Ram", "Nice") does not bar a longer word of our own (Tramontana, Fenice).
const esc = (x: string): string => x.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
const BLOCKED = AIRNAMES.blocked.names.map((b) => new RegExp(`(^|[^\\p{L}\\p{N}])${esc(b)}($|[^\\p{L}\\p{N}])`, 'iu'));
/** True for a name that holds, as whole words, one of the real air units' names on the air block list. */
export const airBlocked = (name: string): boolean => BLOCKED.some((b) => b.test(name));
