// Old wrecks (release 2, looks only; Brian 23 November 20:09Z): each region keeps the armoured vehicles its land battles
// left burnt out on the field, by nation, and the month of the last; the battle ticket carries them (both sides, at most
// 60, each nation's share), the live header sends them only with the "Old wrecks" switch on, and a grand campaign's save
// keeps them; an older save is given them back from its reports.
import { describe, expect, it } from 'vitest';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { newCampaign, offers } from '../src/battle/ui/campaign';
import { orderAttack } from '../src/campaign/turn';
import { loadGame, saveGame } from '../src/campaign/save';
import { liveTicket } from '../src/battle/land/live';
import { WRECKS, wreckBump, wreckLook } from '../src/campaign/game';
import type { BattleReport, ReportUnit } from '../src/campaign/types';

type Look = { ticket: Record<string, any> | null; live?: Record<string, unknown> };

function setUp() {
  const g = newCampaign();
  const TC = battleCalls({ game: () => g, attack: (ids, to, f, field) => ({ report: orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f, field) }) });
  const o = offers(g)[0];
  return { g, TC, o, ids: o.corps.map((c) => c.id), to: o.target.id };
}

const unit = (nat: string, kind: ReportUnit['kind'], divs: number, men: number, lost: number): ReportUnit => ({ name: `${nat} ${kind}`, nat, kind, divs, men, lost, fate: '' });
const report = (level: BattleReport['level'], attackers: ReportUnit[], defenders: ReportUnit[], region = 5, turn = 10) =>
  ({ id: 1, turn, week: 0, region, regionName: 'Here', attNat: attackers[0].nat, defNat: defenders[0].nat, attackers, defenders, lines: [], aPower: 1, dPower: 1, odds: 1, level }) as unknown as BattleReport;

