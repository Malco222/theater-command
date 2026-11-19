// The names of the air battle's groups in its dispatches (the UI director's words): the subject of a line and its verb,
// one aircraft of a group, and 'a' or 'an'. Shared by the battle (sim.ts) and the words for a fall (falls.ts).
// The UI director's rulings: air review 10 (a carrier's fighters are the fleet's), 19 November 14:26Z (an ordinal
// group takes 'the'; the nation word on the aircraft; a joined or plural name takes a plural verb; a box named for its
// aircraft takes 'the' everywhere; no nation word twice) (ui-director/words-air-battle-dispatches.md).

const ORDINAL = '(?:\\d+(?:st|nd|rd|th)|First|Second|Third|Fourth|Fifth|Sixth|Seventh|Eighth|Ninth|Tenth|Eleventh|Twelfth|Thirteenth|Fourteenth|Fifteenth|Sixteenth|Seventeenth|Eighteenth|Nineteenth|Twentieth)';
/**
 * An ordinal group, the enemy's with its nation word first as called() puts it: '8th Bomb Group', 'American 15th
 * Pursuit Group', 'Eighth Air Force', 'US Ninth Air Force', and by Roman numeral 'VIII Bomber Command', 'I Bomber
 * Command' (never read as 'I'). 'No. 11 Group', 'KG 2' and 'Groupe de Chasse 3' stay bare (ruling 1).
 */
const ORDINAL_NAME = new RegExp(`^(?:[A-Z][A-Za-z]* ){0,2}(?:${ORDINAL}|[IVX]+(?= [A-Z])) `);

export interface Subject {
  /** The name opening a sentence, and inside one. */
  who: string;
  whom: string;
  /** Takes a plural verb: named for its aircraft, or joined by 'and' or a comma ('Torpedo Three, Six and Eight'). */
  many: boolean;
  /** Named for its aircraft ('Shokaku and Zuikaku dive bombers', 'the Home Fleet's carrier fighters'): one of it is 'one of the ...'. */
  craft: boolean;
}

/**
 * A group's name as the subject of a dispatch: "Jagdgeschwader 4 is in a fight", "The Home Fleet's carrier fighters
 * are in a fight", the campaign's carrier bombers "The Home Fleet's strike wing is in a fight" (a fleet called by its
 * letter without the article, "Force H's strike wing"), "The 1st Bomber Group is in a fight", "The Shokaku and Zuikaku
 * dive bombers are in a fight", "Torpedo Three, Six and Eight are in a fight".
 */
export function subjectOf(name: string): Subject {
  const m = /^(.+?) (carrier [a-z]+s|strike wing)$/.exec(name);
  if (m) {
    const whom = `${/(^| )Force [A-Z]$/.test(m[1]) ? '' : 'the '}${m[1]}'s ${m[2]}`;
    const craft = m[2] !== 'strike wing';
    return { who: upFirst(whom), whom, many: craft, craft };
  }
  const craft = /(^| )[a-z]+s$/.test(name);
  const whom = craft || ORDINAL_NAME.test(name) ? `the ${name}` : name;
  return { who: upFirst(whom), whom, many: craft || /,| and /.test(name), craft };
}

/**
 * One aircraft of a group named for its aircraft: 'one of the Shokaku and Zuikaku dive bombers', never 'a bomber of
 * ... dive bombers'; 'adj' (the enemy's nation) after the article: 'one of the Japanese Shokaku and Zuikaku dive bombers'.
 */
export function oneOf(name: string, adj = ''): string {
  const whom = subjectOf(name).whom, the = whom.startsWith('the ') || !whom.includes("'s ");
  return `one of ${[the ? 'the' : '', adj, whom.replace(/^the /, '')].filter(Boolean).join(' ')}`;
}

/**
 * One aircraft of a group, the nation word on the aircraft (the UI director, 14:26Z): 'a bomber of the 8th Bomb Group',
 * 'an American aircraft of Bombing Three and Scouting Six', 'a damaged German bomber of Kampfgeschwader 2'; for a
 * group named for its aircraft 'one of the Japanese Shokaku and Zuikaku dive bombers' ('how' after the name).
 */
export function aCraftOf(name: string, noun: string, adj = '', how = ''): string {
  if (subjectOf(name).craft) return `${oneOf(name, adj)}${how ? `, ${how},` : ''}`;
  const what = [how, adj, noun].filter(Boolean).join(' ');
  return `${an(what)} ${what} of ${subjectOf(name).whom}`;
}

/**
 * The nation word a line adds to a group's name, unless the name already says it ('Polish Navy', 'Royal Norwegian
 * Navy') or names another nation's ('US Ninth Air Force', whoever holds it): never two (ruling 5).
 */
export const adjFor = (name: string, adj: string): string => (!adj || /^US /.test(name) || new RegExp(`(^| )${adj.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}( |'|$)`).test(name) ? '' : adj);

export const upFirst = (s: string): string => s[0].toUpperCase() + s.slice(1);

/** 'a' or 'an' before a word: 'an Italian', 'a Uruguayan', 'a European'. */
export const an = (word: string): string => (/^[AEIOU]/i.test(word) && !/^(Eu|Uk|Uni|Uru|Uga|One)/i.test(word) ? 'an' : 'a');

/** A line about a group, its verbs agreeing with the name: told(name, '{N} [is|are] in a fight.'). */
export function told(name: string, text: string): string {
  const s = subjectOf(name);
  return text.replace(/\{N\}/g, s.who).replace(/\[([^|\]]*)\|([^\]]*)\]/g, (_, one: string, many: string) => (s.many ? many : one));
}
