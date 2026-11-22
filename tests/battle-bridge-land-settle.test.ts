// A campaign battle fought in the 3D game's own land rules and handed back (release 2, the production lead's (a)): the
// campaign's TC.battleLook answer carries the live options the 3D game opens it with (TC.landStartTicket), and
// TC.battleSettleLand settles the field's result through the same closure as TC.battleStart's battle: the same result
// handed back the new way leaves the campaign exactly as the old way does.
import { describe, expect, it } from 'vitest';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { newCampaign, offers } from '../src/battle/ui/campaign';
import { attackOutlook, orderAttack } from '../src/campaign/turn';
import { cardKnows } from '../src/battle/land/ticket';
import type { FieldResult } from '../src/battle/core/types';
import type { Game } from '../src/campaign/types';
import { loadGame, saveGame } from '../src/campaign/save';

function rig(): { g: Game; TC: ReturnType<typeof battleCalls>; attacks: number } & { count: () => number } {
  const g = newCampaign();
  let attacks = 0;
  const TC = battleCalls({
    game: () => g,
    attack: (ids, to, f, field) => {
      attacks++;
      const report = orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f, field);
      return { report };
    },
  });
  return { g, TC, attacks, count: () => attacks };
}

/** Fights TC.battleStart's battle to its end, his staff commanding his blocks: the view's over and battleEnd's answer. */
function fought(TC: ReturnType<typeof battleCalls>, ids: number[], to: number): { over: FieldResult; out: unknown } {
  TC.battleStart(ids, to, { staff: true });
  TC.battleSpeed(4);
  let over: FieldResult | null = null;
  for (let i = 0; i < 20000 && !over; i++) over = (TC.battleTick(1) as { over: FieldResult | null }).over;
  expect(over).not.toBeNull();
  return { over: over!, out: TC.battleEnd() };
}

/** What the campaign holds after: who owns what, and each corps' place and strength. */
const state = (g: Game): string => JSON.stringify({ owners: g.regions.map((r) => r.owner), corps: g.corps });

