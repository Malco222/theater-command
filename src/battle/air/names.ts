// The names of the air battle's groups in its dispatches (the UI director's words): the subject of a line and its verb,
// one aircraft of a group, and 'a' or 'an'. Shared by the battle (sim.ts) and the words for a fall (falls.ts).
// The UI director's rulings: air review 10 (a carrier's fighters are the fleet's), 19 November 14:26Z (an ordinal
// group takes 'the'; the nation word on the aircraft; a joined or plural name takes a plural verb; a box named for its
// aircraft takes 'the' everywhere; no nation word twice) and 14:48Z (every group's name takes 'the' unless written as a
// code or a proper name of its own; the enemy's fleet wing as 'a British carrier fighter of the Home Fleet')
// (ui-director/words-air-battle-dispatches.md).

const ORDINAL = '(?:\\d+(?:st|nd|rd|th)|First|Second|Third|Fourth|Fifth|Sixth|Seventh|Eighth|Ninth|Tenth|Eleventh|Twelfth|Thirteenth|Fourteenth|Fifteenth|Sixteenth|Seventeenth|Eighteenth|Nineteenth|Twentieth)';
/**
 * An ordinal group, the enemy's with its nation word first as called() puts it: '8th Bomb Group', 'American 15th
 * Pursuit Group', 'Eighth Air Force', 'US Ninth Air Force', and by Roman numeral 'VIII Bomber Command', 'I Bomber
 * Command' (never read as 'I'). 'No. 11 Group', 'KG 2' and 'Groupe de Chasse 3' stay bare (ruling 1).
 */
const ORDINAL_NAME = new RegExp(`^(?:[A-Z][A-Za-z]* ){0,2}(?:${ORDINAL}|[IVX]+(?= [A-Z])) `);

/** Words of a name left in its own language (the UI director, 14:48Z: 'Jagdgeschwader 2', '4° Stormo', 'Tainan Kokutai'). */
const OWN_TONGUE = /(^| )(?:Jagdgeschwader|Kampfgeschwader|Stukageschwader|Zerstörergeschwader|Nachtjagdgeschwader|Schlachtgeschwader|Geschwader|Luftflotte|Fliegerkorps|Fliegerdivision|Gruppe|Staffel|Stormo|Gruppo|Squadriglia|Groupe|Escadrille|Escadre|Kokutai|Kōkūtai|Sentai|Hikōtai|Hikotai|Hikodan|Butai)( |$)/;

/**
 * A name written as a code or as a proper name of its own, which takes no 'the' (the UI director, 14:48Z), wherever
 * called() has put the enemy's nation word: codes ('KG 2', 'JG 26', 'VMF-221'), 'No.' names ('No. 3 Group'), the
 * American navy's squadrons ('Bombing Three', 'Fighting Six', 'Torpedo Three, Six and Eight'), 'Force H' and its kind,
 * names in their own language, and the RAF's commands as Britain wrote them ('Bomber Command').
 */
const BARE = [
  /(^| )[A-Z]{2,}(?:-| )\d+( |$)/,
  /(^| )No\. /,
  /(^| )(?:Bombing|Fighting|Torpedo|Scouting|Patrol) [A-Z][a-z]+\b/,
  /(^| )Force [A-Z]$/,
  /(^| )(?:Bomber|Fighter|Coastal) Command$/,
  OWN_TONGUE,
];

/** Whether a group's name takes 'the': an ordinal one always ('The I Bomber Command', 'The US Ninth Air Force'); otherwise unless it is BARE. */
export const takesThe = (name: string): boolean => ORDINAL_NAME.test(name) || !BARE.some((r) => r.test(name));

