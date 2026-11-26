// The island battle's ticket with the whole world off (release 2, 25 November 2026; the design director's ruling of
// 12:39Z, B "the island battle's ticket", Part A). On the whole-world chart the ticket the 3D game is handed for a
// battle on an island of the historian's table carries two more keys, the island's look and its own latitude and
// longitude (tests/far-island-ticket.test.ts). The old map has no such island: the islands' rows are not read at all
// there, so its tickets carry neither key and are, to the byte, the tickets they were before the island was dressed.
import { describe, expect, it, vi } from 'vitest';

// The island's two reads can be shut off for a look (stub.off): the ticket then built has no island in it at all. This
// is the same new bridge with its island answers stubbed, not the bridge as it was before the island was dressed;
// that the new lines change nothing else (no import, no draw, no write) was checked at review and again at the mend,
// by laying the map head's battle-bridge (d3acce8c) beside this one over every offer, byte for byte, and by seeds 17,
// 18 and 19.
const stub = vi.hoisted(() => ({ off: false }));
vi.mock('../src/campaign/islands', async (original) => {
  const real = await original<typeof import('../src/campaign/islands')>();
  return {
    ...real,
    islandOf: (...a: Parameters<typeof real.islandOf>) => (stub.off ? null : real.islandOf(...a)),
    islandLook: (...a: Parameters<typeof real.islandLook>) => (stub.off ? '' : real.islandLook(...a)),
  };
});

import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';
import { newCampaign, offers } from '../src/battle/ui/campaign';
import { islandLook, islandNames, islandOf } from '../src/campaign/islands';
import { FAR_STAGE } from '../src/campaign/stage';
import { orderAttack } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

type Look = { ticket: Record<string, unknown> | null; why?: string } | null;

function rig(): { g: Game; TC: ReturnType<typeof battleCalls> } {
  const g = newCampaign();
  const TC = battleCalls({ game: () => g, attack: (ids, to, f, field) => ({ report: orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f, field) }) });
  return { g, TC };
}

/** Every attack the player could order now, looked at: no island key on any ticket, and the same string with the island reads shut off. */
function everyOffer(g: Game, TC: ReturnType<typeof battleCalls>): { looked: number; tickets: number } {
  let looked = 0, tickets = 0;
  for (const o of offers(g)) {
    const ids = o.corps.map((c) => c.id), to = o.target.id;
    stub.off = false;
    const now = TC.battleLook(ids, to) as Look;
    stub.off = true;
    const before = TC.battleLook(ids, to) as Look;
    stub.off = false;
    looked++;
    // (the whole answer, the staff's "why" where it settles that kind of place included)
    expect(JSON.stringify(now), o.target.name).toBe(JSON.stringify(before));
    if (!now?.ticket) continue;
    tickets++;
    // Absent, not empty: no key at all, so nothing is written where the ticket is sent on as JSON.
    expect('look' in now.ticket, o.target.name).toBe(false);
    expect('latLon' in now.ticket, o.target.name).toBe(false);
    expect(Object.keys(now.ticket).filter((k) => k === 'look' || k === 'latLon')).toEqual([]);
    expect(JSON.stringify(now.ticket)).not.toMatch(/"(look|latLon)":/);
  }
  return { looked, tickets };
}

describe("the island battle's ticket with the whole world off", () => {
  it('no ground is an island of the table, and none has a look', () => {
    expect(FAR_STAGE).toBeFalsy();
    const { g } = rig();
    expect(islandNames()).toEqual([]);
    for (const r of g.regions) {
      expect(islandOf(g, r), r.name).toBeNull();
      expect(islandLook(g, r), r.name).toBe('');
    }
  }, 300000);

  it('every attack offered is looked at on the ticket it always had: no look, no latLon, the same bytes', () => {
    const { g, TC } = rig();
    const first = everyOffer(g, TC);
    // (Germany's first month: its attacks into Poland, all but one of them fought on the field)
    expect(first.looked).toBeGreaterThanOrEqual(5);
    expect(first.tickets).toBeGreaterThanOrEqual(5);
    // A battle is fought, so the ground is scarred: the dress the ticket has carried since release 2 still rides on it,
    // and still no island key sits beside it.
    const o = offers(g)[0];
    const to = o.target.id;
    expect(orderAttack(g, o.corps, o.target)).toBeTruthy();
    expect(g.scars?.[to]?.n).toBe(1);
    const second = everyOffer(g, TC);
    expect(second.tickets).toBeGreaterThanOrEqual(1);
    const again = offers(g).find((x) => x.target.id === to);
    if (again) {
      const look = TC.battleLook(again.corps.map((c) => c.id), to) as Look;
      if (look?.ticket) {
        expect(look.ticket.scars).toEqual({ count: 1, months: 0 });
        expect('look' in look.ticket || 'latLon' in look.ticket).toBe(false);
      }
    }
  }, 300000);
});
