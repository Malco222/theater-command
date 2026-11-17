// The landing's refusal (Version 84.3; the design director's ruling 207, on the Playtester's find from America's
// 1942 start): where no sea lane serves a landing the refusal gives the true one of its reasons, never two
// together. A sea on the shortest crossing that is closed to the troopships is named, with each side's strength
// there as the rule counts it and the smallest strength that would open it; a coast out of reach is called too
// far, with how far a landing reaches. Words and display only: the lane itself is seaPath's, as before, and the
// list of crossings the staffs read is not changed by the row the Navy page adds.
import { beforeAll, describe, expect, it } from 'vitest';
import { addCorps, atWar, createGame } from '../src/campaign/game';
import { NAVY, closedLine, closedTo, crossingsOf, fleetPower, laneBar, opensAt, planLanding, seaPath, seaSide, surfaceShips, whyNotLand, type CrossingOutlook } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Fleet, Game, Region } from '../src/campaign/types';

/** A war the computer plays on every side, as it stands once Poland has fallen. */
const opened = (): Game => {
  const g = startOpen(createGame(3, null));
  for (let k = 0; k < 8 && (k < 2 || g.nations.POL.alive); k++) runTurnAI(g);
  return g;
};
const corps = (g: Game, nat: string, at: Region): Corps => addCorps(g, nat, `Test Corps ${g.nextCorps}`, 'inf', 3, at.id, {});
const toBritain = (g: Game): CrossingOutlook => crossingsOf(g, 'GER').find((x) => x.owner === 'UK' && x.home)!;
/** A strong British fleet put to sea in a sea, so that it is closed to Germany's troopships. */
const closeSea = (g: Game, z: number): void => {
  const model = g.fleets.filter((f) => f.nat === 'UK' && surfaceShips(f) > 0).sort((a, b) => fleetPower(g, b) - fleetPower(g, a))[0];
  const f: Fleet = { ...model, id: Math.max(...g.fleets.map((x) => x.id)) + 1, ships: { ...model.ships, bb: model.ships.bb + 40 }, base: -1, home: z, zone: z, stance: 'patrol' };
  g.fleets.push(f);
};
const pairs = (rows: CrossingOutlook[]): string[] => rows.map((x) => `${x.from.id}>${x.to.id}:${x.path.join(',')}:${x.barred}:${x.planned ? 1 : 0}:${x.hope ? 1 : 0}`);

