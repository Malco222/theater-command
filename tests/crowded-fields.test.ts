// The realism version, its first piece: crowded airfields (the design director's b437 and b448).
// 1. A computer staff leaves no field of its own crowded at its month's end: groups go to the nearest fields with room
//    that still reach their stretch of front, and the field they leave stays full.
// 2. A group too big for any field with room is split, and the command is kept at what it was.
// 3. The player's groups are never moved, unless he has handed the air war to his staff; history's campaigns, with
//    no air war, are untouched.
// 4. The player is told (b448, the UI director's words): one tray row that names the worst field, and one line on the
//    month card in the month a field first becomes crowded, not again while it stays crowded.
import { describe, expect, it } from 'vitest';
import { AIR, airRange, airWar, crowdedFields, crowdedNews, crowdedRow, crowding, fieldServes, km, shareWords, spreadAir } from '../src/campaign/air';
import { setDesk } from '../src/campaign/depth';
import { createGame } from '../src/campaign/game';
import { monthCard, monthMark } from '../src/campaign/monthcard';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { AirFleet, Game } from '../src/campaign/types';

const land = (g: Game, nat: string): AirFleet[] => g.airFleets.filter((f) => f.nat === nat && f.ship === undefined && f.ashore === undefined && f.base >= 0);
const based = (g: Game, id: number): number => g.airFleets.reduce((s, f) => s + (f.ship === undefined && f.base === id ? f.ftr + f.bmr : 0), 0);
const kept = (g: Game, id: number): number => g.airFleets.reduce((s, f) => s + (f.ship === undefined && f.base === id ? Math.max(f.ftr + f.bmr, f.est.ftr + f.est.bmr) : 0), 0);
/** A grand campaign's first month, with every one of a nation's groups but those already there piled onto one of its fields. */
function piled(nat: string, seed = 3, human: string | null = null): { g: Game; F: number } {
  const g = startOpen(createGame(seed, human));
  const mine = land(g, nat);
  const F = mine[0].base;
  // (Groups within a month's flying of it, as a staff might have gathered them.)
  for (const f of mine) if (km(g, f.base, F) <= AIR.ferry) f.base = F;
  expect(kept(g, F)).toBeGreaterThan(fieldServes(g, F));
  return { g, F };
}

