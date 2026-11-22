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
//   arrives   China, New Zealand and the Dutch build no aircraft: what the record adds to a command of theirs
//             arrives with the date (ruling 234's rider b518, 7). Australia builds: 'works' below.
//   quality   America's Pacific commands 0.9 to start (ruling 184; 'rise' below says until when), the American
//             Volunteer Group 1.0 (b518, 2: a row, not a country; b525: the historian's figure), the air forces in
//             Malaya and the Indies 0.75 (the historian).
//   bmrRange  America's bombers in the Pacific 1,200 km (ruling 184; its nation row, shared with Europe, is left
//             alone), and the two commands of long bombers 2,550.
//   byBook    b518, 6: a fall in the row's figure, to nothing too, is the record's own move or a battle's work. It is
//             taken at the stops that keep the book; at the others the command is kept at the most the record has
//             given it so far. Every row but the Soviet Union's (the historian checked the list: b525).
//   to        b525: the record ends the command by a change of name, and these rows took its place (HANDS below).
//             b534: where the record moves such a command before its successor stands, the move is laid too (GOES).
//             It is not stood down by the date: it ends when every one of them stands and passes them what it
//             holds, at every stop. Nothing stands twice, and nothing vanishes because a date came.
//   leaves    b525: a part of the command that leaves for a reason outside the game (LEAVES below): with the date
//             at every stop, never through the pool; it stays only where no book is kept and the reason is gone.
//   watch     the Soviet Far East's commands stand apart, watching Japan: at peace with it a computer staff leaves
//             them on their fields at every stop, and at every stop what the record adds arrives and what it takes
//             away leaves with the date, never through the pool (the aircraft that went west in December 1941 are
//             on Europe's rows already: b518). The builder's reading, named for the design director.
//
// Beside the rows: 'rise' (b518, 2: America's Pacific commands read the nation's quality from January 1943 at the
// stops that keep the book, and at the others from the day America holds Long-range fighters), 'jobs' (187(a) and
// b518, 3: from a field beyond Europe's chart America's land fighters reach 550 km over a beach, on patrol and over a
// sea, and 1,250 flying with the bombers on a raid from March 1944) and 'works' (b518, 7: Australia's own works keep
// up its commands).
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
/**
 * Rider b525 (the historian's check of 187(c)): the bombers are scaled nation against nation as the fighters are, not
 * by Japan's own mix: Japan built about 200 bombers a month in 1942 to Germany's 360, 0.55 again, and on Germany's
 * bomber row of 350 that is 190 a month from the same month. The pilots keep the fighters' factor.
 */
const BOMBERS = { germany: 350, ratio: 0.55 };
/** Japan's reserve: between Britain's and the default row, as its air force is. */
const RESERVE = { ftr: 250, bmr: 250, pilots: 150 };
/** America's Pacific commands (paper 10, section 7: "America 0.9"), and its bombers' reach there (ruling 184). */
const USA = { quality: 0.9, bmrRange: 1200 };
/** The commands of long bombers (ruling 184: 2,550 km, from June 1944, as their rows are dated). */
const LONG = { 'XX Bomber Command': 2550, 'XXI Bomber Command': 2550 };
/** The historian: the air forces in Malaya and the Indies in 1941 and 1942, about 0.75. b518, 2 and b525: the volunteer group is a row, not a country; it outfought Japan's army air at several to one and reads 1.0. */
const WEAK = { 'Far East Command': 0.75, 'Army Air Arm of the Indies': 0.75, 'American Volunteer Group': 1.0 };
/** b518, 2: America's 0.9 is "to start". The month is January 1943; the research is tech.json's Long-range fighters. */
const RISE = { USA: { from: 41, tech: 'escorts' } };
/** Ruling 187(a) as b518, 3 has it built: cover over a beach, patrol and the air over a sea 550 km; escorts on a raid 1,250 from March 1944. */
const JOBS = { USA: { cover: 550, raid: [[55, 1250]] } };
/** b518, 7: who builds none (their additions arrive by date), and who builds (arsenal.json smallAir.works keeps its commands up). */
const ARRIVES = ['CHN', 'NZL', 'NED'];
const WORKS = ['AUS'];
/** The commands that stand apart, and the neighbour they watch (the builder's reading). */
const WATCH = { SOV: 'JAP' };
/**
 * Rider b525 (the historian's check of the builder's list, pacific-six/air-withdrawals-b518-historian.md): the
 * record's changes of name. The old command is not stood down by the date: it ends when every command that took its
 * place stands, and passes them what it holds, at every stop (air.ts handOver). Every other fall on these rows is a
 * move a staff can make or a battle's work ('byBook').
 */
