// A ticket made by hand, for tests, the fairness table and the page's practice field: two armies of the usual make
// on any ground, at any odds. Not a campaign battle: nothing is handed back from it.

import { DEFENCE, DESERT, FIELD_DICE, FIELD_HOLD, GUNS, HELD_LOSS, attackLoss, defenceLoss, evenOdds } from '../../campaign/game';
import { combinedArms, groundAttack, groundDefence, isFoot } from '../../campaign/ground';
import { ENGINEERING } from '../../campaign/rivers';
import { antiTank, snipedIn, tollFor, tollOf, type Facing } from '../../campaign/support';
import { FORT_MUL, KIND, TERRAIN_DEF, type Corps, type Game, type Kind, type Region } from '../../campaign/types';
import type { AirOnCall, FieldTicket, ForceKit, ForceTicket, Side, SideTicket } from '../core/types';
import type { LandPlace, Terrain } from './field';
import { LAND } from './rules';

const force = (corps: number, name: string, nat: string, kind: string, divs: number, share: number, dug = false, first = 1, str = 0.9, kit?: ForceKit | null): ForceTicket =>
  ({ corps, name, nat, kind, divs, men: divs * 15000 * str, str, org: 0.85, xp: 0.3, mor: 0.65, share, dug, first, ...(kit && Object.keys(kit).length && (LAND.pierce.foot as readonly string[]).includes(kind) ? { kit: { ...kit } } : {}) });

/**
 * Only anti-tank weapons take a tank (LAND.pierce; the design director's pierce ruling, 25 Nov, P3): what the foot
 * corps of a sample ticket carry that can pierce armour, both sides alike: their own anti-tank guns, no tank destroyers
 * and no launchers. A sample ticket's card is plain odds and prices no kit, so the same kit on both sides keeps it
 * even. Data on the ticket only: nothing reads it unless the battle is opened with LandBattle.pierce.
 */
export const SAMPLE_KIT: ForceKit = { atGuns: true };

export interface SampleOpts {
  river?: boolean; bridgeDown?: boolean; fort?: number; dug?: boolean; town?: LandPlace['townSize']; seed?: number; humanAtt?: boolean; title?: string;
  /** Fighters on call over the field, by side (air support): left out, none. */
  air?: Partial<Record<Side, AirOnCall>>;
  /** The ticket's date, 'September 1939' where not said: the year the fighters fly in, and whether they carry bombs. */
  date?: string;
  /**
   * A computer general's style fixed by hand on either side ('bold', 'methodical', 'mass'; styles.ts): the fairness
   * tables run every pairing so. Left out, the practice armies lean methodical, as any nation with no style of its own.
   */
  style?: Partial<Record<Side, string>>;
  /** The armies' nations, for a sandbox's invented pairing (left out: RED and BLU, no nation's), and the history dial's stop. */
  nat?: Partial<Record<Side, string>>;
  hist?: number;
  /**
   * What a side's foot corps carry that can pierce armour (LAND.pierce), in place of SAMPLE_KIT: for the paired
   * measure's kit lean (tools/land-gate). {} or null: the plain kit (no field written). Kit goes with the corps.
   */
  kit?: Partial<Record<Side, ForceKit | null>>;
}

/** A corps of a hand-made army: its name, kind, divisions, share of its side's strength, and its first division's number. */
type Corps5 = [string, Kind, number, number, number];

/**
 * The test armies: the attack and the defence. On the practice grounds the defence is sized to the ground (b406). No
 * nation's tables reckon them, so they carry no nation's name: the attack is yours and the defence is the enemy, as the
 * sea page names its practice fleets (b408).
 */
const ATT: Corps5[] = [['I Corps', 'inf', 3, 0.38, 1], ['II Corps', 'inf', 3, 0.34, 4], ['III Armoured Corps', 'arm', 2, 0.28, 1]];
const DEF: Corps5[] = [['X Corps', 'inf', 3, 0.45, 21], ['XI Corps', 'inf', 3, 0.4, 24], ['Artillery Corps', 'art', 1, 0.15, 1]];

/** The ticket's corps ids: the attack's from 1, the defence's from 11, in the order their corps are listed. */
const attId = (i: number): number => i + 1, defId = (i: number): number => 11 + i;

export function sampleTicket(terrain: Terrain, odds: number, o: SampleOpts = {}): FieldTicket<LandPlace> {
  const A = 100;
  const kit = { att: o.kit?.att !== undefined ? o.kit.att : SAMPLE_KIT, def: o.kit?.def !== undefined ? o.kit.def : SAMPLE_KIT };
  return made(terrain, { A, D: A / odds, odds, dice: [0.8, 1.2], even: 1.2 }, o, DEF, 0.9, kit);
}

