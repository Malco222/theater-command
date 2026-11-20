// Bombers by name (campaign/bombers.ts; Brian, 20 November 2026): every bomber of a group keeps its name, its missions
// and its gunners' kills through the campaign; a crew goes home after its tour of 25, and the player's crews then lift
// their group, train new crews at the schools or sell war bonds. A computer nation's bombers keep their records and
// nothing else about its war changes.
import { afterEach, describe, expect, it } from 'vitest';
import { assessAir } from '../src/campaign/air';
import { BOMBERS, NOSE_ART, bomberCard, bomberName, bondsMonth, carriesNames, crewExperience, crewGrade, crewsHome, flownMonth, headStart, muster, setCrewOrder, trainedEdge } from '../src/campaign/bombers';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { loadGame, saveGame } from '../src/campaign/save';
import { STORIES, diaryText, storyAir, storyMonth, toursWords } from '../src/campaign/stories';
import { humanUp, orderCrews, runTurnAI } from '../src/campaign/turn';
import type { AirFleet, BomberRow, Game } from '../src/campaign/types';

const KEEP = JSON.parse(JSON.stringify(BOMBERS)) as typeof BOMBERS;
afterEach(() => { Object.assign(BOMBERS, JSON.parse(JSON.stringify(KEEP))); });

const NONE = { sky: false, sunk: 0, attLost: 0, defLost: 0 };
/** The grand campaign, where America has its bomber groups from the first month. */
const war = (seed: number, human: string | null): Game => startOpen(createGame(seed, human));
/** An American bomber command with no fighters of its own. */
const command = (g: Game): AirFleet => g.airFleets.find((f) => f.nat === 'USA' && f.bmr >= 60 && f.ftr === 0 && f.ship === undefined)!;
/** The group loses n bombers as the campaign books a loss. */
const lose = (f: AirFleet, n: number): void => { f.bmr -= n; f.lost.bmr += n; };
const named = (f: AirFleet): BomberRow[] => f.bombers!.on.filter((b) => b[0]);
const sum = (f: AirFleet, i: 2 | 3 | 5): number => f.bombers!.on.reduce((s, b) => s + b[i], 0);

describe('every bomber of a group has a name and a record', () => {
  it('a group is mustered as it stands: a row for every bomber, most with a name, no name twice', () => {
    const g = war(3, 'USA');
    const f = command(g);
    expect(f.bombers).toBeUndefined();
    expect(muster(g, f)).toEqual([]);
    const R = f.bombers!;
    expect(R.on).toHaveLength(f.bmr);
    const names = named(f).map(bomberName);
    expect(new Set(names).size).toBe(names.length);
    const bare = 1 - names.length / f.bmr;
    expect(bare).toBeGreaterThan(BOMBERS.bare / 3);
    expect(bare).toBeLessThan(BOMBERS.bare * 2);
    // A new group has flown nothing, and none of its names is a second of that name.
    expect(R.on.every((b) => b[2] === 0 && b[3] === 0 && b[4] === 0 && b[5] === 0)).toBe(true);
    expect(named(f).every((b) => b[1] === 1 && b[0] >= 1 && b[0] <= NOSE_ART)).toBe(true);
    expect(bomberName([0, 0, 0, 0, 0, 0])).toBe('');
    expect(R.seen).toBe(f.lost.bmr);
  });

  it('the same war musters the same bombers, another war others, and two groups paint different runs of names', () => {
    const a = war(3, 'USA'), b = war(3, 'USA'), c = war(4, 'USA');
    for (const g of [a, b, c]) for (const f of g.airFleets) muster(g, f);
    expect(command(a).bombers).toEqual(command(b).bombers);
    expect(command(a).bombers!.on.map((x) => x[0])).not.toEqual(command(c).bombers!.on.map((x) => x[0]));
    const two = a.airFleets.filter((f) => f.bombers);
    expect(two.length).toBeGreaterThan(1);
    expect(two[0].bombers!.on.slice(0, 20).map((x) => x[0])).not.toEqual(two[1].bombers!.on.slice(0, 20).map((x) => x[0]));
  });

  it('a carrier\'s aircraft carry no names', () => {
    const g = war(3, 'USA');
    const wing = g.airFleets.find((f) => f.nat === 'USA' && f.ship !== undefined && f.bmr > 0)!;
    expect(wing).toBeDefined();
    muster(g, wing);
    storyMonth(g, 'USA');
    expect(wing.bombers).toBeUndefined();
  });

  it('only a nation whose bombers carry names keeps them one by one', () => {
    const g = war(3, 'USA');
    expect(carriesNames('USA')).toBe(true);
    expect(carriesNames('GER')).toBe(false);
    const ger = g.airFleets.find((f) => f.nat === 'GER' && f.bmr > 0)!;
    muster(g, ger);
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [ger], [], NONE);
    expect(ger.bombers).toBeUndefined();
  });

  it('a group that has been flying is mustered with what its bombers have flown, and those have their names', () => {
    const g = war(3, 'USA');
    const f = command(g);
    f.rec = { missions: 15 };
    muster(g, f);
    expect(f.bombers!.on.every((b) => b[2] <= 15 && b[5] === b[2])).toBe(true);
    expect(f.bombers!.on.some((b) => b[2] >= 10)).toBe(true);
    expect(f.bombers!.on.filter((b) => b[2] > 0).every((b) => b[0] > 0)).toBe(true);
  });
});

