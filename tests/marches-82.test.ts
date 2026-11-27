// Marches that carry on (Version 82): src/campaign/marches.ts, the one order both interfaces keep.
//
// Brian's ask of 2026-11-16, playing the 3D build: "if you click on place to move for a unit but they are out of
// moves, then it will wait to the next turn and auto move there". The design director's rulings 87 and 93:
//
// 1. A march to friendly ground beyond this month's reach is kept: the corps goes as far as it can at once and on
//    again as each of the player's months opens, and says so when it arrives.
// 2. An army with no moves left is given the same order, to the province next door as to any other, and sets off
//    as the next month opens. Its first leg always marches: the enemy's reach does not stop it, nor an attack on it.
// 3. From the second leg on it halts only for news: its next halt is ground the enemy can strike now and could not
//    when the order was given; or it was attacked since its last leg; or the way is closed. The line says which.
// 4. Friendly ground only: never an attack, never a neutral. With no moves left, enemy ground says why.
// 5. Any other order ends it; what it is to do if attacked does not.
// 6. Asking is not ordering: the plan and the state read the game and change nothing; what is written down for a
//    march is gone when the march is.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { advanceMarches, enemyReach, forgetMarch, marchOf, marchState, NO_MOVES, orderMarch, planMarch, settleMarches, whyNotMarch } from '../src/campaign/marches';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, orderMove, orderStance, runTurnAI, stepAI } from '../src/campaign/turn';
import type { BattleReport, Corps, Game } from '../src/campaign/types';

const ME = 'FRA';
const others = (g: Game): void => { let guard = 0; while (!humanUp(g) && guard++ < 400) stepAI(g); };
/** September 1939 on the wider world, France's player up with his first month in front of him. */
const START = ((): string => {
  const g = startOpen(createGame(25, ME));
  others(g);
  beginHuman(g);
  return saveGame(g);
})();
const game = (): Game => loadGame(START)!;
const corps = (g: Game, name: string): Corps => g.corps.find((c) => c.nat === ME && !c.dead && c.name === name)!;
const at = (g: Game, name: string): number => g.byName[name].id;
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;
/** The player hands his month over as the page does, the others play, and his next month opens with its marches. */
function nextMonth(g: Game, before?: () => void): ReturnType<typeof advanceMarches> {
  settleMarches(g);
  endHuman(g);
  others(g);
  beginHuman(g);
  before?.();
  return advanceMarches(g);
}
const kept = (g: Game): string[] => Object.keys(g.flags).filter((k) => k.startsWith('march') || k.startsWith('hot:'));
/** A battle in the month's reports in which this corps was attacked. */
function attackOn(g: Game, c: Corps): void {
  const id = (g.reports.length ? g.reports[g.reports.length - 1].id : 0) + 1;
  g.reports.push({
    id, turn: g.turn, week: 2, region: c.reg, regionName: where(g, c), attNat: 'GER', defNat: ME, lines: [], aPower: 1, dPower: 2, odds: 0.5, level: 'fail', taken: false, attLost: 900, defLost: 300, prisoners: 0,
    attackers: [{ name: 'I Corps', nat: 'GER', kind: 'inf', divs: 3, men: 30000, lost: 900, fate: '' }], defenders: [{ name: c.name, nat: c.nat, kind: c.kind, divs: 3, men: 30000, lost: 300, fate: '' }],
  } as unknown as BattleReport);
}

describe('a march too far for this month is kept', () => {
  it('goes as far as it can at once, goes on as each month opens, and says so when it arrives', () => {
    const g = game();
    const c = corps(g, 'Paris garrison');
    const plan = planMarch(g, c, at(g, 'Gascony'));
    expect(plan).toMatchObject({ ok: true, spent: false, now: false });
    expect(plan.months).toBeGreaterThan(1);
    expect(plan.road.map((i) => g.regions[i].name).pop()).toBe('Gascony');
    expect(plan.line).toBe(`Paris garrison marches here over about ${plan.months} months, going on by itself as each month opens.`);
    const order = orderMarch(g, c, at(g, 'Gascony'));
    expect(order.kept).toBe(true);
    expect(order.moved).toBeGreaterThan(0);
    expect(where(g, c)).not.toBe('Paris');
    expect(marchOf(g, c)).toBe(at(g, 'Gascony'));
    expect(marchState(g, c)).toMatchObject({ to: at(g, 'Gascony'), months: order.months });
    let news: ReturnType<typeof advanceMarches> = { ended: [], went: [] };
    for (let i = 0; i < 4 && !news.ended.length; i++) news = nextMonth(g);
    expect(where(g, c)).toBe('Gascony');
    expect(news.ended).toEqual([{ note: 'Paris garrison has arrived at Gascony.', region: at(g, 'Gascony'), corps: c.id, arrived: true, why: 'arrived' }]);
    expect(news.went).toContain(at(g, 'Gascony'));
    // The line is in the month's news, with its place, and nothing is left written down.
    expect(g.headlines.some((h) => h.kind === 'info' && h.nat === ME && h.region === at(g, 'Gascony') && h.text === 'Paris garrison has arrived at Gascony.')).toBe(true);
    expect(marchState(g, c)).toBeNull();
    expect(kept(g)).toEqual([]);
  });

  it('is an ordinary move where the place is in reach this month: nothing is kept', () => {
    const g = game();
    const c = corps(g, 'Paris garrison');
    expect(planMarch(g, c, at(g, 'Marseille'))).toMatchObject({ ok: true, now: true, months: 0, line: '' });
    expect(orderMarch(g, c, at(g, 'Marseille'))).toMatchObject({ kept: false, why: '' });
    expect(where(g, c)).toBe('Marseille');
    expect(kept(g)).toEqual([]);
  });
});

