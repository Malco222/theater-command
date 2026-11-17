// From a raid planned on the raid card to an air battle's ticket.
//
// The ticket is made from the very assessment the raid card shows (planRaid, assessAir, forecastAir): the groups
// that fly, the fighters that rise to meet them, how far each has come, the guns round the target and the radar.
// Nothing in the campaign is changed here; the raid is flown, and paid for, only when the battle hands its result
// back through launchRaid (see campaign/airfought.ts).

import { AIR, AIRWAR, airWar, assessAir, forecastAir, groupAt } from '../../campaign/air';
import { planRaid, type RaidKind } from '../../campaign/airwar';
import { orderRaid } from '../../campaign/turn';
import type { FoughtAir } from '../../campaign/airfought';
import type { AirFleet, AirMission, AirReport, Game } from '../../campaign/types';
import { hashString } from '../../core/rng';
import { turnToDate } from '../../core/ticket';
import type { FieldResult, FieldTicket, ForceTicket, SideTicket } from '../core/types';
import { SKY, type Band } from './book';
import type { AirOutcome, AirPlace } from './types';

/** The raids that can be fought out in the first slice (ruling of 2026-11-17 17:51Z): the player's day raids and fights for the sky, and night raids. */
export const FOUGHT_KINDS: RaidKind[] = ['bomb', 'night', 'superiority'];

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
  if (!FOUGHT_KINDS.includes(kind)) return { ticket: null, why: 'Only bombing raids and fights for the sky can be fought out so far.' };
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
  const most = airWar(g) ? AIRWAR.losses.most : C.maxBomberLoss;
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
  const place: AirPlace = {
    kind, mission: m, target, name: a.place, night, nat, radar: !!a.radar,
    I: C.intensity[m as 'bomb' | 'night' | 'superiority'] ?? 1,
    clash: airWar(g) && m !== 'superiority' ? AIRWAR.losses.clash : 1,
    law: { bombers: L.bombers, power: L.power, most },
    fighters: C.fighters, escortShare: C.escortShare, bomberDefence: C.bomberDefence,
    gunLoss: a.gunLoss,
    worth: { att: a.E > 0 ? a.Epow / a.E : 1, def: a.D > 0 ? a.Dpow / a.D : 1 },
    forecast: { foe: fc.foeLoss, ftr: fc.ftrLoss, bmr: fc.bmrLoss, through: a.B * (1 - a.bomberLoss), bomberLoss: a.bomberLoss },
    chance: fc.success,
    starts,
    saved: { att: AIR.pilots.away, def: AIR.pilots.home },
  };
  const date = turnToDate(g.turn).label;
  const ticket: FieldTicket<AirPlace> = {
    domain: 'air', kind: 'field',
    seed: hashString(`airbattle:${g.seed}:${g.turn}:${kind}:${target}:${nat}:${(g.flags.airBattles ?? 0) + 1}`),
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
  return { lost, dropped: o.dropped, jettisoned: o.jettisoned, flew, chance: o.chance, ...(o.staff ? { staff: o.staff } : {}) };
}

/**
 * Hands a fought raid back: the rules' own raid order (orderRaid), paid with what the battle reports, held inside the
 * dice's run (campaign/airfought.ts), whether it was fought to the end or handed to the staff part way.
 */
export function airHandBack(g: Game, kind: RaidKind, target: number, ids: number[], result: FieldResult): AirReport | null {
  const out = result.extra?.air as AirOutcome | undefined;
  return orderRaid(g, kind, target, ids, out ? foughtAir(out) : undefined);
}
