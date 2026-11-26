// The contact rule's fight half: the battle record (the design director's ruling of 06:10Z, 26 November 2026: "A corps
// is held while it took part in the battle last resolved on its ground (the board's record, no window)"). The board
// kept no such fact, so the game keeps one: `g.lastBattle`, for each province the ids of the living corps that took
// part in the last land battle resolved on it, under the switch 'fromWait' only. No month in it, no count.
// The design: /home/user/runs/contact2/SPEC.md sections 3 and 6.2, with its section 13.
// The record field: kept whole in a save under the switch, never written with it off, and gone when it is turned off.
import { describe, expect, it } from 'vitest';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { createGame } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const open = (): Game => startOpen(createGame(1, null));

describe('the record field', () => {
  it('a save made under the switch keeps the record whole, and loads it back the same', () => {
    const g = open();
    setFromWait(g, true);
    g.lastBattle = { [g.byName.Berlin.id]: [3, 7], [g.byName.Warsaw.id]: [11] };
    const back = loadGame(saveGame(g))!;
    expect(back.lastBattle).toEqual(g.lastBattle);
    expect(JSON.parse(saveGame(back)).lastBattle).toEqual(g.lastBattle);
  });

  it('a war that never had the switch on writes no record in its save', () => {
    const g = open();
    expect(g.lastBattle).toBeUndefined();
    expect('lastBattle' in JSON.parse(saveGame(g))).toBe(false);
  });

  it('turning the switch off deletes the record, so the save is a never-switched war in this field', () => {
    const g = open();
    setFromWait(g, true);
    g.lastBattle = { [g.byName.Berlin.id]: [3] };
    setFromWait(g, false);
    expect(g.lastBattle).toBeUndefined();
    expect('lastBattle' in JSON.parse(saveGame(g))).toBe(false);
    // On again: an empty record, never a battle older than those missed while off.
    setFromWait(g, true);
    expect(g.lastBattle).toBeUndefined();
  });

  it('a record left on a war whose switch is cleared by hand is not saved', () => {
    const g = open();
    g.lastBattle = { [g.byName.Berlin.id]: [3] };
    expect('lastBattle' in JSON.parse(saveGame(g))).toBe(false);
  });
});
