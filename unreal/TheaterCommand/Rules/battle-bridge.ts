// The doorway between the general's ground battle and the campaign (release 1.5): the campaign half of FIGHT IT YOURSELF.
//
// Release 1.5 fights a campaign battle in the 3D game's own land rules (Content/Rules/land-rules.js, TC.landStartTicket);
// this file is the campaign's side of it, bundled with campaign-bridge.ts by Tools/build_rules.sh. It is the land part of
// release 2's battle-bridge.ts, ported onto release 1's campaign: nothing here decides anything. It builds the battle's
// ticket from the campaign's own attack, and settles the battle through the campaign's own attack order with the
// field's R in place of the dice and the field's bill (ruling 226(6)). Release 2's looks ride on the ticket as release 2
// sends them (Brian 23 November 06:35Z, "yes 1 and 2 for 1.5"), each shown only with its Battle rules row on: the
// weather word (wet mud), scarred ground and old ruts, captured tanks, worn uniforms.
//
// Wiring (made in campaign-bridge.ts):
//   1. its attack(ids, to) takes two more arguments, fought?: number and field?: FieldBill (src/campaign/game.ts), and
//      passes them on: orderAttack(s, sent, s.regions[to], fought, field). With no battle fought, nothing changes.
//   2. import { battleCalls } from './battle-bridge';   and in the TC object:   ...battleCalls({ game, attack }),
//      through its playerOrder, so a fought attack is noted as the player's own deed, as the same order from the map is.
//
// The calls, on the same global object TC:
//   TC.battleLook(ids, to)       -> { ticket, live } for the attack the card shows; null where there is no field battle
//                                   to fight; { ticket: null, why } where the staff settles that kind of place
//                                   (src/battle/land/offered.ts). live: { fog, seen, designYears }, the card's fog, the
//                                   corps it identified and each side's armour design year (looks only). The whole answer
//                                   goes to the land rules' TC.landStartTicket.
//   TC.battleSettleLand(ids, to, over, seed?) -> the campaign's attack answer, as TC.attack gives it, for the battle the
//                                   land rules fought (the last delta's over); the same result asked twice in a month is
//                                   settled once; a seed that is not the ticket's is another battle ({ why }).
//
// Release 1.6 adds the air battle (src/battle/air, release 2's, on release 1.5's campaign): day raids and the fight for
// the sky, fought here on the shared clock, as release 2's battle-bridge.ts fights them (AirBattle.cpp BeginLive reads
// these answers). Wiring: the TC object's line is
//   ...battleCalls({ game, attack, raid: (k, t, ids, f) => act(`raid:${k}:${t}:${ids.join('.')}`, -1, f) }),
// and act passes 'fought' to airAct. With no air battle fought, a raid is paid with the dice as before.
//   TC.airLook(kind, target, ids) -> { ticket } for the raid the raid card shows, or { ticket: null, why } where it
//                                   cannot be fought out (src/battle/air FOUGHT_KINDS: 'bomb', 'superiority')
//   TC.airStart(kind, target, ids) -> { sky, view }: the air battle opens paused; sky is the board, sent once
//   TC.battleTick(seconds)       -> view, after running the clock that many real seconds at the speed set
//   TC.battleSpeed(speed)        -> { speed }: 0 (paused), 0.5, 1, 2 or 4
//   TC.battleOrder(command)      -> { ok }: one of the core's commands (src/battle/core/types.ts), for the player's side
//   TC.battleView()              -> view, without running the clock
//   TC.battleEnd()               -> once the battle is over, what TC.act returns for the raid, with its after card
//                                   ('after', with each side's totals 'lost'); null before that
// A view: { sky, t, limit, speed, units, balance, events, zones, ai, over, me }. An air group carries extra: { n, n0,
// band, alt, job, per } (aircraft now and at the start, height band 0 to 2, height, its job, aircraft per figure).

