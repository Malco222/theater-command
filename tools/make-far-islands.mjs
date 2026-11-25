// The islands' rows for the rules, written from the historian's table (rulings 173 and 179): each island's room in
// corps, the floor a cut-off garrison withers to, and whether it is an islet. The table is the source
// (data/campaign/tables/islands-draft.csv: edit it, never this file's output); run `node tools/make-far-islands.mjs`
// after the table changes, and tests/far-islands.test.ts fails until it has been.
//
// A 4th element is the island's look for the 3D ground battle (the design director, 25 November 2026, 12:39Z, B(3),
// narrowed at 13:00Z, D(2)), read off the table's own columns, with one row named by hand: see lookOf below.
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
export function csv(text) {
  const rows = []; let row = [], cell = '', q = false;
  for (let i = 0; i < text.length; i++) {
    const ch = text[i];
    if (q) { if (ch === '"' && text[i + 1] === '"') { cell += '"'; i++; } else if (ch === '"') q = false; else cell += ch; }
    else if (ch === '"') q = true;
    else if (ch === ',') { row.push(cell); cell = ''; } else if (ch === '\n') { row.push(cell); rows.push(row); row = []; cell = ''; } else if (ch !== '\r') cell += ch;
  }
  if (cell !== '' || row.length) { row.push(cell); rows.push(row); }
  return rows;
}

/**
 * The look the 3D ground battle dresses an island's field in, by the table's own columns (the design director, 25
 * November 2026, 12:39Z, B(3), as narrowed at 13:00Z, D(2); the ground battles thread's pacific-ground-1125.md).
 * "jungle" where the table's jungle column says 1 (a big island's fight was inland); otherwise "island" (beach, reef and
 * palms) only where the table's islet flag is 1; otherwise no word, and the 3D game dresses the field by its own climate
 * from the place's latitude and longitude, as it does any other place. No word, whatever the flags, where the climate is
 * subarctic (Attu, Kiska: treeless tundra, no cold island look exists) or the ground is urban (Singapore: a city island),
 * until the art director has seen stills of them and scored them.
 */
// The one row named by hand (13:00Z, D(2)): Iwo Jima is an islet by the table, but its ground is black volcanic ash with
// no palms, so the beach-and-palms look would be false there. It takes no word until the art brief has its look.
export const NO_LOOK_BY_NAME = ['Iwo Jima'];
export function lookOf(name, ground, jungle, climate, islet) {
  if (climate === 'subarctic' || ground === 'urban' || NO_LOOK_BY_NAME.includes(name)) return '';
  if (jungle === '1') return 'jungle';
  return islet === '1' ? 'island' : '';
}

export function islandRows() {
  const [head, ...rows] = csv(fs.readFileSync(path.join(ROOT, 'data/campaign/tables/islands-draft.csv'), 'utf8'));
  const at = (k) => { const i = head.indexOf(k); if (i < 0) throw new Error(`islands table: no column "${k}"`); return i; };
  const iName = at('name'), iRoom = at('room'), iFloor = at('floor'), iIslet = at('islet');
  const iGround = at('ground'), iJungle = at('jungle'), iClimate = at('climate');
  const chart = JSON.parse(fs.readFileSync(path.join(ROOT, 'src/generated/world-whole.json'), 'utf8')).regions;
  const onChart = new Set(chart.map((r) => r.name));
  const islands = {};
  for (const r of rows) {
    const name = r[iName];
    if (!name) continue;
    if (!onChart.has(name)) throw new Error(`islands table: "${name}" is no province of the whole-world chart`);
    const room = Number(r[iRoom]), floor = r[iFloor];
    if (!(room >= 1) || !['quarter', 'half'].includes(floor)) throw new Error(`islands table: "${name}" has room "${r[iRoom]}" and floor "${floor}"`);
    islands[name] = [room, floor, r[iIslet] === '1' ? 1 : 0, lookOf(name, r[iGround], r[iJungle], r[iClimate], r[iIslet])];
  }
  // An islet the chart marks and the table has no row for (the Indian Ocean's two): an atoll's room and floor. Its look
  // is "island" by the islet flag alone, a guess (G): no climate, ground or jungle has been read for it.
  const noRow = [];
  for (const r of chart) if (r.flags?.islet && !islands[r.name]) { islands[r.name] = [1, 'quarter', 1, 'island']; noRow.push(r.name); }
  return { islands, noRow };
}

// Where each chart islet with no table row stands (the design director, 25 November 2026, 13:00Z, C): Addu Atoll's row
// is ruled but is not in the table yet, and the Seychelles stay without one. The note says which, so a reader of the
// file does not take Addu Atoll for a place nobody has read.
const ROW_SAYS = {
  'Addu Atoll': 'its row is ruled (13:00Z, C: fleet base 1, field 2, works 1) but is not in the table yet',
  Seychelles: "it stays without a row (13:00Z, C), its look 'island' by the islet flag",
};

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const { islands, noRow } = islandRows();
  const lines = Object.keys(islands).map((k) => `    ${JSON.stringify(k)}: ${JSON.stringify(islands[k])}`);
  const said = (names) => (names.length > 1 ? `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}` : names.join(''));
  const bare = Object.keys(islands).filter((k) => islands[k][3] === '');
  const about = "The Pacific's islands for the rules (rulings 173 and 179), read only on the whole-world chart: [room in corps, the floor a cut-off garrison withers to, 1 for an islet, the look of its field in the 3D ground battle]. Written by tools/make-far-islands.mjs from data/campaign/tables/islands-draft.csv, which is the source: edit the table, never this file.";
  const look = `The 4th element dresses the 3D ground battle's field and nothing else (the design director, 25 November 2026, 12:39Z, B(3), narrowed at 13:00Z, D(2)): no rule of the campaign or of the fight reads it. By the table's own columns: 'jungle' where jungle is 1; otherwise 'island' only where the islet flag is 1; otherwise no word (''), and the 3D game dresses the field by its climate from the place's latitude and longitude. No word, whatever the flags, where the climate is subarctic or the ground is urban, and for Iwo Jima by name (an islet of black volcanic ash with no palms), until the art director has scored stills of them. Rows with no word: ${said(bare)}. (G) ${said(noRow)} have no row in the table, so no climate, ground or jungle was read for them: they take 'island' by the chart's islet flag alone, a guess (${noRow.map((n) => `${n}: ${ROW_SAYS[n] ?? 'no ruling yet'}`).join('; ')}).`;
  const out = `{\n  "_about": ${JSON.stringify(about)},\n  "_look": ${JSON.stringify(look)},\n  "_noRow": ${JSON.stringify(noRow)},\n  "islands": {\n${lines.join(',\n')}\n  }\n}\n`;
  fs.writeFileSync(path.join(ROOT, 'data/campaign/far-islands.json'), out);
  const count = (w) => Object.values(islands).filter((r) => r[3] === w).length;
  console.log(`far-islands.json: ${Object.keys(islands).length} islands (${noRow.length} islets with no table row: ${noRow.join(', ')}); looks: ${count('jungle')} jungle, ${count('island')} island, ${count('')} with no word`);
}
