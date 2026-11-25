// The campaign's fleet action written out as a ticket for the field: who meets whom in which sea, with the weights
// the rules give them. Read-only: building a ticket rolls no dice and changes nothing in the game.
//
// A ship's weight on the field is its weight in the rules' fleet action (navy.ts fleetPower): the class's power, the
// carrier's deck, and the fleet's own multiplier (quality, fitness for sea, admiral, story, traits). Submarines fight
// no fleet action, but the rules count their small weight in a fleet that has surface ships, so it is shared out over
// that fleet's surface ships: the two sides meet on the field at exactly the odds the rules weigh.

import { hashString } from '../../core/rng';
import { turnToDate, type TicketLine } from '../../core/ticket';
import { deckWorth } from '../../campaign/carriers';
import { fleetPower, toughOf, type SeaMeeting } from '../../campaign/navy';
import { dcOf, shipKit, techRules } from '../../campaign/tech';
import type { Fleet, Game } from '../../campaign/types';
import type { ForceTicket, Side, SideTicket } from '../core/types';
import { DICE, RULES_CLASSES, SEA, waterOf } from './numbers';
import { shipWords } from './result';
import { hullsOf } from './pieces';
import { SEA_CLASSES, SEA_KIND, type FleetTicket, type SeaClass, type SeaSailors, type SeaShips, type SeaTicket, type ShipRow } from './types';
import { seaAction } from '../../campaign/sea-words';

const surfaceOf = (f: Fleet): SeaShips => ({ bb: f.ships.bb, cv: f.ships.cv, ca: f.ships.ca, dd: f.ships.dd });

/**
 * A fleet's rows as the ticket carries them (ships by class, plan 5.1 c1), from the campaign's (shiprows.ts fleetRows,
 * with its switch on): the field's kinds only, each row with only its id, label, worth and n (plan 6.1, test 6), and
 * never more hulls than the kind has, the last rows cut. Undefined where none is left.
 */
export function rowsOf(given: Partial<Record<string, readonly ShipRow[]>> | undefined, ships: SeaShips): Partial<Record<SeaClass, ShipRow[]>> | undefined {
  if (!given) return undefined;
  const out: Partial<Record<SeaClass, ShipRow[]>> = {};
  for (const c of SEA_CLASSES) {
    let left = Math.floor(ships[c] + 1e-9);
    const list: ShipRow[] = [];
    for (const r of given[c] ?? []) {
      const n = Math.min(Math.floor(r.n), left);
      if (n <= 0) continue;
      list.push({ id: r.id, label: r.label, worth: r.worth, n });
      left -= n;
    }
    if (list.length) out[c] = list;
  }
  return Object.keys(out).length ? out : undefined;
}

/**
 * One campaign fleet as the field receives it. 'rows': its ships by row where the campaign plays them by class
 * (shiprows.ts fleetRows): its ships' raw weight then sums each hull's worth, so 'mul' stays the fleet's own multiplier
 * and each ship weighs her row's worth times it (plan 5.1 c1).
 */
export function fleetTicket(g: Game, f: Fleet, side: Side, given?: Partial<Record<string, readonly ShipRow[]>>): FleetTicket {
  const deck = f.ships.cv > 0 ? deckWorth(g, f) : 1;
  const ships = surfaceOf(f), rows = rowsOf(given, ships);
  const raw = SEA_CLASSES.reduce((s, c) => s + hullsOf({ ships, rows }, c) * RULES_CLASSES[c].power * (c === 'cv' ? deck : 1), 0);
  const bombers = g.airFleets.filter((w) => w.ship === f.id).reduce((n, w) => n + w.bmr, 0);
  // Ruling 240: the fleet's own share of loss by class where its damage control eases it, and its ships' kit where they
  // have any; neither is written otherwise, so a ticket with the switches off is release 1's to the byte.
  const kit = shipKit(g, f.nat);
  return {
    id: f.id, name: f.name, nat: f.nat, side, ships,
    mul: raw > 0 ? fleetPower(g, f) / raw : 1, deck, bombers,
    admiral: f.admiral ? { name: f.admiral.name, level: f.admiral.level } : null,
    ...(dcOf(g, f.nat) !== 1 ? { tough: { bb: toughOf(g, f, 'bb'), cv: toughOf(g, f, 'cv'), ca: toughOf(g, f, 'ca'), dd: toughOf(g, f, 'dd') } } : {}),
    ...(kit.stage || kit.vt || kit.dc ? { kit } : {}),
    ...(rows ? { rows } : {}),
  };
}

