// An island's room in men, round 2: the design director's rulings of 00:20Z, 27 November 2026 (D1 to D6 and the row's
// formed figure) and the UI director's answers of 00:19Z, 27 November 2026 (U1 to U8), each held by a test that fails
// without it. (src/campaign/islands.ts and its readers.)
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { addCorps, createGame, men, placed, relocate, removeCorps, retreatTarget, rowCorps, startPhase, whyNoRoom, whyNoSwap } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { fits, islandOf, roomForDivision, roomRefusal, stayBehind, swapRefusal } from '../src/campaign/islands';
import { garrison } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';
import { chartDist } from '../src/campaign/wrap';
import setup from '../data/campaign/setup-1939.json';

type Call = (...a: unknown[]) => any;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
/** A corps of infantry put down for the test, with its month in hand. */
function corps(g: Game, nat: string, name: string, at: Region, divs = 1, str = 1): Corps {
  const c = addCorps(g, nat, name, 'inf', divs, at.id, {});
  c.str = str;
  c.ap = c.apMax;
  return c;
}
type Glob = (p: string, o: { eager: boolean; as: string }) => Record<string, string>;
const SOURCES = (import.meta as unknown as { glob: Glob }).glob('../src/**/*.ts', { eager: true, as: 'raw' });
const ROWS = (setup as unknown as { expeditions: any[][] }).expeditions;
/** The record's two rows for Kwajalein in January 1942 (T27): the Maizuru 2nd landing force and the 61st Guard Unit. */
const KWAJ = [171, 263];
const where = (g: Game, i: number): string[] => g.corps.filter((c) => !c.dead && c.nat === 'JAP' && c.name === ROWS[i][2]).map((c) => g.regions[c.reg].name);
/** Japan's months from T24 to `last`, Japan's phase opening each (its dated rows are read there). */
function months(g: Game, last: number, each: (t: number) => void = () => {}): void {
  for (let t = 24; t <= last; t++) { g.turn = t; startPhase(g, 'JAP'); each(t); }
}

describe('a dated row is never struck for room (the design director, 00:20Z, 27 November 2026, D1)', () => {
  it('with Kwajalein full at stop 3, its two rows land at the nearest held harbour with room by the sea\'s own distance, and neither is struck', () => {
    const g = startOpen(createGame(3, null));
    expect(KWAJ.map((i) => [ROWS[i][0], ROWS[i][5]])).toEqual([[27, 'Kwajalein'], [27, 'Kwajalein']]);
    const k = g.byName['Kwajalein'];
    expect(k.nb).toEqual([]);
    addCorps(g, 'JAP', 'A full division (a test row)', 'inf', 1, k.id, {});
    months(g, 36);
    expect(KWAJ.map((i) => g.flags[`sent:${i}`])).toEqual([1, 1]);
    expect(KWAJ.map((i) => where(g, i))).toEqual([['Eastern Marshalls'], ['Eastern Marshalls']]);
    // The nearest by chartDist (the measure nearHarbour uses) of the harbours Japan holds with room for them.
    const g2 = startOpen(createGame(3, null));
    const k2 = g2.byName['Kwajalein'];
    addCorps(g2, 'JAP', 'A full division (a test row)', 'inf', 1, k2.id, {});
    const one = { nat: 'JAP', kind: 'inf', divs: 1, str: 0.1 } as unknown as Corps;
    const near = g2.regions.filter((r) => r.port && r !== k2 && r.ctrl === 'JAP' && fits(g2, r, 'JAP', [one], { rows: r.units.filter((u) => u.kind !== 'eng').length }))
      .sort((a, b) => chartDist(g2, a, k2) - chartDist(g2, b, k2))[0];
    expect(near.name).toBe('Eastern Marshalls');
  });

  it('where no held ground anywhere has room, the rows wait past their half year unstruck, and land when room comes', () => {
    const g = startOpen(createGame(3, null));
    const k = g.byName['Kwajalein'];
    const fill = addCorps(g, 'JAP', 'A full division (a test row)', 'inf', 1, k.id, {});
    // Every other harbour Japan holds filled to its room for a corps of the rows' size.
    const one = { nat: 'JAP', kind: 'inf', divs: 1, str: 0.07 } as unknown as Corps;
    const filled = (r: Region): boolean => !fits(g, r, 'JAP', [one], { rows: r.units.filter((u) => u.kind !== 'eng').length });
    for (const r of g.regions) if (r.port && r !== k && r.ctrl === 'JAP') for (let n = 0; n < 8 && !filled(r); n++) addCorps(g, 'JAP', `Filler ${r.id}.${n} (a test row)`, 'inf', 1, r.id, {});
    const seen: string[] = [];
    months(g, 40, (t) => {
      seen.push(`T${t} ${KWAJ.map((i) => g.flags[`sent:${i}`] ?? '-').join('/')}`);
      if (t === 37) removeCorps(g, fill);
    });
    // Due T27, half a year out at T34: unstruck through T37, the months waited for room counted apart.
    expect(seen.slice(3, 14)).toEqual(['T27 -/-', 'T28 -/-', 'T29 -/-', 'T30 -/-', 'T31 -/-', 'T32 -/-', 'T33 -/-', 'T34 -/-', 'T35 -/-', 'T36 -/-', 'T37 -/-']);
    // Room comes at Kwajalein at T38: the Maizuru landing force lands there. (The 61st Guard Unit finds the room
    // taken by another row that waited, and waits on, still unstruck.)
    expect(where(g, 171)).toEqual(['Kwajalein']);
    expect(g.flags['sent:171']).toBe(1);
    expect(g.flags['sent:263']).toBeUndefined();
    expect(g.flags['roomwait:263']).toBeGreaterThan(6);
  });
});

