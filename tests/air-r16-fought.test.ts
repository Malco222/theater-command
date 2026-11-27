// Release 1.6: a raid fought out as an air battle in the 3D game, through the whole campaign bridge (TC), on release
// 1.5's campaign. A 1942 grand save: a day raid and a fight for the sky, each started (TC.airStart), run to its end on
// the shared clock (TC.battleTick), settled through the campaign's own raid order (TC.battleEnd), saved and loaded.
// The field books its own losses in place of the dice (airfought.ts); everything after them is the raid's own.
import { describe, expect, it } from 'vitest';
import { planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { loadGame } from '../src/campaign/save';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;
const copy = (): Game => loadGame(TC.save().text)!;
const HUMAN = 'GER';

/** A 1942 grand campaign (May 1942, the computer having played the months before), and the first raid of a kind the field takes that the defence meets. */
function raid1942(kind: RaidKind): { target: number; ids: number[] } {
  TC.start({ view: 'grand', human: HUMAN, from: 33, seed: 11 });
  const g = copy();
  for (const t of raidTargets(g, kind, HUMAN)) {
    const p = planRaid(g, kind, t.id, undefined, HUMAN);
    if (!p.picked?.length) continue;
    // (one the defence comes up to meet, so the field has a fight to show)
    const look = TC.airLook(kind, t.id, p.picked);
    if (look.ticket && look.ticket.def.forces.length > 0) return { target: t.id, ids: p.picked };
  }
  throw new Error(`no ${kind} raid the field takes in May 1942`);
}

/** Runs the battle the bridge holds to its end, at the fastest speed, as the 3D game's clock does. */
function toTheEnd(): { frames: number; over: unknown } {
  TC.battleSpeed(4);
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  let v: any = null, n = 0;
  for (; n < 4000 && !(v && v.over); n++) v = TC.battleTick(0.5);
  return { frames: n, over: v?.over };
}

describe('release 1.6: a raid fought out through the campaign bridge (1942 grand save)', () => {
  for (const kind of ['bomb', 'superiority'] as const) {
    it(`a ${kind === 'bomb' ? 'day raid' : 'fight for the sky'}: started, fought to the end, settled once, saved and loaded`, () => {
      const { target, ids } = raid1942(kind);
      const before = copy();
      const st = TC.airStart(kind, target, ids);
      expect(st.sky.bands).toEqual([1800, 4500, 7200]);
      expect(st.sky.att.nat).toBe(HUMAN);
      // (at the start only the raid is up: the defence rises on the warning)
      expect((st.view.units as { side: string }[]).some((u) => u.side === 'att')).toBe(true);
      expect(TC.battleEnd()).toBeNull();
      const run = toTheEnd();
      expect(run.over).toBeTruthy();
      const out = TC.battleEnd();
      expect(out.ok).toBe(true);
      // The after card: its totals are its rows' sums, and its rows are the campaign's report (the campaign's bill).
      const rows = out.after.groups as { side: 'att' | 'def'; lostFtr: number; lostBmr: number }[];
      const sum = (side: string, k: 'lostFtr' | 'lostBmr') => rows.filter((x) => x.side === side).reduce((t, x) => t + x[k], 0);
      expect(out.after.lost).toEqual({ att: { ftr: sum('att', 'lostFtr'), bmr: sum('att', 'lostBmr') }, def: { ftr: sum('def', 'lostFtr'), bmr: sum('def', 'lostBmr') } });
      const g = copy();
      expect(g.airReports.length).toBe(before.airReports.length + 1);
      const rep = g.airReports[g.airReports.length - 1];
      expect(rep.mission).toBe(kind);
      expect(rep.region).toBe(target);
      expect(rep.def.length).toBeGreaterThan(0);
      expect(rep.att.reduce((t, x) => t + x.lostFtr, 0)).toBe(out.after.lost.att.ftr);
      expect(rep.def.reduce((t, x) => t + x.lostFtr, 0)).toBe(out.after.lost.def.ftr);
      if (kind === 'bomb') expect(rep.dropped).toBeGreaterThanOrEqual(0);
      // The groups lost what the report says.
      for (const s of rep.att) {
        const was = before.airFleets.find((f) => f.id === s.id)!, now = g.airFleets.find((f) => f.id === s.id);
        if (now) expect(was.ftr - now.ftr).toBe(s.lostFtr);
      }
      // Settled once: no battle is held now.
      expect(() => TC.battleEnd()).toThrow();
      // Saved and loaded, the raid stands, and the month closes.
      const text = TC.save().text;
      TC.load(text);
      expect(TC.save().text).toBe(text);
      expect(copy().airReports.length).toBe(g.airReports.length);
      const turn = copy().turn;
      TC.endMonth();
      expect(copy().turn).toBeGreaterThan(turn);
    });
  }

  it('the field runs only day raids and fights for the sky: a night raid is the staff\'s, in words', () => {
    TC.start({ view: 'grand', human: HUMAN, from: 33, seed: 11 });
    const g = copy();
    const t = raidTargets(g, 'night', HUMAN)[0];
    if (!t) return;
    const p = planRaid(g, 'night', t.id, undefined, HUMAN);
    const look = TC.airLook('night', t.id, p.picked ?? []);
    expect(look.ticket).toBeNull();
    expect(look.why).toMatch(/night/);
  });

  it('fought or rolled, a fight for the sky books the same campaign records (air campaigns, the sky over the province)', () => {
    const { target, ids } = raid1942('superiority');
    const rolled = TC.act(`raid:superiority:${target}:${ids.join('.')}`);
    expect(rolled.ok).toBe(true);
    const a = copy();
    const { target: t2, ids: ids2 } = raid1942('superiority');
    expect([t2, ids2]).toEqual([target, ids]);
    TC.airStart('superiority', target, ids);
    toTheEnd();
    expect(TC.battleEnd().ok).toBe(true);
    const b = copy();
    expect(b.airReports.length).toBe(a.airReports.length);
    expect(Object.keys(b.ac?.sky ?? {}).sort()).toEqual(Object.keys(a.ac?.sky ?? {}).sort());
    expect(b.ac?.sky?.[target]?.by).toBe(a.ac?.sky?.[target]?.by);
  });
});
