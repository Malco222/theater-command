// The highest award (campaign/valour.ts; Brian, 20 November 2026: "do the same bond tour or train new recruits for
// people who receive the medal of honor or what ever the highest award is for military service for each nation"). A
// man of a unit that earns an honour may be given his nation's highest award for valour; the player can send him home
// on a war bond tour, which pays for five to ten months, or to train recruits. Every nation's holders are on the
// record, and nothing changes a number for a computer-run nation.
import { afterEach, describe, expect, it } from 'vitest';
import budget from '../data/campaign/budget.json';
import { createGame } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { STORIES, storyAir, storyMonth, training } from '../src/campaign/stories';
import { humanUp, orderHero, runTurnAI } from '../src/campaign/turn';
import type { AirFleet, Game, Hero } from '../src/campaign/types';
import { VALOUR, awardName, awardTitleOf, heroWords, heroesOf, heroesWaiting, sendHero, sendWorth, valourMonth, whyNotSend } from '../src/campaign/valour';

const KEEP = JSON.parse(JSON.stringify(VALOUR)) as typeof VALOUR;
afterEach(() => { Object.assign(VALOUR, JSON.parse(JSON.stringify(KEEP))); });

const NONE = { sky: false, sunk: 0, attLost: 0, defLost: 0 };
/** A German bomber fleet flies a tour of bombing, which earns it the 'Tour of duty' honour (tests/stories.test.ts). */
function tour(g: Game): AirFleet {
  const f = g.airFleets.find((x) => x.nat === 'GER' && x.bmr > x.ftr)!;
  const t = g.byName['Warsaw'].id;
  for (let i = 0; i < Math.ceil(25 / STORIES.tour.perMonth); i++) storyAir(g, 'bomb', t, [f], [], NONE);
  expect((f.hon ?? []).length).toBeGreaterThan(0);
  return f;
}
const sure = (): void => { VALOUR.chance = [1, 1, 1, 1]; };
const heroes = (g: Game): Hero[] => g.stories.heroes ?? [];

describe('each nation\'s highest award for valour', () => {
  it('has its own name, and a plain one where the game names none', () => {
    expect(awardName('USA', 1)).toBe('the Medal of Honor');
    for (const nat of ['UK', 'CAN', 'AUS', 'NZL', 'SAF', 'IND']) expect(awardName(nat, 1)).toBe('the Victoria Cross');
    expect(awardName('GER', 1)).toBe("the Knight's Cross");
    expect(awardName('SOV', 1)).toBe('Hero of the Soviet Union');
    expect(awardName('JAP', 1)).toBe('the Order of the Golden Kite');
    expect(awardName('ITA', 1)).toBe('the Gold Medal of Military Valour');
    expect(awardName('FRA', 1)).toBe('the Legion of Honour');
    expect(awardName('POL', 1)).toBe('the Virtuti Militari');
    // A nation with no award of its own named: never 'awarded the ... award' (the UI director).
    expect(awardName('BRA', 1)).toBe('the highest decoration for valour');
    expect([awardTitleOf('USA', 1), awardTitleOf('SOV', 1), awardTitleOf('BRA', 1)]).toEqual(['Medal of Honor', 'Hero of the Soviet Union', 'Highest decoration for valour']);
  });

  it('Finland\'s Mannerheim Cross is given only from December 1941 (the historian)', () => {
    expect(awardName('FIN', 27)).toBe('the highest decoration for valour');
    expect(awardName('FIN', 28)).toBe('the Mannerheim Cross');
  });

  it('the news says how it was given, and where', () => {
    const g = createGame(1, 'GER');
    const at = g.byName['Warsaw'].id;
    const h = (nat: string, kind: Hero['kind'], where: number): Hero => ({ id: 1, turn: 30, nat, kind, unit: 1, name: '9th Corps', who: 'A sergeant', at: where, award: 1 });
    // A unit named in English takes 'the'; a title is 'made' (the UI director, 20 November 2026).
    expect(heroWords(g, h('USA', 'corps', at))).toBe('A sergeant of the 9th Corps has been awarded the Medal of Honor for valour at Warsaw.');
    expect(heroWords(g, h('SOV', 'corps', at))).toBe('A sergeant of the 9th Corps has been made a Hero of the Soviet Union for valour at Warsaw.');
    expect(heroWords(g, h('UK', 'air', -1))).toBe('A sergeant of the 9th Corps has been awarded the Victoria Cross for valour.');
    expect(heroWords(g, h('BRA', 'corps', -1))).toBe('A sergeant of the 9th Corps has been awarded the highest decoration for valour.');
    expect(heroWords(g, h('UK', 'fleet', 0))).toBe(`A sergeant of the 9th Corps has been awarded the Victoria Cross for valour in the ${g.seas[0].name}.`);
    // A unit written as a code or in its own language stays bare, and a place English gives 'the' keeps it.
    expect(heroWords(g, { ...h('GER', 'air', g.byName['Ruhr'].id), name: 'Kampfgeschwader 4', who: 'A navigator' })).toBe("A navigator of Kampfgeschwader 4 has been awarded the Knight's Cross for valour at the Ruhr.");
    expect(heroWords(g, { ...h('SOV', 'air', at), name: '3rd Fighter Division', who: 'A pilot' })).toBe('A pilot of the 3rd Fighter Division has been made a Hero of the Soviet Union for valour at Warsaw.');
  });
});

