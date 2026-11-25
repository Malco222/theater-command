// A war is continued on the map it was saved on (release 2, Japan and the Pacific, 25 November 2026). The game knew
// a save's map only by its province count. With the whole world on, a war saved on Europe and the Americas loaded
// with the far side laid over it as of September 1939, whatever its month, and the next autosave kept it on the whole
// world for good: a 1943 war with a 1939 Far East, and no way back. The 3D game's bridge now says which map a save is
// of, gives the sentence to show, and neither offers nor loads a war of the other map. The rules' own loadGame
// refuses it too (the design director's ruling of 25 November, form A4); the laying survives only as promoteSave, a
// named call for the test kitchen.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { crossesToWhole, loadGame, promoteSave, saveChart, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { CHART_NOW, CHART_REGIONS, WORLD_FIRST } from '../src/campaign/world';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
/** The same war as it would have been saved on Europe and the Americas: the far side's provinces cut off its end. */
const onAmericas = (text: string): string => { const d = JSON.parse(text); d.regions = d.regions.slice(0, WORLD_FIRST + CHART_REGIONS.americas); return JSON.stringify(d); };

describe('a saved war knows its map, with the whole world on', () => {
  const whole = saveGame(startOpen(createGame(5, 'JAP')));
  const americas = onAmericas(whole);
  const europe = saveGame(createGame(5, 'GER'));

  it('reads the map from the save: the whole world, Europe and the Americas, Europe alone', () => {
    expect(CHART_NOW).toBe('whole');
    expect(CHART_REGIONS).toEqual({ americas: 540, whole: 1692 });
    expect(saveChart(whole)).toBe('whole');
    expect(saveChart(americas)).toBe('americas');
    expect(saveChart(europe)).toBe('europe');
    expect(saveChart('not a save')).toBeNull();
    expect(saveChart(JSON.stringify({ v: 1, flags: { world: 1 }, regions: new Array(WORLD_FIRST + 7).fill('') }))).toBeNull();
    expect([crossesToWhole(whole), crossesToWhole(americas), crossesToWhole(europe)]).toEqual([false, true, false]);
  }, 300000);

  it('the bridge offers and loads a whole-world war', () => {
    expect(TC().peekChart(whole)).toEqual({ chart: 'whole', now: 'whole', fits: true, why: '' });
    expect(TC().peek(whole)).toMatchObject({ human: 'JAP', name: 'Japan', open: true });
    expect(TC().load(whole)).not.toBeNull();
    expect(TC().nationsBook().some((n: { tag: string }) => n.tag === 'JAP')).toBe(true);
  }, 300000);

  it('the bridge neither offers nor loads a war saved on Europe and the Americas, and says the true sentence', () => {
    const was = TC().save().text as string;
    expect(TC().peekChart(americas)).toEqual({ chart: 'americas', now: 'whole', fits: false, why: 'This war was saved on the map of Europe and the Americas. Switch the whole world off under OPTIONS to continue it.' });
    expect(TC().peek(americas)).toBeNull();
    expect(TC().load(americas)).toBeNull();
    // The war in play is the one that was in play: a refused load changes nothing.
    expect(TC().save().text).toBe(was);
  }, 300000);

  it('a war on Europe alone is neither map\'s: offered and loaded as ever', () => {
    expect(TC().peekChart(europe)).toEqual({ chart: 'europe', now: 'whole', fits: true, why: '' });
    expect(TC().peek(europe)).toMatchObject({ human: 'GER' });
  }, 300000);

  it("the rules' own loadGame refuses it too (the design director, form A4); only the test kitchen's named call lays the far side over it", () => {
    expect(loadGame(americas)).toBeNull();
    const g = promoteSave(americas);
    expect(g).not.toBeNull();
    expect(g!.regions.length).toBe(WORLD_FIRST + CHART_REGIONS.whole);
    // A whole-world war and a war of Europe alone read by either call.
    expect(loadGame(whole)).not.toBeNull();
    expect(promoteSave(europe)).not.toBeNull();
  }, 300000);
});
