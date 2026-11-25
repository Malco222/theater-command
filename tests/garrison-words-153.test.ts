// Garrisons in the 1.5.3 patch: the 1.5.1 playtest's finds 14 to 18. Since 1.5.1 a garrison comes out of its town as an
// order goes (comeOut, campaign-bridge.ts): the looks see it as if out, and an order keeps it out only if it went.
// What was left over is what the player is told, and the hand of a garrison and a field army in the 3D game:
// - 17: a garrison asked to take ship was told it had moved this month. It is told what it is, as the railway tells it.
// - 16: the march up and the attack was planned without the garrison in hand (approach filtered it out, and was not
//   looked at as if out); and an order refused with nothing said now says why, in the words the pointer gives.
// - 18: a garrison that went in this month (Warsaw on the eve of war) reached nothing, was not spent, and was told to
//   take the field, which gives it no move. It is told when it can march, and is spent, on the eve too.
// - 15: the garrison's card read "No moves left this month" although it can march: the view says the moves it has once out.
// - 14: the reach names the corps of a garrison in hand that come out with an order, for a hand that has a field army too.
import { describe, expect, it } from 'vitest';
import { createGame, hostileUnits, relocate } from '../src/campaign/game';
import { whyNotLand, whyNotShip } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { besieged, garrison } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { townName } from '../src/campaign/towns';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import { STACK, type Corps, type Game, type Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Seen = { id: number; name: string; reg: number; town?: boolean; ap: number; apMax: number; apOut?: number; garrison?: string; nat: string };
type Row = { id: number; name: string; going: number[]; left: number };
interface Bridge {
  start(o: unknown): unknown;
  load(text: string): unknown;
  save(): { text: string };
  view(): { eve: boolean; turn: number; corps: Seen[] };
  reach(ids: number[]): { regions: { id: number }[]; spent: boolean; why: string; out: number[] };
  garrisonWhy(region: number): { why: string };
  move(ids: number[], to: number): { moved: { id: number; path: number[] }[]; note: string };
  rail(ids: number[], to: number): { moved: unknown[]; said: string };
  railTip(ids: number[], to: number): { no: string };
  approach(ids: number[], to: number): { ok: boolean; none: string; rows: Row[] };
  bySeaWhy(ids: number[], to: number): { why: string };
}
const TC = (globalThis as Record<string, unknown>).TC as Bridge;

function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const goes = (c: Corps): boolean => c.nat === 'GER' && !c.dead && c.kind === 'inf' && c.joint === undefined && c.town === undefined;
/** A month before this one, for a corps that went into its town then (these games are in their opening month). */
const EARLIER = -1;
/** A German infantry corps put inside the town of a province (brought there first if none stands in its field), as having gone in that month. */
function inside(g: Game, at: Region, town: number): Corps {
  const c = at.units.find(goes) ?? g.corps.find((x) => goes(x) && x.reg !== at.id)!;
  if (c.reg !== at.id) relocate(g, c, at);
  expect(garrison(g, c)).toBe(true);
  c.town = town;
  return c;
}
/** A German infantry corps brought into a province's field from elsewhere. */
function bring(g: Game, at: Region): Corps {
  const c = g.corps.find((x) => goes(x) && x.reg !== at.id)!;
  relocate(g, c, at);
  return c;
}
/** German ground next door with no enemy on it. */
const home = (g: Game, r: Region): number => r.nb.find((j) => g.regions[j].ctrl === 'GER' && !hostileUnits(g, g.regions[j], 'GER').length)!;
/** An enemy army next door. */
const foeBy = (g: Game, r: Region): number => r.nb.find((j) => hostileUnits(g, g.regions[j], 'GER').length)!;
const saved = (): string => TC.save().text;
const corpsIn = (text: string, id: number): Corps => loadGame(text)!.corps.find((x) => x.id === id)!;
const seen = (id: number): Seen => TC.view().corps.find((x) => x.id === id)!;
/** The rules' line for a garrison of one, in the first month and in later ones (garrisonLine, armies.ts). */
const told = (c: Corps, r: Region): string => `${c.name} is the garrison of ${townName(r)}. Order it to take the field first.`;
const fresh = (c: Corps, r: Region): string => `${c.name} is the garrison of ${townName(r)}. It went in this month, and can march out next month.`;

describe('find 17: a garrison asked to take ship is told it is a garrison, not that it has moved', () => {
  it('to a port of its own side and to an enemy coast; shut in, no ship leaves the siege; a corps that has marched is told so as before', () => {
    const g = playing();
    const hamburg = g.byName['Hamburg'], bremen = g.byName['Bremen'];
    const c = inside(g, hamburg, EARLIER);
    expect(c.ap).toBe(0);
    const take = 'A garrison does not take ship: TAKE THE FIELD first.';
    expect(whyNotShip(g, c, bremen)).toBe(take);
    const coast = g.regions.find((r) => r.ctrl === 'POL' && r.seas.length > 0)!;
    expect(whyNotLand(g, [c], coast)).toBe(take);
    // A corps in the field that has marched part of its month: the old reason, which is its own.
    const f = bring(g, hamburg);
    f.ap = 1;
    expect(whyNotShip(g, f, bremen)).toBe('Only a corps that has not moved this month can embark.');
    expect(whyNotLand(g, [f], coast)).toBe('Only corps that have not moved this month can embark.');
    // The 3D game's by-sea refusal passes it through.
    TC.load(saveGame(g));
    expect(TC.bySeaWhy([c.id], bremen.id).why).toBe(take);
    // Shut in by an army round the town, it cannot take the field either: it is told what the railway tells it.
    for (const u of hamburg.units.filter((u) => u.nat === 'GER')) relocate(g, u, bremen);
    const poles = g.regions.find((r) => r.ctrl === 'POL' && r.units.some((u) => u.nat === 'POL' && u.kind === 'inf'))!;
    relocate(g, poles.units.find((u) => u.nat === 'POL' && u.kind === 'inf')!, hamburg);
    expect(besieged(g, hamburg)).toBe(true);
    expect(whyNotShip(g, c, bremen)).toBe(`${c.name} is shut in the town at Hamburg. No ship leaves a siege.`);
  });
});

describe('find 16: a garrison in hand marches up and attacks as its field brothers do, and a refusal says why', () => {
  it('the march up and the attack is planned with the garrison as if out, and the game is left as it was', () => {
    const g = playing();
    const eifel = g.byName['Eifel'], lorraine = g.byName['Lorraine'];
    const c = inside(g, eifel, EARLIER);
    expect(eifel.nb.includes(lorraine.id)).toBe(false); // (not next door: the march up and the attack, as one order)
    TC.load(saveGame(g));
    const t0 = saved();
    const plan = TC.approach([c.id], lorraine.id);
    expect(plan.ok).toBe(true);
    expect(plan.rows.length).toBeGreaterThan(0);
    for (const row of plan.rows) expect(row.going).toEqual([c.id]);
    expect(saved()).toBe(t0);
    expect(corpsIn(saved(), c.id).town).toBe(EARLIER);
    // MARCH UP gives the march to the row's province (CarryOutApproach): the garrison comes out as it goes.
    TC.move([c.id], plan.rows[0].id);
    const out = corpsIn(saved(), c.id);
    expect(out.town).toBeUndefined();
    expect(out.reg).toBe(plan.rows[0].id);
  });

  it('a hand of a garrison and a field corps is planned whole: the garrison is not left out of the rows', () => {
    const g = playing();
    const eifel = g.byName['Eifel'], lorraine = g.byName['Lorraine'];
    const c = inside(g, eifel, EARLIER);
    const f = bring(g, eifel);
    TC.load(saveGame(g));
    const t0 = saved();
    const plan = TC.approach([c.id, f.id], lorraine.id);
    expect(plan.ok).toBe(true);
    expect(plan.rows[0].going.slice().sort()).toEqual([c.id, f.id].sort());
    expect(saved()).toBe(t0);
  });

  it('an order refused with nothing said says why, as the pointer does; nothing is spent, and a field corps is answered as before', () => {
    const g = playing();
    const saar = g.byName['Saar'];
    const c = inside(g, saar, EARLIER);
    for (const u of saar.units.filter((u) => u.nat === 'GER')) relocate(g, u, g.regions[home(g, saar)]);
    const foe = foeBy(g, saar);
    expect(foe).toBeDefined();
    TC.load(saveGame(g));
    const t0 = saved();
    const why = TC.garrisonWhy(saar.id).why;
    expect(why).toBe(told(c, saar));
    // A march onto the enemy army next door (that is an attack: the march is refused) and a march to its own ground.
    expect(TC.move([c.id], foe).note).toBe(why);
    expect(TC.move([c.id], saar.id).note).toBe(why);
    expect(saved()).toBe(t0);
    // A train: the railway's own reason, which the pointer gives (railTip).
    const ruhr = g.byName['Ruhr'].id;
    expect(TC.rail([c.id], ruhr).said).toBe('A garrison does not take the train: TAKE THE FIELD first.');
    expect(TC.rail([c.id], ruhr).said).toBe(TC.railTip([c.id], ruhr).no);
    expect(saved()).toBe(t0);
    // The same orders to a corps in the field are answered as they always were: a march that does nothing says nothing.
    const g2 = playing();
    const f = g2.byName['Saar'].units.find(goes)!;
    f.ap = 1; // (part of its month spent: no train)
    TC.load(saveGame(g2));
    expect(TC.move([f.id], f.reg).note).toBe('');
    expect(TC.rail([f.id], g2.byName['Ruhr'].id).said).toBe('');
    expect(TC.approach([f.id], g2.byName['Ruhr'].id)).toMatchObject({ ok: false, none: '' });
  });

  it('a garrison that cannot come out is refused the march up with the rules\' sentence', () => {
    const g = playing();
    const r = g.byName['Saar'];
    // Two corps inside, with room outside for one of them: neither comes out (comeOut, all or none).
    const a = inside(g, r, EARLIER);
    const b = inside(g, r, EARLIER);
    const fill = g.corps.filter((x) => goes(x) && x.reg !== r.id).slice(0, STACK - 1 - r.units.filter((u) => u.nat === 'GER').length);
    for (const x of fill) relocate(g, x, r);
    TC.load(saveGame(g));
    const t0 = saved();
    const plan = TC.approach([a.id, b.id], g.byName['Lorraine'].id);
    expect(plan.ok).toBe(false);
    expect(plan.none).toBe(`They are the garrison of ${townName(r)}. Order them to take the field first.`);
    expect(plan.none).toBe(TC.garrisonWhy(r.id).why);
    expect(saved()).toBe(t0);
  });
});

describe('find 18: a garrison that went in this month says when it can march, and is spent, on the eve of war too', () => {
  it('Warsaw Defence Command on the eve of war (Poland from September 1939, seed 31)', () => {
    TC.start({ view: 'grand', human: 'POL', from: 1, seed: 31 });
    const v = TC.view();
    expect(v.eve).toBe(true);
    const w = v.corps.find((x) => x.name === 'Warsaw Defence Command')!;
    expect(w.town).toBe(true);
    expect(w.ap).toBe(0);
    expect(w.apOut).toBe(0);
    const line = 'Warsaw Defence Command is the garrison of Warsaw. It went in this month, and can march out next month.';
    expect(w.garrison).toBe(line);
    expect(TC.garrisonWhy(w.reg).why).toBe(line);
    const t0 = saved();
    const hand = TC.reach([w.id]);
    expect(hand).toMatchObject({ regions: [], spent: true, why: line, out: [] });
    const near = v.corps.find((x) => x.nat === 'POL' && x.reg !== w.reg && !x.town)!;
    expect(TC.move([w.id], near.reg).note).toBe(line);
    expect(saved()).toBe(t0);
    // The army in the field beside it is placed on the eve as before: it reaches ground, and nothing is kept for next month.
    const field = v.corps.find((x) => x.nat === 'POL' && x.reg === w.reg && !x.town)!;
    for (const ids of [[field.id], [w.id, field.id]]) {
      const army = TC.reach(ids);
      expect(army.regions.length).toBeGreaterThan(0);
      expect(army).toMatchObject({ spent: false, why: '', out: [] });
    }
  });

  it('in a month of the war: one put in this month says so; one put in earlier comes out with its moves', () => {
    const g = playing();
    const eifel = g.byName['Eifel'];
    const c = inside(g, eifel, g.turn);
    TC.load(saveGame(g));
    expect(seen(c.id).garrison).toBe(fresh(c, eifel));
    expect(TC.garrisonWhy(eifel.id).why).toBe(fresh(c, eifel));
    expect(TC.reach([c.id])).toMatchObject({ regions: [], spent: true, why: fresh(c, eifel), out: [] });
    c.town = EARLIER;
    TC.load(saveGame(g));
    expect(seen(c.id).garrison).toBe(told(c, eifel));
    expect(TC.garrisonWhy(eifel.id).why).toBe(told(c, eifel));
    const hand = TC.reach([c.id]);
    expect(hand.regions.length).toBeGreaterThan(0);
    expect(hand).toMatchObject({ spent: false, why: '', out: [c.id] });
  });
});

describe('find 15: a garrison\'s card shows the moves it has once out', () => {
  it('all but one of its month; none if it went in this month or is shut in; its own moves stay 0, and a corps in the field has no such figure', () => {
    const g = playing();
    const saar = g.byName['Saar'];
    const c = inside(g, saar, EARLIER);
    const f = bring(g, saar);
    TC.load(saveGame(g));
    const t0 = saved();
    expect(seen(c.id)).toMatchObject({ town: true, ap: 0, apOut: c.apMax - 1 });
    expect('apOut' in seen(f.id)).toBe(false);
    expect(saved()).toBe(t0);
    // Out, it is a corps in the field: the figure goes, and its moves are what the march left it.
    TC.move([c.id], home(g, saar));
    const out = seen(c.id);
    expect(out.town).toBe(false);
    expect('apOut' in out).toBe(false);
    expect(out.ap).toBeLessThan(c.apMax - 1);
    // Gone in this month: none.
    c.town = g.turn;
    TC.load(saveGame(g));
    expect(seen(c.id)).toMatchObject({ ap: 0, apOut: 0 });
    // Shut in by an army round the town: none.
    c.town = EARLIER;
    for (const u of saar.units.filter((u) => u.nat === 'GER')) relocate(g, u, g.regions[home(g, saar)]);
    const foe = g.regions[foeBy(g, saar)];
    relocate(g, hostileUnits(g, foe, 'GER')[0], saar);
    expect(besieged(g, saar)).toBe(true);
    TC.load(saveGame(g));
    expect(seen(c.id)).toMatchObject({ ap: 0, apOut: 0 });
  });
});

describe('find 14: a hand of a garrison and a field army', () => {
  it('the reach names the corps of the garrison that come out with an order, all or none, and only with a move to make', () => {
    const g = playing();
    const eifel = g.byName['Eifel'];
    const c = inside(g, eifel, EARLIER);
    const f = bring(g, eifel);
    TC.load(saveGame(g));
    const t0 = saved();
    expect(TC.reach([c.id, f.id]).out).toEqual([c.id]);
    expect(TC.reach([c.id]).out).toEqual([c.id]);
    expect(TC.reach([f.id]).out).toEqual([]);
    expect(saved()).toBe(t0);
    // The order goes, and the garrison goes with the field corps.
    const to = home(g, eifel);
    TC.move([c.id, f.id], to);
    for (const id of [c.id, f.id]) {
      expect(corpsIn(saved(), id).reg).toBe(to);
      expect(corpsIn(saved(), id).town).toBeUndefined();
    }
    // Gone in this month, it has no move once out: it is not named, and the order goes without it.
    c.town = g.turn;
    TC.load(saveGame(g));
    expect(TC.reach([c.id, f.id])).toMatchObject({ out: [], why: '', spent: false });
  });

  it('two corps inside with room outside for one, beside a field corps: neither is named', () => {
    const g = playing();
    const r = g.byName['Saar'];
    const a = inside(g, r, EARLIER);
    const b = inside(g, r, EARLIER);
    const fill = g.corps.filter((x) => goes(x) && x.reg !== r.id).slice(0, STACK - 1 - r.units.filter((u) => u.nat === 'GER').length);
    for (const x of fill) relocate(g, x, r);
    const f = r.units.find(goes)!;
    TC.load(saveGame(g));
    expect(TC.reach([a.id, f.id]).out).toEqual([a.id]);
    expect(TC.reach([a.id, b.id, f.id]).out).toEqual([]);
  });
});
