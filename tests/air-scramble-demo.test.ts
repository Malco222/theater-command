// Release 2, the scramble offer (Brian, 24 November 2026 22:21Z: "i want every air battle playable"): the bridge's
// stand-in for the month that stops where an enemy raid meets his fighters (Rules/scramble-demo.ts). The 3D game's half
// is played against it with -TheaterAirScrambleDemo: TC.scrambleDemo() after TC.endMonth's answer, TC.scrambleStart(id),
// the battle on the shared clock (TC.battleTick), TC.battleEnd. It is a demonstration: his side is the defender, every
// view is an air battle's, and nothing is booked (the campaign's save text is the same before and after).
//
// The seat (found by trying UK from January 1940 and GER from January 1943, seeds 1 to 3; every one of them had a raid):
// UK, grand, from turn 5 (January 1940), seed 1, after his first END THE MONTH (February 1940): a German day raid on
// Kent, 300 bombers with no escort, met by No. 11 Group and No. 12 Group (350 fighters). The 3D Mac step file flies the
// same raid (scramble/mac-steps/scramble-demo.steps.txt).
import { describe, expect, it } from 'vitest';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { DEMO_NOTE, scrambleCalls } from '../unreal/TheaterCommand/Rules/scramble-demo';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;
const HUMAN = 'UK';

/** The seat above, its first month ended as the 3D game ends it, and the demonstration's offer then. */
function offered() {
  TC.scrambleMode('big');
  TC.start({ view: 'grand', human: HUMAN, from: 5, seed: 1 });
  const month = TC.endMonth();
  expect(month.view.date).toBe('February 1940');
  // (today's month carries no scramble of its own: the real stop is the warplanes thread's)
  expect(month.scramble).toBeUndefined();
  return TC.scrambleDemo();
}

describe('release 2: the scramble offer, the bridge\'s demonstration', () => {
  it('starts at never: told nothing, it offers nothing and reads nothing of the war', () => {
    // (The reviewer, 25 November: with "Scramble your fighters" off the 3D game sends no TC.scrambleMode at all, so the
    // rules must start where the switch off leaves them. A fresh set of calls, as at a load of the rules.)
    const calls = scrambleCalls(() => { throw new Error('the war was read'); }, () => { throw new Error('a raid was flown'); });
    expect(calls.scrambleDemo()).toBeNull();
    expect(calls.scrambleMode('sometimes')).toEqual({ mode: 'never' }); // (a word it does not know changes nothing)
    expect(calls.scrambleMode('big')).toEqual({ mode: 'big' });
    expect(calls.scrambleMode('never')).toEqual({ mode: 'never' });
    expect(calls.scrambleDemo()).toBeNull();
  });

  it('offers an enemy raid on him in the contract\'s shape', () => {
    const offer = offered();
    expect(offer).toBeTruthy();
    expect(offer).toMatchObject({ kind: 'bomb', target: 102, name: 'Kent', nat: 'GER', adj: 'German', ticket: true, demo: true, bombers: 300, escorts: 0, fighters: 350 });
    expect(offer.id).toBe('demo:bomb:102:GER:26.27');
    expect(offer.groups).toEqual(['No. 11 Group', 'No. 12 Group']);
    expect(offer.words).toBe('A German raid is heading for Kent: about 300 bombers, with no escort. About 350 of your fighters can rise to meet it (No. 11 Group, No. 12 Group).');
    // (never: nothing offered; an id it did not offer: nothing flown; leaving it: the month goes on)
    TC.scrambleMode('never');
    expect(TC.scrambleDemo()).toBeNull();
    TC.scrambleMode('big');
    expect(TC.scrambleStart('demo:bomb:1:GER:1')).toBeNull();
    expect(TC.scrambleLeave(offer.id, { rest: true })).toEqual({ ok: true });
    expect(TC.scrambleStart(offer.id)).toBeNull(); // (left to the staff: no longer offered)
  });

  it('flies it with his side the defender, to the end, and books nothing', () => {
    const offer = offered();
    const before = TC.save().text;
    const st = TC.scrambleStart(offer.id);
    expect(st).toBeTruthy();
    // The board: the raider is the computer's German raid, the defence his.
    expect(st.sky.bands).toEqual([1800, 4500, 7200]);
    expect(st.sky.title).toBe('The air battle over Kent');
    expect(st.sky.att).toEqual({ nat: 'GER', adj: 'German', human: false });
    expect(st.sky.def).toEqual({ nat: HUMAN, adj: 'British', human: true });
    // Every view an air battle's, his side 'def'.
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const airView = (v: any): void => {
      expect(v.me).toBe('def');
      expect(v.sky).toBeTruthy();
      expect(v.sky.me).toBe('def');
      expect(typeof v.sky.chance).toBe('number');
      expect(v.units.length).toBeGreaterThan(0);
      for (const u of v.units) {
        expect(typeof u.extra?.n).toBe('number');
        expect(['att', 'def']).toContain(u.side);
      }
    };
    airView(st.view);
    expect(TC.battleEnd()).toBeNull(); // (not over yet)
    TC.battleSpeed(4);
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    let v: any = null, n = 0;
    for (; n < 4000 && !(v && v.over); n++) {
      v = TC.battleTick(0.5);
      if (n % 50 === 0) airView(v);
    }
    airView(v);
    expect(v.over).toBeTruthy();
    // (his own fighters rose and fought: the defence is in the battle)
    expect((v.units as { side: string }[]).some((u) => u.side === 'def')).toBe(true);
    const out = TC.battleEnd();
    expect(out.ok).toBe(true);
    expect(out.demo).toBe(true);
    expect(out.note).toBe(DEMO_NOTE);
    // The after card the raid would have had, told from his side: his rows are the defence's, its totals its rows' sums.
    const rows = out.after.groups as { side: 'att' | 'def'; lostFtr: number; lostBmr: number; name: string }[];
    expect(rows.some((x) => x.side === 'def' && x.name === 'No. 11 Group')).toBe(true);
    const sum = (side: string, k: 'lostFtr' | 'lostBmr') => rows.filter((x) => x.side === side).reduce((t, x) => t + x[k], 0);
    expect(out.after.lost).toEqual({ att: { ftr: sum('att', 'lostFtr'), bmr: sum('att', 'lostBmr') }, def: { ftr: sum('def', 'lostFtr'), bmr: sum('def', 'lostBmr') } });
    expect(out.after.lines.join(' ')).toMatch(/you/);
    // (the card's verdict is his: the defence held the field, so it reads as his victory)
    expect(v.over.winner).toBe('def');
    expect(out.after.level).toMatch(/victory/i);
    // Nothing booked: the campaign is exactly as it was.
    expect(TC.save().text).toBe(before);
  });
});
