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
// The two reviews of the patch: a refusal names the garrison only where taking the field is what stands in the way (a
// garrison that could come out and is sent onto the enemy next door is answered as a field corps is); the sentence for
// one that went in this month is the rules' own (garrisonLine), so the corps' card, the province and both games say
// the same; and the reach says why a corps of the garrison beside the field army stays inside (`stays`), so that the
// 3D game takes it out of the hand and says so, never leaving it behind without a word.
// The third review: a garrison is sent to TAKE THE FIELD for a ship or a train only where that would put it aboard this
// month (coming out costs a move, and both take a corps' whole month): any other is told it can go out of the town
// next month, and one in a town with no port is told so. Beside the army in the field, the train, the voyage and the
// landing are the field army's, and the garrison is named as it stays. On the eve of war a corps brought out of its
// town has its whole month when the war opens, so Warsaw is told to take the field. Where the field outside is full,
// the sentence gives that reason. And the 3D game asks the sentence for the corps in hand, not for the whole town.
// The fourth review: the province's own railway and harbour are answered before the garrison's way out. A garrison in
// the desert or off the railway is told there is no railway there, one whose railway is cut is told so, and one whose
// harbour is wrecked is told for how long, never that out of the town it can ride or embark.
import { describe, expect, it } from 'vitest';
import { allied, createGame, hostileUnits, relocate, whyNoTrain } from '../src/campaign/game';
import { whyNotLand, whyNotShip } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { besieged, garrison, takeField } from '../src/campaign/siege';
import { wreckPrize } from '../src/campaign/prizes';
import { cutRail } from '../src/campaign/rails';
import { garrisonLine, whyNotOrder, whyNotOrderGarrison } from '../src/campaign/armies';
import { startOpen } from '../src/campaign/staff';
import { townName } from '../src/campaign/towns';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import { STACK, type Corps, type Game, type Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Seen = { id: number; name: string; reg: number; town?: boolean; ap: number; apMax: number; apOut?: number; garrison?: string; noOrders?: string; nat: string };
type Row = { id: number; name: string; going: number[]; left: number };
interface Bridge {
  start(o: unknown): unknown;
  load(text: string): unknown;
  save(): { text: string };
  view(): { eve: boolean; turn: number; corps: Seen[] };
  reach(ids: number[]): { regions: { id: number; attack: boolean }[]; spent: boolean; why: string; out: number[]; stays: string };
  garrisonWhy(region: number, ids?: number[]): { why: string };
  move(ids: number[], to: number): { moved: { id: number; path: number[] }[]; note: string };
  rail(ids: number[], to: number): { moved: { id: number }[]; said: string };
  railReach(ids: number[]): { regions: number[] };
  seaReach(ids: number[]): { ports: number[]; beaches: number[] };
  ship(ids: number[], to: number): { moved: { id: number }[] };
  railTip(ids: number[], to: number): { no: string; ride: string };
  railShow(ids: number[], march: number[]): { can: boolean; line: string; tint: number[] };
  landingPlan(ids: number[], to: number): { ok: boolean; why: string };
  landOn(ids: number[], to: number): { ok: boolean; moved: { id: number }[] };
  work(action: string): { ok: boolean; note: string };
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
/** What a garrison is told of a ship and of a train where taking the field would not put it aboard this month (the third review). */
const NO_SHIP = 'A garrison does not take ship. Out of the town, it can embark next month.';
const NO_TRAIN = 'A garrison does not take the train. Out of the town, it can go by rail next month.';

describe('find 17: a garrison asked to take ship is told it is a garrison, not that it has moved', () => {
  it('to a port of its own side and to an enemy coast; shut in, no ship leaves the siege; a corps that has marched is told so as before', () => {
    const g = playing();
    const hamburg = g.byName['Hamburg'], bremen = g.byName['Bremen'];
    const c = inside(g, hamburg, EARLIER);
    expect(c.ap).toBe(0);
    const take = NO_SHIP;
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

  it('(the third review) the advice gets it aboard: TAKE THE FIELD only where that does this month, else next month out of the town; no port, no ship', () => {
    const g = playing();
    const hamburg = g.byName['Hamburg'], bremen = g.byName['Bremen'], saar = g.byName['Saar'];
    const c = inside(g, hamburg, EARLIER);
    const d = inside(g, hamburg, EARLIER);
    const s = inside(g, saar, EARLIER);
    expect(c.apMax).toBe(4);
    expect(whyNotShip(g, c, bremen)).toBe(NO_SHIP);
    expect(whyNoTrain(g, c)!.line).toBe(NO_TRAIN);
    // Taken out of the town it has spent a move, and a ship and a train take a whole month: neither takes it this month.
    expect(takeField(g, c)).toBe(true);
    expect(c.ap).toBe(c.apMax - 1);
    expect(whyNotShip(g, c, bremen)).toBe('Only a corps that has not moved this month can embark.');
    expect(whyNoTrain(g, c)!.code).toBe('spent');
    // As the next month opens a corps in the field has its whole month: it embarks, and it rides.
    c.ap = c.apMax;
    const port = g.regions.find((r) => r.port && r.id !== hamburg.id && whyNotShip(g, c, r) === null)!;
    expect(port).toBeDefined();
    expect(whyNoTrain(g, c)).toBeNull();
    // A corps with a month longer than a train's still has a whole one once out: it is told to take the field, and
    // that gets it aboard this month.
    d.apMax = 5;
    expect(whyNotShip(g, d, port)).toBe('A garrison does not take ship: TAKE THE FIELD first.');
    expect(whyNoTrain(g, d)!.line).toBe('A garrison does not take the train: TAKE THE FIELD first.');
    expect(takeField(g, d)).toBe(true);
    expect(whyNotShip(g, d, port)).toBeNull();
    expect(whyNoTrain(g, d)).toBeNull();
    // One that went in this month has no move once out, however long its month.
    const g2 = playing();
    const e = inside(g2, g2.byName['Hamburg'], g2.turn);
    e.apMax = 5;
    expect(whyNotShip(g2, e, g2.regions[port.id])).toBe(NO_SHIP);
    expect(whyNoTrain(g2, e)!.line).toBe(NO_TRAIN);
    // A garrison in a town with no port is told so, as a landing force is.
    expect(whyNotShip(g, s, bremen)).toBe('The Saar has no port: troops are shipped from one port to another.');
    TC.load(saveGame(g));
    expect(TC.bySeaWhy([s.id], saar.id).why).toBe('The Saar has no port: troops are shipped from one port to another.');
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
    // A march to its own ground: taking the field is the way out of the town.
    expect(TC.move([c.id], saar.id).note).toBe(why);
    // (The review.) A march onto the enemy army next door is an attack, and is refused as such: the garrison could come
    // out and get there, so nothing stands in its way but what stands in a field army's, and it is answered as a field
    // army is. The pointer, too, says "Right-click: attack" there, not the garrison's line (ComesOutTo).
    expect(TC.reach([c.id]).regions.find((r) => r.id === foe)).toMatchObject({ attack: true });
    expect(TC.move([c.id], foe).note).toBe('');
    expect(TC.approach([c.id], foe)).toMatchObject({ ok: false, none: '' });
    expect(saved()).toBe(t0);
    // A train: the railway's own reason, which the pointer gives (railTip).
    const ruhr = g.byName['Ruhr'].id;
    expect(TC.rail([c.id], ruhr).said).toBe(NO_TRAIN);
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
    expect(TC.move([f.id], foeBy(g2, g2.byName['Saar'])).note).toBe('');
  });

  it('a garrison that could not come out with a move is told why wherever it is sent, the enemy next door too', () => {
    const g = playing();
    const saar = g.byName['Saar'];
    const c = inside(g, saar, g.turn); // (went in this month: no move once out)
    for (const u of saar.units.filter((u) => u.nat === 'GER')) relocate(g, u, g.regions[home(g, saar)]);
    const foe = foeBy(g, saar);
    TC.load(saveGame(g));
    const t0 = saved();
    const why = fresh(c, saar);
    expect(TC.garrisonWhy(saar.id).why).toBe(why);
    expect(TC.move([c.id], foe).note).toBe(why);
    expect(TC.approach([c.id], foe)).toMatchObject({ ok: false, rows: [], none: why });
    expect(saved()).toBe(t0);
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

describe('find 18: a garrison that went in this month says when it can march, and is spent; on the eve of war it is told to take the field', () => {
  it('Warsaw Defence Command on the eve of war (Poland from September 1939, seed 31)', () => {
    TC.start({ view: 'grand', human: 'POL', from: 1, seed: 31 });
    const v = TC.view();
    expect(v.eve).toBe(true);
    const w = v.corps.find((x) => x.name === 'Warsaw Defence Command')!;
    expect(w.town).toBe(true);
    expect(w.ap).toBe(0);
    expect(w.apOut).toBe(0);
    // (The third review.) Brought out on the eve, it has its whole month when the war opens, as every corps in the
    // field has (startPhase): it is told to take the field, as in 1.5.2, and is not spent, as no hand is on the eve.
    const line = 'Warsaw Defence Command is the garrison of Warsaw. Order it to take the field first.';
    expect(w.garrison).toBe(line);
    expect(TC.garrisonWhy(w.reg).why).toBe(line);
    expect(TC.garrisonWhy(w.reg, [w.id]).why).toBe(line);
    // (The review.) The corps' card in both games says the same: the view's noOrders is the rules' whyNotOrder.
    expect(w.noOrders).toBe(line);
    const t0 = saved();
    const hand = TC.reach([w.id]);
    expect(hand).toMatchObject({ regions: [], spent: false, why: line, out: [] });
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
    // So with the train: brought out now, it rides when the war opens.
    expect(TC.railTip([w.id], near.reg).no).toBe('A garrison does not take the train: TAKE THE FIELD first.');
    expect(saved()).toBe(t0);
    // The order it is told of is there to be given on the eve.
    expect(TC.work(`field:${w.id}`)).toMatchObject({ ok: true });
    expect(TC.view().corps.find((x) => x.id === w.id)).toMatchObject({ town: false, reg: w.reg });
  });

  it('in a month of the war: one put in this month says so; one put in earlier comes out with its moves', () => {
    const g = playing();
    const eifel = g.byName['Eifel'];
    const c = inside(g, eifel, g.turn);
    // The rules' own sentence, for the corps' card in the browser and the province's paper line (the review).
    expect(whyNotOrder(g, c)).toBe(fresh(c, eifel));
    expect(whyNotOrderGarrison(g, eifel.id)).toBe(fresh(c, eifel));
    expect(garrisonLine(g, [c])).toBe(fresh(c, eifel));
    TC.load(saveGame(g));
    expect(seen(c.id).garrison).toBe(fresh(c, eifel));
    expect(seen(c.id).noOrders).toBe(fresh(c, eifel));
    expect(TC.garrisonWhy(eifel.id).why).toBe(fresh(c, eifel));
    expect(TC.reach([c.id])).toMatchObject({ regions: [], spent: true, why: fresh(c, eifel), out: [] });
    c.town = EARLIER;
    expect(whyNotOrder(g, c)).toBe(told(c, eifel));
    TC.load(saveGame(g));
    expect(seen(c.id).garrison).toBe(told(c, eifel));
    expect(seen(c.id).noOrders).toBe(told(c, eifel));
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
    expect(TC.reach([c.id, f.id])).toMatchObject({ out: [c.id], stays: '' });
    expect(TC.reach([c.id])).toMatchObject({ out: [c.id], stays: '' });
    expect(TC.reach([f.id])).toMatchObject({ out: [], stays: '' });
    expect(saved()).toBe(t0);
    // The order goes, and the garrison goes with the field corps.
    const to = home(g, eifel);
    TC.move([c.id, f.id], to);
    for (const id of [c.id, f.id]) {
      expect(corpsIn(saved(), id).reg).toBe(to);
      expect(corpsIn(saved(), id).town).toBeUndefined();
    }
    // Gone in this month, it has no move once out: it is not named, and the order goes without it. The reach says why
    // it stays, in the rules' sentence, so that the 3D game takes it out of the hand and says so.
    c.town = g.turn;
    TC.load(saveGame(g));
    const t1 = saved();
    expect(TC.reach([c.id, f.id])).toMatchObject({ out: [], why: '', spent: false, stays: fresh(c, eifel) });
    expect(saved()).toBe(t1);
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
    const t0 = saved();
    expect(TC.reach([a.id, f.id])).toMatchObject({ out: [a.id], stays: '' });
    expect(TC.reach([b.id, f.id])).toMatchObject({ out: [b.id], stays: '' });
    // (The review.) Both clicked in: neither comes out, and the reach says why, in the rules' own words (whyNotTakeField),
    // so that the 3D game takes the last of them out of the hand and the other comes out with the field corps.
    expect(TC.reach([f.id, a.id, b.id])).toMatchObject({ out: [], stays: `There is no room in the field at ${r.name} for another corps.` });
    expect(saved()).toBe(t0);
    // The order would have gone without them: the field corps alone marches.
    const to = home(g, r);
    TC.move([f.id, a.id, b.id], to);
    expect(corpsIn(saved(), a.id).town).toBe(EARLIER);
    expect(corpsIn(saved(), b.id).town).toBe(EARLIER);
    expect(corpsIn(saved(), f.id).reg).toBe(to);
    // A hand that is all garrison is told by its own line (why), not by `stays`.
    TC.load(saveGame(g));
    expect(TC.reach([a.id, b.id]).stays).toBe('');
  });
});

describe('the review: a garrison beside the field army sent by rail or by sea stays in its town, and is named', () => {
  it('by rail: the field corps rides, and the answer names the garrison that stays, with the railway\'s reason, not that there is no room', () => {
    const g = playing();
    const saar = g.byName['Saar'];
    const c = inside(g, saar, EARLIER);
    const f = bring(g, saar);
    f.ap = f.apMax;
    TC.load(saveGame(g));
    const to = TC.railReach([f.id]).regions.find((id) => id !== saar.id)!;
    expect(to).toBeDefined();
    const went = TC.rail([f.id, c.id], to);
    expect(went.moved.map((m) => m.id)).toEqual([f.id]);
    expect(went.said).toMatch(/ by rail: there this month, the whole month spent\. /);
    expect(went.said).not.toMatch(/has no room for more/);
    expect(went.said.endsWith(`${c.name} stays in the town. ${NO_TRAIN}`)).toBe(true);
    expect(corpsIn(saved(), c.id).town).toBe(EARLIER);
  });

  it('(the third review) the railway is offered to the field army as if the garrison were not in hand, and names the garrison as it stays', () => {
    const g = playing();
    const eifel = g.byName['Eifel'];
    const c = inside(g, eifel, EARLIER);
    const f = bring(g, eifel);
    f.ap = f.apMax;
    TC.load(saveGame(g));
    const t0 = saved();
    const alone = TC.railShow([f.id], []), both = TC.railShow([f.id, c.id], []);
    expect(alone.can).toBe(true);
    expect(alone.tint.length).toBeGreaterThan(0);
    // The station dots stay lit: a right-click beyond the march is the field army's train, not a long march for all.
    expect(both).toMatchObject({ can: true, tint: alone.tint });
    expect(both.line).toBe(`${alone.line} ${c.name} stays in the town. ${NO_TRAIN}`);
    const to = alone.tint[0];
    expect(TC.railTip([f.id, c.id], to).ride).toBe(`${TC.railTip([f.id], to).ride} ${c.name} stays in the town.`);
    // A hand that is all garrison is judged whole, as before.
    expect(TC.railShow([c.id], [])).toMatchObject({ can: false, line: NO_TRAIN });
    expect(saved()).toBe(t0);
    // Where the field army cannot ride either, the line is its own reason.
    f.ap = 1;
    TC.load(saveGame(g));
    expect(TC.railShow([f.id, c.id], [])).toMatchObject({ can: false, line: TC.railShow([f.id], []).line });
  });

  it('(the third review) by sea the landing is the field army\'s: the coast stays offered, the garrison stays in its town', () => {
    const g = playing();
    const hamburg = g.byName['Hamburg'];
    const c = inside(g, hamburg, EARLIER);
    const f = bring(g, hamburg);
    f.ap = f.apMax;
    TC.load(saveGame(g));
    const t0 = saved();
    const alone = TC.seaReach([f.id]), both = TC.seaReach([f.id, c.id]);
    expect(alone.beaches.length).toBeGreaterThan(0);
    expect(both).toEqual(alone);
    const beach = alone.beaches[0];
    expect(TC.landingPlan([f.id, c.id], beach)).toMatchObject({ ok: true, why: '' });
    // Asked of a coast the offer leaves out, the field army's own reason, not the garrison's.
    const other = g.regions.find((r) => r.seas.length > 0 && !alone.beaches.includes(r.id) && r.ctrl !== 'GER' && !allied(g, 'GER', r.ctrl))!;
    expect(TC.bySeaWhy([f.id, c.id], other.id).why).toBe(TC.bySeaWhy([f.id], other.id).why);
    expect(saved()).toBe(t0);
    // The landing goes, and the garrison is not in it.
    const landed = TC.landOn([f.id, c.id], beach);
    expect(landed.ok).toBe(true);
    expect(landed.moved.map((m) => m.id)).not.toContain(c.id);
    expect(corpsIn(saved(), c.id)).toMatchObject({ reg: hamburg.id, town: EARLIER });
  });

  it('by sea: asked of its own province, the rules give a garrison its own reason (the 3D game\'s GarrisonAboard); a ship takes the field corps alone', () => {
    const g = playing();
    const hamburg = g.byName['Hamburg'];
    const c = inside(g, hamburg, EARLIER);
    const f = bring(g, hamburg);
    f.ap = f.apMax;
    TC.load(saveGame(g));
    expect(TC.bySeaWhy([c.id], hamburg.id).why).toBe(NO_SHIP);
    const port = TC.seaReach([f.id, c.id]).ports[0];
    expect(port).toBeDefined();
    expect(TC.ship([f.id, c.id], port).moved.map((m) => m.id)).toEqual([f.id]);
    expect(corpsIn(saved(), c.id)).toMatchObject({ reg: hamburg.id, town: EARLIER });
  });
});

describe('the third review: the sentence of the hand, and of a town whose field is full', () => {
  it('a corps that went in this month beside an older garrison is told its own sentence, not the town\'s', () => {
    const g = playing();
    const eifel = g.byName['Eifel'];
    const old = inside(g, eifel, EARLIER);
    const now = inside(g, eifel, g.turn);
    TC.load(saveGame(g));
    const t0 = saved();
    const town = townName(eifel);
    // The town's sentence, for the province's paper line, is for them all.
    expect(TC.garrisonWhy(eifel.id).why).toBe(`They are the garrison of ${town}. Order them to take the field first.`);
    // The hand's, for the 3D game's tray and pointer (GarrisonSays), is the corps' own, as its card says.
    const mine = `${now.name} is in the garrison of ${town}. It went in this month, and can march out next month.`;
    expect(TC.garrisonWhy(eifel.id, [now.id]).why).toBe(mine);
    expect(seen(now.id).garrison).toBe(mine);
    expect(TC.reach([now.id]).why).toBe(mine);
    expect(TC.garrisonWhy(eifel.id, [old.id]).why).toBe(`${old.name} is in the garrison of ${town}. Order it to take the field first.`);
    // Both in hand: only the older one comes out with a move (the 3D game takes the other out of the hand: LeftInside).
    expect(TC.reach([old.id, now.id])).toMatchObject({ out: [old.id], why: '' });
    expect(saved()).toBe(t0);
  });

  it('where the field outside is full, a garrison is told so, whenever it went in, and TAKE THE FIELD is refused with the same reason', () => {
    const g = playing();
    const saar = g.byName['Saar'];
    const now = inside(g, saar, g.turn);
    const ours = (): number => saar.units.filter((u) => allied(g, u.nat, 'GER') && u.kind !== 'eng').length;
    for (const x of g.corps.filter((x) => goes(x) && x.reg !== saar.id)) {
      if (ours() >= STACK) break;
      relocate(g, x, saar);
    }
    expect(ours()).toBe(STACK);
    const full = `There is no room in the field at ${saar.name} for another corps.`;
    const line = `${now.name} is the garrison of ${townName(saar)}. ${full}`;
    expect(whyNotOrder(g, now)).toBe(line);
    expect(whyNotOrderGarrison(g, saar.id)).toBe(line);
    TC.load(saveGame(g));
    expect(seen(now.id)).toMatchObject({ garrison: line, apOut: 0 });
    expect(TC.garrisonWhy(saar.id).why).toBe(line);
    expect(TC.reach([now.id]).why).toBe(line);
    expect(TC.work(`field:${now.id}`)).toMatchObject({ ok: false, note: full });
    // One that went in a month before is told the same.
    now.town = EARLIER;
    expect(whyNotOrder(g, now)).toBe(line);
    TC.load(saveGame(g));
    expect(TC.reach([now.id]).why).toBe(line);
  });
});

describe('the fourth review: the province\'s own railway and harbour come first', () => {
  /** An Italian infantry corps of a province put inside its town, as having gone in a month before. */
  const italian = (g: Game, at: Region): Corps => {
    const c = at.units.find((u) => u.nat === 'ITA' && u.kind === 'inf' && !u.dead)!;
    expect(garrison(g, c)).toBe(true);
    c.town = EARLIER;
    return c;
  };

  it('a garrison in the desert or off the railway is told there is none there, as out of the town (Italy, seed 3: Tobruk, Sardinia)', () => {
    const g = playing('ITA', 3);
    const tobruk = g.byName['Tobruk'], sardinia = g.byName['Sardinia'], rome = g.byName['Rome'];
    const desert = 'No railway crosses the desert.';
    const t = italian(g, tobruk);
    expect(whyNoTrain(g, t)).toEqual({ code: 'desert', line: desert, at: tobruk.id });
    // A month longer than a train's would once have sent it to TAKE THE FIELD, for a train it would never get.
    t.apMax = 5;
    expect(whyNoTrain(g, t)!.line).toBe(desert);
    const s = italian(g, sardinia);
    const road = `No railway runs through Sardinia: ${s.name} goes by road.`;
    expect(whyNoTrain(g, s)).toEqual({ code: 'noline', line: road, at: sardinia.id });
    // The 3D game's order by rail answers with the same line (railTip).
    TC.load(saveGame(g));
    expect(TC.rail([t.id], rome.id).said).toBe(desert);
    expect(TC.rail([s.id], rome.id).said).toBe(road);
    // Out of the town, with a whole month a month on, each is refused for the same reason: the line named the real block.
    for (const c of [t, s]) {
      expect(takeField(g, c)).toBe(true);
      c.ap = c.apMax;
    }
    expect(whyNoTrain(g, t)!.line).toBe(desert);
    expect(whyNoTrain(g, s)!.line).toBe(road);
  });

  it('a garrison whose railway is cut is told so, and is not sent to take the field for a train that does not leave', () => {
    const g = playing();
    const hamburg = g.byName['Hamburg'];
    const c = inside(g, hamburg, EARLIER);
    c.apMax = 5;
    expect(whyNoTrain(g, c)!.line).toBe('A garrison does not take the train: TAKE THE FIELD first.');
    expect(cutRail(g, hamburg.id, 'bombed')).toBe(true);
    const cut = whyNoTrain(g, c)!;
    expect(cut).toMatchObject({ code: 'cut', at: hamburg.id });
    expect(cut.line).toMatch(/^The railway at Hamburg is cut \(bombed, [^)]+\)\. No train leaves until engineers relay the track\.$/);
    expect(takeField(g, c)).toBe(true);
    c.ap = c.apMax;
    expect(whyNoTrain(g, c)!.line).toBe(cut.line);
  });

  it('a garrison whose harbour is wrecked is told so and for how long, never that it can embark next month', () => {
    const g = playing();
    const hamburg = g.byName['Hamburg'], bremen = g.byName['Bremen'];
    const c = inside(g, hamburg, EARLIER);
    const d = inside(g, hamburg, EARLIER);
    d.apMax = 5;
    expect(whyNotShip(g, c, bremen)).toBe(NO_SHIP);
    expect(wreckPrize(g, hamburg, 'harbour', 3)).toBe(true);
    const wrecked = 'The harbour of Hamburg is wrecked: no ship will use it for 3 months more.';
    expect(whyNotShip(g, c, bremen)).toBe(wrecked);
    expect(whyNotShip(g, d, bremen)).toBe(wrecked);
    // The 3D game's BY SEA notice asks of the garrison's own province (GarrisonAboard).
    TC.load(saveGame(g));
    expect(TC.bySeaWhy([c.id], hamburg.id).why).toBe(wrecked);
    // Out of the town with its whole month, the harbour is what keeps it ashore.
    expect(takeField(g, d)).toBe(true);
    expect(d.ap).toBe(4);
    expect(whyNotShip(g, d, bremen)).toBe(wrecked);
  });
});