describe('a man of a unit that earns an honour may be given the award', () => {
  it('he is on his unit\'s record and in the player\'s news, waiting for a decision', () => {
    sure();
    const g = createGame(1, 'GER');
    const news = g.headlines.length;
    const f = tour(g);
    const [h] = heroes(g);
    expect(heroes(g)).toHaveLength((f.hon ?? []).length);
    expect([h.nat, h.kind, h.unit, h.name, h.turn]).toEqual(['GER', 'air', f.id, f.name, g.turn]);
    expect(VALOUR.who.bmr).toContain(h.who);
    expect(f.hon).toContain(h.award);
    expect(heroesOf(g, 'air', f.id)).toEqual(heroes(g));
    expect(heroesWaiting(g)).toEqual(heroes(g));
    const told = g.headlines.slice(news).filter((x) => x.kind === 'honour' && x.text.includes("has been awarded the Knight's Cross for valour"));
    expect(told).toHaveLength(heroes(g).length);
    // The rank once, then 'them': not 'the holder', and not 'him' (the UI director).
    expect(told[0].text).toBe(`${h.who} of ${f.name} has been awarded the Knight's Cross for valour at Warsaw. You can keep them with their unit, or send them home from its record, on a war bond tour or to train recruits.`);
  });

  it('the greater the honour the likelier, and with no chance nobody is', () => {
    expect(VALOUR.chance).toEqual([0, 0.05, 0.12, 0.25]);
    VALOUR.chance = [0, 0, 0, 0];
    const g = createGame(1, 'GER');
    tour(g);
    expect(heroes(g)).toEqual([]);
  });

  it('the same war gives the same men the award', () => {
    sure();
    const a = createGame(1, 'GER'), b = createGame(1, 'GER');
    tour(a); tour(b);
    expect(heroes(a)).toEqual(heroes(b));
    expect(heroes(a).length).toBeGreaterThan(0);
  });

  it('a computer nation\'s man is on the record and no more: no news, no decision, nothing to pay', () => {
    sure();
    const g = createGame(1, 'UK');
    const news = g.headlines.length;
    tour(g);
    const [h] = heroes(g);
    expect(h.nat).toBe('GER');
    expect(g.headlines.slice(news).some((x) => x.text.includes("Knight's Cross"))).toBe(false);
    expect(heroesWaiting(g)).toEqual([]);
    expect(whyNotSend(g, h, 'bonds')).toBe('This is not your unit.');
    expect(sendHero(g, h.id, 'bonds')).toBe(false);
    expect(valourMonth(g, 'GER')).toBe(0);
  });
});

