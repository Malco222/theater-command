// Guns never go forward alone, Britain may stand by its dominions, and the town guard tears up no railway (Version
// 84.2.5; the design director's ruling 211, from the Playtester's trace of Brian's own campaign of 2026-11-17: one
// Canadian corps of guns with no infantry stormed Buffalo and Pittsburgh and would have taken Washington, while
// Britain looked on at a war made on her dominion). The grand campaign only: history's campaigns raise no guns.
//
// (a) A force with no fighting corps in it (infantry, cavalry, mountain troops, motorized troops or tanks) opens no
//     attack, storms no town, makes no sortie, lands on no enemy shore and marches onto no enemy-held ground unless
//     a fighting corps of its side stands there already; left alone round a town it gives up the siege; and guns
//     caught alone in the field are overrun. Player and computer alike, and the refusal is a sentence.
// (b) War on Canada by a country at peace with Britain: she answers six times in ten and stays out four (Brian's
//     change of 2026-11-17, 16:49 UTC), on one roll made when the war is declared; a war Canada starts is not hers.
// (g) A railway is torn up by troops as they fall back: where the loser has no corps next door, nobody does it.
import { describe, expect, it } from 'vitest';
import { notTanksAlone } from '../src/campaign/ai';
import { warForecast } from '../src/campaign/diplomacy';
import {
  ENGINEERS_ALONE, GUNS_ALONE, addCorps, allied, assess, atWar, createGame, declareWar, dominionAnswers, fights, gunsRule, hostileUnits, limitAttackers, moveCorps, relocate, setWar, standsBy, standsByWhom,
} from '../src/campaign/game';
import { monthCard, monthMark } from '../src/campaign/monthcard';
import { whyNotLand } from '../src/campaign/navy';
import { onNet, wreckChance } from '../src/campaign/rails';
import { besieged, garrison, siegeMonth, stormForce, townHeld, whyNotStorm } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { townName } from '../src/campaign/towns';
import { beginHuman, gunsLeftAlone, humanUp, marchOrder, orderAttack, previewAttack, reach, stepAI, whyNotLead } from '../src/campaign/turn';
import type { Corps, Game, Headline, Region } from '../src/campaign/types';
import { WORLD, answersOf, worldMonth } from '../src/campaign/world';

function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const mine = (g: Game, nat: string) => (c: Corps): boolean => c.nat === nat && !c.dead && c.joint === undefined && c.town === undefined;
/** A German province with infantry in its field and a Polish province next door whose field holds a Polish army. */
function front(g: Game): { from: Region; to: Region; inf: Corps } {
  for (const from of g.regions) {
    if (from.ctrl !== 'GER') continue;
    const inf = from.units.find((c) => mine(g, 'GER')(c) && c.kind === 'inf' && c.ap > 0);
    const to = inf ? from.nb.map((j) => g.regions[j]).find((x) => x.ctrl === 'POL' && hostileUnits(g, x, 'GER').length > 0) : undefined;
    if (inf && to) return { from, to, inf };
  }
  throw new Error('no front');
}
/** A province far from the fighting to send corps away to. */
const rear = (g: Game, nat: string, not: Region[]): Region => g.regions.find((x) => x.ctrl === nat && x.owner === nat && !not.includes(x) && !not.some((r) => r.nb.includes(x.id)) && x.units.length < 3)!;
const gunCorps = (g: Game, nat: string, at: Region, name = 'Test artillery'): Corps => addCorps(g, nat, name, 'art', 2, at.id, {});

