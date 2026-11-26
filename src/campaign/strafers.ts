// A raid on troops: which of the raid's fighters go down on the troops, and so pay the ground fire (release 2; the
// design director's ruling of 25 November 2026, 05:12Z, on lane (a)'s review: "the forecast charges ground fire only on
// the fighters expected to be left after the air fight, and only on those with the reach to strafe"; the card, the raid
// the staff settles and the raid fought out must agree, the settled raid pricing the same expected strafers behind the
// same switch).
//
// One rule for all three. A fighter group has the reach to strafe when the troops lie within airwar.json
// ground.reach of its fighter range (G); one farther out escorts its bombers and turns for home: it does not strafe,
// adds no weight on the troops, and is not shot at by the ground. Of those with the reach, the fighters that go down
// are the ones left after the air fight: the card counts those it expects to be left (each group's share of the
// fighters it expects lost in the air), the settled raid those its dice left, the fought raid those the field left
// that flew their passes. Each pays the ground fire (ground.fire) for each fighter that goes down, and counts on the
// troops at ground.fighters.
//
// Behind lane (a)'s switch alone (the design director, 25 November 2026 12:59Z, question 1: "one switch"): on wherever a
// raid on troops can be fought out (flyKinds(['ground']), TC_FLY=ground, TC.airKinds(['ground'])), off as shipped. With
// it on, the staff's raid on troops books as before EXCEPT that the ground fire and the fighters' weight on the troops
// fall only on the fighters left with the reach to strafe (tests/air-troops-r2.test.ts says so). With it off nothing here
// is read and every raid is priced, settled and fought as on d06286d4.

import { theGroupName } from './groupname';

let flown = false;

/** Lane (a)'s switch, as flyKinds (battle/air/ticket.ts) last set it: whether a raid on troops can be fought out. */
export function groundFlown(on: boolean): void { flown = on; }

/** Whether the ruling is in force: lane (a)'s switch on for raids on troops (the call, or TC_FLY=ground). */
export function strafeOn(): boolean {
  const G = globalThis as { TC_FLY?: unknown; process?: { env?: Record<string, string | undefined> } };
  const env = G.process?.env;
  return flown || String(G.TC_FLY ?? env?.TC_FLY ?? '').split(',').map((x) => x.trim()).includes('ground');
}

// The words (release 2, behind the same switch). The design director, 25 November 2026 14:59Z, accepted the card's
// "more likely than not" list and asked the UI director "for words that say the list is the likely one, so a strafer
// who never arrives isn't a broken promise"; her words of 15:04Z, to the letter, no colour on any of them: the card's
// section (a heading, a muted line once, the groups a row each, or the line for none) and the report's line, so the
// plan and the result read as a pair. Nothing here is shown with the switch off: the card's section is built only
// where forecastAir gave the card its strafers, and the report's line only from AirReport.strafed, written only then.

/**
 * The UI director's words, 15:04Z; the muted note as she mended it at 19:17Z (it loses "if the sky is won", which
 * pointed at a verdict a raid on troops' card does not show); the line for none by its cause, 19:10Z (her marks,
 * section A), one line a cause, in the order she confirmed at 19:17Z (WhyNone).
 */
export const STRAFE_WORDS = {
  heading: 'LIKELY TO GO DOWN AND STRAFE',
  note: 'The fighter groups most likely to go down and strafe. Which of them do is settled over the target.',
  none: {
    alone: 'No fighters fly with this raid.',
    reach: 'No fighter group has the range to go down and strafe there.',
    odds: 'No fighter group is likely to go down and strafe: the defenders\' fighters are stronger than yours. One you send down before it meets them still may.',
    fought: 'No fighter group is likely to go down and strafe: the air fight is likely to leave them with no fight left. One you send down before it meets the defence still may.',
  },
  nobody: 'No fighter group went down to strafe.',
  // (The report: a group that went down with too little fight left to do any harm, the UI director 19:10Z A5 and its
  // forms at 19:17Z; the order's answer for a group that carries nothing, A6, said by the battle, src/battle/air/sim.ts.)
  little: 'went down to strafe, but had too little fight left to do any harm.',
} as const;

