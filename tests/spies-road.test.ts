// Work chosen on the road (the design director, 22 November 2026, release 2, from the UI director's ease-of-play
// count): an agent sent abroad can be given its work at once, judged against where it is going, and the work begins
// the month the agent arrives: an act of sabotage is tried as that month opens ("Tried once, next month"), at the odds
// and risk the order showed. Sabotage that took two visits a month apart now takes one. If the target is gone when the
// agent arrives, the agent watches and reports instead, and the month's report says why (ruling 129(d)). An agent
// still in training is given no work. Only the player's agents have orders: computer-only wars do not move.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { intelTick, recruit, sendAgent, setMission, spyRoom, whyNotMission } from '../src/campaign/intel';
import { startOpen } from '../src/campaign/staff';
import type { Agent, Game } from '../src/campaign/types';

const grand = (): Game => startOpen(createGame(3, 'GER'));
function war(g: Game, a: string, b: string): void {
  if (!g.nations[a].atWar.includes(b)) g.nations[a].atWar.push(b);
  if (!g.nations[b].atWar.includes(a)) g.nations[b].atWar.push(a);
}
/** An agent at work in Berlin, sent this month to Paris: on the road, at work there next month. */
function onTheRoad(g: Game): Agent {
  const a: Agent = { id: g.intel.next++, nat: 'GER', name: `Road ${g.intel.next}`, xp: 0, reg: g.byName.Berlin.id, ready: g.turn - 1, mission: 'guard', born: g.turn - 3 };
  g.intel.agents.push(a);
  expect(sendAgent(g, 'GER', a.id, g.byName.Paris.id)).toBe(true);
  return a;
}
const notesOf = (g: Game, a: Agent) => g.intel.notes.filter((n) => n.agent === a.name && n.turn === g.turn);

describe('work chosen on the road', () => {
  it('can be given for where the agent is going, and the card says what it will do on arrival', () => {
    const g = grand();
    war(g, 'GER', 'FRA');
    const a = onTheRoad(g);
    expect(a.mission).toBe('watch');
    expect(whyNotMission(g, 'GER', a.id, 'factory')).toBeNull();
    expect(whyNotMission(g, 'GER', a.id, 'books')).toBeNull();
    // (judged against Paris: no oilfield there, and no guard duty on enemy ground)
    expect(whyNotMission(g, 'GER', a.id, 'oil')).not.toBeNull();
    expect(whyNotMission(g, 'GER', a.id, 'guard')).toMatch(/friendly ground/);
    expect(setMission(g, 'GER', a.id, 'factory')).toBe(true);
    const view = spyRoom(g)!.agents.find((x) => x.id === a.id)!;
    expect(view.atWork).toBe(false);
    expect(view.training).toBe(false);
    expect(view.doing).toBe('On the road to Paris: will wreck a factory there on arrival');
    expect(view.missions.find((m) => m.id === 'factory')!.now).toBe(true);
  });

  it('is never given to an agent still in training', () => {
    const g = grand();
    g.nations.GER.pp = 10000;
    const a = recruit(g, 'GER')!;
    expect(spyRoom(g)!.agents.find((x) => x.id === a.id)!.training).toBe(true);
    for (const m of ['guard', 'hunt'] as const) expect(whyNotMission(g, 'GER', a.id, m)).toMatch(/not yet at work/);
  });

  it('is tried as the month the agent arrives opens', () => {
    const g = grand();
    war(g, 'GER', 'FRA');
    const a = onTheRoad(g);
    setMission(g, 'GER', a.id, 'factory');
    g.turn++;
    intelTick(g, 'GER');
    // (the attempt is made before the month's risk is drawn: whatever became of the agent, the attempt is on file)
    expect(notesOf(g, a).some((n) => n.what === 'sabotage' || n.what === 'failed')).toBe(true);
    expect(notesOf(g, a).some((n) => n.what === 'arrived')).toBe(false);
  });

  it('falls back to watching when the target is gone on arrival, and the report says why', () => {
    const g = grand();
    war(g, 'GER', 'FRA');
    const a = onTheRoad(g);
    setMission(g, 'GER', a.id, 'factory');
    g.byName.Paris.fac = 0;
    g.turn++;
    intelTick(g, 'GER');
    const text = `Agent ${a.name} reaches Paris, but the order to wreck a factory cannot be carried out. There is no industry at Paris. The agent watches and reports instead.`;
    expect(notesOf(g, a).map((n) => n.text)).toContain(text);
    expect(g.headlines.some((h) => h.text === text)).toBe(true);
    expect(notesOf(g, a).some((n) => n.what === 'sabotage' || n.what === 'failed')).toBe(false);
    if (g.intel.agents.includes(a)) expect(a.mission).toBe('watch');
  });

  it('changes nothing for an agent sent with no work chosen: it arrives watching, with no report', () => {
    const g = grand();
    war(g, 'GER', 'FRA');
    const a = onTheRoad(g);
    g.turn++;
    intelTick(g, 'GER');
    expect(notesOf(g, a).some((n) => n.what === 'arrived' || n.what === 'sabotage' || n.what === 'failed')).toBe(false);
    if (g.intel.agents.includes(a)) expect(a.mission).toBe('watch');
  });
});
