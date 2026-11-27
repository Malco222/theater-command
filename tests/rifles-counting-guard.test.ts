// China's rifles, round 6: the guard on a counting-only row (the design director, 14:07Z of 26 November 2026, on round
// 5's open question): "NOT left as two readings. 06:04Z (2) said a counting-only row counts EXACTLY as a binding row,
// and different store figures from one worn fold break that. Mend to one reading (counting-only corps carry the level
// the same way) if it's small. If not, since no data row is counting-only, add a guard test that fails the moment a
// row becomes counting-only before the mend, and put it on the known list."
//
// The mend is not small: a binding row's corps carry an armed level (rifles.ts stampLevels, riflesFormed, riflesSplit,
// riflesJoin, gated on binds()) and their drafts are held to it (armWants, armShares), while a counting-only row's
// drafts are today's, uncapped, so a level stamped on its corps would stand below their strength after one month of
// drafts and no longer say the rifles they hold. Carrying it the same way means holding those drafts to the level
// (which would move that nation's war, against her answer 3 of 19:08Z, far-rifles-taps test 9) or a new rule for drafts past
// it. So until that mend is laid, no row in the data may be counting-only: this test reads every rifles row the data
// lays (rifles.json and every seats file's "rifles.json" rows, and the rows as laid with the far side on, on a live
// first day) and fails, naming the ruling, the moment one says binds: false.
//
// LIFTING THIS GUARD needs the UI director's words first (her eighth read, 13:37Z of 27 November 2026, in
// pacific-japan/ui-ask-rifles-words-2-1126.md). A counting-only row is gated two ways today, on the fold card as in the
// fold's news: with no men sent back the spare line shows ("N spare rifles go to the store."), and with men sent back
// the rifles clause is dropped ("About N men over go back to the reserve."), though the store is given those rifles.
// No player can read either form while this guard stands. Whoever lays a counting-only row asks her for the words of
// BOTH forms before lifting it; her rule will match the news's: every rifle given to the store is said, whether or not
// men are sent back.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { RIFLES, binds, carries } from '../src/campaign/rifles';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';

const RULING = "the design director's 14:07Z of 26 November 2026 (Q2): a counting-only rifles row (binds: false) waits on the mend that carries its corps' armed level the same way as a binding row's; until then no data row may be counting-only (on the known list)";

declare global {
  interface ImportMeta { glob: (pattern: string, options: { eager: true; import: 'default' }) => Record<string, unknown> }
}
/** Every data file, by its path, read whole (vite's own glob, as the test runner gives it). */
const DATA = import.meta.glob('../data/campaign/*.json', { eager: true, import: 'default' });

/** Every rifles row a data file lays, by file and nation: rifles.json's own, and each seats file's "rifles.json" rows. */
function dataRows(): { file: string; nat: string; row: Record<string, unknown> }[] {
  const out: { file: string; nat: string; row: Record<string, unknown> }[] = [];
  for (const path of Object.keys(DATA).sort()) {
    const f = path.slice(path.lastIndexOf('/') + 1), d = DATA[path] as Record<string, any> | null;
    const src = f === 'rifles.json' ? d : d && typeof d === 'object' ? d['rifles.json'] : undefined;
    const nations = src && typeof src === 'object' ? src.nations : undefined;
    if (!nations || typeof nations !== 'object') continue;
    for (const [nat, row] of Object.entries(nations)) if (row && typeof row === 'object') out.push({ file: f, nat, row: row as Record<string, unknown> });
  }
  return out;
}

describe('the guard on a counting-only rifles row (Q2, 14:07Z)', () => {
  it('no rifles row in the data says binds: false', () => {
    // (Every data file is read: rifles.json and the seats files among them.)
    expect(Object.keys(DATA).some((p) => p.endsWith('/rifles.json'))).toBe(true);
    const rows = dataRows();
    // (China's row, far-arms.json, is the one the data lays today, and it binds.)
    expect(rows.some((r) => r.file === 'far-arms.json' && r.nat.replace('+', '') === 'CHN')).toBe(true);
    for (const r of rows) expect(r.row.binds, `${r.file} ${r.nat}: ${RULING}`).not.toBe(false);
  });

  it('with the far side on, every rifles row as laid binds, and on the first day every nation that carries binds', () => {
    for (const [nat, row] of Object.entries(RIFLES.nations)) expect(row.binds, `${nat}: ${RULING}`).not.toBe(false);
    const g = startOpen(createGame(17, null));
    const carrying = Object.keys(g.nations).filter((n) => carries(g, n));
    expect(carrying).toContain('CHN');
    for (const n of carrying) expect(binds(g, n), `${n}: ${RULING}`).toBe(true);
  });
});
