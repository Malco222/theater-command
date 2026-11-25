// The Pacific's islands as the historian's table sets them out (rulings 173 and 179; the design director, 25 November
// 2026, form B3). An island has room for so many corps and no more (an atoll for one), and a garrison cut off there
// withers to a floor, a quarter on an atoll and a half on an island large enough to feed itself (173(e), rider (o)).
// The rows are data/campaign/far-islands.json, written from data/campaign/tables/islands-draft.csv by
// tools/make-far-islands.mjs. They are read on the whole-world chart alone: with the far side off, and for ground
// that is no island of the table, nothing here answers, and every rule that asks reads as it always did.
import data from '../../data/campaign/far-islands.json';
import { FAR_STAGE } from './stage';
import type { Game, Region } from './types';
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

/** Every island of the table, by its province's name (for the lists an interface or a test draws). */
export const islandNames = (): string[] => (ROWS ? Object.keys(ROWS) : []);
