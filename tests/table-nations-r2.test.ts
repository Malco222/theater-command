// The 3D table's own list of countries against the rules', on the maps of today: Europe alone, and Europe with the
// Americas (release 2; the art director, 25 November 2026). See tests/table-nations.ts for why the table keeps a
// list of its own, and tests/far-table-nations.test.ts for the whole world.
import { describe, expect, it } from 'vitest';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { type BookRow, TABLE_FILES, filesTheTableNames, tableAgainstBook, tableNations } from './table-nations';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

describe('the 3D table knows every country the rules know', () => {
  it('the files this test reads are the files the table reads, in its order', () => {
    // (A file added to FCampaignMapData::Load, or taken from it, must be added here or taken from here too.)
    expect(filesTheTableNames()).toEqual(TABLE_FILES.whole);
  });

  it('Europe alone', () => {
    const book = TC().nationsBook() as BookRow[];
    expect(book.length).toBeGreaterThan(20);
    expect(tableAgainstBook('europe', book)).toEqual([]);
  }, 300000);

  it('Europe with the Americas', () => {
    const book = TC().nationsBook(true) as BookRow[];
    expect(book.some((n) => n.tag === 'MEX')).toBe(true);
    expect(tableAgainstBook('americas', book)).toEqual([]);
    // (and the wider chart's own file is what gives the table Mexico: without it the table would not know her)
    expect(tableNations('europe').MEX).toBeUndefined();
    expect(tableNations('americas').MEX?.from).toBe('world.json');
  }, 300000);
});