describe('old wrecks (release 2)', () => {
  it("a battle's armour losses are left on its field: the side that holds it tows half of its own away", () => {
    const { g } = setUp();
    delete g.wrecks;
    const here = () => g.wrecks?.[5];
    // GER attacks with 2 armoured divisions and loses a fifth of its men; SOV defends with 1 armoured and 1 motorised
    // division, loses 30%, and gives the field up.
    wreckBump(g, report('Decisive victory', [unit('GER', 'arm', 2, 30000, 6000), unit('GER', 'inf', 3, 40000, 8000)], [unit('SOV', 'arm', 1, 12000, 3600), unit('SOV', 'mot', 1, 10000, 3000)]));
    const per = WRECKS.per as Record<string, number>;
    expect(here()).toEqual({
      n: { GER: Math.round(per.arm * 2 * 0.2 * (1 - WRECKS.towed)), SOV: Math.round(per.arm * 0.3 + per.mot * 0.3) },
      last: 10,
    });
    // (infantry leaves none; a walk-in leaves none; the next battle adds to it and moves the month on)
    wreckBump(g, { ...report('Close defeat', [unit('GER', 'inf', 3, 40000, 8000)], [unit('SOV', 'inf', 2, 20000, 2000)]) });
    expect(here()?.last).toBe(10);
    wreckBump(g, { ...report('Close defeat', [unit('GER', 'arm', 1, 15000, 1500)], [unit('SOV', 'arm', 1, 12000, 1200)], 5, 12), yielded: true });
    expect(here()?.last).toBe(10);
    wreckBump(g, report('Close defeat', [unit('GER', 'arm', 1, 15000, 1500)], [unit('SOV', 'arm', 1, 12000, 1200)], 5, 12));
    expect(here()?.last).toBe(12);
    expect(here()?.n.GER).toBe(30 + Math.round(per.arm * 0.1));
  });

  it('the ticket shows at most 60, shared out so the nations add up to the count', () => {
    expect(wreckLook(undefined, 10)).toBeNull();
    expect(wreckLook({ n: { GER: 0 }, last: 3 }, 10)).toBeNull();
    expect(wreckLook({ n: { GER: 12, SOV: 7 }, last: 8 }, 10)).toEqual({ count: 19, months: 2, nats: { GER: 12, SOV: 7 } });
    const big = wreckLook({ n: { GER: 100, SOV: 130, ITA: 3 }, last: 10 }, 10)!;
    expect(big.count).toBe(WRECKS.most);
    expect(Object.values(big.nats).reduce((a, b) => a + b, 0)).toBe(big.count);
    expect(big.nats.SOV).toBeGreaterThan(big.nats.GER);
    expect(big.months).toBe(0);
  });

  it('a real battle with armour in it books its wrecks; the next ticket for that place carries them', () => {
    const { g, TC, o, ids, to } = setUp();
    expect((TC.battleLook(ids, to) as Look).ticket?.oldWrecks).toBeUndefined();
    const rep = orderAttack(g, o.corps, o.target)!;
    const armour = [...rep.attackers, ...rep.defenders].some((u) => (u.kind === 'arm' || u.kind === 'mot') && u.lost > 0);
    if (!armour || rep.yielded) expect(g.wrecks?.[to]).toBeUndefined();
    else expect(Object.values(g.wrecks?.[to]?.n ?? {}).reduce((a, b) => a + b, 0)).toBeGreaterThan(0);
    g.wrecks = { [to]: { n: { GER: 9, SOV: 4 }, last: g.turn - 2 } };
    const look = TC.battleLook(ids, to) as Look;
    if (look?.ticket) expect(look.ticket.oldWrecks).toEqual({ count: 13, months: 2, nats: { GER: 9, SOV: 4 } });
  });

  it('the header carries them only with the Old wrecks switch on', () => {
    const { g, TC, ids, to } = setUp();
    g.wrecks = { [to]: { n: { GER: 5, SOV: 2 }, last: g.turn - 1 } };
    const look = TC.battleLook(ids, to) as Look;
    const on = liveTicket(look.ticket as never, { ...(look.live ?? {}), me: null } as never).header() as Record<string, any>;
    const { per, ...rest } = on.oldWrecks as { per?: number };
    expect(rest).toEqual({ count: 7, months: 1, nats: { GER: 5, SOV: 2 } });
    // (per: the real armoured vehicles one tank model stands for in this battle, both sides' armour together; absent
    // where the battle draws no tanks)
    const tanks = (on.blocks as { kind: string; figures0: number }[]).filter((b) => b.kind === 'arm');
    if (!tanks.length) expect(per).toBeUndefined();
    else {
      expect(per).toBeGreaterThanOrEqual(1);
      expect(Math.round(per! * 10)).toBe(per! * 10);
    }
    const off = liveTicket(look.ticket as never, { ...(look.live ?? {}), me: null, wrecks: false } as never).header() as Record<string, any>;
    expect(off.oldWrecks).toBeUndefined();
  });

  it("one tank model's real vehicles: the armour's divisions by strength over the tank models drawn for them", () => {
    const { g, TC, o, ids, to } = setUp();
    o.corps[0].kind = 'arm'; // (the opening's first offer is all foot: one corps of it made armour, cut into regiments)
    g.wrecks = { [to]: { n: { GER: 5 }, last: g.turn } };
    const look = TC.battleLook(ids, to) as Look;
    const live = liveTicket(look.ticket as never, { ...(look.live ?? {}), me: null } as never);
    const units = (live as unknown as { sim: { allBlocks(): { kind: string; figures0: number; worth?: number; tray: number }[] } }).sim.allBlocks().filter((u) => u.kind === 'arm');
    const head = live.header() as Record<string, any>;
    expect(units.length).toBeGreaterThan(0);
    const real = units.reduce((s, u) => s + 150 * (u.worth ?? u.figures0 / u.tray), 0), models = units.reduce((s, u) => s + u.figures0, 0);
    expect(head.oldWrecks.per).toBe(Math.max(1, Math.round((real / models) * 10) / 10));
  });

  it('a grand campaign save keeps them; a save without them is given them back from its reports', () => {
    const { g } = setUp();
    g.flags.open = 1;
    g.wrecks = { 7: { n: { GER: 3 }, last: g.turn } };
    expect(loadGame(saveGame(g))?.wrecks).toEqual(g.wrecks);
    g.reports.push(report('Decisive victory', [unit('GER', 'arm', 1, 15000, 3000)], [unit('FRA', 'arm', 1, 12000, 6000)], 9, g.turn));
    const bare = JSON.parse(saveGame(g));
    delete bare.wrecks;
    expect(loadGame(JSON.stringify(bare))?.wrecks?.[9]?.n).toEqual({ GER: Math.round(150 * 0.2 * 0.5), FRA: 75 });
  });

  it('saved wrecks are not counted again from the reports at each load, with sea mines or without (the assembler, 24 November)', () => {
    // (Navy round 4 set the mines' line between the wrecks' if and its else: a save with wrecks and no mines then
    // added every kept report's wrecks on top of the saved ones at every load, and one with mines and no wrecks
    // never got its wrecks back.)
    const { g } = setUp();
    g.flags.open = 1;
    g.reports.push(report('Decisive victory', [unit('GER', 'arm', 1, 15000, 3000)], [unit('FRA', 'arm', 1, 12000, 6000)], 9, g.turn));
    g.wrecks = { 9: { n: { GER: 15, FRA: 75 }, last: g.turn } };
    delete g.mines;
    const once = loadGame(saveGame(g))!;
    expect(once.wrecks).toEqual(g.wrecks);
    const twice = loadGame(saveGame(once))!;
    expect(twice.wrecks).toEqual(g.wrecks);
    // With mines laid the wrecks are kept the same way, and the mines come back as saved.
    const mined = JSON.parse(saveGame(g));
    mined.mines = [];
    expect(loadGame(JSON.stringify(mined))?.wrecks).toEqual(g.wrecks);
    expect(loadGame(JSON.stringify(mined))?.mines).toEqual([]);
    // And a save with mines but no wrecks is still given its wrecks back from its reports.
    delete mined.wrecks;
    expect(loadGame(JSON.stringify(mined))?.wrecks?.[9]?.n).toEqual({ GER: Math.round(150 * 0.2 * 0.5), FRA: 75 });
  });
});
