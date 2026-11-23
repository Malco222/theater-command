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

import type { FieldResult, FieldTicket } from '../../../src/battle/core/types';
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
  };
}
