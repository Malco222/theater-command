// The islands' rows for the rules, written from the historian's table (rulings 173 and 179): each island's room in
// corps, the floor a cut-off garrison withers to, and whether it is an islet. The table is the source
// (data/campaign/tables/islands-draft.csv: edit it, never this file's output); run `node tools/make-far-islands.mjs`
// after the table changes, and tests/far-islands.test.ts fails until it has been.
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

export function islandRows() {
  const [head, ...rows] = csv(fs.readFileSync(path.join(ROOT, 'data/campaign/tables/islands-draft.csv'), 'utf8'));
  const at = (k) => { const i = head.indexOf(k); if (i < 0) throw new Error(`islands table: no column "${k}"`); return i; };
  const iName = at('name'), iRoom = at('room'), iFloor = at('floor'), iIslet = at('islet');
  const chart = JSON.parse(fs.readFileSync(path.join(ROOT, 'src/generated/world-whole.json'), 'utf8')).regions;
  const onChart = new Set(chart.map((r) => r.name));
  const islands = {};
  for (const r of rows) {
    const name = r[iName];
    if (!name) continue;
    if (!onChart.has(name)) throw new Error(`islands table: "${name}" is no province of the whole-world chart`);
    const room = Number(r[iRoom]), floor = r[iFloor];
    if (!(room >= 1) || !['quarter', 'half'].includes(floor)) throw new Error(`islands table: "${name}" has room "${r[iRoom]}" and floor "${floor}"`);
    islands[name] = [room, floor, r[iIslet] === '1' ? 1 : 0];
  }
  // An islet the chart marks and the table has no row for (the Indian Ocean's two): an atoll's room and floor.
  const noRow = [];
  for (const r of chart) if (r.flags?.islet && !islands[r.name]) { islands[r.name] = [1, 'quarter', 1]; noRow.push(r.name); }
  return { islands, noRow };
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const { islands, noRow } = islandRows();
  const lines = Object.keys(islands).map((k) => `    ${JSON.stringify(k)}: ${JSON.stringify(islands[k])}`);
  const out = `{\n  "_about": "The Pacific's islands for the rules (rulings 173 and 179), read only on the whole-world chart: [room in corps, the floor a cut-off garrison withers to, 1 for an islet]. Written by tools/make-far-islands.mjs from data/campaign/tables/islands-draft.csv, which is the source: edit the table, never this file.",\n  "_noRow": ${JSON.stringify(noRow)},\n  "islands": {\n${lines.join(',\n')}\n  }\n}\n`;
  fs.writeFileSync(path.join(ROOT, 'data/campaign/far-islands.json'), out);
  console.log(`far-islands.json: ${Object.keys(islands).length} islands (${noRow.length} islets with no table row: ${noRow.join(', ')})`);
}
