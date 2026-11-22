// The doorway between the land battle rules and the 3D game's live ground battle (release 2).
//
// Brian, 22 November 2026 02:55Z: "i want the ground battles to be playable. im the general who controls their
// deployment and who to attack and move on a map with tactics". The battle is fought by the browser prototype's own
// land rules (src/battle/land) as he watches; this file only starts it, passes his orders, and hands back what was
// fought as JSON in the recording's own shapes (src/battle/land/live.ts). Tools/build_land_rules.sh bundles it into
// Content/Rules/land-rules.js, which the game runs in a rules engine of its own on a worker thread: the campaign's
// rules (campaign-rules.js, release 1) are not touched and never wait on a battle.
//
// The game calls these by name through the global object TC (this engine's own; the campaign's TC is another):
//
//   TC.landPieces()                        -> [{ key, title, date, att, def, seeds }]: the set pieces that can be fought
//   TC.landStart(key, seed, { me, setup, platoons, staff, radio }) -> the recording's header: the field, the blocks,
//                                             the sky, and the setup room when the player deploys first. me 'att' | 'def'
//                                             | null (a film). staff and radio (direct command) are on unless false
//   TC.landTick(seconds, maxSteps)         -> a delta: what was fought in up to `seconds` more (none while setup is open).
//                                             It opens {"t":..., and {"t":...,"end":... when it is the battle's last
//   TC.landTo(t, maxSteps)                 -> a delta: fought on to battle time t
//   TC.landOrder(command)                  -> { ok }: one of the player's orders, in the rules' shapes (core/types Command):
//                                             move { units, x, y, face?, run? }, line, attack { units, target }, hold,
//                                             withdraw, land:fire, land:staff { units } (back to his staff), auto. Each
//                                             may carry at: the battle time shown when he gave it (the rules run ahead of
//                                             the screen; by radio the wait counts from then)
//   TC.landCanPlace(id, x, y)              -> { ok, why }: whether a block may stand there in the setup step
//   TC.landPlace(id, x, y, face)           -> { ok, why }: places it; the next delta starts the men again (reset)
//   TC.landBegin()                         -> { ok }: ends the setup step; the clock runs
//   TC.landHedges(hedges, gates)           -> { ok, hedges, gates }: the 3D field's hedge lines [{ kind, points: [[x, y], ...] }]
//                                             and gateways [{ x, y, yaw, width, kind: 'gate' }], metres from the field's
//                                             south-west corner, before the clock runs (false after): a block on a hedge
//                                             line hides as in woods, and the men on foot go through the gates
//   TC.landEnd()                           -> { over }: lets the battle go
//
// Nothing here decides anything about the fight.

import type { Command, Side } from '../../../src/battle/core/types';
import { LiveBattle, livePiece, type LiveDelta } from '../../../src/battle/land/live';
import { PIECES } from '../../../src/battle/land/pieces';
import type { Gate, Hedge } from '../../../src/battle/land/sim';

let live: LiveBattle | null = null;

const none: LiveDelta = { t: 0, men: [], tails: [], rounds: [], events: [], views: [], phases: [], contact: null };

function need(): LiveBattle {
  if (!live) throw new Error('No ground battle is being fought: TC.landStart first.');
  return live;
}

(globalThis as Record<string, unknown>).TC = {
  landPieces() {
    return PIECES.map((p) => ({
      key: p.key, title: p.title, date: p.date, seeds: p.seeds,
      att: { nat: p.att.nat, adj: p.att.adj, army: p.att.army, general: p.att.general },
      def: { nat: p.def.nat, adj: p.def.adj, army: p.def.army, general: p.def.general },
    }));
  },

  landStart(key: string, seed?: number | null, opts?: { me?: Side | null; setup?: boolean; platoons?: boolean; staff?: boolean; radio?: boolean; hide?: boolean; armour?: boolean; names?: boolean } | null) {
    live = livePiece(key, seed ?? undefined, {
      me: opts?.me ?? null, setup: !!opts?.setup, platoons: opts?.platoons !== false, staff: opts?.staff !== false, radio: opts?.radio !== false,
      hide: opts?.hide !== false, armour: opts?.armour !== false, names: opts?.names !== false,
    });
    return live.header();
  },

  landTick(seconds: number, maxSteps?: number) {
    return live ? live.tick(seconds, maxSteps ?? 50) : none;
  },

  landTo(t: number, maxSteps?: number) {
    return live ? live.to(t, maxSteps ?? 50) : none;
  },

  landOrder(c: Command) {
    return { ok: need().order(c) };
  },

  landCanPlace(id: number, x: number, y: number) {
    const why = need().canPlace(id, x, y);
    return { ok: why === null, why: why ?? '' };
  },

  landPlace(id: number, x: number, y: number, face?: number | null) {
    const why = need().canPlace(id, x, y);
    const ok = need().place(id, x, y, face ?? undefined);
    return { ok, why: ok ? '' : why ?? 'Not now: the battle has begun.' };
  },

  landBegin() {
    return { ok: need().begin() };
  },

  landHedges(hedges: Hedge[], gates?: Gate[] | null) {
    const l = need();
    const ok = Array.isArray(hedges) && l.hedges(hedges, Array.isArray(gates) ? gates : []);
    return { ok, hedges: l.sim.hedges.length, gates: l.sim.gates.length };
  },

  landEnd() {
    const over = live?.sim.over ?? null;
    live = null;
    return { over };
  },
};
