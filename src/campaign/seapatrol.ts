// The sea patrol against submarines (release 2, Units decide; the design director's ruling of 24 November 2026 15:14Z
// on naval-battles/convoy-r2/sea-patrol-form.md: (a) on all nine questions, with his marks of 15:14Z, 15:23Z and 15:32Z).
//
// Any land bomber group of any nation can take the standing order "Patrol the [sea] against submarines", one sea a
// group, within its bombers' reach as a strike is judged (a sea with no coast only with long-range patrol aircraft, from
// a coast of a sea next to it). It then flies nothing else that month. Its aircraft watch a share of the sea:
//
//   s = R x (1 - e^(-A / A_R)),  A_R = a0 x R / air
//
// R the share of the sea's waters the patrols reach (its 'air' share, all of it with long-range patrol aircraft), A the
// aircraft on patrol there (each group's bombers x readiness x the sky, at the air rules' cover curve over the distance
// it flies), a0 one figure fitted through the game to the historian's arc. In watched waters a boat on station is
// attacked and sunk at the rates of the kit its attackers carry (navy.ts patrolSubs), and aircraft without the kit keep
// the boats down instead: their time on station there counts at 1 - s (navy.ts onWatch). With the kit, OEG T7's cut
// stands at the watched share (navy.ts seaCover).
//
// Every figure is data/campaign/convoycause.json 'patrol'. While a0, the attack rates or the kill shares are unread,
// nothing here is read (the design director's mark of 15:14Z: never remove a working cause before its replacement is
// laid): no order is offered, the staffs send no group, and the flat air term and the cover from coasts held stand.
// Off (no Units decide) none of it exists. Nothing in it reads the flag or the date.
import { CAUSE, NAVY, airKit, aswKit, atSea, onStation, seaRoutes, zoneOf } from './navy';
import { AIRWAR, CARRIERS, airRange, assessAir, cover, deckBred, forecastAir, kmToSea, setMission, whyNotMission } from './air';
import { allied, atWar } from './game';
import { railsAt } from './rails';
import { bombingFor } from './difficulty';
import { unitsDecide } from './units';
import { grounded, skyAtSea, skySupport, weatherOn } from './weather';
import type { AirFleet, Game } from './types';

/** The sea patrol's figures, all laid: a0, the attacks and kill shares by kit. */
interface Laid { a0: number; attacks: { kit: number; none: number }; kill: { kit: number; none: number }; lost: number | null }

/** The sea patrol's figures where every one it needs is laid, else null. */
export function patrolFigures(): Laid | null {
  const P = CAUSE.patrol;
  if (!P || P.a0 === null || P.attacks.kit === null || P.attacks.none === null || P.kill.kit === null || P.kill.none === null) return null;
  return { a0: P.a0, attacks: { kit: P.attacks.kit, none: P.attacks.none }, kill: { kit: P.kill.kit, none: P.kill.none }, lost: P.lost };
}

/** True where this campaign flies sea patrols against submarines: Units decide, and every figure laid. */
export const patrolOn = (g: Game): boolean => unitsDecide(g) && patrolFigures() !== null;

/**
 * How a group reaches a sea on patrol: the share of its waters it can watch (R) and how far it flies to get there, or
 * null where it cannot patrol it. A sea with a coast is reached as a strike is judged, its nearest coast within the
 * group's bomber range: its 'air' share of waters, all of them with long-range patrol aircraft. A sea with no coast is
 * reached only with long-range patrol aircraft, from a base within range of a coast of a sea next to it. Carrier wings
 * keep their escort cover and fighters their reach: neither patrols.
 */
export function patrolReach(g: Game, f: AirFleet, z: number): { R: number; d: number } | null {
  const zone = g.seas[z];
  if (!zone || f.ship !== undefined || f.ashore !== undefined || f.kind === 'ftr') return null;
  const range = airRange(f, g).bmr;
  const long = aswKit(g, f.nat).patrol;
  if (zone.coast.length) {
    const d = kmToSea(g, f, z);
    return d <= range ? { R: long ? 1 : zone.air, d } : null;
  }
  if (!long) return null;
  let d = Infinity;
  for (const a of zone.adj) if (g.seas[a]?.coast.length) d = Math.min(d, kmToSea(g, f, a));
  return d <= range ? { R: 1, d } : null;
}