describe('a computer staff at its month\'s end (b437)', () => {
  it('leaves no field of its own crowded, and the field it leaves full', () => {
    for (const nat of ['SOV', 'GER']) {
      const { g, F } = piled(nat);
      const before = land(g, nat).reduce((s, f) => s + f.ftr + f.bmr, 0);
      const moved = spreadAir(g, nat);
      expect(moved.length, nat).toBeGreaterThan(0);
      for (const id of new Set(land(g, nat).map((f) => f.base))) {
        expect(kept(g, id), `${nat} ${g.regions[id].name}`).toBeLessThanOrEqual(fieldServes(g, id));
        expect(based(g, id)).toBeLessThanOrEqual(fieldServes(g, id));
      }
      // Every group that is left can fly whole.
      for (const f of land(g, nat)) expect(crowding(g, f)).toBe(1);
      // The field they left is still as full as one more group would make it too full.
      const smallest = Math.min(...moved.map((f) => Math.max(f.ftr + f.bmr, f.est.ftr + f.est.bmr)));
      expect(kept(g, F)).toBeGreaterThan(fieldServes(g, F) - smallest);
      // No aircraft are lost or made on the way, and every group moved went a month's flying at most, with its move's cost.
      expect(land(g, nat).reduce((s, f) => s + f.ftr + f.bmr, 0)).toBe(before);
      for (const f of moved) { expect(f.moved).toBe(g.turn); expect(km(g, F, f.base)).toBeLessThanOrEqual(AIR.ferry); }
    }
  });

  it('keeps a group that covers the front within reach of it where a field with room does', () => {
    // Germany in September 1939, over Poland.
    const { g, F } = piled('GER');
    const front = g.regions.filter((r) => r.ctrl === 'POL' && r.units.some((u) => !u.dead && u.nat === 'POL')).map((r) => r.id);
    const covers = (f: AirFleet, at: number): boolean => front.some((t) => km(g, at, t) <= (f.kind === 'bmr' ? airRange(f).bmr : airRange(f).ftr));
    const was = new Map(land(g, 'GER').map((f) => [f.id, covers(f, F)]));
    const moved = spreadAir(g, 'GER');
    const coverers = moved.filter((f) => was.get(f.id));
    expect(coverers.length).toBeGreaterThan(0);
    // (A group that covered the front from the crowded field covers it still, unless every field that would was full.)
    const still = coverers.filter((f) => covers(f, f.base)).length;
    expect(still).toBeGreaterThan(0);
  });

  it('splits a group too big for any field with room, and keeps the command at what it was', () => {
    const { g, F } = piled('SOV');
    // Every field near enough has room for a part of a group, and none for the largest whole.
    const big = land(g, 'SOV').filter((f) => f.base === F).sort((a, b) => b.ftr + b.bmr - (a.ftr + a.bmr))[0];
    for (const f of land(g, 'SOV')) if (f !== big) f.base = F;
    // (Fill the other Soviet fields within reach to leave each 100 aircraft of room: a group of the size of `big` fits none.)
    const others = g.regions.filter((r) => r.id !== F && fieldServes(g, r.id) > 0 && km(g, F, r.id) <= AIR.ferry);
    const stub = (id: number, n: number): AirFleet => ({ id: g.nextAir++, nat: 'POL', name: `Stub ${id}`, base: id, ftr: n, bmr: 0, est: { ftr: n, bmr: 0 }, mission: 'support', target: -1, ready: 1, leader: null, moved: 0, lost: { ftr: 0, bmr: 0 }, kind: 'ftr' });
    for (const r of others) { const n = fieldServes(g, r.id) - kept(g, r.id) - 100; if (n > 0) g.airFleets.push(stub(r.id, n)); }
    const est = (fs: AirFleet[]): number => fs.reduce((s, f) => s + f.est.ftr + f.est.bmr, 0);
    /** Each command's shares of each kind of aircraft. */
    const shares = (fs: AirFleet[]): Record<string, number> => { const o: Record<string, number> = {}; for (const f of fs) o[`${f.parent}:${f.kind}`] = (o[`${f.parent}:${f.kind}`] ?? 0) + (f.share ?? 1); return o; };
    const before = { n: land(g, 'SOV').length, est: est(land(g, 'SOV')), shares: shares(land(g, 'SOV')), names: new Map(land(g, 'SOV').map((f) => [f.id, f.name])) };
    expect(big.ftr + big.bmr).toBeGreaterThan(100);
    const firstNew = g.nextAir;
    spreadAir(g, 'SOV');
    const after = land(g, 'SOV');
    expect(after.length).toBeGreaterThan(before.n);
    // (The parts split off: new groups, each on a field of its own with room for it.)
    const parts = after.filter((f) => f.id >= firstNew);
    expect(parts.length).toBeGreaterThan(0);
    for (const p of parts) {
      expect(p.base).not.toBe(F);
      // (Split from a group of the same command and kind, which keeps its name; the part has a name of its own and no commander.)
      expect(after.some((q) => q.id < firstNew && q.parent === p.parent && q.kind === p.kind)).toBe(true);
      expect(after.filter((q) => q.name === p.name)).toHaveLength(1);
      expect(p.leader).toBeNull();
    }
    for (const f of after) if (f.id < firstNew) expect(f.name).toBe(before.names.get(f.id));
    // Every command's establishment and shares are as they were, to the rounding of a group.
    expect(Math.abs(est(after) - before.est)).toBeLessThanOrEqual(after.length - before.n);
    const now = shares(after);
    for (const [k, v] of Object.entries(before.shares)) expect(now[k], k).toBeCloseTo(v, 9);
  });

  it('never moves the player\'s groups, and leaves history\'s campaigns alone', () => {
    const { g } = piled('SOV');
    g.human = 'SOV';
    expect(spreadAir(g, 'SOV')).toEqual([]);
    const h = createGame(3, null);
    expect(airWar(h)).toBe(false);
    for (const nat of Object.keys(h.nations)) expect(spreadAir(h, nat)).toEqual([]);
  });

  it('spreads the player\'s groups too once he has handed the air war to his staff', () => {
    const { g, F } = piled('SOV', 3, 'SOV');
    expect(spreadAir(g, 'SOV')).toEqual([]);
    expect(setDesk(g, 'air', true)).toBe(true);
    expect(spreadAir(g, 'SOV').length).toBeGreaterThan(0);
    expect(kept(g, F)).toBeLessThanOrEqual(fieldServes(g, F));
  });

  it('runs at every computer nation\'s month\'s end: no Soviet field is crowded as the months go by', () => {
    const g = startOpen(createGame(17, null));
    for (let m = 0; m < 24; m++) {
      runTurnAI(g);
      for (const id of new Set(land(g, 'SOV').map((f) => f.base))) expect(based(g, id), `${g.regions[id].name} month ${g.turn}`).toBeLessThanOrEqual(fieldServes(g, id));
    }
  }, 240_000);
});

