// Writes data/campaign/far-world.json: the far side's rows that are nobody's seat.
//
// The two seat builders wrote the fourteen seats (far-seats-globe.json, far-seats-pacific.json). Everything else the
// far side needs on the first day is drafted in the shared folders by the Pacific planner, the historian and the
// clerk, each in the form of the data file it belongs in. This script copies those rows, unchanged, into one seats
// file the game lays over its data when the far side's switch is on (src/campaign/far-rows.ts):
//
//   Japan, the seventh great power: its nation row, its army of September 1939 in Japan, Korea, Manchukuo and
//     China, its nine fleets, its generals and officers' names, its rows by date to 1944
//   the old powers' garrisons out there on the first day and by date (Britain, the Netherlands, France, the Soviet
//     Far East, and America's in Hawaii, Alaska and the Philippines)
//   America's fleets in the Pacific, with their ships by date, their building and their bases by date, and the
//     bases by date of Japan's battle line (the planner's pacific-fleets.json)
//   the men of rulings 213(b), 216(b) and 218 (pool and class), and China's last levy
//   who holds what on 1 September 1939 where that is not the owner (the clerk's held_1939)
//   Japan's character as a staff: where its budget leans (paper 14, ruled with 181 to 202; the planner's base-seats.json)
//   the far side's shipyards (ruling b445; the planner's far-yards.md, the list between its ```json marks)
//   the far side's oilfields and the Soviet home row (rulings 194 and 231; the planner's far-oil.md, the same way)
//   two towns of the old chart that share a name with a far-side province, and the oil mark the far side takes off
//     the old chart (the planner's paper 21)
//   the armies on both sides of the frontier between India and Burma, by date (ruling 202; the clerk's
//     burma-front-forces.json), with the corps each of Japan's is formed from (the planner's paper 18, section 8)
//
//   node tools/make-far-world.mjs            write the file
//   node tools/make-far-world.mjs --check    exit 1 if the file on disk is not what the sources give today
//
// The sources are read from the shared folder (FAR_SOURCES, default /mnt/project-files). Run it again when the
// planner's, the historian's or the clerk's file changes; nothing else writes far-world.json.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const SRC = process.env.FAR_SOURCES ?? '/mnt/project-files';
const OUT = path.join(ROOT, 'data/campaign/far-world.json');
const read = (p) => JSON.parse(fs.readFileSync(p, 'utf8'));
const src = (p) => read(path.join(SRC, p));
const stamp = (p) => fs.statSync(path.join(SRC, p)).mtime.toISOString().slice(0, 16) + 'Z';

const first = src('pacific-planner/first-day-corps.json');
const opening = src('pacific-planner/opening-armies.json');
const generals = src('historian/generals-usa-japan.json');
const men = src('historian/manpower-213.json');
const levy = src('pacific-planner/runs-219/levy218-seats.json');
const pacific = src('pacific-planner/pacific-fleets.json');
const rig = src('pacific-planner/20-trial/base-seats.json');
const burma = src('map-tables/burma-front-forces.json');
// Ruling b445: "a Japan that can lay no keel is a fault, not a balance choice". The planner's paper carries the list as
// the inside of a JSON object, between ```json marks: province on the chart, level (1 destroyers, submarines and
// troopships; 2 cruisers too; 3 carriers and battleships too).
const yardsPaper = fs.readFileSync(path.join(SRC, 'pacific-planner/far-yards.md'), 'utf8');
const yardsBlock = yardsPaper.match(/```json\n([\s\S]*?)```/);
const yards = yardsBlock ? JSON.parse(`{${yardsBlock[1]}}`) : null;
// The whole world's data gaps (the planner's paper 21, 18 November, answering the suite run with the far switch on).
// It carries its rows the same way, each between ```json marks: a "towns" block (two provinces of the old chart whose
// chief town is a far-side province's name once the whole world is on the chart), and an "oil" block with an "unmark"
// list beside it (ruling 231, 1: the oil marks the far side takes off; the chart tool takes off the far side's own).
const gapsPaper = fs.readFileSync(path.join(SRC, 'pacific-planner/21-whole-world-data-gaps.md'), 'utf8');
const gapsBlocks = [...gapsPaper.matchAll(/```json\n([\s\S]*?)```/g)].map((m) => m[1].trim());
const gapsBlock = (key) => { const b = gapsBlocks.find((x) => x.startsWith(`"${key}"`)); return b ? JSON.parse(`{${b}}`) : null; };
const townNames = gapsBlock('towns')?.towns?.names ?? null;
const unmarkAll = gapsBlock('oil')?.unmark ?? null;
// The far side's oil (rulings 194 and 231; the planner's far-oil.md): what each far-side oilfield yields, and the
// Soviet home row, which falls as its fields come onto the chart. One block between ```json marks.
const oilPaper = fs.readFileSync(path.join(SRC, 'pacific-planner/far-oil.md'), 'utf8');
const oilBlock = oilPaper.match(/```json\n([\s\S]*?)```/);
const oilRows = oilBlock ? JSON.parse(`{${oilBlock[1].trim().replace(/,$/, '')}}`) : null;
const oilFields = oilRows?.fields ?? null;
const oilHome = oilRows?.home ?? null;
const chinaSetup = read(path.join(ROOT, 'data/campaign/china-setup.json'));
const stop3 = read(path.join(ROOT, 'data/campaign/china-stop3.json'));
const chart = read(path.join(ROOT, 'src/generated/world-whole.json'));
const owner = new Map(chart.regions.map((r) => [r.name, r.owner]));
const seaOf = (name, pick = 0) => chart.seas.zones[chart.regions.find((r) => r.name === name).seas[pick]].id;

const notes = [];
const need = (ok, what) => { if (!ok) throw new Error(what); };

// ---- The army ----------------------------------------------------------------------------------------

/** A row's options as the game reads them: the planner's notes ('else', 'built', 'kept', 'src', 'list', '_...') are left out. */
const NOTES = new Set(['else', 'built', 'kept', 'src', 'list']);
const opts = (o) => Object.fromEntries(Object.entries(o).filter(([k]) => !NOTES.has(k) && !k.startsWith('_')));
const firstRow = ([nat, name, kind, divs, at, o]) => [nat, name, kind, divs, at, opts(o)];
const datedRow = ([month, nat, name, kind, divs, at, o]) => [month, nat, name, kind, divs, at, opts(o)];

// Ruling 221 (H): Japan's eight garrison rows keep their hold when a general offensive's release frees the ground.
const stay = new Set(stop3._stay.map(([nat, name, at]) => `${nat}|${name}|${at}`));
const japan = [...first.japan, ...first.manchuria].map(firstRow).map((row) => {
  if (stay.delete(`${row[0]}|${row[1]}|${row[4]}`)) row[5].stay = 1;
  return row;
});
need(stay.size === 0, `china-stop3.json '_stay' names rows the planner's file has not: ${[...stay].join('; ')}`);