import type { BattleSim, BattleView, BreakCause, Command, FieldResult, FieldTicket, Side, UnitBase } from '../../../src/battle/core/types';
import { Clock, SPEEDS, type Speed } from '../../../src/battle/core/clock';
import { SKY } from '../../../src/battle/air/book';
import { AirSim } from '../../../src/battle/air/sim';
import { airTicket, foughtAir } from '../../../src/battle/air/ticket';
import type { AirOutcome, AirUnit } from '../../../src/battle/air/types';
import { afterLost, type AfterCard, type FoughtAir } from '../../../src/campaign/airfought';
import type { RaidKind } from '../../../src/campaign/airwar';
import type { LandPlace } from '../../../src/battle/land/field';
import { cardKnows, fieldBill, landTicket, pursuitLine } from '../../../src/battle/land/ticket';
import { staffOnly } from '../../../src/battle/land/offered';
import { armourYear } from '../../../src/campaign/tech';
import type { BattleReport, Game } from '../../../src/campaign/types';
import { groundWeather, type FieldBill } from '../../../src/campaign/game';
import { attackOutlook } from '../../../src/campaign/turn';

export interface BattleHooks {
  game: () => Game;
  /** The campaign bridge's own attack call; with 'fought', the field's R stands in for the dice. */
  attack: (ids: number[], to: number, fought?: number, field?: FieldBill) => unknown;
  /** The campaign bridge's own raid order (act 'raid:kind:target:ids'); with 'fought', the air battle's losses stand in for the dice (release 1.6). */
  raid?: (kind: string, target: number, ids: number[], fought?: FoughtAir) => unknown;
}

/** An air battle on the shared clock (release 1.6), as release 2's bridge keeps any battle. */
interface Session {
  sim: BattleSim;
  clock: Clock;
  me: Side;
  /** How the campaign takes the result, once the battle is over. Called once. */
  settle: (r: FieldResult) => unknown;
  settled: unknown;
}

/** An air group's loop: its engines while it flies, nothing on the ground or once it is gone. */
function airLoop(a: AirUnit): string {
  return a.act === 'idle' || a.act === 'down' ? '' : SKY.cues.loops[a.air];
}

/**
 * One air group as the 3D game reads it (AirFeed.cpp ReadView): plain fields only, the same as release 2's unitOut gives
 * an air group. Exported for the tests.
 */
export function unitOut(u: UnitBase): Record<string, unknown> {
  const air = u as AirUnit;
  const extra = { ...(u.extra ?? {}), n: air.n, n0: air.n0, band: air.band, alt: Math.round(air.alt), job: air.job.do, per: air.per };
  return {
    id: u.id, side: u.side, kind: u.kind, name: u.name, x: Math.round(u.x * 10) / 10, y: Math.round(u.y * 10) / 10, face: Math.round(u.face * 1000) / 1000,
    str: Math.round(u.str * 1000) / 1000, mor: Math.round(u.mor * 1000) / 1000, state: u.state, figures0: u.figures0, figures: u.figures,
    w: u.w, d: u.d, act: u.act, speed: Math.round(u.speed * 10) / 10, engaged: u.engaged ?? -1, cover: u.cover ?? '',
    loop: airLoop(air), order: null, extra,
  };
}

/**
 * What an air battle offers the screens beyond the shared view (AirFeed.cpp ReadView's 'sky'): the staff's chance now,
 * whether a shot has been fired, whether the fight is winding down, how near each side is to breaking off and why, the
 * field's score, and each group's name as the dispatches call it.
 */
function skyOut(a: AirSim, me: Side): Record<string, unknown> {
  const r3 = (x: number): number => Math.round(x * 1000) / 1000;
  const near = (side: Side) => { const n = a.nearWhy(side); return { at: r3(n.at), why: n.why }; };
  const sc = a.score();
  return {
    chance: r3(a.staffChance()), fired: a.fired, winding: a.winding(), near: { att: near('att'), def: near('def') },
    score: { lost: sc.lost, worth: Math.round(sc.worth), cut: sc.cut, ruined: sc.ruined, ratio: r3(sc.ratio), up: sc.up },
    called: Object.fromEntries(a.units.map((u) => [u.id, a.called(u)])), me,
  };
}