describe('the player is told of his crowded airfields (b448)', () => {
  it('says how much can fly in the UI director\'s words', () => {
    const at: [number, string][] = [[0.99, 'nine in ten'], [0.85, 'nine in ten'], [0.84, 'three quarters'], [0.7, 'three quarters'], [0.65, 'two thirds'], [0.5, 'half'], [0.45, 'half'], [0.31, 'a third'], [0.25, 'a quarter'], [0.2, 'a fifth'], [0.1, 'a tenth'], [0.069, 'hardly any'], [0, 'hardly any']];
    for (const [s, w] of at) expect(shareWords(s), String(s)).toBe(w);
  });

  it('keeps one tray row that names the worst field, with "the" where a place takes it', () => {
    const { g, F } = piled('SOV', 3, 'SOV');
    const cs = crowdedFields(g, 'SOV');
    expect(cs.length).toBeGreaterThan(0);
    for (let i = 1; i < cs.length; i++) expect(cs[i].share).toBeGreaterThanOrEqual(cs[i - 1].share);
    const w = cs.find((c) => c.region === F)!;
    expect(w.based).toBe(based(g, F));
    expect(w.serves).toBe(fieldServes(g, F));
    const n = (x: number): string => x.toLocaleString('en-US');
    const one = crowdedRow(g, [w]);
    expect(one).toBe(`${g.regions[F].name}'s airfield is crowded: it serves ${n(w.serves)} aircraft and has ${n(w.based)}, so ${shareWords(w.share)} of them can fly`);
    expect(one).not.toMatch(/\.$/);
    const ruhr = g.regions.find((r) => r.name === 'Ruhr')!;
    const two = crowdedRow(g, [{ region: ruhr.id, serves: 500, based: 1600, share: 500 / 1600 }, w]);
    expect(two).toBe('2 of your airfields are crowded, the Ruhr\'s worst: it serves 500 aircraft and has 1,600, so a third of them can fly');
    expect(crowdedRow(g, [{ region: ruhr.id, serves: 1500, based: 1700, share: 1500 / 1700 }])).toBe('The Ruhr\'s airfield is crowded: it serves 1,500 aircraft and has 1,700, so nine in ten of them can fly');
    // (Nobody else's fields, and none in a campaign of history's.)
    expect(crowdedFields(g, 'SOV').every((c) => g.airFleets.some((f) => f.nat === 'SOV' && f.base === c.region))).toBe(true);
    expect(crowdedFields(createGame(3, 'SOV'), 'SOV')).toEqual([]);
  });

  it('puts one line on the month card the month a field first becomes crowded, and not again while it stays so', () => {
    const { g, F } = piled('SOV', 3, 'SOV');
    const mark = monthMark(g, 'SOV');
    const before = g.headlines.length;
    crowdedNews(g);
    const news = g.headlines.slice(before);
    expect(news).toHaveLength(1);
    const h = news[0], cs = crowdedFields(g, 'SOV'), w = cs[0];
    expect(h.nat).toBe('SOV');
    expect(h.region).toBe(w.region);
    const name = g.regions[w.region].name, n = (x: number): string => x.toLocaleString('en-US');
    const words = shareWords(w.share), only = w.share < 0.6 && words !== 'hardly any' ? 'only ' : '';
    const line = `serves ${n(w.serves)} aircraft and ${n(w.based)} are based there, so ${only}${words} of them fly. Move some groups to a field with room and they all fly.`;
    if (cs.length === 1) { expect(h.head).toBe(`${name}'s airfield is crowded`); expect(h.text).toBe(`It ${line}`); }
    else { expect(h.head).toMatch(/^[A-Z][a-z]+ airfields are crowded$/); expect(h.text).toBe(`${name}'s is the worst: it ${line}`); }
    // (It is news of his own with a title of its own: a NEWS band on his card, or a row of it where the bands are full.)
    const card = monthCard(g, mark, 'SOV');
    expect(card.bands.some((b) => b.word === 'NEWS' && b.head === h.head) || card.rows.some((r) => JSON.stringify(r).includes(h.head!))).toBe(true);
    // Not again while it stays crowded.
    g.turn++;
    crowdedNews(g);
    expect(g.headlines.length).toBe(before + 1);
    // Cleared (every group stood down for a month), then crowded again: told again.
    const all = g.airFleets.filter((f) => f.ship === undefined);
    const had = new Map(all.map((f) => [f.id, [f.ftr, f.bmr]]));
    for (const f of all) { f.ftr = 0; f.bmr = 0; }
    g.turn++;
    crowdedNews(g);
    expect(crowdedFields(g, 'SOV')).toEqual([]);
    expect(g.headlines.length).toBe(before + 1);
    for (const f of all) [f.ftr, f.bmr] = had.get(f.id)!;
    expect(crowdedFields(g, 'SOV').some((c) => c.region === F)).toBe(true);
    g.turn++;
    crowdedNews(g);
    expect(g.headlines.length).toBe(before + 2);
  });

  it('is quiet while his staff runs the air war, and for the computer\'s nations', () => {
    const { g } = piled('SOV', 3, 'SOV');
    setDesk(g, 'air', true);
    const before = g.headlines.length;
    crowdedNews(g);
    expect(g.headlines.length).toBe(before);
    const c = piled('SOV').g;
    const was = c.headlines.length;
    crowdedNews(c);
    expect(c.headlines.length).toBe(was);
  });
});
