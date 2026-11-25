// A fleet the player keeps by hand on the whole-world chart (the navy thread's read of the far side's join, 24 November
// 2026). Under the admirals (Theatre command) the book's dated rows move a navy's bases: America's Pacific Fleet goes
// from San Diego to Pearl Harbor in May 1940. A fleet the player has ordered himself is his until he hands it back
// (1.5.1, Brian 24 November 18:32Z: "they dont stay where they were moved"), so no book row moves it either. The move
// waits for him and the news says so (the audit's re-check, 25 November 2026): handed back, the fleet goes there with
// the admirals' next month; with the navy his own, its page offers the move.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { setDesk } from '../src/campaign/depth';
import { createGame } from '../src/campaign/game';
import { baseTargets, bookBaseOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { setStop } from '../src/campaign/history';
import { beginHuman, endHuman, handBackFleet, humanUp, orderRebase, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type TCApi = { load: (t: string) => unknown; act: (a: string) => { ok: boolean; note: string }; page: (id: string) => { sections: { heading: string; rows: { title: string; lines: string[]; buttons: { label: string; act: string; ok: boolean }[] }[] }[] } | null; save: () => { text: string } };
const TC = (globalThis as unknown as { TC: TCApi }).TC;

function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
}
/** America's campaign under the admirals (or with the navy his own), at the player's month before the book's move (May 1940 is month 9). */
function america(staff = true): { g: Game; pf: Fleet } {
  const g = startOpen(createGame(3, 'USA'));
  // (The book's dated base moves run at stops 1 and 2 only: Units decide audit item 42.)
  setStop(g, 2, true);
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  setDesk(g, 'navy', staff);
  beginHuman(g);
  while (g.turn < 8) month(g);
  return { g, pf: g.fleets.find((f) => f.nat === 'USA' && f.name === 'Pacific Fleet')! };
}
const at = (g: Game, f: Fleet): string => g.regions[f.base].name;

describe("the book's base moves and the player's own orders", () => {
  it('under the admirals, a fleet he has not ordered goes where the book sends it', () => {
    const { g, pf } = america();
    expect(at(g, pf)).toBe('San Diego');
    month(g); month(g);
    expect(at(g, pf)).toBe('Pearl Harbor');
  }, 300000);
  it('a fleet he has moved himself stays where he sent it', () => {
    const { g, pf } = america();
    const to = baseTargets(g, pf).find((r) => r.name === 'San Francisco')!;
    expect(orderRebase(g, pf.id, to.id)).toBe(true);
    month(g); month(g);
    expect(at(g, pf)).toBe('San Francisco');
    expect(g.headlines.some((h) => h.text === 'The Pacific Fleet is ordered to stay at Pearl Harbor.')).toBe(false);
    // (The move waits, and the news tells him so.)
    expect(bookBaseOf(g, pf)?.region.name).toBe('Pearl Harbor');
    expect(g.headlines.some((h) => /ordered to stay at Pearl Harbor, by the staff's plan\. You keep the Pacific Fleet under your own orders, so it stays at San Francisco: hand it back/.test(h.text))).toBe(true);
    // (Saved and loaded, it still waits; the fleet page says so, with the move.)
    TC.load(saveGame(g));
    const row = TC.page(`fleet:${pf.id}`)!.sections.find((x) => x.heading === "THE STAFF'S PLAN")!.rows[0];
    expect(row.title).toBe('Pearl Harbor');
    expect(row.lines.join(' ')).toMatch(/hand it back to your admirals and it goes there/);
    expect(row.buttons[0].label).toBe('MOVE BASE TO PEARL HARBOR');
    // (Handed back, it goes there with the admirals' next month, in the book's words.)
    const h = loadGame(saveGame(g))!;
    const pf2 = h.fleets.find((f) => f.id === pf.id)!;
    expect(handBackFleet(h, pf.id)).toBe(true);
    month(h);
    expect(at(h, pf2)).toBe('Pearl Harbor');
    expect(bookBaseOf(h, pf2)).toBeNull();
    expect(h.headlines.some((x) => x.text === 'The Pacific Fleet is ordered to stay at Pearl Harbor.')).toBe(true);
    // (The dial turned past stop 2 before he hands it back: the move is the book's, so it is dropped. The fourth review, 6.)
    const k = loadGame(saveGame(g))!;
    const pf3 = k.fleets.find((f) => f.id === pf.id)!;
    setStop(k, 3, true);
    expect(handBackFleet(k, pf.id)).toBe(true);
    month(k);
    expect(at(k, pf3)).not.toBe('Pearl Harbor');
    expect(bookBaseOf(k, pf3)).toBeNull();
    expect(k.headlines.some((x) => x.text === 'The Pacific Fleet is ordered to stay at Pearl Harbor.')).toBe(false);
  }, 300000);
  it('with the navy his own, the news tells him of the move and the page makes it in one order', () => {
    const { g, pf } = america(false);
    month(g); month(g);
    expect(at(g, pf)).toBe('San Diego');
    expect(g.headlines.some((h) => /by the staff's plan\. You run the navy yourself: the Pacific Fleet stays at San Diego until you move its base to Pearl Harbor/.test(h.text))).toBe(true);
    TC.load(saveGame(g));
    const row = TC.page(`fleet:${pf.id}`)!.sections.find((x) => x.heading === "THE STAFF'S PLAN")!.rows[0];
    expect(row.lines.join(' ')).toMatch(/the move is yours to make/);
    const r = TC.act(row.buttons[0].act);
    expect([r.ok, r.note]).toEqual([true, 'Its new home port is Pearl Harbor.']);
    const after = loadGame(TC.save().text)!;
    const f = after.fleets.find((x) => x.id === pf.id)!;
    expect(at(after, f)).toBe('Pearl Harbor');
    expect(bookBaseOf(after, f)).toBeNull();
    expect(TC.page(`fleet:${pf.id}`)!.sections.some((x) => x.heading === "THE STAFF'S PLAN")).toBe(false);
  }, 300000);
});
