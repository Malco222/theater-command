// The computer's admirals use every fleet (ruling 52; the grand campaign). A patrol fleet that is free to move and has
// nothing better than its usual beat is not left idle on it: it sails as the escort of a lane of its own side's that
// the raiders are bleeding and nobody guards, and failing that its destroyers hunt the submarines at work on its beat.
import { describe, expect, it } from 'vitest';
import fleetData from '../data/campaign/fleets.json';
import { createGame, declareWar } from '../src/campaign/game';
import { FLEETS, atSea, navalAI, sail } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Fleet, Game } from '../src/campaign/types';

const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
/** With one of the two dials off for the length of a test. */
const without = (dial: 'guard' | 'hunt', run: () => void): void => {
  const was = FLEETS.ai[dial];
  FLEETS.ai[dial] = false;
  try { run(); } finally { FLEETS.ai[dial] = was; }
};

describe('the data', () => {
  it('has a dial for each, and says so in the admirals\' note', () => {
    expect(fleetData.ai.guard).toBe(true);
    expect(fleetData.ai.hunt).toBe(true);
    expect(fleetData._about.ai).toContain('Ruling 52');
  });
});

describe('hunting the submarines on its beat', () => {
  it('a free patrol fleet with no enemy fleet to fight hunts where the submarines are at work', () => {
    const g = grand();
    const f = fleet(g, 'FRA', 'Atlantic Fleet'), u = fleet(g, 'GER', 'U-boat Fleet');
    navalAI(g, 'GER');
    expect(u.stance).toBe('raid');
    // The submarines are in a sea next to its own.
    expect(g.seas[f.station].adj).toContain(u.zone);
    navalAI(g, 'FRA');
    expect(f.stance).toBe('hunt');
    expect(f.zone).toBe(u.zone);
    // With the dial off it keeps its usual beat.
    without('hunt', () => { navalAI(g, 'FRA'); });
    expect(f.stance).toBe('patrol');
    expect(f.zone).toBe(f.station);
  });

  it('not a fleet without destroyers, nor one tied to its station, nor beyond its beat', () => {
    const g = grand();
    const f = fleet(g, 'FRA', 'Atlantic Fleet');
    navalAI(g, 'GER');
    const dd = f.ships.dd;
    f.ships.dd = 0;
    navalAI(g, 'FRA');
    expect(f.stance).toBe('patrol');
    f.ships.dd = dd;
    f.tied = true;
    navalAI(g, 'FRA');
    expect(f.stance).not.toBe('hunt');
    f.tied = false;
    // The Home Fleet's beat is the North Sea: the submarines in the Atlantic are the escorts' business.
    const home = fleet(g, 'UK', 'Home Fleet'), u = fleet(g, 'GER', 'U-boat Fleet');
    expect(g.seas[home.station].adj).not.toContain(u.zone);
    navalAI(g, 'UK');
    expect(home.stance).toBe('patrol');
    expect(home.zone).toBe(home.station);
  });

  it('not while enemy surface ships are out on its beat, and it goes back to an enemy fleet it can beat', () => {
    const g = grand();
    const f = fleet(g, 'FRA', 'Atlantic Fleet'), u = fleet(g, 'GER', 'U-boat Fleet'), b = fleet(g, 'GER', 'Battle Fleet');
    navalAI(g, 'GER');
    navalAI(g, 'FRA');
    expect(f.stance).toBe('hunt');
    // A few German cruisers put out into the sea the submarines are in.
    b.ships = { bb: 0, cv: 0, ca: 2, dd: 2, ss: 0 };
    b.zone = u.zone; b.stance = 'patrol';
    expect(atSea(b)).toBe(true);
    navalAI(g, 'FRA');
    expect(f.stance).toBe('patrol');
    expect(f.zone).toBe(b.zone);
  });

  it('keeps out of a sea where aircraft have just struck it', () => {
    const g = grand();
    const f = fleet(g, 'FRA', 'Atlantic Fleet'), u = fleet(g, 'GER', 'U-boat Fleet');
    navalAI(g, 'GER');
    f.struck = [g.turn, 1, u.zone];
    navalAI(g, 'FRA');
    expect(f.stance).toBe('patrol');
    expect(f.zone).toBe(f.station);
  });
});