/** kit: what each side's foot corps carry that can pierce armour (LAND.pierce); left out, the plain kit, no field written. */
function made(terrain: Terrain, reckoning: FieldTicket<LandPlace>['reckoning'], o: SampleOpts, defence: Corps5[] = DEF, defStr = 0.9, kit?: Record<Side, ForceKit | null>): FieldTicket<LandPlace> {
  const nA = o.nat?.att ?? 'RED', nD = o.nat?.def ?? 'BLU';
  const att: SideTicket = {
    nat: nA, adj: 'Your', color: '#7d6a4f', human: !!o.humanAtt, general: { name: 'General Hale', level: 2 },
    forces: ATT.map(([name, kind, divs, share, first], i) => force(attId(i), name, nA, kind, divs, share, false, first, 0.9, kit?.att)),
    ...(o.air?.att ? { air: o.air.att } : {}), ...(o.style?.att ? { style: o.style.att } : {}),
  };
  const def: SideTicket = {
    nat: nD, adj: 'Enemy', color: '#5a6f86', human: false, general: { name: 'General Vane', level: 2 },
    forces: defence.map(([name, kind, divs, share, first], i) => force(defId(i), name, nD, kind, divs, share, !!o.dug && kind !== 'art', first, defStr, kit?.def)),
    ...(o.air?.def ? { air: o.air.def } : {}), ...(o.style?.def ? { style: o.style.def } : {}),
  };
  return {
    domain: 'land', kind: 'field', seed: o.seed ?? 7, turn: 1, date: o.date ?? 'September 1939', title: o.title ?? 'A practice field',
    place: { region: -1, name: 'Practice ground', terrain, river: o.river ? 'Wend' : null, bridgeDown: !!o.bridgeDown, town: o.town === null ? null : 'Millbrook', townSize: o.town === undefined ? 'village' : o.town, fort: o.fort ?? 0, mines: 0 },
    att, def, reckoning, lines: [], limit: LAND.limit, ...(o.hist !== undefined ? { hist: o.hist } : {}),
  };
}

/**
 * A practice ground: its ground, a river or fortress works where it has them, and the defence there: the divisions of
 * its two corps (infantry, or motorized where said), its divisions of guns (one where not said), and how strong they
 * stand (0.9 where not said: a worn defence holds the hard ground).
 */
export interface PracticeGround {
  terrain: Terrain; river?: boolean; fort?: number; def?: [number, number]; guns?: number; kind?: 'inf' | 'mot'; str?: number;
  /** Fighters on call over the field, both sides (PRACTICE_AIR): the card counts them, as the campaign's sky term would. */
  air?: boolean;
}

/**
 * The practice armies' fighters on call, where a ground has them (air support): a group a side, the attack's with three
 * strikes and a tenth of its strength on the card, the defence's with two and a sixteenth. In 1942, so they carry bombs
 * (LAND.air.bombsFrom). Planes lost a strike, as the campaign would price them: about one in two strikes.
 */
export const PRACTICE_AIR: Record<Side, AirOnCall> = {
  att: { share: 0.1, strikes: 3, groups: [{ id: 1, name: '1st Fighter Group', nat: 'RED', planes: 36, lose: 0.5 }] },
  def: { share: 0.0625, strikes: 2, groups: [{ id: 11, name: '11th Fighter Group', nat: 'BLU', planes: 24, lose: 0.6 }] },
};
/** The practice date where fighters are on call: they carry bombs then. */
export const PRACTICE_AIR_DATE = 'June 1942';

/**
 * The page's practice grounds: the seven kinds of ground, a river crossing and fortress works. The attack is the same
 * on each; the defence is sized to the ground so that every card is a contest, the staff giving it between 4 and 8 in
 * 10 (the design director, b406), and the card is still reckoned exactly the campaign's way for those forces there.
 * Each make-up was measured on the field before it was chosen: the plain attack wins within a few points of its card
 * over 120 battles (the field's result also leans on the defence's make-up, not only on the odds: see b406's note).
 */
export const PRACTICE_GROUNDS: PracticeGround[] = [
  { terrain: 'plains', def: [4, 3], str: 0.85 }, { terrain: 'forest', def: [4, 3], str: 0.65 }, { terrain: 'hills', def: [3, 3], str: 0.8 },
  { terrain: 'mountain', def: [3, 3], str: 0.5 }, { terrain: 'marsh', def: [3, 3], str: 0.6 }, { terrain: 'urban', def: [3, 3], str: 0.55 },
  { terrain: 'desert', def: [4, 3], guns: 3, kind: 'mot', str: 0.85 },
  { terrain: 'plains', river: true, def: [3, 3], guns: 2 }, { terrain: 'hills', fort: 2, def: [2, 2], guns: 0, kind: 'mot' },
  { terrain: 'plains', def: [4, 3], str: 0.85, air: true },
];

