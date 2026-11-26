// Builds data/battle/armour.json, what the ground battle's rules read of each tank's and gun's measured performance
// (release 2; Brian, 22 November 04:03Z to 04:16Z: the named tanks have real advantages from their measured
// performance, and German tanks are far weaker at the back). From the land vehicle modeller's sheet,
// performance-land.json (tools/battle-models/land/performance.py on its branch; the project's battle-models/land
// folder): the chance a hit comes through each face at each range, the share of those that put her out of the fight,
// how often one that comes through sets her burning and how often the fire reaches her ammunition.
//   node tools/build-armour.mjs /mnt/project-files/battle-models/land/performance-land.json
import { readFileSync, writeFileSync } from 'node:fs';

const src = process.argv[2] ?? '/mnt/project-files/battle-models/land/performance-land.json';
const d = JSON.parse(readFileSync(src, 'utf8'));
const r2 = (x) => Math.round(x * 100) / 100;
const faces = (o) => ({ front: o.front.map(r2), side: o.side.map(r2), rear: o.rear.map(r2) });
const models = {};
for (const [name, v] of Object.entries(d.vehicles)) {
  const gun = v.gun ? d.guns[v.gun] : null;
  models[name] = {
    kills: r2(v.kills_when_it_pierces ?? 0),
    ...(v.fire_when_pierced ? { fire: { front: r2(v.fire_when_pierced.front), side: r2(v.fire_when_pierced.side), rear: r2(v.fire_when_pierced.rear) } } : {}),
    ...(v.explodes_of_fires !== undefined ? { explodes: r2(v.explodes_of_fires) } : {}),
    ...(gun?.heat_from ? { heat: gun.heat_from } : {}),
    ...(v.radio !== undefined ? { radio: r2(v.radio) } : {}),
    sure: v.sure ?? 'design',
  };
}
const duels = {};
for (const [s, row] of Object.entries(d.duels.table)) {
  duels[s] = {};
  for (const [t, c] of Object.entries(row)) duels[s][t] = { ...faces(c), ...(c.before_hollow_charge ? { before: faces(c.before_hollow_charge) } : {}) };
}
const out = {
  _about: [
    "Measured armour and guns (release 2; Brian, 22 November 04:03Z to 04:16Z: 'the named tanks have real advantages from their measured performance against their peers; German tanks are far weaker at the back, so hits there pay'). Built by tools/build-armour.mjs from the land vehicle modeller's sheet (performance-land.json); the historian checks the figures (sure: high, medium, low, or design for our own inventions). Do not edit by hand: rebuild.",
    'ranges: metres. duels[shooter][target][face]: the chance a hit on that face comes through at each range (before: the same before the shooter had its hollow-charge round, from the month models[shooter].heat). models[name]: kills, the share of the hits that come through that put her out of the fight (the shooter\'s gun); fire, how often one that comes through sets her burning, by face; explodes, how often a fire reaches her ammunition; radio, the share of her kind with a set that talks (orders reach her the sooner).',
  ],
  source: src.split('/').slice(-1)[0],
  ranges: d.ranges_m,
  models,
  duels,
};
writeFileSync(new URL('../data/battle/armour.json', import.meta.url), JSON.stringify(out) + '\n');
console.log(`armour.json: ${Object.keys(models).length} models, ${Object.keys(duels).length} shooters`);
