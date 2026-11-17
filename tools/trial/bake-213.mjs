// TRIAL (far-side seats rig). Copies the rows of rulings 213 and 214 into data/campaign/trial-213.json, unchanged, so
// that the rig can play them before the rules builder's own levers exist:
//
//   landFactor  /mnt/project-files/map-tables/land-factor-214.json   (ruling 214: land and harbours pay, with the ground)
//   pool, class /mnt/project-files/historian/manpower-213.json       (ruling 213(b): a smaller power's men)
//   works       /mnt/project-files/historian/aircraft-works-213.json (ruling 213(d): who builds aircraft at home, and when)
//   dated       the same file's dated deliveries, kept for the record only: the rig does not play them
//
//   node tools/trial/bake-213.mjs          write the rows
//   node tools/trial/bake-213.mjs --off    write an empty table (the game as it stood before the rulings)
//   node tools/trial/bake-213.mjs --oil IRN=0.2   a question, not a ruling: also pay that country's oilfields at this share
//                                          (the director's watch on Iran, 16:57Z); leave it out for the rows as ruled
//
// Then build again: node tools/trial/build.mjs.
import { readFileSync, writeFileSync } from 'node:fs';
const off = process.argv.includes('--off');
const read = (p) => JSON.parse(readFileSync(p, 'utf8'));
const out = {
  _about: 'TRIAL. The rows of rulings 213 and 214, copied unchanged by tools/trial/bake-213.mjs from the clerk\'s and the historian\'s tables. Read by economy.ts (landFactor), manpower.ts (pool, class) and arsenal.ts (works, great). Not a data file of the game: the levers are the rules builder\'s to write.',
  on: !off,
  // The countries the works list does not speak for: they keep the two- and three-factory test.
  great: ['GER', 'ITA', 'UK', 'FRA', 'SOV', 'USA', 'VIC', 'JAP'],
  landFactor: {}, oilFactor: {}, pool: {}, class: {}, works: {}, dated: [],
};
if (!off) {
  out.landFactor = read('/mnt/project-files/map-tables/land-factor-214.json').landFactor;
  // Every block of rows in the file: 'rows' (16:48Z) and those added later, such as 'rows_17_15' (Iran, Iraq, Saudi Arabia, Nepal).
  const men = read('/mnt/project-files/historian/manpower-213.json');
  for (const key of Object.keys(men).filter((k) => k.startsWith('rows'))) for (const [nat, row] of Object.entries(men[key])) if (row && typeof row === 'object' && typeof row.pool === 'number') { out.pool[nat] = row.pool; out.class[nat] = row.class; }
  const air = read('/mnt/project-files/historian/aircraft-works-213.json');
  // The historian's form of 17:03Z: the month each works opens for fighters and for bombers (null: never); it then
  // stays open while the country stands. The first form gave [from, to] and is still read.
  const span = (from, both) => (both ? both : from === null || from === undefined ? null : [from, 9999]);
  for (const w of air.works) out.works[w.nat] = { ftr: span(w.fighters_from, w.fighters), bmr: span(w.bombers_from, w.bombers) };
  out.dated = air.dated.map((r) => r.slice(0, 3));
}
const oil = process.argv.indexOf('--oil');
if (oil >= 0) for (const pair of (process.argv[oil + 1] ?? '').split(',')) { const [nat, share] = pair.split('='); if (nat && share !== undefined) out.oilFactor[nat] = Number(share); }
writeFileSync(new URL('../../data/campaign/trial-213.json', import.meta.url), JSON.stringify(out, null, 1) + '\n');
console.log(off ? 'wrote data/campaign/trial-213.json: empty (rulings 213 and 214 off)' : `wrote data/campaign/trial-213.json: land factor for ${Object.keys(out.landFactor).length} countries, men for ${Object.keys(out.pool).length}, aircraft works for ${Object.keys(out.works).length}, ${out.dated.length} dated deliveries (not played)${Object.keys(out.oilFactor).length ? `; OIL QUESTION ON: ${JSON.stringify(out.oilFactor)}` : ''}`);
