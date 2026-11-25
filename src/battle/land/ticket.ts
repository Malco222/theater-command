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
import { SIEGE, guardForce, guardMen, guardOf, guardPower } from '../../campaign/siege';
import { stopOf } from '../../campaign/history';
import { techRules } from '../../campaign/tech';
import { isFoot } from '../../campaign/ground';
import { SUPPORT } from '../../campaign/support';
import { canOrder, jointForce, orderAttack, previewAttack } from '../../campaign/turn';
import { corpsKnown, oddsBand } from '../../campaign/intel';
import { KIND, type BattleReport, type Corps, type Game, type Region } from '../../campaign/types';
import type { AirOnCall, FieldResult, FieldTicket, ForceKit, ForceTicket, SideTicket } from '../core/types';
import { streetsOf, type LandPlace } from './field';
import { LAND } from './rules';
import { bearingOf, streamFor } from './stream';

/** The corps that would actually go in, as the attack card and the battle count them. */
export function goingIn(g: Game, corps: Corps[], target: Region): Corps[] {
  return jointForce(g, limitAttackers(g, jointForce(g, corps, target), target), target);
}

/** The research a computer general's style grows by (data/battle/land-styles.json grow.tech). */
const DOCTRINES = ['combined', 'deep'];

/**
 * Only anti-tank weapons take a tank (LAND.pierce; the design director's pierce ruling, 25 Nov, P3): what a corps on
 * foot (isFoot: not tanks, guns or teams) carries that can pierce armour, from its own nation's research: Anti-tank
 * guns, Tank destroyers and the hand-held weapons (support.json tech). Read straight off the nation's record, as the
 * doctrine line below is, and only with the research rules on: never through techOf or hasTech, which open research on
 * a first look. Nothing where it holds none of the three, or without research rules: the plain kit, both sides alike.
 * Never the nation's name, never the date. Read by the field only with LandBattle.pierce on.
 */
function pierceKit(g: Game, c: Corps): { kit: ForceKit } | Record<string, never> {
  if (!techRules(g) || !isFoot(c)) return {};
  const done = g.nations[c.nat]?.tech?.done, T = SUPPORT.tech;
  if (!done) return {};
  const kit: ForceKit = { ...(done.includes(T.antiTank) ? { atGuns: true } : {}), ...(done.includes(T.tankDestroyers) ? { atTd: true } : {}), ...(done.includes(T.handHeld) ? { atHand: true } : {}) };
  return Object.keys(kit).length ? { kit } : {};
}