describe('what the player does with a holder of the award', () => {
  it('a war bond tour pays the treasury every month for five to ten months, and then it is over', () => {
    sure();
    const g = createGame(1, 'GER');
    tour(g);
    const [h] = heroes(g);
    expect(whyNotSend(g, h, 'bonds')).toBe('');
    expect(sendWorth(h, 'bonds')).toBe(`+${(VALOUR.bonds.pp * budget.scale).toLocaleString('en-US')} a month for the treasury, for 5 to 10 months`);
    const news = g.headlines.length;
    expect(sendHero(g, h.id, 'bonds')).toBe(true);
    expect(h.sent).toBe('bonds');
    expect(h.sentOn).toBe(g.turn);
    const months = h.left!;
    expect(months).toBeGreaterThanOrEqual(5);
    expect(months).toBeLessThanOrEqual(10);
    expect(g.headlines[news].text).toBe(`${h.who} of ${h.name}, holder of the Knight's Cross, goes home on a war bond tour, which will bring in ${(VALOUR.bonds.pp * budget.scale).toLocaleString('en-US')} a month for ${months} months.`);
    // He is no longer waiting, and cannot be sent twice.
    expect(heroesWaiting(g).some((x) => x.id === h.id)).toBe(false);
    expect(whyNotSend(g, h, 'school')).toBe('Already home on a war bond tour.');
    expect(sendHero(g, h.id, 'school')).toBe(false);
    const pp = g.nations.GER.pp;
    let paid = 0;
    for (let m = 0; m < 12; m++) paid += valourMonth(g, 'GER');
    expect(paid).toBe(VALOUR.bonds.pp * months);
    expect(g.nations.GER.pp).toBeCloseTo(pp + paid, 9);
    expect(h.left).toBe(0);
  });

  it('it is paid when the nation\'s month is made up', () => {
    sure();
    const g = createGame(1, 'GER');
    tour(g);
    const [h] = heroes(g);
    sendHero(g, h.id, 'bonds');
    const left = h.left!;
    storyMonth(g, 'GER');
    expect(h.left).toBe(left - 1);
    storyMonth(g, 'UK');
    expect(h.left).toBe(left - 1);
  });

  it('training recruits, every later draft is better trained, and an airman brings on new pilots', () => {
    sure();
    const g = createGame(1, 'GER');
    tour(g);
    const [h] = heroes(g);
    g.airPool.GER = { ftr: 0, bmr: 0, pilots: 100, lostFtr: 0, lostBmr: 0, lostPilots: 0 };
    const was = training(g, 'GER'), schools = g.stories.schools.GER ?? 0, pilots = g.airPool.GER.pilots, pp = g.nations.GER.pp;
    expect(sendWorth(h, 'school')).toBe(`${STORIES.cash.pilots} more trained pilots at once, and every later draft and new corps is trained ${STORIES.veterans.perSchool * 100} points better`);
    expect(sendHero(g, h.id, 'school')).toBe(true);
    expect(g.stories.schools.GER).toBe(schools + VALOUR.school.steps);
    expect(training(g, 'GER')).toBeCloseTo(was + STORIES.veterans.perSchool * VALOUR.school.steps, 9);
    expect(g.airPool.GER.pilots).toBe(pilots + STORIES.cash.pilots);
    expect(g.nations.GER.pp).toBe(pp);
    expect(g.headlines.at(-1)!.text).toBe(`${h.who} of ${h.name}, holder of the Knight's Cross, goes home to train recruits. From now on every draft reaches the front better trained.`);
    expect(whyNotSend(g, h, 'bonds')).toBe('Already home training recruits.');
    expect(valourMonth(g, 'GER')).toBe(0);
  });

  it('a title is what its holder is: a Hero of the Soviet Union goes home, not a holder of one', () => {
    const g = createGame(1, 'SOV');
    const f = g.airFleets.find((x) => x.nat === 'SOV' && x.ftr > 0)!;
    const h: Hero = { id: 1, turn: g.turn, nat: 'SOV', kind: 'air', unit: f.id, name: f.name, who: 'A pilot', at: -1, award: 1 };
    (g.stories.heroes ??= []).push(h);
    expect(f.name).toBe('Leningrad District Air Force');
    expect(sendHero(g, h.id, 'school')).toBe(true);
    expect(g.headlines.at(-1)!.text).toBe('A pilot of the Leningrad District Air Force, a Hero of the Soviet Union, goes home to train recruits. From now on every draft reaches the front better trained.');
  });

  it('the schools can take only so many, and a man whose unit is gone cannot be sent', () => {
    sure();
    const g = createGame(1, 'GER');
    const f = tour(g);
    const [h] = heroes(g);
    g.stories.schools.GER = STORIES.veterans.maxSchools;
    expect(whyNotSend(g, h, 'school')).toBe('The schools have all the instructors they can use.');
    expect(whyNotSend(g, h, 'bonds')).toBe('');
    g.airFleets = g.airFleets.filter((x) => x !== f);
    expect(whyNotSend(g, h, 'bonds')).toBe('The unit no longer exists.');
    expect(heroesWaiting(g)).toEqual([]);
  });

  it('is an order of the player\'s own turn, and is kept in a saved game', () => {
    sure();
    const g = createGame(1, 'GER');
    tour(g);
    const [h] = heroes(g);
    expect(orderHero(g, h.id, 'bonds')).toBe(humanUp(g));
    const back = loadGame(saveGame(g))!;
    expect(back.stories.heroes).toEqual(g.stories.heroes);
  });
});

describe('the record is kept, and history is not moved', () => {
  it('a computer-run war is the same war whether or not anyone is given the award', () => {
    const state = (g: Game): string => JSON.stringify({ ...g, stories: { ...g.stories, heroes: undefined } });
    sure();
    const a = startOpen(createGame(5, null));
    for (let t = 0; t < 8; t++) runTurnAI(a);
    expect(heroes(a).length).toBeGreaterThan(0);
    VALOUR.chance = [0, 0, 0, 0];
    const b = startOpen(createGame(5, null));
    for (let t = 0; t < 8; t++) runTurnAI(b);
    expect(heroes(b)).toEqual([]);
    expect(state(a)).toBe(state(b));
  });
});