const HANDS = {
  '11th Air Fleet (21st and 23rd Air Flotillas)': ['23rd Air Flotilla', '25th Air Flotilla'],
  'American Volunteer Group': ['China Air Task Force (Fourteenth Air Force from March 1943)'],
  '19th Bomb Group (Del Monte)': ['Fifth Air Force'],
  '49th Fighter Group (Darwin)': ['Fifth Air Force'],
  'XX Bomber Command': ['XXI Bomber Command'],
};
/**
 * Rider b525: a part of a command that leaves for a reason outside the game, [month, fighters, bombers, who, foe]:
 * with the date at every stop, unless no book is kept and `who` is at war with `foe` that month. The Soviet
 * volunteers left China after Moscow's pact with Tokyo of April 1941 (the Chinese Air Force's row carries their 50
 * fighters and 50 bombers until then).
 */
const LEAVES = { 'Chinese Air Force': [[20, 50, 50, 'SOV', 'JAP']] };
/**
 * Rider b534(3): where the record moves an old command before its successor stands, it moves by date at the stops
 * that keep the book and hands over there. The historian's pass over the list (hand-over-places-b534-historian.md):
 * - The 19th Bomb Group's B-17s stopped at Batchelor Field for about ten days in December 1941, and from about 30
 *   December fought from Singosari, by Malang in east Java (inside the chart's Soerabaja), until 1 to 3 March 1942.
 *   So the row stands at Soerabaja from January 1942 until the Fifth Air Force forms.
 * - XX Bomber Command flew its last mission from Chengtu on 15 January 1945: from February its 50 stand at Kharagpur,
 *   until the XXI takes them in April.
 * (Read only where the source gives the row no moves of its own: once the planner's file carries one, his stands.)
 */
const GOES = { '19th Bomb Group (Del Monte)': [[29, 'Soerabaja']], 'XX Bomber Command': [[66, 'Kharagpur']] };
/**
 * The design director's ruling of 22:05Z, 21 November 2026, on America's air rows in the Philippines (the historian's
 * count, historian/luzon-1941-42-philippine-army-feaf.md section 4b: counted as Japan's Formosa rows are, on hand in
 * the groups, older types included). In month 28, 8 December 1941:
 * - the Far East Air Force on Luzon is 120 fighters (91 P-40s and the P-35As) and 40 bombers (19 B-17s at Clark, 16
 *   B-18s, a few A-27s), where the planner's row reads 100 and 19;
 * - Del Monte's 16 B-17s and 2 B-18s are 18 bombers on Mindanao, a row of their own, sent south out of Formosa's
 *   reach, where the planner's row reads 16;
 * - the Philippine Army Air Corps is a row of its own, the 16 P-26As of its 6th Pursuit Squadron, not in the Far East
 *   Air Force's. Its fields (Zablan, Batangas) are in the chart's Manila: the builder's reading, named for the
 *   director. Its end is not laid, as Wake's squadron's is not: the fall of its field ends it.
 * The month-29 fall to 10 stays the book's at the stops that keep it; where no book is kept the row is kept at its
 * new most, as `byBook` reads. (RULED replaces one month's figures on a row of the source, and is read only while the
 * source still gives the figures named in `was`: once the planner's file carries the ruling, his stands. ADDS are
 * rows the source has not: once it has one of that name, his stands.)
 */
