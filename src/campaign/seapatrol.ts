// The sea patrol against submarines (release 2, Units decide; the design director's ruling of 24 November 2026 15:14Z
// on naval-battles/convoy-r2/sea-patrol-form.md: (a) on all nine questions, with his marks of 15:14Z, 15:23Z and 15:32Z).
//
// Any land bomber group of any nation can take the standing order "Patrol the [sea] against submarines", one sea a
// group, within its bombers' reach as a strike is judged (a sea with no coast only with long-range patrol aircraft, from
// a coast of a sea next to it). It then flies nothing else that month. Its aircraft watch a share of the sea:
//
//   s = R x (1 - e^(-A / A_R)),  A_R = a0 x the waters reached / 10,000 square nautical miles
//
// R the share of the sea's waters the patrols reach: the share of its squares on the chart within the group's bomber
// range of its airfield ('map', as the design director ruled for the dormant interim at 17:49Z, to be replaced at the one
// a0 refit by the aircraft type's radius on the map geometry; the sea's 'air' share, with A_R = a0 x R / air, was the
// reading first ruled at 15:14Z and stays only as the data's other setting), all of it with long-range patrol aircraft.
// A the aircraft on patrol there (each group's bombers x readiness x the sky, at the air rules' cover curve over the
// distance it flies), a0 one figure fitted through the game to the historian's arc. In watched waters a boat on station is
// attacked and sunk at the rates of the kit its attackers carry (navy.ts patrolSubs), and aircraft without the kit keep
// the boats down instead: their time on station there counts at 1 - s (navy.ts onWatch). With the kit, OEG T7's cut
// stands at the watched share (navy.ts seaCover).
//
// Every figure is data/campaign/convoycause.json 'patrol'. The patrol is dormant until the one a0 refit (the design
// director's ruling of 24 November 2026 17:49Z, 5 (a)): while its switch 'live' is false, as shipped, or a0, the attack
// rates or the kill shares are unread, nothing here is read (his mark of 15:14Z: never remove a working cause before its
// replacement is laid): no order is offered, the staffs send no group, and the flat air term and the cover from coasts
// held stand. Once live, 'intensity' gates only the enemy fighters' meeting a patrol: while it is unread none meets one
// and none of its bombers is lost to them, and the patrol itself works. Off (no Units decide) none of it exists. Nothing
// in it reads the flag or the date.
//
// Each staff counts the other side's units where its own side last saw them, never where they truly are (the design
// director's 24 November 2026 17:49Z amendment to ruling 1, and his ruling of 17:54Z on
// naval-battles/convoy-r2/sight-store-form.md, (a) on all five questions). So the patrol staff here sends its groups
// where its side last saw boats (seasight.ts seenBoats), and the boats' staff moves its beats out of the aircraft its
// side's boats last met (navy.ts quieterBeat, seenAir); neither predicts the other's move. The record is written only
// while the patrol is live (seasight.ts noteSeaSight).
import { CAUSE, NAVY, airKit, aswKit, seaLatitude, seaRoutes, seaSquares, zoneOf } from './navy';
import { AIR, AIRWAR, CARRIERS, airRange, assessAir, cover, deckBred, kmToSea, setMission, whyNotMission } from './air';
import { mapDist } from './wrap';
import { allied, atWar } from './game';
import { unitsDecide } from './units';
import { seenBoats } from './seasight';
import { dayShare, grounded, skyAtSea, skySupport, weatherOn } from './weather';
import { turnToDate } from '../core/ticket';
import type { AirFleet, Game } from './types';

type ConvoyRoute = (typeof NAVY.convoys)[number];

/** The sea patrol's figures, live and laid: a0, the attacks and kill shares by kit; the fighters' row and the operational loss where laid. */
interface Laid { a0: number; intensity: number | null; attacks: { kit: number; none: number }; kill: { kit: number; none: number }; lost: number | null }

/**
 * The sea patrol's figures where its switch is live and every figure it needs is laid (a0, the attacks and the kill
 * shares), else null (the design director's ruling of 24 November 2026 17:49Z, 5 (a)). 'intensity' is not among them:
 * it gates only the enemy fighters' meeting a patrol (air.ts comesUp).
 */