describe('an army with no moves left is given the order and sets off as the next month opens', () => {
  it('to the province next door, out of the enemy\'s reach: his own case at the front', () => {
    const g = game();
    const c = corps(g, '3rd Army');
    expect(where(g, c)).toBe('Lorraine');
    // Lorraine and the road out of it are under the guns: the enemy he can see could strike both.
    expect(enemyReach(g, (id) => g.regions[id].ctrl === ME).has(at(g, 'Lorraine'))).toBe(true);
    c.ap = 0;
    const plan = planMarch(g, c, at(g, 'Verdun'));
    expect(plan).toMatchObject({ ok: true, spent: true, now: false, months: 1, road: [at(g, 'Verdun')] });
    expect(`${NO_MOVES} Right-click: ${plan.line}`).toBe('No moves left this month. Right-click: 3rd Army marches here from next month, about a month.');
    expect(orderMarch(g, c, at(g, 'Verdun'))).toEqual({ kept: true, moved: 0, months: 1, why: '' });
    expect(where(g, c)).toBe('Lorraine');
    // He is attacked where he stands before the month is out. The first leg marches all the same.
    const news = nextMonth(g, () => attackOn(g, c));
    expect(where(g, c)).toBe('Verdun');
    expect(news.ended.map((e) => e.why)).toEqual(['arrived']);
    expect(kept(g)).toEqual([]);
  });

  it('towards the front: the second leg ends on ground the enemy could strike when the order was given, and it marches on', () => {
    const g = game();
    const c = corps(g, 'General reserve');
    c.ap = 0;
    expect(orderMarch(g, c, at(g, 'Lorraine')).kept).toBe(true);
    expect(g.flags[`marchLegs:${c.id}`]).toBe(0);
    expect(g.flags[`hot:${g.turn}:${at(g, 'Lorraine')}`]).toBe(1);
    // A short first leg, then the rest.
    expect(nextMonth(g, () => { c.ap = 1; }).ended).toEqual([]);
    expect(where(g, c)).toBe('Verdun');
    expect(g.flags[`marchLegs:${c.id}`]).toBe(1);
    const news = nextMonth(g);
    expect(where(g, c)).toBe('Lorraine');
    expect(news.ended.map((e) => e.why)).toEqual(['arrived']);
  });
});

describe('from the second leg on it halts only for news, and says which', () => {
  /** The reserve, a leg out on its way to Lorraine. */
  const onItsWay = (): { g: Game; c: Corps; ordered: number } => {
    const g = game();
    const c = corps(g, 'General reserve');
    c.ap = 0;
    const ordered = g.turn;
    orderMarch(g, c, at(g, 'Lorraine'));
    nextMonth(g, () => { c.ap = 1; });
    expect(where(g, c)).toBe('Verdun');
    return { g, c, ordered };
  };

  it('the enemy can strike its next halt now, and could not when the order was given', () => {
    const { g, c, ordered } = onItsWay();
    delete g.flags[`hot:${ordered}:${at(g, 'Lorraine')}`];
    const news = nextMonth(g);
    expect(where(g, c)).toBe('Verdun');
    expect(news.ended).toEqual([{ note: 'General reserve has halted at Verdun on its march to Lorraine: the enemy can now reach Lorraine, its next halt. It waits for orders.', region: at(g, 'Verdun'), corps: c.id, arrived: false, why: 'enemy' }]);
    expect(g.headlines[g.headlines.length - 1]).toMatchObject({ kind: 'info', nat: ME, region: at(g, 'Verdun'), text: news.ended[0].note });
    expect(kept(g)).toEqual([]);
  });

  it('it was attacked since its last leg', () => {
    const { g, c } = onItsWay();
    const news = nextMonth(g, () => attackOn(g, c));
    expect(where(g, c)).toBe('Verdun');
    expect(news.ended.map((e) => [e.why, e.note])).toEqual([['attacked', 'General reserve has halted at Verdun on its march to Lorraine: it was attacked. It waits for orders.']]);
  });

  it('the way is closed', () => {
    const { g, c } = onItsWay();
    // (Lorraine is lost in the month between.)
    const news = nextMonth(g, () => { g.regions[at(g, 'Lorraine')].ctrl = 'GER'; });
    expect(news.ended.map((e) => [e.why, e.note])).toEqual([['closed', 'General reserve has halted at Verdun on its march to Lorraine: the way is closed. It waits for orders.']]);
    expect(marchOf(g, c)).toBe(-1);
  });

  it('a march with no record of its legs, as one kept before the ruling, is read as past its first', () => {
    const { g, c } = onItsWay();
    delete g.flags[`marchLegs:${c.id}`];
    delete g.flags[`marchAt:${c.id}`];
    expect(nextMonth(g).ended.map((e) => e.why)).toEqual(['enemy']);
  });
});

