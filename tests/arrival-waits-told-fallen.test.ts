// A nation that leaves the war for good keeps no telling flags (arrivalwaits.ts `nationDone`). The design director,
// 05:00Z, 26 November 2026, on the round 3 sceptic's truth3-4: "Edge 1: mend it. Clear the four telling flags in
// capitulate and in domination's layDown and leave, so no dead data stays in a lost war, with a test." The four flags
// of a row i are `rowTold:<i>`, `rowAt:<i>`, `rowFront:<i>` and `rowId:<i>:<k>`. Only those go: the rule's own
// `wait:<i>` stays, the row is not struck (no `sent:<i>`), and the rows of other nations are not touched. With the
// switch never on no telling flag exists, and the save is byte for byte the same with and without the helper's call.
//
// The boards are tests/arrival-waits-told-raised's: September 1939, Grenzmark beside Poznan, Poland at war with
// Germany; Magdeburg quiet. Two rows of the player's are told in month 2, each waiting on its corps at the front in the
// Grenzmark. A British row carries the four flags written by hand, so that the test sees another nation's row left as it
// was by this nation's fall.
import { afterEach, describe, expect, it, vi } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { startDomination } from '../src/campaign/domination';
import { addCorps, capitulate, createGame, startPhase } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

// (The helper's calls are counted, and in the switch-off proof left out on one of two equal boards: the save with the
// call must equal the save without it.)
const spy = vi.hoisted(() => ({ calls: [] as string[], skip: false }));
vi.mock('../src/campaign/arrivalwaits', async (importOriginal) => {
  const m = await importOriginal<typeof import('../src/campaign/arrivalwaits')>();
  return { ...m, nationDone: (g: Game, nat: string): void => { spy.calls.push(nat); if (!spy.skip) m.nationDone(g, nat); } };
});

const ROWS = (setup as any).expeditions as any[];
let added = 0;
afterEach(() => { while (added > 0) { ROWS.pop(); added--; } spy.calls.length = 0; spy.skip = false; });
const addRow = (row: any[]): number => { ROWS.push(row); added++; return ROWS.length - 1; };
const flagsOf = (g: Game, i: number): string[] => Object.entries(g.flags).filter(([k]) => k === `rowTold:${i}` || k === `rowAt:${i}` || k === `rowFront:${i}` || k.startsWith(`rowId:${i}:`)).map(([k, v]) => `${k}=${v}`).sort();

/**
 * (The contact rule's second form, the design director's ruling of 06:10Z, 26 November 2026: a corps beside the enemy
 * is held while it took part in the battle last resolved on its ground, or where nobody of its side keeps its province.
 * This board's corps at the front stand with others of their side, so each is set, by hand, as having fought the battle
 * last resolved where it stands, and is held as the board means. The relieved case: tests/contact-fact.test.ts.)
 */
const fought = (g: Game): void => { if (g.flags.fromWait !== 1) return; g.lastBattle = {}; for (const c of g.corps) if (!c.dead) (g.lastBattle[c.reg] ??= []).push(c.id); };
interface Rows { i: number; u: number; k: number }
interface Board extends Rows { g: Game; c: Corps; d: Corps; nat: string; at: number }

/**
 * The rows, laid once for a test at the foot of the data's list, due the month after `turn`: `i` from the player's
 * Test Corps, `u` from his Second Test Corps, and `k`, a British row. (A row that waits only for another row is the far
 * side's alone, game.ts `rowHolds`: on the old chart both rows wait on their corps at the front.)
 */
function lay(nat: string, place: string, turn = 1): Rows {
  const i = addRow([turn + 1, nat, 'Test Army', 'inf', 2, place, { from: ['Test Corps'] }]);
  const u = addRow([turn + 1, nat, 'Second Test Army', 'inf', 2, place, { from: ['Second Test Corps'] }]);
  const k = addRow([9, 'UK', 'Other Test Army', 'inf', 1, 'Magdeburg', { from: ['Other Test Corps'] }]);
  return { i, u, k };
}
/**
 * The player's month: both his corps at the front in the Grenzmark, and his two rows due. With the switch on the
 * British row carries the four flags written by hand (with it never on no telling flag exists).
 */