export function patrolFigures(): Laid | null {
  const P = CAUSE.patrol;
  if (!P || P.live !== true || P.a0 === null || P.attacks.kit === null || P.attacks.none === null || P.kill.kit === null || P.kill.none === null) return null;
  return { a0: P.a0, intensity: P.intensity, attacks: { kit: P.attacks.kit, none: P.attacks.none }, kill: { kit: P.kill.kit, none: P.kill.none }, lost: P.lost };
}

/** True where enemy fighters meet a patrol: the patrol live and laid, and the air fighting's own row for it ('intensity') laid too. */
export const patrolMet = (): boolean => (patrolFigures()?.intensity ?? null) !== null;

/** True where this campaign flies sea patrols against submarines: Units decide, and the patrol live and laid. */
export const patrolOn = (g: Game): boolean => unitsDecide(g) && patrolFigures() !== null;

/**
 * How a group reaches a sea on patrol: the share of its waters it can watch (R), their area in square nautical miles,
 * and how far it flies to get there, or null where it cannot patrol it. A sea with a coast is reached as a strike is
 * judged, its nearest coast within the group's bomber range. A sea with no coast is reached only with long-range patrol
 * aircraft, from a base within range of a coast of a sea next to it. Carrier wings keep their escort cover and fighters
 * their reach: neither patrols.
 * The waters reached (data 'reach'): 'map', the share of the sea's squares on the chart within the group's bomber range
 * of its airfield, the map's own measure, which the design director ruled for the dormant interim at 17:49Z, to be
 * replaced at the one a0 refit by the aircraft type's radius on the map geometry; 'air', the sea's air share, as first
 * ruled at 15:14Z (Q2 (a)) and kept only as the data's other setting. All of it with long-range patrol aircraft, either
 * way. A group that reaches none of a sea's waters (none of its squares within range, though its nearest coast is) does
 * not reach that sea: it is refused the order as beyond its bombers, and adds nothing to another group's watch there.
 */
export function patrolReach(g: Game, f: AirFleet, z: number): { R: number; d: number; nm2: number } | null {
  const zone = g.seas[z];
  if (!zone || f.ship !== undefined || f.ashore !== undefined || f.kind === 'ftr') return null;
  const range = airRange(f, g).bmr;
  const long = aswKit(g, f.nat).patrol;
  let d = Infinity;
  if (zone.coast.length) d = kmToSea(g, f, z);
  else if (long) for (const a of zone.adj) if (g.seas[a]?.coast.length) d = Math.min(d, kmToSea(g, f, a));
  if (d > range) return null;
  const sq = seaSquares(g), all = sq.at.get(z)?.length ?? 0;
  const R = long ? 1 : CAUSE.patrol.reach === 'map' ? squaresWithin(g, f.base, z, range) : zone.air;
  if (R <= 0) return null;
  return { R, d, nm2: R * all * sq.nm2 };
}

/** The share of a sea's squares within so many kilometres of a region (an airfield): kept, since the chart never changes. */
const within = new Map<string, number>();
function squaresWithin(g: Game, base: number, z: number, km: number): number {
  const key = `${g.flags.world === 1 ? 'w' : 'e'}:${base}:${z}:${km}`;
  let v = within.get(key);
  if (v !== undefined) return v;
  const at = seaSquares(g).at.get(z) ?? [];
  const r = g.regions[base], lim = km / AIR.kmPerUnit;
  let n = 0;
  for (const p of at) if (mapDist(g, p.x, p.y, r.x, r.y) <= lim) n++;
  v = at.length ? n / at.length : 0;
  within.set(key, v);
  return v;
}

/** The aircraft a group keeps on patrol over a sea this month: its bombers x its readiness x the sky (none under a closed sky or from a grounded field, half under cloud), all of them out to 60% of its range and fewer towards its limit (the air rules' cover curve). */
export function patrolAircraft(g: Game, f: AirFleet, z: number, at = patrolReach(g, f, z)): number {
  if (!at || f.bmr <= 0) return 0;
  const sky = weatherOn(g) ? (grounded(g, f) ? 0 : skySupport(skyAtSea(g, z))) : 1;
  return f.bmr * f.ready * sky * cover(at.d, airRange(f, g).bmr);
}

