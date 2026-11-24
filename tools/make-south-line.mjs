// Writes data/campaign/far-south-line.json: the record's line in the south (ruling 234(a)).
//
// The Pacific planner drafts the line in the shared folder (pacific-planner/south-line.json, with south-line.md
// beside it; the historian checks its months): for each province and city of today's chart that Japan's side took
// from December 1941, the months it held it ('line', in the form src/campaign/line.ts reads); the month the Allied
// staffs may first come back to each ('back'); and the edge that stays shut, by group ('shut'). This script copies
// those rows, unchanged, into a seats file the game lays over history.json when the far side's switch is on
// (src/campaign/far-rows.ts), and adds the four words the game needs to read a line drawn on five countries'
// ground at once (line.ts): 'ground' (the Pacific's ground and the provinces named), 'stays' (a span's end sends
// nobody home), 'field' (the floor reads who holds the field), 'slides' (the calendar moves with Japan's strike south) and 'land' left empty
// beside 'lands'.
//
//   node tools/make-south-line.mjs            write the file
//   node tools/make-south-line.mjs --check    exit 1 if the file on disk is not what the source gives today
//
// The source is read from the shared folder (FAR_SOURCES, default /mnt/project-files). Nothing else writes the file.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const SRC = process.env.FAR_SOURCES ?? '/mnt/project-files';
const FILE = 'pacific-planner/south-line.json';
const OUT = path.join(ROOT, 'data/campaign/far-south-line.json');
const read = (p) => JSON.parse(fs.readFileSync(p, 'utf8'));
const need = (ok, what) => { if (!ok) throw new Error(what); };

const src = read(path.join(SRC, FILE));
const chart = read(path.join(ROOT, 'src/generated/world-whole.json'));
const owner = new Map(chart.regions.map((r) => [r.name, r.owner]));
const count = new Map();
for (const r of chart.regions) count.set(r.name, (count.get(r.name) ?? 0) + 1);

const L = src.line;
need(L && L.id === 'south' && Array.isArray(L.lands) && L.lands.length > 0 && L.taker && L.spans && L.floor, 'south-line.json: no line in line.ts\'s form');
for (const [name, spans] of Object.entries(L.spans)) {
  need(count.get(name) === 1, `span "${name}": ${count.get(name) ?? 0} provinces of that name on the chart`);
  need(L.lands.includes(owner.get(name)), `span "${name}": its owner ${owner.get(name)} is not one of the line's lands`);
  for (const s of spans) need(Number.isInteger(s.from) && Number.isInteger(s.to) && s.from <= s.to && typeof s.kind === 'string', `span "${name}": ${JSON.stringify(s)}`);
}
const back = src.back ?? {};
for (const [name, month] of Object.entries(back)) {
  need(L.spans[name], `back "${name}": the line has no span for it`);
  need(Number.isInteger(month), `back "${name}": ${month}`);
}
const shut = Object.values(src.shut ?? {}).flat().sort();
// (A province named under 'shut' is bound whoever owns it: New Caledonia is France's, and France is not one of the
// line's lands, or the line would be drawn on Indochina, which Japan holds by the demand.)
const lands = [...L.lands];
for (const name of shut) {
  need(count.get(name) === 1, `shut "${name}": ${count.get(name) ?? 0} provinces of that name on the chart`);
  need(!L.spans[name], `shut "${name}" has a span too`);
}

const out = {
  _about: "The record's line in the south (ruling 234(a)), written by tools/make-south-line.mjs from the Pacific planner's south-line.json and never by hand; the game lays it over history.json's 'lines' only where the far side's switch is on (src/campaign/far-rows.ts) and reads it in src/campaign/line.ts. At the stops that keep the book, while the Pacific's book stands, computer against computer: Japan's side neither attacks, enters nor lands on a province of the five 'lands' before a span's first month or after its last, nor ever on one no span names (the ceiling); and the Allied computer staffs do not retake a province a span of the floor's kinds covers before its own month under 'back', or before the floor's last month where it has none (the floor). 'ground': the line is drawn only on the Pacific's ground (pacific.json 'shores') and on the provinces named here, in a span or under 'shut'. 'stays': when a span ends Japan's corps keep what they hold. 'field': the floor also covers a province where Japan's army holds the field while the town holds out. 'slides': at a stop with slack the whole calendar moves with the month of Japan's strike south. Kinds: 'opening' (December 1941 to August 1942), 'tried' (Port Moresby and Midway: open to attack, no floor), 'release' (the march on India of March 1944), 'later'.",
  _source: { [FILE]: fs.statSync(path.join(SRC, FILE)).mtime.toISOString().slice(0, 16) + 'Z' },
  history: {
    lines: [{ id: L.id, theatre: L.theatre, land: '', lands, taker: L.taker, until: L.until, floor: L.floor, ground: 'pacific', stays: true, field: true, slides: 'JAP:south', back, shut, spans: L.spans }],
  },
};

const text = JSON.stringify(out, null, 1) + '\n';
if (process.argv.includes('--check')) {
  const on = fs.existsSync(OUT) ? fs.readFileSync(OUT, 'utf8') : '';
  const rows = (t) => { const j = JSON.parse(t || '{}'); delete j._source; return JSON.stringify(j); };
  if (rows(on) !== rows(text)) { console.error('far-south-line.json is not what the source gives today: run node tools/make-south-line.mjs'); process.exit(1); }
  console.log('far-south-line.json is what the source gives today');
} else {
  fs.writeFileSync(OUT, text);
  console.log(`wrote ${path.relative(ROOT, OUT)}: ${Object.keys(L.spans).length} provinces with spans, ${Object.keys(back).length} with a month back, ${shut.length} named as shut, lands ${lands.join(' ')}`);
}
