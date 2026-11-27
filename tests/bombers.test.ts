// Bombers by name (campaign/bombers.ts; Brian, 20 November 2026): every bomber of a group keeps its name, its missions
// and its gunners' kills through the campaign; a crew goes home after its tour of 25, and the player's crews then lift
// their group, train new crews at the schools or sell war bonds. A computer nation's bombers keep their records and
// nothing else about its war changes.
import { afterEach, describe, expect, it } from 'vitest';
import { assessAir } from '../src/campaign/air';
import { markOf } from '../src/campaign/bomber-marks';
import { BOMBERS, NOSE_ART, bomberCard, bomberMark, bomberName, bondsMonth, carriesNames, crewExperience, crewGrade, crewsHome, flownMonth, flownWord, countWord, headStart, keptByName, muster, setCrewOrder, styleOf, theUnit, thePlace, tourOf, trainedEdge, type ToursDone } from '../src/campaign/bombers';
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
/** What each bomber of a group is called. */
const called = (g: Game, f: AirFleet) => (b: BomberRow): string => bomberName(g, f, b);
const TOUR = tourOf('USA');

describe('every bomber of a group has a name and a record', () => {
  it('a group is mustered as it stands: a row for every bomber, most with a name, no name twice', () => {
    const g = war(3, 'USA');
    const f = command(g);
    expect(f.bombers).toBeUndefined();
    expect(muster(g, f)).toEqual([]);
    const R = f.bombers!;
    expect(R.on).toHaveLength(f.bmr);
    const names = named(f).map(called(g, f));
    expect(new Set(names).size).toBe(names.length);
    const bare = 1 - names.length / f.bmr;
    expect(bare).toBeGreaterThan(BOMBERS.bare / 3);
    expect(bare).toBeLessThan(BOMBERS.bare * 2);
    // A new group has flown nothing, and none of its names is a second of that name.
    expect(R.on.every((b) => b[2] === 0 && b[3] === 0 && b[4] === 0 && b[5] === 0)).toBe(true);
    expect(named(f).every((b) => b[1] === 1 && b[0] >= 1 && b[0] <= NOSE_ART)).toBe(true);
    expect(bomberName(g, f, [0, 0, 0, 0, 0, 0])).toBe('');
    expect(R.seen).toBe(f.lost.bmr);
  });

  it('the same war musters the same bombers, another war others, and two groups paint different runs of names', () => {
    const a = war(3, 'USA'), b = war(3, 'USA'), c = war(4, 'USA');
    for (const g of [a, b, c]) for (const f of g.airFleets) muster(g, f);
    expect(command(a).bombers).toEqual(command(b).bombers);
    expect(command(a).bombers!.on.map((x) => x[0])).not.toEqual(command(c).bombers!.on.map((x) => x[0]));
    const two = a.airFleets.filter((f) => f.bombers && f.nat === 'USA');
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

  it('every nation keeps its bombers one by one, and only America paints names on them', () => {
    const g = war(3, 'USA');
    expect(carriesNames('USA')).toBe(true);
    for (const nat of ['GER', 'UK', 'ITA', 'SOV', 'JAP', 'FRA']) expect(carriesNames(nat)).toBe(false);
    expect(['USA', 'UK', 'CAN', 'GER', 'JAP', 'ITA', 'SOV', 'FRA'].map(styleOf)).toEqual(['nose', 'letter', 'letter', 'code', 'tail', 'squadron', 'tactical', 'number']);
    const ger = g.airFleets.find((f) => f.nat === 'GER' && f.bmr > 0 && keptByName(f))!;
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [ger], [], NONE);
    const R = ger.bombers!;
    expect(R.on).toHaveLength(ger.bmr);
    // Every German bomber has its unit code from the day it is mustered, and no two the same.
    const marks = R.on.map((b) => bomberMark(g, ger, b));
    expect(marks.every((m) => /^[0-9A-Z]{2}\+[A-N][H-W]$/.test(m.mark) && m.name === m.mark && !!m.word)).toBe(true);
    expect(new Set(marks.map((m) => m.mark)).size).toBe(marks.length);
    expect(R.on.every((b) => b[2] > 0)).toBe(true);
  });

  it('each air force marks its bombers its own way', () => {
    const key = '3:17';
    // Britain: the squadron's two letters and the aircraft's own, called by the word of the day for its letter.
    expect(markOf('letter', key, 7, 1, 1941)).toEqual({ name: 'G for George', mark: expect.stringMatching(/^[A-Z]{2}-G$/), word: 'George' });
    expect(markOf('letter', key, 2, 1, 1942).name).toBe('B for Beer');
    expect(markOf('letter', key, 2, 1, 1943).name).toBe('B for Baker');
    expect(markOf('letter', key, 2, 1, 1942).mark).toBe(markOf('letter', key, 2, 1, 1943).mark);
    // The twenty-seventh aircraft is another squadron's A, and no squadron carries the letters of the two famous Lancasters.
    const a1 = markOf('letter', key, 1).mark, a2 = markOf('letter', key, 27).mark;
    expect(a1.slice(-1)).toBe('A');
    expect(a2.slice(-1)).toBe('A');
    expect(a1).not.toBe(a2);
    for (let s = 0; s < 600; s++) expect(['PO', 'AR']).not.toContain(markOf('letter', key, 1 + 26 * s).mark.slice(0, 2));
    // Germany: the wing's code, the aircraft's letter and its squadron's, and the radio word for the letter.
    expect(markOf('code', key, 4)).toEqual({ name: expect.stringMatching(/^[0-9A-Z]{2}\+DH$/), mark: expect.stringMatching(/^[0-9A-Z]{2}\+DH$/), word: 'Dora' });
    expect(markOf('code', key, 13).mark.slice(-2)).toBe('AK');
    // Japan, Italy, the Soviet Union and the rest.
    expect(markOf('tail', key, 12).mark).toMatch(/^[A-Z][0-9]?-312$/);
    expect(markOf('squadron', key, 7).mark).toMatch(/^[0-9]{3}-7$/);
    expect(markOf('squadron', key, 13).mark).toMatch(/^[0-9]{3}-1$/);
    expect(markOf('tactical', key, 23).mark).toBe('White 23');
    expect(markOf('tactical', key, 51).mark).toBe('Red 1');
    expect(markOf('number', key, 14)).toEqual({ name: 'No. 14', mark: '14' });
    // The same group always paints the same; another group paints other letters; nothing is painted on a bomber not yet marked.
    expect(markOf('code', key, 30)).toEqual(markOf('code', key, 30));
    expect([1, 27, 53, 79].map((n) => markOf('letter', key, n).mark)).not.toEqual([1, 27, 53, 79].map((n) => markOf('letter', '3:18', n).mark));
    expect(markOf('letter', key, 0)).toEqual({ name: '', mark: '' });
    for (const style of ['letter', 'code', 'tail', 'squadron', 'tactical', 'number'] as const) {
      const all = Array.from({ length: 250 }, (_, i) => markOf(style, key, i + 1).mark);
      expect(new Set(all).size).toBe(250);
    }
  });

  it('no bomber carries a famous real aircraft\'s marking (the modeller\'s layer paints nothing for them)', () => {
    // The modeller, 2026-11-20: any AJ letters (the dams raid), KM-B, PM-M, and the tail codes T1-323 and T1-326.
    const famous = (m: string): boolean => m.startsWith('AJ-') || m === 'KM-B' || m === 'PM-M' || m === 'T1-323' || m === 'T1-326';
    // One group through every squadron's letters and every unit's code: none of them, whoever's turn it was.
    const key = '3:17';
    for (let n = 1; n <= 26 * 600; n++) expect(famous(markOf('letter', key, n).mark), `letter ${n}`).toBe(false);
    const tails = Array.from({ length: 27 * 60 }, (_, i) => markOf('tail', key, i + 1).mark);
    expect(tails.some(famous)).toBe(false);
    expect(new Set(tails).size).toBe(tails.length);
    expect(tails.filter((m) => /^T1-3[0-9]{2}$/.test(m)).length).toBe(60);
    expect(tails).toContain('T1-383');
    expect(tails).toContain('T1-386');
    // Many groups' first two squadrons: still none, and the bomber whose turn it was carries letters nobody else in
    // its group has.
    let took = 0;
    for (let k = 0; k < 3000; k++) {
      const all = Array.from({ length: 52 }, (_, i) => markOf('letter', `9:${k}`, i + 1).mark);
      expect(all.some(famous), `group ${k}`).toBe(false);
      expect(new Set(all).size, `group ${k}`).toBe(52);
      if (new Set(all.map((m) => m.slice(0, 2))).size > 2) took++;
    }
    // Some of those groups did have a famous marking's turn come up (a third pair of letters shows where).
    expect(took).toBeGreaterThan(0);
  });

  it('a replacement takes the marking of the bomber it replaces, and has it from the day it arrives', () => {
    const g = war(3, 'UK');
    const f = g.airFleets.find((x) => x.nat === 'UK' && x.bmr >= 30 && keptByName(x))!;
    muster(g, f);
    expect(f.bombers!.on.every((b) => b[0] > 0 && b[1] === 1)).toBe(true);
    const was = f.bombers!.on.map((b) => bomberMark(g, f, b).mark).sort();
    lose(f, 3);
    const fell = muster(g, f).map((b) => bomberMark(g, f, b).mark);
    expect(fell).toHaveLength(3);
    f.bmr += 3;
    muster(g, f);
    const now = f.bombers!.on;
    expect(now.map((b) => bomberMark(g, f, b).mark).sort()).toEqual(was);
    const fresh = now.filter((b) => b[1] === 2);
    expect(fresh.map((b) => bomberMark(g, f, b).mark).sort()).toEqual([...fell].sort());
    expect(fresh.every((b) => b[2] === 0 && b[5] === 0)).toBe(true);
    expect(bomberName(g, f, fresh[0])).toMatch(/^[A-Z] for [A-Z]/);
  });

  it('a British crew flies thirty operations, and goes home as an American one does', () => {
    expect([tourOf('USA'), tourOf('UK'), tourOf('CAN'), tourOf('GER'), tourOf('SOV'), tourOf('JAP'), tourOf('ITA'), tourOf('FRA')]).toEqual([25, 30, 30, 60, 40, 40, 40, 25]);
    expect([flownWord('USA'), flownWord('UK'), flownWord('GER')]).toEqual(['mission', 'operation', 'mission']);
    const g = war(3, 'UK');
    const f = g.airFleets.find((x) => x.nat === 'UK' && x.bmr >= 30 && x.ftr === 0 && keptByName(x))!;
    muster(g, f);
    f.bombers!.on.forEach((b, i) => { b[5] = i < 2 ? 29 : i < 4 ? 24 : 0; b[2] = b[5]; });
    const who = f.bombers!.on.slice(0, 2).map(called(g, f)), near = f.bombers!.on.slice(2, 4);
    const ready = (f.ready = 0.5);
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], NONE);
    expect(f.bombers!.home).toBe(2);
    // A month on from twenty-four is past an American tour and short of a British one.
    expect(near.every((b) => b[5] < 30 && b[5] >= 25 && f.bombers!.on.includes(b))).toBe(true);
    expect(f.ready).toBeCloseTo(ready + 2 * BOMBERS.lift.each, 9);
    const news = g.headlines.at(-1)!.text;
    // 'No. 1 Bomber Group' is written as Britain wrote it and takes no 'the'; a crew's tour is of operations there.
    expect(news).toBe(`The crews of ${who[0]} and ${who[1]}, of ${f.name}, have finished their tours of 30 operations. One goes home to train new crews, and one goes on a war bond tour. The group's readiness rises.`);
    const C = crewsHome(g, 'UK');
    expect([C.school, C.bonds.length]).toEqual([1, 1]);
  });

  it('a unit and a place read inside a sentence as English has them', () => {
    // A group named in English takes 'the'; a code, a 'No.' name or a name in its own language stays bare, as in the
    // air battle's dispatches, so a group reads the same in both (the UI director, 20 November 2026).
    expect(['8th Bomb Group', '9th Corps', 'Home Fleet', 'I Bomber Command', 'Pursuit Brigade'].map(theUnit)).toEqual(['the 8th Bomb Group', 'the 9th Corps', 'the Home Fleet', 'the I Bomber Command', 'the Pursuit Brigade']);
    for (const bare of ['No. 5 Group', 'Kampfgeschwader 4', 'KG 4', 'II. Gruppe JG 1', 'Luftflotte 2', 'Stormo Bombardieri 1', 'Force H', 'Army Karpaty']) expect(theUnit(bare)).toBe(bare);
    expect(['Ruhr', 'Crimea', 'Bremen', 'Warsaw', ''].map(thePlace)).toEqual(['the Ruhr', 'the Crimea', 'Bremen', 'Warsaw', '']);
  });

  it('the news of one crew calls a bomber with a marking "it"', () => {
    const t: ToursDone = { done: [[4, 1, 25, 0, 0, 25]], names: ['5J+DH'], tour: 25, flown: 'mission', named: false, school: 0, bonds: 1, lift: 0 };
    expect(toursWords('KG 4', t)).toBe('The bomber 5J+DH of KG 4 brings its crew home from their 25th mission, and their tour is over. They fly it home for a war bond tour.');
    // A group named in its own language stays bare, and Britain's tour is of operations.
    const g4: ToursDone = { ...t, names: ['G for George'], tour: 30, flown: 'operation', school: 1, bonds: 0 };
    expect(toursWords('No. 5 Group', g4)).toBe('The bomber G for George of No. 5 Group brings its crew home from their 30th operation, and their tour is over. They go home to train new crews.');
    expect(toursWords('Kampfgeschwader 4', { ...t, tour: 60 })).toBe('The bomber 5J+DH of Kampfgeschwader 4 brings its crew home from their 60th mission, and their tour is over. They fly it home for a war bond tour.');
  });

  it('a kill goes on the bomber\'s tally with the nation it was taken from', () => {
    const g = war(3, 'USA');
    const f = command(g);
    muster(g, f);
    flownMonth(g, f, 6, 'GER');
    flownMonth(g, f, 2, 'ITA');
    const by: Record<string, number> = {};
    for (const b of f.bombers!.on) for (const [nat, n] of Object.entries(b[6] ?? {})) by[nat] = (by[nat] ?? 0) + n;
    expect(by).toEqual({ GER: 6, ITA: 2 });
    expect(f.bombers!.on.every((b) => b[3] === Object.values(b[6] ?? {}).reduce((s, n) => s + n, 0))).toBe(true);
    // The fleet that flew against the raid names the nation.
    const def = g.airFleets.find((x) => x.nat === 'GER' && x.ftr > 0)!;
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [def], { ...NONE, defLost: 5 });
    expect(sum(f, 3)).toBe(13);
    expect(f.bombers!.on.reduce((s, b) => s + (b[6]?.GER ?? 0), 0)).toBe(11);
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
    const names = f.bombers!.on.map(called(g, f));
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
    // With fighters of its own by it the gunners never have more than a third of the kills, however many the bombers
    // (the historian, raid-card-defence-count.md: a great box with a thin escort does not make better shots).
    const weight = (x: typeof mixed) => (x.bmr * BOMBERS.gunners) / (x.ftr + x.bmr * BOMBERS.gunners);
    expect(weight(mixed)).toBeGreaterThan(BOMBERS.escorted);
    expect(sum(mixed, 3)).toBe(Math.round(40 * BOMBERS.escorted));
    expect(sum(mixed, 3)).toBeLessThan(40);
    // With as many fighters as bombers the share is the bombers' weight, well under the cap.
    const even = g.airFleets.find((x) => x.nat === 'USA' && x.bmr >= 60 && x.ftr === 0 && x.ship === undefined && x !== f && x !== mixed)!;
    even.ftr = even.bmr;
    muster(g, even);
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [even], [], { ...NONE, defLost: 40 });
    expect(weight(even)).toBeLessThan(BOMBERS.escorted);
    expect(sum(even, 3)).toBe(Math.round(40 * weight(even)));
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
    expect(crewGrade(fresh[0], 'USA')).toBe('Green');
  });

  it('a fought raid strikes off the very bombers the field saw fall', () => {
    const g = war(3, 'USA');
    const f = command(g);
    f.rec = { missions: 20 };
    muster(g, f);
    const fell = named(f).filter((_, i) => i === 3 || i === 11 || i === 40).map((b) => [...b] as BomberRow);
    expect(fell).toHaveLength(3);
    const names = fell.map(called(g, f));
    expect(names.every(Boolean)).toBe(true);
    lose(f, 3);
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], { ...NONE, attLost: 3, fallen: { [f.id]: fell } });
    const left = f.bombers!.on.map(called(g, f));
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
    const name = bomberName(g, f, old), flown = old[2];
    lose(f, 1);
    storyAir(g, 'superiority', g.byName['Warsaw'].id, [f], [], { ...NONE, attLost: 1, fallen: { [f.id]: [[...old] as BomberRow] } });
    expect(g.headlines.at(-1)!.text).toBe(`The bomber ${name} of ${theUnit(f.name)}, with ${flown} missions flown, did not come back from Warsaw.`);
    expect(f.bombers!.roll.at(-1)).toEqual({ t: g.turn, how: 'lost', b: old });
  });
});

