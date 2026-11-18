// The campaign's side of the hand-off: an attack the player could order, written as a FieldTicket; and the field's
// result handed back through the rules' own attack order, with R standing in for the dice.

import { hashString } from '../../core/rng';
import { turnToDate } from '../../core/ticket';
import { generalOf } from '../../campaign/commanders';
import { assess, corpsBase, evenOdds, limitAttackers, men, worksOf, type FieldBill } from '../../campaign/game';
import { divisionsOf } from '../../campaign/armies';
import { armyOf } from '../../campaign/joint';
import { bridgeDown, riverBetween, riverName } from '../../campaign/rivers';
import { minesAt } from '../../campaign/engineers';
import { townName, townSize } from '../../campaign/towns';
import { jointForce, orderAttack, previewAttack } from '../../campaign/turn';
import { KIND, type BattleReport, type Corps, type Game, type Region } from '../../campaign/types';
import type { FieldResult, FieldTicket, ForceTicket, SideTicket } from '../core/types';
import type { LandPlace } from './field';
import { LAND } from './rules';

/** The corps that would actually go in, as the attack card and the battle count them. */
export function goingIn(g: Game, corps: Corps[], target: Region): Corps[] {
  return jointForce(g, limitAttackers(g, jointForce(g, corps, target), target), target);
}

function sideOf(g: Game, nat: string, cs: Corps[], weigh: (c: Corps) => number): SideTicket {
  const N = g.nations[nat];
  const w = cs.map(weigh), tot = w.reduce((a, b) => a + b, 0) || 1;
  const forces: ForceTicket[] = cs.map((c, i) => ({
    corps: c.id, name: c.name, nat: c.nat, kind: c.kind, divs: c.divs, men: men(c), str: c.str, org: c.org, xp: c.xp,
    ...(c.mor !== undefined ? { mor: c.mor } : {}), ...(c.dug ? { dug: true } : {}), ...(c.dn !== undefined ? { first: c.dn } : {}),
    // Each block is one of these divisions, named as the army tray names it (the UI and design directors, b358).
    divisions: divisionsOf(g, c).map((d) => ({ name: d.name, short: d.short, str: d.str, xp: d.xp, men: d.men })),
    share: w[i] / tot,
  }));
  // The general of the strongest corps commands on the field.
  const lead = cs.map((c) => generalOf(g, c)).filter((x) => !!x).sort((a, b) => b!.level - a!.level)[0] ?? null;
  return { nat, adj: N?.adj ?? nat, color: N?.color ?? '#888888', human: nat === g.human, forces, general: lead ? { name: lead.name, level: lead.level } : null };
}

/**
 * Where the record has the attack over its river at more places than the road's own bridge by the town, as far as
 * the field's three kilometres allow: each a share of the field's width, west to east. Sedan, 13 May 1940: the Meuse
 * was crossed at Donchery (2nd Panzer, late in the day), at Gaulier and Glaire by the town (1st Panzer: the road's
 * bridge) and at Wadelincourt (10th Panzer), the infantry first in assault boats and the bridges built by nightfall
 * (the design director, b439).
 */
const CROSSINGS: Record<string, { river: string; at: number[] }> = {
  Sedan: { river: 'Meuse', at: [0.12, 0.85] },
};

/** The ticket for an attack the player could order now, or null where there is no attack to fight. */
export function landTicket(g: Game, corps: Corps[], target: Region): FieldTicket<LandPlace> | null {
  const a = previewAttack(g, corps, target);
  if (!a || !a.defenders.length) return null;
  const going = goingIn(g, corps, target);
  if (!going.length) return null;
  const attNat = armyOf(going[0]);
  const defNat = a.defenders[0].nat;
  const crossing = going.map((c) => riverBetween(g, c.reg, target.id)).find((x) => !!x) ?? null;
  const from = g.regions[going[0].reg];
  const place: LandPlace = {
    region: target.id, name: target.name, terrain: target.terrain,
    river: crossing ? riverName(crossing) : null, bridgeDown: crossing ? bridgeDown(g, crossing) : false,
    town: townName(target), townSize: townSize(g, target),
    fort: target.fort && target.fort.vs.includes(from.owner) ? target.fort.lv : 0,
    mines: minesAt(g, target),
    ...(crossing && CROSSINGS[target.name]?.river === riverName(crossing) ? { crossings: CROSSINGS[target.name].at } : {}),
  };
  const date = turnToDate(g.turn);
  return {
    domain: 'land', kind: 'field', seed: hashString(`field:${g.seed}:${g.turn}:${target.id}:${attNat}`) >>> 0,
    turn: g.turn, date: date.label, title: `The Battle of ${target.name}`, place,
    att: sideOf(g, attNat, going, (c) => corpsBase(g, c) * KIND[c.kind].atk),
    def: sideOf(g, defNat, a.defenders, (c) => corpsBase(g, c) * KIND[c.kind].def * (1 + 0.25 * Math.min(1, worksOf(c)))),
    reckoning: { A: a.A, D: a.D, odds: a.odds, dice: [a.dice[0], a.dice[1]], even: evenOdds(a.dice) },
    lines: a.lines, limit: LAND.limit,
    // The attacks already made on this place this month by this side: the same ground, a fresh day's luck.
    attempt: g.reports.filter((x) => x.turn === g.turn && x.region === target.id && x.attNat === attNat).length,
  };
}

/**
 * What the field cost, for the campaign's bill (ruling 226(6)): each corps' own share lost, so the side's bill falls on
 * its corps as the field had it, and each side's share, so the after card's headline and its field line agree.
 */
export function fieldBill(result: FieldResult): FieldBill {
  const corps: Record<number, number> = {};
  for (const f of result.forces ?? []) corps[f.corps] = f.lost;
  return { corps, att: result.lost.att, def: result.lost.def, whole: result.how !== 'objective' };
}

/** Hands a fought battle back: the rules' own attack order, with the field's R in place of the dice. */
export function handBack(g: Game, corps: Corps[], target: Region, result: FieldResult): BattleReport | null {
  return orderAttack(g, corps, target, result.R, fieldBill(result));
}

/** The odds the campaign would give this attack now, for a check that nothing moved between the ticket and the hand-back. */
export function oddsNow(g: Game, corps: Corps[], target: Region): number {
  return assess(g, goingIn(g, corps, target), target).odds;
}
