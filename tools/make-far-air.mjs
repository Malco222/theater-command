// Writes data/campaign/far-air.json: the Pacific's land air groups and Japan's air industry (rulings 184 and 187).
//
// The Pacific planner writes the groups in the shared folder (pacific-planner/pacific-air-rows.json, with
// pacific-air-rows.md beside it): paper 10's section 7 as the historian corrected it, his rows for Britain, the
// Dutch, Australia, New Zealand and China, and paper 15's two Burma rows, in air.json's own fleet form. This script
// copies each row's nat, name, base, from, leader, est, home and moves, unchanged, into a seats file the game lays
// over air.json when the far side's switch is on (src/campaign/far-rows.ts), and adds what the game needs to read
// them (src/campaign/air.ts, FleetRow):
//
//   posted    every row: while the Pacific's book stands, a computer staff leaves the command on the record's field
//             and takes it forward by the record's moves (without it Japan's staff flies the southern commands to
//             the China front, where its main effort and the enemy's aircraft are).
//   ally      the row's base, or one of its forward bases, is another country's ground on the first day: the
//             command may stand on ground an ally holds (America's in Australia, New Zealand's on Fiji).
//   arrives   its country has no production row in air.json: what the record adds to the command arrives with the
//             date. (China, Australia, New Zealand, the Dutch.)
//   quality   the builder's starting values, named for the tuning pass and not tuned: America's Pacific commands
//             0.9 (paper 10, section 7), the air forces in Malaya and the Indies 0.75 (the historian).
//   bmrRange  America's bombers in the Pacific 1,200 km (ruling 184; its nation row, shared with Europe, is left
//             alone), and the two commands of long bombers 2,550.
//
// Japan's nation row is rulings 184 and 187's: quality 1.0, fighters 300 km before the step (the month's own reach
// is airwar.json's dated row, ruling 227), bombers 1,300. 'decline' is 187(b)'s table, read only at the stops of the
// history dial that keep the book. Japan's production and pilots are the planner's history figures cut to the scale
// of Europe's rows (187(c)): history's own until January 1942, then held at SCALE below (the builder's reading,
// named for the design director and not tuned), with a reserve row.
//
//   node tools/make-far-air.mjs            write the file
//   node tools/make-far-air.mjs --check    exit 1 if the file on disk is not what the source gives today
//
// The source is read from the shared folder (FAR_SOURCES, default /mnt/project-files). Nothing else writes the file.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const SRC = process.env.FAR_SOURCES ?? '/mnt/project-files';
const FILE = 'pacific-planner/pacific-air-rows.json';
const OUT = path.join(ROOT, 'data/campaign/far-air.json');
const read = (p) => JSON.parse(fs.readFileSync(p, 'utf8'));
const need = (ok, what) => { if (!ok) throw new Error(what); };

// ---- The builder's readings (named, not tuned) ------------------------------------------------------------------

/** Japan's nation row (rulings 184 and 187). */
const JAPAN = { quality: 1.0, ftrRange: 300, bmrRange: 1300 };
/**
 * Ruling 187(c): Europe's rows stop rising in mid 1941 (Germany's at 360 fighters a month) where history's went on
 * to 2,100 in 1944, and Japan built 1,150 fighters a month that year to Germany's 2,100. So Japan's row follows
 * history until it would pass 360 x 1,150 / 2,100 = 197 fighters a month, and stops there: from January 1942.
 * Bombers and pilots are cut by the same factor on that month's history row (200 bombers and 120 pilots at 245
 * fighters).
 */
const SCALE = { germany: 360, ratio: 1150 / 2100 };
/** Japan's reserve: between Britain's and the default row, as its air force is. */
const RESERVE = { ftr: 250, bmr: 250, pilots: 150 };
/** America's Pacific commands (paper 10, section 7: "America 0.9"), and its bombers' reach there (ruling 184). */
const USA = { quality: 0.9, bmrRange: 1200 };
/** The commands of long bombers (ruling 184: 2,550 km, from June 1944, as their rows are dated). */
const LONG = { 'XX Bomber Command': 2550, 'XXI Bomber Command': 2550 };
/** The historian: the air forces in Malaya and the Indies in 1941 and 1942, about 0.75. */
const WEAK = { 'Far East Command': 0.75, 'Army Air Arm of the Indies': 0.75 };