function board(g: Game, nat: string, place: string, rows: Rows, on = true): Board {
  if (on) g.flags.fromWait = 1;
  const c = addCorps(g, nat, 'Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
  const d = addCorps(g, nat, 'Second Test Corps', 'inf', 2, g.byName.Grenzmark.id, {});
  const k = rows.k;
  if (on) Object.assign(g.flags, { [`rowTold:${k}`]: 1, [`rowAt:${k}`]: g.byName.Magdeburg.id, [`rowFront:${k}`]: 1, [`rowId:${k}:0`]: 99999 });
  g.turn += 1;
  fought(g);
  startPhase(g, nat);
  return { ...rows, g, c, d, nat, at: g.byName[place].id };
}
const other = (b: Board): string[] => [`rowAt:${b.k}=${b.g.byName.Magdeburg.id}`, `rowFront:${b.k}=1`, `rowId:${b.k}:0=99999`, `rowTold:${b.k}=1`];

/** The four flags are there after the telling, and the rule's own month. */
function told(b: Board): void {
  const { g, c, d, i, u, at } = b;
  for (const r of [i, u]) expect(g.flags[`wait:${r}`]).toBe(2);
  expect(flagsOf(g, i)).toEqual([`rowAt:${i}=${at}`, `rowFront:${i}=1`, `rowId:${i}:0=${c.id}`, `rowTold:${i}=2`]);
  expect(flagsOf(g, u)).toEqual([`rowAt:${u}=${at}`, `rowFront:${u}=1`, `rowId:${u}:0=${d.id}`, `rowTold:${u}=2`]);
  expect(flagsOf(g, b.k)).toEqual(other(b));
}
/** After the fall: none of the four for his rows; `wait:<i>` kept, the row not sent; the British row as it was. */
function gone(b: Board): void {
  const { g } = b;
  expect(g.nations[b.nat].alive).toBe(false);
  for (const r of [b.i, b.u]) {
    expect(flagsOf(g, r)).toEqual([]);
    expect(g.flags[`wait:${r}`]).toBe(2);
    expect(g.flags[`sent:${r}`]).toBeUndefined();
  }
  expect(flagsOf(g, b.k)).toEqual(other(b));
  expect(spy.calls).toContain(b.nat);
}

describe('a nation that falls keeps no telling flags', () => {
  it('Germany, seated on the old chart, capitulates while two rows of his are told (game.ts `capitulate`)', () => {
    const b = board(startOpen(createGame(1, 'GER')), 'GER', 'Magdeburg', lay('GER', 'Magdeburg'));
    told(b);
    expect(b.g.headlines.filter((h) => h.turn === 2 && /Test Army/.test(h.text)).map((h) => h.text)).toEqual([
      'The Test Army is due in Magdeburg this month, formed from your Test Corps, which is at the front in the Grenzmark. The arrival waits, and forms the month after that corps leaves the front.',
      'The Second Test Army is due in Magdeburg this month, formed from your Second Test Corps, which is at the front in the Grenzmark. The arrival waits, and forms the month after that corps leaves the front.',
    ]);
    capitulate(b.g, 'GER', 'POL');
    gone(b);
  });

  it('Sweden, seated, lays down its arms when the last great power takes the world (domination.ts `layDown`, through `crown`)', () => {
    // (The last act as tests/domination.test.ts plays it: Germany beaten, the last war opened, so Sweden is at war with
    // Poland across the Grenzmark; then the other great powers fall to Britain, which holds the rest of the map.)
    const g = startOpen(createGame(1, 'SWE'));
    capitulate(g, 'GER', 'UK');
    expect(startDomination(g)).toBe(true);
    const b = board(g, 'SWE', 'Stockholm', lay('SWE', 'Stockholm', g.turn));
    told(b);
    for (const id of ['FRA', 'ITA', 'USA']) capitulate(g, id, 'UK');
    for (const r of g.regions) if (r.ctrl !== 'SOV' && r.owner !== 'ESP' && r.owner !== 'SWE') r.ctrl = 'UK';
    expect(flagsOf(g, b.i)).toHaveLength(4);
    capitulate(g, 'SOV', 'UK');
    // (Sweden did not capitulate: it laid down its arms, and only `layDown` writes 'terms'.)
    expect(g.nations.SWE.fellOn).toMatchObject({ to: 'UK', how: 'terms' });
    expect(spy.calls.filter((n) => n === 'SWE')).toHaveLength(1);
    gone(b);
  });

  it('America, seated on a campaign kept to the map of Europe, takes its forces home as the last act opens (domination.ts `leave`, through `startDomination`)', () => {
    // (As tests/domination.test.ts builds it: on the map of Europe the United States lies beyond the map's edge, and
    // is set in the war by hand; its war with Poland is set by hand too, to put its corps at the front.)
    const g = startOpen(createGame(1, 'USA'), false);
    const U = g.nations.USA;
    expect(U.capital).toBeLessThan(0);
    U.alive = true; U.dormant = false;
    U.atWar.push('POL'); g.nations.POL.atWar.push('USA');
    const b = board(g, 'USA', 'Magdeburg', lay('USA', 'Magdeburg', g.turn));
    told(b);
    capitulate(g, 'GER', 'UK');
    expect(flagsOf(g, b.i)).toHaveLength(4);
    expect(startDomination(g)).toBe(true);
    expect(U.dormant).toBe(true);
    expect(U.fellOn).toBeUndefined();
    gone(b);
  });
});

describe('with the switch never on the fall changes nothing of the save', () => {
  // (As tests/from-wait-switch-r2 proves "a war never switched on is the whole save of off": two equal wars, saved and
  // compared whole. Here the helper is called on one and left out on the other.)
  const twice = (play: (g: Game) => void): void => {
    const rows = lay('GER', 'Magdeburg');
    const saves = [false, true].map((skip) => {
      spy.skip = skip;
      const g = startOpen(createGame(1, 'GER'));
      board(g, 'GER', 'Magdeburg', rows, false);
      play(g);
      return saveGame(g);
    });
    expect(spy.calls.filter((n) => n === 'GER')).toEqual(['GER', 'GER']);
    expect(saves[0]).not.toMatch(/fromWait|rowTold|rowAt|rowFront|rowId/);
    expect(saves[1]).toBe(saves[0]);
  };

  it('Germany capitulates: the save is byte-equal with and without the helper', () => {
    twice((g) => { capitulate(g, 'GER', 'POL'); expect(g.nations.GER.alive).toBe(false); });
  });
});