/**
 * How worn a practice defence stands, against the attack's 90 in 100 (b408), in the UI director's words: the card says
 * so, so the enemy's lower strength on the field is no surprise. Nothing from 85 to 94, where it stands about as the
 * attack does. Read by the whole number its strength meter shows, so a strength a hair under 85 still reads as 85.
 */
export function wornWords(str: number): string {
  const s = Math.round(str * 100);
  return s >= 95 ? 'the defence at full strength' : s >= 85 ? '' : s >= 75 ? 'the defence worn to four fifths strength'
    : s >= 60 ? 'the defence worn to two thirds strength' : 'the defence worn to half strength';
}

/** The defence on a practice ground: its two infantry corps as the ground has them, and the artillery; each corps' share of the strength as the campaign's ticket counts it (divisions by their worth in defence). */
function defenceOn(p: PracticeGround): Corps5[] {
  const [x, xi] = p.def ?? [3, 3];
  const guns = p.guns ?? 1;
  const k: Kind = p.kind ?? 'inf', word = k === 'mot' ? ' Motorized' : '';
  const rows: [string, Kind, number, number][] = [[`X${word} Corps`, k, x, 21], [`XI${word} Corps`, k, xi, 21 + x], ...(guns > 0 ? [['Artillery Corps', 'art', guns, 1] as [string, Kind, number, number]] : [])];
  const kept = rows.filter((r) => r[2] > 0);
  const worth = kept.reduce((a, r) => a + r[2] * KIND[r[1]].def, 0);
  return kept.map(([name, kind, divs, first]) => [name, kind, divs, (divs * KIND[kind].def) / worth, first]);
}

/** The practice armies on a ground as the campaign's own sums see them: a stub game, the ground, both sides' corps, and what the attack faces there. */
function practiceArmies(p: PracticeGround) {
  // Only the grand campaign's ground rules ('open') are switched on: no nation, no technology, no flags beyond them.
  const g = { flags: { open: 1 }, turn: 1, nations: {} } as unknown as Game;
  const target = { id: -1, name: 'Practice ground', terrain: p.terrain, fort: null } as unknown as Region;
  const corps = (nat: string, [name, kind, divs]: Corps5, str = 0.9): Corps =>
    ({ nat, name, kind, divs, str, org: 0.85, xp: 0.3, mor: 0.65, supplied: true, dug: false, reg: -2 }) as unknown as Corps;
  const att = ATT.map((x) => corps('RED', x)), def = defenceOn(p).map((x) => corps('BLU', x, p.str ?? 0.9));
  const fort = p.fort ?? 0;
  const armsA = combinedArms(g, att, target, false), armsD = combinedArms(g, def, target, true);
  const gunsD = def.reduce((s, c) => s + (c.kind === 'art' ? c.divs * c.str : 0), 0);
  const facing = { works: () => 0, guns: gunsD, mines: 0, sappers: 0, fort, foot: armsA ? armsA.foot : 0, tanks: armsA ? armsA.tanks : 0 } as Facing;
  const at = antiTank(g, att, def, target, facing);
  return { g, target, att, def, fort, armsA, armsD, gunsD, facing, at };
}

/**
 * The practice armies' card on this ground, reckoned as the campaign's attack card reckons those forces there (the
 * design director, b400): the parts of the campaign's assess() (game.ts) that the ground, a river and fortress works
 * touch, using its own tables and its own sums for the tanks beside the infantry and for the defence's anti-tank
 * strength. Everything else in assess() falls the same on both armies here or is not there at all: one general each,
 * no air, no technology of their own (September 1939), fed, fresh, attacking from one place. The bridges stand.
 */
export function practiceReckoning(p: PracticeGround): FieldTicket<LandPlace>['reckoning'] {
  const { g, target, att, def, fort, armsA, armsD, gunsD, at } = practiceArmies(p);
  const thicken = gunsD ? Math.min(GUNS.thickenMost, GUNS.thicken * gunsD) : 0;
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
  // Fighters on call: the card's sky term counted them, so they made their share of each side's strength.
  if (p.air) { A /= 1 - PRACTICE_AIR.att.share; D /= 1 - PRACTICE_AIR.def.share; }
  const dice: [number, number] = [FIELD_DICE[0], FIELD_DICE[1]];
  return { A, D, odds: A / D, dice, even: evenOdds(dice) };
}

/**
 * What the staff's settlement of a practice battle costs each side (the design director, b410): the campaign's own
 * bill at the ratio R for these armies on this ground, as resolveBattle books it (game.ts): the attack's share by R and
 * who pays it, the defence's by R, with the push of tanks when the ground is won, foot caught in the open desert, the
 * lighter loss of a defence that holds, and the weight of the blow. What the field had already cost a side counts
 * inside the bill, as on a campaign hand-over (FIELD_HOLD), not on top of it. Shares of each side's men, 0..1.
 */
