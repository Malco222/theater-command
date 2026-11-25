// Earned nicknames and supply columns (release 2; Brian's yes of 23 November 2026 12:05Z, ideas 2 and 3). Each is
// behind its own switch (stage.json 'nicknames' and 'columns'): off, no campaign carries any of it.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import nations from '../data/campaign/nations.json';
import worldData from '../data/campaign/world.json';
import farWorld from '../data/campaign/far-world.json';
import { NICKNAMES, earnedName, hardWin, isBlocked, nicknamesOn, roomForName, setNicknames, styleKey } from '../src/campaign/nicknames';
import { COLUMNS, columnShare, columnsOn, columnsSettable, columnsView, partisansAt, setColumns } from '../src/campaign/columns';
import { createGame } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { COLUMNS_STAGE, NICKNAMES_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

/** A grand campaign from September 1939, nobody at the table, with the switches given. */
function war(on: { nicks?: boolean; cols?: boolean }, seed = 1): Game {
  const g = startOpen(createGame(seed, null));
  if (on.nicks) setNicknames(g, true);
  if (on.cols) expect(setColumns(g, true)).toBe(true);
  return g;
}

/** Plays whole months, every nation in turn. */
function months(g: Game, n: number): void {
  const end = g.turn + n;
  let guard = 0;
  while (g.turn < end && guard++ < 4000) stepAI(g);
}

describe('the switches', () => {
  it('ship off, and are read only where a campaign carries them', () => {
    expect((stage as { nicknames?: boolean }).nicknames).toBe(false);
    expect((stage as { columns?: boolean }).columns).toBe(false);
    expect(NICKNAMES_STAGE).toBe(false);
    expect(COLUMNS_STAGE).toBe(false);
    const g = startOpen(createGame(1, null));
    expect(nicknamesOn(g)).toBe(false);
    expect(columnsOn(g)).toBe(false);
    expect(g.flags.nicknames).toBeUndefined();
    expect(g.flags.columns).toBeUndefined();
  });
  it("columns are set in a campaign's opening month only; nicknames whenever the player likes", () => {
    const g = war({});
    expect(columnsSettable(g)).toBe(true);
    months(g, 1);
    expect(columnsSettable(g)).toBe(false);
    expect(setColumns(g, true)).toBe(false);
    expect(columnsOn(g)).toBe(false);
    setNicknames(g, true);
    expect(nicknamesOn(g)).toBe(true);
    setNicknames(g, false);
    expect(nicknamesOn(g)).toBe(false);
  });
});

describe('earned nicknames: the names', () => {
  it('every name is our own: none holds a real formation nickname', () => {
    for (const [key, s] of Object.entries(NICKNAMES.styles)) {
      for (const list of [s.any, s.arm, s.place]) for (const n of list) expect(isBlocked(n), `${key}: ${n}`).toBe(false);
      expect(s.any.length, key).toBeGreaterThanOrEqual(6);
      expect(s.arm.length, key).toBeGreaterThanOrEqual(2);
      expect(s.place.every((t) => t.includes('{place}')), key).toBe(true);
    }
  });
  it('every nation of every map names its formations in a style of its own', () => {
    const ids = new Set<string>([
      ...Object.keys(nations).filter((k) => /^[A-Z]{3}$/.test(k)),
      ...Object.keys((worldData as { nations: Record<string, unknown> }).nations),
      ...Object.keys((farWorld as { world: { nations: Record<string, unknown> } }).world.nations),
      'CHN',
    ]);
    for (const id of ids) expect(styleKey(id), id).not.toBe('_default');
  });
  it('a hard fight: an attack carried at poor odds or a heavy price, a line held against equal odds or better', () => {
    const H = NICKNAMES.hard;
    expect(hardWin('att', 1.0, 0.05)).toBe(true);
    expect(hardWin('att', H.attackOdds, 0)).toBe(true);
    expect(hardWin('att', 2.0, 0.05)).toBe(false);
    expect(hardWin('att', 2.0, H.bloody)).toBe(true);
    expect(hardWin('att', H.walkover, 0.5)).toBe(false); // a walkover never is, whatever it cost
    expect(hardWin('def', 1.0, 0)).toBe(true);
    expect(hardWin('def', 0.6, 0.05)).toBe(false);
    expect(hardWin('def', 0.6, H.bloody)).toBe(true);
  });
  it('a name is the same for the same corps in the same war, in its nation\'s style, never one its nation already carries', () => {
    const g = war({ nicks: true });
    const c = g.corps.find((x) => x.nat === 'GER' && x.kind === 'arm')!;
    const a = earnedName(g, c, 'Sedan'), b = earnedName(g, c, 'Sedan');
    expect(a).toBe(b);
    const s = NICKNAMES.styles.german;
    expect([...s.arm, ...s.any, ...s.place.map((t) => t.replace('{place}', 'Sedan'))]).toContain(a);
    const other = g.corps.find((x) => x.nat === 'GER' && x.id !== c.id)!;
    other.nick = a;
    expect(earnedName(g, c, 'Sedan')).not.toBe(a);
  });
});

describe('earned nicknames in a war', () => {
  it('off: no corps counts a hard fight or earns a name by it', () => {
    const g = war({});
    months(g, 10);
    expect(g.corps.some((c) => (c.rec?.hard ?? 0) > 0)).toBe(false);
  });
  it('on: corps of more than one nation win names by their hard fights, under the cap, logged in their diaries', () => {
    const g = war({ nicks: true });
    months(g, 36);
    const hard = g.corps.filter((c) => (c.rec?.hard ?? 0) > 0);
    expect(hard.length).toBeGreaterThan(5);
    const named = g.corps.filter((c) => c.log?.some((l) => l[1] === 'named'));
    expect(named.length).toBeGreaterThan(0);
    for (const c of named) {
      expect(c.nick).toBeTruthy();
      expect(isBlocked(c.nick!)).toBe(false);
      expect(c.rec!.hard).toBeGreaterThanOrEqual(NICKNAMES.need);
    }
    // No nation carries more names than its cap allows (a name earned before a corps was lost is kept on the wall).
    for (const nat of new Set(named.map((c) => c.nat))) {
      const field = g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort');
      const n = field.filter((c) => c.log?.some((l) => l[1] === 'named')).length;
      expect(n, nat).toBeLessThanOrEqual(Math.max(1, Math.floor(field.length * NICKNAMES.most)) + 1);
    }
    // A name changes no number: it is in the save and comes back with it.
    const back = loadGame(saveGame(g));
    for (const c of named) expect(back!.corps.find((x) => x.id === c.id)?.nick).toBe(c.nick);
    expect(roomForName(g, 'GER') === true || roomForName(g, 'GER') === false).toBe(true);
  }, 240_000);
});

describe('supply columns', () => {
  it('off: no column is sent and every corps takes its whole share', () => {
    const g = war({});
    months(g, 3);
    expect(g.columns).toBeUndefined();
    expect(columnsView(g)).toEqual([]);
    for (const c of g.corps.slice(0, 40)) expect(columnShare(g, c)).toBe(1);
  });
  it('partisans: none among a nation\'s own people; on occupied ground they grow with the months and are held down by troops', () => {
    const g = war({ cols: true });
    const own = g.regions.find((r) => r.owner === 'GER' && r.ctrl === 'GER')!;
    expect(partisansAt(g, 'GER', own.id)).toBe(0);
    const r = g.regions.find((x) => x.owner === 'POL' && x.terrain === 'forest') ?? g.regions.find((x) => x.owner === 'POL')!;
    const keep = { ctrl: r.ctrl, since: r.since, units: r.units };
    r.ctrl = 'GER'; r.units = [];
    r.since = g.turn;
    expect(partisansAt(g, 'GER', r.id)).toBe(0); // the bands take months to form
    r.since = g.turn - COLUMNS.partisans.rampMonths;
    const free = partisansAt(g, 'GER', r.id);
    expect(free).toBeGreaterThan(0);
    const garrison = g.corps.find((c) => c.nat === 'GER' && c.kind === 'inf')!;
    r.units = [garrison];
    expect(partisansAt(g, 'GER', r.id)).toBeLessThan(free);
    Object.assign(r, keep);
  });
  it('on: columns run from the railhead to the corps beyond it; what is lost is lost to the front, and the save keeps it', () => {
    const g = war({ cols: true });
    months(g, 14);
    expect(g.columns).toBeDefined();
    const all = Object.entries(g.columns!).flatMap(([nat, runs]) => runs.map((r) => ({ nat, ...r })));
    expect(all.length).toBeGreaterThan(0);
    for (const r of all) {
      expect(r.path.length).toBeGreaterThanOrEqual(2);
      for (let k = 1; k < r.path.length; k++) expect(g.regions[r.path[k - 1]].nb).toContain(r.path[k]);
      expect(r.share).toBeGreaterThanOrEqual(0);
      expect(r.share).toBeLessThanOrEqual(1);
      expect(r.lost).toBeLessThanOrEqual(r.lorries);
    }
    // A corps standing where its column went takes the column's share of its drafts and rest, never below the floor.
    for (const [nat, runs] of Object.entries(g.columns!)) for (const run of runs) {
      const c = g.corps.find((x) => x.nat === nat && x.reg === run.path[run.path.length - 1] && !x.dead);
      if (!c) continue;
      const k = columnShare(g, c);
      expect(k).toBeGreaterThanOrEqual(COLUMNS.floor);
      expect(k).toBeCloseTo(COLUMNS.floor + (1 - COLUMNS.floor) * run.share, 6);
    }
    const view = columnsView(g, null);
    expect(view.length).toBe(all.length);
    expect(view.every((v) => v.points.length === v.path.length)).toBe(true);
    const back = loadGame(saveGame(g));
    expect(back!.columns).toEqual(g.columns);
  }, 240_000);
  it('on, over a longer war: some columns are caught, by partisans on occupied ground and by aircraft', () => {
    // (Staged again, 25 November 2026: with the computer's air forces fighting back, 1.5.1's rule, seed 2's war runs
    // another course, and in its 26 months aircraft catch columns, 3,790 lorries of them, and partisans none. Seeds 1,
    // 3 and 4 have both: 32, 28 and 17 lorries to partisans. The war is seed 3's; what is asked of it is unchanged.)
    const g = war({ cols: true }, 3);
    const by = { air: 0, partisans: 0 };
    for (let m = 0; m < 26; m++) {
      months(g, 1);
      for (const runs of Object.values(g.columns ?? {})) for (const run of runs) for (const [, who, n] of run.hits) by[who] += n;
    }
    expect(by.air + by.partisans).toBeGreaterThan(0);
    expect(by.partisans).toBeGreaterThan(0);
  }, 400_000);
});

void humanUp;
