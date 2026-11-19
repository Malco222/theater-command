// A forecast read off a copy of the campaign is kept with the save it was made from (the 3D table's hover asks the
// same question again and again, and the march card asks it once more). The same save and the same question give the
// same answer whether it is worked out afresh or kept, the game is untouched, and an answer handed out cannot spoil
// the one kept.
import { describe, expect, it } from 'vitest';
import { createGame, allied, relocate } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { SIEGE, garrisonOf } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { townSize } from '../src/campaign/towns';
import { beginHuman, humanUp, marchOrder, marchOutlook, orderMove, orderTake, reach, stepAI, stormChoice, stormOutlook } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

function playing(): Game {
  const g = startOpen(createGame(3, 'GER'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const germansBy = (g: Game, r: Region): Corps[] =>
  r.nb.flatMap((j) => g.regions[j].units.filter((u) => u.nat === 'GER' && u.kind !== 'fort' && u.kind !== 'eng' && u.ap > 0));
/** A Polish city with German corps beside it and nobody in its field: only its guard holds it. */
function city(): { g: Game; town: Region; ger: Corps[] } {
  const g = playing();
  const town = g.regions.find((r) => r.ctrl === 'POL' && SIEGE.summonsNot.includes(townSize(g, r)) && !garrisonOf(r).length && germansBy(g, r).length >= 3)!;
  expect(town).toBeDefined();
  const away = g.regions.find((r) => r.ctrl === 'POL' && r.id !== town.id && !town.nb.includes(r.id))!;
  for (const u of [...town.units]) relocate(g, u, away);
  return { g, town, ger: germansBy(g, town).filter((c) => !!reach(g, c).get(town.id)) };
}

/** What the copy says, worked out as before there was any keeping: a fresh load of the save, every time. */
function afresh(g: Game, corps: Corps[], to: number) {
  const h = loadGame(saveGame(g))!;
  const there: number[] = [];
  for (const c of marchOrder(g, corps)) { const hc = h.corps.find((x) => x.id === c.id); if (hc && orderMove(h, hc, to) > 0 && hc.reg === to) there.push(c.id); }
  if (!there.length || allied(h, g.human!, h.regions[to].ctrl)) return { there, outlook: null, prepared: null };
  const outlook = stormOutlook(h, to);
  h.flags[`siege:${to}`] = h.turn - 1;
  return { there, outlook, prepared: stormOutlook(h, to) };
}

describe('a forecast off a copy, kept', () => {
  it('answers as a fresh copy does, asked once or many times, and leaves the game as it was', () => {
    const { g, town, ger } = city();
    const force = ger.slice(0, 3);
    const before = saveGame(g);
    const first = marchOutlook(g, force, town.id)!;
    expect(first.meets).toBe('guard');
    const want = afresh(g, force, town.id);
    expect(first.going).toEqual(want.there);
    expect(first.outlook).toEqual(want.outlook);
    expect(first.prepared).toEqual(want.prepared);
    // Asked again, as the pointer rests over the town: the same answer, from the kept copy.
    for (let k = 0; k < 3; k++) expect(marchOutlook(g, force, town.id)).toEqual(first);
    expect(saveGame(g)).toBe(before);
    // An answer handed out and then scribbled on does not change the next one.
    const scribbled = marchOutlook(g, force, town.id)!;
    scribbled.outlook!.lines[0].label = 'scribbled';
    scribbled.going.push(-1);
    expect(marchOutlook(g, force, town.id)).toEqual(first);
    // Another question, or the same one once the game has moved, is worked out afresh.
    const two = ger.slice(0, 2);
    expect(marchOutlook(g, two, town.id)!.outlook).toEqual(afresh(g, two, town.id).outlook);
    g.flags['forecast-kept-test'] = 1;
    expect(marchOutlook(g, force, town.id)!.outlook).toEqual(afresh(g, force, town.id).outlook);
  });

  it('the storm choice, made ready, answers as a fresh copy does', () => {
    const { g, town, ger } = city();
    expect(orderTake(g, ger.slice(0, 3), town.id, false).stands).toBe('guard');
    stormOutlook(g, town.id);
    const c = stormChoice(g, town.id)!;
    expect(c.hasty).toBe(true);
    const h = loadGame(saveGame(g))!;
    h.flags[`siege:${town.id}`] = h.turn - 1;
    expect(c.prepared).toEqual(stormOutlook(h, town.id));
    expect(stormChoice(g, town.id)).toEqual(c);
  });
});