/** Every crew of the group one mission short of its tour, and every bomber sure to fly. */
function onTheLast(g: Game, f: AirFleet): void {
  muster(g, f);
  BOMBERS.fit = 1;
  for (const b of f.bombers!.on) { b[5] = TOUR - 1; b[2] = 30; b[3] = 2; if (!b[0]) b[0] = 0; }
  storyMonth(g, f.nat);
}

describe('the tour of duty', () => {
  it('a crew goes home after its twenty-fifth mission, and not before', () => {
    const g = war(3, 'USA');
    const f = command(g);
    muster(g, f);
    BOMBERS.fit = 1;
    f.bombers!.on.forEach((b, i) => { b[5] = i < 5 ? TOUR - STORIES.tour.perMonth : 0; b[2] = b[5]; });
    setCrewOrder(g, 'school');
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], NONE);
    expect(f.bombers!.home).toBe(5);
    expect(f.rec!.tours).toBe(5);
    expect(f.bombers!.on.every((b) => b[5] < TOUR)).toBe(true);
    // The group's diary says it bombed; who went home is on the bombers' own roll.
    expect(diaryText(g, f, f.log!.at(-1)!)).toBe('Bombed Warsaw.');
    expect(f.bombers!.roll.filter((x) => x.how === 'home')).toHaveLength(5);
  });

  it('for a computer nation the bomber flies on with a new crew, and nothing else changes', () => {
    const g = war(3, 'GER');
    const f = command(g);
    onTheLast(g, f);
    const names = f.bombers!.on.map(called(g, f)), ready = f.ready, pp = g.nations.USA.pp, news = g.headlines.length;
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], NONE);
    const R = f.bombers!;
    expect(R.home).toBe(f.bmr);
    expect(R.on.map(called(g, f))).toEqual(names);
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
    const n = f.bmr, names = f.bombers!.on.map(called(g, f));
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
    expect(R.on.filter((b) => b[0]).every((b) => names.includes(bomberName(g, f, b)) && b[2] === 31 && b[3] === 2 && b[5] === 0)).toBe(true);
    const line = g.headlines.at(-1)!.text;
    expect(line).toContain(`and ${n - 3} more, of ${theUnit(f.name)}, have finished their tours of 25 missions.`);
    expect(line).toContain(`${S.crews * S.most} go home to train new crews, ${D.most} go on a war bond tour, and ${n - S.crews * S.most - D.most} go home.`);
    expect(line.endsWith("The group's readiness rises.")).toBe(true);
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
    f.bombers!.on.forEach((b, i) => { b[5] = i < 4 ? TOUR - 1 : 0; });
    expect(orderCrews(g, 'school')).toBe(humanUp(g));
    expect(setCrewOrder(g, 'bonds')).toBe(true);
    storyAir(g, 'bomb', g.byName['Warsaw'].id, [f], [], NONE);
    const C = crewsHome(g, 'USA');
    expect([C.school, C.bonds.length]).toEqual([0, 4]);
    expect(g.headlines.at(-1)!.text).toContain('have finished their tours of 25 missions. They go on a war bond tour.');
    // Every nation's crews fly tours (Brian, 20 November 2026: "applied for all nations not just the US").
    const h = war(3, 'GER');
    expect(setCrewOrder(h, 'bonds')).toBe(true);
    expect(setCrewOrder(war(3, null), 'bonds')).toBe(false);
  });

  it('the news of one crew names its bomber', () => {
    const done = (rows: BomberRow[], school: number, bonds: number, lift = 0): ToursDone => ({ done: rows, names: rows.map((b) => markOf('nose', 'k', b[0], b[1]).name), tour: 25, flown: 'mission', named: true, school, bonds, lift });
    const one = (b: BomberRow, school: number, bonds: number, lift = 0) => toursWords('VIII Bomber Command', done([b], school, bonds, lift));
    const b: BomberRow = [62, 1, 25, 3, 0, 25];
    // The UI director's words (20 November 2026): the bomber's kind on first mention, 'the' before a group named in
    // English, the tour is the crew's and a crew takes the plural, and the gain is the game's own word, readiness.
    expect(one(b, 1, 0)).toBe('The bomber Prize Pumpkin of the VIII Bomber Command brings her crew home from their 25th mission, and their tour is over. They go home to train new crews.');
    expect(one(b, 0, 1, 0.02)).toBe("The bomber Prize Pumpkin of the VIII Bomber Command brings her crew home from their 25th mission, and their tour is over. They fly her home for a war bond tour. The group's readiness rises.");
    expect(one([62, 2, 25, 0, 0, 25], 0, 0)).toBe('The bomber Prize Pumpkin II of the VIII Bomber Command brings her crew home from their 25th mission, and their tour is over. They go home.');
    expect(one([0, 0, 25, 0, 0, 25], 1, 0)).toBe('A bomber of the VIII Bomber Command brings its crew home from their 25th mission, and their tour is over. They go home to train new crews.');
    const many = (school: number, bonds: number, lift = 0) => toursWords('8th Bomb Group', done([b, [19, 1, 40, 0, 0, 25], [187, 1, 25, 1, 0, 25], [1, 1, 25, 0, 0, 25], [2, 1, 25, 0, 0, 25]].slice(0, school + bonds) as BomberRow[], school, bonds, lift));
    expect(many(2, 1, 0.02)).toBe("The crews of Prize Pumpkin, Highball Pearl and Welcome Mat, of the 8th Bomb Group, have finished their tours of 25 missions. Two go home to train new crews, and one goes on a war bond tour. The group's readiness rises.");
    expect(many(3, 2)).toMatch(/^The crews of Prize Pumpkin, Highball Pearl, Welcome Mat and 2 more, of the 8th Bomb Group, have finished their tours of 25 missions\. Three go home to train new crews, and 2 go on a war bond tour\.$/);
    // The game's one count rule: a word only to open the sentence, one to nine; 'one' anywhere; figures otherwise.
    expect([countWord(2, true), countWord(9, true), countWord(10, true), countWord(1), countWord(3), countWord(56)]).toEqual(['Two', 'Nine', '10', 'one', '3', '56']);
    const mixed: ToursDone = { ...done([b, [19, 1, 40, 0, 0, 25], [187, 1, 25, 1, 0, 25], [1, 1, 25, 0, 0, 25], [2, 1, 25, 0, 0, 25], [3, 1, 25, 0, 0, 25]], 2, 3), names: [] };
    expect(toursWords('8th Bomb Group', mixed)).toBe('Six crews of the 8th Bomb Group have finished their tours of 25 missions. Two go home to train new crews, 3 go on a war bond tour, and one goes home.');
    // All the same way.
    expect(many(3, 0)).toBe('The crews of Prize Pumpkin, Highball Pearl and Welcome Mat, of the 8th Bomb Group, have finished their tours of 25 missions. They go home to train new crews.');
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
    expect(crewGrade([0, 0, 0, 0, 5, 0], 'USA')).toBe('Trained');
    expect(crewGrade([0, 0, 0, 0, 0, 0], 'USA')).toBe('Green');
    expect(crewGrade([1, 1, 24, 0, 0, 24], 'USA')).toBe('Elite');
    BOMBERS.fit = 1;
    const b = f.bombers!.on.at(-1)!;
    b[4] = S.most; b[5] = TOUR - STORIES.tour.perMonth - 1;
    flownMonth(g, f, 0);
    expect(f.bombers!.on).toContain(b);
    expect(b[5]).toBe(TOUR - 1);
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
    const card = bomberCard(g, f);
    expect(card).toHaveLength(f.bmr);
    for (let i = 1; i < card.length; i++) expect(card[i - 1].missions).toBeGreaterThanOrEqual(card[i].missions);
    expect(card[0].name).toBe(bomberName(g, f, card[0].b));
  });

  it('a computer-run war is the same war with the bombers kept by name and without', () => {
    // The records themselves are left out of the comparison: the rosters and the count of tours finished.
    const state = (g: Game): string => JSON.stringify({ ...g, airFleets: g.airFleets.map((f) => ({ ...f, bombers: undefined, rec: f.rec && { ...f.rec, tours: undefined } })) });
    const a = war(5, null);
    for (let t = 0; t < 6; t++) runTurnAI(a);
    for (const nat of ['USA', 'GER', 'UK']) expect(a.airFleets.some((f) => f.nat === nat && f.bombers && f.bombers.on.length === f.bmr)).toBe(true);
    BOMBERS.names = {};
    const b = war(5, null);
    for (let t = 0; t < 6; t++) runTurnAI(b);
    expect(b.airFleets.some((f) => f.bombers)).toBe(false);
    expect(state(a)).toBe(state(b));
  });
});
