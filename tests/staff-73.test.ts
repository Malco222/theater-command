// The last round of staff judgement (Version 73), the grand campaign only. Two things. The reckoning of a crossing
// now says what comes after it: whether the sea behind the beach could be kept open to the supply ships, counting
// whole every fleet the enemy could bring into it, so that no landing is cut off without a warning. And a staff
// named in rivals.json under `relief` rests its worn corps: one standing in the line comes out of it where another
// holds the place and a quiet place lies behind, it is thrown into no attack, and it is left alone until the drafts
// have filled it again. The player's own army is never moved, and history's scenarios use none of it.
import { describe, expect, it } from 'vitest';
import { reliefJudgement, runAI } from '../src/campaign/ai';
import { addCorps, atWar, createGame, declareWar, hostileUnits, removeCorps } from '../src/campaign/game';
import { NAVY, closedTo, crossingsOf, fleetPower, fleetsInReach, planLanding, sail, seaHold, seaSide, surfaceShips, type CrossingOutlook } from '../src/campaign/navy';
import { RIVALS, startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

const RELIEF = RIVALS.relief!;

describe('holding the sea behind the beach', () => {
  /** A war the computer plays on every side, as it stands once Poland has fallen, with every enemy fleet of Germany's in harbour. */
  const opened = (): Game => {
    const g = startOpen(createGame(3, null));
    for (let k = 0; k < 8 && (k < 2 || g.nations.POL.alive); k++) runTurnAI(g);
    for (const f of g.fleets) if (atWar(g, f.nat, 'GER')) sail(g, f, f.home, 'port');
    return g;
  };
  const toBritain = (g: Game): CrossingOutlook => crossingsOf(g, 'GER').find((x) => x.owner === 'UK' && x.home)!;
  /** Every enemy fleet in reach of the crossing puts to sea in its last sea. */
  const allCome = (g: Game, x: CrossingOutlook): void => {
    const sea = x.path[x.path.length - 1];
    for (const f of fleetsInReach(g, 'GER', x.path)) { f.zone = sea; f.stance = 'patrol'; }
  };

  it('counts whole what the enemy could bring into the sea off the beach', () => {
    const g = opened();
    const x = toBritain(g);
    const h = x.account.hold;
    const sea = x.path[x.path.length - 1];
    expect(h.sea).toBe(sea);
    const fleets = fleetsInReach(g, 'GER', x.path);
    expect(fleets.length).toBeGreaterThan(0);
    expect(fleets.every((f) => atWar(g, f.nat, 'GER') && surfaceShips(f) > 0)).toBe(true);
    expect(h.come).toBeCloseTo(fleets.reduce((s, f) => s + fleetPower(g, f), 0), 9);
    const e = seaSide(g, sea, 'GER', true), own = seaSide(g, sea, 'GER', false);
    expect(h.there).toBeCloseTo(e.ships, 9);
    expect(h.air).toBeCloseTo(e.air, 9);
    expect(h.foe).toBeCloseTo(e.ships + e.air + h.come, 9);
    expect(h.own.total).toBeCloseTo(own.total, 9);
    expect(h.port).toBe(!!x.to.port);
    // The same forecast rides on the landing plan, where the card reads it.
    expect(seaHold(g, 'GER', x.path, x.to)).toEqual(h);
  });

  it('says the sea can be closed exactly when it can be, with all of them in it', () => {
    const g = opened();
    const x = toBritain(g);
    const h = x.account.hold;
    // The Royal Navy within reach is far more than twice what Germany has in that sea.
    expect(h.foe).toBeGreaterThanOrEqual(NAVY.closed.min);
    expect(h.open).toBe(false);
    expect(h.need).toBeCloseTo(h.foe / NAVY.closed.ratio, 9);
    expect(h.own.total).toBeLessThan(h.need);
    expect(h.lines.some((l) => l.label === 'The sea behind the beach' && l.value === 'can be closed')).toBe(true);
    expect(h.lines.some((l) => l.label === 'Enemy ships that could come' && l.value === Math.round(h.come).toLocaleString('en-US'))).toBe(true);
    allCome(g, x);
    expect(closedTo(g, h.sea, 'GER')).toBe(true);
    expect(seaSide(g, h.sea, 'GER', true).total).toBeCloseTo(h.foe, 6);
  });

  it('keeps it open once the invader has more there than half of what could come (and since Version 76 calls it contested)', () => {
    const g = opened();
    const x = toBritain(g);
    const before = x.account.hold;
    // A German fleet in that sea strong enough: one for every two of his.
    const f = g.fleets.find((k) => k.nat === 'GER' && surfaceShips(k) > 0)!;
    f.base = -1; f.home = before.sea; f.zone = before.sea; f.stance = 'patrol';
    for (let n = 0; n < 400 && seaSide(g, before.sea, 'GER', false).total <= before.need * 1.05; n++) f.ships.bb += 5;
    const h = seaHold(g, 'GER', x.path, x.to);
    expect(h.own.total).toBeGreaterThan(h.need);
    expect(h.open).toBe(true);
    // More than half and less than all of it: open today, and contested (tests/latin-76.test.ts).
    expect(h.contested).toBe(true);
    expect(h.lines.some((l) => l.label === 'The sea behind the beach' && l.value === 'contested')).toBe(true);
    allCome(g, x);
    expect(closedTo(g, h.sea, 'GER')).toBe(false);
  });

  it('says so where the beach has no harbour', () => {
    const g = opened();
    const x = toBritain(g);
    const bare = { ...x.to, port: false } as Region;
    const h = seaHold(g, 'GER', x.path, bare);
    expect(h.port).toBe(false);
    expect(h.lines[h.lines.length - 1].label).toBe('A harbour');
    expect(seaHold(g, 'GER', x.path, { ...x.to, port: true } as Region).lines.some((l) => l.label === 'A harbour')).toBe(false);
  });

  it('rides on the landing plan the card reads', () => {
    const g = opened();
    const x = toBritain(g);
    g.nations.GER.sealift = Math.max(g.nations.GER.sealift, 4);
    for (const c of [...x.from.units]) removeCorps(g, c);
    const c = addCorps(g, 'GER', 'Test Corps', 'inf', 3, x.from.id, {});
    c.ap = c.apMax;
    const plan = planLanding(g, [c], x.to);
    if (plan.why === null) {
      expect(plan.hold).toBeTruthy();
      expect(plan.hold!.sea).toBe(plan.path[plan.path.length - 1]);
      if (plan.account) expect(plan.hold).toEqual(plan.account.hold);
    } else {
      // The way is barred as things stand (a closed sea, or the sky): the list of crossings still carries the forecast.
      expect(x.account.hold.lines.length).toBeGreaterThan(0);
    }
  });
});

describe('a staff that rests its worn corps', () => {
  /** The grand campaign with Germany and Poland at war. */
  const war = (human: string | null = null): Game => {
    const g = startOpen(createGame(3, human));
    if (!atWar(g, 'GER', 'POL')) declareWar(g, 'GER', 'POL');
    return g;
  };
  const empty = (g: Game, r: Region): void => { for (const c of [...r.units]) removeCorps(g, c); };
  const put = (g: Game, nat: string, r: Region, str = 1): Corps => { const c = addCorps(g, nat, `Test Corps ${g.nextCorps}`, 'inf', 3, r.id, {}); c.ap = c.apMax; c.str = str; return c; };
  const foeNear = (g: Game, r: Region): boolean => r.nb.some((j) => hostileUnits(g, g.regions[j], 'GER').length > 0);
  /**
   * A German province with a Polish corps next door, and behind it a German province with no enemy beside it.
   * Both are emptied. Returns the place in the line, the quiet place, and the enemy's.
   */
  const scene = (g: Game): { at: Region; back: Region; foe: Region } => {
    for (const at of g.regions) {
      if (at.owner !== 'GER' || at.ctrl !== 'GER' || at.cap) continue;
      const foe = at.nb.map((j) => g.regions[j]).find((r) => r.ctrl === 'POL');
      if (!foe) continue;
      const back = at.nb.map((j) => g.regions[j]).find((r) => r.owner === 'GER' && r.ctrl === 'GER' && !r.cap && !hostileUnits(g, r, 'GER').length && !foeNear(g, r));
      if (!back) continue;
      empty(g, at); empty(g, back);
      if (!hostileUnits(g, foe, 'GER').length) put(g, 'POL', foe);
      if (foeNear(g, back)) continue;
      g.supplyDirty = true;
      return { at, back, foe };
    }
    throw new Error('no province fits');
  };
  const ids = (g: Game, nat = 'GER'): number[] => reliefJudgement(g, nat).map((x) => x.id);

  it('takes a worn corps out of the line where another holds the place', () => {
    const g = war();
    const { at, back } = scene(g);
    const fresh = put(g, 'GER', at), worn = put(g, 'GER', at, RELIEF.worn - 0.1);
    const out = reliefJudgement(g, 'GER');
    const mine = out.find((x) => x.id === worn.id)!;
    expect(mine).toBeTruthy();
    // It is out of the line, on ground no enemy stands beside, and no further from home than it need be.
    expect(worn.reg).not.toBe(at.id);
    expect(foeNear(g, g.regions[worn.reg])).toBe(false);
    expect(mine.reg).toBe(worn.reg);
    expect(at.nb.includes(worn.reg) || g.regions[worn.reg].nb.some((j) => at.nb.includes(j))).toBe(true);
    expect(back.id >= 0).toBe(true);
    // The fresh corps holds the place and is nobody's to rest.
    expect(fresh.reg).toBe(at.id);
    expect(out.some((x) => x.id === fresh.id)).toBe(false);
    // Its rest is dated from this month, and the hold that kept it out of the month's moves is taken off again.
    expect(g.flags[`rl:${worn.id}`]).toBe(g.turn);
    expect(worn.hold).toBe(false);
  });

  it('leaves a worn corps that stands alone where it is, and throws it into no attack', () => {
    const g = war();
    const { at } = scene(g);
    const worn = put(g, 'GER', at, RELIEF.worn - 0.1);
    const out = reliefJudgement(g, 'GER');
    expect(worn.reg).toBe(at.id);
    // Kept out of the month's operations all the same, and no rest is dated: it has had none.
    expect(out.some((x) => x.id === worn.id && x.reg === at.id)).toBe(true);
    expect(g.flags[`rl:${worn.id}`]).toBeUndefined();
    expect(worn.hold).toBe(false);
  });

  it('sends the weaker of two worn corps and keeps the stronger in the place', () => {
    const g = war();
    const { at } = scene(g);
    const a = put(g, 'GER', at, RELIEF.worn - 0.2), b = put(g, 'GER', at, RELIEF.worn - 0.05);
    const out = reliefJudgement(g, 'GER');
    expect(a.reg).not.toBe(at.id);
    expect(b.reg).toBe(at.id);
    expect(out.map((x) => x.id).sort()).toEqual(expect.arrayContaining([a.id, b.id]));
  });

  it('leaves a corps in the line that is fit enough to fight', () => {
    const g = war();
    const { at } = scene(g);
    put(g, 'GER', at);
    const c = put(g, 'GER', at, RELIEF.worn + 0.05);
    expect(ids(g)).not.toContain(c.id);
    expect(c.reg).toBe(at.id);
  });

  it('rests a worn corps out of the line until the drafts have filled it, and no longer than its months', () => {
    const g = war();
    const { back } = scene(g);
    const c = put(g, 'GER', back, RELIEF.worn - 0.1);
    expect(ids(g)).toContain(c.id);
    expect(c.reg).toBe(back.id);
    expect(g.flags[`rl:${c.id}`]).toBe(g.turn);
    // Still short of fit a month on: it rests on, though it is over the mark that sent it out.
    g.turn += 1;
    c.str = (RELIEF.worn + RELIEF.fit) / 2;
    expect(ids(g)).toContain(c.id);
    // Filled: back at the staff's disposal, and the rest is forgotten.
    c.str = RELIEF.fit;
    expect(ids(g)).not.toContain(c.id);
    expect(g.flags[`rl:${c.id}`]).toBeUndefined();
    // A corps the drafts never reach is not kept out for ever: after its months it is the staff's again, worn or not,
    // and it is not rested a second time until as many months more have passed.
    c.str = RELIEF.worn - 0.1;
    expect(ids(g)).toContain(c.id);
    const since = g.flags[`rl:${c.id}`];
    g.turn = since + RELIEF.months;
    expect(ids(g)).not.toContain(c.id);
    expect(g.flags[`rl:${c.id}`]).toBe(since);
    g.turn = since + 2 * RELIEF.months;
    expect(ids(g)).toContain(c.id);
    expect(g.flags[`rl:${c.id}`]).toBe(g.turn);
  });

  it('leaves alone a corps that is under strength and was never worn', () => {
    const g = war();
    const { back } = scene(g);
    const c = put(g, 'GER', back, (RELIEF.worn + RELIEF.fit) / 2);
    expect(ids(g)).not.toContain(c.id);
    expect(g.flags[`rl:${c.id}`]).toBeUndefined();
  });

  it('is the judgement of the staffs named, never of a player', () => {
    expect(RELIEF.staffs).toEqual(['GER']);
    // A player's Germany: nothing of his is moved or held.
    const g = war('GER');
    const { at } = scene(g);
    put(g, 'GER', at);
    const worn = put(g, 'GER', at, RELIEF.worn - 0.1);
    expect(reliefJudgement(g, 'GER')).toEqual([]);
    expect(worn.reg).toBe(at.id);
    // A staff that is not named keeps its worn corps in the line as before.
    const h = war();
    const s = scene(h);
    put(h, 'POL', s.foe);
    const theirs = put(h, 'POL', s.foe, RELIEF.worn - 0.1);
    expect(reliefJudgement(h, 'POL')).toEqual([]);
    expect(theirs.reg).toBe(s.foe.id);
  });

  it('is no part of history\'s scenarios', () => {
    const g = createGame(3, null);
    const worn = g.corps.find((c) => c.nat === 'GER' && c.kind === 'inf')!;
    worn.str = RELIEF.worn - 0.1;
    expect(reliefJudgement(g, 'GER')).toEqual([]);
  });

  it('holds through the staff\'s whole month: the corps it rested is where the relief left it', () => {
    const g = war();
    const { at } = scene(g);
    put(g, 'GER', at);
    const worn = put(g, 'GER', at, RELIEF.worn - 0.1);
    runAI(g, 'GER');
    expect(worn.dead).toBeFalsy();
    expect(worn.reg).not.toBe(at.id);
    expect(worn.fought).toBeFalsy();
    expect(worn.hold).toBe(false);
    expect(g.flags[`rl:${worn.id}`]).toBe(g.turn);
  });
});