/** Ground emptied of every corps, field and town. */
const clear = (g: Game, r: Region): void => { for (const u of [...r.units, ...(r.garr ?? [])]) removeCorps(g, u); };

describe('a beaten engineer corps falls back onto an island at room plus two (the design director, 00:20Z, 27 November 2026, D2)', () => {
  it('a British engineer corps beaten in Johore falls back onto Singapore, which holds its two engineers and a corps', () => {
    const g = startOpen(createGame(3, null));
    const sg = g.byName['Singapore'], jo = g.byName['Johore'];
    for (const r of [sg, jo]) clear(g, r);
    addCorps(g, 'UK', '1st Engineers (a test row)', 'eng', 1, sg.id, {});
    addCorps(g, 'UK', '2nd Engineers (a test row)', 'eng', 1, sg.id, {});
    addCorps(g, 'UK', 'A corps (a test row)', 'inf', 1, sg.id, {});
    const beaten = addCorps(g, 'UK', '3rd Engineers (a test row)', 'eng', 1, jo.id, {});
    const banned = new Set(jo.nb.filter((j) => j !== sg.id));
    // Ordered there, it does not fit: the island takes two engineer corps.
    expect(fits(g, sg, 'UK', [beaten])).toBe(false);
    // Beaten, it has the retreat's two places more.
    expect(fits(g, sg, 'UK', [beaten], { slack: 2 })).toBe(true);
    expect(retreatTarget(g, beaten, jo, banned)?.name).toBe('Singapore');
  });
});

/** A whole-world war with a player, at his first month (seed 3). */
function war(human: string): Game {
  const g = startOpen(createGame(3, human));
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
  return g;
}