describe('guns never go forward alone (Version 84.2.5, ruling 211(a))', () => {
  it('the rule is the grand campaign\'s, and a fighting corps is infantry, cavalry, mountain or motorized troops or tanks', () => {
    expect(gunsRule(createGame(3, 'GER'))).toBe(false);
    const g = playing();
    expect(gunsRule(g)).toBe(true);
    const kinds = (k: string): boolean => fights({ kind: k } as Corps);
    expect(['inf', 'cav', 'mtn', 'mot', 'arm'].every(kinds)).toBe(true);
    expect(['art', 'eng', 'fort'].some(kinds)).toBe(false);
  });

  it('guns alone open no attack, and say why; with infantry beside them they go in', () => {
    const g = playing();
    const { from, to, inf } = front(g);
    const gun = gunCorps(g, 'GER', from);
    gun.ap = 4;
    expect(limitAttackers(g, [gun], to)).toEqual([]);
    expect(previewAttack(g, [gun], to)).toBeNull();
    expect(whyNotLead(g, [gun], to.id)).toBe(GUNS_ALONE);
    expect(GUNS_ALONE).toBe('Guns cannot take ground alone. Send infantry or tanks in with them.');
    // The reach still shows the gun corps its part in an attack: it can join one.
    expect(reach(g, gun).get(to.id)?.attack).toBe(true);
    const before = g.reports.length;
    expect(orderAttack(g, [gun], to)).toBeNull();
    expect(g.reports.length).toBe(before);
    expect(gun.reg).toBe(from.id);
    // Engineers alone are told the same of themselves.
    const eng = addCorps(g, 'GER', 'Test engineers', 'eng', 1, from.id, {});
    eng.ap = 4;
    expect(whyNotLead(g, [eng], to.id)).toBe(ENGINEERS_ALONE);
    expect(whyNotLead(g, [eng, gun], to.id)).toBe('Guns and engineers cannot take ground alone. Send infantry or tanks in with them.');
    // With a fighting corps the force goes, guns and all.
    expect(whyNotLead(g, [gun, inf], to.id)).toBeNull();
    const force = limitAttackers(g, [gun, inf], to);
    expect(force).toContain(inf);
    expect(previewAttack(g, [gun, inf], to)).not.toBeNull();
    // The staffs: a pick of guns alone takes a fighting corps from the pool, or is not made.
    expect(notTanksAlone(g, [gun], [gun, inf], to)).toEqual([gun, inf]);
    expect(notTanksAlone(g, [gun], [gun], to)).toBeNull();
  });

  it('guns march onto no enemy-held ground alone: they follow a fighting corps there', () => {
    const g = playing();
    const { from, to, inf } = front(g);
    // The Polish field army goes away: the province is enemy-held ground with nobody in its field.
    const polRear = rear(g, 'POL', [from, to]);
    for (const u of [...to.units]) relocate(g, u, polRear);
    expect(hostileUnits(g, to, 'GER').length).toBe(0);
    // The gun corps stands by itself in a province of its own.
    const park = from.nb.map((j) => g.regions[j]).find((x) => x.ctrl === 'GER' && x.nb.includes(to.id) && x.id !== to.id) ?? from;
    const gerRear = rear(g, 'GER', [from, to, park]);
    if (park !== from) for (const u of [...park.units]) relocate(g, u, gerRear);
    else for (const u of [...from.units]) if (u !== inf) relocate(g, u, gerRear);
    const gun = gunCorps(g, 'GER', park);
    gun.ap = 4;
    if (park === from) relocate(g, inf, gerRear);
    expect(g.regions[gun.reg].units.some((u) => u !== gun && fights(u))).toBe(false);
    expect(reach(g, gun).has(to.id)).toBe(false);
    expect(whyNotLead(g, [gun], to.id)).toBe(GUNS_ALONE);
    expect(moveCorps(g, gun, to)).toBe(false);
    expect(gun.reg).toBe(park.id);
    // The infantry goes first, and the guns follow onto the ground it stands on (or has taken).
    relocate(g, inf, from);
    inf.ap = 4;
    expect(marchOrder(g, [gun, inf])).toEqual([inf, gun]);
    expect(moveCorps(g, inf, to)).toBe(true);
    expect(inf.reg).toBe(to.id);
    gun.ap = 4;
    expect(whyNotLead(g, [gun], to.id)).toBeNull();
    expect(reach(g, gun).has(to.id)).toBe(true);
    expect(moveCorps(g, gun, to)).toBe(true);
    expect(gun.reg).toBe(to.id);
  });

  it('guns caught alone in the field are overrun, and the card says so before the battle', () => {
    const g = playing();
    const { to, inf } = front(g);
    const polRear = rear(g, 'POL', [to, g.regions[inf.reg]]);
    for (const u of [...to.units]) relocate(g, u, polRear);
    // (The town's guard is spent: a city's guard beside the guns (ruling 232) means they are not alone.)
    g.flags[`guard:${to.id}`] = 100;
    const gun = gunCorps(g, 'POL', to, 'Polish test artillery');
    const a = previewAttack(g, [inf], to)!;
    expect(a).not.toBeNull();
    expect(a.overrun).toBe(true);
    expect(a.odds * a.dice[0]).toBeGreaterThanOrEqual(3);
    expect(a.lines.find((l) => l.label === 'Guns alone')).toEqual({ label: 'Guns alone', value: 'The guns stand with no infantry or tanks in front of them', effect: 'An attack overruns them: every gun is taken' });
    expect(a.lines.some((l) => (l.effect ?? '').includes('other defenders'))).toBe(false);
    const rep = orderAttack(g, [inf], to)!;
    expect(rep.taken).toBe(true);
    expect(gun.dead).toBe(true);
    expect(rep.defenders.find((u) => u.name === 'Polish test artillery')?.fate).not.toBe('retreated');
    expect(rep.lines.find((l) => l.label === 'Guns alone')?.effect).toBe('They were overrun: every gun was taken');
  });

  it('guns beside infantry are not overrun, and thicken its line as before', () => {
    const g = playing();
    const { to, inf } = front(g);
    gunCorps(g, 'POL', to, 'Polish test artillery');
    const a = assess(g, [inf], to);
    expect(a.overrun).toBeUndefined();
    expect(a.lines.find((l) => l.label === 'Artillery')?.effect).toMatch(/other defenders fight \d+% stronger under their own guns/);
  });

  it('guns storm no town and keep up no siege alone: left by themselves they fall back and the siege is raised', () => {
    const g = playing();
    const { from, to, inf } = front(g);
    const polRear = rear(g, 'POL', [from, to]);
    // One Polish corps goes inside the town (a town held by its guard alone opens its gates to a big enough army), the rest go away.
    const inside = to.units.find((u) => u.nat === 'POL' && u.kind === 'inf')!;
    expect(garrison(g, inside)).toBe(true);
    for (const u of [...to.units]) relocate(g, u, polRear);
    expect(townHeld(g, to)).toBe(true);
    inf.ap = 4;
    expect(moveCorps(g, inf, to)).toBe(true);
    expect(to.ctrl).toBe('POL');
    expect(besieged(g, to)).toBe(true);
    const gun = gunCorps(g, 'GER', from);
    const gun2 = gunCorps(g, 'GER', from, 'Second test artillery');
    gun.ap = gun2.ap = 4;
    expect(moveCorps(g, gun, to)).toBe(true);
    expect(moveCorps(g, gun2, to)).toBe(true);
    // A staff that can run only a small battle still sends a fighting corps into the storm.
    const most = g.nations.GER.maxAttack;
    g.nations.GER.maxAttack = 2;
    const going = stormForce(g, [gun, gun2, inf], to);
    expect(going.length).toBe(2);
    expect(going).toContain(inf);
    g.nations.GER.maxAttack = most;
    // The infantry marches away: the guns are alone round the town.
    expect(gunsLeftAlone(g, to.id)).toBeNull();
    relocate(g, inf, from);
    expect(gunsLeftAlone(g, to.id)).toBe(`Only guns are left round ${townName(to)}: the siege is raised unless infantry or tanks join them this month.`);
    delete g.flags[`storm:${to.id}`];
    expect(whyNotStorm(g, to, 'GER')).toBe(GUNS_ALONE);
    expect(stormForce(g, [gun, gun2], to)).toEqual([]);
    expect(besieged(g, to)).toBe(true);
    const h = g.headlines.length;
    siegeMonth(g, 'POL');
    expect(besieged(g, to)).toBe(false);
    expect(to.ctrl).toBe('POL');
    for (const c of [gun, gun2]) { expect(c.dead ?? false).toBe(false); expect(c.reg).not.toBe(to.id); expect(allied(g, 'GER', g.regions[c.reg].ctrl)).toBe(true); }
    const said = g.headlines.slice(h).map((x) => x.text).join(' | ');
    expect(said).toMatch(/The German guns round .+, left with no infantry or tanks beside them, fall back to [^.]+\. The siege is raised\./);
  });

  it('guns land on no enemy shore by themselves', () => {
    const g = playing();
    const { from, to, inf } = front(g);
    const gun = gunCorps(g, 'GER', from);
    expect(whyNotLand(g, [gun], to)).toBe(GUNS_ALONE);
    expect(whyNotLand(g, [inf], to)).not.toBe(GUNS_ALONE);
  });
});