describe('The landing says why no sea lane serves it (ruling 207)', () => {
  let g: Game, x: CrossingOutlook, force: Corps[];
  beforeAll(() => {
    g = opened();
    x = toBritain(g);
    g.nations.GER.sealift = Math.max(g.nations.GER.sealift, 3);
    force = [corps(g, 'GER', x.from)];
  });

  it('gives no lane reason while the lane is open', () => {
    const open = crossingsOf(g, 'GER').find((y) => y.barred === null)!;
    expect(open).toBeDefined();
    const there = [corps(g, 'GER', open.from)];
    expect(seaPath(g, 'GER', open.from, open.to, { max: NAVY.landing.range })).not.toBeNull();
    expect(planLanding(g, there, open.to).bar).toBeUndefined();
    expect(whyNotLand(g, there, open.to) ?? '').not.toMatch(/closed to your troopships|too far|No sea joins|shut to your ships/);
  });

  it('says "too far" only of a coast beyond a landing reach, with how far it reaches and how far the coast is', () => {
    const far = g.regions.filter((r) => r.seas.length > 0 && atWar(g, 'GER', r.ctrl) && (seaPath(g, 'GER', x.from, r, { surprise: true })?.length ?? 0) > NAVY.landing.range);
    expect(far.length).toBeGreaterThan(0);
    let told = 0;
    for (const r of far) {
      const bar = laneBar(g, 'GER', x.from, r);
      expect(bar.kind).toBe('far');
      const why = whyNotLand(g, force, r) ?? '';
      expect(why).not.toMatch(/or the enemy holds the sea between/);
      const m = /is too far for a landing from .+: troopships cross (\d+) seas at most, and this is (\d+)\.$/.exec(why);
      if (!m) continue; // refused for another reason before the lane is looked at (ground no corps can land on)
      told++;
      expect(Number(m[1])).toBe(NAVY.landing.range);
      expect(Number(m[2])).toBe(bar.kind === 'far' ? bar.seas : -1);
      expect(Number(m[2])).toBeGreaterThan(NAVY.landing.range);
      expect(planLanding(g, force, r).bar).toEqual(bar);
    }
    expect(told).toBeGreaterThan(0);
  });

  it('names the closed sea, both strengths and the smallest strength that opens it', () => {
    for (const z of x.from.seas) closeSea(g, z);
    for (const z of x.from.seas) expect(closedTo(g, z, 'GER')).toBe(true);
    expect(seaPath(g, 'GER', x.from, x.to, { max: NAVY.landing.range })).toBeNull();
    const bar = laneBar(g, 'GER', x.from, x.to);
    expect(bar.kind).toBe('sea');
    const sea = bar.kind === 'sea' ? bar.sea : -1;
    expect(x.from.seas).toContain(sea);
    const e = seaSide(g, sea, 'GER', true).total, own = seaSide(g, sea, 'GER', false).total;
    // The rule: closed while the enemy has at least `ratio` for every one of his, so exactly that is still closed.
    const n = opensAt(g, sea, 'GER');
    expect(n).toBe(Math.max(Math.floor(e / NAVY.closed.ratio), Math.floor(Math.round(e) / NAVY.closed.ratio)) + 1);
    // One less is still closed by the figure the sentence prints; that many is open by the rule itself.
    expect(Math.round(e) >= NAVY.closed.ratio * (n - 1) || e >= NAVY.closed.ratio * (n - 1)).toBe(true);
    expect(e >= NAVY.closed.ratio * n).toBe(false);
    expect(n).toBeGreaterThan(own);
    const why = whyNotLand(g, force, x.to)!;
    expect(why).toBe(closedLine(g, sea, 'GER'));
    expect(why).toMatch(new RegExp(`^The ${g.seas[sea].name} is closed to your troopships: the enemy has [\\d,]+ there to your [\\d,]+, and ${n} would open it\\. Put a fleet to sea there, or (win the sky over the enemy's coast on it|beat the enemy's fleets in it)\\.$`));
    expect(why).not.toMatch(/too far/);
    expect(planLanding(g, force, x.to).bar).toEqual(bar);
    // A sea that is open has no such number.
    const open = g.seas.find((s) => !closedTo(g, s.id, 'GER'))!;
    expect(opensAt(g, open.id, 'GER')).toBe(0);
  });

  it('leaves the list the staffs read as it was, and adds the short crossing for the page alone', () => {
    const plain = crossingsOf(g, 'GER'), page = crossingsOf(g, 'GER', { short: true });
    expect(pairs(page.filter((y) => !y.short))).toEqual(pairs(plain));
    expect(plain.some((y) => y.short)).toBe(false);
    for (const s of page.filter((y) => y.short)) {
      expect(s.barred).toBe('sea');
      expect(s.shut).toBeGreaterThanOrEqual(0);
      expect(closedTo(g, s.shut!, 'GER')).toBe(true);
      const same = plain.filter((y) => y.foe === s.foe && y.owner === s.owner && y.home === s.home);
      expect(same.length).toBeGreaterThan(0);
      for (const y of same) { expect(y.barred).not.toBe('sea'); expect(y.path.length).toBeGreaterThan(s.path.length); }
    }
    // A row a closed sea bars carries the sea, for the page's sentence.
    for (const y of page) expect(y.shut !== undefined).toBe(y.barred === 'sea');
  });
});