// The old powers' garrisons of the first day, America's among them.
const garrisons = opening.first_day.map(firstRow);
for (const row of [...japan, ...garrisons]) need(owner.has(row[4]), `first-day row "${row[1]}": ${row[4]} is not a province of the chart`);

// Rows by date. A row china-setup.json lays already (the raids' four) is not laid twice.
const laid = new Set(chinaSetup['setup-1939'].expeditions.map((r) => `${r[0]}|${r[1]}|${r[2]}`));
const dated = { expeditions: [], formations: [] };
for (const row of [...opening.by_date, ...opening.japan_by_date, ...opening.japan_1944]) {
  if (laid.has(`${row[0]}|${row[1]}|${row[2]}`)) continue;
  const list = row[6].list ?? 'expeditions';
  need(list in dated, `row by date "${row[2]}": list "${list}"`);
  need(owner.has(row[5]), `row by date "${row[2]}": ${row[5]} is not a province of the chart`);
  dated[list].push(datedRow(row));
}
for (const l of Object.values(dated)) l.sort((a, b) => a[0] - b[0]);

// Ruling 202: the armies on both sides of the frontier between India and Burma (the clerk's rows, in the game's own
// form). The first-day rows stand with the old powers' garrisons; the rows by date go at the END of the expeditions,
// in the clerk's order (a saved game remembers an expedition by its place in the list). Five of Japan's are formed
// from corps that already stand in the game (the planner's paper 18, section 8): the row names them, and the game
// strikes those off the day the new one forms (game.ts, 'from'), so no division is counted twice.
const BURMA_FROM = {
  '15th Army (33rd and 55th Divisions)': ['11th Army (33rd Division)'],
  '18th Division': ['25th Army (18th Division)', 'Kawaguchi Detachment (35th Brigade, of the 18th Division)'],
  '15th Division': ['13th Army (15th Division, Nanking)'],
  '2nd Division': ['16th Army (2nd Division)'],
  '56th Division': ['Sakaguchi Detachment (56th Mixed Infantry Group)'],
};
const burmaFirst = burma.world_corps.map(firstRow);
for (const row of burmaFirst) {
  need(owner.has(row[4]), `Burma's first-day row "${row[1]}": ${row[4]} is not a province of the chart`);
  need(!garrisons.some((x) => x[0] === row[0] && x[1] === row[1]), `Burma's first-day row "${row[1]}" is laid already`);
}
garrisons.push(...burmaFirst);
const standing = new Set([...japan, ...garrisons].map((r) => r[1]).concat(dated.expeditions.map((r) => r[2])));
const fromLeft = new Set(Object.keys(BURMA_FROM));
const burmaDated = burma.expeditions.map(datedRow).map((row) => {
  need(owner.has(row[5]), `Burma's row by date "${row[2]}": ${row[5]} is not a province of the chart`);
  need(!dated.expeditions.some((x) => x[1] === row[1] && x[2] === row[2]), `Burma's row by date "${row[2]}" is laid already`);
  const from = row[1] === 'JAP' ? BURMA_FROM[row[2]] : undefined;
  if (from) {
    for (const nm of from) need(standing.has(nm), `Burma's row "${row[2]}" is formed from "${nm}", which no row lays`);
    need(row[6].from === undefined, `Burma's row "${row[2]}" names its own 'from' now: take it out of BURMA_FROM`);
    row[6].from = from;
    fromLeft.delete(row[2]);
  }
  return row;
});
need(fromLeft.size === 0, `no row of Burma's for: ${[...fromLeft].join('; ')}`);
dated.expeditions.push(...burmaDated);

