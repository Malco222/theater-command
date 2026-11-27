// Ruling 215 (17 November 2026) on the Pacific planner's paper 19, what stopped Japan in China, with the riders of
// 17:08Z. Three things in the rules, each switched on by data alone, so that no war of Europe's book changes:
//   (b) the staff's one line: by the book, a front that waits on a dated great choice carrying 'wait' opens no general
//       offensive, neither out of impatience nor on a staff's temper row (staff.ts, decide).
//   a great offensive in a war already on: a choice that declares on nobody and is no side-show is weighed on its own
//       front and, when it goes, opens the offensive there against the enemies it names (staff.ts, prospect and go).
//       Without it Japan's offensive of 1944 is never taken up at all (the Pacific seat builder's run 1).
//   (c) the record's line: by the book a computer staff's bound in a theatre with a line is the record's line for the
//       month, a ceiling for the invader and a floor for the invaded, never the player's (line.ts, ai.ts, staff.ts).
// China and Japan are not on main's chart until step 13, so the rules are tried here on Europe's ground with rows made
// up for the test, and the data the ruling makes (data/campaign/china-line.json) is checked for its shape.
import { afterEach, describe, expect, it } from 'vitest';
import openData from '../data/campaign/open.json';
import china from '../data/campaign/china-line.json';
import setupRows from '../data/campaign/china-setup.json';
import { lineGiveBack, lineHome, ringJudgement } from '../src/campaign/ai';
import { EASTERN, addCorps, allied, atWar, createGame, declareWar, removeCorps } from '../src/campaign/game';
import { HISTORY, setStop } from '../src/campaign/history';
import { inLine, lineBinds, lineLeaves, lineShut, lines, type Line } from '../src/campaign/line';
import { decide, prospect, startOpen, type Op } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { whyNotLand, whyNotShip } from '../src/campaign/navy';
import { fieldCtrl } from '../src/campaign/siege';
import { TRAITS, levyRule, traitLevy, traitRule, type Levy } from '../src/campaign/traits';
import type { Corps, Game, Region } from '../src/campaign/types';

const OPEN = openData as unknown as { ops: Record<string, Op[]>; temper: Record<string, unknown>; impatience: { after: number[] } };
const H = HISTORY as unknown as { dates: Record<string, Record<string, number>>; theatre: Record<string, string>; lines?: Line[] };

const grand = (seed = 2, human: string | null = null, stop = 3): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  return g;
};
/** Germany's western front with nobody on the other side of it: any staff that may attack there does. */
const westEmpty = (g: Game): Game => { for (const c of g.corps.filter((c) => c.nat === 'FRA' || c.nat === 'UK' || c.nat === 'CAN')) removeCorps(g, c); return g; };

// A great choice made up for the test: Germany's blow against France, dated by the book to month `date`, in a theatre
// of its own, declaring on nobody, as Japan's offensive of 1944 is written (china-line.json).
const blow = (over: Partial<Op> = {}): Op => ({ id: 't215', name: 'The blow in the west', if: ['war:GER,FRA', 'alive:FRA'], from: 1, to: 40, ratio: 0.8, prep: 2, chance: 0.3, season: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12], push: 3, wait: true, ...over });
const kept: { ops?: Op[]; temper?: unknown; lines?: Line[] } = {};
function withBlow(op: Op, date = 30): void {
  kept.ops = OPEN.ops.GER;
  OPEN.ops.GER = [op, ...OPEN.ops.GER];
  H.dates.GER.t215 = date;
  H.theatre['GER:t215'] = 't215';
}
afterEach(() => {
  if (kept.ops) OPEN.ops.GER = kept.ops;
  if (kept.temper !== undefined) OPEN.temper.GER = kept.temper;
  if ('lines' in kept) { if (kept.lines) H.lines = kept.lines; else delete H.lines; }
  delete H.dates.GER.t215;
  delete H.theatre['GER:t215'];
  delete kept.ops; delete kept.temper; delete kept.lines;
});

describe('215(b): a front waiting on a dated choice with "wait" opens no general offensive by the book', () => {
  it('holds a staff with a temper row, which would otherwise attack whenever its ratio is reached (the catch in 202(l))', () => {
    // Without the key the temper row opens the front at once: the ratio is far past Germany's 1.2.
    withBlow(blow({ wait: false }));
    const plain = westEmpty(grand(2, null, 1));
    plain.turn = 2;
    decide(plain, 'GER');
    expect(plain.flags['st:on:GER:FRA']).toBe(2);
    OPEN.ops.GER[0] = blow();
    const held = westEmpty(grand(2, null, 1));
    held.turn = 2;
    decide(held, 'GER');
    expect(held.flags['st:on:GER:FRA']).toBeUndefined();
  });

  it('holds a staff with no temper row through the last term of impatience', () => {
    kept.temper = OPEN.temper.GER;
    delete OPEN.temper.GER;
    withBlow(blow());
    const late = OPEN.impatience.after[OPEN.impatience.after.length - 1] + 1;
    const g = westEmpty(grand(2, null, 1));
    g.turn = late;
    decide(g, 'GER');
    expect(g.flags['st:on:GER:FRA']).toBeUndefined();
    // Stop 2 keeps the book too.
    const two = westEmpty(grand(2, null, 2));
    two.turn = late;
    decide(two, 'GER');
    expect(two.flags['st:on:GER:FRA']).toBeUndefined();
  });

  it('is the book\'s alone: at History\'s shape, when the choice has gone, or when the book is closed in its theatre, the front is a front like any other', () => {
    withBlow(blow());
    const free = westEmpty(grand(2, null, 3));
    free.turn = 2;
    decide(free, 'GER');
    expect(free.flags['st:on:GER:FRA']).toBe(2);
    const done = westEmpty(grand(2, null, 1));
    done.turn = 2;
    done.flags['st:done:GER:t215'] = 1;
    decide(done, 'GER');
    expect(done.flags['st:on:GER:FRA']).toBe(2);
    const closed = westEmpty(grand(2, null, 1));
    closed.turn = 2;
    closed.flags['hist:off:t215'] = 1;
    decide(closed, 'GER');
    expect(closed.flags['st:on:GER:FRA']).toBe(2);
    // Past its last month the choice holds nothing.
    const past = westEmpty(grand(2, null, 1));
    past.turn = 41;
    decide(past, 'GER');
    expect(past.flags['st:on:GER:FRA']).toBe(41);
  });

  it('leaves the local attacks alone: the record\'s raids', async () => {
    const { probing } = await import('../src/campaign/staff');
    withBlow(blow());
    const g = westEmpty(grand(2, null, 1));
    g.turn = OPEN.impatience.after[0] + 1;
    expect(probing(g, 'GER')?.enemies.has('FRA')).toBe(true);
  });
});

