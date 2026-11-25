// What an order buys on the battle card for its price, at the staff's own sight (release 2; the design director's
// ruling of 25 November 2026, 19:48Z, on the support teams' quota): "a staff orders a brigade only when, at its own
// sight, it buys more on the card for its price than the corps or refit the same money would, using the build pick's
// one value test for every order. The quota goes; the price is not touched."
//
// One test for every order: the strength the order adds to the battle card where it would stand, per point of its
// price. The strength is the card's own (game.ts assess): a corps' worth (corpsBase) times its arm's figure in attack or
// defence, on its ground and in its works, and what the card's lifts make of the corps beside it:
//   - the machine-gun brigades stiffen a defence against the men on foot (TEAMS.card.mg),
//   - the massed guns thicken a defence (GUNS.thicken) and soften the works an attack goes in against (GUNS.soften),
//   - the mortar brigades break up a share of those works (TEAMS.card.mor),
// the works counted both ways the card counts them: the trenches' cover (DUG_IN) and the day's luck that runs against
// an attack on a line that is ready (the prepared share). An order is weighed in the role it serves, machine guns in
// defence and mortars in attack, against what the other orders would add in the same role.
//
// At its own sight: the enemy is what stands in the provinces next door the staff can see (intel.ts sightOf), and the
// order stands at the front sector where it is worth most and there is room for it, as teamsUp and gunsUp send them.
// Teams and guns on order count as standing in every sector already, so a staff does not buy twice for one want.
// A fresh formation arrives full, at a new corps' readiness and training (economy.ts FRESH), as every corps of the
// staff's does. A refit is the drafts the staff's corps in the field are waiting for this month, at the drafts' price
// a division, and it is what the money would buy only where the purse, the order paid, would not cover them.
//
// Only the support teams' rule asks it (economy.ts aiTeams); nothing reads it while they are off.

import { DEFENCE, DUG_IN, FIELD_DICE, GUNS, armySize, corpsBase, fights, friendlyUnits, hostileUnits, luckOf, winChanceOn, worksOf, worksShare } from './game';
import { FRESH, PRODUCTION, corpsCost, specOf } from './economy';
import { SIGHT } from './intel';
import { techMul } from './research';
import { raisedXp } from './stories';
import { SUPPORT, footShare, mortars } from './support';
import { TEAMS, isTeam, teamsIn } from './teams';
import { KIND, STACK, TERRAIN_DEF, type Corps, type Game, type Kind, type Region } from './types';

/** The card's side an order is weighed on. */
export type Role = 'atk' | 'def';

/** What a fresh formation of this kind adds to the card in attack or defence, before any lift (full, ready as a new corps, green). */
export function freshWorth(g: Game, nat: string, kind: Kind, divs: number, role: Role): number {
  const xp = nat === g.human ? raisedXp(g, nat) : FRESH.xp;
  const kit = techMul(g, nat, kind === 'arm' || kind === 'mot' ? 'armour' : 'infantry');
  return divs * Math.sqrt(0.15 + 0.85 * FRESH.org) * g.nations[nat].quality * (0.85 + 0.4 * xp) * kit * KIND[kind][role];
}

/** Divisions of massed guns among these corps, at their strength, half where cut off (as the card counts them). */
const gunsAmong = (cs: Corps[]): number => cs.reduce((s, c) => s + (c.kind === 'art' ? c.divs * c.str * (c.supplied ? 1 : 0.5) : 0), 0);
/** A corps' worth in defence on its ground, before its trenches and the lifts. */
const held = (g: Game, d: Corps, r: Region): number => corpsBase(g, d) * KIND[d.kind].def * TERRAIN_DEF[r.terrain];
/** The share of finished works a defending corps has, where it is dug in and fed. */
const worksAt = (d: Corps): number => (d.dug && d.supplied ? worksShare(worksOf(d)) : 0);
const standing = (cs: Corps[]): Corps[] => cs.filter((u) => !u.dead && u.town === undefined);

/** Extra machine-gun, mortar and gun divisions an order brings to a sector, over those there and on order. */
interface Adds { mg: number; mor: number; guns: number }

/**
 * What a defence at r gains on the card from these extra divisions, against the foes the staff sees next door: the
 * machine guns' lift to its fighting corps by the share of the foes on foot and their works, and the guns' thickening.
 */
function defenceGain(g: Game, nat: string, r: Region, foes: Corps[], base: Adds, add: Adds): number {
  const side = standing(friendlyUnits(g, r, nat));
  const foot = footShare(foes);
  const M = SUPPORT.machineGuns, C = TEAMS.card.mg;
  const mg0 = teamsIn(side, 'mg') + base.mg, gun0 = gunsAmong(side) + base.guns;
  const up = (x: number): number => (x > 0 ? Math.min(C.most, C.perDiv * x) : 0);
  const thick = (x: number): number => (x > 0 ? Math.min(GUNS.thickenMost, GUNS.thicken * x) : 0);
  let gain = 0;
  for (const d of side) {
    if (d.kind === 'art') continue;
    const p = held(g, d, r) * (d.dug ? DUG_IN : 1);
    const lift = (mg: number, gun: number): number => (fights(d) ? 1 + up(mg) * foot * (M.open + (1 - M.open) * worksAt(d)) : 1) * (1 + thick(gun));
    gain += p * (lift(mg0 + add.mg, gun0 + add.guns) - lift(mg0, gun0));
  }
  return gain;
}

/**
 * What an attack from r into t gains on the card from these extra divisions, as attack strength: the guns and mortars
 * break up the dug-in foes' works, which takes off their trenches' cover and the day's luck a ready line has.
 */
