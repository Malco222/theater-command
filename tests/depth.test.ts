// Version 66: how deep the player plays (the grand campaign; none of it in the historical scenarios).
//
// 1. Six desks the player can run himself or leave to his staff: research, building and trade, diplomacy, the secret
//    service, the air war, the navy. For research, building, the air war and the navy the staff is the routine a
//    computer nation uses. Armies and battles are always the player's.
// 2. The staff spends only out of the share of the treasury the player allows it for the month.
// 3. Three depths set several switches at once: Field command (Simple), Theatre command (Standard), Supreme command
//    (Full). A depth also sets the cabinet, whether it can fall, and the fog of war.
// 4. A desk's month ends in one line of news tagged with the desk, and a desk can be taken back in any month.
// 5. Computer nations take no notice of any of it, and a game in which no switch is set is the game as it was.
import { describe, expect, it } from 'vitest';
import { afloat, airFleetsOf } from '../src/campaign/air';
import { DEPTH, DESKS, capOf, setDesk, staffRuns, type DeskId } from '../src/campaign/depth';
import { allowance, commandView, depthNow, setDepth, staffMonth } from '../src/campaign/desks';
import { DIPLOMACY } from '../src/campaign/diplomacy';
import { createGame, startPhase } from '../src/campaign/game';
import { cabinetOff, officeSecure } from '../src/campaign/government';
import { abroad, agentsOf, fogOn } from '../src/campaign/intel';
import { atSea, fleetsOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { idleTeams, teamsOf, techOf } from '../src/campaign/tech';
import { TRADE, dealsOf, endTrade, housesOf } from '../src/campaign/trade';
import { beginHuman, commandOf, current, endHuman, humanUp, orderDepth, orderDesk, orderStaffCap, runTurnAI, staffRunsDesk, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

/** A grand campaign brought to the player's first month, which is not yet opened. */
function ready(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  return g;
}
/** The same with the first month open, as the game's own screen would have it. */
function playing(human = 'GER', seed = 3): Game {
  const g = ready(human, seed);
  beginHuman(g);
  return g;
}
/** Ends the player's month without an order of his own and opens the next. */
function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
}
/** Hands a desk to the staff before the month is opened: the staff goes to work when it opens, as it does every month after. */
const hand = (g: Game, desk: DeskId): boolean => setDesk(g, desk, true);
const lines = (g: Game, desk: DeskId, turn = g.turn): string[] => g.headlines.filter((h) => h.desk === desk && h.turn === turn).map((h) => h.text);

describe('the switches', () => {
  it('a new grand campaign is the full game: every desk is the player\'s own', () => {
    const g = playing();
    for (const d of DESKS) expect(staffRunsDesk(g, d)).toBe(false);
    const v = commandOf(g)!;
    expect(v.depth).toBe('supreme');
    expect(v.desks.map((d) => d.id)).toEqual(['research', 'build', 'diplomacy', 'spies', 'air', 'navy']);
    expect(v.desks.every((d) => !d.staff && d.news === null && d.state.length > 0)).toBe(true);
    expect(v.cap).toBe(DEPTH.cap.start);
    expect(g.headlines.some((h) => h.desk)).toBe(false);
  });

  it('a desk is handed over and taken back, one at a time', () => {
    const g = playing();
    expect(orderDesk(g, 'air', true)).toBe(true);
    expect(orderDesk(g, 'air', true)).toBe(false);
    expect(staffRunsDesk(g, 'air')).toBe(true);
    expect(staffRunsDesk(g, 'navy')).toBe(false);
    expect(commandOf(g)!.depth).toBeNull();
    expect(commandOf(g)!.name).toBe('Your own mix');
    expect(orderDesk(g, 'air', false)).toBe(true);
    expect(commandOf(g)!.depth).toBe('supreme');
  });

  it('there are no depths in a historical scenario, and none with nobody playing', () => {
    const h = createGame(3, 'GER');
    expect(commandOf(h)).toBeNull();
    expect(orderDesk(h, 'air', true)).toBe(false);
    expect(orderDepth(h, 'field')).toBe(false);
    h.flags['dk:air'] = 1;
    expect(staffRuns(h, 'GER', 'air')).toBe(false);
    const w = startOpen(createGame(3, null));
    expect(commandOf(w)).toBeNull();
    expect(orderDepth(w, 'field')).toBe(false);
  });

  it('what the staff may spend is one of the steps on offer', () => {
    const g = playing();
    expect(orderStaffCap(g, 75)).toBe(true);
    expect(commandOf(g)!.cap).toBe(75);
    expect(capOf(g)).toBe(0.75);
    expect(orderStaffCap(g, 60)).toBe(false);
    expect(orderStaffCap(g, DEPTH.cap.start)).toBe(true);
    expect(g.flags['dk:cap']).toBeUndefined();
  });
});

describe('the three depths', () => {
  it('Field command: the staff runs every desk, there is no cabinet and no fog', () => {
    const g = ready();
    expect(orderDepth(g, 'field')).toBe(true);
    for (const d of DESKS) expect(staffRunsDesk(g, d)).toBe(true);
    expect(cabinetOff(g)).toBe(true);
    expect(officeSecure(g)).toBe(true);
    expect(fogOn(g)).toBe(false);
    expect(depthNow(g)?.id).toBe('field');
    expect(commandOf(g)!.depths.find((d) => d.on)?.under).toBe('Simple');
  });

  it('Theatre command: the staff flies, sails and spies; the cabinet sits and cannot bring the player down', () => {
    const g = ready();
    orderDepth(g, 'theatre');
    expect(DESKS.filter((d) => staffRunsDesk(g, d))).toEqual(['spies', 'air', 'navy']);
    expect(cabinetOff(g)).toBe(false);
    expect(officeSecure(g)).toBe(true);
    expect(fogOn(g)).toBe(true);
    const v = commandOf(g)!;
    expect(v.depth).toBe('theatre');
    expect(v.depths.find((d) => d.id === 'theatre')!.yours).toEqual(['The armies and their battles', 'Research', 'Building and trade', 'Diplomacy']);
  });

  it('Supreme command puts everything back on the player\'s desk, and the cabinet can bring him down', () => {
    const g = ready();
    orderDepth(g, 'field');
    orderDepth(g, 'supreme');
    for (const d of DESKS) expect(staffRunsDesk(g, d)).toBe(false);
    expect(cabinetOff(g)).toBe(false);
    expect(officeSecure(g)).toBe(false);
    expect(fogOn(g)).toBe(true);
    expect(depthNow(g)?.id).toBe('supreme');
  });

  it('any switch can be changed after a depth is picked, and the game then calls it a mix of the player\'s own', () => {
    const g = ready();
    orderDepth(g, 'theatre');
    orderDesk(g, 'research', true);
    expect(commandOf(g)!.depth).toBeNull();
    orderDesk(g, 'research', false);
    expect(commandOf(g)!.depth).toBe('theatre');
  });

  it('a depth picked in the open month sets the staff to work at once, and only once', () => {
    const g = playing();
    const from = g.headlines.length;
    orderDepth(g, 'field');
    expect(techOf(g, 'GER')!.work.length).toBe(teamsOf(g, 'GER'));
    const told = g.headlines.slice(from).filter((h) => h.desk).length;
    expect(told).toBeGreaterThan(0);
    orderDepth(g, 'supreme');
    orderDepth(g, 'field');
    expect(g.headlines.slice(from).filter((h) => h.desk).length).toBe(told);
  });
});

describe('research left to the advisers', () => {
  it('by hand the teams wait for the player; with the advisers every team is at work each month', () => {
    const own = playing();
    expect(idleTeams(own, 'GER')).toBeGreaterThan(0);
    const g = ready();
    hand(g, 'research');
    beginHuman(g);
    expect(idleTeams(g, 'GER')).toBe(0);
    expect(techOf(g, 'GER')!.work.length).toBe(teamsOf(g, 'GER'));
    expect(lines(g, 'research')[0]).toMatch(/^Your advisers put \d research teams to work on /);
    for (let k = 0; k < 10; k++) { month(g); expect(idleTeams(g, 'GER')).toBe(0); }
    // A project finished under the advisers is not announced as a team waiting for work.
    const done = g.headlines.filter((h) => h.text.startsWith('Research complete'));
    expect(done.length).toBeGreaterThan(0);
    expect(done.every((h) => !h.text.includes('free for new work'))).toBe(true);
  });
});

describe('building and trade left to the staff', () => {
  it('by hand nothing is ordered; the staff raises corps and builds, as a computer nation does', () => {
    const own = playing();
    for (let k = 0; k < 6; k++) month(own);
    expect(own.nations.GER.queue.length).toBe(0);
    const g = ready();
    hand(g, 'build');
    beginHuman(g);
    let ordered = 0;
    for (let k = 0; k < 6; k++) { month(g); ordered += lines(g, 'build').filter((t) => t.includes('ordered')).length; }
    expect(ordered).toBeGreaterThan(0);
    expect(g.nations.GER.queue.length + g.corps.filter((c) => c.nat === 'GER').length).toBeGreaterThan(own.corps.filter((c) => c.nat === 'GER').length);
    expect(lines(g, 'build').every((t) => t.startsWith('Your staff '))).toBe(true);
  });

  it('the staff never spends more in a month than the share of the treasury it is allowed', () => {
    for (const cap of [25, 100]) {
      const g = ready('SOV', 5);
      hand(g, 'build');
      hand(g, 'spies');
      orderStaffCap(g, cap);
      let spentAll = 0;
      for (let k = 0; k < 8; k++) {
        startPhase(g, 'SOV');
        const n = g.nations.SOV;
        n.pp += 60;
        const had = n.pp, may = had * (cap / 100);
        expect(allowance(g, 'SOV')).toBeCloseTo(may, 6);
        staffMonth(g);
        const spent = had - n.pp;
        expect(spent).toBeLessThanOrEqual(may + 1e-6);
        expect(spent).toBeGreaterThanOrEqual(0);
        spentAll += spent;
        endHuman(g);
        for (let j = 0; !humanUp(g) && j < 200; j++) stepAI(g);
      }
      expect(spentAll).toBeGreaterThan(0);
    }
  });

  it('a treasury in debt allows the staff nothing', () => {
    const g = ready('SOV', 5);
    hand(g, 'build');
    startPhase(g, 'SOV');
    g.nations.SOV.pp = -20;
    staffMonth(g);
    expect(g.flags['dk:ran:build']).toBe(g.turn);
    expect(allowance(g, 'SOV')).toBe(0);
    expect(g.nations.SOV.pp).toBe(-20);
    expect(g.nations.SOV.queue.length).toBe(0);
  });

  it('a trading house that stands idle is given an agreement, at the player\'s price in political capital', () => {
    const g = playing('GER', 3);
    const mine = dealsOf(g, 'GER');
    expect(mine.length).toBeGreaterThan(0);
    endTrade(g, mine[0].a, mine[0].b);
    const n = g.nations.GER;
    n.pc = 10;
    const had = dealsOf(g, 'GER').length;
    expect(had).toBeLessThan(housesOf(g, 'GER'));
    orderDesk(g, 'build', true);
    expect(dealsOf(g, 'GER').length).toBe(had + 1);
    expect(n.pc).toBe(10 - TRADE.sign.pc);
    expect(lines(g, 'build').join(' ')).toMatch(/signed a trade agreement with /);
  });
});

describe('diplomacy left to the envoys', () => {
  it('they court one neutral a month while the political capital allows, and keep a little in hand', () => {
    const g = playing('GER', 3);
    const n = g.nations.GER;
    n.pc = DIPLOMACY.court.cost + DEPTH.diplomacy.keep - 1;
    orderDesk(g, 'diplomacy', true);
    expect(lines(g, 'diplomacy')).toEqual([]);
    orderDesk(g, 'diplomacy', false);
    g.flags['dk:ran:diplomacy'] = 0;
    n.pc = 12;
    orderDesk(g, 'diplomacy', true);
    expect(n.pc).toBe(12 - DIPLOMACY.court.cost);
    expect(lines(g, 'diplomacy')[0]).toMatch(/^Your envoys courted /);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('court:GER:') && g.flags[k] === g.turn).length).toBe(1);
  });
});

