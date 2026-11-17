// A ticket made by hand, for tests, the fairness table and the page's practice field: two armies of the usual make
// on any ground, at any odds. Not a campaign battle: nothing is handed back from it.

import { DESERT, FIELD_DICE, GUNS, evenOdds } from '../../campaign/game';
import { combinedArms, groundAttack, groundDefence, isFoot } from '../../campaign/ground';
import { ENGINEERING } from '../../campaign/rivers';
import { antiTank } from '../../campaign/support';
import { FORT_MUL, KIND, TERRAIN_DEF, type Corps, type Game, type Kind, type Region } from '../../campaign/types';
import type { FieldTicket, ForceTicket, SideTicket } from '../core/types';
import type { LandPlace, Terrain } from './field';
import { LAND } from './rules';

const force = (corps: number, name: string, nat: string, kind: string, divs: number, share: number, dug = false, first = 1): ForceTicket =>
  ({ corps, name, nat, kind, divs, men: divs * 15000 * 0.9, str: 0.9, org: 0.85, xp: 0.3, mor: 0.65, share, dug, first });

export interface SampleOpts { river?: boolean; bridgeDown?: boolean; fort?: number; dug?: boolean; town?: LandPlace['townSize']; seed?: number; humanAtt?: boolean; title?: string }

/** The two practice armies: the Northern attack and the Southern defence, the same on every ground. */
const ATT: [string, Kind, number, number, number][] = [['I Corps', 'inf', 3, 0.38, 1], ['II Corps', 'inf', 3, 0.34, 4], ['III Armoured Corps', 'arm', 2, 0.28, 1]];
const DEF: [string, Kind, number, number, number][] = [['X Corps', 'inf', 3, 0.45, 21], ['XI Corps', 'inf', 3, 0.4, 24], ['Artillery Corps', 'art', 1, 0.15, 1]];

export function sampleTicket(terrain: Terrain, odds: number, o: SampleOpts = {}): FieldTicket<LandPlace> {
  const A = 100;
  return made(terrain, { A, D: A / odds, odds, dice: [0.8, 1.2], even: 1.2 }, o);
}

function made(terrain: Terrain, reckoning: FieldTicket<LandPlace>['reckoning'], o: SampleOpts): FieldTicket<LandPlace> {
  const att: SideTicket = {
    nat: 'RED', adj: 'Northern', color: '#7d6a4f', human: !!o.humanAtt, general: { name: 'General Hale', level: 2 },
    forces: ATT.map(([name, kind, divs, share, first], i) => force(i + 1, name, 'RED', kind, divs, share, false, first)),
  };
  const def: SideTicket = {
    nat: 'BLU', adj: 'Southern', color: '#5a6f86', human: false, general: { name: 'General Vane', level: 2 },
    forces: DEF.map(([name, kind, divs, share, first], i) => force(11 + i, name, 'BLU', kind, divs, share, !!o.dug && kind !== 'art', first)),
  };
  return {
    domain: 'land', kind: 'field', seed: o.seed ?? 7, turn: 1, date: 'September 1939', title: o.title ?? 'A practice field',
    place: { region: -1, name: 'Practice ground', terrain, river: o.river ? 'Wend' : null, bridgeDown: !!o.bridgeDown, town: o.town === null ? null : 'Millbrook', townSize: o.town === undefined ? 'village' : o.town, fort: o.fort ?? 0, mines: 0 },
    att, def, reckoning, lines: [], limit: LAND.limit,
  };
}

/** A practice ground: its ground, and a river or fortress works where it has them. */
export interface PracticeGround { terrain: Terrain; river?: boolean; fort?: number }

/** The page's practice grounds: the seven kinds of ground, a river crossing and fortress works. */
export const PRACTICE_GROUNDS: PracticeGround[] = [
  { terrain: 'plains' }, { terrain: 'forest' }, { terrain: 'hills' }, { terrain: 'mountain' }, { terrain: 'marsh' }, { terrain: 'urban' }, { terrain: 'desert' },
  { terrain: 'plains', river: true }, { terrain: 'hills', fort: 2 },
];