/**
 * The bombers a group on patrol over a sea is expected to lose in a month, and the only two losses the ruling allows
 * (Q6 (a)): 'op', the operational share 'lost' of the aircraft on patrol (patrolAircraft), where it is laid; and
 * 'fighters', what the enemy's fighters take where they reach the sea and meet the patrol, as they meet a strike there
 * (air.ts assessAir), at the patrol's own intensity row. Only the fighters' part of the strike's loss is taken: the
 * strike's guns' part is a target's flak, and an open sea has none. Nothing where no fighter comes up, and none comes
 * up while that row is unread (air.ts comesUp; the design director's ruling of 24 November 2026 17:49Z, 5 (a)).
 */
export function patrolLoss(g: Game, f: AirFleet, z: number): { op: number; fighters: number } {
  const lost = patrolFigures()?.lost ?? null;
  const op = lost !== null ? patrolAircraft(g, f, z) * lost : 0;
  const a = assessAir(g, [f], 'seapatrol', z);
  const fighters = !a.why && a.D > 0 ? a.B * Math.max(0, a.bomberLoss - a.gunLoss) : 0;
  return { op, fighters };
}

/**
 * The watch the aircraft on patrol keep over a sea, for one side: s, the share of its waters they watch by day; 'kit'
 * and 'none', the parts of a boat's time watched by aircraft with anti-submarine kit (depth charges and sea-search
 * radar) and without, s x the day's share ('day') split between them;
 * A and R; 'attacks', the attacks on a boat a month in watched waters and 'kills' the boats sunk a boat-month there,
 * each the patrols' own by their aircraft; and each nation's aircraft, for whose aircraft sank what. Null where no
 * group of the side patrols it, or the patrol is not live.
 */
export interface Watch { s: number; kit: number; none: number; day: number; A: number; R: number; attacks: number; kills: number; by: Record<string, number> }

function watch(g: Game, z: number, side: (nat: string) => boolean): Watch | null {
  const F = patrolFigures();
  if (!F || !unitsDecide(g) || !g.seas[z]) return null;
  let A = 0, Ak = 0, R = 0, nm2 = 0, attacks = 0, kills = 0;
  const by: Record<string, number> = {};
  for (const f of g.airFleets) {
    if (f.mission !== 'seapatrol' || f.target !== z || !side(f.nat)) continue;
    const at = patrolReach(g, f, z);
    const a = patrolAircraft(g, f, z, at);
    if (!at || a <= 0) continue;
    const kit = airKit(aswKit(g, f.nat));
    A += a; if (kit) Ak += a;
    if (at.R > R) { R = at.R; nm2 = at.nm2; }
    const rate = kit ? F.attacks.kit : F.attacks.none;
    attacks += a * rate; kills += a * rate * (kit ? F.kill.kit : F.kill.none);
    by[f.nat] = (by[f.nat] ?? 0) + a;
  }
  if (A <= 0 || R <= 0) return null;
  // (The aircraft needed to watch the waters reached: by their area on the chart, a0 to each 10,000 square nautical
  // miles, the interim reading the design director ruled at 17:49Z; or, as first ruled at 15:14Z, by the share reached
  // over the sea's air share, the only scale the old map had.)
  const AR = CAUSE.patrol.reach === 'map' ? (F.a0 * nm2) / 1e4 : (F.a0 * R) / g.seas[z].air;
  const s = R * (1 - Math.exp(-A / AR));
  // (A patrol watches by day only: without a searchlight, which no tree has yet, radar and eye find nothing to attack by
  // night (the design director's rulings of 24 November 2026 17:06Z and 17:10Z). So the boats are kept down, and the
  // kit's cut acts, for the day's share of their time: the sun's day over the sea's mean latitude this month. The
  // attacks are the record's per boat-month watched, which already held its nights.)
  const day = dayShare(seaLatitude(g, z), turnToDate(g.turn).month);
  return { s, kit: (s * day * Ak) / A, none: (s * day * (A - Ak)) / A, day, A, R, attacks: attacks / A, kills: kills / A, by };
}

/** The watch kept over a sea against the boats of this nation: every patrol of a nation at war with it. */
export const watchAgainst = (g: Game, z: number, boats: string): Watch | null => watch(g, z, (nat) => atWar(g, nat, boats));
/** The watch a side keeps over a sea for its convoys: every patrol of a nation allied with this one. */
export const watchFor = (g: Game, z: number, side: (nat: string) => boolean): Watch | null => watch(g, z, side);