/** The aircraft a group keeps on patrol over a sea this month: its bombers x its readiness x the sky (none under a closed sky or from a grounded field, half under cloud), all of them out to 60% of its range and fewer towards its limit (the air rules' cover curve). */
export function patrolAircraft(g: Game, f: AirFleet, z: number, at = patrolReach(g, f, z)): number {
  if (!at || f.bmr <= 0) return 0;
  const sky = weatherOn(g) ? (grounded(g, f) ? 0 : skySupport(skyAtSea(g, z))) : 1;
  return f.bmr * f.ready * sky * cover(at.d, airRange(f, g).bmr);
}

/**
 * The watch the aircraft on patrol keep over a sea, for one side: s, the share of its waters they watch; 'kit' and
 * 'none', the parts of it watched by aircraft with anti-submarine kit (depth charges and sea-search radar) and without;
 * A and R; 'attacks', the attacks on a boat a month in watched waters and 'kills' the boats sunk a boat-month there,
 * each the patrols' own by their aircraft; and each nation's aircraft, for whose aircraft sank what. Null where no
 * group of the side patrols it, or the patrol is not laid.
 */
export interface Watch { s: number; kit: number; none: number; A: number; R: number; attacks: number; kills: number; by: Record<string, number> }

function watch(g: Game, z: number, side: (nat: string) => boolean): Watch | null {
  const F = patrolFigures();
  if (!F || !unitsDecide(g) || !g.seas[z]) return null;
  let A = 0, Ak = 0, R = 0, attacks = 0, kills = 0;
  const by: Record<string, number> = {};
  for (const f of g.airFleets) {
    if (f.mission !== 'seapatrol' || f.target !== z || !side(f.nat)) continue;
    const at = patrolReach(g, f, z);
    const a = patrolAircraft(g, f, z, at);
    if (!at || a <= 0) continue;
    const kit = airKit(aswKit(g, f.nat));
    A += a; if (kit) Ak += a;
    R = Math.max(R, at.R);
    const rate = kit ? F.attacks.kit : F.attacks.none;
    attacks += a * rate; kills += a * rate * (kit ? F.kill.kit : F.kill.none);
    by[f.nat] = (by[f.nat] ?? 0) + a;
  }
  if (A <= 0 || R <= 0) return null;
  const AR = (F.a0 * R) / g.seas[z].air;
  const s = R * (1 - Math.exp(-A / AR));
  return { s, kit: (s * Ak) / A, none: (s * (A - Ak)) / A, A, R, attacks: attacks / A, kills: kills / A, by };
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

/** The boats of a nation's enemies on station in a sea now (each fleet's boats x its share of the month on station). */
function boatsOnStation(g: Game, nat: string, z: number): number {
  let b = 0;
  for (const q of g.fleets) if (q.zone === z && atSea(q) && q.ships.ss > 0 && atWar(g, q.nat, nat)) b += q.ships.ss * onStation(g, q, z);
  return b;
}

/**
 * What one more group on patrol over a sea is worth to a staff, on its raid scoring's own scale (aircraft): the enemy
 * boats its side's patrols would sink there besides, at a strike's worth of a submarine, and the cargoes they would save
 * on its side's convoy routes through the sea, at a strike's worth of trade (the share of the receiving nation's goods
 * the route carries, times 3000 for the whole), by last month's losses on the route a boat on station; less the
 * aircraft it would lose on patrol where that is laid.
 */
function patrolWorth(g: Game, nat: string, f: AirFleet, z: number): number {
  const F = patrolFigures();
  if (!F) return 0;
  const side = (n: string): boolean => allied(g, n, nat);
  const before = watchFor(g, z, side);
  const was = { m: f.mission, t: f.target };
  f.mission = 'seapatrol'; f.target = z;
  let after: Watch | null;
  try { after = watchFor(g, z, side); } finally { f.mission = was.m; f.target = was.t; }
  if (!after) return 0;
  const k0 = before ? before.s * before.kills : 0, n0 = before?.none ?? 0, c0 = before?.kit ?? 0;
  const B = boatsOnStation(g, nat, z);
  let v = B * (after.s * after.kills - k0) * (CARRIERS.strike.value.ss ?? 0);
  // (The cargoes: a boat-month kept down saves what a boat on station sank last month; one watched with kit, T7's share of it.)
  const kept = (after.none - n0) + (after.kit - c0) * (1 - CAUSE.cover);
  for (const r of NAVY.convoys) {
    const rz = r.zones.map((k) => zoneOf(g, k));
    const rec = g.convoys[r.id];
    if (!rz.includes(z) || !allied(g, r.to, nat) || !rec?.loss || !rec.count?.boats) continue;
    v += 3000 * r.share * rec.loss * ((B * kept) / rec.count.boats);
  }
  if (F.lost !== null) v -= F.lost * f.bmr;
  return v;
}

/**
 * The raid this group would fly on its own this month, on the staff's raid scoring (air.ts raidStaff): the best of
 * the 'look' most promising works in its reach, where the forecast clears the staff's gates; 0 where none does.
 */
function raidWorth(g: Game, nat: string, f: AirFleet): number {
  const R = AIRWAR.ai.raids;
  const B = bombingFor(g, nat);
  const rules = B ? { ...R, ...B } : R;
  let best = 0;
  const works = g.regions
    .filter((r) => atWar(g, nat, r.ctrl) && (r.fac > 0 || railsAt(r.id).length >= 3) && whyNotMission(g, f, 'bomb', r.id) === null)
    .sort((a, b) => b.fac + rules.rail * railsAt(b.id).length - (a.fac + rules.rail * railsAt(a.id).length) || a.id - b.id)
    .slice(0, R.look);
  for (const r of works) {
    const a = assessAir(g, [f], 'bomb', r.id);
    if (a.why) continue;
    const fc = forecastAir(g, a, 'bomb', r.id);
    if (fc.bomberLoss > rules.loss || fc.success < R.success) continue;
    const v = fc.worth * (r.fac <= 0 ? rules.rail : 1) + fc.foeLoss - fc.ftrLoss - fc.bmrLoss;
    if (v > best) best = v;
  }
  return best;
}

/**
 * The computer's air staff and the sea patrol (Q7 (a)): each month, before its raids, over each sea of its side's
 * convoy routes where enemy boats are on station, the most enemy boats first, it sends its land bomber groups in reach,
 * the best placed first, while one more is worth more than the raid it would fly, and at least what a raid must be
 * worth to be flown. Its orders stand till its next month (the hunt and the convoys read them at its start).
 */
export function patrolStaff(g: Game, nat: string, mine: AirFleet[]): void {
  if (!patrolOn(g)) return;
  const R = AIRWAR.ai.raids;
  const seas = new Map<number, number>();
  for (const r of NAVY.convoys) {
    const to = g.nations[r.to];
    if (!to?.alive || !to.atWar.length || !allied(g, r.to, nat)) continue;
    for (const k of r.zones) { const z = zoneOf(g, k); if (z >= 0 && !seas.has(z)) seas.set(z, boatsOnStation(g, nat, z)); }
  }
  const order = [...seas.entries()].filter(([, b]) => b > 0).sort((a, b) => b[1] - a[1] || a[0] - b[0]);
  for (const [z] of order) {
    const fit = mine.filter((f) => f.flew !== g.turn && f.mission === 'support' && !f.close && !deckBred(f) && f.kind !== 'ftr' && f.bmr > 0 && f.ready >= R.ready && whyNotMission(g, f, 'seapatrol', z) === null);
    fit.sort((a, b) => patrolAircraft(g, b, z) - patrolAircraft(g, a, z) || a.id - b.id);
    for (const f of fit) {
      const v = patrolWorth(g, nat, f, z);
      if (v <= R.worth || v <= raidWorth(g, nat, f)) break;
      setMission(g, f, 'seapatrol', z);
    }
  }
}
