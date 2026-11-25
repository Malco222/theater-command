// Carrier strikes on a harbour by the player's own hand (release 2; Brian, 24 November 2026 21:45Z, "Resume the
// Pacific for release 2", on the card about Pearl Harbor; the 19 November Pacific playtest's first finding).
//
// A player who runs his navy himself could already raid a harbour from his carriers, but only in four separate orders
// across two desks, none of which said what it was for: move the carrier fleet's base forward while at peace, declare
// war, sail into a sea from which its aircraft reach the harbour, and send each wing in. Nothing told him that from
// Kure Pearl Harbor is beyond the carriers' aircraft, or that from Hitokappu Bay in the Kuriles it is not. Here a
// carrier fleet's page lists the enemy harbours where warships lie at anchor, with what is there as his side knows it,
// and gives one order for each:
//
//   in reach    STRIKE FROM THE AIR: the fleet sails to the nearest sea in its reach from which its strike aircraft
//               reach the harbour, and its wings are sent at the harbour. They fly with the month's air war, as every
//               raid does (air.ts), and a raid in the month war was declared goes in without warning.
//   too far     the nearest base of his from which they would reach it, and an order to move there.
//   at peace    the same reckoning, and the line that war must be declared first; a strike flown in the month it is
//               declared goes in without warning.
//
// The player's order only: no staff reads any of it, so every computer war is the war without it. The grand
// campaign's flag 'carrierStrike', set as a campaign opens where stage.json switches it on (or by the player in a
// campaign under way). Off, nothing is offered.

import { carrierRules, harbourInReach, setMission, whyNotMission } from './air';
import { fleetKnown, wingsOf } from './carriers';
import { allied, atWar, theName } from './game';
import { CLASSES, atSea, baseTargets, fleetName, fleetReach, passageWhy, sail, shipCount, shipList, surfaceShips } from './navy';
import type { AirFleet, Fleet, Game, Region, Ships } from './types';

export const carrierStrikeOn = (g: Game): boolean => g.flags.open === 1 && g.flags.carrierStrike === 1;

/** Switches the carrier strike on a harbour on or off in a campaign under way. */
export function setCarrierStrike(g: Game, on: boolean): void {
  if (on) g.flags.carrierStrike = 1;
  else delete g.flags.carrierStrike;
}

/** The player's own fleet, in his own month, or null. */
const ownFleet = (g: Game, id: number): Fleet | null => {
  const f = g.fleets.find((x) => x.id === id);
  return f && f.nat === g.human && g.order[g.phase] === f.nat ? f : null;
};

/** The wing aboard that leads a strike (its strike aircraft), and the fighters that may go with it. */
const leadOf = (g: Game, c: Fleet): AirFleet | undefined => wingsOf(g, c.id).find((w) => w.kind !== 'ftr' && w.bmr > 0);
const fightersOf = (g: Game, c: Fleet): AirFleet | undefined => wingsOf(g, c.id).find((w) => w.kind === 'ftr' && w.ftr > 0);

/** Reads `test` with the fleet put, for the reading only, at sea in `zone` (or at base `base`): the fleet as it was after. */
function placed<T>(c: Fleet, at: { zone?: number; base?: Region }, test: () => T): T {
  const keep = { zone: c.zone, stance: c.stance, base: c.base, home: c.home };
  if (at.base) { c.base = at.base.id; c.home = at.base.seas[0]; c.zone = c.home; c.stance = 'port'; }
  if (at.zone !== undefined) { c.zone = at.zone; c.stance = 'patrol'; }
  try { return test(); } finally { c.zone = keep.zone; c.stance = keep.stance; c.base = keep.base; c.home = keep.home; }
}

/** The nearest sea in the fleet's reach from which its strike aircraft reach the harbour (the sea it is in first), or -1. */
export function launchSea(g: Game, c: Fleet, r: Region): number {
  const lead = leadOf(g, c);
  if (!lead) return -1;
  if (atSea(c) && harbourInReach(g, lead, r.id)) return c.zone;
  for (const z of fleetReach(g, c)) if (placed(c, { zone: z }, () => harbourInReach(g, lead, r.id))) return z;
  return -1;
}