/** The watch this group would add to its side's over a sea: the share of the waters it reaches that its side would then watch (the row's words), and s itself. */
export function watchWith(g: Game, f: AirFleet, z: number): { share: number; s: number; R: number } {
  const was = { m: f.mission, t: f.target };
  f.mission = 'seapatrol'; f.target = z;
  try {
    const w = watchFor(g, z, (nat) => allied(g, nat, f.nat));
    return w ? { share: w.s / w.R, s: w.s, R: w.R } : { share: 0, s: 0, R: patrolReach(g, f, z)?.R ?? 0 };
  } finally { f.mission = was.m; f.target = was.t; }
}

/** The seas a land bomber group is offered to patrol: those within its reach that carry a convoy route or sea lane of its side, and the one it patrols now. */
export function patrolSeas(g: Game, f: AirFleet): number[] {
  if (!patrolOn(g)) return [];
  const out: number[] = [];
  for (const z of g.seas) {
    if (!patrolReach(g, f, z.id)) continue;
    const ours = seaRoutes(g).some((r) => r.zones.includes(z.id) && allied(g, r.to, f.nat));
    if (ours || (f.mission === 'seapatrol' && f.target === z.id)) out.push(z.id);
  }
  return out;
}

/**
 * The enemy boats a nation's side last saw on station in a sea (seasight.ts seenBoats), never the boats truly there:
 * none where its side never saw any (the design director's ruling of 24 November 2026 17:54Z, Q2 (a)).
 */
const boatsOnStation = (g: Game, nat: string, z: number): number => seenBoats(g, nat, z)?.n ?? 0;

/**
 * The enemy boats on a route's seas as the patrol staff counts them this month, by sea: the boats its side last saw on
 * station there (boatsOnStation), each for its time up, less the part of it its own side's aircraft without kit keep
 * them down (the side's own patrols, which it knows; seapatrol.ts watchFor). The boats are counted where they were last
 * seen, and their own staff's move is not predicted (the design director's ruling of 24 November 2026 17:54Z, Q5 (a)):
 * the next month's sightings follow the boats once they move. A sea where its side saw none is left out.
 */
function boatsExpected(g: Game, nat: string, zs: number[]): Map<number, number> {
  const at = new Map<number, number>();
  const side = (n: string): boolean => allied(g, n, nat);
  for (const z of zs) {
    const b = boatsOnStation(g, nat, z);
    if (!(b > 0)) continue;
    const w = watchFor(g, z, side);
    at.set(z, b * (w && w.none > 0 ? 1 - w.none : 1));
  }
  return at;
}

/**
 * What one more group on patrol over a sea of a route is worth to a staff, on its raid scoring's own scale (aircraft),
 * for the boats it counts in that sea as things stand (B, boatsExpected: where its side last saw them): those its side's
 * patrols would sink there besides, at a strike's worth of a submarine, and the cargoes saved on the route by those it
 * would keep down or cut (T7) there, by last month's losses on the route shared over the boats its side last saw on the
 * route's seas ('all', never the boats truly on station there), at a strike's worth of trade (the share of the receiving
 * nation's goods the route carries, times 3000 for the whole); less the bombers it would lose on patrol (patrolLoss: the
 * operational share of the aircraft on patrol where it is laid, and the enemy's fighters where they meet it), aircraft
 * for aircraft as the raid scoring subtracts a raid's bombers lost. Where the boats then move, the next month's sightings
 * show it, and the next groups go after them (the design director's ruling of 24 November 2026 17:54Z, Q5 (a)).
 */
export function patrolWorth(g: Game, nat: string, f: AirFleet, z: number, route: ConvoyRoute, B: number, all: number): number {
  const F = patrolFigures();
  if (!F || B <= 0) return 0;
  const side = (n: string): boolean => allied(g, n, nat);
  const w0 = watchFor(g, z, side);
  const was = { m: f.mission, t: f.target };
  f.mission = 'seapatrol'; f.target = z;
  let w1: Watch | null;
  try { w1 = watchFor(g, z, side); } finally { f.mission = was.m; f.target = was.t; }
  if (!w1) return 0;
  let v = B * (w1.s * w1.kills - (w0 ? w0.s * w0.kills : 0)) * (CARRIERS.strike.value.ss ?? 0);
  // (Kept down without kit; with it, T7's share of what they would sink cut.)
  const kept = (w1.none - (w0?.none ?? 0)) + (w1.kit - (w0?.kit ?? 0)) * (1 - CAUSE.cover);
  const rec = g.convoys[route.id];
  if (rec?.loss && all > 0) v += 3000 * route.share * rec.loss * ((B * kept) / all);
  const lost = patrolLoss(g, f, z);
  v -= lost.op + lost.fighters;
  return v;
}

