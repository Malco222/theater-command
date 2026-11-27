// The contact rule, round 4, the mender's boards: the rule sceptic's S4-1 and S4-2 and the telling sceptic's T4-1 and
// T4-2 (reports /home/user/runs/contact2/r4/reports/r3-rule-sceptic4.json and r4-telling-sceptic4.json). The far side
// on, seed 3, the first day.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { answering, frontAnswering } from '../src/campaign/ai';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { contactRead, corpsInContact } from '../src/campaign/drawn';
import { addCorps, atWar, createGame, relocate } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { farOn } from '../src/campaign/world';
import type { Corps, Game, Region } from '../src/campaign/types';


// ---- S4-1: a held corps whose general takes its hold off this month (ai.ts `answering`, `frontAnswering`) is free --

describe("S4-1: an enemy corps under orders to hold that answers this month (the general's own answer rule) is not bound", () => {
  // (Hainan, Japan's on the first day, has one neighbour, Pakhoi, China's, where the Southern Kwangtung garrison stands
  // held; on the far side the war itself frees the holds on the front facing Japan, so it answers: ai.ts
  // `frontAnswering`. A lone Japanese test corps on Hainan, Japan's own garrison moved off.)
  function board(): { g: Game; c: Corps; pk: Region; gar: Corps } {
    const g = startOpen(createGame(3, null));
    expect(farOn(g)).toBe(true);
    expect(setFromWait(g, true)).toBe(true);
    const hn = g.byName.Hainan, pk = g.byName.Pakhoi;
    expect(hn.nb.map((j) => g.regions[j].name)).toEqual(['Pakhoi']);
    expect(atWar(g, 'JAP', 'CHN')).toBe(true);
    for (const u of [...hn.units]) relocate(g, u, g.byName.Canton);
    const gar = pk.units.find((u) => u.name === 'Southern Kwangtung garrison')!;
    expect([pk.ctrl, pk.units.filter((u) => !u.dead).length, gar.hold, !!gar.stay]).toEqual(['CHN', 1, true, false]);
    const c = addCorps(g, 'JAP', 'Lone Test Corps', 'inf', 1, hn.id, {});
    return { g, c, pk, gar };
  }

  it('the held garrison answers this month: it is free to come on, and the lone corps beside it is held', () => {
    const { g, c, gar } = board();
    expect(frontAnswering(g, 'CHN').includes(gar) || answering(g, 'CHN').includes(gar)).toBe(true);
    expect(contactRead(g, 'JAP', c)).toEqual({ beside: true, battle: false, kept: false });
    expect(corpsInContact(g, 'JAP', c)).toBe(true);
  });

  it('the mirror: the same garrison marked to stay answers nothing: bound, and the lone corps is not held (R4-1 as built)', () => {
    const { g, c, gar } = board();
    gar.stay = true;
    expect(frontAnswering(g, 'CHN').includes(gar) || answering(g, 'CHN').includes(gar)).toBe(false);
    expect(contactRead(g, 'JAP', c)).toEqual({ beside: true, battle: false, kept: true });
    expect(corpsInContact(g, 'JAP', c)).toBe(false);
  });
});
