// The hop staff's odds count every defender, with the far side's switch on (rider b614 to ruling 234, (2)).
//
// A staff reckons a shore's odds against whatever fights there: the corps in the field and, where nobody stands in
// the field, the town itself, its garrison and its guard. An empty shore is one with nothing that fights. Before the
// rider a shore with only a town's guard was read as empty: no odds were asked, and it was ranked as a free gift.
// The states are set by hand; no war is played here.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { hop } from '../src/campaign/ai';
import { addCorps, assess, atWar, createGame, declareWar, hostileUnits } from '../src/campaign/game';
import { landingOpts } from '../src/campaign/navy';
import { guardOf, townHeld } from '../src/campaign/siege';
import { descentAsks, startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

/**
 * January 1942, a war nobody plays: Japan at war with America, Britain, Australia and New Zealand, Rabaul already
 * Japan's, every corps of Japan's own held where it stands, and `n` small detachments of `str` ready on Palau's quay.
 */
const war = (n: number, str: number): { g: Game; force: Corps[] } => {
  const g = startOpen(createGame(17, null));
  for (const foe of ['USA', 'UK', 'AUS', 'NZL']) if (!atWar(g, 'JAP', foe)) declareWar(g, 'JAP', foe);
  g.turn = 29;
  for (const c of g.corps) if (c.nat === 'JAP') c.hold = true;
  g.byName.Rabaul.ctrl = 'JAP';
  const force = Array.from({ length: n }, (_, k) => {
    const c = addCorps(g, 'JAP', `Test Detachment ${k + 1}`, 'inf', 1, 'Palau', { est: str });
    c.supplied = true; c.ap = c.apMax; c.org = 1; c.str = str; c.hold = false;
    return c;
  });
  return { g, force };
};
const at = (g: Game, c: Corps): string => g.regions[c.reg].name;
const ask = descentAsks()!.odds;
/** What a force would have against a shore's town, and against whoever stands in its field. */
const onTown = (g: Game, w: Corps[], r: Region): number => assess(g, w, r, { town: true }).odds;
const inField = (g: Game, w: Corps[], r: Region): number => assess(g, w, r, landingOpts(g, w, r)).odds;

describe("the hop staff's odds count every defender (rider b614 to ruling 234, (2))", () => {
  it("a shore held by its town's guard alone is not an empty shore: a detachment too weak for the guard is not put ashore there", () => {
    const { g, force } = war(1, 0.05);
    const strong = g.byName.Guadalcanal, cebu = g.byName.Cebu, weak = g.byName.Bougainville;
    // Nobody stands in the field of any of the three; each town has its guard.
    for (const r of [strong, cebu, weak]) expect([r.name, hostileUnits(g, r, 'JAP').length, townHeld(g, r), guardOf(g, r) > 0]).toEqual([r.name, 0, true, true]);
    // The detachment has the staff's odds on Bougainville's guard and not on Guadalcanal's or Cebu's, the worthier shores.
    expect(onTown(g, force, strong)).toBeLessThan(ask);
    expect(onTown(g, force, cebu)).toBeLessThan(ask);
    expect(onTown(g, force, weak)).toBeGreaterThanOrEqual(ask);
    expect(strong.vp + cebu.vp).toBeGreaterThan(weak.vp);
    hop(g, 'JAP', null);
    expect(at(g, force[0])).toBe('Bougainville');
    expect(g.flags[`landedAt:${strong.id}:JAP`]).toBeUndefined();
    expect(g.flags[`landedAt:${cebu.id}:JAP`]).toBeUndefined();
  });

  it('with the odds on the guard it goes ashore on the worthier shore, and the town is its to summon', () => {
    const { g, force } = war(1, 0.3);
    // (Guam, ranked first, is given more than the detachment can ask for, so that the guarded shore is read.)
    g.corps.find((c) => c.nat === 'USA' && !c.dead && g.regions[c.reg].name === 'Guam')!.str = 1;
    const guam = g.byName.Guam, shore = g.byName.Guadalcanal;
    expect(inField(g, force, guam)).toBeLessThan(ask);
    expect([hostileUnits(g, shore, 'JAP').length, guardOf(g, shore) > 0]).toEqual([0, true]);
    expect(onTown(g, force, shore)).toBeGreaterThanOrEqual(ask);
    hop(g, 'JAP', null);
    expect(at(g, force[0])).toBe('Guadalcanal');
    expect(g.flags[`landedAt:${shore.id}:JAP`]).toBe(29);
  });

  it('where corps stand in the field the odds are against them, as they were, and an empty shore asks for none', () => {
    // Guam: its garrison stands in the field and the town has no guard of its own (a post, the record's garrison its whole defence).
    const { g, force } = war(1, 0.15);
    const guam = g.byName.Guam;
    expect([hostileUnits(g, guam, 'JAP').length > 0, townHeld(g, guam)]).toEqual([true, false]);
    expect(inField(g, force, guam)).toBeGreaterThanOrEqual(ask);
    hop(g, 'JAP', null);
    expect(at(g, force[0])).toBe('Guam');
    // Rabaul with nobody in the field and no guard is empty: the weakest detachment goes ashore, and it is ranked ahead of the held shores.
    const open = war(1, 0.05);
    const rabaul = open.g.byName.Rabaul;
    rabaul.ctrl = 'AUS';
    expect([hostileUnits(open.g, rabaul, 'JAP').length, townHeld(open.g, rabaul)]).toEqual([0, false]);
    hop(open.g, 'JAP', null);
    expect(at(open.g, open.force[0])).toBe('Rabaul');
  });
});
