// From a raid planned on the raid card to an air battle's ticket.
//
// The ticket is made from the very assessment the raid card shows (planRaid, assessAir, forecastAir): the groups
// that fly, the fighters that rise to meet them, how far each has come, the guns round the target and the radar.
// Nothing in the campaign is changed here; the raid is flown, and paid for, only when the battle hands its result
// back through launchRaid (see campaign/airfought.ts).

import { AIR, AIRWAR, airWar, assessAir, bombLaw, carrierOf, flakGuns, forecastAir, groupAt } from '../../campaign/air';
import { planRaid, type RaidKind } from '../../campaign/airwar';
import { placeName } from '../../campaign/game';
import { orderRaid } from '../../campaign/turn';
import { weatherOn } from '../../campaign/weather';
import type { FoughtAir } from '../../campaign/airfought';
import type { AirFleet, AirMission, AirReport, BomberSeats, Game } from '../../campaign/types';
import { hashString } from '../../core/rng';
import { turnToDate } from '../../core/ticket';
import type { FieldResult, FieldTicket, ForceTicket, SideTicket } from '../core/types';
import { SKY, type Band } from './book';
import { bomberOf, designYear, fighterOf } from './doctrine';
import { bomberGuns, fighterGuns } from './guns';
import { groundOf, type AirGround } from './ground';
import { shipPlace } from './ships';
import { cloudOf } from './sky';
import { onBoard, STAGES } from './stages';
import { airStyles, STYLES } from './styles';
import type { AirOutcome, AirPlace } from './types';

/**
 * The raids that can be fought out in the first slice (ruling of 2026-11-17 17:51Z): the player's day raids and fights
 * for the sky. A night raid is not (the design director, b384(c)): its bombers' only choices on the field were their
 * height and whether to jettison, so the staff settles it, as it settles any raid. The sim keeps what it knows of the
 * night for the day it has choices worth giving the player.
 */
export const FOUGHT_KINDS: RaidKind[] = ['bomb', 'superiority'];

/**
 * Strikes on ships (ruling 238): a raid on a harbour and a strike on a fleet at sea, fought out with the ships under
 * attack on the field. They are ticketed only when asked for ({ ships: true }), and today only the air recorder
 * (tools/air-record: the set pieces the 3D game replays) and the tests ask: the browser's air page and the battle
 * bridge's airLook and airStart do not, so no played raid is fought out on ships. Once the playtester has passed them
 * they join FOUGHT_KINDS and the raid card offers them (ruling 217).
 */
export const SHIP_KINDS: RaidKind[] = ['harbour', 'fleet'];

const clampTo = (v: number, lo: number, hi: number): number => Math.max(lo, Math.min(hi, v));

export interface AirTicketResult {
  ticket: FieldTicket<AirPlace> | null;
  why: string | null;
}

/**
 * The ticket for fighting out a raid: the same groups against the same target as `planRaid` would fly. `ids` as the
 * raid card passes them (absent: the staff's proposal). Returns why not when the raid cannot be fought.
 */