function attackGain(g: Game, nat: string, r: Region, t: Region, base: Adds, add: Adds): number {
  const mine = standing(r.units.filter((u) => u.nat === nat));
  const foes = standing(hostileUnits(g, t, nat));
  if (!foes.length || !foes.some((d) => d.dug)) return 0;
  let A = 0;
  for (const c of mine) A += corpsBase(g, c) * KIND[c.kind].atk;
  if (A <= 0) return 0;
  const own = mortars(g, mine), C = TEAMS.card.mor;
  const mor0 = teamsIn(mine, 'mor') + base.mor, gun0 = gunsAmong(mine) + base.guns;
  // The card's odds per point of attack: the luck the day runs at over what the defence is worth.
  const odds = (mor: number, gun: number): number => {
    const soften = gun > 0 ? Math.min(GUNS.softenMost, GUNS.soften * gun) : 0;
    const up = mor > 0 ? Math.min(C.most, C.perDiv * mor) : 0;
    const left = (1 - soften) * (1 - (1 - (1 - own) * (1 - up)));
    let D = 0, ready = 0;
    for (const d of foes) {
      const p = held(g, d, t) * (d.dug ? 1 + (DUG_IN - 1) * left : 1);
      D += p;
      ready += p * worksAt(d);
    }
    if (D <= 0) return 0;
    const prepared = Math.min(1, ready / D) * left;
    const P = DEFENCE.prepared.dice;
    const luck = winChanceOn(g) ? luckOf(prepared)[1] / luckOf(0)[1]
      : (FIELD_DICE[0] + FIELD_DICE[1] + (P[0] - FIELD_DICE[0] + P[1] - FIELD_DICE[1]) * prepared) / (FIELD_DICE[0] + FIELD_DICE[1]);
    return luck / D;
  };
  const before = odds(mor0, gun0);
  return before > 0 ? A * (odds(mor0 + add.mor, gun0 + add.guns) / before - 1) : 0;
}

/** The most an order of this kind adds through the card's lifts at one of the staff's fronts with room for it. */
function bestGain(g: Game, nat: string, kind: Kind, divs: number, role: Role, sight: Uint8Array): number {
  const add: Adds = { mg: kind === 'mg' ? divs : 0, mor: kind === 'mor' ? divs : 0, guns: kind === 'art' ? divs : 0 };
  if (!add.mg && !add.mor && !add.guns) return 0;
  const base: Adds = { mg: 0, mor: 0, guns: 0 };
  for (const b of g.nations[nat].queue) {
    if (b.what !== 'corps' || !b.divs) continue;
    if (b.kind === 'mg') base.mg += b.divs; else if (b.kind === 'mor') base.mor += b.divs; else if (b.kind === 'art') base.guns += b.divs;
  }
  let best = 0;
  for (const r of g.regions) {
    const mine = standing(r.units.filter((u) => u.nat === nat));
    if (!mine.some(fights) || friendlyUnits(g, r, nat).length >= STACK) continue;
    if (isTeam({ kind }) && mine.filter(isTeam).length >= TEAMS.frontage) continue;
    if (role === 'def') {
      const foes: Corps[] = [];
      for (const j of r.nb) if (sight[j] !== SIGHT.none) foes.push(...standing(hostileUnits(g, g.regions[j], nat)));
      if (foes.length) best = Math.max(best, defenceGain(g, nat, r, foes, base, add));
    } else {
      for (const j of r.nb) if (sight[j] !== SIGHT.none) best = Math.max(best, attackGain(g, nat, r, g.regions[j], base, add));
    }
  }
  return best;
}

/**
 * What the drafts the staff's corps in the field are waiting for this month add to the card, per point, where the
 * purse, with 'spend' paid out of it, would not cover them; 0 where it would (the money is then not theirs).
 */
function refitWorth(g: Game, nat: string, role: Role, spend: number): number {
  const n = g.nations[nat];
  let want = 0, worth = 0;
  for (const c of g.corps) {
    if (c.nat !== nat || c.dead || !c.supplied || c.vol || c.str <= 0) continue;
    const gap = Math.min(Math.max(0, (c.full ?? 1) - c.str) * c.divs, 0.2 * c.divs);
    if (gap <= 0) continue;
    want += gap;
    worth += gap * (corpsBase(g, c) / (c.divs * c.str)) * KIND[c.kind][role];
  }
  // The depots send out no more in a month than their share of the army's full size.
  const most = PRODUCTION.manpower * Math.max(n.armyPeak, armySize(g, nat));
  if (want > most && want > 0) { worth *= most / want; want = most; }
  const bill = want * PRODUCTION.replacementCost;
  return want > 0 && n.pp - spend < bill ? worth / bill : 0;
}

/**
 * What an order adds to the card per point of its price, in the role it is weighed in, at the staff's own sight: a
 * corps of a kind (its own weight, and for guns and teams what their lifts add at the front where that is most), or
 * 'refit' (the drafts the money would otherwise pay, 'spend' being the price of the order it is weighed against).
 * 0 for an order with no price in points (a corps paid in men alone).
 */
export function orderWorth(g: Game, nat: string, order: Kind | 'refit', role: Role, sight: Uint8Array, spend = 0): number {
  if (order === 'refit') return refitWorth(g, nat, role, spend);
  const spec = specOf(g, order), price = corpsCost(g, nat, order);
  if (!spec || price <= 0) return 0;
  return (freshWorth(g, nat, order, spec.divs, role) + bestGain(g, nat, order, spec.divs, role, sight)) / price;
}
