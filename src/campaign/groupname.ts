/**
 * A group's name in the lines about a bought wing: 'the' before a name in English, which opens with an English
 * ordinal ('the 1st Fighter Group'); a name in its own language stays bare ('1. Hava Alayı', 'Regimiento Aéreo 1').
 * The UI director, 21 November (ui-director/words-bought-wings.md). Her one rule for a raid on troops' report lines too
 * (25 November 2026, 21:22Z, g): "Groupement de Chasse 3", "JG 26" bare; "the 2nd Fighter Group", "The" at the head of a
 * sentence. A module of its own, with nothing imported, so the battle (src/battle/air/sim.ts) reads the same rule.
 */
export const theGroupName = (name: string): string => (/^\d+(?:st|nd|rd|th) /.test(name) ? `the ${name}` : name);