export function airTicket(g: Game, kind: RaidKind, target: number, ids?: number[], nat: string | null = g.human, opts: { ships?: boolean } = {}): AirTicketResult {
  if (!FOUGHT_KINDS.includes(kind) && !(opts.ships && SHIP_KINDS.includes(kind))) return { ticket: null, why: 'Only day raids and fights for the sky can be fought out so far: the staff settles a night raid.' };
  if (nat === null) return { ticket: null, why: 'Nobody is at the controls.' };
  const plan = planRaid(g, kind, target, ids, nat);
  if (plan.why) return { ticket: null, why: plan.why };
  const fleets = plan.picked.map((id) => g.airFleets.find((f) => f.id === id)!).filter(Boolean);
  const m = kind as AirMission;
  const a = assessAir(g, fleets, m, target);
  if (a.why) return { ticket: null, why: a.why };
  const fc = forecastAir(g, a, m, target);
  const C = AIR.combat;
  const night = m === 'night';
  const L = airWar(g) ? (night ? AIRWAR.losses.night : AIRWAR.losses) : { bombers: C.bombers, power: 1 };
  // The bomber-loss law as the raid card reads it (assessAir, ruling 227): the largest share one raid can lose (a day
  // raid of the air war up to the record's worst, AIRWAR.losses.day; a night raid no more than AIRWAR.losses.most),
  // the escort counted AIRWAR.losses.escort times over by day in the air war, and the bombers' own defence of the month.
  const most = airWar(g) ? (night ? AIRWAR.losses.most : AIRWAR.losses.day) : C.maxBomberLoss;
  const escort = airWar(g) && !night ? AIRWAR.losses.escort : 1;
  // A strike on a fleet is fought over the sea it is in; a raid on a harbour over the port.
  const quarry = kind === 'fleet' ? a.ships[0] : null;
  const defNat = a.def[0]?.f.nat ?? quarry?.nat ?? g.regions[target].ctrl;

  // The board is turned so that the raid comes on from the south edge; each defending airfield keeps its true
  // bearing and distance from the target on it.
  const T: { x: number; y: number } = quarry ? g.seas[quarry.zone] : g.regions[target];
  const vec = (f: AirFleet): [number, number] => { const p = groupAt(g, f); return [(p.x - T.x) * AIR.kmPerUnit * 1000, -(p.y - T.y) * AIR.kmPerUnit * 1000]; };
  let ax = 0, ay = 0;
  for (const x of a.att) { const [vx, vy] = vec(x.f); const d = Math.hypot(vx, vy) || 1; ax += vx / d; ay += vy / d; }
  // Rotate so (ax, ay), the way the raid comes from, points south (0, -1).
  const from = Math.atan2(ay, ax) || -Math.PI / 2, turn = -Math.PI / 2 - from;
  const rot = ([x, y]: [number, number]): [number, number] => [x * Math.cos(turn) - y * Math.sin(turn), x * Math.sin(turn) + y * Math.cos(turn)];
  const clampBoard = (x: number, y: number, edge = 2_000): [number, number] => [clampTo(x, edge, SKY.w - edge), clampTo(y, edge, SKY.h - edge)];

  const when = { year: turnToDate(g.turn).year, month: turnToDate(g.turn).month };
  // A strike on ships: the campaign's law, the ships as pieces, and the stage by place (ships.ts, stages.ts).
  const ships = SHIP_KINDS.includes(kind) ? shipPlace(g, a, m, target, nat, when, SKY.target, turn) : null;
  const stage = ships?.stage ? STAGES[ships.stage.key] : null;
  let field = 0;
  const starts: AirPlace['starts'] = {};
  const att: ForceTicket[] = [], def: ForceTicket[] = [];
  const share = (pow: number, all: number): number => (all > 0 ? pow / all : 0);
  const Atot = a.Epow + a.B * C.bomberDefence, Dtot = a.Dpow;
  a.att.forEach((x) => {
    const [rx] = rot(vec(x.f));
    const sx = clampTo(SKY.target.x + rx * 0.25, 20_000, 80_000);
    const ftr = Math.round(x.f.ftr * x.crowd), bmr = Math.round(x.bmr);
    // Fighters that cannot reach the target (or, at night, do not go) stay out of it, as they count for nothing in the forecast.
    if (x.esc > 0 && ftr > 0) {
      starts[x.f.id] = { x: sx, y: 1_000, band: 1, fuel: SKY.fuel.full * x.esc, wait: false };
      att.push(force(x.f, 'ftr', ftr, share(x.f.ftr * x.esc * x.crowd * (a.E > 0 ? a.Epow / a.E : 1), Atot)));
    }
    if (bmr > 0) {
      starts[x.f.id] = starts[x.f.id] ?? { x: sx, y: 1_000, band: 1, fuel: SKY.fuel.home, wait: false };
      att.push(force(x.f, 'bmr', bmr, share(bmr * C.bomberDefence, Atot)));
    }
  });
  // Where the raid comes on from, for a fleet's own cover: the bearing from the target to the raid's start.
  const raidAt = Object.values(starts), rax = raidAt.reduce((s, p) => s + p.x, 0) / Math.max(1, raidAt.length) - SKY.target.x;
  const ray = raidAt.reduce((s, p) => s + p.y, 0) / Math.max(1, raidAt.length) - SKY.target.y, rad = Math.hypot(rax, ray) || 1;
  for (const x of a.def) {
    const n = Math.round(x.f.ftr * x.w);
    if (n <= 0) continue;
    // A fleet's own fighters over it (the card's lookout case, campaign distance 0) stand in the air over the fleet as a
    // strike comes on, a little way out toward it, warned by the lookouts (the tactics panel's S1). A harbour raid and
    // land fighters are as before: Pearl Harbor in the month war comes is still caught on the ground (238(1)).
    if (kind === 'fleet' && x.f.ship !== undefined && x.d <= 0 && raidAt.length) {
      const ahead = STYLES.cap.ahead;
      starts[x.f.id] = { x: SKY.target.x + (rax / rad) * ahead, y: SKY.target.y + (ray / rad) * ahead, band: 1, fuel: SKY.fuel.home, wait: false, over: true };
      def.push(force(x.f, 'ftr', n, share(n * (a.D > 0 ? a.Dpow / a.D : 1), Dtot)));
      continue;
    }
    // The campaign has already weighed how far each field is (cover) and whether radar warned it: on the board every
    // defending field stands within reach of the raid's path, at its true bearing, nearer the farther it really is.
    // On a stage, the island's own fighters rise from its fighter fields in turn, at their true bearing.
    const home = stage && x.f.ship === undefined && stage.places.includes(g.regions[x.f.base]?.name ?? '') ? stage.fields.filter((f) => f.kind === 'fighters') : [];
    const [rx, ry] = home.length ? ((p) => [p[0] - SKY.target.x, p[1] - SKY.target.y])(onBoard(ships!.stage!, home[field++ % home.length].at, SKY.target)) : rot(vec(x.f));
    const real = Math.hypot(rx, ry), d = SKY.fields.near + Math.min(SKY.fields.far - SKY.fields.near, real * SKY.fields.squeeze);
    const ux = real > 0 ? rx / real : 0, uy = real > 0 ? ry / real : 1;
    const [bx, by] = clampBoard(SKY.target.x + ux * d, SKY.target.y + uy * d);
    starts[x.f.id] = { x: bx, y: by, band: (a.radar ? 1 : 0) as Band, fuel: SKY.fuel.home, wait: !a.radar };
    def.push(force(x.f, 'ftr', n, share(n * (a.D > 0 ? a.Dpow / a.D : 1), Dtot)));
  }

  const side = (n: string, forces: ForceTicket[], human: boolean): SideTicket => ({
    nat: n, adj: g.nations[n]?.adj ?? n, color: g.nations[n]?.color ?? '#888888', human, forces,
    general: null,
  });
  const models = Object.fromEntries([...new Set([nat, defNat, ...att.map((f) => f.nat), ...def.map((f) => f.nat)])].map((n) => [n, { ftr: fighterOf(n, designYear(g, n, 'fighters')), bmr: bomberOf(n, designYear(g, n, 'bombers')) }]));
  const seed = hashString(`airbattle:${g.seed}:${g.turn}:${kind}:${target}:${nat}:${(g.flags.airBattles ?? 0) + 1}`);
  // How the computer flies its side (styles.ts; the tactics panel's item 5, ruling 239): a computer defence's style, and
  // at stops 4 and 5 a computer raid's escort roles. Read from the game, never written to it.
  const styled = airStyles(g, { nat, forces: att }, { nat: defNat, forces: def }, { escort: a.Epow, defence: a.Dpow });
  // The raid's bombers one by one (release 2's bombers.ts) are not in release 1.6: no bomber is seated by name, so the
  // battle names none and hands none back.
  const bombers: Record<number, BomberSeats> = {};
  // The guns round a target on land as a count, for the picture only (release 2 keeps it on the assessment).
  const guns = m !== 'strike' && m !== 'fleet' && a.B > 0 ? flakGuns(a.aa) : { light: 0, heavy: 0 };
  const place: AirPlace = {
    kind, mission: m, target, name: a.place, night, nat, radar: !!a.radar,
    I: C.intensity[m as 'bomb' | 'night' | 'superiority'] ?? 1,
    clash: airWar(g) && m !== 'superiority' ? AIRWAR.losses.clash : 1,
    law: { bombers: L.bombers, power: L.power, most, escort, guns: a.defence },
    fighters: C.fighters, escortShare: C.escortShare, bomberDefence: C.bomberDefence,
    gunLoss: a.gunLoss, flakGuns: { light: guns.light, heavy: guns.heavy },
    worth: { att: a.E > 0 ? a.Epow / a.E : 1, def: a.D > 0 ? a.Dpow / a.D : 1 },
    forecast: { foe: fc.foeLoss, foeGuns: fc.foeGuns, ftr: fc.ftrLoss, bmr: fc.bmrLoss, through: a.B * (1 - a.bomberLoss), bomberLoss: a.bomberLoss },
    chance: fc.success,
    reckon: { k: AIRWAR.success.k, value: fc.worth - 300 * ((fc.rail ?? 0) + (fc.ruin ?? 0)), rail: fc.rail ?? 0, ruin: fc.ruin ?? 0 },
    bombs: bombLaw(g, a, m, target),
    starts,
    from: fromOf(att.map((f) => !!carrierOf(g, a.att.find((x) => x.f.id === f.corps)!.f))),
    saved: { att: AIR.pilots.away, def: AIR.pilots.home },
    ...(ships ? { ships } : {}),
    // When it is fought, the aircraft each side's groups are, as the 3D game draws them (doctrine.ts), and their guns
    // that month (guns.ts).
    when,
    models,
    guns: Object.fromEntries(Object.entries(models).map(([n, k]) => [n, { ftr: fighterGuns(k.ftr, n, when), bmr: bomberGuns(k.bmr, n, when) }])),
    // The country round the target, turned as the board is (b384(j)).
    // Open water has no ground to lay, but it keeps its true north (the sun, the wind and a wake's heading can use it).
    ground: quarry ? seaGround(turn) : groundOf(g, target, (x, y) => { const [rx, ry] = rot([(x - T.x) * AIR.kmPerUnit * 1000, -(y - T.y) * AIR.kmPerUnit * 1000]); return [SKY.target.x + rx, SKY.target.y + ry]; }),
    // The sky that day over the target (sky.ts): drawn from the battle's own seed, so the same battle has the same sky;
    // where the campaign keeps weather, under the sky its card priced (a.sky, ruling 127).
    cloud: cloudOf(T.x, T.y, when, seed, weatherOn(g) ? a.sky : undefined),
    ...styled,
    ...(Object.keys(bombers).length ? { bombers } : {}),
  };
  const date = turnToDate(g.turn).label;
  const ticket: FieldTicket<AirPlace> = {
    domain: 'air', kind: 'field',
    seed,
    turn: g.turn, date,
    title: m === 'superiority' ? `The fight for the sky over ${a.place}` : m === 'harbour' ? `The raid on ${harbourOf(a.place)}` : quarry ? `The strike on the ${quarry.name}` : `The air battle over ${a.place}`,
    place,
    att: side(nat, att, g.human === nat),
    def: side(defNat, def, g.human === defNat),
    reckoning: { A: Atot, D: Dtot, odds: fc.odds, dice: [1 - C.luck, 1 + C.luck], even: 1 },
    lines: a.lines,
    limit: SKY.limit,
  };
  return { ticket, why: null };
}

