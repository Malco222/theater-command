// Guns that guard a railway (the design director's ruling 125; the art director's guard-the-line brief). The rule
// is the old one: anti-aircraft guns in a province take 15% a level off the chance that a raid cuts its railway.
// These are the words that say so: the sentence and the order on the railway's row, the signs beside the track,
// and a bombed line that says whether guns stood there.
import { describe, expect, it } from 'vitest';
import { turnToDate } from '../src/core/ticket';
import { airBattles, airFleetsOf, setMission } from '../src/campaign/air';
import { ARSENAL, offers, orderOffer, workLevel } from '../src/campaign/arsenal';
import { createGame } from '../src/campaign/game';
import { GUARD_TOP, guardSays, guardStops, guardTabs } from '../src/campaign/guard';
import { cutSays } from '../src/campaign/intel';
import { RAILWAYS, bombRail, cutRail, cutThroughGuns, mendRail, onNet, railBombChance, railCut } from '../src/campaign/rails';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { orderArsenal } from '../src/campaign/turn';
import type { Game, Region } from '../src/campaign/types';

const grand = (seed = 3, human: string | null = 'GER'): Game => startOpen(createGame(seed, human));
const date = (t: number): string => turnToDate(t).label;
const with_ = <T>(table: Record<string, number>, key: string, value: number, run: () => T): T => { const was = table[key]; table[key] = value; try { return run(); } finally { table[key] = was; } };
const guns = (g: Game, r: Region, level: number): void => { if (level > 0) g.flags[`wk:aa:${r.id}`] = level; else delete g.flags[`wk:aa:${r.id}`]; };
const STOPS = 'No train runs into, out of or through';

describe('the sentence on the railway\'s row', () => {
  it('says no guns guard the line, and what guns of each level stop', () => {
    const g = grand();
    const berlin = g.byName['Berlin'];
    guns(g, berlin, 0); // (1.5.1: the start guns stand at Berlin, Germany's capital; this test needs bare ground)
    expect(onNet(berlin.id)).toBe(true);
    expect(guardSays(g, berlin.id)).toMatchObject({ level: 0, coming: 0, ready: null, stops: 0, line: 'No guns are built here to guard the railway.' });
    guns(g, berlin, 1);
    expect(guardSays(g, berlin.id)!.line).toBe('Level 1 guns guard the railway here: they stop 15% of the bombing that would cut it.');
    guns(g, berlin, 2);
    expect(guardSays(g, berlin.id)).toMatchObject({ level: 2, line: 'Level 2 guns guard the railway here: they stop 30% of the bombing that would cut it.' });
    guns(g, berlin, 3);
    expect(guardSays(g, berlin.id)!.line).toBe('Level 3 guns guard the railway here: they stop 45% of the bombing that would cut it.');
  });

  it('says what the rule does: the share is the one a raid\'s chance loses', () => {
    const g = grand();
    const berlin = g.byName['Berlin'];
    const bare = railBombChance(g, berlin.id, 450);
    expect(bare).toBeCloseTo(0.5, 6);
    for (let level = 1; level <= GUARD_TOP; level++) {
      guns(g, berlin, level);
      const said = guardSays(g, berlin.id)!;
      expect(said.stops).toBeCloseTo(level * ARSENAL.works.aa.cut, 6);
      expect(guardStops(level)).toBe(said.stops);
      expect(railBombChance(g, berlin.id, 450, said.stops)).toBeCloseTo(bare * (1 - said.stops), 6);
    }
  });

  it('has nothing to say where no railway runs, and outside the grand campaign', () => {
    const g = grand();
    const none = g.regions.find((r) => !onNet(r.id) && r.ctrl === 'GER') ?? g.regions.find((r) => !onNet(r.id))!;
    expect(none).toBeTruthy();
    expect(guardSays(g, none.id)).toBeNull();
    const plain = createGame(3, 'GER');
    expect(guardSays(plain, plain.byName['Berlin'].id)).toBeNull();
    expect(guardTabs(plain, [plain.byName['Berlin'].id], true)).toEqual([]);
  });

  it('says the guns on enemy ground too, with no order', () => {
    const g = grand();
    const warsaw = g.byName['Warsaw'];
    expect(warsaw.ctrl).not.toBe('GER');
    expect(guardSays(g, warsaw.id)).toMatchObject({ line: 'No guns are built here to guard the railway.', order: null });
    guns(g, warsaw, 2);
    expect(guardSays(g, warsaw.id)).toMatchObject({ level: 2, coming: 0, order: null, line: 'Level 2 guns guard the railway here: they stop 30% of the bombing that would cut it.' });
  });
});