const forceOf = (f: FleetTicket, weight: number, total: number): ForceTicket => {
  const n = SEA_CLASSES.reduce((s, c) => s + f.ships[c], 0);
  return {
    corps: f.id, name: f.name, nat: f.nat, kind: 'fleet', divs: n,
    men: SEA_CLASSES.reduce((s, c) => s + f.ships[c] * SEA.classes[c].crew, 0),
    str: 1, org: 1, xp: f.admiral ? f.admiral.level / 5 : 0, share: total > 0 ? weight / total : 0,
  };
};

const weightOf = (f: FleetTicket): number => SEA_CLASSES.reduce((s, c) => s + hullsOf(f, c) * RULES_CLASSES[c].power * (c === 'cv' ? f.deck : 1) * f.mul, 0);

function sideTicket(nat: string, adj: string, color: string, human: boolean, fleets: FleetTicket[], doctrine: string[] = []): SideTicket {
  const total = fleets.reduce((s, f) => s + weightOf(f), 0);
  const top = [...fleets].sort((a, b) => weightOf(b) - weightOf(a))[0];
  return {
    nat, adj, color, human, forces: fleets.map((f) => forceOf(f, weightOf(f), total)), general: top?.admiral ?? null,
    ...(doctrine.length ? { doctrine } : {}),
  };
}

/**
 * The research a navy's carriers grow by (sea.json carriers.grow: Carrier task forces), as its nation holds it: read
 * straight off the nation's record, as the land ticket reads doctrines, never through techOf (the ticket only reads),
 * and only where the campaign keeps research. Never priced on the field: the card prices it already (b418).
 */
function researchOf(g: Game, nat: string): string[] {
  const done = techRules(g) ? g.nations[nat]?.tech?.done ?? [] : [];
  return SEA.carriers.grow.filter((id) => done.includes(id));
}

/** The lines the card shows before the fight, in the rules' own words (navy.ts seaBattles, seaplan.ts). */
function linesOf(fleets: FleetTicket[], air: Record<Side, number>, odds: number): TicketLine[] {
  const ships = (side: Side): SeaShips => { const s: SeaShips = { bb: 0, cv: 0, ca: 0, dd: 0 }; for (const f of fleets) if (f.side === side) for (const c of SEA_CLASSES) s[c] += f.ships[c]; return s; };
  const lines: TicketLine[] = [{ label: 'Forces', value: `${shipWords(ships('att'))} against ${shipWords(ships('def'))}` }];
  if (air.att > 0.5 || air.def > 0.5) lines.push({ label: 'Air', value: air.att > air.def ? 'The attacker has the stronger air cover from the coast' : air.def > air.att ? 'The defender has the stronger air cover from the coast' : 'Even', effect: `Worth ${Math.round(air.att)} against ${Math.round(air.def)}` });
  lines.push({ label: 'Odds', value: `${odds.toFixed(1)} to 1`, effect: `Between ${(odds * DICE[0]).toFixed(1)} and ${(odds * DICE[1]).toFixed(1)} to 1 as the day goes` });
  return lines;
}

/**
 * Whether the field can fight this action: each side has a gun ship (a battleship, cruiser or destroyer). An action
 * where one side has none, such as carriers against carriers, is an air battle, and the field flies no planes: the
 * staff settles it, not offered on the field, as any action the field cannot honour is (the design director, b438,
 * as b418). The Pacific's carrier actions mostly fall here.
 */
export function fieldFights(t: SeaTicket): boolean {
  const guns = (side: Side): boolean => t.place.fleets.some((f) => f.side === side && f.mul > 0 && f.ships.bb + f.ships.ca + f.ships.dd > 0);
  return guns('att') && guns('def') && !(SEA.carrierBattle === 'staff' && carriersOnly(t));
}