export function practiceBill(p: PracticeGround, R: number, field: Record<Side, number> = { att: 0, def: 0 }, corps?: Record<number, number>): Record<Side, number> {
  const { g, target, att, def, at, facing } = practiceArmies(p);
  const toll = tollOf(g, att, def, at, facing, snipedIn(target, false));
  const k = practiceReckoning(p);
  // (One general each with no traits, and no works dug: those parts of the campaign's bill fall at 1 here.)
  const win = R >= 1.2, mobile = att.some((c) => KIND[c.kind].mobile);
  const caught = win && mobile && p.terrain === DESERT.terrain && !p.fort ? DESERT.retreat[R >= 2 ? 1 : 0] : 1;
  const weight = Math.min(1, k.odds / DEFENCE.wear.fullAt);
  // A side's share of its men, from each corps' share (men go by divisions and strength, as men() in game.ts).
  const share = (cs: Corps[], part: (c: Corps, i: number) => number): number => {
    const w = cs.reduce((a, c) => a + c.divs * c.str, 0);
    return w > 0 ? cs.reduce((a, c, i) => a + c.divs * c.str * part(c, i), 0) / w : 0;
  };
  // Each corps' part of its side's bill as the field split it, as splitWeights in game.ts: its field loss over its side's.
  const split = (cs: Corps[], id: (i: number) => number): number[] => {
    const mean = corps ? share(cs, (_c, i) => corps[id(i)] ?? 0) : 0;
    return cs.map((_c, i) => (corps && mean > 1e-6 && corps[id(i)] !== undefined ? Math.min(3, corps[id(i)] / mean) : 1));
  };
  const wA = split(att, attId), wD = split(def, defId);
  const attPart = (c: Corps, i: number): number => attackLoss(R) * wA[i] * tollFor(toll, c);
  const defPart = (c: Corps, i: number): number => defenceLoss(R) * wD[i] * (win && mobile ? 1.3 : 1) * (KIND[c.kind].mobile ? 1 : caught) * (win ? 1 : HELD_LOSS);
  // Held near what the field cost each side, never less than it would have bled there (fieldScale in game.ts, on a hand-over).
  const scale = (now: number, lost: number): number => (now <= 1e-9 ? 1 : Math.max(lost / FIELD_HOLD, now) / now);
  const kA = scale(share(att, (c, i) => Math.min(0.6, attPart(c, i))), field.att);
  const kD = scale(share(def, (c, i) => Math.min(0.8, defPart(c, i)) * weight), field.def);
  return { att: share(att, (c, i) => Math.min(0.6, attPart(c, i) * kA)), def: share(def, (c, i) => Math.min(0.8, defPart(c, i) * kD) * weight) };
}

/**
 * A practice ground's ticket: the practice armies on that ground, at the odds their own card gives them there. Its foot
 * carries the plain kit (no ForceKit field: LAND.pierce): its card is reckoned with no research, and the field's kit
 * must match the card it plays out (the pierce ruling, 25 Nov, P3).
 */
export function practiceTicket(p: PracticeGround, o: Omit<SampleOpts, 'river' | 'fort' | 'bridgeDown' | 'dug' | 'kit'> = {}): FieldTicket<LandPlace> {
  return made(p.terrain, practiceReckoning(p), { ...o, river: p.river, fort: p.fort, ...(p.air ? { air: PRACTICE_AIR, date: PRACTICE_AIR_DATE } : {}) }, defenceOn(p), p.str ?? 0.9);
}

/**
 * A ticket with a machine-gun team and a mortar team on each side (release 2, the test kitchen's ?teams and the
 * tests): each team a twenty-fifth of its side's power, the corps' shares cut to make room, so the side's power and the
 * odds are the ticket's own. The campaign builds and prices its own teams (stage 2); this only puts some on the field.
 */
export function withTeams(t: FieldTicket<LandPlace>, share = 0.04): FieldTicket<LandPlace> {
  const add = (s: SideTicket, base: number): SideTicket => {
    const nat = s.forces[0]?.nat ?? s.nat;
    const team = (kind: 'mg' | 'mor', i: number): ForceTicket => ({
      corps: base + i, name: kind === 'mg' ? 'Machine-Gun Battalion' : 'Mortar Battalion', nat, kind, divs: 1, men: 900,
      str: 0.9, org: 0.85, xp: 0.3, mor: 0.65, share, first: 1,
    });
    return { ...s, forces: [...s.forces.map((f) => ({ ...f, share: f.share * (1 - 2 * share) })), team('mg', 1), team('mor', 2)] };
  };
  return { ...t, att: add(t.att, 9000), def: add(t.def, 9100) };
}
