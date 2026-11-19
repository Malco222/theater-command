// The names of the air battle's groups in its dispatches (the UI director's words): the subject of a line and its verb,
// one aircraft of a group, and 'a' or 'an'. Shared by the battle (sim.ts) and the words for a fall (falls.ts).

/**
 * A group's name as the subject of a dispatch, and whether it takes a plural verb (the UI director, air review 10):
 * "Jagdgeschwader 4 is in a fight", but "The Home Fleet's carrier fighters are in a fight", and so the campaign's
 * carrier bombers, "The Home Fleet's strike wing is in a fight". A fleet called by its letter takes no article
 * ("Force H's strike wing"). 'whom' is the name inside a sentence.
 */
export function subjectOf(name: string): { who: string; whom: string; many: boolean } {
  const m = /^(.+?) (carrier [a-z]+s|strike wing)$/.exec(name);
  if (m) {
    const whom = `${/(^| )Force [A-Z]$/.test(m[1]) ? '' : 'the '}${m[1]}'s ${m[2]}`;
    return { who: whom[0].toUpperCase() + whom.slice(1), whom, many: m[2] !== 'strike wing' };
  }
  return { who: name, whom: name, many: /(^| )[a-z]+s$/.test(name) };
}

/**
 * One aircraft of a group named for its aircraft (subjectOf's plural: 'Shokaku and Zuikaku dive bombers', as the
 * Pearl Harbor set piece names its boxes): 'one of the Shokaku and Zuikaku dive bombers', never 'a bomber of ...
 * dive bombers'; 'adj' (the enemy's nation) after the article: 'one of the Japanese Shokaku and Zuikaku dive bombers'.
 */
export function oneOf(name: string, adj = ''): string {
  const whom = subjectOf(name).whom, the = whom.startsWith('the ') || !whom.includes("'s ");
  return `one of ${[the ? 'the' : '', adj, whom.replace(/^the /, '')].filter(Boolean).join(' ')}`;
}

export const upFirst = (s: string): string => s[0].toUpperCase() + s.slice(1);

/** 'a' or 'an' before a word: 'an Italian', 'a Uruguayan', 'a European'. */
export const an = (word: string): string => (/^[AEIOU]/i.test(word) && !/^(Eu|Uk|Uni|Uru|Uga|One)/i.test(word) ? 'an' : 'a');

/** A line about a group, its verbs agreeing with the name: told(name, '{N} [is|are] in a fight.'). */
export function told(name: string, text: string): string {
  const s = subjectOf(name);
  return text.replace(/\{N\}/g, s.who).replace(/\[([^|\]]*)\|([^\]]*)\]/g, (_, one: string, many: string) => (s.many ? many : one));
}
