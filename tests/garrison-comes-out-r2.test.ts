// A garrison sent somewhere comes out of its town on the way (release 2; Brian, 24 November 2026: "if a unit is dug in
// and i click them to attack and move I want them to over ride their orders insted of me having to press a button").
// The rules give the hand of a garrison that may come out its reach from the open country of its province, with the
// move that coming out costs, so the 3D game can show it and bring the corps out as the order goes (TAKE THE FIELD,
// with no press). One that may not come out (shut in by a siege, or gone in this month and so with no move once out)
// is shown nothing, as before, and still names its cure.
import { describe, expect, it } from 'vitest';
import { createGame, hostileUnits, relocate } from '../src/campaign/game';
import { STACK } from '../src/campaign/types';
import { loadGame, saveGame } from '../src/campaign/save';
import { apOutOfTown, garrison, garrisonOf, roomIn } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { apForOrders, beginHuman, humanUp, orderTakeField, reach, reachOutOfTown, stepAI, whyNotLeaveTown } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

interface Bridge {
  load(text: string): unknown;
  save(): { text: string };
  reach(ids: number[]): { regions: { id: number; cost: number; attack: boolean }[]; spent: boolean };
  marchPlan(ids: number[], to: number): unknown;
  previewAttack(ids: number[], to: number, open?: boolean, glance?: boolean): unknown;
  takeOutlook(ids: number[], to: number): unknown;
  lead(ids: number[], to: number): { why?: string };
  battleLook(ids: number[], to: number): unknown;
  move(ids: number[], to: number): unknown;
  attack(ids: number[], to: number): { report?: unknown } | null;
  take(ids: number[], to: number): unknown;
  view(): unknown;
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

// ---- The 1.5 release lead's cure (24 Nov 18:42Z): out only as the order goes ---------------------------------------
// The bridge brings the garrison out inside the order's own call, and keeps it out only if the order went; the calls
// that only look see it as if out and put it back. Each look, and each order refused, leaves the saved game the same
// to the byte (no note, event or month-log line either: they are all in the save). A card's No and the war question's
// No make no call at all, so they are this same case.
describe('a garrison comes out only as the order goes (release 2 and 1.5.1)', () => {
  const saved = (): string => TC.save().text;
  /** Neumark: III Corps inside its town since an earlier month, nobody in its field, the Poles in Poznan next door. */
  function front(): { g: Game; r: Region; c: Corps; foe: number; home: number; far: number } {
    const g = playing();
    const r = g.byName['Neumark'];
    const c = r.units.find(goes)!;
    expect(c).toBeDefined();
    for (const u of r.units.filter((u) => u !== c && u.nat === 'GER')) relocate(g, u, g.byName['Grenzmark']);
    expect(garrison(g, c)).toBe(true);
    c.town = EARLIER;
    const foe = r.nb.find((j) => hostileUnits(g, g.regions[j], 'GER').length)!;
    const home = r.nb.find((j) => g.regions[j].ctrl === 'GER' && !hostileUnits(g, g.regions[j], 'GER').length)!;
    // (German ground well out of a month's reach: a march there is kept, never made at once)
    const far = g.regions.find((x) => x.ctrl === 'GER' && x.id !== r.id && !reachOutOfTown(g, c).has(x.id))!.id;
    expect(foe).toBeDefined();
    expect(home).toBeDefined();
    TC.load(saveGame(g));
    return { g, r, c, foe, home, far };
  }
  const corpsIn = (text: string, id: number): Corps => loadGame(text)!.corps.find((x) => x.id === id)!;
  /** How the picture of the war an answer carries shows a corps: inside its town or not, and its moves. */
  const pictured = (answer: unknown, id: number): { town: boolean; ap: number } | undefined => {
    const v = (answer as { view?: { corps?: { id: number; town?: boolean; ap: number }[] } } | null)?.view ?? (answer as { corps?: { id: number; town?: boolean; ap: number }[] });
    const c = v?.corps?.find((x) => x.id === id);
    return c ? { town: !!c.town, ap: c.ap } : undefined;
  };

  it('the looks see it as if out and leave the game the same to the byte', () => {
    const { c, foe, home } = front();
    const t0 = saved();
    const hand = TC.reach([c.id]);
    expect(hand.regions.some((x) => x.id === foe && x.attack)).toBe(true);
    expect(hand.regions.some((x) => x.id === home && !x.attack)).toBe(true);
    expect(saved()).toBe(t0);
    TC.marchPlan([c.id], home);
    expect(saved()).toBe(t0);
    const odds = TC.previewAttack([c.id], foe, false, true) as { valid?: boolean; refused?: string } | null;
    expect(odds).toBeTruthy();
    expect(saved()).toBe(t0);
    TC.previewAttack([c.id], foe, true, false);
    expect(saved()).toBe(t0);
    TC.takeOutlook([c.id], home);
    expect(saved()).toBe(t0);
    TC.lead([c.id], foe);
    expect(saved()).toBe(t0);
    TC.battleLook([c.id], foe);
    expect(saved()).toBe(t0);
    expect(corpsIn(saved(), c.id).town).toBe(EARLIER);
  });

  it('an order refused leaves it inside, nothing spent, the game the same to the byte', () => {
    const { c, far } = front();
    const t0 = saved();
    // (an attack on a place with no enemy next door, and on its own ground: refused)
    // The picture of the war each answer carries (the 3D game takes it as the map's) shows it inside, with no move,
    // as the save and a fresh view do: not as it stood while the call was made (the 1.5 pre-check, 19:11Z).
    for (const answer of [TC.attack([c.id], far), TC.attack([c.id], c.reg), TC.take([c.id], c.reg), TC.move([c.id], c.reg)]) {
      expect(saved()).toBe(t0);
      expect(pictured(answer, c.id)).toEqual({ town: true, ap: 0 });
    }
    expect(pictured(TC.view(), c.id)).toEqual({ town: true, ap: 0 });
    const back = corpsIn(saved(), c.id);
    expect(back.town).toBe(EARLIER);
    expect(back.ap).toBe(0);
  });

  it('an order that goes brings it out: a march, and an attack', () => {
    const marched = front();
    const t0 = saved();
    TC.move([marched.c.id], marched.home);
    const m = corpsIn(saved(), marched.c.id);
    expect(saved()).not.toBe(t0);
    expect(m.town).toBeUndefined();
    expect(m.reg).toBe(marched.home);
    expect(m.ap).toBeLessThan(m.apMax - 1);
    const fought = front();
    const rep = TC.attack([fought.c.id], fought.foe);
    expect(rep?.report).toBeTruthy();
    const a = corpsIn(saved(), fought.c.id);
    expect(a.town).toBeUndefined();
    expect(a.ap).toBeLessThan(a.apMax - 1);
  });

  it('guns alone: the refusal is said and nothing comes out', () => {
    const g = playing();
    const left = g.byName['Neumark'];
    // The town held by a corps of guns, alone in hand. (The Germans open with none, and engineers never go into a town, so
    // an infantry corps elsewhere is made one.)
    const guns = g.corps.find((x) => goes(x) && x.reg !== left.id)!;
    guns.kind = 'art';
    for (const u of left.units.filter((u) => u.nat === 'GER')) relocate(g, u, g.byName['Grenzmark']);
    relocate(g, guns, left);
    expect(garrison(g, guns)).toBe(true);
    guns.town = EARLIER;
    const foe = left.nb.find((j) => hostileUnits(g, g.regions[j], 'GER').length)!;
    expect(foe).toBeDefined();
    TC.load(saveGame(g));
    const t0 = saved();
    TC.reach([guns.id]);
    TC.lead([guns.id], foe);
    TC.previewAttack([guns.id], foe, true, false);
    TC.attack([guns.id], foe);
    expect(saved()).toBe(t0);
    expect(corpsIn(saved(), guns.id).town).toBe(EARLIER);
  });

  /**
   * Each of these corps stands in exactly one list, its own province's field or town (the town only if it is inside),
   * as the save writes the lists down (read raw: loading a save mends a corps missing from its lists).
   */
  const placedOnce = (text: string, ids: number[]): void => {
    const g = loadGame(text)!;
    const lists = (JSON.parse(text) as { regions: unknown[][] }).regions;
    for (const id of ids) {
      const c = g.corps.find((x) => x.id === id)!;
      if (c.dead) continue;
      const at = lists.flatMap((r, k) => [
        ...((r[6] as number[]) ?? []).filter((x) => x === id).map(() => `${k}:field`),
        ...((r[7] as number[] | undefined) ?? []).filter((x) => x === id).map(() => `${k}:town`),
      ]);
      expect(at).toEqual([`${c.reg}:${c.town === undefined ? 'field' : 'town'}`]);
    }
  };
  /** Saar, with two places in its town and the French next door: a German corps put inside it, in the month given. */
  function saar(g: Game, town: number, from?: Corps): Corps {
    const r = g.byName['Saar'];
    const c = from ?? r.units.find(goes) ?? g.corps.find((x) => goes(x) && x.reg !== r.id)!;
    if (c.reg !== r.id) relocate(g, c, r);
    expect(garrison(g, c)).toBe(true);
    c.town = town;
    return c;
  }
  const french = (g: Game): number => g.byName['Saar'].nb.find((j) => hostileUnits(g, g.regions[j], 'GER').length)!;

  it('two corps out, one with no move: the one that went to the attack stays out, the other is put back inside', () => {
    const g = playing();
    const a = saar(g, EARLIER); // (moves once out)
    const b = saar(g, g.turn); // (went in this month: none once out, so the attack goes without it)
    const foe = french(g);
    TC.load(saveGame(g));
    const rep = TC.attack([a.id, b.id], foe);
    expect(rep?.report).toBeTruthy();
    const text = saved();
    const gb = corpsIn(text, b.id);
    expect(gb.town).toBe(g.turn);
    expect(gb.reg).toBe(g.byName['Saar'].id);
    expect(gb.ap).toBe(0);
    expect(pictured(rep, b.id)).toEqual({ town: true, ap: 0 }); // (the answer's picture shows it back inside)
    expect(corpsIn(text, a.id).town === EARLIER).toBe(false);
    placedOnce(text, [a.id, b.id]);
  });

  it('a corps given a march to keep goes with the order: it stays out to make it, as the rules\' note says', () => {
    const g = playing();
    const a = saar(g, EARLIER);
    const b = saar(g, g.turn);
    const home = g.byName['Saar'].nb.find((j) => g.regions[j].ctrl === 'GER' && !hostileUnits(g, g.regions[j], 'GER').length)!;
    TC.load(saveGame(g));
    TC.move([a.id, b.id], home);
    const text = saved();
    expect(corpsIn(text, a.id).reg).toBe(home);
    const gb = corpsIn(text, b.id);
    expect(gb.town).toBeUndefined();
    expect(loadGame(text)!.flags[`march:${b.id}`]).toBeDefined();
    placedOnce(text, [a.id, b.id]);
  });

  it('a hand of a garrison and a corps in the field: looks leave the game the same, and putting back touches only the garrison', () => {
    const g = playing();
    const a = saar(g, g.turn); // (no move once out: the attack goes without it)
    const f = g.corps.find((x) => goes(x) && x.reg !== a.reg)!;
    relocate(g, f, g.byName['Saar']);
    const foe = french(g);
    const home = g.byName['Saar'].nb.find((j) => g.regions[j].ctrl === 'GER' && !hostileUnits(g, g.regions[j], 'GER').length)!;
    TC.load(saveGame(g));
    const t0 = saved();
    TC.reach([a.id, f.id]);
    TC.marchPlan([a.id, f.id], home);
    TC.previewAttack([a.id, f.id], foe, true, false);
    TC.takeOutlook([a.id, f.id], home);
    TC.lead([a.id, f.id], foe);
    expect(saved()).toBe(t0);
    // The corps in the field attacks; the garrison is left behind and put back inside. Whatever the attack did with
    // the field corps (it may have gone forward), it is left as the attack left it: in one list, its own.
    const rep = TC.attack([a.id, f.id], foe);
    expect(rep?.report).toBeTruthy();
    const text = saved();
    expect(corpsIn(text, a.id).town).toBe(g.turn);
    placedOnce(text, [a.id, f.id]);
  });

  it('putting back leaves the province\'s field as the order left it: a corps that marched in during the call stays', () => {
    const g = playing();
    const a = saar(g, g.turn); // (no move once out, and already where the order goes: it is put back inside)
    const r = g.byName['Saar'];
    const home = r.nb.find((j) => g.regions[j].ctrl === 'GER' && !hostileUnits(g, g.regions[j], 'GER').length)!;
    const f = g.corps.find((x) => goes(x) && x.reg !== r.id && x.reg !== home)!;
    relocate(g, f, g.regions[home]);
    TC.load(saveGame(g));
    // (a hand the 3D game does not make, a town corps and a corps of the next province, sent to the town's province:
    // the bridge must not count on the hand being one army)
    TC.move([a.id, f.id], r.id);
    const text = saved();
    expect(corpsIn(text, f.id).reg).toBe(r.id);
    expect(corpsIn(text, a.id).town).toBe(g.turn);
    placedOnce(text, [a.id, f.id]);
  });

  it('two corps inside with room outside for one: neither is offered, and neither comes out', () => {
    const g = playing();
    const r = g.byName['Saar'];
    const a = r.units.find(goes)!;
    const b = g.corps.find((x) => goes(x) && x.reg !== r.id)!;
    relocate(g, b, r);
    expect(garrison(g, a)).toBe(true);
    expect(garrison(g, b)).toBe(true);
    a.town = EARLIER;
    b.town = EARLIER;
    // The field filled to one short of a full stack: room outside for one corps of the two.
    const fill = g.corps.filter((x) => goes(x) && x.reg !== r.id && x.id !== a.id && x.id !== b.id).slice(0, STACK - 1);
    for (const x of fill) relocate(g, x, r);
    expect(whyNotLeaveTown(g, a)).toBeNull(); // (each alone could come out)
    const home = r.nb.find((j) => g.regions[j].ctrl === 'GER' && !hostileUnits(g, g.regions[j], 'GER').length)!;
    TC.load(saveGame(g));
    const t0 = saved();
    const hand = TC.reach([a.id, b.id]);
    expect(hand.regions.length).toBe(0);
    expect(hand.spent).toBe(true);
    TC.move([a.id, b.id], home);
    expect(saved()).toBe(t0);
    for (const id of [a.id, b.id]) expect(corpsIn(saved(), id).town).toBe(EARLIER);
  });
});
