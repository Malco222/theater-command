// The air calls through the 3D doorway (from the air battles thread): look, start, tick to the end, and settle through the raid order.
import { describe, expect, it } from 'vitest';
import { planRaid, raidTargets } from '../src/campaign/airwar';
import { airAct } from '../src/campaign/airpages';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';

function setup() {
  const g = startOpen(createGame(7, 'GER'));
  for (const w of ['UK', 'FRA']) setWar(g, 'GER', w);
  const t = raidTargets(g, 'bomb', 'GER').find((x) => planRaid(g, 'bomb', x.id, undefined, 'GER').place === 'Alsace')!;
  const day = planRaid(g, 'bomb', t.id, undefined, 'GER');
  const TC = battleCalls({ game: () => g, attack: () => null, raid: (k, tg, ids, f) => airAct(g, `raid:${k}:${tg}:${ids.join('.')}`, f) });
  return { g, TC, target: t.id, ids: day.picked };
}

describe('air through the bridge', () => {
  it('looks, starts, ticks to the end and settles through the raid order', () => {
    const { g, TC, target, ids } = setup();
    expect(TC.airLook('bomb', target, ids).ticket).toBeTruthy();
    expect(TC.airLook('ground', target, ids).ticket).toBeNull();
    const st = TC.airStart('bomb', target, ids)!;
    expect(st.sky.bands).toEqual([1800, 4500, 7200]);
    const u0 = (st.view.units as any[])[0];
    expect(u0.extra.n).toBeGreaterThan(0);
    expect(['bomber_medium_loop', 'fighter_engine_loop', '']).toContain(u0.loop);
    TC.battleSpeed(4);
    expect(TC.battleEnd()).toBeNull();
    let v: any = null;
    for (let k = 0; k < 2000 && !(v && v.over); k++) v = TC.battleTick(5);
    expect(v.over).toBeTruthy();
    const r0 = g.airReports.length;
    const out: any = TC.battleEnd();
    console.log(JSON.stringify({ ok: out.ok, note: out.note, report: out.report }));
    expect(out.ok).toBe(true);
    expect(g.airReports.length).toBe(r0 + 1);
    // The after card's totals (AirFeed.cpp ReadAfter reads after.lost): each side's fighters and bombers lost, the very
    // sums of the card's rows (the lead, 09:12Z).
    const rows = out.after.groups as { side: 'att' | 'def'; lostFtr: number; lostBmr: number }[];
    const sum = (side: string, k: 'lostFtr' | 'lostBmr') => rows.filter((x) => x.side === side).reduce((t, x) => t + x[k], 0);
    expect(out.after.lost).toEqual({
      att: { ftr: sum('att', 'lostFtr'), bmr: sum('att', 'lostBmr') },
      def: { ftr: sum('def', 'lostFtr'), bmr: sum('def', 'lostBmr') },
    });
    expect(rows.some((x) => x.side === 'att') && rows.some((x) => x.side === 'def')).toBe(true);
    expect(out.after.lost.att.ftr + out.after.lost.att.bmr + out.after.lost.def.ftr).toBeGreaterThan(0);
  });
  it('hands it to the staff at once', () => {
    const { TC, target, ids } = setup();
    TC.airStart('bomb', target, ids);
    expect(TC.battleOrder({ do: 'dice' }).ok).toBe(true);
    const out: any = TC.battleEnd();
    expect(out.ok).toBe(true);
    // Handed to the staff at once, the card's totals are still its rows' sums.
    const rows = out.after.groups as { side: 'att' | 'def'; lostFtr: number; lostBmr: number }[];
    for (const side of ['att', 'def'] as const) {
      expect(out.after.lost[side].ftr).toBe(rows.filter((x) => x.side === side).reduce((t, x) => t + x.lostFtr, 0));
      expect(out.after.lost[side].bmr).toBe(rows.filter((x) => x.side === side).reduce((t, x) => t + x.lostBmr, 0));
    }
  });
});
