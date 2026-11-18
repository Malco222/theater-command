// The doorway between the played battles (src/battle) and the Unreal game.
//
// The battles are the browser's own TypeScript sims, run in the game by the same JavaScript engine as the campaign
// rules (Tools/build_rules.sh bundles this file with campaign-bridge.ts). Nothing here decides anything: it builds the
// battle from the campaign's own attack, runs the shared clock, passes the player's orders, hands plain pictures of
// the field back as JSON, and settles the battle through the campaign's own attack order with the field's R in place
// of the dice. The 3D game draws the pictures: the models, their walk and fire, the sounds the events name.
//
// Wiring (two lines in campaign-bridge.ts, for the 3D builder):
//   1. its attack(ids, to) takes two more arguments, fought?: number and field?: FieldBill (src/campaign/game.ts), and
//      passes them on: orderAttack(s, sent, s.regions[to], fought, field) (field: what the field cost each corps and
//      each side, so the bill falls on the corps as the field had it and the after card agrees with the field, ruling 226(6))
//   2. import { battleCalls } from './battle-bridge';   and in the TC object:   ...battleCalls({ game, attack }),
// With no battle fought, nothing changes: attack(ids, to) rolls the dice as before.
// Air adds one more line: its act(action, region) takes a third argument, fought?: FoughtAir, passes it to
// airAct(s, action, fought), and puts airAct's after on what it returns (after: air.after, the after card written
// from what the campaign booked: { lines, level, groups }); and the TC object's line becomes
//   ...battleCalls({ game, attack, raid: (k, t, ids, f) => act(`raid:${k}:${t}:${ids.join('.')}`, -1, f) }),
// With no air battle fought, nothing changes there either: the raid is paid with the dice as before.
//
// The calls, on the same global object TC:
//   TC.battleLook(ids, to)       -> { ticket } for the attack the card shows, null where there is no field battle to fight,
//                                   or { ticket: null, why } where the staff settles that kind of place (offered.ts): keep
//                                   FIGHT IT YOURSELF on the card, off, with why beside it
//   TC.battleStart(ids, to)      -> { field, view }: the battle opens paused; field is the ground, sent once
//   TC.battleTick(seconds)       -> view: runs the clock for that much real time at the speed set. Until the armies first
//                                   meet (view.contact null) a land battle runs ×4 whatever the speed below that, then
//                                   drops to the speed set (ruling 226(1)): say so on the field, "×4 until the armies
//                                   meet", then "The armies meet · Running ×1" when view.contact turns to a time
//   TC.battleSpeed(speed)        -> { speed }: 0 (paused), 0.5, 1, 2 or 4
//   TC.battleOrder(command)      -> { ok }: one of the core's commands (src/battle/core/types.ts), for the player's side
//   TC.battleView()              -> view, without running the clock
//   TC.battleChance()            -> { chance, contact }: the staff's chance now that the attack carries the ground, 0 to
//                                   1, and whether the armies have met, for HAND IT TO THE STAFF's question (ruling
//                                   226(5)); null where the battle has no such question (sea and air word their own)
//   TC.battleEnd()               -> what TC.attack returns ({ report, siege, note, price, paid, moved, view }), once the
//                                   battle is over; null before that (after an air battle: what TC.act returns for the raid)
//   TC.airLook(kind, target, ids) -> { ticket } for the raid the raid card shows, or { ticket: null, why } where it
//                                   cannot be fought out (src/battle/air FOUGHT_KINDS: 'bomb', 'superiority')
//   TC.airStart(kind, target, ids) -> { sky, view }: the air battle opens paused; sky is the board, sent once
//
// A view: { t, limit, speed, units, balance, events, zones, ai, over, me }, and for a land battle breaking: { att, def }
// (how near each army is to breaking, 0 to 1, for the two army bars; an army breaks at 1, ruling 226(3)) and contact
// (battle seconds of the first contact, null before it). Each unit: { id, side, kind, name, x, y, face,
// str, mor, state, figures0, figures, w, d, act, speed, engaged, cover, loop, order }. Field metres, x east, y north,
// the attacker from the south; face in radians, 0 north, clockwise. 'loop' is the sound it plays while it does what it
// does; each event's 'cue' is a one-shot, at its x and y. Sound names are the sound director's (sound-list.json).
// An air group also carries extra: { n, n0, band, alt, job, per } (aircraft now and at the start, height band 0 to 2,
// metres up, what it is doing, aircraft per model); /mnt/project-files/air-battles/3d-handoff.md says how to draw it.
//
// Sea and air plug in by the same rule (their threads send the diff): a start call of their own that builds their sim
// and hands it to begin() with how to settle it; tick, speed, order, view and end are shared.
//
// Fleet actions (src/battle/sea): the rules fight them while a month closes, not from a card, so they are found ahead:
//   TC.seaLook()                 -> { actions: [{ key, title, ticket }] }: the player's fleet actions the month's end
//                                   will bring, found on a copy of the game (call it only when the player has asked to
//                                   fight his fleet actions: it plays the computer's months once more on the copy)
//   TC.seaStart(key)             -> { field: { size, edge, zoneName }, view }: that action, opened paused
// then tick, speed, order, view as above (the sea's own orders: { do: 'sea:speed', units, speed: 'slow' | 'cruise' |
// 'flank' }; the carrier strike is the core's ability command with key 'strike'), and TC.battleEnd() -> { key, R,
// level, lines, sunk, burnt } once it is over. Nothing is booked yet: the played result waits until TC.endMonth()
// reaches that action, and stands in for the dice there (navy.ts seaField). An action not fought, or one that comes
// out of the month with other fleets in it, rolls the dice as before. Each sea unit's extra carries its class (cls),
// its ships afloat and sunk, and its fleet; 'loop' comes from src/battle/sea/cues.ts.