describe('a great offensive in a war already on', () => {
  it('is weighed on the front it is meant for, where a choice that declares on nobody was weighed on none', () => {
    const g = westEmpty(grand(2, null, 1));
    const p = prospect(g, 'GER', blow(), undefined);
    expect(p?.enemies).toContain('FRA');
    // A side-show that declares on nobody (Britain's for Norway) is not one: it is weighed as before.
    expect(prospect(g, 'GER', blow({ side: true }), undefined)).toBeNull();
    // Nor is a choice against an enemy not yet at war.
    expect(prospect(g, 'GER', blow({ if: ['war:GER,SOV'] }), undefined)).toBeNull();
  });

  it('goes in its month by the book and opens the offensive against the enemy it names, pressed for its months', () => {
    withBlow(blow({ push: 3 }), 6);
    const g = westEmpty(grand(2, null, 1));
    // The army is up: brought forward from month 4 (prep 2).
    g.turn = 6;
    g.flags['st:op:GER'] = 1;
    g.flags['st:opAt:GER'] = 4;
    decide(g, 'GER');
    expect(g.flags['st:done:GER:t215']).toBe(6);
    expect(g.flags['st:on:GER:FRA']).toBe(6);
    expect(g.flags['st:push:GER']).toBe(8);
    // And it was taken up two months before, on the ratio of the front as it stood.
    withBlow(blow({ push: 3 }), 6);
    const up = westEmpty(grand(2, null, 1));
    up.turn = 4;
    // (Poland's front left out of it: a general offensive there would keep the army busy.)
    delete up.flags['st:on:GER:POL'];
    up.flags['st:rest:GER:POL'] = 99;
    decide(up, 'GER');
    expect(up.flags['st:op:GER']).toBe(1);
    expect(up.flags['st:on:GER:FRA']).toBeUndefined();
  });
});

describe('219: the record\'s raids as dated choices in 216(a)\'s form, with no new code', () => {
  // A raid made up for the test: Germany's in month 3 against France, a great choice that declares on nobody, beside
  // the dated blow of month 30 that makes the front wait (as Japan's raids of 1939 to 1941 stand beside ichigo).
  const raid = (over: Partial<Op> = {}): Op => ({ id: 'r219', name: 'A raid', if: ['war:GER,FRA', 'alive:FRA'], from: 1, to: 10, prep: 0, chance: 0.3, push: 1, ...over });
  function month3(op: Op): Game {
    withBlow(blow(), 30);
    OPEN.ops.GER = [OPEN.ops.GER[0], op, ...OPEN.ops.GER.slice(1)];
    H.dates.GER.r219 = 3;
    H.theatre['GER:r219'] = 't215';
    const g = westEmpty(grand(2, null, 1));
    g.turn = 3;
    // (Poland's front left out of it: a general offensive there would keep the army busy.)
    delete g.flags['st:on:GER:POL'];
    g.flags['st:rest:GER:POL'] = 99;
    decide(g, 'GER');
    return g;
  }
  afterEach(() => { delete H.dates.GER.r219; delete H.theatre['GER:r219']; });

  it('goes in its month and opens the offensive on the front it names, though another dated choice holds that front waiting', () => {
    const g = month3(raid());
    expect(g.flags['st:done:GER:r219']).toBe(3);
    expect(g.flags['st:on:GER:FRA']).toBe(3);
    expect(g.flags['st:push:GER']).toBe(3);
  });

  it('as a side operation opens no offensive at all', () => {
    const side = month3(raid({ side: true }));
    expect(side.flags['st:done:GER:r219']).toBe(3);
    expect(side.flags['st:on:GER:FRA']).toBeUndefined();
  });

  it('as a side operation with odds asked of it is never taken: it has no front to weigh them on', () => {
    const odds = month3(raid({ side: true, ratio: 0.8 }));
    expect(odds.flags['st:done:GER:r219']).toBeUndefined();
    expect(odds.flags['st:on:GER:FRA']).toBeUndefined();
  });
});