describe('friendly ground only', () => {
  it('never an attack and never a neutral, and with no moves left enemy ground says why', () => {
    const g = game();
    const c = corps(g, '3rd Army');
    expect(whyNotMarch(g, c, at(g, 'Saar'))).toBe('A march is never kept onto enemy ground: an attack is planned in the month it is made, with the odds in front of you.');
    expect(whyNotMarch(g, c, at(g, 'Luxembourg'))).toBe('A kept march goes through friendly ground only.');
    c.ap = 0;
    expect(planMarch(g, c, at(g, 'Saar'))).toMatchObject({ ok: false, spent: true, line: 'No moves left this month. An attack is planned in the month it is made.' });
    expect(orderMarch(g, c, at(g, 'Saar'))).toMatchObject({ kept: false, moved: 0 });
    expect(orderMarch(g, c, at(g, 'Luxembourg')).why).toBe('A kept march goes through friendly ground only.');
    expect(orderMarch(g, c, c.reg).why).toBe('3rd Army is there already.');
    expect(kept(g)).toEqual([]);
  });

  it('is nobody\'s order but the player\'s: a war nobody plays keeps none', () => {
    const g = startOpen(createGame(25, null));
    for (let i = 0; i < 3; i++) runTurnAI(g);
    expect(advanceMarches(g)).toEqual({ ended: [], went: [] });
    expect(kept(g)).toEqual([]);
  });
});

describe('any other order ends it', () => {
  it('a move ends it as the month is handed over; what it is to do if attacked does not', () => {
    const g = game();
    const a = corps(g, 'Paris garrison'), b = corps(g, 'General reserve');
    b.ap = 0;
    orderMarch(g, b, at(g, 'Lorraine'));
    a.ap = 2;
    expect(orderMarch(g, a, at(g, 'Gascony')).kept).toBe(true);
    // The reserve is told to give ground if attacked: its march stands. The garrison is marched somewhere else.
    expect(orderStance(g, b, 'back')).toBe(true);
    a.ap = 4;
    expect(orderMove(g, a, at(g, 'Paris'))).toBeGreaterThan(0);
    settleMarches(g);
    expect(marchOf(g, b)).toBe(at(g, 'Lorraine'));
    expect(marchOf(g, a)).toBe(-1);
    // An interface may also end one as the order is given.
    forgetMarch(g, [b.id]);
    settleMarches(g);
    expect(kept(g)).toEqual([]);
  });

  it('a new march replaces the old one', () => {
    const g = game();
    const c = corps(g, 'General reserve');
    c.ap = 0;
    orderMarch(g, c, at(g, 'Lorraine'));
    orderMarch(g, c, at(g, 'Gascony'));
    expect(marchOf(g, c)).toBe(at(g, 'Gascony'));
    expect(kept(g).filter((k) => k.startsWith('march:'))).toEqual([`march:${c.id}`]);
  });
});

describe('asking is not ordering', () => {
  it('the plan, the state and the enemy\'s reach read the game and change nothing', () => {
    const g = game();
    const c = corps(g, 'General reserve');
    c.ap = 0;
    orderMarch(g, c, at(g, 'Lorraine'));
    const was = saveGame(g);
    planMarch(g, c, at(g, 'Gascony'));
    planMarch(g, corps(g, '3rd Army'), at(g, 'Saar'));
    marchState(g, c);
    enemyReach(g, () => true);
    expect(saveGame(g)).toBe(was);
  });

  it('a kept march is kept with the saved game', () => {
    const g = game();
    const c = corps(g, 'General reserve');
    c.ap = 0;
    orderMarch(g, c, at(g, 'Lorraine'));
    const back = loadGame(saveGame(g))!;
    expect(marchState(back, corps(back, 'General reserve'))).toEqual(marchState(g, c));
    nextMonth(back, () => { corps(back, 'General reserve').ap = 1; });
    expect(where(back, corps(back, 'General reserve'))).toBe('Verdun');
  });
});