/** A harbour of his side's from which the fleet's aircraft would reach `r`, nearest the fleet's own base first; null if none. */
export function forwardBase(g: Game, c: Fleet, r: Region): Region | null {
  const lead = leadOf(g, c);
  if (!lead) return null;
  const from = g.regions[c.base];
  const near = (b: Region): number => (from ? Math.hypot(b.x - from.x, b.y - from.y) : 0);
  for (const b of baseTargets(g, c).sort((a, b2) => near(a) - near(b2) || a.id - b2.id)) {
    if (placed(c, { base: b }, () => fleetReach(g, c).some((z) => placed(c, { zone: z }, () => harbourInReach(g, lead, r.id))))) return b;
  }
  return null;
}

/** The warships of another country lying in a harbour, as `viewer`'s side knows them. */
function atAnchor(g: Game, r: Region, nat: string, viewer: string): Fleet[] {
  return g.fleets.filter((x) => x.base === r.id && !atSea(x) && x.nat !== nat && !allied(g, x.nat, nat) && surfaceShips(x) > 0 && fleetKnown(g, x, viewer) !== 'none');
}

/** One harbour on a carrier fleet's page. */
export interface StrikeHarbour {
  region: number;
  name: string;
  /** The country that holds it, and whether it is at war with the fleet's. */
  holder: string;
  atWar: boolean;
  /** The fleets known to lie there, in words, and their ships where his side has identified them. */
  fleets: string[];
  ships: Ships | null;
  /** The sea it sails to and strikes from, or -1; else a base of his from which its aircraft would reach, or -1. */
  launch: number;
  forward: number;
  /** Why the strike cannot be ordered now, or null. */
  why: string | null;
}

/**
 * The enemy harbours a carrier fleet's page lists: those where warships lie at anchor, of a country at war with his
 * or (where `mayDeclare` says so) one he could declare war on, that its strike aircraft reach from a sea in its
 * reach or would reach from another base of his. At war first, then in reach, then by name. Read-only.
 */
export function strikeHarbours(g: Game, id: number, mayDeclare: (nat: string) => boolean = () => false): StrikeHarbour[] {
  const c = g.fleets.find((x) => x.id === id);
  if (!carrierStrikeOn(g) || !carrierRules(g) || !c || c.ships.cv <= 0 || !leadOf(g, c) || c.base < 0) return [];
  const viewer = c.nat;
  const out: StrikeHarbour[] = [];
  for (const r of g.regions) {
    if (!r.port || !r.seas.length || r.ctrl === c.nat || allied(g, r.ctrl, c.nat)) continue;
    const war = atWar(g, c.nat, r.ctrl);
    if (!war && !mayDeclare(r.ctrl)) continue;
    const there = atAnchor(g, r, c.nat, viewer);
    if (!there.length) continue;
    const launch = launchSea(g, c, r);
    const forward = launch < 0 ? forwardBase(g, c, r) : null;
    if (launch < 0 && !forward) continue;
    const exact = there.every((x) => fleetKnown(g, x, viewer) === 'exact');
    const ships = exact ? there.reduce((s, x) => { for (const k of CLASSES) s[k] += x.ships[k]; return s; }, { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 } as Ships) : null;
    out.push({
      region: r.id, name: r.name, holder: r.ctrl, atWar: war,
      fleets: anchorNames(g, there, viewer),
      ships, launch, forward: forward?.id ?? -1, why: whyNotCarrierStrike(g, id, r.id),
    });
  }
  return out.sort((a, b) => Number(b.atWar) - Number(a.atWar) || Number(b.launch >= 0) - Number(a.launch >= 0) || a.name.localeCompare(b.name));
}