function viewOut(s: Session, v: BattleView): Record<string, unknown> {
  return {
    ...(s.sim instanceof AirSim ? { sky: skyOut(s.sim, s.me) } : {}),
    t: v.t, limit: v.limit, speed: s.clock.speed, units: v.units.map((u) => unitOut(u)), balance: v.balance, events: v.events, zones: v.zones, ai: v.ai, over: v.over, me: s.me,
    ...(v.breaking ? { breaking: { att: Math.round(v.breaking.att * 1000) / 1000, def: Math.round(v.breaking.def * 1000) / 1000 } } : {}),
    ...(v.breakingBy ? { breakingBy: v.breakingBy as Record<Side, BreakCause> } : {}),
    ...(v.contact !== undefined ? { contact: v.contact } : {}),
    ...(v.flights ? { flights: v.flights } : {}),
    ...(v.air ? { air: v.air } : {}),
  };
}

/**
 * A land battle's options for the 3D game's own land rules (TC.landStartTicket): the card's fog where it did not know the
 * defence (b453), the corps it identified (b476(3)), each side's armour design year (armourYear: looks only).
 */
export function liveOf(g: Game, t: FieldTicket<LandPlace>, look: { exact: boolean } | null): { fog: boolean; seen: number[]; designYears: Record<string, number> } {
  const fog = !!look && !look.exact;
  const designYears: Record<string, number> = {};
  // (every nation with a force on the field: an ally's corps fights in its own nation's tanks)
  for (const nat of new Set([t.att.nat, t.def.nat, ...t.att.forces.map((f) => f.nat), ...t.def.forces.map((f) => f.nat)])) {
    const y = armourYear(g, nat);
    if (y) designYears[nat] = y;
  }
  return { fog, seen: fog ? [...cardKnows(g, t)] : [], designYears };
}

