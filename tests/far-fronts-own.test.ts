// The offensive held by front (rider b646 to ruling 234, the design director, 19:50Z of 19 November 2026;
// src/campaign/staff.ts, "The offensive held by front").
//
// Beyond the old chart a staff's offensive was remembered by enemy country, and one country's ground lies there in
// many pieces: a piece of China's ground with half a division on it (Yencheng on the first day), or with nobody,
// read a strength ratio in the hundreds and opened the offensive against all of China, which the main front's corps
// then fought though their own front had never earned it. Now each far-side front opens, keeps and calls off its own
// offensive by its own reading and rests by itself, and the orders are bounded to the fronts that are on. No number
// is new. The states are set by hand on the first day's chart and the staff's month is read; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { createGame, declareWar } from '../src/campaign/game';
import { decide, fronts, offensiveWatch, ownFront, staffPhase, startOpen, temper, type Front } from '../src/campaign/staff';
import { shoreOf } from '../src/campaign/world';
import type { Game } from '../src/campaign/types';

type Told = { nat: string; front: Front; own: boolean; was: number; now: number; how: string };

/** A war nobody plays on the first day, far side on. */
const war = (): Game => startOpen(createGame(3, null));
/** Japan's army on the mainland at a share of its strength: its main front in China then reads under its mark to open. */
const thin = (g: Game, share: number): void => { for (const c of g.corps) if (c.nat === 'JAP' && !c.dead && g.regions[c.reg].owner !== 'JAP') c.str *= share; };
/** What Japan's staff decides this month, front by front. */
const month = (g: Game, nat = 'JAP'): Told[] => {
  const told: Told[] = [];
  offensiveWatch.tell = (e) => { if (e.nat === nat) told.push(e); };
  decide(g, nat);
  offensiveWatch.tell = null;
  return told;
};
const names = (g: Game, ids: number[]): string[] => ids.map((id) => g.regions[id].name);
/** The front a named province stands on, among those told. */
const at = (g: Game, told: Told[], place: string): Told => told.find((e) => e.front.depth[g.byName[place].id] >= 1)!;
const marks = (g: Game, kind: string, nat = 'JAP'): number[] => Object.keys(g.flags).filter((k) => k.startsWith(`st:${kind}:${nat}:`)).map((k) => Number(k.split(':')[3]));

afterEach(() => { offensiveWatch.tell = null; });

