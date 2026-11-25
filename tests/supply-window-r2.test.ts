// The supply window (release 2; the map-speed lane's finding of 24 November 2026). A step that ended with a province
// taken, a fleet sailed or a bridge blown left the supply picture stale until whoever read it next drew it afresh. On
// the 3D table that reader could be a picture (the glance's guard marks walk an enemy's reach, which asks the long
// supply line), so when a picture was drawn decided when supply was refreshed, and two tables given the same orders
// could play different wars. Supply is now clean whenever the rules hand control back: at the end of every nation's
// step, as the player's month opens and closes, after his standing marches, and at the end of every call the table
// makes. A picture never finds it stale, so a picture never refreshes it.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { advanceMarches } from '../src/campaign/marches';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const saved = (): { dirty?: boolean; turn: number } => JSON.parse((TC().save() as { text: string }).text);

describe('the supply window: the rules hand the month on with supply fresh', () => {
  it('a war nobody plays: no nation ends its step with supply stale, though most steps make it stale on the way', () => {
    const g = startOpen(createGame(3, null));
    let steps = 0, dirtied = 0;
    // (Watch the mark itself: how often a step dirtied it before the step's end made it fresh.)
    let v = g.supplyDirty;
    Object.defineProperty(g, 'supplyDirty', { enumerable: true, configurable: true, get: () => v, set: (x: boolean) => { if (x && !v) dirtied++; v = x; } });
    while (g.turn < 9) {
      stepAI(g);
      steps++;
      expect(g.supplyDirty, `after step ${steps}, month ${g.turn}`).toBe(false);
    }
    expect(steps).toBeGreaterThan(50);
    // The test would mean nothing if no step ever made supply stale.
    expect(dirtied).toBeGreaterThan(10);
  });

  it('the player\'s month opens and closes on fresh supply, and so do his standing marches', () => {
    const g = startOpen(createGame(3, 'GER'));
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    g.supplyDirty = true;
    beginHuman(g);
    expect(g.supplyDirty).toBe(false);
    g.supplyDirty = true;
    advanceMarches(g);
    // (With no standing march there is nothing to do and nothing is touched: the month's own opening already made it fresh.)
    expect(g.supplyDirty).toBe(true);
    endHuman(g);
    expect(g.supplyDirty).toBe(false);
  });
});

describe('the supply window through the table', () => {
  const germany = (): Game => {
    const g = startOpen(createGame(3, 'GER'));
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    beginHuman(g);
    return g;
  };

  it('a saved war whose supply was stale is fresh as soon as the table has loaded it', () => {
    const g = germany();
    g.supplyDirty = true;
    const text = saveGame(g);
    // The rules' own load keeps the mark as it was saved: the table's call is what makes it fresh.
    expect(loadGame(text)!.supplyDirty).toBe(true);
    TC().load(text);
    expect(saved().dirty).toBe(false);
  });

  it('every call on the table is wrapped, the looks as well as the orders', () => {
    const table = TC();
    const names = Object.keys(table).filter((k) => typeof table[k] === 'function');
    expect(names.length).toBeGreaterThan(100);
    for (const k of names) expect(table[k].name, k).toBe('supplyKept');
  });

  it('a month through the table ends fresh, and looking at pictures on the way changes nothing', () => {
    const text = saveGame(germany());
    // The same month twice: once with a picture and the looks asked for at every chance, once with none.
    const play = (looking: boolean): string => {
      TC().load(text);
      if (looking) { TC().view(); TC().view(); }
      TC().endMonth();
      if (looking) TC().view();
      expect(saved().dirty).toBe(false);
      TC().endMonth();
      return (TC().save() as { text: string }).text;
    };
    const quiet = play(false), looked = play(true);
    expect(looked).toBe(quiet);
  });
});