// ---- The fleets (the planner's paper 11, section 7, with the historian's first-day destroyers; names and admirals
// ruling 208(f) and the planner's paper 16; Britain's and the Dutch rows the historian's, pacific-allied-fleet-rows.md)
const fleet = (nat, name, base, port, sea, duty, ships, admiral) => {
  need(owner.has(base), `fleet "${name}": ${base} is not a province of the chart`);
  const home = typeof sea === 'number' ? seaOf(base, sea) : sea;
  need(chart.seas.zones.some((z) => z.id === home), `fleet "${name}": no sea "${home}"`);
  return { nat, name, base, port, home, zone: home, duty, ships: { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0, ...ships }, admiral };
};
const fleets = [
  fleet('JAP', 'Combined Fleet', 'Hiroshima', 'Kure', 'inland', 'port', { bb: 6, cv: 1, ca: 4, dd: 27 }, 'Isoroku Yamamoto'),
  fleet('JAP', 'Mobile Fleet', 'Hiroshima', 'Kure', 'inland', 'port', { bb: 1, cv: 4, ca: 3, dd: 3 }, 'Chuichi Nagumo'),
  fleet('JAP', '2nd Fleet', 'Formosa', 'Takao', 'luzonstrait', 'patrol', { bb: 2, ca: 4, dd: 10 }, 'Nobutake Kondo'),
  fleet('JAP', 'Southern Expeditionary Fleet', 'Hainan', 'Samah', 'schina', 'patrol', { ca: 5, dd: 10 }, 'Jisaburo Ozawa'),
  fleet('JAP', '3rd Fleet', 'Formosa', 'Mako', 'luzonstrait', 'patrol', { cv: 1, ca: 5, dd: 12 }, 'Ibo Takahashi'),
  fleet('JAP', '4th Fleet', 'Truk', 'Truk', 'truk', 'patrol', { ca: 7, dd: 10, ss: 9 }, 'Shigeyoshi Inoue'),
  fleet('JAP', '5th Fleet', 'Aomori', 'Ominato', 0, 'patrol', { ca: 2, dd: 4 }, 'Boshiro Hosogaya'),
  fleet('JAP', 'China Area Fleet', 'Shanghai', 'Shanghai', 'echina', 'patrol', { ca: 2, dd: 6 }, 'Mineichi Koga'),
  fleet('JAP', '6th Fleet', 'Kwajalein', 'Kwajalein', 'marshalls', 'port', { ss: 46 }, 'Mitsumi Shimizu'),
  fleet('UK', 'China Fleet', 'Singapore', 'Singapore', 0, 'patrol', { cv: 1, ca: 8, dd: 9, ss: 15 }, 'Percy Noble'),
  fleet('NED', 'Dutch East Indies Fleet', 'Soerabaja', 'Soerabaja', 'java', 'patrol', { ca: 3, dd: 7, ss: 15 }, 'Conrad Helfrich'),
];
// Ruling 189(b): a light carrier that sailed alone has a row of its own and, where a row can carry one, its own wing.
// The historian's figure (pacific-fleets-check.md, section 4): a third of a wing. Ryujo with the 3rd Fleet; Shoho, who
// joins the 4th Fleet in January 1942. 'deck' is the share of its navy's wing each of the fleet's carriers brings.
for (const name of ['3rd Fleet', '4th Fleet']) fleets.find((f) => f.nat === 'JAP' && f.name === name).deck = 0.33;
// Japan's ships that joined between September 1939 and Pearl Harbor, in navy.json's form (the historian's table of
// destroyers; the planner's of carriers and battleships). Later arrivals and the building rows come with the hunt.
const arrivals = [
  [3, 'JAP', 'Mobile Fleet', { dd: 1 }, 'a new destroyer'],
  [4, 'JAP', 'Mobile Fleet', { dd: 1 }, 'a new destroyer'],
  [5, 'JAP', 'Mobile Fleet', { bb: 1 }, 'a battleship rebuilt as a fast battleship'],
  [5, 'JAP', '2nd Fleet', { dd: 2 }, 'two new destroyers'],
  [6, 'JAP', '2nd Fleet', { dd: 1 }, 'a new destroyer'],
  [12, 'JAP', '2nd Fleet', { dd: 3 }, 'three new destroyers'],
  [14, 'JAP', '2nd Fleet', { dd: 1 }, 'a new destroyer'],
  [15, 'JAP', 'Mobile Fleet', { dd: 1 }, 'a new destroyer'],
  [16, 'JAP', 'Mobile Fleet', { dd: 1 }, 'a new destroyer'],
  [16, 'JAP', '2nd Fleet', { dd: 1 }, 'a new destroyer'],
  [17, 'JAP', '2nd Fleet', { dd: 1 }, 'a new destroyer'],
  [19, 'JAP', '2nd Fleet', { dd: 1 }, 'a new destroyer'],
  [20, 'JAP', 'Mobile Fleet', { dd: 1 }, 'a new destroyer'],
  [20, 'JAP', '2nd Fleet', { dd: 1 }, 'a new destroyer'],
  [22, 'JAP', 'Mobile Fleet', { dd: 1 }, 'a new destroyer'],
  [23, 'JAP', '2nd Fleet', { dd: 1 }, 'a new destroyer'],
  [25, 'JAP', 'Mobile Fleet', { dd: 1, cv: 2 }, 'two new fleet carriers'],
  [28, 'JAP', 'Combined Fleet', { dd: 1 }, 'a new destroyer'],
  // Japan's ships from January 1942 (ruling 189(f): the historian's rows stand; pacific-fleets-check.md, section 2, and
  // the planner's table it corrects). Light carriers count two to one (189(2)): Chiyoda and Chitose are one carrier, the
  // last class's three decks with no air groups are one, and Ryuho (November 1942) is counted with the two converted
  // liners, three decks of about 130 aircraft, so she brings no row. Shoho sailed alone, with the 4th Fleet, and has a
  // row of her own as one carrier (189(b)). The fleet each ship joins is the navy thread's reading where the historian
  // names none (25 November 2026): battleships and later flagships to the Combined Fleet, carriers to the Mobile Fleet,
  // the light cruisers that led destroyer squadrons to the 2nd Fleet.
  [29, 'JAP', '4th Fleet', { cv: 1 }, 'the light carrier Shoho'],
  [30, 'JAP', 'Combined Fleet', { bb: 1 }, 'the new battleship Yamato'],
  [33, 'JAP', 'Mobile Fleet', { cv: 1 }, 'the carrier Junyo, converted from a liner'],
  [35, 'JAP', 'Mobile Fleet', { cv: 1 }, 'the carrier Hiyo, converted from a liner'],
  [38, 'JAP', '2nd Fleet', { ca: 1 }, 'the new light cruiser Agano'],
  [41, 'JAP', 'Combined Fleet', { bb: 1 }, 'the new battleship Musashi'],
  [42, 'JAP', 'Combined Fleet', { ca: 1 }, 'the new light cruiser Oyodo'],
  [46, 'JAP', '2nd Fleet', { ca: 1 }, 'the new light cruiser Noshiro'],
  [52, 'JAP', '2nd Fleet', { ca: 1 }, 'the new light cruiser Yahagi'],
  [53, 'JAP', 'Mobile Fleet', { cv: 1 }, 'the light carriers Chiyoda and Chitose, converted from seaplane carriers'],
  [55, 'JAP', 'Mobile Fleet', { cv: 1 }, 'the new carrier Taiho'],
  [60, 'JAP', 'Mobile Fleet', { cv: 1 }, 'the new carriers Unryu and Amagi, short of aircraft to fly from them'],
  [63, 'JAP', 'Combined Fleet', { ca: 1 }, 'the new light cruiser Sakawa'],
];
// Japan's building (189(f)): destroyers about 1.5 a month from 1942 to 1944 (the historian: about 10 in 1942, 13 in 1943
// and 31 in 1944, most of them the small class of 1944), laid in whole ships as one a month in 1942 and 1943 to the
// carriers' screen and three a month for seven months of 1944 and two for five to the battle line (55 in all). Japan's
// submarines are far-books.json's (the director's rulings of 22 November). The convoy escorts of 1943 to 1945 are not
// laid: they want a fleet whose duty is the lanes, which no row has yet.
const japanBuilding = [
  { nat: 'JAP', fleet: 'Mobile Fleet', cls: 'dd', from: 29, to: 52, perMonth: 1 },
  { nat: 'JAP', fleet: 'Combined Fleet', cls: 'dd', from: 53, to: 59, perMonth: 3 },
  { nat: 'JAP', fleet: 'Combined Fleet', cls: 'dd', from: 60, to: 64, perMonth: 2 },
];

