// From a raid planned on the raid card to an air battle's ticket.
//
// The ticket is made from the very assessment the raid card shows (planRaid, assessAir, forecastAir): the groups
// that fly, the fighters that rise to meet them, how far each has come, the guns round the target and the radar.
// Nothing in the campaign is changed here; the raid is flown, and paid for, only when the battle hands its result
// back through launchRaid (see campaign/airfought.ts).

import { AIR, AIRWAR, airWar, assessAir, bombLaw, carrierOf, forecastAir, groupAt } from '../../campaign/air';
import { planRaid, type RaidKind } from '../../campaign/airwar';
import { seatsOf } from '../../campaign/bombers';
import { orderRaid } from '../../campaign/turn';
import type { FoughtAir } from '../../campaign/airfought';
import type { AirFleet, AirMission, AirReport, BomberSeats, Game } from '../../campaign/types';
import { hashString } from '../../core/rng';
import { turnToDate } from '../../core/ticket';
import type { FieldResult, FieldTicket, ForceTicket, SideTicket } from '../core/types';
import { SKY, type Band } from './book';
import { bomberOf, designYear, fighterOf } from './doctrine';
import { bomberGuns, fighterGuns } from './guns';
import { groundOf } from './ground';
import { cloudOf } from './sky';
import { airStyles } from './styles';
import type { AirOutcome, AirPlace } from './types';

/**
 * The raids that can be fought out in the first slice (ruling of 2026-11-17 17:51Z): the player's day raids and fights
 * for the sky. A night raid is not (the design director, b384(c)): its bombers' only choices on the field were their
 * height and whether to jettison, so the staff settles it, as it settles any raid. The sim keeps what it knows of the
 * night for the day it has choices worth giving the player.
 */
export const FOUGHT_KINDS: RaidKind[] = ['bomb', 'superiority'];

const clampTo = (v: number, lo: number, hi: number): number => Math.max(lo, Math.min(hi, v));

export interface AirTicketResult {
  ticket: FieldTicket<AirPlace> | null;
  why: string | null;
}

/**
 * The ticket for fighting out a raid: the same groups against the same target as `planRaid` would fly. `ids` as the
 * raid card passes them (absent: the staff's proposal). Returns why not when the raid cannot be fought.
 */
