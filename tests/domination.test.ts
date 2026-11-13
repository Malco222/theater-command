// World Domination, the last act of the grand campaign: when the war between the alliances is decided, the
// choice the player is given, what dissolving the alliances does, how the staffs fight it, and how it ends.
import { describe, expect, it } from 'vitest';
import { DIPLOMACY } from '../src/campaign/diplomacy';
import {
  DOMINATION, contenders, crown, dominating, keepPeace, startDomination, warDecided, whyNotDomination, worldBoard, worldWinner,
} from '../src/campaign/domination';
import { allied, atWar, bloc, capitulate, createGame, fieldArmy } from '../src/campaign/game';
import { fogOn } from '../src/campaign/intel';
import { pathOptions } from '../src/campaign/league';
import { loadGame, saveGame } from '../src/campaign/save';
import { fronts, startOpen, temper } from '../src/campaign/staff';
import {
  beginHuman, declarable, endHuman, humanUp, lastAct, orderDomination, orderKeepPeace, paths, runTurnAI, standings, stepAI, whyNotOrderDomination,
} from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const grand = (human: string | null = 'UK', seed = 3): Game => startOpen(createGame(seed, human));

function war(g: Game, a: string, b: string): void {
  if (!g.nations[a].atWar.includes(b)) g.nations[a].atWar.push(b);
  if (!g.nations[b].atWar.includes(a)) g.nations[b].atWar.push(a);
}
/** A grand campaign in the player's first month. */
function playing(human = 'UK', seed = 3): Game {
  const g = grand(human, seed);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** The same, with Germany beaten and Italy never in the war: the Axis has lost its fight. */
function decided(human = 'UK', seed = 3): Game {
  const g = playing(human, seed);
  capitulate(g, 'GER', 'UK');
  return g;
}
/** The player's next month, with every other nation's month played by the computer in between. */
function nextMonth(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
}

describe('the data', () => {
  it('has the staffs strike at shorter odds, and sends guests home in some disorder', () => {
    expect(DOMINATION.nerve.attackAt).toBeGreaterThan(0.5);
    expect(DOMINATION.nerve.attackAt).toBeLessThan(1);
    expect(DOMINATION.nerve.persist).toBeGreaterThan(0.5);
    expect(DOMINATION.nerve.persist).toBeLessThanOrEqual(1);
    expect(DOMINATION.guests.org).toBeGreaterThan(0);
    expect(DOMINATION.guests.org).toBeLessThan(1);
    for (const line of Object.values(DOMINATION.lines)) if (typeof line === 'string') expect(line.length).toBeGreaterThan(20);
  });
});

describe('when the war is decided', () => {
  it('never is in a historical scenario', () => {
    const g = createGame(3, 'UK');
    capitulate(g, 'GER', 'UK');
    expect(warDecided(g)).toBeNull();
    expect(whyNotDomination(g, 'UK')).toMatch(/grand campaign/);
    expect(lastAct(g)).toBeNull();
    expect(startDomination(g, 'UK')).toBe(false);
    expect(dominating(g)).toBe(false);
  });

  it('is not while every great power stands, nor while two of them still fight', () => {
    const g = playing();
    expect(warDecided(g)).toBeNull();
    expect(whyNotDomination(g)).toMatch(/not decided/);
    expect(lastAct(g)).toMatchObject({ decided: null, open: false, on: false, peace: 0, winner: null });
    // A small country going under decides nothing.
    capitulate(g, 'POL', 'GER');
    expect(warDecided(g)).toBeNull();
    // France falls: Britain fights on.
    capitulate(g, 'FRA', 'GER');
    expect(atWar(g, 'UK', 'GER')).toBe(true);
    expect(warDecided(g)).toBeNull();
  });

  it('is when the last great power still fighting the others has fallen', () => {
    const g = decided();
    expect(g.nations.ITA.alive).toBe(true);
    expect(warDecided(g)).toEqual({ beaten: ['axis'], victor: 'allies', fallen: ['GER'], standing: expect.arrayContaining(['ITA', 'UK', 'FRA', 'SOV']) });
    expect(whyNotDomination(g)).toBeNull();
    expect(lastAct(g)).toMatchObject({ open: true, why: null, on: false, peace: 0 });
    expect(lastAct(g)!.left).toBe(contenders(g).length);
  });

  it('waits for an ally of the beaten power that is still in the fight', () => {
    const g = playing();
    war(g, 'ITA', 'UK');
    capitulate(g, 'GER', 'UK');
    expect(warDecided(g)).toBeNull();
    capitulate(g, 'ITA', 'UK');
    expect(warDecided(g)).toMatchObject({ beaten: ['axis'], victor: 'allies', fallen: ['GER', 'ITA'] });
  });

  it('can go the other way, and can leave neither alliance standing', () => {
    const g = playing('GER');
    capitulate(g, 'FRA', 'GER');
    g.turn += 1;
    capitulate(g, 'UK', 'GER');
    expect(warDecided(g)).toMatchObject({ beaten: ['allies'], victor: 'axis', fallen: ['FRA', 'UK'] });
    // The Soviet Union, outside both, brings the victors down in their turn.
    expect(bloc(g.nations.SOV)).toBeNull();
    capitulate(g, 'GER', 'SOV');
    capitulate(g, 'ITA', 'SOV');
    const d = warDecided(g)!;
    expect(d.beaten.sort()).toEqual(['allies', 'axis']);
    expect(d.victor).toBeNull();
    expect(d.standing).toEqual(['SOV']);
  });

  it('names the alliance of the last great power to fall when both have lost one', () => {
    const g = playing();
    capitulate(g, 'FRA', 'GER');
    g.turn += 3;
    capitulate(g, 'GER', 'UK');
    // France fell first and Britain still stands: it is the Axis that is beaten.
    expect(warDecided(g)).toMatchObject({ beaten: ['axis'], victor: 'allies' });
  });
});

describe('the choice', () => {
  it('is the player\'s alone, in his own month', () => {
    const g = decided();
    expect(whyNotOrderDomination(g)).toBeNull();
    endHuman(g);
    expect(whyNotOrderDomination(g)).toMatch(/your own month/);
    expect(orderDomination(g)).toBe(false);
    expect(orderKeepPeace(g)).toBe(false);
    expect(dominating(g)).toBe(false);
    const watched = grand(null);
    capitulate(watched, 'GER', 'UK');
    expect(warDecided(watched)).not.toBeNull();
    expect(lastAct(watched)).toBeNull();
    expect(whyNotOrderDomination(watched)).toMatch(/Nobody/);
    expect(whyNotDomination(watched)).toMatch(/out of the war/);
    expect(startDomination(watched)).toBe(false);
  });

  it('can be to take the victory, which changes nothing and closes nothing', () => {
    const g = decided();
    const before = saveGame(g);
    expect(orderKeepPeace(g)).toBe(true);
    expect(g.flags['dom:peace']).toBe(g.turn);
    expect(lastAct(g)).toMatchObject({ open: true, on: false, peace: g.turn });
    // Nothing else moved.
    const h = loadGame(before)!;
    expect(Object.values(g.nations).map((n) => [n.id, n.align, n.alive, [...n.atWar].sort()])).toEqual(Object.values(h.nations).map((n) => [n.id, n.align, n.alive, [...n.atWar].sort()]));
    expect(keepPeace(g)).toBe(false);
    nextMonth(g);
    expect(whyNotOrderDomination(g)).toBeNull();
    expect(orderDomination(g)).toBe(true);
    expect(dominating(g)).toBe(true);
  });

  it('is refused to a nation that has fallen', () => {
    const g = decided();
    capitulate(g, 'UK', 'ITA');
    expect(whyNotDomination(g)).toMatch(/out of the war/);
    expect(startDomination(g)).toBe(false);
  });
});

describe('dissolving the alliances', () => {
  it('sets every nation still standing against every other', () => {
    const g = decided();
    const before = contenders(g).map((n) => n.id);
    const news = g.headlines.length;
    expect(allied(g, 'UK', 'FRA')).toBe(true);
    expect(orderDomination(g)).toBe(true);
    const live = contenders(g);
    expect(live.map((n) => n.id)).toEqual(before);
    expect(g.flags.dom).toBe(g.turn);
    expect(g.flags['dom:n']).toBe(live.length);
    for (const a of live) {
      expect(a.align, a.id).toBe('independent');
      expect(a.league, a.id).toBeFalsy();
      expect(bloc(a), a.id).toBeNull();
      for (const b of live) {
        if (a === b) continue;
        expect(atWar(g, a.id, b.id), `${a.id} and ${b.id}`).toBe(true);
        expect(allied(g, a.id, b.id), `${a.id} and ${b.id}`).toBe(false);
      }
      expect(a.atWar.length, a.id).toBe(live.length - 1);
    }
    const said = g.headlines.slice(news);
    expect(said.filter((h) => h.kind === 'war')).toHaveLength(1);
    expect(said[0].text).toContain('World Domination');
    expect(said[0].text).toContain(`${live.length} nations`);
    expect(lastAct(g)).toMatchObject({ open: false, on: true, since: g.turn, left: live.length, winner: null });
    expect(lastAct(g)!.why).toMatch(/already begun/);
    // It is done once.
    expect(orderDomination(g)).toBe(false);
    expect(startDomination(g)).toBe(false);
    // Nobody is left to declare war on, and a small country has no path left to choose.
    expect(declarable(g)).toEqual([]);
  });

  it('sends troops on a former ally\'s soil home', () => {
    const g = decided();
    // A British corps in Paris, as the expeditionary force stood.
    const paris = g.byName['Paris'];
    const c = g.corps.find((x) => x.nat === 'UK' && !x.dead && x.kind !== 'fort')!;
    const from = g.regions[c.reg];
    from.units.splice(from.units.indexOf(c), 1);
    c.reg = paris.id; paris.units.push(c);
    c.ap = 4; c.org = 1;
    expect(allied(g, 'UK', paris.ctrl)).toBe(true);
    const guests = g.corps.filter((x) => !x.dead && g.regions[x.reg].ctrl !== x.nat && allied(g, x.nat, g.regions[x.reg].ctrl));
    expect(guests).toContain(c);
    startDomination(g);
    for (const x of guests) {
      expect(x.dead, `${x.nat} corps ${x.id}`).toBeFalsy();
      expect(g.regions[x.reg].ctrl, `${x.nat} corps ${x.id}`).toBe(x.nat);
      expect(x.ap).toBe(0);
      expect(x.org).toBeLessThanOrEqual(DOMINATION.guests.org);
    }
    expect(g.regions[c.reg].ctrl).toBe('UK');
    expect(g.headlines.some((h) => /safe conduct/.test(h.text))).toBe(true);
    // Nobody stands on ground held by a nation that was its friend yesterday.
    for (const x of g.corps) if (!x.dead && g.regions[x.reg].ctrl !== x.nat) expect(guests.includes(x)).toBe(false);
  });

  it('sees a nation from beyond the map take its forces home', () => {
    const g = decided();
    const U = g.nations.USA;
    U.alive = true; U.dormant = false;
    war(g, 'USA', 'ITA');
    // It holds a stretch of French Africa, taken in the war.
    const held = g.regions.filter((r) => r.owner === 'FRA' && r.overseas).slice(0, 2);
    expect(held.length).toBe(2);
    for (const r of held) r.ctrl = 'USA';
    expect(U.capital).toBeLessThan(0);
    startDomination(g);
    expect(U.alive).toBe(false);
    expect(U.dormant).toBe(true);
    expect(U.atWar).toEqual([]);
    expect(contenders(g).some((n) => n.id === 'USA')).toBe(false);
    for (const n of Object.values(g.nations)) expect(n.atWar).not.toContain('USA');
    expect(g.corps.some((c) => c.nat === 'USA')).toBe(false);
    expect(g.fleets.some((f) => f.nat === 'USA')).toBe(false);
    for (const r of held) expect(r.ctrl).toBe('FRA');
    expect(g.headlines.some((h) => /United States/.test(h.text) && /home/.test(h.text))).toBe(true);
    // It was never beaten: it left.
    expect(U.fellOn).toBeUndefined();
  });

  it('leaves a small country in the player\'s hands no path to choose', () => {
    const g = playing('SWE');
    expect(paths(g).length).toBeGreaterThan(0);
    capitulate(g, 'GER', 'UK');
    expect(orderDomination(g)).toBe(true);
    expect(paths(g)).toEqual([]);
    expect(pathOptions(g, 'SWE')).toEqual([]);
    expect(atWar(g, 'SWE', 'NOR')).toBe(true);
  });

  it('is kept by a saved game', () => {
    const g = decided();
    orderKeepPeace(g);
    startDomination(g);
    const h = loadGame(saveGame(g))!;
    expect(dominating(h)).toBe(true);
    expect(h.flags.dom).toBe(g.flags.dom);
    expect(h.flags['dom:n']).toBe(g.flags['dom:n']);
    expect(h.flags['dom:peace']).toBe(g.flags['dom:peace']);
    expect(contenders(h).map((n) => n.id)).toEqual(contenders(g).map((n) => n.id));
    for (const n of contenders(h)) { expect(n.align).toBe('independent'); expect(n.atWar.length).toBe(contenders(h).length - 1); }
    expect(saveGame(h)).toBe(saveGame(g));
  });
});

describe('the last war', () => {
  it('has no alliances, no treaties and no armistice', () => {
    const g = decided();
    startDomination(g);
    const n0 = contenders(g).length;
    for (let k = 0; k < 4; k++) nextMonth(g);
    const live = contenders(g);
    expect(live.length).toBeLessThanOrEqual(n0);
    for (const a of live) {
      expect(a.align, a.id).toBe('independent');
      expect(a.league, a.id).toBeFalsy();
      for (const b of live) if (a !== b) expect(atWar(g, a.id, b.id), `${a.id} and ${b.id}`).toBe(true);
    }
    // Nobody new has come onto the stage: no government at Vichy, no power from beyond the map.
    expect(g.nations.USA.alive).toBe(false);
    expect(g.nations.VIC.alive).toBe(false);
    // France conquered is France gone, not a new country in the south.
    if (g.nations.FRA.alive) {
      const by = live.find((n) => n.id !== 'FRA')!.id;
      capitulate(g, 'FRA', by);
      expect(g.nations.VIC.alive).toBe(false);
      expect(g.regions.some((r) => r.ctrl === 'VIC' || r.owner === 'VIC')).toBe(false);
      expect(g.corps.some((c) => c.nat === 'VIC')).toBe(false);
    }
  });

  it('has each staff weigh its neighbours one at a time, and strike at shorter odds', () => {
    const g = decided();
    const calm = temper(g, 'ITA');
    const one = fronts(g, 'FRA', { extra: new Set(['ITA', 'ESP', 'SUI', 'BEL']) });
    // In an ordinary war the enemies along a frontier are one front where their ground joins.
    expect(one.some((f) => new Set(f.edge.map((id) => g.regions[id].ctrl)).size > 1)).toBe(true);
    startDomination(g);
    const many = fronts(g, 'FRA');
    expect(many.length).toBeGreaterThan(one.length);
    for (const f of many) expect(new Set(f.edge.map((id) => g.regions[id].ctrl)).size).toBe(1);
    const keen = temper(g, 'ITA');
    expect(keen.attackAt).toBeCloseTo(calm.attackAt * DOMINATION.nerve.attackAt, 9);
    expect(keen.persist).toBeCloseTo(calm.persist * DOMINATION.nerve.persist, 9);
    expect(keen.minOdds).toBeCloseTo(calm.minOdds, 9);
  });

  it('ends when one nation is left standing', () => {
    const g = decided();
    startDomination(g);
    expect(worldWinner(g)).toBeNull();
    const rest = contenders(g).filter((n) => n.id !== 'UK').map((n) => n.id);
    for (const id of rest.slice(0, -1)) capitulate(g, id, 'UK');
    expect(worldWinner(g)).toBeNull();
    expect(g.flags['dom:won']).toBeUndefined();
    capitulate(g, rest[rest.length - 1], 'UK');
    expect(contenders(g).map((n) => n.id)).toEqual(['UK']);
    expect(g.flags['dom:won']).toBe(g.turn);
    expect(worldWinner(g)).toBe('UK');
    expect(lastAct(g)).toMatchObject({ on: true, left: 1, winner: 'UK' });
    const last = g.headlines.filter((h) => h.kind === 'goal').at(-1)!;
    expect(last.text).toContain('United Kingdom stands alone');
    expect(last.nat).toBe('UK');
    // Said once.
    const n = g.headlines.length;
    crown(g);
    expect(g.headlines.length).toBe(n);
    expect(g.regions.every((r) => r.ctrl === 'UK')).toBe(true);
  });

  it('can be won by somebody else', () => {
    const g = decided();
    startDomination(g);
    for (const n of contenders(g)) if (n.id !== 'SOV') capitulate(g, n.id, 'SOV');
    expect(worldWinner(g)).toBe('SOV');
    expect(lastAct(g)).toMatchObject({ winner: 'SOV', left: 1 });
    expect(whyNotDomination(g)).toMatch(/out of the war/);
  });

  it('can be played for months without a hitch', () => {
    const g = decided('UK', 5);
    startDomination(g);
    let left = contenders(g).length;
    for (let k = 0; k < 8; k++) {
      nextMonth(g);
      const now = contenders(g).length;
      expect(now).toBeLessThanOrEqual(left);
      left = now;
      for (const c of g.corps) expect(Number.isFinite(c.str) && c.str >= 0).toBe(true);
    }
    expect(dominating(g)).toBe(true);
    // The continent does not stand still: somebody has gone under.
    expect(left).toBeLessThan(g.flags['dom:n']);
  }, 60000);
});

describe('the standings', () => {
  it('rank those still standing by how much of the world they hold', () => {
    const g = decided();
    startDomination(g);
    const B = standings(g);
    expect(B.on).toBe(true);
    expect(B.since).toBe(g.turn);
    expect(B.left).toBe(contenders(g).length);
    expect(B.began).toBe(B.left);
    expect(B.winner).toBeNull();
    expect(B.rows.map((r) => r.nat).sort()).toEqual(contenders(g).map((n) => n.id).sort());
    for (let i = 1; i < B.rows.length; i++) expect(B.rows[i - 1].share).toBeGreaterThanOrEqual(B.rows[i].share);
    const sum = B.rows.reduce((s, r) => s + r.share, 0);
    expect(sum).toBeGreaterThan(0.95);
    expect(sum).toBeLessThanOrEqual(1 + 1e-9);
    expect(B.rows.reduce((s, r) => s + r.provinces, 0)).toBe(g.regions.filter((r) => contenders(g).some((n) => n.id === r.ctrl)).length);
    const me = B.rows.find((r) => r.you)!;
    expect(me.nat).toBe('UK');
    expect(B.rows.filter((r) => r.you)).toHaveLength(1);
    expect(me.exact).toBe(true);
    expect(me.divisions).toBeCloseTo(fieldArmy(g, 'UK'), 9);
    expect(me.income).toBeGreaterThan(0);
    for (const r of B.rows) { expect(r.alive).toBe(true); expect(r.will).toBeGreaterThan(0); expect(r.will).toBeLessThanOrEqual(1); expect(r.fell).toBeNull(); }
    // Germany fell before the last act: it is on the list of the fallen with its conqueror.
    expect(B.fallen.find((r) => r.nat === 'GER')).toMatchObject({ alive: false, provinces: 0, share: 0, divisions: 0, fell: { to: 'UK' } });
  });

  it('know a former ally\'s strength only as well as the fog allows', () => {
    const g = decided();
    expect(fogOn(g)).toBe(true);
    // An ally's books are open.
    expect(standings(g).rows.find((r) => r.nat === 'FRA')).toMatchObject({ exact: true });
    expect(standings(g).rows.find((r) => r.nat === 'FRA')!.income).not.toBeNull();
    startDomination(g);
    const fra = standings(g).rows.find((r) => r.nat === 'FRA')!;
    expect(fra.exact).toBe(false);
    expect(fra.income).toBeNull();
    expect(fra.divisions % 10).toBe(0);
    // A watcher with nobody at the controls is told everything.
    const all = worldBoard(g, null);
    expect(all.rows.every((r) => r.exact && r.income !== null && !r.you)).toBe(true);
    // A very small army is known only to be small.
    const few = standings(g).rows.filter((r) => r.few);
    for (const r of few) { expect(r.exact).toBe(false); expect(fieldArmy(g, r.nat)).toBeLessThan(5); }
    expect(standings(g).rows.some((r) => !r.few && !r.exact)).toBe(true);
  });

  it('list the fallen of the last war, newest first', () => {
    const g = decided();
    startDomination(g);
    capitulate(g, 'BEL', 'FRA');
    g.turn += 2;
    capitulate(g, 'NED', 'UK');
    const B = standings(g);
    expect(B.left).toBe(B.began - 2);
    expect(B.fallen.slice(0, 2).map((r) => [r.nat, r.fell!.to])).toEqual([['NED', 'UK'], ['BEL', 'FRA']]);
    expect(B.rows.some((r) => r.nat === 'BEL' || r.nat === 'NED')).toBe(false);
  });
});

describe('a war with nobody at the table', () => {
  it('is never given a last act, however it ends', () => {
    const g = grand(null, 1);
    // In this war Germany and Italy are beaten somewhere in its sixth or seventh year: the month moves a little whenever the rules do.
    while (g.turn <= 96 && !warDecided(g)) runTurnAI(g);
    expect(warDecided(g)).not.toBeNull();
    for (let i = 0; i < 3; i++) runTurnAI(g);
    expect(dominating(g)).toBe(false);
    expect(g.flags.dom).toBeUndefined();
    expect(Object.keys(g.flags).some((k) => k.startsWith('dom'))).toBe(false);
    expect(DIPLOMACY.majors.filter((id) => g.nations[id].alive).every((id) => g.nations[id].align !== 'independent' || id === 'SOV')).toBe(true);
  }, 60000);
});