describe('the offensive held by front (rider b646 to ruling 234)', () => {
  it('reads on a front whose whole edge is on the far side, and on no other', () => {
    const g = war();
    const jap = fronts(g, 'JAP');
    expect(jap.length).toBeGreaterThanOrEqual(2);
    for (const f of jap) expect(ownFront(g, f)).toBe(true);
    // (Germany's front against Poland is on the old chart.)
    const ger = fronts(g, 'GER');
    expect(ger.length).toBeGreaterThan(0);
    for (const f of ger) { expect(f.edge.every((id) => shoreOf(id) === 0)).toBe(true); expect(ownFront(g, f)).toBe(false); }
  });

  it('lets a thin patch open its own offensive while the main front, under its mark, stays quiet', () => {
    const g = war();
    thin(g, 0.5);
    const T = temper(g, 'JAP');
    const told = month(g);
    const main = at(g, told, 'Chungking'), patch = at(g, told, 'Yencheng');
    expect(main.front).not.toBe(patch.front);
    // The readings: the main front under the mark to open, the patch far over it.
    expect(main.front.ratio).toBeLessThan(T.attackAt);
    expect(patch.front.ratio).toBeGreaterThanOrEqual(T.attackAt);
    expect(patch.how).toBe('opened');
    expect(patch.now).toBe(g.turn);
    expect(main.how).toBe('quiet');
    expect(main.now).toBe(0);
    // What is remembered is written on the patch's ground alone.
    expect(names(g, marks(g, 'fo'))).toEqual(['Yencheng']);
    // The by-country word is kept for those who read it by country (the air staff, the engineers, the player's view).
    expect(g.flags['st:on:JAP:CHN']).toBe(g.turn);
    // And the orders are bounded to the patch: no province of the main front is a target.
    const ph = staffPhase(g, 'JAP')!;
    expect(ph.posture).toBe('attack');
    expect(ph.targets).toEqual(['Yencheng']);
  });

  it('opens the main front on its own reading, and both fronts are then on', () => {
    const g = war();
    const T = temper(g, 'JAP');
    const told = month(g);
    const main = at(g, told, 'Chungking'), patch = at(g, told, 'Yencheng');
    expect(main.front.ratio).toBeGreaterThanOrEqual(T.attackAt);
    expect(main.how).toBe('opened');
    expect(patch.how).toBe('opened');
    expect(marks(g, 'fo').length).toBe(main.front.edge.length ? [...main.front.depth].filter((d) => d >= 1).length + 1 : 0);
    const ph = staffPhase(g, 'JAP')!;
    expect(ph.targets!.length).toBeGreaterThan(1);
    expect(ph.targets).toContain('Yencheng');
  });

  it('calls the main front off by its own reading and rests it alone, the patch keeping its own', () => {
    const g = war();
    month(g);
    // Two months on, no longer a new offensive, and Japan's army there cut to a fraction: the main front reads under its mark to keep.
    g.turn += 2;
    thin(g, 0.35);
    const T = temper(g, 'JAP');
    const told = month(g);
    const main = at(g, told, 'Chungking'), patch = at(g, told, 'Yencheng');
    expect(main.was).toBe(1);
    expect(main.front.ratio).toBeLessThan(T.persist);
    expect(main.how).toBe('off');
    expect(main.now).toBe(0);
    expect(patch.was).toBe(1);
    expect(patch.now).toBe(1);
    expect(names(g, marks(g, 'fo'))).toEqual(['Yencheng']);
    // The rest is the main front's: written on its ground, two months as open.json has it.
    const rest = marks(g, 'fr');
    expect(rest.length).toBe([...main.front.depth].filter((d) => d >= 1).length);
    expect(g.flags[`st:fr:JAP:${g.byName.Chungking.id}`]).toBe(g.turn + 2);
    expect(g.flags['st:rest:JAP:CHN']).toBeUndefined();
    // The month after, its strength back, it is still resting; the month the rest ends it opens on its own reading.
    for (const c of g.corps) if (c.nat === 'JAP' && !c.dead && g.regions[c.reg].owner !== 'JAP') c.str = Math.min(1, c.str / 0.35);
    g.turn += 1;
    expect(at(g, month(g), 'Chungking').how).toBe('quiet');
    g.turn += 1;
    const again = at(g, month(g), 'Chungking');
    expect(again.front.ratio).toBeGreaterThanOrEqual(T.attackAt);
    expect(again.how).toBe('opened');
    expect(again.now).toBe(g.turn);
  });

  it('knows a front again when its line has moved', () => {
    const g = war();
    const first = at(g, month(g), 'Chungking');
    // A third of the edge changes hands, and the line behind it becomes the edge.
    const gone = first.front.edge.slice(0, Math.floor(first.front.edge.length / 3));
    for (const id of gone) { const r = g.regions[id]; for (const u of [...r.units]) u.dead = true; r.units = []; r.ctrl = 'JAP'; }
    g.corps = g.corps.filter((c) => !c.dead);
    g.turn += 1;
    const next = at(g, month(g), 'Chungking');
    expect(next.front.edge).not.toEqual(first.front.edge);
    expect(next.was).toBe(1);
    expect(next.how).toBe('fresh');
    // What was remembered of the ground that is Japan's now is forgotten.
    for (const id of gone) expect(g.flags[`st:fo:JAP:${id}`]).toBeUndefined();
  });

  it('opens every front against the enemies a great choice names for its months, then leaves each to its own reading', () => {
    const g = war();
    g.turn = 28;
    declareWar(g, 'JAP', 'UK');
    // (As the strike south's choice writes it: staff.ts, `go`.)
    g.flags['st:push:JAP'] = g.turn + 5;
    g.flags['st:pushed:JAP:UK'] = 1;
    g.flags['st:on:JAP:UK'] = g.turn;
    thin(g, 0.2);
    const T = temper(g, 'JAP');
    const told = month(g);
    const uk = told.filter((e) => e.front.enemies.includes('UK'));
    expect(uk.length).toBeGreaterThan(0);
    for (const e of uk) { expect(e.how).toBe('pushed'); expect(e.now).toBe(28); }
    // China's own fronts are not the choice's: under their marks they stay quiet.
    const china = told.filter((e) => !e.front.enemies.includes('UK'));
    expect(china.length).toBeGreaterThan(0);
    for (const e of china) if (e.front.ratio < T.attackAt) expect(e.now).toBe(0);
    // The months over, a front against Britain under its mark to keep is called off like any other: as one that fell
    // short, or (nobody having fought on it) as an offensive in name only, and that too is remembered by front.
    g.turn = 34;
    const later = month(g).filter((e) => e.front.enemies.includes('UK') && e.was);
    expect(later.length).toBeGreaterThan(0);
    for (const e of later) {
      if (e.front.ratio >= T.persist) { expect(e.how).toBe('kept'); continue; }
      expect(['off', 'idle']).toContain(e.how);
      expect(e.now).toBe(0);
      const kind = e.how === 'idle' ? 'fi' : 'fr';
      expect(g.flags[`st:${kind}:JAP:${e.front.edge[0]}`]).toBe(e.how === 'idle' ? 34 : 36);
    }
    expect(g.flags['st:idle:JAP:UK']).toBeUndefined();
    expect(g.flags['st:rest:JAP:UK']).toBeUndefined();
    // (With none of its fronts on, the by-country word against Britain is off too.)
    if (later.every((e) => !e.now)) expect(g.flags['st:on:JAP:UK']).toBeUndefined();
  });

  it('leaves the old chart to remember by country', () => {
    const g = war();
    g.turn = 2;
    const told = month(g, 'GER');
    expect(told.length).toBeGreaterThan(0);
    for (const e of told) expect(e.own).toBe(false);
    expect(g.flags['st:on:GER:POL']).toBeGreaterThan(0);
    expect(marks(g, 'fo', 'GER')).toEqual([]);
    expect(marks(g, 'fr', 'GER')).toEqual([]);
  });
});
