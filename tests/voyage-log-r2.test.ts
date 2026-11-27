// The voyage log (release 2, voyage.ts; Brian 24 November 2026 19:47Z, "yes to all"): each fleet writes where it was and
// what it fought at every month's end, shown on its page newest first. A record only. Behind its own switch.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { createGame } from '../src/campaign/game';
import { setFleetAttack, orderFleetAttack } from '../src/campaign/fleetattack';
import { setFog } from '../src/campaign/intel';
import { saveGame } from '../src/campaign/save';
import { VOYAGELOG_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import { VOYAGE, setVoyageLog, voyageLines, voyageOn } from '../src/campaign/voyage';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

function germany(on: boolean): Game {
  const g = startOpen(createGame(3, 'GER'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  setVoyageLog(g, on);
  setFog(g, false);
  beginHuman(g);
  return g;
}
function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
}
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;

describe('the voyage log', () => {
  it('ships off, and off nothing is written or shown', () => {
    expect((stage as { voyageLog?: boolean }).voyageLog).toBe(false);
    expect(VOYAGELOG_STAGE).toBe(false);
    const g = germany(false);
    expect(voyageOn(g)).toBe(false);
    month(g);
    expect(g.fleets.some((f) => f.voyage !== undefined)).toBe(false);
    expect(voyageLines(g, fleet(g, 'GER', 'Battle Fleet'))).toEqual([]);
  });

  it("on, every fleet writes a line at each month's end, and the page reads it newest first", () => {
    const g = germany(true);
    const first = g.turn;
    month(g);
    month(g);
    const battle = fleet(g, 'GER', 'Battle Fleet');
    expect(battle.voyage!.map((e) => e.t)).toEqual([first, first + 1]);
    // Every country's fleets keep one: it is a record of the war, not of the player's side.
    expect(fleet(g, 'UK', 'Home Fleet').voyage!.length).toBe(2);
    const lines = voyageLines(g, battle);
    expect(lines.length).toBeGreaterThan(0);
    expect(lines[0]).toMatch(/1939: (in harbour at|on patrol|raiding|hunting|escorting)/);
  });

  it('quiet months doing the same thing run together; the newest come first; it keeps three years', () => {
    const g = germany(true);
    const f = fleet(g, 'GER', 'Battle Fleet');
    f.voyage = [
      { t: 1, s: 'port', z: f.home, p: 'Wilhelmshaven' },
      { t: 2, s: 'port', z: f.home, p: 'Wilhelmshaven' },
      { t: 3, s: 'port', z: f.home, p: 'Wilhelmshaven' },
      { t: 4, s: 'patrol', z: f.home, p: 'Wilhelmshaven', f: ['Won a fleet action in the North Sea against the Home Fleet: sank 1 cruiser, lost no ships.'] },
      { t: 5, s: 'port', z: f.home, p: 'Wilhelmshaven' },
    ];
    const lines = voyageLines(g, f);
    expect(lines[0]).toBe('January 1940: in harbour at Wilhelmshaven.');
    expect(lines[1]).toMatch(/^December 1939: on patrol .*\. Won a fleet action/);
    expect(lines[2]).toBe('September to November 1939: in harbour at Wilhelmshaven.');
    for (let k = 0; k < VOYAGE.keep + 5; k++) month(g);
    expect(f.voyage!.length).toBe(VOYAGE.keep);
  });

  it('a fleet action is written in the log of every fleet that fought it, from its own side', () => {
    const g = germany(true);
    setFleetAttack(g, true);
    const battle = fleet(g, 'GER', 'Battle Fleet'), home = fleet(g, 'UK', 'Home Fleet');
    const out = orderFleetAttack(g, battle.id, home.id);
    expect(out?.outcome).toBe('fought');
    const mine = battle.voyage!.find((e) => e.t === g.turn)!, theirs = home.voyage!.find((e) => e.t === g.turn)!;
    expect(mine.f![0]).toMatch(/^(Won|Lost) a fleet action .* against the Home Fleet: sank .*, lost .*\.$/);
    expect(theirs.f![0]).toMatch(/^(Won|Lost) a fleet action .* against the Battle Fleet/);
    expect(mine.f![0].startsWith('Won')).toBe(!theirs.f![0].startsWith('Won'));
    // Through the bridge: the fleet's page has the log.
    type Call = (...a: unknown[]) => unknown;
    const TC = (globalThis as unknown as { TC: Record<string, Call> }).TC;
    TC.load(saveGame(g));
    const page = TC.page(`fleet:${battle.id}`) as { sections: { heading: string; text: string }[] };
    expect(page.sections.find((x) => x.heading === 'VOYAGE LOG')!.text).toMatch(/fleet action/);
    expect((TC.act('voyagelogrule:off') as { ok: boolean }).ok).toBe(true);
    expect((TC.page(`fleet:${battle.id}`) as { sections: { heading: string }[] }).sections.some((x) => x.heading === 'VOYAGE LOG')).toBe(false);
  });
});