import { Clock, SPEEDS, type Speed } from '../../../src/battle/core/clock';
import { LOOPS } from '../../../src/battle/core/cues';
import type { BattleSim, BattleView, Command, FieldResult, Side, UnitBase } from '../../../src/battle/core/types';
import { GROUNDS } from '../../../src/battle/land/field';
import { fieldBill, landTicket } from '../../../src/battle/land/ticket';
import { LandBattle } from '../../../src/battle/land/sim';
import { staffOnly } from '../../../src/battle/land/offered';
import { MOTION, type Kind } from '../../../src/battle/land/rules';
import type { LandUnit } from '../../../src/battle/land/units';
import { SKY } from '../../../src/battle/air/book';
import { AirSim } from '../../../src/battle/air/sim';
import { airTicket, foughtAir } from '../../../src/battle/air/ticket';
import type { AirOutcome, AirUnit } from '../../../src/battle/air/types';
import type { FoughtAir } from '../../../src/campaign/airfought';
import type { RaidKind } from '../../../src/campaign/airwar';
import type { Game } from '../../../src/campaign/types';
import type { FieldBill } from '../../../src/campaign/game';
import { seaField } from '../../../src/campaign/navy';
import { endEve, endHuman, humanUp, onEve, stepAI } from '../../../src/campaign/turn';
import { SEA_LOOPS } from '../../../src/battle/sea/cues';
import { foughtOf, playedHook, seaAhead, type SeaAhead, type SeaPlayed } from '../../../src/battle/sea/handoff';
import { SEA } from '../../../src/battle/sea/numbers';
import { SeaSim, type SeaLosses } from '../../../src/battle/sea/sim';

export interface BattleHooks {
  game: () => Game;
  /** The campaign bridge's own attack call; with 'fought', the field's R stands in for the dice. */
  attack: (ids: number[], to: number, fought?: number, field?: FieldBill) => unknown;
  /** The campaign bridge's own raid order (act 'raid:kind:target:ids'); with 'fought', the air battle's losses stand in for the dice. */
  raid?: (kind: string, target: number, ids: number[], fought?: FoughtAir) => unknown;
}

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

