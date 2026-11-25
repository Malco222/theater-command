// A fleet cut off beyond a shut strait (1.5.2; the 1.5.1 playtest, 25 November 2026). A strait can shut behind a fleet
// at sea: here Italy's 2nd Fleet is out past Gibraltar when Italy goes to war with Britain, who holds the strait. Its
// reach is counted from Naples, so nothing saw it was out there: the order to return to harbour sailed it home through
// the shut strait, the staff did the same, and a fleet the player kept by hand had no other order that worked. Now it
// is cut off: it makes for the nearest friendly harbour on its side, which becomes its base, or with none stays at sea,
// and its owner is told once.
import { describe, expect, it } from 'vitest';
import { allied, atWar, createGame, declareWar } from '../src/campaign/game';
import { canSailTo, cutOff, harbourFor, navalAI, navalUpkeep, sail, zoneOf } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { saveGame } from '../src/campaign/save';
import { beginHuman, humanUp, orderFleet, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;

/** Italy's 2nd Fleet on patrol past Gibraltar in the seas off Iberia, and then Italy at war with Britain. */
function outPast(human: string | null): { g: Game; f: Fleet } {
  const g = startOpen(createGame(3, human));
  if (human) for (let k = 0; !humanUp(g) && k < 60; k++) stepAI(g);
  const f = fleet(g, 'ITA', '2nd Fleet');
  expect(sail(g, f, zoneOf(g, 'iberia'), 'patrol')).toBe(true);
  expect(cutOff(g, f)).toBe(false);
  if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
  return { g, f };
}

describe('a fleet at sea when a strait shuts behind it', () => {
  it('is cut off, and makes for the nearest friendly harbour on its side, not through the strait', () => {
    const { g, f } = outPast('ITA');
    expect(cutOff(g, f)).toBe(true);
    const haven = harbourFor(g, f);
    expect(haven).not.toBeNull();
    expect(allied(g, 'ITA', haven!.ctrl)).toBe(true);
    // Its own harbour is beyond the strait: it can reach no sea of Naples.
    expect(canSailTo(g, f, g.byName['Naples'])).toBe(false);
    expect(canSailTo(g, f, haven!)).toBe(true);
    // No sea of its reach from Naples: every one lies beyond the strait.
    expect(orderFleet(g, f.id, zoneOf(g, 'tyrr'), 'patrol')).toBe(false);
    const news = g.headlines.length;
    expect(orderFleet(g, f.id, f.home, 'port')).toBe(true);
    expect(f.stance).toBe('port');
    expect(f.base).toBe(haven!.id);
    expect(f.port).toBe(haven!.name);
    expect(g.regions[f.base].name).not.toBe('Naples');
    expect(g.headlines.slice(news).some((h) => /cannot reach Naples: the Strait of Gibraltar is shut to it\. It makes for/.test(h.text))).toBe(true);
    expect(cutOff(g, f)).toBe(false);
  });

  it('kept by the player, he is told once that it is cut off and where it can go', () => {
    const { g, f } = outPast('ITA');
    f.ordered = true;
    const news = g.headlines.length;
    navalUpkeep(g, 'ITA');
    const told = g.headlines.slice(news).filter((h) => /is cut off in the/.test(h.text));
    expect(told.length).toBe(1);
    expect(told[0].text).toMatch(/the Strait of Gibraltar is shut to it, so it cannot sail home to Naples\. Send it to harbour and it makes for/);
    navalUpkeep(g, 'ITA');
    expect(g.headlines.slice(news).filter((h) => /is cut off in the/.test(h.text)).length).toBe(1);
    // His admirals leave a kept fleet where it is.
    navalAI(g, 'ITA');
    expect(f.stance).toBe('patrol');
  });

  it("under the computer's staff it makes for that harbour, and never sails home through the strait", () => {
    const { g, f } = outPast(null);
    const haven = harbourFor(g, f)!;
    navalAI(g, 'ITA');
    expect(g.regions[f.base].name).not.toBe('Naples');
    expect(f.base).toBe(haven.id);
  });

  it('with no friendly harbour on its side it stays at sea', () => {
    const { g, f } = outPast('ITA');
    // Every harbour on the Atlantic side in British hands for the test.
    expect(harbourFor(g, f)).not.toBeNull();
    for (let r = harbourFor(g, f); r; r = harbourFor(g, f)) r.ctrl = 'UK';
    expect(orderFleet(g, f.id, f.home, 'port')).toBe(false);
    expect(f.stance).toBe('patrol');
    expect(g.regions[f.base].name).toBe('Naples');
    f.ordered = true;
    navalUpkeep(g, 'ITA');
    expect(g.headlines.some((h) => /No friendly harbour on its side of the strait can take it: it stays at sea\./.test(h.text))).toBe(true);
  });

  it('a fleet in reach of its harbour is not cut off, and goes home as before', () => {
    const g = startOpen(createGame(3, null));
    const f = fleet(g, 'ITA', '2nd Fleet');
    expect(sail(g, f, zoneOf(g, 'central'), 'patrol')).toBe(true);
    declareWar(g, 'ITA', 'UK');
    expect(cutOff(g, f)).toBe(false);
    expect(sail(g, f, f.home, 'port')).toBe(true);
    expect(g.regions[f.base].name).toBe('Naples');
    expect(f.zone).toBe(f.home);
  });
});

// ---- Through the 3D game's doorway ------------------------------------------------------------------------------
type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface PageOut { note: string; top: { label: string; act: string; ok: boolean; tip: string }[] }
interface Panel { lines: string[]; options: { key: string; label: string; detail: string; ok: boolean; why: string }[] }
interface View { fleets: { id: number; reach: number[]; bases: number[] }[] }

describe('the cut-off fleet on its page, its panel and the map', () => {
  it('offers no sea, and RETURN TO HARBOUR names the harbour on its side', () => {
    const { g, f } = outPast('ITA');
    beginHuman(g);
    const haven = harbourFor(g, f)!;
    TC().load(saveGame(g));
    const v = (TC().view() as View).fleets.find((x) => x.id === f.id)!;
    expect(v.reach).toEqual([]);
    expect(v.bases).toContain(haven.id);
    const panel = TC().force('fleet', f.id) as Panel;
    expect(panel.options.some((o) => /:(patrol|raid)$/.test(o.key))).toBe(false);
    const home = panel.options.find((o) => o.key.endsWith(':port'))!;
    expect(home.label).toBe(`Return to harbour at ${haven.name}`);
    expect(home.ok).toBe(true);
    expect(home.detail).toMatch(/cut off beyond a shut strait/);
    // The card says it while it lasts (the DD's standing line), on the panel and at the head of the page's note.
    const line = `Cut off: the Strait of Gibraltar is shut to it. Nearest friendly harbour on its side: ${haven.name}.`;
    expect(panel.lines).toContain(line);
    expect((TC().page(`fleet:${f.id}`) as PageOut).note.startsWith(`${line} `)).toBe(true);
    const button = (TC().page(`fleet:${f.id}`) as PageOut).top.find((b) => b.label === 'RETURN TO HARBOUR')!;
    expect(button.ok).toBe(true);
    expect(button.tip).toBe(`Cut off beyond a shut strait: it makes for ${haven.name}, which becomes its base.`);
    expect((TC().forceOrder(home.key) as { ok: boolean }).ok).toBe(true);
    const after = TC().force('fleet', f.id) as Panel;
    expect(after.options.some((o) => /:patrol$/.test(o.key))).toBe(true);
    expect(after.lines.some((l) => /^Cut off/.test(l))).toBe(false);
    expect((TC().page(`fleet:${f.id}`) as PageOut).note).not.toMatch(/Cut off/);
  });

  it('with no friendly harbour on its side the button is there but cannot be pressed, and says why', () => {
    const { g, f } = outPast('ITA');
    beginHuman(g);
    for (let r = harbourFor(g, f); r; r = harbourFor(g, f)) r.ctrl = 'UK';
    TC().load(saveGame(g));
    const home = (TC().force('fleet', f.id) as Panel).options.find((o) => o.key.endsWith(':port'))!;
    expect(home.ok).toBe(false);
    expect(home.why).toBe('It is cut off beyond a shut strait, and no friendly harbour on its side can take it.');
    expect((TC().force('fleet', f.id) as Panel).lines).toContain('Cut off: the Strait of Gibraltar is shut to it. No friendly harbour on its side can take it.');
    expect((TC().forceOrder(home.key) as { ok: boolean }).ok).toBe(false);
  });
});