export function airTicket(g: Game, kind: RaidKind, target: number, ids?: number[], nat: string | null = g.human): AirTicketResult {
  if (!FOUGHT_KINDS.includes(kind)) return { ticket: null, why: 'Only day raids and fights for the sky can be fought out so far: the staff settles a night raid.' };
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
  const defNat = a.def[0]?.f.nat ?? g.regions[target].ctrl;

  // The board is turned so that the raid comes on from the south edge; each defending airfield keeps its true
  // bearing and distance from the target on it.
  const T = g.regions[target];
  const vec = (f: AirFleet): [number, number] => { const p = groupAt(g, f); return [(p.x - T.x) * AIR.kmPerUnit * 1000, -(p.y - T.y) * AIR.kmPerUnit * 1000]; };
  let ax = 0, ay = 0;
  for (const x of a.att) { const [vx, vy] = vec(x.f); const d = Math.hypot(vx, vy) || 1; ax += vx / d; ay += vy / d; }
  // Rotate so (ax, ay), the way the raid comes from, points south (0, -1).
  const from = Math.atan2(ay, ax) || -Math.PI / 2, turn = -Math.PI / 2 - from;
  const rot = ([x, y]: [number, number]): [number, number] => [x * Math.cos(turn) - y * Math.sin(turn), x * Math.sin(turn) + y * Math.cos(turn)];
  const onBoard = (x: number, y: number, edge = 2_000): [number, number] => [clampTo(x, edge, SKY.w - edge), clampTo(y, edge, SKY.h - edge)];

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
  for (const x of a.def) {
    const n = Math.round(x.f.ftr * x.w);
    if (n <= 0) continue;
    // The campaign has already weighed how far each field is (cover) and whether radar warned it: on the board every
    // defending field stands within reach of the raid's path, at its true bearing, nearer the farther it really is.
    const [rx, ry] = rot(vec(x.f));
    const real = Math.hypot(rx, ry), d = SKY.fields.near + Math.min(SKY.fields.far - SKY.fields.near, real * SKY.fields.squeeze);
    const ux = real > 0 ? rx / real : 0, uy = real > 0 ? ry / real : 1;
    const [bx, by] = onBoard(SKY.target.x + ux * d, SKY.target.y + uy * d);
    starts[x.f.id] = { x: bx, y: by, band: (a.radar ? 1 : 0) as Band, fuel: SKY.fuel.home, wait: !a.radar };
    def.push(force(x.f, 'ftr', n, share(n * (a.D > 0 ? a.Dpow / a.D : 1), Dtot)));
  }

  const side = (n: string, forces: ForceTicket[], human: boolean): SideTicket => ({
    nat: n, adj: g.nations[n]?.adj ?? n, color: g.nations[n]?.color ?? '#888888', human, forces,
    general: null,
  });
  const when = { year: turnToDate(g.turn).year, month: turnToDate(g.turn).month };
  const models = Object.fromEntries([...new Set([nat, defNat, ...att.map((f) => f.nat), ...def.map((f) => f.nat)])].map((n) => [n, { ftr: fighterOf(n, designYear(g, n, 'fighters')), bmr: bomberOf(n, designYear(g, n, 'bombers')) }]));
  const seed = hashString(`airbattle:${g.seed}:${g.turn}:${kind}:${target}:${nat}:${(g.flags.airBattles ?? 0) + 1}`);
  // How the computer flies its side (styles.ts; the tactics panel's item 5, ruling 239): a computer defence's style, and
  // at stops 4 and 5 a computer raid's escort roles. Read from the game, never written to it.
  const styled = airStyles(g, { nat, forces: att }, { nat: defNat, forces: def }, { escort: a.Epow, defence: a.Dpow });
  // The raid's bombers one by one (campaign/bombers.ts): what each is called and its record, for the 3D game to paint
  // and for the battle to say which of them falls. A copy of each roster: the game is not written to.
  const bombers: Record<number, BomberSeats> = {};
  for (const f of att) {
    if (f.kind !== 'bmr') continue;
    const seats = seatsOf(g, a.att.find((x) => x.f.id === f.corps)!.f, f.men, seed);
    if (seats) bombers[f.corps] = seats;
  }
  const place: AirPlace = {
    kind, mission: m, target, name: a.place, night, nat, radar: !!a.radar,
    I: C.intensity[m as 'bomb' | 'night' | 'superiority'] ?? 1,
    clash: airWar(g) && m !== 'superiority' ? AIRWAR.losses.clash : 1,
    law: { bombers: L.bombers, power: L.power, most, escort, defence: a.defence },
    fighters: C.fighters, escortShare: C.escortShare, bomberDefence: C.bomberDefence,
    gunLoss: a.gunLoss, flakGuns: { light: a.guns.light, heavy: a.guns.heavy },
    worth: { att: a.E > 0 ? a.Epow / a.E : 1, def: a.D > 0 ? a.Dpow / a.D : 1 },
    forecast: { foe: fc.foeLoss, foeGuns: fc.foeGuns, ftr: fc.ftrLoss, bmr: fc.bmrLoss, through: a.B * (1 - a.bomberLoss), bomberLoss: a.bomberLoss },
    chance: fc.success,
    reckon: { k: AIRWAR.success.k, value: fc.worth - 300 * ((fc.rail ?? 0) + (fc.ruin ?? 0)), rail: fc.rail ?? 0, ruin: fc.ruin ?? 0 },
    bombs: bombLaw(g, a, m, target),
    starts,
    from: fromOf(att.map((f) => !!carrierOf(g, a.att.find((x) => x.f.id === f.corps)!.f))),
    saved: { att: AIR.pilots.away, def: AIR.pilots.home },
    // When it is fought, the aircraft each side's groups are, as the 3D game draws them (doctrine.ts), and their guns
    // that month (guns.ts).
    when,
    models,
    guns: Object.fromEntries(Object.entries(models).map(([n, k]) => [n, { ftr: fighterGuns(k.ftr, n, when), bmr: bomberGuns(k.bmr, n, when) }])),
    // The country round the target, turned as the board is (b384(j)).
    ground: groundOf(g, target, (x, y) => { const [rx, ry] = rot([(x - T.x) * AIR.kmPerUnit * 1000, -(y - T.y) * AIR.kmPerUnit * 1000]); return [SKY.target.x + rx, SKY.target.y + ry]; }),
    // The sky that day over the target (sky.ts): drawn from the battle's own seed, so the same battle has the same sky.
    cloud: cloudOf(T.x, T.y, when, seed),
    ...styled,
    ...(Object.keys(bombers).length ? { bombers } : {}),
  };
  const date = turnToDate(g.turn).label;
  const ticket: FieldTicket<AirPlace> = {
    domain: 'air', kind: 'field',
    seed,
    turn: g.turn, date,
    title: m === 'superiority' ? `The fight for the sky over ${a.place}` : `The air battle over ${a.place}`,
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
  const seats = plan.seats.filter((x) => x.ship < 0 && !x.why);
  const land = new Set(seats.map((x) => x.id));
  let picked = plan.picked.filter((id) => land.has(id));
  if (!picked.length) {
    const lead = seats.filter((x) => x.kind !== 'ftr').sort((a, b) => a.km - b.km)[0];
    if (lead) picked = planRaid(g, kind, target, seats.filter((x) => x.kind !== 'ftr' && x.base === lead.base).map((x) => x.id), nat).picked;
  }
  if (kind === 'night') return picked;
  return [...picked, ...seats.filter((x) => x.kind === 'ftr' && !picked.includes(x.id)).map((x) => x.id)];
}

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
  for (const gr of o.groups) {
    if (gr.fell) fell[gr.id] = [...(fell[gr.id] ?? []), ...gr.fell];
    if (gr.shot) shot[gr.id] = [...(shot[gr.id] ?? []), ...gr.shot];
  }
  return {
    lost, dropped: o.dropped, jettisoned: o.jettisoned, lostBefore: o.lostBefore, lostAfter: o.lostAfter, flew, chance: o.chance,
    ...(o.gaveUp ? { gaveUp: o.gaveUp } : {}), ...(o.staff ? { staff: o.staff } : {}), ...(o.rolls ? { rolls: o.rolls } : {}),
    ...(Object.keys(fell).length ? { fell, shot } : {}),
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
