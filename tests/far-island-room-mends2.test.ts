// An island's room in men, round 2's mends: the findings of rules sceptic 2 and words sceptic 2 on head 4c863d9c4
// that needed no ruling, each held by a test that fails without its mend. (src/campaign/islands.ts and its readers.)
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame, men, removeCorps, setWar, startPhase } from '../src/campaign/game';
import { atWar } from '../src/campaign/game';
import setup from '../data/campaign/setup-1939.json';
import { fits, islandOf, roomRefusal } from '../src/campaign/islands';
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

describe("a row's months waited for room count against its half year only from its current due month (rules sceptic 2, S2R-7; the design director, 00:20Z, 27 November 2026, D1)", () => {
  it('the 61st Guard Unit waits four months for room, then waits on a posting held in peace: the room months before that wait are not carried past it', () => {
    const ROWS = (setup as unknown as { expeditions: any[][] }).expeditions;
    expect([ROWS[263][0], ROWS[263][5]]).toEqual([27, 'Kwajalein']);
    const g = startOpen(createGame(3, null));
    const k = g.byName['Kwajalein'];
    addCorps(g, 'JAP', 'A full division (a test row)', 'inf', 1, k.id, {});
    const one = { nat: 'JAP', kind: 'inf', divs: 1, str: 0.07 } as unknown as Corps;
    const filled = (r: Region): boolean => !fits(g, r, 'JAP', [one], { rows: r.units.filter((u) => u.kind !== 'eng').length });
    for (const r of g.regions) if (r.port && r !== k && r.ctrl === 'JAP') for (let n = 0; n < 8 && !filled(r); n++) addCorps(g, 'JAP', `Filler ${r.id}.${n} (a test row)`, 'inf', 1, r.id, {});
    for (let t = 24; t <= 30; t++) { g.turn = t; startPhase(g, 'JAP'); }
    expect(g.flags['roomwait:263']).toBe(4);
    // T31: Kwajalein held by America, at peace with Japan: the row waits for that war (ruling B7), its due moved to T32.
    expect(atWar(g, 'JAP', 'USA')).toBe(false);
    const owner = k.owner;
    k.ctrl = 'USA'; k.owner = 'USA';
    g.turn = 31; startPhase(g, 'JAP');
    expect(g.flags['wait:263']).toBe(31);
    // The four months it waited for room lie before its new due month: none of them is taken off its new half year.
    expect(g.flags['roomwait:263'] ?? 0).toBe(0);
    // Japan holds it again with no room: from T32 each month waited for room counts again.
    k.ctrl = 'JAP'; k.owner = owner;
    g.turn = 32; startPhase(g, 'JAP');
    expect(g.flags['sent:263']).toBeUndefined();
    expect(g.flags['roomwait:263']).toBe(1);
  });
});

describe("a corps named in an island's line shows its card's own figure, never a shortfall (words sceptic 2, W2-4; the UI director, 21:37Z, 26 November 2026, 2, and 00:19Z, 27 November 2026, U7)", () => {
  it('Wake, 6,000 Marines in the field: the Defense Battalion of 9,400 coming out of the town reads its 9,400 against the 9,000 that can go', () => {
    const g = startOpen(createGame(3, null));
    const wake = g.byName['Wake'], hon = g.byName['Honolulu'];
    clear(g, wake);
    wake.ctrl = 'USA';
    corps(g, 'USA', 'Marines', wake, 1, 0.4);
    const bn = corps(g, 'USA', 'Defense Battalion', hon, 1, 9400 / 15000);
    expect(men(bn)).toBe(9400);
    expect(roomRefusal(g, wake, [bn], { named: true })).toBe('Wake has room for one division, about 15,000 men, and 6,000 of yours are there: only 9,000 more can go, and the Defense Battalion has 9,400.');
    // Unnamed, what he sends keeps the shortfall her answer gives "you are sending".
    expect(roomRefusal(g, wake, [bn])).toBe('Wake has room for one division, about 15,000 men, and 6,000 of yours are there: you are sending 400 men more than fit.');
  });
});
