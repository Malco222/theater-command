// The staff's planned strike and a fleet the player keeps by hand (the audit's re-check of the far side's join, 25
// November 2026, item FC-09's residue). Japan with its navy under the admirals: the plan's carrier strike on Pearl
// Harbor is flown as he declares war (turn.ts orderDeclare, carriers.ts plannedStrike). Where he has taken the carrier
// fleet under his own orders the plan does not sail it out and leave it at sea: it is not flown, and he is told.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { beforeAll, describe, expect, it } from 'vitest';
import { setDesk } from '../src/campaign/depth';
import { createGame } from '../src/campaign/game';
import { forwardBase } from '../src/campaign/harbourstrike';
import { atSea } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, handBackFleet, humanUp, orderDeclare, orderRebase, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
}
const mobile = (g: Game): Fleet => g.fleets.find((f) => f.nat === 'JAP' && f.name === 'Mobile Fleet')!;
/**
 * America's Pacific fleets at anchor at Pearl Harbor, as the book's row of May 1940 puts them at stops 1 and 2. At stop 3
 * America's staff bases its own fleets (Units decide audit item 42): here they are put there, for the strike to find.
 */
function atPearl(g: Game): void {
  const sea = g.seas.findIndex((z) => z.key === 'hawaii');
  for (const f of g.fleets.filter((x) => x.nat === 'USA' && (x.name === 'Pacific Fleet' || x.name === 'Pacific Carrier Fleet'))) {
    f.base = g.byName['Pearl Harbor'].id; f.port = 'Pearl Harbor'; f.home = sea; f.zone = sea; f.station = sea; f.stance = 'port';
    delete f.passage;
  }
}
const struck = (g: Game, f: Fleet): boolean => g.airFleets.some((w) => w.ship === f.id && w.mission === 'harbour' && w.target === g.byName['Pearl Harbor'].id && w.flew === g.turn);

let base: string;
beforeAll(() => {
  // Japan under the admirals at November 1941 (month 27), its carrier fleet moved by his own order to the base from
  // which its aircraft reach Pearl Harbor.
  const g = startOpen(createGame(17, 'JAP'));
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  setDesk(g, 'navy', true);
  beginHuman(g);
  while (g.turn < 27) month(g);
  const mf = mobile(g), fb = forwardBase(g, mf, g.byName['Pearl Harbor'])!;
  expect(fb).toBeTruthy();
  expect(orderRebase(g, mf.id, fb.id)).toBe(true);
  month(g);
  base = saveGame(g);
}, 300000);

describe("the plan's strike as he declares war", () => {
  it('handed back to the admirals, the carrier fleet strikes Pearl Harbor as war is declared', () => {
    const g = loadGame(base)!;
    atPearl(g);
    expect(handBackFleet(g, mobile(g).id)).toBe(true);
    expect(orderDeclare(g, 'USA')).toBe(true);
    expect(struck(g, mobile(g))).toBe(true);
  });
  it('kept under his own orders, it is not sailed out or left at sea, and the news tells him', () => {
    const g = loadGame(base)!;
    atPearl(g);
    const mf = mobile(g), was = mf.base;
    expect(orderDeclare(g, 'USA')).toBe(true);
    expect(struck(g, mf)).toBe(false);
    expect([atSea(mf), mf.base]).toEqual([false, was]);
    expect(g.headlines.some((h) => /The staff's plan has the Mobile Fleet strike Pearl Harbor from the air as war comes\. You keep it under your own orders, so the plan's strike is not flown\./.test(h.text))).toBe(true);
  });
});