describe('215(c): the record\'s line', () => {
  // A line made up for the test on Poland's ground: Germany may hold the western strip from the first day, take Lodz
  // in months 2 and 3 only, and Kielce from month 5 to 8 in an offensive; Warsaw is never his.
  const test: Line = {
    id: 'test215', theatre: 'test215', land: 'POL', taker: 'GER', until: 20,
    floor: { until: 12, kinds: ['day one', 'lasting', 'ichigo'] },
    spans: {
      Poznan: [{ from: 1, to: 20, kind: 'day one' }], Pomorze: [{ from: 1, to: 20, kind: 'day one' }],
      'Danzig Corridor': [{ from: 1, to: 20, kind: 'day one' }], 'Polish Silesia': [{ from: 1, to: 20, kind: 'lasting' }],
      Lodz: [{ from: 2, to: 3, kind: 'raid' }], Kielce: [{ from: 5, to: 8, kind: 'ichigo' }],
    },
  };
  const drawn = (): void => { kept.lines = H.lines; H.lines = [test]; };
  const at = (g: Game, nm: string) => g.byName[nm];

  it('is nothing without data, and Europe\'s book has none', () => {
    expect(lines()).toEqual([]);
    const g = grand(2, null, 1);
    expect(lineShut(g, 'GER', at(g, 'Warsaw'))).toBe(false);
  });

  it('is a ceiling for the invader\'s computer staff: what the line leaves out this month is shut to it and its allies', () => {
    drawn();
    const g = grand(2, null, 1);
    expect(lineBinds(g, test)).toBe(true);
    expect(lineShut(g, 'GER', at(g, 'Warsaw'))).toBe(true);
    expect(lineShut(g, 'GER', at(g, 'Poznan'))).toBe(false);
    expect(lineShut(g, 'GER', at(g, 'Lodz'))).toBe(true);
    g.turn = 2;
    expect(lineShut(g, 'GER', at(g, 'Lodz'))).toBe(false);
    g.turn = 4;
    expect(lineShut(g, 'GER', at(g, 'Lodz'))).toBe(true);
    // An ally's army is bound with it; ground that is not the invaded country's is not the line's business.
    expect(allied(g, 'SVK', 'GER')).toBe(true);
    expect(lineShut(g, 'SVK', at(g, 'Warsaw'))).toBe(true);
    expect(lineShut(g, 'GER', at(g, 'Paris'))).toBe(false);
    // The Soviet Union is on neither side of this line.
    expect(lineShut(g, 'SOV', at(g, 'Warsaw'))).toBe(false);
  });

  it('lands nobody beyond the line: a computer staff\'s landing there is barred, the player\'s is not', () => {
    drawn();
    const g = grand(2, null, 1);
    const gdynia = at(g, 'Danzig Corridor'), warsaw = at(g, 'Warsaw');
    const cs = g.corps.filter((c) => c.nat === 'GER' && c.kind !== 'fort').slice(0, 1);
    expect(whyNotLand(g, cs, warsaw)).toMatch(/beyond the record's line/);
    expect(whyNotLand(g, cs, gdynia) ?? '').not.toMatch(/record's line/);
    const mine = grand(2, 'FRA', 1);
    const fr = mine.corps.filter((c) => c.nat === 'FRA' && c.kind !== 'fort').slice(0, 1);
    expect(whyNotLand(mine, fr, at(mine, 'Warsaw')) ?? '').not.toMatch(/record's line/);
  });

  it('binds a break-out from a pocket: a ringed staff neither walks into nor attacks floor ground the invader holds (216(f))', () => {
    // Germany's own ground made the invaded country, Poland the invader holding the one way home by a lasting span: a
    // German corps ringed there is bound as China's would be, and stays where it is until the floor's month.
    const put = (g: Game, nat: string, r: Region, divs: number): Corps => { const c = addCorps(g, nat, `Test Corps ${g.nextCorps}`, 'inf', divs, r.id, {}); c.ap = c.apMax; return c; };
    const empty = (g: Game, r: Region): void => { for (const c of [...r.units]) removeCorps(g, c); };
    const fromBerlin = (g: Game, lost: Set<number>): Int16Array => {
      const d = new Int16Array(g.regions.length).fill(-1);
      const q = [g.nations.GER.capital];
      d[q[0]] = 0;
      for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) {
        if (d[j] >= 0 || lost.has(j) || fieldCtrl(g, g.regions[j]) !== 'GER') continue;
        d[j] = d[q[i]] + 1; q.push(j);
      }
      return d;
    };
    /** A German province ringed by Poland, one province of the ring joining ground Germany holds (ring-71's test). */
    const ringed = (door: 'empty' | 'held'): { g: Game; at: Region; door: Region; inside: Corps[] } => {
      const g = grand(3, null, 1);
      if (!atWar(g, 'GER', 'POL')) declareWar(g, 'GER', 'POL');
      for (const at of g.regions) {
        if (at.owner !== 'GER' || at.ctrl !== 'GER' || at.cap || !at.nb.every((j) => ['GER', 'POL'].includes(g.regions[j].ctrl) && !g.regions[j].cap)) continue;
        const lost = new Set(at.nb);
        const d = fromBerlin(g, lost);
        if (d[at.id] >= 0) continue;
        const k = at.nb.find((j) => g.regions[j].owner === 'GER' && g.regions[j].nb.some((x) => d[x] >= 0));
        if (k === undefined) continue;
        for (const j of lost) { empty(g, g.regions[j]); g.regions[j].ctrl = 'POL'; }
        g.supplyDirty = true;
        empty(g, at);
        for (const j of lost) for (const x of g.regions[j].nb) if (x !== at.id && !lost.has(x) && g.regions[x].ctrl === 'GER') empty(g, g.regions[x]);
        for (const j of at.nb) if (j !== k) for (let n = 0; n < 2; n++) put(g, 'POL', g.regions[j], 4);
        if (door === 'held') put(g, 'POL', g.regions[k], 1);
        return { g, at, door: g.regions[k], inside: [put(g, 'GER', at, 4), put(g, 'GER', at, 4)] };
      }
      throw new Error('no province fits');
    };
    const bound = (g: Game, at: Region): void => {
      if (!('lines' in kept)) kept.lines = H.lines;
      const spans = Object.fromEntries(at.nb.map((j) => [g.regions[j].name, [{ from: 1, to: 20, kind: 'lasting' }]]));
      H.lines = [{ id: 'ring215', theatre: 'ring215', land: 'GER', taker: 'POL', until: 20, floor: { until: 12, kinds: ['lasting'] }, spans }];
    };
    // Without a line the ringed corps attack the door where Poland stands weakest, and walk through it where it is empty.
    const free = ringed('held');
    ringJudgement(free.g, 'GER');
    expect(free.g.reports.filter((b) => b.attNat === 'GER' && b.region === free.door.id).length).toBe(1);
    const walk = ringed('empty');
    ringJudgement(walk.g, 'GER');
    expect(fieldCtrl(walk.g, walk.door)).toBe('GER');
    // With the ring Poland's by a lasting span, before the floor's month neither happens.
    const held = ringed('held');
    bound(held.g, held.at);
    ringJudgement(held.g, 'GER');
    expect(held.g.reports.filter((b) => b.attNat === 'GER' && held.at.nb.includes(b.region)).length).toBe(0);
    expect(held.inside.every((c) => c.reg === held.at.id)).toBe(true);
    const shut = ringed('empty');
    bound(shut.g, shut.at);
    ringJudgement(shut.g, 'GER');
    expect(fieldCtrl(shut.g, shut.door)).toBe('POL');
    expect(shut.inside.every((c) => c.reg === shut.at.id)).toBe(true);
  });

  it('is a floor for the invaded country\'s computer staff: what the invader holds by a lasting span stays his before the floor\'s month, raid ground does not', () => {
    drawn();
    const g = grand(2, null, 1);
    g.turn = 3;
    at(g, 'Poznan').ctrl = 'GER';
    at(g, 'Lodz').ctrl = 'GER';
    expect(lineShut(g, 'POL', at(g, 'Poznan'))).toBe(true);
    expect(lineShut(g, 'POL', at(g, 'Lodz'))).toBe(false);
    // The 1944 offensive's ground while the invader holds it (the rider of 17:08Z).
    g.turn = 6;
    at(g, 'Kielce').ctrl = 'GER';
    expect(lineShut(g, 'POL', at(g, 'Kielce'))).toBe(true);
    g.turn = 9;
    expect(lineShut(g, 'POL', at(g, 'Kielce'))).toBe(false);
    g.turn = 12;
    expect(lineShut(g, 'POL', at(g, 'Poznan'))).toBe(false);
    // Ground of its own it still holds is not the floor's: it may march where it likes.
    expect(lineShut(g, 'POL', at(g, 'Warsaw'))).toBe(false);
  });

  it('reads the floor\'s month province by province where the line gives one: the invaded country comes in behind a withdrawal, not ahead of it', () => {
    // (The design director, 00:12Z of 21 November 2026: the 1944 ground in China. Kielce's span runs to month 8; the
    // floor lifts for all at month 7.)
    kept.lines = H.lines;
    H.lines = [{ ...test, floor: { until: 7, kinds: test.floor.kinds } }];
    const g = grand(2, null, 1);
    at(g, 'Kielce').ctrl = 'GER';
    at(g, 'Poznan').ctrl = 'GER';
    g.turn = 6;
    expect(lineShut(g, 'POL', at(g, 'Kielce'))).toBe(true);
    // With one floor month the invaded country may come at month 7, a month before the invader's rows are due to leave.
    g.turn = 7;
    expect(lineShut(g, 'POL', at(g, 'Kielce'))).toBe(false);
    // With the province's own month past its span's end it is shut while the span runs, and the span's own end
    // decides: it is open the month after the span's last, though that month is still before the figure.
    H.lines = [{ ...test, floor: { until: 7, kinds: test.floor.kinds }, back: { Kielce: 10 } }];
    for (const t of [7, 8]) { g.turn = t; expect(lineShut(g, 'POL', at(g, 'Kielce')), `month ${t}`).toBe(true); }
    g.turn = 9;
    expect(lineShut(g, 'POL', at(g, 'Kielce'))).toBe(false);
    // The rest of the line keeps the floor's own month.
    g.turn = 7;
    expect(lineShut(g, 'POL', at(g, 'Poznan'))).toBe(false);
  });

  it('never binds the player, on either side, and ground the player holds is open to all', () => {
    drawn();
    expect(lineShut(grand(2, 'POL', 1), 'GER', at(grand(2, 'POL', 1), 'Warsaw'))).toBe(false);
    const asGer = grand(2, 'GER', 1);
    expect(lineShut(asGer, 'GER', at(asGer, 'Warsaw'))).toBe(false);
    at(asGer, 'Poznan').ctrl = 'GER';
    expect(lineShut(asGer, 'POL', at(asGer, 'Poznan'))).toBe(false);
    // A player on neither side: the computers on both are bound, and his own ground is anybody's.
    const asFra = grand(2, 'FRA', 1);
    expect(lineShut(asFra, 'GER', at(asFra, 'Warsaw'))).toBe(true);
    at(asFra, 'Warsaw').ctrl = 'FRA';
    expect(lineShut(asFra, 'GER', at(asFra, 'Warsaw'))).toBe(false);
  });

  it('is the book\'s alone: not at History\'s shape, not once the book is closed in its theatre, not after its last month', () => {
    drawn();
    expect(lineShut(grand(2, null, 3), 'GER', at(grand(2, null, 3), 'Warsaw'))).toBe(false);
    const g = grand(2, null, 1);
    g.flags['hist:off:test215'] = 5;
    expect(lineShut(g, 'GER', at(g, 'Warsaw'))).toBe(false);
    const late = grand(2, null, 1);
    late.turn = 21;
    expect(lineShut(late, 'GER', at(late, 'Warsaw'))).toBe(false);
  });

  it('moves a span by the stop\'s slack at stop 2, by the war\'s seed, keeping its length, and never the first day or the last month', () => {
    drawn();
    const moved = new Set<string>();
    for (let seed = 1; seed <= 12; seed++) {
      const g = grand(seed, null, 2);
      const L = test;
      expect(inLine(g, L, at(g, 'Poznan'), 1)).toBe(true);
      expect(inLine(g, L, at(g, 'Poznan'), 20)).toBe(true);
      const first = [1, 2, 3].find((t) => inLine(g, L, at(g, 'Lodz'), t));
      const last = [5, 4, 3, 2].find((t) => inLine(g, L, at(g, 'Lodz'), t));
      expect(first).toBeGreaterThanOrEqual(1);
      expect(first).toBeLessThanOrEqual(3);
      expect(last! - first!).toBe(1);
      moved.add(`${first}-${last}`);
    }
    expect(moved.size).toBeGreaterThan(1);
  });

  it('sends a raid home when its months are over', () => {
    drawn();
    const g = grand(2, null, 1);
    const lodz = at(g, 'Lodz');
    expect(lineLeaves(g, 'GER', lodz, 3)).toBe(false);
    expect(lineLeaves(g, 'GER', lodz, 4)).toBe(true);
    expect(lineLeaves(g, 'GER', at(g, 'Poznan'), 4)).toBe(false);
    expect(lineLeaves(g, 'POL', at(g, 'Warsaw'), 4)).toBe(false);
  });

  it('sends raid ground back when its months are over, to whoever held it before, once none of the invader\'s side stands on it (ruling 219)', () => {
    drawn();
    /** Lodz taken from Poland in the raid of months 2 and 3, and nobody on it. */
    const raided = (h: Game, t: number): Region => {
      const r = at(h, 'Lodz');
      for (const c of [...r.units]) removeCorps(h, c);
      r.ctrl = 'GER'; r.prev = 'POL'; r.since = 2; h.supplyDirty = true;
      h.turn = t;
      return r;
    };
    // Inside the raid's months it stays.
    const during = grand(2, null, 1);
    raided(during, 3);
    expect(lineGiveBack(during, 'GER')).toEqual([]);
    expect(at(during, 'Lodz').ctrl).toBe('GER');
    // After them, not while a corps of the invader's side stands there.
    const g = grand(2, null, 1);
    const lodz = raided(g, 4);
    const stays = addCorps(g, 'GER', 'Test Corps', 'inf', 2, lodz.id, {});
    expect(lineGiveBack(g, 'GER')).toEqual([]);
    removeCorps(g, stays);
    // With nobody there it goes back to Poland, and ground the line still gives the invader does not.
    at(g, 'Poznan').ctrl = 'GER';
    for (const c of [...at(g, 'Poznan').units]) removeCorps(g, c);
    expect(lineGiveBack(g, 'GER').map((r) => r.name)).toEqual(['Lodz']);
    expect(lodz.ctrl).toBe('POL');
    expect(at(g, 'Poznan').ctrl).toBe('GER');
    expect(lineGiveBack(g, 'GER')).toEqual([]);
  });

  it('never hands back a player\'s ground, or to a player, and nothing at all when the player is either country', () => {
    drawn();
    const raided = (h: Game, holder = 'GER', prev = 'POL'): Region => {
      const r = at(h, 'Lodz');
      for (const c of [...r.units]) removeCorps(h, c);
      r.ctrl = holder; r.prev = prev; h.turn = 4; h.supplyDirty = true;
      return r;
    };
    // The player's Germany, or the player's Poland: the line binds nobody, and nothing goes back.
    for (const human of ['GER', 'POL']) {
      const g = grand(2, human, 1);
      const r = raided(g);
      expect(lineGiveBack(g, 'GER')).toEqual([]);
      expect(r.ctrl).toBe('GER');
    }
    // A player on the invader's side holds the ground: it is his, and it stays his.
    const svk = grand(2, 'SVK', 1);
    const r = raided(svk, 'SVK');
    expect(lineGiveBack(svk, 'GER')).toEqual([]);
    expect(r.ctrl).toBe('SVK');
    // Whoever held it before is the player: it goes back to the invaded country's computer, never to him.
    const fra = grand(2, 'FRA', 1);
    const f = raided(fra, 'GER', 'FRA');
    expect(lineGiveBack(fra, 'GER').map((x) => x.name)).toEqual(['Lodz']);
    expect(f.ctrl).toBe('POL');
  });

  it('never strands a corps beyond the line: with no road home inside it, it is taken out by sea (the director, 18:59Z)', () => {
    drawn();
    const E = EASTERN as unknown as Record<string, unknown>;
    const keep = E.nearRefuge;
    /** A German corps left at Kielce when the offensive's months (5 to 8) are over, with Polish ground all round it. */
    const stranded = (h: Game): Corps => {
      const kielce = at(h, 'Kielce');
      for (const c of [...kielce.units]) removeCorps(h, c);
      kielce.ctrl = 'GER'; kielce.prev = 'POL'; kielce.since = 5;
      for (const j of kielce.nb) { const r = h.regions[j]; if (r.owner === 'POL') { r.ctrl = 'POL'; for (const c of r.units.filter((x) => x.nat !== 'POL')) removeCorps(h, c); } }
      at(h, 'Danzig Corridor').ctrl = 'GER';
      for (const c of [...at(h, 'Danzig Corridor').units]) removeCorps(h, c);
      h.turn = 9; h.supplyDirty = true;
      return addCorps(h, 'GER', 'Raid Corps', 'inf', 2, kielce.id, {});
    };
    try {
      // Under F: to the nearest harbour Germany holds on Poland's ground inside the line, at an evacuation's cost.
      E.nearRefuge = true;
      const g = grand(2, null, 1);
      const c = stranded(g);
      expect(at(g, 'Kielce').nb.some((j) => allied(g, 'GER', g.regions[j].ctrl))).toBe(false);
      lineHome(g, 'GER', 9);
      expect(g.regions[c.reg].name).toBe('Danzig Corridor');
      expect(c.str).toBeCloseTo(0.75, 9);
      expect(g.headlines.some((h) => /taken out by sea to the Danzig Corridor/.test(h.text))).toBe(true);
      // The raid ground, empty now, goes back at the next turn of the month (219).
      expect(lineGiveBack(g, 'GER').map((r) => r.name)).toEqual(['Kielce']);
      // A harbour beyond the line is no refuge: with none inside it, the corps is taken home.
      const far = grand(2, null, 1);
      const d = stranded(far);
      test.spans['Danzig Corridor'] = [{ from: 1, to: 8, kind: 'day one' }];
      lineHome(far, 'GER', 9);
      expect(far.regions[d.reg].id).toBe(far.nations.GER.capital);
      test.spans['Danzig Corridor'] = [{ from: 1, to: 20, kind: 'day one' }];
      // Without F it goes home as any lifted-off corps does.
      E.nearRefuge = undefined;
      const off = grand(2, null, 1);
      const e = stranded(off);
      lineHome(off, 'GER', 9);
      expect(off.regions[e.reg].id).toBe(off.nations.GER.capital);
      // Computer against computer only: with the player on either side the line binds nobody, and the corps stays.
      E.nearRefuge = true;
      for (const human of ['GER', 'POL']) {
        const p = grand(2, human, 1);
        const x = stranded(p);
        lineHome(p, 'GER', 9);
        expect(p.regions[x.reg].name, human).toBe('Kielce');
      }
    } finally {
      test.spans['Danzig Corridor'] = [{ from: 1, to: 20, kind: 'day one' }];
      if (keep === undefined) delete E.nearRefuge; else E.nearRefuge = keep;
    }
  });

  it('ships nobody into a harbour beyond the line, though his side holds it, as it lands nobody there (224, b390)', () => {
    drawn();
    /** A fresh German corps in a German harbour, and the Danzig Corridor's harbour in German hands with nobody in it. */
    const ferry = (h: Game): { c: Corps; to: Region } => {
      const to = at(h, 'Danzig Corridor');
      for (const c of [...to.units]) removeCorps(h, c);
      to.ctrl = 'GER'; to.prev = 'POL'; to.since = 1;
      const quay = h.regions.find((r) => r.owner === 'GER' && r.ctrl === 'GER' && r.port && r.id !== to.id)!;
      return { c: addCorps(h, 'GER', 'Ferry Corps', 'inf', 1, quay.id, {}), to };
    };
    const shut = /record's line/;
    try {
      const g = grand(2, null, 1);
      const { c, to } = ferry(g);
      expect(String(whyNotShip(g, c, to))).not.toMatch(shut);
      // Its span over, the harbour is beyond the line: the ferry is barred as a landing would be. (A game reads its
      // spans once, so the shorter span is tried on a fresh one.)
      test.spans['Danzig Corridor'] = [{ from: 1, to: 8, kind: 'day one' }];
      const late = grand(2, null, 1);
      const y = ferry(late);
      late.turn = 9;
      expect(lineShut(late, 'GER', y.to)).toBe(true);
      expect(whyNotShip(late, y.c, y.to)).toMatch(shut);
      expect(whyNotLand(late, [y.c], y.to)).toMatch(shut);
      // Never the player's ships: with Germany played the line binds nobody.
      const p = grand(2, 'GER', 1);
      const x = ferry(p);
      p.turn = 9;
      expect(String(whyNotShip(p, x.c, x.to))).not.toMatch(shut);
    } finally {
      test.spans['Danzig Corridor'] = [{ from: 1, to: 20, kind: 'day one' }];
    }
  });

  it('holds in a war nobody plays: the invader\'s army takes nothing beyond the line and leaves a raid when its months are over', () => {
    drawn();
    const g = grand(2, null, 1);
    for (let m = 0; m < 4 && g.nations.POL.alive; m++) {
      runTurnAI(g);
      const t = g.turn - 1;
      const off = g.regions.filter((r) => r.owner === 'POL' && r.ctrl === 'GER' && !inLine(g, test, r, t)).map((r) => r.name);
      // (A raid's ground goes back the month after its last, once his corps have left it: ruling 219.)
      expect(off, `month ${t}`).toEqual([]);
      expect(g.corps.some((c) => !c.dead && c.nat === 'GER' && g.regions[c.reg].owner === 'POL' && !inLine(g, test, g.regions[c.reg], t + 1) && g.regions[c.reg].name !== 'Lodz'), `month ${t}`).toBe(false);
      if (t >= 3) expect(g.corps.filter((c) => !c.dead && c.nat === 'GER' && c.kind !== 'fort' && g.regions[c.reg].name === 'Lodz').map((c) => c.name), `month ${t}`).toEqual([]);
    }
    // Without the line Poland is gone in two months. With it Germany has taken its strip and Warsaw stands.
    expect(g.nations.POL.alive).toBe(true);
    expect(at(g, 'Warsaw').ctrl).toBe('POL');
    expect(at(g, 'Poznan').ctrl).toBe('GER');
  });
});

describe('the data ruling 215 makes, for step 13 (data/campaign/china-line.json)', () => {
  const D = china as unknown as { history: { dates: Record<string, Record<string, number>>; theatre: Record<string, string>; theatres: Record<string, unknown>; bookIf: Record<string, string[]>; marks: { theatre: string; until?: number; if: string[] }[]; lines: Line[] }; open: { ops: Record<string, Op[]>; temper: Record<string, unknown> } };
  const L = D.history.lines[0];

  it('dates Japan\'s offensive of 1944 to April 1944, in its own theatre, with the key that makes its front wait', () => {
    expect(D.history.dates['JAP+'].ichigo).toBe(56);
    expect(D.history.theatre['JAP:ichigo']).toBe('china');
    expect(D.history.theatres.china).toBeTruthy();
    const op = D.open.ops['JAP+'].find((o) => o.id === 'ichigo')!;
    expect(op).toMatchObject({ wait: true, from: 56, to: 66, push: 8 });
    expect(op.declare).toBeUndefined();
    expect(op.side).toBeUndefined();
    expect(op.if).toContain('war:JAP,CHN');
    expect(D.history.bookIf['JAP:ichigo']).toEqual(op.if);
    expect(D.history.marks).toEqual([{ theatre: 'china', until: 72, if: ['alive:CHN'], why: 'China has fallen' }]);
    expect(D.open.temper.JAP).toEqual({ attackAt: 1.2, persist: 0.9, minOdds: 1.3 });
  });

  it('draws the record\'s line on China\'s ground: 39 provinces on the first day (the finer chart), the floor to month 69 over the lasting ground and the 1944 offensive\'s', () => {
    expect([L.id, L.theatre, L.land, L.taker, L.until]).toEqual(['china', 'china', 'CHN', 'JAP', 72]);
    expect(L.floor).toEqual({ until: 69, kinds: ['day one', 'lasting', 'ichigo'] });
    const all = Object.entries(L.spans);
    expect(all.filter(([, s]) => s.some((x) => x.kind === 'day one' && x.from === 1)).length).toBe(39);
    for (const [nm, spans] of all) for (const s of spans) {
      expect(['day one', 'lasting', 'raid', 'ichigo'], nm).toContain(s.kind);
      expect(s.from, nm).toBeLessThanOrEqual(s.to);
      expect(s.to, nm).toBeLessThanOrEqual(L.until);
    }
    // Never Japan's, by the record.
    for (const nm of ['Chungking', 'Chengtu', 'Kweiyang', 'Kunming', 'Sian', 'Lanchow', 'Yenan', 'Tsunyi', 'Enshih', 'Pengshui']) expect(L.spans[nm], nm).toBeUndefined();
    // The two lasting gains and the furthest point of 1944.
    expect(L.spans.Ichang).toEqual([{ from: 10, to: 72, kind: 'lasting' }]);
    expect(L.spans.Ningpo).toEqual([{ from: 20, to: 72, kind: 'lasting' }]);
    expect(L.spans.Tushan).toEqual([{ from: 64, to: 64, kind: 'ichigo' }]);
    // Ruling 219: the ceiling holds to the end of the record's spans in August 1945, so the 1944 ground whose span
    // ends in 1945 goes back as in the record; only the floor lifts at month 69.
    expect(L.until).toBe(Math.max(...all.flatMap(([, s]) => s.map((x) => x.to))));
    expect(all.some(([, s]) => s.some((x) => x.kind === 'ichigo' && x.to > L.floor.until && x.to < L.until))).toBe(true);
  });

  it('gives every province of the floor\'s kinds its own floor month, past its span\'s end: China comes in behind the withdrawal of 1945 and not at all where Japan stood to the surrender', () => {
    // (The design director, 00:12Z and 00:16Z of 21 November 2026, with the historian's ichigo-withdrawal-1945.md. The
    // floor binds only while the span runs, so the span's own end decides; two months past its last covers every draw
    // of a stop with slack, where a span's months move a month either way. At 00:42Z the director gave the day-one and
    // lasting ground the same rule, on the trace of China's gains of 1945: with one floor month, 69, China's staff walked
    // into ground whose spans run to the line's last month.)
    const want: Record<string, number> = {};
    for (const [nm, spans] of Object.entries(L.spans)) for (const s of spans) if (L.floor.kinds.includes(s.kind)) want[nm] = s.to + 2;
    expect(L.back).toEqual(want);
    // The corridor goes on its months, and what was held to the surrender is shut to the line's end.
    expect([L.back!.Nanning, L.back!.Liuchow, L.back!.Wuchow, L.back!.Kweilin]).toEqual([71, 72, 72, 73]);
    for (const nm of ['Loyang', 'Changsha', 'Hengyang', 'Lingling', 'Kukong', 'Kanchow']) expect(L.back![nm], nm).toBeGreaterThan(L.until);
    // The day-one and lasting ground was held to the surrender too: shut to the line's end. A raid has no floor.
    for (const nm of ['Peiping', 'Nanking', 'Hankow', 'Canton', 'Yochow', 'Ichang', 'Ningpo']) expect(L.back![nm], nm).toBe(L.until + 2);
    for (const nm of ['Pakhoi', 'Chuchow', 'Changteh']) expect(L.back![nm], nm).toBeUndefined();
    // (Sixty, and Waichow: Japan's from January 1945 to the end, the design director's ruling of 08:12Z of 22 November
    // 2026 on the historian's waichow-1938-45.md. tests/far-china-attacks.test.ts.)
    expect(Object.keys(L.back!).length).toBe(61);
    expect(L.back!.Waichow).toBe(L.until + 2);
  });

  it('dates the record\'s raids of 1939 to 1941 as great choices of the book alone, Nanning landing from a home port (219(3), b375)', () => {
    const raids = ['nanning', 'wuyuan', 'ichang', 'coast41', 'yencheng', 'changsha41'];
    for (const id of raids) {
      const op = D.open.ops['JAP+'].find((o) => o.id === id)!;
      expect(op, id).toBeTruthy();
      // A great choice in a war already on: no side-show, nobody to declare on, so it opens the China front (216(a)).
      // (But the 1941 coast raid, a side-show since the design director's 13:39Z of 21 November 2026: a blockade
      // landing beside the main front, as the 1944 coast's is. tests/china-rows-1121.test.ts.)
      expect(op.side, id).toBe(id === 'coast41' ? true : undefined);
      expect(op.declare, id).toBeUndefined();
      expect(D.history.theatre[`JAP:${id}`], id).toBe('china');
      // Off the book its own conditions can never hold; the book reads its own.
      expect(op.if, id).toEqual(['war:JAP,CHN', 'peace:JAP,CHN']);
      expect(D.history.bookIf[`JAP:${id}`], id).toEqual(['war:JAP,CHN', 'alive:CHN']);
      const date = D.history.dates['JAP+'][id];
      expect(op.from, id).toBe(date);
      expect(op.to!, id).toBeGreaterThan(date);
    }
    // No two on one date: one great choice a month at most.
    expect(new Set(raids.map((id) => D.history.dates['JAP+'][id])).size).toBe(raids.length);
    // Nanning: ashore at Pakhoi, the historian's raid span, from Taihoku, where the 5th Division is brought the month before.
    expect(L.spans.Pakhoi).toEqual([{ from: 3, to: 15, kind: 'raid' }]);
    const nanning = D.open.ops['JAP+'].find((o) => o.id === 'nanning')!;
    expect(nanning.landings).toEqual([{ corps: '21st Army (5th Division)', with: ['Formosa Army'], to: ['Pakhoi'] }]);
    const row = (setupRows as unknown as { 'setup-1939': { expeditions: [number, string, string, string, number, string, Record<string, unknown>][] } })['setup-1939'].expeditions.find((r) => r[2] === '21st Army (5th Division)')!;
    expect(row.slice(0, 6)).toEqual([2, 'JAP', '21st Army (5th Division)', 'inf', 1, 'Taihoku']);
    expect(row[0]).toBeLessThan(D.history.dates['JAP+'].nanning);
  });

  it('lands the 1941 coast raid from Japan\'s own ports, each force alone, in the month every span opens (224, b375)', () => {
    const coast = D.open.ops['JAP+'].find((o) => o.id === 'coast41')!;
    expect(D.history.dates['JAP+'].coast41).toBe(20);
    for (const nm of ['Foochow', 'Ningpo', 'Wenchow']) expect(L.spans[nm][0].from, nm).toBe(20);
    expect(coast.landings).toEqual([
      { corps: '23rd Army (48th Division)', to: ['Foochow', 'Wenchow'] },
      { corps: '13th Army (Chekiang landing force)', to: ['Ningpo'] },
      { corps: '13th Army (Wenchow detachment)', to: ['Wenchow', 'Ningpo'] },
    ]);
    // Each force comes to a home port the month before, by a row of its own beside the 5th Division's.
    // (They head the file; the record's rows of 21 November 2026 follow them: tests/china-rows-1121.test.ts.)
    const rows = (setupRows as unknown as { 'setup-1939': { expeditions: [number, string, string, string, number, string, { str: number; from?: string[]; war: string[]; until?: number }][] } })['setup-1939'].expeditions.slice(0, 4);
    expect(rows.map((r) => r[2])).toEqual(['21st Army (5th Division)', ...coast.landings!.map((l) => l.corps)]);
    for (const l of coast.landings!) {
      const r = rows.find((x) => x[2] === l.corps)!;
      expect(r[0], l.corps).toBe(D.history.dates['JAP+'].coast41 - 1);
      expect(r[1], l.corps).toBe('JAP');
      expect(['Formosa', 'Taihoku'], l.corps).toContain(r[5]);
      expect(r[6].war, l.corps).toEqual(['JAP', 'CHN']);
    }
    expect(rows.map((r) => r[6].str)).toEqual([0.9, 0.9, 0.9, 0.3]);
    // None adds to Japan's lasting strength in China (b387): two are taken from rows already standing, and the
    // detachment is lent for Wenchow's span (months 20 to 21) and struck off the month after.
    expect(rows[1][6].from).toEqual(['Formosa Army']);
    expect(rows[2][6].from).toEqual(['13th Army (116th Division, Anking)']);
    expect(rows[3][6].from).toBeUndefined();
    expect(rows[3][6].until).toBe(L.spans.Wenchow[0].to + 1);
    expect(rows.slice(0, 3).every((r) => r[6].until === undefined)).toBe(true);
  });

  it('names only China\'s provinces, where the chart has China', () => {
    const g = grand(2, null, 3);
    const chinese = g.regions.filter((r) => r.owner === 'CHN');
    if (!chinese.length) return; // main's chart until step 13
    for (const nm of Object.keys(L.spans)) expect(g.byName[nm]?.owner, nm).toBe('CHN');
  });
});

describe('218: China\'s last levy in words that give no wrong count of men', () => {
  // China's levy as the historian sized it, on the row the game counts at 15,000 men to a division: 6.5 divisions
  // would read 98,000 men where the record's are about 470,000. The row asks for the divisions alone.
  const china: Levy = { under: 32, spent: 0.5, men: 6.5, gap: 12, most: 3, words: 'divisions' };

  it('gives the card the UI director\'s sentence, in the Soviet form, naming the divisions and no men', () => {
    expect(levyRule(china)).toBe('With the enemy on its soil, its reserve of men all but spent and its army in the field under 32 divisions, it calls up every man fit to bear arms: men enough for 6.5 divisions, three times in a war at most and never twice within a year.');
    expect(levyRule(china)).not.toMatch(/\d,\d{3}|million/);
    // The Soviet card is as it was.
    const sov = TRAITS.nations.SOV.traits.find((t) => t.levy)!;
    expect(traitRule(sov)).toContain('it calls up every man fit to bear arms: 2.3 million more men, three times in a war at most and never twice within a year.');
  });

  it('gives the news the divisions and no men, and leaves the Soviet news as it was', () => {
    const row = TRAITS.nations.SOV.traits.find((t) => t.levy)!;
    const kept = row.levy!;
    const pressed = (): Game => {
      const g = startOpen(createGame(2, null));
      for (const [a, b] of [['GER', 'SOV'], ['SOV', 'GER']]) if (!g.nations[a].atWar.includes(b)) g.nations[a].atWar.push(b);
      g.regions.find((r) => r.owner === 'SOV' && r.ctrl === 'SOV' && !r.overseas && !r.cap)!.ctrl = 'GER';
      g.flags['mp:SOV'] = 0;
      return g;
    };
    try {
      row.levy = { ...kept, men: 6.5, words: 'divisions' };
      const g = pressed();
      expect(traitLevy(g, 'SOV')).toBe(6.5);
      expect(g.headlines[g.headlines.length - 1].text).toBe('The Soviet Union calls up every man fit to bear arms: men enough for 6.5 divisions.');
    } finally { row.levy = kept; }
    const g = pressed();
    traitLevy(g, 'SOV');
    expect(g.headlines[g.headlines.length - 1].text).toBe('The Soviet Union calls up every man fit to bear arms: 2.3 million more men for its armies.');
  });
});
