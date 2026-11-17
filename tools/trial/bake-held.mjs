// TRIAL (far-side seats rig). Bakes the first-day holders into data/campaign/trial-far.json.
//
// The chart's provinces carry only the sovereign ('owner'). Who HOLDS a province on 1 September 1939 is in the
// clerk's tables, column 'held_1939', by province name (data/campaign/tables/*.csv; the same files as
// /mnt/project-files/map-tables/). This script writes every province whose holder differs from its owner into
// the 'held' table of trial-far.json, which world.ts reads at attach. The rest of that file (capitals, drop,
// wars) is kept as it stands. Run it again whenever the tables or the chart change:
//
//   node tools/trial/bake-held.mjs [--tables=DIR] [--chart=src/generated/world-cities.json]
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const arg = (k, d) => (process.argv.find((a) => a.startsWith(`--${k}=`)) ?? `--${k}=${d}`).split('=').slice(1).join('=');
const TABLES = path.resolve(ROOT, arg('tables', 'data/campaign/tables'));
const CHART = path.resolve(ROOT, arg('chart', 'src/generated/world-cities.json'));
const OUT = path.join(ROOT, 'data/campaign/trial-far.json');
const FILES = ['pacific.csv', 'islands-draft.csv', 'globe.csv', 'cities.csv', 'cities-parents.csv'];

/** A CSV file as rows of cells: quoted cells may hold commas, line breaks and doubled quotes. */
export function csv(text) {
  const rows = [];
  let row = [], cell = '', q = false;
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (q) {
      if (ch === '"' && text[i + 1] === '"') { cell += '"'; i++; } else if (ch === '"') q = false; else cell += ch;
    } else if (ch === '"') q = true;
    else if (ch === ',') { row.push(cell); cell = ''; } else if (ch === '\n') { row.push(cell); rows.push(row); row = []; cell = ''; } else if (ch !== '\r') cell += ch;
  }
  if (cell !== '' || row.length) { row.push(cell); rows.push(row); }
  return rows;
}

const chart = JSON.parse(fs.readFileSync(CHART, 'utf8'));
const owner = new Map(chart.regions.map((r) => [r.name, r.owner]));
const held = {};
const notes = [];
for (const f of FILES) {
  const p = path.join(TABLES, f);
  if (!fs.existsSync(p)) { notes.push(`${f}: not found in ${TABLES}`); continue; }
  const [head, ...rows] = csv(fs.readFileSync(p, 'utf8'));
  const iName = head.indexOf('name'), iHeld = head.indexOf('held_1939'), iOwner = head.indexOf('owner');
  let n = 0;
  for (const r of rows) {
    const name = r[iName], by = r[iHeld];
    if (!name) continue;
    if (!owner.has(name)) { notes.push(`${f}: "${name}" is not a province of the chart`); continue; }
    if (r[iOwner] && r[iOwner] !== owner.get(name)) notes.push(`${f}: "${name}" is owned by ${r[iOwner]} in the table and ${owner.get(name)} on the chart`);
    if (by && by !== owner.get(name)) { if (held[name] && held[name] !== by) notes.push(`${f}: "${name}" held by ${by} here and ${held[name]} in an earlier file`); held[name] = by; n++; }
  }
  notes.push(`${f}: ${rows.length} rows, ${n} held by another than the owner`);
}
const was = fs.existsSync(OUT) ? JSON.parse(fs.readFileSync(OUT, 'utf8')) : {};
const out = {
  _about: was._about ?? "TRIAL (far-side seats rig, never for main). What the rig lays over the wider chart on the first day, read by src/campaign/world.ts at attach. 'held': a province held on 1 September 1939 by another than its owner (baked from the clerk's held_1939 column by tools/trial/bake-held.mjs). 'capitals': a capital that moves once the whole homeland is on the chart. 'drop': first-day corps rows a seats file replaces, [nation, name]. 'wars': wars standing on the first day. Every seats file's 'setup' block is laid over this file when the rig is built (tools/trial/seats.mjs).",
  held: Object.fromEntries(Object.entries(held).sort(([a], [b]) => (a < b ? -1 : 1))),
  capitals: was.capitals ?? { IRN: 'Tehran', IRQ: 'Baghdad' },
  drop: was.drop ?? [],
  wars: was.wars ?? [],
};
fs.writeFileSync(OUT, JSON.stringify(out, null, 1) + '\n');
const by = {};
for (const [name, n] of Object.entries(held)) (by[`${n} in ${owner.get(name)}'s`] ??= []).push(name);
for (const l of notes) console.log(l);
for (const [k, v] of Object.entries(by)) console.log(`${k}: ${v.length}`);
console.log(`wrote ${path.relative(ROOT, OUT)}: ${Object.keys(held).length} provinces held by another than their owner`);
