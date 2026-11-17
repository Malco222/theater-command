// A save keeps the order of the corps inside a town (Version 84.3; the code checker's find of 2026-11-17). The region
// row of a save listed the corps in the field only, and a load put a garrison back in the order of the corps' roll.
// That order is not idle: it decides who defends a storm, who is turned out of a full town and what becomes of each
// corps when the town falls, so a campaign went on differently after a reload than it would have unsaved. The row
// now carries the garrison as an eighth entry where there is one. A province with nobody inside its town is written
// as it always was, and a save from an older build loads as it did.
import { describe, expect, it } from 'vitest';
import { createGame, relocate } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { garrison, garrisonOf, roomIn } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Game, Region } from '../src/campaign/types';

function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/**
 * A German province whose town has room for two corps, with two infantry corps in its field (no province of the
 * September 1939 start has both, so the second is brought up from another): they go in with the newest first,
 * against the roll.
 */
function garrisoned(g: Game): Region {
  const goes = (c: { nat: string; kind: string; joint?: string; town?: number; dead?: boolean }): boolean => c.nat === 'GER' && !c.dead && c.kind === 'inf' && c.joint === undefined && c.town === undefined;
  const r = g.regions.find((x) => x.ctrl === 'GER' && !garrisonOf(x).length && roomIn(g, x) >= 2 && x.units.filter(goes).length >= 1)!;
  expect(r).toBeDefined();
  const second = g.corps.find((c) => goes(c) && c.reg !== r.id)!;
  expect(second).toBeDefined();
  if (r.units.filter(goes).length < 2) relocate(g, second, r);
  const going = r.units.filter(goes).sort((a, b) => b.id - a.id).slice(0, 2);
  for (const c of going) expect(garrison(g, c), c.name).toBe(true);
  return r;
}
const inside = (r: Region): number[] => garrisonOf(r).map((c) => c.id);

describe('a save keeps the order of the corps inside a town (Version 84.3)', () => {
  it('a garrison comes back from a save in the order it stood in', () => {
    const g = playing();
    const r = garrisoned(g);
    const was = inside(r);
    expect(was.length).toBeGreaterThanOrEqual(2);
    expect(was).not.toEqual([...was].sort((a, b) => a - b)); // not the order of the roll: the old load would have changed it
    const text = saveGame(g);
    const back = loadGame(text)!;
    expect(back).not.toBeNull();
    expect(inside(back.regions[r.id])).toEqual(was);
    // And the save of the game that came back is the same save, to the letter.
    expect(saveGame(back)).toBe(text);
  });

  it('a province with nobody inside its town is written as it always was', () => {
    const g = playing();
    const rows = JSON.parse(saveGame(g)).regions as unknown[][];
    for (const [i, row] of rows.entries()) expect(row.length, g.regions[i].name).toBe(garrisonOf(g.regions[i]).length ? 8 : 7);
    const r = garrisoned(g);
    const after = JSON.parse(saveGame(g)).regions as unknown[][];
    expect(after[r.id].length).toBe(8);
    expect(after[r.id][7]).toEqual(inside(r));
    expect(after.filter((row) => row.length === 8).length).toBe(g.regions.filter((x) => garrisonOf(x).length > 0).length);
  });

  it('a save from an older build, with no such entry, puts the garrison back in the order of the roll as before', () => {
    const g = playing();
    const r = garrisoned(g);
    const d = JSON.parse(saveGame(g));
    for (const row of d.regions as unknown[][]) row.length = 7;
    const back = loadGame(JSON.stringify(d))!;
    expect(back).not.toBeNull();
    expect(inside(back.regions[r.id])).toEqual([...inside(r)].sort((a, b) => a - b));
    // Nobody is lost or doubled either way.
    expect(back.regions[r.id].units.some((c) => inside(r).includes(c.id))).toBe(false);
  });
});