/**
 * The groups a fought raid sends, as the raid card picks them: the staff's pick and every fighter group that can go with
 * it by day, all from the nation's own airfields (the design director, b397: Bremen is flown by the RAF, not by the Home
 * Fleet's carriers). Where the staff would send only carrier groups, the bombers of the nearest airfield go in their
 * place, as the staff picks a raid's bombers. A night raid takes the staff's pick alone.
 */
export function raidGroups(g: Game, kind: RaidKind, target: number, nat: string): number[] {
  const plan = planRaid(g, kind, target, undefined, nat);
  // A strike on ships takes the carriers' own groups too: they are what carriers are for.
  const seats = plan.seats.filter((x) => (x.ship < 0 || SHIP_KINDS.includes(kind)) && !x.why);
  const land = new Set(seats.map((x) => x.id));
  let picked = plan.picked.filter((id) => land.has(id));
  if (!picked.length) {
    const lead = seats.filter((x) => x.kind !== 'ftr').sort((a, b) => a.km - b.km)[0];
    if (lead) picked = planRaid(g, kind, target, seats.filter((x) => x.kind !== 'ftr' && x.base === lead.base).map((x) => x.id), nat).picked;
  }
  if (kind === 'night') return picked;
  return [...picked, ...seats.filter((x) => x.kind === 'ftr' && !picked.includes(x.id)).map((x) => x.id)];
}

