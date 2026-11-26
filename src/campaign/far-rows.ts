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
//   data/campaign/far-pacific-war.json     rulings 197 and 200: the Pacific war's events, Japan's great choice and its marks
//   data/campaign/far-south-line.json      ruling 234(a): the record's line in the south
//   data/campaign/far-air.json             rulings 184 and 187: the Pacific's land air groups by date, Japan's air industry
//                                          (written by tools/make-far-air.mjs from the planner's rows)
//   data/campaign/far-guards.json          the far chart's town guard, land by land, at the record's figures (the
//                                          design director's rulings of 21 November 2026 on the historian's tables)
//   data/campaign/far-shores.json          ruling 234's notes of 22:05Z and 22:10Z of 20 November 2026: the owed
//                                          shores' rows at the record's weights, their works and their will
//   data/campaign/far-africa.json          the design director's C of 11:40Z and 13:09Z of 22 November 2026: Italian
//                                          East Africa and the Sahara posts by the record, and the forces facing them
//   data/campaign/far-books.json           rulings 194 to 196: Japan's stocks of oil and supplies, the lanes that
//                                          feed them, and America's submarines' worth against them
//   data/campaign/far-arms.json            the design director's 05:37Z and 05:44Z of 25 November 2026: China's
//                                          rifles, its three arsenals and the four roads that bring it arms
//
// While the far side's switch is off (stage.ts) nothing here happens and the game reads its data files as they are.
// With it on, each block is laid over the data file it names, in place and once, as this module loads: objects by
// key, lists appended (a row with an "id" takes the place of the row of that id; a row already there is not added
// twice), and a value under a nation's code (two or three capital letters) replaces what was there whole. Those are
// the far-side rig's rules (tools/trial/seats.mjs on the seat builders' branch), so a seats file that played there
// plays here unchanged. One rule is this file's own: a nation's code followed by "+" ("JAP+") lays its value over
// what stands under that code, by the same rules, in place of replacing it: so two files can each give Japan great
// choices. Their choices stand in the list in the order the files are laid, and staff.ts takes one a month, in the
// list's order: so far-pacific-war.json, whose strike south must not wait behind a raid in China, is laid before
// china-line.json.
//
// Every module that reads one of those data files imports this one first ("import './far-rows';"), so the rows are
// in before anything reads them. This file imports nothing of the game but the switch.
//
// Three blocks name no data file and are the far side's own: "setup" (the first day beyond the old chart: who holds
// what, capitals that move, wars standing, rows left out), "door" (a seat's path on the front door's paper) and
// "rows" (a hand-kept file's amendments to rows that other seats files lay: far-world.json and far-seats-pacific.json
// are written by their tools from the planner's and the historian's files and never by hand, and a row of theirs
// that a later ruling re-weighs is amended here, by its country and name, once every file is laid. "as" gives the
// row another country's code; "set" lays words over the row's own. An amendment that finds no row is an error.)

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
import air from '../../data/campaign/air.json';
import siege from '../../data/campaign/siege.json';
import globe from '../../data/campaign/far-seats-globe.json';
import pacific from '../../data/campaign/far-seats-pacific.json';
import farWorld from '../../data/campaign/far-world.json';
import pacificWar from '../../data/campaign/far-pacific-war.json';
import chinaLine from '../../data/campaign/china-line.json';
import chinaSetup from '../../data/campaign/china-setup.json';
import chinaStop3 from '../../data/campaign/china-stop3.json';
import southLine from '../../data/campaign/far-south-line.json';
import farAir from '../../data/campaign/far-air.json';
import farShores from '../../data/campaign/far-shores.json';
import farGuards from '../../data/campaign/far-guards.json';
import farAfrica from '../../data/campaign/far-africa.json';
import farBooks from '../../data/campaign/far-books.json';
import farArms from '../../data/campaign/far-arms.json';
import japanSeat from '../../data/campaign/far-japan-seat.json';
import goals from '../../data/campaign/goals.json';
import tech from '../../data/campaign/tech.json';
import arms from '../../data/campaign/arms.json';
import rifles from '../../data/campaign/rifles.json';

type Obj = Record<string, any>;

/** The data files a seats file may name, by name without ".json". */
const TARGETS: Record<string, Obj> = {
  grand, world, traits, eve, relations, diplomacy, government, commanders, open, 'setup-1939': setup1939, history, east, rivals, manpower, league, navy, arsenal, towns, resources, air, siege,
  // (What a player of a far seat plays for, and what the far side adds to an old seat's goals: ruling 196, points 6 and 7.)
  goals,
  // (What a far great power opens with in the tree: ruling 68(a), starting lists and never private technologies.)
  tech,
  // (A nation's stock of arms and its rifles' rule: the design director's 05:37Z and 05:44Z of 25 November 2026.)
  arms, rifles,
};