/** One unit as the 3D game reads it: plain fields only. */
function unitOut(u: UnitBase): Record<string, unknown> {
  const land = u as LandUnit;
  const air = (u as AirUnit).air ? (u as AirUnit) : null;
  const motion = MOTION[u.kind as Kind] as string | undefined;
  const loops = motion && motion !== 'fixed' ? LOOPS[motion as keyof typeof LOOPS] : undefined;
  const extra = air ? { ...(u.extra ?? {}), n: air.n, n0: air.n0, band: air.band, alt: Math.round(air.alt), job: air.job.do, per: air.per } : u.extra;
  return {
    id: u.id, side: u.side, kind: u.kind, name: u.name, x: Math.round(u.x * 10) / 10, y: Math.round(u.y * 10) / 10, face: Math.round(u.face * 1000) / 1000,
    str: Math.round(u.str * 1000) / 1000, mor: Math.round(u.mor * 1000) / 1000, state: u.state, figures0: u.figures0, figures: u.figures,
    w: u.w, d: u.d, act: u.act, speed: Math.round(u.speed * 10) / 10, engaged: u.engaged ?? -1, cover: u.cover ?? '',
    loop: air ? airLoop(air) : (u.extra as { cls?: string } | undefined)?.cls ? SEA_LOOPS[u.act] ?? '' : loops?.[u.act] ?? '',
    // A land block's name for a card and what it belongs to under it ("11th Infantry" over "I Corps"), beside its full name.
    ...(land.short ? { short: land.short, parent: land.parent } : {}),
    order: land.order ? { kind: land.order.kind, x: land.order.x, y: land.order.y, target: land.order.target ?? -1, path: land.path } : null,
    ...(extra ? { extra } : {}),
  };
}

function viewOut(s: Session, v: BattleView): Record<string, unknown> {
  return {
    t: v.t, limit: v.limit, speed: s.clock.speed, units: v.units.map(unitOut), balance: v.balance, events: v.events, zones: v.zones, ai: v.ai, over: v.over, me: s.me,
    ...(v.breaking ? { breaking: { att: Math.round(v.breaking.att * 1000) / 1000, def: Math.round(v.breaking.def * 1000) / 1000 } } : {}),
    ...(v.contact !== undefined ? { contact: v.contact } : {}),
  };
}

/** Fast until the armies meet, then the speed the player set (ruling 226(1)); the browser page runs the same. */
const RUSH = 4;