// Britain east of Suez (ruling 189(e); the historian's pacific-allied-fleet-rows.md, sections 2a to 2e), at stops 1 and
// 2 only: the ships that went east come out of the European rows on their dates and back on theirs, so no ship is in two
// oceans. Stops 3 to 5 are the staffs' plans (Britain's staff sends a fleet east only when at war with Japan), which
// are the design director's to rule; none of these rows is read there. The China Fleet stands as Britain's eastern
// fleet all war (the far books already pool its submarines and move its base to Colombo): Force Z, the Eastern Fleet
// and, from November 1944, the East Indies Fleet are its ships by date. The British Pacific Fleet forms by its book at
// stops 1 and 2 (its plain date is past the war's end, so it never forms at 3 to 5). The navy thread's readings (25
// November 2026), where the historian names no fleet or row: the China Station's submarines stay (the far books' boats
// are drawn from them); the destroyers that went to the Mediterranean in 1940 join the Mediterranean Fleet; the
// cruisers that went to hunt raiders leave the chart and come back in January 1942; Hermes (a third of a carrier) and
// the lent Saratoga have no row; Formidable goes back to the Mediterranean Fleet in October 1942, so Torch's row of turn
// 39 finds her there. Rows of one month, one fleet and one source are one row: the game keys a row by them.
const S12 = { stops: [1, 2] };
const BPF = 'British Pacific Fleet';
const britainEast = [
  [8, 'UK', 'Mediterranean Fleet', { dd: 5 }, 'five of the China Station\'s destroyers', { ...S12, from: 'China Fleet' }],
  [10, 'UK', 'China Fleet', { ca: -5 }, 'Five cruisers of the China Station leave to hunt raiders in the Indian Ocean and the South Atlantic', S12],
  [28, 'UK', 'China Fleet', { bb: 2, dd: 2 }, 'Force Z, the battleship Prince of Wales, the battlecruiser Repulse and two destroyers,', { ...S12, from: 'Home Fleet' }],
  [28, 'UK', 'China Fleet', { dd: 2 }, 'two more of Force Z\'s destroyers', { ...S12, from: 'Mediterranean Fleet' }],
  [29, 'UK', 'China Fleet', { ca: 5 }, 'the cruisers Cornwall and Dorsetshire and three more, back from hunting raiders,', { ...S12, back: [10, 'China Fleet'] }],
  [30, 'UK', 'China Fleet', { bb: 2, dd: 8 }, 'the battleships Royal Sovereign and Ramillies and eight destroyers', { ...S12, from: 'Home Fleet' }],
  [30, 'UK', 'China Fleet', { bb: 1, dd: 8 }, 'the battleship Warspite, repaired in America, and eight destroyers', { ...S12, from: 'Mediterranean Fleet' }],
  [30, 'UK', 'China Fleet', { bb: 2, ca: 2 }, 'the old battleships Revenge and Resolution and the cruisers Emerald and Caledon', S12],
  [31, 'UK', 'China Fleet', { cv: 1 }, 'the carrier Formidable, repaired in America,', { ...S12, from: 'Mediterranean Fleet', by: 19 }],
  [33, 'UK', 'China Fleet', { cv: 1 }, 'the carrier Illustrious', { ...S12, from: 'Mediterranean Fleet' }],
  [36, 'UK', 'China Fleet', { cv: -1 }, 'The carrier Indomitable leaves the China Fleet for the Malta convoy', S12],
  [38, 'UK', 'Mediterranean Fleet', { cv: 1 }, 'the carrier Formidable', { ...S12, from: 'China Fleet' }],
  [41, 'UK', 'Home Fleet', { bb: 2, cv: 1 }, 'the battleships Royal Sovereign and Ramillies and the carrier Illustrious', { ...S12, from: 'China Fleet' }],
  [41, 'UK', 'Mediterranean Fleet', { bb: 1 }, 'the battleship Warspite', { ...S12, from: 'China Fleet' }],
  [41, 'UK', 'China Fleet', { bb: -2 }, 'The old battleships Revenge and Resolution go home to the reserve', S12],
  [53, 'UK', 'China Fleet', { bb: 1 }, 'the battleship Valiant', { ...S12, from: 'Mediterranean Fleet' }],
  [53, 'UK', 'China Fleet', { bb: 1 }, 'the battlecruiser Renown', { ...S12, from: 'Force H' }],
  [53, 'UK', 'China Fleet', { cv: 1 }, 'the carrier Illustrious', { ...S12, from: 'Home Fleet' }],
  [53, 'UK', 'China Fleet', { bb: 1 }, 'the battleship Queen Elizabeth, repaired,', S12],
  [59, 'UK', 'China Fleet', { cv: 1 }, 'the carrier Victorious', { ...S12, from: 'Home Fleet' }],
  [59, 'UK', 'China Fleet', { cv: 1 }, 'the carrier Indomitable', { ...S12, back: [36, 'China Fleet'] }],
  [60, 'UK', 'China Fleet', { bb: 1 }, 'the new battleship Howe', S12],
  [63, 'UK', BPF, { bb: 1, cv: 3, ca: 6 }, 'the battleship Howe, the carriers Indomitable, Victorious and Illustrious and six cruisers', { ...S12, from: 'China Fleet' }],
  [63, 'UK', BPF, { bb: 1, dd: 15 }, 'the battleship King George V and fifteen destroyers', { ...S12, from: 'Home Fleet' }],
  [63, 'UK', BPF, { cv: 1 }, 'the new carrier Indefatigable', S12],
  [63, 'UK', 'China Fleet', { cv: 1, ca: 2 }, 'escort carriers with about a fleet carrier\'s aircraft, and two cruisers, for the East Indies Fleet,', S12],
  [66, 'UK', 'China Fleet', { cv: 1 }, 'more of the escort carriers', S12],
  [68, 'UK', 'China Fleet', { cv: 1 }, 'more of the escort carriers', S12],
  [70, 'UK', BPF, { cv: 1 }, 'the new carrier Implacable', S12],
];
const britainEastFleets = [
  { ...fleet('UK', BPF, 'Trincomalee', 'Trincomalee', 'ceylon', 'port', {}, 'Bruce Fraser'), from: 99, book: { stops: [1, 2], from: 63, ships: { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 } } },
];
const britainEastMoves = [
  { turn: 33, nat: 'UK', fleet: 'China Fleet', base: 'Mombasa', port: 'Kilindini', home: 'zanzibar', words: 'After the raid on Ceylon the China Fleet withdraws to Kilindini', stops: [1, 2] },
  { turn: 49, nat: 'UK', fleet: 'China Fleet', base: 'Trincomalee', port: 'Trincomalee', home: 'ceylon', words: 'The China Fleet returns to Trincomalee', stops: [1, 2] },
  { turn: 66, nat: 'UK', fleet: BPF, base: 'Sydney', port: 'Sydney', home: 'tasman', words: 'The British Pacific Fleet moves to Sydney', stops: [1, 2], ally: true },
  { turn: 67, nat: 'UK', fleet: BPF, base: 'Admiralty Islands', port: 'Manus', home: 'bismarck', words: 'The British Pacific Fleet moves forward to Manus, for Okinawa', stops: [1, 2], ally: true },
];
{
  const uk = new Set(['China Fleet', 'Home Fleet', 'Mediterranean Fleet', 'Force H', BPF]);
  const keys = new Set();
  for (const a of britainEast) {
    need(uk.has(a[2]) && (!a[5].from || uk.has(a[5].from)), `Britain east: "${a[4]}" names a fleet that is not one of hers`);
    const key = `${a[0]}:${a[2]}<${a[5].from ?? ''}`;
    need(!keys.has(key), `Britain east: two rows of one month, fleet and source (${key}): the game reads only one`);
    keys.add(key);
  }
  for (const m of britainEastMoves) need(owner.has(m.base) && chart.seas.zones.some((z) => z.id === m.home), `Britain east: move "${m.words}": ${m.base} or the sea "${m.home}" is not on the chart`);
}

