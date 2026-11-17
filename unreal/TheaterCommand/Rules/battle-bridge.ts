// The doorway between the played battles (src/battle) and the Unreal game.
//
// The battles are the browser's own TypeScript sims, run in the game by the same JavaScript engine as the campaign
// rules (Tools/build_rules.sh bundles this file with campaign-bridge.ts). Nothing here decides anything: it builds the
// battle from the campaign's own attack, runs the shared clock, passes the player's orders, hands plain pictures of
// the field back as JSON, and settles the battle through the campaign's own attack order with the field's R in place
// of the dice. The 3D game draws the pictures: the models, their walk and fire, the sounds the events name.
//
// Wiring (two lines in campaign-bridge.ts, for the 3D builder):
//   1. its attack(ids, to) takes a third argument, fought?: number, and passes it on: orderAttack(s, sent, s.regions[to], fought)
//   2. import { battleCalls } from './battle-bridge';   and in the TC object:   ...battleCalls({ game, attack }),
// With no battle fought, nothing changes: attack(ids, to) rolls the dice as before.
//
// The calls, on the same global object TC:
//   TC.battleLook(ids, to)       -> { ticket } for the attack the card shows, or null where there is no field battle to fight
//   TC.battleStart(ids, to)      -> { field, view }: the battle opens paused; field is the ground, sent once
//   TC.battleTick(seconds)       -> view: runs the clock for that much real time at the speed set
//   TC.battleSpeed(speed)        -> { speed }: 0 (paused), 0.5, 1, 2 or 4
//   TC.battleOrder(command)      -> { ok }: one of the core's commands (src/battle/core/types.ts), for the player's side
//   TC.battleView()              -> view, without running the clock
//   TC.battleEnd()               -> what TC.attack returns ({ report, siege, note, price, paid, moved, view }), once the
//                                   battle is over; null before that
//
// A view: { t, limit, speed, units, balance, events, zones, ai, over }. Each unit: { id, side, kind, name, x, y, face,
// str, mor, state, figures0, figures, w, d, act, speed, engaged, cover, loop, order }. Field metres, x east, y north,
// the attacker from the south; face in radians, 0 north, clockwise. 'loop' is the sound it plays while it does what it
// does; each event's 'cue' is a one-shot, at its x and y. Sound names are the sound director's (sound-list.json).
//
// Sea and air plug in by the same rule (their threads send the diff): a start call of their own that builds their sim
// and hands it to begin() with how to settle it; tick, speed, order, view and end are shared.

import { Clock, SPEEDS, type Speed } from '../../../src/battle/core/clock';
import { LOOPS } from '../../../src/battle/core/cues';
import type { BattleSim, BattleView, Command, FieldResult, Side, UnitBase } from '../../../src/battle/core/types';
import { GROUNDS } from '../../../src/battle/land/field';
import { goingIn, landTicket } from '../../../src/battle/land/ticket';
import { LandBattle } from '../../../src/battle/land/sim';
import { MOTION, type Kind } from '../../../src/battle/land/rules';
import type { LandUnit } from '../../../src/battle/land/units';
import type { Game } from '../../../src/campaign/types';

export interface BattleHooks {
  game: () => Game;
  /** The campaign bridge's own attack call; with 'fought', the field's R stands in for the dice. */
  attack: (ids: number[], to: number, fought?: number) => unknown;
}

interface Session {
  sim: BattleSim;
  clock: Clock;
  me: Side;
  /** How the campaign takes the result, once the battle is over. Called once. */
  settle: (r: FieldResult) => unknown;
  settled: unknown;
}

/** One unit as the 3D game reads it: plain fields only. */
function unitOut(u: UnitBase): Record<string, unknown> {
  const land = u as LandUnit;
  const motion = MOTION[u.kind as Kind] as string | undefined;
  const loops = motion && motion !== 'fixed' ? LOOPS[motion as keyof typeof LOOPS] : undefined;
  return {
    id: u.id, side: u.side, kind: u.kind, name: u.name, x: Math.round(u.x * 10) / 10, y: Math.round(u.y * 10) / 10, face: Math.round(u.face * 1000) / 1000,
    str: Math.round(u.str * 1000) / 1000, mor: Math.round(u.mor * 1000) / 1000, state: u.state, figures0: u.figures0, figures: u.figures,
    w: u.w, d: u.d, act: u.act, speed: Math.round(u.speed * 10) / 10, engaged: u.engaged ?? -1, cover: u.cover ?? '',
    loop: loops?.[u.act] ?? '',
    order: land.order ? { kind: land.order.kind, x: land.order.x, y: land.order.y, target: land.order.target ?? -1, path: land.path } : null,
    ...(u.extra ? { extra: u.extra } : {}),
  };
}

function viewOut(s: Session, v: BattleView): Record<string, unknown> {
  return { t: v.t, limit: v.limit, speed: s.clock.speed, units: v.units.map(unitOut), balance: v.balance, events: v.events, zones: v.zones, ai: v.ai, over: v.over, me: s.me };
}

export function battleCalls(h: BattleHooks) {
  let s: Session | null = null;
  const need = (): Session => { if (!s) throw new Error('No battle is being fought'); return s; };

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
      return t ? { ticket: t } : null;
    },
    battleStart(ids: number[], to: number) {
      const g = h.game();
      const corps = ids.map((i) => g.corps.find((c) => c.id === i)).filter((c): c is NonNullable<typeof c> => !!c);
      const target = g.regions[to];
      const t = landTicket(g, corps, target);
      if (!t) return null;
      const sim = new LandBattle(t);
      // The campaign is asked again at the end with the corps the player picked: it decides who goes in, as it did for the card.
      const going = goingIn(g, corps, target).map((c) => c.id);
      const ses = begin(sim, (r) => h.attack(ids, to, r.R));
      const f = sim.field;
      return {
        field: { w: f.w, h: f.h, cell: f.cell, cols: f.cols, rows: f.rows, grounds: GROUNDS, ground: f.ground, height: f.height.map((x) => Math.round(x * 10) / 10), zones: f.zones, town: f.town, place: t.place },
        going, view: viewOut(ses, sim.view(ses.me)),
      };
    },
    battleTick(seconds: number) {
      const ses = need();
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
    battleEnd() {
      const ses = need();
      if (!ses.sim.over) return null;
      if (ses.settled === undefined) ses.settled = ses.settle(ses.sim.over) ?? null;
      const out = ses.settled;
      s = null;
      return out;
    },
    // Sea and air add their start calls here: build the sim from their ticket, then begin(sim, (r) => their hand-back).
  };
}