/** A carrier's wing: '{fleet} carrier fighters', '{fleet} strike wing' (data/campaign/carriers.json). */
const FLEET_WING = /^(.+?) (carrier [a-z]+s|strike wing)$/;

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
  const m = FLEET_WING.exec(name);
  if (m) {
    const whom = `${takesThe(m[1]) ? 'the ' : ''}${m[1]}'s ${m[2]}`;
    const craft = m[2] !== 'strike wing';
    return { who: upFirst(whom), whom, many: craft, craft };
  }
  const craft = /(^| )[a-z]+s$/.test(name);
  const whom = craft || takesThe(name) ? `the ${name}` : name;
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
 * group named for its aircraft 'one of the Japanese Shokaku and Zuikaku dive bombers' ('how' after the name); the
 * enemy's fleet wing by its own aircraft and fleet, 'a British carrier fighter of the Home Fleet', 'of Force H' (14:48Z).
 * 'adj' is the enemy's nation word ('' for one's own), dropped where the name already says it: 'a carrier fighter of
 * the Brazilian Fleet', 'a bomber of the Polish Navy's strike wing' (ruling 5).
 */
export function aCraftOf(name: string, noun: string, adj = '', how = ''): string {
  const w = adj ? FLEET_WING.exec(name) : null, said = adjFor(name, adj);
  if (w && w[2] !== 'strike wing') {
    const what = [how, said, w[2].replace(/s$/, '')].filter(Boolean).join(' ');
    return `${an(what)} ${what} of ${takesThe(w[1]) ? 'the ' : ''}${w[1]}`;
  }
  if (subjectOf(name).craft) return `${oneOf(name, said)}${how ? `, ${how},` : ''}`;
  const what = [how, said, noun].filter(Boolean).join(' ');
  return `${an(what)} ${what} of ${subjectOf(name).whom}`;
}

/**
 * Two aircraft of one group, named once with the count as a word (the UI director, 20 November 15:38Z), each form as
 * aCraftOf names one: 'two bombers of the 8th Bomb Group', 'two German fighters of JG 26', 'two of the Shokaku and
 * Zuikaku dive bombers', 'two British carrier fighters of the Home Fleet'.
 */
export function twoOf(name: string, noun: string, adj = ''): string {
  const w = adj ? FLEET_WING.exec(name) : null, said = adjFor(name, adj);
  if (w && w[2] !== 'strike wing') return `two ${[said, w[2]].filter(Boolean).join(' ')} of ${takesThe(w[1]) ? 'the ' : ''}${w[1]}`;
  if (subjectOf(name).craft) return `two${oneOf(name, said).slice(3)}`;
  return `two ${[said, noun === 'aircraft' ? noun : `${noun}s`].filter(Boolean).join(' ')} of ${subjectOf(name).whom}`;
}

/**
 * The nation word a line adds to a group's name, unless the name already says it ('Polish Navy', 'Royal Norwegian
 * Navy') or names another nation's ('US Ninth Air Force', whoever holds it): never two (ruling 5).
 */
export const adjFor = (name: string, adj: string): string => (!adj || /^US /.test(name) || new RegExp(`(^| )${adj.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}( |'|$)`).test(name) ? '' : adj);

export const upFirst = (s: string): string => s[0].toUpperCase() + s.slice(1);

/** 'a' or 'an' before a word: 'an Italian', 'a Uruguayan', 'a European'. */
export const an = (word: string): string => (/^[AEIOU]/i.test(word) && !/^(Eu|Uk|Uni|Uru|Uga|One)/i.test(word) ? 'an' : 'a');

/**
 * A line about a group, its verbs agreeing with the name: told(name, '{N} [is|are] in a fight.'); {n} where the name
 * falls mid-sentence, its "the" kept small: 'Warned by radar, {n} [climbs|climb] ...'.
 */
export function told(name: string, text: string): string {
  const s = subjectOf(name);
  return text.replace(/\{N\}/g, s.who).replace(/\{n\}/g, s.whom).replace(/\[([^|\]]*)\|([^\]]*)\]/g, (_, one: string, many: string) => (s.many ? many : one));
}