/**
 * Why the card names no group, the first cause that is true (the UI director, 19:17Z: "No fighters; else no reach; else
 * the odds; else the fought-out line. The first cause that is true is the only one printed"): no fighter group flies
 * with the raid ('alone'); else none has the reach to strafe ('reach', canStrafe false for every one); else the fight
 * each carries is 0 ('odds': the defenders' weight at or above the raid's fighters', strafeCarry); else the air fight
 * is expected to leave none of them a fighter more likely than not ('fought').
 */
export type WhyNone = keyof typeof STRAFE_WORDS.none;

/**
 * The raid card's section: the heading; the muted line once, and the groups' names a row each; or, where it names
 * none, the line for none of its cause in place of both ('' in `note`, no names).
 */
export interface StrafeSection { heading: string; note: string; names: string[]; none: string }

export function strafeSection(names: string[], why: WhyNone): StrafeSection {
  return names.length
    ? { heading: STRAFE_WORDS.heading, note: STRAFE_WORDS.note, names: [...names], none: '' }
    : { heading: STRAFE_WORDS.heading, note: '', names: [], none: STRAFE_WORDS.none[why] };
}

// (Names joined as the report's other sentences join them: air.ts `list`, "A, B and C", the same line every module keeps.)
const list = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

// (A sentence opens with a capital, the group named as the news names it: the UI director, 19:17Z.)
const cap = (x: string): string => x.charAt(0).toUpperCase() + x.slice(1);

// (The groups of one line, each named by the UI director's one rule for this lane, 21:22Z g, theGroupName: a name in its
// own language bare, "Groupement de Chasse 3", "JG 26"; an English one with "the", "The" at the head of the sentence.)
const named = (xs: string[]): string => cap(list(xs.map(theGroupName)));

/**
 * The raid report's line on who went down, as `reader` reads it: only a raid on troops whose report kept its strafers
 * (the switch on when it was flown), and only for the side that flew it. '' otherwise: for the defender, whom her words
 * do not cover, for any other raid, and for a report saved without the field (an older save, or the switch off). The
 * groups that went down with too little fight left to do any harm (strafeLittle) are told apart, after the others
 * (the UI director, 19:17Z: "{A} went down and strafed. {B} went down to strafe, but had too little fight left to do any
 * harm."), each sentence joining its names as the report joins them.
 */
export function strafedLine(rep: { mission: string; attNat: string; strafed?: string[]; strafeLittle?: { names: string[] } }, reader: string | null): string {
  if (rep.mission !== 'ground' || !rep.strafed || reader === null || reader !== rep.attNat) return '';
  if (!rep.strafed.length) return STRAFE_WORDS.nobody;
  const little = rep.strafeLittle?.names ?? [];
  const full = rep.strafed.filter((n) => !little.includes(n)), few = rep.strafed.filter((n) => little.includes(n));
  return [full.length ? `${named(full)} went down and strafed.` : '', few.length ? `${named(few)} ${STRAFE_WORDS.little}` : ''].filter(Boolean).join(' ');
}

/**
 * The raid report's own result sentence on the raider's report card (the browser's, ui/main.ts airReportCard): the
 * report's, except where the only aircraft that reached the troops were groups with too little fight left
 * (strafeLittle.alone), whose line already gives the cause (the UI director, 19:17Z: "Print the A5 line alone there"; 21:22Z,
 * h: "Only the raider's report card prints the A5 line by itself. Every other reader of the raid's result keeps i's
 * line, by the cut: the news, the raided side's report, the month card's raid row ... and the 3D raid list", which read
 * the report's own sentence). The defender reads the report's own sentence; every other report reads as it always did.
 */
export function resultFor(rep: { mission: string; attNat: string; result: string; strafeLittle?: { alone?: true } }, reader: string | null): string {
  return rep.mission === 'ground' && rep.strafeLittle?.alone && reader !== null && reader === rep.attNat ? '' : rep.result;
}

/**
 * Who heard what each raid on troops charged the ground fire on: the tests (the card, the settled raid, the fought
 * raid's booking say whom, by the one rule). Never set in play; read only with the rule on.
 */
export const strafeWatch: {
  tell?: (x: { route: 'card' | 'settled' | 'fought'; target: number; nat: string; strafers: number[]; fire: number; weight: number; cut?: number }) => void;
} = {};
