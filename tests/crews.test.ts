// Veteran crews (release 2): fleets and air groups learn from their fights, wear a rank, fight a little better for
// it, and are thinned by the new men who fill their gaps. One switch; with it off nothing is read or written.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { saveGame } from '../src/campaign/save';
import { fleetCard, merge, detach } from '../src/campaign/fleets';
import { fit } from '../src/campaign/navy';
import { drafted } from '../src/campaign/stories';
import {
  CREWS, RANKS, crewAim, crewFought, crewMul, crewOf, crewRules, crewSize, crewView, crewXp, crewsLine, crewsMonth,
  rankIndex, rankName, setCrews, startXp, trainXp,
} from '../src/campaign/crews';
import type { Fleet, Game } from '../src/campaign/types';

const grand = (seed = 17): Game => startOpen(createGame(seed, null));
const fleetOf = (g: Game, nat: string): Fleet => g.fleets.find((f) => f.nat === nat && f.ships.bb + f.ships.ca + f.ships.dd > 0)!;

describe('veteran crews: the switch', () => {
  it('is off in the rules, and with it off every multiplier is exactly 1 and nothing is written', () => {
    const g = grand();
    expect(crewRules(g)).toBe(false);
    for (let t = 0; t < 6; t++) runTurnAI(g);
    expect(g.fleets.every((f) => f.crew === undefined)).toBe(true);
    expect(g.airFleets.every((f) => f.crew === undefined)).toBe(true);
    const f = fleetOf(g, 'UK');
    expect(crewMul(g, f, 'sea')).toBe(1);
    expect(crewMul(g, g.airFleets[0], 'air')).toBe(1);
    expect(crewAim(g, g.airFleets)).toBe(1);
    expect(crewView(g, f, 'sea')).toBeNull();
    expect(crewsLine(g, [f], [fleetOf(g, 'GER')], 'sea')).toBeNull();
    expect('crew' in fleetCard(g, f)).toBe(false);
    expect(saveGame(g)).not.toContain('"crew"');
  });

  it('turns on and off with setCrews, leaving no flag behind when off', () => {
    const g = grand();
    setCrews(g, true);
    expect(g.flags.crews).toBe(1);
    setCrews(g, false);
    expect('crews' in g.flags).toBe(false);
  });
});

describe('veteran crews: ranks', () => {
  it('reads the same five ranks a corps wears, a chevron a step', () => {
    expect(RANKS.map((r) => r[1])).toEqual(['Green', 'Trained', 'Seasoned', 'Veteran', 'Elite']);
    expect([0, 0.19, 0.2, 0.45, 0.6, 0.79, 0.8, 0.95].map(rankIndex)).toEqual([0, 0, 1, 2, 3, 3, 4, 4]);
    expect(rankName(0.65)).toBe('Veteran');
  });

  it('starts each navy and air force where its country stood: Japan\'s naval pilots elite-trained, the Soviet air force green', () => {
    const g = grand();
    expect(startXp(g, 'JAP', 'air')).toBeGreaterThan(startXp(g, 'USA', 'air'));
    expect(startXp(g, 'UK', 'sea')).toBeGreaterThan(startXp(g, 'GER', 'sea'));
    expect(rankName(startXp(g, 'SOV', 'air'))).toBe('Green');
    // The schools by 1944: Germany's and Japan's pilots come out greener than America's.
    g.turn = 54;
    expect(trainXp(g, 'GER', 'air')).toBeLessThan(trainXp(g, 'USA', 'air'));
    expect(trainXp(g, 'JAP', 'air')).toBeLessThan(0.15);
  });

  it('keeps the edge modest: green crews fight at about 95%, elite ones at about 111%', () => {
    const g = grand();
    setCrews(g, true);
    const f = fleetOf(g, 'UK');
    crewOf(g, f, 'sea').xp = 0.1;
    expect(crewMul(g, f, 'sea')).toBeCloseTo(0.95, 2);
    f.crew!.xp = 0.9;
    expect(crewMul(g, f, 'sea')).toBeCloseTo(1.11, 2);
    f.crew!.xp = CREWS.pivot;
    expect(crewMul(g, f, 'sea')).toBe(1);
  });

  it('puts the crews into how fit a fleet is to fight', () => {
    const g = grand();
    const f = fleetOf(g, 'UK');
    const before = fit(g, f);
    setCrews(g, true);
    crewOf(g, f, 'sea').xp = 0.85;
    expect(fit(g, f)).toBeCloseTo(before * crewMul(g, f, 'sea'), 10);
    expect(fit(g, f)).toBeGreaterThan(before);
  });
});

