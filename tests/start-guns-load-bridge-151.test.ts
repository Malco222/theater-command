// The start guns in a war saved before 1.5.1, in the 3D game (Brian, 24 November 2026 22:31Z, on the art director's
// card: "Yes, when I load a saved war"): the campaign bridge's load lays them once, for every great power, with one
// line in the news. The rules' half is tests/airversion.test.ts.
import { describe, expect, it } from 'vitest';
import { gunsAt } from '../src/campaign/air';
import { startGunSites } from '../src/campaign/airversion';
import { majorsOf } from '../src/campaign/diplomacy';
import { loadGame } from '../src/campaign/save';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

interface Bridge { start(o: Record<string, unknown>): unknown; save(): { text: string }; load(text: string): unknown }
const TC = (globalThis as Record<string, unknown>).TC as Bridge;
const LINE = "Anti-aircraft guns now stand round each great power's capital and its two chief factory towns.";

/** The war in play, saved and taken back as a save made before the guns: no flag, no guns. */
function oldSave(): string {
  const old = JSON.parse(TC.save().text);
  delete old.flags['av:start'];
  for (const k of Object.keys(old.flags)) if (k.startsWith('wk:aa:')) delete old.flags[k];
  return JSON.stringify(old);
}
const lines = (text: string): number => JSON.parse(text).headlines.filter((h: { text: string }) => h.text === LINE).length;

describe("the 3D game's load: start guns in a war saved before them (1.5.1)", () => {
  it('a grand campaign of 1939 gets level 1 guns round every great power once, with one line in the news', () => {
    TC.start({ view: 'grand', human: 'GER', seed: 7 });
    const text = oldSave();
    const bare = loadGame(text)!;
    expect(bare.regions.some((r) => gunsAt(bare, r.id) > 0)).toBe(false);
    TC.load(text);
    const once = TC.save().text;
    const g = loadGame(once)!;
    expect(g.flags['av:start']).toBe(1);
    for (const nat of majorsOf(g)) {
      expect(startGunSites(g, nat).length, nat).toBeGreaterThan(0);
      for (const s of startGunSites(g, nat)) expect(gunsAt(g, s), nat).toBe(1);
    }
    expect(lines(once)).toBe(1);
    // Saved and loaded again: nothing more is laid and no second line.
    TC.load(once);
    expect(lines(TC.save().text)).toBe(1);
  });

  it('a grand campaign begun in 1941 gets level 2; a war begun with them, or a history campaign, is not touched', () => {
    TC.start({ view: 'grand', human: 'UK', from: 17, seed: 7 });
    TC.load(oldSave());
    const g = loadGame(TC.save().text)!;
    expect(gunsAt(g, g.nations.UK.capital)).toBe(2);
    expect(gunsAt(g, g.nations.GER.capital)).toBe(2);
    TC.start({ view: 'grand', human: 'GER', seed: 7 });
    TC.load(TC.save().text);
    expect(lines(TC.save().text)).toBe(0);
    TC.start({ human: 'GER', seed: 7 });
    TC.load(oldSave());
    const h = TC.save().text;
    expect(JSON.parse(h).flags['av:start']).toBeUndefined();
    expect(lines(h)).toBe(0);
  });
});