describe('the secret service left to itself', () => {
  it('in peace it trains a guard and sends nobody abroad', () => {
    const g = ready('SOV', 5);
    hand(g, 'spies');
    beginHuman(g);
    for (let k = 0; k < 5; k++) month(g);
    expect(g.nations.SOV.atWar.length).toBe(0);
    const mine = agentsOf(g, 'SOV');
    expect(mine.length).toBe(1);
    expect(mine.every((a) => !abroad(g, a))).toBe(true);
  });

  it('at war it fills the service, reads the books in the enemy capital, watches the front and tries no sabotage', () => {
    const g = ready('GER', 3);
    hand(g, 'spies');
    beginHuman(g);
    let sent = 0;
    for (let k = 0; k < 8; k++) {
      month(g);
      sent += lines(g, 'spies').filter((t) => t.includes('sent ')).length;
      expect(agentsOf(g, 'GER').every((a) => a.mission === 'guard' || a.mission === 'watch' || a.mission === 'books')).toBe(true);
    }
    expect(sent).toBeGreaterThan(0);
    expect(agentsOf(g, 'GER').some((a) => abroad(g, a))).toBe(true);
    expect(g.headlines.some((h) => h.desk === 'spies' && /to read the books/.test(h.text))).toBe(true);
  });
});

