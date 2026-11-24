// A garrison sent somewhere comes out of its town on the way (release 2; Brian, 24 November 2026: "if a unit is dug in
// and i click them to attack and move I want them to over ride their orders insted of me having to press a button").
// The rules give the hand of a garrison that may come out its reach from the open country of its province, with the
// move that coming out costs, so the 3D game can show it and bring the corps out as the order goes (TAKE THE FIELD,
// with no press). One that may not come out (shut in by a siege, or gone in this month and so with no move once out)
// is shown nothing, as before, and still names its cure.
import { describe, expect, it } from 'vitest';
import { createGame, hostileUnits, relocate } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { apOutOfTown, garrison, garrisonOf, roomIn } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { apForOrders, beginHuman, humanUp, orderTakeField, reach, reachOutOfTown, stepAI, whyNotLeaveTown } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

interface Bridge {
  load(text: string): unknown;
  reach(ids: number[]): { regions: { id: number; cost: number; attack: boolean }[]; spent: boolean };
}
const TC = (globalThis as Record<string, unknown>).TC as Bridge;

function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const goes = (c: Corps): boolean => c.nat === 'GER' && !c.dead && c.kind === 'inf' && c.joint === undefined && c.town === undefined;
/** A German province with room in its town, nobody inside and no enemy about, and one of its infantry corps put inside it a month ago. */
function inside(g: Game): { r: Region; c: Corps } {
  const r = g.regions.find((x) => x.ctrl === 'GER' && !garrisonOf(x).length && roomIn(g, x) >= 1 && x.units.some(goes) && !hostileUnits(g, x, 'GER').length)!;
  expect(r).toBeDefined();
  const c = r.units.find(goes)!;
  expect(garrison(g, c)).toBe(true);
  c.town = EARLIER; // (in the town since an earlier month: coming out leaves it all but one of its moves)
  return { r, c };
}
/** A month before this one, for a corps that went into its town then (these games are in their opening month, the first, and the rules take any month but this one alike). */
const EARLIER = -1;
const asList = (m: Map<number, { cost: number; attack: boolean }>, not: number): string[] =>
  [...m].filter(([id]) => id !== not).map(([id, x]) => `${id}:${x.cost}:${x.attack}`).sort();

describe('a garrison sent somewhere comes out of its town on the way (release 2)', () => {
  it('its reach is the one it has once out, with the move coming out costs', () => {
    const g = playing();
    const { r, c } = inside(g);
    expect(c.ap).toBe(0);
    expect(reach(g, c).size).toBe(0); // (inside, as ever, it goes nowhere)
    expect(whyNotLeaveTown(g, c)).toBeNull();
    const out = reachOutOfTown(g, c);
    expect(out.size).toBeGreaterThan(1);
    expect(apForOrders(g, c)).toBe(c.apMax - 1);
    expect(apOutOfTown(g, c)).toBe(c.apMax - 1);
    // Taken out for real, it reaches the same places at the same cost.
    expect(orderTakeField(g, c)).toBe(true);
    expect(c.town).toBeUndefined();
    expect(c.ap).toBe(c.apMax - 1);
    expect(asList(reach(g, c), r.id)).toEqual(asList(out, r.id));
    // (and out of the town the hand's reach is the plain one)
    expect(reachOutOfTown(g, c).size).toBe(0);
    expect(apForOrders(g, c)).toBe(c.ap);
  });

  it('the bridge gives the 3D game that reach for the hand, with moves in it', () => {
    const g = playing();
    const { r, c } = inside(g);
    const out = reachOutOfTown(g, c);
    TC.load(saveGame(g));
    const hand = TC.reach([c.id]);
    expect(hand.spent).toBe(false);
    expect(hand.regions.map((x) => `${x.id}:${x.cost}:${x.attack}`).sort()).toEqual(asList(out, r.id));
    // (the game the bridge loaded keeps the corps inside: showing where it could go moves nobody)
    const kept = loadGame(saveGame(g))!;
    expect(kept.corps.find((x) => x.id === c.id)?.town).toBe(EARLIER);
  });

  it('none for a garrison that went in this month: once out it would have no move left', () => {
    const g = playing();
    const { c } = inside(g);
    c.town = g.turn;
    expect(apOutOfTown(g, c)).toBe(0);
    expect(reachOutOfTown(g, c).size).toBe(0);
    TC.load(saveGame(g));
    const hand = TC.reach([c.id]);
    expect(hand.regions.length).toBe(0);
    expect(hand.spent).toBe(true);
  });

  it('none for a garrison shut in by an enemy army round its town', () => {
    const g = playing();
    const { r, c } = inside(g);
    const away = g.regions.find((x) => x.ctrl === 'GER' && x.id !== r.id && !r.nb.includes(x.id))!;
    for (const u of [...r.units]) relocate(g, u, away);
    const foe = g.regions.find((x) => x.ctrl === 'POL' && x.units.some((u) => u.nat === 'POL' && u.kind === 'inf'))!;
    for (const u of [...foe.units]) relocate(g, u, r);
    expect(hostileUnits(g, r, 'GER').length).toBeGreaterThan(0);
    expect(whyNotLeaveTown(g, c)).not.toBeNull();
    expect(reachOutOfTown(g, c).size).toBe(0);
    expect(apForOrders(g, c)).toBe(0);
  });
});