/** One raid's card twice from one reckoning: flown alone, and escorted; and what the escort is worth between them. */
export interface EscortPair {
  /** The bombers raidGroups sends, and no fighter group. */
  alone: { ids: number[]; ticket: FieldTicket<AirPlace> };
  /** The same bombers with every fighter group that can go with them by day (raidGroups). */
  escorted: { ids: number[]; ticket: FieldTicket<AirPlace> };
  /**
   * The escort's worth, as the difference between the two cards and nothing of its own: the staff's chance it adds,
   * and the bombers the card expects lost that it saves.
   */
  worth: { chance: number; bmr: number };
}

/**
 * The same day raid flown alone and escorted, both cards from one reckoning (the design director, b401 as re-ruled at
 * 08:48Z and 08:50Z on 22 November): one game, one month, one target, the same bombers, the card worked out by airTicket
 * for each, and the escort's worth as the difference between the two. Null where either cannot be flown, or where no
 * fighter can go with the bombers, so that there is no choice to show.
 */
export function escortPair(g: Game, kind: RaidKind, target: number, nat: string): EscortPair | null {
  if (kind === 'night' || kind === 'superiority') return null;
  const ids = raidGroups(g, kind, target, nat);
  const bombers = ids.filter((id) => (g.airFleets.find((f) => f.id === id)?.bmr ?? 0) > 0);
  if (!bombers.length || bombers.length === ids.length) return null;
  const alone = airTicket(g, kind, target, bombers, nat).ticket;
  const escorted = airTicket(g, kind, target, ids, nat).ticket;
  if (!alone || !escorted || !escorted.att.forces.some((f) => f.kind !== 'bmr')) return null;
  return {
    alone: { ids: bombers, ticket: alone },
    escorted: { ids, ticket: escorted },
    worth: { chance: escorted.place.chance - alone.place.chance, bmr: alone.place.forecast.bmr - escorted.place.forecast.bmr },
  };
}