// ---- The source -------------------------------------------------------------------------------------------------

const src = read(path.join(SRC, FILE));
const chart = read(path.join(ROOT, 'src/generated/world-whole.json'));
const air = read(path.join(ROOT, 'data/campaign/air.json'));
const owner = new Map(chart.regions.map((r) => [r.name, r.owner]));
const count = new Map();
for (const r of chart.regions) count.set(r.name, (count.get(r.name) ?? 0) + 1);
const place = (name, what) => need(count.get(name) === 1, `${what}: ${count.get(name) ?? 0} provinces named "${name}" on the chart`);
const sched = (rows, what, width) => {
  need(Array.isArray(rows) && rows.length > 0, `${what}: no rows`);
  let last = 0;
  for (const r of rows) {
    need(Array.isArray(r) && r.length === width && r.every((x) => typeof x === 'number' && x >= 0) && Number.isInteger(r[0]) && r[0] > last, `${what}: ${JSON.stringify(r)}`);
    last = r[0];
  }
};

need(Array.isArray(src.fleets) && src.fleets.length > 0 && src.japan, `${FILE}: no fleets or no 'japan'`);
const seen = new Set(air.fleets.map((r) => `${r.nat}|${r.name}`));
const fleets = [];
for (const r of src.fleets) {
  const what = `${r.nat} "${r.name}"`;
  need(typeof r.nat === 'string' && typeof r.name === 'string' && r.name.length > 0, `a row with no nation or no name: ${JSON.stringify(r).slice(0, 80)}`);
  need(!seen.has(`${r.nat}|${r.name}`), `${what}: a row of that name stands already`);
  seen.add(`${r.nat}|${r.name}`);
  place(r.base, `${what} base`);
  sched(r.est, `${what} est`, 3);
  need(r.from === undefined || (Number.isInteger(r.from) && r.from > 1), `${what}: from ${r.from}`);
  need((r.from ?? 1) <= 1 ? r.est[0][0] === 1 : r.est[0][0] === r.from, `${what}: its first est row is not its first month`);
  const moves = r.moves ?? [];
  let last = r.from ?? 1;
  for (const m of moves) {
    need(Array.isArray(m) && m.length === 2 && Number.isInteger(m[0]) && m[0] > last, `${what} move ${JSON.stringify(m)}`);
    place(m[1], `${what} move`);
    last = m[0];
  }
  const out = { nat: r.nat, name: r.name, base: r.base, posted: true };
  if (r.from !== undefined) out.from = r.from;
  if (r.leader) out.leader = r.leader;
  out.est = r.est;
  if (r.home) out.home = true;
  if (moves.length) out.moves = moves;
  if ([r.base, ...moves.map((m) => m[1])].some((p) => owner.get(p) !== r.nat)) out.ally = true;
  if (r.nat !== 'JAP' && !air.production[r.nat]) out.arrives = true;
  const quality = WEAK[r.name] ?? (r.nat === 'USA' ? USA.quality : undefined);
  if (quality !== undefined) out.quality = quality;
  const bmrRange = LONG[r.name] ?? (r.nat === 'USA' ? USA.bmrRange : undefined);
  if (bmrRange !== undefined) out.bmrRange = bmrRange;
  fleets.push(out);
}
for (const k of [...Object.keys(WEAK), ...Object.keys(LONG)]) need(fleets.some((r) => r.name === k), `the builder's readings name "${k}", which the source no longer has`);

