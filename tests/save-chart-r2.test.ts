// A saved war knows its map, with the whole world off (release 2, 25 November 2026): today's game. A whole-world save
// was only ever "a save that could not be read"; the bridge now names its map and gives the sentence to show. Nothing
// else about saving or loading on Europe and the Americas changes.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { crossesToWhole, loadGame, peekSave, saveChart, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { CHART_NOW, CHART_REGIONS, WORLD_FIRST, attachWorld } from '../src/campaign/world';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
/** The same war as a whole-world game would have saved it: the far side's provinces on its end. */
const onWhole = (text: string): string => { const d = JSON.parse(text); while (d.regions.length < WORLD_FIRST + CHART_REGIONS.whole) d.regions.push(d.regions[d.regions.length - 1]); return JSON.stringify(d); };

describe('a saved war knows its map, with the whole world off', () => {
  const wide = createGame(5, 'USA');
  attachWorld(wide);
  const americas = saveGame(startOpen(wide));
  const whole = onWhole(americas);
  const europe = saveGame(createGame(5, 'GER'));

  it('reads the map from the save', () => {
    expect(CHART_NOW).toBe('americas');
    expect([saveChart(americas), saveChart(whole), saveChart(europe)]).toEqual(['americas', 'whole', 'europe']);
    expect([crossesToWhole(americas), crossesToWhole(whole), crossesToWhole(europe)]).toEqual([false, false, false]);
  }, 300000);

  it('a war on Europe and the Americas is offered and loaded exactly as before', () => {
    expect(TC().peekChart(americas)).toEqual({ chart: 'americas', now: 'americas', fits: true, why: '' });
    expect(TC().peek(americas)).toMatchObject({ human: 'USA', open: true });
    expect(TC().load(americas)).not.toBeNull();
    expect(saveChart(TC().save().text)).toBe('americas');
    expect(JSON.parse(TC().save().text).regions).toHaveLength(WORLD_FIRST + CHART_REGIONS.americas);
  }, 300000);

  it('a whole-world war is not read, as before, and now the game can say why', () => {
    expect(peekSave(whole)).toBeNull();
    expect(loadGame(whole)).toBeNull();
    expect(TC().peek(whole)).toBeNull();
    expect(TC().load(whole)).toBeNull();
    expect(TC().peekChart(whole)).toEqual({ chart: 'whole', now: 'americas', fits: false, why: 'This war was saved on the whole-world map. Switch the whole world on under OPTIONS to continue it.' });
  }, 300000);

  it('text that is no save has no map', () => {
    expect(TC().peekChart('{}')).toBeNull();
    expect(TC().peekChart('nothing')).toBeNull();
  });
});
