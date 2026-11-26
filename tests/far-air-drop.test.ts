// A drop from the air is answered as a landing, with the far side's switch on (the design director's ruling of 13:39Z,
// 25 November 2026, on B11's eighth point, design-director/pacific-japan-1125.md: "A drop from the air is answered as
// a landing: yes. Mark it like a beach, so b641's landing answer governs it, not B11's march overland.").
//
// A shore won by a landing from the sea leaves its month on the ground (navy.ts `landCorps`, `shoreWon`), and the held
// rows on a front do not answer a beach (ai.ts `beach`): only a march overland from it. Ground taken from the air left
// no mark, so the held rows answered a drop as if the enemy had marched in. It is marked now, on the same condition:
// the ground really changed hands, or the army was first laid round a town that holds.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { answering, frontAnswering } from '../src/campaign/ai';
import { addCorps, capitulate, createGame, declareWar, moveCorps, relocate, setWar } from '../src/campaign/game';
import { landCorps } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { current, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

const wide = (human: string | null = null): Game => startOpen(createGame(1, human));
const names = (cs: Corps[]): string[] => cs.map((c) => c.name).sort();
const called = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name && !c.dead)!;
const upTo = (g: Game, nat: string): void => { for (let k = 0; current(g) !== nat && k < 200; k++) stepAI(g); expect(current(g)).toBe(nat); };

describe('a drop from the air is marked like a beach (B11, point 8)', () => {
  /**
   * Java in Britain's keeping, Japan at war with Britain, Britain's month about to open, as far-front-answer's Java:
   * the three Dutch divisions held at Batavia, Soerabaja (Central Java's drawn east so Semarang stands open) and
   * Soerabaja. Japan holds Ketapang on Borneo across the Java Sea, with two airborne corps on its airfield.
   */
  const java = (): { g: Game; at: (nm: string) => Region; knil: Corps[]; para: Corps[] } => {
    const g = wide();
    declareWar(g, 'GER', 'NED');
    capitulate(g, 'NED', 'GER');
    setWar(g, 'JAP', 'UK');
    upTo(g, 'UK');
    const at = (nm: string): Region => g.byName[nm];
    const knil = ['KNIL 1st Division (West Java)', 'KNIL 2nd Division (Central Java)', 'KNIL 3rd Division (East Java)'].map((nm) => called(g, nm));
    for (const c of knil) { c.str = 1; c.org = 1; c.supplied = true; }
    relocate(g, knil[1], at('Soerabaja'));
    for (const u of [...at('Ketapang').units]) relocate(g, u, at('Pontianak'));
    at('Ketapang').ctrl = 'JAP';
    const para = [2, 1].map((divs, i) => addCorps(g, 'JAP', `Test airborne ${i + 1}`, 'inf', divs, 'Ketapang', { airborne: true }));
    return { g, at, knil, para };
  };

  it('a drop that takes Bandoeng leaves the mark a landing leaves, and frees no held row on Java\'s front', () => {
    const { g, at, knil, para } = java();
    expect(at('Bandoeng').units.filter((u) => !u.dead)).toEqual([]);
    const res = landCorps(g, para, at('Bandoeng'));
    expect([res.ok, res.air, res.taken]).toEqual([true, true, true]);
    expect([at('Bandoeng').ctrl, at('Bandoeng').since]).toEqual(['JAP', g.turn]);
    // (As built no mark was left, Bandoeng read as entered by land, and all three Dutch divisions answered.)
    expect(g.flags[`shoreWon:${at('Bandoeng').id}`]).toBe(g.turn);
    expect(frontAnswering(g, 'UK').filter((c) => knil.includes(c))).toEqual([]);
  }, 600000);

  it('a later march overland from the ground the drop took does free them', () => {
    const { g, at, knil, para } = java();
    expect(landCorps(g, para, at('Bandoeng')).taken).toBe(true);
    expect(frontAnswering(g, 'UK').filter((c) => knil.includes(c))).toEqual([]);
    const walker = para[1];
    walker.ap = walker.apMax;
    expect(moveCorps(g, walker, at('Semarang'))).toBe(true);
    expect([at('Semarang').ctrl, g.flags[`shoreWon:${at('Semarang').id}`]]).toEqual(['JAP', undefined]);
    expect(names(frontAnswering(g, 'UK').filter((c) => knil.includes(c)))).toEqual(names(knil));
  }, 600000);

  it('a drop beaten off marks nothing', () => {
    const { g, at, knil, para } = java();
    // West Java's division stands at Bandoeng in strength; one weak airborne corps drops on it.
    relocate(g, knil[0], at('Bandoeng'));
    para[1].str = 0.2;
    const res = landCorps(g, [para[1]], at('Bandoeng'));
    expect([res.ok, res.air, res.taken, at('Bandoeng').ctrl]).toEqual([true, true, false, 'UK']);
    expect(g.flags[`shoreWon:${at('Bandoeng').id}`]).toBeUndefined();
  }, 600000);

  it("b641's landing answer governs a drop on home ground: Soviet airborne troops at Kushiro are answered by Japan's held home rows", () => {
    const g = wide();
    setWar(g, 'JAP', 'SOV');
    const kushiro = g.byName.Kushiro;
    for (const u of [...kushiro.units]) relocate(g, u, g.byName.Sapporo);
    const para = addCorps(g, 'SOV', 'Test Soviet airborne', 'inf', 1, 'Petropavlovsk', { airborne: true });
    const res = landCorps(g, [para], kushiro);
    // (Kushiro's town holds: the army is laid round it, and the field is the drop's.)
    expect([res.ok, res.air, res.taken]).toEqual([true, true, true]);
    expect(g.flags[`shoreWon:${kushiro.id}`]).toBe(g.turn);
    expect(names(answering(g, 'JAP'))).toEqual(['General reserve (16th Division, home from China)', 'General reserve (9th Division, home from China)', 'Home district depots', 'Imperial Guards Division']);
  }, 600000);
});
