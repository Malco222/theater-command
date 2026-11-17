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
  landFactor: {}, pool: {}, class: {}, works: {}, dated: [],
};
if (!off) {
  out.landFactor = read('/mnt/project-files/map-tables/land-factor-214.json').landFactor;
  for (const [nat, row] of Object.entries(read('/mnt/project-files/historian/manpower-213.json').rows)) { out.pool[nat] = row.pool; out.class[nat] = row.class; }
  const air = read('/mnt/project-files/historian/aircraft-works-213.json');
  for (const w of air.works) out.works[w.nat] = { ftr: w.fighters ?? null, bmr: w.bombers ?? null };
  out.dated = air.dated.map((r) => r.slice(0, 3));
}
writeFileSync(new URL('../../data/campaign/trial-213.json', import.meta.url), JSON.stringify(out, null, 1) + '\n');
console.log(off ? 'wrote data/campaign/trial-213.json: empty (rulings 213 and 214 off)' : `wrote data/campaign/trial-213.json: land factor for ${Object.keys(out.landFactor).length} countries, men for ${Object.keys(out.pool).length}, aircraft works for ${Object.keys(out.works).length}, ${out.dated.length} dated deliveries (not played)`);