describe('the air war and the navy left to the staff', () => {
  it('by hand nothing flies and no fleet sails unless the player says so', () => {
    const g = ready('GER', 3);
    const fleets = fleetsOf(g, 'GER').map((f) => `${f.id}:${f.zone}:${f.stance}`);
    const from = g.airReports.length;
    beginHuman(g);
    expect(g.airReports.slice(from).some((r) => r.attNat === 'GER')).toBe(false);
    expect(fleetsOf(g, 'GER').map((f) => `${f.id}:${f.zone}:${f.stance}`)).toEqual(fleets);
  });

  it('the air staff flies raids for the player in a war, and tells him in one line', () => {
    const g = ready('GER', 3);
    hand(g, 'air');
    let flown = 0, told = 0;
    for (let k = 0; k < 4; k++) {
      const from = g.airReports.length;
      if (k) month(g); else beginHuman(g);
      flown += g.airReports.slice(from).filter((r) => r.attNat === 'GER' && r.turn === g.turn).length;
      told += lines(g, 'air').length;
      expect(lines(g, 'air').length).toBeLessThanOrEqual(1);
    }
    expect(flown).toBeGreaterThan(0);
    expect(told).toBeGreaterThan(0);
    expect(g.headlines.find((h) => h.desk === 'air')!.text).toMatch(/^Your air staff flew (a raid|\d+ raids): /);
  });

  it('the admirals send the fleets out in a war and keep them in harbour in peace', () => {
    const g = ready('GER', 3);
    hand(g, 'navy');
    beginHuman(g);
    let atSeaEver = false;
    for (let k = 0; k < 4; k++) { atSeaEver ||= fleetsOf(g, 'GER').some((f) => atSea(f)); month(g); }
    expect(atSeaEver).toBe(true);
    const p = ready('SOV', 5);
    hand(p, 'navy');
    beginHuman(p);
    for (let k = 0; k < 3; k++) month(p);
    expect(p.nations.SOV.atWar.length).toBe(0);
    expect(fleetsOf(p, 'SOV').every((f) => !atSea(f))).toBe(true);
  });

  it('the carriers go with the navy: an air staff alone leaves their wings on deck', () => {
    const g = ready('UK', 7);
    hand(g, 'air');
    beginHuman(g);
    for (let k = 0; k < 4; k++) {
      expect(airFleetsOf(g, 'UK').filter((f) => afloat(f)).every((f) => f.flew !== g.turn)).toBe(true);
      month(g);
    }
    const n = ready('UK', 7);
    hand(n, 'navy');
    beginHuman(n);
    for (let k = 0; k < 4; k++) {
      expect(airFleetsOf(n, 'UK').filter((f) => !afloat(f)).every((f) => f.flew !== n.turn)).toBe(true);
      month(n);
    }
  });
});

