// One name to a thing (the UI director's ruling of 17 November 2026, placed with the rules builder for Version 84.2.6).
// The four American armies of the first day are called by their names alone: "Third Army", not "Third Army (2nd
// Division and 1st Cavalry Division)", whose bracket listed divisions that disagreed with the chips under it. Names and
// words only: no rule reads these names. A campaign saved before is read under the new names.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { armyWords, loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import world from '../data/campaign/world.json';

const wide = (): Game => startOpen(createGame(1, null));
const OLD: [string, string][] = [
  ['First Army (1st Division)', 'First Army'], ['Second Army (5th and 6th Divisions)', 'Second Army'],
  ['Third Army (2nd Division and 1st Cavalry Division)', 'Third Army'], ['Fourth Army (3rd Division)', 'Fourth Army'],
];

describe('the American armies are called by their names alone', () => {
  it('opens with First, Second, Third and Fourth Army, and no American name in brackets', () => {
    const g = wide();
    const usa = g.corps.filter((c) => c.nat === 'USA').map((c) => c.name);
    for (const [, now] of OLD) expect(usa).toContain(now);
    for (const n of usa) expect(n).not.toMatch(/\(/);
    // The Latin American names that carry a place in brackets stand.
    expect((world.corps as unknown[][]).some((r) => r[0] === 'BRA' && r[1] === '1st Military Region (Rio de Janeiro)')).toBe(true);
    expect(g.corps.some((c) => c.nat === 'PER' && c.name === '2nd Light Division (Lima)')).toBe(true);
  });

  it('reads a campaign saved under the old names under the new ones, on the corps, in the news and in the reports', () => {
    const g = wide();
    let text = saveGame(g);
    for (const [was, now] of OLD) text = text.split(JSON.stringify(now)).join(JSON.stringify(was));
    text = text.replace(/"headlines":\[/, `"headlines":[{"turn":1,"week":0,"kind":"info","text":"The Third Army (2nd Division and 1st Cavalry Division) stands at North Texas."},`);
    expect(text).toContain('Fourth Army (3rd Division)');
    const h = loadGame(text)!;
    expect(h).not.toBeNull();
    expect(h.corps.map((c) => c.name)).toEqual(g.corps.map((c) => c.name));
    expect(h.headlines.some((x) => x.text === 'The Third Army stands at North Texas.')).toBe(true);
    expect(saveGame(h)).not.toMatch(/Army \((1st|5th|2nd|3rd) /);
  });

  it('touches no other name', () => {
    expect(armyWords('Second Army: XXX Corps (50th, 49th and 7th Armoured Divisions)')).toBe('Second Army: XXX Corps (50th, 49th and 7th Armoured Divisions)');
    expect(armyWords('First Army: 78th Division')).toBe('First Army: 78th Division');
    expect(armyWords('2nd Light Division (Lima)')).toBe('2nd Light Division (Lima)');
  });
});
