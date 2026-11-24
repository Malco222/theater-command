// Release 1.5's FLY IT YOURSELF: breaking off before contact must not win (the playtester's ruling-217 pass, 23 Nov
// 17:00Z, playtest/results-r15/fly-live). A side whose groups all go home before they meet an enemy has given the battle
// up, whether by BREAK OFF ALL or one group at a time: it loses, wins no sky, earns no honours, and the map still gets
// its note. A tie with nothing lost and nothing hit is not a win. Flown battles only: the dice and the staff are as before.
import { describe, expect, it } from 'vitest';
import { planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { loadGame } from '../src/campaign/save';
import type { Game } from '../src/campaign/types';
import { airBattle } from '../src/battle/air/sim';
import { airTicket } from '../src/battle/air/ticket';
import { skyFight, skyFought } from '../src/campaign/aircampaigns';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;
const copy = (): Game => loadGame(TC.save().text)!;
const HUMAN = 'GER';

/** The May 1942 grand save of the fought-raid test, and its first raid of a kind the defence comes up to meet (or, empty, that no defender meets). */
function raid1942(kind: RaidKind, empty = false): { target: number; ids: number[] } {
  TC.start({ view: 'grand', human: HUMAN, from: 33, seed: 11 });
  const g = copy();
  for (const t of raidTargets(g, kind, HUMAN)) {
    const p = planRaid(g, kind, t.id, undefined, HUMAN);
    if (!p.picked?.length) continue;
    const look = TC.airLook(kind, t.id, p.picked);
    if (look.ticket && (look.ticket.def.forces.length > 0) !== empty) return { target: t.id, ids: p.picked };
  }
  throw new Error(`no ${kind} raid in May 1942`);
}

type Way = 'all' | 'each' | 'none';
/** Fly it: at one battle second, break off all, send each group home in turn, or give no orders; then to the end. */
function fly(kind: RaidKind, way: Way, empty = false) {
  const { target, ids } = raid1942(kind, empty);
  const st = TC.airStart(kind, target, ids);
  TC.battleSpeed(1);
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  let v: any = TC.battleTick(1);
  const mine = (st.view.units as { id: number; side: string }[]).filter((u) => u.side === 'att').map((u) => u.id);
  if (way === 'all') expect(TC.battleOrder({ do: 'withdrawAll', side: 'att' }).ok).toBe(true);
  if (way === 'each') for (const id of mine) expect(TC.battleOrder({ do: 'withdraw', units: [id] }).ok).toBe(true);
  let n = 0;
  for (; n < 4000 && !v.over; n++) v = TC.battleTick(1);
  const out = TC.battleEnd();
  const g = copy();
  return { out, rep: g.airReports[g.airReports.length - 1], g, target, seconds: v.t as number };
}

describe('breaking off before contact does not win (release 1.5 FLY IT YOURSELF)', () => {
  for (const way of ['all', 'each'] as const) {
    it(`a fight for the sky, ${way === 'all' ? 'BREAK OFF ALL' : 'each group sent home in turn'} at one second: lost, no sky, a note on the map`, () => {
      const { out, rep, g, target, seconds } = fly('superiority', way);
      expect(out.ok).toBe(true);
      expect(rep.won).toBe(false);
      expect(rep.sky).toBe(false);
      expect(rep.level).toMatch(/defeat/);
      expect(g.sky[target]?.nat === HUMAN && g.sky[target]?.turn === g.turn).toBe(false);
      expect(out.after.lost.att).toEqual({ ftr: 0, bmr: 0 });
      // The map's note says what happened, and the battle ended as soon as the last group turned.
      expect(out.note).toBe('The fighters broke off and turned for home before they met the enemy.');
      expect(seconds).toBeLessThan(10);
    });
  }

  it('a day raid, each group sent home in turn at one second: no bomb falls, and it is no victory', () => {
    const { out, rep } = fly('bomb', 'each');
    expect(out.ok).toBe(true);
    expect(rep.won).toBe(false);
    expect(rep.level).toMatch(/defeat/);
    expect(rep.dropped ?? 0).toBe(0);
    expect(out.note).toBe('The raid broke off and turned for home before it met the enemy, its bombs jettisoned.');
  });

  it('over an empty sky, going home is not giving up (the design director, 16:37Z); with a defence on the field, it is', () => {
    // The field itself, on the same save: each group sent home at one second, over a sky no defender comes up to, and
    // over one the defence rises to (its groups wait on their airfields for the warning at that moment).
    const gaveUp = (empty: boolean): string | undefined => {
      const { target, ids } = raid1942('superiority', empty);
      const s = airBattle(airTicket(copy(), 'superiority', target, ids, HUMAN).ticket!);
      expect(s.units.some((u) => u.side === 'def')).toBe(!empty);
      while (s.t < 1) s.step();
      for (const u of s.units.filter((x) => x.side === 'att')) s.command({ do: 'withdraw', units: [u.id] }, 'att');
      while (!s.over) s.step();
      return s.outcome().gaveUp;
    };
    expect(gaveUp(true)).toBeUndefined();
    expect(gaveUp(false)).toBe('att');
    // Through the bridge, a sweep over the empty sky sent home at once says nothing of breaking off.
    expect(fly('superiority', 'each', true).out.note).not.toMatch(/before (they|it) met the enemy/);
  });

  it("the raid card's check (airLook) answers without the ground's outlines; the battle's board keeps them", () => {
    const { target, ids } = raid1942('bomb');
    const look = TC.airLook('bomb', target, ids);
    expect(look.ticket.def.forces.length).toBeGreaterThan(0);
    expect(look.ticket.place.ground).toBeUndefined();
    expect(JSON.stringify(look).length).toBeLessThan(20_000);
    const st = TC.airStart('bomb', target, ids);
    expect(st.sky.place.ground.land.length).toBeGreaterThan(0);
  });

  it('breaking off at once every month over Zaporozhye builds no sky there, as staying at home (the playtester, 23 Nov)', () => {
    // SOV from January 1942, seed 7: the playtester's re-run held the sky in February and March this way, 0 lost.
    TC.start({ view: 'grand', human: 'SOV', from: 29, seed: 7, world: true });
    const t = copy().regions.find((r) => r.name === 'Zaporozhye')!.id;
    let flown = 0;
    for (let m = 0; m < 4; m++) {
      const p = planRaid(copy(), 'superiority', t, undefined, 'SOV');
      if (p.picked?.length && TC.airLook('superiority', t, p.picked)?.ticket) {
        TC.airStart('superiority', t, p.picked);
        TC.battleSpeed(1);
        let v = TC.battleTick(1);
        expect(TC.battleOrder({ do: 'withdrawAll', side: 'att' }).ok).toBe(true);
        while (!v.over) v = TC.battleTick(1);
        expect(TC.battleEnd().note).toBe('The fighters broke off and turned for home before they met the enemy.');
        flown++;
      }
      const g = copy();
      expect(g.sky[t]?.nat === 'SOV' && g.sky[t]?.turn === g.turn).toBe(false);
      expect(skyFight(g, t)?.now ?? 'contested').not.toBe('held');
      expect(skyFight(g, t)?.run ?? 0).toBeLessThanOrEqual(0);
      TC.endMonth();
    }
    expect(flown).toBe(4);
  });

  it("a fight given up counts only against the side that gave it up: outnumbered, it still goes on to 'lost'", () => {
    TC.start({ view: 'grand', human: 'SOV', from: 29, seed: 7, world: true });
    const g = copy();
    const seen = { beaten: 0, not: 0 };
    for (const r of raidTargets(g, 'superiority', 'SOV')) {
      const quit = loadGame(TC.save().text)!, fought = loadGame(TC.save().text)!;
      skyFought(quit, 'SOV', r.id, false, true);
      skyFought(fought, 'SOV', r.id, false);
      const q = quit.ac?.sky[r.id], f = fought.ac?.sky[r.id]!;
      if (f.streak < 0) {
        // The enemy outnumbers it there: the month counts against it exactly as a fight would, on to 'lost'.
        seen.beaten++;
        expect(q).toEqual(f);
        for (let i = 0; i < 6 && quit.ac!.sky[r.id].state !== 'lost'; i++) { quit.turn++; skyFought(quit, 'SOV', r.id, false, true); }
        expect(quit.ac!.sky[r.id].state).toBe('lost');
      } else {
        // Otherwise the month stands as if it had stayed at home: no fight is begun there.
        seen.not++;
        expect(q).toBeUndefined();
      }
    }
    expect(seen.beaten).toBeGreaterThan(0);
    expect(seen.not).toBeGreaterThan(0);
  });

  it('flown with no orders, the same raid is fought and judged as before (nobody gave up)', () => {
    const { out, rep } = fly('superiority', 'none');
    expect(out.ok).toBe(true);
    expect(out.note).not.toMatch(/before meeting the enemy/);
    expect(rep.att.reduce((s, x) => s + x.lostFtr, 0) + rep.def.reduce((s, x) => s + x.lostFtr, 0)).toBeGreaterThan(0);
  });
});