describe('one line a month, and the rest of the world unmoved', () => {
  it('each desk tells its month in at most one line, tagged with the desk; a desk taken back falls silent', () => {
    const g = ready('GER', 3);
    setDepth(g, 'field');
    beginHuman(g);
    for (let k = 0; k < 5; k++) {
      for (const d of DESKS) expect(lines(g, d).length).toBeLessThanOrEqual(1);
      const v = commandView(g)!;
      for (const d of v.desks) expect(d.news).toBe(lines(g, d.id)[0] ?? null);
      month(g);
    }
    expect(g.headlines.filter((h) => h.desk).length).toBeGreaterThan(5);
    for (const d of DESKS) orderDesk(g, d, false);
    const from = g.headlines.length;
    month(g);
    month(g);
    expect(g.headlines.slice(from).some((h) => h.desk)).toBe(false);
  });

  it('computer nations take no notice of the switches', () => {
    const play = (set: boolean): string => {
      const g = startOpen(createGame(9, null));
      if (set) { for (const d of DESKS) g.flags[`dk:${d}`] = 1; g.flags['dk:cap'] = 25; }
      for (let k = 0; k < 6; k++) runTurnAI(g);
      const s = JSON.parse(saveGame(g));
      for (const k of Object.keys(s.flags)) if (k.startsWith('dk:')) delete s.flags[k];
      return JSON.stringify(s);
    };
    expect(play(true)).toBe(play(false));
  });

  it('with a desk in the staff\'s hands the other nations play the same month they would have played', () => {
    // The staff's own orders change the war, but a desk that has nothing to do (the navy of a country at peace) changes nothing.
    const play = (set: boolean): string => {
      const g = ready('SOV', 5);
      if (set) hand(g, 'navy');
      beginHuman(g);
      for (let k = 0; k < 3; k++) month(g);
      const s = JSON.parse(saveGame(g));
      for (const k of Object.keys(s.flags)) if (k.startsWith('dk:')) delete s.flags[k];
      return JSON.stringify(s);
    };
    expect(play(true)).toBe(play(false));
  });

  it('the switches and the tagged news are kept in a save', () => {
    const g = ready('GER', 3);
    setDepth(g, 'field');
    orderStaffCap(g, 75);
    beginHuman(g);
    month(g);
    const back = loadGame(saveGame(g))!;
    expect(back).not.toBeNull();
    expect(current(back)).toBe('GER');
    const a = commandOf(g)!, b = commandOf(back)!;
    expect(b.depth).toBe('field');
    expect(b.cap).toBe(75);
    expect(b.desks.map((d) => [d.id, d.staff, d.news])).toEqual(a.desks.map((d) => [d.id, d.staff, d.news]));
  });
});
