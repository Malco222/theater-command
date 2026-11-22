// The set-piece stages of the air battle (ruling 238, the design director, 19 November 2026): Pearl Harbor and
// Midway, chosen by place and not by script. Any raid on the harbour of Pearl Harbor is fought over Oahu, and any
// strike in Midway Waters over the atoll, in whatever month it comes. A stage changes only what is drawn and where:
// the ships at the record's moorings, the defending fighters rising from the island's own fields. The counts are the
// campaign's, as for every air battle (ruling 217).
//
// The data (data/battle/air-stages.json) is in metres east and north of each stage's origin, the frame of the 3D
// game's stage geometry (unreal/TheaterCommand/Content/Data/Stages/<key>.json). The ticket turns it onto the board
// as it turns the campaign's map: the raid comes on from the south edge.

import data from '../../../data/battle/air-stages.json';
import type { ShipClass } from '../../campaign/types';
import type { When } from './doctrine';

export type Pt = [number, number];

/** A mooring the record has: its class that day ('bb'..'ss', or a class the campaign does not have), and its name. */
export interface StageBerth { id: string; cls: string; name: string | null; at: Pt; face: number; open: number; note?: string; dock?: boolean }

export interface StageRow {
  name: string;
  /** The regions whose harbour is this stage, and the seas whose strikes are. */
  places: string[];
  seas: string[];
  origin: Pt;
  /** The record's date (month 0 is January), and whose ships lay at its berths. */
  record: { year: number; month: number; nat?: string };
  /** Where a raid of each kind goes for. */
  aim: { harbour?: Pt; fleet?: Pt };
  fields: { name: string; at: Pt; kind: string }[];
  berths: StageBerth[];
  spots: { at: Pt; face: number; nest?: boolean }[];
  /** The record's names for ships at sea on its date, by nation and class, in the record's order. */
  fleets?: Record<string, Partial<Record<ShipClass, string[]>>>;
}

export const STAGES = (data as unknown as { stages: Record<string, StageRow> }).stages;

/** The stage a strike on ships is fought on: by the harbour's region, or the sea the fleet struck is in. */
export function stageOf(kind: 'harbour' | 'fleet', region: string | null, sea: string | null): string | null {
  for (const [key, s] of Object.entries(STAGES)) {
    if (!s.aim[kind]) continue;
    if (kind === 'harbour' && region !== null && s.places.includes(region)) return key;
    if (kind === 'fleet' && sea !== null && s.seas.includes(sea)) return key;
  }
  return null;
}

/**
 * Where the stage lies on the board: its key, the aim of the raid in stage metres, and the board's turn (radians,
 * counter-clockwise), the ticket's own. A stage point p is at T + turn(p - aim) on the board.
 */
export interface StagePlace { key: string; aim: Pt; turn: number }

/** A stage point (metres east and north of the stage's origin) on the board, round the board's target T. */
export function onBoard(st: StagePlace, p: Pt, T: { x: number; y: number }): Pt {
  const dx = p[0] - st.aim[0], dy = p[1] - st.aim[1], c = Math.cos(st.turn), s = Math.sin(st.turn);
  return [T.x + dx * c - dy * s, T.y + dx * s + dy * c];
}

/** A compass heading on the stage (degrees) as a heading on the board (radians, the sim's own: 0 is up the board). */
export const faceOnBoard = (st: StagePlace, deg: number): number => (deg * Math.PI) / 180 - st.turn;

/** Whether the battle is fought in the record's own month: only then do the berths and ships carry the record's names (ruling 238(4)). */
export const recordMonth = (key: string, w: When): boolean => STAGES[key].record.year === w.year && STAGES[key].record.month === w.month;