describe('a fortress crew takes no room on an island, whatever the fortress rows say (the design director, 00:20Z, 27 November 2026, D4)', () => {
  it("Pearl Harbor reads its one field division's men there, not its coast artillery's, with the fortress rows off", () => {
    const g = war('USA');
    expect(g.flags.fortRows).toBeUndefined();
    const pearl = g.byName['Pearl Harbor'], hon = g.byName['Honolulu'];
    const all = [...pearl.units, ...(pearl.garr ?? [])].filter((u) => u.nat === 'USA');
    expect(all.map((u) => [u.kind, Math.round(u.divs * u.str * 15000)]).sort()).toEqual([['fort', 15000], ['inf', 10570]]);
    const two = addCorps(g, 'USA', 'Two divisions (a test row)', 'inf', 2, hon.id, {});
    expect(roomRefusal(g, pearl, [two])).toBe('Pearl Harbor has room for 2 divisions, about 30,000 men, and 11,000 of yours are there: only 19,000 more can go, and you are sending 30,000.');
    // A corps of 19,000 fits beside them, where the crew's 15,000 counted would have left 4,000.
    const one = addCorps(g, 'USA', 'One more (a test row)', 'inf', 2, hon.id, {});
    one.str = 19000 / 30000;
    expect(fits(g, pearl, 'USA', [one])).toBe(true);
  }, 300000);
});

describe('an island holds no more corps than any ground (the design director, 00:20Z, 27 November 2026, D5)', () => {
  it("Singapore with five small corps of his refuses a sixth whose men fit, in the sentence any ground that holds five says; seven on a retreat; engineers and the coast guns apart", () => {
    const g = war('UK');
    const sg = g.byName['Singapore'], jo = g.byName['Johore'];
    // (Malaya Command and four small corps of 3,000: five fighting corps, 19,050 men of a room of 75,000; the coast
    // guns, a fortress crew, beside them.)
    for (let k = 1; k <= 4; k++) corps(g, 'UK', `Small ${k}`, sg, 1, 0.2);
    const field = placed(g, sg, 'UK');
    expect(field.filter((u) => u.kind !== 'eng' && u.kind !== 'fort').length).toBe(5);
    expect([...field, ...(sg.garr ?? [])].some((u) => u.kind === 'fort')).toBe(true);
    const sixth = corps(g, 'UK', 'Sixth (a test row)', jo, 1, 0.2), seventh = corps(g, 'UK', 'Seventh (a test row)', jo, 1, 0.2);
    expect(fits(g, sg, 'UK', [sixth])).toBe(false);
    expect(whyNoRoom(g, sixth, sg)).toBe('Singapore has no room for another corps.');
    expect(roomRefusal(g, sg, [sixth, seventh])).toBe('Singapore has no room for 2 more corps.');
    TC().load(saveGame(g));
    expect(TC().railTip([sixth.id], sg.id).no).toBe('Singapore has no room for another corps.');
    expect(TC().railTip([sixth.id, seventh.id], sg.id).no).toBe('Singapore has no room for 2 more corps.');
    // A retreat: seven, as on any ground.
    expect(fits(g, sg, 'UK', [sixth, seventh], { slack: 2 })).toBe(true);
    const eighth = corps(g, 'UK', 'Eighth (a test row)', jo, 1, 0.2);
    expect(fits(g, sg, 'UK', [sixth, seventh, eighth], { slack: 2 })).toBe(false);
    // Engineers are not barred by the five.
    const eng = addCorps(g, 'UK', 'Engineers (a test row)', 'eng', 1, jo.id, {});
    expect(fits(g, sg, 'UK', [eng])).toBe(true);
    // Ground that is no island: five, as it always was.
    for (const u of [...jo.units]) if (u.nat === 'UK' && u.kind !== 'eng') removeCorps(g, u);
    for (let k = 1; k <= 5; k++) corps(g, 'UK', `Johore ${k}`, jo, 1, 0.2);
    expect(fits(g, jo, 'UK', [corps(g, 'UK', 'One more', g.byName['Kuala Lumpur'], 1, 0.2)])).toBe(false);
  }, 300000);
});