const FLEETS_COUNTED = ['no', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten'];
/**
 * The fleets at anchor as the striker knows them: each identified fleet by name, and those only reported once a nation,
 * counted ("American ships reported", "two American fleets reported"), never the same words twice (the playtest's #49).
 */
function anchorNames(g: Game, there: Fleet[], viewer: string): string[] {
  const named: string[] = [], seen = new Map<string, number>();
  for (const x of there) {
    if (fleetKnown(g, x, viewer) === 'exact') named.push(fleetName(x));
    else seen.set(x.nat, (seen.get(x.nat) ?? 0) + 1);
  }
  for (const [nat, n] of seen) {
    const adj = g.nations[nat]?.adj ?? nat;
    named.push(n === 1 ? `${adj} ships reported` : `${FLEETS_COUNTED[n] ?? n} ${adj} fleets reported`);
  }
  return named;
}

/** Why fleet `id` cannot be sent to strike harbour `region` from the air now, or null if it can. Read-only. */
export function whyNotCarrierStrike(g: Game, id: number, region: number): string | null {
  if (!carrierStrikeOn(g)) return 'Carrier strikes on harbours are given from a fleet\'s page only where that is switched on.';
  if (!carrierRules(g)) return 'Harbours are raided only in the grand campaign.';
  const c = ownFleet(g, id);
  if (!c) return 'Only your own fleet can be given the order, in your own month.';
  if (c.ships.cv <= 0) return `${cap(fleetName(c))} has no carriers.`;
  // (A fleet in passage to a new base does nothing on its way: release 2, the far chart.)
  if (c.stance === 'passage') return passageWhy(g, c);
  const lead = leadOf(g, c);
  if (!lead) return `${cap(fleetName(c))} has no strike aircraft aboard.`;
  if (lead.flew === g.turn) return 'Its strike aircraft have flown this month.';
  if (c.attacked === g.turn) return `${cap(fleetName(c))} has been sent at the enemy this month and stays where the battle left it.`;
  const r = g.regions[region];
  if (!r || !r.port) return 'Pick a harbour.';
  if (!atWar(g, c.nat, r.ctrl)) return `${theName(g.nations[c.nat], true)} is at peace with ${g.nations[r.ctrl] ? theName(g.nations[r.ctrl]) : r.ctrl}: declare war first. A strike flown in the month war is declared goes in without warning.`;
  if (!atAnchor(g, r, c.nat, c.nat).length) return `No enemy warships are known to lie at ${r.name}.`;
  const z = launchSea(g, c, r);
  if (z < 0) {
    const b = forwardBase(g, c, r);
    return b
      ? `${r.name} is beyond its aircraft from every sea it reaches from ${c.port}. Move its base to ${b.name} and they reach it.`
      : `${r.name} is beyond its aircraft from every sea it reaches.`;
  }
  // The raid's own tests, from the sea it would strike from: the sky over both ends, a wrecked field, the ships there.
  return placed(c, { zone: z }, () => whyNotMission(g, lead, 'harbour', r.id));
}

/** What the order did: the sea the fleet sails to, and the wings sent in. */
export interface CarrierStrikeResult { zone: number; wings: number[] }

/**
 * Sends fleet `id` to strike harbour `region` from the air: it sails to the nearest sea in its reach from which its
 * strike aircraft reach the harbour, and its wings (the fighters too, where they reach) are sent at the harbour. They
 * fly with the month's air war. Null where it cannot be given (whyNotCarrierStrike).
 */
export function orderCarrierStrike(g: Game, id: number, region: number): CarrierStrikeResult | null {
  if (whyNotCarrierStrike(g, id, region)) return null;
  const c = ownFleet(g, id)!, r = g.regions[region];
  const z = launchSea(g, c, r);
  if (!(atSea(c) && c.zone === z) && !sail(g, c, z, 'patrol')) return null;
  const wings: number[] = [];
  const lead = leadOf(g, c)!;
  if (!setMission(g, lead, 'harbour', r.id)) return null;
  wings.push(lead.id);
  const ftr = fightersOf(g, c);
  if (ftr && ftr.flew !== g.turn && setMission(g, ftr, 'harbour', r.id)) wings.push(ftr.id);
  return { zone: z, wings };
}

/** The ships at anchor in a line, for the page: "9 battleships, 3 carriers and 15 cruisers". */
export const anchorLine = (h: StrikeHarbour): string => (h.ships && shipCount(h.ships) > 0 ? shipList(h.ships) : '');

const cap = (s: string): string => s[0].toUpperCase() + s.slice(1);