// America's fleets in the Pacific (the planner's pacific-fleets.json, in navy.json's own forms). A fleet that forms
// later ('from') is a row of navy.json, where the navy's month looks for it; the others stand on the first day.
// 'standing' is the planner's note of a rule that comes with the hunt, and is left out.
const seas = new Set(chart.seas.zones.map((z) => z.id));
const usFleets = pacific.fleets.map(({ standing, ...f }) => f);
for (const f of usFleets) {
  need(owner.get(f.base) === f.nat, `fleet "${f.name}": ${f.base} is not ${f.nat}'s on the chart`);
  need(seas.has(f.home) && seas.has(f.zone), `fleet "${f.name}": no sea "${f.home}" or "${f.zone}"`);
}
const fleetNames = new Set([...fleets, ...usFleets].map((f) => `${f.nat}|${f.name}`));
for (const a of arrivals) need(fleetNames.has(`${a[1]}|${a[2]}`), `arrival "${a[4]}": no fleet "${a[2]}"`);
for (const b of japanBuilding) need(fleetNames.has(`${b.nat}|${b.fleet}`), `building row: no fleet "${b.fleet}"`);
// The Atlantic Fleet is world.json's own row: three battleships and a carrier cross to it in May 1941 and come back (ruling 189(1)).
fleetNames.add('USA|Atlantic Fleet');
for (const a of pacific.arrivals) need(fleetNames.has(`${a[1]}|${a[2]}`), `arrival "${a[4]}": no fleet "${a[2]}"`);
// The playtest's #50 (25 Nov): the words of America's rows as the news reads them. A carrier "named for the one lost at
// Midway" names a battle this war may never have (sandbox law: nothing in the news is the record's unless it happened
// here), so those four come plainly, by their class ("Another new Essex-class carrier joins the Pacific Fleet"). And a row's words are the subject of "join(s) <the fleet>": a
// verb of their own reads twice. The planner's file is left as it is; these are laid over it here.
const usWords = (w) => (/, named for the one lost\b/.test(w) ? 'another new Essex-class carrier'
  : w === 'twenty-three submarines reach Manila' ? 'twenty-three submarines, come out to Manila,' : w);
const usArrivals = pacific.arrivals.map((a) => [a[0], a[1], a[2], a[3], usWords(a[4]), ...a.slice(5)]);
for (const a of usArrivals) need(!/named for the one lost|\b(reach|reaches)\b/.test(a[4]), `arrival "${a[4]}": words the news cannot read`);
for (const b of pacific.building) need(fleetNames.has(`${b.nat}|${b.fleet}`), `building row: no fleet "${b.fleet}"`);
// Ruling 234(d): a move with 'sea' in place of a base is the record's month at sea in a month of peace (the Pacific
// Fleet's carriers in December 1941, at the stops that keep the book). The script's own marks beside it: 'with', the
// great choice whose month it keeps where that month is drawn (Japan's strike south: the carriers are at sea in the
// strike's month, whenever the draw puts it); and on the fleet's row 'cruise', [that sea, CRUISE, the choice's nation]:
// at a stop that keeps no book the fleet is at sea in a month of peace on that chance by the seed (the ruling's one in
// two), and peace is peace with that nation (rider b593: America is at war with Germany from the close of December 1941).
const CRUISE = 0.5;
const STRIKE = 'JAP:south';
const usMoves = pacific.moves.map((m) => (m.sea !== undefined ? { ...m, with: STRIKE } : m));
for (const m of usMoves) {
  need(fleetNames.has(`${m.nat}|${m.fleet}`), `move "${m.words}": no fleet "${m.fleet}"`);
  if (m.sea !== undefined) {
    need(seas.has(m.sea) && m.base === undefined && m.home === undefined, `move "${m.words}": the sea "${m.sea}" is not on the chart, or the row names a base as well`);
    const f = usFleets.find((x) => x.nat === m.nat && x.name === m.fleet);
    need(f && !f.cruise, `move "${m.words}": "${m.fleet}" is not one of these fleets, or has two months at sea`);
    f.cruise = [m.sea, CRUISE, STRIKE.split(':')[0]];
  } else need(owner.has(m.base) && seas.has(m.home), `move "${m.words}": ${m.base} or the sea "${m.home}" is not on the chart`);
}

// ---- The men (rulings 213(b), 216(b), 218) -----------------------------------------------------------
const pool = {}, klass = {};
const menRow = (code, row, from) => { pool[code] = row.pool; klass[code] = row.class; notes.push(`men ${code}: pool ${row.pool}, class ${row.class} (${from})`); };
for (const code of ['NZL', 'SAF', 'MAN']) menRow(code, men.rows[code], 'rows');
for (const code of ['IRN', 'IRQ', 'SAU', 'NEP']) menRow(code, men.rows_17_15[code], 'rows_17_15');
for (const code of ['CHN', 'MGL', 'AUS', 'AFG']) menRow(code, men.rows_216[code], 'rows_216');
need(same(levy['manpower.json'].pool.CHN, pool.CHN) && same(levy['manpower.json'].class.CHN, klass.CHN), "the levy file's pool row for China is not the historian's");

function same(a, b) { return JSON.stringify(a) === JSON.stringify(b); }