describe('GUARD THE LINE: the build page\'s order by a second door', () => {
  it('offers level 1 where no guns stand, at the build page\'s own cost and months', () => {
    const g = grand();
    const berlin = g.byName['Berlin'];
    guns(g, berlin, 0); // (1.5.1: the start guns stand at Berlin, Germany's capital; this test needs bare ground)
    g.nations.GER.pp = 500;
    const o = offers(g, 'GER', berlin.id).find((x) => x.key === 'work:aa')!;
    expect(guardSays(g, berlin.id)!.order).toEqual({ head: 'Guard the line', text: 'build level 1 guns', level: 1, cost: o.cost, months: o.months, why: null, poor: false });
    expect(o.months).toBe(2);
    expect(o.blurb).toBe('Heavy guns round the factories and the rail yards. Each level stops part of the bombing of this province, its railway included, and brings down more of the bombers.');
  });

  it('is one order with two doors: placed from the build page it shows on the railway, building and then standing', () => {
    const g = grand();
    const berlin = g.byName['Berlin'];
    guns(g, berlin, 0); // (1.5.1: the start guns stand at Berlin, Germany's capital; this test needs bare ground)
    g.nations.GER.pp = 500;
    const b = orderArsenal(g, 'work:aa', berlin.id)!;
    expect(b).toMatchObject({ what: 'work', item: 'aa', region: berlin.id, eta: g.turn + 2 });
    const said = guardSays(g, berlin.id)!;
    expect(said).toMatchObject({ level: 0, coming: 1, ready: g.turn + 2, line: `Level 1 guns are being built here: ready in ${date(g.turn + 2)}.` });
    // The next order from either door is the next level.
    const next = offers(g, 'GER', berlin.id).find((x) => x.key === 'work:aa')!;
    expect(next.size).toBe('level 2');
    expect(said.order).toMatchObject({ head: 'Guard it better', text: 'build level 2 guns', level: 2, cost: next.cost });
    // Guns standing with a level more on order say both.
    guns(g, berlin, 1);
    g.nations.GER.queue = g.nations.GER.queue.filter((x) => x !== b);
    orderOffer(g, 'GER', 'work:aa', berlin.id);
    expect(guardSays(g, berlin.id)).toMatchObject({ level: 1, coming: 1, line: `Level 1 guns guard the railway here: they stop 15% of the bombing that would cut it. Level 2 guns are being built here: ready in ${date(g.turn + 2)}.` });
    expect(guardSays(g, berlin.id)!.order).toMatchObject({ head: 'Guard it better', level: 3 });
  });

  it('offers nothing at the top level, or with the top level on order', () => {
    const g = grand();
    const berlin = g.byName['Berlin'];
    g.nations.GER.pp = 500;
    guns(g, berlin, 3);
    expect(guardSays(g, berlin.id)!.order).toBeNull();
    guns(g, berlin, 2);
    expect(guardSays(g, berlin.id)!.order).toMatchObject({ head: 'Guard it better', text: 'build level 3 guns', level: 3 });
    orderOffer(g, 'GER', 'work:aa', berlin.id);
    expect(guardSays(g, berlin.id)).toMatchObject({ level: 2, coming: 1, order: null });
  });

  it('says why where the treasury cannot cover it, and offers nothing on an ally\'s ground', () => {
    const g = grand();
    const berlin = g.byName['Berlin'];
    guns(g, berlin, 0); // (1.5.1: the start guns stand at Berlin, Germany's capital; this test needs bare ground)
    g.nations.GER.pp = 0;
    expect(guardSays(g, berlin.id)!.order).toMatchObject({ head: 'Guard the line', why: 'The treasury cannot cover it.', poor: true });
    expect(guardSays(g, berlin.id, 'ITA')!.order).toBeNull();
    expect(guardSays(g, berlin.id, null)!.order).toBeNull();
  });
});

describe('the guard signs beside the track', () => {
  it('gives the guarded stretches alone for the lit railways, and every stretch for a route under the pointer', () => {
    const g = grand();
    const berlin = g.byName['Berlin'], saxony = g.byName['Saxony'], hamburg = g.byName['Hamburg'];
    guns(g, berlin, 0); // (1.5.1: the start guns stand at Berlin, Germany's capital; this test needs bare ground)
    const none = g.regions.find((r) => !onNet(r.id))!;
    guns(g, saxony, 2);
    const route = [berlin.id, saxony.id, hamburg.id, none.id, saxony.id];
    expect(guardTabs(g, route, false)).toEqual([{ region: saxony.id, level: 2 }]);
    expect(guardTabs(g, route, true)).toEqual([{ region: berlin.id, level: 0 }, { region: saxony.id, level: 2 }, { region: hamburg.id, level: 0 }]);
    expect(workLevel(g, 'aa', saxony.id)).toBe(2);
  });
});