/** The seats files, in the order they are laid. */
const FILES: [string, Obj][] = [
  ['far-seats-globe', globe], ['far-seats-pacific', pacific], ['far-world', farWorld],
  // (Before the China front's: both give Japan great choices under "JAP+", and the strike south has the first call.)
  ['far-pacific-war', pacificWar],
  ['china-line', chinaLine], ['china-setup', chinaSetup], ['china-stop3', chinaStop3],
  // (The record's line in the south, ruling 234(a): after China's, whose line stands first in the list.)
  ['far-south-line', southLine],
  // (The Pacific's air forces, rulings 184 and 187.)
  ['far-air', farAir],
  // (The far chart's town guard, land by land: siege.json's 'farGuard', read by guardOf alone.)
  ['far-guards', farGuards],
  // (Italian East Africa and the Sahara posts, the design director's C of 22 November 2026: dated rows and the six
  // seats' depot marks. Before the shores file, which stays last.)
  ['far-africa', farAfrica],
  // (Japan's books, rulings 194 to 196: stocks, lanes and the submarines that raid them.)
  ['far-books', farBooks],
  // (China's rifles, the design director's 05:37Z and 05:44Z of 25 November 2026: its arsenals and roads, after the books.)
  ['far-arms', farArms],
  // (Japan's seat, ruling 196 points 6 and 7: its two goals, and Japan in America's long one.)
  ['far-japan-seat', japanSeat],
  // (The owed shores, ruling 234's notes of 22:05Z and 22:10Z: last, so its amendments find every row laid.)
  ['far-shores', farShores],
];

/** A row another seats file lays, amended: the data file and its list, the row's country and name, and what changes (its country, its place, its words). */
interface Amend { file: string; list: string; nat: string; name: string; src?: string; as?: string; at?: string; set?: Obj }

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
  /**
   * Provinces of the far side that carry the map's depot mark: their owner's army is fed from them while he holds
   * them, as from a depot of the old chart (the design director's 13:43Z of 22 November 2026: Italian East Africa's
   * six command seats, far-africa.json).
   */
  depots: string[];
}

export const FAR_SETUP: FarSetup = { held: {}, capitals: {}, drop: [], wars: [], unmark: [], depots: [] };

/** A seat's path, as the front door prints it under ITS PATH: nation, words. */
export const FAR_PATHS: Record<string, string> = {};

/** The files that were laid, for a test to ask. Empty while the switch is off. */
export const FAR_LAID: string[] = [];

const CODE = /^[A-Z]{2,3}$/;
/** A nation's code with "+": its value is laid over what stands under the code, not put in its place. */
const MORE = /^[A-Z]{2,3}\+$/;
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
    for (const [k0, v] of Object.entries(add)) {
      const more = MORE.test(k0), k = more ? k0.slice(0, -1) : k0;
      base[k] = k in base ? lay(base[k], v, more ? '' : k) : copy(v);
    }
    return base;
  }
  return copy(add);
}

/**
 * Amends one row in place. A dated row begins with its month ([month, country, name, kind, divisions, place, words]);
 * a first-day row with its country ([country, name, kind, divisions, place, words]).
 */
export function amendRow(targets: Record<string, Obj>, from: string, a: Amend): void {
  // (Each line names the tool-written file whose row it amends: it leaves when that file's tool takes the figure.)
  if (!a.src) throw new Error(`${from}.json: the amended row "${a.name}" names no source file`);
  const list = targets[a.file]?.[a.list];
  if (!Array.isArray(list)) throw new Error(`${from}.json: an amended row names no list "${a.file}.${a.list}"`);
  const at = (r: any[]): number => (typeof r[0] === 'number' ? 1 : 0);
  const row = list.find((r) => Array.isArray(r) && r[at(r)] === a.nat && r[at(r) + 1] === a.name);
  if (!row) throw new Error(`${from}.json: no row "${a.name}" of ${a.nat} stands in ${a.file}.${a.list} to amend`);
  const k = at(row);
  if (a.as) row[k] = a.as;
  // (The place, where the record stood the row elsewhere: it keeps the name a later dated row takes it by.)
  if (a.at) row[k + 4] = a.at;
  if (a.set) row[k + 5] = { ...(row[k + 5] ?? {}), ...copy(a.set) };
}

if (FAR_STAGE) {
  const amends: [string, Amend][] = [];
  for (const [name, file] of FILES) {
    for (const [key, block] of Object.entries(file)) {
      if (key.startsWith('_') || !isObj(block)) continue;
      const k = key.replace(/\.json$/, '');
      if (k === 'setup') lay(FAR_SETUP, block);
      else if (k === 'door') lay(FAR_PATHS, block.path ?? {});
      else if (k === 'rows') for (const a of (block.amend ?? []) as Amend[]) amends.push([name, a]);
      else if (TARGETS[k]) lay(TARGETS[k], block);
      else throw new Error(`${name}.json: "${key}" names no data file the far side lays rows over`);
    }
    FAR_LAID.push(name);
  }
  for (const [name, a] of amends) amendRow(TARGETS, name, a);
}