describe('missions, kills and losses', () => {
  it('a month of bombing gives each bomber most of the month\'s missions, and never more', () => {
    const g = war(3, 'USA');
    const f = command(g);
    muster(g, f);
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], NONE);
    const per = STORIES.tour.perMonth;
    expect(f.rec!.missions).toBe(per);
    expect(f.bombers!.on.every((b) => b[2] >= 0 && b[2] <= per && b[5] === b[2])).toBe(true);
    const mean = sum(f, 2) / f.bmr;
    expect(mean).toBeGreaterThan(per * BOMBERS.fit - 0.5);
    expect(mean).toBeLessThan(per);
    // A fight for the sky is not a bombing mission.
    const before = sum(f, 2);
    storyAir(g, 'superiority', g.byName['Warsaw'].id, [f], [], NONE);
    expect(sum(f, 2)).toBe(before);
  });

  it('a crew names its bomber once it has flown', () => {
    const g = war(3, 'USA');
    const f = command(g);
    muster(g, f);
    expect(f.bombers!.on.some((b) => !b[0])).toBe(true);
    BOMBERS.fit = 1;
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], NONE);
    storyMonth(g, 'USA');
    expect(f.bombers!.on.every((b) => b[0] > 0)).toBe(true);
    const names = f.bombers!.on.map(bomberName);
    expect(new Set(names).size).toBe(names.length);
  });

  it('a bomber command\'s kills are its gunners\'; in a mixed fleet the gunners have the bombers\' share', () => {
    const g = war(3, 'USA');
    const f = command(g);
    muster(g, f);
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], { ...NONE, defLost: 30 });
    expect(f.rec!.kills).toBe(30);
    expect(sum(f, 3)).toBe(30);
    // A mixed fleet, as history lists an air force: fighters and bombers under one name.
    const mixed = g.airFleets.find((x) => x.nat === 'USA' && x.bmr >= 60 && x.ftr === 0 && x.ship === undefined && x !== f)!;
    mixed.ftr = 40;
    muster(g, mixed);
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [mixed], [], { ...NONE, defLost: 40 });
    const share = (mixed.bmr * BOMBERS.gunners) / (mixed.ftr + mixed.bmr * BOMBERS.gunners);
    expect(sum(mixed, 3)).toBe(Math.round(40 * share));
    expect(sum(mixed, 3)).toBeLessThan(40);
  });

  it('the bombers the campaign says were lost are struck off, and replacements join with no name', () => {
    const g = war(3, 'USA');
    const f = command(g);
    muster(g, f);
    const before = f.bombers!.on.map((b) => [...b]);
    lose(f, 7);
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], { ...NONE, attLost: 7 });
    const R = f.bombers!;
    expect(R.on).toHaveLength(f.bmr);
    expect(R.seen).toBe(f.lost.bmr);
    expect(R.gone).toBeGreaterThan(0);
    expect(R.gone).toBeLessThanOrEqual(7);
    // Made up to strength again, the new aircraft have no name, no missions and a green crew.
    f.bmr += 7;
    storyMonth(g, 'USA');
    expect(R.on).toHaveLength(before.length);
    const fresh = R.on.slice(-7);
    expect(fresh.every((b) => b[0] === 0 && b[2] === 0 && b[5] === 0)).toBe(true);
    expect(crewGrade(fresh[0])).toBe('Green');
  });

  it('a fought raid strikes off the very bombers the field saw fall', () => {
    const g = war(3, 'USA');
    const f = command(g);
    f.rec = { missions: 20 };
    muster(g, f);
    const fell = named(f).filter((_, i) => i === 3 || i === 11 || i === 40).map((b) => [...b] as BomberRow);
    expect(fell).toHaveLength(3);
    const names = fell.map(bomberName);
    expect(names.every(Boolean)).toBe(true);
    lose(f, 3);
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], { ...NONE, attLost: 3, fallen: { [f.id]: fell } });
    const left = f.bombers!.on.map(bomberName);
    for (const n of names) expect(left).not.toContain(n);
    expect(f.bombers!.on).toHaveLength(f.bmr);
    expect(f.bombers!.gone).toBe(3);
  });

  it('green crews are the likeliest to be lost', () => {
    const g = war(3, 'USA');
    const f = command(g);
    muster(g, f);
    f.bombers!.on.forEach((b, i) => { b[5] = i % 2 ? 20 : 0; b[2] = b[5]; });
    const green0 = f.bombers!.on.filter((b) => b[5] === 0).length;
    lose(f, Math.floor(f.bmr / 2));
    muster(g, f);
    const green = f.bombers!.on.filter((b) => b[5] === 0).length, old = f.bombers!.on.length - green;
    expect(green0 - green).toBeGreaterThan(f.bombers!.on.length / 2 - old + 4);
  });

  it('an old hand that does not come back is news for the player, and is on the group\'s roll', () => {
    const g = war(3, 'USA');
    const f = command(g);
    f.rec = { missions: 24 };
    muster(g, f);
    const old = [...f.bombers!.on].sort((a, b) => b[2] - a[2])[0];
    expect(old[2]).toBeGreaterThanOrEqual(BOMBERS.told);
    const name = bomberName(old), flown = old[2];
    lose(f, 1);
    storyAir(g, 'superiority', g.byName['Warsaw'].id, [f], [], { ...NONE, attLost: 1, fallen: { [f.id]: [[...old] as BomberRow] } });
    expect(g.headlines.at(-1)!.text).toBe(`${name} of ${f.name}, with ${flown} missions flown, did not come back from Warsaw.`);
    expect(f.bombers!.roll.at(-1)).toEqual({ t: g.turn, how: 'lost', b: old });
  });
});

