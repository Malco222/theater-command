// The hop staff's odds count every defender, with the far side's switch on (rider b614 to ruling 234, (2)).
//
// A staff reckons a shore's odds against whatever fights there: the corps in the field and, where nobody stands in
// the field, the town itself, its garrison and its guard. An empty shore is one with nothing that fights. Before the
// rider a shore with only a town's guard was read as empty: no odds were asked, and it was ranked as a free gift.
// The army's-shore rule (rider b599, (ii)) reads a shore the same way.
// The states are set by hand; no war is played here.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { armysShore, hop } from '../src/campaign/ai';
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
    const strong = g.byName.Guadalcanal, cebu = g.byName.Cebu, open = g.byName.Bougainville;
    // Nobody stands in the field of any of the three. Guadalcanal's town and Cebu's have their guards. Bougainville's
    // small town has none since the guard table of 21 November 2026 (far-guards.json: New Guinea's towns stand under
    // the rules' measure of too small to count), so it is an empty shore; before the table it was the example of a
    // guard the detachment had the odds on.
    for (const r of [strong, cebu]) expect([r.name, hostileUnits(g, r, 'JAP').length, townHeld(g, r), guardOf(g, r) > 0]).toEqual([r.name, 0, true, true]);
    expect([hostileUnits(g, open, 'JAP').length, townHeld(g, open), guardOf(g, open)]).toEqual([0, false, 0]);
    // The detachment has not the staff's odds on Guadalcanal's guard or on Cebu's, the worthier shores: read as empty,
    // as before the rider, either would have been taken ahead of Bougainville.
    expect(onTown(g, force, strong)).toBeLessThan(ask);
    expect(onTown(g, force, cebu)).toBeLessThan(ask);
    expect(strong.vp + cebu.vp).toBeGreaterThan(open.vp);
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

  it("the army's shore is read the same way: beside a town that only its guard holds, it is the army's only with the army's odds on the town", () => {
    const { g } = war(0, 0);
    // (Jesselton beside Brunei, with Brunei in Japan's hands. Cape Gloucester beside Rabaul was the example before the
    // guard table of 21 November 2026, under which New Guinea's small towns have no guard; Jesselton, British Borneo's
    // seat, has the record's 0.025.)
    const town = g.byName.Jesselton, base = g.byName.Brunei;
    base.ctrl = 'JAP';
    expect([town.nb.includes(base.id), hostileUnits(g, town, 'JAP').length, hostileUnits(g, base, 'JAP').length, guardOf(g, town)]).toEqual([true, 0, 0, 0.025]);
    const c = addCorps(g, 'JAP', 'Test Detachment', 'inf', 1, 'Brunei', { est: 0.08 });
    c.supplied = true; c.ap = c.apMax; c.org = 1; c.hold = false;
    // Too weak for the guard (the army's attacks ask 1.3): the ships' shore. Before the rider it was the army's for the walking in.
    c.str = 0.05;
    expect(onTown(g, [c], town)).toBeLessThan(1.3);
    expect(armysShore(g, 'JAP', town, null)).toBe(false);
    // With the odds on the town it is the army's, unless the phase asks more of its attacks.
    c.str = 0.08;
    expect(onTown(g, [c], town)).toBeGreaterThanOrEqual(1.3);
    expect(armysShore(g, 'JAP', town, null)).toBe(true);
    expect(armysShore(g, 'JAP', town, { minOdds: 99 } as never)).toBe(false);
    // The guard worn away, nothing fights there: the weakest corps beside it makes it the army's.
    c.str = 0.02;
    g.flags[`guard:${town.id}`] = 100;
    expect([guardOf(g, town), townHeld(g, town)]).toEqual([0, false]);
    expect(armysShore(g, 'JAP', town, null)).toBe(true);
  });
});