export function battleCalls(h: BattleHooks) {
  let s: Session | null = null;
  const need = (): Session => { if (!s) throw new Error('No battle is being fought'); return s; };

  // Fleet actions the month's end will bring, and the ones the player has fought, waiting for the close to reach them.
  let seaAhead0: SeaAhead[] = [];
  const seaPlayed: SeaPlayed = new Map();
  // Left in place for the session: with nothing played it answers nothing, so every fleet action rolls the dice as before.
  seaField.fought = playedHook(seaPlayed);
  /** The month's end as the rules run it, on a copy: the player's close, then the computer's months to his next. */
  const closeMonth = (c: Game): void => {
    if (onEve(c)) endEve(c); else endHuman(c);
    for (let guard = 0; guard < 200 && !humanUp(c) && c.human; guard++) stepAI(c);
  };

  /** Any domain's battle on the shared clock: it opens paused, the player's side commanding. */
  function begin(sim: BattleSim, settle: (r: FieldResult) => unknown): Session {
    const clock = new Clock();
    clock.set(1); clock.toggle();
    s = { sim, clock, me: sim.ticket.att.human ? 'att' : 'def', settle, settled: undefined };
    return s;
  }

  return {
    battleLook(ids: number[], to: number) {
      const g = h.game();
      const corps = ids.map((i) => g.corps.find((c) => c.id === i)).filter((c): c is NonNullable<typeof c> => !!c);
      const t = landTicket(g, corps, g.regions[to]);
      if (!t) return null;
      // The safety net (src/battle/land/offered.ts): a kind of place the field still lands more than a tenth off its card
      // is settled by the staff; the card keeps FIGHT IT YOURSELF, off, with this one sentence beside it.
      const why = staffOnly(t);
      return why ? { ticket: null, why } : { ticket: t };
    },
    battleStart(ids: number[], to: number) {
      const g = h.game();
      const corps = ids.map((i) => g.corps.find((c) => c.id === i)).filter((c): c is NonNullable<typeof c> => !!c);
      const target = g.regions[to];
      const t = landTicket(g, corps, target);
      if (!t || staffOnly(t)) return null;
      const sim = new LandBattle(t);
      // The campaign is asked again at the end with the corps the player picked: it decides who goes in, as it did for the
      // card. Those going in are the ticket's own, so a corps spent since it was picked is not shown going (b453).
      const going = t.att.forces.map((f) => f.corps);
      const ses = begin(sim, (r) => h.attack(ids, to, r.R, fieldBill(r)));
      const f = sim.field;
      return {
        field: { w: f.w, h: f.h, cell: f.cell, cols: f.cols, rows: f.rows, grounds: GROUNDS, ground: f.ground, height: f.height.map((x) => Math.round(x * 10) / 10), zones: f.zones, town: f.town, place: t.place },
        going, view: viewOut(ses, sim.view(ses.me)),
      };
    },
    battleTick(seconds: number) {
      const ses = need();
      const met = (ses.sim as Partial<LandBattle>).contactAt;
      const sp = ses.clock.speed;
      ses.clock.rush = met === null && sp > 0 && sp < RUSH ? RUSH / sp : 1;
      ses.clock.advance(ses.sim, seconds);
      return viewOut(ses, ses.sim.view(ses.me));
    },
    battleSpeed(speed: number) {
      const ses = need();
      const sp = (SPEEDS as readonly number[]).includes(speed) ? (speed as Speed) : 1;
      ses.clock.set(sp);
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
    battleChance() {
      const ses = need();
      const land = ses.sim instanceof LandBattle ? ses.sim : null;
      return land ? { chance: Math.round(land.chanceNow() * 1000) / 1000, contact: land.contactAt !== null } : null;
    },
    battleEnd() {
      const ses = need();
      if (!ses.sim.over) return null;
      if (ses.settled === undefined) ses.settled = ses.settle(ses.sim.over) ?? null;
      const out = ses.settled;
      s = null;
      return out;
    },
    seaLook() {
      const g = h.game();
      const me = g.human;
      seaAhead0 = me ? seaAhead(g, closeMonth).filter((a) => a.ticket.place.fleets.some((f) => f.nat === me)) : [];
      return { actions: seaAhead0.map((a) => ({ key: a.key, title: a.ticket.title, ticket: a.ticket })) };
    },
    seaStart(key: string) {
      const a = seaAhead0.find((x) => x.key === key);
      if (!a) return null;
      const sim = new SeaSim(a.ticket);
      const ses = begin(sim, (r) => {
        seaPlayed.set(a.key, { meeting: a.meeting, fought: foughtOf(r) });
        const x = r.extra as unknown as SeaLosses;
        return { key: a.key, R: r.R, level: r.level, lines: r.lines, sunk: x.sunk, burnt: x.burnt };
      });
      return { field: { size: SEA.field.size, edge: SEA.field.edge, zoneName: a.ticket.place.zoneName }, view: viewOut(ses, sim.view(ses.me)) };
    },
    airLook(kind: string, target: number, ids: number[]) {
      const t = airTicket(h.game(), kind as RaidKind, target, ids);
      return t.ticket ? { ticket: t.ticket } : { ticket: null, why: t.why };
    },
    airStart(kind: string, target: number, ids: number[]) {
      const raid = h.raid;
      if (!raid) return null;
      const t = airTicket(h.game(), kind as RaidKind, target, ids);
      if (!t.ticket) return null;
      const sim = new AirSim(t.ticket);
      // The raid is ordered at the end with the groups the player picked, and paid with what the battle reports
      // (the campaign keeps it inside the dice's own run). Handed to the staff partway, the battle reports the whole raid.
      const ses = begin(sim, (r) => raid(kind, target, ids, r.extra?.air ? foughtAir(r.extra.air as AirOutcome) : undefined));
      return {
        sky: { w: SKY.w, h: SKY.h, scale: SKY.scale, target: SKY.target, flakR: SKY.flakR, bands: SKY.bands.alt, place: t.ticket.place },
        view: viewOut(ses, sim.view(ses.me)),
      };
    },
  };
}