/**
 * A harbour raid's place in words (the UI director, 03:43Z): where the place's name already says harbour it stands
 * alone ("The raid on Pearl Harbor"); elsewhere "the harbour of Taranto", the place with its article where the map's
 * list gives it one: "the harbour of the Highlands" (the UI director's final read, 25 November 2026, 15:42Z).
 */
export const harbourOf = (place: string): string => (/\bHarbou?r\b/.test(place) ? place : `the harbour of ${placeName(place)}`);

/** The ground of a strike at sea: open water across the board (a stage draws its own). */
const OPEN_SEA: AirGround = { land: [], rivers: [], borders: [], target: [], terrain: 'sea', inland: false, north: [0, 1] };

/**
 * Open water on a board turned by `turn` (radians, counter-clockwise, the ticket's and a stage's own): no ground to lay,
 * but its true north, the map's north turned as the board is ([0, 1] unturned; [0, -1] turned half round, as Pearl
 * Harbor's stage is), to the millionth. The ticket's strike at sea and the recorder's set pieces (tools/air-record) both
 * lay it.
 */
export const seaGround = (turn: number): AirGround => ({ ...OPEN_SEA, north: [Math.round(-Math.sin(turn) * 1e6) / 1e6 + 0, Math.round(Math.cos(turn) * 1e6) / 1e6 + 0] });