describe('a campaign battle handed back from the 3D game\'s land rules', () => {
  it('battleLook carries the live options battleStart sets on its own field', () => {
    const { g, TC } = rig();
    const o = offers(g).find((x) => attackOutlook(g, x.corps, x.target)?.exact === false) ?? offers(g)[0];
    const ids = o.corps.map((c) => c.id);
    const look = TC.battleLook(ids, o.target.id) as { ticket: { seed: number }; live: { fog: boolean; seen: number[]; designYears: Record<string, number> } };
    const exact = attackOutlook(g, o.corps, o.target)?.exact;
    expect(look.live.fog).toBe(exact === false);
    if (look.live.fog) expect(new Set(look.live.seen)).toEqual(cardKnows(g, look.ticket as never));
    expect(typeof look.live.designYears).toBe('object');
  });

  it('settles the same result the same way as battleStart\'s battle: the round trip', () => {
    const a = rig(), b = rig();
    expect(state(a.g)).toBe(state(b.g));
    const o = offers(a.g)[0];
    const ids = o.corps.map((c) => c.id), to = o.target.id;
    const seed = (b.TC.battleLook(ids, to) as { ticket: { seed: number } }).ticket.seed;
    const old = fought(a.TC, ids, to);
    // the 3D game's land rules hand back TC.landResult's answer as it stands, with the ticket's seed
    const back = b.TC.battleSettleLand(ids, to, { over: structuredClone(old.over) }, seed);
    expect(back).toEqual(old.out);
    expect(state(b.g)).toBe(state(a.g));
    expect(b.count()).toBe(1);
    // asked again with the same result: the same answer, settled once
    expect(b.TC.battleSettleLand(ids, to, { over: structuredClone(old.over) }, seed)).toEqual(old.out);
    expect(b.count()).toBe(1);
    expect(state(b.g)).toBe(state(a.g));
  });

  it('refuses a result for another battle than the one the campaign would make now, and nothing is settled', () => {
    const a = rig(), b = rig();
    const o = offers(a.g)[0];
    const ids = o.corps.map((c) => c.id), to = o.target.id;
    const seed = (b.TC.battleLook(ids, to) as { ticket: { seed: number } }).ticket.seed;
    const old = fought(a.TC, ids, to);
    const before = state(b.g);
    expect(b.TC.battleSettleLand(ids, to, old.over, seed + 1)).toEqual({ why: expect.any(String) });
    expect(b.TC.battleSettleLand(ids, to, { over: null })).toBeNull();
    expect(b.count()).toBe(0);
    expect(state(b.g)).toBe(before);
  });

  it('settles once in the campaign it was fought in: a load or a new campaign settles the same battle afresh', () => {
    // (one TC over a game that is swapped, as campaign-bridge's TC.load and TC.start swap theirs)
    let cur = newCampaign();
    let attacks = 0;
    const TC = battleCalls({
      game: () => cur,
      attack: (ids, to, f, field) => {
        attacks++;
        return { report: orderAttack(cur, ids.map((i) => cur.corps.find((c) => c.id === i)!), cur.regions[to], f, field) };
      },
    });
    const before = saveGame(cur);
    const o = offers(cur)[0];
    const ids = o.corps.map((c) => c.id), to = o.target.id;
    const seed = (TC.battleLook(ids, to) as { ticket: { seed: number } }).ticket.seed;
    const field = rig();
    const old = fought(field.TC, ids, to);
    const first = TC.battleSettleLand(ids, to, { over: structuredClone(old.over) }, seed);
    const after = state(cur);
    expect(attacks).toBe(1);
    // asked again with another seed: another battle, refused, not the answer it had
    expect(TC.battleSettleLand(ids, to, { over: structuredClone(old.over) }, seed + 1)).toEqual({ why: expect.any(String) });
    expect(attacks).toBe(1);
    // the save from before the battle loaded: the same battle refought to the same result is settled in it
    cur = loadGame(before)!;
    expect((TC.battleLook(ids, to) as { ticket: { seed: number } }).ticket.seed).toBe(seed);
    const again = TC.battleSettleLand(ids, to, { over: structuredClone(old.over) }, seed);
    expect(attacks).toBe(2);
    expect(state(cur)).toBe(after);
    expect(again).not.toBe(first);
    expect(again).toEqual(first);
    // and a new campaign as well
    cur = newCampaign();
    TC.battleSettleLand(ids, to, { over: structuredClone(old.over) }, seed);
    expect(attacks).toBe(3);
  });

  it('remembers every battle settled in its campaign: one settled between does not let the first be booked twice', () => {
    const { g, TC, count } = rig();
    const field = rig();
    const all = offers(g);
    const a = all[0], b = all.find((x) => x.target.id !== a.target.id && !x.corps.some((c) => a.corps.some((d) => d.id === c.id)));
    if (!b) return;
    const ia = a.corps.map((c) => c.id), ib = b.corps.map((c) => c.id);
    const sa = (TC.battleLook(ia, a.target.id) as { ticket: { seed: number } }).ticket.seed;
    const sb = (TC.battleLook(ib, b.target.id) as { ticket: { seed: number } } | null)?.ticket?.seed;
    const ra = fought(field.TC, ia, a.target.id).over;
    const fieldB = rig();
    const rb = sb === undefined ? null : fought(fieldB.TC, ib, b.target.id).over;
    TC.battleSettleLand(ia, a.target.id, { over: structuredClone(ra) }, sa);
    expect(count()).toBe(1);
    if (rb && sb !== undefined) {
      TC.battleSettleLand(ib, b.target.id, { over: structuredClone(rb) }, sb);
      expect(count()).toBe(2);
    }
    const n = count();
    TC.battleSettleLand(ia, a.target.id, { over: structuredClone(ra) }, sa);
    expect(count()).toBe(n);
  });
});
