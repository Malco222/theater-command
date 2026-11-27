// An island's room in men, round 2's mends: the mended sentences as a player reads them on live boards through the 3D
// game's bridge (TC), each asserted whole (words sceptic 2's findings on head 4c863d9c4).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame, removeCorps, whyNoRoom } from '../src/campaign/game';
import { refusedWhole, swapHead } from '../src/campaign/islands';
import { saveGame } from '../src/campaign/save';
import { garrison } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
/** A whole-world war with a player, at his first month (seed 3). */
function war(human: string): Game {
  const g = startOpen(createGame(3, human));
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** A corps of infantry put down for the test, with its month in hand. */
function corps(g: Game, nat: string, name: string, at: Region, divs = 1, str = 1): Corps {
  const c = addCorps(g, nat, name, 'inf', divs, at.id, {});
  c.str = str;
  c.ap = c.apMax;
  return c;
}
const clear = (g: Game, r: Region): void => { for (const u of [...r.units, ...(r.garr ?? [])]) removeCorps(g, u); };
type Glob = (p: string, o: { eager: boolean; as: string }) => Record<string, string>;
/** The page's source (ui/main.ts), read to see which function its march calls. */
const PAGE = Object.values((import.meta as unknown as { glob: Glob }).glob('../src/campaign/ui/main.ts', { eager: true, as: 'raw' }))[0];
/** What the board printed, for the report: one line a sentence. */
const say = (board: string, line: string): void => { (globalThis as unknown as { process: { stderr: { write: (s: string) => void } } }).process.stderr.write(`\nLIVE ${board} | ${line}`); };

describe("round 2's mended sentences on live boards through the bridge", () => {
  it('W2-4: the Defense Battalion of 9,400 taking the field on Wake beside 6,000 Marines reads its card\'s figure', () => {
    const g = war('USA');
    const wake = g.byName['Wake'];
    clear(g, wake);
    const inside = corps(g, 'USA', 'Defense Battalion', wake, 1, 9400 / 15000);
    expect(garrison(g, inside)).toBe(true);
    corps(g, 'USA', 'Marines', wake, 1, 0.4);
    TC().load(saveGame(g));
    const w = TC().work(`field:${inside.id}`, wake.id) as { ok: boolean; note: string };
    say('W2-4 take the field', w.note);
    expect(w.ok).toBe(false);
    expect(w.note).toBe('Wake has room for one division, about 15,000 men, and 6,000 of yours are there: only 9,000 more can go, and the Defense Battalion has 9,400.');
  }, 300000);

  it('W2-1: two division groups of 15,000 refused whole onto Pearl Harbor read the island\'s line once for 30,000, the page by the same function', () => {
    const g = war('USA');
    const pearl = g.byName['Pearl Harbor'], hon = g.byName['Honolulu'];
    clear(g, pearl);
    expect(garrison(g, corps(g, 'USA', 'Inside', pearl, 1, 20000 / 15000))).toBe(true);
    const d24 = corps(g, 'USA', '24th Division Group', hon), d25 = corps(g, 'USA', '25th Division Group', hon);
    TC().load(saveGame(g));
    const m = TC().move([d24.id, d25.id], pearl.id) as { moved: { id: number }[]; note: string };
    say('W2-1 march refused whole (bridge)', m.note);
    expect(m.moved).toEqual([]);
    const want = 'Pearl Harbor has room for 2 divisions, about 30,000 men, and 20,000 of yours are there: only 10,000 more can go, and you are sending 30,000.';
    expect(m.note).toBe(want);
    // The page's sentence is the same function's (ui/main.ts marchTo calls it: the page's wiring is a reading).
    expect(refusedWhole(g, pearl, [d24, d25])).toBe(want);
    // (The page's line as it was, the first corps' alone, which said "you are sending 15,000" for the same order.)
    expect(whyNoRoom(g, d24, pearl)).toBe('Pearl Harbor has room for 2 divisions, about 30,000 men, and 20,000 of yours are there: only 10,000 more can go, and you are sending 15,000.');
    say('W2-1 march refused whole (islands.ts refusedWhole, the page)', refusedWhole(g, pearl, [d24, d25]) ?? '');
    const page = PAGE.split('\n');
    const at = page.findIndex((l) => /^export function marchTo\(|function marchTo\(/.test(l));
    expect(at).toBeGreaterThan(0);
    expect(page.slice(at, at + 40).some((l) => /refusedWhole\(g, r, /.test(l))).toBe(true);
  }, 300000);

  it('W2-2: the offer to change places onto Pearl Harbor and the swap card\'s head name both corps as the news names them', () => {
    const g = war('USA');
    const pearl = g.byName['Pearl Harbor'], hon = g.byName['Honolulu'];
    clear(g, pearl);
    const hd = corps(g, 'USA', 'Hawaiian Division', pearl, 2, 1);
    const d24 = corps(g, 'USA', '24th Division Group', hon);
    TC().load(saveGame(g));
    const m = TC().move([d24.id], pearl.id) as { moved: { id: number }[]; note: string };
    say('W2-2 the offer to change places (bridge)', m.note);
    expect(m.moved).toEqual([]);
    expect(m.note).toBe('Pearl Harbor has room for 2 divisions, about 30,000 men. Order the march again and the 24th Division Group changes places with the Hawaiian Division, which comes back to Honolulu.');
    expect(hd.reg).toBe(pearl.id);
    // The swap card's head (ui/main.ts swapCard calls it: the page's wiring is a reading).
    say('W2-2 the swap card head (islands.ts swapHead, the page)', swapHead(g, pearl, d24) ?? '');
    expect(swapHead(g, pearl, d24)).toBe('Pearl Harbor has room for 2 divisions, about 30,000 men. The 24th Division Group cannot march in beside them.');
    expect(swapHead(g, hon, d24)).toBeNull();
    const page = PAGE.split('\n');
    const at = page.findIndex((l) => /^function swapCard\(/.test(l));
    expect(page.slice(at, at + 30).some((l) => /swapHead\(g, r, c \?\? null\)/.test(l))).toBe(true);
  }, 300000);

  it("W2-3: the Eastern Marshalls' room line in the 3D game's province card names the island with its article", () => {
    const g = war('JAP');
    const em = g.byName['Eastern Marshalls'];
    TC().load(saveGame(g));
    const line = (TC().province(em.id) as { island: { line: string } }).island.line;
    say('W2-3 province card, Eastern Marshalls', line);
    expect(line).toBe('The Eastern Marshalls has room for one division, about 15,000 men.');
  }, 300000);
});
