// Guns never go forward alone, Britain stands by its dominions, and the town guard tears up no railway (Version
// 84.2.5; the design director's ruling 211, from the Playtester's trace of Brian's own campaign of 2026-11-17: one
// Canadian corps of guns with no infantry stormed Buffalo and Pittsburgh and would have taken Washington, while
// Britain looked on at a war made on her dominion). The grand campaign only: history's campaigns raise no guns.
//
// (a) A force with no fighting corps in it (infantry, cavalry, mountain troops, motorized troops or tanks) opens no
//     attack, storms no town, makes no sortie, lands on no enemy shore and marches onto no enemy-held ground unless
//     a fighting corps of its side stands there already; left alone round a town it gives up the siege; and guns
//     caught alone in the field are overrun. Player and computer alike, and the refusal is a sentence.
// (b) War on Canada is war with Britain the same month, whoever declares it; a war Canada starts is not Britain's.
// (g) A railway is torn up by troops as they fall back: where the loser has no corps next door, nobody does it.
import { describe, expect, it } from 'vitest';
import { notTanksAlone } from '../src/campaign/ai';
import { warForecast } from '../src/campaign/diplomacy';
import {
  ENGINEERS_ALONE, GUNS_ALONE, addCorps, allied, assess, atWar, createGame, declareWar, fights, gunsRule, hostileUnits, limitAttackers, moveCorps, relocate, setWar, standsByWhom,
} from '../src/campaign/game';
import { whyNotLand } from '../src/campaign/navy';
import { onNet, wreckChance } from '../src/campaign/rails';
import { besieged, garrison, siegeMonth, stormForce, townHeld, whyNotStorm } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, marchOrder, orderAttack, previewAttack, reach, stepAI, whyNotLead } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';
import { worldMonth } from '../src/campaign/world';

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
    expect(whyNotLead(g, [eng, gun], to.id)).toBe(GUNS_ALONE);
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
    const gun = gunCorps(g, 'POL', to, 'Polish test artillery');
    const a = previewAttack(g, [inf], to)!;
    expect(a).not.toBeNull();
    expect(a.overrun).toBe(true);
    expect(a.odds * a.dice[0]).toBeGreaterThanOrEqual(3);
    expect(a.lines.find((l) => l.label === 'Guns alone')).toEqual({ label: 'Guns alone', value: 'The guns stand with no infantry or tanks in front of them', effect: 'They are overrun: every gun is taken' });
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
    relocate(g, inf, from);
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
    expect(said).toMatch(/The German guns round .+, left with no infantry or tanks beside them, give up the siege and fall back to /);
  });

  it('guns land on no enemy shore by themselves', () => {
    const g = playing();
    const { from, to, inf } = front(g);
    const gun = gunCorps(g, 'GER', from);
    expect(whyNotLand(g, [gun], to)).toBe(GUNS_ALONE);
    expect(whyNotLand(g, [inf], to)).not.toBe(GUNS_ALONE);
  });
});

describe('Britain stands by its dominions, one way only (Version 84.2.5, ruling 211(b))', () => {
  it('war on Canada is war with Britain the same month, with her headline; France and the rest stay as they were', () => {
    const g = playing('USA');
    expect(allied(g, 'UK', 'CAN')).toBe(true);
    expect(atWar(g, 'USA', 'UK')).toBe(false);
    expect(standsByWhom(g, 'USA', 'CAN')).toBe('UK');
    expect(warForecast(g, 'USA', 'CAN').patron).toBe('UK');
    expect(warForecast(g, 'USA', 'MEX').patron).toBeNull();
    const align = g.nations.USA.align;
    const h = g.headlines.length;
    declareWar(g, 'USA', 'CAN');
    expect(atWar(g, 'USA', 'CAN')).toBe(true);
    expect(atWar(g, 'USA', 'UK')).toBe(true);
    expect(g.flags['war:UK:USA']).toBe(g.turn);
    const news = g.headlines.slice(h).find((x) => x.warn === 'Britain stands by Canada')!;
    expect(news).toBeDefined();
    expect(news.kind).toBe('war');
    expect(news.nat).toBe('UK');
    expect(news.text).toBe('Britain declares war on the United States: war on a dominion is war with Britain.');
    // One member alone: nobody else of the alliance, and America stands where it stood.
    expect(atWar(g, 'USA', 'FRA')).toBe(false);
    expect(g.nations.USA.align).toBe(align);
    expect(standsByWhom(g, 'USA', 'CAN')).toBeNull();
  });

  it('a war a dominion starts is not Britain\'s', () => {
    const g = playing('USA');
    declareWar(g, 'CAN', 'MEX');
    expect(atWar(g, 'CAN', 'MEX')).toBe(true);
    expect(atWar(g, 'UK', 'MEX')).toBe(false);
    worldMonth(g);
    expect(atWar(g, 'UK', 'MEX')).toBe(false);
  });

  it('a campaign saved before the rule: Britain answers as the month ends, and says since when', () => {
    const g = playing('USA');
    // The war as an older build left it: made on Canada two months ago, and Britain at peace.
    setWar(g, 'USA', 'CAN');
    g.flags['war:USA:CAN'] = g.turn;
    g.turn += 2;
    expect(atWar(g, 'USA', 'UK')).toBe(false);
    const h = g.headlines.length;
    worldMonth(g);
    expect(atWar(g, 'USA', 'UK')).toBe(true);
    const news = g.headlines.slice(h).find((x) => x.warn === 'Britain stands by Canada')!;
    expect(news.text).toMatch(/^The United States has been at war with Canada since (September|October|November|December) 1939\. Britain declares war on the United States: war on a dominion is war with Britain\.$/);
    // Once: the next month says nothing more.
    const k = g.headlines.length;
    worldMonth(g);
    expect(g.headlines.slice(k).some((x) => x.warn === 'Britain stands by Canada')).toBe(false);
  });

  it('not in World Domination, where a dominion follows nobody either', () => {
    const g = playing('USA');
    g.flags.dom = 1;
    expect(standsByWhom(g, 'USA', 'CAN')).toBeNull();
    declareWar(g, 'USA', 'CAN');
    expect(atWar(g, 'USA', 'UK')).toBe(false);
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
