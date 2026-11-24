// Release 1.5's battlefield looks from the campaign (Brian, 23 November 06:35Z, "yes 1 and 2 for 1.5"; from release 2):
// the weather word (wet mud), scarred ground and old ruts, captured tanks and worn uniforms ride on the battle card's
// ticket as the campaign remembers them, and the land rules tell the 3D game of each only with its Battle rules row on.
// Looks only: nothing in the war reads them, and a release 1 save (which has none of them) loads and plays on.
import { describe, expect, it } from 'vitest';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { liveTicket, ticketSky } from '../src/battle/land/live';
import { loadGame, saveGame } from '../src/campaign/save';
import { orderAttack } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const TC = (globalThis as unknown as { TC: Record<string, (...a: unknown[]) => any> }).TC;
type Look = { ticket: Record<string, any> | null; live?: Record<string, unknown> };

/** Germany, September 1939 (seed 1), its own copy of the war: XIX Corps (id 1) onto Pomorze (39). */
function setUp() {
  TC.start({ human: 'GER', from: 1, view: 'grand', seed: 1 });
  const g = loadGame(TC.save().text) as Game;
  const B = battleCalls({ game: () => g, attack: (ids, to, f, field) => ({ report: orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f, field) }) });
  const look = (): Look => B.battleLook([1], 39) as Look;
  return { g, B, look };
}

describe("release 1.5: the battlefield looks from the campaign's memory", () => {
  it('scarred ground, and old ruts from the battles fought in the mud, ride on the ticket; the weather word is the 3D game\'s', () => {
    const { g, look } = setUp();
    expect(look().ticket!.scars).toBeUndefined();
    g.scars = { 39: { n: 2, last: g.turn - 1 } };
    expect(look().ticket!.scars).toEqual({ count: 2, months: 1 });
    expect(look().ticket!.oldRuts).toBeUndefined();
    g.scars[39] = { n: 2, last: g.turn - 1, mudN: 1, mudLast: g.turn - 3 };
    expect(look().ticket!.oldRuts).toEqual({ count: 1, months: 3 });
    const w = look().ticket!.weather;
    if (w !== undefined) expect(['mud', 'snow', 'rain', 'clear']).toContain(w);
  });

  it('a fought battle scars its field; a heavy win over tanks may leave some in the winner\'s hands, for six months', () => {
    const { g, look } = setUp();
    g.captured = { GER: [{ from: 'ZZZ', n: 2, turn: g.turn - 2 }] };
    expect(look().ticket!.captured).toEqual({ att: { from: 'ZZZ', n: 2 } });
    g.captured.GER[0].turn = g.turn - 6;
    expect(look().ticket!.captured).toBeUndefined();
    orderAttack(g, [g.corps.find((c) => c.id === 1)!], g.regions[39]);
    const rep = g.reports[g.reports.length - 1];
    expect(rep.region).toBe(39);
    expect(g.scars?.[39]?.n).toBe(rep.yielded ? undefined : 1);
  });

  it("each side's months in the line ride on its side of the ticket, and the month's end counts them", () => {
    const { g, look } = setUp();
    expect(look().ticket!.att.worn).toBeUndefined();
    g.corps.find((c) => c.id === 1)!.line = 13;
    expect(look().ticket!.att.worn).toBe(3);
    // (the month's end: a corps facing the enemy counts four weeks; the save keeps them)
    TC.start({ human: 'GER', from: 1, view: 'grand', seed: 1 });
    TC.endMonth();
    const after = loadGame(TC.save().text)!;
    expect(after.corps.find((c) => c.id === 1)?.line).toBe(4);
  });

  it('the header carries each look only with its row on: wet mud, scars, old ruts, captured tanks, worn uniforms', () => {
    const { g, look } = setUp();
    g.scars = { 39: { n: 1, last: g.turn, mudN: 1, mudLast: g.turn } };
    g.captured = { GER: [{ from: 'POL', n: 1, turn: g.turn }] };
    g.corps.find((c) => c.id === 1)!.line = 9;
    const L = look();
    L.ticket!.weather = 'mud'; // (as the thaw would give it)
    const on = liveTicket(structuredClone(L.ticket) as never, { ...(L.live ?? {}), me: null } as never).header() as Record<string, any>;
    expect(on.weather).toBe('mud');
    expect(on.scars).toEqual({ count: 1, months: 0 });
    expect(on.oldRuts).toEqual({ count: 1, months: 0 });
    expect(on.sides.att.worn).toBe(2);
    expect(on.sky).toEqual(ticketSky(L.ticket as never));
    const off = liveTicket(structuredClone(L.ticket) as never, { ...(L.live ?? {}), me: null, wet: false, scars: false, mud: false, captured: false, worn: false } as never).header() as Record<string, any>;
    for (const k of ['weather', 'scars', 'oldRuts']) expect(off[k]).toBeUndefined();
    expect(off.sides.att.worn).toBeUndefined();
    expect((off.blocks as { captured?: string }[]).some((b) => b.captured)).toBe(false);
    expect(off.sky).toEqual(ticketSky({ ...L.ticket, weather: undefined } as never));
  });

  it('a 1.5 save keeps its scars and captures; a release 1 save (none of them) loads, its scars written up from its reports, and plays on', () => {
    TC.start({ human: 'SOV', from: 22, view: 'grand', seed: 3 });
    for (let i = 0; i < 3; i++) TC.endMonth();
    const text = TC.save().text as string;
    const g = loadGame(text)!;
    expect(g.flags.open).toBe(1);
    expect(Object.keys(g.scars ?? {}).length).toBeGreaterThan(0);
    g.captured = { SOV: [{ from: 'GER', n: 3, turn: g.turn }] };
    const again = loadGame(saveGame(g))!;
    expect(again.scars).toEqual(g.scars);
    expect(again.captured).toEqual(g.captured);
    // (release 1's own save: no scars, no captures, no weeks in the line)
    const d = JSON.parse(text);
    delete d.scars; delete d.captured;
    for (const c of d.corps) delete c.line;
    const old = JSON.stringify(d);
    const was = loadGame(old)!;
    const fought = new Set(was.reports.filter((r) => !r.yielded).map((r) => r.region));
    expect(Object.keys(was.scars ?? {}).map(Number).sort((a, b) => a - b)).toEqual([...fought].sort((a, b) => a - b));
    expect(was.captured).toBeUndefined();
    expect(was.corps.some((c) => c.line !== undefined)).toBe(false);
    expect(TC.load(old)).toBeTruthy();
    TC.endMonth();
    expect(loadGame(TC.save().text)!.corps.some((c) => c.line !== undefined)).toBe(true);
  });
});