describe('guarding a lane nobody guards', () => {
  /** Italy at war with Britain, which holds Malta astride the Libya convoys. */
  const med = (): Game => { const g = grand(); declareWar(g, 'ITA', 'UK'); return g; };

  it('a free patrol fleet sails with its own nation\'s lane that the enemy bleeds, and one fleet is enough', () => {
    const g = med();
    const a = fleet(g, 'ITA', '1st Fleet'), b = fleet(g, 'ITA', '2nd Fleet');
    expect(g.byName.Malta.ctrl).toBe('UK');
    navalAI(g, 'ITA');
    const sailing = [a, b].filter((f) => f.stance === 'escort');
    expect(sailing.length).toBe(1);
    expect(sailing[0].lane).toBe('libya');
    // The other keeps its beat: the lane is guarded.
    expect([a, b].find((f) => f.stance !== 'escort')!.stance).toBe('patrol');
    // And next month it is the same fleet still: it does not hand the lane back and forth.
    navalAI(g, 'ITA');
    expect([a, b].filter((f) => f.stance === 'escort')).toEqual(sailing);
  });

  it('not with the dial off, not at peace with the fortress\'s owner, and not a fleet with an enemy fleet to fight', () => {
    const g = med();
    const a = fleet(g, 'ITA', '1st Fleet'), b = fleet(g, 'ITA', '2nd Fleet');
    without('guard', () => { navalAI(g, 'ITA'); });
    expect([a, b].some((f) => f.stance === 'escort')).toBe(false);
    // Germany's war alone bleeds no Italian lane.
    const p = grand();
    p.nations.ITA.atWar = ['POL']; p.nations.POL.atWar.push('ITA');
    navalAI(p, 'ITA');
    expect(p.fleets.filter((f) => f.nat === 'ITA').some((f) => f.stance === 'escort')).toBe(false);
    // A weak British squadron at sea on the 1st Fleet's own station is its business first.
    const q = med();
    const m = fleet(q, 'UK', 'Mediterranean Fleet'), q1 = fleet(q, 'ITA', '1st Fleet');
    m.ships = { bb: 0, cv: 0, ca: 2, dd: 2, ss: 0 };
    m.zone = q1.station; m.stance = 'patrol';
    navalAI(q, 'ITA');
    expect(q1.stance).toBe('patrol');
    expect(q1.zone).toBe(q1.station);
  });

  it('leaves a lane another fleet of its side already sails with', () => {
    const g = med();
    const a = fleet(g, 'ITA', '1st Fleet'), b = fleet(g, 'ITA', '2nd Fleet');
    // The 2nd Fleet is out with the convoys already: the 1st has no call to join it, and the 2nd stays with them.
    expect(sail(g, b, g.seas.findIndex((s) => s.name === 'Central Mediterranean'), 'escort', 'libya')).toBe(true);
    navalAI(g, 'ITA');
    expect(a.stance).toBe('patrol');
    expect(b.stance).toBe('escort');
    expect(b.lane).toBe('libya');
  });
});

describe('who is left as he was', () => {
  it('the player\'s own fleets, and every fleet in a history campaign', () => {
    const h = grand('FRA');
    const f = fleet(h, 'FRA', 'Atlantic Fleet');
    const was = [f.zone, f.stance];
    navalAI(h, 'GER');
    navalAI(h, 'FRA');
    expect([f.zone, f.stance]).toEqual(was);

    const s = createGame(1, null);
    for (const n of Object.values(s.nations)) navalAI(s, n.id);
    expect(s.fleets.some((x) => x.stance === 'hunt')).toBe(false);
  });
});
