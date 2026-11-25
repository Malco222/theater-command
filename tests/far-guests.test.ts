// Guests on friendly ground, with the far side's switch on (the Pacific playtester's findings 5 and 6, 19 November
// 2026).
//
// 5. An American corps that marched into Canada was the Continent's guest: its host was the biggest ally with a
//    capital on land, Moscow, there is no road from Vancouver to Moscow, and it was lifted off the beaches every month
//    at a quarter of its strength. A corps of a country beyond Europe now stands on an ally's ground in the Americas
//    as it does on the far side, and comes off only when cut off or broken. Britain's keep the host rule.
// 6. When a country returns and its allies hand its ground back, a corps left on it of a country at peace with it and
//    not its ally goes home, as on any ground that changes hands: Mongolia's, tied to the Soviet Union alone, stood on
//    for good on the Chinese ground the Red Army gave back.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { wantsOut } from '../src/campaign/ai';
import { allied, atWar, createGame, relocate, setWar } from '../src/campaign/game';
import { handBack } from '../src/campaign/liberation';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import { farOn, shoreOf } from '../src/campaign/world';

const wide = (seed = 1): Game => startOpen(createGame(seed, null));
/** A corps of a country set down, supplied and in good order, on a province. */
const guest = (g: Game, nat: string, at: Region): Corps => {
  const c = g.corps.find((x) => x.nat === nat && !x.dead && x.kind !== 'fort' && x.kind !== 'eng' && !x.town && g.regions[x.reg].owner === nat)!;
  relocate(g, c, at);
  c.supplied = true; c.org = 1;
  return c;
};
/** America in the war beside the Allies, by hand. */
const allies = (g: Game, ...ids: string[]): void => { for (const id of ids) { g.nations[id].align = 'allies'; setWar(g, id, 'GER'); } };

describe('an ally on friendly ground in the Americas (finding 5)', () => {
  it('an American corps in Canada is nobody\'s guest: it stays, and comes off only when cut off or broken', () => {
    const g = wide();
    expect(farOn(g)).toBe(true);
    allies(g, 'USA', 'CAN');
    const here = g.byName['Vancouver'];
    expect([here.owner, here.ctrl, shoreOf(here)]).toEqual(['CAN', 'CAN', 1]);
    expect(allied(g, 'USA', 'CAN')).toBe(true);
    const c = guest(g, 'USA', here);
    expect(wantsOut(g, c)).toBe(false);
    c.org = 0.25;
    expect(wantsOut(g, c)).toBe(true);
    c.org = 1; c.supplied = false;
    expect(wantsOut(g, c)).toBe(true);
  });

  it("so does a Canadian corps in the United States, and an Australian one in Canada", () => {
    const g = wide();
    allies(g, 'USA', 'CAN', 'AUS');
    const states = g.regions.find((r) => r.owner === 'USA' && r.ctrl === 'USA' && shoreOf(r) === 1 && r.nb.some((j) => g.regions[j].owner === 'CAN'))!;
    expect(wantsOut(g, guest(g, 'CAN', states))).toBe(false);
    expect(wantsOut(g, guest(g, 'AUS', g.byName['Vancouver']))).toBe(false);
  });

  it("a British corps there keeps the old host rule, and nobody is at home on an enemy's ground by it", () => {
    const g = wide();
    allies(g, 'USA', 'CAN', 'UK');
    const b = guest(g, 'UK', g.byName['Vancouver']);
    expect(wantsOut(g, b)).toBe(true);
    // Ground of a country that is no ally is not friendly ground: the new line says nothing of it.
    const m = g.regions.find((r) => r.owner === 'MEX' && r.ctrl === 'MEX' && shoreOf(r) === 1)!;
    expect(allied(g, 'USA', 'MEX')).toBe(false);
    const c = guest(g, 'USA', m);
    expect(wantsOut(g, c)).toBe(true);
  });
});

describe('ground handed back to a country that returns (finding 6)', () => {
  it("sends home a corps of a country at peace with it and not its ally: Mongolia's on Chinese ground", () => {
    const g = wide();
    g.nations.CHN.align = 'allies'; g.nations.SOV.align = 'allies';
    expect(allied(g, 'CHN', 'SOV')).toBe(true);
    expect(allied(g, 'MGL', 'SOV')).toBe(true);
    expect(allied(g, 'MGL', 'CHN')).toBe(false);
    expect(atWar(g, 'MGL', 'CHN')).toBe(false);
    const r = g.regions.find((x) => x.owner === 'CHN' && x.ctrl === 'CHN' && x.nb.some((j) => g.regions[j].owner === 'MGL'))!;
    r.ctrl = 'SOV';
    const m = guest(g, 'MGL', r);
    const s = guest(g, 'SOV', r);
    expect(handBack(g, g.nations.CHN)).toBeGreaterThan(0);
    expect(r.ctrl).toBe('CHN');
    // The Mongolian corps is home; the Soviet one, China's ally, stays where it stands.
    expect(g.regions[m.reg].owner).toBe('MGL');
    expect(m.ap).toBe(0);
    expect(s.reg).toBe(r.id);
  });
});
