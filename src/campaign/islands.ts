// The Pacific's islands as the historian's table sets them out (rulings 173 and 179; the design director, 25 November
// 2026, form B3). An island has room for so many corps and no more (an atoll for one), and a garrison cut off there
// withers to a floor, a quarter on an atoll and a half on an island large enough to feed itself (173(e), rider (o)).
// The rows are data/campaign/far-islands.json, written from data/campaign/tables/islands-draft.csv by
// tools/make-far-islands.mjs. They are read on the whole-world chart alone: with the far side off, and for ground
// that is no island of the table, nothing here answers, and every rule that asks reads as it always did.
// (This file and game.ts read each other's functions, as world.ts and game.ts do: nothing of either runs as they load.)
import data from '../../data/campaign/far-islands.json';
import { FAR_STAGE } from './stage';
import { allied, roomless } from './game';
import { STACK, type Game, type Region } from './types';
import { farOn } from './world';

/** An island's row: its room in corps, the floor a cut-off garrison withers to, and whether it is an islet. */
export interface Island { room: number; floor: number; islet: boolean }

type Row = [number, 'quarter' | 'half', 0 | 1];
const ROWS: Record<string, Row> | null = FAR_STAGE ? (data as unknown as { islands: Record<string, Row> }).islands : null;

/** The island a province is, by the table. Null with the far side off, and for ground that is no island of the table. */
export function islandOf(g: Game, r: Region | undefined | null): Island | null {
  if (!ROWS || !r || !farOn(g)) return null;
  const row = ROWS[r.name];
  return row ? { room: row[0], floor: row[1] === 'quarter' ? 0.25 : 0.5, islet: row[2] === 1 } : null;
}

/**
 * How many corps of a side a province has room for in its field (ruling 173(b): each island carries "its room, in
 * corps"; ruling 179, rider (s): "the limit of an open beach is the island's room of one corps and what the corps
 * carries"). An island of the table on the whole-world chart: its room, less the corps of that side already inside
 * its town, who stand on the island as much as those outside it (an atoll with its corps in the town has room for
 * nobody more of that side; the enemy who lands counts only his own). All other ground, and every province with the
 * far side off: the five a province has always held (STACK).
 *
 * This is the one count every rule of room reads where a province is at hand: a march, a train, a ship, a landing,
 * a new corps, the winners of a fight, and the staffs' own placing. It is counted as the five are: a side's corps,
 * engineers apart where the rule already sets them apart, fortress crews apart under the fortress rows. Nobody is
 * ever moved or struck off to keep it: corps over an island's room (a beaten army fallen back on it, a row the
 * record put there) stand until their side moves them.
 *
 * The record's own hand is the one thing that does not read it: a dated row is put down at the record's place, and
 * the book's station taken, counted against five as before (game.ts roomNear and expeditions, stations.ts). The
 * record was written before this table; where the two disagree the row is not sent away, kept waiting or struck,
 * and the historian mends the one or the other (form B3 of 25 November 2026, (c)).
 * (This is one reading of (c). The other, a row for a full island sent on or kept waiting at the island's own room
 * and the book's stations counted against it, was built and measured on 25 November 2026 and left out: by the book,
 * at stop 1 of the history dial, the British divisions of Malaya could not step back onto Singapore Island, which
 * has room for two, so Johore held and Singapore still stood in month 40 where it had fallen in month 31; the guard
 * rows of Kwajalein, Yap and Chichi Jima waited two months for their landing forces to sail; and the standing tests
 * of the islands' garrisons, the lent rows, Oahu's rows and the book's stations failed. The record is to be mended,
 * or the reading ruled, before the record's hand reads the table.)
 */
export function roomAt(g: Game, r: Region | undefined | null, nat: string): number {
  if (!ROWS || !r || !farOn(g)) return STACK;
  const row = ROWS[r.name];
  if (!row) return STACK;
  // (Each corps in the town is asked for itself: only those of this side take this side's room.)
  const inTown = r.garr ? r.garr.filter((c) => allied(g, nat, c.nat) && !roomless(g, c)).length : 0;
  return Math.max(0, row[0] - inTown);
}

/** How a room is said: "one corps", "4 corps". */
const corpsWord = (n: number): string => (n === 1 ? 'one corps' : `${n} corps`);

/**
 * What a full island tells the player who orders one more corps onto it: "Wake has room for one corps." Null for
 * ground that is no island of the table (and with the far side off), where each rule keeps the sentence it had.
 */
export function roomSays(g: Game, r: Region | undefined | null): string | null {
  const isle = islandOf(g, r);
  return isle && r ? `${r.name} has room for ${corpsWord(isle.room)}.` : null;
}

/** Every island of the table, by its province's name (for the lists an interface or a test draws). */
export const islandNames = (): string[] => (ROWS ? Object.keys(ROWS) : []);
