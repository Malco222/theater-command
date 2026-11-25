// Fog on raids on the far chart: the 'near' radius of a place never seen reads the defenders' real reach there (the
// join's read of r2-head-1125k, fault 2; joins/r2-head-1125k-READ.md).
//
// skyKnown's 'near' (airfog.ts), for a place never seen, counts the fighters last seen at places within the reach of
// its holder's fighters (fighterReach, air.ts). On this line reach() reads the far side's figures for a group based
// beyond Europe's chart (far-air.json ftrReach: Japan 1,048 km from month 13; the cover cap jobs.cover: America's land
// fighters 550 km outside a raid), but fighterReach asked with no base and so always read Europe's dated row (Japan
// 750/850, America up to 1,200). Now it asks as for a group based at the target. Off the far side nothing changes.
// Behind fog on raids (g.flags.rfog), off as shipped; the far chart behind TC_FAR.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { fighterReach, km } from '../src/campaign/air';
import { setRaidFog, skyKnown } from '../src/campaign/airfog';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game, Region } from '../src/campaign/types';
import { FAR_FIRST } from '../src/campaign/world';

function world(turn: number): Game {
  const g = startOpen(createGame(3, null));
  setRaidFog(g, true);
  g.turn = turn;
  return g;
}
const farHeld = (g: Game, nat: string): Region => g.regions.find((r) => r.id >= FAR_FIRST && r.ctrl === nat && r.fac > 0) ?? g.regions.find((r) => r.id >= FAR_FIRST && r.ctrl === nat)!;
/** One place this side saw fighters at, `ago` months back, and nothing else; the target never seen and out of sight. */
function nearOf(g: Game, nat: string, t: number, seenAt: number, fighters: number): number {
  ((g.intel as unknown as { airSeen?: Record<string, Record<number, number[]>> }).airSeen ??= {})[nat] = { [seenAt]: [g.turn, fighters, 1, 0, 0] };
  const k = skyKnown(g, nat, t, new Uint8Array(g.regions.length));
  expect(k.live).toBe(false);
  expect(k.ago).toBe(-1);
  return k.near;
}
/** A province at a distance from t between two figures (km, as the rules read it), else a failure that names the band. */
function placeBetween(g: Game, t: number, lo: number, hi: number): Region {
  const r = g.regions.find((x) => x.id !== t && km(g, x.id, t) > lo && km(g, x.id, t) < hi);
  if (!r) throw new Error(`no province between ${lo} and ${hi} km of ${g.regions[t].name}`);
  return r;
}

describe('fog\'s "near" for a place never seen reads the far side\'s reach of the defenders there', () => {
  it('a Japanese-held target: Japan\'s far-side reach (1,048 km from month 13), not Europe\'s row', () => {
    const g = world(20);
    const t = farHeld(g, 'JAP');
    expect(fighterReach(g, 'JAP', t.id)).toBe(1048);
    const europe = fighterReach(g, 'JAP');
    expect(europe).toBeLessThan(1048);
    // Fighters last seen at a place beyond Europe's row and within Japan's far-side reach are counted 'near'.
    const p = placeBetween(g, t.id, europe + 20, 1040);
    expect(nearOf(g, 'USA', t.id, p.id, 77)).toBe(77);
  });

  it('a US-held target late in the war: the cover cap (550 km), not Europe\'s 1,200', () => {
    const g = world(58);
    // (A US-held province of the far side with places at both distances: the Philippines in 1939.)
    const has = (t: number, lo: number, hi: number): boolean => g.regions.some((x) => x.id !== t && km(g, x.id, t) > lo && km(g, x.id, t) < hi);
    const t = g.regions.find((r) => r.id >= FAR_FIRST && r.ctrl === 'USA' && has(r.id, 600, 1150) && has(r.id, 100, 540))!;
    expect(t).toBeTruthy();
    expect(fighterReach(g, 'USA', t.id)).toBe(550);
    expect(fighterReach(g, 'USA')).toBe(1200);
    // Fighters last seen 600 to 1,150 km off are no longer counted; within 550 they are.
    const far = placeBetween(g, t.id, 600, 1150);
    expect(nearOf(g, 'JAP', t.id, far.id, 40)).toBe(-1);
    const close = placeBetween(g, t.id, 100, 540);
    expect(nearOf(g, 'JAP', t.id, close.id, 40)).toBe(40);
  });

  it('off the far side (a target of the old chart) the radius is Europe\'s row, as before', () => {
    const g = world(20);
    const t = g.byName.Ruhr;
    expect(t.id).toBeLessThan(FAR_FIRST);
    const R = fighterReach(g, t.ctrl);
    expect(fighterReach(g, t.ctrl, t.id)).toBe(R);
    const inside = placeBetween(g, t.id, R - 60, R - 1), outside = placeBetween(g, t.id, R + 1, R + 60);
    expect(nearOf(g, 'UK', t.id, inside.id, 12)).toBe(12);
    expect(nearOf(g, 'UK', t.id, outside.id, 12)).toBe(-1);
  });
});
