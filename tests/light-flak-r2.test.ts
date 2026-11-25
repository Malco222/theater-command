// Light flak on the map (release 2; the art director's flak brief, 2c). Bombers over a target on land with no guns
// built still lose some of their number to the light guns any target puts up. The 3D map shows that as light flak
// round the target when a bomber flight's row says so (`light`, a count above 0); a target with guns built shows
// the guns' own ring (`aa`) and never both.
import { describe, expect, it } from 'vitest';
import { AIR, standingGuns } from '../src/campaign/air';
import { planRaid, raidTargets } from '../src/campaign/airwar';
import { workLevel } from '../src/campaign/arsenal';
import { createGame } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
type Row = { bombers: boolean; escort: boolean; region: number; aa: number; night?: boolean; light?: number };
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

/** Germany at the table in its first month, and a raid of the kind asked for that it can fly on a target with no guns built: bombers alone, or with fighters beside them. */
function table(kind: 'bomb' | 'night' | 'superiority', escorted = false): { g: Game; target: number; ids: number[] } {
  const g = startOpen(createGame(3, 'GER'));
  let guard = 0;
  while (!humanUp(g) && guard++ < 200) stepAI(g);
  beginHuman(g);
  for (const t of raidTargets(g, kind, 'GER')) {
    if (workLevel(g, 'aa', t.id) !== 0) continue;
    const p = planRaid(g, kind, t.id, undefined, 'GER');
    if (p.why || !p.picked.length) continue;
    const kinds = (ids: number[]) => ids.map((id) => p.seats.find((x) => x.id === id)!.kind);
    if (kind === 'superiority') { if (kinds(p.picked).every((k) => k === 'ftr')) return { g, target: t.id, ids: p.picked }; continue; }
    if (!escorted) return { g, target: t.id, ids: p.picked };
    const escort = p.seats.find((x) => x.kind === 'ftr' && !x.why && !x.picked);
    if (escort) return { g, target: t.id, ids: [...p.picked, escort.id] };
  }
  throw new Error(`no ${kind} raid on a target without guns`);
}
const flown = (g: Game, kind: string, target: number, ids: number[]): Row[] => {
  TC().load(saveGame(g));
  const out = TC().act(`raid:${kind}:${target}:${ids.join('.')}`) as { ok: boolean; flies: Row[] };
  expect(out.ok).toBe(true);
  expect(out.flies.length).toBeGreaterThan(0);
  return out.flies;
};

describe('light flak on a raid\'s flights (release 2)', () => {
  it('bombers over a target with no guns built: every bomber flight says light flak, with the count of the light guns any target puts up', () => {
    expect(AIR.combat.flak).toBeGreaterThan(0);
    const { g, target, ids } = table('bomb');
    const rows = flown(g, 'bomb', target, ids);
    expect(rows.every((r) => r.bombers)).toBe(true);
    for (const r of rows) { expect(r.region).toBe(target); expect(r.aa).toBe(0); expect(r.light).toBe(standingGuns()); }
    expect(standingGuns()).toBeGreaterThan(0);
  });

  it('the fighters that go with them say nothing: light flak is the bombers\' row alone', () => {
    const { g, target, ids } = table('bomb', true);
    const rows = flown(g, 'bomb', target, ids);
    expect(rows.some((r) => r.bombers) && rows.some((r) => !r.bombers)).toBe(true);
    for (const r of rows) expect(r.light).toBe(r.bombers ? standingGuns() : undefined);
  });

  it('a fight for the sky by fighters alone has no light flak', () => {
    const { g, target, ids } = table('superiority');
    for (const r of flown(g, 'superiority', target, ids)) { expect(r.bombers).toBe(false); expect(r.light).toBeUndefined(); }
  });

  it('by night the light guns still fire', () => {
    expect(AIR.combat.night.flak).toBeGreaterThan(0);
    const { g, target, ids } = table('night');
    const rows = flown(g, 'night', target, ids);
    for (const r of rows) { expect(r.night).toBe(true); expect(r.light).toBe(r.bombers ? standingGuns() : undefined); }
    expect(rows.some((r) => r.light)).toBe(true);
  });

  it('over built guns no flight says light flak, at any level: the ring is the guns\' own', () => {
    for (const level of [1, 3]) {
      const { g, target, ids } = table('bomb');
      g.flags[`wk:aa:${target}`] = level;
      const rows = flown(g, 'bomb', target, ids);
      expect(rows[0].aa).toBe(level);
      for (const r of rows) expect('light' in r).toBe(false);
    }
  });
});
