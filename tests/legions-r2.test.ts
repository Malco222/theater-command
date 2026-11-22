// Volunteer legions (release 2; Brian's yes of 22 November 2026, the design director's frame of 03:42 UTC and rulings
// of 03:47 and 03:55 UTC). A country at peace outside both alliances sends one legion at a time, a division at most, to
// fight in a patron's army under its own name and flag; its men come from its home's reserve, and go home at the
// tour's end or on recall. Switched by stage.json 'legions': off, no campaign carries any of it.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import {
  LEGIONS, attackedBy, friendAttacked, genericEst, landing, legionDrafts, legionFeeds, legionOffers, legionRules, legionsMonth, legionsView,
  recallLegion, sendLegion, startLegions, whyNoLegion,
} from '../src/campaign/legions';
import { bargainsMonth, patienceOf, startBargains } from '../src/campaign/bargains';
import { PRODUCTION } from '../src/campaign/economy';
import { atWar, createGame, removeCorps } from '../src/campaign/game';
import { setStop, stopOf } from '../src/campaign/history';
import { menLeft } from '../src/campaign/manpower';
import { loadGame, saveGame } from '../src/campaign/save';
import { LEGIONS_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, orderLegion, orderRecall, stepAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';

/** A grand campaign with the legions on, at this stop of the dial, nobody at the table. */
function world(stop = 1, seed = 1): Game {
  const g = startOpen(createGame(seed, null));
  if (stop !== stopOf(g)) setStop(g, stop, true);
  startLegions(g);
  return g;
}

/** A grand campaign with the legions on and this country at the table, at its first month of orders. */
function playing(nat: string, stop = 1, seed = 1): Game {
  const g = startOpen(createGame(seed, nat));
  if (stop !== stopOf(g)) setStop(g, stop, true);
  startLegions(g);
  let guard = 0;
  while (!humanUp(g) && guard++ < 200) stepAI(g);
  beginHuman(g);
  return g;
}

function month(g: Game): void {
  endHuman(g);
  let guard = 0;
  while (!humanUp(g) && guard++ < 400) stepAI(g);
  beginHuman(g);
}

const war = (g: Game, a: string, b: string, opened = true): void => {
  if (!atWar(g, a, b)) { g.nations[a].atWar.push(b); g.nations[b].atWar.push(a); }
  if (opened) g.flags[`opening:${a}:${b}`] = g.turn - 1;
  if (opened && g.nations[b].align !== 'allies' && g.nations[b].align !== 'axis') g.flags[`shock:${b}`] = g.turn - 1;
  // (Germany and the Soviet Union share no border on the first day: Poland's eastern ground is given to the Soviet Union, as in the record.)
  if (a === 'GER' && b === 'SOV') for (const r of g.regions) if (r.owner === 'POL' && r.sov) r.ctrl = 'SOV';
  if (a === 'GER' && b === 'SOV') for (const r of g.regions) if (r.owner === 'POL' && !r.sov) r.ctrl = 'GER';
};
const peace = (g: Game, a: string, b: string): void => {
  g.nations[a].atWar = g.nations[a].atWar.filter((x) => x !== b);
  g.nations[b].atWar = g.nations[b].atWar.filter((x) => x !== a);
};
const legionOf = (g: Game, home: string): Corps | undefined => g.corps.find((c) => c.vol === home && !c.dead);
const r2 = (x: number): number => Math.round(x * 1000) / 1000;

describe('the switch', () => {
  it('is off in release 1: no campaign carries the legions', () => {
    expect((stage as { legions?: boolean }).legions).toBe(false);
    if (LEGIONS_STAGE) return; // (a run with TC_LEGIONS set)
    const g = startOpen(createGame(1, null));
    expect(g.legions).toBeUndefined();
    expect(g.flags.legions).toBeUndefined();
    expect(legionRules(g)).toBe(false);
    g.turn = 22;
    war(g, 'GER', 'SOV');
    legionsMonth(g);
    expect(g.legions).toBeUndefined();
    expect(g.corps.some((c) => c.vol)).toBe(false);
    expect(legionsView(g)).toEqual([]);
    expect(legionFeeds(g)).toEqual([]);
    expect(legionOffers(g, 'ESP')).toEqual([]);
    expect(sendLegion(g, 'ESP', 'GER', 'SOV')).not.toBeNull();
  });

  it('never reaches a history campaign', () => {
    const g = createGame(1, null);
    startLegions(g);
    expect(g.legions).toBeUndefined();
    expect(legionRules(g)).toBe(false);
  });

  it('a save with the switch off is written as it always was', () => {
    if (LEGIONS_STAGE) return;
    const g = startOpen(createGame(1, null));
    expect(saveGame(g)).not.toContain('"legions"');
  });
});

describe('the record', () => {
  it('the historian\'s rows: the Blue Division, the Blue Legion after it, the Swedish volunteers in Finland', () => {
    const by = Object.fromEntries(LEGIONS.rows.map((r) => [r.id, r]));
    expect(by['esp-blue']).toMatchObject({ home: 'ESP', patron: 'GER', against: 'SOV', est: 1, from: 23, to: 51, then: 'esp-legion' });
    expect(by['esp-legion']).toMatchObject({ home: 'ESP', patron: 'GER', against: 'SOV', est: 0.12, from: 52, to: 55 });
    expect(by['swe-corps']).toMatchObject({ home: 'SWE', patron: 'FIN', against: 'SOV', est: 0.4, from: 6, to: 7 });
    expect(LEGIONS).toMatchObject({ cost: 6, send: 50, leanAt: 30, backAt: 20, tour: 24, train: 0.25, legionFall: 2 });
  });

  it('the Swedish volunteers: sent in January 1940, in Finland\'s line February and March, home in April', () => {
    const g = world(1);
    g.turn = 5;
    war(g, 'SOV', 'FIN');
    const pool = menLeft(g, 'SWE');
    legionsMonth(g);
    expect(g.legions!.abroad.SWE).toMatchObject({ corps: -1, patron: 'FIN', against: 'SOV', name: 'Swedish Volunteer Corps', since: 6, until: 7, row: 'swe-corps' });
    expect(r2(pool - menLeft(g, 'SWE'))).toBe(0.4);
    g.turn = 6;
    legionsMonth(g);
    const c = legionOf(g, 'SWE')!;
    expect(c).toBeDefined();
    expect(c.nat).toBe('FIN');
    expect(c.name).toBe('Swedish Volunteer Corps');
    expect(c.str).toBe(0.4);
    expect(c.full).toBe(0.4);
    // On Finland's ground on the front against the Soviet Union.
    const at = g.regions[c.reg];
    expect(at.ctrl).toBe('FIN');
    expect(at.nb.some((j) => g.regions[j].ctrl === 'SOV')).toBe(true);
    expect(atWar(g, 'SWE', 'SOV')).toBe(false);
    g.turn = 7;
    legionsMonth(g);
    expect(legionOf(g, 'SWE')).toBeDefined();
    // Home in April: the men go back to the reserve, and the army at home learns from them.
    c.xp = 0.6;
    const home = g.corps.filter((k) => k.nat === 'SWE' && k.kind !== 'fort');
    const before = home.map((k) => k.xp);
    g.turn = 8;
    legionsMonth(g);
    expect(legionOf(g, 'SWE')).toBeUndefined();
    expect(g.legions!.abroad.SWE).toBeUndefined();
    expect(g.legions!.done['swe-corps']).toBe(8);
    expect(r2(menLeft(g, 'SWE'))).toBe(r2(pool));
    home.forEach((k, i) => {
      expect(k.xp).toBeGreaterThanOrEqual(before[i]);
      expect(k.xp).toBeLessThanOrEqual(Math.max(before[i], 0.6));
      if (before[i] < 0.6) expect(r2(k.xp)).toBe(r2(Math.min(0.6, before[i] + 0.25 * 0.6)));
    });
    expect(g.headlines.some((h) => /Swedish Volunteer Corps comes home/.test(h.text))).toBe(true);
    // Not sent twice.
    g.turn = 6;
    legionsMonth(g);
    expect(g.legions!.abroad.SWE).toBeUndefined();
  });

  it('the Blue Division: in Germany\'s army with its own number, then the Blue Legion with the men who stayed, then home', () => {
    const g = world(1);
    g.turn = 22;
    war(g, 'GER', 'SOV');
    const pool = menLeft(g, 'ESP'), counter = g.flags['dn:GER:inf'];
    legionsMonth(g);
    g.turn = 23;
    legionsMonth(g);
    const c = legionOf(g, 'ESP')!;
    expect(c.nat).toBe('GER');
    expect(c.name).toBe('Blue Division');
    expect(c.dn).toBe(250);
    expect(c.str).toBe(1);
    expect(g.flags['dn:GER:inf']).toBe(counter);
    expect(r2(pool - menLeft(g, 'ESP'))).toBe(1);
    expect(g.legions!.abroad.ESP.until).toBe(51);
    // Its tour runs to November 1943.
    g.turn = 51;
    legionsMonth(g);
    expect(legionOf(g, 'ESP')?.name).toBe('Blue Division');
    c.str = 0.9;
    const mid = menLeft(g, 'ESP');
    g.turn = 52;
    legionsMonth(g);
    expect(c.dead).toBe(false);
    expect(c.name).toBe('Blue Legion');
    expect(c.str).toBe(0.12);
    expect(c.full).toBe(0.12);
    expect(r2(menLeft(g, 'ESP') - mid)).toBe(0.78);
    expect(g.legions!.abroad.ESP).toMatchObject({ row: 'esp-legion', until: 55 });
    expect(g.legions!.done['esp-blue']).toBe(52);
    g.turn = 56;
    legionsMonth(g);
    expect(legionOf(g, 'ESP')).toBeUndefined();
    expect(g.legions!.done['esp-legion']).toBe(56);
  });

  it('at stops 1 and 2 no other legion is sent', () => {
    const g = world(2);
    g.turn = 10;
    g.nations.ESP.opinion = 90;
    g.nations.ESP.pc = 30;
    legionsMonth(g);
    expect(g.legions!.abroad).toEqual({});
  });

  it('a campaign opened while the record\'s legion is in the line has it there on the first day', () => {
    const g = startOpen(createGame(1, null));
    if (stopOf(g) !== 1) setStop(g, 1, true);
    g.turn = 30;
    war(g, 'GER', 'SOV');
    startLegions(g);
    const c = legionOf(g, 'ESP');
    expect(c?.name).toBe('Blue Division');
    expect(g.legions!.abroad.ESP).toMatchObject({ since: 30, until: 51 });
  });
});

describe('in the field', () => {
  function blue(): { g: Game; c: Corps } {
    const g = world(1);
    g.turn = 22;
    war(g, 'GER', 'SOV');
    legionsMonth(g);
    g.turn = 23;
    legionsMonth(g);
    return { g, c: legionOf(g, 'ESP')! };
  }

  it('its drafts come from its home\'s reserve, equipped at the patron\'s cost', () => {
    const { g, c } = blue();
    c.str = 0.6;
    const esp = menLeft(g, 'ESP'), ger = menLeft(g, 'GER');
    g.nations.GER.pp = 100;
    legionDrafts(g, c, 0.2);
    expect(r2(c.str)).toBe(0.8);
    expect(r2(esp - menLeft(g, 'ESP'))).toBe(0.2);
    expect(menLeft(g, 'GER')).toBe(ger);
    expect(r2(100 - g.nations.GER.pp)).toBe(r2(0.2 * PRODUCTION.replacementCost));
  });

  it('a home whose reserve is spent sends no drafts', () => {
    const { g, c } = blue();
    c.str = 0.6;
    g.flags['mp:ESP'] = 0;
    legionDrafts(g, c, 0.2);
    expect(c.str).toBe(0.6);
  });

  it('destroyed in the field: its men are lost, with news at home', () => {
    const { g, c } = blue();
    const esp = menLeft(g, 'ESP');
    removeCorps(g, c);
    legionsMonth(g);
    expect(g.legions!.abroad.ESP).toBeUndefined();
    expect(menLeft(g, 'ESP')).toBe(esp);
    expect(g.headlines.some((h) => /Blue Division.*destroyed/.test(h.text))).toBe(true);
  });

  it('comes home when its patron makes peace with the enemy it was sent to fight', () => {
    const { g } = blue();
    peace(g, 'GER', 'SOV');
    g.turn = 24;
    legionsMonth(g);
    expect(legionOf(g, 'ESP')).toBeUndefined();
    expect(g.headlines.some((h) => /comes home.*at peace/.test(h.text))).toBe(true);
  });

  it('comes home when its own country goes to war: the home stays out of its patron\'s wars until then', () => {
    const { g } = blue();
    expect(atWar(g, 'ESP', 'SOV')).toBe(false);
    war(g, 'ESP', 'UK');
    g.turn = 24;
    legionsMonth(g);
    expect(legionOf(g, 'ESP')).toBeUndefined();
  });

  it('fights the great power it was sent against: that power loses patience with its home (bargains on)', () => {
    const base = (withLegion: boolean): number => {
      const g = world(1);
      startBargains(g);
      g.turn = 22;
      war(g, 'GER', 'SOV');
      if (withLegion) { legionsMonth(g); g.turn = 23; legionsMonth(g); } else g.turn = 23;
      const was = patienceOf(g, 'SOV', 'ESP');
      bargainsMonth(g);
      return was - patienceOf(g, 'SOV', 'ESP');
    };
    expect(base(true) - base(false)).toBe(LEGIONS.legionFall);
  });

  it('is listed for every viewer: whose, with whom, against whom, how strong and for how long', () => {
    const { g, c } = blue();
    const v = legionsView(g);
    expect(v).toHaveLength(1);
    expect(v[0]).toMatchObject({ home: 'ESP', patron: 'GER', against: 'SOV', name: 'Blue Division', corps: c.id, region: c.reg, men: 15000, est: 1, until: 51, monthsLeft: 29, record: true, recall: false });
    expect(legionFeeds(g)).toEqual([{ power: 'SOV', home: 'ESP', name: 'the Blue Division' }]);
  });

  it('survives a save', () => {
    const { g, c } = blue();
    const back = loadGame(saveGame(g))!;
    expect(back.legions).toEqual(g.legions);
    const k = back.corps.find((x) => x.id === c.id)!;
    expect(k.vol).toBe('ESP');
    expect(legionRules(back)).toBe(true);
  });
});

describe('the computer\'s judgment at stops 3 to 5', () => {
  it('a small country\'s volunteers are few: a tenth of its reserve, a division at most', () => {
    const g = world(3);
    expect(genericEst(g, 'ESP')).toBe(0.75);
    expect(genericEst(g, 'DEN')).toBe(0.3);
    expect(genericEst(g, 'LUX')).toBe(0.2);
    expect(genericEst(g, 'BEL')).toBe(1);
  });


  it('a country leaning far toward a side sends its legion to that side\'s front, for political capital', () => {
    const g = world(3);
    // (Overland, through ground held by nobody at war with the patron: here France has made its peace.)
    peace(g, 'GER', 'FRA');
    g.nations.ESP.opinion = 80;
    g.nations.ESP.pc = 10;
    legionsMonth(g);
    const L = g.legions!.abroad.ESP;
    expect(L).toBeDefined();
    expect(L.patron).toBe('GER');
    expect(L.against).toBe('POL');
    expect(L.name).toBe('Spanish Volunteer Legion');
    // A legion the record does not name is a share of its country's reserve: Spain's, three quarters of a division.
    expect(L.est).toBe(0.75);
    expect(L.until).toBe(L.since + LEGIONS.tour - 1);
    expect(g.nations.ESP.pc).toBe(4);
    g.turn++;
    legionsMonth(g);
    expect(legionOf(g, 'ESP')?.nat).toBe('GER');
    // Its leaning falls away: it calls them home.
    g.nations.ESP.opinion = 10;
    g.turn++;
    legionsMonth(g);
    expect(legionOf(g, 'ESP')).toBeUndefined();
  });

  it('volunteers go only where they can get to: not across an enemy of the patron', () => {
    const g = world(3);
    g.nations.ESP.opinion = 80;
    g.nations.ESP.pc = 10;
    expect(whyNoLegion(g, 'ESP', 'GER', 'POL', false)).toMatch(/no front .* for volunteers to reach/);
    legionsMonth(g);
    // (Slovakia's front with Poland can be reached through France, which is not at war with Slovakia.)
    expect(g.legions!.abroad.ESP?.patron).not.toBe('GER');
  });

  it('a patron takes one legion at a time', () => {
    const g = world(3);
    peace(g, 'GER', 'FRA');
    for (const n of ['ESP', 'POR']) { g.nations[n].opinion = 80; g.nations[n].pc = 10; }
    legionsMonth(g);
    const sent = Object.values(g.legions!.abroad).filter((L) => L.patron === 'GER');
    expect(sent).toHaveLength(1);
    expect(whyNoLegion(g, sent[0] === g.legions!.abroad.ESP ? 'POR' : 'ESP', 'GER', 'POL', false)).toMatch(/one legion at a time/);
  });

  it('a country leaning only a little sends none', () => {
    const g = world(3);
    g.nations.ESP.opinion = 40;
    g.nations.ESP.pc = 10;
    legionsMonth(g);
    expect(g.legions!.abroad.ESP).toBeUndefined();
  });

  it('a friend attacked: Sweden sends its volunteers to Finland, under the record\'s name', () => {
    const g = world(3);
    g.turn = 4;
    war(g, 'SOV', 'FIN');
    expect(attackedBy(g, 'FIN', 'SOV')).toBe(true);
    expect(friendAttacked(g, 'SWE', 'FIN', 'SOV')).toBe(true);
    g.nations.SWE.pc = 10;
    g.nations.SWE.opinion = 0;
    legionsMonth(g);
    expect(g.legions!.abroad.SWE).toMatchObject({ patron: 'FIN', against: 'SOV', name: 'Swedish Volunteer Corps', est: 0.4 });
    // A friend still attacked keeps them there, whatever the leaning.
    g.turn++;
    legionsMonth(g);
    g.turn++;
    legionsMonth(g);
    expect(legionOf(g, 'SWE')?.nat).toBe('FIN');
  });

  it('a friend attacked by the side its public leans toward gets no volunteers', () => {
    const g = world(3);
    war(g, 'GER', 'DEN');
    g.nations.SWE.opinion = 40;
    expect(friendAttacked(g, 'SWE', 'DEN', 'GER')).toBe(false);
    g.nations.SWE.opinion = 0;
    expect(friendAttacked(g, 'SWE', 'DEN', 'GER')).toBe(true);
  });

  it('a great power sends none', () => {
    const g = world(3);
    g.nations.USA.opinion = -90;
    g.nations.USA.pc = 30;
    legionsMonth(g);
    expect(g.legions!.abroad.USA).toBeUndefined();
    expect(whyNoLegion(g, 'USA', 'UK', 'GER')).toMatch(/great power/);
  });
});

describe('the player\'s legion', () => {
  it('sent from the trade office for political capital, called home again', () => {
    const g = playing('ESP', 3);
    peace(g, 'GER', 'FRA');
    g.nations.ESP.opinion = 40;
    g.nations.ESP.pc = 20;
    const offer = legionOffers(g, 'ESP').find((o) => o.patron === 'GER' && o.why === null);
    expect(offer).toBeDefined();
    expect(landing(g, 'ESP', 'GER', offer!.against)).not.toBeNull();
    expect(orderLegion(g, 'GER', offer!.against)).toBeNull();
    expect(g.nations.ESP.pc).toBe(14);
    expect(orderLegion(g, 'GER', offer!.against)).toMatch(/one legion at a time/);
    // Called back before it reaches the line: the men are home at once.
    const pool = menLeft(g, 'ESP');
    expect(orderRecall(g)).toBeNull();
    expect(g.legions!.abroad.ESP).toBeUndefined();
    expect(r2(menLeft(g, 'ESP') - pool)).toBe(r2(genericEst(g, 'ESP')));
    expect(orderLegion(g, 'GER', offer!.against)).toBeNull();
    month(g);
    const c = legionOf(g, 'ESP');
    expect(c?.nat).toBe('GER');
    expect(orderRecall(g)).toBeNull();
    expect(legionsView(g)[0].recall).toBe(true);
    month(g);
    expect(legionOf(g, 'ESP')).toBeUndefined();
  });

  it('only toward a side his public leans to, or for a friend attacked', () => {
    const g = playing('ESP', 3);
    g.nations.ESP.opinion = 10;
    g.nations.ESP.pc = 20;
    expect(whyNoLegion(g, 'ESP', 'GER', 'FRA')).toMatch(/does not lean/);
    g.nations.ESP.opinion = 40;
    g.nations.ESP.pc = 3;
    expect(whyNoLegion(g, 'ESP', 'GER', 'FRA')).toMatch(/political capital/);
  });

  it('a computer-run country at the record\'s stops does not send the player\'s country\'s legion for him', () => {
    const g = playing('SWE', 1);
    g.turn = 5;
    war(g, 'SOV', 'FIN');
    legionsMonth(g);
    expect(g.legions!.abroad.SWE).toBeUndefined();
  });
});