/**
 * The practice armies' card on this ground, reckoned as the campaign's attack card reckons those forces there (the
 * design director, b400): the parts of the campaign's assess() (game.ts) that the ground, a river and fortress works
 * touch, using its own tables and its own sums for the tanks beside the infantry and for the defence's anti-tank
 * strength. Everything else in assess() falls the same on both armies here or is not there at all: one general each,
 * no air, no technology of their own (September 1939), fed, fresh, attacking from one place. The bridges stand.
 */
export function practiceReckoning(p: PracticeGround): FieldTicket<LandPlace>['reckoning'] {
  // Only the grand campaign's ground rules ('open') are switched on: no nation, no technology, no flags beyond them.
  const g = { flags: { open: 1 }, turn: 1, nations: {} } as unknown as Game;
  const target = { id: -1, name: 'Practice ground', terrain: p.terrain, fort: null } as unknown as Region;
  const corps = (nat: string, [name, kind, divs]: [string, Kind, number, number, number]): Corps =>
    ({ nat, name, kind, divs, str: 0.9, org: 0.85, xp: 0.3, mor: 0.65, supplied: true, dug: false, reg: -2 }) as unknown as Corps;
  const att = ATT.map((x) => corps('RED', x)), def = DEF.map((x) => corps('BLU', x));
  const fort = p.fort ?? 0;
  const armsA = combinedArms(g, att, target, false), armsD = combinedArms(g, def, target, true);
  const gunsD = def.reduce((s, c) => s + (c.kind === 'art' ? c.divs * c.str : 0), 0);
  const thicken = gunsD ? Math.min(GUNS.thickenMost, GUNS.thicken * gunsD) : 0;
  const at = antiTank(g, att, def, target, { works: () => 0, guns: gunsD, mines: 0, sappers: 0, fort, foot: armsA ? armsA.foot : 0, tanks: armsA ? armsA.tanks : 0 });
  const openDesert = p.terrain === DESERT.terrain && !fort && att.some((c) => KIND[c.kind].mobile);
  // The strength both share (corpsBase less its divisions, and heart) is left out: it is the same for every corps here.
  let A = 0;
  for (const c of att) {
    let a = c.divs * c.str * KIND[c.kind].atk;
    a *= groundAttack(g, c.kind, target);
    if (armsA) a *= c.kind === 'arm' ? armsA.armour : isFoot(c) ? armsA.infantry : 1;
    if (p.river) a *= ENGINEERING.river.attack;
    if (c.kind === 'arm') {
      if (p.terrain === 'mountain' || p.terrain === 'urban' || p.terrain === 'marsh') a *= 0.6;
      else {
        if (at.strength < 1) a *= 1 + 0.25 * (1 - at.strength);
        if (at.alone > 0) { const old = armsA ? armsA.alone : 1; if (1 - at.alone < old) a *= (1 - at.alone) / old; }
      }
    }
    if (fort) a /= FORT_MUL[fort];
    A += a;
  }
  let D = 0;
  for (const d of def) {
    let v = d.divs * d.str * KIND[d.kind].def * TERRAIN_DEF[p.terrain];
    v *= groundDefence(g, d.kind, target);
    if (armsD) v *= d.kind === 'arm' ? armsD.armour : isFoot(d, true) ? armsD.infantry : 1;
    if (thicken && d.kind !== 'art') v *= 1 + thicken;
    // Infantry on foot with no works to anchor on, against tanks: driven round in the open desert.
    if (openDesert && !KIND[d.kind].mobile && d.kind !== 'fort') v *= DESERT.open;
    D += v;
  }
  const dice: [number, number] = [FIELD_DICE[0], FIELD_DICE[1]];
  return { A, D, odds: A / D, dice, even: evenOdds(dice) };
}

/** A practice ground's ticket: the practice armies on that ground, at the odds their own card gives them there. */
export function practiceTicket(p: PracticeGround, o: Omit<SampleOpts, 'river' | 'fort' | 'bridgeDown' | 'dug'> = {}): FieldTicket<LandPlace> {
  return made(p.terrain, practiceReckoning(p), { ...o, river: p.river, fort: p.fort });
}
