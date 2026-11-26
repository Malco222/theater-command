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
//   TC.landStart(key, seed, { me, setup, platoons, staff, radio, relief, rider, cutters, smoke, bocage, phones, bearers, patrol, captured, worn, mud, scars, blast, escort, mech, spg, td, cars, aa, light, japan }) -> the recording's header: the field, the blocks,
//                                             the sky, and the setup room when the player deploys first. me 'att' | 'def'
//                                             | null (a film). staff and radio (direct command) are on unless false
//   TC.landStartTicket(look, opts)         -> the same header for a campaign battle (release 2, switched): the campaign's
//                                             TC.battleLook answer as it stands ({ ticket, live: { fog, seen, designYears,
//                                             sky } }) or a bare ticket, opts as landStart's (and any of live's, which they
//                                             override); the last delta's over goes back to TC.battleSettleLand
//   TC.landResult()                        -> { over }: the battle's result once it is over (null before), as the
//                                             campaign's TC.battleSettleLand takes it; the battle is kept
//   TC.landTick(seconds, maxSteps)         -> a delta: what was fought in up to `seconds` more (none while setup is open).
//                                             It opens {"t":..., and {"t":...,"end":... when it is the battle's last
//   TC.landTo(t, maxSteps)                 -> a delta: fought on to battle time t
//   TC.landOrder(command)                  -> { ok }: one of the player's orders, in the rules' shapes (core/types Command):
//                                             move { units, x, y, face?, run? }, line, attack { units, target }, hold,
//                                             withdraw, land:fire, land:staff { units } (back to his staff), auto. Each
//                                             may carry at: the battle time shown when he gave it (the rules run ahead of
//                                             the screen; by radio the wait counts from then)
//                                             land:wait { units } (hold until relieved: dig in for a corps coming up
//                                             from next door; release 2, the rules' sim.relief)
//   TC.landWaitWhy(id)                     -> { why, due }: why that block of his may not wait for help (null where it
//                                             may), and the battle second help could reach it there (null where not)
//                                             land:cut { units, x, y } (hedge cutters: tanks cut a gap through the hedge
//                                             there), land:smoke { units, x0, y0, x1, y1 } (smoke on call: men on foot lay
//                                             a screen along that line); a refusal says why
//   TC.landCutWhy(id, x?, y?)              -> { why }: why that tank block of his may not cut a gap there (null where it may)
//   TC.landSmokeWhy(id, x0?, y0?, x1?, y1?) -> { why }: why that block of his may not lay smoke along that line (null where
//                                             it may); with no point or line, whether it may at all
//   TC.landCanPlace(id, x, y)              -> { ok, why }: whether a block may stand there in the setup step
//   TC.landPlace(id, x, y, face)           -> { ok, why }: places it; the next delta starts the men again (reset)
//   TC.landPlaceMany([[id, x, y, face], ...]) -> { ok, why, id?, who? }: places several as one act, all or none; a
//                                             refusal names the block that cannot go (release 2: drawing up together)
//   TC.landDrawUp(kind, ids?)              -> { ok, why, id?, who?, placed }: draws those blocks (none named: all of his)
//                                             up in a 'line', 'two' lines, a 'wedge' or a 'column', facing the enemy
//   TC.landBegin()                         -> { ok }: ends the setup step; the clock runs
//   TC.landHedges(hedges, gates)           -> { ok, hedges, gates }: the 3D field's hedge lines [{ kind, points: [[x, y], ...] }]
//                                             and gateways [{ x, y, yaw, width, kind: 'gate' }], metres from the field's
//                                             south-west corner, before the clock runs (false after): a block on a hedge
//                                             line hides as in woods, and the men on foot go through the gates
//   TC.landEnd()                           -> { over }: lets the battle go
//
// Nothing here decides anything about the fight.

