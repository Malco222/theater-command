// A fleet cut off beyond a shut strait (1.5.2; the 1.5.1 playtest, 25 November 2026). A strait can shut behind a fleet
// at sea: here Italy's 2nd Fleet is out past Gibraltar when Italy goes to war with Britain, who holds the strait. Its
// reach is counted from Naples, so nothing saw it was out there: the order to return to harbour sailed it home through
// the shut strait, the staff did the same, and a fleet the player kept by hand had no other order that worked. Now it
// is cut off: it makes for the nearest friendly harbour on its side, which becomes its base, or with none stays at sea,
// and its owner is told once.
import { describe, expect, it } from 'vitest';
import { allied, atWar, createGame, declareWar } from '../src/campaign/game';
import { baseSeas, canSailTo, cutOff, fleetReach, harbourFor, navalAI, navalUpkeep, rebase, sail, zoneOf } from '../src/campaign/navy';
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

// The playtest's #28: Spanish Morocco is a harbour on both sides of the Strait of Gibraltar (the Western Mediterranean and
// the Iberian Coast). It is no way round the strait: a fleet based there is on the side it came in from.
describe('a harbour on both sides of a shut strait', () => {
  /** Italy at war with Britain, holding Spanish Morocco for the test; its 2nd Fleet out on the Iberian Coast first if asked. */
  function morocco(out = false): { g: Game; f: Fleet; sm: number; med: number; atl: number } {
    const g = startOpen(createGame(3, 'ITA'));
    for (let k = 0; !humanUp(g) && k < 60; k++) stepAI(g);
    if (out) expect(sail(g, fleet(g, 'ITA', '2nd Fleet'), zoneOf(g, 'iberia'), 'patrol')).toBe(true);
    if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
    const r = g.byName['Spanish Morocco'];
    r.ctrl = 'ITA';
    const med = zoneOf(g, 'wmed'), atl = zoneOf(g, 'iberia');
    expect(r.seas).toEqual(expect.arrayContaining([med, atl]));
    return { g, f: fleet(g, 'ITA', '2nd Fleet'), sm: r.id, med, atl };
  }

  it('a fleet from the Mediterranean that bases there reaches no sea beyond the strait', () => {
    const { g, f, sm, med, atl } = morocco();
    expect(rebase(g, f, g.regions[sm])).toBe(true);
    expect(f.home).toBe(med);
    expect(baseSeas(g, f)).toEqual([med]);
    expect(fleetReach(g, f)).not.toContain(atl);
    expect(orderFleet(g, f.id, atl, 'raid')).toBe(false);
  });

  it('a fleet cut off on the Atlantic side that bases there stays on the Atlantic side', () => {
    const { g, f, sm, med, atl } = morocco(true);
    expect(f.zone).toBe(atl);
    expect(cutOff(g, f)).toBe(true);
    expect(rebase(g, f, g.regions[sm])).toBe(true);
    expect(f.home).toBe(atl);
    expect(f.zone).toBe(atl);
    expect(fleetReach(g, f)).toContain(atl);
    expect(fleetReach(g, f)).not.toContain(med);
    expect(cutOff(g, f)).toBe(false);
  });

  it('a fleet based there that went out to the Atlantic side comes in on that side when the strait shuts (the release review)', () => {
    // Based at Spanish Morocco from the Mediterranean while the strait was open, out on the Iberian Coast when it shuts,
    // with no other friendly harbour on that side: its nearest harbour is its own. It comes in on the Atlantic side,
    // keeping its base, where rebase() would have refused its own base and left it at sea for good.
    const g = startOpen(createGame(3, 'ITA'));
    for (let k = 0; !humanUp(g) && k < 60; k++) stepAI(g);
    const f = fleet(g, 'ITA', '2nd Fleet'), sm = g.byName['Spanish Morocco'];
    const med = zoneOf(g, 'wmed'), atl = zoneOf(g, 'iberia');
    sm.ctrl = 'ITA';
    expect(rebase(g, f, sm)).toBe(true);
    expect(f.home).toBe(med);
    expect(sail(g, f, atl, 'patrol')).toBe(true);
    if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
    expect(cutOff(g, f)).toBe(true);
    for (let r = harbourFor(g, f); r && r.id !== sm.id; r = harbourFor(g, f)) r.ctrl = 'UK';
    expect(harbourFor(g, f)?.id).toBe(sm.id);
    expect(orderFleet(g, f.id, f.home, 'port')).toBe(true);
    expect(f.base).toBe(sm.id);
    expect(f.home).toBe(atl);
    expect(f.zone).toBe(atl);
    expect(f.stance).toBe('port');
    expect(cutOff(g, f)).toBe(false);
  });

  it("the strait's holder, and a nation it is open to, use both its seas as before", () => {
    const { g, sm, med, atl } = morocco();
    const uk = g.fleets.find((x) => x.nat === 'UK' && fleetReach(g, x).includes(atl))!;
    g.regions[sm].ctrl = 'UK';
    expect(rebase(g, uk, g.regions[sm])).toBe(true);
    expect(baseSeas(g, uk).sort()).toEqual([med, atl].sort());
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
    // (The release review) Its page offers no sea and no lane to escort either: every one lies beyond the strait.
    const rows = (TC().page(`fleet:${f.id}`) as PageOut & { sections: { rows: { buttons: { act: string; ok: boolean }[] }[] }[] }).sections.flatMap((x) => x.rows.flatMap((r) => r.buttons));
    expect(rows.filter((b) => b.ok && (/^fleet:\d+:\d+:(patrol|raid|hunt)$/.test(b.act) || /^fleetescort:/.test(b.act)))).toEqual([]);
    // A sea order sent anyway is refused with the card's line, not the bare refusal.
    const tyrr = zoneOf(g, 'tyrr');
    expect(TC().forceOrder(`fleet:${f.id}:${tyrr}:patrol`)).toMatchObject({ ok: false, note: line });
    expect(TC().act(`fleet:${f.id}:${tyrr}:raid`)).toMatchObject({ ok: false, note: line });
    // The 3D tray reads the line and the harbour from the fleet in hand, to dim what it cannot do and name where it goes.
    expect(TC().fleetInHand(f.id)).toMatchObject({ cut: line, haven: haven.name, held: '' });
    expect((TC().forceOrder(home.key) as { ok: boolean }).ok).toBe(true);
    expect(TC().fleetInHand(f.id)).toMatchObject({ cut: '', haven: '' });
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
    expect(TC().forceOrder(home.key)).toMatchObject({ ok: false, note: 'Cut off: the Strait of Gibraltar is shut to it. No friendly harbour on its side can take it.' });
    expect(TC().fleetInHand(f.id)).toMatchObject({ cut: 'Cut off: the Strait of Gibraltar is shut to it. No friendly harbour on its side can take it.', haven: '' });
  });
});
