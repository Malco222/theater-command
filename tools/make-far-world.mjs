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
];

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
// The Atlantic Fleet is world.json's own row: three battleships and a carrier cross to it in May 1941 and come back (ruling 189(1)).
fleetNames.add('USA|Atlantic Fleet');
for (const a of pacific.arrivals) need(fleetNames.has(`${a[1]}|${a[2]}`), `arrival "${a[4]}": no fleet "${a[2]}"`);
for (const b of pacific.building) need(fleetNames.has(`${b.nat}|${b.fleet}`), `building row: no fleet "${b.fleet}"`);
for (const m of pacific.moves) {
  need(fleetNames.has(`${m.nat}|${m.fleet}`), `move "${m.words}": no fleet "${m.fleet}"`);
  need(owner.has(m.base) && seas.has(m.home), `move "${m.words}": ${m.base} or the sea "${m.home}" is not on the chart`);
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

// ---- The file ----------------------------------------------------------------------------------------
const out = {
  _about: "The far side's rows that are nobody's seat, written by tools/make-far-world.mjs from the Pacific planner's, the historian's and the clerk's files and never by hand. Keyed by the data file each block belongs in, as the seat builders' files are; the game lays it over its data only where the far side's switch is on (src/campaign/far-rows.ts). Japan is the seventh great power (ruling 141(a)): quality 1.0 (ruling 215), outside both blocs and leaning to the Axis (ruling 197), a sea lift of 6 (ruling 200). Its army is the planner's paper 17 (the historian's weights), its fleets paper 11 section 7 as ruling 189 amended them, with the names and admirals of ruling 208(f). The garrisons of Britain, the Netherlands, France, the Soviet Far East and America are paper 18's. America's fleets in the Pacific, the ships that join and leave them, and the bases by date of the Pacific Fleet and of Japan's battle line are the planner's pacific-fleets.json (rulings 185, 189, 208(f)); a fleet that forms later is a row of navy.json. 'manpower' is rulings 213(b), 216(b) and 218; China's row under 'traits' is the seat's with the levy of ruling 218. 'setup.held' is the clerk's held_1939 where it is not the owner.",
  _sources: Object.fromEntries(['pacific-planner/first-day-corps.json', 'pacific-planner/opening-armies.json', 'pacific-planner/pacific-fleets.json', 'historian/generals-usa-japan.json', 'historian/manpower-213.json', 'pacific-planner/runs-219/levy218-seats.json'].map((p) => [p, stamp(p)])),
  _draft: "Japan's words on the front door, its two abilities and its formation are the builder's draft from the record already in these files, for the historian and the UI director to mark: nobody had written them.",
  grand: {
    nations: [{
      id: 'JAP',
      years: { 1939: "Two years into a war in China that will not end. The coast, the railways and the great cities are yours, and the interior is not. The strongest fleet in the Pacific, six carriers, and oil for two years. Choose where the next blow falls." },
    }],
  },
  world: {
    nations: { JAP: { name: 'Japan', adj: 'Japanese', color: '#c4204a', quality: 1, air: 60, resilience: 0.95, align: 'leaning_axis', seaSupply: true, sealift: 6 } },
    order: ['JAP'],
    great: ['JAP'],
    parts: { of: { JAP: 'asia' } },
    corps: [...japan, ...garrisons],
    fleets: [...fleets, ...usFleets.filter((f) => f.from === undefined)],
  },
  navy: {
    fleets: usFleets.filter((f) => f.from !== undefined),
    arrivals: [...arrivals, ...pacific.arrivals].sort((a, b) => a[0] - b[0]),
    building: pacific.building,
    moves: pacific.moves,
  },
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
  setup: { held: Object.fromEntries(Object.entries(held).sort(([a], [b]) => (a < b ? -1 : 1))) },
  'setup-1939': dated,
};

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
