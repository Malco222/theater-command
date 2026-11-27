// The view sends a corps's standing order (`posture`: stand, dig or back) only where the player knows the corps as it
// is today. An army only glimpsed, a corps not identified and a corps known from a stale report carry none. (Release 2;
// the design director, 27 November 2026 21:26Z: "the true order crossing the fog is still data that should not cross",
// with the mend "the view sends posture only where the corps is known"; the lines' keeper, the release 2 assembler,
// yes at 21:43Z.) No reader in the 3D game showed a fogged army's order: the army piece reads it only for a corps
// known today (ArmyPiece.cpp), and the order tray only for the player's own corps. This pins the data itself.
import { describe, expect, it } from 'vitest';
import PIECE from '../unreal/TheaterCommand/Source/TheaterCommand/Campaign/ArmyPiece.cpp?raw';
import SESSION from '../unreal/TheaterCommand/Source/TheaterCommand/Campaign/CampaignSession.cpp?raw';
import { allied, createGame } from '../src/campaign/game';
import { INTEL, SIGHT, fogOn, setFog, sightOf } from '../src/campaign/intel';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
type Sent = { id: number; nat: string; posture?: string; dim?: boolean; stale?: number };
/** The view as the table reads it, for the war as it stands. */
function viewOf(g: Game): { corps: Sent[] } {
  TC().load(saveGame(g));
  return JSON.parse(JSON.stringify(TC().view())) as { corps: Sent[] };
}
/** A grand campaign with Germany seated under the fog, and some foreign corps known from reports gone stale. */
function war(seed = 17): { g: Game; stale: Corps[] } {
  const g = startOpen(createGame(seed, 'GER'));
  expect(fogOn(g)).toBe(true);
  const seen = sightOf(g, g.human);
  const stale = g.corps.filter((c) => !c.dead && c.nat !== 'GER' && !allied(g, c.nat, 'GER') && seen[c.reg] >= SIGHT.full).slice(0, 6);
  g.intel.known = { ...(g.intel.known ?? {}) };
  for (const c of stale) g.intel.known[c.id] = g.turn - (INTEL.reports.fresh + 1); // (a report one month past fresh)
  return { g, stale };
}
/** The other standing order from the one the corps is under. */
function turnOrder(c: Corps): void {
  if (c.yields) { delete c.yields; c.digs = true; } else { delete c.digs; c.yields = true; }
}

describe('the view sends a standing order only for a corps known as it is today', () => {
  it('a glimpsed army, a corps not identified and a stale report carry none; a known corps carries its own', () => {
    const { g, stale } = war();
    const v = viewOf(g);
    const dim = v.corps.filter((c) => c.dim === true), old = v.corps.filter((c) => (c.stale ?? 0) > 0);
    expect(dim.length).toBeGreaterThan(10);
    expect(old.map((c) => c.id).sort()).toEqual(stale.map((c) => c.id).sort());
    for (const c of [...dim, ...old]) expect('posture' in c, `corps ${c.id}`).toBe(false);
    const known = v.corps.filter((c) => c.dim !== true && !((c.stale ?? 0) > 0));
    expect(known.length).toBeGreaterThan(10);
    for (const c of known) expect(['stand', 'dig', 'back'], `corps ${c.id}`).toContain(c.posture);
    // (his own corps under each order say so)
    const mine = g.corps.filter((c) => !c.dead && c.nat === 'GER').slice(0, 2);
    mine[0].digs = true; delete mine[0].yields;
    mine[1].yields = true; delete mine[1].digs;
    const w = viewOf(g);
    expect(w.corps.find((c) => c.id === mine[0].id)!.posture).toBe('dig');
    expect(w.corps.find((c) => c.id === mine[1].id)!.posture).toBe('back');
  });

  it('the sight test: the order of a corps he cannot know moves nothing in the picture, on two seeds', () => {
    for (const seed of [17, 3]) {
      const { g } = war(seed);
      const before = viewOf(g);
      const hidden = new Set(before.corps.filter((c) => c.dim === true || (c.stale ?? 0) > 0).map((c) => c.id));
      expect(hidden.size).toBeGreaterThan(10);
      let turned = 0;
      for (const c of g.corps) if (hidden.has(c.id)) { turnOrder(c); turned++; }
      expect(turned).toBe(hidden.size);
      expect(JSON.stringify(viewOf(g))).toBe(JSON.stringify(before));
    }
  });

  it('with the fog lifted every corps is known, and carries its order', () => {
    const { g } = war();
    setFog(g, false);
    const v = viewOf(g);
    expect(v.corps.some((c) => c.dim === true || (c.stale ?? 0) > 0)).toBe(false);
    for (const c of v.corps) expect(['stand', 'dig', 'back']).toContain(c.posture);
  });

  it('the table reads a corps with no order sent as it read it before: the two readers never asked', () => {
    // The reader takes the field where it is sent and leaves it empty where it is not.
    expect(SESSION).toContain('Unit->TryGetStringField(TEXT("posture"), C.Posture);');
    // The army piece counts a corps's order only past its own "known as it is today" test.
    const skip = PIECE.indexOf('if (Unit.bDim || Unit.Stale > 0)');
    const reads = PIECE.indexOf('Unit.Posture == TEXT("dig")');
    expect(skip).toBeGreaterThan(0);
    expect(reads).toBeGreaterThan(skip);
  });
});
