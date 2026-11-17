// A ticket made by hand, for tests, the fairness table and the page's practice field: two armies of the usual make
// on any ground, at any odds. Not a campaign battle: nothing is handed back from it.

import type { FieldTicket, ForceTicket, SideTicket } from '../core/types';
import type { LandPlace, Terrain } from './field';
import { LAND } from './rules';

const force = (corps: number, name: string, nat: string, kind: string, divs: number, share: number, dug = false, first = 1): ForceTicket =>
  ({ corps, name, nat, kind, divs, men: divs * 15000 * 0.9, str: 0.9, org: 0.85, xp: 0.3, mor: 0.65, share, dug, first });

export interface SampleOpts { river?: boolean; bridgeDown?: boolean; fort?: number; dug?: boolean; town?: LandPlace['townSize']; seed?: number; humanAtt?: boolean }

export function sampleTicket(terrain: Terrain, odds: number, o: SampleOpts = {}): FieldTicket<LandPlace> {
  const att: SideTicket = {
    nat: 'RED', adj: 'Northern', color: '#7d6a4f', human: !!o.humanAtt, general: { name: 'General Hale', level: 2 },
    forces: [force(1, 'I Corps', 'RED', 'inf', 3, 0.38, false, 1), force(2, 'II Corps', 'RED', 'inf', 3, 0.34, false, 4), force(3, 'III Armoured Corps', 'RED', 'arm', 2, 0.28, false, 1)],
  };
  const def: SideTicket = {
    nat: 'BLU', adj: 'Southern', color: '#5a6f86', human: false, general: { name: 'General Vane', level: 2 },
    forces: [force(11, 'X Corps', 'BLU', 'inf', 3, 0.45, !!o.dug, 21), force(12, 'XI Corps', 'BLU', 'inf', 3, 0.4, !!o.dug, 24), force(13, 'Artillery Corps', 'BLU', 'art', 1, 0.15, false, 1)],
  };
  const A = 100;
  return {
    domain: 'land', kind: 'field', seed: o.seed ?? 7, turn: 1, date: 'September 1939', title: 'A practice field',
    place: { region: -1, name: 'Practice ground', terrain, river: o.river ? 'the Stream' : null, bridgeDown: !!o.bridgeDown, town: o.town === null ? null : 'Millbrook', townSize: o.town === undefined ? 'village' : o.town, fort: o.fort ?? 0, mines: 0 },
    att, def, reckoning: { A, D: A / odds, odds, dice: [0.8, 1.2], even: 1.2 }, lines: [], limit: LAND.limit,
  };
}