const RULED = {
  'Far East Air Force': { month: 28, was: [100, 19], is: [120, 40] },
  '19th Bomb Group (Del Monte)': { month: 28, was: [0, 16], is: [0, 18] },
};
const ADDS = [{ nat: 'USA', name: 'Philippine Army Air Corps (6th Pursuit Squadron)', base: 'Manila', from: 28, est: [[28, 16, 0]] }];

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
const ruled = (r) => {
  const R = RULED[r.name];
  if (!R) return r;
  const i = r.est.findIndex((x) => x[0] === R.month);
  need(i >= 0, `"${r.name}": the ruling names month ${R.month}, which its row does not have`);
  if (r.est[i][1] === R.is[0] && r.est[i][2] === R.is[1]) return r; // the planner's file carries the ruling
  need(r.est[i][1] === R.was[0] && r.est[i][2] === R.was[1], `"${r.name}": month ${R.month} reads ${r.est[i].slice(1)}, neither the figures the ruling replaced nor its own`);
  return { ...r, est: r.est.map((x, j) => (j === i ? [x[0], ...R.is] : x)) };
};
const rowsIn = [...src.fleets.map(ruled), ...ADDS.filter((x) => !src.fleets.some((r) => r.nat === x.nat && r.name === x.name))];
for (const r of rowsIn) {
  const what = `${r.nat} "${r.name}"`;
  need(typeof r.nat === 'string' && typeof r.name === 'string' && r.name.length > 0, `a row with no nation or no name: ${JSON.stringify(r).slice(0, 80)}`);
  need(!seen.has(`${r.nat}|${r.name}`), `${what}: a row of that name stands already`);
  seen.add(`${r.nat}|${r.name}`);
  place(r.base, `${what} base`);
  sched(r.est, `${what} est`, 3);
  need(r.from === undefined || (Number.isInteger(r.from) && r.from > 1), `${what}: from ${r.from}`);
  need((r.from ?? 1) <= 1 ? r.est[0][0] === 1 : r.est[0][0] === r.from, `${what}: its first est row is not its first month`);
  const moves = r.moves ?? GOES[r.name] ?? [];
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
  if (ARRIVES.includes(r.nat)) out.arrives = true;
  if (WATCH[r.nat]) out.watch = WATCH[r.nat];
  else out.byBook = true;
  if (HANDS[r.name]) out.to = HANDS[r.name];
  if (LEAVES[r.name]) out.leaves = LEAVES[r.name];
  const quality = WEAK[r.name] ?? (r.nat === 'USA' ? USA.quality : undefined);
  if (quality !== undefined) out.quality = quality;
  const bmrRange = LONG[r.name] ?? (r.nat === 'USA' ? USA.bmrRange : undefined);
  if (bmrRange !== undefined) out.bmrRange = bmrRange;
  fleets.push(out);
}
for (const k of [...Object.keys(WEAK), ...Object.keys(LONG), ...Object.keys(HANDS), ...Object.values(HANDS).flat(), ...Object.keys(LEAVES), ...Object.keys(GOES), ...Object.keys(RULED)]) need(fleets.some((r) => r.name === k), `the builder's readings name "${k}", which the source no longer has`);
// A command that hands over ends by the record (its last row reads nothing), and to its own side; what leaves a command is on its row as a fall of at least as much that month.
for (const [k, to] of Object.entries(HANDS)) {
  const r = fleets.find((x) => x.name === k), last = r.est[r.est.length - 1];
  need(last[1] + last[2] === 0, `"${k}" hands over, but the record does not end it`);
  for (const t of to) need(t !== k && !HANDS[t], `"${k}" hands over to "${t}", which hands over itself`);
}
for (const [k, rows] of Object.entries(LEAVES)) {
  const r = fleets.find((x) => x.name === k);
  for (const L of rows) {
    const i = r.est.findIndex((x) => x[0] === L[0]);
    need(i > 0 && r.est[i - 1][1] - r.est[i][1] >= L[1] && r.est[i - 1][2] - r.est[i][2] >= L[2], `"${k}": what leaves in month ${L[0]} is not a fall on its row that month`);
  }
}
// Every country of these rows is fed somehow: its own production row (air.json's, or Japan's here), its works, or the date.
const tech = read(path.join(ROOT, 'data/campaign/tech.json'));
const arsenal = read(path.join(ROOT, 'data/campaign/arsenal.json'));
for (const nat of new Set(fleets.map((r) => r.nat))) need(nat === 'JAP' || air.production[nat] || ARRIVES.includes(nat) || WORKS.includes(nat), `${nat}: no production row, no works and no arrivals`);
for (const nat of WORKS) need(arsenal.smallAir.works[nat], `${nat}: no works in arsenal.json`);
for (const [nat, r] of Object.entries(RISE)) need(JSON.stringify(tech).includes(`"id":"${r.tech}"`) || JSON.stringify(tech).includes(`"id": "${r.tech}"`), `${nat}: no research "${r.tech}" in tech.json`);

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
  production.push([turn, cap, Math.round((BOMBERS.germany * BOMBERS.ratio) / 10) * 10]);
  pilots.push([turn, Math.round((men * k) / 10) * 10]);
  break;
}
// (A pilots row that repeats the one before it says nothing.)
const trained = pilots.filter((r, i) => i === 0 || r[1] !== pilots[i - 1][1]);