import type { Command, FieldTicket, Side } from '../../../src/battle/core/types';
import { LiveBattle, livePiece, liveTicket, type LiveDelta, type LiveOpts, type LiveSky } from '../../../src/battle/land/live';
import type { LandPlace } from '../../../src/battle/land/field';
import { FORMATIONS } from '../../../src/battle/land/formation';
import { PIECES } from '../../../src/battle/land/pieces';
import { ownTeamsLook } from '../../../src/battle/land/own-teams';
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

  landStart(key: string, seed?: number | null, opts?: { me?: Side | null; setup?: boolean; platoons?: boolean; staff?: boolean; radio?: boolean; hide?: boolean; armour?: boolean; names?: boolean; relief?: boolean; rider?: boolean; cutters?: boolean; smoke?: boolean; bocage?: boolean; phones?: boolean; bearers?: boolean; patrol?: boolean; captured?: boolean; worn?: boolean; mud?: boolean; scars?: boolean; blast?: boolean; escort?: boolean; mech?: boolean; spg?: boolean; td?: boolean; cars?: boolean; aa?: boolean; light?: boolean; japan?: boolean } | null) {
    live = livePiece(key, seed ?? undefined, {
      me: opts?.me ?? null, setup: !!opts?.setup, platoons: opts?.platoons !== false, staff: opts?.staff !== false, radio: opts?.radio !== false,
      hide: opts?.hide !== false, armour: opts?.armour !== false, names: opts?.names !== false, relief: opts?.relief !== false, rider: opts?.rider !== false,
      cutters: opts?.cutters !== false, smoke: opts?.smoke !== false, bocage: !!opts?.bocage,
      phones: opts?.phones !== false, bearers: opts?.bearers !== false, teams: (opts as { teams?: boolean } | null | undefined)?.teams === true, facing: (opts as { facing?: boolean } | null | undefined)?.facing === true, stand: (opts as { stand?: boolean } | null | undefined)?.stand === true, patrol: opts?.patrol !== false, captured: opts?.captured !== false, worn: opts?.worn !== false, mud: opts?.mud !== false, scars: opts?.scars !== false, blast: opts?.blast === true, escort: opts?.escort === true, mech: opts?.mech === true, spg: opts?.spg === true, td: opts?.td === true, cars: opts?.cars === true, aa: opts?.aa === true, light: opts?.light === true, japan: opts?.japan === true,
    });
    return live.header();
  },

  /**
   * A campaign battle live (release 2, behind the game's switch): its ticket as the campaign's TC.battleLook gives it,
   * the card's fog (fog, seen: the corps it identified) and each nation's armour design year, with the same switches as
   * landStart. Its result (the last delta's over) goes back to the campaign's TC.battleSettleLand(ids, to, over).
   */
  landStartTicket(
    ticketOrLook: FieldTicket<LandPlace> | { ticket: FieldTicket<LandPlace>; live?: Partial<LiveOpts> },
    opts?: { me?: Side | null; setup?: boolean; platoons?: boolean; staff?: boolean; radio?: boolean; hide?: boolean; armour?: boolean; names?: boolean; relief?: boolean; rider?: boolean; cutters?: boolean; smoke?: boolean; bocage?: boolean; phones?: boolean; bearers?: boolean; patrol?: boolean; captured?: boolean; worn?: boolean; mud?: boolean; scars?: boolean; blast?: boolean; escort?: boolean; mech?: boolean; spg?: boolean; td?: boolean; cars?: boolean; aa?: boolean; light?: boolean; japan?: boolean; island?: boolean; fog?: boolean; seen?: number[]; designYears?: Record<string, number>; sky?: LiveSky } | null,
  ) {
    // (release 2, the Battle rules card's "teams", off unless true: every infantry force of a campaign battle with its
    // own machine-gun and mortar battalions, cut from its own share and its own men: src/battle/land/own-teams.ts.
    // The same switch turns on the rules' sim.teamsRule, here and in landStart: a team's fire shakes by its own harm,
    // and a defence's line stays in its places while its teams fire; the design director, 25 November 17:52Z.
    // It works only with the card's "facing" on, Units face the fire that hurts most (the rules' sim.faceRule; the
    // design director, 25 November 21:54Z): with `teams` alone the forces are as they were and no team block is laid.
    // "facing" works only with the card's "stand" on, A stand is kept (sim.standRule; the design director, 26 November
    // 01:02Z), so the teams are laid only with all three rows on.)
    const sw = opts as { teams?: boolean; facing?: boolean; stand?: boolean } | null | undefined;
    if (sw?.teams === true && sw?.facing === true && sw?.stand === true) ticketOrLook = ownTeamsLook(ticketOrLook);
    // The campaign's TC.battleLook answer as it stands ({ ticket, live: { fog, seen, designYears, sky } }), or a bare ticket.
    const look = 'ticket' in ticketOrLook && ticketOrLook.ticket ? ticketOrLook : null;
    const ticket = look ? look.ticket : (ticketOrLook as FieldTicket<LandPlace>);
    const o = { ...(look?.live ?? {}), ...(opts ?? {}) };
    live = liveTicket(ticket, {
      me: o.me ?? null, setup: !!o.setup, platoons: o.platoons !== false, staff: o.staff !== false, radio: o.radio !== false,
      hide: o.hide !== false, armour: o.armour !== false, names: o.names !== false, relief: o.relief !== false, rider: o.rider !== false,
      cutters: o.cutters !== false, smoke: o.smoke !== false, bocage: !!o.bocage,
      phones: o.phones !== false, bearers: o.bearers !== false, teams: (o as { teams?: boolean }).teams === true, facing: (o as { facing?: boolean }).facing === true, stand: (o as { stand?: boolean }).stand === true, patrol: o.patrol !== false, captured: o.captured !== false, worn: o.worn !== false, mud: o.mud !== false, scars: o.scars !== false, blast: o.blast === true, escort: o.escort === true, mech: o.mech === true, spg: o.spg === true, td: o.td === true, cars: o.cars === true, aa: o.aa === true, light: o.light === true, japan: o.japan === true, island: o.island === true,
      fog: !!o.fog, seen: o.seen ?? [], designYears: o.designYears ?? {}, ...(o.sky ? { sky: o.sky } : {}),
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
    const ok = need().order(c);
    // (a wait refused says why in the rules' own words: the first of its blocks' reason, for the general's note)
    const o = c as { do?: string; units?: number[] };
    if (!ok && o.do === 'land:wait' && o.units?.length) return { ok, why: need().waitWhy(Number(o.units[0])).why ?? '' };
    // (so do a cut and a screen refused: hedge cutters and smoke on call)
    const p = c as { x?: number; y?: number; x0?: number; y0?: number; x1?: number; y1?: number };
    if (!ok && o.do === 'land:cut' && o.units?.length) return { ok, why: need().cutWhy(Number(o.units[0]), p.x, p.y) ?? '' };
    if (!ok && o.do === 'land:smoke' && o.units?.length) return { ok, why: need().smokeWhy(Number(o.units[0]), p.x0, p.y0, p.x1, p.y1) ?? '' };
    // (and a night patrol refused)
    const n = c as { unit?: number };
    if (!ok && o.do === 'land:patrol') return { ok, why: need().patrolWhy(Number(n.unit ?? o.units?.[0])) ?? '' };
    return { ok };
  },

  landWaitWhy(id: number) {
    return need().waitWhy(Number(id));
  },

  landCutWhy(id: number, x?: number | null, y?: number | null) {
    return { why: need().cutWhy(Number(id), x ?? undefined, y ?? undefined) };
  },

  landSmokeWhy(id: number, x0?: number | null, y0?: number | null, x1?: number | null, y1?: number | null) {
    return { why: need().smokeWhy(Number(id), x0 ?? undefined, y0 ?? undefined, x1 ?? undefined, y1 ?? undefined) };
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

  landPlaceMany(list: number[][]) {
    const l = need(), rows = Array.isArray(list) ? list.filter((p) => Array.isArray(p)).map((p) => p.map(Number)) : [];
    const no = l.canPlaceMany(rows);
    const ok = no === null && l.placeMany(rows);
    return { ok, why: ok ? '' : no?.why ?? 'Not now: the battle has begun.', ...(no && no.id >= 0 ? { id: no.id, who: no.who } : {}) };
  },

  landDrawUp(kind: string, ids?: number[] | null) {
    const k = FORMATIONS.find((f) => f === kind);
    if (!k) return { ok: false, why: 'No such formation.', placed: 0 };
    const r = need().drawUp(k, Array.isArray(ids) ? ids.map(Number) : []);
    return { ok: r.ok, why: r.why, ...(r.id !== undefined ? { id: r.id, who: r.who ?? '' } : {}), placed: r.placed.length };
  },

  landBegin() {
    return { ok: need().begin() };
  },

  landHedges(hedges: Hedge[], gates?: Gate[] | null) {
    const l = need();
    const ok = Array.isArray(hedges) && l.hedges(hedges, Array.isArray(gates) ? gates : []);
    return { ok, hedges: l.sim.hedges.length, gates: l.sim.gates.length };
  },

  landResult() {
    return { over: live?.sim.over ?? null };
  },

  landEnd() {
    const over = live?.sim.over ?? null;
    live = null;
    return { over };
  },
};