/** Where a raid's groups fly from, by whether each comes off a carrier. */
function fromOf(decks: boolean[]): 'land' | 'sea' | 'both' {
  return decks.length && decks.every(Boolean) ? 'sea' : decks.some(Boolean) ? 'both' : 'land';
}

function force(f: AirFleet, kind: 'ftr' | 'bmr', n: number, share: number): ForceTicket {
  return {
    corps: f.id, name: f.name, nat: f.nat, kind, divs: 0, men: n,
    str: 1, org: f.ready, xp: f.leader ? f.leader.level / 3 : 0, share,
  };
}

/** What a fought raid hands back to the rules (launchRaid's `fought`): each campaign group's losses, and the bombs dropped. */
export function foughtAir(o: AirOutcome): FoughtAir {
  const lost: FoughtAir['lost'] = {}, flew: NonNullable<FoughtAir['flew']> = {};
  for (const gr of o.groups) {
    const row = (lost[gr.id] ??= { ftr: 0, bmr: 0, back: 0 });
    row.ftr += gr.lostFtr; row.bmr += gr.lostBmr; row.back += gr.back;
    const up = (flew[gr.id] ??= { ftr: 0, bmr: 0 });
    up.ftr += gr.flewFtr; up.bmr += gr.flewBmr;
  }
  // The bombers the battle knew by name: which of them fell, and what their gunners shot down.
  const fell: NonNullable<FoughtAir['fell']> = {}, shot: NonNullable<FoughtAir['shot']> = {};
  // And every group's gunners' kills, whether its bombers were known by name or not (ruling 227).
  const guns: NonNullable<FoughtAir['guns']> = {};
  for (const gr of o.groups) {
    if (gr.fell) fell[gr.id] = [...(fell[gr.id] ?? []), ...gr.fell];
    if (gr.shot) shot[gr.id] = [...(shot[gr.id] ?? []), ...gr.shot];
    if (gr.guns) guns[gr.id] = (guns[gr.id] ?? 0) + gr.guns;
  }
  return {
    lost, dropped: o.dropped, jettisoned: o.jettisoned, lostBefore: o.lostBefore, lostAfter: o.lostAfter, flew, chance: o.chance,
    ...(o.gaveUp ? { gaveUp: o.gaveUp } : {}), ...(o.sunk ? { sunk: o.sunk } : {}), ...(o.kept ? { kept: o.kept } : {}), ...(o.staff ? { staff: o.staff } : {}), ...(o.rolls ? { rolls: o.rolls } : {}),
    ...(Object.keys(fell).length ? { fell, shot } : {}), ...(Object.keys(guns).length ? { guns } : {}),
  };
}

/**
 * Hands a fought raid back: the rules' own raid order (orderRaid), paid with what the battle reports (campaign/airfought.ts),
 * whether it was fought to the end or handed to the staff part way.
 */
export function airHandBack(g: Game, kind: RaidKind, target: number, ids: number[], result: FieldResult): AirReport | null {
  const out = result.extra?.air as AirOutcome | undefined;
  return orderRaid(g, kind, target, ids, out ? foughtAir(out) : undefined);
}