describe("a staff's check asks with its corps where it names one, and is named 'room for a division' where it has none (the design director, 00:20Z, 27 November 2026, D6)", () => {
  it('Kwajalein with 7,500 of Japan\'s men has no room for a division, and still takes a corps of 4,000', () => {
    const g = startOpen(createGame(3, null));
    const k = g.byName['Kwajalein'];
    corps(g, 'JAP', 'Half a division (a test row)', k, 1, 0.5);
    const small = corps(g, 'JAP', 'A small corps (a test row)', g.byName['Eastern Marshalls'], 1, 4000 / 15000);
    expect(roomForDivision(g, k, 'JAP')).toBe(false);
    expect(fits(g, k, 'JAP', [small])).toBe(true);
    // Ground that is no island: one place among the five, as before.
    const kl = g.byName['Kuala Lumpur'];
    expect(islandOf(g, kl)).toBeNull();
    expect(roomForDivision(g, kl, 'JAP', 4)).toBe(true);
    expect(roomForDivision(g, kl, 'JAP', 5)).toBe(false);
  });

  it('no reader tests the room left against one division by a path of its own: it asks roomForDivision, or fits with its corps', () => {
    const bad: string[] = [];
    for (const [f, src] of Object.entries(SOURCES)) {
      if (f.endsWith('/campaign/islands.ts')) continue;
      src.split('\n').forEach((line, i) => { if (/roomLeft\([^;]*\)\s*(<|>=)\s*1\b/.test(line)) bad.push(`${f}:${i + 1}: ${line.trim()}`); });
    }
    expect(bad).toEqual([]);
  });
});

describe("a dated row's room is read on the corps it forms, not its stated size (laid without asking, all right by the design director, 00:20Z, 27 November 2026)", () => {
  it('a row made from corps on the board at the strength it takes out of them, and a row with opening divisions at those', () => {
    const g = startOpen(createGame(3, null));
    expect(g.flags.open).toBe(1);
    // America's 24th Infantry Division at Pearl Harbor (far-shores.json): stated one division at 0.65, 9,750 men; made
    // from what is left of the Hawaiian Division, here 0.3 of a division, 4,500 men, it forms at 4,500.
    const row24 = [26, 'USA', '24th Infantry Division', 'inf', 1, 'Pearl Harbor', { str: 0.65, from: ['Hawaiian Division'] }];
    const left = { nat: 'USA', kind: 'inf', divs: 1, str: 0.3 } as unknown as Corps;
    expect(rowCorps(g, 'USA', row24, row24[6], [left]).map(men)).toEqual([4500]);
    expect(rowCorps(g, 'USA', row24, row24[6]).map(men)).toEqual([9750]);
    // XIII Corps, stated two divisions, opens with one in the grand campaign (setup-1939.json openDivs).
    const row13 = [24, 'UK', 'XIII Corps, Eighth Army', 'inf', 2, 'Alexandria', { openDivs: 1 }];
    expect(rowCorps(g, 'UK', row13, row13[6]).map(men)).toEqual([15000]);
  });
});

// The UI director's answers of 00:19Z, 27 November 2026, on the live sentences.
describe('U1: under 1,000 there, both figures whole and the line adds up', () => {
  it('Guam with 750 of his there: only 29,250 more can go; the same in the swap line', () => {
    const g = war('USA');
    const guam = g.byName['Guam'], manila = g.byName['Manila'];
    clear(g, guam);
    corps(g, 'USA', 'Guam Insular Force (a test row)', guam, 1, 750 / 15000);
    const big = corps(g, 'USA', 'Big', manila, 2);
    expect(roomRefusal(g, guam, [big])).toBe('Guam has room for 2 divisions, about 30,000 men, and 750 of yours are there: only 29,250 more can go, and you are sending 30,000.');
    TC().load(saveGame(g));
    expect(TC().bySeaWhy([big.id], guam.id).why).toBe('Guam has room for 2 divisions, about 30,000 men, and 750 of yours are there: only 29,250 more can go, and you are sending 30,000.');
    const out = corps(g, 'USA', 'Goes Out (a test row)', guam, 1, 600 / 15000);
    expect(swapRefusal(g, guam, big, out)).toBe('Guam has room for 2 divisions, about 30,000 men, and 750 of yours stay there after the swap: only 29,250 more can go, and Big has 30,000.');
  }, 300000);
});