/** Every crew of the group one mission short of its tour, and every bomber sure to fly. */
function onTheLast(g: Game, f: AirFleet): void {
  muster(g, f);
  BOMBERS.fit = 1;
  for (const b of f.bombers!.on) { b[5] = BOMBERS.tour - 1; b[2] = 30; b[3] = 2; if (!b[0]) b[0] = 0; }
  storyMonth(g, f.nat);
}

describe('the tour of duty', () => {
  it('a crew goes home after its twenty-fifth mission, and not before', () => {
    const g = war(3, 'USA');
    const f = command(g);
    muster(g, f);
    BOMBERS.fit = 1;
    f.bombers!.on.forEach((b, i) => { b[5] = i < 5 ? BOMBERS.tour - STORIES.tour.perMonth : 0; b[2] = b[5]; });
    setCrewOrder(g, 'school');
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], NONE);
    expect(f.bombers!.home).toBe(5);
    expect(f.rec!.tours).toBe(5);
    expect(f.bombers!.on.every((b) => b[5] < BOMBERS.tour)).toBe(true);
    expect(diaryText(g, f, f.log!.at(-1)!)).toBe('5 crews flew their 25th mission and went home.');
    expect(f.bombers!.roll.filter((x) => x.how === 'home')).toHaveLength(5);
  });

  it('for a computer nation the bomber flies on with a new crew, and nothing else changes', () => {
    const g = war(3, 'GER');
    const f = command(g);
    onTheLast(g, f);
    const names = f.bombers!.on.map(bomberName), ready = f.ready, pp = g.nations.USA.pp, news = g.headlines.length;
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], NONE);
    const R = f.bombers!;
    expect(R.home).toBe(f.bmr);
    expect(R.on.map(bomberName)).toEqual(names);
    expect(R.on.every((b) => b[5] === 0 && b[4] === 0 && b[2] === 31 && b[3] === 2)).toBe(true);
    expect(f.ready).toBe(ready);
    expect(g.nations.USA.pp).toBe(pp);
    expect(g.stories.crews).toBeUndefined();
    expect(g.headlines).toHaveLength(news);
    expect(trainedEdge(g, f)).toBe(0);
    expect(headStart(g, 'USA')).toBe(0);
  });

  it('the player\'s crews lift their group and go turn about to the schools and on bond tours', () => {
    const g = war(3, 'USA');
    const f = command(g);
    onTheLast(g, f);
    f.ready = 0.5;
    const n = f.bmr, names = f.bombers!.on.map(bomberName);
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], NONE);
    const C = crewsHome(g, 'USA'), S = BOMBERS.school, D = BOMBERS.bonds;
    // Turn about until each is full: the schools take 'crews' x 'most', the bond drive 'most' at once.
    expect(C.order).toBe('split');
    expect(C.n).toBe(n);
    expect(C.school).toBe(S.crews * S.most);
    expect(C.bonds).toHaveLength(D.most);
    expect(C.bonds.every((m) => m >= D.months[0] && m <= D.months[1])).toBe(true);
    // The group is in better heart: so much a crew, and no more than the most in a month.
    expect(f.ready).toBeCloseTo(0.5 + BOMBERS.lift.most, 9);
    // A crew on a bond tour flew its bomber home: a new aircraft with no name is in its place. The rest fly on.
    const R = f.bombers!;
    expect(R.on).toHaveLength(n);
    expect(R.on.filter((b) => !b[0])).toHaveLength(D.most);
    expect(R.on.filter((b) => b[0]).every((b) => names.includes(bomberName(b)) && b[2] === 31 && b[3] === 2 && b[5] === 0)).toBe(true);
    const line = g.headlines.at(-1)!.text;
    expect(line).toContain(`crews of ${f.name} have flown their 25th mission`);
    expect(line).toContain(`${S.crews * S.most} go to train new crews and ${D.most} go on a war bond tour, and ${n - S.crews * S.most - D.most} to other duties.`);
    expect(line.endsWith('The group is in better heart for it.')).toBe(true);
  });

  it('readiness never passes full, and a second month lifts it again', () => {
    const g = war(3, 'USA');
    const f = command(g);
    onTheLast(g, f);
    f.ready = 0.99;
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], NONE);
    expect(f.ready).toBe(1);
    expect(f.bombers!.lift).toEqual([g.turn, expect.closeTo(0.01, 9)]);
  });

  it('the standing order sends them all one way while there is room', () => {
    const g = war(3, 'USA');
    const f = command(g);
    muster(g, f);
    BOMBERS.fit = 1;
    f.bombers!.on.forEach((b, i) => { b[5] = i < 4 ? BOMBERS.tour - 1 : 0; });
    expect(orderCrews(g, 'school')).toBe(humanUp(g));
    expect(setCrewOrder(g, 'bonds')).toBe(true);
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], NONE);
    const C = crewsHome(g, 'USA');
    expect([C.school, C.bonds.length]).toEqual([0, 4]);
    expect(g.headlines.at(-1)!.text).toContain('Their tours are over and they are going home: 4 go on a war bond tour.');
    const h = war(3, 'GER');
    expect(setCrewOrder(h, 'bonds')).toBe(false);
  });

  it('the news of one crew names its bomber', () => {
    const one = (b: BomberRow, school: number, bonds: number, lift = 0) => toursWords('VIII Bomber Command', { done: [b], school, bonds, lift });
    const b: BomberRow = [62, 1, 25, 3, 0, 25];
    expect(one(b, 1, 0)).toBe('Prize Pumpkin of VIII Bomber Command has flown her 25th mission. Her crew has finished its tour and goes home to train new crews.');
    expect(one(b, 0, 1, 0.02)).toBe('Prize Pumpkin of VIII Bomber Command has flown her 25th mission. Her crew has finished its tour and goes home on a war bond tour, and takes her along. The group is in better heart for it.');
    expect(one([62, 2, 25, 0, 0, 25], 0, 0)).toBe('Prize Pumpkin II of VIII Bomber Command has flown her 25th mission. Her crew has finished its tour and goes home.');
    expect(one([0, 0, 25, 0, 0, 25], 1, 0)).toBe('A bomber of VIII Bomber Command has flown its 25th mission. Its crew has finished its tour and goes home to train new crews.');
    const three = toursWords('VIII Bomber Command', { done: [b, [19, 1, 40, 0, 0, 25], [187, 1, 25, 1, 0, 25], [1, 1, 25, 0, 0, 25], [2, 1, 25, 0, 0, 25]], school: 3, bonds: 2, lift: 0 });
    expect(three).toBe('Five crews of VIII Bomber Command have flown their 25th mission: Prize Pumpkin, Highball Pearl, Welcome Mat and 2 more. Their tours are over and they are going home: 3 go to train new crews and 2 go on a war bond tour.');
  });
});

