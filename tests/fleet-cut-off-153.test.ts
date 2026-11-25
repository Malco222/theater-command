// The cut-off fleet's last two notes (1.5.3; the 1.5.2 review). A fleet at sea beyond a strait that shut behind it is cut
// off (navy.ts cutOff): Italy's 2nd Fleet out past Gibraltar when Italy goes to war with Britain, as in
// fleet-cut-off-152.test.ts.
//  1. Its page's note no longer says "A fleet reaches three seas from its base": it reaches none. It keeps "one order a
//     month" where a harbour on its side can take it, and with none says nothing more than its cut-off line.
//  2. The browser rings no sea for it (fleetReach still counts from its base, beyond the strait, as the staffs and the
//     rules want), and a click on a sea, or on its work, is refused with its cut-off line.
//  3. (The review of 2.) The browser's paper, its hint bar and a right-click on land no longer tell it to click a ringed
//     sea: they give its cut-off line, as the 3D game's tray does. A fleet also held where this month's battle left it is
//     refused with that reason first, as the bridge answers it (fleetWhyId). (The review of 3.) A right-click on land in the
//     3D game gives the same lines, from fleetInHand's held and cut (RightClickOn).
import { describe, expect, it } from 'vitest';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { cutOff, cutOffLine, fleetReach, harbourFor, rebase, sail, zoneOf } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { saveGame } from '../src/campaign/save';
import { beginHuman, fleetOptions, humanUp, orderFleet, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import browser from '../src/campaign/ui/main.ts?raw';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface PageOut { note: string; sections: { heading: string; text: string }[] }
interface Panel { lines: string[]; options: { key: string; detail: string; why: string }[] }

const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const THREE_SEAS = 'A fleet reaches three seas from its base, and can be given one order a month.';
const count = (text: string, part: string): number => text.split(part).length - 1;

/** Italy's 2nd Fleet on patrol past Gibraltar in the seas off Iberia, and then Italy at war with Britain, in Italy's month. */
function outPast(): { g: Game; f: Fleet } {
  const g = startOpen(createGame(3, 'ITA'));
  for (let k = 0; !humanUp(g) && k < 60; k++) stepAI(g);
  const f = fleet(g, 'ITA', '2nd Fleet');
  expect(sail(g, f, zoneOf(g, 'iberia'), 'patrol')).toBe(true);
  if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
  expect(cutOff(g, f)).toBe(true);
  beginHuman(g);
  return { g, f };
}

const page = (id: number): PageOut => TC().page(`fleet:${id}`) as PageOut;

describe("the cut-off fleet's page note", () => {
  it('with a harbour on its side: no three seas, and its one order a month', () => {
    const { g, f } = outPast();
    const haven = harbourFor(g, f)!;
    TC().load(saveGame(g));
    const line = `Cut off: the Strait of Gibraltar is shut to it. Nearest friendly harbour on its side: ${haven.name}.`;
    const note = page(f.id).note;
    expect(note.startsWith(`${line} `)).toBe(true);
    expect(note).not.toMatch(/three seas/);
    expect(note).toMatch(/ships, readiness \d+%\. It can be given one order a month\./);
    expect(count(note, 'one order a month')).toBe(1);
    // Its harbour is named once, in the cut-off line, and nothing of it is said again.
    expect(count(note, haven.name)).toBe(1);
    // The panel and the 3D tray never carried the sentence, and still do not.
    expect(JSON.stringify(TC().force('fleet', f.id) as Panel)).not.toMatch(/three seas|one order a month/);
    expect(JSON.stringify(TC().fleetInHand(f.id))).not.toMatch(/three seas|one order a month/);
    // Home on its side, it is no longer cut off, and the note is as for any fleet.
    const home = (TC().force('fleet', f.id) as Panel).options.find((o) => o.key.endsWith(':port'))!;
    expect((TC().forceOrder(home.key) as { ok: boolean }).ok).toBe(true);
    const after = page(f.id).note;
    expect(after).not.toMatch(/Cut off/);
    expect(after).toContain(` ${THREE_SEAS}`);
  });

  it('with no friendly harbour on its side: neither sentence, and the cut-off line says it once', () => {
    const { g, f } = outPast();
    for (let r = harbourFor(g, f); r; r = harbourFor(g, f)) r.ctrl = 'UK';
    TC().load(saveGame(g));
    const note = page(f.id).note;
    expect(note.startsWith('Cut off: the Strait of Gibraltar is shut to it. No friendly harbour on its side can take it. ')).toBe(true);
    expect(note).not.toMatch(/three seas|one order a month/);
    expect(count(note, 'No friendly harbour')).toBe(1);
    // (no stray space where the sentence was)
    expect(note).not.toMatch(/ {2}|\. $/);
    expect(note).toMatch(/ships, readiness \d+%\.( |$)/);
    // Its seas section still says why it has none (1.5.2), apart from the note.
    expect(page(f.id).sections.find((x) => x.heading === 'SEAS IN ITS REACH')?.text)
      .toBe('Every sea in its reach lies beyond the shut strait. No friendly harbour on its side can take it, so it stays at sea.');
  });

  it('its own harbour on both sides of the strait (Spanish Morocco) is a harbour on its side: one order a month', () => {
    const g = startOpen(createGame(3, 'ITA'));
    for (let k = 0; !humanUp(g) && k < 60; k++) stepAI(g);
    const f = fleet(g, 'ITA', '2nd Fleet'), sm = g.byName['Spanish Morocco'];
    sm.ctrl = 'ITA';
    expect(rebase(g, f, sm)).toBe(true);
    expect(sail(g, f, zoneOf(g, 'iberia'), 'patrol')).toBe(true);
    if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
    for (let r = harbourFor(g, f); r && r.id !== sm.id; r = harbourFor(g, f)) r.ctrl = 'UK';
    expect(cutOff(g, f)).toBe(true);
    expect(harbourFor(g, f)?.id).toBe(sm.id);
    beginHuman(g);
    TC().load(saveGame(g));
    const note = page(f.id).note;
    expect(note).not.toMatch(/three seas/);
    expect(note).toContain('It can be given one order a month.');
  });

  it('a fleet that is not cut off keeps the sentence word for word', () => {
    const g = startOpen(createGame(3, 'ITA'));
    for (let k = 0; !humanUp(g) && k < 60; k++) stepAI(g);
    const f = fleet(g, 'ITA', '2nd Fleet');
    expect(sail(g, f, zoneOf(g, 'central'), 'patrol')).toBe(true);
    if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
    expect(cutOff(g, f)).toBe(false);
    beginHuman(g);
    TC().load(saveGame(g));
    expect(page(f.id).note).toContain(` ${THREE_SEAS}`);
  });
});

describe('the cut-off fleet on the browser map', () => {
  it('fleetReach still counts its seas from its base, so the map asks cutOff before it rings any', () => {
    const { g, f } = outPast();
    // The rules' reach is the staffs' and the order checks' (navy.ts, fleets.ts, carriers.ts, fleetattack.ts): it is left
    // as it is, and the browser's ring, like the 3D view's reach, is empty for a cut-off fleet.
    expect(fleetReach(g, f).length).toBeGreaterThan(0);
    expect(browser).toContain('if (!cutOff(g, fl)) for (const z of fleetReach(g, fl)) marks.push(');
    TC().load(saveGame(g));
    expect((TC().view() as { fleets: { id: number; reach: number[] }[] }).fleets.find((x) => x.id === f.id)?.reach).toEqual([]);
  });

  it('a click on a sea, on its work, or on its port is refused with its cut-off line', () => {
    const { g, f } = outPast();
    const line = cutOffLine(g, f);
    expect(line).toMatch(/^Cut off: the Strait of Gibraltar is shut to it\./);
    // A sea its base reaches (ringed before), and its work changed where it is: both refused by the rules.
    expect(fleetReach(g, f)).toContain(zoneOf(g, 'tyrr'));
    expect(orderFleet(g, f.id, zoneOf(g, 'tyrr'), 'patrol')).toBe(false);
    expect(orderFleet(g, f.id, f.zone, 'raid')).toBe(false);
    // The browser answers each refusal with the line before any other reason (fleetWhy: held first, then cut off).
    expect(browser).toContain('if (!orderFleet(g, fl.id, z, ui.stance)) ui.msg = fleetWhy(fl) || (gateBar(g, fl, z)?.line ?? ');
    expect(browser).toContain(': fleetWhy(f) || `The ${f.name} cannot ${seaWork(ui.stance)} in the ${g.seas[f.zone].name} now.`;');
    expect(browser).toContain(': fleetWhy(f) || `The ${f.name} cannot be given orders now.`;');
    // Its list of seas says so in place of "click a ringed sea".
    expect(browser).toContain(' : !cut ? `It can reach the seas within ${NAVY.range} of ${esc(f.port)}: click a ringed sea on the map, or pick one here.');
    // Not cut off, no line: the old reasons stand.
    expect(orderFleet(g, f.id, f.home, 'port')).toBe(true);
    expect(cutOffLine(g, f)).toBe('');
  });
});

describe("the browser's paper for a cut-off fleet in hand", () => {
  it('the hint bar, a right-click on land and its work buttons give its cut-off line, not "click a ringed sea"', () => {
    // Each line that sends a fleet in hand to a ringed sea is said only when fleetStuck has nothing to say.
    expect(browser).toContain('return f ? fleetStuck(f) || `Click a ringed sea to send the ${f.name} there.` : ');
    expect(browser).toContain('if (fl) return id >= 0 ? fleetStuck(fl) || `The ${fl.name} keeps to the sea: click a ringed sea to send it there.` : \'\';');
    expect(browser).toContain('} else if (f) ui.msg = fleetStuck(f) || `Click a ringed sea: the ${f.name} will ${seaWork(ui.stance)} there.`;');
    // fleetStuck is the 3D tray's line: held first, then its cut-off line, its way home where a harbour on its side can
    // take it, and the red coasts its guns can still shell.
    expect(browser).toContain('const held = heldLine(f), cut = held ? \'\' : cutOffLine(g, f);');
    expect(browser).toContain("return `${held || cut}${cut && harbourFor(g, f) ? ' Return to port takes it to a harbour on its side.' : ''}${shellAt.length ? ' Click a red coast to shell it.' : ''}`;");
    expect(browser).toContain('return heldLine(f) || cutOffLine(g, f);');

    // The rules give the browser what the bridge gives the 3D tray (fleetInHand's cut and homeTip): the same cut-off line,
    // and a way home exactly where a harbour on its side can take it.
    const { g, f } = outPast();
    TC().load(saveGame(g));
    const hand = TC().fleetInHand(f.id) as { cut: string; homeTip: string; held: string };
    expect(hand.cut).toBe(cutOffLine(g, f));
    expect(hand.held).toBe('');
    expect(!!hand.homeTip).toBe(!!harbourFor(g, f));
    expect(harbourFor(g, f)).not.toBeNull();
    for (let r = harbourFor(g, f); r; r = harbourFor(g, f)) r.ctrl = 'UK';
    TC().load(saveGame(g));
    const none = TC().fleetInHand(f.id) as { cut: string; homeTip: string };
    expect(none.cut).toBe(cutOffLine(g, f));
    expect(none.cut).toMatch(/No friendly harbour on its side can take it\.$/);
    expect(none.homeTip).toBe('');
  });

  it('held where this month\'s battle left it as well as cut off, it is refused with the held reason first, as the bridge answers', () => {
    const { g, f } = outPast();
    f.attacked = g.turn;
    const held = fleetOptions(g, f.id)!.held!;
    expect(held).toBe(`The ${f.name} was sent at the enemy this month: it stays where the battle left it until next month.`);
    expect(cutOffLine(g, f)).toMatch(/^Cut off: /);
    expect(harbourFor(g, f)).not.toBeNull();
    // Return to port would take it to a harbour on its side, but it is held: the rules refuse it, and every sea.
    expect(orderFleet(g, f.id, f.home, 'port')).toBe(false);
    expect(orderFleet(g, f.id, f.zone, 'raid')).toBe(false);
    // The bridge answers with the held reason, not the cut-off line, and its page's seas say the same.
    TC().load(saveGame(g));
    const home = (TC().force('fleet', f.id) as Panel).options.find((o) => o.key.endsWith(':port'))!;
    const out = TC().forceOrder(home.key) as { ok: boolean; note: string };
    expect(out.ok).toBe(false);
    expect(out.note).toBe(held);
    expect(page(f.id).sections.find((x) => x.heading === 'SEAS IN ITS REACH')?.text).toBe(held);
    // The 3D game's tray, and a right-click on land there, give the held reason before the cut-off line: fleetInHand has both.
    const hand = TC().fleetInHand(f.id) as { held: string; cut: string };
    expect(hand.held).toBe(held);
    expect(hand.cut).toBe(cutOffLine(g, f));
    // The browser asks the held reason first (fleetWhy, fleetStuck) and its list of seas gives it in place of the cut-off line.
    expect(browser).toContain('return f.attacked === g.turn ? fleetOptions(g, f.id)?.held ?? \'\' : \'\';');
    expect(browser).toContain('const cut = o.held ? \'\' : cutOffLine(g, f);');
    expect(browser).toContain('const reachLine = o.held ? esc(o.held) : !cut ? ');
  });
});