describe('U3: a march order reads the march sentence in 3D as on the page; the swap sentence only where a swap is asked', () => {
  it('Pearl Harbor, 11,000 in its town and a corps of 15,000 in its field: the 24th Division Group marches from Honolulu', () => {
    const g = war('USA');
    const pearl = g.byName['Pearl Harbor'], hon = g.byName['Honolulu'];
    clear(g, pearl);
    expect(garrison(g, corps(g, 'USA', 'Inside', pearl, 1, 11000 / 15000))).toBe(true);
    const field = corps(g, 'USA', 'Field', pearl, 1, 1);
    const group = corps(g, 'USA', '24th Division Group', hon, 2);
    const march = 'Pearl Harbor has room for 2 divisions, about 30,000 men, and 26,000 of yours are there: only 4,000 more can go, and you are sending 30,000.';
    expect(whyNoRoom(g, group, pearl)).toBe(march);
    TC().load(saveGame(g));
    expect(TC().move([group.id], pearl.id).note).toBe(march);
    // Asked to change places with the corps in the field, the swap's own sentence (game.ts whyNoSwap).
    expect(whyNoSwap(g, group, field)).toBe('Pearl Harbor has room for 2 divisions, about 30,000 men, and 11,000 of yours stay there after the swap: only 19,000 more can go, and 24th Division Group has 30,000.');
  }, 300000);
});

describe('U6: a group march let in only in part names the corps that stay behind, on the page and in 3D alike', () => {
  const board = (inside: number) => {
    const g = war('USA');
    const pearl = g.byName['Pearl Harbor'], hon = g.byName['Honolulu'];
    clear(g, pearl);
    expect(garrison(g, corps(g, 'USA', 'Inside', pearl, 1, inside / 15000))).toBe(true);
    return { g, pearl, hon };
  };
  it('one stays behind on a full island; two stay behind; one stays behind where some room is left', () => {
    {
      const { g, pearl, hon } = board(14000);
      const small = corps(g, 'USA', 'Small', hon, 1, 4000 / 15000), d24 = corps(g, 'USA', '24th Division Group', hon), d25 = corps(g, 'USA', '25th Division Group', hon);
      TC().load(saveGame(g));
      // (26,000 there: 12,000 more fit, the small one goes in; then 4,000 can go, and the full line is a reading below.)
      const one = TC().move([small.id, d24.id], pearl.id) as { moved: { id: number }[]; note: string };
      expect(one.moved.map((x) => x.id)).toEqual([small.id]);
      expect(one.note).toBe('The 24th Division Group stays behind. Pearl Harbor has room for 2 divisions, about 30,000 men, and 18,000 of yours are there: only 12,000 more can go, and you are sending 15,000.');
      TC().load(saveGame(g));
      const two = TC().move([small.id, d24.id, d25.id], pearl.id) as { moved: { id: number }[]; note: string };
      expect(two.note).toBe('The 24th Division Group and the 25th Division Group stay behind. Pearl Harbor has room for 2 divisions, about 30,000 men, and 18,000 of yours are there: only 12,000 more can go, and you are sending 30,000.');
      // The page's sentence is the same function's (ui/main.ts marchTo calls it: the page's wiring is a reading).
      relocate(g, small, pearl);
      expect(stayBehind(g, pearl, [d24, d25])).toBe(two.note);
    }
    {
      const { g, pearl, hon } = board(26000);
      const small = corps(g, 'USA', 'Small', hon, 1, 4000 / 15000), d24 = corps(g, 'USA', '24th Division Group', hon);
      TC().load(saveGame(g));
      const full = TC().move([small.id, d24.id], pearl.id) as { moved: { id: number }[]; note: string };
      expect(full.moved.map((x) => x.id)).toEqual([small.id]);
      expect(full.note).toBe('The 24th Division Group stays behind. Pearl Harbor is full: it has room for about 30,000 men and 30,000 of yours are there.');
    }
  }, 300000);
});