describe('what the player\'s finished crews are worth', () => {
  it('a bond tour pays the treasury every month for five to ten months, and then it is over', () => {
    const g = war(3, 'USA');
    const C = crewsHome(g, 'USA');
    C.bonds = [5, 7, 10];
    const pp = g.nations.USA.pp;
    let paid = 0;
    for (let m = 0; m < 12; m++) paid += bondsMonth(g, 'USA');
    expect(paid).toBe(BOMBERS.bonds.pp * (5 + 7 + 10));
    expect(g.nations.USA.pp).toBeCloseTo(pp + paid, 9);
    expect(C.sold).toBe(paid);
    expect(C.bonds).toEqual([]);
    // It is paid when the nation's month is made up, and nobody else's crews pay anything.
    C.bonds = [3];
    const was = g.nations.USA.pp;
    expect(bondsMonth(g, 'GER')).toBe(0);
    const h = war(3, 'GER');
    crewsHome(h, 'USA').bonds = [3];
    expect(bondsMonth(h, 'USA')).toBe(0);
    expect(g.nations.USA.pp).toBe(was);
  });

  it('crews at the schools give every new bomber experience before its first mission', () => {
    const g = war(3, 'USA');
    const f = command(g);
    muster(g, f);
    const C = crewsHome(g, 'USA'), S = BOMBERS.school;
    expect(headStart(g, 'USA')).toBe(0);
    C.school = S.crews * 3;
    expect(headStart(g, 'USA')).toBe(3);
    lose(f, 10);
    muster(g, f);
    f.bmr += 10;
    muster(g, f);
    const fresh = f.bombers!.on.slice(-10);
    expect(fresh.every((b) => b[4] === 3 && b[5] === 0 && b[2] === 0 && crewExperience(b) === 3)).toBe(true);
    C.school = 999;
    expect(headStart(g, 'USA')).toBe(S.most);
    // What a crew arrives with is experience, not missions: its tour is still twenty-five of its own.
    expect(crewGrade([0, 0, 0, 0, 5, 0])).toBe('Trained');
    expect(crewGrade([0, 0, 0, 0, 0, 0])).toBe('Green');
    expect(crewGrade([1, 1, 24, 0, 0, 24])).toBe('Elite');
    BOMBERS.fit = 1;
    const b = f.bombers!.on.at(-1)!;
    b[4] = S.most; b[5] = BOMBERS.tour - STORIES.tour.perMonth - 1;
    flownMonth(g, f, 0);
    expect(f.bombers!.on).toContain(b);
    expect(b[5]).toBe(BOMBERS.tour - 1);
  });

  it('fighters bring down fewer of a group the schools trained, and the guns as many', () => {
    BOMBERS.names.GER = 'nose';
    const g = createGame(1, 'GER');
    const f = g.airFleets.find((x) => x.nat === 'GER' && x.bmr > 0)!;
    const t = g.byName['Warsaw'].id;
    muster(g, f);
    const plain = assessAir(g, [f], 'bomb', t);
    expect(plain.why).toBeNull();
    expect(plain.drilled).toBe(0);
    expect(plain.bomberLoss).toBeGreaterThan(plain.gunLoss);
    for (const b of f.bombers!.on) b[4] = BOMBERS.school.most;
    expect(trainedEdge(g, f)).toBeCloseTo(BOMBERS.school.edge, 9);
    const drilled = assessAir(g, [f], 'bomb', t);
    expect(drilled.drilled).toBeCloseTo(BOMBERS.school.edge, 9);
    expect(drilled.gunLoss).toBe(plain.gunLoss);
    expect(drilled.bomberLoss - drilled.gunLoss).toBeCloseTo((plain.bomberLoss - plain.gunLoss) * (1 - BOMBERS.school.edge), 9);
    // Half the crews trained half as well: a quarter of the edge. And never for a nation the player does not run.
    f.bombers!.on.forEach((b, i) => { b[4] = i % 2 ? BOMBERS.school.most / 2 : 0; });
    const half = f.bombers!.on.filter((_, i) => i % 2).length / f.bombers!.on.length;
    expect(trainedEdge(g, f)).toBeCloseTo((BOMBERS.school.edge * half) / 2, 9);
    g.human = 'UK';
    expect(trainedEdge(g, f)).toBe(0);
    expect(assessAir(g, [f], 'bomb', t).bomberLoss).toBe(plain.bomberLoss);
  });
});

