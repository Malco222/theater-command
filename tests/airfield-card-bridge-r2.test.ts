// The 3D table's airfield card and row (release 2; Brian 24 November 2026: "its hard to click on an airfield"; the art
// director's spec, item 2): where airfields are targets, each field the player knows says whether its runway is shut,
// whether a repair is in hand, and whether it is a strip or an airfield. Off, the bridge says none of it.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, setAirfieldTargets, stepAI } from '../src/campaign/turn';
import { damageField } from '../src/campaign/fieldstate';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface ViewField { region: number; side: string; groups: number; shut?: boolean; mend?: boolean; tier?: string }
const fields = (): ViewField[] => (TC().view() as { airfields: ViewField[] }).airfields;

function germany(on: boolean): Game {
  const g = startOpen(createGame(3, 'GER'));
  let guard = 0;
  while (!humanUp(g) && guard++ < 200) stepAI(g);
  beginHuman(g);
  setAirfieldTargets(g, on);
  return g;
}

describe('the airfield card through the bridge', () => {
  it('off: no shut, repair or tier is said', () => {
    const g = germany(false);
    TC().load(saveGame(g));
    const own = fields().filter((f) => f.side === 'own');
    expect(own.length).toBeGreaterThan(0);
    for (const f of own) {
      expect(f.shut).toBeUndefined();
      expect(f.tier).toBeUndefined();
    }
  });

  it('on: an own field an agent shut says shut and repairing, and every own field its tier', () => {
    const g = germany(true);
    TC().load(saveGame(g));
    const own = fields().filter((f) => f.side === 'own');
    expect(own.length).toBeGreaterThan(0);
    for (const f of own) {
      expect(f.shut).toBe(false);
      expect(['strip', 'full']).toContain(f.tier);
    }
    const hit = own.map((f) => f.region).find((r) => damageField(g, r, 'agent') !== null);
    expect(hit).toBeDefined();
    TC().load(saveGame(g));
    const after = fields().find((f) => f.region === hit)!;
    expect(after.shut).toBe(true);
    expect(after.mend).toBe(true);
  });
});
