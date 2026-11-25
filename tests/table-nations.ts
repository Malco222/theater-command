// The 3D table's own list of countries, read here the way the game reads it (release 2; the art director,
// 25 November 2026). The table is built and painted before the rules are started (ACampaignGameMode::InitGame), so
// it cannot ask them who the countries are: it reads the data files (FCampaignMapData::Load and LoadNations,
// CampaignMapData.cpp) and stands in a bare country for any owner on the far side with no row, as world.ts
// standInCountries does. The two tests beside this file set that list against TC.nationsBook(), every country the
// rules know, on each map: a country the rules gain that the table would not know fails there, at the rules'
// pre-check, and not on the Mac as grey ground under a three-letter name.
import CPP from '../unreal/TheaterCommand/Source/TheaterCommand/Campaign/CampaignMapData.cpp?raw';
import globe from '../data/campaign/far-seats-globe.json?raw';
import pacific from '../data/campaign/far-seats-pacific.json?raw';
import farWorld from '../data/campaign/far-world.json?raw';
import nations from '../data/campaign/nations.json?raw';
import world from '../data/campaign/world.json?raw';

/**
 * The data files by the names the table's own code calls them, each read as its text, as the table reads it from the
 * disk. (Not as the rules import them: the far side's rows are laid into the rules' own copy of world.json as their
 * bundle loads, far-rows.ts, and the table reads the file, not that copy.)
 */
const FILES: Record<string, string> = {
  'nations.json': nations, 'world.json': world, 'far-world.json': farWorld, 'far-seats-pacific.json': pacific, 'far-seats-globe.json': globe,
};

export type Chart = 'europe' | 'americas' | 'whole';
/** A row of TC.nationsBook(). */
export interface BookRow { tag: string; name: string; adjective: string; colour: string; alignment: string }
/** A country as the table has it before its own paints are laid over the data's colours. */
export interface TableRow { name: string; adjective: string; colour: string; from: string }

/** FCampaignMapData::Load's own lines: the files it reads countries from, in its order. */
export function filesTheTableNames(): string[] {
  const first = /NationsPath = ResolveDataFile\(TEXT\("([\w.-]+)"\), TEXT\("data\/campaign"\)\)/.exec(CPP);
  const later = [...CPP.matchAll(/LoadNations\(ResolveDataFile\(TEXT\("([\w.-]+)"\), TEXT\("data\/campaign"\)\)/g)].map((m) => m[1]);
  return [first ? first[1] : '', ...later];
}

/** The same files as this test knows them, by the map they are read for: the wider chart adds one, the far side three more. */
export const TABLE_FILES: Record<Chart, string[]> = {
  europe: ['nations.json'],
  americas: ['nations.json', 'world.json'],
  whole: ['nations.json', 'world.json', 'far-world.json', 'far-seats-pacific.json', 'far-seats-globe.json'],
};

/** The table's stand-ins by name (its StandIns rows), and the grey every other bare owner takes. */
export function tableStandIns(): { rows: Record<string, [string, string]>; grey: string } {
  const block = /StandIns\[\]\[3\] = \{([\s\S]*?)\};/.exec(CPP);
  const rows: Record<string, [string, string]> = {};
  for (const m of (block ? block[1] : '').matchAll(/\{ TEXT\("(\w+)"\), TEXT\("([^"]+)"\), TEXT\("([0-9A-Fa-f]{6})"\) \}/g)) rows[m[1]] = [m[2], m[3].toLowerCase()];
  const grey = /Nation\.Color = FColor::FromHex\(TEXT\("([0-9A-Fa-f]{6})"\)\);/.exec(CPP);
  return { rows, grey: grey ? grey[1].toLowerCase() : '' };
}

const hex = (c: unknown): string => String(c ?? '').replace(/^#/, '').toLowerCase();

/** The countries the table reads for a map, as LoadNations reads them: the first row of a tag wins, a key that begins "_" is a note. */
export function tableNations(chart: Chart): Record<string, TableRow> {
  const out: Record<string, TableRow> = {};
  for (const file of TABLE_FILES[chart]) {
    const root = JSON.parse(FILES[file]) as Record<string, any>;
    const isRows = (v: unknown): v is Record<string, any> => !!v && typeof v === 'object' && !Array.isArray(v);
    const from = isRows(root.nations) ? root.nations : isRows(root.world) && isRows(root.world.nations) ? root.world.nations : root;
    for (const [tag, row] of Object.entries(from)) {
      if (tag.startsWith('_') || !isRows(row) || out[tag]) continue;
      out[tag] = { name: String(row.name ?? ''), adjective: String(row.adj ?? ''), colour: hex(row.color), from: file };
    }
  }
  return out;
}

/**
 * Where the table and the rules differ on a map, a line for each (none where they agree). A country the table has
 * from a file must carry the rules' name and colour, and their adjective where its row gives one. (Where a country
 * stands is the rules' alone: the table shows it nowhere.) A country it has from no file is one of its stand-ins on
 * the far side (the rules' name and colour again), and on a narrower map is a fault: the table stands nobody in there.
 */
export function tableAgainstBook(chart: Chart, book: BookRow[]): string[] {
  const table = tableNations(chart);
  const stand = tableStandIns();
  const faults: string[] = [];
  for (const n of book) {
    const row = table[n.tag];
    if (!row) {
      if (chart !== 'whole') { faults.push(`${n.tag} (${n.name}): the rules know it on this map and no file the table reads has it`); continue; }
      const [name, colour] = stand.rows[n.tag] ?? [n.tag, stand.grey];
      if (name !== n.name) faults.push(`${n.tag}: no file has it, and the table's stand-in would call it "${name}" where the rules say "${n.name}"`);
      if (colour !== hex(n.colour)) faults.push(`${n.tag}: no file has it, and the table's stand-in would paint it ${colour} where the rules say ${hex(n.colour)}`);
      continue;
    }
    if (row.name !== n.name) faults.push(`${n.tag}: ${row.from} calls it "${row.name}" and the rules "${n.name}"`);
    if (row.colour !== hex(n.colour)) faults.push(`${n.tag}: ${row.from} paints it ${row.colour} and the rules ${hex(n.colour)}`);
    if (row.adjective && row.adjective !== n.adjective) faults.push(`${n.tag}: ${row.from} says "${row.adjective}" and the rules "${n.adjective}"`);
  }
  return faults;
}