describe('a line cut by bombing says whether guns stood there', () => {
  it('keeps the level of the guns with the cut, and forgets it when the line is relaid', () => {
    const g = grand();
    const berlin = g.byName['Berlin'], saxony = g.byName['Saxony'];
    const B = RAILWAYS.bomb as unknown as Record<string, number>;
    with_(B, 'scale', 1, () => with_(B, 'most', 1, () => {
      expect(bombRail(g, 'UK', berlin.id, 900, 0, 0)).toBe(true);
      expect(bombRail(g, 'UK', saxony.id, 900, 0, 2)).toBe(true);
    }));
    expect(cutThroughGuns(g, berlin.id)).toBe(0);
    expect(cutThroughGuns(g, saxony.id)).toBe(2);
    expect(cutSays(g, berlin.id)!.line).toBe(`Bombed in ${date(g.turn)}. No guns were built here to guard the railway. ${STOPS} Berlin until engineers relay the track. Only engineers can be carried up to the break.`);
    expect(cutSays(g, saxony.id)!.line).toBe(`Bombed in ${date(g.turn)}, through level 2 guns. ${STOPS} Saxony until engineers relay the track. Only engineers can be carried up to the break.`);
    // The pointing box's short line stays as built.
    expect(cutSays(g, saxony.id)!.short).toBe(`railway cut (bombed, ${date(g.turn)})`);
    // It is kept through a save.
    const back = loadGame(saveGame(g))!;
    expect(cutThroughGuns(back, saxony.id)).toBe(2);
    expect(mendRail(g, saxony.id)).toBe(true);
    expect(cutThroughGuns(g, saxony.id)).toBeNull();
    expect(g.flags[`railaa:${saxony.id}`]).toBeUndefined();
  });

  it('says nothing of guns for another cause, or for a cut whose guns were never kept', () => {
    const g = grand();
    const berlin = g.byName['Berlin'], saxony = g.byName['Saxony'], hamburg = g.byName['Hamburg'];
    guns(g, berlin, 2);
    expect(cutRail(g, berlin.id, 'saboteurs')).toBe(true);
    expect(cutThroughGuns(g, berlin.id)).toBeNull();
    expect(cutSays(g, berlin.id)!.line).toBe(`Blown by saboteurs in ${date(g.turn)}. ${STOPS} Berlin until engineers relay the track. Only engineers can be carried up to the break.`);
    expect(cutRail(g, saxony.id, 'bombed')).toBe(true);
    expect(cutThroughGuns(g, saxony.id)).toBeNull();
    expect(cutSays(g, saxony.id)!.line).toBe(`Bombed in ${date(g.turn)}. ${STOPS} Saxony until engineers relay the track. Only engineers can be carried up to the break.`);
    // A raid that does not cut the line keeps nothing.
    expect(bombRail(g, 'UK', hamburg.id, 0, 0, 3)).toBe(false);
    expect(railCut(g, hamburg.id)).toBe(false);
    expect(g.flags[`railaa:${hamburg.id}`]).toBeUndefined();
  });

  it('is kept by a raid as it is flown: the guns that stood under the bombers', () => {
    const g = grand(3, 'GER');
    const f = airFleetsOf(g, 'GER').find((x) => x.bmr > 0 && g.regions[x.base].name === 'Berlin')!;
    const poznan = g.byName['Poznan'];
    guns(g, poznan, 1);
    expect(setMission(g, f, 'bomb', poznan.id)).toBe(true);
    // Bombers enough to make it certain, and guns that stop nothing of it: the level is kept all the same.
    const B = RAILWAYS.bomb as unknown as Record<string, number>, A = ARSENAL.works.aa as unknown as Record<string, number>;
    with_(A, 'cut', 0, () => with_(B, 'scale', 1, () => with_(B, 'most', 1, () => airBattles(g, 'GER'))));
    expect(railCut(g, poznan.id)).toBe(true);
    expect(cutThroughGuns(g, poznan.id)).toBe(1);
  });

  it('with the order following the cut: guns can be ordered while the line lies broken', () => {
    const g = grand();
    const berlin = g.byName['Berlin'];
    guns(g, berlin, 0); // (1.5.1: the start guns stand at Berlin, Germany's capital; this test needs bare ground)
    g.nations.GER.pp = 500;
    expect(cutRail(g, berlin.id, 'bombed', 'UK')).toBe(true);
    expect(guardSays(g, berlin.id)!.order).toMatchObject({ head: 'Guard the line', why: null });
  });
});
