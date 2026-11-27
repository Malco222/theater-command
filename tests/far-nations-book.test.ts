// The 3D table's list of countries, from the rules (release 2, Japan and the Pacific, 24 November 2026). The table
// kept its own list from the data files and so knew none of the far side's countries: their ground was grey and
// their armies, fleets and wings were not drawn. TC.nationsBook() hands it every country the rules know. And the
// pointer's place on a table whose eye follows the map round the world is read in the chart's own frame (seam-3).
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { seaAt } from '../src/campaign/navy';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { WORLD_WIDTH } from '../src/campaign/wrap';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface Row { tag: string; name: string; adjective: string; colour: string; alignment: string }
const STANDS = ['allies', 'allies_cobelligerent', 'leaning_allies', 'independent', 'leaning_axis', 'axis_cobelligerent', 'axis'];
const FAR = ['JAP', 'CHN', 'MAN', 'MGL', 'THA', 'AUS', 'NZL', 'SAF', 'SAU', 'YEM', 'AFG', 'NEP', 'LBR', 'TIB'];

describe('the nations book on the whole-world chart', () => {
  it('with no campaign running: every country of a first day on the chart asked for', () => {
    const europe = TC().nationsBook() as Row[];
    const whole = TC().nationsBook(true) as Row[];
    expect(europe.some((n) => n.tag === 'GER')).toBe(true);
    expect(europe.some((n) => n.tag === 'JAP')).toBe(false);
    const g = startOpen(createGame(1, null));
    expect(whole.map((n) => n.tag).sort()).toEqual(Object.keys(g.nations).sort());
    for (const tag of FAR) expect(whole.some((n) => n.tag === tag), tag).toBe(true);
    for (const n of whole) {
      expect(n.colour, n.tag).toMatch(/^#[0-9a-fA-F]{6}$/);
      expect(STANDS, n.tag).toContain(n.alignment);
      expect(n.name.length && n.adjective.length, n.tag).toBeGreaterThan(0);
    }
    expect(whole.find((n) => n.tag === 'JAP')).toMatchObject({ name: 'Japan', adjective: 'Japanese' });
    expect(whole.find((n) => n.tag === 'TIB')).toMatchObject({ name: 'Tibet', alignment: 'independent' });
    // Asked again, the same rows (the first day is laid once and kept).
    expect(TC().nationsBook(true)).toEqual(whole);
  }, 300000);

  it('with a campaign running: that campaign\'s countries as they stand, and asking changes nothing', () => {
    const g = startOpen(createGame(3, 'JAP'));
    g.nations.THA.align = 'axis';
    TC().load(saveGame(g));
    const before = (TC().save() as { text: string }).text;
    const book = TC().nationsBook() as Row[];
    expect(book.length).toBe(Object.keys(g.nations).length);
    expect(book.find((n) => n.tag === 'THA')!.alignment).toBe('axis');
    expect((TC().save() as { text: string }).text === before).toBe(true);
  }, 300000);
});

describe('the sea under the pointer where the chart goes round the world', () => {
  it('a point a whole turn east or west of the frame is the same sea; inside the frame nothing moves', () => {
    const g = startOpen(createGame(1, null));
    let seen = 0;
    for (const z of g.seas) {
      const here = seaAt(z.x, z.y, g);
      if (here < 0) continue; // (a zone whose own mark lies on an islet's square)
      seen++;
      expect(seaAt(z.x + WORLD_WIDTH, z.y, g), z.name).toBe(here);
      expect(seaAt(z.x - WORLD_WIDTH, z.y, g), z.name).toBe(here);
    }
    expect(seen).toBeGreaterThan(100);
    // Off the chart north and south there is still no sea.
    expect(seaAt(g.seas[0].x + WORLD_WIDTH, -50, g)).toBe(-1);
  }, 300000);
});
