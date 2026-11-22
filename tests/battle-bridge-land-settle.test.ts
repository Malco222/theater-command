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
});