export function battleCalls(h: BattleHooks) {
  // The air battle being fought (release 1.6): one at a time.
  let s: Session | null = null;
  const need = (): Session => { if (!s) throw new Error('No battle is being fought'); return s; };
  /** A battle on the shared clock: it opens paused, the player's side commanding. */
  function begin(sim: BattleSim, settle: (r: FieldResult) => unknown): Session {
    const clock = new Clock();
    clock.set(1); clock.toggle();
    s = { sim, clock, me: sim.ticket.att.human ? 'att' : 'def', settle, settled: undefined };
    return s;
  }
  /** How the campaign takes the land rules' result: its own attack order with the field's R and bill, and the pursuit. */
  const landSettle = (ids: number[], to: number, t: FieldTicket<LandPlace>) => (r: FieldResult): unknown => {
    const out = h.attack(ids, to, r.R, fieldBill(r));
    // The army that broke, or pulled out under fire, booked more than the field cost it: the after card's line (b464, b474).
    const rep = (out as { report?: BattleReport | null } | null)?.report;
    // His army fell back on his order (ALL FALL BACK, release 1.5): the report says so, for the month's card.
    // (on the campaign's own report as kept, and on the answer, which is a copy of it)
    if (rep && r.fellBack) {
      rep.fellBack = r.fellBack;
      const kept = h.game().reports.find((x) => x.id === rep.id);
      if (kept) kept.fellBack = r.fellBack;
    }
    const pursuit = rep ? pursuitLine(t, r, rep) : null;
    // Why the army broke, for the after card's words where it broke (b496(2)).
    const more = { ...(pursuit ? { pursuit } : {}), ...(r.brokeBy ? { brokeBy: r.brokeBy } : {}) };
    return out && typeof out === 'object' && Object.keys(more).length ? { ...out, ...more } : out;
  };
  /** The battles handed back, so the same result asked again is settled once: kept by the campaign they were settled in. */
  const landSettled = new WeakMap<Game, Map<string, { seed: number; out: unknown }>>();
  const WRONG_BATTLE = 'The attack the campaign would make now is not the battle that was fought.';

  return {
    battleLook(ids: number[], to: number) {
      const g = h.game();
      const corps = ids.map((i) => g.corps.find((c) => c.id === i)).filter((c): c is NonNullable<typeof c> => !!c);
      const t = landTicket(g, corps, g.regions[to]);
      if (!t) return null;
      // The safety net (src/battle/land/offered.ts): a kind of place the field still lands more than a tenth off its card
      // is settled by the staff.
      const why = staffOnly(t);
      if (why) return { ticket: null, why };
      const ticket: typeof t = { ...t };
      // Scarred ground: the battles fought here before this one, and the months since the last, for the field to be laid
      // already torn up; absent where none has been. Old ruts: of those, the ones fought in the mud.
      const scar = g.scars?.[to];
      if (scar && scar.n > 0) ticket.scars = { count: scar.n, months: Math.max(0, g.turn - scar.last) };
      if (scar?.mudN && scar.mudLast !== undefined) ticket.oldRuts = { count: scar.mudN, months: Math.max(0, g.turn - scar.mudLast) };
      // The ground's weather this month, in the 3D game's words (thaw mud, snow, rain, clear); looks only. The land
      // rules send it to the 3D game only with the Battle rules card's "Wet mud" row on.
      const wx = groundWeather(g, g.regions[to]);
      if (wx && !ticket.weather) ticket.weather = wx;
      // Captured kit: each side's latest capture of the last six months, whose tanks and how many blocks.
      const took = (nat: string) => (g.captured?.[nat] ?? []).filter((k) => g.turn - k.turn < 6).slice(-1)[0];
      const ca = took(t.att.nat), cd = took(t.def.nat);
      if (ca || cd) ticket.captured = { ...(ca ? { att: { from: ca.from, n: ca.n } } : {}), ...(cd ? { def: { from: cd.from, n: cd.n } } : {}) };
      // Worn uniforms: each side's months in the line without a rest (its corps' weeks, weighed by divisions), where
      // the campaign counts them; the Worn uniforms row (off at first in 1.5) decides whether the 3D game hears of it.
      const worn = (cs: typeof corps) => {
        const divs = cs.reduce((s2, c) => s2 + c.divs, 0);
        return divs > 0 && cs.some((c) => c.line !== undefined) ? Math.round(cs.reduce((s2, c) => s2 + (c.line ?? 0) * c.divs, 0) / divs / 4.345) : undefined;
      };
      const inAttack = new Set(corps.map((c) => c.id));
      const wa = worn(corps), wd = worn(g.corps.filter((c) => c.reg === to && !inAttack.has(c.id) && c.kind !== 'fort'));
      if (wa !== undefined) ticket.att = { ...ticket.att, worn: wa };
      if (wd !== undefined) ticket.def = { ...ticket.def, worn: wd };
      return { ticket, live: liveOf(g, t, attackOutlook(g, corps, g.regions[to])) };
    },
    battleSettleLand(ids: number[], to: number, result: FieldResult | { over: FieldResult | null }, seed?: number) {
      const r = result && 'over' in result ? result.over : (result as FieldResult | null);
      if (!r || typeof r.R !== 'number' || !Array.isArray(r.lines)) return null;
      const g = h.game();
      const given = seed !== undefined && seed !== null ? Number(seed) : null;
      // (settled already in this campaign this month: its answer again, nothing booked twice; a seed given must be the one
      // it was settled under, or it is another battle)
      const done = landSettled.get(g) ?? new Map<string, { seed: number; out: unknown }>();
      const key = JSON.stringify([g.turn, ids, to, r]);
      const was = done.get(key);
      if (was) return given !== null && given !== was.seed ? { why: WRONG_BATTLE } : was.out;
      const corps = ids.map((i) => g.corps.find((c) => c.id === i)).filter((c): c is NonNullable<typeof c> => !!c);
      const t = landTicket(g, corps, g.regions[to]);
      if (!t || staffOnly(t)) return null;
      // (the battle fought was the one the campaign would make now: the ticket's own seed, where the 3D game passes it)
      if (given !== null && given !== t.seed) return { why: WRONG_BATTLE };
      const out = landSettle(ids, to, t)(r) ?? null;
      done.set(key, { seed: t.seed, out });
      landSettled.set(g, done);
      return out;
    },
    airLook(kind: string, target: number, ids: number[]) {
      const t = airTicket(h.game(), kind as RaidKind, target, ids);
      if (!t.ticket) return { ticket: null, why: t.why };
      // The raid card only asks whether there is a battle in the air for the pick, each time the pick changes, so the
      // ground's outlines (about 154 KB of the 160 KB ticket) stay out of its answer: the battle's own board (airStart)
      // carries them (the playtester's finding 3, 23 Nov: about 52 ms a pick change against the 33 ms order).
      const { ground: _ground, ...place } = t.ticket.place as typeof t.ticket.place & { ground?: unknown };
      return { ticket: { ...t.ticket, place } };
    },
    airStart(kind: string, target: number, ids: number[]) {
      const raid = h.raid;
      if (!raid) return null;
      const t = airTicket(h.game(), kind as RaidKind, target, ids);
      if (!t.ticket) return null;
      // Shown, never counted (ruling 237): who shot whom, passes, stragglers, guns, escort roles for the 3D game to act out.
      const sim = new AirSim(t.ticket, { show: true });
      // The raid is ordered at the end with the groups the player picked, and paid with what the battle reports (the
      // campaign keeps it inside the dice's own run). Handed to the staff partway, the battle reports the whole raid. The
      // after card goes out with its totals, each side's fighters and bombers lost (AirFeed.cpp ReadAfter reads
      // after.lost), summed from its own rows (afterLost), so the totals and the rows never disagree.
      const withLost = (out: unknown): unknown => {
        const after = (out as { after?: AfterCard } | null)?.after;
        return after ? { ...(out as object), after: { ...after, lost: afterLost(after) } } : out;
      };
      const ses = begin(sim, (r) => withLost(raid(kind, target, ids, r.extra?.air ? foughtAir(r.extra.air as AirOutcome) : undefined)));
      // The title, the date and the two sides as the 3D game reads them (AirFeed.cpp ReadSky).
      const side = (x: typeof t.ticket.att) => ({ nat: x.nat, adj: x.adj, human: x.human });
      return {
        sky: {
          w: SKY.w, h: SKY.h, scale: SKY.scale, target: SKY.target, flakR: SKY.flakR, bands: SKY.bands.alt, place: t.ticket.place,
          title: t.ticket.title, date: t.ticket.date, att: side(t.ticket.att), def: side(t.ticket.def),
        },
        view: viewOut(ses, sim.view(ses.me)),
      };
    },
    battleTick(seconds: number) {
      const ses = need();
      ses.clock.advance(ses.sim, seconds);
      return viewOut(ses, ses.sim.view(ses.me));
    },
    battleSpeed(speed: number) {
      const ses = need();
      ses.clock.set((SPEEDS as readonly number[]).includes(speed) ? (speed as Speed) : 1);
      return { speed: ses.clock.speed };
    },
    battleOrder(c: Command) {
      const ses = need();
      return { ok: ses.sim.command(c, ses.me) };
    },
    battleView() {
      const ses = need();
      return viewOut(ses, ses.sim.view(ses.me));
    },
    battleEnd() {
      const ses = need();
      if (!ses.sim.over) return null;
      if (ses.settled === undefined) ses.settled = ses.settle(ses.sim.over) ?? null;
      const out = ses.settled;
      s = null;
      return out;
    },
  };
}
