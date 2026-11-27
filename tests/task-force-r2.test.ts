// Task forces (release 2, taskforce.ts; Brian 24 November 2026 18:24Z, "i like these"): one of the player's fleets at sea
// on a job is made a task force; each month it comes in to refit when worn and goes back to its job when fit, until he
// changes it. Behind its own switch.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { atSea, fleetsOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { TASKFORCE, onJob, setTaskForce, taskForceOn, taskLine } from '../src/campaign/taskforce';
import { beginHuman, endHuman, fleetOptions, humanUp, orderEscort, orderFleet, orderTaskForce, releaseTaskForce, stepAI, whyNotTaskForce } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

function britain(on: boolean): Game {
  const g = startOpen(createGame(4, 'UK'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  if (on) setTaskForce(g, true);
  beginHuman(g);
  return g;
}
function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
}
const home = (g: Game): Fleet => fleetsOf(g, 'UK').find((f) => f.name === 'Home Fleet')!;
/** The Home Fleet sent to raid in the North Atlantic, where no German fleet comes out to meet it (navy-stays-151). */
function sent(g: Game): Fleet {
  const f = home(g);
  const to = fleetOptions(g, f.id)!.seas.find((z) => !z.here && z.meet === 'clear')!.zone;
  expect(orderFleet(g, f.id, to, 'raid')).toBe(true);
  return f;
}

describe('task forces', () => {
  it('off: none can be made, and a campaign without the switch has none', () => {
    const g = britain(false);
    expect(taskForceOn(g)).toBe(false);
    const f = sent(g);
    expect(whyNotTaskForce(g, f.id)).toMatch(/switched on/);
    expect(orderTaskForce(g, f.id)).toBe(false);
    expect(taskLine(g, f)).toBe('');
  });

  it('a fleet in harbour is sent out on a job first; at sea its job becomes the standing order', () => {
    const g = britain(true);
    const f = home(g);
    orderFleet(g, f.id, f.home, 'port');
    expect(whyNotTaskForce(g, f.id)).toMatch(/Send it out on a job first/);
    sent(g);
    expect(whyNotTaskForce(g, f.id)).toBeNull();
    expect(orderTaskForce(g, f.id)).toBe(true);
    expect(f.task).toEqual({ zone: f.zone, stance: 'raid' });
    expect(f.ordered).toBe(true);
    expect(whyNotTaskForce(g, f.id)).toMatch(/already/);
    expect(taskLine(g, f)).toMatch(/^Task force: it raids shipping in the /);
  });

  it('worn, it comes in to refit; fit again, it goes back to its job by itself', () => {
    const g = britain(true);
    const f = sent(g);
    orderTaskForce(g, f.id);
    const job = { ...f.task! };
    f.ready = 0.3; // (as after a battle lost at sea)
    month(g);
    expect(atSea(f)).toBe(false);
    expect(g.headlines.some((h) => h.turn === g.turn && /comes into .* to refit/.test(h.text))).toBe(true);
    expect(taskLine(g, f)).toMatch(/refitting/);
    let back = -1;
    for (let k = 0; k < 4 && back < 0; k++) { month(g); if (onJob(f)) back = g.turn; }
    expect(back).toBeGreaterThan(0);
    expect(f.ready).toBeGreaterThanOrEqual(TASKFORCE.back);
    expect({ zone: f.zone, stance: f.stance }).toEqual({ zone: job.zone, stance: job.stance });
    expect(g.headlines.some((h) => h.turn === back && /is back on its job/.test(h.text))).toBe(true);
  });

  it('a new order rewrites the job; an order into harbour or RELEASE ends it', () => {
    const g = britain(true);
    const f = sent(g);
    orderTaskForce(g, f.id);
    const other = fleetOptions(g, f.id)!.seas.find((z) => z.zone !== f.zone && z.meet === 'clear')!.zone;
    orderFleet(g, f.id, other, 'patrol');
    expect(f.task).toEqual({ zone: other, stance: 'patrol' });
    orderFleet(g, f.id, f.home, 'port');
    expect(f.task).toBeUndefined();
    sent(g);
    orderTaskForce(g, f.id);
    expect(releaseTaskForce(g, f.id)).toBe(true);
    expect(f.task).toBeUndefined();
    expect(f.ordered).toBe(true);
  });

  it('an escort task force keeps to its lane, through a refit', () => {
    const g = britain(true);
    const f = fleetsOf(g, 'UK').find((x) => x.name === 'Western Approaches Fleet')!;
    const lane = f.lane ?? fleetOptions(g, f.id)!.lanes.find((l) => !l.why)!.lane;
    if (f.stance !== 'escort') expect(orderEscort(g, f.id, lane)).toBe(true);
    expect(orderTaskForce(g, f.id)).toBe(true);
    expect(f.task).toMatchObject({ stance: 'escort', lane });
    f.ready = 0.3;
    month(g);
    expect(atSea(f)).toBe(false);
    for (let k = 0; k < 4 && !onJob(f); k++) month(g);
    expect(f.stance).toBe('escort');
    expect(f.lane).toBe(lane);
  });

  it('kept in a saved game; switched off, every task force is released; no computer nation has one', () => {
    const g = britain(true);
    const f = sent(g);
    orderTaskForce(g, f.id);
    const h = loadGame(saveGame(g))!;
    expect(h.fleets.find((x) => x.id === f.id)!.task).toEqual(f.task);
    month(g);
    expect(g.fleets.some((x) => x.nat !== 'UK' && x.task)).toBe(false);
    setTaskForce(g, false);
    expect(g.fleets.some((x) => x.task)).toBe(false);
  });
});

// ---- Through the 3D game's doorway ------------------------------------------------------------------------------
type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface PageOut { note: string; top: { label: string; act: string; ok: boolean; tip: string }[] }
interface Panel { lines: string[]; options: { key: string; label: string; ok: boolean; why?: string }[] }

describe('task forces through the bridge', () => {
  it('the switch, the page button, the panel key and the line', () => {
    const g = britain(false);
    const f = sent(g);
    TC().load(saveGame(g));
    const page = (): PageOut => TC().page(`fleet:${f.id}`) as PageOut;
    const panel = (): Panel => TC().force('fleet', f.id) as Panel;
    expect((TC().view() as { taskForce: boolean }).taskForce).toBe(false);
    expect(page().top.some((b) => b.act.startsWith('taskforce:'))).toBe(false);
    expect((TC().act('taskforcerule:on') as { ok: boolean }).ok).toBe(true);
    expect((TC().view() as { taskForce: boolean }).taskForce).toBe(true);
    expect(page().top.find((b) => b.act === `taskforce:${f.id}:on`)?.ok).toBe(true);
    const made = TC().act(`taskforce:${f.id}:on`) as { ok: boolean; note: string };
    expect(made.ok).toBe(true);
    expect(made.note).toMatch(/is a task force: it raids shipping/);
    expect(page().note).toMatch(/Task force: it raids shipping/);
    expect(page().top.find((b) => b.act === `taskforce:${f.id}:off`)?.ok).toBe(true);
    expect(panel().lines.join(' ')).toMatch(/Task force/);
    expect(panel().options.find((o) => o.key === `fleet:${f.id}:untask`)?.ok).toBe(true);
    expect((TC().forceOrder(`fleet:${f.id}:untask`) as { ok: boolean }).ok).toBe(true);
    expect(panel().options.find((o) => o.key === `fleet:${f.id}:task`)?.ok).toBe(true);
    expect((TC().forceOrder(`fleet:${f.id}:task`) as { ok: boolean }).ok).toBe(true);
    expect((TC().act('taskforcerule:off') as { ok: boolean }).ok).toBe(true);
    expect(page().note).not.toMatch(/Task force/);
  });
});