// ---- Who holds what on the first day (the clerk's held_1939) -----------------------------------------
function csv(text) {
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
const held = {};
for (const f of ['pacific.csv', 'islands-draft.csv', 'globe.csv', 'cities.csv', 'cities-parents.csv']) {
  const p = path.join(ROOT, 'data/campaign/tables', f);
  if (!fs.existsSync(p)) continue;
  const [head, ...rows] = csv(fs.readFileSync(p, 'utf8'));
  const iName = head.indexOf('name'), iHeld = head.indexOf('held_1939');
  if (iName < 0 || iHeld < 0) continue;
  for (const r of rows) {
    const name = r[iName], by = r[iHeld];
    if (!name || !by || !owner.has(name) || by === owner.get(name)) continue;
    need(!held[name] || held[name] === by, `${f}: "${name}" held by ${by} here and ${held[name]} in an earlier table`);
    held[name] = by;
  }
}
// Every first-day row stands on ground its own country holds, or on its client's or patron's (the rig's rule).
const TIES = { JAP: ['MAN'], SOV: ['MGL'] };
for (const [nat, name, , , at] of [...japan, ...garrisons]) {
  const by = held[at] ?? owner.get(at);
  need(by === nat || (TIES[nat] ?? []).includes(by), `first-day row "${name}" stands at ${at}, which ${by} holds`);
}

// Japan's temper is ruling 202(l)'s row. The record's line laid it (china-line.json); the rig's base seats must agree.
const chinaLine = read(path.join(ROOT, 'data/campaign/china-line.json'));
need(JSON.stringify(chinaLine.open?.temper?.JAP) === JSON.stringify(rig.open.temper.JAP), "Japan's temper in china-line.json is not the planner's base-seats.json row");
need(Array.isArray(rig.rivals?.lean?.JAP) && rig.rivals.lean.JAP.length === 3, "the planner's base-seats.json has no rivals.lean.JAP row of three");

// The yards: every one a province of the chart that touches a sea, at a level the arsenal knows, and none a yard of
// the older chart's already (arsenal.json's own rows are not the far side's to change).
const oldYards = read(path.join(ROOT, 'data/campaign/arsenal.json')).yards;
need(yards && Object.keys(yards).length > 0, "the planner's far-yards.md has no list between ```json marks");
for (const [name, level] of Object.entries(yards)) {
  const r = chart.regions.find((x) => x.name === name);
  need(r, `far-yards.md: ${name} is not a province of the chart`);
  need(r.seas.length > 0, `far-yards.md: ${name} touches no sea`);
  need([1, 2, 3].includes(level), `far-yards.md: ${name} has level ${level}`);
  need(!(name in oldYards), `far-yards.md: ${name} has a yard in arsenal.json already`);
}
notes.push(`yards: ${Object.entries(yards).map(([n, l]) => `${n} ${l} (${owner.get(n)})`).join(', ')}`);

// The towns: each a province of the old chart whose chief town in towns.json is the name of a far-side province, and
// the new name one that no province and no town on the whole chart carries.
const oldChart = read(path.join(ROOT, 'src/generated/campaign-map.json'));
const oldTowns = read(path.join(ROOT, 'data/campaign/towns.json')).names;
const everyName = new Set([...oldChart.regions, ...chart.regions].map((r) => r.name));
need(townNames && Object.keys(townNames).length > 0, "the planner's paper 21 has no \"towns\" block between ```json marks");
for (const [prov, town] of Object.entries(townNames)) {
  need(oldChart.regions.some((r) => r.name === prov), `paper 21, towns: ${prov} is not a province of the old chart`);
  need(chart.regions.some((r) => r.name === oldTowns[prov]), `paper 21, towns: ${prov}'s town (${oldTowns[prov]}) is no far-side province's name, so it needs no new one`);
  need(!everyName.has(town) && !Object.values(oldTowns).includes(town), `paper 21, towns: ${town} is a name on the chart already`);
}
notes.push(`towns: ${Object.entries(townNames).map(([p, t]) => `${p}: ${oldTowns[p]} becomes ${t}`).join(', ')}`);

// The oilfields (rulings 194 and 231): each a far-side province with the oil mark and no yield of its own in
// resources.json, and every far-side province with the mark has a row. A field on colony ground gives nothing at
// home (194 point 1, resources.ts); the note says which are. The home rows are of countries resources.json gives one.
const oldOil = read(path.join(ROOT, 'data/campaign/resources.json')).oil;
const oldFields = oldOil.fields;
const farFirst = chart.first + chart.stages[1];
need(oilFields && Object.keys(oilFields).length > 0, "the planner's far-oil.md has no \"fields\" between ```json marks");
for (const [name, y] of Object.entries(oilFields)) {
  const r = chart.regions.find((x) => x.name === name);
  need(r && r.id >= farFirst, `far-oil.md, fields: ${name} is not a far-side province`);
  need(r.flags?.oil, `far-oil.md, fields: ${name} has no oil mark on the chart`);
  need(typeof y === 'number' && y > 0, `far-oil.md, fields: ${name} yields ${y}`);
  need(!(name in oldFields), `far-oil.md, fields: ${name} has a yield in resources.json already`);
}
const unnamed = chart.regions.filter((r) => r.id >= farFirst && r.flags?.oil && !(r.name in oilFields)).map((r) => r.name);
need(unnamed.length === 0, `far-oil.md, fields: no row for ${unnamed.join(', ')}`);
need(oilHome && Object.keys(oilHome).length > 0, "the planner's far-oil.md has no \"home\" row");
for (const [nat, v] of Object.entries(oilHome)) need(nat in oldOil.home && typeof v === 'number' && v >= 0, `far-oil.md, home: ${nat} ${v}`);
// The marks that come off: a far-side province's is the chart tool's and must be off already; an old-chart
// province's is the far setup's to take off as the chart is attached (far.ts), and it must carry one today.
need(Array.isArray(unmarkAll) && unmarkAll.length > 0, "the planner's paper 21 has no \"unmark\" list beside its \"oil\" block");
const unmark = [];
for (const name of unmarkAll) {
  const far = chart.regions.find((x) => x.name === name), old = oldChart.regions.find((x) => x.name === name);
  need(far || old, `paper 21, unmark: ${name} is not a province of the chart`);
  if (far) need(!far.flags?.oil, `paper 21, unmark: ${name} still carries the oil mark on the far chart (tools/build-world-whole.mjs takes it off)`);
  else { need(old.flags?.oil, `paper 21, unmark: ${name} carries no oil mark on the old chart`); unmark.push(name); }
}
// The Soviet sum (ruling 231, 1(f)): what her own fields and her home row give, the old chart against the whole world.
const sovOff = oldChart.regions.filter((r) => r.owner === 'SOV' && r.flags?.oil).reduce((s, r) => s + (oldFields[r.name] ?? oldFields._default), 0) + oldOil.home.SOV;
const sovOn = oldChart.regions.filter((r) => r.owner === 'SOV' && r.flags?.oil && !unmark.includes(r.name)).reduce((s, r) => s + (oldFields[r.name] ?? oldFields._default), 0)
  + chart.regions.filter((r) => r.id >= farFirst && owner.get(r.name) === 'SOV' && r.flags?.oil && !r.flags?.col).reduce((s, r) => s + oilFields[r.name], 0) + (oilHome.SOV ?? oldOil.home.SOV);
need(sovOn === sovOff + 1, `the Soviet Union's oil is ${sovOff} on the old chart and ${sovOn} on the whole world: ruling 231 asks for one more (North Sakhalin)`);
notes.push(`oil: ${Object.entries(oilFields).map(([n, y]) => `${n} ${y} (${owner.get(n)}${chart.regions.find((x) => x.name === n).flags.col ? ', colony' : ''})`).join(', ')}`);
notes.push(`oil: home ${Object.entries(oilHome).map(([n, v]) => `${n} ${v} (was ${oldOil.home[n]})`).join(', ')}; the old chart's mark off: ${unmark.join(', ')}; the Soviet Union's own oil ${sovOff} on the old chart, ${sovOn} on the whole world`);

// ---- The file ----------------------------------------------------------------------------------------
const out = {
  _about: "The far side's rows that are nobody's seat, written by tools/make-far-world.mjs from the Pacific planner's, the historian's and the clerk's files and never by hand. Keyed by the data file each block belongs in, as the seat builders' files are; the game lays it over its data only where the far side's switch is on (src/campaign/far-rows.ts). Japan is the seventh great power (ruling 141(a)): quality 1.0 (ruling 215), outside both blocs and leaning to the Axis (ruling 197), a sea lift of 6 (ruling 200). Its army is the planner's paper 17 (the historian's weights), its fleets paper 11 section 7 as ruling 189 amended them, with the names and admirals of ruling 208(f). The garrisons of Britain, the Netherlands, France, the Soviet Far East and America are paper 18's. America's fleets in the Pacific, the ships that join and leave them, and the bases by date of the Pacific Fleet and of Japan's battle line are the planner's pacific-fleets.json (rulings 185, 189, 208(f)); a fleet that forms later is a row of navy.json. 'manpower' is rulings 213(b), 216(b) and 218; China's row under 'traits' is the seat's with the levy of ruling 218. 'setup.held' is the clerk's held_1939 where it is not the owner.",
  _sources: Object.fromEntries(['pacific-planner/first-day-corps.json', 'pacific-planner/opening-armies.json', 'pacific-planner/pacific-fleets.json', 'historian/generals-usa-japan.json', 'historian/manpower-213.json', 'pacific-planner/runs-219/levy218-seats.json', 'pacific-planner/20-trial/base-seats.json', 'map-tables/burma-front-forces.json', 'pacific-planner/far-yards.md', 'pacific-planner/far-oil.md', 'pacific-planner/21-whole-world-data-gaps.md', 'pacific-six/riders-234.json', 'pacific-six/riders-234-b591.json', 'pacific-six/riders-234-b599.json'].map((p) => [p, stamp(p)])),
  _draft: "Japan's words on the front door, its two abilities and its formation are the builder's draft from the record already in these files, for the historian and the UI director to mark: nobody had written them.",
  grand: {
    nations: [{
      id: 'JAP',
      years: { 1939: "Two years into a war in China that will not end. The coast, the railways and the great cities are yours, and the interior is not. The strongest fleet in the Pacific, six carriers, and oil for two years. Choose where the next blow falls." },
    }],
  },
  world: {
    // (Her sea lift on the first day is 4: rider b644 to ruling 234, laid in the data by hand in 35a0bfe and carried so since; this
    // line gave 6, ruling 200's figure, so the file as committed was not what the script wrote. Her lift by date is pacific.json's.)
    nations: { JAP: { name: 'Japan', adj: 'Japanese', color: '#c4204a', quality: 1, air: 60, resilience: 0.95, align: 'leaning_axis', seaSupply: true, sealift: 4 } },
    order: ['JAP'],
    great: ['JAP'],
    parts: { of: { JAP: 'asia' } },
    corps: [...japan, ...garrisons],
    fleets: [...fleets, ...usFleets.filter((f) => f.from === undefined)],
  },
  navy: {
    fleets: [...usFleets.filter((f) => f.from !== undefined), ...britainEastFleets],
    arrivals: [...arrivals, ...usArrivals, ...britainEast].sort((a, b) => a[0] - b[0]),
    building: [...pacific.building, ...japanBuilding],
    moves: [...usMoves, ...britainEastMoves],
    // The design director's B1 of 25 November 2026, 02:35Z (pacific-japan-1125.md): ship quality for the two Pacific
    // navies, which had none and fought at navy.json's 0.7 default, seeded from the 1939 record at the historian's range
    // middles (historian/japan-seed-checks-1125.md, section 2): Japan 1.15 (1.1 to 1.2: crews, night fighting and the
    // torpedo; weakest at damage control), America 0.95 (0.9 to 1.0: damage control; weakest at night and with the
    // torpedo). Radar is the tree's, not counted here. Whole world only, so the old chart's wars do not move.
    quality: { USA: 0.95, JAP: 1.15 },
  },
  // Japan's character as a staff (paper 14, section 8, item 13; ruled with 181 to 202): where its budget leans. Its temper
  // (ruling 202(l)) is laid by china-line.json already; the two files must say the same thing.
  rivals: { lean: { JAP: rig.rivals.lean.JAP } },
  // The far side's shipyards (ruling b445): history's yards beyond the Americas, by port. A port's yard is the greater of this and what has been built there.
  arsenal: { yards },
  // Two chief towns of the old chart whose names far-side provinces carry as their own: the old provinces take another town's (the planner's paper 21).
  towns: { names: townNames },
  // What each far-side oilfield yields, and the Soviet home row once its fields are on the chart (rulings 194 and 231; the planner's far-oil.md).
  resources: { oil: { fields: oilFields, home: oilHome } },
  // A great power founds and joins no League. The two clients are barred by their tie (league.ts), and keep a small country's own path.
  league: { never: ['JAP'] },
  traits: {
    nations: {
      CHN: levy['traits.json'].nations.CHN,
      JAP: {
        traits: [
          { id: 'striking', name: 'The carrier striking force', text: 'Six fleet carriers trained to strike together, with the best naval airmen in the world at the start of the war.', cost: { what: ['ship:cv'], mul: 0.85 } },
          { id: 'landing', name: 'Landings by night', text: 'An army and a navy that have practised landing on a hostile shore since the war in China began, from the Yangtze to Hainan.', attack: 1.1, when: { landing: true } },
        ],
        unit: { id: 'snlf', label: 'Naval landing force', word: 'Landing', kind: 'inf', divs: 1, cost: 18, months: 4, most: 3, attack: 1.2, when: { landing: true }, text: "The navy's own infantry, raised at the four naval bases to seize harbours and islands ahead of the army." },
      },
    },
  },
  commanders: { roster: { JAP: generals.roster.JAP }, names: { JAP: generals.names.JAP } },
  relations: {
    pairs: [
      ['JAP', 'GER', 'partners in the pact against the Comintern of 1936'],
      ['JAP', 'ITA', 'partners in the pact against the Comintern of 1936'],
      ['JAP', 'THA', 'a treaty of friendship, and the one free kingdom of south-east Asia'],
    ],
  },
  manpower: { pool, class: klass },
  setup: { held: Object.fromEntries(Object.entries(held).sort(([a], [b]) => (a < b ? -1 : 1))), unmark },
  'setup-1939': dated,
};

// ---- Ruling 234's riders ---------------------------------------------------------------------------------
// The record's rows the opening was measured with (the design director's riders to ruling 234; the Pacific six's
// builder keeps the files). They are laid last, over what the sources above give, one file after the other:
//   riders-234.json        riders b552 to b583, the book's rows of the opening
//   riders-234-b591.json   riders b591 to b614, the facts of the day at every stop of the dial (Japan's armour roster
//                          and her island garrisons by half-year)
//   riders-234-b641.json   rider b641, a fact of the day at every stop: the 7th Division, home to Hokkaido in August
//                          1940 from the 6th Army's row in Manchuria, which is cut in two for it
//   riders-234-b642.json   rider b642 part 2: the rows a lost posting may not bring ashore (a local levy), and what
//                          a row that takes corps on the board adds from home (the 56th Division's body)
//   riders-234-b599.json   rider b599 (i), the corps of Japan's second ring the world had not (the book's, stops 1-2)
// And after them the director's later orders, kept by the rules builder of release 2's far side:
//   pacific-r2/riders-luzon-1121.json   his order for Luzon of 21 November 2026: the 65th Brigade's size, and his rulings of
//                          22:34Z to 22:50Z on the Lingayen reading: the 14th Army's rows and Luzon's garrison by the record
//   pacific-r2/riders-lent-1122.json    his rulings of 22:55Z, 21 November, and 03:49Z, 22 November 2026: the place each lent
//                          row of the far side was lent for ('takes', 'holds'; game.ts, lentOver)
//   pacific-r2/riders-tanks-1122.json   his ruling of 04:01Z, 22 November 2026, item 4: the later moves of the tank regiments
//                          the 1st and 2nd Tank Divisions were made of (the book's, stops 1-2), each cut from its
//                          division by a sister row as the 21st Division's detachment for Bataan is
// A 'set' names its row and what the row reads before it: a source that moves under a rider stops the build instead
// of being written over. A word set to null is taken off the row. A 'move' gives a row by date another month or
// another place, and the row keeps its place in the list. An 'add' row goes at the END of its list, in the file's
// order (a saved game remembers an expedition by its place in the list).
const sameWord = (a, b) => JSON.stringify(a ?? null) === JSON.stringify(b ?? null);
const setRow = (what, rows, words, r) => {
  need(rows.length === 1, `rider: ${rows.length} ${what} named ${JSON.stringify(r.row)}`);
  for (const [k, v] of Object.entries(r.was)) need(sameWord(words(rows[0])[k], v), `rider: ${what} ${JSON.stringify(r.row)} reads ${k} ${JSON.stringify(words(rows[0])[k])}, not ${JSON.stringify(v)}`);
  for (const [k, v] of Object.entries(r.now)) { if (v === null) delete words(rows[0])[k]; else words(rows[0])[k] = v; }
};
const byDate = (r) => dated.expeditions.filter((x) => x[0] === r.row[0] && x[1] === r.row[1] && x[2] === r.row[2]);
function layRiders(file) {
  const riders = src(file);
  const E = riders.expeditions ?? {}, C = riders.corps ?? {};
  for (const r of E.set ?? []) setRow('row by date', byDate(r), (x) => x[6], r);
  // (A formation of the opening armies, the dated rows the far world lists as 'formations', is named the same way.)
  for (const r of riders.formations?.set ?? []) setRow('formation by date', dated.formations.filter((x) => x[0] === r.row[0] && x[1] === r.row[1] && x[2] === r.row[2]), (x) => x[6], r);
  for (const r of E.move ?? []) {
    const rows = byDate(r);
    need(rows.length === 1, `rider: ${rows.length} rows by date named ${JSON.stringify(r.row)}`);
    need(rows[0][5] === r.was.place, `rider: the row by date ${JSON.stringify(r.row)} stands at ${rows[0][5]}, not ${r.was.place}`);
    if (r.place !== undefined) { need(owner.has(r.place), `rider: "${r.row[2]}": ${r.place} is not a province of the chart`); rows[0][5] = r.place; }
    if (r.turn !== undefined) { need(!dated.expeditions.some((x) => x !== rows[0] && x[0] === r.turn && x[1] === r.row[1] && x[2] === r.row[2]), `rider: the row by date "${r.row[2]}" is laid already in turn ${r.turn}`); rows[0][0] = r.turn; }
  }
  for (const { row } of E.add ?? []) {
    need(!dated.expeditions.some((x) => x[0] === row[0] && x[1] === row[1] && x[2] === row[2]), `rider: the row by date "${row[2]}" is laid already`);
    need(owner.has(row[5]), `rider: "${row[2]}": ${row[5]} is not a province of the chart`);
    dated.expeditions.push(row);
  }
  for (const r of C.set ?? []) {
    const rows = out.world.corps.filter((x) => x[0] === r.row[0] && x[1] === r.row[1]);
    setRow('first-day row', rows, (x) => x[5], r);
    // (A first-day row a rider cuts in two by name gives up the divisions its other half takes: 'divs' is before and after.)
    if (r.divs) { need(rows[0][3] === r.divs[0], `rider: the first-day row ${JSON.stringify(r.row)} has ${rows[0][3]} divisions, not ${r.divs[0]}`); rows[0][3] = r.divs[1]; }
  }
  for (const { row } of C.add ?? []) {
    need(!out.world.corps.some((x) => x[0] === row[0] && x[1] === row[1]), `rider: the first-day row "${row[1]}" is laid already`);
    need(owner.has(row[4]), `rider: "${row[1]}": ${row[4]} is not a province of the chart`);
    out.world.corps.push(row);
  }
  for (const r of riders.fleets?.set ?? []) setRow('fleet', out.world.fleets.filter((f) => f.nat === r.row[0] && f.name === r.row[1]), (f) => f.ships, r);
  for (const { row } of riders.arrivals?.add ?? []) {
    need(!out.navy.arrivals.some((x) => sameWord(x, row)), `rider: the arrival "${row[4]}" is laid already`);
    need([...out.world.fleets, ...out.navy.fleets].some((f) => f.nat === row[1] && f.name === row[2]), `rider: the arrival "${row[4]}" names no fleet`);
    out.navy.arrivals.push(row);
  }
  for (const [nat, r] of Object.entries(riders.roster ?? {})) {
    if (nat === 'why') continue;
    if (Array.isArray(r)) { need(out.commanders.roster[nat] === undefined, `rider: a roster for ${nat} is laid already`); out.commanders.roster[nat] = r; continue; }
    const roster = out.commanders.roster[nat].map((x) => [...x]);
    for (const [name, was, now] of r.month ?? []) {
      const rows = roster.filter((x) => x[1] === name);
      need(rows.length === 1 && rows[0][0] === was, `rider: ${name} is not rostered once, in turn ${was}`);
      rows[0][0] = now;
    }
    for (const x of r.first ?? []) need(!roster.some((y) => y[1] === x[1]), `rider: ${x[1]} is rostered already`);
    out.commanders.roster[nat] = [...(r.first ?? []), ...roster];
  }
  const n = (xs) => (xs ?? []).length, rosters = Object.keys(riders.roster ?? {}).filter((k) => k !== 'why');
  notes.push(`ruling 234's riders (${path.basename(file)}): ${n(E.set)} rows by date changed, ${n(E.move)} moved and ${n(E.add)} added, ${n(riders.formations?.set) ? `${n(riders.formations.set)} formations by date changed, ` : ''}${n(C.set)} first-day rows changed and ${n(C.add)} added, ${n(riders.fleets?.set)} fleet, ${n(riders.arrivals?.add)} arrival${rosters.length ? `, the rosters of ${rosters.join(' and ')}` : ''}`);
}
const RIDERS = ['pacific-six/riders-234.json', 'pacific-six/riders-234-b591.json', 'pacific-six/riders-234-b641.json', 'pacific-six/riders-234-b642.json', 'pacific-six/riders-234-b599.json', 'pacific-r2/riders-luzon-1121.json', 'pacific-r2/riders-lent-1122.json', 'pacific-r2/riders-tanks-1122.json'];
for (const file of RIDERS) layRiders(file);

const text = JSON.stringify(out, null, 1) + '\n';
if (process.argv.includes('--check')) {
  const on = fs.existsSync(OUT) ? fs.readFileSync(OUT, 'utf8') : '';
  // The stamps move when a source is saved again unchanged: the rows are what is compared.
  const rows = (t) => { const j = JSON.parse(t || '{}'); delete j._sources; return JSON.stringify(j); };
  if (rows(on) !== rows(text)) { console.error('far-world.json is not what the sources give today: run node tools/make-far-world.mjs'); process.exit(1); }
  console.log('far-world.json is what the sources give today');
} else {
  fs.writeFileSync(OUT, text);
  console.log(`wrote ${path.relative(ROOT, OUT)}: Japan ${japan.length} first-day rows, ${garrisons.length} garrison rows of the old powers, ${fleets.length + usFleets.length} fleets, ${arrivals.length + pacific.arrivals.length} arrivals and ${pacific.moves.length} moves of base by date, ${dated.expeditions.length} expeditions and ${dated.formations.length} formations by date, ${Object.keys(held).length} provinces held by another than their owner`);
  for (const n of notes) console.log('  ' + n);
}
