// The supply window (release 2; the map-speed lane's finding of 24 November 2026). A step that ended with a province
// taken, a fleet sailed or a bridge blown left the supply picture stale until whoever read it next drew it afresh. On
// the 3D table that reader could be a picture (the glance's guard marks walk an enemy's reach, which asks the long
// supply line), so when a picture was drawn decided when supply was refreshed, and two tables given the same orders
// could play different wars. Supply is now clean whenever the rules hand control back: at the end of every nation's
// step, as the player's month opens and closes, after his standing marches, and at the end of every call the table
// makes. A picture never finds it stale, so a picture never refreshes it.
import { describe, expect, it } from 'vitest';
import { createGame, keepSupply } from '../src/campaign/game';
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

  // (The design director's ruling of 25 November 2026, 02:55Z. A fortified port overseas keeps the month its sea was
  // last found open, and feeds its garrison from its stores for a while after. Its sea is read whenever supply is
  // drawn, and supply is now drawn at the close of a step that left it stale: open then means supplied that month,
  // and the mark carries the month as it stands at the close. Before, the mark waited for the next step's opening.)
  it('a fortified port overseas has its sea read at the close of a step: open then, it is marked for that month', () => {
    const g = startOpen(createGame(3, 'GER'));
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    beginHuman(g);
    const mark = (id: number): number | undefined => g.flags[`stores:${id}`];
    // The ports whose sea the month's opening found open.
    const open = g.regions.filter((r) => r.overseas && r.port && r.fort && mark(r.id) === g.turn);
    expect(open.length).toBeGreaterThan(0);
    // Their marks wound back, as if the sea had last been open two months ago, and supply left stale as an order leaves it.
    for (const r of open) g.flags[`stores:${r.id}`] = g.turn - 2;
    g.supplyDirty = true;
    endHuman(g);
    for (const r of open) expect(mark(r.id), r.name).toBe(g.turn);
    // A step that left supply fresh draws nothing at its close, and the marks stand as its last drawing left them.
    for (const r of open) g.flags[`stores:${r.id}`] = g.turn - 2;
    advanceMarches(g);
    for (const r of open) expect(mark(r.id), r.name).toBe(g.turn - 2);
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

  it('a month through the table ends fresh, and looking on the way changes nothing', () => {
    // A saved war whose supply is stale, as an order leaves it. (Before the supply window a look at a province's card
    // drew supply afresh here and the quiet table did not: the two saves parted at once.)
    const g = germany();
    g.supplyDirty = true;
    const text = saveGame(g);
    // The same month twice: once with a picture and a province's card asked for at every chance, once with none.
    const play = (looking: boolean): string => {
      TC().load(text);
      if (looking) { TC().view(); TC().province(36); }
      const opened = (TC().save() as { text: string }).text;
      TC().endMonth();
      if (looking) { TC().view(); TC().province(36); }
      expect(saved().dirty).toBe(false);
      TC().endMonth();
      return `${opened}\n${(TC().save() as { text: string }).text}`;
    };
    const quiet = play(false), looked = play(true);
    expect(looked).toBe(quiet);
  });

  it('a call that fails hands back fresh supply too, and reports its own error', () => {
    const g = startOpen(createGame(3, null));
    // An order that takes ground, say, and then fails: supply is left stale behind it.
    const failing = keepSupply(() => { g.supplyDirty = true; throw new Error('the order failed'); }, () => g);
    expect(failing.name).toBe('supplyKept');
    expect(() => failing()).toThrow('the order failed');
    expect(g.supplyDirty).toBe(false);
    // And a call opens on fresh supply, whatever was done to the game from outside the table.
    g.supplyDirty = true;
    let found: boolean | null = null;
    const out = keepSupply((a) => { found = g.supplyDirty; return a; }, () => g)(7);
    expect(found).toBe(false);
    expect(out).toBe(7);
    // (No game on the table yet: the call is made as it is.)
    expect(keepSupply(() => 'no game', () => null)()).toBe('no game');
  });
});
