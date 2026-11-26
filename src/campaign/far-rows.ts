// The far side's rows.
//
// The countries of the far side of the world (Japan, the Pacific's six seats, the globe's eight, and what the old
// powers keep out there) are written in files of their own, in the shape their builders drafted them and the design
// director ruled on them: a "seats file", keyed by the data file each block belongs in.
//
//   data/campaign/far-seats-globe.json     South Africa, Iran, Iraq, Saudi Arabia, Afghanistan, Yemen, Nepal, Liberia
//   data/campaign/far-seats-pacific.json   China, Australia, New Zealand, Thailand, Manchukuo, Mongolia
//   data/campaign/far-world.json           Japan, the old powers' garrisons out there, the men of rulings 213 and 218,
//                                          the far side's shipyards (ruling b445), its oilfields (rulings 194, 231),
//                                          two towns of the old chart renamed
//                                          (written by tools/make-far-world.mjs from the planner's and the historian's files)
//   data/campaign/china-line.json          ruling 215: the record's line in China
//   data/campaign/china-setup.json         the dated rows its raids need
//   data/campaign/china-stop3.json         ruling 221: what Szechwan costs when history is loose
//
// While the far side's switch is off (stage.ts) nothing here happens and the game reads its data files as they are.
// With it on, each block is laid over the data file it names, in place and once, as this module loads: objects by
// key, lists appended (a row with an "id" takes the place of the row of that id; a row already there is not added
// twice), and a value under a nation's code (two or three capital letters) replaces what was there whole. Those are
// the far-side rig's rules (tools/trial/seats.mjs on the seat builders' branch), so a seats file that played there
// plays here unchanged.
//
// Every module that reads one of those data files imports this one first ("import './far-rows';"), so the rows are
// in before anything reads them. This file imports nothing of the game but the switch.
//
// Two blocks name no data file and are the far side's own: "setup" (the first day beyond the old chart: who holds
// what, capitals that move, wars standing, rows left out) and "door" (a seat's path on the front door's paper).

import { FAR_STAGE } from './stage';
import grand from '../../data/campaign/grand.json';
import world from '../../data/campaign/world.json';
import traits from '../../data/campaign/traits.json';
import eve from '../../data/campaign/eve.json';
import relations from '../../data/campaign/relations.json';
import diplomacy from '../../data/campaign/diplomacy.json';
import government from '../../data/campaign/government.json';
import commanders from '../../data/campaign/commanders.json';
import open from '../../data/campaign/open.json';
import setup1939 from '../../data/campaign/setup-1939.json';
import history from '../../data/campaign/history.json';
import east from '../../data/campaign/east.json';
import rivals from '../../data/campaign/rivals.json';
import manpower from '../../data/campaign/manpower.json';
import league from '../../data/campaign/league.json';
import navy from '../../data/campaign/navy.json';
import arsenal from '../../data/campaign/arsenal.json';
import towns from '../../data/campaign/towns.json';
import resources from '../../data/campaign/resources.json';
import globe from '../../data/campaign/far-seats-globe.json';
import pacific from '../../data/campaign/far-seats-pacific.json';
import farWorld from '../../data/campaign/far-world.json';
import chinaLine from '../../data/campaign/china-line.json';
import chinaSetup from '../../data/campaign/china-setup.json';
import chinaStop3 from '../../data/campaign/china-stop3.json';

type Obj = Record<string, any>;

/** The data files a seats file may name, by name without ".json". */
const TARGETS: Record<string, Obj> = {
  grand, world, traits, eve, relations, diplomacy, government, commanders, open, 'setup-1939': setup1939, history, east, rivals, manpower, league, navy, arsenal, towns, resources,
};

/** The seats files, in the order they are laid. */
const FILES: [string, Obj][] = [
  ['far-seats-globe', globe], ['far-seats-pacific', pacific], ['far-world', farWorld],
  ['china-line', chinaLine], ['china-setup', chinaSetup], ['china-stop3', chinaStop3],
];

/** The first day beyond the old chart, as the seats files give it. */
export interface FarSetup {
  /** Province by name: the country that holds it on 1 September 1939 where that is not its owner. */
  held: Record<string, string>;
  /** Nation: the province its capital stands in once the whole world is on the chart. */
  capitals: Record<string, string>;
  /** [nation, corps name]: first-day rows of the older files that are left out on the whole world. */
  drop: [string, string][];
  /** Wars standing on the first day. */
  wars: [string, string][];
  /** Provinces of the old chart whose oil mark comes off once the whole world is on the chart (ruling 231: the Caspian's oil stands at Baku). */
  unmark: string[];
}

export const FAR_SETUP: FarSetup = { held: {}, capitals: {}, drop: [], wars: [], unmark: [] };

/** A seat's path, as the front door prints it under ITS PATH: nation, words. */
export const FAR_PATHS: Record<string, string> = {};

/** The files that were laid, for a test to ask. Empty while the switch is off. */
export const FAR_LAID: string[] = [];

const CODE = /^[A-Z]{2,3}$/;
const isObj = (v: unknown): v is Obj => v !== null && typeof v === 'object' && !Array.isArray(v);
const same = (a: unknown, b: unknown): boolean => JSON.stringify(a) === JSON.stringify(b);
/** A copy of a seats file's value: what is laid into the data is never the seats file's own object. */
const copy = <T>(v: T): T => (v !== null && typeof v === 'object' ? JSON.parse(JSON.stringify(v)) : v);

/** Lays 'add' over 'base' in place, by the rig's rules, and gives back what now stands there. */
export function lay(base: any, add: any, key = ''): any {
  if (CODE.test(key)) return copy(add);
  if (Array.isArray(base) && Array.isArray(add)) {
    for (const row of add) {
      if (isObj(row) && 'id' in row) {
        const i = base.findIndex((x) => isObj(x) && x.id === row.id);
        if (i >= 0) base[i] = copy(row); else base.push(copy(row));
      } else if (!base.some((x) => same(x, row))) base.push(copy(row));
    }
    return base;
  }
  if (isObj(base) && isObj(add)) {
    for (const [k, v] of Object.entries(add)) base[k] = k in base ? lay(base[k], v, k) : copy(v);
    return base;
  }
  return copy(add);
}

if (FAR_STAGE) {
  for (const [name, file] of FILES) {
    for (const [key, block] of Object.entries(file)) {
      if (key.startsWith('_') || !isObj(block)) continue;
      const k = key.replace(/\.json$/, '');
      if (k === 'setup') lay(FAR_SETUP, block);
      else if (k === 'door') lay(FAR_PATHS, block.path ?? {});
      else if (TARGETS[k]) lay(TARGETS[k], block);
      else throw new Error(`${name}.json: "${key}" names no data file the far side lays rows over`);
    }
    FAR_LAID.push(name);
  }
}