describe('the record is kept', () => {
  it('a saved game keeps every bomber and the crews at home', () => {
    const g = war(3, 'USA');
    const f = command(g);
    onTheLast(g, f);
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], { ...NONE, defLost: 12 });
    const back = loadGame(saveGame(g))!;
    expect(back.airFleets.find((x) => x.id === f.id)!.bombers).toEqual(f.bombers);
    expect(back.stories.crews).toEqual(g.stories.crews);
  });

  it('the card lists the old hands first', () => {
    const g = war(3, 'USA');
    const f = command(g);
    f.rec = { missions: 20 };
    muster(g, f);
    const card = bomberCard(f);
    expect(card).toHaveLength(f.bmr);
    for (let i = 1; i < card.length; i++) expect(card[i - 1].missions).toBeGreaterThanOrEqual(card[i].missions);
    expect(card[0].name).toBe(bomberName(card[0].b));
  });

  it('a computer-run war is the same war with the bombers kept by name and without', () => {
    const state = (g: Game): string => JSON.stringify({ ...g, airFleets: g.airFleets.map((f) => ({ ...f, bombers: undefined })) });
    const a = war(5, null);
    for (let t = 0; t < 6; t++) runTurnAI(a);
    expect(a.airFleets.some((f) => f.nat === 'USA' && f.bombers && f.bombers.on.length === f.bmr)).toBe(true);
    BOMBERS.names = {};
    const b = war(5, null);
    for (let t = 0; t < 6; t++) runTurnAI(b);
    expect(b.airFleets.some((f) => f.bombers)).toBe(false);
    expect(state(a)).toBe(state(b));
  });
});