const J = src.japan;
sched(J.production_history, 'japan production_history', 3);
sched(J.pilots_history, 'japan pilots_history', 2);
sched(J.quality, 'japan quality', 2);
need(J.production_history.length === J.pilots_history.length && J.production_history.every((r, i) => r[0] === J.pilots_history[i][0]), 'japan: production and pilots are not dated alike');
const cap = Math.round(SCALE.germany * SCALE.ratio);
const production = [], pilots = [];
for (let i = 0; i < J.production_history.length; i++) {
  const [turn, ftr, bmr] = J.production_history[i], men = J.pilots_history[i][1];
  if (ftr <= cap) { production.push([turn, ftr, bmr]); pilots.push([turn, men]); continue; }
  const k = cap / ftr;
  production.push([turn, cap, Math.round((bmr * k) / 10) * 10]);
  pilots.push([turn, Math.round((men * k) / 10) * 10]);
  break;
}
// (A pilots row that repeats the one before it says nothing.)
const trained = pilots.filter((r, i) => i === 0 || r[1] !== pilots[i - 1][1]);

const out = {
  _about: "The Pacific's land air groups and Japan's air industry (rulings 184 and 187), written by tools/make-far-air.mjs from the Pacific planner's pacific-air-rows.json and never by hand; the game lays it over air.json only where the far side's switch is on (src/campaign/far-rows.ts). The rows are the historian's: who, where, how many by date. The script's own marks are 'posted' (while the Pacific's book stands a computer staff leaves the command on the record's field and takes it forward by the record's moves), 'ally' (the command may stand on ground an ally holds), 'arrives' (its country has no production row: what the record adds arrives with the date), 'quality' and 'bmrRange' (this command's own, in place of its nation's: America's Pacific commands 0.9 and 1,200 km, the long bombers 2,550 km, Malaya's and the Indies' air 0.75), and it cuts Japan's production and pilots to the scale of Europe's rows (187(c)). 'moves' are the record's forward bases, [month, province]: a computer staff takes each once, while the Pacific's book stands, if its side holds the ground. 'decline' is Japan's quality by date (187(b)), read at those stops only. A command whose est comes to nothing was withdrawn: it is stood down that month. The quality figures, the production cut and the reserve are the builder's readings, named for the design director and not tuned.",
  _source: { [FILE]: fs.statSync(path.join(SRC, FILE)).mtime.toISOString().slice(0, 16) + 'Z' },
  air: {
    nations: { JAP: JAPAN },
    production: { JAP: production },
    pilots: { perMonth: { JAP: trained } },
    reserve: { JAP: RESERVE },
    decline: { JAP: J.quality },
    fleets,
  },
};

// One row to a line, as air.json is written.
const line = (v) => JSON.stringify(v).replace(/,"/g, ', "').replace(/":/g, '": ');
const text = `{
  "_about": ${JSON.stringify(out._about)},
  "_source": ${line(out._source)},
  "air": {
    "nations": ${line(out.air.nations)},
    "production": ${line(out.air.production)},
    "pilots": ${line(out.air.pilots)},
    "reserve": ${line(out.air.reserve)},
    "decline": ${line(out.air.decline)},
    "fleets": [
${fleets.map((r) => `      ${line(r)}`).join(',\n')}
    ]
  }
}
`;
need(JSON.stringify(JSON.parse(text)) === JSON.stringify(out), 'the file as written does not read back as it was made');

if (process.argv.includes('--check')) {
  const strip = (s) => s.replace(/"_source": .*\n/, '');
  const on = fs.existsSync(OUT) ? fs.readFileSync(OUT, 'utf8') : '';
  if (strip(on) !== strip(text)) { console.error('data/campaign/far-air.json is not what the planner\'s rows give today: run node tools/make-far-air.mjs'); process.exit(1); }
  console.log('far-air.json is up to date');
} else {
  fs.writeFileSync(OUT, text);
  const by = {};
  for (const r of fleets) by[r.nat] = (by[r.nat] ?? 0) + 1;
  console.log(`far-air.json: ${fleets.length} commands (${Object.entries(by).map(([k, v]) => `${k} ${v}`).join(', ')}); Japan builds ${JSON.stringify(production)}, trains ${JSON.stringify(trained)}`);
}