const out = {
  _about: "The Pacific's land air groups and Japan's air industry (rulings 184 and 187, with ruling 234's riders b518, b522 and b525), written by tools/make-far-air.mjs from the Pacific planner's pacific-air-rows.json and never by hand; the game lays it over air.json only where the far side's switch is on (src/campaign/far-rows.ts). The rows are the historian's: who, where, how many by date. The script's own marks on a row are 'posted' (while the Pacific's book stands a computer staff leaves the command on the record's field and takes it forward by the record's moves), 'ally' (the command may stand on ground an ally holds), 'arrives' (China, New Zealand and the Dutch build no aircraft: what the record adds arrives with the date), 'byBook' (a fall in the row's figure, to nothing too, is the record's own move or a battle's work: taken at the stops that keep the book, and at the others the command is kept at the most the record has given it so far), 'to' (the record ends the command by a change of name and these rows took its place: it is not stood down by the date but ends when every one of them stands, and passes them what it holds, at every stop), 'leaves' ([month, fighters, bombers, who, foe]: a part of the command that leaves for a reason outside the game, with the date at every stop and never through the pool, unless no book is kept and `who` is at war with `foe` that month), 'watch' (the Soviet Far East's commands stand apart, watching Japan: at peace with it a computer staff leaves them on their fields at every stop, and what the record adds or takes away comes and goes with the date, never through the pool), 'quality' and 'bmrRange' (this command's own, in place of its nation's: America's Pacific commands 0.9 and 1,200 km, the volunteer group 1.0, the long bombers 2,550 km, Malaya's and the Indies' air 0.75). 'moves' are the record's forward bases, [month, province]: a computer staff takes each once, while the Pacific's book stands, if its side holds the ground. Beside the rows: Japan's production and pilots cut to the scale of Europe's rows (187(c); the bombers nation against nation, 190 a month, rider b525); 'decline', Japan's quality by date (187(b)), read at the stops that keep the book only; 'rise', until when a nation's commands keep a quality of their own (America's 0.9 is to start: the nation's from the month at a stop that keeps the book, from the research at the others); 'jobs', how far a nation's land fighters reach from a field beyond Europe's chart by what they are doing (187(a): 'cover' over a beach, on patrol and over a sea; 'raid' [month, km] with the bombers against industry, railways or a town); 'works', the smaller powers whose own works (arsenal.json smallAir.works) keep up their commands. A command whose figure comes to nothing is stood down that month. Japan's production cut and reserve and the Soviet reading are the builder's, named for the design director and not tuned; the hand-overs and what leaves are the historian's check of the builder's list (rider b525).",
  _source: { [FILE]: fs.statSync(path.join(SRC, FILE)).mtime.toISOString().slice(0, 16) + 'Z' },
  air: {
    nations: { JAP: JAPAN },
    production: { JAP: production },
    pilots: { perMonth: { JAP: trained } },
    reserve: { JAP: RESERVE },
    decline: { JAP: J.quality },
    rise: RISE,
    jobs: JOBS,
    works: WORKS,
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
    "rise": ${line(out.air.rise)},
    "jobs": ${line(out.air.jobs)},
    "works": ${line(out.air.works)},
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
