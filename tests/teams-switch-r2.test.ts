// The support teams' release 2 switch (the balance sweep thread, 25 November 2026: the coordinator's 16:49Z ask, the
// second half of Brian's "Both" for machine-gun and mortar teams). Machine-gun and mortar brigades ship on the release 2
// ship list as 'teamsrule': a grand war takes them once, as other release 2 switches are taken, and the player can turn
// them off in Options in any month. These tests hold the switch through the bridge: off is the game as it was, the ship
// list turns it on once, off stops new brigades, and brigades already raised keep going to the front.
import { describe, expect, it } from 'vitest';
import { offers } from '../src/campaign/arsenal';
import { teamsUp } from '../src/campaign/ai';
import { deliver, orderCorps } from '../src/campaign/economy';
import { createGame } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { setTeams, teamsOn } from '../src/campaign/teams';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const grand = (seed = 3, human: string | null = 'GER'): Game => startOpen(createGame(seed, human));
const teamOffers = (g: Game): string[] =>
  offers(g, 'GER', g.nations.GER.capital).map((o) => o.key).filter((k) => k === 'corps:mg' || k === 'corps:mor');

describe('the support teams switch through the bridge', () => {
  it('a war loaded without it has no teams; the ship list turns them on once', () => {
    TC().load(saveGame(grand()));
    expect((TC().view() as { supportTeams: boolean }).supportTeams).toBe(false);
    expect(TC().r2Ship(['teamsrule'])).not.toBeNull();
    expect((TC().view() as { supportTeams: boolean }).supportTeams).toBe(true);
    // Turned off afterwards, a second pass of the ship list leaves it off (the war is marked as having taken the list).
    const off = TC().act('teamsrule:off') as { ok: boolean; note: string };
    expect(off.ok).toBe(true);
    expect(off.note).toMatch(/brigades are off/);
    expect(TC().r2Ship(['teamsrule'])).toBeNull();
    expect((TC().view() as { supportTeams: boolean }).supportTeams).toBe(false);
  });

  it('Options turns them on and off in any month, and the build list follows', () => {
    const g = grand();
    TC().load(saveGame(g));
    const on = TC().act('teamsrule:on') as { ok: boolean; note: string; view: { supportTeams: boolean } };
    expect(on.ok).toBe(true);
    expect(on.note).toMatch(/brigades are on/);
    expect(on.view.supportTeams).toBe(true);
    expect((TC().supportTeams(false) as { on: boolean }).on).toBe(false);
    expect((TC().supportTeams(true) as { on: boolean }).on).toBe(true);
    // The same flag the rules read: on, the brigades are on the player's build list; off, they are not.
    const h = grand();
    setTeams(h, true);
    expect(teamOffers(h).sort()).toEqual(['corps:mg', 'corps:mor']);
    setTeams(h, false);
    expect(teamOffers(h)).toEqual([]);
  });

  it('a historical scenario cannot take them', () => {
    TC().load(saveGame(createGame(3, 'GER')));
    expect(TC().r2Ship(['teamsrule'])).toBeNull();
    expect(teamsOn(createGame(3, 'GER'))).toBe(false);
  });
});

describe('off stops new brigades, not the ones already raised', () => {
  it("a computer power's brigade raised before the switch went off still goes to the front", () => {
    const raise = (): { g: Game; id: number; at: number } => {
      const g = grand(3, 'UK');
      setTeams(g, true);
      g.nations.GER.pp = 500;
      const b = orderCorps(g, 'GER', 'mg', g.nations.GER.capital)!;
      b.eta = g.turn;
      deliver(g, 'GER');
      const c = g.corps.find((x) => x.nat === 'GER' && x.kind === 'mg' && !x.dead)!;
      c.ap = Math.max(c.ap, 1);
      return { g, id: c.id, at: g.regions.findIndex((r) => r.units.includes(c)) };
    };
    const where = (g: Game, id: number): number => g.regions.findIndex((r) => r.units.some((u) => u.id === id));
    const a = raise(), b = raise();
    setTeams(b.g, false);
    expect(teamsOn(b.g)).toBe(false);
    teamsUp(a.g, 'GER', new Set(['POL']), null);
    teamsUp(b.g, 'GER', new Set(['POL']), null);
    expect(where(a.g, a.id)).not.toBe(a.at);
    expect(where(b.g, b.id)).toBe(where(a.g, a.id));
    // And no new one can be ordered while it is off.
    b.g.nations.GER.pp = 500;
    expect(orderCorps(b.g, 'GER', 'mor', b.g.nations.GER.capital)).toBeNull();
  });
});
