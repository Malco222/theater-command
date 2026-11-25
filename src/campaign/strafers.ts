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

/** The UI director's words, 15:04Z; the line for none as she softened it at 16:04Z. */
export const STRAFE_WORDS = {
  heading: 'LIKELY TO GO DOWN AND STRAFE',
  note: 'The fighter groups most likely to go down and strafe if the sky is won. Which of them do is settled over the target.',
  none: 'No fighter group is likely to go down and strafe, though one still may.',
  nobody: 'No fighter group went down to strafe.',
} as const;

/**
 * The raid card's section: the heading; the muted line once, and the groups' names a row each; or, where it names
 * none, the line for none in place of both ('' in `note`, no names).
 */
export interface StrafeSection { heading: string; note: string; names: string[]; none: string }

export function strafeSection(names: string[]): StrafeSection {
  return names.length
    ? { heading: STRAFE_WORDS.heading, note: STRAFE_WORDS.note, names: [...names], none: '' }
    : { heading: STRAFE_WORDS.heading, note: '', names: [], none: STRAFE_WORDS.none };
}

// (Names joined as the report's other sentences join them: air.ts `list`, "A, B and C", the same line every module keeps.)
const list = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

/**
 * The raid report's line on who went down, as `reader` reads it: only a raid on troops whose report kept its strafers
 * (the switch on when it was flown), and only for the side that flew it. '' otherwise: for the defender, whom her words
 * do not cover, for any other raid, and for a report saved without the field (an older save, or the switch off).
 */
export function strafedLine(rep: { mission: string; attNat: string; strafed?: string[] }, reader: string | null): string {
  if (rep.mission !== 'ground' || !rep.strafed || reader === null || reader !== rep.attNat) return '';
  return rep.strafed.length ? `${list(rep.strafed)} went down and strafed.` : STRAFE_WORDS.nobody;
}

/**
 * Who heard what each raid on troops charged the ground fire on: the tests (the card, the settled raid, the fought
 * raid's booking say whom, by the one rule). Never set in play; read only with the rule on.
 */
export const strafeWatch: {
  tell?: (x: { route: 'card' | 'settled' | 'fought'; target: number; nat: string; strafers: number[]; fire: number; weight: number; cut?: number }) => void;
} = {};
