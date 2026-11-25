// The far-side fronts' word under a key of its own (the design director's 11:40Z of 22 November 2026 to ruling 234;
// src/campaign/staff.ts, `farWordKey` and `frontsKept`).
//
// A far-side front decides for itself and remembers on its own ground, and the word by country was kept for those who
// read by country (the air staff, the engineers, the player's view of a rival). It was written under the old chart's
// key, and an old-chart front reads that key as its own offensive's month. So the month Italy entered the war,
// France's far-side front against Italian East Africa (empty ground: a ratio in the hundreds) opened, the word against
// Italy went on, France's front at home, with Italy among its enemies, read on at a third of Germany's strength, the
// month's orders became an offensive against Germany, and their release let go every held corps on the jump-off
// ground from Lorraine to Mareth: the Tunisia Command walked into Tripoli. Now the far-side word has its own key and
// an old-chart front reads only what its own decision, or a plan, wrote. The states are set by hand on the first
// day's chart (the far-side front's offensive is written on its ground as its own month writes it, so the case does
// not rest on how East Africa reads once its garrisons are laid) and the staff's month is read; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import { decide, fronts, offensiveWatch, ownFront, staffPhase, startOpen, type Front } from '../src/campaign/staff';
import { release } from '../src/campaign/ai';
import { shoreOf } from '../src/campaign/world';
import type { Game } from '../src/campaign/types';

type Told = { nat: string; front: Front; own: boolean; was: number; now: number; how: string };

/** The first day's chart in June 1940, Italy in the war against France and Britain, who are at war with Germany. */
const june40 = (): Game => {
  const g = startOpen(createGame(11, null));
  g.turn = 12;
  declareWar(g, 'FRA', 'GER');
  declareWar(g, 'UK', 'GER');
  declareWar(g, 'ITA', 'FRA');
  declareWar(g, 'ITA', 'UK');
  return g;
};
const month = (g: Game, nat: string): Told[] => {
  const told: Told[] = [];
  offensiveWatch.tell = (e) => { if (e.nat === nat) told.push(e); };
  decide(g, nat);
  offensiveWatch.tell = null;
  return told;
};
const tunisia = (g: Game) => g.corps.find((c) => c.nat === 'FRA' && c.name === 'Tunisia Command')!;
/** France's far-side front against Italian East Africa, as the board stands. */
const eastOf = (g: Game): Front => fronts(g, 'FRA').find((f) => ownFront(g, f) && f.enemies.includes('ITA'))!;
/** Its offensive opened this month: written on its ground, as its own month writes it (staff.ts, MARK.on). */
const open = (g: Game): void => { const f = eastOf(g); f.depth.forEach((d, id) => { if (d >= 1) g.flags[`st:fo:FRA:${id}`] = g.turn; }); };

afterEach(() => { offensiveWatch.tell = null; });

describe("the far-side fronts' word under its own key (the design director, 11:40Z of 22 November 2026)", () => {
  it("opens France's front against East Africa while its front at home stays held", () => {
    const g = june40();
    expect(tunisia(g).hold).toBe(true);
    expect(g.regions[tunisia(g).reg].name).toBe('Mareth');
    const front = eastOf(g);
    expect(front.edge.map((id) => g.regions[id].name)).toContain('Asmara');
    expect(front.edge.every((id) => shoreOf(id) === 2)).toBe(true);
    open(g);
    const told = month(g, 'FRA');
    // The far-side front: Italian East Africa, every province of its edge beyond the old chart, its offensive on.
    const east = told.filter((e) => e.own && e.front.enemies.includes('ITA'));
    expect(east).toHaveLength(1);
    expect(east[0].was).toBe(12);
    expect(east[0].now).toBe(12);
    // Its word is kept by country under its own key, and the old chart's key is not written.
    expect(g.flags['st:on:FRA:ITA:far']).toBe(12);
    expect(g.flags['st:on:FRA:ITA']).toBeUndefined();
    expect(g.flags['st:on:FRA:GER']).toBeUndefined();
    // The old-chart fronts with Italy among their enemies read their own decision: under their marks, quiet.
    const old = told.filter((e) => !e.own && e.front.enemies.includes('ITA'));
    expect(old.length).toBeGreaterThan(0);
    for (const e of old) { expect(e.was).toBe(0); expect(e.how).toBe('quiet'); expect(e.now).toBe(0); }
    // The month's orders are the far-side front's alone: no ground at home or in Tunisia is let go, none of Libya's
    // old-chart ground is a target, and the Tunisia Command stays held at Mareth.
    const ph = staffPhase(g, 'FRA')!;
    for (const nm of ['Mareth', 'Lorraine', 'Vosges', 'Riviera', 'Savoy']) expect(ph.release ?? []).not.toContain(nm);
    for (const nm of ['Tripoli', 'Nalut', 'Saar', 'Piedmont']) expect(ph.targets ?? []).not.toContain(nm);
    expect(ph.targets ?? []).toContain('Asmara');
    expect(ph.name).not.toBe('The offensive against Germany');
    if (ph.release) release(g, 'FRA', ph.release);
    expect(tunisia(g).hold).toBe(true);
    // The next month the old-chart fronts still read quiet: nothing was remembered for them.
    g.turn = 13;
    const next = month(g, 'FRA').filter((e) => !e.own && e.front.enemies.includes('ITA'));
    expect(next.length).toBeGreaterThan(0);
    for (const e of next) { expect(e.was).toBe(0); expect(e.how).toBe('quiet'); }
    expect(g.flags['st:on:FRA:ITA']).toBeUndefined();
    expect(g.flags['st:on:FRA:ITA:far']).toBe(12);
  });

  it('forgets the far-side word when none of that ground is under an offensive any more', () => {
    const g = june40();
    open(g);
    month(g, 'FRA');
    expect(g.flags['st:on:FRA:ITA:far']).toBe(12);
    // (The marks taken off the ground, as a front called off leaves it.)
    for (const k of Object.keys(g.flags)) if (k.startsWith('st:fo:FRA:')) delete g.flags[k];
    g.turn = 13;
    const east = month(g, 'FRA').filter((e) => e.own && e.front.enemies.includes('ITA'));
    expect(east).toHaveLength(1);
    if (!east[0].now) {
      expect(g.flags['st:on:FRA:ITA:far']).toBeUndefined();
    } else {
      // (Its own reading opened it again this month: the word is this month's, not the old one.)
      expect(g.flags['st:on:FRA:ITA:far']).toBe(13);
    }
    expect(g.flags['st:on:FRA:ITA']).toBeUndefined();
  });
});