function sideOf(g: Game, nat: string, cs: Corps[], weigh: (c: Corps) => number, crosses?: (c: Corps) => boolean): SideTicket {
  const N = g.nations[nat];
  const w = cs.map(weigh), tot = w.reduce((a, b) => a + b, 0) || 1;
  const forces: ForceTicket[] = cs.map((c, i) => ({
    corps: c.id, name: c.name, nat: c.nat, kind: c.kind, divs: c.divs, men: men(c), str: c.str, org: c.org, xp: c.xp,
    ...(c.mor !== undefined ? { mor: c.mor } : {}), ...(c.dug ? { dug: true } : {}), ...(c.dn !== undefined ? { first: c.dn } : {}),
    // Each block is one of these divisions, named as the army tray names it (the UI and design directors, b358).
    divisions: divisionsOf(g, c).map((d) => ({ name: d.name, short: d.short, str: d.str, xp: d.xp, men: d.men })),
    share: w[i] / tot,
    battles: c.rec?.battles ?? 0,
    // A corps the card has come on with no river before it does not cross on the field either (b464(5)).
    ...(crosses && !crosses(c) ? { cross: false } : {}),
    // How worn its uniforms and kit look (release 2, looks only): absent while it is fresh.
    ...(c.wear !== undefined ? { wear: c.wear } : {}),
    ...(c.fresh ? { fresh: c.fresh } : {}),
    ...pierceKit(g, c),
  }));
  // The general of the strongest corps commands on the field.
  const lead = cs.map((c) => generalOf(g, c)).filter((x) => !!x).sort((a, b) => b!.level - a!.level)[0] ?? null;
  // What a computer general's style grows by besides its army's experience (styles.ts): the doctrines its nation holds.
  // Read straight off the nation's record, never through techOf, which opens research on a first look: the ticket only reads.
  const doctrine = techRules(g) ? DOCTRINES.filter((id) => !!N?.tech?.done.includes(id)) : [];
  return {
    nat, adj: N?.adj ?? nat, color: N?.color ?? '#888888', human: nat === g.human, forces, general: lead ? { name: lead.name, level: lead.level } : null,
    ...(doctrine.length ? { doctrine } : {}),
  };
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

/**
 * Where some of the attack's corps come on with no river before them and others cross (b464(5)): the end of the field
 * those that do not cross take, the end on their side as the attack faces the place, its left the field's west (the
 * campaign map's y runs south). Null where all cross. Even, the west.
 */
function dryEnd(g: Game, going: Corps[], target: Region): 'west' | 'east' | null {
  const dry = going.filter((c) => !riverBetween(g, c.reg, target.id)), wet = going.filter((c) => !dry.includes(c));
  if (!dry.length || !wet.length) return null;
  const mid = (cs: Corps[]): [number, number] => [cs.reduce((a, c) => a + g.regions[c.reg].x, 0) / cs.length, cs.reduce((a, c) => a + g.regions[c.reg].y, 0) / cs.length];
  const [mx, my] = mid(going), dx = target.x - mx, dy = target.y - my;
  const left = (cs: Corps[]): number => { const [x, y] = mid(cs); return (x - mx) * dy - (y - my) * dx; };
  return left(dry) >= left(wet) ? 'west' : 'east';
}

/**
 * One side's fighters on call over the battle, as the attack card counted them (Brian, 2026-11-18 18:58Z; the design
 * director, b533 and b536): the share of the side's strength they make on the card, the strikes and the groups, read
 * straight from the card's own air row (Assessment.air in game.ts, CloseSide in air.ts) and never worked out again.
 * Read by shape, so the field takes it from the card wherever the card carries it. Nothing where a side has nothing on
 * call: that field is the one it was without air (b533's bar).
 */
type CardAir = { att?: Partial<AirOnCall>; def?: Partial<AirOnCall> };
function onCall(s: Partial<AirOnCall> | undefined): { air: AirOnCall } | Record<string, never> {
  if (!s || !(Number(s.share) > 0) || !(Number(s.strikes) > 0) || !s.groups?.length) return {};
  return { air: { share: s.share!, strikes: Math.floor(s.strikes!), groups: s.groups.map((x) => ({ id: x.id, name: x.name, nat: x.nat, planes: x.planes, lose: x.lose })) } };
}

/** The ticket for an attack the player could order now, or null where there is no attack to fight. */
export function landTicket(g: Game, chosen: Corps[], target: Region): FieldTicket<LandPlace> | null {
  // The army the attack order will send, and no other: a corps spent on a probe since it was chosen, or no longer next
  // to the target, stays out of the field as the hand-back leaves it out of the report (the review, b453).
  const corps = chosen.filter((c) => canOrder(g, c) && c.ap > 0 && g.regions[c.reg].nb.includes(target.id));
  const a = previewAttack(g, corps, target);
  if (!a || !a.defenders.length) return null;
  const going = goingIn(g, corps, target);
  if (!going.length) return null;
  const attNat = armyOf(going[0]);
  const defNat = a.defenders[0].nat;
  const crossing = going.map((c) => riverBetween(g, c.reg, target.id)).find((x) => !!x) ?? null;
  const from = g.regions[going[0].reg];
  const dry = crossing ? dryEnd(g, going, target) : null;
  const place: LandPlace = {
    region: target.id, name: target.name, terrain: target.terrain,
    river: crossing ? riverName(crossing) : null, bridgeDown: crossing ? bridgeDown(g, crossing) : false,
    town: townName(target), townSize: townSize(g, target),
    fort: target.fort && target.fort.vs.includes(from.owner) ? target.fort.lv : 0,
    mines: minesAt(g, target),
    ...(crossing && CROSSINGS[target.name]?.river === riverName(crossing) ? { crossings: CROSSINGS[target.name].at } : {}),
    ...(dry ? { dryEnd: dry } : {}),
  };
  // A capital's or a city's own river through its streets, as it lies to the way this attack comes on (b526): none
  // where the card prices a crossing, so one river a field.
  if (!crossing && streetsOf(place)) {
    const mx = going.reduce((s, c) => s + g.regions[c.reg].x, 0) / going.length, my = going.reduce((s, c) => s + g.regions[c.reg].y, 0) / going.length;
    const stream = streamFor(place.town, bearingOf(mx, my, target.x, target.y));
    if (stream) place.stream = stream;
  }
  const date = turnToDate(g.turn);
  // The odds as the attack card put them to the player (attackOutlook in turn.ts): a band where it did not know the
  // defence, its bar at the middle. The safety net reads the card (offered.ts), never the truth behind it.
  const band = oddsBand(g, a.defenders, a.odds, target.id, g.human);
  const cardAir = (a as { air?: CardAir }).air;
  const weighDef = (c: Corps): number => corpsBase(g, c) * KIND[c.kind].def * (1 + 0.25 * Math.min(1, worksOf(c)));
  return {
    domain: 'land', kind: 'field', seed: hashString(`field:${g.seed}:${g.turn}:${target.id}:${attNat}`) >>> 0,
    turn: g.turn, date: date.label, title: `The Battle of ${target.name}`, place,
    // The card's river line counts the corps that cross as those with the river between them and the place (assess in
    // game.ts): the field lays the river before those, and no others.
    att: { ...sideOf(g, attNat, going, (c) => corpsBase(g, c) * KIND[c.kind].atk, crossing ? (c) => !!riverBetween(g, c.reg, target.id) : undefined), ...onCall(cardAir?.att) },
    def: { ...withGuard(g, target, a.guard ?? 0, a.defenders.reduce((s, c) => s + weighDef(c), 0), sideOf(g, defNat, a.defenders, weighDef)), ...onCall(cardAir?.def) },
    reckoning: { A: a.A, D: a.D, odds: a.odds, dice: [a.dice[0], a.dice[1]], even: evenOdds(a.dice), ...(a.luck ? { luck: [a.luck[0], a.luck[1]] as [number, number] } : {}), ...(band.exact ? {} : { seen: band.mid }) },
    lines: a.lines, limit: LAND.limit,
    // The attacks already made on this place this month by this side: the same ground, a fresh day's luck.
    attempt: g.reports.filter((x) => x.turn === g.turn && x.region === target.id && x.attNat === attNat).length,
    // The history dial's stop: at 1 to 3 the computer general fights in its nation's style, at 4 and 5 the situation's (styles.ts).
    hist: stopOf(g),
  };
}

/**
 * The town's guard where it fights beside the field army (ruling 232): on the field as a block of its own, so what the
 * field draws is what the card counted and the field's bill falls on the guard as the field had it (the coordinator's
 * ruling, 22 November; b464). Its share of the defence's power by its worth (guardPower) behind its walls, weighed as a
 * corps fully dug in is; its men the guard's; depot troops, police and militia (green, loosely held). Id by
 * guardForce. The side as it was where no guard stands in the field.
 */
function withGuard(g: Game, target: Region, guard: number, tot: number, side: SideTicket): SideTicket {
  const d = guard > 0 ? guardOf(g, target) : 0;
  if (d <= 0) return side;
  // Weighed as the corps are ('tot', their weights summed): the corps' shares scale down by what the guard adds.
  const w = guardPower(g, target) * 1.25, all = tot + w;
  if (!(all > 0)) return side;
  const men = guardMen(g, target), n = Math.max(1, Math.round(d)), str = Math.min(1, d / n);
  const name = `Town guard of ${townName(target)}`, short = `${townName(target)} guard`;
  const force: ForceTicket = {
    corps: guardForce(target), name, nat: target.ctrl, kind: 'inf', divs: d, men, str, org: SIEGE.guardOrg, xp: 0.1, mor: 0.5, dug: true,
    divisions: Array.from({ length: n }, (_, i) => ({ name: n > 1 ? `${name} (${i + 1})` : name, short, str, xp: 0.1, men: Math.round(men / n) })),
    share: w / all, battles: 0,
  };
  return { ...side, forces: [...side.forces.map((f) => ({ ...f, share: (f.share * tot) / all })), force] };
}

/**
 * What the field cost, for the campaign's bill (ruling 226(6)): each corps' own share lost, so the side's bill falls on
 * its corps as the field had it, and each side's share, so the after card's headline and its field line agree; and the
 * army that broke, whose bill the rules settle for the pursuit (b464).
 */
export function fieldBill(result: FieldResult): FieldBill {
  const corps: Record<number, number> = {};
  for (const f of result.forces ?? []) corps[f.corps] = f.lost;
  // The strikes each group flew, by group id, for the rules to book (AttackOpts.closeSorties): what was flown, not what
  // the card offered. Only where some side had fighters on call.
  const sorties: Record<number, number> = {};
  for (const s of ['att', 'def'] as const) for (const f of result.air?.[s] ?? []) sorties[f.group] = (sorties[f.group] ?? 0) + f.sorties;
  return {
    corps, att: result.lost.att, def: result.lost.def, whole: result.how !== 'objective', ...(result.broke ? { broke: result.broke } : {}), ...(result.pulled ? { pulled: { ...result.pulled } } : {}),
    ...(result.air ? { air: sorties } : {}),
  };
}

/**
 * The after card's line for the pursuit (b464, the UI director's words), straight after the field's strength line:
 * where the army that broke was booked more men than its share lost on the field, the rest fell in the pursuit and as
 * prisoners. Null where it was not (a rounding's worth is not a pursuit). The price line already counts these men.
 */
export function pursuitLine(t: FieldTicket<LandPlace>, result: FieldResult, rep: BattleReport): string | null {
  // The army that broke, or the one that pulled out under fire (b474): its rearguard and stragglers.
  const s = result.broke ?? result.pulled?.side;
  // Only where that army also lost the day, as the bill has it (game.ts: R under 1.2 for the attack).
  if (!s || (s === 'def') !== result.R >= 1.2) return null;
  const us = s === 'att' ? rep.attackers : rep.defenders;
  const lost = Math.max(s === 'att' ? rep.attLost : rep.defLost, us.reduce((a, u) => a + u.lost, 0));
  const onField = Math.round(result.lost[s] * t[s].forces.reduce((a, f) => a + f.men, 0));
  const more = lost - onField;
  if (more <= Math.max(10, onField * 0.01)) return null;
  const army = t[s].human ? 'your army' : `the ${t[s].adj} army`;
  if (!result.broke) return `As ${army} withdrew under fire, it lost ${more.toLocaleString('en-GB')} more men in the rearguard and as stragglers.`;
  return `After ${army} broke, it lost ${more.toLocaleString('en-GB')} more men in the pursuit and as prisoners.`;
}

/**
 * The other side's corps the player's staff has identified, as the attack card names them (this month's or an old
 * report): on a field the card did not fully know, their blocks show as the card shows them and only the rest stay
 * plain stands until met (the design director, b476(3)).
 */
export function cardKnows(g: Game, t: FieldTicket<LandPlace>): Set<number> {
  const them = t.att.human ? t.def : t.att;
  const out = new Set<number>();
  for (const f of them.forces) {
    // The town's guard on the field (ruling 232, b464) is no secret: the card names it beside the field army.
    if (f.corps < 0) { out.add(f.corps); continue; }
    const c = g.corps.find((x) => x.id === f.corps);
    if (c && corpsKnown(g, c, g.human).level !== 'none') out.add(f.corps);
  }
  return out;
}

/** A side's corps on a field: its forces but the town's guard standing beside them (ruling 232, guardForce). */
export const corpsOn = (s: SideTicket): number => s.forces.filter((f) => f.corps >= 0).length;

/** Hands a fought battle back: the rules' own attack order, with the field's R in place of the dice. */
export function handBack(g: Game, corps: Corps[], target: Region, result: FieldResult): BattleReport | null {
  // The field drew a capital or a city as streets (read before the attack moves anything): the after card tells the
  // battle as fought in them.
  const streets = streetsOf({ terrain: target.terrain, townSize: townSize(g, target) } as LandPlace);
  const rep = orderAttack(g, corps, target, result.R, fieldBill(result));
  if (rep && streets) rep.streets = streets;
  return rep;
}

/** The odds the campaign would give this attack now, for a check that nothing moved between the ticket and the hand-back. */
export function oddsNow(g: Game, corps: Corps[], target: Region): number {
  return assess(g, goingIn(g, corps, target), target).odds;
}
