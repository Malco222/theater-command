// A fleet that attacked this month is not offered the orders the rules will refuse it (1.5.1; the playtester, 25 November
// 2026: "RETURN TO HARBOUR offered then refused". A fleet sent at the enemy fleet, and since ruling A every fleet that
// joined it, stays where the battle left it until next month, and turn.ts refuses its sailing, harbour, new base and
// escort orders. The panel and the page showed those orders as open, and a press was answered "That order cannot be
// given now.") Now each is dimmed with the reason, and an order sent from the map anyway is answered with it.
import { describe, expect, it } from 'vitest';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
type Fleet = { id: number; name: string; nat: string; zone: number; home: number; stance: string; reach: number[]; bases: number[] };
type Option = { key: string; label: string; ok: boolean; why: string };
type Button = { label: string; act: string; ok: boolean; tip: string };
const HELD = /was sent at the enemy this month: it stays where the battle left it until next month/;

/** Britain in September 1939 as the 3D game begins it (seed 4, Theatre command), with fleets sent at fleets. */
function britain(): void {
  TC().start({ human: 'UK', from: 1, seed: 4, view: 'grand', depth: 'theatre' });
  TC().fleetAttack(true);
}
const fleet = (name: string): Fleet => (TC().view() as { fleets: Fleet[] }).fleets.find((f) => f.name === name)!;
const harbour = (id: number): Button => (TC().page(`fleet:${id}`) as { top: Button[] }).top.find((b) => b.label === 'RETURN TO HARBOUR')!;
const options = (id: number): Option[] => (TC().force('fleet', id) as { options: Option[] }).options;

describe('a fleet that attacked this month is not offered what the rules refuse it', () => {
  it('before the attack its harbour and seas are open; after it, each is dimmed with the reason', () => {
    britain();
    const home = fleet('Home Fleet');
    const battle = (TC().view() as { fleets: Fleet[] }).fleets.find((f) => f.name === 'Battle Fleet')!;
    expect(harbour(home.id).ok).toBe(true);
    expect(options(home.id).filter((o) => o.ok).length).toBeGreaterThan(0);

    const fought = TC().act(`fleetattack:${home.id}:${battle.id}`) as { ok: boolean };
    expect(fought.ok).toBe(true);

    // The page: RETURN TO HARBOUR is dim, and says why.
    const button = harbour(home.id);
    expect(button.ok).toBe(false);
    expect(button.tip).toMatch(HELD);
    // The panel: every sea, every raid and the harbour, dim and with the reason (the hand-back is not a sailing order).
    const sailing = options(home.id).filter((o) => !o.key.endsWith(':staff'));
    expect(sailing.length).toBeGreaterThan(2);
    for (const o of sailing) {
      expect(o.ok).toBe(false);
      expect(o.why).toMatch(o.key.endsWith(':port') || o.why === 'It is doing so now.' ? /./ : HELD);
    }
    expect(sailing.find((o) => o.key.endsWith(':port'))!.why).toMatch(HELD);
  });

  it('the map tints no port to offer a held fleet a new home, and a new base sent anyway is answered with the reason', () => {
    britain();
    const home = fleet('Home Fleet');
    const battle = (TC().view() as { fleets: Fleet[] }).fleets.find((f) => f.name === 'Battle Fleet')!;
    const offered = home.bases;
    expect(offered.length).toBeGreaterThan(0);
    expect((TC().act(`fleetattack:${home.id}:${battle.id}`) as { ok: boolean }).ok).toBe(true);
    expect(fleet('Home Fleet').bases).toEqual([]);
    const out = TC().forceOrder(`fleetbase:${home.id}:${offered[0]}`) as { ok: boolean; note: string };
    expect(out.ok).toBe(false);
    expect(out.note).toMatch(HELD);
    // The hand-back is not a sailing order: a refused one keeps its own words.
    expect((TC().forceOrder(`fleet:${home.id}:staff`) as { ok: boolean }).ok).toBe(true);
    const again = TC().forceOrder(`fleet:${home.id}:staff`) as { ok: boolean; note: string };
    expect(again.ok).toBe(false);
    expect(again.note).toBe('That order cannot be given now.');
    // Next month its ports are offered again.
    TC().endMonth();
    expect(fleet('Home Fleet').bases.length).toBeGreaterThan(0);
  });

  it('an order sent anyway, from the map or the page, is answered with the reason', () => {
    britain();
    const home = fleet('Home Fleet');
    const battle = (TC().view() as { fleets: Fleet[] }).fleets.find((f) => f.name === 'Battle Fleet')!;
    expect((TC().act(`fleetattack:${home.id}:${battle.id}`) as { ok: boolean }).ok).toBe(true);
    const after = fleet('Home Fleet');
    const sea = after.reach.find((z) => z !== after.zone)!;
    // The map's right-click and its own-harbour click go through forceOrder; the page's buttons through act.
    for (const out of [
      TC().forceOrder(`fleet:${home.id}:${sea}:patrol`),
      TC().forceOrder(`fleet:${home.id}:${after.home}:port`),
      TC().act(`fleet:${home.id}:${after.home}:port`),
    ] as { ok: boolean; note: string }[]) {
      expect(out.ok).toBe(false);
      expect(out.note).toMatch(HELD);
    }
    // Still where the battle left it.
    expect(fleet('Home Fleet').zone).toBe(after.zone);
  });

  it('a fleet that did not attack is untouched, and next month the one that did is free again', () => {
    britain();
    const home = fleet('Home Fleet');
    const dover = fleet('Dover Fleet');
    const battle = (TC().view() as { fleets: Fleet[] }).fleets.find((f) => f.name === 'Battle Fleet')!;
    expect((TC().act(`fleetattack:${home.id}:${battle.id}`) as { ok: boolean }).ok).toBe(true);
    // The Dover Fleet fought nowhere: its harbour is open and its orders are as before.
    if (dover.stance !== 'port' && fleet('Dover Fleet').zone !== fleet('Home Fleet').zone) {
      expect(harbour(dover.id).ok).toBe(true);
      expect(options(dover.id).some((o) => o.ok)).toBe(true);
    }
    // A refusal for another reason keeps its old words (a sea out of reach).
    const out = TC().forceOrder(`fleet:${dover.id}:99999:patrol`) as { ok: boolean; note: string };
    expect(out.ok).toBe(false);
    expect(out.note).toBe('That order cannot be given now.');
    // Next month the Home Fleet may sail again.
    TC().endMonth();
    const next = fleet('Home Fleet');
    expect(harbour(next.id).ok).toBe(next.stance !== 'port');
    expect(harbour(next.id).tip).not.toMatch(HELD);
    expect(options(next.id).some((o) => o.ok)).toBe(true);
  });
});
