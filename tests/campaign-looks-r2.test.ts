// Looks from the campaign (release 2): mud memory (a region's battles fought in the mud ride on the ticket as oldRuts),
// captured kit (a side's latest capture of the last six months rides on the ticket and turns that many of its tank
// blocks into the loser's model), worn uniforms (each side's months in the line) and the ground's weather word. Each
// has its own switch on the Battle rules card; off, the 3D game is told nothing of it.
import { describe, expect, it } from 'vitest';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { newCampaign, offers } from '../src/battle/ui/campaign';
import { orderAttack } from '../src/campaign/turn';
import { loadGame, saveGame } from '../src/campaign/save';
import { liveTicket } from '../src/battle/land/live';

type Look = { ticket: Record<string, any> | null; live?: Record<string, unknown> };

function setUp() {
  const g = newCampaign();
  const TC = battleCalls({ game: () => g, attack: (ids, to, f, field) => ({ report: orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f, field) }) });
  const o = offers(g)[0];
  return { g, TC, o, ids: o.corps.map((c) => c.id), to: o.target.id };
}

describe('looks from the campaign (release 2)', () => {
  it('a field fought over in the mud keeps its ruts on the ticket; dry battles leave none', () => {
    const { g, TC, ids, to } = setUp();
    g.scars = { [to]: { n: 2, last: g.turn - 1 } };
    expect((TC.battleLook(ids, to) as Look).ticket?.oldRuts).toBeUndefined();
    g.scars[to] = { n: 2, last: g.turn - 1, mudN: 1, mudLast: g.turn - 3 };
    const t = (TC.battleLook(ids, to) as Look).ticket!;
    expect(t.oldRuts).toEqual({ count: 1, months: 3 });
    expect(t.scars).toEqual({ count: 2, months: 1 });
    if (t.weather !== undefined) expect(['mud', 'snow', 'rain', 'clear']).toContain(t.weather);
  });

  it("a side's capture of the last six months rides on the ticket; an older one has gone", () => {
    const { g, TC, ids, to } = setUp();
    const t0 = (TC.battleLook(ids, to) as Look).ticket!;
    const nat = t0.att.nat as string;
    g.captured = { [nat]: [{ from: 'ZZZ', n: 2, turn: g.turn - 2 }] };
    expect((TC.battleLook(ids, to) as Look).ticket!.captured).toEqual({ att: { from: 'ZZZ', n: 2 } });
    g.captured[nat][0].turn = g.turn - 6;
    expect((TC.battleLook(ids, to) as Look).ticket!.captured).toBeUndefined();
  });

  it("each side's months in the line ride on its side of the ticket", () => {
    const { g, TC, o, ids, to } = setUp();
    for (const c of o.corps) c.line = 13;
    expect((TC.battleLook(ids, to) as Look).ticket!.att.worn).toBe(3);
  });

  it('the header carries them only with their switches on', () => {
    const { g, TC, ids, to } = setUp();
    const t0 = (TC.battleLook(ids, to) as Look).ticket!;
    g.scars = { [to]: { n: 1, last: g.turn, mudN: 1, mudLast: g.turn } };
    g.captured = { [t0.att.nat]: [{ from: t0.def.nat, n: 1, turn: g.turn }] };
    for (const c of g.corps) if (ids.includes(c.id)) c.line = 9;
    const look = TC.battleLook(ids, to) as Look;
    const on = liveTicket(look.ticket as never, { ...(look.live ?? {}), me: null } as never).header() as Record<string, any>;
    expect(on.oldRuts).toEqual({ count: 1, months: 0 });
    expect(on.scars).toEqual({ count: 1, months: 0 });
    expect(on.sides.att.worn).toBe(2);
    const arms = (on.blocks as { side: string; captured?: string }[]).filter((b) => b.side === 'att');
    if (arms.some((b) => (b as { kind?: string }).kind === 'arm')) expect(arms.filter((b) => b.captured === t0.def.nat).length).toBe(1);
    const off = liveTicket(look.ticket as never, { ...(look.live ?? {}), me: null, captured: false, worn: false, mud: false, scars: false } as never).header() as Record<string, any>;
    expect(off.oldRuts).toBeUndefined();
    expect(off.scars).toBeUndefined();
    expect(off.sides.att.worn).toBeUndefined();
    expect((off.blocks as { captured?: string }[]).some((b) => b.captured)).toBe(false);
  });

  it('a grand campaign save keeps the captures', () => {
    const { g } = setUp();
    g.flags.open = 1;
    g.captured = { GER: [{ from: 'SOV', n: 3, turn: g.turn }] };
    expect(loadGame(saveGame(g))?.captured).toEqual(g.captured);
  });
});
