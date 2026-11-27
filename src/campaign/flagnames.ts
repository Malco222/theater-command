// (Speed, the far map: 24 November 2026.) A province's flag names, made once and kept. Several rules read a flag of
// every province at every nation's month (2,067 provinces and 64 nations on the far chart), and a name built afresh
// each time is a new string to make and to look up. These are the same names: `${prefix}${id}`, so "fw:" and 12 give
// "fw:12". The prefix carries its own colon. Nothing here reads or writes a flag.
const made = new Map<string, string[]>();

/** The flag name `${prefix}${id}`, made the first time it is asked for and kept. */
export function flagName(prefix: string, id: number): string {
  let names = made.get(prefix);
  if (!names) made.set(prefix, (names = []));
  return (names[id] ??= `${prefix}${id}`);
}