/**
 * The computer's air staff and the sea patrol (Q7 (a)), run after its raid staff and weighed against the raid that staff
 * chose (the design director's ruling of 24 November 2026 17:49Z, 2 (a) and 3 (a)). Over each of its side's convoy
 * routes with enemy boats seen on its seas, the most boats first, it sends its land bomber groups one at a time to the
 * sea of the route where its side last saw boats (boatsExpected, seasight.ts seenBoats: never where the boats truly are,
 * and with no guess at their own staff's move, the design director's ruling of 17:54Z, Q5 (a)), the most boats first,
 * less those its own patrols already keep down there, the best placed group first, while one more is worth more than
 * what it would give up for it, with no other test. A group whose patrol is not worth that is passed over and the next
 * best placed is tried; the route is left only once no fit group is worth sending. Where the boats have moved since they
 * were seen, its patrols find them only once its side sees them again. Its orders stand till its next month (the hunt
 * and the convoys read them at its start).
 * The raid staff flies each raid as soon as it chooses it (air.ts raidStaff). So before a raid flies the staff is asked
 * about that raid's bomber group alone ('raid': the group and the raid's own score on the raid scoring), and weighs its
 * patrol against that score; it returns true where it takes the group onto patrol, and the group then gives up the raid,
 * which is not flown. Once the raids are flown it is asked again, without 'raid', and weighs every group left on support
 * against nothing, since such a group flies nothing else that month. True where it sent a group.
 */
export function patrolStaff(g: Game, nat: string, mine: AirFleet[], raid?: { f: AirFleet; v: number }): boolean {
  if (!patrolOn(g)) return false;
  const R = AIRWAR.ai.raids;
  // (Only groups with no fighters: a patrol flies alone, and a group with fighters would leave the ground it guards
  // (comesUp raises only groups left on support). A bomber group defends nothing, so nothing defended is given up.)
  const free = (f: AirFleet): boolean => f.flew !== g.turn && f.mission === 'support' && !f.close && !deckBred(f) && f.kind !== 'ftr' && f.ftr <= 0 && f.bmr > 0 && f.ready >= R.ready;
  const pool = raid ? [raid.f] : mine;
  // (What a group gives up to patrol: the raid the raid staff chose for it, at that raid's own score; nothing for a
  // group left on support.)
  const given = (f: AirFleet): number => (raid && f === raid.f ? raid.v : 0);
  // (Each route by the boats its side last saw on its seas: seasight.ts.)
  const routes: { r: ConvoyRoute; zs: number[]; boats: number }[] = [];
  for (const r of NAVY.convoys) {
    const to = g.nations[r.to];
    if (!to?.alive || !to.atWar.length || !allied(g, r.to, nat)) continue;
    const zs = [...new Set(r.zones.map((k) => zoneOf(g, k)).filter((z) => z >= 0))];
    const boats = zs.reduce((a, z) => a + boatsOnStation(g, nat, z), 0);
    if (boats > 0) routes.push({ r, zs, boats });
  }
  routes.sort((a, b) => b.boats - a.boats || a.zs[0] - b.zs[0]);
  let sent = false;
  for (const { r, zs } of routes) {
    for (;;) {
      const at = boatsExpected(g, nat, zs);
      const all = [...at.values()].reduce((x, y) => x + y, 0);
      let z = -1, most = 0, fit: AirFleet[] = [];
      for (const s of zs) {
        const b = at.get(s) ?? 0;
        if (b <= most + 1e-12) continue;
        const can = pool.filter((f) => free(f) && whyNotMission(g, f, 'seapatrol', s) === null);
        if (can.length) { z = s; most = b; fit = can; }
      }
      if (z < 0) break;
      fit.sort((a, b) => patrolAircraft(g, b, z) - patrolAircraft(g, a, z) || a.id - b.id);
      // (The best placed group first; a group whose patrol is worth no more than what it would give up stays as it is,
      // and the next is tried. The route is left only once no fit group is worth sending.)
      const f = fit.find((x) => patrolWorth(g, nat, x, z, r, most, all) > given(x));
      if (!f) break;
      setMission(g, f, 'seapatrol', z);
      sent = true;
      if (raid) return true;
    }
  }
  return sent;
}