describe('Britain may stand by its dominions: six times in ten (Version 84.2.5, ruling 211(b) as Brian changed it)', () => {
  const ANSWERS = 'Britain stands by Canada', STAYS = 'Britain stays out of Canada\'s war';
  /** The campaign's seed is set so that the roll for a war America makes on Canada in the month `war` goes the way asked. */
  function fix(g: Game, answers: boolean, war = g.turn): Game {
    for (let s = 1; s < 400; s++) { g.seed = s; if (dominionAnswers(g, 'UK', 'USA', 'CAN', war) === answers) return g; }
    throw new Error('no seed gives that answer');
  }
  const titled = (g: Game, from: number): Headline[] => g.headlines.slice(from).filter((x) => x.head === ANSWERS || x.head === STAYS);

  it('the odds sit in the data beside the row, and of 100 seeded declarations about 60 answer', () => {
    expect(WORLD.dominions!.UK).toMatchObject({ of: ['CAN', 'AUS', 'NZL', 'SAF'], answers: 0.6 });
    expect(answersOf('UK')).toBe(0.6);
    expect(answersOf('FRA')).toBe(0);
    const g = playing('USA');
    let yes = 0;
    for (let s = 1; s <= 100; s++) { g.seed = s; if (dominionAnswers(g, 'UK', 'USA', 'CAN', g.turn)) yes++; }
    expect(yes).toBeGreaterThanOrEqual(50);
    expect(yes).toBeLessThanOrEqual(70);
    // Flat: over many campaigns, months, declarers and dominions the share is six in ten, and asking again
    // gives the same answer.
    let all = 0, n = 0;
    for (let s = 1; s <= 250; s++) for (const war of [1, 9, 17, 30, 52]) for (const [by, on] of [['USA', 'CAN'], ['MEX', 'CAN'], ['JPN', 'AUS'], ['ITA', 'SAF']]) {
      g.seed = s;
      const first = dominionAnswers(g, 'UK', by, on, war);
      expect(dominionAnswers(g, 'UK', by, on, war)).toBe(first);
      all += Number(first); n++;
    }
    expect(all / n).toBeGreaterThan(0.57);
    expect(all / n).toBeLessThan(0.63);
  });

  it('the card says the odds and never a promise', () => {
    const g = playing('USA');
    expect(allied(g, 'UK', 'CAN')).toBe(true);
    expect(atWar(g, 'USA', 'UK')).toBe(false);
    expect(standsByWhom(g, 'USA', 'CAN')).toBe('UK');
    const f = warForecast(g, 'USA', 'CAN');
    expect(f.patron).toBe('UK');
    expect(f.answers).toBe(0.6);
    // Ruling 222: the odds as her war in Europe stands that day, with the reason (tests/britain-222.test.ts has the four).
    expect(f.patronSays).toEqual(['Britain may stand by it.', 'Six times in ten Britain declares war on you the same month, and four times in ten it stays out of this war: Britain is at war in Europe, with France still fighting beside it.']);
    const m = warForecast(g, 'USA', 'MEX');
    expect([m.patron, m.answers, m.patronSays]).toEqual([null, 0, null]);
  });

  it('she answers: war with Britain the same month, with her headline; France and the rest stay as they were', () => {
    const g = fix(playing('USA'), true);
    const align = g.nations.USA.align;
    const h = g.headlines.length, mark = monthMark(g);
    declareWar(g, 'USA', 'CAN');
    expect(atWar(g, 'USA', 'CAN')).toBe(true);
    expect(atWar(g, 'USA', 'UK')).toBe(true);
    expect(g.flags['war:UK:USA']).toBe(g.turn);
    const news = titled(g, h);
    expect(news.map((x) => [x.head, x.kind, x.nat, x.warn, x.text])).toEqual([[ANSWERS, 'war', 'UK', undefined, 'Britain declares war on the United States: it answers for its dominion.']]);
    // One member alone: nobody else of the alliance, and America stands where it stood.
    expect(atWar(g, 'USA', 'FRA')).toBe(false);
    expect(g.nations.USA.align).toBe(align);
    expect(standsByWhom(g, 'USA', 'CAN')).toBeNull();
    // On the month card it is a band of war, headed by its title, under Britain's flag.
    const band = monthCard(g, mark).bands.find((x) => x.head === ANSWERS)!;
    expect(band).toBeDefined();
    expect([band.word, band.nats[0]]).toEqual(['WAR', 'UK']);
  });

  it('she stays out: no war, news the same month in the brass band, and no second roll while that war lasts', () => {
    const g = fix(playing('USA'), false);
    const h = g.headlines.length, mark = monthMark(g);
    declareWar(g, 'USA', 'CAN');
    expect(atWar(g, 'USA', 'CAN')).toBe(true);
    expect(atWar(g, 'USA', 'UK')).toBe(false);
    expect(g.flags['war:UK:USA']).toBeUndefined();
    expect(titled(g, h).map((x) => [x.head, x.kind, x.nat, x.warn, x.text])).toEqual([[STAYS, 'info', 'UK', undefined, 'Britain does not declare war on the United States: Canada fights this war without it.']]);
    // The month card: news, never a warning and never red.
    const card = monthCard(g, mark);
    const band = card.bands.find((x) => x.head === STAYS)!;
    expect(band).toBeDefined();
    expect([band.word, band.nats[0]]).toEqual(['NEWS', 'UK']);
    expect(card.bands.some((x) => x.word === 'WARNING')).toBe(false);
    // No second roll while that war lasts, whatever the dice would say now: months pass and she is still out.
    const war = g.flags['war:USA:CAN'];
    fix(g, true, war);
    const k = g.headlines.length;
    expect(standsBy(g, 'USA', 'CAN', war)).toBe(false);
    for (let i = 0; i < 3; i++) { g.turn++; worldMonth(g); }
    expect(atWar(g, 'USA', 'UK')).toBe(false);
    expect(titled(g, k)).toEqual([]);
    // Her own staff stays free: the forecast still names her, for a war of her own choosing is no roll.
    expect(standsByWhom(g, 'USA', 'CAN')).toBe('UK');
    // A war made again another month is a new declaration, and a new roll.
    g.flags['war:USA:CAN'] = g.turn;
    fix(g, true);
    expect(standsBy(g, 'USA', 'CAN')).toBe(true);
    expect(atWar(g, 'USA', 'UK')).toBe(true);
  });

  it('a war a dominion starts is not Britain\'s, and has no roll', () => {
    const g = playing('USA');
    const h = g.headlines.length;
    declareWar(g, 'CAN', 'MEX');
    expect(atWar(g, 'CAN', 'MEX')).toBe(true);
    expect(atWar(g, 'UK', 'MEX')).toBe(false);
    worldMonth(g);
    expect(atWar(g, 'UK', 'MEX')).toBe(false);
    expect(g.headlines.slice(h).some((x) => x.head !== undefined)).toBe(false);
    expect(Object.keys(g.flags).some((key) => key.startsWith('domroll:'))).toBe(false);
  });

  it.each([[true], [false]])('a campaign saved before the rule: one roll as the month ends, and the news says since when (answers: %s)', (answers) => {
    const g = playing('USA');
    // The war as an older build left it: made on Canada two months ago, and Britain at peace.
    setWar(g, 'USA', 'CAN');
    const since = g.turn;
    g.flags['war:USA:CAN'] = since;
    g.turn += 2;
    fix(g, answers, since);
    expect(atWar(g, 'USA', 'UK')).toBe(false);
    const h = g.headlines.length;
    worldMonth(g);
    expect(atWar(g, 'USA', 'UK')).toBe(answers);
    const news = titled(g, h);
    expect(news.length).toBe(1);
    expect(news[0].head).toBe(answers ? ANSWERS : STAYS);
    expect(news[0].text).toMatch(answers
      ? /^The United States has been at war with Canada since (September|October|November|December) 1939\. Britain now declares war on the United States: it answers for its dominion\.$/
      : /^The United States has been at war with Canada since (September|October|November|December) 1939\. Britain does not declare war on the United States: Canada fights this war without it\.$/);
    // Once: the months after say nothing more, whatever the dice would say then.
    fix(g, true, since);
    const k = g.headlines.length;
    g.turn++;
    worldMonth(g);
    expect(titled(g, k)).toEqual([]);
    expect(atWar(g, 'USA', 'UK')).toBe(answers);
  });

  it('not in World Domination, where a dominion follows nobody either', () => {
    const g = fix(playing('USA'), true);
    g.flags.dom = 1;
    expect(standsByWhom(g, 'USA', 'CAN')).toBeNull();
    expect(warForecast(g, 'USA', 'CAN').patronSays).toBeNull();
    const h = g.headlines.length;
    declareWar(g, 'USA', 'CAN');
    expect(atWar(g, 'USA', 'UK')).toBe(false);
    expect(titled(g, h)).toEqual([]);
  });
});

describe('the town guard tears up no railway (Version 84.2.5, ruling 211(g))', () => {
  it('where the loser has no corps next door the track is not torn up; with one next door it may be', () => {
    const g = playing();
    const { to } = front(g);
    expect(wreckChance(g, to, 'GER')).toBeGreaterThanOrEqual(0);
    // A Polish province on the net with a Polish corps next door: the chance stands.
    const held = g.regions.find((x) => x.ctrl === 'POL' && onNet(x.id) && x.nb.some((j) => g.regions[j].units.some((u) => u.nat === 'POL' && !u.dead)))!;
    expect(held).toBeDefined();
    expect(wreckChance(g, held, 'GER')).toBeGreaterThan(0);
    // Every Polish corps near it goes away: nobody is left to tear up the track as he falls back.
    const away = g.regions.find((x) => x.ctrl === 'POL' && x.id !== held.id && !held.nb.includes(x.id) && !x.nb.some((j) => held.nb.includes(j)))!;
    for (const j of [held.id, ...held.nb]) for (const u of [...g.regions[j].units]) if (atWar(g, u.nat, 'GER')) relocate(g, u, away);
    expect(wreckChance(g, held, 'GER')).toBe(0);
  });
});