/** Both sides bring carriers and neither a battleship (ruling 239, 11): what 'carrierBattle' decides, and what the design
 *  director's count counts. */
export function carriersOnly(t: SeaTicket): boolean {
  const has = (side: Side, c: 'bb' | 'cv'): boolean => t.place.fleets.some((f) => f.side === side && f.mul > 0 && f.ships[c] > 0);
  return has('att', 'cv') && has('def', 'cv') && !has('att', 'bb') && !has('def', 'bb');
}

/**
 * Men in the water (release 2's sailor rules): on in a grand campaign whose 'sailors' flag is set, as the support teams'
 * switch is (teams.ts teamsOn). The flag is the campaign's, set by setSailors (the options page's 'sailorsrule'); the
 * ticket only reads it.
 */
export const sailorsOn = (g: Game): boolean => g.flags.open === 1 && g.flags.sailors === 1;

/** Turns the sailor rules on or off in a campaign under way, as the voyage log's switch is (voyage.ts setVoyageLog): off
 *  deletes the flag, so the save is as one that never had them. It changes nothing else. */
export function setSailors(g: Game, on: boolean): void {
  if (on) g.flags.sailors = 1;
  else delete g.flags.sailors;
}

/**
 * Captains mind their own side's torpedoes (release 2's own water: Brian, 25 November 12:52Z; sim.ts ownWater): on in a
 * grand campaign whose 'ownWater' flag is set, as the sailor rules are. Set by setOwnWater (the options page's
 * 'ownwaterrule'); the ticket only reads it.
 */
export const ownWaterOn = (g: Game): boolean => g.flags.open === 1 && g.flags.ownWater === 1;

/** Turns the own-water rule on or off in a campaign under way, as the sailor rules' switch is: off deletes the flag, so
 *  the save is as one that never had it. It changes nothing else. */
export function setOwnWater(g: Game, on: boolean): void {
  if (on) g.flags.ownWater = 1;
  else delete g.flags.ownWater;
}

/**
 * What the field needs of men in the water for an action in this sea this month, with the switch on: the water's band
 * from the historian's table. The calls are left to the staff (stop for survivors where it holds the water; fight for
 * every ship) until the campaign carries the player's calls and the staff's submarine rule (form B3: the rules
 * builder's). Null with the switch off, or for a sea the table lacks.
 */
export function sailorsOf(g: Game, zone: number): SeaSailors | null {
  if (!sailorsOn(g)) return null;
  const water = waterOf(g.seas[zone].key, turnToDate(g.turn).month);
  return water ? { water } : null;
}

/** A fleet action the rules are about to fight (navy.ts seaBattles, through seaField), written out for the field. */
export function seaTicket(g: Game, m: SeaMeeting): SeaTicket {
  const zone = g.seas[m.zone];
  const byId = new Map(g.fleets.map((f) => [f.id, f]));
  const fleets = [
    ...m.mine.map((id) => fleetTicket(g, byId.get(id)!, 'att')),
    ...m.met.map((id) => fleetTicket(g, byId.get(id)!, 'def')),
  ];
  const attNat = m.nat, defNat = byId.get(m.met[0])!.nat;
  const nation = (nat: string) => g.nations[nat];
  const human = (side: Side): boolean => !!g.human && fleets.some((f) => f.side === side && f.nat === g.human);
  const odds = m.A / Math.max(1e-6, m.D);
  const air = { att: m.airA, def: m.airD };
  const sailors = sailorsOf(g, m.zone);
  return {
    domain: 'sea', kind: SEA_KIND,
    seed: hashString(`seafield:${g.seed}:${g.turn}:${m.zone}:${m.nat}`),
    turn: g.turn, date: turnToDate(g.turn).label,
    // The second in the same sea this month is "The second action ...", as the battle stars name it.
    title: seaAction(zone.name, 1 + g.seaReports.filter((r) => r.turn === g.turn && r.zone === m.zone).length),
    place: { zone: m.zone, zoneName: zone.name, fleets, air, ...(sailors ? { sailors } : {}), ...(ownWaterOn(g) ? { ownWater: true as const } : {}) },
    att: sideTicket(attNat, nation(attNat).adj, nation(attNat).color, human('att'), fleets.filter((f) => f.side === 'att'), researchOf(g, attNat)),
    def: sideTicket(defNat, nation(defNat).adj, nation(defNat).color, human('def'), fleets.filter((f) => f.side === 'def'), researchOf(g, defNat)),
    reckoning: { A: m.A, D: m.D, odds, dice: DICE, even: 1 },
    lines: linesOf(fleets, air, odds),
    limit: SEA.field.limit,
  };
}