describe('veteran crews: learning and new men', () => {
  it('teaches more from a fight won, less as the men know more, and never past the most', () => {
    const g = grand();
    setCrews(g, true);
    const f = fleetOf(g, 'GER');
    const c = crewOf(g, f, 'sea');
    c.xp = 0.3;
    crewFought(g, f, 'sea', true);
    const won = c.xp - 0.3;
    c.xp = 0.3;
    crewFought(g, f, 'sea', false);
    const lost = c.xp - 0.3;
    expect(won).toBeGreaterThan(lost);
    c.xp = 0.8;
    crewFought(g, f, 'sea', true);
    expect(c.xp - 0.8).toBeLessThan(won);
    for (let i = 0; i < 200; i++) crewFought(g, f, 'sea', true);
    expect(c.xp).toBeLessThanOrEqual(CREWS.most);
    expect(c.fought).toBe(203);
  });

  it('thins the crews when new ships join, by what the newcomers count for, and keeps what the rest know after losses', () => {
    const g = grand();
    setCrews(g, true);
    const f = fleetOf(g, 'UK');
    const c = crewOf(g, f, 'sea');
    c.xp = 0.8;
    const before = crewSize(f, 'sea');
    f.ships.dd += 10;
    crewOf(g, f, 'sea');
    const add = (crewSize(f, 'sea') - before) * CREWS.dilute;
    expect(c.xp).toBeCloseTo((0.8 * before + trainXp(g, 'UK', 'sea') * add) / (before + add), 3);
    const now = c.xp;
    f.ships.dd -= 5;
    crewOf(g, f, 'sea');
    expect(c.xp).toBe(now);
    expect(c.n).toBeCloseTo(crewSize(f, 'sea'), 3);
  });

  it('weighs two fleets\' crews by their size when they join, and lets ships sent away keep theirs', () => {
    const g = grand();
    setCrews(g, true);
    const uk = g.fleets.filter((f) => f.nat === 'UK' && f.ships.dd > 2);
    const a = uk[0], b = uk[1];
    // Put them in one harbour so that they may join.
    a.zone = b.zone; a.base = b.base; a.home = b.home; a.stance = b.stance;
    crewOf(g, a, 'sea').xp = 0.8;
    crewOf(g, b, 'sea').xp = 0.2;
    const na = crewSize(a, 'sea'), nb = crewSize(b, 'sea');
    expect(merge(g, a, b)).toBe(true);
    expect(b.crew!.xp).toBeCloseTo((0.8 * na + 0.2 * nb) / (na + nb), 3);
    const sq = detach(g, b, { dd: 2 });
    expect(sq).not.toBeNull();
    expect(sq!.crew!.xp).toBe(b.crew!.xp);
  });

  it('thins every nation\'s corps with green drafts when on, and only the player\'s when off', () => {
    const g = grand();
    const c = g.corps.find((x) => x.nat === 'GER' && x.kind !== 'fort')!;
    c.xp = 0.8;
    drafted(g, c, 0.6, 0.4);
    expect(c.xp).toBe(0.8);
    setCrews(g, true);
    drafted(g, c, 0.6, 0.4);
    expect(c.xp).toBeLessThan(0.8);
  });

  it('names a player\'s unit in the news when it reaches Veteran, once', () => {
    const g = grand();
    g.human = 'UK';
    setCrews(g, true);
    const f = fleetOf(g, 'UK');
    crewOf(g, f, 'sea').xp = 0.58;
    const n = g.headlines.length;
    crewFought(g, f, 'sea', true);
    expect(rankName(f.crew!.xp)).toBe('Veteran');
    expect(g.headlines.slice(n).some((h) => h.text.includes(f.name) && /veteran/.test(h.text))).toBe(true);
    const m = g.headlines.length;
    crewFought(g, f, 'sea', true);
    expect(g.headlines.length).toBe(m);
  });
});

describe('veteran crews: in a war', () => {
  it('gives every fleet and air group a record within the first months, and moves the veterans up', () => {
    const g = grand(21);
    setCrews(g, true);
    for (let t = 0; t < 14; t++) runTurnAI(g);
    // (A group formed during another nation's month is counted at its own nation's next month.)
    for (const nat of g.order) crewsMonth(g, nat);
    const live = g.fleets.filter((f) => g.nations[f.nat]?.alive);
    expect(live.every((f) => f.crew !== undefined)).toBe(true);
    expect(g.airFleets.filter((f) => g.nations[f.nat]?.alive && f.ftr + f.bmr > 0).every((f) => f.crew !== undefined)).toBe(true);
    // Someone has been in action and learned from it.
    expect(g.airFleets.some((f) => (f.crew?.fought ?? 0) > 0)).toBe(true);
    expect(g.airFleets.some((f) => f.crew && f.crew.xp > startXp(g, f.nat, 'air') + 0.02)).toBe(true);
    // Every crew stays within its bounds.
    for (const u of [...g.fleets, ...g.airFleets]) if (u.crew) { expect(u.crew.xp).toBeGreaterThanOrEqual(0); expect(u.crew.xp).toBeLessThanOrEqual(CREWS.most); }
    // And the views read it.
    const v = crewView(g, fleetOf(g, 'UK'), 'sea')!;
    expect(v.rank).toBe(rankName(crewXp(g, fleetOf(g, 'UK'), 'sea')));
    expect(v.line).toMatch(/crews/);
    expect(fleetCard(g, fleetOf(g, 'UK')).crew).toEqual(v);
  });

  it('is kept by a save', () => {
    const g = grand();
    setCrews(g, true);
    for (let t = 0; t < 3; t++) runTurnAI(g);
    crewsMonth(g, 'UK');
    expect(saveGame(g)).toContain('"crew"');
  });
});
