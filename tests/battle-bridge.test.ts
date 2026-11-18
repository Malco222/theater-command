// The 3D game's doorway to the field battles (unreal/TheaterCommand/Rules/battle-bridge.ts): a real campaign attack
// fought through it, the way the game will call it, and settled through the campaign's own attack order.
import { describe, expect, it } from 'vitest';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { newCampaign, offers } from '../src/battle/ui/campaign';
import { orderAttack } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

function rig(): { g: Game; TC: ReturnType<typeof battleCalls>; fought: (number | undefined)[] } {
  const g = newCampaign();
  const fought: (number | undefined)[] = [];
  const TC = battleCalls({
    game: () => g,
    attack: (ids, to, f) => {
      fought.push(f);
      const report = orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f);
      return { report };
    },
  });
  return { g, TC, fought };
}

describe('the battle doorway for the 3D game', () => {
  it('opens paused on the attack card\'s own ticket, and sends the ground once', () => {
    const { g, TC } = rig();
    const o = offers(g)[0];
    const ids = o.corps.map((c) => c.id);
    expect(TC.battleLook(ids, o.target.id)!.ticket!.reckoning.odds).toBeCloseTo(o.odds);
    const st = TC.battleStart(ids, o.target.id)!;
    expect(st.field.ground.length).toBe(st.field.cols * st.field.rows);
    expect(st.view.speed).toBe(0);
    expect(TC.battleTick(5).t).toBe(0);
    const u = (st.view.units as { side: string; loop: string; figures0: number }[]).find((x) => x.side === 'att')!;
    expect(u.figures0).toBeGreaterThan(0);
    expect(typeof u.loop).toBe('string');
  });
  it('takes the player\'s orders, runs the clock, and settles through the campaign with the field\'s R', () => {
    const { g, TC, fought } = rig();
    const o = offers(g)[0];
    const ids = o.corps.map((c) => c.id);
    const st = TC.battleStart(ids, o.target.id)!;
    const units = st.view.units as { id: number; side: string }[];
    const mine = units.filter((x) => x.side === 'att').map((x) => x.id), foe = units.find((x) => x.side === 'def')!.id;
    expect(TC.battleOrder({ do: 'attack', units: mine, target: foe }).ok).toBe(true);
    expect(TC.battleSpeed(4).speed).toBe(4);
    for (let i = 0; i < 30; i++) TC.battleTick(1);
    expect(TC.battleView().t).toBeGreaterThan(100);
    expect(TC.battleEnd()).toBeNull();
    TC.battleOrder({ do: 'dice' });
    const v = TC.battleView() as { over: { R: number } | null };
    expect(v.over).not.toBeNull();
    const out = TC.battleEnd() as { report: { odds: number } | null };
    expect(fought).toEqual([v.over!.R]);
    expect(out.report!.odds).toBeCloseTo(v.over!.R);
    expect(() => TC.battleView()).toThrow();
  });
});
