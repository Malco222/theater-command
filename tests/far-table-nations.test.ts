// The 3D table's own list of countries against the rules', on the whole world (release 2, the far side; the art
// director, 25 November 2026). See tests/table-nations.ts for why the table keeps a list of its own. The far side is
// where the two came apart once: the rules laid fourteen countries the table had never heard of, and their ground
// was grey. The table now reads their rows from the far side's files and stands Tibet in as the rules do.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { type BookRow, tableAgainstBook, tableNations, tableStandIns } from './table-nations';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const FAR = ['JAP', 'CHN', 'MAN', 'MGL', 'THA', 'AUS', 'NZL', 'SAF', 'SAU', 'YEM', 'AFG', 'NEP', 'LBR'];

describe('the 3D table knows every country the rules know on the whole world', () => {
  it('every country of the rules\' first day is the table\'s, by the same name and in the same colour', () => {
    const book = TC().nationsBook(true) as BookRow[];
    for (const tag of [...FAR, 'TIB']) expect(book.some((n) => n.tag === tag), tag).toBe(true);
    expect(tableAgainstBook('whole', book)).toEqual([]);
  }, 300000);

  it('the far side\'s thirteen come from its files, and Tibet from the table\'s stand-ins as from the rules\'', () => {
    const table = tableNations('whole');
    for (const tag of FAR) expect(table[tag]?.from, tag).toMatch(/^far-/);
    expect(table.TIB).toBeUndefined();
    const stand = tableStandIns();
    const tibet = (TC().nationsBook(true) as BookRow[]).find((n) => n.tag === 'TIB')!;
    expect(stand.rows.TIB).toEqual([tibet.name, tibet.colour.replace(/^#/, '').toLowerCase()]);
    // (any other owner with no row is the rules' grey on the table too)
    expect(stand.grey).toBe('8a8a80');
  }, 300000);

  it('a country the table would not know is told, by name', () => {
    const book = TC().nationsBook(true) as BookRow[];
    const made = [...book, { tag: 'XXA', name: 'Atlantis', adjective: 'Atlantean', colour: '#123456', alignment: 'independent' }];
    const faults = tableAgainstBook('whole', made);
    expect(faults.length).toBe(2);
    expect(faults[0]).toContain('XXA');
    // (and a bare owner as the rules' own guard makes it, its code for a name in their grey, is the table's too)
    expect(tableAgainstBook('whole', [...book, { tag: 'XXB', name: 'XXB', adjective: 'XXB', colour: '#8a8a80', alignment: 'independent' }])).toEqual([]);
    // (on a narrower map the table stands nobody in)
    expect(tableAgainstBook('americas', [{ tag: 'XXB', name: 'XXB', adjective: 'XXB', colour: '#8a8a80', alignment: 'independent' }]).length).toBe(1);
  }, 300000);
});
