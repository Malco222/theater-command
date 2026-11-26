// 1.5.1 (Brian, 24 November 2026 18:32Z): "when i move my navy they go right back to where they were before the next
// turn. they dont stay where they were moved." At Theatre command (the depth the new-campaign screen offers first) the
// admirals run the navy desk, and every month they gave each fleet its orders afresh, his own included. A fleet he
// orders himself now stays where he sent it until he hands it back to them, or hands them the navy afresh.
import { describe, expect, it } from 'vitest';
import { setDesk } from '../src/campaign/depth';
import { handOver } from '../src/campaign/desks';
import { createGame } from '../src/campaign/game';
import { atSea, fleetsOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, fleetOptions, handBackFleet, humanUp, orderFleet, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

/** A grand campaign at the player's first month, with the admirals running the navy (Theatre command). */
function admirals(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  setDesk(g, 'navy', true);
  beginHuman(g);
  return g;
}
function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
}
/** A sea in the fleet's reach other than the one it is in, with a patrol that would not be thrown out at once. */
function elsewhere(g: Game, f: Fleet): number {
  const seas = fleetOptions(g, f.id)!.seas.filter((z) => !z.here && z.meet !== 'battle');
  return (seas.find((z) => z.meet === 'clear') ?? seas[0]).zone;
}
/** A fleet of the player's that can be ordered, with ships, at war. */
const pick = (g: Game): Fleet => fleetsOf(g, g.human!).filter((f) => Object.values(f.ships).some((n) => n > 0) && fleetOptions(g, f.id)!.seas.length > 1)[0];

describe('a fleet the player moves stays where he sent it', () => {
  it('under the admirals, his order holds into the next month and the ones after', () => {
    // (Britain's Home Fleet, sent to raid in the North Atlantic: no German fleet comes out to meet it there.)
    const g = admirals('UK', 4);
    const f = pick(g);
    expect(f).toBeTruthy();
    const to = elsewhere(g, f);
    expect(orderFleet(g, f.id, to, 'raid')).toBe(true);
    let held = 0;
    for (let k = 0; k < 3; k++) {
      month(g);
      const now = g.fleets.find((x) => x.id === f.id);
      if (!now || !Object.values(now.ships).some((n) => n > 0)) break; // (sunk: nothing to keep)
      // Beaten in a battle a fleet makes for harbour, as before; otherwise it is where he sent it.
      if (now.fought === g.turn - 1 || now.fought === g.turn) break;
      expect([now.zone, now.stance]).toEqual([to, 'raid']);
      held++;
    }
    expect(held).toBe(3);
  });

  it('beaten in battle it makes for harbour as before, and waits there for his next order', () => {
    // (The German battle fleet, sent to raid in the North Sea, meets the Home Fleet there in its second month.)
    const g = admirals('GER', 3);
    const f = pick(g);
    orderFleet(g, f.id, elsewhere(g, f), 'raid');
    let beaten = -1;
    for (let k = 0; k < 4; k++) {
      month(g);
      if (beaten < 0 && f.fought === g.turn - 1 && !atSea(f)) beaten = g.turn;
    }
    expect(beaten).toBeGreaterThan(0);
    expect(atSea(f)).toBe(false);
    expect(f.ordered).toBe(true);
  });

  it('the other fleets are still the admirals\' to sail', () => {
    const g = admirals();
    const f = pick(g);
    orderFleet(g, f.id, elsewhere(g, f), 'raid');
    expect(f.ordered).toBe(true);
    expect(fleetsOf(g, 'GER').filter((x) => x !== f).every((x) => !x.ordered)).toBe(true);
  });

  it('handed back, the admirals take it again; handing them the navy afresh takes every fleet back', () => {
    const g = admirals();
    const f = pick(g);
    orderFleet(g, f.id, elsewhere(g, f), 'raid');
    expect(handBackFleet(g, f.id)).toBe(true);
    expect(f.ordered).toBeUndefined();
    expect(handBackFleet(g, f.id)).toBe(false);
    orderFleet(g, f.id, f.home, 'port');
    expect(f.ordered).toBe(true);
    expect(atSea(f)).toBe(false);
    handOver(g, 'navy', false);
    handOver(g, 'navy', true);
    expect(fleetsOf(g, 'GER').some((x) => x.ordered)).toBe(false);
  });

  it('the mark is kept in a saved game, and a computer nation never has it', () => {
    const g = admirals();
    const f = pick(g);
    orderFleet(g, f.id, elsewhere(g, f), 'raid');
    const h = loadGame(saveGame(g));
    expect(h!.fleets.find((x) => x.id === f.id)!.ordered).toBe(true);
    month(g);
    expect(g.fleets.filter((x) => x.nat !== 'GER').some((x) => x.ordered)).toBe(false);
  });
});

// ---- Through the 3D game's doorway ------------------------------------------------------------------------------
type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface PageOut { note: string; top: { label: string; act: string; ok: boolean }[] }
interface Panel { lines: string[]; options: { key: string; label: string; ok: boolean }[] }

describe('the fleet\'s page and panel say who sails it', () => {
  it('under the admirals: the line, the order through the panel, and HAND BACK', () => {
    const g = admirals('UK', 4);
    const f = pick(g);
    const to = elsewhere(g, f);
    TC().load(saveGame(g));
    const page = (): PageOut => TC().page(`fleet:${f.id}`) as PageOut;
    const panel = (): Panel => TC().force('fleet', f.id) as Panel;
    expect(page().note).toMatch(/Your admirals sail it each month/);
    expect(page().top.some((b) => b.act.startsWith('fleetstaff:'))).toBe(false);
    expect(panel().lines.join(' ')).toMatch(/Your admirals sail it each month/);
    // The map's drag sends the order through the panel's doorway.
    expect((TC().forceOrder(`fleet:${f.id}:${to}:raid`) as { ok: boolean }).ok).toBe(true);
    expect(page().note).toMatch(/Under your own orders/);
    expect(page().top.find((b) => b.act === `fleetstaff:${f.id}`)?.ok).toBe(true);
    expect(panel().options.find((o) => o.key === `fleet:${f.id}:staff`)?.ok).toBe(true);
    const back = TC().act(`fleetstaff:${f.id}`) as { ok: boolean; note: string };
    expect(back.ok).toBe(true);
    expect(back.note).toMatch(/admirals' to sail again/);
    expect(page().note).toMatch(/Your admirals sail it each month/);
    expect((TC().forceOrder(`fleet:${f.id}:${to}:patrol`) as { ok: boolean }).ok).toBe(true);
    expect((TC().forceOrder(`fleet:${f.id}:staff`) as { ok: boolean }).ok).toBe(true);
    expect((TC().forceOrder(`fleet:${f.id}:staff`) as { ok: boolean }).ok).toBe(false);
  });

  it('with the navy on his own desk nothing is said and nothing is offered', () => {
    const g = admirals('UK', 4);
    setDesk(g, 'navy', false);
    const f = pick(g);
    TC().load(saveGame(g));
    expect((TC().forceOrder(`fleet:${f.id}:${elsewhere(g, f)}:raid`) as { ok: boolean }).ok).toBe(true);
    const page = TC().page(`fleet:${f.id}`) as PageOut;
    expect(page.note).not.toMatch(/admirals/);
    expect(page.top.some((b) => b.act.startsWith('fleetstaff:'))).toBe(false);
  });
});