/** A plain fleet for tests and the browser preview, with no game behind it. */
export interface PlainFleet { name: string; nat: string; ships: Partial<SeaShips>; mul?: number; deck?: number; bombers?: number; admiral?: number; rows?: Partial<Record<SeaClass, ShipRow[]>> }

/**
 * A ticket with no game behind it: two sides' fleets, their coasts' air, and a seed. 'odds', if given, scales the
 * defender's fleets so the two sides meet at exactly those odds (tests and calibration only). 'title' heads the field
 * in place of the sea's ("Two battle lines" for a practice action: practice.ts). 'research': what each side holds of the
 * research its carriers grow by (sea.json carriers.grow), set by hand; none where left out. 'sailors': men in the water,
 * as the switch would put them on the ticket (its 'early' names fleets by their place in att then def, from 1, as the
 * ids are given here); none where left out.
 */
export function plainTicket(o: { att: PlainFleet[]; def: PlainFleet[]; air?: Partial<Record<Side, number>>; seed: number; odds?: number; human?: Side | null; zoneName?: string; title?: string; research?: Partial<Record<Side, string[]>>; sailors?: SeaSailors; ownWater?: boolean }): SeaTicket {
  let id = 1;
  const make = (p: PlainFleet, side: Side): FleetTicket => {
    const ships = { bb: p.ships.bb ?? 0, cv: p.ships.cv ?? 0, ca: p.ships.ca ?? 0, dd: p.ships.dd ?? 0 }, rows = rowsOf(p.rows, ships);
    return {
      id: id++, name: p.name, nat: p.nat, side, ships,
      mul: p.mul ?? 1, deck: p.deck ?? 1, bombers: p.bombers ?? 24 * (p.ships.cv ?? 0),
      admiral: p.admiral ? { name: `Admiral ${p.name}`, level: p.admiral } : null,
      ...(rows ? { rows } : {}),
    };
  };
  const fleets = [...o.att.map((p) => make(p, 'att')), ...o.def.map((p) => make(p, 'def'))];
  const air = { att: o.air?.att ?? 0, def: o.air?.def ?? 0 };
  const sum = (side: Side): number => fleets.filter((f) => f.side === side).reduce((s, f) => s + weightOf(f), 0) + air[side];
  if (o.odds !== undefined) {
    const k = sum('att') / (o.odds * Math.max(1e-6, sum('def')));
    for (const f of fleets) if (f.side === 'def') f.mul *= k;
    air.def *= k;
  }
  const A = sum('att'), D = sum('def'), odds = A / Math.max(1e-6, D);
  const nat = (side: Side): string => fleets.find((f) => f.side === side)?.nat ?? side;
  return {
    domain: 'sea', kind: SEA_KIND, seed: o.seed, turn: 0, date: 'September 1939',
    title: o.title ?? seaAction(o.zoneName ?? 'North Sea'),
    place: { zone: 0, zoneName: o.zoneName ?? 'North Sea', fleets, air, ...(o.sailors ? { sailors: o.sailors } : {}), ...(o.ownWater ? { ownWater: true as const } : {}) },
    att: sideTicket(nat('att'), nat('att'), '#5577aa', o.human === 'att', fleets.filter((f) => f.side === 'att'), o.research?.att),
    def: sideTicket(nat('def'), nat('def'), '#aa5544', o.human === 'def', fleets.filter((f) => f.side === 'def'), o.research?.def),
    reckoning: { A, D, odds, dice: DICE, even: 1 },
    lines: linesOf(fleets, air, odds),
    limit: SEA.field.limit,
  };
}
