// An island's room in men, round 2's mends: the findings of rules sceptic 2 and words sceptic 2 on head 4c863d9c4
// that needed no ruling, each held by a test that fails without its mend. (src/campaign/islands.ts and its readers.)
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame, men, removeCorps, setWar } from '../src/campaign/game';
import { fits, islandOf } from '../src/campaign/islands';
import { rearMonth, startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

/** A corps of infantry put down for the test, with its month in hand. */
function corps(g: Game, nat: string, name: string, at: Region, divs = 1, str = 1, kind: any = 'inf'): Corps {
  const c = addCorps(g, nat, name, kind, divs, at.id, {});
  c.str = str;
  c.ap = c.apMax;
  return c;
}
const clear = (g: Game, r: Region): void => { for (const u of [...r.units, ...(r.garr ?? [])]) removeCorps(g, u); };

describe("a rear guard raised on an island asks whether that corps fits, the five among it (rules sceptic 2, S2R-1; the design director, 00:20Z, 27 November 2026, D5 and D6)", () => {
  it('Rabaul, taken from Australia and holding five small Japanese corps, gets no sixth: its 1,500-man base force does not fit', () => {
    const g = startOpen(createGame(3, null));
    const rb = g.byName['Rabaul'];
    expect(islandOf(g, rb)?.room).toBe(4);
    clear(g, rb);
    setWar(g, 'JAP', 'AUS');
    rb.ctrl = 'JAP'; rb.prev = 'AUS'; g.turn = 30; rb.since = 29;
    for (let k = 0; k < 5; k++) corps(g, 'JAP', `Small ${k}`, rb, 1, 0.2);
    expect(fits(g, rb, 'JAP', [{ nat: 'JAP', kind: 'inf', divs: 1, str: 0.1 } as Corps])).toBe(false);
    rearMonth(g, 'JAP');
    expect(rb.units.filter((u) => u.nat === 'JAP').map((u) => `${u.name} ${men(u)}`)).toHaveLength(5);
    // With four there the base force comes, as before.
    removeCorps(g, rb.units.find((u) => u.name === 'Small 4')!);
    rearMonth(g, 'JAP');
    expect(rb.units.filter((u) => u.nat === 'JAP').map((u) => `${u.name} ${men(u)}`)).toContain('8th Base Force (Rabaul) 1500');
  });
});

type Glob = (p: string, o: { eager: boolean; as: string }) => Record<string, string>;
const SOURCES = (import.meta as unknown as { glob: Glob }).glob('../src/**/*.ts', { eager: true, as: 'raw' });

describe("a check with no corps at hand is named for what it asks, room for a division, never 'full' (rules sceptic 2, S2R-6; words sceptic 2, W2-6; the design director, 00:20Z, 27 November 2026, D6)", () => {
  it("no line of code or comment within six lines before a roomForDivision call calls the room 'full'", () => {
    const hits: string[] = [];
    for (const [file, text] of Object.entries(SOURCES)) {
      if (file.endsWith('/islands.ts')) continue;
      const lines = text.split('\n');
      lines.forEach((l, i) => {
        if (!/roomForDivision\(/.test(l)) return;
        for (let k = Math.max(0, i - 6); k <= i; k++) if (/\bfull\b(?! strength)/.test(lines[k])) hits.push(`${file.replace(/^.*\/src\//, 'src/')}:${k + 1}`);
      });
    }
    expect([...new Set(hits)]).toEqual([]);
  });
});
