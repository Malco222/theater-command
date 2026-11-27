// Scarred ground (release 2): each region keeps the land battles fought there and the month of the last, the battle
// ticket the 3D game is handed carries them, and a grand campaign's save keeps them; an older save is given them back
// from its reports.
import { describe, expect, it } from 'vitest';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { newCampaign, offers } from '../src/battle/ui/campaign';
import { orderAttack } from '../src/campaign/turn';
import { loadGame, saveGame } from '../src/campaign/save';

describe('scarred ground', () => {
  it('a battle leaves its scar, the next ticket for that place carries it, and a save keeps it', () => {
    const g = newCampaign();
    const TC = battleCalls({ game: () => g, attack: (ids, to, f, field) => ({ report: orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f, field) }) });
    const o = offers(g)[0];
    const ids = o.corps.map((c) => c.id), to = o.target.id;
    const before = TC.battleLook(ids, to) as { ticket: { scars?: unknown } | null };
    expect(before.ticket?.scars).toBeUndefined();
    const rep = orderAttack(g, o.corps, o.target);
    expect(rep).toBeTruthy();
    const fought = !(rep as { yielded?: boolean }).yielded;
    expect(g.scars?.[to]).toEqual(fought ? { n: 1, last: g.turn } : undefined);
    expect(fought).toBe(true);
    // (the same ground looked at again, by whoever can still reach it: the scars ride on the ticket)
    const again = offers(g).find((x) => x.target.id === to);
    if (again) {
      const look = TC.battleLook(again.corps.map((c) => c.id), to) as { ticket: { scars?: { count: number; months: number } } | null };
      if (look.ticket) expect(look.ticket.scars).toEqual({ count: 1, months: 0 });
    }
    // (a save and a load keep it; a save without it is given it back from its reports)
    const back = loadGame(saveGame(g));
    expect(back?.scars?.[to]?.n ?? 0).toBeGreaterThanOrEqual(1);
    const bare = JSON.parse(saveGame(g));
    delete bare.scars;
    expect(loadGame(JSON.stringify(bare))?.scars?.[to]?.n ?? 0).toBeGreaterThanOrEqual(1);
  });
});
